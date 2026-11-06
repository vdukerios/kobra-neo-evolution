/**
 * feature/modos_impresion.h - Modo de impresion Completo / Express
 *
 * print_mode lo elige el usuario en la pantalla "Confirmar impresion" (menu de la SD).
 * El G-code de Cura llama M1100 y el firmware ejecuta la secuencia segun este modo.
 * Si nadie eligio nada, el modo es Completo.
 */
#pragma once

#include "../inc/MarlinConfigPre.h"

#if ENABLED(MODOS_IMPRESION)

#include <stdint.h>

#define MODO_COMPLETO 0
#define MODO_EXPRESS  1

extern uint8_t print_mode;          // 0 = Completo, 1 = Express

// Fin o aborto de la impresion (o cancelacion de la pantalla): vuelve a Completo y
// descarta la miniatura cargada.
void modos_impresion_fin();

// true mientras M1100 esta preparando la impresion (calentando, esperando, midiendo la malla).
// La proteccion por inactividad (hotend_idle) lo cuenta como "ocupado".
bool m1100_activo();

#endif // MODOS_IMPRESION
