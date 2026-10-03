/*
 * hiscore_port.h on the internal flash of the STM32F429, after the flash
 * port of the INSLIB configuration store (cfg_flash_stm32.c there).
 *
 * The store sits in sectors 14 and 15 of BANK 2 (16 kB each, from
 * 0x08108000). Not 12 and 13: those hold the INSLIB configuration, and
 * this board may well go back and forth between the two firmwares. Each
 * keeps its own data that way. The linker script caps the image at bank 1,
 * so the code can never grow into the store.
 *
 * Bank 2 is erased and programmed while the code runs from bank 1, which
 * the dual bank part allows without stalling instruction fetches.
 */
#include "hiscore_port.h"
#include "stm32f4xx_hal.h"

#define HS_FLASH_BASE   0x08108000UL      /* bank 2, sector 14 */

static const uint32_t s_sector_id[HS_SECTORS] = {
    FLASH_SECTOR_14,
    FLASH_SECTOR_15,
};

int hs_port_read(uint32_t off, void *dst, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)(HS_FLASH_BASE + off);
    uint8_t *d = (uint8_t *)dst;
    uint32_t i;

    if (off > HS_SECTORS * HS_SECTOR_SZ || len > HS_SECTORS * HS_SECTOR_SZ - off)
        return -1;
    for (i = 0; i < len; i++)
        d[i] = src[i];
    return 0;
}

static void clear_error_flags(void)
{
    /* A stale error flag from an earlier operation would make the HAL fail
     * the next one before it starts. */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

int hs_port_program(uint32_t off, const void *src, uint32_t len)
{
    const uint8_t *s = (const uint8_t *)src;
    uint32_t i;
    int rc = 0;

    if ((off & 3U) != 0U || (len & 3U) != 0U)
        return -1;
    if (off > HS_SECTORS * HS_SECTOR_SZ || len > HS_SECTORS * HS_SECTOR_SZ - off)
        return -1;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return -1;
    clear_error_flags();
    for (i = 0; i < len; i += 4U) {
        /* Assembled byte by byte, the caller's buffer is not promised to
         * be word aligned. */
        const uint32_t w = (uint32_t)s[i] | ((uint32_t)s[i + 1] << 8) |
                           ((uint32_t)s[i + 2] << 16) | ((uint32_t)s[i + 3] << 24);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HS_FLASH_BASE + off + i, w)
                != HAL_OK) {
            rc = -1;
            break;
        }
    }
    HAL_FLASH_Lock();
    return rc;
}

int hs_port_erase(uint32_t sector)
{
    FLASH_EraseInitTypeDef e;
    uint32_t err = 0;
    HAL_StatusTypeDef st;

    if (sector >= HS_SECTORS)
        return -1;

    e.TypeErase    = FLASH_TYPEERASE_SECTORS;
    e.Banks        = FLASH_BANK_2;
    e.Sector       = s_sector_id[sector];
    e.NbSectors    = 1;
    /* 2.7 V to 3.6 V, 32 bit parallelism, the width HAL_FLASH_Program
     * uses above. The board runs at 3.0 V. */
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return -1;
    clear_error_flags();
    st = HAL_FLASHEx_Erase(&e, &err);
    HAL_FLASH_Lock();

    return (st == HAL_OK && err == 0xFFFFFFFFU) ? 0 : -1;
}
