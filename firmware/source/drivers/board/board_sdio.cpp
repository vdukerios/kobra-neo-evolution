#include "board_sdio.h"
#include "bsp_sdio.h"


en_result_t sdio_main(void)
{

}

bool steup_sdio(void)
{
    return hal_sdio_init();
}

uint32_t SDIO_CardSize(void) { return sdio_card_blocks(); }

bool SDIO_ReadBlock_DMA(uint32_t blockAddress, uint8_t *data)
{
    return sdio_read(blockAddress, data);
}

bool SDIO_WriteBlockDMA(uint32_t blockAddress, const uint8_t *data)
{
    return sdio_write(blockAddress, (const uint8_t *)data);
}


