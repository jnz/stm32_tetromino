#ifndef TETRIS_PERIPH_H
#define TETRIS_PERIPH_H
/*
 * Peripheral bring-up for the parts of the board this firmware owns:
 * clock tree, LEDs, I2C3, the hardware random number generator and the
 * independent watchdog. LTDC, FMC/SDRAM, SPI5 and DMA2D belong to the ST
 * board support package (display.c).
 *
 * Taken from the INSLIB sensor firmware, minus
 * everything the sensors needed.
 */
#include "main.h"

/* I2C3 carries the STMPE811 touch controller. The BSP binds its I2C
 * helpers to this handle (stm32f429i_discovery.c), so it has to exist even
 * while nothing reads the touch panel. */
extern I2C_HandleTypeDef hi2c3;

/* Core clock from the 8 MHz HSE, CPU_MHZ (periph.c). Runs before anything
 * else. */
void periph_clock_config(void);

/* LEDs, the gyro chip select and I2C3. */
void periph_init(void);

/* 32 random bits from the hardware RNG, 0 if it did not deliver. */
uint32_t periph_random(void);

/* Independent watchdog, see periph.c for the timeout. Once started it
 * cannot be stopped, periph_watchdog_kick() has to run in the main loop. */
void periph_watchdog_start(void);
void periph_watchdog_kick(void);

#endif /* TETRIS_PERIPH_H */
