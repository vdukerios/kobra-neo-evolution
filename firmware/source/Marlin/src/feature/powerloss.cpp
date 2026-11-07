/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * feature/powerloss.cpp - Resume an SD print after power-loss
 */

#include "../inc/MarlinConfigPre.h"

#if ENABLED(POWER_LOSS_RECOVERY)

#include "powerloss.h"
#ifdef REGISTRO_APRENDIZAJE_ACTIVO
  #include "registro_aprendizaje.h"
#endif
#include "power_monitor.h"
#include "../core/macros.h"
#include "../module/stepper/indirection.h"
#include "../HAL/shared/eeprom_api.h"

#if ENABLED(BABYSTEPPING)
#include "babystep.h"
#endif

#include "bltouch.h"
bool PrintJobRecovery::outage_monitor_enabled = false;
bool PrintJobRecovery::enabled; // Initialized by settings.load()

SdFile PrintJobRecovery::file;
job_recovery_info_t PrintJobRecovery::info;
const char PrintJobRecovery::filename[10] = "/log.txt";
uint8_t PrintJobRecovery::queue_index_r;
uint32_t PrintJobRecovery::cmd_sdpos, // = 0
         PrintJobRecovery::sdpos[BUFSIZE];

#if ENABLED(DWIN_CREALITY_LCD)
  bool PrintJobRecovery::dwin_flag; // = false
#endif

#include "../sd/cardreader.h"
#include "../lcd/marlinui.h"
#include "../gcode/queue.h"
#include "../gcode/gcode.h"
#include "../module/motion.h"
#include "../module/planner.h"
#include "../module/printcounter.h"
#include "../module/temperature.h"
#include "../core/serial.h"
#include "../MarlinCore.h"
#include "../libs/crc16.h"
#include <stddef.h>
#if ENABLED(ADVANCED_PAUSE_FEATURE)
  #include "pause.h"
#endif

#if ENABLED(FWRETRACT)
  #include "fwretract.h"
#endif

#if ENABLED(EXTENSIBLE_UI)
  #include "../lcd/extui/ui_api.h"
#endif
#define DEBUG_OUT ENABLED(DEBUG_POWER_LOSS_RECOVERY)
#include "../core/debug_out.h"

static const uint32_t RECOVERY_MAGIC = 0x4B4E4532UL;
static const uint16_t RECOVERY_VERSION = 1;
static uint16_t recovery_checksum(const job_recovery_info_t &saved) {
  uint16_t crc = 0;
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&saved);
  const size_t offset = offsetof(job_recovery_info_t, checksum);
  crc16(&crc, bytes, offset);
  crc16(&crc, bytes + offset + sizeof(saved.checksum), sizeof(saved) - offset - sizeof(saved.checksum));
  return crc;
}

bool job_recovery_info_t::valid() const {
  if (!PrintJobRecovery::supported) return false;
  if (!valid_head || valid_head == 0xFF || valid_head != valid_foot
      || format_magic != RECOVERY_MAGIC || format_version != RECOVERY_VERSION
      || format_size != sizeof(*this) || checksum != recovery_checksum(*this)) return false;
  if (!memchr(sd_filename, 0, sizeof(sd_filename)) || !sd_filename[0]
      || !memchr(display_filename, 0, sizeof(display_filename))) return false;
  LOOP_XYZE(i) if (!isfinite(current_position[i]) || ABS(current_position[i]) > 1000000.0f) return false;
  if (!WITHIN(current_position.x, X_MIN_POS, X_MAX_POS)
      || !WITHIN(current_position.y, Y_MIN_POS, Y_MAX_POS)
      || !WITHIN(current_position.z, Z_MIN_POS, Z_MAX_POS)
      || !isfinite(zraise) || zraise < 0 || zraise > Z_MAX_POS - Z_MIN_POS
      || !feedrate || print_progress > 100 || axis_relative >= 64) return false;
  #if HAS_HOTEND
    HOTEND_LOOP() if (target_temperature[e] < 0 || target_temperature[e] > thermalManager.hotend_max_target(e)) return false;
  #endif
  #if HAS_HEATED_BED
    if (target_temperature_bed < 0 || target_temperature_bed > BED_MAX_TARGET) return false;
  #endif
  #if HAS_LEVELING
    if (!isfinite(fade) || fade < 0 || fade > Z_MAX_POS) return false;
  #endif
  #if HAS_MULTI_EXTRUDER
    if (active_extruder >= EXTRUDERS) return false;
  #endif
  #if DISABLED(NO_VOLUMETRICS)
    if (volumetric_enabled) LOOP_L_N(e, EXTRUDERS)
      if (!isfinite(filament_size[e]) || filament_size[e] <= 0 || filament_size[e] > 1000) return false;
  #endif
  #if HAS_HOME_OFFSET
    LOOP_XYZ(i) if (!isfinite(home_offset[i]) || ABS(home_offset[i]) > 1000000.0f) return false;
  #endif
  #if HAS_POSITION_SHIFT
    LOOP_XYZ(i) if (!isfinite(position_shift[i]) || ABS(position_shift[i]) > 1000000.0f) return false;
  #endif
  #if ENABLED(FWRETRACT)
    LOOP_L_N(e, EXTRUDERS) if (!isfinite(retract[e]) || ABS(retract[e]) > 1000000.0f) return false;
    if (!isfinite(retract_hop) || ABS(retract_hop) > 1000000.0f) return false;
  #endif
  return true;
}

