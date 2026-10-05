/*
 * Host tests for the game core, the high score store, the ornament flow
 * and the renderer. Plain C, no framework: scenario_* functions with CHECK
 * macros, the binary exits non-zero on the first failing scenario set.
 */
#include "app.h"
#include "hiscore.h"
#include "render.h"
#include "tetris.h"
#include "flash_ram.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>

static int s_fail;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        s_fail++; \
    } \
} while (0)

static void empty_game(tetris_t *t)
{
    tetris_init(t, 42U);
    t->ai_active = 0;
    memset(t->map, 0, sizeof t->map);
}

/* --------------------------------------------------------------------- */

static void scenario_bag_is_permutation(void)
{
    tetris_t t;
    int seen[TETRIS_PIECES] = { 0 };
    int i;

    tetris_init(&t, 7U);
    /* After init the bag has handed out two pieces (current and next),
     * five more complete the first bag. */
    seen[t.block]++;
    seen[t.next]++;
    for (i = 0; i < 5; i++) {
        t.state = TETRIS_FIRSTBLOCK;
        t.block = t.next;
        t.next = t.bag[t.bagindex++];
        seen[t.next]++;
    }
    for (i = 0; i < TETRIS_PIECES; i++)
        CHECK(seen[i] == 1);
}

static void scenario_single_line_clear(void)
{
    tetris_t t;
    int c, n;

    empty_game(&t);
    /* Bottom row full except the four cells a flat I fills. */
    for (c = 4; c < TETRIS_COLS; c++)
        t.map[TETRIS_ROWS - 1][c] = 2;
    t.block = 0;
    t.rot = 0;
    t.x = 0;
    t.y = 5;      /* rotation 0 of I occupies row 1 of its box */

    for (n = 0; n < 200 && t.state != TETRIS_CLEARLINES; n++) {
        t.keys.fast_down = 1;
        tetris_tick(&t);
    }
    CHECK(t.state == TETRIS_CLEARLINES);
    CHECK(t.lines == 1U);
    CHECK(t.score == 40U);
    CHECK(t.linestat[0] == 1U);
    CHECK(t.map[TETRIS_ROWS - 1][0] < 0);   /* flashing */

    for (n = 0; n < 20 && t.state == TETRIS_CLEARLINES; n++)
        tetris_tick(&t);
    CHECK(t.state == TETRIS_FIRSTBLOCK);
    for (c = 0; c < TETRIS_COLS; c++)
        CHECK(t.map[TETRIS_ROWS - 1][c] == 0);
}

static void scenario_tetromino_clear_scores_1200(void)
{
    tetris_t t;
    int r, c, n;

    empty_game(&t);
    for (r = TETRIS_ROWS - 4; r < TETRIS_ROWS; r++)
        for (c = 0; c < TETRIS_COLS - 1; c++)
            t.map[r][c] = 3;
    t.block = 0;
    t.rot = 1;           /* vertical, occupies column 2 of its box */
    t.x = (int8_t)(TETRIS_COLS - 1 - 2);
    t.y = 2;

    for (n = 0; n < 200 && t.state != TETRIS_CLEARLINES; n++) {
        t.keys.fast_down = 1;
        tetris_tick(&t);
    }
    CHECK(t.lines == 4U);
    CHECK(t.score == 1200U);
    for (n = 0; n < 40 && t.state == TETRIS_CLEARLINES; n++)
        tetris_tick(&t);
    for (r = 0; r < TETRIS_ROWS; r++)
        for (c = 0; c < TETRIS_COLS; c++)
            CHECK(t.map[r][c] == 0);
}

static void scenario_score_saturates(void)
{
    tetris_t t;
    int c, n;

    empty_game(&t);
    t.score = UINT64_MAX - 10U;
    for (c = 4; c < TETRIS_COLS; c++)
        t.map[TETRIS_ROWS - 1][c] = 2;
    t.block = 0;
    t.rot = 0;
    t.x = 0;
    t.y = 5;
    for (n = 0; n < 200 && t.state != TETRIS_CLEARLINES; n++) {
        t.keys.fast_down = 1;
        tetris_tick(&t);
    }
    CHECK(t.lines == 1U);
    CHECK(t.score == UINT64_MAX);
}

