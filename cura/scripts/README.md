# AgregarM73 - progress and remaining time on the Kobra Neo screen

This Cura script adds `M73` lines to the G-code so the printer screen shows the percentage and the time left.

## Install

1. In Cura: **Help > Show configuration folder**.
2. Open the **scripts** folder (create it if it does not exist).
3. Copy `AgregarM73.py` there.
4. Close Cura completely and open it again.

## Enable

1. With your model loaded: **Extensions > Post processing > Modify G-code**.
2. Click **Add a script** and choose **Add M73 (progress and remaining time)**.
3. Adjust the **Time correction factor** (optional):
   - `1.0` = Cura's estimate, unchanged.
   - `1.18` = if your printer usually takes 18 % longer. It only changes the remaining time, not the percentage.
4. Slice again and save the G-code.

## Notes

- It can be applied several times safely: the lines it adds carry the marker `;AgregarM73` and are replaced, not duplicated.
- If the G-code has no `;TIME:` in the header, the script does nothing.
- Tests are in `tests/` (`python -I tests/test_agregar_m73.py`). They do not need Cura.
- The script file names (`AgregarM73`, `AgregarMiniatura`) are kept because Cura requires the file name to match the class name; the texts shown in Cura are in English.

---

# AgregarMiniatura - thumbnail for the printer screen

Adds a still image of the model (80x80 by default, up to 96x96) at the start of the G-code so the printer screen can show it.

## Install and enable

1. Copy `AgregarMiniatura.py` to Cura's **scripts** folder (same as `AgregarM73.py`) and restart Cura.
2. **Extensions > Post processing > Modify G-code > Add a script > Add thumbnail (printer screen)**.
3. Adjust the size if you wish (80 recommended). Slice again and save the G-code.

## Format in the G-code (comments, no PNG)

```
;kn_thumb_begin 80 80 rgb565
;<base64, 76 characters per line: RGB565 little-endian pixels, row by row>
;kn_thumb_end
```

It goes right after the `;FLAVOR` line (or at the very beginning if there is none). It takes about 17 KB at 80x80. The image is composed over a `#0A0F1C` background and downscaled keeping its aspect ratio.

## Notes

- Idempotent: if a `kn_thumb` block already exists it is replaced, not duplicated.
- If Cura cannot provide the model view, the script leaves the G-code untouched (it only writes a warning to Cura's log).
- [assumption] Not tested with a real Cura: it is assumed that `cura.Snapshot.Snapshot.snapshot()` works from a post-processing script (the CreateThumbnail script bundled with Cura uses it the same way). If it does not work in your version, no thumbnail is inserted.
- Tests (no Cura needed): `python -I tests/test_miniatura.py`.

## Recommended start code

See [`../start_end_gcode.md`](../start_end_gcode.md) (start G-code using the new `M1100` command, and end G-code).
