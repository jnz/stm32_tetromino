/*
 * hiscore_port.h on a RAM array that behaves like NOR flash: erase sets
 * every bit, program can only clear bits. Shared by the simulator and the
 * tests. flash_ram_* below let them inspect and damage it.
 */
#include "hiscore_port.h"
#include "flash_ram.h"
#include <string.h>

uint8_t  flash_ram[HS_SECTORS * HS_SECTOR_SZ];
uint32_t flash_ram_erases;
int      flash_ram_fail_program;   /* > 0: the next n programs fail halfway */

void flash_ram_reset(uint8_t fill)
{
    memset(flash_ram, fill, sizeof flash_ram);
    flash_ram_erases = 0;
    flash_ram_fail_program = 0;
}

int hs_port_read(uint32_t off, void *dst, uint32_t len)
{
    if (off > sizeof flash_ram || len > sizeof flash_ram - off)
        return -1;
    memcpy(dst, flash_ram + off, len);
    return 0;
}

int hs_port_program(uint32_t off, const void *src, uint32_t len)
{
    const uint8_t *s = src;
    uint32_t i;

    if ((off & 3U) != 0U || (len & 3U) != 0U)
        return -1;
    if (off > sizeof flash_ram || len > sizeof flash_ram - off)
        return -1;
    if (flash_ram_fail_program > 0) {
        /* A write cut short: the first half lands, the rest does not. */
        flash_ram_fail_program--;
        for (i = 0; i < len / 2U; i++)
            flash_ram[off + i] &= s[i];
        return -1;
    }
    for (i = 0; i < len; i++)
        flash_ram[off + i] &= s[i];
    return 0;
}

int hs_port_erase(uint32_t sector)
{
    if (sector >= HS_SECTORS)
        return -1;
    memset(flash_ram + sector * HS_SECTOR_SZ, 0xFF, HS_SECTOR_SZ);
    flash_ram_erases++;
    return 0;
}
