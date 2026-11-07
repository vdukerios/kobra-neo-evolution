#!/usr/bin/env python3
"""Automatic Cura setup for the Kobra Neo Evolution (KNE) firmware.

What it does (all optional steps can be skipped with flags):
  1. Copies the post-processing scripts (AgregarM73, AgregarMiniatura) into Cura's "scripts" folder.
  2. Turns both scripts ON for your printer (Cura then runs them on every slice).
  3. Writes the KNE start/end G-code and the machine limits into your printer's settings.
  4. (--profiles) Copies the author's PLA / PETG / TPU quality profiles (a starting point).

Safe by design:
  * Cura must be closed (otherwise it would overwrite the changes when it exits).
  * Every file it changes is backed up first, in <cura folder>/kne_installer_backup/<date>/.
  * Running it twice changes nothing the second time.
  * --uninstall puts the original files back.
  * Python standard library only. Needs Python 3.8+.

Usage examples:
  python install_cura.py                  # find Cura, ask nothing, install
  python install_cura.py --dry-run        # show what would change, change nothing
  python install_cura.py --printer "My Kobra Neo" --version 5.12
  python install_cura.py --classic-start  # start G-code without M1100 (see docs/CURA_SETUP.md)
  python install_cura.py --uninstall      # undo the last installation
"""
import argparse
import configparser
import datetime
import io
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = ("AgregarM73.py", "AgregarMiniatura.py")
PRINTER_PROFILE = os.path.join(HERE, "profiles", "printer", "Anycubic+Kobra+Neo_settings.inst.cfg")
QUALITY_DIR = os.path.join(HERE, "profiles", "quality")
BACKUP_ROOT_NAME = "kne_installer_backup"

# Start G-code without M1100: explicit steps, uses the mesh already stored in the printer.
# Needs a valid saved mesh (run "Level and center" or G29 + M500 once). The short soak is optional.
CLASSIC_START = (
    ";Clean start - Kobra Neo with KNE firmware (classic, no M1100)\n"
    "M140 S{material_bed_temperature_layer_0} ; bed to temperature\n"
    "M104 S{material_print_temperature_layer_0} ; nozzle to temperature, at the same time as the bed\n"
    "M190 S{material_bed_temperature_layer_0} ; wait for the bed\n"
    "G4 S120 ; short soak so the bed temperature settles\n"
    "G28 ; home\n"
    "M420 S1 ; switch on the mesh stored in the printer\n"
    "M109 S{material_print_temperature_layer_0} ; wait for the nozzle\n"
    "G92 E0 ; reset extruder\n"
    "G1 Z5 F600 ; lift before starting"
)


# --------------------------------------------------------------------------- helpers
def say(msg=""):
    print(msg, flush=True)


def new_parser():
    p = configparser.ConfigParser(interpolation=None)
    p.optionxform = str  # setting keys are case sensitive
    return p


def read_cfg(path):
    p = new_parser()
    with open(path, "r", encoding="utf-8") as f:
        p.read_file(f)
    return p


def cfg_to_text(p):
    buf = io.StringIO()
    p.write(buf)
    return buf.getvalue()


def config_roots():
    """Folders that may hold Cura versions (e.g. .../cura/5.12)."""
    home = os.path.expanduser("~")
    roots = []
    if sys.platform.startswith("win"):
        appdata = os.environ.get("APPDATA")
        if appdata:
            roots.append(os.path.join(appdata, "cura"))
    elif sys.platform == "darwin":
        roots.append(os.path.join(home, "Library", "Application Support", "cura"))
    else:
        roots.append(os.path.join(home, ".local", "share", "cura"))
        roots.append(os.path.join(home, ".var", "app", "com.ultimaker.cura", "data", "cura"))
    return roots


def version_key(name):
    return tuple(int(x) for x in name.split("."))


