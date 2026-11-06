#pragma once
/**
 * feature/registro_aprendizaje.h
 * Registro de impresiones en la SD y aprendizaje del tiempo restante (proyecto Valentin).
 * Ajustes y banderas: src/inc/valentin/registro_aprendizaje.h
 */

#include "../inc/MarlinConfigPre.h"

#include <stdint.h>

#ifdef REGISTRO_APRENDIZAJE_ACTIVO

// Llamadas desde el firmware (ganchos)
void registro_inicio();                       // Al iniciar una impresion desde SD (PrintCounter::start)
void registro_fin(const uint32_t real_s);     // Al terminar/detener (PrintCounter::stop), duracion real en segundos
void registro_motivo_finalizada();            // M1001: la impresion llego al final del archivo
void registro_marca_filamento();              // El sensor detecto fin de filamento
void registro_prep_fin();                      // M1100 termino la preparacion: lo anterior no cuenta para el aprendizaje
void registro_estimado_m73(const uint32_t minutos); // M73 R: el primero tras iniciar es el estimado inicial
void registro_arranque(const bool corte_luz); // Al montar la SD por primera vez (powerloss.check)

#ifdef APRENDIZAJE_TIEMPOS
  float tiempo_factor();       // Factor aprendido (1.0 si hay pocos datos)
  uint8_t tiempo_n_datos();    // Cuantos cocientes hay guardados (0..10)
#endif

#ifdef REGISTRO_IMPRESIONES
  uint8_t registro_cargar_ultimas();           // Lee las ultimas 5 lineas del CSV (y el factor); devuelve cuantas
  const char* registro_linea(const uint8_t i); // Linea resumida i (0 = la mas antigua de las cargadas)
#endif

#endif // REGISTRO_APRENDIZAJE_ACTIVO
