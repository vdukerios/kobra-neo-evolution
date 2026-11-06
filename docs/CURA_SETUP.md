# Cura setup

> The scripts and profiles were prepared with **Cura 5.12** on Windows. These settings still need validation by more users, and the thumbnail script has not been tested inside a real Cura. Check every value before printing.

## 1. Install the post-processing scripts

Two Cura scripts live in [`../cura/scripts/`](../cura/scripts/). Copy both to Cura's `scripts` folder and restart Cura. The file names (`AgregarM73`, `AgregarMiniatura`) are Spanish because Cura requires the file name to match the script class; the texts shown inside Cura are in English.

1. In Cura: **Help > Show configuration folder**, open the `scripts` folder (create it if missing).
2. Copy `AgregarM73.py` and `AgregarMiniatura.py` into it.
3. Close Cura completely and open it again.
4. With a model loaded: **Extensions > Post processing > Modify G-code > Add a script**, then add:
   - **Add M73 (progress and remaining time)**: adds `M73` lines so the printer screen shows the percentage and time left. Optional "time correction factor" (1.0 = Cura's estimate). Needed for the remaining-time display and the learned factor.
   - **Add thumbnail (printer screen)**: adds a small picture (80 x 80 recommended) of the model at the start of the G-code for the printer's print screen. If Cura cannot supply the picture, the G-code is left untouched.
5. Slice again and save the G-code. Both scripts can be applied repeatedly without duplicating their lines.

More details: [`../cura/scripts/README.md`](../cura/scripts/README.md).

## 2. Start and end G-code

Open **Settings > Printer > Manage printers > Machine settings** and paste the blocks from [`../cura/start_end_gcode.md`](../cura/start_end_gcode.md). The start block calls `M1100` with the bed temperature, the nozzle temperature and the soak time; the end block turns off heaters, fan and motors. **Remove** any `G28`, `M190`, `M109`, `G29` and waits from your own start G-code; `M1100` does all that ([`PRINT_MODES.md`](PRINT_MODES.md)).

## 3. Profiles (starting point)

[`../cura/profiles/`](../cura/profiles/) contains the author's printer settings and PLA / PETG / TPU quality profiles. They are a snapshot, not tuned for your printer or your filament. Install steps are in [`../cura/profiles/README.md`](../cura/profiles/README.md). Check temperatures, speeds and flow for your own filament.

## 4. Machine settings for realistic time estimates

By default Cura assumes a very fast printer (about 4000 mm/s2 acceleration), so its time estimates come out far too short (around 30 % in the author's case). In **Machine settings** (and in the printer file of the profiles) enter limits that match the firmware defaults:

| Setting | Value |
|---|---|
| Printing acceleration (`machine_acceleration`) | 500 mm/s2 |
| Maximum acceleration X / Y | 1000 mm/s2 |
| Maximum acceleration Z / E | 100 / 500 mm/s2 |
| Maximum speed X / Y | 300 mm/s |
| Maximum speed Z / E | 4 / 50 mm/s |
| Jerk X-Y (used only for the estimate) | 8 |

These are the firmware's default limits. If you change the firmware limits (`M201`, `M204`), update Cura too. The firmware uses junction deviation, so Cura's jerk value affects only the estimate.

## 5. Optional: ArcWelder plugin

The ArcWelder plugin (installed from Cura's Marketplace or from its own project page) converts many tiny line segments into arc commands (`G2`/`G3`), which can make circles smoother and shrink the G-code file. It is optional and **not tested** with KNE. If you try it, print a small round model first and watch for stuttering, since arc handling has not been tested on this firmware. Arc commands are enabled in the firmware configuration (`ARC_SUPPORT`). If the printer stutters or rejects the commands, disable the plugin.

## 6. Check the result

Save a G-code file and open it in a text editor: near the top you should see the `;kn_thumb_begin` block, and the start G-code should contain a single `M1100` line. Then follow the steps in [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md).
