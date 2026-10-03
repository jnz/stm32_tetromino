/*
 * Tetromino desktop ornament, STM32F429I-DISC1.
 *
 * The AI plays the port of the browser Tetromino (../game, from
 * https://zwiener.org/tetromino.html), the panel shows it, and the
 * all-time best score lives in flash. Everything game related is
 * platform independent and shared with the host simulator, this file
 * only paces it: one game step every TETRIS_TICK_MS from the
 * SysTick clock, and a fresh frame into the back buffer after each step.
 *
 * Copyright (c) 2026 Jan Zwiener (jan@zwiener.org)
 */
#include "main.h"
#include "periph.h"
#include "display.h"
#include "touch.h"
#include "app.h"
#include "render.h"

/* Per mille of pieces the AI places at random. 0 = perfect play, which in
 * host runs went on for hours at level 20 without losing. With 50 the
 * median game lasts about 20 minutes. "make AI_BLUNDER=..." */
#ifndef AI_BLUNDER
#define AI_BLUNDER 0
#endif

/* Touch control, off for now: it does not work reliably yet on the board.
 * "make TOUCH=1" builds it in. Without it no touch is ever seen, the game
 * logic for it (app_touch()) stays in and simply never takes over. */
#ifndef TETRIS_TOUCH
#define TETRIS_TOUCH 0
#endif

/* Green LED per cleared line, red for records and game overs (app.h).
 * "make LEDS=0" keeps both dark. */
#ifndef TETRIS_LEDS
#define TETRIS_LEDS 1
#endif

_Static_assert(RENDER_W == DISPLAY_WIDTH && RENDER_H == DISPLAY_HEIGHT,
               "renderer and panel disagree on the frame size");

/* Timing of the main loop in CPU cycles (180 per microsecond), for a look
 * through the debugger: worst game step (the AI thinks in the first step
 * of every piece), worst frame, and how many of each there were. */
typedef struct {
    uint32_t tick_max_cyc;
    uint32_t render_max_cyc;
    uint32_t render_last_cyc;  /* the frame just drawn, usually a partial one */
    uint32_t ticks;
    uint32_t frames;
    uint32_t late_ticks;      /* steps that ran more than a period late */
} timing_t;

volatile timing_t g_timing;

#if TETRIS_TOUCH
/* Last touch panel state, for a look through the debugger. */
volatile touch_state_t g_touch;
#endif

static app_t s_app;

/* What each of the two framebuffers shows, so a frame only redraws what
 * changed in the buffer it goes into (render.h). Zero means "unknown", the
 * first frame into each buffer draws everything. */
static render_cache_t s_cache[2];

/* "make DEBUG=1": keep the core clocked while it sleeps in __WFI(), so a
 * probe can attach to the running board ("mode=HOTPLUG") to read g_timing
 * or the framebuffer. Without it that connect fails more often than not,
 * because the core sleeps nearly all the time. Off by default: it keeps
 * the core's clock running in sleep, which is most of what __WFI() saves.
 * Flashing is not affected, it connects under reset. */
#ifndef TETRIS_DEBUG
#define TETRIS_DEBUG 0
#endif

static void debug_init(void)
{
#if TETRIS_DEBUG
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
#endif

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

int main(void)
{
    uint32_t next;
    int dirty = 0;
    app_touch_t tp = {0};

    HAL_Init();
    periph_clock_config();
    periph_init();
    debug_init();

    if (display_init() != 0) {
        /* Without a panel there is nothing to show. Red LED, and no
         * watchdog, so it stays that way for someone to look at. */
        HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
        for (;;) {
        }
    }

#if TETRIS_TOUCH
    /* A board without a working touch controller still plays: poll()
     * then reports nothing and never touches the bus. */
    (void)touch_init();
#endif

    periph_watchdog_start();

    next = HAL_GetTick();
    app_init(&s_app, periph_random() ^ DWT->CYCCNT, AI_BLUNDER, next);

    for (;;) {
        const uint32_t now = HAL_GetTick();
#if TETRIS_TOUCH
        touch_state_t ts;

        /* Polled every pass, consumed every game step. The press edge is
         * latched until then, a tap can begin and end between two steps. */
        (void)touch_poll(&ts);
        g_touch = ts;
        tp.pressed = ts.pressed;
        tp.x = ts.x;
        tp.y = ts.y;
        if (ts.down)
            tp.down = 1;
#endif

        if ((int32_t)(now - next) >= 0) {
            const uint32_t t0 = DWT->CYCCNT;
            uint32_t dt;

            next += TETRIS_TICK_MS;
            /* Behind by more than a step (a flash erase, a debugger halt):
             * pick up from now instead of running the missed steps back to
             * back. */
            if ((int32_t)(now - next) >= 0) {
                next = now + TETRIS_TICK_MS;
                g_timing.late_ticks++;
            }
            app_touch(&s_app, &tp, now);
            tp.down = 0;
            app_tick(&s_app, now);
            dt = DWT->CYCCNT - t0;
            if (dt > g_timing.tick_max_cyc)
                g_timing.tick_max_cyc = dt;
            g_timing.ticks++;
            dirty = 1;
        }

        if (dirty && display_ready()) {
            const uint32_t t0 = DWT->CYCCNT;
            uint32_t dt;

            app_render(&s_app, display_back_buffer(),
                       &s_cache[display_back_index()]);
            display_present();
            dt = DWT->CYCCNT - t0;
            g_timing.render_last_cyc = dt;
            if (dt > g_timing.render_max_cyc)
                g_timing.render_max_cyc = dt;
            g_timing.frames++;
            dirty = 0;
        }

#if TETRIS_LEDS
        {
            const uint32_t leds = app_leds(&s_app, HAL_GetTick());

            HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin,
                              (leds & APP_LED_GREEN) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin,
                              (leds & APP_LED_RED) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
#endif

        periph_watchdog_kick();
        /* SysTick wakes this every millisecond. */
        __WFI();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    NVIC_SystemReset();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
