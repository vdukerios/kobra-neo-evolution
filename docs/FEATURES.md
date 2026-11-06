# KNE features

> **The current 1.0.0 source candidate is uncompiled and untested on hardware; an earlier Keil build booted on the author's Kobra Neo. Each feature still needs validation by more users; see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md).** Use at your own risk; keep a backup firmware and an SD card with a known-good `firmware.bin`. Every feature below is **implemented**, not **verified**: "should" in this document means "is designed to". Please report what you find.

## Validation status

| Feature | Status |
|---|---|
| Boot, menus, English/Spanish UI | Earlier build confirmed; current menu source uncompiled |
| New home and print screens (thumbnail, progress bar) | Implemented, awaiting validation |
| On-device guide and long-press help | Implemented, awaiting validation |
| Sound menu | Implemented, awaiting validation |
| SD files sorted alphabetically | Implemented, awaiting validation |
| Preheat menu (PLA, PETG) | Implemented, awaiting validation |
| Print modes Full and Express (`M1100`) | Implemented, awaiting validation |
| Level and center | Implemented, awaiting validation |
| Junction deviation and bed leveling fade height | Implemented, awaiting validation |
| Motor modes (Silent, Mixed, High speed) | Implemented, awaiting validation |
| Idle heater shutdown | Implemented, awaiting validation |
| Print counter and maintenance reminders | Implemented, awaiting validation |
| Night mode | Implemented, awaiting validation |
| Print log (`REGISTRO.CSV`) | Implemented, awaiting validation |
| Learned remaining-time factor (`FACTOR.TXT`) | Implemented, awaiting validation |

Each feature can be switched off at build time with one line; see [`ARCHITECTURE.md`](ARCHITECTURE.md).

Terms used here: **nozzle** = the hot end that melts plastic; **bed** = the heated build plate; **mesh** = a map of how flat the bed is, used to compensate for small bumps; **Z offset** = the distance between where the probe triggers and where the nozzle touches the bed; **knob** = the single clickable dial used to navigate the menus.

## Menus and screens

### Language (English and Spanish)
Configuration > Preferences > Language switches the interface between English and Spanish. The default after a reset is English. The choice is saved in the settings memory. The Spanish texts are written in plain ASCII, **without accents**, because the display font does not render accented letters well.

### New home and print screens
Home has additional preheat, load/unload, leveling/centering and editable Z offset shortcuts. The original menu entries are preserved. Menu lists, editors and confirmations share dark cards and selection colors. See [the current source changes](CHANGES_2026-11-06.md). Redesigned home and print screens. When the G-code carries a model thumbnail (see [`CURA_SETUP.md`](CURA_SETUP.md)) it is shown on the print screen, and a progress bar fills as the print advances. While printing, turn the knob to switch pages (for example remaining time, elapsed time, speed, flow and fan). Without a thumbnail the screen still works.

### On-device guide and long-press help
- **Guide and help** (main menu): seven short topics for beginners:
  1. First print
  2. Load filament
  3. Level and height (paper)
  4. If power is cut
  5. If the part lifts off
  6. Basic maintenance
  7. What each menu does
- **Long-press help:** hold the knob for about 0.8 seconds on a menu option to read what it does. Because of this, a short click in menus now takes effect when you **release** the knob, not when you press it. If the menus feel slow or odd, report it.

### Sound menu
A sound on/off switch in Configuration > Preferences turns the buzzer on or off.

### SD files sorted alphabetically
Files and folders in the SD card menu are listed in alphabetical order. The firmware sorts up to 40 entries (a limit set at build time); with more entries in one folder, sorting may not apply to all of them. Keep print folders small.

### Load, unload and preheat

Choose a material (PLA initially selected) and review its nozzle temperature before loading or unloading. The nozzle heats before extrusion, which stops with a click. Finishing a load returns Home, keeps the selected nozzle temperature and applies the material bed/fan recipe; it does not start a file. Cancelling heat-up or unloading does not apply that extra preparation. Edit recipes under Configuration > Materials.

- PLA: nozzle 200 C, bed 60 C.
- PETG: nozzle 240 C, bed 80 C.
- ABS entry (240 C / 110 C) is inherited from the base firmware.

You may want to adjust these to your filament brand.

## Printing

### Print modes: Full and Express
When you pick a file from the SD card, a **Confirm print** screen lets you choose the mode before starting. The G-code file prepares the printer with the `M1100` command. Full is the careful default; Express is faster and relies on a saved mesh. See [`PRINT_MODES.md`](PRINT_MODES.md).

