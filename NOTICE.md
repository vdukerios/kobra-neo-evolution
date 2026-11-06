# Notices, credits and licenses

## No affiliation
Kobra Neo Evolution (KNE) is **not affiliated with, endorsed by or sponsored by Anycubic**. "Anycubic" and "Kobra" are trademarks of their respective owners and are used only to indicate compatibility with the printer.

## Use at your own risk
Version 1.0.0 boots and runs on the author's Kobra Neo. Each feature still needs validation by more users (see `testing/TEST_PLAN.md`). Installing it may leave the printer unusable, damage it, or create heat/fire hazards. It is provided "AS IS", without warranty of any kind (see GPL-3.0). Always keep a backup firmware and an SD card with a known-good `firmware.bin`, and never leave the printer unattended while testing.

## Credits
- **Marlin Firmware** (https://marlinfw.org) - GPL-3.0 or later. Basis of the whole firmware.
- **Anycubic** - Marlin source code adapted to the Kobra Neo, published by the manufacturer under the GPL.
- **NuclearPhoenixx/Kobra_Neo** (https://github.com/NuclearPhoenixx/Kobra_Neo) - the firmware this project directly derives from. Its original README is in `firmware/README_NUCLEAR.md`, including the list of projects it builds on (jokubasver, sjorge, jojos38, Auburn, Kuba1842, 1coderookie).
- **shadow578/Marlin-H32** (https://github.com/shadow578/Marlin-H32) - Marlin port for the HC32F46x microcontroller; mentioned in Nuclear's README as a reference. [To verify] No code copied from it was found in this repository; if any is incorporated, it must be credited here.
- **Huada Semiconductor (HDSC)** - microcontroller libraries (under `firmware/source/drivers`, `component`, `board`, etc.).
- Other authors appear in the headers of the source files (Arduino, LeafLabs, Bob Cousins and others); they are kept as is.

## Third-party license notes
- The project as a whole is distributed under **GPL-3.0** (`LICENSE`).
- The **HDSC** library files carry their own clause: "licensed by HDSC to be adapted only for use in systems utilizing HDSC components... AS IS". This is not a standard GPL license; the files are kept unchanged and are only valid for devices with HDSC microcontrollers (the Kobra Neo has one). Whether this is compatible with a GPL-3.0 distribution is an open question for the author.
- Some bitmap font sources carry their own notices (Adobe, Digital Equipment); they are kept.
- **Keil MDK / Arm Compiler 5** (used to build the release) and the **Arm GNU Toolchain** (experimental GCC path) are NOT included; get them from their official sites (see `docs/BUILD.md` and `build/download_toolchain.md`). Keil needs a license that covers Arm Compiler 5.
- The **HDSC.HC32F460** device pack needed for the Keil build comes from <https://github.com/OrloDavid/HC32F460> and is not included.
- Backups of the Nuclear firmware and of version 1.3.3 are NOT included: they are not ours. Get them from their original repositories.
