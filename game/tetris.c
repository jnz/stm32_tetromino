/*
 * See tetris.h. Function names follow tetromino.js, the browser version
 * at https://zwiener.org/tetromino.html, where there is a direct
 * counterpart, so the two can be read side by side.
 */
#include "tetris.h"
#include <string.h>

/* Scoring: 1 line = 40 points ... 4 lines = 1200 points, times the level. */
static const uint32_t k_linescore[4] = { 40U, 100U, 300U, 1200U };

/* How often complete lines flash per removed line (flashcount in JS). */
#define FLASH_COUNT 1

/* Ticks the "Level n!" notice stays up, 2 s as in the browser. */
#define LEVELUP_TICKS (2000U / TETRIS_TICK_MS)

/* Pieces, as blockmap[] in the JavaScript: per rotation a size x size
 * matrix, row major, holding the colour index or 0. */
static const uint8_t k_size[TETRIS_PIECES] = { 4, 3, 3, 2, 3, 3, 3 };
static const uint8_t k_rotations[TETRIS_PIECES] = { 4, 4, 4, 1, 4, 4, 4 };

static const uint8_t k_shape[TETRIS_PIECES][4][16] = {
    { /* I */
        { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 },
        { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 },
    },
    { /* J */
        { 2, 0, 0, 2, 2, 2, 0, 0, 0 },
        { 0, 2, 2, 0, 2, 0, 0, 2, 0 },
        { 0, 0, 0, 2, 2, 2, 0, 0, 2 },
        { 0, 2, 0, 0, 2, 0, 2, 2, 0 },
    },
    { /* L */
        { 0, 0, 3, 3, 3, 3, 0, 0, 0 },
        { 0, 3, 0, 0, 3, 0, 0, 3, 3 },
        { 0, 0, 0, 3, 3, 3, 3, 0, 0 },
        { 3, 3, 0, 0, 3, 0, 0, 3, 0 },
    },
    { /* O */
        { 4, 4, 4, 4 },
    },
    { /* S */
        { 0, 5, 5, 5, 5, 0, 0, 0, 0 },
        { 0, 5, 0, 0, 5, 5, 0, 0, 5 },
        { 0, 0, 0, 0, 5, 5, 5, 5, 0 },
        { 5, 0, 0, 5, 5, 0, 0, 5, 0 },
    },
    { /* T */
        { 0, 6, 0, 6, 6, 6, 0, 0, 0 },
        { 0, 6, 0, 0, 6, 6, 0, 6, 0 },
        { 0, 0, 0, 6, 6, 6, 0, 6, 0 },
        { 0, 6, 0, 6, 6, 0, 0, 6, 0 },
    },
    { /* Z */
        { 7, 7, 0, 0, 7, 7, 0, 0, 0 },
        { 0, 0, 7, 0, 7, 7, 0, 7, 0 },
        { 0, 0, 0, 7, 7, 0, 0, 7, 7 },
        { 0, 7, 0, 7, 7, 0, 7, 0, 0 },
    },
};

/* Wall kicks, [from][to] -> five (x, y) offsets tried in order. Only the
 * transitions to the neighbouring rotation are used. y is added to the row
 * as it is in the JavaScript. */
static const int8_t k_wallkick[4][4][10] = {
    [0][1] = { 0, 0, -1, 0, -1,  1, 0, -2, -1, -2 },
    [0][3] = { 0, 0,  1, 0,  1,  1, 0, -2,  1, -2 },
    [1][0] = { 0, 0,  1, 0,  1, -1, 0,  2,  1,  2 },
    [1][2] = { 0, 0,  1, 0,  1, -1, 0,  2,  1,  2 },
    [2][1] = { 0, 0, -1, 0, -1,  1, 0, -2, -1, -2 },
    [2][3] = { 0, 0,  1, 0,  1,  1, 0, -2,  1, -2 },
    [3][2] = { 0, 0, -1, 0, -1, -1, 0,  2, -1,  2 },
    [3][0] = { 0, 0, -1, 0, -1, -1, 0,  2, -1,  2 },
};

