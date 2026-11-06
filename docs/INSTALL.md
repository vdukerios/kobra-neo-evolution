# Installing KNE on the Kobra Neo

> **Note:** no firmware binary is attached to the repository yet; it will appear on the GitHub Releases page after the current source is built and validated. Until then build it yourself ([`BUILD.md`](BUILD.md)). The file you install must be named `firmware.bin`.

> **The current 1.0.0 source candidate is uncompiled and untested on hardware; an earlier Keil build booted on the author's Kobra Neo.** Each feature still needs validation by more users; see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md). Use at your own risk; keep a backup firmware and an SD card with a known-good `firmware.bin`. If you are not comfortable with the risk, do not flash.

Time needed: about 30 minutes for the first install, plus the calibration afterwards.

## 1. What you need

- A Kobra Neo (original model) and its power cable.
- A microSD card formatted as **FAT32** (a card of 32 GB or less is the safest choice) and a card reader.
- A USB cable and a terminal program (for example the serial monitor of your slicer or any serial terminal, 115200 baud) to send G-code such as `M503`. Optional but strongly recommended.
- A backup of the firmware you run today (see below).
- A validated Keil/Arm Compiler 5 binary. **None is published yet**; the release notes are in [`../releases/v1.0.0/`](../releases/v1.0.0/).

Only the file built with Keil and Arm Compiler 5 is a supported release. Do **not** flash binaries produced by the experimental GCC scripts in `build/`: they do not boot ([`BUILD.md`](BUILD.md)).

## 2. Back up first

1. **Save your current settings.** Connect the printer by USB, send `M503` and save the full reply to a text file. It contains your PID values, extruder steps (E-steps), Z offset and other settings. Take a photo of the Z offset on the printer screen too.
2. **Get a copy of the firmware you run today** and keep it on a separate SD card or in a folder. This project does not include them because they are not ours:
   - NuclearPhoenixx firmware: <https://github.com/NuclearPhoenixx/Kobra_Neo>
   - Anycubic's original firmware (for example 1.3.3): see the repositories listed in `firmware/README_NUCLEAR.md`.
3. Once a binary is published, verify it against its accompanying SHA-256 checksum. There is no published binary/checksum pair yet.

## 3. Install procedure (SD card)

The printer's bootloader flashes a file called `firmware.bin` found in the root of the SD card.

1. On your computer, copy **only one** `.bin` file to the root of the SD card: `KNE-1.0.0.bin`, **renamed to `firmware.bin`**. The card should contain no other firmware file.
2. Turn the printer **off** and insert the card.
3. Turn the printer **on**. The screen may stay black for a while during the update. Wait until the **home screen** appears. Do not turn the printer off while it is updating.
4. Turn the printer off, remove the card and **delete `firmware.bin`** from it before using the card for printing. Otherwise the printer may flash it again on the next boot.

If the printer does **not** boot (black or frozen screen for more than 2 minutes), power off, install your backup firmware with the same procedure and report the problem.

The complete test order is in [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md).

## 4. First boot: language

The default after a reset is English. Change it in **Configuration > Preferences > Language** (English or Spanish). Spanish texts have no accents on purpose (the display font does not render them well).

## 5. After flashing: check your settings

The current source changes the settings version to K10. On the first upgrade from the older Nuclear settings version it **restores factory settings**. This behavior remains uncompiled and untested; do not assume your calibration survived.

1. Send `M503` and compare the output with the file you saved in step 2.
2. If a value changed, set it again and store it with `M500`. Typical commands: `M301` (nozzle PID), `M304` (bed PID), `M92 E<steps>` (extruder steps), `M851 Z<offset>` (Z offset).
3. **Run your own PID tune** for the nozzle and the bed (`M303`) rather than reusing someone else's numbers; values depend on your unit and your fan. Save them with `M500`.
4. **Re-measure the bed mesh** with a heated bed (see [`LEVELING_AND_Z_OFFSET.md`](LEVELING_AND_Z_OFFSET.md)), and set the Z offset again if it was reset.
5. After turning the nozzle off with `M104 S0`, confirm with `M105` that it reads 0 target. To wait for an exact temperature in your own G-code use `M109 R` (not `M109 S`).

## 6. Going back to the previous firmware

Repeat the SD procedure (section 3) with your backup file renamed to `firmware.bin`. Afterwards check your settings again with `M503`.

Limits you should know about:

- This is the same route you used to install the current firmware, and KNE starts at address 0x8000, after the board's bootloader, so it is not expected to overwrite the bootloader.
- It is **not known** whether another way out exists if the bootloader were damaged. There is no evidence that KNE touches it.
- The bootloader is protected and could not be inspected, so how it treats the file size and the stack pointer set in the firmware is an assumption (see [`SAFETY_AND_AUDIT.md`](SAFETY_AND_AUDIT.md)).

Continue with [`CURA_SETUP.md`](CURA_SETUP.md) and [`LEVELING_AND_Z_OFFSET.md`](LEVELING_AND_Z_OFFSET.md).