def find_versions(extra_root=None):
    """Returns [(version, path)] newest first."""
    found = []
    for root in ([extra_root] if extra_root else config_roots()):
        if not root or not os.path.isdir(root):
            continue
        for name in os.listdir(root):
            full = os.path.join(root, name)
            if re.fullmatch(r"\d+\.\d+", name) and os.path.isdir(os.path.join(full, "machine_instances")):
                found.append((name, full))
    found.sort(key=lambda t: version_key(t[0]), reverse=True)
    return found


def cura_is_running():
    """True/False, or None if it cannot be determined."""
    try:
        if sys.platform.startswith("win"):
            out = subprocess.run(["tasklist"], capture_output=True, text=True, timeout=20).stdout.lower()
            return any(n in out for n in ("ultimaker-cura.exe", "cura.exe"))
        out = subprocess.run(["ps", "-A", "-o", "comm="], capture_output=True, text=True, timeout=20).stdout.lower()
        return any(n in out for n in ("ultimaker-cura", "ultimaker_cura", "/cura", "cura-bin", "\ncura\n"))
    except Exception:
        return None


def list_printers(cura_dir):
    """[(display name, global cfg path)]"""
    out = []
    folder = os.path.join(cura_dir, "machine_instances")
    for fn in sorted(os.listdir(folder)):
        if fn.endswith(".global.cfg"):
            path = os.path.join(folder, fn)
            try:
                name = read_cfg(path)["general"]["name"]
            except Exception:
                continue
            out.append((name, path))
    return out


def choose_printer(printers, wanted):
    if wanted:
        for name, path in printers:
            if name.lower() == wanted.lower():
                return name, path
        raise SystemExit("Printer '%s' not found. Printers in this Cura: %s"
                         % (wanted, ", ".join(n for n, _ in printers) or "(none)"))
    hits = [(n, p) for n, p in printers if "kobra neo" in n.lower()]
    if len(hits) == 1:
        return hits[0]
    if not hits:
        raise SystemExit(
            "No printer called 'Kobra Neo' found in Cura.\n"
            "  1) Open Cura > Settings > Printer > Add printer > Custom FFF printer, name it 'Anycubic Kobra Neo'.\n"
            "  2) Close Cura and run this script again (or pass --printer \"<your printer name>\").")
    raise SystemExit("Several Kobra Neo printers found (%s). Choose one with --printer \"<name>\"."
                     % ", ".join(n for n, _ in hits))


def script_defaults(script_path):
    """Reads (key, {setting: default}) from the JSON inside getSettingDataString()."""
    text = open(script_path, "r", encoding="utf-8").read()
    m = re.search(r'getSettingDataString\(self\):\s*return\s*"""(.*?)"""', text, re.S)
    if not m:
        raise SystemExit("Cannot read the settings of " + script_path)
    data = json.loads(m.group(1))
    return data["key"], {k: v.get("default_value") for k, v in data["settings"].items()}


def serialize_script(key, values):
    """Same encoding Cura uses for 'post_processing_scripts' (see PostProcessingPlugin.writeScriptsToStack)."""
    p = new_parser()
    p.add_section(key)
    for k, v in values.items():
        p[key][k] = str(v)
    text = cfg_to_text(p)
    return text.replace("\\\\", r"\\\\").replace("\n", r"\\\n")


def scripts_in_value(value):
    """Keys of the scripts already stored in a 'post_processing_scripts' value."""
    keys = []
    for part in (value or "").split("\n"):
        m = re.match(r"\[([^\]]+)\]", part.strip())
        if m:
            keys.append(m.group(1))
    return keys