PrintJobRecovery recovery;

#ifndef POWER_LOSS_PURGE_LEN
  #define POWER_LOSS_PURGE_LEN 0
#endif
#ifndef POWER_LOSS_ZRAISE
  #define POWER_LOSS_ZRAISE 0     // Move on loss with backup power, or on resume without it
#endif

#if DISABLED(BACKUP_POWER_SUPPLY)
  #undef POWER_LOSS_RETRACT_LEN   // No retract at outage without backup power
#endif
#ifndef POWER_LOSS_RETRACT_LEN
  #define POWER_LOSS_RETRACT_LEN 0
#endif

/**
 * Clear the recovery info
 */
void PrintJobRecovery::init() { enabled = supported && enabled; memset(&info, 0, sizeof(info)); }

void PrintJobRecovery::report_unavailable() {
  SERIAL_ECHO_MSG("Recovery unavailable: executed position not verified");
  #if HAS_DISPLAY
    ui.set_status_P(TERN0(HAS_MULTI_LANGUAGE, ui.language > 0)
      ? PSTR("Recovery unavailable") : PSTR("Recuperacion no disponible"));
  #endif
}

/**
 * Enable or disable then call changed()
 */
void PrintJobRecovery::enable(const bool onoff) {
  if (!supported) {
    if (!onoff) outage_monitor_enabled = false; // Explicit disarm remains available.
    init(); report_unavailable(); return;
  }
  enabled = onoff;
  changed();
}

/**
 * The enabled state was changed:
 *  - Enabled: Purge the job recovery file
 *  - Disabled: Write the job recovery file
 */
void PrintJobRecovery::changed() {
  if (!supported) { init(); return; }
  if (!enabled)
    purge();
  else if (IS_SD_PRINTING())
    save(true);
}

/**
 * Check for Print Job Recovery during setup()
 *
 * If a saved state exists send 'M1000 S' to initiate job recovery.
 */
void PrintJobRecovery::check() {
  if (!supported) { init(); report_unavailable(); return; }
  //if (!card.isMounted()) card.mount();
  if(card.isMounted()) {
    load();
    #ifdef REGISTRO_APRENDIZAJE_ACTIVO
      registro_arranque(valid()); // anota una impresion interrumpida (corte de luz)
    #endif
    if (!valid()) 	 return cancel();
	
	//open(false);
	//file.writeln_P("creat log.txt");
    gcode.process_subcommands_now_P(PSTR("M1000S"));
  }
}

/**
 * Delete the recovery file and clear the recovery data
 */
void PrintJobRecovery::purge() {
	
#if 0
	  card.removeJobRecoveryFile();
#endif
	
	  if(info.valid_head != 0xFF || info.valid_foot != 0xFF) {
        if (persistentStore.FLASH_If_Erase(FLASH_OUTAGE_DATA_ADDR, FLASH_OUTAGE_DATA_ADDR+0x400) != FLASHIF_OK)
          SERIAL_ERROR_MSG("Recovery erase failed");
	  }
	
	  memset(&info, 0, sizeof(info));	// init();

}

