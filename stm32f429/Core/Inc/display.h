#ifndef TETRIS_DISPLAY_H
#define TETRIS_DISPLAY_H
/*
 * ILI9341 (240x320) on the LTDC of the STM32F429I-DISC1.
 *
 * Two framebuffers of one byte per pixel in internal SRAM, drawn into
 * alternately and swapped at vertical blanking. A pixel is an index into
 * the 256 colour palette asset_pal (assets.h), which the LTDC looks up in
 * hardware (format L8 with CLUT). The board's SDRAM is not used and is
 * kept in power-down.
 *
 * LTDC pins, SPI5 and the panel register sequence come from the ST board
 * support package in Drivers/BSP.
 */
#include <stdint.h>

/* Panel orientation at start. 0 = as the board is labelled (ST-LINK USB at
 * the top), 1 = turned by 180 degrees, e.g. to stand the board with the
 * cable going down. Set from the Makefile, "make ROTATE=1".
 * display_set_rotated() changes it while running.
 *
 * One register in the panel (MADCTL), not a transform of the framebuffer,
 * so it costs nothing. 90 degrees is not possible this way. */
#ifndef DISPLAY_ROTATE_180
#define DISPLAY_ROTATE_180  0
#endif

#define DISPLAY_WIDTH    240U
#define DISPLAY_HEIGHT   320U

/* Bring up LTDC, panel and both layers. 0 = ok, -1 = clock or LTDC setup
 * failed, -2 = layer or palette setup failed. */
int display_init(void);

/* 1 = the back buffer is free to draw into. 0 = the swap scheduled by the
 * last present() has not happened yet, so the back buffer is still on
 * screen. */
int display_ready(void);

/* The buffer to draw the next frame into, DISPLAY_WIDTH x DISPLAY_HEIGHT
 * palette indices, row major. Only valid while display_ready(). */
uint8_t *display_back_buffer(void);

/* 0 or 1, which of the two buffers display_back_buffer() is. Stays with
 * the buffer, so it can index per buffer state (render_cache_t). */
int display_back_index(void);

/* Hand the back buffer to the LTDC and take the other one. Returns
 * immediately, the hardware swaps at the next vertical blanking. */
void display_present(void);

/* The colours for the palette indices, 256 entries 0x00RRGGBB, copied.
 * NULL = asset_pal. Shown from the next present() on, each layer gets it
 * as it comes on screen: it costs 256 register writes per present(), so
 * set it only when it changes. */
void display_set_palette(const uint32_t pal[256]);

/* The whole picture moved by dx, dy pixels (right, down), each clamped to
 * +-DISPLAY_SHIFT_MAX, from the next present() on. Moves the layer
 * window, not the pixels: what is pushed over the edge is not shown, the
 * strip left on the other side is black. */
#define DISPLAY_SHIFT_MAX  4
void display_set_shift(int dx, int dy);

/* Picture turned by 180 degrees (1) or not (0). Writes the panel register
 * over SPI only when it changes. */
void display_set_rotated(int rotated);
int  display_rotated(void);

#endif /* TETRIS_DISPLAY_H */