static const int8_t k_wallkick_i[4][4][10] = {
    [0][1] = { 0, 0, -2, 0,  1, 0, -2, -1,  1,  2 },
    [0][3] = { 0, 0, -1, 0,  2, 0, -1,  2,  2, -1 },
    [1][0] = { 0, 0,  2, 0, -1, 0,  2,  1, -1, -2 },
    [1][2] = { 0, 0, -1, 0,  2, 0, -1,  2,  2, -1 },
    [2][1] = { 0, 0,  1, 0, -2, 0,  1, -2, -2,  1 },
    [2][3] = { 0, 0,  2, 0, -1, 0,  2,  1, -1, -2 },
    [3][2] = { 0, 0, -2, 0,  1, 0, -2, -1,  1,  2 },
    [3][0] = { 0, 0,  1, 0, -2, 0,  1, -2, -2,  1 },
};

typedef int8_t map_t[TETRIS_ROWS][TETRIS_COLS];

/* Scores saturate instead of wrapping. The perfect AI makes about a
 * million points an hour, so an ornament left running for months would
 * otherwise come round to zero. */
static void add_score(tetris_t *t, uint32_t points)
{
    t->score = (points > 0xFFFFFFFFU - t->score) ? 0xFFFFFFFFU : t->score + points;
}

/* --------------------------------------------------------------------- */
/* Pieces and the map                                                    */
/* --------------------------------------------------------------------- */

int tetris_piece_size(int block)
{
    return k_size[block];
}

int tetris_piece_cell(int block, int rot, int row, int col)
{
    const int n = k_size[block];

    rot %= k_rotations[block];
    if (rot < 0)
        rot += k_rotations[block];
    return k_shape[block][rot][row * n + col];
}

static int check_collision(const map_t map, int block, int rot, int x, int y)
{
    const int n = k_size[block];
    const uint8_t *bm = k_shape[block][rot];
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (bm[i * n + j] == 0U)
                continue;
            if (x + j >= TETRIS_COLS || x + j < 0 ||
                y + i >= TETRIS_ROWS || y + i < 0 ||
                map[y + i][x + j] != 0)
                return 1;
        }
    }
    return 0;
}

static void stamp_block(map_t map, int block, int rot, int x, int y)
{
    const int n = k_size[block];
    const uint8_t *bm = k_shape[block][rot];
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            const int row = y + i;
            const int col = x + j;

            if (bm[i * n + j] == 0U)
                continue;
            if (row >= 0 && row < TETRIS_ROWS && col >= 0 && col < TETRIS_COLS)
                map[row][col] = (int8_t)bm[i * n + j];
        }
    }
}

/* getGhostPos() */
static int ghost_pos(const map_t map, int block, int rot, int x, int y)
{
    int gy;

    for (gy = y + 1; gy < TETRIS_ROWS; gy++) {
        if (check_collision(map, block, rot, x, gy))
            return gy - 1;
    }
    return -1;
}

int tetris_ghost_y(const tetris_t *t)
{
    return ghost_pos((const int8_t (*)[TETRIS_COLS])t->map, t->block, t->rot,
                     t->x, t->y);
}

static int row_full(const map_t map, int row)
{
    int j;

    for (j = 0; j < TETRIS_COLS; j++) {
        if (map[row][j] == 0)
            return 0;
    }
    return 1;
}

/* flashCompleteLine(): flips the sign of every complete line, which is
 * what makes them flash, and returns how many there are. */
static int flash_complete_lines(map_t map)
{
    int count = 0;
    int i, j;

    for (i = TETRIS_ROWS - 1; i >= 0; i--) {
        if (!row_full(map, i))
            continue;
        count++;
        for (j = 0; j < TETRIS_COLS; j++)
            map[i][j] = (int8_t)-map[i][j];
    }
    return count;
}

/* clearBottomLine(): removes the lowest complete line. Row 0 is emptied,
 * the original left it as it was (a copy of itself in row 1), which only
 * made a difference if something was ever stamped into it. */
static void clear_bottom_line(map_t map)
{
    int i;

    for (i = TETRIS_ROWS - 1; i >= 1; i--) {
        if (!row_full(map, i))
            continue;
        memmove(&map[1][0], &map[0][0], (size_t)i * TETRIS_COLS);
        memset(&map[0][0], 0, TETRIS_COLS);
        return;
    }
}

/* --------------------------------------------------------------------- */
/* Randomizer                                                            */
/* --------------------------------------------------------------------- */

