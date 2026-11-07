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
 * M421.cpp - Auto Bed Leveling
 */

#include "../../../inc/MarlinConfig.h"

#if ENABLED(AUTO_BED_LEVELING_BILINEAR)

#include "../../gcode.h"
#include "../../../feature/bedlevel/bedlevel.h"
#include "../../../module/planner.h"

#if ENABLED(EXTENSIBLE_UI)
  #include "../../../lcd/extui/ui_api.h"
#endif

/**
 * M421: Set one or more Mesh Bed Leveling Z coordinates
 *
 * Usage:
 *   M421 I<xindex> J<yindex> Z<linear>
 *   M421 I<xindex> J<yindex> Q<offset>
 *
 *  - If I is omitted, set the entire row
 *  - If J is omitted, set the entire column
 *  - If both I and J are omitted, set all
 */
void GcodeSuite::M421() {
  const float ix_input = parser.floatval('I', -1), iy_input = parser.floatval('J', -1);
  if (!isfinite(ix_input) || !isfinite(iy_input)
    || !WITHIN(ix_input, -1, GRID_MAX_POINTS_X - 1) || ix_input != FLOOR(ix_input)
    || !WITHIN(iy_input, -1, GRID_MAX_POINTS_Y - 1) || iy_input != FLOOR(iy_input)) {
    SERIAL_ERROR_MSG(STR_ERR_MESH_XY);
    return;
  }
  const int8_t ix = ix_input, iy = iy_input;
  const bool hasZ = parser.seenval('Z'),
             hasQ = !hasZ && parser.seenval('Q');

  if (hasZ || hasQ) {
    if (WITHIN(ix, -1, GRID_MAX_POINTS_X - 1) && WITHIN(iy, -1, GRID_MAX_POINTS_Y - 1)) {
      const float zval = parser.value_linear_units();
      uint8_t sx = ix >= 0 ? ix : 0, ex = ix >= 0 ? ix : GRID_MAX_POINTS_X - 1,
              sy = iy >= 0 ? iy : 0, ey = iy >= 0 ? iy : GRID_MAX_POINTS_Y - 1;
      // Validate the complete edit before changing any cell.
      if (!isfinite(zval)) { SERIAL_ERROR_MSG("Bad mesh value"); return; }
      LOOP_S_LE_N(x, sx, ex)
        LOOP_S_LE_N(y, sy, ey)
          if (!isfinite(zval + (hasQ ? z_values[x][y] : 0))) {
            SERIAL_ERROR_MSG("Bad mesh value"); return;
          }
      bed_mesh_t candidate;
      memcpy(candidate, z_values, sizeof(candidate));
      LOOP_S_LE_N(x, sx, ex) LOOP_S_LE_N(y, sy, ey)
        candidate[x][y] = zval + (hasQ ? z_values[x][y] : 0);
      if (!bilinear_mesh_valid(candidate, bilinear_grid_spacing, bilinear_start, planner.settings.axis_steps_per_mm[Z_AXIS])) {
        SERIAL_ERROR_MSG("Unrepresentable mesh"); return;
      }
      LOOP_S_LE_N(x, sx, ex) {
        LOOP_S_LE_N(y, sy, ey) {
          z_values[x][y] = zval + (hasQ ? z_values[x][y] : 0);
          TERN_(EXTENSIBLE_UI, ExtUI::onMeshUpdate(x, y, z_values[x][y]));
        }
      }
      TERN_(ABL_BILINEAR_SUBDIVISION, bed_level_virt_interpolate());
    }
    else
      SERIAL_ERROR_MSG(STR_ERR_MESH_XY);
  }
  else
    SERIAL_ERROR_MSG(STR_ERR_M421_PARAMETERS);
}

#endif // AUTO_BED_LEVELING_BILINEAR
