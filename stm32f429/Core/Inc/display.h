#ifndef TETRIS_DISPLAY_H
#define TETRIS_DISPLAY_H
/*
 * ILI9341 (240x320) on the LTDC of the STM32F429I-DISC1. Taken from the
 * INSLIB sensor firmware (display.c there).
 *
 * SDRAM, LTDC timings, PLLSAI and the panel register sequence over SPI5
 * come from the ST board support package in Drivers/BSP. This is the thin
 * layer above it: two full frame buffers in SDRAM, drawn into alternately
 * and swapped at vertical blanking.
 */
#include <stdint.h>

/* Panel orientation. 0 = as the board is labelled (ST-LINK USB at the
 * top), 1 = turned by 180 degrees, e.g. to stand the board with the cable
 * going down. Set from the Makefile, "make ROTATE=1".
 *
 * One register in the panel (MADCTL), not a transform of the framebuffer,
 * so it costs nothing. 90 degrees is not possible this way. */
#ifndef DISPLAY_ROTATE_180
#define DISPLAY_ROTATE_180  0
#endif

#define DISPLAY_WIDTH    240U
#define DISPLAY_HEIGHT   320U
/* ARGB8888, as configured by BSP_LCD_LayerDefaultInit(). */
#define DISPLAY_BPP      4U
#define DISPLAY_FB_BYTES (DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP)

/* Bring up SDRAM + LTDC + panel and both layers. 0 = ok, -1 = BSP init
 * failed, -2 = the SDRAM did not read back what was written. */
int display_init(void);

/* 1 = the back buffer is free to draw into. 0 = the swap scheduled by the
 * last present() has not happened yet, so the back buffer is still on
 * screen. */
int display_ready(void);

/* The buffer to draw the next frame into, DISPLAY_WIDTH x DISPLAY_HEIGHT
 * ARGB8888 pixels, row major. Only valid while display_ready(). */
uint32_t *display_back_buffer(void);

/* Hand the back buffer to the LTDC and take the other one. Returns
 * immediately, the hardware swaps at the next vertical blanking. */
void display_present(void);

#endif /* TETRIS_DISPLAY_H */