static void scenario_hard_drop_adds_distance(void)
{
    tetris_t t;

    empty_game(&t);
    t.block = 3;      /* O, 2x2 */
    t.rot = 0;
    t.x = 4;
    t.y = 0;
    t.frame = 1;      /* no gravity in this tick */
    t.keys.warp_down = 1;
    tetris_tick(&t);
    CHECK(t.score == (uint64_t)(TETRIS_ROWS - 2));
    CHECK(t.warpcount == 1U);
}

static void scenario_level_up_every_ten_lines(void)
{
    tetris_t t;
    int c, n, k;

    empty_game(&t);
    for (k = 0; k < 10; k++) {
        for (c = 0; c < TETRIS_COLS; c++)
            t.map[TETRIS_ROWS - 1][c] = (c < 2) ? 0 : 1;
        t.state = TETRIS_FIRSTBLOCK;
        t.block = 3;
        t.rot = 0;
        t.x = 0;
        t.y = 5;
        for (n = 0; n < 300 && t.state != TETRIS_CLEARLINES; n++) {
            t.keys.fast_down = 1;
            tetris_tick(&t);
        }
        /* The O fills two rows, one of them completes. */
        for (n = 0; n < 20 && t.state == TETRIS_CLEARLINES; n++)
            tetris_tick(&t);
        memset(t.map, 0, sizeof t.map);
    }
    CHECK(t.lines == 10U);
    CHECK(t.level == 2U);
    CHECK(t.speed == 19U);
}

static void scenario_game_over_when_spawn_blocked(void)
{
    tetris_t t;
    int r, c;

    empty_game(&t);
    for (r = 1; r < TETRIS_ROWS; r++)
        for (c = 0; c < TETRIS_COLS; c++)
            t.map[r][c] = (c == r % TETRIS_COLS) ? 0 : 5;
    t.state = TETRIS_FIRSTBLOCK;
    t.frame = 0;
    tetris_tick(&t);
    tetris_tick(&t);
    CHECK(t.state == TETRIS_GAMEOVER);

    /* Over is over. */
    {
        const uint32_t ticks = t.ticks;

        tetris_tick(&t);
        CHECK(t.ticks == ticks);
    }
}

static void scenario_perfect_ai_survives(void)
{
    tetris_t t;
    uint32_t n;

    tetris_init(&t, 99U);
    for (n = 0; n < 20000U && t.state != TETRIS_GAMEOVER; n++)
        tetris_tick(&t);
    CHECK(t.state != TETRIS_GAMEOVER);
    CHECK(t.lines > 300U);
}

static void scenario_ai_reaches_right_wall(void)
{
    /* Rows 19..21 full up to column 7, column 8 only in row 21, column 9
     * empty. A T pointing left (rotation 3) at box column 8 drops its nub
     * onto (21,8) and completes rows 20 and 21. The column range of the
     * browser AI (-1 .. COLS-3) never tried box column 8, and no other
     * placement of the T completes two lines here.
     *
     * A tower in column 0, at least as high as AI_SAFE_HEIGHT in
     * tetris.c, makes the AI play for survival. Below it, it would keep
     * column 9 empty as the well for a tetromino clear, and not want the
     * double. */
    tetris_t t;
    int r, c, n;

    empty_game(&t);
    t.ai_active = 1;
    for (r = TETRIS_ROWS - 3; r < TETRIS_ROWS; r++)
        for (c = 0; c < TETRIS_COLS - 2; c++)
            t.map[r][c] = 1;
    for (r = TETRIS_ROWS - 10; r < TETRIS_ROWS; r++)
        t.map[r][0] = 1;
    t.map[TETRIS_ROWS - 1][TETRIS_COLS - 2] = 1;
    t.block = 5;   /* T */
    t.next = 3;    /* O */
    t.state = TETRIS_FIRSTBLOCK;
    for (n = 0; n < 400 && t.lines == 0U && t.state != TETRIS_GAMEOVER; n++)
        tetris_tick(&t);
    CHECK(t.linestat[1] == 1U);
}

/* --------------------------------------------------------------------- */

