# Cura profiles (starting point)

These are the author's Cura 5.12 settings for the **Anycubic Kobra Neo**, shared as a **starting point**. They are a **snapshot taken on 5 November 2026**; the author keeps tuning them, so values (temperatures, speeds, flow) will change and have **not been validated by anyone else**. Check every value before printing.

## Files

| File | What it is |
|---|---|
| `printer/Anycubic+Kobra+Neo_settings.inst.cfg` | Machine settings of the printer "Anycubic Kobra Neo" (a *Custom FFF printer*): 220 x 220 x 250 mm, heated bed, maximum feedrates/accelerations/jerk, and start/end G-code. **Note:** the start G-code in this file calls the KNE command `M1100` (home, heat, mesh, with a 7 minute soak in Full mode) and the end G-code turns off heaters, fan and motors; the comments are in English. It needs the KNE firmware: with another firmware `M1100` does nothing useful, so paste your own start G-code instead. The same blocks, explained, are in [`../start_end_gcode.md`](../start_end_gcode.md). |
| `quality/custom_pla.inst.cfg`, `custom_petg.inst.cfg`, `custom_tpu.inst.cfg` | Global part of the PLA / PETG / TPU profiles: 0.2 mm layers, skirt, no supports, bed temperatures (PLA 60, PETG 70/75, TPU 50). |
| `quality/custom_extruder_1_%232_pla.inst.cfg` (and `_petg`, `_tpu`) | Extruder part of each profile: nozzle temperatures, speeds, retraction, fan, flow, walls/infill. |
| `quality/custom_pla_rapido.inst.cfg`, `custom_extruder_1_%232_pla_rapido.inst.cfg` | **PLA Rapido** (fast PLA): 0.28 mm layers, 2 walls, 10 % gyroid, nozzle 220 C, speeds 80 mm/s (outer wall 45, infill 90, travel 200), acceleration control 800 mm/s2 print / 1000 travel (the firmware maximum is 1000), junction deviation instead of jerk. Chosen from the firmware limits; the hotend's real maximum flow was **not measured** (about 9 mm3/s at these speeds). If you see gaps or weak layers, lower the speeds or raise the temperature. |

You can install all of these automatically with `python ../install_cura.py --profiles` (see [`../../docs/CURA_SETUP.md`](../../docs/CURA_SETUP.md)).

The material itself is Cura's stock *Generic PLA 1.75* (or your own); no material file is shipped.

## How to use them

1. **Close Cura completely.**
2. In Cura 5.12 create the printer first (*Settings > Printer > Add printer > Custom FFF printer*), name it exactly `Anycubic Kobra Neo`, then close Cura again.
3. Open the configuration folder: `%APPDATA%\cura\5.12\` (Windows). Back it up first.
4. Copy `printer/Anycubic+Kobra+Neo_settings.inst.cfg` into `definition_changes\` (overwrite the file with the same name).
5. Copy the six files from `quality/` into `quality_changes\`.
6. Start Cura and select the PLA, PETG or TPU profile in the profile selector.

## Notes

- The extruder file names contain `%232` (an encoded `#2`) because they come from the author's installation. If Cura does not show the profiles, rename them to match your extruder name or enter the values by hand.
- The files are plain text (UTF-8, no BOM) and contain no personal data.
- Machine-specific values (e.g. `M92`, PID) are intentionally not set here: the firmware carries its own calibration.
