"""Tests for AgregarMiniatura using Cura/QImage stubs. Run:  python -I tests/test_miniatura.py
(from any folder; with -I Python does not add the current directory to the path). They do not need Cura."""
import base64
import importlib.util
import json
import os
import struct
import sys
import types

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, AQUI)  # para importar el paquete fake_cura

spec = importlib.util.spec_from_file_location(
    "fake_cura.scripts.AgregarMiniatura", os.path.join(RAIZ, "AgregarMiniatura.py"))
modulo = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = modulo
spec.loader.exec_module(modulo)

ALFABETO = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"


class ImagenFalsa:
    """Minimal imitation of QImage: width(), height(), pixel(x, y) -> 0xAARRGGBB."""
    def __init__(self, w, h, fn):
        self._w, self._h, self._fn = w, h, fn

    def width(self):
        return self._w

    def height(self):
        return self._h

    def pixel(self, x, y):
        a, r, g, b = self._fn(x, y)
        return (a << 24) | (r << 16) | (g << 8) | b


def instalar_snapshot(resultado):
    """Installs a fake cura.Snapshot. 'resultado' is an image, None or an exception to raise."""
    llamadas = []

    class Snapshot:
        @staticmethod
        def snapshot(width=300, height=300):
            llamadas.append((width, height))
            if isinstance(resultado, Exception):
                raise resultado
            return resultado

    cura = types.ModuleType("cura")
    cura.__path__ = []
    mod = types.ModuleType("cura.Snapshot")
    mod.Snapshot = Snapshot
    sys.modules["cura"] = cura
    sys.modules["cura.Snapshot"] = mod
    return llamadas


def quitar_snapshot():
    sys.modules.pop("cura", None)
    sys.modules.pop("cura.Snapshot", None)


def nuevo(lado=None):
    s = modulo.AgregarMiniatura()
    if lado is not None:
        s._valores["lado"] = lado
    return s


def gcode_sintetico(con_flavor=True):
    cab = (";FLAVOR:Marlin\n" if con_flavor else "") + ";TIME:600\n;Generated with Cura_SteamEngine 5.x\n"
    return [cab, "M140 S60\nG28\n", ";LAYER:0\nG1 X10 Y10 E1\n", "M104 S0\nM84\n"]


def lineas_datos(texto):
    """';<base64>' lines between kn_thumb_begin and kn_thumb_end."""
    out, dentro = [], False
    for l in texto.split("\n"):
        if l.startswith(";kn_thumb_end"):
            break
        if dentro:
            out.append(l)
        if l.startswith(";kn_thumb_begin"):
            dentro = True
    return out