static void scenario_hiscore_fresh_flash(void)
{
    hiscore_t h;

    flash_ram_reset(0xFF);
    hiscore_load(&h);
    CHECK(h.best_score == 0U && h.games == 0U);

    /* Leftovers of something else in the sectors are not records. */
    flash_ram_reset(0x5A);
    hiscore_load(&h);
    CHECK(h.best_score == 0U && h.games == 0U);
    h.best_score = 1234U;
    h.games = 1U;
    CHECK(hiscore_save(&h) == 0);
    CHECK(flash_ram_erases == 1U);
    memset(&h, 0, sizeof h);
    hiscore_load(&h);
    CHECK(h.best_score == 1234U && h.games == 1U);
}

static void scenario_hiscore_rotates_sectors(void)
{
    hiscore_t h;
    uint32_t i;
    const uint32_t saves = 2U * (HS_SECTOR_SZ / 64U) + 100U;

    flash_ram_reset(0xFF);
    hiscore_load(&h);
    for (i = 1; i <= saves; i++) {
        h.games = i;
        h.best_score = i * 10U;
        CHECK(hiscore_save(&h) == 0);
    }
    /* First save, then once per sector change. */
    CHECK(flash_ram_erases == 3U);
    memset(&h, 0, sizeof h);
    hiscore_load(&h);
    CHECK(h.games == saves);
    CHECK(h.best_score == saves * 10U);

    /* And it keeps going after a reload. */
    h.games++;
    CHECK(hiscore_save(&h) == 0);
    hiscore_load(&h);
    CHECK(h.games == saves + 1U);
}

/* Scores past 2^32, which a perfect AI reaches within weeks. */
static void scenario_hiscore_64bit_scores(void)
{
    hiscore_t h;

    flash_ram_reset(0xFF);
    hiscore_load(&h);
    h.best_score = 0x123456789ABCULL;
    h.human_best = 0xFFFFFFFFULL + 7U;
    h.best_lines = 4000000000U;
    h.best_level = 20U;
    h.settings = 0xABCDEFU;
    h.games = 77U;
    CHECK(hiscore_save(&h) == 0);
    memset(&h, 0, sizeof h);
    hiscore_load(&h);
    CHECK(h.best_score == 0x123456789ABCULL);
    CHECK(h.human_best == 0xFFFFFFFFULL + 7U);
    CHECK(h.best_lines == 4000000000U && h.best_level == 20U);
    CHECK(h.settings == 0xABCDEFU && h.games == 77U);
}

static void scenario_hiscore_survives_torn_write(void)
{
    hiscore_t h;

    flash_ram_reset(0xFF);
    hiscore_load(&h);
    h.games = 5U;
    h.best_score = 500U;
    CHECK(hiscore_save(&h) == 0);

    /* The next write is cut short. The save moves on to the next slot. */
    flash_ram_fail_program = 1;
    h.games = 6U;
    h.best_score = 600U;
    CHECK(hiscore_save(&h) == 0);
    hiscore_load(&h);
    CHECK(h.games == 6U && h.best_score == 600U);

    /* Cut short with nothing after it: the old record wins. */
    flash_ram_fail_program = 1000000;
    h.games = 7U;
    CHECK(hiscore_save(&h) == -1);
    flash_ram_fail_program = 0;
    hiscore_load(&h);
    CHECK(h.games == 6U && h.best_score == 600U);
}

/* --------------------------------------------------------------------- */

static void scenario_app_records_game_over(void)
{
    static app_t a;
    uint32_t now = 0;
    uint32_t n;
    uint64_t score;

    flash_ram_reset(0xFF);
    /* Every piece placed at random: the game is over in a minute or two. */
    app_init(&a, 3U, 1000U, now);
    for (n = 0; n < 100000U && !a.game_over; n++) {
        now += TETRIS_TICK_MS;
        app_tick(&a, now);
    }
    CHECK(a.game_over);
    score = a.game.score;
    CHECK(a.hs.games == 1U);
    CHECK(a.hs.best_score == score);
    CHECK(a.new_record == (score > 0U));

    /* The game over screen stays, then a new game starts. */
    now += APP_GAMEOVER_MS - TETRIS_TICK_MS;
    app_tick(&a, now);
    CHECK(a.game_over);
    now += TETRIS_TICK_MS;
    app_tick(&a, now);
    CHECK(!a.game_over);
    CHECK(a.game.score == 0U);
    CHECK(a.best_before == score);

    /* Power cycle. */
    memset(&a, 0, sizeof a);
    app_init(&a, 4U, 1000U, 0U);
    CHECK(a.hs.games == 1U);
    CHECK(a.hs.best_score == score);
}

