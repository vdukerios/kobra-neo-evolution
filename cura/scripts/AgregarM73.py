# Post-processing script for Ultimaker Cura 5.x
# Adds M73 commands (percentage and remaining time) for Marlin firmware.
#
# Based on the format Cura writes:
#   - header:          ;TIME:<total seconds>
#   - end of each layer: ;TIME_ELAPSED:<accumulated seconds>
#
# The lines this script adds carry the marker ";AgregarM73" so they can be
# removed and re-added (idempotent: they are never duplicated).

import re

from ..Script import Script

MARCA = ";AgregarM73"
RE_TIME = re.compile(r"^;TIME:\s*([0-9]+(?:\.[0-9]+)?)\s*$", re.MULTILINE)
RE_ELAPSED = re.compile(r"^;TIME_ELAPSED:\s*([0-9]+(?:\.[0-9]+)?)\s*$")


def _redondear(x):
    # "Normal" rounding (0.5 rounds up), not Python's banker's rounding.
    return int(x + 0.5)


class AgregarM73(Script):
    def getSettingDataString(self):
        return """{
            "name": "Add M73 (progress and remaining time)",
            "key": "AgregarM73",
            "metadata": {},
            "version": 2,
            "settings":
            {
                "factor_tiempo":
                {
                    "label": "Time correction factor",
                    "description": "Multiplies the remaining time shown on the screen. 1.0 = Cura's estimate unchanged. Example: 1.18 if the printer takes 18% longer than Cura says.",
                    "type": "float",
                    "default_value": 1.0,
                    "minimum_value": 0.5,
                    "maximum_value": 3.0
                }
            }
        }"""

    def execute(self, data):
        try:
            factor = float(self.getSettingValueByKey("factor_tiempo"))
        except (TypeError, ValueError):
            factor = 1.0
        factor = min(3.0, max(0.5, factor))

        # 1) Remove lines from a previous pass (idempotency).
        data = [self._limpiar(capa) for capa in data]

        # 2) Look for the total time (;TIME:) in the header. If missing, do nothing.
        total = None
        for capa in data[:3]:
            m = RE_TIME.search(capa)
            if m:
                total = float(m.group(1))
                break
        if total is None or total <= 0:
            return data

        # 3) Initial M73: at the start of the start G-code (or end of header).
        inicial = "M73 P0 R%d %s\n" % (_redondear(total * factor / 60.0), MARCA)
        if len(data) > 1:
            data[1] = inicial + data[1]
        else:
            data[0] = data[0] + ("" if data[0].endswith("\n") else "\n") + inicial

        # 4) M73 at the end of each layer, right after ;TIME_ELAPSED:
        for i in range(len(data)):
            lineas = data[i].split("\n")
            for j, linea in enumerate(lineas):
                m = RE_ELAPSED.match(linea)
                if not m:
                    continue
                elapsed = min(float(m.group(1)), total)
                porcentaje = min(100, max(0, int(elapsed / total * 100)))
                restante = max(0, _redondear((total - elapsed) * factor / 60.0))
                lineas.insert(j + 1, "M73 P%d R%d %s" % (porcentaje, restante, MARCA))
                data[i] = "\n".join(lineas)
                break

        # 5) Final M73.
        final = "M73 P100 R0 " + MARCA + "\n"
        if not data[-1].endswith("\n"):
            data[-1] += "\n"
        data[-1] += final
        return data

    @staticmethod
    def _limpiar(texto):
        if MARCA not in texto:
            return texto
        lineas = [l for l in texto.split("\n") if not l.rstrip().endswith(MARCA)]
        return "\n".join(lineas)