static uint32_t rnd(tetris_t *t)
{
    /* xorshift32 */
    uint32_t x = t->rng;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    t->rng = x;
    return x;
}

/* selectRandomBlock(): 7-bag, shuffled with Fisher-Yates like shuffle()
 * in the browser version. */
static uint8_t select_random_block(tetris_t *t)
{
    if (t->bagindex >= TETRIS_PIECES) {
        int m;

        for (m = 0; m < TETRIS_PIECES; m++)
            t->bag[m] = (uint8_t)m;
        for (m = TETRIS_PIECES; m > 1; m--) {
            const uint32_t i = rnd(t) % (uint32_t)m;
            const uint8_t tmp = t->bag[m - 1];

            t->bag[m - 1] = t->bag[i];
            t->bag[i] = tmp;
        }
        t->bagindex = 0;
    }
    return t->bag[t->bagindex++];
}

/* --------------------------------------------------------------------- */
/* AI                                                                    */
/* --------------------------------------------------------------------- */

enum { AI_THINK = 0, AI_MOVE, AI_IDLE };

/* Weights from the browser version, which took them from
 * https://codemyroad.wordpress.com/2013/04/14/tetris-ai-the-near-perfect-player/
 * The line weight is the article's 0.760666, the browser version had
 * 0.760066 by a typo.
 *
 * In millionths, so the evaluation is exact integer arithmetic. With
 * floats, two placements of equal value were decided by the last bit of
 * rounding, which differs between compilers: the host simulator and the
 * board could pick different moves from the same position. Integers tie
 * exactly, and a tie goes to the placement found first, everywhere.
 *
 * Range: at most 220 cells of height or holes and 4 lines, so a score
 * stays within +-3e8, well inside int32_t. */
#define AI_LINE_W    760666
#define AI_HEIGHT_W  510066
#define AI_ROUGH_W   356630
#define AI_VALLEY_W  184483

/* Column range the AI tries for a piece's bounding box. The browser
 * version used -1 .. COLS-3, which cannot reach the right wall with
 * pieces whose right box column is empty (J, L, T in the rotation that
 * occupies the box's left two columns). Wider than needed is harmless:
 * placements that leave the field fail the collision check. */
#ifndef TETRIS_AI_X_MIN
#define TETRIS_AI_X_MIN  (-2)
#endif
#ifndef TETRIS_AI_X_MAX
#define TETRIS_AI_X_MAX  (TETRIS_COLS - 1)
#endif

static int row_complete(const map_t map, int row)
{
    int j;

    for (j = 0; j < TETRIS_COLS; j++) {
        if (map[row][j] <= 0)
            return 0;
    }
    return 1;
}

/* ai_removeCompletedLines(), without its slip: the browser version moved
 * on to the next row after a removal, so of two adjacent complete lines it
 * counted and removed only one, and the AI saw a tetromino clear as a
 * single. Here the row that moved in is looked at again.
 *
 * Only rows lo..hi are looked at. The AI stamps a piece into a map that
 * has no complete line, so only the rows the piece covers can complete. */
static int ai_remove_completed_lines(map_t map, int lo, int hi)
{
    int count = 0;
    int i;

    if (lo < 1)
        lo = 1;
    if (hi > TETRIS_ROWS - 1)
        hi = TETRIS_ROWS - 1;
    for (i = hi; i >= lo; i--) {
        if (!row_complete(map, i))
            continue;
        count++;
        memmove(&map[1][0], &map[0][0], (size_t)i * TETRIS_COLS);
        /* Emptying row 0 also bounds the loop below: every removal takes
         * cells off the map, so the re-check cannot go on forever. */
        memset(&map[0][0], 0, TETRIS_COLS);
        i++;    /* the row that moved in has not been looked at yet */
        lo++;   /* and the rows still to look at moved down with it */
    }
    return count;
}

/* ai_penaltyScore(). Rows above from are known to be empty. */
static int32_t ai_penalty(const map_t map, int from)
{
    int rough = 0;
    int height = 0;
    int valley = 0;
    int prev = 0;
    int i, j;

    for (j = 0; j < TETRIS_COLS; j++) {
        int col_height = 0;
        int roof = 0;

        for (i = from; i < TETRIS_ROWS; i++) {
            if (roof) {
                if (map[i][j] == 0)
                    rough++;     /* hole below the column's top */
            } else if (map[i][j] != 0) {
                roof = 1;
                col_height = TETRIS_ROWS - i;
            }
        }
        height += col_height;
        if (j > 0)
            valley += (col_height > prev) ? col_height - prev : prev - col_height;
        prev = col_height;
    }
    return AI_ROUGH_W * rough + AI_HEIGHT_W * height + AI_VALLEY_W * valley;
}

