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

#include "../../inc/MarlinConfig.h"

#if HAS_PID_HEATING

#include "../gcode.h"
#include "../../lcd/marlinui.h"
#include "../../module/temperature.h"

#if ENABLED(EXTENSIBLE_UI)
  #include "../../lcd/extui/ui_api.h"
#endif

/**
 * M303: PID relay autotune
 *
 *  S<temperature>  Set the target temperature. (Default: 150C / 70C)
 *  E<extruder>     Extruder number to tune, or -1 for the bed. (Default: E0)
 *  C<cycles>       Number of times to repeat the procedure. (Minimum: 3, Default: 5)
 *  U<bool>         Flag to apply the result to the current PID values
 *
 * With PID_DEBUG:
 *  D               Toggle PID debugging and EXIT without further action.
 */

#if ENABLED(PID_DEBUG)
  bool pid_debug_flag = 0;
#endif

void GcodeSuite::M303() {

  #if ENABLED(PID_DEBUG)
    if (parser.seen('D')) {
      pid_debug_flag = !pid_debug_flag;
      SERIAL_ECHO_START();
      SERIAL_ECHOPGM("PID Debug ");
      serialprintln_onoff(pid_debug_flag);
      return;
    }
  #endif

  #define SI TERN(PIDTEMPBED, H_BED, H_E0)
  #define EI TERN(PIDTEMP, HOTENDS - 1, H_BED)
  const float requested_heater = parser.floatval('E', 0);
  const float requested_cycles = parser.floatval('C', 5);
  if ((parser.seen('E') && !parser.has_value()) || !isfinite(requested_heater)
      || requested_heater != floorf(requested_heater) || requested_heater < SI || requested_heater > EI) {
    SERIAL_ECHOLNPGM(STR_PID_BAD_HEATER_ID); return;
  }
  if ((parser.seen('C') && !parser.has_value()) || !isfinite(requested_cycles)
      || requested_cycles != floorf(requested_cycles) || requested_cycles < 3 || requested_cycles > 127) {
    SERIAL_ERROR_MSG("Invalid PID cycles"); return;
  }
  const heater_id_t e = heater_id_t(requested_heater);
  const float temp = parser.celsiusval('S', e < 0 ? PREHEAT_1_TEMP_BED : PREHEAT_1_TEMP_HOTEND);
  #if HAS_PID_FOR_BOTH
    const float maximum = e < 0 ? BED_MAX_TARGET : thermalManager.hotend_max_target(e);
  #elif ENABLED(PIDTEMPBED)
    const float maximum = BED_MAX_TARGET;
  #else
    const float maximum = thermalManager.hotend_max_target(e);
  #endif
  if ((parser.seen('S') && !parser.has_value()) || !thermalManager.valid_target_temperature(temp, maximum) || temp <= 0) {
    SERIAL_ERROR_MSG("Invalid PID target"); return;
  }
  const int8_t c = int8_t(requested_cycles);
  const bool u = parser.boolval('U');

  #if DISABLED(BUSY_WHILE_HEATING)
    KEEPALIVE_STATE(NOT_BUSY);
  #endif

  LCD_MESSAGEPGM(MSG_PID_AUTOTUNE);
  thermalManager.PID_autotune(temp, e, c, u);
  ui.reset_status();
}

#endif // HAS_PID_HEATING
