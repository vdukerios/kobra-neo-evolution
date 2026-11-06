# Troubleshooting

> Version 1.0.0 boots and runs on the author's printer, but most features have not been validated by other users yet, so this list is based on how the firmware is designed and on the base firmware's known behavior. If you hit a problem, please report it (what you did, what the screen showed, `M115` output).

**In any emergency: switch the printer off at the power switch.**

## The printer does not boot after flashing

- Wait up to 2 minutes: the screen may stay black during the update.
- Power off, remove the SD card, power on again. If the screen is still black or frozen, put your backup firmware on the card as `firmware.bin` and flash it ([`INSTALL.md`](INSTALL.md)).
- Make sure the file is `KNE-1.0.0.bin` renamed to `firmware.bin`, built with Keil/Arm Compiler 5. Binaries built with the experimental GCC scripts do not boot ([`BUILD.md`](BUILD.md)).
- If it still fails, go back to your backup and report it (printer batch, what the screen showed).
- It is not known whether there is another recovery path if the bootloader were damaged.

## The printer keeps flashing the firmware at every boot

You left `firmware.bin` on the SD card. Delete it from the card.

## My settings are gone (PID, E-steps, Z offset, mesh)

Flashing can reset the settings memory. Send `M503`, set your values again, run your own PID tune (`M303`) and store with `M500`. Re-measure the mesh ([`LEVELING_AND_Z_OFFSET.md`](LEVELING_AND_Z_OFFSET.md)).

## The first layer is bad (squashed, ridged, or not sticking)

Separate the causes one at a time: bed cleanliness, bed temperature, Z offset, and mesh. Re-set the Z offset with the paper method, then re-measure the mesh hot. The mesh and the offset are different things: a perfect mesh with a wrong offset still gives a bad first layer.

## "Level and center" shows an error or does nothing

- "Not available: printer busy": wait for the current job or heating wait to end.
- A leveling failure (probe did not trigger or a point could not be reached) should cancel the process without lowering the nozzle. Check the probe cable, clean the bed, remove clips from the probe's path and try again.
- If you cancel, wait for the head to finish its current move.

## Print preparation (`M1100`) was cancelled

The mesh measurement failed, or you pressed Stop. The heaters are switched off. Fix the cause, then start again. If the printer still looks as if it is printing, press Stop.

## Express mode behaves like Full

Express falls back to Full on purpose when there is no valid saved mesh. Measure the mesh once and save it with `M500`.

## The heaters switched off by themselves

That is the idle protection: after 10 minutes without activity with the nozzle at 100 C or more, or the bed on, it turns them off ("Heater off: idle timeout"). It is not meant to act during printing or while waiting for a temperature. If it happens in the middle of a print or a heat-up, please report it.

## Menus feel different (clicks react on release)

With long-press help, a short click takes effect when you release the knob. To go back to the original behavior, the long-press feature can be disabled at build time ([`ARCHITECTURE.md`](ARCHITECTURE.md)).

## Night mode: screen is black and I cannot see anything

Turn or click the knob once to wake the screen (the first movement only wakes it). Night mode turns itself off on a critical error screen and at reboot. The backlight stays on; only the picture is black.

## Motors are hot or skipping steps

In High speed mode the X and Y motors run at higher current. Switch to Mixed or Silent ([`MOTOR_MODES.md`](MOTOR_MODES.md)). Check belt tension and lubrication; the maintenance reminder appears every 100 and 200 print hours.

## Remaining time is wrong

- It needs the `M73` lines from the Cura script ([`CURA_SETUP.md`](CURA_SETUP.md)); without them it falls back to a rough estimate.
- The learned factor needs at least 3 finished prints on the SD card (`FACTOR.TXT`). Delete `FACTOR.TXT` to start learning from scratch.
- Make sure the machine's acceleration in Cura matches the firmware (500).

## No thumbnail on the print screen

The G-code needs the thumbnail script, and the thumbnail block must be near the top of the file (the firmware searches only the first 24 KB). If Cura could not create the image, the G-code has no block. Open the file in a text editor and look for `;kn_thumb_begin`.

## SD card problems

Use FAT32. Keep file names short and the number of files per folder small (only 40 are sorted alphabetically). `REGISTRO.CSV`, `REGACTIV.TXT` and `FACTOR.TXT` are written in the card root; if the card is read-only or full they will not be written.

## After a power cut

Power-loss recovery is **off by default**. If you enable it (`M413 S1`, `M500`), be aware that after a power cut it homes X and Y without raising Z first, so the nozzle can drag across the printed part. Inspect the part and remove it if in doubt.

## Still stuck

Go back to your backup firmware, and open an issue with the exact steps, the screen text and the `M115` and `M503` output.
