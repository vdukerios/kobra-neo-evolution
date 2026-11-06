"""Tests for AgregarM73 using a Cura stub. Run:  python -I tests/test_agregar_m73.py
(from any folder; with -I Python does not add the current directory to the path)."""
import importlib.util
import json
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, AQUI)  # to import the fake_cura package

spec = importlib.util.spec_from_file_location(
    "fake_cura.scripts.AgregarM73", os.path.join(RAIZ, "AgregarM73.py"))
modulo = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = modulo
spec.loader.exec_module(modulo)


def nuevo(factor=None):
    s = modulo.AgregarM73()
    if factor is not None:
        s._valores["factor_tiempo"] = factor
    return s


def gcode_sintetico(con_time=True):
    """Lista de bloques como la entrega Cura: cabecera, arranque, capas, final."""
    cab = ";FLAVOR:Marlin\n" + (";TIME:600\n" if con_time else "") + ";Generated with Cura_SteamEngine 5.x\n"
    arranque = "M140 S60\nM104 S200\nG28\nG1 Z0.3 F3000\n"
    elapsed = [100, 220, 340, 480, 590]
    capas = []
    for n, t in enumerate(elapsed):
        capas.append(";LAYER:%d\nG1 X10 Y10 E1\nG1 X20 Y20 E2\n" % n
                     + (";TIME_ELAPSED:%s\n" % t if con_time else ""))
    fin = "M104 S0\nM140 S0\nG28 X0\nM84\n"
    return [cab, arranque] + capas + [fin]


def m73(data):
    return [l for b in data for l in b.split("\n") if l.startswith("M73")]


def sin_marca(lineas):
    return [l.replace(" ;AgregarM73", "") for l in lineas]


ok = 0


def check(nombre, cond, extra=""):
    global ok
    print(("OK    " if cond else "FALLA ") + nombre + (" " + extra if not cond else ""))
    if not cond:
        sys.exit(1)
    ok += 1


# --- Factor 1.0 ---
d = nuevo().execute(gcode_sintetico())
lin = sin_marca(m73(d))
check("M73 inicial", lin[0] == "M73 P0 R10", lin[0])
check("M73 por capa (factor 1.0)",
      lin[1:6] == ["M73 P16 R8", "M73 P36 R6", "M73 P56 R4", "M73 P80 R2", "M73 P98 R0"], str(lin[1:6]))
check("M73 final", lin[-1] == "M73 P100 R0" and len(lin) == 7, str(lin))
check("M73 va tras ;TIME_ELAPSED",
      d[2].split("\n")[-3:-1] == [";TIME_ELAPSED:100", "M73 P16 R8 ;AgregarM73"], str(d[2].split("\n")))
check("M73 inicial al comienzo del arranque", d[1].startswith("M73 P0 R10 "))

# --- Factor 1.18 ---
lin = sin_marca(m73(nuevo(1.18).execute(gcode_sintetico())))
check("factor 1.18: inicial", lin[0] == "M73 P0 R12", lin[0])
check("factor 1.18: capas (porcentaje sin cambio)",
      lin[1:6] == ["M73 P16 R10", "M73 P36 R7", "M73 P56 R5", "M73 P80 R2", "M73 P98 R0"], str(lin[1:6]))
check("factor 1.18: final", lin[-1] == "M73 P100 R0")

# --- Fuera de rango: se limita a 0.5-3.0 ---
lin = sin_marca(m73(nuevo(10).execute(gcode_sintetico())))
check("factor fuera de rango se limita a 3.0", lin[0] == "M73 P0 R30", lin[0])

# --- Idempotencia ---
una = nuevo(1.18).execute(gcode_sintetico())
dos = nuevo(1.18).execute(list(una))
check("idempotente (2 pasadas = 1 pasada)", una == dos)
check("sin M73 duplicados", len(m73(dos)) == 7)
tres = nuevo(1.0).execute(list(una))
check("repasar con otro factor reemplaza, no duplica",
      tres == nuevo(1.0).execute(gcode_sintetico()))

# --- Sin ;TIME: ---
orig = gcode_sintetico(con_time=False)
res = nuevo().execute(list(orig))
check("sin ;TIME: no cambia nada ni falla", res == orig and m73(res) == [])
check("lista vacia no falla", nuevo().execute([]) == [])
check("ajustes JSON validos con rango 0.5-3.0",
      (lambda s: s["default_value"] == 1.0 and s["minimum_value"] == 0.5 and s["maximum_value"] == 3.0)(
          json.loads(nuevo().getSettingDataString())["settings"]["factor_tiempo"]))

print("\nTodas las pruebas pasaron (%d)." % ok)
