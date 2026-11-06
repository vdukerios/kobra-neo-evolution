/**
 * M1100 - Preparacion de la impresion segun el modo (Completo / Express)
 *
 *   M1100 S<temp cama> T<temp boquilla> [W<minutos de espera termica>]
 *
 * Modo COMPLETO: G28 > M140 S > M190 R > espera W min (cancelable) > G29 > M104 S > M109 R
 * Modo EXPRESS : G28 (solo si no esta en origen) > M140 S + M104 S > M420 S1 > M190 R > M109 R
 *
 * El modo lo da print_mode (por defecto Completo). Todo paso bloqueante se corta con la
 * parada normal (ui.abort_print activa card.flag.abort_sd_printing); si se corta,
 * se apagan los calentadores.
 */

#include "../../../inc/MarlinConfig.h"

#if ENABLED(MODOS_IMPRESION)

#include "../../gcode.h"
#include "../../../feature/modos_impresion.h"
#include "../../../feature/gcode_thumb.h"
#include "../../../module/temperature.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../sd/cardreader.h"
#include "../../../lcd/marlinui.h"
#include "../../../MarlinCore.h"   // idle(), wait_for_heatup, marlin_state
#include "../../../feature/registro_aprendizaje.h"
#include "../../../feature/bedlevel/bedlevel.h"   // leveling_is_valid()

extern bool g29_fallo;   // definida en G29.cpp: la ultima medicion de la malla fallo

uint8_t print_mode = MODO_COMPLETO;

static bool m1100_en_curso = false;   // true durante la preparacion
bool m1100_activo() { return m1100_en_curso; }

void modos_impresion_fin() {
  print_mode = MODO_COMPLETO;
  GcodeThumb::unload();
}

// Hay que parar: parada normal pedida por el usuario, o la impresora esta detenida por error.
static bool mp_parar() {
  return card.flag.abort_sd_printing || marlin_state == MF_KILLED;
}

// Ejecuta un G-code fijo si no hay parada. Devuelve false si hay que parar (antes o despues).
static bool mp_cmd_P(PGM_P const cmd) {
  if (mp_parar()) return false;
  gcode.process_subcommands_now_P(cmd);
  return !mp_parar();
}

// Ejecuta "<prefijo><n>" (por ejemplo "M190 R60"). Numero entero sin signo, sin sprintf.
static bool mp_cmd_num_P(PGM_P const prefijo, uint16_t n) {
  if (mp_parar()) return false;
  char buf[16];
  strncpy_P(buf, prefijo, 8); buf[8] = '\0';
  char tmp[6]; uint8_t k = 0;
  do { tmp[k++] = '0' + (n % 10); n /= 10; } while (n && k < 5);
  char *p = buf + strlen(buf);
  while (k) *p++ = tmp[--k];
  *p = '\0';
  gcode.process_subcommands_now(buf);
  return !mp_parar();
}

// Espera con el cabezal quieto; las temperaturas ya objetivo siguen mantenidas por idle().
static bool mp_esperar(const float minutos) {
  if (minutos <= 0) return true;
  ui.set_status_P(GET_TEXT(MSG_KNE_THERMAL_WAIT));
  const millis_t fin = millis() + (millis_t)(minutos * 60000.0f);
  while (PENDING(millis(), fin)) {
    if (mp_parar()) return false;
    idle();
  }
  return !mp_parar();
}

// Cancelacion: nada de calentadores encendidos.
static void mp_cancelar() {
  wait_for_heatup = false;
  thermalManager.disable_all_heaters();
  thermalManager.zero_fan_speeds();
  card.abortFilePrintSoon();   // la preparacion fallo o se cancelo: NO seguir imprimiendo el archivo
  if (!card.isFileOpen()) SERIAL_ECHOLNPGM("//action:cancel");   // M1100 desde host/USB: avisar al host para que deje de enviar
  ui.set_status_P(GET_TEXT(MSG_KNE_PREP_CANCELLED));
}

void GcodeSuite::M1100() {
  // Leer parametros ANTES de ejecutar subcomandos (el analizador se reutiliza)
  int16_t cama   = (int16_t)parser.floatval('S', 0);
  int16_t boq    = (int16_t)parser.floatval('T', 0);
  float   espera = parser.floatval('W', 0);
  if (cama < 0) cama = 0;
  if (boq < 0) boq = 0;
  if (espera < 0) espera = 0;
  if (espera > 60) espera = 60;

  // Sin archivo de la SD abierto (M1100 lanzado por host/USB, o sin pasar por la pantalla de confirmacion)
  // el modo es siempre Completo: se limpia un Express o una miniatura que hubieran quedado pegados.
  const bool con_archivo = card.isFileOpen();
  if (!con_archivo) modos_impresion_fin();

  // Express solo si hay una malla valida guardada; si no, se hace la medicion completa (Completo)
  const bool express = con_archivo && (print_mode == MODO_EXPRESS) && leveling_is_valid();
  bool ok = true;
  m1100_en_curso = true;

  if (express) {
    ui.set_status_P(GET_TEXT(MSG_KNE_STATUS_EXPRESS));
    if (!all_axes_trusted()) ok = mp_cmd_P(PSTR("G28"));
    if (ok) ok = mp_cmd_num_P(PSTR("M140 S"), cama);
    if (ok) ok = mp_cmd_num_P(PSTR("M104 S"), boq);
    if (ok) ok = mp_cmd_P(PSTR("M420 S1"));            // malla ya guardada
    if (ok && !planner.leveling_active) ok = false;   // no imprimir si no se pudo activar
    if (ok && cama) ok = mp_cmd_num_P(PSTR("M190 R"), cama);
    if (ok && boq)  ok = mp_cmd_num_P(PSTR("M109 R"), boq);
  }
  else {
    ui.set_status_P(GET_TEXT(MSG_KNE_STATUS_FULL));
    ok = mp_cmd_P(PSTR("G28"));
    if (ok) ok = mp_cmd_num_P(PSTR("M140 S"), cama);
    if (ok && cama) ok = mp_cmd_num_P(PSTR("M190 R"), cama);
    if (ok) ok = mp_esperar(espera);
    if (ok) ok = mp_cmd_P(PSTR("G29"));                // malla en caliente
    if (ok && (g29_fallo || !leveling_is_valid() || !planner.leveling_active)) ok = false;
    // G29 termina dejando la cama en otra temperatura: volver a la pedida
    if (ok) ok = mp_cmd_num_P(PSTR("M140 S"), cama);
    if (ok && cama) ok = mp_cmd_num_P(PSTR("M190 R"), cama);
    if (ok) ok = mp_cmd_num_P(PSTR("M104 S"), boq);
    if (ok && boq) ok = mp_cmd_num_P(PSTR("M109 R"), boq);
  }

  m1100_en_curso = false;

  if (!ok) mp_cancelar();
  else {
    #if ENABLED(APRENDIZAJE_TIEMPOS)
      registro_prep_fin();   // el tiempo de preparacion no entra en el factor aprendido
    #endif
    ui.set_status_P(GET_TEXT(MSG_KNE_PRINTING));
  }
}

#endif // MODOS_IMPRESION