### Level and center
A main menu button that levels the bed and moves the nozzle to the center of the bed (about 0.5 mm above the surface) so you can set the Z offset with a sheet of paper. It keeps your current Z offset and aborts if the mesh measurement fails. See [`LEVELING_AND_Z_OFFSET.md`](LEVELING_AND_Z_OFFSET.md).

### Junction deviation instead of classic jerk
"Jerk" is the speed jump the printer allows at corners. KNE replaces the classic jerk setting with *junction deviation* (0.013 mm), which slows down for sharp corners automatically and tends to give smoother, quieter motion. Default accelerations are 500 mm/s2 for printing and 1000 mm/s2 for travel moves.

### Bed leveling fade height
The mesh correction fades out gradually and is gone at 10 mm height (default), so tall prints are not forced to follow a bump in the bed all the way up.

### Hybrid motor thresholds and motor modes
Three motor modes (Silent, Mixed, High speed) change how the X and Y motors are driven. See [`MOTOR_MODES.md`](MOTOR_MODES.md).

## Safety and maintenance

### Idle heater shutdown
If nothing is happening for 10 minutes, the heaters switch off and the screen shows "Heater off: idle timeout". It applies when the nozzle is at 100 C or more, or when the bed is heated, and it does **not** trigger while printing, while the printer is waiting for a temperature, or while moving. Extruder movement restarts the timer.

### Print counter and maintenance reminders
The printer counts print hours and reminds you to **lubricate the axes every 100 print hours** and to **check the belts every 200 print hours**. Preparation and printing count; pause and idle do not. Counters save at job end/stop, reminder reset and hourly checkpoints, independently of ordinary settings. Stored totals survive power-off; an abrupt cut can lose up to approximately the latest unsaved hour. Resetting one reminder preserves the other. The reminder names are localized: they follow the interface language ("Lubricate axes" / "Check belts" in English, "Lubricar ejes" / "Revisar correas" in Spanish).

### Night mode
A "Night mode" switch (main menu) is meant for printing overnight:

- the screen turns black after about 3 seconds and the buzzer is silent for everything;
- the first turn or click of the knob only wakes the screen (it does not trigger the action); it goes dark again after 30 seconds;
- the screen wakes by itself (for 120 seconds) on alerts, filament run-out, pause and the end of a print; a critical error screen turns night mode off completely.

Limits: it is **not saved** (it turns off at reboot); the backlight **cannot be switched off yet** (the screen is painted black but the light stays on; a build option for real backlight control exists and is off by default); and a muted buzzer also mutes warnings such as filament change beeps. The safety audit lists some low-priority alerts that may not wake the screen (see [`SAFETY_AND_AUDIT.md`](SAFETY_AND_AUDIT.md)).

## Data on the SD card

### Print log: `REGISTRO.CSV`
After each print from the SD card, one line is appended to `REGISTRO.CSV` in the card root, with the outcome: finished, no filament, power loss, interrupted or cancelled. A helper file `REGACTIV.TXT` marks "a print is in progress"; if the printer finds it filled at boot, the previous print is logged as interrupted. File names are in the short 8.3 format on purpose. Nothing is written to the card during the print itself, only when it starts and ends. The exact reason of a critical error (kill) is not logged.

### Learned remaining-time factor: `FACTOR.TXT`
Slicer estimates are often off for your printer. KNE remembers the ratio between real and estimated time of your last 10 finished prints in `FACTOR.TXT` and applies the average to the remaining-time display. With fewer than 3 prints the factor is 1.0; it is limited to between 0.7 and 2.0. The preparation time of `M1100` (heating and meshing) is excluded from the measurement. The remaining time needs the `M73` lines added by the Cura script (see [`CURA_SETUP.md`](CURA_SETUP.md)).

## Known disabled or inherited items

| Item | Status |
|---|---|
| G26 mesh validation pattern | Switched off |
| Bilinear mesh subdivision | Switched off |
| Power-loss recovery | Present but **off by default** (enable with `M413 S1` then `M500` at your own risk). After a power cut it moves the head to home on X and Y **without raising Z first**, which can drag the nozzle across the printed part. This is base firmware behavior and has not been changed |
| Backlight control | Not available yet (see Night mode) |
| Linear advance | Not enabled (see `firmware/README_NUCLEAR.md` for why) |
