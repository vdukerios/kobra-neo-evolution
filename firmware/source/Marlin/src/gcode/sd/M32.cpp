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

#include "../../inc/MarlinConfig.h"

#if HAS_MEDIA_SUBCALLS

#include "../gcode.h"
#include "../../sd/cardreader.h"
#include "../../lcd/marlinui.h"
#include "../../module/planner.h" // for synchronize()

#include "../../MarlinCore.h" // for startOrResumeJob

/**
 * M32: Select file and start SD Print
 *
 * Examples:
 *
 *    M32 !PATH/TO/FILE.GCO#      ; Start FILE.GCO
 *    M32 P !PATH/TO/FILE.GCO#    ; Start FILE.GCO as a procedure
 *    M32 S60 !PATH/TO/FILE.GCO#  ; Start FILE.GCO at byte 60
 */
void GcodeSuite::M32() {
  if (!parser.string_arg || strlen(parser.string_arg) >= MAXPATHNAMELENGTH) return;
  char requested_file[MAXPATHNAMELENGTH];
  strcpy(requested_file, parser.string_arg);
  const uint8_t call_procedure = parser.boolval('P');
  const bool has_offset = parser.seenval('S');
  uint32_t offset = 0;
  if (has_offset && !parser.value_ulong_checked(offset)) return;
  const uint32_t abort_epoch = TERN0(HAS_DISPLAY, ui.print_abort_count);
  if (IS_SD_PRINTING()) planner.synchronize();

  if (card.isMounted() && !card.flag.abort_sd_printing
      && !TERN0(HAS_DISPLAY, abort_epoch != ui.print_abort_count)) {
    card.openFileRead(requested_file, call_procedure);
    if (has_offset) {
      if (offset > card.getFileSize()) return;
      card.setIndex(offset);
    }

    if (!card.isFileOpen()) return;
    // Procedure calls count as normal print time.
    if (!call_procedure && !startOrResumeJob()) return;
    if (card.flag.abort_sd_printing || !card.isMounted() || !card.isFileOpen()
        || TERN0(HAS_DISPLAY, abort_epoch != ui.print_abort_count)) return;
    card.startFileprint();
  }
}

#endif // HAS_MEDIA_SUBCALLS
