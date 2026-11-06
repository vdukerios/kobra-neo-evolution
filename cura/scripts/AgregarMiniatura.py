# Post-processing script for Ultimaker Cura 5.x
# Adds a thumbnail (still image of the model) at the START of the G-code for the printer screen.
#
# Format (comments, no PNG so the firmware needs no decompressor):
#   ;kn_thumb_begin <width> <height> rgb565
#   ;<76-character base64>          raw RGB565 little-endian pixels, row by row
#   ;kn_thumb_end
#
# Default 80x80 (12,800 bytes = 17,067 base64 characters); up to 96x96 supported.
#
# [assumption] Could not be tested with a real Cura. It is assumed that cura.Snapshot.Snapshot.snapshot(width, height)
# can be called from a post-processing script (the CreateThumbnail script bundled with Cura does it this way).
# If anything fails (no view, no Cura, error), the G-code is left EXACTLY as it was.

import base64
import re
import struct

from ..Script import Script

try:
    from UM.Logger import Logger
except Exception:  # outside Cura (tests)
    Logger = None

MARCA_INICIO = ";kn_thumb_begin"
MARCA_FIN = ";kn_thumb_end"
FONDO = (0x0A, 0x0F, 0x1C)  # #0A0F1C
LADO_DEFECTO = 80
LADO_MAX = 96
LADO_MIN = 32
SOBREMUESTREO = 2            # the view is requested at double size and downscaled by averaging
ANCHO_LINEA_B64 = 76

# A complete previous block: begin, data lines (comments) and end.
RE_BLOQUE = re.compile(
    r"^;kn_thumb_begin[^\n]*\n(?:;[^\n]*\n)*?;kn_thumb_end[^\n]*(?:\n|\Z)", re.MULTILINE)


def _log(nivel, msg):
    if Logger is not None:
        try:
            Logger.log(nivel, "AgregarMiniatura: " + msg)
        except Exception:
            pass


def rgb565(r, g, b):
    """Converts 3 8-bit channels to an RGB565 integer."""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def convertir(img, lado):
    """Downscales 'img' (QImage-like object: width(), height(), pixel(x, y) -> 0xAARRGGBB) onto a
    square side x side canvas over a dark background, keeping the aspect ratio.
    Returns the RGB565 little-endian bytes (side*side*2) or None if the image is not valid."""
    iw, ih = int(img.width()), int(img.height())
    if iw <= 0 or ih <= 0:
        return None
    if iw * ih > 4000000:  # safeguard: do not read huge images pixel by pixel
        return None

    if iw >= ih:
        nw = lado
        nh = max(1, (ih * lado + iw // 2) // iw)
    else:
        nh = lado
        nw = max(1, (iw * lado + ih // 2) // ih)
    ox, oy = (lado - nw) // 2, (lado - nh) // 2

    fr, fg, fb = FONDO
    fondo565 = rgb565(fr, fg, fb)
    pix = [fondo565] * (lado * lado)

    for dy in range(nh):
        y0 = dy * ih // nh
        y1 = max(y0 + 1, (dy + 1) * ih // nh)
        for dx in range(nw):
            x0 = dx * iw // nw
            x1 = max(x0 + 1, (dx + 1) * iw // nw)
            sr = sg = sb = 0
            n = 0
            for y in range(y0, min(y1, ih)):
                for x in range(x0, min(x1, iw)):
                    p = int(img.pixel(x, y)) & 0xFFFFFFFF
                    a = (p >> 24) & 0xFF
                    r = (p >> 16) & 0xFF
                    g = (p >> 8) & 0xFF
                    b = p & 0xFF
                    # compositing over the background: c*a + background*(1-a)
                    sr += (r * a + fr * (255 - a) + 127) // 255
                    sg += (g * a + fg * (255 - a) + 127) // 255
                    sb += (b * a + fb * (255 - a) + 127) // 255
                    n += 1
            if n:
                pix[(oy + dy) * lado + ox + dx] = rgb565(
                    (sr + n // 2) // n, (sg + n // 2) // n, (sb + n // 2) // n)

    return struct.pack("<%dH" % len(pix), *pix)


def construir_bloque(ancho, alto, crudo):
    """Text of the block ';kn_thumb_begin' ... ';kn_thumb_end' (each line ends with a newline)."""
    b64 = base64.b64encode(crudo).decode("ascii")
    lineas = [MARCA_INICIO + " %d %d rgb565" % (ancho, alto)]
    for i in range(0, len(b64), ANCHO_LINEA_B64):
        lineas.append(";" + b64[i:i + ANCHO_LINEA_B64])
    lineas.append(MARCA_FIN)
    return "\n".join(lineas) + "\n"


def quitar_bloque(texto):
    return RE_BLOQUE.sub("", texto)


def insertar_bloque(data, bloque):
    """Removes previous blocks and puts the new one after the ';FLAVOR' line at the start (or at the very beginning)."""
    data = [quitar_bloque(capa) for capa in data]
    if not data:
        return [bloque]
    primera = data[0]
    m = re.match(r"(;FLAVOR[^\n]*\n)", primera)
    if m:
        corte = m.end()
    else:
        corte = 0
    data[0] = primera[:corte] + bloque + primera[corte:]
    return data


def obtener_imagen(tam):
    """Asks Cura for the model view. Returns the image or None."""
    from cura.Snapshot import Snapshot  # [assumption] available in Cura 5.x
    return Snapshot.snapshot(width=tam, height=tam)


class AgregarMiniatura(Script):
    def getSettingDataString(self):
        return """{
            "name": "Add thumbnail (printer screen)",
            "key": "AgregarMiniatura",
            "metadata": {},
            "version": 2,
            "settings":
            {
                "lado":
                {
                    "label": "Thumbnail size (pixels)",
                    "description": "Side of the square thumbnail stored in the G-code. 80 is the recommended value; the firmware supports up to 96.",
                    "type": "int",
                    "default_value": 80,
                    "minimum_value": 32,
                    "maximum_value": 96
                }
            }
        }"""

    def execute(self, data):
        try:
            try:
                lado = int(self.getSettingValueByKey("lado"))
            except (TypeError, ValueError):
                lado = LADO_DEFECTO
            lado = min(LADO_MAX, max(LADO_MIN, lado))

            img = obtener_imagen(lado * SOBREMUESTREO)
            if img is None:
                _log(30, "Cura no entrego vista del modelo; no se agrega miniatura.")
                return data
            crudo = convertir(img, lado)
            if crudo is None:
                _log(30, "La vista del modelo no es valida; no se agrega miniatura.")
                return data
            return insertar_bloque(data, construir_bloque(lado, lado, crudo))
        except Exception as e:  # never break the G-code
            _log(30, "no se pudo crear la miniatura: %r" % (e,))
            return data
