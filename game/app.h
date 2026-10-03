#ifndef TETRIS_APP_H
#define TETRIS_APP_H
/*
 * The desktop ornament around the game: the AI plays, and when it loses
 * the game over screen shows the score against the all-time best for a
 * while, then the next game starts. The best score is kept in flash
 * (hiscore.h).
 *
 * Touch takes over: the first touch while the AI plays (or during a game
 * over screen) starts a game for a human, the way any key did in the
 * browser version (https://zwiener.org/tetromino.html). Controls, as
 * the mobile buttons there:
 *
 *    playfield, left third     move left   (held: repeats)
 *    playfield, middle third   rotate
 *    playfield, right third    move right  (held: repeats)
 *    side panel                hard drop
 *
 * After the human's game over the AI plays again. Humans get their own
 * best score, next to the AI's.
 *
 * Platform independent. The firmware and the host simulator both drive it
 * the same way: app_init() once, app_tick() every TETRIS_TICK_MS with a
 * millisecond clock, app_render() whenever a frame is wanted.
 */
#include <stdint.h>
#include "tetris.h"
#include "hiscore.h"
#include "render.h"

/* How long the game over screen stays up. */
#define APP_GAMEOVER_MS     8000U

/* While a game is ahead of the stored best, it is saved this often, so a
 * record game survives a power cut, losing at most this much play. Every
 * save is one 32 byte record, 512 fit a sector. Every 20 minutes, a
 * perfect AI game that never ends erases each of the two sectors every
 * 14 days, and the 10,000 erase cycles the part is rated for last close
 * to 400 years. */
#define APP_CHECKPOINT_MS   (20U * 60U * 1000U)

/* Held left/right: first repeat after the delay, then every period. */
#define APP_REPEAT_DELAY_MS 250U
#define APP_REPEAT_MS       100U

/* How long the control hints stay up at the start of a human game. */
#define APP_HINT_MS         4000U

/* Touches this soon after a game over are ignored, so the press that was
 * still trying to save the game does not start the next one. */
#define APP_TAKEOVER_GUARD_MS 1000U

/* The user button (B1, blue, on the DISC1) switches fast drop on and off
 * as it goes down: the AI drops a piece the moment it is in place
 * (ai_superfast, the "i" key of the browser version), which scores a point
 * per row dropped. The setting is kept in flash, APP_SETTINGS_SAVE_MS after
 * the last change, so a few presses in a row write one record. */
#define APP_SETTINGS_SAVE_MS  5000U

/* The two user LEDs (green and red on the DISC1):
 *   green  blinks once per line cleared, four times for a tetromino clear
 *   red    6 blinks when the running game overtakes the all-time best,
 *          blinks along with "NEW RECORD!" on a record game over screen,
 *          on for a second after any other game over */
#define APP_LED_GREEN  0x1U
#define APP_LED_RED    0x2U

/* A blink pattern: count times on_ms on, off_ms off, from start_ms. */
typedef struct {
    uint32_t start_ms;
    uint16_t on_ms;
    uint16_t off_ms;
    uint16_t count;
} app_blink_t;

/* Touch panel state in render coordinates (RENDER_W x RENDER_H). */
typedef struct {
    uint8_t  pressed;   /* finger on the panel */
    uint8_t  down;      /* a press began since the last app_touch() */
    uint16_t x;
    uint16_t y;
} app_touch_t;

typedef struct {
    tetris_t  game;
    hiscore_t hs;              /* as stored */
    uint32_t  best_before;     /* best score when this game started */
    uint32_t  lines_before;    /* ... and its lines */
    uint32_t  rng;             /* seeds the games */
    uint32_t  now_ms;
    uint32_t  over_since_ms;
    uint32_t  checkpoint_ms;
    uint16_t  blunder;         /* tetris_t.ai_blunder for every game */
    uint8_t   game_over;
    uint8_t   new_record;
    uint8_t   human;           /* a person plays this game */
    int8_t    hold;            /* zone held for repeat, -1 = none */
    uint32_t  hold_since_ms;
    uint32_t  repeat_ms;
    uint32_t  started_ms;      /* start of this game */
    app_blink_t led_green;
    app_blink_t led_red;
    uint8_t   fast;            /* fast drop for the AI */
    uint8_t   btn_down;        /* button seen pressed at the last call */
    uint8_t   settings_dirty;  /* changed, not saved yet */
    uint32_t  settings_ms;     /* time of the last change */
} app_t;

/* Loads the high score and starts the first game. seed should differ
 * from boot to boot (hardware RNG on the target). blunder is the AI's
 * per mille of random placements, 0 = perfect play (which in practice
 * never ends). */
void app_init(app_t *a, uint32_t seed, uint16_t blunder, uint32_t now_ms);

/* The user button's level, 1 = pressed. Call once per app_tick(). Sampled
 * at the game step, which also debounces it: a contact bounces for a few
 * milliseconds, a step is 50. */
void app_button(app_t *a, int pressed, uint32_t now_ms);

/* Touch input. Call right before every app_tick(), it sets the keys that
 * tick consumes. */
void app_touch(app_t *a, const app_touch_t *tp, uint32_t now_ms);

/* One game step. Call every TETRIS_TICK_MS. May write the flash when a
 * game ends, which blocks for a few ms (and for an erase every 512th
 * save). */
void app_tick(app_t *a, uint32_t now_ms);

/* Which LEDs are lit at now_ms, APP_LED_* bits. Blinks are shorter than a
 * game step, so call this on every main loop pass, not only per step. */
uint32_t app_leds(const app_t *a, uint32_t now_ms);

/* Draws the current state into a RENDER_W x RENDER_H buffer of palette
 * indices (render.h).
 * cache describes what fb shows (render.h), one per framebuffer. NULL
 * draws every pixel. */
void app_render(const app_t *a, uint8_t *fb, render_cache_t *cache);

#endif /* TETRIS_APP_H */
