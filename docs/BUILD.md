# Building KNE from source

You do **not** need to build anything to try KNE: the release binary `KNE-1.0.0.bin` is in [`../releases/v1.0.0/`](../releases/v1.0.0/). Build it yourself if you want to change the code or check that the binary matches the source.

> **Status of the two build paths.** The **Keil MDK + Arm Compiler 5** build below is the one used for the release; the resulting firmware boots and runs on the author's Kobra Neo. The **GCC (Arm GNU Toolchain) build is experimental and currently does NOT boot** on the printer; see the last section.

## 1. Primary build: Keil MDK (uVision) with Arm Compiler 5

### What you need

| Item | Version | Notes |
|---|---|---|
| Windows PC | | |
| Keil MDK (uVision) | 5.36 or newer | Includes Arm Compiler 5 (ARMCC 5.06 update 7) |
| Device pack `HDSC.HC32F460` | 1.0.11 | From <https://github.com/OrloDavid/HC32F460> |
| `ARM.CMSIS` pack | 5.7.0 | Installed with Keil's Pack Installer |
| License | See below | A full MDK Professional license or Arm's free 30-day evaluation |

Both packs must be installed in the Arm pack root (for example `%LOCALAPPDATA%\Arm\Packs`), so that uVision finds `HDSC.HC32F460.1.0.11` and `ARM.CMSIS.5.7.0`.

### Step by step

1. Install **Keil MDK** (5.36 or newer) from Arm/Keil's official site.
2. Install the pack **ARM.CMSIS 5.7.0** (uVision > Pack Installer, pack `ARM::CMSIS`, version 5.7.0).
3. Install the pack **HDSC.HC32F460 1.0.11**: download it from <https://github.com/OrloDavid/HC32F460> and double-click the `.pack` file (or copy it as described in that repository) so it ends up in the Arm pack root.
4. Get a suitable license (see "License caveats" below), for example **File > License Management > Evaluate MDK Professional**.
5. Open `firmware/workspace/anycubic.uvprojx` in uVision. The target is `firmware_Release` and the compiler is Arm Compiler 5 (the project is set to use ARMCC, not Arm Compiler 6).
6. Choose **Project > Rebuild all target files** (F7 builds only what changed; use Rebuild for the first build).
7. When the build ends without errors, the post-build step runs `fromelf --bin` and writes the raw firmware image to `firmware/workspace/firmware.bin` (next to the project file; the intermediate `firmware.axf` is in `firmware/workspace/obj/`). Verified with the unmodified Nuclear source: the produced image is identical to the published Nuclear `firmware.bin` except for the build-date strings.
8. Copy `firmware.bin` to the SD card as described in [`INSTALL.md`](INSTALL.md). A release build is published as `KNE-1.0.0.bin`; that is the same file, and it is renamed to `firmware.bin` when installed.

The date and time of the build are embedded in the firmware text, so two builds of the same source differ in a few bytes.

### Command line

uVision can build without opening the window:

```
"C:\Keil_v5\UV4\UV4.exe" -b firmware\workspace\anycubic.uvprojx -j0 -o log.txt
```

- `-b` builds the project, `-j0` hides the GUI and `-o log.txt` writes the build log to that file. Adjust the path to `UV4.exe` to where you installed Keil.
- Read `log.txt` afterwards: the build succeeded only if it ends with `0 Error(s)`. The exit code of `UV4.exe` is 0 for success and 1 for warnings; higher values mean errors.

### Reproducibility check against Nuclear

To check that your toolchain is the right one, build the **unmodified** upstream source of NuclearPhoenixx/Kobra_Neo (v1.3.6) with exactly the same steps. The author did this: the result is identical, byte for byte, to the `firmware.bin` published by Nuclear, except **20 bytes** that hold the build date and time strings. If your result differs more than that, check the compiler version (ARMCC 5.06 update 7) and the pack versions.

### License caveats

- **Keil MDK-Lite (the unlicensed evaluation mode) cannot link this firmware.** It has a 32 KB code size limit and the link fails with error `L6050U`.
- **The free MDK Community license does not cover Arm Compiler 5**; it only covers Arm Compiler 6. Therefore it cannot be used for this build.
- You need a full **MDK Professional** license, or Arm's free **30-day MDK Professional evaluation** (**File > License Management > Evaluate MDK Professional**).
- Do **not** try to bypass the limits with cracks, license generators or patched tools: that is a license violation and this project will not help with it.

### Future work: Arm Compiler 6

A port to Arm Compiler 6 compiles with 0 errors, but it has **not been tested on hardware** and it is **not part of this release**.

## 2. Experimental: GCC (Arm GNU Toolchain)

> **EXPERIMENTAL, DOES NOT BOOT.** Firmware images built with this path (Arm GNU Toolchain 14.3 and a custom start-up file) did **not** start on the author's printer: black screen and no serial output. The cause has not been identified. The scripts are kept for reference and for contributors who want to debug it. **Do not flash binaries built this way** and do not treat them as release files.

The scripts are in [`../build/`](../build/). Toolchain download and checksum: [`../build/download_toolchain.md`](../build/download_toolchain.md).

Requirements: Windows with Git for Windows (Git Bash and `patch`), Python 3, and Arm GNU Toolchain 14.3.Rel1.

From Git Bash:

```
cd build
python -I scripts/build_all.py -j 16 --variant nuclear
```

- `-j 16` is the number of parallel jobs; use your CPU thread count.
- `--variant nuclear` builds the KNE firmware in `../firmware/` (the name is historical). Output: `build/out_nuclear/firmware.bin`.
- Without `--variant`, the "base" variant builds an unmodified checkout of NuclearPhoenixx/Kobra_Neo (point to it with `KNE_BASE_DIR`, default `../upstream_nuclear`). Output: `build/out/firmware.bin`.
- `build_all.py` runs `gen_startup.py` (start-up code), `prepare_src.py` (copies the sources and applies `patches/0001-gcc-compat.patch`) and `build.py` (compile, link, make the `.bin`).
- Add `--clean`, or delete the `out*/` and `obj*/` folders, for a clean build. Results are ignored by git.

Main settings of this path: `-O2`, `-ffunction-sections`, `-fdata-sections`, `-fno-strict-aliasing`, `-ffp-contract=off`, newlib-nano, flash from 0x8000 to 0x80000, 128 KB RAM.

If you debug this path, the static audit and its limits are described in [`SAFETY_AND_AUDIT.md`](SAFETY_AND_AUDIT.md).
