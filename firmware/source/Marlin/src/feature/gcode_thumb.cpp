/**
 * feature/gcode_thumb.cpp - Lee la miniatura (formato kn_thumb) de la cabecera de un G-code en la SD.
 * Sin asignacion dinamica: un bufer estatico de MINIATURA_LADO_MAX^2 pixeles RGB565.
 * Lee el archivo por trozos de 128 bytes y decodifica base64 al vuelo.
 */

#include "../inc/MarlinConfigPre.h"

#include "gcode_thumb.h"

#if ENABLED(MINIATURA_GCODE)

#include "../inc/MarlinConfig.h"

#if ENABLED(SDSUPPORT)
  #include "../sd/cardreader.h"
  #include "../module/printcounter.h"
  #include "../MarlinCore.h"
#endif

#include <string.h>

namespace GcodeThumb {

  static uint16_t datos[MINIATURA_LADO_MAX * MINIATURA_LADO_MAX];
  static uint8_t ancho = 0, alto = 0;
  static bool lista = false;

  bool ready() { return lista; }
  uint8_t width() { return lista ? ancho : 0; }
  uint8_t height() { return lista ? alto : 0; }
  const uint16_t* pixels() { return lista ? datos : nullptr; }
  void unload() { lista = false; ancho = alto = 0; }

  #if ENABLED(SDSUPPORT)

  #define KN_BUSCANDO  0
  #define KN_LEYENDO   1
  #define KN_TERMINADO 2
  #define KN_FALLO     3

  #define KN_LINEA_MAX 96   // Las lineas del bloque miden ~77; las mas largas se ignoran (o invalidan el bloque).
  #define KN_TROZO     128  // Bytes por lectura de la SD.

  struct Estado {
    uint8_t  fase;
    uint8_t  w, h;
    uint32_t total;     // bytes de pixeles esperados (w*h*2)
    uint32_t escritos;  // bytes de pixeles ya decodificados
    uint32_t inicio;    // bytes leidos del archivo cuando se vio ";kn_thumb_begin"
    uint32_t acc;       // acumulador base64
    uint8_t  nbits;     // bits validos en el acumulador
    bool     relleno;   // ya se vio '=' (fin de los datos)
  };

  static int8_t valor_b64(const char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  }

  // Lee un entero decimal (1..255) saltando espacios previos; avanza el puntero. Devuelve 0 si no es valido.
  static uint8_t leer_lado(const char *&p) {
    while (*p == ' ') p++;
    if (*p < '0' || *p > '9') return 0;
    uint16_t v = 0;
    while (*p >= '0' && *p <= '9') {
      v = v * 10 + (*p - '0');
      if (v > 255) return 0;
      p++;
    }
    return (uint8_t)v;
  }

  // Guarda un byte de pixel (little-endian: primero el bajo). false si se excede el tamano esperado.
  static bool poner_byte(Estado &e, const uint8_t b) {
    if (e.escritos >= e.total) return false;
    if (e.escritos & 1) datos[e.escritos >> 1] |= (uint16_t)((uint16_t)b << 8);
    else                datos[e.escritos >> 1] = b;
    e.escritos++;
    return true;
  }

