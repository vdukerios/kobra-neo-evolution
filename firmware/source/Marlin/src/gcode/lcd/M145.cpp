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

#if PREHEAT_COUNT

#include "../gcode.h"
#include "../../lcd/marlinui.h"

/**
 * M145: Set the heatup state for a material in the LCD menu
 *
 *   S<material>
 *   H<hotend temp>
 *   B<bed temp>
 *   F<fan speed>
 */
void GcodeSuite::M145() {
  uint32_t material = 0;
  if (parser.seen('S') && (!parser.has_value() || !parser.value_ulong_checked(material))) {
    SERIAL_ERROR_MSG(STR_ERR_MATERIAL_INDEX); return;
  }
  if (material >= PREHEAT_COUNT) { SERIAL_ERROR_MSG(STR_ERR_MATERIAL_INDEX); return; }
  preheat_t next = ui.material_preset[material];
  #if HAS_HOTEND
    if (parser.seen('H')) {
      uint32_t requested;
      if (!parser.has_value() || !parser.value_ulong_checked(requested)
          || requested < EXTRUDE_MINTEMP || requested > (HEATER_0_MAXTEMP) - (HOTEND_OVERSHOOT)) {
        SERIAL_ERROR_MSG("Invalid material hotend temperature"); return;
      }
      next.hotend_temp = int16_t(requested);
    }
  #endif
  #if HAS_HEATED_BED
    if (parser.seen('B')) {
      uint32_t requested;
      if (!parser.has_value() || !parser.value_ulong_checked(requested)
          || requested < BED_MINTEMP || requested > BED_MAX_TARGET) {
        SERIAL_ERROR_MSG("Invalid material bed temperature"); return;
      }
      next.bed_temp = int16_t(requested);
    }
  #endif
  #if HAS_FAN
    if (parser.seen('F')) {
      uint32_t requested;
      if (!parser.has_value() || !parser.value_ulong_checked(requested) || requested > 255) {
        SERIAL_ERROR_MSG("Invalid material fan speed"); return;
      }
      next.fan_speed = uint8_t(requested);
    }
  #endif
  ui.material_preset[material] = next; // Commit only after every supplied field is valid.

}

#endif // PREHEAT_COUNT