/**
 * Load the recovery data, if it exists
 */
void PrintJobRecovery::load() {
  if (!supported) { init(); return; }
     memcpy(&info, (uint8_t *)(FLASH_OUTAGE_DATA_ADDR), sizeof(info));
     if (!info.valid()) init();
}

/**
 * Set info fields that won't change
 */
void PrintJobRecovery::prepare() {
  if (!supported) { init(); return; }
  card.getAbsFilename(info.sd_filename);  // SD filename
  cmd_sdpos = card.getIndex();
}

/**
 * Save the current machine state to the power-loss recovery file
 */
bool PrintJobRecovery::save(const bool force/*=false*/, const float zraise/*=0*/) {
  if (!supported) { init(); return false; }

  #if SAVE_INFO_INTERVAL_MS > 0
    static millis_t next_save_ms; // = 0
    millis_t ms = millis();
  #endif

  #ifndef POWER_LOSS_MIN_Z_CHANGE
    #define POWER_LOSS_MIN_Z_CHANGE 0.05  // Vase-mode-friendly out of the box
  #endif

  // Did Z change since the last call?
  if (force
    #if DISABLED(SAVE_EACH_CMD_MODE)      // Always save state when enabled
      #if SAVE_INFO_INTERVAL_MS > 0       // Save if interval is elapsed
        || ELAPSED(ms, next_save_ms)
      #endif
      // Save if Z is above the last-saved position by some minimum height
      || current_position.z > info.current_position.z + POWER_LOSS_MIN_Z_CHANGE
    #endif
  ) {

    #if SAVE_INFO_INTERVAL_MS > 0
      next_save_ms = ms + SAVE_INFO_INTERVAL_MS;
    #endif

    // Set Head and Foot to matching non-zero values
    if (!++info.valid_head || info.valid_head == 0xFF) info.valid_head = 1; // non-zero in sequence
    //if (!IS_SD_PRINTING()) info.valid_head = 0;
    info.valid_foot = info.valid_head;
//	SERIAL_ECHOLNPAIR("head=", info.valid_head,"foot=", info.valid_foot);

    // Machine state
    info.current_position = current_position;
    const float saved_feedrate = MMS_TO_MMM(feedrate_mm_s);
    if (!isfinite(saved_feedrate) || saved_feedrate < 1 || saved_feedrate > UINT16_MAX) {
      SERIAL_ERROR_MSG("Recovery feedrate out of range"); return false;
    }
    info.feedrate = uint16_t(saved_feedrate);
    info.zraise = zraise;

    TERN_(GCODE_REPEAT_MARKERS, info.stored_repeat = repeat);
    TERN_(HAS_HOME_OFFSET, info.home_offset = home_offset);
    TERN_(HAS_POSITION_SHIFT, info.position_shift = position_shift);

    #if HAS_MULTI_EXTRUDER
      info.active_extruder = active_extruder;
    #endif

    #if DISABLED(NO_VOLUMETRICS)
      info.volumetric_enabled = parser.volumetric_enabled;
      #if HAS_MULTI_EXTRUDER
        for (int8_t e = 0; e < EXTRUDERS; e++) info.filament_size[e] = planner.filament_size[e];
      #else
        if (parser.volumetric_enabled) info.filament_size[0] = planner.filament_size[active_extruder];
      #endif
    #endif

    #if EXTRUDERS
      HOTEND_LOOP() info.target_temperature[e] = thermalManager.temp_hotend[e].target;
    #endif

    TERN_(HAS_HEATED_BED, info.target_temperature_bed = thermalManager.temp_bed.target);

    #if HAS_FAN
      COPY(info.fan_speed, thermalManager.fan_speed);
    #endif

    #if HAS_LEVELING
      info.flag.leveling = planner.leveling_active;
      info.fade = TERN0(ENABLE_LEVELING_FADE_HEIGHT, planner.z_fade_height);
    #endif

    TERN_(GRADIENT_MIX, memcpy(&info.gradient, &mixer.gradient, sizeof(info.gradient)));

    #if ENABLED(FWRETRACT)
      COPY(info.retract, fwretract.current_retract);
      info.retract_hop = fwretract.current_hop;
    #endif

    // Elapsed print job time
    info.print_job_elapsed = print_job_timer.duration();

    info.print_progress = card.percentDone();

    // Relative axis modes
    info.axis_relative = gcode.axis_relative;

    // Misc. Marlin flags
    info.flag.dryrun = !!(marlin_debug_flags & MARLIN_DEBUG_DRYRUN);
    info.flag.allow_cold_extrusion = TERN0(PREVENT_COLD_EXTRUSION, thermalManager.allow_cold_extrude);

    if (!write()) { info.valid_head = info.valid_foot = 0; return false; }

  }
  return true;
}

