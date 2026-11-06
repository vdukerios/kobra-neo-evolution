# Print modes: Full and Express

> **Awaiting validation** (version 1.0.0 boots and runs on the author's printer, but the print modes have not been validated one by one; see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md)). Try both modes with a small model (for example a 20 mm cube), stay next to the printer, and be ready to press Stop or switch the power off.

KNE adds a G-code command, `M1100`, that prepares the printer for a print. Your slicer's start G-code calls it once (see [`CURA_SETUP.md`](CURA_SETUP.md)). The way it prepares depends on the **mode** you choose on the **Confirm print** screen.

## Choosing the mode

1. In the SD card menu, pick the file. The **Confirm print** screen opens, showing the file name.
2. Click the mode line to switch between **Mode: Full** and **Mode: Express**. The default is Full.
3. Click **Print** to start or **Cancel** to go back.

If a print is started in another way (for example from a USB host program) with no mode chosen, **Full** is used. If the file does not call `M1100` at all, the firmware does nothing special and old G-code files behave as before.

## What each mode does

`M1100 S<bed> T<nozzle> W<soak minutes>`: `S` bed temperature in C, `T` nozzle temperature in C, `W` soak time in minutes (0 to 60, default 0).

| Step | Full | Express |
|---|---|---|
| 1 | Home all axes (`G28`) | Home only if not already homed |
| 2 | Heat the bed and **wait for the exact temperature** | Start heating the bed **and** the nozzle at the same time |
| 3 | Optional soak: wait `W` minutes with the head still so the bed temperature settles (Stop cancels it) | Switch on the **saved mesh** (`M420 S1`) |
| 4 | Measure the bed mesh **hot** (`G29`) | Wait for the bed temperature, then the nozzle temperature |
| 5 | Re-apply the bed temperature, then heat the nozzle and wait for it | Print |
| 6 | Print | |

Why Full waits and measures: a heated bed changes shape slightly as it warms up, so a mesh measured hot and settled follows the real surface better. Express saves time (no soak, no measuring) and trusts the mesh you saved earlier.

## Safety rules built into the modes

- **Express needs a valid saved mesh.** If there is none (for example after the settings memory was reset), the firmware quietly falls back to Full.
- If the mesh measurement **fails** in Full mode, the preparation is cancelled, the heaters and the fan are switched off and the print file is aborted: the nozzle is not heated and printing does not start. (Not yet validated on hardware.)
- Pressing **Stop** during the preparation turns off the bed, the nozzle and the fan.
- The idle-heater protection is aware of the preparation, so a long wait for the bed does not switch the heaters off by mistake.
- The time spent in `M1100` is not counted when KNE learns your remaining-time factor ([`FEATURES.md`](FEATURES.md)).

## Tips

- Use Full for the first print of the day, after changing filament type, or after moving the printer. Use Express for a second print on an already hot, settled printer.
- Measure the mesh hot at least once so that Express has a good mesh to use ([`LEVELING_AND_Z_OFFSET.md`](LEVELING_AND_Z_OFFSET.md)).
- Do not keep `G28`, `M190`, `M109`, `G29` or a long wait in your start G-code; `M1100` already does them.