/* Topmost occupied row of every column, TETRIS_ROWS for an empty one. */
static void column_tops(const map_t map, int8_t top[TETRIS_COLS])
{
    int i, j;

    for (j = 0; j < TETRIS_COLS; j++) {
        top[j] = TETRIS_ROWS;
        for (i = 0; i < TETRIS_ROWS; i++) {
            if (map[i][j] != 0) {
                top[j] = (int8_t)i;
                break;
            }
        }
    }
}

/* Row a piece dropped straight down from row 0 comes to rest at: in each
 * of its columns the lowest cell lands on that column's top. Same result
 * as ghost_pos() from row 0, without testing every row on the way down,
 * which made the AI's 2-ply search take longer than one game tick on the
 * target. Only valid when the piece fits at row 0. */
static int landing_row(const int8_t top[TETRIS_COLS], int block, int rot, int x)
{
    const int n = k_size[block];
    const uint8_t *bm = k_shape[block][rot];
    int y = TETRIS_ROWS;
    int i, j;

    for (j = 0; j < n; j++) {
        for (i = n - 1; i >= 0; i--) {
            if (bm[i * n + j] != 0U) {
                const int land = top[x + j] - 1 - i;

                if (land < y)
                    y = land;
                break;
            }
        }
    }
    return y;
}

/* Drops block straight down from row 0 at (x, rot) into a copy of map and
 * removes the lines it completes. top holds column_tops() of map. Returns
 * the number of lines, -1 if the piece does not fit at the top at all.
 * *from gets a row above which out is empty, for ai_penalty(). */
static int ai_drop(const map_t map, const int8_t top[TETRIS_COLS], int block,
                   int x, int rot, map_t out, int *from)
{
    int y, j;

    if (check_collision(map, block, rot, x, 0))
        return -1;
    y = landing_row(top, block, rot, x);
    if (y < 0)
        y = 0;
    memcpy(out, map, sizeof(map_t));
    stamp_block(out, block, rot, x, y);
    *from = y;
    for (j = 0; j < TETRIS_COLS; j++) {
        if (top[j] < *from)
            *from = top[j];
    }
    return ai_remove_completed_lines(out, y, y + k_size[block] - 1);
}

/* Below any real score, and far enough from INT32_MIN that adding the
 * first ply's line bonus cannot overflow. */
#define AI_NO_MOVE (-(INT32_C(1) << 30))

/* Second ply of ai_move(): the best score any placement of block reaches
 * on map, counting the lines it clears and the penalty of what is left. */
static int32_t ai_best_leaf(const map_t map, int block)
{
    map_t tmp;
    int8_t top[TETRIS_COLS];
    int32_t best = AI_NO_MOVE;
    int x, r;

    column_tops(map, top);
    for (x = TETRIS_AI_X_MIN; x <= TETRIS_AI_X_MAX; x++) {
        for (r = 0; r < k_rotations[block]; r++) {
            int from;
            const int lines = ai_drop(map, top, block, x, r, tmp, &from);
            int32_t score;

            if (lines < 0)
                continue;
            score = AI_LINE_W * lines - ai_penalty(tmp, from);
            if (score > best)
                best = score;
        }
    }
    return best;
}

/* A blunder: any placement the piece fits in, picked uniformly. */
static void ai_blunder(tetris_t *t)
{
    int8_t xs[(TETRIS_AI_X_MAX - TETRIS_AI_X_MIN + 1) * 4];
    int8_t rs[(TETRIS_AI_X_MAX - TETRIS_AI_X_MIN + 1) * 4];
    int n = 0;
    int x, r;

    for (x = TETRIS_AI_X_MIN; x <= TETRIS_AI_X_MAX; x++) {
        for (r = 0; r < k_rotations[t->block]; r++) {
            if (check_collision((const int8_t (*)[TETRIS_COLS])t->map,
                                t->block, r, x, 0))
                continue;
            xs[n] = (int8_t)x;
            rs[n] = (int8_t)r;
            n++;
        }
    }
    if (n > 0) {
        const uint32_t k = rnd(t) % (uint32_t)n;

        t->ai_x = xs[k];
        t->ai_rot = rs[k];
    }
}

