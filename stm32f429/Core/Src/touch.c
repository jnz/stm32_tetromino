/*
 * See touch.h. From the INSLIB sensor firmware, comments trimmed to what
 * matters on this board.
 */
#include "touch.h"
#include "display.h"
#include "stm32f429i_discovery.h"
#include "stmpe811.h"
#include <string.h>

/* 30 Hz. Fast enough that a tap of normal length cannot fall between two
 * poll cycles (a deliberate press is 80 ms and up). The game steps at
 * 20 Hz, so faster polling would buy nothing. */
#define TOUCH_PERIOD_MS  33U

/* The BSP's timeout for its blocking I2C helpers, defined in
 * stm32f429i_discovery.c. It ships as I2Cx_TIMEOUT_MAX = 0x3000, which HAL
 * reads as 12288 MILLISECONDS: one unanswered register read would stall
 * the main loop for twelve seconds (and the watchdog would reset the
 * board). The longest transfer here is four bytes, ~200 us at 400 kHz, so
 * 2 ms is ten times the transaction. */
extern uint32_t I2cxTimeout;
#define TOUCH_I2C_TIMEOUT_MS  2U

/* One register transfer per call. Reading a point takes five: status,
 * FIFO level, the point itself and two writes to flush the FIFO.
 * BSP_TS_GetState() does all five back to back, ~420 us, and five
 * timeouts in a row if the chip stops answering. Spread over five passes
 * of the main loop it is one transfer and at most one timeout per pass,
 * and the point is still complete well inside the 33 ms poll period. */
typedef enum {
    TS_IDLE = 0,   /* waiting for the next poll slot              */
    TS_SIZE,       /* contact reported, read the FIFO level       */
    TS_DATA,       /* a point is queued, read X/Y/Z               */
    TS_FLUSH1,     /* FIFO reset, write 1                         */
    TS_FLUSH2      /* FIFO reset, write 0 -- collecting again     */
} ts_step_t;

static uint8_t              s_ok;
static ts_step_t            s_step;
static uint32_t             s_last_ms;
static touch_state_t s_state;

/*
 * Raw 12-bit ADC counts to pixels on the 240x320 portrait panel.
 *
 * The constants are ST's, lifted from BSP_TS_GetState() rather than
 * measured on this unit. They live here because the read path above cannot
 * use that function -- it does all five transfers in one call.
 *
 * Two deliberate differences from the original:
 *   - The arithmetic is signed. Upstream subtracts 360 from a uint16_t and
 *     then tests the result for "<= 0", which is never true on an unsigned
 *     value; it survives on the clamp above it. Signed says what is meant.
 *   - The upper clamp is >= rather than >, so a raw value landing exactly
 *     on the boundary yields the last pixel instead of one past it.
 */
static void map_to_pixels(uint16_t raw_x, uint16_t raw_y,
                          uint16_t *px, uint16_t *py)
{
    int32_t x = (int32_t)raw_x;
    int32_t y = (int32_t)raw_y;

#ifdef USE_STM32F429I_DISCOVERY_REVD
    if      (y > 3700) y = 3700;
    else if (y <  180) y =  180;
    y = 3700 - y;
#else
    y = y - 360;
#endif
    y /= 11;
    if      (y < 0)                  y = 0;
    else if (y >= (int32_t)DISPLAY_HEIGHT) y = (int32_t)DISPLAY_HEIGHT - 1;

    x = (x <= 3000) ? (3870 - x) : (3800 - x);
    x /= 15;
    if      (x < 0)                  x = 0;
    else if (x >= (int32_t)DISPLAY_WIDTH)  x = (int32_t)DISPLAY_WIDTH - 1;

    if (display_rotated()) {
        /* The picture is turned in the panel (display.h), which the touch
         * panel underneath it knows nothing about. Turning the coordinates
         * the same way keeps them what they claim to be: pixels on the
         * picture the user is looking at. Both clamps above ran first, so
         * this cannot leave the screen. */
        x = (int32_t)DISPLAY_WIDTH  - 1 - x;
        y = (int32_t)DISPLAY_HEIGHT - 1 - y;
    }

    *px = (uint16_t)x;
    *py = (uint16_t)y;
}

