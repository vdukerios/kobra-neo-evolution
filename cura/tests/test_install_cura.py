"""Tests for cura/install_cura.py. They use a fake Cura folder in a temp dir; they never touch a real Cura.
Run: python -I tests/test_install_cura.py
"""
import contextlib
import io
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import install_cura as ic  # noqa: E402

GLOBAL = """[general]
version = 6
name = Anycubic Kobra Neo
id = Anycubic Kobra Neo

[metadata]
setting_version = 27
type = machine
is_online = False

[containers]
0 = Anycubic Kobra Neo_user
1 = custom_pla
2 = empty_intent
3 = draft
4 = empty_material
5 = empty_variant
6 = Anycubic Kobra Neo_settings
7 = custom
"""
SETTINGS = """[general]
version = 4
name = Anycubic Kobra Neo_settings
definition = custom

[metadata]
type = definition_changes
setting_version = 27

[values]
machine_width = 300
machine_start_gcode = G28
\tM104 S200
my_own_value = keep_me
"""


def make_cura(root, global_text=GLOBAL):
    cura = os.path.join(root, "5.14")
    for d in ("machine_instances", "definition_changes", "quality_changes", "scripts"):
        os.makedirs(os.path.join(cura, d))
    open(os.path.join(cura, "machine_instances", "Anycubic+Kobra+Neo.global.cfg"), "w", encoding="utf-8", newline="\n").write(global_text)
    open(os.path.join(cura, "definition_changes", "Anycubic+Kobra+Neo_settings.inst.cfg"), "w", encoding="utf-8", newline="\n").write(SETTINGS)
    return cura


def run(args):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        try:
            code = ic.main(args + ["--skip-running-check"])
        except SystemExit as e:
            code = e.code
    return code, out.getvalue()


def snapshot(cura):
    snap = {}
    for r, _d, fs in os.walk(cura):
        if ic.BACKUP_ROOT_NAME in r:
            continue
        for f in fs:
            p = os.path.join(r, f)
            snap[os.path.relpath(p, cura)] = open(p, "rb").read()
    return snap


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)
    print("ok  -", msg)


def main():
    BS3 = chr(92) * 3   # Cura stores each newline of a script as three backslashes plus n
    tmp = tempfile.mkdtemp()
    try:
        # 1) dry run changes nothing
        cura = make_cura(os.path.join(tmp, "a"))
        before = snapshot(cura)
        code, out = run(["--cura-dir", cura, "--dry-run"])
        check(code in (0, None) and snapshot(cura) == before and "Dry run" in out, "dry run writes nothing")

        # 2) clean install
        code, out = run(["--cura-dir", cura])
        check(code in (0, None), "install finishes")
        after = snapshot(cura)
        for fn in ic.SCRIPTS:
            check(after[os.path.join("scripts", fn)] == open(os.path.join(ic.HERE, "scripts", fn), "rb").read(), fn + " copied")
        g = ic.read_cfg(os.path.join(cura, "machine_instances", "Anycubic+Kobra+Neo.global.cfg"))
        val = g["metadata"]["post_processing_scripts"]
        check(ic.scripts_in_value(val) == ["AgregarM73", "AgregarMiniatura"], "both scripts enabled: %r" % ic.scripts_in_value(val))
        check(BS3 + "n" in val and "factor_tiempo = 1.0" in val.replace(BS3 + "n", chr(10)), "scripts stored with Cura escaping and default values")
        s = ic.read_cfg(os.path.join(cura, "definition_changes", "Anycubic+Kobra+Neo_settings.inst.cfg"))
        check("M1100" in s["values"]["machine_start_gcode"], "start G-code uses M1100")
        check(s["values"]["machine_width"] == "220.0" and s["values"]["my_own_value"] == "keep_me", "limits updated, unrelated values kept")
        check(s["general"]["name"] == "Anycubic Kobra Neo_settings" and s["metadata"]["setting_version"] == "27", "general/metadata sections untouched")

        # 3) second run changes nothing
        snap2 = snapshot(cura)
        code, out = run(["--cura-dir", cura])
        check(snapshot(cura) == snap2 and "Nothing to change" in out, "second run is a no-op (idempotent)")

        # 4) uninstall restores the original bytes
        code, out = run(["--cura-dir", cura, "--uninstall"])
        restored = snapshot(cura)
        orig = snapshot(make_cura(os.path.join(tmp, "ref")))
        check(restored == orig, "uninstall restores every original file byte for byte")

        # 5) keeps scripts the user already had
        other = GLOBAL.replace("is_online = False", "is_online = False" + chr(10) + "post_processing_scripts = [PauseAtHeight]" + BS3 + "nx = 1" + BS3 + "n" + BS3 + "n")
        cura2 = make_cura(os.path.join(tmp, "b"), other)
        run(["--cura-dir", cura2])
        g2 = ic.read_cfg(os.path.join(cura2, "machine_instances", "Anycubic+Kobra+Neo.global.cfg"))
        check(ic.scripts_in_value(g2["metadata"]["post_processing_scripts"]) == ["PauseAtHeight", "AgregarM73", "AgregarMiniatura"],
              "existing post-processing scripts are kept, ours appended")

        # 6) classic start and profiles flag
        cura3 = make_cura(os.path.join(tmp, "c"))
        run(["--cura-dir", cura3, "--classic-start", "--profiles"])
        s3 = ic.read_cfg(os.path.join(cura3, "definition_changes", "Anycubic+Kobra+Neo_settings.inst.cfg"))
        st = s3["values"]["machine_start_gcode"]
        check("M1100 S" not in st and "G28" in st and "M420 S1" in st, "--classic-start has no M1100")
        check(os.path.exists(os.path.join(cura3, "quality_changes", "custom_pla.inst.cfg")), "--profiles copies quality profiles")

        # 7) errors
        code, out = run(["--cura-dir", os.path.join(tmp, "nada")])
        check(code not in (0, None), "unknown folder is an error")
        code, out = run(["--cura-dir", cura3, "--printer", "No Existe"])
        check(code not in (0, None), "unknown printer is an error")
        print("\nALL OK")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
