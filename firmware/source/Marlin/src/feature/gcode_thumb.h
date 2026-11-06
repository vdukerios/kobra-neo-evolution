/**
 * feature/gcode_thumb.h - Miniatura del diseno guardada en la cabecera del G-code
 *
 * Formato (comentarios del G-code, al inicio del archivo):
 *   ;kn_thumb_begin <ancho> <alto> rgb565
 *   ;<base64 de 76 caracteres>   (pixeles RGB565 little-endian, fila por fila)
 *   ;kn_thumb_end
 *
 * Uso: load() solo funciona sin impresion en curso y sin archivo abierto en la SD.
 * 'nombre' es la ruta DOS (nombre corto, como card.filename); relativa a la carpeta
 * actual de la SD, o absoluta si empieza por '/'.
 * Si la funcion esta apagada (MINIATURA_GCODE sin definir) las funciones son stubs
 * que siempre fallan, para que el codigo que las usa compile igual.
 */
#pragma once

#include "../inc/MarlinConfigPre.h"

#if ENABLED(MINIATURA_GCODE)

namespace GcodeThumb {
  bool load(const char *nombre_archivo_en_sd);  // true si dejo una miniatura valida lista
  bool ready();                                 // hay una miniatura cargada
  uint8_t width();                              // 0 si no hay miniatura
  uint8_t height();                             // 0 si no hay miniatura
  const uint16_t* pixels();                     // width()*height() pixeles RGB565, fila por fila; nullptr si no hay
  void unload();                                // descarta la miniatura
}

#else

namespace GcodeThumb {
  inline bool load(const char *) { return false; }
  inline bool ready() { return false; }
  inline uint8_t width() { return 0; }
  inline uint8_t height() { return 0; }
  inline const uint16_t* pixels() { return nullptr; }
  inline void unload() {}
}

#endif
