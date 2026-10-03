#ifndef TETRIS_FLASH_RAM_H
#define TETRIS_FLASH_RAM_H
/* RAM backed flash for the high score store, see flash_ram.c. */
#include <stdint.h>
#include "hiscore_port.h"

extern uint8_t  flash_ram[HS_SECTORS * HS_SECTOR_SZ];
extern uint32_t flash_ram_erases;
extern int      flash_ram_fail_program;

/* Fill the whole store with one byte, 0xFF = factory fresh. */
void flash_ram_reset(uint8_t fill);

#endif /* TETRIS_FLASH_RAM_H */
