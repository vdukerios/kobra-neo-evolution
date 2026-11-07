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

#include "../gcode.h"
#include "../../module/planner.h"
#include "../../module/motion_numeric.h"

void report_M92(const bool echo=true, const int8_t e=-1) {
  if (echo) SERIAL_ECHO_START(); else SERIAL_CHAR(' ');
  SERIAL_ECHOPAIR_P(PSTR(" M92 X"), LINEAR_UNIT(planner.settings.axis_steps_per_mm[X_AXIS]),
                          SP_Y_STR, LINEAR_UNIT(planner.settings.axis_steps_per_mm[Y_AXIS]),
                          SP_Z_STR, LINEAR_UNIT(planner.settings.axis_steps_per_mm[Z_AXIS]));
  #if DISABLED(DISTINCT_E_FACTORS)
    SERIAL_ECHOPAIR_P(SP_E_STR, VOLUMETRIC_UNIT(planner.settings.axis_steps_per_mm[E_AXIS]));
  #endif
  SERIAL_EOL();

  #if ENABLED(DISTINCT_E_FACTORS)
    LOOP_L_N(i, E_STEPPERS) {
      if (e >= 0 && i != e) continue;
      if (echo) SERIAL_ECHO_START(); else SERIAL_CHAR(' ');
      SERIAL_ECHOLNPAIR_P(PSTR(" M92 T"), (int)i,
                        SP_E_STR, VOLUMETRIC_UNIT(planner.settings.axis_steps_per_mm[E_AXIS_N(i)]));
    }
  #endif

  UNUSED_E(e);
}

/**
 * M92: Set axis steps-per-unit for one or more axes, X, Y, Z, and E.
 *      (Follows the same syntax as G92)
 *
 *      With multiple extruders use T to specify which one.
 *
 *      If no argument is given print the current values.
 *
 *    With MAGIC_NUMBERS_GCODE:
 *      Use 'H' and/or 'L' to get ideal layer-height information.
 *      'H' specifies micro-steps to use. We guess if it's not supplied.
 *      'L' specifies a desired layer height. Nearest good heights are shown.
 */
void GcodeSuite::M92() {

  const int8_t target_extruder = get_target_extruder_from_command();
  if (target_extruder < 0) return;

  // No arguments? Show M92 report.
  if (!parser.seen("XYZE"
    #if ENABLED(MAGIC_NUMBERS_GCODE)
      "HL"
    #endif
  )) return report_M92(true, target_extruder);

  planner_settings_t candidate = planner.settings;
  float e_scaling = 1;
  LOOP_XYZE(i) if (parser.seenval(axis_codes[i])) {
    const uint8_t axis = i == E_AXIS ? E_AXIS_N(target_extruder) : i;
    const float value = parser.value_per_axis_units((AxisEnum)axis);
    if (!kne_numeric::positive(value)) { SERIAL_ERROR_MSG("Invalid steps per unit"); return; }
    candidate.axis_steps_per_mm[axis] = value;
    if (i == E_AXIS && value < 20) {
      e_scaling = planner.settings.axis_steps_per_mm[axis] / value;
      candidate.max_feedrate_mm_s[axis] *= e_scaling;
    }
  }
  if (!kne_numeric::settings_valid(candidate) || !kne_numeric::position_valid(current_position, candidate, active_extruder)) {
    SERIAL_ERROR_MSG("Unrepresentable steps per unit"); return;
  }
  #if HAS_CLASSIC_JERK && HAS_CLASSIC_E_JERK
    if (!isfinite(planner.max_jerk.e * e_scaling)) { SERIAL_ERROR_MSG("Invalid extrusion jerk"); return; }
    planner.max_jerk.e *= e_scaling;
  #endif
  planner.settings = candidate;
  planner.refresh_positioning();

  #if ENABLED(MAGIC_NUMBERS_GCODE)
    #ifndef Z_MICROSTEPS
      #define Z_MICROSTEPS 16
    #endif
    const float wanted = parser.floatval('L');
    if (parser.seen('H') || wanted) {
      const uint16_t argH = parser.ushortval('H'),
                     micro_steps = argH ?: Z_MICROSTEPS;
      const float z_full_step_mm = micro_steps * planner.steps_to_mm[Z_AXIS];
      SERIAL_ECHO_START();
      SERIAL_ECHOPAIR("{ micro_steps:", micro_steps, ", z_full_step_mm:", z_full_step_mm);
      if (wanted) {
        const float best = uint16_t(wanted / z_full_step_mm) * z_full_step_mm;
        SERIAL_ECHOPAIR(", best:[", best);
        if (best != wanted) { SERIAL_CHAR(','); SERIAL_DECIMAL(best + z_full_step_mm); }
        SERIAL_CHAR(']');
      }
      SERIAL_ECHOLNPGM(" }");
    }
  #endif
}