/* --------------------------------------------------------------------- */

static void touch_at(app_t *a, int down, int pressed, int x, int y, uint32_t now)
{
    app_touch_t tp;

    tp.down = (uint8_t)down;
    tp.pressed = (uint8_t)pressed;
    tp.x = (uint16_t)x;
    tp.y = (uint16_t)y;
    app_touch(a, &tp, now);
}

/* Fresh store, AI game running, then one tap: a human game. */
static uint32_t human_game(app_t *a)
{
    uint32_t now = 0;
    int n;

    flash_ram_reset(0xFF);
    app_init(a, 11U, 50U, now);
    for (n = 0; n < 40; n++) {
        now += TETRIS_TICK_MS;
        touch_at(a, 0, 0, 0, 0, now);
        app_tick(a, now);
    }
    now += TETRIS_TICK_MS;
    touch_at(a, 1, 1, 75, 200, now);
    app_tick(a, now);
    /* finger up */
    now += TETRIS_TICK_MS;
    touch_at(a, 0, 0, 75, 200, now);
    app_tick(a, now);
    return now;
}

static void scenario_touch_takes_over(void)
{
    static app_t a;

    human_game(&a);
    CHECK(a.human == 1U);
    CHECK(a.game.ai_active == 0U);
    /* The takeover tap starts the game and nothing else. */
    CHECK(a.game.rot == 0U);
    CHECK(a.game.x == TETRIS_COLS / 2 - 2);
}

static void scenario_touch_zones(void)
{
    static app_t a;
    uint32_t now = human_game(&a);
    int x0 = a.game.x;
    int r0 = a.game.rot;

    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 10, 200, now);       /* left third */
    app_tick(&a, now);
    CHECK(a.game.x == x0 - 1);

    now += TETRIS_TICK_MS;
    touch_at(&a, 0, 0, 10, 200, now);
    app_tick(&a, now);
    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 140, 200, now);      /* right third */
    app_tick(&a, now);
    CHECK(a.game.x == x0);

    now += TETRIS_TICK_MS;
    touch_at(&a, 0, 0, 140, 200, now);
    app_tick(&a, now);
    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 75, 200, now);       /* middle: rotate */
    app_tick(&a, now);
    CHECK(a.game.rot == (uint8_t)((r0 + 1) % 4) || a.game.block == 3);

    now += TETRIS_TICK_MS;
    touch_at(&a, 0, 0, 75, 200, now);
    app_tick(&a, now);
    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 200, 200, now);      /* side panel: hard drop */
    app_tick(&a, now);
    CHECK(a.game.warpcount == 1U);
    CHECK(a.game.score > 0U);
}

static void scenario_touch_hold_repeats(void)
{
    static app_t a;
    uint32_t now = human_game(&a);
    const int x0 = a.game.x;
    int n;

    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 140, 200, now);
    app_tick(&a, now);
    CHECK(a.game.x == x0 + 1);
    /* Held: nothing more until the delay, then one step per period. */
    for (n = 0; n < 4; n++) {
        now += TETRIS_TICK_MS;
        touch_at(&a, 0, 1, 140, 200, now);
        app_tick(&a, now);
    }
    CHECK(a.game.x == x0 + 1);
    for (n = 0; n < 40; n++) {
        now += TETRIS_TICK_MS;
        touch_at(&a, 0, 1, 140, 200, now);
        app_tick(&a, now);
    }
    /* At the wall by now, whatever the piece. */
    CHECK(a.game.x >= TETRIS_COLS - tetris_piece_size(a.game.block));
}