#if PIN_EXISTS(POWER_LOSS)
    void PrintJobRecovery::outage() {

      static uint8_t cnt = 0;
      static uint32_t adc_raw_last = 0;

      if (!(supported ? enabled : outage_monitor_enabled)) {
        return ;
      }

        if(power_monitor.getVoltsADC() < 3000) {

        //SERIAL_ECHOLNPAIR("v:", power_monitor.getVoltsADC());

        if(cnt >= 2 ) {
            _outage();
			
        }

        if(power_monitor.getVoltsADC() < adc_raw_last) {
            cnt++;
        }

      } else {

        if(cnt != 0) {
            cnt = 0;
        }
      }

      adc_raw_last = power_monitor.getVoltsADC();
    }

    void PrintJobRecovery::adc_raw() {
    }

  #if ENABLED(BACKUP_POWER_SUPPLY)

    void PrintJobRecovery::retract_and_lift(const float &zraise) {
      #if POWER_LOSS_RETRACT_LEN || POWER_LOSS_ZRAISE

        gcode.set_relative_mode(true);  // Use relative coordinates

        #if POWER_LOSS_RETRACT_LEN
          // Retract filament now
          gcode.process_subcommands_now_P(PSTR("G1 F3000 E-" STRINGIFY(POWER_LOSS_RETRACT_LEN)));
        #endif

        #if POWER_LOSS_ZRAISE
          // Raise the Z axis now
          if (zraise) {
            char cmd[20], str_1[16];
            sprintf_P(cmd, PSTR("G0 Z%s"), dtostrf(zraise, 1, 3, str_1));
            gcode.process_subcommands_now(cmd);
          }
        #else
          UNUSED(zraise);
        #endif

        //gcode.axis_relative = info.axis_relative;
        planner.synchronize();
      #endif
    }

  #endif

  /**
   * An outage was detected by a sensor pin.
   *  - If not SD printing, let the machine turn off on its own with no "KILL" screen
   *  - Disable all heaters first to save energy
   *  - Save the recovery data for the current instant
   *  - If backup power is available Retract E and Raise Z
   *  - Go to the KILL screen
   */
  void PrintJobRecovery::_outage() {
    #if ENABLED(BACKUP_POWER_SUPPLY)
      static bool lock = false;
      if (lock) return; // No re-entrance from idle() during retract_and_lift()
      lock = true;
    #endif

  //    TERN_(USE_WATCHDOG, HAL_watchdog_refresh());
  //    __disable_irq();
  
    #if POWER_LOSS_ZRAISE
      // Get the limited Z-raise to do now or on resume
      const float zraise = _MAX(0, _MIN(current_position.z + POWER_LOSS_ZRAISE, Z_MAX_POS - 1) - current_position.z);
    #else
      constexpr float zraise = 0;
    #endif
  
    WRITE(HEATER_0_PIN, 0);
    WRITE(HEATER_BED_PIN, 0);
//    ExtUI::onPowerLoss();
    WRITE(X_ENABLE_PIN, 1);
    WRITE(Y_ENABLE_PIN, 1);
    WRITE(Z_ENABLE_PIN, 1);
  
    // Save, including the limited Z raise
    if (supported && IS_SD_PRINTING()) save(true, zraise);
  
    // Disable all heaters to reduce power loss
    thermalManager.disable_all_heaters();
  
    #if ENABLED(BACKUP_POWER_SUPPLY)
      // Do a hard-stop of the steppers (with possibly a loud thud)
      quickstop_stepper();
      // With backup power a retract and raise can be done now
      if (supported) retract_and_lift(zraise);
    #endif
  
    //kill(GET_TEXT(MSG_OUTAGE_RECOVERY));
	minkill();
	
  }

