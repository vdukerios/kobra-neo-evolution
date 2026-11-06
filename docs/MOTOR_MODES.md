# Motor modes

> **Awaiting validation.** An earlier Keil build booted on the author's Kobra Neo; the current source is uncompiled and the motor modes have not been validated one by one (see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md)). **High speed** mode uses a higher motor current: the first times, check by touch that the motors and drivers do not get too hot, and switch to Mixed or Silent if they do.

The Kobra Neo drives its stepper motors with TMC2208 chips. These chips have two ways of working:

- **stealthChop**: very quiet, but gives less torque at high speed.
- **spreadCycle**: louder, but keeps more torque at high speed and acceleration.

KNE adds a menu (**Configuration > Motor mode**) to choose how the **X and Y motors** behave. The extruder and the Z axis are not changed.

| Mode | What it does | Current (X and Y, RMS) | Noise | When to use it |
|---|---|---|---|---|
| **Silent** | stealthChop always | 800 mA (factory value) | Lowest | Default; night printing, normal speeds |
| **Mixed** | stealthChop below 60 mm/s, spreadCycle above that speed | 800 mA | Quiet when slow, louder on fast moves | Fast travel moves without giving up quiet printing |
| **High speed** | spreadCycle always | 900 mA | Loudest | Fast printing and high acceleration |

"RMS current" is the average motor current the driver is told to deliver.

## How to use it

1. Open **Configuration > Motor mode** and pick a mode. The menu shows the current mode.
2. The mode is applied immediately and **saved automatically** (like sending `M500`). It is restored on every boot. If the saved values are missing or out of range, the factory current is used.
3. After the first changes, try the mode with the printer empty and restart once to check that it is kept.

## High speed: watch the motor temperature

More current means hotter motors. The X and Y motors have no fan. After the first prints in High speed, **touch the X and Y motors** (carefully, after pausing the print): if you cannot keep a finger on them for a few seconds, go back to Mixed or Silent. The TMC2208 limit without cooling is roughly 1100 to 1200 mA RMS; KNE accepts values from 400 to 1200 mA when restoring settings, and the commands `M906` (set current manually) are not limited by KNE. Do not raise the current above the High speed value without adding cooling.

## Notes

- Only the X and Y drivers are changed; the Z axis and the extruder keep their settings.
- The current mode is deduced from the driver settings that are saved in the settings memory (stealthChop on/off, speed threshold and current), not stored as a separate variable.
- Values are set in `firmware/source/Marlin/src/inc/valentin/modos_motor.h` (see [`ARCHITECTURE.md`](ARCHITECTURE.md)). Changing them needs a rebuild ([`BUILD.md`](BUILD.md)).
- Reminder from the safety audit: the upper limit of 1200 mA is on the high side; see [`SAFETY_AND_AUDIT.md`](SAFETY_AND_AUDIT.md).