static void scenario_human_best_separate(void)
{
    static app_t a;
    uint32_t now = human_game(&a);
    uint32_t ai_best, n;

    ai_best = a.hs.best_score;
    /* Nobody touches: gravity alone ends the game, after a few minutes. */
    for (n = 0; n < 200000U && !a.game_over; n++) {
        now += TETRIS_TICK_MS;
        touch_at(&a, 0, 0, 0, 0, now);
        app_tick(&a, now);
    }
    CHECK(a.game_over);
    CHECK(a.hs.best_score == ai_best);
    CHECK(a.hs.human_best == a.game.score);

    /* A touch right after the game over does not restart yet ... */
    now += TETRIS_TICK_MS;
    touch_at(&a, 1, 1, 75, 200, now);
    app_tick(&a, now);
    CHECK(a.game_over);
    /* ... and without one the AI is back after the game over screen. */
    now += APP_GAMEOVER_MS;
    touch_at(&a, 0, 0, 0, 0, now);
    app_tick(&a, now);
    CHECK(!a.game_over);
    CHECK(a.human == 0U && a.game.ai_active == 1U);

    {
        hiscore_t h;

        hiscore_load(&h);
        CHECK(h.human_best == a.hs.human_best);
        CHECK(h.games == a.hs.games);
    }
}

static void scenario_leds(void)
{
    static app_t a;
    uint32_t now = 0;
    uint32_t n, lines;

    flash_ram_reset(0xFF);
    app_init(&a, 21U, 0U, now);
    CHECK(app_leds(&a, now) == 0U);

    /* Green: one blink per line, on the tick the lines are counted. */
    for (n = 0; n < 20000U && a.game.lines == 0U; n++) {
        now += TETRIS_TICK_MS;
        app_tick(&a, now);
    }
    lines = a.game.lines;
    CHECK(lines > 0U);
    CHECK(app_leds(&a, now) & APP_LED_GREEN);
    CHECK(!(app_leds(&a, now + 70U) & APP_LED_GREEN));
    CHECK(((app_leds(&a, now + 150U * (lines - 1U)) & APP_LED_GREEN) != 0U));
    CHECK(!(app_leds(&a, now + 150U * lines) & APP_LED_GREEN));

    /* Red: overtaking the record. */
    a.best_before = a.game.score + 1U;
    for (n = 0; n < 20000U && a.game.score <= a.best_before; n++) {
        now += TETRIS_TICK_MS;
        app_tick(&a, now);
    }
    CHECK(app_leds(&a, now) & APP_LED_RED);
    CHECK(!(app_leds(&a, now + 6U * 300U) & APP_LED_RED));

    /* Red: a game over without a record is one second on. */
    a.best_before = 0xFFFFFFFFU;
    a.blunder = 1000U;
    a.game.ai_blunder = 1000U;
    for (n = 0; n < 200000U && !a.game_over; n++) {
        now += TETRIS_TICK_MS;
        app_tick(&a, now);
    }
    CHECK(a.game_over && !a.new_record);
    CHECK(app_leds(&a, now + 900U) & APP_LED_RED);
    CHECK(!(app_leds(&a, now + 1000U) & APP_LED_RED));
}

/* Drawing only what changed must give the same pixels as drawing it all.
 * Two buffers alternate like the display's, each with its own cache, and
 * start out as garbage, as the SDRAM does. Several games with game overs,
 * level ups, line flashes and a touch takeover with its hint overlay. */
/* One game step with the button at the given level. */
static uint32_t step_button(app_t *a, int pressed, uint32_t now)
{
    now += TETRIS_TICK_MS;
    app_button(a, pressed, now);
    app_tick(a, now);
    return now;
}

static void scenario_button_short_press_fast_drop(void)
{
    static app_t a;
    uint32_t now = 0;
    int n;

    flash_ram_reset(0xFF);
    app_init(&a, 9U, 0U, now);
    CHECK(!a.fast && !a.game.ai_superfast);

    /* Switches when let go, not while held. */
    now = step_button(&a, 1, now);
    now = step_button(&a, 1, now);
    CHECK(!a.fast);
    now = step_button(&a, 0, now);
    CHECK(a.fast && a.game.ai_superfast);
    CHECK(!a.flipped);

    /* The AI now hard drops. */
    for (n = 0; n < 2000; n++)
        now = step_button(&a, 0, now);
    CHECK(a.game.warpcount > 10U);
    CHECK(!a.settings_dirty);   /* saved by now */

    /* Kept across a power cycle, and the next game has it too. */
    memset(&a, 0, sizeof a);
    app_init(&a, 10U, 0U, 0U);
    CHECK(a.fast && a.game.ai_superfast);

    /* Off again with the next press. */
    now = step_button(&a, 1, 0U);
    now = step_button(&a, 0, now);
    CHECK(!a.fast && !a.game.ai_superfast);
}