#endif

/**
 * Save the recovery info the recovery file
 */
bool PrintJobRecovery::write() {
  if (!supported) return false;
  info.format_magic = RECOVERY_MAGIC;
  info.format_version = RECOVERY_VERSION;
  info.format_size = sizeof(info);
  job_recovery_info_t snapshot;
  memcpy(&snapshot, &info, sizeof(snapshot));
  // Only sdpos is updated by the stepper interrupt; capture its aligned word once.
  snapshot.sdpos = info.sdpos;
  snapshot.checksum = recovery_checksum(snapshot);
  info.checksum = snapshot.checksum;
  if (!snapshot.valid()) { SERIAL_ERROR_MSG("Invalid recovery state"); return false; }
  // FLASH_If_Write owns erase and readback verification.
  if (persistentStore.FLASH_If_Write(FLASH_OUTAGE_DATA_ADDR, &snapshot, sizeof(snapshot)) != FLASHIF_OK) {
    SERIAL_ERROR_MSG("Recovery write failed"); return false;
  }
  return true;
}

/**
 * Resume the saved print job
 */
void PrintJobRecovery::resume() {
  if (!supported) { init(); report_unavailable(); return; }
  if (!valid() || card.flag.abort_sd_printing || marlin_state == MF_KILLED) return;
  const uint32_t abort_epoch = TERN0(HAS_DISPLAY, ui.print_abort_count);
  bool resume_cancelled = false;
  auto stop_recovery = [&]() {
    resume_cancelled = true;
    wait_for_heatup = wait_for_user = false;
    thermalManager.disable_all_heaters();
    TERN_(ADVANCED_PAUSE_FEATURE, cancel_pause());
    card.abortFilePrintSoon();
    purge();
  };
  auto cancelled = [&]() -> bool {
    if (resume_cancelled || card.flag.abort_sd_printing || !card.isMounted()
        || marlin_state == MF_KILLED || TERN0(HAS_DISPLAY, ui.print_abort_count != abort_epoch)) {
      stop_recovery(); return true;
    }
    return false;
  };
  // Open before applying any heat or position from the saved job.
  card.openFileRead(info.sd_filename);
  if (!card.isFileOpen() || info.sdpos > card.getFileSize() || cancelled()) { stop_recovery(); return; }


 char cmd[MAX_CMD_SIZE+16], str_1[16], str_2[16];

  const uint32_t resume_sdpos = info.sdpos; // Get here before the stepper ISR overwrites it

  // Apply the dry-run flag if enabled
  if (info.flag.dryrun) marlin_debug_flags |= MARLIN_DEBUG_DRYRUN;

  // Restore cold extrusion permission
  TERN_(PREVENT_COLD_EXTRUSION, thermalManager.allow_cold_extrude = info.flag.allow_cold_extrusion);

  #if HAS_LEVELING
    // Make sure leveling is off before any G92 and G28
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR("M420 S0 Z0"));
    if (cancelled()) return;
  #endif

  // Restore the bed temperature,no waiting
  #if HAS_HEATED_BED
    int16_t bt = info.target_temperature_bed;
    if (bt) {
      sprintf_P(cmd, PSTR("M140 S%i"), bt);
      if (cancelled()) return;
      gcode.process_subcommands_now(cmd);
      if (cancelled()) return;
    }
  #endif

  // Restore all hotend temperatures,no waiting
  #if HAS_HOTEND
    int16_t et = 0;
    HOTEND_LOOP() {
      et = info.target_temperature[e];
      if (et) {
    #if HAS_MULTI_HOTEND
          sprintf_P(cmd, PSTR("T%i S"), e);
          if (cancelled()) return;
          gcode.process_subcommands_now(cmd);
          if (cancelled()) return;
    #endif
        sprintf_P(cmd, PSTR("M104 S%i"), et);
        if (cancelled()) return;
        gcode.process_subcommands_now(cmd);
        if (cancelled()) return;
      }
    }
  #endif

  #if HAS_HEATED_BED
    bt = info.target_temperature_bed;
    if (bt) {
      // Restore the bed temperature
      if (cancelled() || !thermalManager.wait_for_bed()) { stop_recovery(); return; }
    }
  #endif

  // Restore all hotend temperatures
  #if HAS_HOTEND
    HOTEND_LOOP() {
      et = info.target_temperature[e];
      if (et) {

        if (cancelled() || !thermalManager.wait_for_hotend(e)) { stop_recovery(); return; }
      }
    }
  #endif

  // Reset E, raise Z, home XY...
  #if Z_HOME_DIR > 0

    // If Z homing goes to max, just reset E and home all
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR(
      "G92.9 E0\n"
      "G28R0"
    ));
    if (cancelled()) return;

  #else // "G92.9 E0 ..."

    // If a Z raise occurred at outage restore Z, otherwise raise Z now
    sprintf_P(cmd, PSTR("G92.9 E0 " TERN(BACKUP_POWER_SUPPLY, "Z%s", "Z0\nG1Z%s")), dtostrf(info.zraise, 1, 3, str_1));
    if (cancelled()) return;
    gcode.process_subcommands_now(cmd);
    if (cancelled()) return;

    // Home safely with no Z raise
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR(
      "G28R0"                               // No raise during G28
      #if IS_CARTESIAN && DISABLED(POWER_LOSS_RECOVER_ZHOME)
        "XY"                                // Don't home Z on Cartesian unless overridden
      #endif
    ));
    if (cancelled()) return;

  #endif

  // Pretend that all axes are homed
  set_all_homed();

  #if ENABLED(POWER_LOSS_RECOVER_ZHOME)
    // Z has been homed so restore Z to ZsavedPos + POWER_LOSS_ZRAISE
    sprintf_P(cmd, PSTR("G1 F500 Z%s"), dtostrf(info.current_position.z + POWER_LOSS_ZRAISE, 1, 3, str_1));
    if (cancelled()) return;
    gcode.process_subcommands_now(cmd);
    if (cancelled()) return;
  #endif

  // Recover volumetric extrusion state
  #if DISABLED(NO_VOLUMETRICS)
    #if HAS_MULTI_EXTRUDER
      for (int8_t e = 0; e < EXTRUDERS; e++) {
        sprintf_P(cmd, PSTR("M200 T%i D%s"), e, dtostrf(info.filament_size[e], 1, 3, str_1));
        if (cancelled()) return;
        gcode.process_subcommands_now(cmd);
        if (cancelled()) return;
      }
      if (!info.volumetric_enabled) {
        sprintf_P(cmd, PSTR("M200 T%i D0"), info.active_extruder);
        if (cancelled()) return;
        gcode.process_subcommands_now(cmd);
        if (cancelled()) return;
      }
    #else
      if (info.volumetric_enabled) {
        sprintf_P(cmd, PSTR("M200 D%s"), dtostrf(info.filament_size[0], 1, 3, str_1));
        if (cancelled()) return;
        gcode.process_subcommands_now(cmd);
        if (cancelled()) return;
      }
    #endif
  #endif

  // Select the previously active tool (with no_move)
  #if HAS_MULTI_EXTRUDER
    sprintf_P(cmd, PSTR("T%i S"), info.active_extruder);
    if (cancelled()) return;
    gcode.process_subcommands_now(cmd);
    if (cancelled()) return;
  #endif

  // Restore print cooling fan speeds
  #if HAS_FAN
    FANS_LOOP(i) {
      const int f = info.fan_speed[i];
      if (f) {
        sprintf_P(cmd, PSTR("M106 P%i S%i"), i, f);
        if (cancelled()) return;
        gcode.process_subcommands_now(cmd);
        if (cancelled()) return;
      }
    }
  #endif

  // Restore retract and hop state
  #if ENABLED(FWRETRACT)
    LOOP_L_N(e, EXTRUDERS) {
      if (info.retract[e] != 0.0) {
        fwretract.current_retract[e] = info.retract[e];
        fwretract.retracted[e] = true;
      }
    }
    fwretract.current_hop = info.retract_hop;
  #endif

  #if HAS_LEVELING
    // Restore leveling state before 'G92 Z' to ensure
    // the Z stepper count corresponds to the native Z.
    if (info.fade || info.flag.leveling) {
      sprintf_P(cmd, PSTR("M420 S%i Z%s"), int(info.flag.leveling), dtostrf(info.fade, 1, 1, str_1));
      if (cancelled()) return;
      gcode.process_subcommands_now(cmd);
      if (cancelled()) return;
    }
  #endif

  #if ENABLED(GRADIENT_MIX)
    memcpy(&mixer.gradient, &info.gradient, sizeof(info.gradient));
  #endif

  // Un-retract if there was a retract at outage
  #if POWER_LOSS_RETRACT_LEN
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR("G1 E" STRINGIFY(POWER_LOSS_RETRACT_LEN) " F3000"));
    if (cancelled()) return;
  #endif

  // Additional purge if configured
  #if POWER_LOSS_PURGE_LEN
    sprintf_P(cmd, PSTR("G1 E%d F200"), (POWER_LOSS_PURGE_LEN) + (POWER_LOSS_RETRACT_LEN));
    if (cancelled()) return;
    gcode.process_subcommands_now(cmd);
    if (cancelled()) return;
  #endif

  #if ENABLED(NOZZLE_CLEAN_FEATURE)
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR("G12"));
    if (cancelled()) return;
  #endif

  // Move back to the saved XY
  sprintf_P(cmd, PSTR("G1 X%s Y%s F3000"),
    dtostrf(info.current_position.x, 1, 3, str_1),
    dtostrf(info.current_position.y, 1, 3, str_2)
  );
  if (cancelled()) return;
  gcode.process_subcommands_now(cmd);
  if (cancelled()) return;

  // Move back to the saved Z
  dtostrf(info.current_position.z, 1, 3, str_1);
  #if Z_HOME_DIR > 0 || ENABLED(POWER_LOSS_RECOVER_ZHOME)
    sprintf_P(cmd, PSTR("G1 Z%s F200"), str_1);
  #else
    if (cancelled()) return;
    gcode.process_subcommands_now_P(PSTR("G1 Z0 F200"));
    if (cancelled()) return;
    sprintf_P(cmd, PSTR("G92.9 Z%s"), str_1);
  #endif
  if (cancelled()) return;
  gcode.process_subcommands_now(cmd);
  if (cancelled()) return;

  // Restore the feedrate
  sprintf_P(cmd, PSTR("G1 F%d"), info.feedrate);
  if (cancelled()) return;
  gcode.process_subcommands_now(cmd);
  if (cancelled()) return;

  // Restore E position with G92.9
  sprintf_P(cmd, PSTR("G92.9 E%s"), dtostrf(info.current_position.e, 1, 3, str_1));
  if (cancelled()) return;
  gcode.process_subcommands_now(cmd);
  if (cancelled()) return;

  TERN_(GCODE_REPEAT_MARKERS, repeat = info.stored_repeat);
  TERN_(HAS_HOME_OFFSET, home_offset = info.home_offset);
  TERN_(HAS_POSITION_SHIFT, position_shift = info.position_shift);
  #if HAS_HOME_OFFSET || HAS_POSITION_SHIFT
    LOOP_XYZ(i) update_workspace_offset((AxisEnum)i);
  #endif

  // Relative axis modes
  gcode.axis_relative = info.axis_relative;

  #if ENABLED(DEBUG_POWER_LOSS_RECOVERY)
    const uint8_t old_flags = marlin_debug_flags;
    marlin_debug_flags |= MARLIN_DEBUG_ECHO;
  #endif

  // Continue to apply PLR when a file is resumed!
  enable(true);

  // Resume the SD file from the last position
  if (cancelled() || !card.isFileOpen()) { stop_recovery(); return; }
  sprintf_P(cmd, PSTR("M24 S%ld T%ld"), resume_sdpos, info.print_job_elapsed);
  if (cancelled()) return;
  gcode.process_subcommands_now(cmd);
  if (cancelled() || !IS_SD_PRINTING()) { stop_recovery(); return; }

  TERN_(DEBUG_POWER_LOSS_RECOVERY, marlin_debug_flags = old_flags);
}

