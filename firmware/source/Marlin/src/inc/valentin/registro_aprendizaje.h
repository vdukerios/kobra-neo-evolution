#pragma once
// Banderas y ajustes de la funcion "registro_aprendizaje". Solo este archivo se edita para esta funcion.
// Cada funcion debe poder apagarse comentando su #define principal.

// Registro de impresiones desde SD en un CSV (una linea al terminar o detenerse una impresion).
#define REGISTRO_IMPRESIONES

// Aprendizaje del tiempo restante: promedio real/estimado de las ultimas impresiones finalizadas.
#define APRENDIZAJE_TIEMPOS

// Ambas funciones necesitan SD y el contador de impresiones.
#if !defined(SDSUPPORT) || !defined(PRINTCOUNTER)
  #undef REGISTRO_IMPRESIONES
  #undef APRENDIZAJE_TIEMPOS
#endif

#if defined(REGISTRO_IMPRESIONES) || defined(APRENDIZAJE_TIEMPOS)
  #define REGISTRO_APRENDIZAJE_ACTIVO

  // Nombres de archivo en la raiz de la SD. IMPORTANTE: solo formato 8.3 (la biblioteca SD de
  // Marlin no crea nombres largos), por eso no son "registro_impresiones.csv" ni "factor_tiempo.txt".
  #define REGISTRO_ARCHIVO     "REGISTRO.CSV"    // una linea por impresion
  #define REGISTRO_PENDIENTE   "REGACTIV.TXT"    // marca "impresion en curso" (vacio = ninguna)
  #define APRENDIZAJE_ARCHIVO  "FACTOR.TXT"      // ultimos cocientes real/estimado (milesimas)

  #define APRENDIZAJE_MAX_DATOS  10     // cuantos cocientes recordar
  #define APRENDIZAJE_MIN_DATOS  3      // con menos datos el factor es 1.0
  #define APRENDIZAJE_FACTOR_MIN 0.7f
  #define APRENDIZAJE_FACTOR_MAX 2.0f
#endif
