#ifndef TETRIS_TOUCH_H
#define TETRIS_TOUCH_H
/*
 * Resistive touch panel of the STM32F429I-DISC1, read through the STMPE811
 * on I2C3. Taken from the INSLIB sensor firmware (touch.c there), where the
 * bus is shared with the baro and the magnetometer. That is why the read is
 * a state machine doing one register transfer per call: here nothing else
 * is on the bus, but the split costs nothing and keeps every pass of the
 * main loop short.
 *
 * Chip bring-up comes from the ST BSP (stmpe811 component) and runs once
 * from init. poll() advances the read by one step, paced at 30 Hz, and
 * reports a press edge besides the level.
 *
 * COORDINATES are pixels on the 240x320 panel as the picture is shown,
 * rotation included (display_rotated()). The conversion is ST's fixed
 * calibration for a revision D panel (USE_STM32F429I_DISCOVERY_REVD in the
 * Makefile), nominal rather than measured: expect the corners to be a few
 * pixels off.
 */
#include <stdint.h>

typedef struct {
    uint8_t  present;   /* controller answered at init, 0 = no touch at all */
    uint8_t  pressed;   /* 1 while the panel reports contact               */
    uint8_t  down;      /* 1 for the poll cycle in which contact began     */
    uint16_t x;         /* last position with contact, 0..239              */
    uint16_t y;         /* last position with contact, 0..319              */
    uint32_t presses;   /* completed press edges                           */
} touch_state_t;

/* Probes the controller and starts the touch engine. 0 = ok, -1 = the chip
 * did not identify itself, poll() then reports an empty state and never
 * touches the bus again. Blocks for a few ms in the ST reset sequence. */
int touch_init(void);

/* Advances the read by one step and writes the current state. Returns 1
 * if this call used the bus. Call once per main loop pass. */
int touch_poll(touch_state_t *out);

#endif /* TETRIS_TOUCH_H */
