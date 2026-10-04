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
    t->score = (points > UINT64_MAX - t->score) ? UINT64_MAX : t->score + points;
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

/* The kicks for a rotation from -> to of a piece at (*x, *y): moves it by
 * the first one that fits. 0 = rotated, 1 = blocked, *x and *y unchanged. */
static int kick_rotation(const map_t map, int block, int from, int to,
                         int *x, int *y)
{
    const int8_t *kick = (block == 0) ? k_wallkick_i[from][to]
                                      : k_wallkick[from][to];
    int i;

    for (i = 0; i < 10; i += 2) {
        if (!check_collision(map, block, to, *x + kick[i], *y + kick[i + 1])) {
            *x += kick[i];
            *y += kick[i + 1];
            return 0;
        }
    }
    return 1;
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

/* Evaluation: Pierre Dellacherie's features with the weights El-Tetris
 * (Islam El-Ashi, 2011) found for them by a genetic algorithm. Per
 * placement:
 *   landing    height of the piece's middle above the floor where it
 *              came to rest
 *   eroded     lines cleared x cells of the piece in them (a reward)
 *   row trans  filled/empty changes along each row, the walls filled
 *   col trans  the same down each column, the floor filled
 *   holes      empty cells below a column's top
 *   wells      per run of empty cells with both neighbours filled (or the
 *              wall), 1 + 2 + .. + its depth
 * El-Tetris itself rewards the plain number of lines instead of the
 * eroded cells, and counts every empty cell below a well cell into the
 * well. Neither made a measurable difference in host runs of either AI.
 *
 * The browser version had four simpler features (height, holes,
 * bumpiness, lines). Those let the stack fill up with holes it never dug
 * out of again, once the tetromino play below kept it high.
 *
 * In thousandths, so the evaluation is exact integer arithmetic. With
 * floats, two placements of equal value were decided by the last bit of
 * rounding, which differs between compilers: the host simulator and the
 * board could pick different moves from the same position. Integers tie
 * exactly, and a tie goes to the placement found first, everywhere.
 *
 * Range: every feature stays below 3000 on a 10x22 field, a score within
 * +-1e8, well inside int32_t. */
#define AI_LANDING_W   4500
#define AI_ERODED_W    3418
#define AI_ROWTRANS_W  3218
#define AI_COLTRANS_W  9349
#define AI_HOLES_W     7899
#define AI_WELLS_W     3386

/* Tetromino play. While the stack is low, the AI keeps the rightmost
 * column empty as a well and fills the rest, so that an I dropped into the
 * well clears four lines at once: 1200 points per level instead of 4 x 40
 * for the same lines as singles. Then a clear of four is worth
 * AI_TETRIS_W, one of fewer lines costs per line, every cell in the well
 * column costs, and the well column is left out of the row transitions,
 * the column transitions and the wells. A gap in it under a cell that
 * covers the well still counts as a hole: without that, covering the well
 * cost only the one cell, and the AI did it every 30 pieces or so, then
 * had to dig the well out again with singles. Once the highest of the
 * other columns reaches
 * AI_SAFE_HEIGHT, it plays for survival with the plain features. */
#define AI_WELL_COL     (TETRIS_COLS - 1)
#ifndef AI_SAFE_HEIGHT
#define AI_SAFE_HEIGHT  10
#endif
#ifndef AI_TETRIS_W
#define AI_TETRIS_W     50000
#endif
#ifndef AI_FEW_LINES_W
#define AI_FEW_LINES_W  5000    /* per line of a 1..3 line clear */
#endif
#ifndef AI_WELL_W
#define AI_WELL_W       5000    /* per cell in the well column   */
#endif

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

/* The AI works on a bitboard: one row per uint16_t, bit j = column j.
 * Copying a field, finding complete rows and every feature below take a
 * few operations per row instead of one per cell. With the cell by cell
 * version, the two ply search took longer than a game tick on the target
 * (78 ms at 90 MHz). */
typedef uint16_t ai_board_t[TETRIS_ROWS];
#define AI_FULL ((uint32_t)((1U << TETRIS_COLS) - 1U))

static int popcount16(uint32_t v)
{
    v = v - ((v >> 1) & 0x5555U);
    v = (v & 0x3333U) + ((v >> 2) & 0x3333U);
    v = (v + (v >> 4)) & 0x0F0FU;
    return (int)((v + (v >> 8)) & 0x1FU);
}

static void ai_board(const map_t map, ai_board_t b)
{
    int i, j;

    for (i = 0; i < TETRIS_ROWS; i++) {
        uint32_t r = 0U;

        for (j = 0; j < TETRIS_COLS; j++) {
            if (map[i][j] != 0)
                r |= 1U << j;
        }
        b[i] = (uint16_t)r;
    }
}

/* Topmost occupied row of every column, TETRIS_ROWS for an empty one.
 * Returns the topmost occupied row of the whole field. */
static int column_tops(const ai_board_t b, int8_t top[TETRIS_COLS])
{
    uint32_t seen = 0U;
    int first = TETRIS_ROWS;
    int i, j;

    for (j = 0; j < TETRIS_COLS; j++)
        top[j] = TETRIS_ROWS;
    for (i = 0; i < TETRIS_ROWS && seen != AI_FULL; i++) {
        uint32_t fresh = b[i] & ~seen;

        if (b[i] != 0U && first == TETRIS_ROWS)
            first = i;
        while (fresh != 0U) {
            top[__builtin_ctz(fresh)] = (int8_t)i;
            fresh &= fresh - 1U;
        }
        seen |= b[i];
    }
    return first;
}

/* Row a piece dropped straight down from row 0 comes to rest at: in each
 * of its columns the lowest cell lands on that column's top. Same result
 * as ghost_pos() from row 0, without testing every row on the way down.
 * Only valid when the piece fits at row 0. */
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

/* A placement as ai_drop() leaves it. */
typedef struct {
    int lines;      /* cleared */
    int eroded;     /* lines x cells of the piece in them */
    int landing2;   /* twice the landing height */
    int from;       /* rows above this one are empty */
} ai_drop_t;

/* The rows of a piece's box at column x as row masks. -1 if a cell lies
 * outside the walls. */
static int piece_masks(int block, int rot, int x, uint32_t m[4])
{
    const int n = k_size[block];
    const uint8_t *bm = k_shape[block][rot];
    int i, j;

    for (i = 0; i < n; i++) {
        m[i] = 0U;
        for (j = 0; j < n; j++) {
            if (bm[i * n + j] == 0U)
                continue;
            if (x + j < 0 || x + j >= TETRIS_COLS)
                return -1;
            m[i] |= 1U << (x + j);
        }
    }
    return 0;
}

/* Drops block straight down from row 0 at (x, rot) into a copy of b and
 * removes the lines it completes. top and first are column_tops() of b.
 * Returns 0, or -1 if the piece does not fit at the top at all.
 *
 * The browser version's ai_removeCompletedLines() moved on to the next
 * row after a removal, so of two adjacent complete lines it counted and
 * removed only one, and the AI saw a tetromino clear as a single. Here
 * every complete row goes. Only the piece's rows can be complete: the AI
 * stamps pieces into fields without complete lines. */
static int ai_drop(const ai_board_t b, const int8_t top[TETRIS_COLS],
                   int first, int block, int x, int rot, ai_board_t out,
                   ai_drop_t *d)
{
    const int n = k_size[block];
    uint32_t m[4];
    int lo = n, hi = -1;
    int cells = 0;
    int y, i;

    if (piece_masks(block, rot, x, m) < 0)
        return -1;
    for (i = 0; i < n; i++) {
        if ((m[i] & b[i]) != 0U)
            return -1;
    }
    y = landing_row(top, block, rot, x);
    if (y < 0)
        y = 0;
    memcpy(out, b, sizeof(ai_board_t));

    /* The piece's rows, and its cells in the rows it completes. */
    for (i = 0; i < n; i++) {
        if (m[i] == 0U)
            continue;
        if (i < lo)
            lo = i;
        hi = i;
        out[y + i] = (uint16_t)(out[y + i] | m[i]);
        if (out[y + i] == AI_FULL)
            cells += popcount16(m[i]);
    }
    d->landing2 = 2 * (TETRIS_ROWS - 1 - (y + hi)) + (hi - lo);
    d->from = (y < first) ? y : first;

    d->lines = 0;
    if (cells > 0) {
        /* Every row that is not complete moves down over the complete
         * ones, the top fills up with empty rows. */
        int k = y + hi;

        for (i = y + hi; i >= 0; i--) {
            if (out[i] != AI_FULL)
                out[k--] = out[i];
        }
        d->lines = k + 1;
        for (; k >= 0; k--)
            out[k] = 0U;
    }
    d->eroded = d->lines * cells;
    return 0;
}

/* The part of the score that belongs to the piece itself. */
static int32_t ai_piece_value(const ai_drop_t *d, int tetris_play)
{
    const int32_t v = -AI_LANDING_W * d->landing2 / 2;

    if (!tetris_play || d->lines == 0)
        return v + AI_ERODED_W * d->eroded;
    return v + ((d->lines == 4) ? AI_TETRIS_W : -AI_FEW_LINES_W * d->lines);
}

/* The part that belongs to the field left behind. Rows above from are
 * known to be empty: each has the two row transitions at the walls and
 * nothing else. With tetris_play, the well column as described at
 * AI_WELL_COL: filled for the row transitions, left out of the column
 * transitions and the wells (its neighbour does not count as a well
 * either), but holes under a covered well count. */
static int32_t ai_field_value(const ai_board_t b, int from, int tetris_play)
{
    const uint32_t wellbit = tetris_play ? (1U << AI_WELL_COL) : 0U;
    const uint32_t cols = AI_FULL & ~wellbit;
    const uint32_t well_cols = cols & ~(wellbit >> 1) & ~(wellbit << 1);
    uint32_t prev = 0U, roof = 0U, wprev = 0U;
    int8_t depth[TETRIS_COLS];
    int rowtrans = 2 * from;
    int coltrans = 0;
    int holes = 0;
    int wells = 0;
    int in_well = 0;
    int i;

    for (i = from; i < TETRIS_ROWS; i++) {
        const uint32_t r = b[i];
        /* The row with a wall bit on either side, bits 0..COLS+1. */
        const uint32_t v = ((r | wellbit) << 1) | 1U | (1U << (TETRIS_COLS + 1));
        uint32_t w;

        rowtrans += popcount16((v ^ (v >> 1)) & ((1U << (TETRIS_COLS + 1)) - 1U));
        coltrans += popcount16((r ^ prev) & cols);
        holes += popcount16(~r & roof & AI_FULL);
        in_well += (r & wellbit) != 0U;
        prev = r;
        roof |= r;

        /* Well cells: empty, filled (or the wall) left and right. Each
         * adds the depth of its run so far. */
        w = ~r & ((r << 1) | 1U) & ((r >> 1) | (1U << (TETRIS_COLS - 1))) &
            well_cols;
        {
            uint32_t k = w;

            while (k != 0U) {
                const int j = __builtin_ctz(k);

                depth[j] = (int8_t)(((wprev >> j) & 1U) ? depth[j] + 1 : 1);
                wells += depth[j];
                k &= k - 1U;
            }
        }
        wprev = w;
    }
    coltrans += popcount16(~prev & cols);   /* the floor is filled */

    return -AI_ROWTRANS_W * rowtrans - AI_COLTRANS_W * coltrans -
           AI_HOLES_W * holes - AI_WELLS_W * wells - AI_WELL_W * in_well;
}

/* Whether the key presses of ai_run() get the falling piece to (x, rot)
 * before it locks, at the game's current speed: they move it one column
 * and one rotation per tick while gravity goes on. ai_drop() lets the
 * piece fall straight from the top, which on a high stack at a high level
 * it does not get to do: it lands on the way over. Played out tick by
 * tick from the tick ai_move() runs in, by the rules of state_normal().
 * Once in place the piece only goes straight down, to where ai_drop()
 * puts it. */
static int ai_reachable(const tetris_t *t, int x, int rot)
{
    const map_t *map = (const map_t *)&t->map;
    const int nrot = k_rotations[t->block];
    int px = t->x, py = t->y, pr = t->rot;
    unsigned frame = t->frame;
    int first = 1;

    for (;;) {
        if (!first) {
            int nr;

            if (++frame > t->speed)
                frame = 0;
            if (px == x && pr == rot)
                return 1;
            if (px != x) {
                const int dx = (x > px) ? 1 : -1;

                if (!check_collision(*map, t->block, pr, px + dx, py))
                    px += dx;
            }
            nr = pr;
            if (rot > pr)
                nr = (pr + 1) % nrot;
            else if (rot < pr)
                nr = (pr + nrot - 1) % nrot;
            if (nr != pr && t->block != 3 &&
                !kick_rotation(*map, t->block, pr, nr, &px, &py))
                pr = nr;
        }
        first = 0;
        if (frame == 0) {
            if (check_collision(*map, t->block, pr, px, py + 1))
                return px == x && pr == rot;
            py++;
        }
    }
}

/* Below any real score, and far enough from INT32_MIN that adding the
 * first ply's piece value cannot overflow. */
#define AI_NO_MOVE (-(INT32_C(1) << 30))

/* Second ply of ai_move(): the best score any placement of block reaches
 * on b, for the piece and the field it leaves. */
static int32_t ai_best_leaf(const ai_board_t b, int block, int tetris_play)
{
    ai_board_t tmp;
    int8_t top[TETRIS_COLS];
    const int first = column_tops(b, top);
    int32_t best = AI_NO_MOVE;
    int x, r;

    for (x = TETRIS_AI_X_MIN; x <= TETRIS_AI_X_MAX; x++) {
        for (r = 0; r < k_rotations[block]; r++) {
            ai_drop_t d;
            int32_t score;

            if (ai_drop(b, top, first, block, x, r, tmp, &d) < 0)
                continue;
            score = ai_piece_value(&d, tetris_play) +
                    ai_field_value(tmp, d.from, tetris_play);
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
    ai_board_t board, tmp;
    int8_t top[TETRIS_COLS];
    int32_t best = AI_NO_MOVE;
    int tetris_play = 1;
    int first;
    int x, r;

    t->ai_x = t->x;
    t->ai_rot = (int8_t)t->rot;
    if (t->ai_blunder > 0U && (rnd(t) % 1000U) < t->ai_blunder) {
        ai_blunder(t);
        return;
    }
    ai_board((const int8_t (*)[TETRIS_COLS])t->map, board);
    first = column_tops(board, top);
    for (x = 0; x < TETRIS_COLS; x++) {
        if (x != AI_WELL_COL && TETRIS_ROWS - top[x] >= AI_SAFE_HEIGHT)
            tetris_play = 0;
    }
    for (x = TETRIS_AI_X_MIN; x <= TETRIS_AI_X_MAX; x++) {
        for (r = 0; r < k_rotations[t->block]; r++) {
            ai_drop_t d;
            int32_t score;

            if (ai_drop(board, top, first, t->block, x, r, tmp, &d) < 0)
                continue;
            score = ai_piece_value(&d, tetris_play) +
                    ai_best_leaf(tmp, t->next, tetris_play);
            if (score > best && ai_reachable(t, x, r)) {
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
    int x = t->x, y = t->y;

    if (kick_rotation((const int8_t (*)[TETRIS_COLS])t->map, t->block, from,
                      t->rot, &x, &y))
        return 1;
    t->x = (int8_t)x;
    t->y = (int8_t)y;
    return 0;
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
