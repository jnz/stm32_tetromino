#ifndef TETRIS_HISCORE_PORT_H
#define TETRIS_HISCORE_PORT_H
/*
 * The platform dependent part of the high score store: raw access to the
 * two flash sectors it lives in.
 *
 * Offsets are relative to the start of the store, sector n begins at
 * n * HS_SECTOR_SZ. The host simulator and the tests implement this on a
 * RAM array, the firmware on bank 2 of the STM32F429 (hiscore_flash.c).
 */
#include <stdint.h>

#define HS_SECTOR_SZ   (16U * 1024U)
#define HS_SECTORS     2U

/* Copy len bytes out of the store. 0 = ok. */
int hs_port_read(uint32_t off, void *dst, uint32_t len);

/* Program len bytes into erased flash. off and len are multiples of 4.
 * 0 = ok. */
int hs_port_program(uint32_t off, const void *src, uint32_t len);

/* Erase one sector, 0 or 1. Blocks for the whole erase. 0 = ok. */
int hs_port_erase(uint32_t sector);

#endif /* TETRIS_HISCORE_PORT_H */
