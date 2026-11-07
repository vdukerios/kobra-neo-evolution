/**
 * Marlin 3D Printer Firmware
 *
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 * Copyright (c) 2016 Bob Cousins bobcousins42@googlemail.com
 * Copyright (c) 2015-2016 Nico Tonnhofer wurstnase.reprap@gmail.com
 * Copyright (c) 2016 Victor Perez victor_pv@hotmail.com
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
#if defined(ARDUINO_ARCH_STM32) && !defined(STM32GENERIC)

#include "../../inc/MarlinConfig.h"

#if ENABLED(FLASH_EEPROM_EMULATION)

#include "../shared/eeprom_api.h"

/**
 * The STM32 HAL supports chips that deal with "pages" and some with "sectors" and some that
 * even have multiple "banks" of flash.
 *
 * This code is a bit of a mashup of
 *   framework-arduinoststm32/cores/arduino/stm32/stm32_eeprom.c
 *   hal/hal_lpc1768/persistent_store_flash.cpp
 *
 * This has only be written against those that use a single "sector" design.
 *
 * Those that deal with "pages" could be made to work. Looking at the STM32F07 for example, there are
 * 128 "pages", each 2kB in size. If we continued with our EEPROM being 4Kb, we'd always need to operate
 * on 2 of these pages. Each write, we'd use 2 different pages from a pool of pages until we are done.
 */

static bool eeprom_data_written = false, eeprom_access_active = false, eeprom_session_written = false, eeprom_session_failed = false, eeprom_buffer_confirmed = false;

size_t PersistentStore::capacity() { return EEPROM_SIZE; }

bool PersistentStore::access_start() {
  if (eeprom_access_active) return false;
  // Each session starts from the committed snapshot. Owners retain their RAM
  // values and must explicitly retry their own save after a failed transaction.
  eeprom_buffer_confirmed = Intflash::eeprom_buffer_fill();
  if (!eeprom_buffer_confirmed) return false;
  eeprom_data_written = false;
  eeprom_session_written = eeprom_session_failed = false;
  eeprom_access_active = true;
  return true;
}

bool PersistentStore::access_finish() {
  if (!eeprom_access_active) return false;
  bool ok = !eeprom_session_failed;
  if (ok && eeprom_data_written && eeprom_session_written) {
    TERN_(HAS_PAUSE_SERVO_OUTPUT, PAUSE_SERVO_OUTPUT());
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ok = Intflash::eeprom_buffer_flush();
    __set_PRIMASK(primask);
    TERN_(HAS_PAUSE_SERVO_OUTPUT, RESUME_SERVO_OUTPUT());
    if (ok) { eeprom_data_written = false; eeprom_buffer_confirmed = true; }
  }
  if (!ok) {
    // Roll back transport RAM; a stats write must never commit failed settings.
    eeprom_buffer_confirmed = Intflash::eeprom_buffer_fill();
    eeprom_data_written = false;
  }
  eeprom_access_active = false;
  return ok;
}

static bool valid_access(const int pos, const size_t size) {
  return pos >= 0 && (size_t)pos <= EEPROM_SIZE && size <= EEPROM_SIZE - (size_t)pos;
}
bool PersistentStore::write_data(int &pos, const uint8_t *value, size_t size, uint16_t *crc) {
  if (!valid_access(pos, size)) { eeprom_session_failed = true; return true; }
  if (!size) return false;
  if (!eeprom_access_active || !value || !crc) { eeprom_session_failed = true; return true; }
  eeprom_session_written = true;
  while (size--) {
    const uint8_t v = *value++;
    if (v != Intflash::eeprom_buffered_read_byte(pos)) {
      Intflash::eeprom_buffered_write_byte(pos, v);
      eeprom_data_written = true;
      eeprom_buffer_confirmed = false;
    }
    crc16(crc, &v, 1); ++pos;
  }
  return false;
}
bool PersistentStore::read_data(int &pos, uint8_t *value, size_t size, uint16_t *crc, const bool writing) {
  if (!valid_access(pos, size)) { eeprom_session_failed = true; return true; }
  if (!size) return false;
  if ((!eeprom_access_active && !eeprom_buffer_confirmed) || (writing && !value) || !crc) { eeprom_session_failed = true; return true; }
  while (size--) {
    const uint8_t c = Intflash::eeprom_buffered_read_byte(pos++);
    if (writing) *value++ = c;
    crc16(crc, &c, 1);
  }
  return false;
}
uint32_t PersistentStore::FLASH_If_Erase(uint32_t start, uint32_t end) {
  if (end <= start || end - start > FLASH_SECTOR_SIZE) return FLASHIF_ERASEKO;
  return Intflash::FlashErasePage(start) == Ok ? FLASHIF_OK : FLASHIF_ERASEKO;
}
uint32_t PersistentStore::FLASH_If_Write(uint32_t destination, const void *source, uint32_t length) {
  return Intflash::Flash_Updata(destination, source, length) == Ok ? FLASHIF_OK : FLASHIF_WRITING_ERROR;
}

#endif // FLASH_EEPROM_EMULATION
#endif // ARDUINO_ARCH_STM32 && !STM32GENERIC
