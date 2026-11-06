# Recommended start and end G-code (for the KNE firmware)

> **Awaiting validation** (version 1.0.0 boots and runs on the author's printer; these G-code blocks still need more users to confirm them, see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md)). Stay next to the printer for the first prints and be ready to press Stop or switch the power off.

Paste the blocks below in Cura: **Settings > Printer > Manage printers > Machine settings** (*Start G-code* and *End G-code*). Replace what you have. Do **not** keep `G28`, `M190`, `M109`, `M420`, `G29` or a long wait before `G28` in the start G-code, because `M1100` handles all of that.

The same blocks are already in the printer file under [`profiles/`](profiles/), so if you install that file you do not need to paste them.

## Start G-code

```
;Clean start - Kobra Neo with KNE firmware
;The firmware already carries the extruder steps and PID: they are not set here
M1100 S{material_bed_temperature_layer_0} T{material_print_temperature_layer_0} W7
G92 E0 ; reset extruder
G1 Z5 F600 ; lift before starting
```

`M1100 S<bed> T<nozzle> W<soak minutes>`:

| Parameter | Meaning |
|---|---|
| `S` | Bed temperature in C (Cura fills it in) |
| `T` | Nozzle temperature in C (Cura fills it in) |
| `W` | Soak time in minutes, 0 to 60. Used only by Full mode: the head waits with the bed hot so its temperature settles before the mesh is measured. `W7` equals a 7 minute wait; use 0 to skip it |

How it works (the mode is chosen on the "Confirm print" screen when you pick the file on the SD card; default Full). Details in [`../docs/PRINT_MODES.md`](../docs/PRINT_MODES.md):

1. **Full**: home, heat the bed and wait for its exact temperature, wait `W` minutes, measure the hot mesh, re-apply the bed temperature, heat the nozzle and wait for it.
2. **Express**: home (only if needed), heat bed and nozzle at the same time, switch on the saved mesh; no measuring and no soak. If there is no valid saved mesh it runs as Full.
3. If the file does not call `M1100`, the firmware does nothing special (old files behave as before). If `M1100` runs without a mode chosen (for example from a USB host), Full is used.
4. If the mesh measurement fails, or you press Stop during the preparation, the heaters are switched off.

### Optional: purge line

`M1100` does not draw a purge (priming) line. Cura's skirt is enough for many users. If you want one, add something like this after `G92 E0` and adapt it to your printer:

```
G1 X10.1 Y20 Z0.28 F5000.0
G1 X10.1 Y200.0 Z0.28 F1500.0 E15
G1 X10.4 Y200.0 Z0.28 F5000.0
G1 X10.4 Y20 Z0.28 F1500.0 E30
G92 E0
```

## End G-code

```
;Clean end - Kobra Neo
G91 ; relative moves
G1 E-1 F1800 ; retract 1 mm
G1 Z5 F600 ; lift 5 mm
G90 ; absolute moves
M104 S0 ; nozzle off
M140 S0 ; bed off
M106 S0 ; fan off
G1 X0 Y220 F5000 ; present the part
M84 ; motors off
```

After a print, check with `M105` that the nozzle target really is 0. As a safety net, the firmware also switches off heaters that sit idle for 10 minutes ([`../docs/FEATURES.md`](../docs/FEATURES.md)).

## Post-processing scripts

Install them from [`scripts/`](scripts/) (see its README and [`../docs/CURA_SETUP.md`](../docs/CURA_SETUP.md)): **AgregarM73** (progress and remaining time on the printer screen) and **AgregarMiniatura** (thumbnail on the printer screen). In Cura: *Extensions > Post processing > Modify G-code > Add a script*.