static void scenario_button_long_press_flips(void)
{
    static app_t a;
    uint32_t now = 0;
    uint32_t held = 0;
    int n;

    flash_ram_reset(0xFF);
    app_init(&a, 9U, 0U, now);
    CHECK(!a.flipped);

    /* Flips while still held, once the long press time is reached. */
    while (held < APP_LONG_PRESS_MS + TETRIS_TICK_MS) {
        now = step_button(&a, 1, now);
        held += TETRIS_TICK_MS;
        if (held <= APP_LONG_PRESS_MS - TETRIS_TICK_MS)
            CHECK(!a.flipped);
    }
    CHECK(a.flipped);
    /* Held on: no second flip. Let go: not also a short press. */
    for (n = 0; n < 40; n++)
        now = step_button(&a, 1, now);
    now = step_button(&a, 0, now);
    CHECK(a.flipped && !a.fast);

    for (n = 0; n < (int)(APP_SETTINGS_SAVE_MS / TETRIS_TICK_MS) + 1; n++)
        now = step_button(&a, 0, now);
    memset(&a, 0, sizeof a);
    app_init(&a, 10U, 0U, 0U);
    CHECK(a.flipped && !a.fast);
}

static void scenario_settings_and_level_share_a_word(void)
{
    hiscore_t h;

    flash_ram_reset(0xFF);
    hiscore_load(&h);
    CHECK(h.settings == 0U);
    h.best_level = 20U;
    h.settings = 0x5U;
    CHECK(hiscore_save(&h) == 0);
    memset(&h, 0, sizeof h);
    hiscore_load(&h);
    CHECK(h.best_level == 20U);
    CHECK(h.settings == 0x5U);
}

static void scenario_partial_render_matches_full(void)
{
    static uint8_t fb[2][RENDER_W * RENDER_H];
    static uint8_t ref[RENDER_W * RENDER_H];
    static render_cache_t cache[2];
    static app_t a;
    uint32_t now = 0;
    uint32_t n;
    int mismatches = 0;
    int seen_levelup = 0;
    int seen_flash = 0;
    int seen_hint = 0;

    flash_ram_reset(0xFF);
    app_init(&a, 77U, 300U, now);
    a.infotext = "zwiener.org";
    memset(fb, 0x5A, sizeof fb);
    memset(cache, 0, sizeof cache);

    for (n = 0; n < 12000U; n++) {
        const int b = (int)(n % 2U);

        now += TETRIS_TICK_MS;
        if (n == 7000U) {
            /* a human takes over: hint overlay, then a human game */
            touch_at(&a, 1, 1, 75, 200, now);
        } else {
            touch_at(&a, 0, 0, 0, 0, now);
        }
        /* a press at one point: fast drop and its title tag */
        app_button(&a, n >= 3000U && n < 3004U, now);
        app_tick(&a, now);
        seen_levelup |= a.game.levelup_ticks > 0U;
        seen_flash |= a.game.state == TETRIS_CLEARLINES;
        seen_hint |= a.human && !a.game_over;

        app_render(&a, fb[b], &cache[b]);
        app_render(&a, ref, NULL);
        if (memcmp(fb[b], ref, sizeof ref) != 0) {
            if (mismatches == 0) {
                size_t i = 0;

                while (fb[b][i] == ref[i])
                    i++;
                printf("  frame %u differs first at x %u y %u\n", (unsigned)n,
                       (unsigned)(i % RENDER_W), (unsigned)(i / RENDER_W));
            }
            mismatches++;
        }
    }
    CHECK(mismatches == 0);
    /* and the run did cover what it claims to */
    CHECK(a.hs.games >= 3U);
    CHECK(seen_levelup && seen_flash && seen_hint);
}

