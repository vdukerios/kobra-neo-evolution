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
#include "../../module/motion.h"
#include "../../module/stepper.h"
#include "../../module/planner.h"
#include "../../module/motion_numeric.h"

#if ENABLED(I2C_POSITION_ENCODERS)
  #include "../../feature/encoder_i2c.h"
#endif

/**
 * G92: Set current position to given X Y Z E
 */
void GcodeSuite::G92() {

  bool sync_E = false, sync_XYZ = false;

  #if ENABLED(USE_GCODE_SUBCODES)
    const uint8_t subcode_G92 = parser.subcode;
  #else
    constexpr uint8_t subcode_G92 = 0;
  #endif

  xyze_pos_t candidate_position = current_position;
  LOOP_XYZE(i) if (parser.seenval(axis_codes[i]) && (i == E_AXIS || subcode_G92 == 9 || !HAS_POSITION_SHIFT))
    candidate_position[i] = parser.value_axis_units((AxisEnum)i);
  xyze_pos_t candidate_machine = candidate_position;
  TERN_(HAS_POSITION_MODIFIERS, planner.apply_modifiers(candidate_machine, true));
  if (!kne_numeric::position_valid(candidate_machine, planner.settings, active_extruder)) {
    SERIAL_ERROR_MSG("Unrepresentable position"); return;
  }
  // Validate all requested positions and workspace sums before changing any axis.
  LOOP_XYZE(i) if (parser.seenval(axis_codes[i])) {
    const float value = parser.value_axis_units((AxisEnum)i),
                native = (i == E_AXIS || subcode_G92 == 9) ? value : LOGICAL_TO_NATIVE(value, i);
    if (!kne_numeric::axis_value_valid(native, planner.settings.axis_steps_per_mm[i == E_AXIS ? E_AXIS_N(active_extruder) : i])) {
      SERIAL_ERROR_MSG("Unrepresentable position"); return;
    }
    #if HAS_POSITION_SHIFT
      if (i != E_AXIS && subcode_G92 == 0) {
        const double shift = double(position_shift[i]) + double(native) - current_position[i],
                     workspace = double(home_offset[i]) + shift;
        if (!isfinite(shift) || fabs(shift) > FLT_MAX || !isfinite(workspace) || fabs(workspace) > FLT_MAX
          || !kne_numeric::axis_value_valid(float(workspace), planner.settings.axis_steps_per_mm[i])) {
          SERIAL_ERROR_MSG("Unrepresentable workspace"); return;
        }
      }
    #endif
  }
  switch (subcode_G92) {
    default: break;
    #if ENABLED(CNC_COORDINATE_SYSTEMS)
      case 1: {
        // Zero the G92 values and restore current position
        #if !IS_SCARA
          LOOP_XYZ(i) if (position_shift[i]) {
            position_shift[i] = 0;
            update_workspace_offset((AxisEnum)i);
          }
        #endif // Not SCARA
      } return;
    #endif
    #if ENABLED(POWER_LOSS_RECOVERY)
      case 9: {
        LOOP_XYZE(i) {
          if (parser.seenval(axis_codes[i])) {
            current_position[i] = parser.value_axis_units((AxisEnum)i);
            if (i == E_AXIS) sync_E = true; else sync_XYZ = true;
          }
        }
      } break;
    #endif
    case 0: {
      LOOP_XYZE(i) {
        if (parser.seenval(axis_codes[i])) {
          const float l = parser.value_axis_units((AxisEnum)i),
                      v = i == E_AXIS ? l : LOGICAL_TO_NATIVE(l, i),
                      d = v - current_position[i];
          if (!NEAR_ZERO(d)) {
            #if IS_SCARA || !HAS_POSITION_SHIFT
              if (i == E_AXIS) sync_E = true; else sync_XYZ = true;
              current_position[i] = v;        // Without workspaces revert to Marlin 1.0 behavior
            #elif HAS_POSITION_SHIFT
              if (i == E_AXIS) {
                sync_E = true;
                current_position.e = v;       // When using coordinate spaces, only E is set directly
              }
              else {
                position_shift[i] += d;       // Other axes simply offset the coordinate space
                update_workspace_offset((AxisEnum)i);
              }
            #endif
          }
        }
      }
    } break;
  }

  #if ENABLED(CNC_COORDINATE_SYSTEMS)
    // Apply workspace offset to the active coordinate system
    if (WITHIN(active_coordinate_system, 0, MAX_COORDINATE_SYSTEMS - 1))
      coordinate_system[active_coordinate_system] = position_shift;
  #endif

  if    (sync_XYZ) sync_plan_position();
  else if (sync_E) sync_plan_position_e();

  #if DISABLED(DIRECT_STEPPING)
    report_current_position();
  #endif
}
