#ifndef FLASH_H
#define FLASH_H


#include <stdint.h>

#include "hc32_ddl.h"


#ifdef __cplusplus
extern "C" {
#endif


// HC32F460: 512KB, 64 sectors of 8KB. Data sectors reserved by linker.
#define FLASH_SECTOR_TOTAL    64
#define FLASH_SECTOR_SIZE     ((uint32_t)(8*1024U))
#define FLASH_ALL_START       0
#define FLASH_ALL_END         ((uint32_t)0x0007FFFFU)

// Preserve logical legacy offsets; container metadata follows its 1KB payload.
#define FLASH_EEPROM_SHADOW   ((uint32_t)0x0007A000U)

// use last sector to emulate eeprom
#define FLASH_EEPROM_BASE     ((uint32_t)0x0007E000U)


#define FLASH_BASE            ((uint32_t)0x0007E000U)              /*!< FLASH base address in the alias region */

// just use 1k bytes, not a full sector
#define EEPROM_SIZE           1024


// power outage
#define FLASH_OUTAGE_DATA_ADDR  ((uint32_t)0x0007C000U)



namespace Intflash {
    bool eeprom_buffer_fill();
    uint8_t eeprom_buffered_read_byte(const uint32_t pos);
    void eeprom_buffered_write_byte(uint32_t pos, uint8_t value);
    uint8_t eeprom_read_byte(const uint32_t pos);
    bool eeprom_write_byte(uint32_t pos, uint8_t value);
    bool eeprom_buffer_flush();
     uint32_t Flash_Updata(uint32_t flashAddr, const void * dataBuf, uint32_t length);
     en_result_t FlashErasePage(uint32_t u32Addr);
}

#ifdef __cplusplus
}
#endif

#endif 

