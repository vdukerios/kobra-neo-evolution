# Safety and audit summary

> **Version 1.0.0: boots and runs on the author's Kobra Neo. Each feature still needs validation by more users; see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md).** Use at your own risk; keep a backup firmware and an SD card with a known-good `firmware.bin`. The audits below are **static reviews**: someone read the code and the disassembly. They found problems, some were fixed, and they cannot replace testing on hardware. Watch the heaters and the motors during your first tests.

Two audits were done on 5 November 2026 (the original reports are in Spanish and are not part of this repository; this page summarizes them). They were made on the first KNE builds, which were compiled with GCC (see section 2). Marks used below: **[verified]** = confirmed by reading code or disassembly; **[assumption]** = could not be confirmed without the printer.

## 1. Thermal and machine safety audit

**Scope:** all the new logic (print modes `M1100`, Level and center, idle shutdown, night mode, motor modes) and how it interacts with the base firmware. Read-only: nothing was run. Some new files (log/learning, screen queue, canvas, guide) were **not** reviewed in depth.

**Main result:** no critical findings. The thermal protections of the base firmware (thermal runaway and temperature limits, watchdog, minimum/maximum temperature) do **not** appear in the list of changes and were left intact **[verified]**. The idle shutdown was switched on, which adds protection. Five findings were rated high.

| Finding | What could happen | Status |
|---|---|---|
| A failed bed mesh (`G29`) was not detected by "Level and center": the nozzle could go down to 0.5 mm with an empty mesh | Possible nozzle-to-bed crash after a probe failure | **Fixed**: a dedicated failure flag is set by the leveling routine and checked by the button; the action aborts on failure. A nested `G29 J` can no longer clear it |
| `M1100` did not detect a failed mesh: the print could start with no mesh and the bed off | Printing blind, heaters in an odd state | **Fixed**: after the mesh, `M1100` checks the failure flag; on failure it cancels the preparation and switches the heaters off |
| Idle shutdown could trigger while Express was still waiting for the bed to heat, switching nozzle and bed off | Print would start with a cold bed | **Fixed**: the idle logic now treats heat-up waits and active printing as "busy" |
| Heaters left on indefinitely after "Level and center" (nozzle 120 C, bed 60 C) | No time limit on heaters | **Mitigated**: the idle shutdown now also watches the bed and starts at 100 C, so these heaters switch off after 10 minutes |
| Power-loss recovery homes X and Y at the height of the print, without raising Z first | Nozzle may drag across or knock the part | **Not fixed.** Base firmware behavior; the feature is **off by default**. Do not enable it unless you accept this |

Other findings, medium and low:

| Finding | Status |
|---|---|
| `G29` inside Full mode set the bed to its own preheat temperature (60 C) instead of the requested one | **Fixed** [verified]: `M1100` re-applies the requested bed temperature after the mesh |
| Express accepted an empty mesh without warning | **Fixed**: Express runs only if a valid mesh exists, otherwise it falls back to Full |
| Pressing Stop during `G29` or a long `M1100` wait is applied late | Not re-checked |
| Night mode: some low-level messages do not wake the screen; the muted buzzer also mutes `M300` and filament-change beeps; the screen can go black again while waiting for you during a filament change | Not re-checked. Critical error screens do turn night mode off [verified] |
| Bed stays on indefinitely while paused (nozzle turns off after 45 s) | Not re-checked |
| Motor current limit of 1200 mA is on the high side and `M906` is not limited; High speed uses 900 mA | Not changed; the audit suggests lowering the limit to about 1000 mA |
| Low: `M1100` S/T values have no upper limit of their own (Marlin's own temperature limits still apply); the thumbnail reader may block briefly on a slow SD card; removing the SD card during `M1100` is not handled specially; a cancel during the final lowering finishes the current move first | Open, low risk |

## 2. Boot, installation and build-chain audit

**Outcome in one paragraph.** The static audit of the GCC-built images found **no definite boot failure**, but those GCC-built images **did not boot** on the author's printer (black screen, no serial output). **The cause was not identified.** That is why the release is built with **Keil MDK and Arm Compiler 5** instead: that path reproduces the published Nuclear v1.3.6 firmware byte for byte (except 20 bytes of build-date strings), and the resulting KNE firmware boots and runs. The GCC scripts remain in `build/` as an experimental, non-booting path ([`BUILD.md`](BUILD.md)). The findings below are therefore kept as the record of what was checked, not as proof that a GCC image would start.

**Scope:** the GCC-built binaries (an unmodified Nuclear rebuild and the new firmware) were compared against the reference Keil builds by static disassembly (vector table, start-up code, floating-point unit setup, memory layout, linker map, binary format). **Nothing was executed during the audit.**

**Verified OK in the static audit [verified]:** vector table (valid stack pointer and reset address, no even or out-of-range entries, same reserved slots and interrupts as the working reference), the vector table offset is set as the first action of `main`, the floating-point unit is enabled before any floating-point instruction, data/bss initialization and C++ constructors in the same order as Keil, the linker layout (flash from 0x8000, nothing outside it, 128 KB RAM, about 77 % used, no overlaps), the binary is a plain raw image like the working reference, and the compatibility patch changes only types and attributes, not logic.

| Finding | Status |
|---|---|
| **A1 (high, conditional):** the GCC start-up code did not set the stack pointer, while Keil does. If the bootloader leaves its own stack in the area the firmware clears, boot would fail or memory would be corrupted | **Fixed: the reset code now sets the stack pointer as its first instruction** (verified in the disassembly of both released binaries: `ldr r0, =_estack; mov sp, r0`; it lives in `build/scripts/gen_startup.py`, which generates the start-up file). The bootloader is protected and cannot be inspected, so its behavior is an **[assumption]** |
| A2 (medium): stack depth with GCC `-O2` not measured | **Open.** Needs a measurement on hardware or `-fstack-usage` analysis; there is RAM left to enlarge the stack if needed |
| A3 (medium): GCC was not told `-fno-strict-aliasing`, and 32-bit types changed from `int` to `long` | **Fixed**: `-fno-strict-aliasing` added to the compile flags |
| A4 (medium/low): GCC fuses multiply-add operations by default, which can change the last digits of some results compared with Keil | **Fixed**: `-ffp-contract=off` added |
| B1: system init runs before data initialization (same as Keil) | No action needed |
| B2: all fault handlers are an endless loop (silent freeze, no diagnosis); heaters are not switched off by a fault handler | **Open**, same as the original. A fault handler that turns heaters off is a possible improvement |
| B3/B4: `printf` output is discarded; newlib-nano heap use not measured | Open, low |
| B5: builds differ in the embedded build date | By design, harmless |

## 3. What remains unverified

Confirmed on one printer (the author's): the Keil/ARMCC5 build boots, and the menus work in English and Spanish. Everything else needs the real printer and more testers:

- Why the GCC-built images do not boot (unknown). This only matters if you want to revive the GCC path.
- Maximum stack and heap use at run time.
- Timing-critical code (step pulses, temperature reading, serial, SPI, DMA) over long prints.
- Every motor mode, night mode, thumbnail, SD logging, time learning, `M1100` Full and Express, and Level and center.
- Behavior of the thermal protections on this build (they are unchanged in the source; check them yourself, see the test plan).
- The parts of the new code that were not audited in depth (log/learning, screen drawing queue, canvas, guide).

Whatever you test, follow [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md): unheated checks first, then heat, then one short print, and never leave the printer alone.