static void scenario_render_smoke(void)
{
    static uint8_t fb[RENDER_W * RENDER_H];
    static uint8_t fb2[RENDER_W * RENDER_H];
    static app_t a;
    uint32_t i;

    flash_ram_reset(0xFF);
    app_init(&a, 5U, 50U, 0U);
    /* Every pixel written: two buffers that start out different end up
     * the same. */
    memset(fb, 0x00, sizeof fb);
    memset(fb2, 0xFF, sizeof fb2);
    app_render(&a, fb, NULL);
    app_render(&a, fb2, NULL);
    CHECK(memcmp(fb, fb2, sizeof fb) == 0);
    /* Title bar gradient starts at grey 50. */
    CHECK(fb[0] == asset_grey[50]);

    /* The largest score fits the panel: the separator to the playfield
     * stays untouched next to every value row. */
    a.game.score = 0xFFFFFFFFU;
    a.game.lines = 0xFFFFFFFFU;
    app_render(&a, fb, NULL);
    for (i = 120; i < 245; i++)
        CHECK(fb[i * RENDER_W + RENDER_FIELD_W] == asset_grey[80]);

    /* The info text, and one far too long for the panel stays in it. */
    a.game.score = 0U;
    a.game.lines = 0U;
    app_render(&a, fb, NULL);
    a.infotext = "zwiener.org";
    app_render(&a, fb2, NULL);
    CHECK(memcmp(fb, fb2, sizeof fb) != 0);
    a.infotext = "a very long text that does not fit the panel at all";
    app_render(&a, fb2, NULL);
    for (i = 0; i < RENDER_H; i++)
        CHECK(fb2[i * RENDER_W + RENDER_FIELD_W - 1] == fb[i * RENDER_W + RENDER_FIELD_W - 1]);
    a.infotext = NULL;

    a.game_over = 1;
    a.new_record = 1;
    app_render(&a, fb, NULL);
    {
        /* red tint */
        const uint32_t c = asset_pal[fb[RENDER_W * 300 + 5]];

        CHECK(((c >> 16) & 0xFFU) > ((c >> 8) & 0xFFU) + 40U);
    }
}

int main(void)
{
    static const struct {
        const char *name;
        void (*fn)(void);
    } k_scenarios[] = {
        { "bag_is_permutation", scenario_bag_is_permutation },
        { "single_line_clear", scenario_single_line_clear },
        { "tetromino_clear_scores_1200", scenario_tetromino_clear_scores_1200 },
        { "hard_drop_adds_distance", scenario_hard_drop_adds_distance },
        { "score_saturates", scenario_score_saturates },
        { "level_up_every_ten_lines", scenario_level_up_every_ten_lines },
        { "game_over_when_spawn_blocked", scenario_game_over_when_spawn_blocked },
        { "perfect_ai_survives", scenario_perfect_ai_survives },
        { "ai_reaches_right_wall", scenario_ai_reaches_right_wall },
        { "hiscore_fresh_flash", scenario_hiscore_fresh_flash },
        { "hiscore_rotates_sectors", scenario_hiscore_rotates_sectors },
        { "hiscore_64bit_scores", scenario_hiscore_64bit_scores },
        { "hiscore_survives_torn_write", scenario_hiscore_survives_torn_write },
        { "app_records_game_over", scenario_app_records_game_over },
        { "touch_takes_over", scenario_touch_takes_over },
        { "touch_zones", scenario_touch_zones },
        { "touch_hold_repeats", scenario_touch_hold_repeats },
        { "human_best_separate", scenario_human_best_separate },
        { "leds", scenario_leds },
        { "button_short_press_fast_drop", scenario_button_short_press_fast_drop },
        { "button_long_press_flips", scenario_button_long_press_flips },
        { "settings_and_level_share_a_word", scenario_settings_and_level_share_a_word },
        { "partial_render_matches_full", scenario_partial_render_matches_full },
        { "render_smoke", scenario_render_smoke },
    };
    size_t i;
    int failed = 0;

    for (i = 0; i < sizeof k_scenarios / sizeof k_scenarios[0]; i++) {
        const int before = s_fail;

        k_scenarios[i].fn();
        printf("%s %s\n", (s_fail == before) ? "ok  " : "FAIL", k_scenarios[i].name);
        if (s_fail != before)
            failed++;
    }
    printf("%d of %d scenarios failed\n", failed, (int)i);
    return failed ? 1 : 0;
}
