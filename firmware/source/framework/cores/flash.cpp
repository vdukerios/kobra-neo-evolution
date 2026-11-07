#include "flash.h"
#include <string.h>

namespace Intflash {

static uint8_t eeprom_buffer[EEPROM_SIZE] __attribute__((aligned(8)));
static uint32_t active_base = 0, active_generation = 0;
// Each item occupies a separate programmable 32-bit word. Commit is always last.
struct SnapshotHeader {
  uint32_t magic, format, length, generation, payload_crc, header_crc, commit;
};
static_assert(sizeof(SnapshotHeader) == 28, "Snapshot header must use seven 32-bit words");
static const uint32_t SNAP_MAGIC = 0x4B4E4531UL, SNAP_COMMIT = 0x434F4D31UL;

static uint32_t checksum(const void *data, uint32_t length) {
  const uint8_t *p = (const uint8_t*)data;
  uint32_t crc = 0xFFFFFFFFUL;
  while (length--) {
    crc ^= *p++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
  }
  return ~crc;
}

static bool data_sector(const uint32_t base) {
  return base == FLASH_EEPROM_SHADOW || base == FLASH_OUTAGE_DATA_ADDR || base == FLASH_EEPROM_BASE;
}
static bool data_range(const uint32_t address, const uint32_t length) {
  const uint32_t base = address & ~(FLASH_SECTOR_SIZE - 1);
  return data_sector(base) && length <= FLASH_SECTOR_SIZE - (address - base);
}
static bool ready() {
  uint32_t remaining = 1000000UL;
  while (EFM_GetFlagStatus(EFM_FLAG_RDY) != Set && remaining) --remaining;
  return remaining != 0;
}
static bool program(const uint32_t addr, const void *data, const uint32_t length) {
  if ((addr & 3) || !data_range(addr, length) || (length && !data)) return false;
  const uint8_t *p = (const uint8_t*)data;
  for (uint32_t offset = 0; offset < length; offset += 4) {
    uint32_t word = 0xFFFFFFFFUL;
    const uint32_t count = length - offset < 4 ? length - offset : 4;
    memcpy(&word, p + offset, count); // Never read beyond the caller's last byte.
    if (!ready() || EFM_SingleProgram(addr + offset, word) != Ok
        || *(const volatile uint32_t*)(addr + offset) != word) return false;
  }
  return true;
}

en_result_t FlashErasePage(uint32_t address) {
  if (!data_sector(address)) return Error;
  EFM_Unlock(); EFM_FlashCmd(Enable);
  const en_result_t result = ready() ? EFM_SectorErase(address) : Error;
  EFM_Lock();
  return result;
}

uint32_t Flash_Updata(uint32_t address, const void *data, uint32_t length) {
  // Recovery uses this erase-and-replace API; EEPROM uses the transaction below.
  if (!data_sector(address) || !length || !data || !data_range(address, length)) return Error;
  if (FlashErasePage(address) != Ok) return Error; // Never program after failed erase.
  EFM_Unlock(); EFM_FlashCmd(Enable);
  const bool result = program(address, data, length);
  EFM_Lock();
  return result ? Ok : Error;
}

static bool valid_snapshot(const uint32_t base, SnapshotHeader &header) {
  memcpy(&header, (const void*)(base + EEPROM_SIZE), sizeof(header));
  return header.magic == SNAP_MAGIC && header.format == 1 && header.length == EEPROM_SIZE
      && header.commit == SNAP_COMMIT
      && header.header_crc == checksum(&header, 20)
      && header.payload_crc == checksum((const void*)base, EEPROM_SIZE);
}

bool eeprom_buffer_fill() {
  if (!ready()) return false;
  SnapshotHeader main_header, shadow_header;
  const bool main_ok = valid_snapshot(FLASH_EEPROM_BASE, main_header),
             shadow_ok = valid_snapshot(FLASH_EEPROM_SHADOW, shadow_header);
  active_base = 0; active_generation = 0;
  if (main_ok || shadow_ok) {
    const bool use_shadow = shadow_ok && (!main_ok || (int32_t)(shadow_header.generation - main_header.generation) > 0);
    active_base = use_shadow ? FLASH_EEPROM_SHADOW : FLASH_EEPROM_BASE;
    active_generation = use_shadow ? shadow_header.generation : main_header.generation;
    memcpy(eeprom_buffer, (const void*)active_base, EEPROM_SIZE);
  }
  else {
    // Legacy 1KB remains readable until the first shadow commit. A torn new
    // container is never interpreted as legacy merely because its CRC failed.
    const uint8_t *metadata = (const uint8_t*)(FLASH_EEPROM_BASE + EEPROM_SIZE);
    bool legacy = true;
    for (uint32_t i = 0; i < sizeof(SnapshotHeader); ++i) if (metadata[i] != 0xFF) legacy = false;
    if (legacy) memcpy(eeprom_buffer, (const void*)FLASH_EEPROM_BASE, EEPROM_SIZE);
    else memset(eeprom_buffer, 0xFF, EEPROM_SIZE);
  }
  return true;
}

bool eeprom_buffer_flush() {
  const uint32_t target = active_base == FLASH_EEPROM_SHADOW ? FLASH_EEPROM_BASE : FLASH_EEPROM_SHADOW;
  SnapshotHeader h = { SNAP_MAGIC, 1, EEPROM_SIZE, active_generation + 1,
                       checksum(eeprom_buffer, EEPROM_SIZE), 0, 0xFFFFFFFFUL };
  h.header_crc = checksum(&h, 20);
  if (FlashErasePage(target) != Ok) return false;
  EFM_Unlock(); EFM_FlashCmd(Enable);
  bool ok = program(target, eeprom_buffer, EEPROM_SIZE)
         && program(target + EEPROM_SIZE, &h, 24);
  if (ok) {
    // Verify both complete regions before touching the commit word.
    ok = !memcmp((const void*)target, eeprom_buffer, EEPROM_SIZE)
      && !memcmp((const void*)(target + EEPROM_SIZE), &h, 24);
  }
  if (ok) ok = program(target + EEPROM_SIZE + 24, &SNAP_COMMIT, 4);
  EFM_Lock();
  SnapshotHeader verified;
  if (!ok || !valid_snapshot(target, verified)) return false;
  active_base = target; active_generation = h.generation;
  return true;
}

uint8_t eeprom_read_byte(const uint32_t pos) {
  if (pos >= EEPROM_SIZE) return 0xFF;
  if (!eeprom_buffer_fill()) return 0xFF;
  return eeprom_buffered_read_byte(pos);
}
bool eeprom_write_byte(uint32_t pos, uint8_t value) {
  if (pos >= EEPROM_SIZE) return false;
  if (!eeprom_buffer_fill()) return false;
  eeprom_buffered_write_byte(pos, value);
  return eeprom_buffer_flush();
}
uint8_t eeprom_buffered_read_byte(const uint32_t pos) {
  return pos < EEPROM_SIZE ? eeprom_buffer[pos] : 0xFF;
}
void eeprom_buffered_write_byte(uint32_t pos, uint8_t value) {
  if (pos < EEPROM_SIZE) eeprom_buffer[pos] = value;
}
}
