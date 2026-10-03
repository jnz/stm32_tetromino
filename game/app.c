/*
 * See app.h.
 */
#include "app.h"
#include "render.h"

enum { ZONE_NONE = -1, ZONE_LEFT, ZONE_ROT, ZONE_RIGHT, ZONE_DROP };

static void blink(app_blink_t *b, uint32_t now_ms, uint16_t on_ms,
                  uint16_t off_ms, uint16_t count)
{
    b->start_ms = now_ms;
    b->on_ms = on_ms;
    b->off_ms = off_ms;
    b->count = count;
}

static int blink_lit(const app_blink_t *b, uint32_t now_ms)
{
    const uint32_t period = (uint32_t)b->on_ms + b->off_ms;
    const uint32_t t = now_ms - b->start_ms;

    if (b->count == 0U || period == 0U || t / period >= b->count)
        return 0;
    return (t % period) < b->on_ms;
}

uint32_t app_leds(const app_t *a, uint32_t now_ms)
{
    return (blink_lit(&a->led_green, now_ms) ? APP_LED_GREEN : 0U) |
           (blink_lit(&a->led_red, now_ms) ? APP_LED_RED : 0U);
}

static uint32_t next_seed(app_t *a)
{
    /* xorshift32, only to spread one boot seed over many games */
    uint32_t x = a->rng;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    a->rng = x;
    return x;
}

static void new_game(app_t *a, int human)
{
    tetris_init(&a->game, next_seed(a));
    a->game.ai_blunder = a->blunder;
    a->game.ai_active = (uint8_t)!human;
    a->human = (uint8_t)(human != 0);
    a->best_before = human ? a->hs.human_best : a->hs.best_score;
    a->lines_before = human ? 0U : a->hs.best_lines;
    a->game_over = 0;
    a->new_record = 0;
    a->hold = ZONE_NONE;
    a->started_ms = a->now_ms;
    a->checkpoint_ms = a->now_ms;
}

void app_init(app_t *a, uint32_t seed, uint16_t blunder, uint32_t now_ms)
{
    hiscore_load(&a->hs);
    /* The game count keeps the seed moving even if the platform's source
     * of randomness came up empty. */
    a->rng = (seed ^ (a->hs.games * 0x9E3779B9U)) | 1U;
    a->blunder = blunder;
    a->now_ms = now_ms;
    new_game(a, 0);
}

/* --------------------------------------------------------------------- */
/* Touch                                                                 */
/* --------------------------------------------------------------------- */

static int zone_of(const app_touch_t *tp)
{
    if (tp->x >= RENDER_FIELD_W)
        return ZONE_DROP;
    if (tp->x < RENDER_FIELD_W / 3)
        return ZONE_LEFT;
    if (tp->x < 2 * RENDER_FIELD_W / 3)
        return ZONE_ROT;
    return ZONE_RIGHT;
}

static void press(app_t *a, int zone)
{
    switch (zone) {
    case ZONE_LEFT:  a->game.keys.left = 1;      break;
    case ZONE_RIGHT: a->game.keys.right = 1;     break;
    case ZONE_ROT:   a->game.keys.rot_left = 1;  break;
    case ZONE_DROP:  a->game.keys.warp_down = 1; break;
    default:                                     break;
    }
}

