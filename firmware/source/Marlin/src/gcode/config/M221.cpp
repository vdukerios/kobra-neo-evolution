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

#if EXTRUDERS

/**
 * M221: Set extrusion percentage (M221 T0 S95)
 */
void GcodeSuite::M221() {

  uint32_t target = active_extruder;
  if (parser.seen('T') && (!parser.has_value() || !parser.value_ulong_checked(target) || target >= EXTRUDERS)) {
    SERIAL_ERROR_MSG("Invalid extruder"); return;
  }
  const bool has_flow = parser.seen('S');
  uint32_t flow = 0;
  if (has_flow && (!parser.has_value() || !parser.value_ulong_checked(flow) || flow > 999)) {
    SERIAL_ERROR_MSG("Invalid flow percentage (0..999)"); return;
  }
  const int8_t target_extruder = int8_t(target);
  // S0 is deliberate: printing continues with zero extrusion flow.
  if (has_flow)
    planner.set_flow(target_extruder, int16_t(flow));
  else {
    SERIAL_ECHO_START();
    SERIAL_CHAR('E', '0' + target_extruder);
    SERIAL_ECHOPAIR(" Flow: ", planner.flow_percentage[target_extruder]);
    SERIAL_CHAR('%');
    SERIAL_EOL();
  }
}

#endif // EXTRUDERS
