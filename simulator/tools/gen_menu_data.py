#!/usr/bin/env python
"""Generate the simulator menu data from the firmware menu inventory.

Usage (run from anywhere):
    python -I gen_menu_data.py <path/to/menu_inventory.json> [path/to/simulator.html]

Writes ../menu_data.js and rewrites, in place, the block
    <script id="menu-data"> ... </script>
inside simulator.html (default: ../simulator.html). Re-run after a firmware
change (after regenerating the inventory) to refresh the simulator.

Only ACTIVE screens that can be reached from the real UI are emitted. Every
item keeps: type (k), EN/ES label, destination (to), action key (a),
visibility conditions (c), numeric range/unit/decimals/default.
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SIM_DIR = HERE.parent

# Values the inventory leaves as C++ expressions. Resolved from the firmware
# configuration (HEATER_0_MAXTEMP 275, hotend_max_target = MAXTEMP - 15, ...).
RANGE_FIX = {
    "thermalManager . hotend_max_target(0)": (260, False),
    "thermalManager . hotend_max_target(extruder)": (260, False),
    "_MAX(1, 0) - 1": (0, False),
    "_MIN(5, 999)": (5, False),
    "_MAX(275, 0) - (15)": (260, False),
    "max_accel": (1000, True),
    "planner . settings . max_acceleration_mm_per_s2[E_AXIS]": (500, False),
}
AXIS_IDX = {"A": 0, "B": 1, "C": 2, "E": 3}

ACTION_MAP = [
    ("ui . pause_print", "pause"), ("ui . resume_print", "resume"),
    ("repeat . cancel", "repeat_cancel"), ("boton_nivelar_inicio", "level_center"),
    ("ui . store_settings", "store"), ("ui . load_settings", "load"),
    ("ui . reset_settings", "restore"), ("lcd_cooldown", "cooldown"),
    ("MM_SILENCIOSO", "motor:silent"), ("MM_MIXTO", "motor:mixed"), ("MM_FUERZA", "motor:high"),
    ("set_lcd_language(0)", "lang:en"), ("set_lcd_language(1)", "lang:es"),
    ("M428", "homeoff"), ("lcd_power_loss_recovery_resume", "recov_resume"),
    ("lcd_power_loss_recovery_cancel", "recov_cancel"), ("lcd_sd_updir", "updir"),
    ("_change_filament_with_preset", "filop_preset"), ("do_preheat_end_m", "preheat_end"),
    ("menu_info_registro_abrir", "goto:menu_info_registro"),
    ("_preheat_both", "preheat_both"), ("_preheat_bed", "preheat_bed"),
]
GCODE_MAP = [("G28_STR", "gcode:home"), ("G28X", "gcode:homex"), ("G28Y", "gcode:homey"),
             ("G28ZR10", "gcode:homez"), ("M84", "gcode:m84")]
DEST_MAP = {"preheat_pla": "preheat:0", "preheat_ABS": "preheat:1",
            "begin_bed_lever": "bedlevel", "lcd_babystep_zoffset": "zoffset",
            "_lcd_level_bed_corners": "tramming", "boton_nivelar_inicio": "levelcenter", "version_message": "about"}
LAMBDA_BY_LABEL = {"Load Filament": "filop:load", "Unload Filament": "filop:unload",
                   "Move X": "move:x", "Move Y": "move:y", "Move Z": "move:z"}
EXTRA_EDGES = {  # lambda / dynamic destinations -> screens
    "preheat:0": "menu_preheat_m", "preheat:1": "menu_preheat_m",
    "filop:load": "_menu_temp_filament_op", "filop:unload": "_menu_temp_filament_op",
    "move:x": "_menu_move_distance", "move:y": "_menu_move_distance", "move:z": "_menu_move_distance",
}
# ---------------------------------------------------------------------------
# PENDING OVERLAY: "Menu restructure" (firmware branch mejoras-valentin, commit "Menu restructure",
# written but not yet compiled, so it is NOT in menu_inventory.json v2).
# When the inventory is regenerated from firmware that contains this commit, set the flag to False.
APPLY_PENDING_RESTRUCTURE = True


def apply_pending_restructure(out, notes):
    """Edits the generated screens in memory (before reachability and before menu_data.js is written):
    idle main menu = Back | Print | Move Axis | Level and center | Prepare | Configuration | Maintenance |
    Night mode | Guide and help; new Maintenance submenu (Lubricate axes / Check belts dialogs);
    Configuration gains Bed Leveling, Language, About Printer, About before Store Settings;
    Bed Leveling loses 'Level Bed' (same as Level and center)."""
    main = out["menu_main"]["items"]
    name = lambda it: (it.get("en") or "").strip()
    idle = lambda it: "idle" in it.get("c", [])

    def pop_item(items, pred):
        for i, it in enumerate(items):
            if pred(it):
                return items.pop(i)
        raise KeyError("overlay: item not found (inventory changed? disable APPLY_PENDING_RESTRUCTURE)")

    moved = {}
    for n in ("Bed Leveling", "Language", "About", "About Printer", "Lubricate axes", "Check belts"):
        moved[n] = pop_item(main, lambda it, n=n: name(it) == n and idle(it))
        moved[n].pop("c", None)
    pos = next(i for i, it in enumerate(main) if name(it) == "Configuration" and idle(it))
    main.insert(pos + 1, {"k": "sub", "to": "menu_maintenance", "en": "Maintenance", "es": "Mantenimiento", "c": ["idle"]})
    out["menu_maintenance"] = {"id": "menu_maintenance", "t": ["Maintenance", "Mantenimiento"],
                               "items": [{"k": "back", "en": "Back", "es": "Atras"}, moved["Lubricate axes"], moved["Check belts"]]}
    cfg = out["menu_configuration"]["items"]
    pos = next(i for i, it in enumerate(cfg) if it.get("a") == "store")
    cfg[pos:pos] = [moved[n] for n in ("Bed Leveling", "Language", "About Printer", "About")]
    pop_item(out["menu_bed_leveling"]["items"], lambda it: name(it) == "Level Bed")
    notes.append("PENDING OVERLAY 'Menu restructure' is applied by the generator (APPLY_PENDING_RESTRUCTURE): Maintenance submenu; "
                 "Bed Leveling, Language, About Printer and About moved to Configuration; Level Bed removed. "
                 "Disable the flag once the inventory is regenerated from that firmware.")


EVENT_ROOTS = ["menu_job_recovery"]  # opened by an event (power loss), not by the menu tree


def lab(e):
    et = e.get("etiqueta") or {}
    return (et.get("en") or ""), (et.get("es") or ""), (et.get("expr") or "")


def conds(vis):
    """Map inventory visibility strings to simulator tokens. Returns (tokens, flags, unknown)."""
    toks, flags, unk = [], set(), []
    for s in vis:
        if s == "siempre":
            continue
        if "solo imprimiendo" in s:
            toks.append("busy")
        elif "solo sin imprimir" in s:
            toks.append("idle")
        elif "solo en pausa" in s:
            toks.append("paused")
        elif "!is_busy" in s:
            toks.append("idle")
        elif "registro_linea" in s:
            toks.append("reglines")
        elif "!alguna" in s:
            toks.append("nolog")
        elif "i < 5" in s:
            pass
        elif "printingIsPaused" in s:
            toks.append("notpaused")
        elif "card_open" in s:
            toks.append("cardopen")
        elif "[card_detected]" in s:
            toks.append("card")
        elif "repeat.is_active" in s:
            toks.append("repeat")
        elif "controllerFan.settings.auto_mode" in s:
            toks.append("fan_auto")
        elif "parser.volumetric_enabled" in s:
            toks.append("volumetric")
        elif "is_valid" in s:
            toks.append("mesh")
        elif "NOT(card_detected)" in s:
            toks.append("nocard")
        elif "NOT(card.flag.workDirIsRoot)" in s:
            toks.append("subdir")
        elif "filenameIsDir" in s:
            toks.append("isdir")
        elif "fileCnt" in s:
            flags.add("files")
        elif "_menuLineNr == _thisItemNr" in s:
            pass
        elif "m<(3)" in s:
            flags.add("loop")
        elif "[switch axis]" in s:
            toks.append("axis")
        elif "axis == Z_AXIS" in s:
            toks.append("axisZ")
        elif "NOT(parser.using_inch_units())" in s:
            pass
        elif "parser.using_inch_units()" in s:
            flags.add("drop")      # inch units are not active in this firmware
        elif ">= 1000" in s:
            flags.add("drop")      # 100 mm move only when an axis is >= 1000 mm long
        elif re.search(r"7 >= 4|\[condicion: 1\]|all_axes_homed", s):
            pass
        else:
            toks.append("u:" + s)
            unk.append(s)
    return sorted(set(toks)), flags, unk


def num(x):
    if isinstance(x, (int, float)):
        return x
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def rng(x, notes_ctx, which):
    """Resolve a range bound; returns (value, approximated)."""
    n = num(x)
    if n is not None:
        return (int(n) if float(n).is_integer() else n), False
    if isinstance(x, str) and x in RANGE_FIX:
        return RANGE_FIX[x]
    return None, True


def default_for(e):
    d = e.get("valor_defecto_Configuration")
    if not isinstance(d, dict):
        return None
    val = str(d.get("valor", "")).strip()
    cfg = str(d.get("config", ""))
    var = str(e.get("variable", ""))
    m = re.match(r"^\{(.*)\}$", val)
    if m:
        arr = [num(p.strip()) for p in m.group(1).split(",")]
        if "NOZZLE_TO_PROBE_OFFSET" in cfg:
            idx = {"x": 0, "y": 1, "z": 2}.get(var[-1:].lower())
        else:
            ax = re.search(r"_AXIS\((A|B|C)\)|(E)_AXIS", var)
            idx = AXIS_IDX.get(ax.group(1) or ax.group(2)) if ax else None
        return arr[idx] if idx is not None and idx < len(arr) else None
    return num(val)


def build(inv):
    P = inv["pantallas"]
    out, notes, unknown_conds = {}, [], set()
    for sid, p in P.items():
        act = [e for e in p["elementos"] if e.get("activo")]
        if not act:
            continue
        t = p.get("titulo") or {}
        sc = {"id": sid}
        if t.get("en"):
            sc["t"] = [t["en"], t.get("es") or t["en"]]
        items = []
        for e in act:
            en, es, expr = lab(e)
            tp = e["tipo"]
            c, fl, unk = conds(e.get("visibilidad") or ["siempre"])
            unknown_conds.update(unk)
            if "drop" in fl:
                continue
            it = {}
            if "files" in fl and "isdir" in c:
                tp = "folder"
            if tp == "folder":
                it["k"] = "folder"
            elif tp == "retroceso":
                it["k"] = "back"
            elif tp == "submenu":
                it["k"] = "sub"
                d = e.get("destino")
                if d == "(lambda)":
                    raw = str(e.get("accion_raw") or "")
                    g = re.search(r"guia_abrir\((\d+)\)", raw)
                    it["to"] = ("guide:" + g.group(1)) if g else LAMBDA_BY_LABEL.get(en.strip())
                    if it["to"] is None and re.search(r"Move \d|^$", en.strip()):
                        it["to"] = "dist:z" if not en.strip() else "dist:" + re.sub(r"[^0-9.]", "", en)
                else:
                    it["to"] = DEST_MAP.get(d, d)
            elif tp.startswith("submenu->pantalla"):
                it["k"] = "confsub"
            elif tp == "editar valor numerico":
                it["k"] = "edit"
            elif tp == "interruptor on-off":
                it["k"] = "tog"
            elif tp == "estatico":
                it["k"] = "info"
            elif tp == "accion":
                it["k"] = "act"
                raw = str(e.get("accion") or "") + " " + str(e.get("accion_raw") or "")
                it["a"] = next((v for k, v in ACTION_MAP if k in raw), raw.strip()[:30] or "noop")
            elif tp == "gcode":
                it["k"] = "act"
                g = str(e.get("gcode") or "")
                it["a"] = next((v for k, v in GCODE_MAP if k in g), "gcode:?")
            elif tp == "confirmacion":
                it["k"] = "conf"
            elif tp.startswith("archivo SD"):
                it["k"] = "file"
            else:
                it["k"] = "info"
            if en.strip() == "Attach":
                it["a"] = "attach"
            if it["k"] == "act" and en.strip() == "Cooldown":
                it["a"] = "cooldown"
            # labels
            if en or es:
                it["en"], it["es"] = en, es or en
            elif expr:
                it["x"] = expr
            if "loop" in fl:
                it["lp"] = 1
            if c:
                it["c"] = c
            # numeric edit data
            if tp == "editar valor numerico" and not tp.startswith("archivo"):
                mn, amn = rng(e.get("min"), sid, "min")
                mx, amx = rng(e.get("max"), sid, "max")
                df = default_for(e)
                if mn is None:
                    mn = 0
                if mx is None:
                    mx = (df * 2) if df else 100
                    if e.get("min") is not None or e.get("max") is not None:
                        notes.append("%s: range max of '%s' is an expression (%s), approximated as %s"
                                     % (sid, en.strip() or expr, e.get("max"), mx))
                    amx = True
                it.update({"min": mn, "max": mx, "d": e.get("decimales") or 0})
                if amx:
                    it["ap"] = 1
                if e.get("unidad"):
                    it["u"] = e["unidad"]
                if e.get("tipo_valor") == "percent":
                    it["pct"] = 1
                if e.get("edicion_en_vivo"):
                    it["live"] = 1
                if df is not None:
                    it["df"] = df
                else:
                    notes.append("%s: default of '%s' is n/d in the inventory (shown as n/a)" % (sid, en.strip() or expr))
                it["v"] = re.sub(r"\s+", "", str(e.get("variable") or ""))[:60]
            if tp == "interruptor on-off":
                it["v"] = re.sub(r"\s+", "", str(e.get("variable") or ""))[:40]
            if tp == "confirmacion" or tp.startswith("submenu->pantalla"):
                bt = e.get("botones") or e.get("confirmacion") or []
                bt = [[b.get("en") or "", b.get("es") or b.get("en") or ""] for b in bt]
                if tp == "confirmacion":
                    pr = e.get("pantalla_confirmacion") or []
                    it["bt"] = bt[:2]
                    it["pr"] = [[b.get("en") or "", b.get("es") or b.get("en") or ""] for b in pr] if pr else [[en, es]]
                else:
                    it["bt"] = bt[:2]
                    it["pr"] = [bt[2]] if len(bt) > 2 else [[en, es]]
            items.append(it)
        sc["items"] = items
        if p.get("tipo_pantalla") == "pantalla estatica":
            sc["static"] = 1  # click anywhere returns to the previous screen
        out[sid] = sc

    active_before = sum(len(s["items"]) for s in out.values())
    if APPLY_PENDING_RESTRUCTURE:
        apply_pending_restructure(out, notes)

    # reachability from the real UI
    seen, todo = set(), ["menu_main"] + EVENT_ROOTS
    # entering a destination can open further screens
    while todo:
        s = todo.pop()
        if s in seen or s not in out:
            continue
        seen.add(s)
        for it in out[s]["items"]:
            to = it.get("to")
            if to:
                todo.append(EXTRA_EDGES.get(to, to))
            if str(it.get("a", "")).startswith("goto:"):
                todo.append(it["a"][5:])
        if s == "menu_media":
            todo += ["menu_media_filelist", "menu_confirmar_impresion"]
    unreachable = sorted(set(out) - seen)
    screens = {k: out[k] for k in out if k in seen}
    n_items = sum(len(s["items"]) for s in screens.values())
    meta = {"screens": len(screens), "items": n_items,
            "active_items": active_before, "overlay": bool(APPLY_PENDING_RESTRUCTURE), "unreachable": unreachable,
            "inventory_screens": len(P)}
    return screens, notes, meta, sorted(unknown_conds)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    inv_path = Path(sys.argv[1])
    html_path = Path(sys.argv[2]) if len(sys.argv) > 2 else SIM_DIR / "simulator.html"
    inv = json.loads(inv_path.read_text(encoding="utf-8"))
    screens, notes, meta, unk = build(inv)
    if unk:
        notes.append("Visibility conditions not understood by the generator (treated as always true): " + "; ".join(unk))
    data = {"meta": meta, "screens": screens, "notes": notes}
    js = "var MENU_DATA = " + json.dumps(data, ensure_ascii=False, separators=(",", ":")) + ";\n"
    (SIM_DIR / "menu_data.js").write_text("/* generated by tools/gen_menu_data.py - do not edit */\n" + js, encoding="utf-8")
    if html_path.exists():
        html = html_path.read_text(encoding="utf-8")
        block = ('<script id="menu-data">\n/* BEGIN GENERATED MENU DATA (tools/gen_menu_data.py) */\n' + js
                 + '/* END GENERATED MENU DATA */\n</script>')
        pat = re.compile(r'<script id="menu-data">.*?</script>', re.S)
        if pat.search(html):
            html = pat.sub(lambda m: block, html, count=1)
        else:
            html = html.replace('<script id="model">', block + '\n\n<script id="model">', 1)
        html_path.write_text(html, encoding="utf-8")
    print("screens: %d  items: %d  unreachable: %d  notes: %d  size: %d bytes"
          % (meta["screens"], meta["items"], len(meta["unreachable"]), len(notes), len(js)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
