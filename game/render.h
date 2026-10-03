#ifndef TETRIS_RENDER_H
#define TETRIS_RENDER_H
/*
 * Draws one complete frame of the game into an ARGB8888 framebuffer of
 * RENDER_W x RENDER_H pixels (the 240x320 portrait panel of the
 * STM32F429I-DISC1).
 *
 * Only what differs from what the buffer already shows is drawn. That
 * memory lives in a render_cache_t, one per framebuffer: with double
 * buffering the buffer about to be drawn holds the frame from two frames
 * ago, not the last one, so a single "what changed" list would leave stale
 * pixels in every other frame. Each cache knows its own buffer, which
 * makes the double buffer correct by construction. Platform independent,
 * the host simulator uses the same code.
 *
 * Layout:
 *    y   0..19   title bar (the two hidden spawn rows of the browser
 *                version are drawn as this bar there too)
 *    x   0..149  playfield, 10 x 20 visible tiles of 15 px
 *    x 151..239  side panel: next piece, score, lines, level, best, games
 */
#include <stdint.h>
#include "tetris.h"
#include "assets.h"

#define RENDER_W  240
#define RENDER_H  320

/* The playfield's width in pixels, the side panel starts right of it. */
#define RENDER_FIELD_W  (TETRIS_COLS * (int)ASSET_TILE)

/* What the frame shows besides the game itself. */
typedef struct {
    uint32_t best_score;     /* all-time high score, not counting this game */
    uint32_t best_lines;     /* not shown for a human */
    uint32_t games;          /* games finished, ever */
    uint8_t  game_over;      /* show the game over screen */
    uint8_t  new_record;     /* ... and that this game set the high score */
    uint8_t  human;          /* a person plays, best_* are the human's */
    uint8_t  hint;           /* show where to touch */
    uint32_t anim_ms;        /* clock for blinking, 0 = start of game over */
} render_info_t;

/* What one framebuffer shows, as far as the renderer is concerned. Every
 * region is drawn again when the content it should show differs from the
 * content recorded here. */
typedef struct {
    uint8_t  valid;          /* 0: nothing known, the next frame draws all */
    int8_t   next;           /* preview piece */
    uint16_t cell[TETRIS_ROWS - TETRIS_HIDDEN_ROWS][TETRIS_COLS];
    uint32_t title[5];       /* see title_key() in render.c */
    uint32_t value[5];       /* score, lines, level, seconds, game number */
} render_cache_t;

/* Forget what the buffer shows, the next render_frame() draws all of it. */
void render_invalidate(render_cache_t *cache);

/* Draws the frame into fb, which cache describes. cache NULL draws every
 * pixel. */
void render_frame(uint32_t *fb, render_cache_t *cache, const tetris_t *t,
                  const render_info_t *info);

#endif /* TETRIS_RENDER_H */
