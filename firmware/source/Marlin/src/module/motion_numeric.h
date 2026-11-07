#pragma once

// Representation contracts, not mechanical limits. Include after planner.h.
#include <float.h>
#include "../HAL/STM32/timers.h"
namespace kne_numeric {
  inline bool positive(const float v) { return isfinite(v) && v > 0 && isfinite(1.0f / v); }
  inline bool feedrate_valid(const float v) { return positive(v) && isfinite(v * v); }
  inline bool fade_valid(const float v) { return isfinite(v) && v >= 0 && (!v || positive(v)); }
  inline bool axis_value_valid(const float value, const float steps) {
    const float product = value * steps;
    return isfinite(value) && positive(steps) && isfinite(product)
        && double(product) >= -2147483647.0 && double(product) <= 2147483647.0;
  }
  inline bool acceleration_product_valid(const double value, const float steps) {
    const double product = value * double(steps);
    return value > 0 && value <= 2147483647.0 && positive(steps)
        && product >= 1 && product <= 2147483520.0;
  }
  inline bool acceleration_valid(const double value, const float steps) {
    return value >= 1 && floor(value) == value && acceleration_product_valid(value, steps);
  }
  inline bool settings_valid(const planner_settings_t &s) {
    if (!positive(s.acceleration) || !positive(s.retract_acceleration) || !positive(s.travel_acceleration)
      || !isfinite(s.min_feedrate_mm_s) || s.min_feedrate_mm_s < 0
      || !isfinite(s.min_travel_feedrate_mm_s) || s.min_travel_feedrate_mm_s < 0
      || (s.min_feedrate_mm_s && !feedrate_valid(s.min_feedrate_mm_s))
      || (s.min_travel_feedrate_mm_s && !feedrate_valid(s.min_travel_feedrate_mm_s))
      || s.min_segment_time_us > 2147483647UL) return false;
    LOOP_XYZE_N(i) {
      if (!positive(s.axis_steps_per_mm[i]) || !feedrate_valid(s.max_feedrate_mm_s[i])
        || !acceleration_valid(s.max_acceleration_mm_per_s2[i], s.axis_steps_per_mm[i])
        || !acceleration_product_valid(s.acceleration, s.axis_steps_per_mm[i])
        || !acceleration_product_valid(s.travel_acceleration, s.axis_steps_per_mm[i])
        || !acceleration_product_valid(s.retract_acceleration, s.axis_steps_per_mm[i])
        || double(s.max_feedrate_mm_s[i]) * s.axis_steps_per_mm[i] > 2147483520.0
        || double(s.max_feedrate_mm_s[i]) * s.axis_steps_per_mm[i] < 1) return false;
    }
    return true;
  }
  inline bool position_valid(const xyze_pos_t &p, const planner_settings_t &s, const uint8_t extruder) {
    LOOP_XYZE(i) if (!axis_value_valid(p[i], s.axis_steps_per_mm[i == E_AXIS ? E_AXIS_N(extruder) : i])) return false;
    return true;
  }
  inline bool move_valid(const xyze_pos_t &from, const xyze_pos_t &to, const planner_settings_t &s,
                         const float speed, const uint8_t extruder, const float efactor=1) {
    if (!position_valid(from, s, extruder) || !position_valid(to, s, extruder)
      || !feedrate_valid(speed) || !isfinite(efactor) || efactor < 0) return false;
    double length2 = 0, events = 0, extrusion_length = 0;
    LOOP_XYZE(i) {
      const uint8_t axis = i == E_AXIS ? E_AXIS_N(extruder) : i;
      // Match the planner's rounded absolute positions before taking a delta.
      // Position contracts above make both LROUND operations representable.
      const double delta_steps = double(LROUND(to[i] * s.axis_steps_per_mm[axis]))
                               - double(LROUND(from[i] * s.axis_steps_per_mm[axis])),
                   scaled_steps = fabs(delta_steps * (i == E_AXIS ? efactor : 1));
      // Bound the raw signed subtraction even when extrusion flow is zero.
      if (fabs(delta_steps) > 1073741696.0 || scaled_steps > 1073741696.0) return false;
      const double steps = i == E_AXIS ? floor(scaled_steps + 0.5) : scaled_steps;
      if (steps > events) events = steps;
      const double distance = delta_steps / s.axis_steps_per_mm[axis];
      if (i < E_AXIS) length2 += distance * distance;
      else extrusion_length = fabs(distance * efactor);
    }
    double length = sqrt(length2);
    if (!length) length = extrusion_length;
    if (!events || !length) return true;
    const double duration = 1000000.0 * length / speed,
                 rate = ceil(events * speed / length), density = events / length;
    if (!(duration >= 0.5 && duration <= 2147483520.0) || !(rate >= 1 && rate <= 2147483520.0)) return false;
    const double accelerations[] = { s.acceleration, s.travel_acceleration, s.retract_acceleration };
    for (uint8_t i=0; i<3; ++i) {
      const double accel = ceil(accelerations[i] * density);
      if (!(accel >= 1 && accel <= 2147483520.0)) return false;
      // Both squared rates / acceleration and S-curve ticks must fit their consumers.
      if (rate * rate / (2 * accel) > 2147483520.0) return false;
      #if ENABLED(S_CURVE_ACCELERATION)
        if (rate * double(STEPPER_TIMER_RATE) / accel > 4294967040.0) return false;
      #endif
    }
    return true;
  }
}
