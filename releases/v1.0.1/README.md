# KNE 1.0.1 (pre-release)

`KNE-1.0.1.bin` is the firmware built from this repository's source with Keil MDK 5 / Arm Compiler 5 (install it by renaming it to `firmware.bin`, see [`docs/INSTALL.md`](../../docs/INSTALL.md)).

- SHA-256: `939d3336ba3152586bff4ce1c7a42c6db333da767e42e0785c34eedd8f9d6e1d` (404 940 bytes).
- Validation: it **boots and ran on the author's Kobra Neo** (one printer). It is **not** a validated release; each feature still needs validation by more users ([`testing/TEST_PLAN.md`](../../testing/TEST_PLAN.md)).
- Changes: [`docs/CHANGES_2026-11-07.md`](../../docs/CHANGES_2026-11-07.md) and [`docs/CHANGES_2026-11-08.md`](../../docs/CHANGES_2026-11-08.md).
- The first boot restores factory settings; back up `M503` first and keep a known-good `firmware.bin` on an SD card.
- Known open item: the estimated-time line on the "Confirm print" screen is not included (it stopped the firmware from booting).
