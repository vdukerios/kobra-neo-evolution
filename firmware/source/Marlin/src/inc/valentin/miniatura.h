#pragma once
// Banderas y ajustes de la funcion "miniatura". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.

// Miniatura del diseno guardada en la cabecera del G-code (formato kn_thumb, RGB565 en base64).
// Para apagar la funcion, comentar la siguiente linea.
#define MINIATURA_GCODE

#if ENABLED(MINIATURA_GCODE)
  #define MINIATURA_LADO_MAX      80      // Lado maximo (px). El bufer estatico ocupa LADO_MAX^2 * 2 bytes (96 -> 18 KB de RAM).
  #define MINIATURA_BUSQUEDA_MAX  24576   // Bytes iniciales del archivo donde se busca el bloque ";kn_thumb_begin".
#endif
