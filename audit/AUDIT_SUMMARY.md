# Audit summary

Two static (read-only) audits were done on 5 November 2026 on the KNE sources and on the first, GCC-built binaries (before version 1.0.0). **Nothing was executed during the audits.** Version 1.0.0 (KNE 1.0.0) boots and runs on the author's Kobra Neo; the release is built with Keil/Arm Compiler 5, because the GCC-built images did not boot (cause not identified).

| Audit | Result in one line |
|---|---|
| Thermal and machine safety (new logic: `M1100`, Level and center, idle shutdown, night mode, motor modes) | No critical findings; base thermal protections untouched; 5 high findings in the new logic or base power-loss recovery, 4 fixed or mitigated; power-loss recovery (off by default) still homes X and Y without raising Z |
| Boot, installation and build chain (GCC 14.3 binaries compared with the working Keil build) | No definite boot failure found. Vector table, start-up order, floating-point setup and memory layout looked consistent with the Keil build; one conditional boot risk (stack pointer) and compiler-flag issues were fixed. **Yet the GCC-built images did not boot on hardware and the cause was not found**, so the release uses Keil/Arm Compiler 5 |

Full details, what was fixed and the list of what remains unverified: [`../docs/SAFETY_AND_AUDIT.md`](../docs/SAFETY_AND_AUDIT.md).

The original audit reports are written in Spanish and are kept by the author outside this repository. A static audit cannot replace hardware testing: follow [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md).
