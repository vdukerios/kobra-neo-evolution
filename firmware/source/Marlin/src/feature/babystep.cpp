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

#include "../inc/MarlinConfig.h"

#if ENABLED(BABYSTEPPING)

#include "babystep.h"
#include "../MarlinCore.h"
#include "../module/motion.h"   // for axes_should_home()
#include "../module/planner.h"  // for axis_steps_per_mm[]
#include "../module/stepper.h"

#if ENABLED(BABYSTEP_ALWAYS_AVAILABLE)
  #include "../gcode/gcode.h"
#endif

Babystep babystep;

volatile int16_t Babystep::steps[BS_AXIS_IND(Z_AXIS) + 1];
#if ENABLED(BABYSTEP_DISPLAY_TOTAL)
  int16_t Babystep::axis_total[BS_TOTAL_IND(Z_AXIS) + 1];
#endif
int16_t Babystep::accum;

void Babystep::step_axis(const AxisEnum axis) {
  const int16_t curTodo = steps[BS_AXIS_IND(axis)]; // get rid of volatile for performance
  if (curTodo) {
    stepper.do_babystep((AxisEnum)axis, curTodo > 0);
    if (curTodo > 0) steps[BS_AXIS_IND(axis)]--; else steps[BS_AXIS_IND(axis)]++;
  }
}

bool Babystep::add_mm(const AxisEnum axis, const float &mm) {
  const float amount = mm * planner.settings.axis_steps_per_mm[axis];
  if (!isfinite(amount) || !WITHIN(amount, -32768.0f, 32767.0f)) return false;
  return add_steps(axis, int16_t(amount));
}

bool Babystep::add_steps(const AxisEnum axis, const int16_t distance) {
  if (!distance || (DISABLED(BABYSTEP_WITHOUT_HOMING) && !all_axes_trusted())) return false;
  const uint32_t saved_mask = __get_PRIMASK();
  __disable_irq();
  const int32_t next_steps = int32_t(steps[BS_AXIS_IND(axis)]) + distance,
                next_accum = int32_t(accum) + distance;
  bool accepted = WITHIN(next_steps, -32768L, 32767L) && WITHIN(next_accum, -32768L, 32767L);
  #if ENABLED(BABYSTEP_DISPLAY_TOTAL)
    const int32_t next_total = int32_t(axis_total[BS_TOTAL_IND(axis)]) + distance;
    accepted = accepted && WITHIN(next_total, -32768L, 32767L);
  #endif
  if (accepted) {
    steps[BS_AXIS_IND(axis)] = next_steps;
    accum = next_accum;
    TERN_(BABYSTEP_DISPLAY_TOTAL, axis_total[BS_TOTAL_IND(axis)] = next_total);
  }
  __set_PRIMASK(saved_mask);
  if (!accepted) return false;
  TERN_(BABYSTEP_ALWAYS_AVAILABLE, gcode.reset_stepper_timeout());
  TERN_(INTEGRATED_BABYSTEPPING, if (has_steps()) stepper.initiateBabystepping());
  return true;
}

#endif // BABYSTEPPING
