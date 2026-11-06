# KNE screen simulator

Open `simulator.html` locally, or use the [hosted demo](https://vdukerios.github.io/kobra-neo-evolution/). It simulates the original 320 x 240 display with one clickable wheel. No printer connection is made.

The current source candidate has not been compiled or tested on the printer. Native text uses the existing 20 px font and a simpler layout; browser fonts, overlays, timings and calibration results are approximations.

## Current behavior

- Configuration has seven groups: Calibration, Materials, Motor mode, Preferences, Saved settings, Information and Service. Maintenance lives in the main menu. Ordinary menu actions have one location.
- Home shortcuts are intentionally additional: Menu, Preheat, Load, Unload, Level and center, and editable **Z offset**. Existing menu entries are preserved. **Z offset** keeps this exact name in English and Spanish.
- Loading and unloading select a material, with PLA initially selected, then review the nozzle temperature. Continue heats the nozzle before extrusion; clicking during extrusion stops it. Finishing a load returns Home, keeps the chosen nozzle temperature and applies the material's bed and fan recipe. Cancelling heat-up or finishing an unload does not trigger that preparation. No file starts automatically.
- Maintenance counts job time, including preparation, excluding pause and idle. Saved counters survive a restart independently of user settings; an abrupt power cut can lose the interval since the last checkpoint (up to approximately one hour).
- The same dark cards and selection colors are used throughout the candidate, with seven visible list rows. Factory reset and heating actions require confirmation. Calibrations and faults are simulations.

## Refresh and validate

From the repository root, with Python 3 and Node.js installed:

```sh
python -I -B simulator/tools/gen_menu_data.py
python -I -B simulator/tools/verify_candidate.py
```

After regenerating, copy `simulator/simulator.html` to `docs/index.html` before validation or publication. The check only reads firmware source; it does not invoke a compiler, use USB, or operate a printer. It writes simulator result records under `testing/`.

`tools/inventory_base.json` is the preserved earlier inventory. `build_kne_candidate.py` combines that baseline with explicitly reviewed source changes, then checks routes and records source hashes. This is **not** a regenerated inventory extracted from compiled firmware. The pinned firmware base revision is historical; source hashes identify the current candidate without a circular dependency on the publication commit.

Latest checks: 88 model cases passed; all 42 generated screens reached across 12 language/state/settings combinations; 11 injected duplicate regressions rejected. The renderer was also exported offscreen in 1,382 views without text outside its canvas. Browser appearance and physical TFT behavior remain unverified.

Representative documentation images can be exported from the actual renderer with `node simulator/tools/preview_screens.js <local-@napi-rs/canvas-module-path>`. That optional package is needed only for image export, not for opening or validating the simulator. These exports are not browser or printer screenshots.
