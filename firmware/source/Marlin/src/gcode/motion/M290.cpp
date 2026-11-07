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

#if ENABLED(BABYSTEPPING)

#include "../gcode.h"
#include "../../feature/babystep.h"
#include "../../module/probe.h"
#include "../../module/planner.h"
#include "../../module/motion.h"

#if ENABLED(BABYSTEP_ZPROBE_OFFSET)
  #include "../../core/serial.h"
#endif

#if ENABLED(MESH_BED_LEVELING)
  #include "../../feature/bedlevel/bedlevel.h"
#endif

#if ENABLED(BABYSTEP_ZPROBE_OFFSET)

  FORCE_INLINE void mod_probe_offset(const float &offs) {
    if (TERN1(BABYSTEP_HOTEND_Z_OFFSET, active_extruder == 0)) {
      probe.offset.z += offs;
      SERIAL_ECHO_START();
      SERIAL_ECHOLNPAIR(STR_PROBE_OFFSET " " STR_Z, probe.offset.z);
    }
    else {
      #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
        hotend_offset[active_extruder].z -= offs;
        SERIAL_ECHO_START();
        SERIAL_ECHOLNPAIR(STR_PROBE_OFFSET STR_Z ": ", hotend_offset[active_extruder].z);
      #endif
    }
  }

#endif

// Change the offset only for a representable, accepted physical adjustment.
static void m290_adjust(const AxisEnum axis, const float requested, const bool change_offset) {
  if ((DISABLED(BABYSTEP_WITHOUT_HOMING) && !all_axes_trusted()) || !isfinite(requested)) return;
  const float spmm = planner.settings.axis_steps_per_mm[axis],
              steps_f = constrain(requested, -2, 2) * spmm;
  if (!isfinite(spmm) || spmm <= 0 || !isfinite(steps_f) || !WITHIN(steps_f, -32768, 32767)) return;
  const int16_t steps = steps_f;
  if (!steps) return;
  const float accepted = steps / spmm;
  #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
    if (axis == Z_AXIS && change_offset) {
      const float next_offset = TERN(BABYSTEP_HOTEND_Z_OFFSET,
        active_extruder == 0 ? probe.offset.z + accepted : hotend_offset[active_extruder].z - accepted,
        probe.offset.z + accepted);
      if (!isfinite(next_offset) || !WITHIN(next_offset, Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX)) return;
    }
  #else
    UNUSED(change_offset);
  #endif
  // The pending step counter must not wrap and reverse the adjustment.
  if (!WITHIN(int32_t(babystep.steps[BS_AXIS_IND(axis)]) + steps, -32768L, 32767L)) return;
  if (!babystep.add_steps(axis, steps)) return;
  #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
    if (axis == Z_AXIS && change_offset) mod_probe_offset(accepted);
  #endif
}

/**
 * M290: Babystepping
 *
 * Send 'R' or no parameters for a report.
 *
 *  X<linear> - Distance to step X
 *  Y<linear> - Distance to step Y
 *  Z<linear> - Distance to step Z
 *  S<linear> - Distance to step Z (alias for Z)
 *
 * With BABYSTEP_ZPROBE_OFFSET:
 *  P0 - Don't adjust the Z probe offset
 */
void GcodeSuite::M290() {
  const bool change_offset = !parser.seen('P') || parser.value_bool();
  #if ENABLED(BABYSTEP_XY)
    LOOP_XYZ(a)
      if (parser.seenval(XYZ_CHAR(a)) || (a == Z_AXIS && parser.seenval('S')))
        m290_adjust((AxisEnum)a, parser.value_axis_units((AxisEnum)a), change_offset);
  #else
    if (parser.seenval('Z') || parser.seenval('S'))
      m290_adjust(Z_AXIS, parser.value_axis_units(Z_AXIS), change_offset);
  #endif

  if (!parser.seen("XYZ") || parser.seen('R')) {
    SERIAL_ECHO_START();

    #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
      SERIAL_ECHOLNPAIR(STR_PROBE_OFFSET " " STR_Z, probe.offset.z);
    #endif

    #if ENABLED(BABYSTEP_HOTEND_Z_OFFSET)
    {
      SERIAL_ECHOLNPAIR_P(
        PSTR("Hotend "), int(active_extruder)
        #if ENABLED(BABYSTEP_XY)
          , PSTR("Offset X"), hotend_offset[active_extruder].x
          , SP_Y_STR, hotend_offset[active_extruder].y
          , SP_Z_STR
        #else
          , PSTR("Offset Z")
        #endif
        , hotend_offset[active_extruder].z
      );
    }
    #endif

    #if ENABLED(MESH_BED_LEVELING)
      SERIAL_ECHOLNPAIR("MBL Adjust Z", mbl.z_offset);
    #endif

    #if ENABLED(BABYSTEP_DISPLAY_TOTAL)
    {
      SERIAL_ECHOLNPAIR_P(
        #if ENABLED(BABYSTEP_XY)
            PSTR("Babystep X"), babystep.axis_total[X_AXIS]
          , SP_Y_STR, babystep.axis_total[Y_AXIS]
          , SP_Z_STR
        #else
          PSTR("Babystep Z")
        #endif
        , babystep.axis_total[BS_TOTAL_IND(Z_AXIS)]
      );
    }
    #endif
  }
}

#endif // BABYSTEPPING
