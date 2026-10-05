#ifndef TETRIS_H
#define TETRIS_H
/*
 * Tetromino game core, a port of the browser version (tetromino.js, Jan
 * Zwiener, 2011 - 2016, https://zwiener.org/tetromino.html) to C.
 *
 * Platform independent: no I/O, no heap, no clock. The caller runs
 * tetris_tick() at a fixed rate (TETRIS_TICK_MS, the setInterval() period
 * of the browser version) and draws the state in between. Everything the
 * game knows lives in tetris_t.
 *
 * Coordinates and conventions are the JavaScript ones: the map has
 * TETRIS_ROWS rows of TETRIS_COLS cells, row 0 at the top. The two top rows
 * are the spawn area and are not drawn. A cell holds 0 for empty, 1..7 for
 * the colour of the piece that left it there (I J L O S T Z), and the
 * negative colour while its line flashes before being removed.
 *
 * Input is one tetris_keys_t per tick, set either by the built-in AI
 * (tetris_t.ai_active, the default) or by the caller for a human player.
 */
#include <stdint.h>

#define TETRIS_COLS        10
#define TETRIS_ROWS        22
#define TETRIS_HIDDEN_ROWS 2        /* spawn rows, not drawn            */
#define TETRIS_TICK_MS     50U      /* game step, as in the browser     */
#define TETRIS_MAX_LEVEL   20
#define TETRIS_PIECES      7

typedef enum {
    TETRIS_FIRSTBLOCK = 0,  /* a new piece just appeared on top       */
    TETRIS_NORMAL,          /* piece falling                          */
    TETRIS_CLEARLINES,      /* complete lines flash and are removed   */
    TETRIS_GAMEOVER         /* no room for the new piece              */
} tetris_state_t;

/* One tick worth of input. All of them are edge-free levels for exactly
 * the tick they are set in, except fast_down, which stays on until the
 * piece locks (as the held down arrow key does in the browser). */
typedef struct {
    uint8_t left;
    uint8_t right;
    uint8_t rot_left;       /* rotation index + 1 */
    uint8_t rot_right;      /* rotation index - 1 */
    uint8_t fast_down;      /* soft drop, one row per tick */
    uint8_t warp_down;      /* hard drop */
} tetris_keys_t;

/* Candidates of the AI's search the third ply is still to look at, see
 * ai_refine() in tetris.c. */
#ifndef TETRIS_AI_BEAM
#define TETRIS_AI_BEAM 10
#endif

typedef struct {
    int32_t  score;                 /* two ply: pieces and the field left */
    int32_t  pieces;                /* the part of it from the two pieces */
    int8_t   x, rot;                /* placement of the falling piece */
    uint16_t board[TETRIS_ROWS];    /* field after both, a row per word */
} tetris_ai_cand_t;

typedef struct {
    int8_t   map[TETRIS_ROWS][TETRIS_COLS];

    uint8_t  state;         /* tetris_state_t */
    uint8_t  block;         /* falling piece, 0..6 */
    uint8_t  rot;           /* its rotation */
    uint8_t  next;          /* preview piece */
    int8_t   x;             /* column of the piece's bounding box */
    int8_t   y;             /* row of the piece's bounding box */

    uint8_t  frame;         /* gravity counter, piece drops at 0 */
    uint8_t  speed;         /* ticks per gravity step */
    int8_t   clearflashcount;

    uint64_t score;         /* 64 bit: a perfect AI passes 2^32 within weeks */
    uint32_t lines;
    uint32_t level;         /* 1..TETRIS_MAX_LEVEL */
    uint32_t linestat[4];   /* single, double, triple, tetromino clears */
    uint32_t warpcount;
    uint32_t ticks;         /* ticks since the game started */
    uint32_t levelup_ticks; /* > 0 while the level up notice is shown */

    tetris_keys_t keys;

    /* 7-bag randomizer: every piece once per bag of seven. */
    uint8_t  bag[TETRIS_PIECES];
    uint8_t  bagindex;
    uint32_t rng;

    /* AI */
    uint8_t  ai_active;
    uint8_t  ai_superfast;  /* hard drop once in place (the "i" key) */
    uint16_t ai_blunder;    /* per mille of pieces placed at random */
    uint8_t  ai_state;
    int8_t   ai_x;          /* target placement of the falling piece */
    int8_t   ai_rot;
    uint8_t  ai_tetris_play;
    uint8_t  ai_nbeam;      /* candidates of this piece */
    uint8_t  ai_refined;    /* ... of them looked at three pieces deep */
    int32_t  ai_best;       /* three ply score of the target */
    tetris_ai_cand_t ai_beam[TETRIS_AI_BEAM];
} tetris_t;

/* Fresh game. seed only needs to differ between games, 0 is replaced by
 * a fixed non-zero value. The AI is switched on and plays its best, set
 * ai_blunder afterwards to make it fallible. */
void tetris_init(tetris_t *t, uint32_t seed);

/* One step of the game loop (gameLoop() in the browser version). Uses and
 * then clears t->keys, except fast_down, see tetris_keys_t. Does nothing
 * once the game is over. */
void tetris_tick(tetris_t *t);

/* Row at which the falling piece would land, -1 if it cannot move down at
 * all. */
int tetris_ghost_y(const tetris_t *t);

/* Cell (row, col) of the piece's bounding box in the given rotation: the
 * colour 1..7, or 0. Rotation counts wrap around, so any rot is valid. */
int tetris_piece_cell(int block, int rot, int row, int col);

/* Edge length of the bounding box of a piece (4 for I, 2 for O, else 3). */
int tetris_piece_size(int block);

#endif /* TETRIS_H */
