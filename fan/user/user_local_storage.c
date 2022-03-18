#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"
#include "user_interface.h"
#include "user_local_storage.h"

static uint32 user_sect = 0xFC;

struct USER_LOCAL_CONFIG ICACHE_FLASH_ATTR *user_read_local_storage(void)
{
    struct USER_LOCAL_CONFIG *plocal_config = (struct USER_LOCAL_CONFIG *)os_zalloc(sizeof(struct USER_LOCAL_CONFIG));
    spi_flash_read(user_sect * SPI_FLASH_SEC_SIZE, (uint32 *)plocal_config, sizeof(struct USER_LOCAL_CONFIG));
    return plocal_config;
}

void ICACHE_FLASH_ATTR user_save_local_storage(struct USER_LOCAL_CONFIG *plocal_config)
{
    spi_flash_erase_sector(user_sect);
    spi_flash_write(user_sect * SPI_FLASH_SEC_SIZE, (uint32 *)plocal_config, sizeof(struct USER_LOCAL_CONFIG));
}