/* ai_move(): two ply search over the current and the preview piece. The
 * browser version recursed with a leaf marker, here the two levels are
 * spelled out so the depth is fixed.
 *
 * Played straight, this AI does not lose: in host runs it was still going
 * after hours of game time at level 20. ai_blunder makes it drop a piece
 * at random now and then, which is what lets a game end at all. */
static void ai_move(tetris_t *t)
{
    map_t tmp;
    int8_t top[TETRIS_COLS];
    int32_t best = AI_NO_MOVE;
    int x, r;

    t->ai_x = t->x;
    t->ai_rot = (int8_t)t->rot;
    if (t->ai_blunder > 0U && (rnd(t) % 1000U) < t->ai_blunder) {
        ai_blunder(t);
        return;
    }
    column_tops((const int8_t (*)[TETRIS_COLS])t->map, top);
    for (x = TETRIS_AI_X_MIN; x <= TETRIS_AI_X_MAX; x++) {
        for (r = 0; r < k_rotations[t->block]; r++) {
            int from;
            const int lines = ai_drop((const int8_t (*)[TETRIS_COLS])t->map,
                                      top, t->block, x, r, tmp, &from);
            int32_t score;

            if (lines < 0)
                continue;
            score = AI_LINE_W * lines + ai_best_leaf(tmp, t->next);
            if (score > best) {
                best = score;
                t->ai_x = (int8_t)x;
                t->ai_rot = (int8_t)r;
            }
        }
    }
}

/* ai_run(): turns the chosen placement into key presses, one step per
 * tick, so the AI plays by the same rules as a human. */
static void ai_run(tetris_t *t)
{
    if (t->state == TETRIS_FIRSTBLOCK)
        t->ai_state = AI_THINK;

    if (t->ai_state == AI_THINK) {
        ai_move(t);
        t->ai_state = AI_MOVE;
    } else if (t->ai_state == AI_MOVE) {
        const int err_x = t->ai_x - t->x;
        const int err_rot = t->ai_rot - (int)t->rot;

        if (err_x > 0)
            t->keys.right = 1;
        if (err_x < 0)
            t->keys.left = 1;
        if (err_rot > 0)
            t->keys.rot_left = 1;
        if (err_rot < 0)
            t->keys.rot_right = 1;
        if (err_x == 0 && err_rot == 0) {
            if (t->ai_superfast)
                t->keys.warp_down = 1;
            t->ai_state = AI_IDLE;
        }
    } else {
        t->keys.fast_down = 1;
    }
}

/* --------------------------------------------------------------------- */
/* Game loop                                                             */
/* --------------------------------------------------------------------- */

static void spawn(tetris_t *t)
{
    t->block = t->next;
    t->rot = 0;
    t->x = TETRIS_COLS / 2 - 2;
    t->y = 0;
    t->next = select_random_block(t);
}

void tetris_init(tetris_t *t, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    t->rng = (seed != 0U) ? seed : 0x2545F491U;
    t->bagindex = TETRIS_PIECES;     /* empty bag, refilled on first draw */
    t->speed = 20;
    t->level = 1;
    t->next = select_random_block(t);
    spawn(t);
    t->state = TETRIS_FIRSTBLOCK;
    t->ai_active = 1;
    t->ai_state = AI_THINK;
}

/* checkRotation(): tries the kicks for the transition from -> t->rot and
 * moves the piece by the first one that fits. 0 = rotated, 1 = blocked. */
static int check_rotation(tetris_t *t, int from)
{
    const int8_t *kick = (t->block == 0) ? k_wallkick_i[from][t->rot]
                                         : k_wallkick[from][t->rot];
    int i;

    for (i = 0; i < 10; i += 2) {
        if (!check_collision((const int8_t (*)[TETRIS_COLS])t->map, t->block,
                             t->rot, t->x + kick[i], t->y + kick[i + 1])) {
            t->x = (int8_t)(t->x + kick[i]);
            t->y = (int8_t)(t->y + kick[i + 1]);
            return 0;
        }
    }
    return 1;
}