int touch_init(void)
{
    memset(&s_state, 0, sizeof s_state);
    s_ok      = 0U;
    s_step    = TS_IDLE;
    s_last_ms = HAL_GetTick();

    I2cxTimeout = TOUCH_I2C_TIMEOUT_MS;

    /* Init and Start are the ST component driver's, unchanged. They run
     * once, blocking, at start-up. Only the per-poll path below had to be
     * rewritten. */
    if (stmpe811_ts_drv.ReadID(TS_I2C_ADDRESS) != STMPE811_ID)
        return -1;

    stmpe811_ts_drv.Init(TS_I2C_ADDRESS);
    stmpe811_ts_drv.Start(TS_I2C_ADDRESS);

    s_ok            = 1U;
    s_state.present = 1U;
    return 0;
}

int touch_poll(touch_state_t *out)
{
    int used = 0;

    /* One call wide, cleared before anything can set it again, so a
     * caller acting on the edge sees one press as one event. */
    s_state.down = 0U;

    if (!s_ok) {
        *out = s_state;
        return 0;
    }

    switch (s_step) {
    case TS_IDLE:
        if ((HAL_GetTick() - s_last_ms) < TOUCH_PERIOD_MS)
            break;                          /* not due, no bus access */
        s_last_ms = HAL_GetTick();


        if (IOE_Read(TS_I2C_ADDRESS, STMPE811_REG_TSC_CTRL)
                & STMPE811_TS_CTRL_STATUS) {
            s_step = TS_SIZE;
        } else {
            /* Nothing on the panel. The FIFO is flushed anyway, exactly as
             * the ST driver does it: a point left over from the press that
             * has just ended would otherwise be read as the next one. */
            s_state.pressed = 0U;
            s_step = TS_FLUSH1;
        }
        used = 1;
        break;

    case TS_SIZE:
        /* Contact, but the conversion may not have produced a point yet.
         * Then this cycle ends without one and the next slot asks again --
         * the same thing the ST driver reports as "not touched". */
        s_step = (IOE_Read(TS_I2C_ADDRESS, STMPE811_REG_FIFO_SIZE) > 0U)
                     ? TS_DATA : TS_IDLE;
        used = 1;
        break;

    case TS_DATA: {
        uint8_t  b[4] = {0};
        uint32_t xyz;
        uint16_t px, py;

        (void)IOE_ReadMultiple(TS_I2C_ADDRESS, STMPE811_REG_TSC_DATA_NON_INC,
                               b, sizeof b);
        xyz = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
              ((uint32_t)b[2] <<  8) |  (uint32_t)b[3];
        map_to_pixels((uint16_t)((xyz >> 20) & 0xFFFU),
                      (uint16_t)((xyz >>  8) & 0xFFFU), &px, &py);

        /* Deadband, ST's: a resistive panel wanders by a pixel or two under
         * a finger that is not moving, and a position that twitches every
         * poll is worse than one that lags a couple of pixels. A new press
         * always takes the position, otherwise the first point of a tap
         * near the last one would be swallowed. */
        {
            const int32_t dx = (int32_t)px - (int32_t)s_state.x;
            const int32_t dy = (int32_t)py - (int32_t)s_state.y;
            const int32_t ax = (dx < 0) ? -dx : dx;
            const int32_t ay = (dy < 0) ? -dy : dy;

            if (!s_state.pressed || (ax + ay) > 5) {
                s_state.x = px;
                s_state.y = py;
            }
        }

        if (!s_state.pressed) {
            s_state.down = 1U;
            s_state.presses++;
        }
        s_state.pressed = 1U;
        s_step = TS_FLUSH1;
        used = 1;
        break;
    }

    case TS_FLUSH1:
        IOE_Write(TS_I2C_ADDRESS, STMPE811_REG_FIFO_STA, 0x01U);
        s_step = TS_FLUSH2;
        used = 1;
        break;

    case TS_FLUSH2:
    default:
        IOE_Write(TS_I2C_ADDRESS, STMPE811_REG_FIFO_STA, 0x00U);
        s_step = TS_IDLE;
        used = 1;
        break;
    }

    *out = s_state;
    return used;
}
