#ifndef __MAIN_H
#define __MAIN_H
/*
 * Tetromino desktop ornament on the STM32F429I-DISC1.
 *
 * Only the pins this firmware drives itself are named here. The display
 * path (LTDC, FMC/SDRAM, SPI5, ILI9341 control lines) belongs to the ST
 * board support package in Drivers/BSP and is named there.
 */
#include "stm32f4xx_hal.h"

void Error_Handler(void);

/* On-board L3GD20 gyro chip select. Not used, but it shares SPI5 with the
 * ILI9341, so it is held high to keep the gyro off the bus. */
#define NCS_MEMS_SPI_Pin         GPIO_PIN_1
#define NCS_MEMS_SPI_GPIO_Port   GPIOC

/* User button B1 (blue), active high, pulled down on the board. */
#define B1_Pin                   GPIO_PIN_0
#define B1_GPIO_Port             GPIOA

/* The two on-board LEDs. */
#define LD3_Pin                  GPIO_PIN_13   /* green */
#define LD4_Pin                  GPIO_PIN_14   /* red   */
#define LD3_GPIO_Port            GPIOG
#define LD4_GPIO_Port            GPIOG

#endif /* __MAIN_H */
