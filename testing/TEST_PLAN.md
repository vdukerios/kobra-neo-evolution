# Firmware test plan (Kobra Neo)

**Status:** version 1.0.0 boots and runs on the author's Kobra Neo. Each feature still needs validation by more users. This document is the checklist: please run it and report your results (good or bad) as an issue on <https://github.com/vdukerios/kobra-neo-evolution>.

Use at your own risk. Keep a backup firmware and an SD card with a known-good `firmware.bin`.

## What you need

| Item | What it is | Purpose |
|---|---|---|
| Backup: Nuclear v1.3.6 | The firmware many owners run today (get it from the NuclearPhoenixx/Kobra_Neo repository; not included) | Go back to it |
| Backup: community firmware 1.3.3 | Second backup (not included; see `firmware/README_NUCLEAR.md` for sources) | Second fallback |
| `releases/v1.0.0/KNE-1.0.0.bin` | KNE 1.0.0, built with Keil / Arm Compiler 5 | The firmware under test |

`releases/v1.0.0/SHA256.txt` has the checksum, in case you want to verify the file was not altered.

## How to install a firmware (same as when you installed Nuclear)

1. Use an empty SD card (FAT32). Copy ONLY ONE .bin file to the card root, renamed to `firmware.bin`.
2. With the printer OFF, insert the card and power on. Wait for the home screen (the screen may stay black for a while during installation).
3. Power off, remove the card, and DELETE `firmware.bin` from the card before using it again.

## Test order (do not skip steps)

**Step 1: boot.** Install `KNE-1.0.0.bin`. The printer should show the language selection on the first boot, then the home screen, move the axes and read temperatures. If it does NOT boot (black or frozen screen for more than 2 minutes): power off, install a backup and report it (include the printer's purchase period or batch if you know it). Do not continue.

**Step 2: check the saved settings.** After installing any firmware, the settings memory may be erased. Check with the information menu or by sending M503. Compare with the values you saved before flashing: nozzle PID (M301), bed PID (M304), extruder steps (M92 E), Z offset (M851 Z). The bed mesh has to be measured again.

If any value changed, fix it and save with M500. Remember: after turning the nozzle off with M104 S0, verify with M105 that it is at 0; to wait for an exact temperature use M109 R, not M109 S.

**Step 3: WITHOUT heating anything at first.**

1. Menus and texts: check that they display correctly, in English and in Spanish (Spanish has no accents on purpose), without strange characters; review Settings, Information, Guide and help.
2. Long-press help: hold the click for almost a second on an option. If a normal click feels odd or slow in the menus, note it.
3. Move axes and origin: they should move as before.
4. Motor mode (Settings): try the three modes with the printer empty, and restart to check the chosen mode is kept. In High speed mode touch the motors after a few minutes to check they are not too hot.
5. Night mode: the screen should go black and wake up with the wheel.
6. Home screen: temperatures and Z offset.

**Step 4: with heat.** Preheat PLA and PETG from the menu and check that the heater turns off at the end. Check the idle shutdown (heaters off after 10 minutes of inactivity). Try Level and center with a hot nozzle, watching the height, with a hand ready on the switch.

**Step 5: one short print** (20 mm cube) in Full mode, then another in Express. The start G-code to use is in `cura/start_end_gcode.md`. Check the print log (`REGISTRO.CSV`) afterwards.

## Result checklist (copy it into your report)

| Feature | Result (OK / problem / not tested) | Notes |
|---|---|---|
| Boot, menus, English/Spanish UI | | |
| Guide and long-press help | | |
| Home and print screens (thumbnail, progress bar) | | |
| Sound menu, preheat, SD sorting | | |
| Motor modes (Silent / Mixed / High speed) | | |
| Night mode | | |
| Level and center | | |
| Print mode Full (`M1100`) | | |
| Print mode Express (`M1100`) | | |
| Idle heater shutdown | | |
| Maintenance reminders (language follows the menu) | | |
| Print log `REGISTRO.CSV` | | |
| Learned time factor `FACTOR.TXT` | | |

Also include: your printer's firmware before KNE, the `M115` output, and what the screen showed.

## If something goes wrong

- Install a backup firmware again (same procedure).
- It is not known whether there is another way out if the board's bootloader were damaged; so far there is no evidence that our firmware touches it (it starts at address 0x8000, after it).

## What is NOT validated

Confirmed on one printer (the author's): boot, menus and the English/Spanish interface. Everything else depends on testers: screens in detail, motor modes, night mode, thumbnail, SD logging, time learning, print modes, Level and center.
