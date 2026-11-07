/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * Hotend Idle Timeout
 * Prevent filament in the nozzle from charring and causing a critical jam.
 */

#include "../inc/MarlinConfig.h"

#if ENABLED(HOTEND_IDLE_TIMEOUT)

#include "hotend_idle.h"
#include "../gcode/gcode.h"

#include "../module/temperature.h"
#include "../module/motion.h"
#include "../module/planner.h"
#include "../lcd/marlinui.h"
#include "../MarlinCore.h"   // printingIsActive(), wait_for_heatup

#if ENABLED(MODOS_IMPRESION)
  #include "modos_impresion.h"   // m1100_activo()
#endif

extern HotendIdleProtection hotend_idle;

millis_t HotendIdleProtection::next_protect_ms = 0;

// Pausa de impresion sin estacionar (por ejemplo M25 desde OctoPrint): el temporizador de inactividad
// podria apagar boquilla y cama y luego la reanudacion (M24) imprimiria sin calor.
// Solucion: (1) durante una pausa el plazo es el triple; (2) si aun asi se agota, se recuerdan las
// temperaturas y, al reanudar, se vuelve a calentar y se espera antes de seguir.
static bool    pausa_apagada = false;   // se apagaron calentadores con la impresion en pausa
static int16_t pausa_boq[HOTENDS];      // objetivos de boquilla previos
static int16_t pausa_cama = 0;          // objetivo de cama previo

static bool resume_in_progress = false;

bool HotendIdleProtection::resume() {
  if (resume_in_progress) return false;
  if (!pausa_apagada) return true;
  if (!printingIsPaused() && !printingIsActive()) { pausa_apagada = false; return true; }
  resume_in_progress = true; // block reentry during the waits' idle service
  ui.set_status_P(GET_TEXT(MSG_KNE_REHEAT_RESUME));
  #if HAS_HEATED_BED
    thermalManager.setTargetBed(pausa_cama);
  #endif
  HOTEND_LOOP() thermalManager.setTargetHotend(pausa_boq[e], e);
  bool ready = true;
  #if HAS_HEATED_BED
    if (pausa_cama > 0) ready = thermalManager.wait_for_bed(false);
    ready = ready && thermalManager.degTargetBed() == pausa_cama;
  #endif
  HOTEND_LOOP() {
    if (!ready || (!printingIsPaused() && !printingIsActive())) { ready = false; break; }
    if (pausa_boq[e] > 0) ready = thermalManager.wait_for_hotend(e, false);
    if (thermalManager.degTargetHotend(e) != pausa_boq[e]) ready = false;
  }
  ready = ready && (printingIsPaused() || printingIsActive());
  if (!ready) thermalManager.disable_all_heaters();
  else pausa_apagada = false;
  resume_in_progress = false;
  return ready; // caller must keep the job paused on cancellation
}

void HotendIdleProtection::check_hotends(const millis_t &ms) {
  bool do_prot = false;
  // "Ocupada" incluye imprimir (M1100, esperas de temperatura, G29) y esperar calentamiento:
  // si no, el modo Express se apagaria mientras la cama sube a temperatura.
  const bool busy = TERN0(HAS_RESUME_CONTINUE, wait_for_user) || planner.has_blocks_queued() || wait_for_heatup || resume_in_progress || printingIsActive()
                   || TERN0(MODOS_IMPRESION, m1100_activo());   // M1100 en curso (host o SD): no apagar la cama a mitad de la espera termica
  if (!busy) {
    HOTEND_LOOP() {
      if (thermalManager.degHotend(e) >= (HOTEND_IDLE_MIN_TRIGGER)) { do_prot = true; break; }
    }
    #if HAS_HEATED_BED
      if (!do_prot && thermalManager.degTargetBed() > 0) do_prot = true;   // cama encendida sin uso tambien cuenta
    #endif
  }
  if (bool(next_protect_ms) != do_prot)
    next_protect_ms = do_prot ? ms + (printingIsPaused() ? hp_interval * 3 : hp_interval) : 0;   // en pausa: triple de plazo
}

void HotendIdleProtection::check_e_motion(const millis_t &ms) {
  static float old_e_position = 0;
  if (old_e_position != current_position.e) {
    old_e_position = current_position.e;          // Track filament motion
    if (next_protect_ms)                          // If some heater is on then...
      next_protect_ms = ms + hp_interval;         // ...delay the timeout till later
  }
}

void HotendIdleProtection::check() {
  const millis_t ms = millis();                   // Shared millis

  // Reheat only on an explicit resume request, before starting the job.
  if (!resume_in_progress && !printingIsPaused() && !printingIsActive()) pausa_apagada = false;

  check_hotends(ms);                              // Any hotends need protection?
  check_e_motion(ms);                             // Motion will protect them

  // Hot and not moving for too long...
  if (next_protect_ms && ELAPSED(ms, next_protect_ms))
    timed_out();
}

// Lower (but don't raise) hotend / bed temperatures
void HotendIdleProtection::timed_out() {
  next_protect_ms = 0;
  SERIAL_ECHOLNPGM("Hotend Idle Timeout");
  ui.set_status_P(GET_TEXT(MSG_KNE_IDLE_OFF));
  if (printingIsPaused()) {               // recordar los objetivos para recalentar al reanudar
    HOTEND_LOOP() pausa_boq[e] = thermalManager.degTargetHotend(e);
    #if HAS_HEATED_BED
      pausa_cama = thermalManager.degTargetBed();
    #endif
    pausa_apagada = true;
  }
  HOTEND_LOOP() {
    if ((HOTEND_IDLE_NOZZLE_TARGET) < thermalManager.degTargetHotend(e))
      thermalManager.setTargetHotend(HOTEND_IDLE_NOZZLE_TARGET, e);
  }
  #if HAS_HEATED_BED
    if ((HOTEND_IDLE_BED_TARGET) < thermalManager.degTargetBed())
      thermalManager.setTargetBed(HOTEND_IDLE_BED_TARGET);
  #endif
}

#endif // HOTEND_IDLE_TIMEOUT