def decodificar_como_firmware(texto):
    """Re-implements in Python the (strict) rules of the firmware reader gcode_thumb.cpp.
    Returns (width, height, [pixels]) or None."""
    estado, total, buf, acc, nbits, relleno = 0, 0, bytearray(), 0, 0, False
    w = h = 0
    for l in texto.split("\n"):
        l = l.rstrip("\r")
        if estado == 0:
            if l.startswith(";kn_thumb_begin"):
                p = l[len(";kn_thumb_begin"):].split()
                if len(p) != 3 or p[2] != "rgb565":
                    return None
                w, h = int(p[0]), int(p[1])
                if not (0 < w <= 96 and 0 < h <= 96):
                    return None
                total = w * h * 2
                estado = 1
            continue
        if l.startswith(";kn_thumb_end"):
            if len(buf) != total:
                return None
            return w, h, list(struct.unpack("<%dH" % (total // 2), bytes(buf)))
        if not l.startswith(";"):
            return None
        for c in l[1:]:
            if c == "=":
                relleno = True
                continue
            if c not in ALFABETO or relleno:
                return None
            acc = ((acc << 6) | ALFABETO.index(c)) & 0xFFFFFF
            nbits += 6
            if nbits >= 8:
                nbits -= 8
                buf.append((acc >> nbits) & 0xFF)
                if len(buf) > total:
                    return None
    return None


ok = 0


def check(nombre, cond, extra=""):
    global ok
    if not cond:
        print("FALLA:", nombre, extra)
        sys.exit(1)
    ok += 1
    print("ok  -", nombre)


def ROJO(x, y):
    return (255, 255, 0, 0)


# 1) Conversion RGB565 exacta de colores conocidos
check("rgb565 rojo", modulo.rgb565(255, 0, 0) == 0xF800)
check("rgb565 verde", modulo.rgb565(0, 255, 0) == 0x07E0)
check("rgb565 azul", modulo.rgb565(0, 0, 255) == 0x001F)
check("rgb565 blanco", modulo.rgb565(255, 255, 255) == 0xFFFF)
check("rgb565 negro", modulo.rgb565(0, 0, 0) == 0x0000)
check("rgb565 fondo #0A0F1C", modulo.rgb565(0x0A, 0x0F, 0x1C) == 0x0863)
check("rgb565 gris 128", modulo.rgb565(128, 128, 128) == 0x8410)

# 2) Bloque correcto con imagen roja opaca
llamadas = instalar_snapshot(ImagenFalsa(160, 160, ROJO))
orig = gcode_sintetico()
res = nuevo().execute(list(orig))
texto = "".join(res)
lineas = res[0].split("\n")
check("bloque justo despues de ;FLAVOR", lineas[0] == ";FLAVOR:Marlin" and lineas[1] == ";kn_thumb_begin 80 80 rgb565")
check("se pidio la vista al doble (160x160)", llamadas == [(160, 160)])
datos = lineas_datos(texto)
check("lineas de datos de 76 caracteres tras el ';' (salvo la ultima)",
      all(len(l) == 77 for l in datos[:-1]) and 1 <= len(datos[-1]) <= 77)
check("17068 caracteres base64 en total", sum(len(l) - 1 for l in datos) == 17068)
check(";kn_thumb_end seguido del resto de la cabecera",
      lineas[lineas.index(";kn_thumb_end") + 1] == ";TIME:600")
crudo = base64.b64decode("".join(l[1:] for l in datos))
check("12 800 bytes de pixeles", len(crudo) == 12800)
check("todos los pixeles rojos 0xF800 little-endian", crudo == b"\x00\xf8" * 6400)
check("resto del G-code intacto", res[1:] == orig[1:])
check("el parser tipo firmware lo decodifica", decodificar_como_firmware(texto) == (80, 80, [0xF800] * 6400))

# 3) Transparencia sobre el fondo oscuro, y semitransparencia
instalar_snapshot(ImagenFalsa(160, 160, lambda x, y: (0, 255, 255, 255)))
r = decodificar_como_firmware("".join(nuevo().execute(gcode_sintetico())))
check("transparente = fondo #0A0F1C (0x0863)", r[2] == [0x0863] * 6400)
instalar_snapshot(ImagenFalsa(160, 160, lambda x, y: (128, 255, 0, 0)))
r = decodificar_como_firmware("".join(nuevo().execute(gcode_sintetico())))
check("alfa 128 rojo sobre fondo = 0x8021", r[2] == [0x8021] * 6400)

# 4) Orden fila por fila: filas de arriba verdes; abajo, izquierda azul y derecha roja
instalar_snapshot(ImagenFalsa(160, 160, lambda x, y: (255, 0, 255, 0) if y < 80 else
                              ((255, 0, 0, 255) if x < 80 else (255, 255, 0, 0))))
w, h, px = decodificar_como_firmware("".join(nuevo().execute(gcode_sintetico())))
check("orden de filas (verde arriba, azul/rojo abajo)",
      px[0] == 0x07E0 and px[39 * 80 + 79] == 0x07E0 and px[40 * 80] == 0x001F
      and px[40 * 80 + 39] == 0x001F and px[40 * 80 + 40] == 0xF800 and px[79 * 80 + 79] == 0xF800)

# 5) Imagen no cuadrada: se mantiene la proporcion, con bandas de fondo
instalar_snapshot(ImagenFalsa(160, 80, ROJO))
w, h, px = decodificar_como_firmware("".join(nuevo().execute(gcode_sintetico())))
check("160x80 -> 80x40 centrado (3200 rojos, 3200 de fondo)",
      sum(1 for p in px if p == 0xF800) == 3200 and sum(1 for p in px if p == 0x0863) == 3200
      and px[19 * 80 + 79] == 0x0863 and px[20 * 80] == 0xF800
      and px[59 * 80 + 79] == 0xF800 and px[60 * 80] == 0x0863)

# 6) Promedio al reducir (tablero blanco/negro -> gris)
instalar_snapshot(ImagenFalsa(160, 160, lambda x, y: (255, 255, 255, 255) if (x + y) % 2 == 0 else (255, 0, 0, 0)))
w, h, px = decodificar_como_firmware("".join(nuevo().execute(gcode_sintetico())))
check("tablero reducido da gris 128 (0x8410)", set(px) == {0x8410})

# 7) Sin ;FLAVOR: el bloque va al principio
instalar_snapshot(ImagenFalsa(160, 160, ROJO))
res = nuevo().execute(gcode_sintetico(con_flavor=False))
check("sin FLAVOR: bloque al principio", res[0].startswith(";kn_thumb_begin 80 80 rgb565\n")
      and ";TIME:600" in res[0])

# 8) Idempotencia
una = nuevo().execute(gcode_sintetico())
dos = nuevo().execute(list(una))
check("idempotente: dos pasadas = una", una == dos)
check("un solo bloque", "".join(dos).count(";kn_thumb_begin") == 1 and "".join(dos).count(";kn_thumb_end") == 1)
instalar_snapshot(ImagenFalsa(160, 160, lambda x, y: (255, 0, 255, 0)))
tres = nuevo().execute(list(una))
w, h, px = decodificar_como_firmware("".join(tres))
check("segunda pasada con otra imagen reemplaza el bloque",
      px == [0x07E0] * 6400 and "".join(tres).count(";kn_thumb_begin") == 1)
check("el reemplazo deja el resto igual al original", modulo.quitar_bloque("".join(tres)) == "".join(gcode_sintetico()))

# 9) Tamano configurable y limites
llamadas = instalar_snapshot(ImagenFalsa(192, 192, ROJO))
res = nuevo(96).execute(gcode_sintetico())
check("lado 96", res[0].split("\n")[1] == ";kn_thumb_begin 96 96 rgb565" and llamadas[-1] == (192, 192)
      and decodificar_como_firmware("".join(res))[2] == [0xF800] * 9216)
check("lado se limita a 96", ";kn_thumb_begin 96 96 rgb565" in nuevo(500).execute(gcode_sintetico())[0])
check("lado se limita a 32 como minimo", ";kn_thumb_begin 32 32 rgb565" in nuevo(3).execute(gcode_sintetico())[0])
check("ajustes JSON validos", json.loads(nuevo().getSettingDataString())["key"] == "AgregarMiniatura")

# 10) Fallos del snapshot: el G-code no cambia y no se inserta nada
casos = [
    ("snapshot lanza excepcion", lambda: instalar_snapshot(RuntimeError("sin contexto OpenGL"))),
    ("snapshot devuelve None", lambda: instalar_snapshot(None)),
    ("imagen de 0x0", lambda: instalar_snapshot(ImagenFalsa(0, 0, ROJO))),
    ("imagen cuyos pixeles fallan al leerse", lambda: instalar_snapshot(ImagenFalsa(10, 10, lambda x, y: 1 // 0))),
    ("cura.Snapshot no existe (ImportError)", quitar_snapshot),
]
for nombre, preparar in casos:
    preparar()
    original = gcode_sintetico()
    res = nuevo().execute(list(original))
    check(nombre + ": G-code sin cambios", res == original and "kn_thumb" not in "".join(res))

# 11) Fallo con un bloque previo: no se toca nada
instalar_snapshot(ImagenFalsa(160, 160, ROJO))
con_bloque = nuevo().execute(gcode_sintetico())
instalar_snapshot(None)
check("fallo con bloque previo: queda igual", nuevo().execute(list(con_bloque)) == con_bloque)

# 12) Casos limite
instalar_snapshot(ImagenFalsa(160, 160, ROJO))
check("lista vacia no rompe", "kn_thumb_begin" in "".join(nuevo().execute([])))
check("un solo bloque de texto", "kn_thumb_end" in nuevo().execute([";FLAVOR:Marlin\n"])[0])

print("\n%d comprobaciones correctas" % ok)
