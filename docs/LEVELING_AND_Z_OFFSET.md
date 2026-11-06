# Bed leveling and Z offset

> **Awaiting validation** (version 1.0.0 boots and runs on the author's printer, but "Level and center" has not been validated one feature at a time; see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md)). Keep a hand near the power switch the first times you use "Level and center" and watch the nozzle height.

## Two settings that decide the first layer

- **Bed mesh**: the Kobra Neo has an inductive probe (LeviQ). The command `G29` touches the bed at a grid of 6 x 6 points and stores how much the bed height deviates at each one. During printing the firmware compensates for those deviations.
- **Z offset**: the probe does not sit where the nozzle is, and it triggers some distance above the bed. The Z offset is the correction that makes "Z = 0" mean "nozzle touching the bed (a sheet of paper in between)". If it is wrong, the first layer is too squashed or does not stick, whatever the mesh says.

## Fade height

The mesh correction fades out with height and is gone at **10 mm** (default). Without it the printer would reproduce the bed's bumps all the way up in tall models.

## Level and center (main menu)

A button that does the leveling and parks the nozzle where you can set the Z offset by hand.

What it does, in order:

1. Homes the axes if needed.
2. Measures the bed mesh (`G29`). The probing routine heats the bed and the nozzle to its own preheat values first; the heaters turn off by themselves after 10 minutes of inactivity.
3. Lifts to a safe height, moves the nozzle (not the probe) to the **center of the bed**, and lowers slowly to about **0.5 mm**.
4. Opens the Z offset adjustment screen.

Your current Z offset is **kept** during the whole process. If the mesh measurement fails or you cancel before it ends, the button aborts and the nozzle does **not** go down; your offset is restored. The button is not available while the printer is busy.

### Setting the Z offset with a sheet of paper

1. Run Level and center with a clean bed. Do it with the bed hot (for example the bed temperature you print with) as the metal expands.
2. When the nozzle stops at the center, slide a normal sheet of printer paper between the nozzle and the bed.
3. Turn the knob in small steps (each step is 0.01 mm) to lower or raise the nozzle until the paper moves with a **slight drag**: you feel some friction but it does not tear or get stuck.
4. Click to **save** (the offset is stored in the settings memory, like `M851` followed by `M500`).
5. Print a first-layer test and fine-tune by a few hundredths of a millimeter. Squashed and ridged means too close; round, separated lines mean too far.

### If the settings memory was reset

After flashing, the Z offset may be back to 0. The "about 0.5 mm" target assumes the offset is already roughly right, so with a reset offset the nozzle will **not** end up at 0.5 mm from the bed. Lower the nozzle only slowly with the knob and watch it. A safer first step is to set an approximate offset with `M851 Z<value>` or the offset menu, and then run Level and center. The safety audit suggests the firmware should refuse the action when the offset is 0; this is not confirmed to be implemented.

## Measuring the mesh by hand

1. Heat the bed to your printing temperature and let it settle (about 7 minutes without moving).
2. Home (`G28`) and run `G29`. Wait until the screen reports that leveling has finished. The result is saved automatically.
3. To use the saved mesh in a later print, `M420 S1` switches it on (Express mode does this for you, see [`PRINT_MODES.md`](PRINT_MODES.md)).
4. Check `M503` afterwards and compare the Z offset with your notes.

If the screen shows a leveling failure, check that the probe cable is firmly connected, the bed is clean, and no cable or clip is in the probe's path, then try again. See [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md).
