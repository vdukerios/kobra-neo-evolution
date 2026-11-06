# Architecture: switches, texts and file map

> **Version 1.0.0 boots and runs on the author's printer; features are still being validated** (see [`../testing/TEST_PLAN.md`](../testing/TEST_PLAN.md)). This page is for people who want to change or disable parts of the firmware. Changing any setting requires a rebuild ([`BUILD.md`](BUILD.md)). Some source comments and header names are in Spanish (the author's language); identifiers are kept as they are.

All paths are relative to `firmware/source/Marlin/`.

## One switch per feature

The new features are controlled from small header files in `src/inc/valentin/`. They are all included by `src/inc/valentin/all.h`, which `Configuration_adv.h` includes at its end. To turn a feature off, comment out its main `#define` (put `//` in front of it) and rebuild.

| Feature | Header | Main switch | Notes |
|---|---|---|---|
| Print log and learned remaining-time factor | `registro_aprendizaje.h` | `REGISTRO_IMPRESIONES` (log), `APRENDIZAJE_TIEMPOS` (time factor) | Both need `SDSUPPORT` and `PRINTCOUNTER`; without them they switch off by themselves. File names, number of samples and factor limits are in the same header |
| "Level and center" button | `boton_nivelar.h` | `BOTON_NIVELAR_CENTRAR` | Requires `BABYSTEP_ZPROBE_OFFSET` (build stops with an error if missing). Heights, speeds and the bed-center position are set here |
| Night mode | `modo_noche.h` | `MODO_NOCHE` | Timings are set here. Optional, off by default: `NOCHE_PERFIL_SUAVE` (lower speed in night mode), `NOCHE_LUZ_FONDO` (switch the backlight pin off; 320x240 only, not validated) |
| Model thumbnail | `miniatura.h` | `MINIATURA_GCODE` | Maximum side 80 px (a larger side costs RAM) and search window of 24 KB |
| New home and print screens | `pantalla.h` | `PANTALLA_NUEVA` | Commenting it restores the original interface |
| Guide and long-press help | `guia_ayuda.h` | `GUIA_AYUDA` (guide menu), `GUIA_AYUDA_LARGA` (long press) | Long press needs the guide; `GUIA_AYUDA_MS` (800) is the hold time. Disabling the long press also restores "click on press" in menus |
| Motor modes | `modos_motor.h` | `MODOS_MOTOR` | Thresholds and currents per mode are in the same header |
| Print modes (`M1100` and Confirm print) | `modos_impresion.h` | `MODOS_IMPRESION` | |

Other features are standard Marlin switches in `Configuration.h` / `Configuration_adv.h`:

| Feature | Switch |
|---|---|
| Idle heater shutdown | `HOTEND_IDLE_TIMEOUT` (`HOTEND_IDLE_TIMEOUT_SEC`, `HOTEND_IDLE_MIN_TRIGGER`) |
| Print counter and reminders | `PRINTCOUNTER`, `SERVICE_NAME_n` / `SERVICE_INTERVAL_n` |
| SD files sorted | `SDCARD_SORT_ALPHA` (`SDSORT_LIMIT`) |
| Sound menu | `SOUND_MENU_ITEM` |
| Preheat values | `PREHEAT_n_*` |
| Junction deviation / classic jerk | `JUNCTION_DEVIATION_MM`; enable `CLASSIC_JERK` to go back to jerk |
| Mesh fade height | `ENABLE_LEVELING_FADE_HEIGHT`, `DEFAULT_LEVELING_FADE_HEIGHT` |
| Languages | `LCD_LANGUAGE` (en), `LCD_LANGUAGE_2` (es), `LCD_LANGUAGE_AUTO_SAVE` |
| Power-loss recovery | `POWER_LOSS_RECOVERY`, `PLR_ENABLED_DEFAULT` (false) |

## Language string tables

- User-interface texts: `src/lcd/language/language_en.h` and `src/lcd/language/language_es.h`. The new texts are named `MSG_KNE_*` (57 in each file, for example `MSG_KNE_GUIDE`, `MSG_KNE_MOTOR_MIXED`, `MSG_KNE_CONFIRM_PRINT`). The "Level and center" texts are `MSG_NIVELAR_CENTRAR` and `MSG_BNC_*`. Every `MSG_KNE_*` must exist in both files; use it in code with `GET_TEXT(MSG_KNE_...)`.
- Guide and help texts: `src/lcd/menu/menu_guia_textos.h`. They are plain ASCII (no accents) so they do not depend on the screen font, at most 33 characters per line and 5 lines per page. Only English and Spanish exist; any other interface language shows English.
- To add a language, add a `language_xx.h` with all the strings, select it in `LCD_LANGUAGE_n` (up to 5 slots) and extend the guide text table.

## File map of the new code

| Area | Files |
|---|---|
| Feature switches | `src/inc/valentin/*.h` |
| Print modes | `src/gcode/feature/modos_impresion/M1100.cpp` (the command), `src/feature/modos_impresion.h` (mode state), `src/lcd/menu/menu_media.cpp` (Confirm print screen), `src/gcode/gcode.cpp` / `gcode.h` (command registration) |
| Bed mesh failure flag | `src/gcode/bedlevel/abl/G29.cpp` (`g29_fallo`) |
| Level and center | `src/lcd/menu/menu_main.cpp` (button and state machine) |
| Motor modes | `src/lcd/menu/menu_configuration.cpp` (menu and apply), `src/module/settings.cpp` (restore from saved settings) |
| Idle shutdown | `src/feature/hotend_idle.cpp` / `.h` |
| Print log and time learning | `src/feature/registro_aprendizaje.cpp` / `.h`, `src/gcode/sd/M1001.cpp` (end-of-file hook), `src/libs/stopwatch.cpp` |
| Thumbnail reader | `src/feature/gcode_thumb.cpp` / `.h` |
| New screens | `src/lcd/tft/ui_320x240.cpp`, `src/lcd/tft/canvas.cpp` / `.h`, `src/lcd/tft/tft_queue.cpp` / `.h`, `src/lcd/tft/tft.h` |
| Guide and long-press help | `src/lcd/menu/menu_guia.cpp`, `src/lcd/menu/menu_guia_textos.h`, `src/lcd/menu/menu.cpp`, `src/lcd/menu/menu_item.h` |
| Night mode | `src/lcd/marlinui.cpp` / `.h`, `src/libs/buzzer.cpp`, `src/feature/runout.cpp` |
| Other menus touched | `src/lcd/menu/menu_info.cpp` |
| Core | `src/MarlinCore.cpp` |

## Build-side files (outside `firmware/`)

| File | Purpose |
|---|---|
| `firmware/workspace/anycubic.uvprojx` | The Keil project used for the release build (Arm Compiler 5); see [`BUILD.md`](BUILD.md) |
| `build/scripts/build.py` | **Experimental, non-booting** GCC path: compiles and links with Arm GNU GCC; compiler flags are set here |
| `build/scripts/gen_startup.py` | Generates `build/gcc_support/startup_hc32f460.S` (vector table and reset code) |
| `build/gcc_support/hc32f460.ld`, `syscalls.c` | Linker script and minimal C library support |
| `build/patches/0001-gcc-compat.patch` | Small changes so the Keil-oriented source builds with GCC (no change of logic) |
