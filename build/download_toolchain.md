# Toolchains

> **Which one do I need?** To build the firmware that is released (`KNE-1.0.0.bin`) you need **Keil MDK with Arm Compiler 5**, not anything described in the GCC section below. See [`../docs/BUILD.md`](../docs/BUILD.md) for the step-by-step instructions.

## Primary: Keil MDK (uVision) 5.36+ with Arm Compiler 5

Nothing in this repository downloads or bundles Keil; install it from Arm/Keil's official site.

| Item | Version | Where |
|---|---|---|
| Keil MDK (uVision) with Arm Compiler 5 (ARMCC 5.06 update 7) | 5.36 or newer | Arm/Keil official site |
| `HDSC.HC32F460` device pack | 1.0.11 | <https://github.com/OrloDavid/HC32F460> |
| `ARM.CMSIS` pack | 5.7.0 | uVision Pack Installer |

Install both packs in the Arm pack root (for example `%LOCALAPPDATA%\Arm\Packs`). Then open `firmware/workspace/anycubic.uvprojx` and rebuild.

License notes (do not circumvent them):

- MDK-Lite cannot link this firmware (32 KB code limit, error `L6050U`).
- The free MDK Community license does **not** cover Arm Compiler 5 (only Arm Compiler 6).
- Use a full MDK Professional license, or Arm's free 30-day evaluation (**File > License Management > Evaluate MDK Professional**).

Command-line build: `UV4.exe -b <project> -j0 -o log.txt` (details in [`../docs/BUILD.md`](../docs/BUILD.md)).

## Experimental: Arm GNU Toolchain 14.3.Rel1 (GCC) - DOES NOT BOOT

> **EXPERIMENTAL.** Binaries built with the scripts in this folder (GCC 14.3 and a custom start-up file) did **not** boot on the author's printer (black screen, no serial output). The cause is unknown. The scripts are kept for reference and for contributors who want to debug the problem. Do not flash binaries built this way; they are not the release.

The compiler is not part of this repository (it weighs hundreds of MB). Download it once if you want to work on this path.

### 1. Download
- Official page: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads (version **14.3.Rel1**).
- Windows file (mingw-w64 x86_64 host, `arm-none-eabi` target):
  `arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi.zip`
- Expected direct URL: https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi.zip (if Arm changes the path, go through the official page).
- **Expected SHA256** (from `sha256.txt`):
  `864c0c8815857d68a1bbba2e5e2782255bb922845c71c97636004a3d74f60986`
- Check in Git Bash: `sha256sum arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi.zip`. If it does not match, do not use it.

### 2. Where to put it
Extract the zip so that the compiler binaries end up in `build/toolchain/bin/` (i.e. `build/toolchain/bin/arm-none-eabi-gcc.exe`). You may keep the downloaded zip in `build/dl/`. Both folders are listed in `.gitignore`.

### 3. Build (from Git Bash, inside the `build/` folder)
```
python -I scripts/build_all.py -j 16 --variant nuclear
```
- Requires Python 3 and the `patch` program, which ships with Git for Windows (Git Bash).
- `-j 16` = number of parallel jobs; adjust it to your CPU.
- `--variant nuclear` builds the KNE firmware in `../firmware/` (the variant name is historical).
- Without `--variant` the "base" variant builds an **unmodified** checkout of NuclearPhoenixx/Kobra_Neo. Clone that repo yourself and point to it with the environment variable `KNE_BASE_DIR` (default: `../upstream_nuclear`).
- Results go to the `out*/` and `obj*/` folders (ignored by git).