class Plan:
    """Collects file changes so they can be shown (dry run), backed up and applied."""

    def __init__(self, cura_dir):
        self.cura_dir = cura_dir
        self.writes = []  # (relative path, bytes, description)

    def add(self, rel, data, desc):
        full = os.path.join(self.cura_dir, rel)
        if isinstance(data, str):
            data = data.encode("utf-8")
        old = open(full, "rb").read() if os.path.exists(full) else None
        if old == data:
            return
        self.writes.append((rel, data, desc, old is not None))

    def empty(self):
        return not self.writes

    def apply(self, backup_dir):
        manifest = {"created": [], "replaced": []}
        for rel, data, desc, existed in self.writes:
            full = os.path.join(self.cura_dir, rel)
            if existed:
                dest = os.path.join(backup_dir, rel)
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.copy2(full, dest)
                manifest["replaced"].append(rel)
            else:
                manifest["created"].append(rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as f:
                f.write(data)
        os.makedirs(backup_dir, exist_ok=True)
        with open(os.path.join(backup_dir, "manifest.json"), "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2)
        return manifest


# --------------------------------------------------------------------------- planning
def plan_install(cura_dir, printer_name, global_cfg, args):
    plan = Plan(cura_dir)
    scripts_info = []

    # 1) scripts
    if not args.no_scripts:
        for fn in SCRIPTS:
            src = os.path.join(HERE, "scripts", fn)
            plan.add(os.path.join("scripts", fn), open(src, "rb").read(), "post-processing script " + fn)
            scripts_info.append(script_defaults(src))

    # 2) enable scripts for this printer (global stack metadata)
    if scripts_info:
        gp = read_cfg(global_cfg)
        old = gp["metadata"].get("post_processing_scripts", "")
        present = scripts_in_value(old)
        add = [serialize_script(k, v) for k, v in scripts_info if k not in present]
        # A previous test copy of the thumbnail script under another name would double the picture: warn only.
        if add:
            new = "\n".join([x for x in [old] if x] + add)
            gp["metadata"]["post_processing_scripts"] = new
            plan.add(os.path.relpath(global_cfg, cura_dir), cfg_to_text(gp), "enable scripts for '%s'" % printer_name)

    # 3) machine settings: start/end G-code + limits
    if not args.no_machine:
        gp = read_cfg(global_cfg)
        cont = gp["containers"] if gp.has_section("containers") else {}
        ident = cont.get("6")
        if not ident or not ident.endswith("_settings"):
            cands = [v for v in cont.values() if v.endswith("_settings")]
            ident = cands[0] if cands else None
        if not ident:
            raise SystemExit("Cannot find the machine settings container of '%s'." % printer_name)
        rel = os.path.join("definition_changes", urllib.parse.quote_plus(ident) + ".inst.cfg")
        target = os.path.join(cura_dir, rel)
        if not os.path.exists(target):
            raise SystemExit("Missing %s. Open the printer once in Cura (Machine settings) and try again." % rel)
        tp = read_cfg(target)
        if not tp.has_section("values"):
            tp.add_section("values")
        src = read_cfg(PRINTER_PROFILE)
        for k, v in src["values"].items():
            if k == "machine_start_gcode" and args.classic_start:
                v = CLASSIC_START
            tp["values"][k] = v
        plan.add(rel, cfg_to_text(tp), "start/end G-code and machine limits")

    # 4) quality profiles (optional)
    if args.profiles:
        for fn in sorted(os.listdir(QUALITY_DIR)):
            if fn.endswith(".inst.cfg"):
                plan.add(os.path.join("quality_changes", fn), open(os.path.join(QUALITY_DIR, fn), "rb").read(),
                         "quality profile " + fn)
    return plan


# --------------------------------------------------------------------------- uninstall
def latest_backup(cura_dir):
    root = os.path.join(cura_dir, BACKUP_ROOT_NAME)
    if not os.path.isdir(root):
        return None
    items = sorted(d for d in os.listdir(root) if os.path.exists(os.path.join(root, d, "manifest.json")))
    return os.path.join(root, items[-1]) if items else None


def uninstall(cura_dir, dry):
    b = latest_backup(cura_dir)
    if not b:
        raise SystemExit("No backup found in %s: nothing to undo." % os.path.join(cura_dir, BACKUP_ROOT_NAME))
    m = json.load(open(os.path.join(b, "manifest.json"), encoding="utf-8"))
    say("Restoring from %s" % b)
    for rel in m["replaced"]:
        say("  restore  " + rel)
        if not dry:
            shutil.copy2(os.path.join(b, rel), os.path.join(cura_dir, rel))
    for rel in m["created"]:
        say("  remove   " + rel)
        if not dry and os.path.exists(os.path.join(cura_dir, rel)):
            os.remove(os.path.join(cura_dir, rel))
    if not dry:
        shutil.move(b, b + "_undone")
        say("Done. The backup folder was renamed to *_undone.")


# --------------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description="Set up Cura for the Kobra Neo Evolution firmware.")
    ap.add_argument("--version", help="Cura version folder to use (e.g. 5.12). Default: the newest one found.")
    ap.add_argument("--printer", help="Printer name in Cura. Default: the only one containing 'Kobra Neo'.")
    ap.add_argument("--cura-dir", help="Use this Cura configuration folder directly (advanced / tests).")
    ap.add_argument("--profiles", action="store_true", help="Also install the PLA/PETG/TPU quality profiles.")
    ap.add_argument("--classic-start", action="store_true", help="Use the start G-code without M1100.")
    ap.add_argument("--no-scripts", action="store_true", help="Do not install or enable the scripts.")
    ap.add_argument("--no-machine", action="store_true", help="Do not touch start/end G-code and machine limits.")
    ap.add_argument("--dry-run", action="store_true", help="Show what would change; change nothing.")
    ap.add_argument("--uninstall", action="store_true", help="Undo the last installation using its backup.")
    ap.add_argument("--skip-running-check", action="store_true", help=argparse.SUPPRESS)
    args = ap.parse_args(argv)

    # which Cura folder
    if args.cura_dir:
        cura_dir = os.path.abspath(args.cura_dir)
        if not os.path.isdir(os.path.join(cura_dir, "machine_instances")):
            raise SystemExit("%s does not look like a Cura configuration folder (no machine_instances)." % cura_dir)
        version = os.path.basename(cura_dir)
    else:
        versions = find_versions()
        if not versions:
            raise SystemExit("No Cura configuration found in: %s\nOpen Cura once so it creates its folder, "
                             "then run this script again." % ", ".join(config_roots()))
        if args.version:
            match = [v for v in versions if v[0] == args.version]
            if not match:
                raise SystemExit("Cura %s not found. Available: %s" % (args.version, ", ".join(v for v, _ in versions)))
            version, cura_dir = match[0]
        else:
            version, cura_dir = versions[0]
    say("Cura %s  (%s)" % (version, cura_dir))

    if not args.skip_running_check and not args.dry_run:
        running = cura_is_running()
        if running:
            raise SystemExit("Cura is running. Close it completely (check the system tray) and run this script again.")
        if running is None:
            say("Warning: could not check whether Cura is running. Make sure it is closed.")

    if args.uninstall:
        uninstall(cura_dir, args.dry_run)
        return 0

    printers = list_printers(cura_dir)
    printer_name, global_cfg = choose_printer(printers, args.printer)
    say("Printer: %s" % printer_name)

    plan = plan_install(cura_dir, printer_name, global_cfg, args)
    if plan.empty():
        say("Everything is already set up. Nothing to change.")
        return 0
    say("Changes:")
    for rel, _data, desc, existed in plan.writes:
        say("  %-8s %s  (%s)" % ("replace" if existed else "create", rel, desc))
    if args.dry_run:
        say("Dry run: nothing was written.")
        return 0

    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    backup = os.path.join(cura_dir, BACKUP_ROOT_NAME, stamp)
    plan.apply(backup)
    say("Done. Originals saved in: " + backup)
    say("Open Cura: Extensions > Post processing > Modify G-code should list the two KNE scripts.")
    say("To undo: python install_cura.py --uninstall")
    return 0


if __name__ == "__main__":
    sys.exit(main())