/* gameStateNormal() */
static void state_normal(tetris_t *t)
{
    const int rotsave = t->rot;
    const int nrot = k_rotations[t->block];
    int xshift = 0;
    int yoffset = 0;

    if (t->ai_active)
        ai_run(t);

    /* sideways */
    if (t->keys.left)
        xshift -= 1;
    if (t->keys.right)
        xshift += 1;
    if (xshift != 0 &&
        !check_collision((const int8_t (*)[TETRIS_COLS])t->map, t->block,
                         t->rot, t->x + xshift, t->y))
        t->x = (int8_t)(t->x + xshift);

    /* rotation, O does not rotate */
    if (t->keys.rot_left)
        t->rot = (uint8_t)(t->rot + 1);
    if (t->keys.rot_right)
        t->rot = (uint8_t)(t->rot + nrot - 1);
    t->rot = (uint8_t)(t->rot % nrot);
    if (t->rot != rotsave && t->block != 3) {
        if (check_rotation(t, rotsave))
            t->rot = (uint8_t)rotsave;
    }

    /* hard drop */
    if (t->keys.warp_down) {
        const int gy = tetris_ghost_y(t);

        if (gy != t->y && gy > 0) {
            add_score(t, (uint32_t)(gy - t->y));
            t->warpcount++;
            t->y = (int8_t)gy;
            t->frame = 0;
        }
    }

    /* gravity */
    if (t->frame == 0 || t->keys.fast_down)
        yoffset = 1;

    if (!check_collision((const int8_t (*)[TETRIS_COLS])t->map, t->block,
                         t->rot, t->x, t->y + yoffset)) {
        t->y = (int8_t)(t->y + yoffset);
        t->state = TETRIS_NORMAL;
        return;
    }

    /* The piece cannot move down. Right after spawning that means the
     * stack has reached the top. */
    if (t->state == TETRIS_FIRSTBLOCK && t->y < 1) {
        t->state = TETRIS_GAMEOVER;
        return;
    }

    stamp_block(t->map, t->block, t->rot, t->x, t->y);

    {
        const int curlines = flash_complete_lines(t->map);

        if (curlines > 0) {
            uint32_t newlevel;

            t->linestat[curlines - 1]++;
            t->lines += (uint32_t)curlines;
            add_score(t, t->level * k_linescore[curlines - 1]);
            newlevel = t->lines / 10U + 1U;
            if (newlevel > TETRIS_MAX_LEVEL)
                newlevel = TETRIS_MAX_LEVEL;
            if (newlevel > t->level)
                t->levelup_ticks = LEVELUP_TICKS;
            t->level = newlevel;
            t->speed = (uint8_t)(21U - t->level);
            t->state = TETRIS_CLEARLINES;
            t->clearflashcount = (int8_t)(curlines * FLASH_COUNT);
            t->frame = 0;
        } else {
            t->state = TETRIS_FIRSTBLOCK;
        }
    }

    spawn(t);
    t->keys.fast_down = 0;
}

/* gameStateClearLines(): the lines flash, then go one per step from the
 * bottom up. */
static void state_clear_lines(tetris_t *t)
{
    const int lines = flash_complete_lines(t->map);

    t->clearflashcount--;
    if (t->clearflashcount < 0) {
        clear_bottom_line(t->map);
        t->clearflashcount = (int8_t)(lines * FLASH_COUNT);
        if (lines <= 1)
            t->state = TETRIS_FIRSTBLOCK;
    }
}

void tetris_tick(tetris_t *t)
{
    if (t->state == TETRIS_GAMEOVER)
        return;

    t->ticks++;
    if (t->levelup_ticks > 0U)
        t->levelup_ticks--;

    t->frame++;
    if (t->frame > t->speed)
        t->frame = 0;

    if (t->state == TETRIS_FIRSTBLOCK || t->state == TETRIS_NORMAL)
        state_normal(t);
    else if (t->state == TETRIS_CLEARLINES)
        state_clear_lines(t);

    /* Single tick keys. fast_down is a held key and is cleared when the
     * piece locks. */
    t->keys.left = 0;
    t->keys.right = 0;
    t->keys.rot_left = 0;
    t->keys.rot_right = 0;
    t->keys.warp_down = 0;
}