  // Procesa una linea completa (sin \r ni \n, terminada en 0).
  static void procesar_linea(const char *l, Estado &e) {
    if (e.fase == KN_BUSCANDO) {
      if (strncmp(l, ";kn_thumb_begin", 15) != 0) return;
      const char *p = l + 15;
      e.w = leer_lado(p);
      e.h = leer_lado(p);
      while (*p == ' ') p++;
      if (e.w == 0 || e.h == 0 || e.w > MINIATURA_LADO_MAX || e.h > MINIATURA_LADO_MAX || strcmp(p, "rgb565") != 0) {
        e.fase = KN_FALLO;
        return;
      }
      e.total = (uint32_t)e.w * e.h * 2;
      e.escritos = 0; e.acc = 0; e.nbits = 0; e.relleno = false;
      e.fase = KN_LEYENDO;
      return;
    }

    // KN_LEYENDO
    if (strncmp(l, ";kn_thumb_end", 13) == 0) {
      e.fase = (e.escritos == e.total) ? KN_TERMINADO : KN_FALLO;
      return;
    }
    if (l[0] != ';') { e.fase = KN_FALLO; return; }  // linea ajena dentro del bloque

    for (const char *p = l + 1; *p; p++) {
      if (*p == ' ') continue;
      if (*p == '=') { e.relleno = true; continue; }
      const int8_t v = valor_b64(*p);
      if (v < 0 || e.relleno) { e.fase = KN_FALLO; return; }
      e.acc = ((e.acc << 6) | (uint8_t)v) & 0xFFFFFF;
      e.nbits += 6;
      if (e.nbits >= 8) {
        e.nbits -= 8;
        if (!poner_byte(e, (uint8_t)((e.acc >> e.nbits) & 0xFF))) { e.fase = KN_FALLO; return; }
      }
    }
  }

  bool load(const char *nombre) {
    unload();

    if (!nombre || !nombre[0]) return false;
    if (!card.isMounted()) return false;

    // No interferir con una impresion: ni archivo abierto (imprimiendo, en pausa o guardando),
    // ni temporizador de trabajo, ni impresion activa desde el host.
    if (card.isFileOpen() || card.isPrinting() || printingIsActive()) return false;
    if (print_job_timer.isRunning() || print_job_timer.isPaused()) return false;

    SdFile *dir = nullptr;
    const char * const fname = card.diveToFile(false, dir, nombre);
    if (!fname || !dir) return false;

    SdFile f;
    if (!f.open(dir, fname, O_READ)) return false;

    Estado e;
    e.fase = KN_BUSCANDO;
    e.w = e.h = 0; e.total = e.escritos = e.inicio = e.acc = 0; e.nbits = 0; e.relleno = false;

    char linea[KN_LINEA_MAX];
    uint8_t ln = 0;
    bool larga = false;
    uint8_t trozo[KN_TROZO];
    uint32_t leidos = 0;

    while (e.fase == KN_BUSCANDO || e.fase == KN_LEYENDO) {
      if (e.fase == KN_BUSCANDO && leidos >= (uint32_t)(MINIATURA_BUSQUEDA_MAX)) break;
      if (e.fase == KN_LEYENDO && leidos - e.inicio > e.total * 2 + 256) break;  // base64 mide ~1,37x

      const int16_t n = f.read(trozo, KN_TROZO);
      if (n <= 0) break;  // fin de archivo o error de lectura

      for (int16_t i = 0; i < n && (e.fase == KN_BUSCANDO || e.fase == KN_LEYENDO); i++) {
        const char c = (char)trozo[i];
        if (c == '\r') continue;
        if (c == '\n') {
          if (!larga) {
            linea[ln] = '\0';
            const uint8_t antes = e.fase;
            procesar_linea(linea, e);
            if (antes == KN_BUSCANDO && e.fase == KN_LEYENDO) e.inicio = leidos + i;
          }
          else if (e.fase == KN_LEYENDO) e.fase = KN_FALLO;
          ln = 0; larga = false;
        }
        else if (ln < KN_LINEA_MAX - 1) linea[ln++] = c;
        else larga = true;
      }
      leidos += n;
    }

    // Ultima linea sin salto de linea al final del archivo (p. ej. ";kn_thumb_end").
    if ((e.fase == KN_BUSCANDO || e.fase == KN_LEYENDO) && ln > 0 && !larga) {
      linea[ln] = '\0';
      procesar_linea(linea, e);
    }

    f.close();

    if (e.fase != KN_TERMINADO) return false;
    ancho = e.w; alto = e.h; lista = true;
    return true;
  }

  #else // !SDSUPPORT

  bool load(const char *) { unload(); return false; }

  #endif

} // namespace GcodeThumb

#endif // MINIATURA_GCODE