#if ENABLED(DEBUG_POWER_LOSS_RECOVERY)

  void PrintJobRecovery::debug(PGM_P const prefix) {
    DEBUG_PRINT_P(prefix);
    DEBUG_ECHOLNPAIR(" Job Recovery Info...\nvalid_head:", int(info.valid_head), " valid_foot:", int(info.valid_foot));
    if (info.valid()) {
      if (info.valid_head == info.valid_foot) {
        DEBUG_ECHOPGM("current_position: ");
        LOOP_XYZE(i) {
          if (i) DEBUG_CHAR(',');
          DEBUG_DECIMAL(info.current_position[i]);
        }
        DEBUG_EOL();

        DEBUG_ECHOLNPAIR("zraise: ", info.zraise);

        #if HAS_HOME_OFFSET
          DEBUG_ECHOPGM("home_offset: ");
          LOOP_XYZ(i) {
            if (i) DEBUG_CHAR(',');
            DEBUG_DECIMAL(info.home_offset[i]);
          }
          DEBUG_EOL();
        #endif

        #if HAS_POSITION_SHIFT
          DEBUG_ECHOPGM("position_shift: ");
          LOOP_XYZ(i) {
            if (i) DEBUG_CHAR(',');
            DEBUG_DECIMAL(info.position_shift[i]);
          }
          DEBUG_EOL();
        #endif

        DEBUG_ECHOLNPAIR("feedrate: ", info.feedrate);

        #if HAS_MULTI_EXTRUDER
          DEBUG_ECHOLNPAIR("active_extruder: ", int(info.active_extruder));
        #endif

        #if HAS_HOTEND
          DEBUG_ECHOPGM("target_temperature: ");
          HOTEND_LOOP() {
            DEBUG_ECHO(info.target_temperature[e]);
            if (e < HOTENDS - 1) DEBUG_CHAR(',');
          }
          DEBUG_EOL();
        #endif

        #if HAS_HEATED_BED
          DEBUG_ECHOLNPAIR("target_temperature_bed: ", info.target_temperature_bed);
        #endif

        #if HAS_FAN
          DEBUG_ECHOPGM("fan_speed: ");
          FANS_LOOP(i) {
            DEBUG_ECHO(int(info.fan_speed[i]));
            if (i < FAN_COUNT - 1) DEBUG_CHAR(',');
          }
          DEBUG_EOL();
        #endif

        #if HAS_LEVELING
          DEBUG_ECHOLNPAIR("leveling: ", int(info.flag.leveling), " fade: ", info.fade);
        #endif
        #if ENABLED(FWRETRACT)
          DEBUG_ECHOPGM("retract: ");
          for (int8_t e = 0; e < EXTRUDERS; e++) {
            DEBUG_ECHO(info.retract[e]);
            if (e < EXTRUDERS - 1) DEBUG_CHAR(',');
          }
          DEBUG_EOL();
          DEBUG_ECHOLNPAIR("retract_hop: ", info.retract_hop);
        #endif
        DEBUG_ECHOLNPAIR("sd_filename: ", info.sd_filename);
        DEBUG_ECHOLNPAIR("sdpos: ", info.sdpos);
        DEBUG_ECHOLNPAIR("print_job_elapsed: ", info.print_job_elapsed);
        DEBUG_ECHOLNPAIR("dryrun: ", int(info.flag.dryrun));
        DEBUG_ECHOLNPAIR("allow_cold_extrusion: ", int(info.flag.allow_cold_extrusion));
      }
      else
        DEBUG_ECHOLNPGM("INVALID DATA");
    }
    DEBUG_ECHOLNPGM("---");
  }

#endif // DEBUG_POWER_LOSS_RECOVERY

#endif // POWER_LOSS_RECOVERY