void app_touch(app_t *a, const app_touch_t *tp, uint32_t now_ms)
{
    a->now_ms = now_ms;

    if (tp->down) {
        /* Taking over. This touch only starts the game, it does not also
         * move the piece of a game the player has not seen yet. */
        if (!a->human || a->game_over) {
            if (a->game_over && now_ms - a->over_since_ms < APP_TAKEOVER_GUARD_MS)
                return;
            new_game(a, 1);
            return;
        }
        a->hold = (int8_t)zone_of(tp);
        a->hold_since_ms = now_ms;
        a->repeat_ms = now_ms;
        press(a, a->hold);
        if (a->hold != ZONE_LEFT && a->hold != ZONE_RIGHT)
            a->hold = ZONE_NONE;
        return;
    }

    if (!tp->pressed || !a->human || a->game_over) {
        a->hold = ZONE_NONE;
        return;
    }

    if (a->hold != ZONE_NONE) {
        /* Sliding the finger over to the other side keeps it going, in the
         * new direction. */
        const int z = zone_of(tp);

        if (z == ZONE_LEFT || z == ZONE_RIGHT)
            a->hold = (int8_t)z;
        if (now_ms - a->hold_since_ms >= APP_REPEAT_DELAY_MS &&
            now_ms - a->repeat_ms >= APP_REPEAT_MS) {
            a->repeat_ms = now_ms;
            press(a, a->hold);
        }
    }
}

/* --------------------------------------------------------------------- */
/* Game                                                                  */
/* --------------------------------------------------------------------- */

/* Moves this game's score into the stored bests where it beats them. */
static void take_score(app_t *a)
{
    if (a->human) {
        if (a->game.score > a->hs.human_best)
            a->hs.human_best = a->game.score;
        return;
    }
    if (a->game.score > a->hs.best_score) {
        a->hs.best_score = a->game.score;
        a->hs.best_lines = a->game.lines;
        a->hs.best_level = a->game.level;
    }
}

void app_tick(app_t *a, uint32_t now_ms)
{
    a->now_ms = now_ms;

    if (a->game_over) {
        /* After a human's game the AI takes over again. */
        if (now_ms - a->over_since_ms >= APP_GAMEOVER_MS)
            new_game(a, 0);
        return;
    }

    {
        const uint32_t lines = a->game.lines;
        const uint32_t score = a->game.score;

        tetris_tick(&a->game);

        if (a->game.lines > lines)
            blink(&a->led_green, now_ms, 60U, 90U,
                  (uint16_t)(a->game.lines - lines));
        /* The moment this game passes the record, once. Not for the very
         * first game on a fresh board, where any score would do it. */
        if (a->best_before > 0U && score <= a->best_before &&
            a->game.score > a->best_before)
            blink(&a->led_red, now_ms, 150U, 150U, 6U);
    }

    if (a->game.state == TETRIS_GAMEOVER) {
        a->game_over = 1;
        a->hold = ZONE_NONE;
        a->over_since_ms = now_ms;
        a->new_record = a->game.score > a->best_before;
        if (a->new_record)   /* in step with "NEW RECORD!" on screen */
            blink(&a->led_red, now_ms, 400U, 400U,
                  (uint16_t)(APP_GAMEOVER_MS / 800U));
        else
            blink(&a->led_red, now_ms, 1000U, 0U, 1U);
        a->hs.games++;
        take_score(a);
        (void)hiscore_save(&a->hs);   /* a failed save only costs the record */
        return;
    }

    if (a->game.score > (a->human ? a->hs.human_best : a->hs.best_score) &&
        now_ms - a->checkpoint_ms >= APP_CHECKPOINT_MS) {
        a->checkpoint_ms = now_ms;
        take_score(a);
        (void)hiscore_save(&a->hs);
    }
}

void app_render(const app_t *a, uint8_t *fb, render_cache_t *cache)
{
    render_info_t info;

    /* The best as it was before this game. The panel shows the running
     * game against it, and the game over screen decides "new record" on
     * it, so a checkpoint saved during the game changes neither. */
    info.best_score = a->best_before;
    info.best_lines = a->lines_before;
    info.games = a->hs.games;
    info.game_over = a->game_over;
    info.new_record = a->new_record;
    info.human = a->human;
    info.hint = a->human && !a->game_over &&
                a->now_ms - a->started_ms < APP_HINT_MS;
    /* Counted from the game over, so "NEW RECORD!" blinks in step with
     * the red LED (app_leds()). */
    info.anim_ms = a->game_over ? a->now_ms - a->over_since_ms : a->now_ms;
    render_frame(fb, cache, &a->game, &info);
}
