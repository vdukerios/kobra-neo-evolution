# Stub minimo del modulo Script de Cura (solo para pruebas, sin Cura instalado).
import json


class Script:
    def __init__(self):
        self._valores = {}

    def getSettingDataString(self):
        raise NotImplementedError

    def getSettingValueByKey(self, key):
        if key in self._valores:
            return self._valores[key]
        # Valor por defecto tomado del JSON de ajustes, como hace Cura.
        return json.loads(self.getSettingDataString())["settings"][key]["default_value"]
