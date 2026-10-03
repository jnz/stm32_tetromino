/*
 * See render.h. Plain CPU drawing, every primitive clips to the screen.
 */
#include "render.h"
#include "assets.h"
#include <stddef.h>

#define TILE      ((int)ASSET_TILE)
#define FIELD_X   0
#define FIELD_Y   20                         /* top of the first visible row */
#define FIELD_W   RENDER_FIELD_W
#define FIELD_H   ((TETRIS_ROWS - TETRIS_HIDDEN_ROWS) * TILE)
#define PANEL_X   (FIELD_W + 1)
#define PANEL_W   (RENDER_W - PANEL_X)
#define PANEL_PAD 6

#define RGB(r, g, b) \
    (0xFF000000U | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define C_WHITE   RGB(255, 255, 255)
#define C_LABEL   RGB(150, 150, 150)
#define C_GOLD    RGB(240, 200, 90)
#define C_RED     RGB(255, 40, 40)
#define C_GREEN   RGB(90, 230, 90)
#define C_FRAME   RGB(80, 80, 80)

/* --------------------------------------------------------------------- */
/* Primitives                                                            */
/* --------------------------------------------------------------------- */

/* src over dst with alpha a (0..255), both opaque ARGB8888. */
static uint32_t blend(uint32_t dst, uint32_t src, uint32_t a)
{
    const uint32_t na = 255U - a;
    const uint32_t rb = ((dst & 0x00FF00FFU) * na + (src & 0x00FF00FFU) * a) >> 8;
    const uint32_t g  = ((dst & 0x0000FF00U) * na + (src & 0x0000FF00U) * a) >> 8;

    return 0xFF000000U | (rb & 0x00FF00FFU) | (g & 0x0000FF00U);
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > RENDER_W) *w = RENDER_W - *x;
    if (*y + *h > RENDER_H) *h = RENDER_H - *y;
    return *w > 0 && *h > 0;
}

static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t c)
{
    int i, j;

    if (!clip(&x, &y, &w, &h))
        return;
    for (j = 0; j < h; j++) {
        uint32_t *p = fb + (size_t)(y + j) * RENDER_W + (size_t)x;

        for (i = 0; i < w; i++)
            p[i] = c;
    }
}

static void blend_rect(uint32_t *fb, int x, int y, int w, int h,
                       uint32_t c, uint32_t a)
{
    int i, j;

    if (!clip(&x, &y, &w, &h))
        return;
    for (j = 0; j < h; j++) {
        uint32_t *p = fb + (size_t)(y + j) * RENDER_W + (size_t)x;

        for (i = 0; i < w; i++)
            p[i] = blend(p[i], c, a);
    }
}

/* One tile bitmap at pixel position (x, y). Tiles are only ever drawn
 * inside the screen, so there is no clipping here. */
static void draw_tile(uint32_t *fb, int x, int y, const uint32_t *tile)
{
    int i, j;

    for (j = 0; j < TILE; j++) {
        uint32_t *p = fb + (size_t)(y + j) * RENDER_W + (size_t)x;
        const uint32_t *s = tile + j * TILE;

        for (i = 0; i < TILE; i++)
            p[i] = s[i];
    }
}

static void draw_ghost(uint32_t *fb, int x, int y, uint32_t a)
{
    int i, j;

    for (j = 0; j < TILE; j++) {
        uint32_t *p = fb + (size_t)(y + j) * RENDER_W + (size_t)x;
        const uint8_t *s = asset_tile_ghost + j * TILE;

        for (i = 0; i < TILE; i++)
            p[i] = blend(p[i], 0xFF000000U | (uint32_t)s[i] * 0x010101U, a);
    }
}

/* --------------------------------------------------------------------- */
/* Text                                                                  */
/* --------------------------------------------------------------------- */

static const asset_glyph_t *glyph(const asset_font_t *f, char ch)
{
    if (ch < ASSET_FONT_FIRST || ch > ASSET_FONT_LAST)
        ch = '?';
    return &f->glyph[ch - ASSET_FONT_FIRST];
}

static int text_width(const asset_font_t *f, const char *s)
{
    int w = 0;

    while (*s != '\0')
        w += glyph(f, *s++)->advance;
    return w;
}

/* Text with its box's top left corner at (x, y), alpha a over everything
 * the glyph covers. */
static void draw_text_a(uint32_t *fb, const asset_font_t *f, int x, int y,
                        const char *s, uint32_t c, uint32_t a)
{
    for (; *s != '\0'; s++) {
        const asset_glyph_t *g = glyph(f, *s);
        const uint8_t *bm = f->bitmap + g->offset;
        int i, j;

        for (j = 0; j < f->height; j++) {
            const int py = y + j;

            if (py < 0 || py >= RENDER_H)
                continue;
            for (i = 0; i < g->width; i++) {
                const int px = x + g->xoff + i;
                const uint32_t cov = bm[j * g->width + i];
                uint32_t *p;

                if (cov == 0U || px < 0 || px >= RENDER_W)
                    continue;
                p = fb + (size_t)py * RENDER_W + (size_t)px;
                *p = blend(*p, c, cov * a / 255U);
            }
        }
        x += g->advance;
    }
}

/* With the half transparent drop shadow of drawtext() in the browser. */
static void draw_text(uint32_t *fb, const asset_font_t *f, int x, int y,
                      const char *s, uint32_t c)
{
    draw_text_a(fb, f, x + 1, y + 1, s, 0xFF000000U, 128U);
    draw_text_a(fb, f, x, y, s, c, 255U);
}

static void draw_text_right(uint32_t *fb, const asset_font_t *f, int xr,
                            int y, const char *s, uint32_t c)
{
    draw_text(fb, f, xr - text_width(f, s), y, s, c);
}

static void draw_text_center(uint32_t *fb, const asset_font_t *f, int xc,
                             int y, const char *s, uint32_t c)
{
    draw_text(fb, f, xc - text_width(f, s) / 2, y, s, c);
}

/* Decimal with thousands separators, like toLocaleString() in the browser.
 * buf holds at least 14 characters (4294967295 -> "4,294,967,295"). */
static const char *fmt_u32(char *buf, uint32_t v)
{
    char tmp[10];
    int n = 0;
    int i;
    char *p = buf;

    do {
        tmp[n++] = (char)('0' + v % 10U);
        v /= 10U;
    } while (v != 0U);
    for (i = n - 1; i >= 0; i--) {
        *p++ = tmp[i];
        if (i > 0 && i % 3 == 0)
            *p++ = ',';
    }
    *p = '\0';
    return buf;
}

/* "prefix" + number, for the short lines that need both. */
static const char *fmt_join(char *buf, const char *prefix, uint32_t v,
                            const char *suffix)
{
    char num[14];
    char *p = buf;
    const char *s;

    for (s = prefix; *s != '\0'; s++)
        *p++ = *s;
    for (s = fmt_u32(num, v); *s != '\0'; s++)
        *p++ = *s;
    for (s = suffix; *s != '\0'; s++)
        *p++ = *s;
    *p = '\0';
    return buf;
}

/* --------------------------------------------------------------------- */
/* Screen parts                                                          */
/* --------------------------------------------------------------------- */

/* Short form for numbers that do not fit in full: one decimal and k, M or
 * G, truncated ("2,049,999" -> "2.0M"). */
static const char *fmt_short(char *buf, uint32_t v)
{
    static const char k_unit[3] = { 'k', 'M', 'G' };
    uint32_t div = 1000U;
    int u = 0;
    uint32_t tenths;
    char *p = buf;
    char num[14];
    const char *s;

    if (v < 10000U)
        return fmt_u32(buf, v);
    while (u < 2 && v / div >= 1000U) {
        div *= 1000U;
        u++;
    }
    tenths = (uint32_t)((uint64_t)v * 10U / div);
    for (s = fmt_u32(num, tenths / 10U); *s != '\0'; s++)
        *p++ = *s;
    *p++ = '.';
    *p++ = (char)('0' + tenths % 10U);
    *p++ = k_unit[u];
    *p = '\0';
    return buf;
}

/* "BEST <score>" and the lines of that game after it, as large as the
 * space left of the AI/YOU tag allows: score in the big font, then both
 * small, then the lines shortened. Holds for the largest values a uint32_t
 * takes, which a perfect AI running for months could get near. */
static void draw_title_best(uint32_t *fb, uint32_t score, uint32_t lines,
                            int show_lines, int xmax)
{
    const int x0 = 6;
    const int gap = 6;
    char sbuf[24];
    char lbuf[24];
    char num[14];
    const asset_font_t *sf = &font_big;
    int ws, wl;

    fmt_join(sbuf, "BEST ", score, "");
    fmt_join(lbuf, "", lines, " lines");
    ws = text_width(&font_big, sbuf);
    wl = show_lines ? gap + text_width(&font_small, lbuf) : 0;

    if (x0 + ws + wl > xmax) {
        sf = &font_small;
        ws = text_width(sf, sbuf);
    }
    if (show_lines && x0 + ws + wl > xmax) {
        char *p = lbuf;
        const char *q;

        for (q = fmt_short(num, lines); *q != '\0'; q++)
            *p++ = *q;
        for (q = " lines"; *q != '\0'; q++)
            *p++ = *q;
        *p = '\0';
        wl = gap + text_width(&font_small, lbuf);
    }
    if (x0 + ws + wl > xmax)
        show_lines = 0;

    /* All on the baseline of the big font. */
    draw_text(fb, sf, x0, 3 + font_big.baseline - sf->baseline, sbuf, C_GOLD);
    if (show_lines)
        draw_text(fb, &font_small, x0 + ws + gap,
                  3 + font_big.baseline - font_small.baseline, lbuf, C_LABEL);
}

static void draw_title_bar(uint32_t *fb, const tetris_t *t,
                           const render_info_t *info)
{
    /* The best game so far, this one included once it is ahead. */
    const int leading = t->score > info->best_score;
    const char *tag = t->ai_active ? "AI" : "YOU";
    const int xmax = RENDER_W - 6 - text_width(&font_big, tag) - 8;
    char buf[24];
    int y;

    /* The grey gradient the browser draws over the two spawn rows. */
    for (y = 0; y < FIELD_Y; y++) {
        const uint32_t c = 50U + 30U * (uint32_t)y / (FIELD_Y - 1);

        fill_rect(fb, 0, y, RENDER_W, 1, RGB(c, c, c));
    }

    if (t->levelup_ticks > 0U && t->level > 1U)
        draw_text(fb, &font_big, 6, 3, fmt_join(buf, "Level ", t->level, "!"),
                  C_WHITE);
    else
        draw_title_best(fb, leading ? t->score : info->best_score,
                        leading ? t->lines : info->best_lines,
                        !info->human, xmax);

    draw_text_right(fb, &font_big, RENDER_W - 6, 3, tag,
                    t->ai_active ? C_RED : C_GREEN);
}

static void draw_field(uint32_t *fb, const tetris_t *t)
{
    const int top = TETRIS_HIDDEN_ROWS;
    int r, c, x, y;

    /* Background, grey only. The visible rows start TETRIS_HIDDEN_ROWS
     * tiles into the image, as they do in the browser, where the title bar
     * covers its top. */
    for (y = 0; y < FIELD_H; y++) {
        uint32_t *p = fb + (size_t)(FIELD_Y + y) * RENDER_W + FIELD_X;
        const uint8_t *s = asset_bg + (size_t)(y + top * TILE) * ASSET_BG_W;

        for (x = 0; x < FIELD_W; x++)
            p[x] = 0xFF000000U | (uint32_t)s[x] * 0x010101U;
    }

    for (r = top; r < TETRIS_ROWS; r++) {
        for (c = 0; c < TETRIS_COLS; c++) {
            const int v = t->map[r][c];
            const int px = FIELD_X + c * TILE;
            const int py = FIELD_Y + (r - top) * TILE;

            if (v > 0)
                draw_tile(fb, px, py, asset_tile[v - 1]);
            else if (v < 0)
                draw_tile(fb, px, py, asset_tile_flash[-v - 1]);
        }
    }

    if (t->state != TETRIS_FIRSTBLOCK && t->state != TETRIS_NORMAL)
        return;

    {
        const int n = tetris_piece_size(t->block);
        const int gy = tetris_ghost_y(t);
        /* Fainter near the top, as ghostalpha in the browser. */
        const uint32_t ga = (uint32_t)(255 * (gy * 50 / TETRIS_ROWS + 10) / 100);
        int i, j;

        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                const int cell = tetris_piece_cell(t->block, t->rot, i, j);
                const int px = FIELD_X + (t->x + j) * TILE;

                if (cell == 0)
                    continue;
                if (gy >= 0 && gy + i >= top)
                    draw_ghost(fb, px, FIELD_Y + (gy + i - top) * TILE, ga);
            }
        }
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                const int cell = tetris_piece_cell(t->block, t->rot, i, j);
                const int px = FIELD_X + (t->x + j) * TILE;

                if (cell != 0 && t->y + i >= top)
                    draw_tile(fb, px, FIELD_Y + (t->y + i - top) * TILE,
                              asset_tile[cell - 1]);
            }
        }
    }
}

/* A value right aligned in the side panel. The big font while it fits,
 * else the small one, which takes even the largest uint32_t. */
static void draw_value_str(uint32_t *fb, int xr, int y, const char *s, uint32_t c)
{
    const asset_font_t *f = &font_big;

    if (text_width(f, s) > xr - PANEL_X - 1) {
        f = &font_small;
        y += font_big.baseline - font_small.baseline;   /* same baseline */
    }
    draw_text_right(fb, f, xr, y, s, c);
}

static void draw_value(uint32_t *fb, int xr, int y, uint32_t v, uint32_t c)
{
    char buf[14];

    draw_value_str(fb, xr, y, fmt_u32(buf, v), c);
}

/* Game time as m:ss, or h:mm:ss from the first hour on. Hours are not
 * capped and have no thousands separator: a perfect AI game can run for
 * months. */
static const char *fmt_time(char *buf, uint32_t ticks)
{
    const uint32_t sec = ticks / (1000U / TETRIS_TICK_MS);
    const uint32_t h = sec / 3600U;
    const uint32_t m = (sec / 60U) % 60U;
    const uint32_t s = sec % 60U;
    uint32_t lead = (h > 0U) ? h : m;
    char tmp[10];
    int n = 0;
    char *p = buf;

    do {
        tmp[n++] = (char)('0' + lead % 10U);
        lead /= 10U;
    } while (lead != 0U);
    while (n > 0)
        *p++ = tmp[--n];
    if (h > 0U) {
        *p++ = ':';
        *p++ = (char)('0' + m / 10U);
        *p++ = (char)('0' + m % 10U);
    }
    *p++ = ':';
    *p++ = (char)('0' + s / 10U);
    *p++ = (char)('0' + s % 10U);
    *p = '\0';
    return buf;
}

static void draw_preview(uint32_t *fb, int bx, int by, int bsize, int block)
{
    const int n = tetris_piece_size(block);
    int minr = n, maxr = -1, minc = n, maxc = -1;
    int i, j, x0, y0;

    fill_rect(fb, bx - 1, by - 1, bsize + 2, bsize + 2, C_FRAME);
    fill_rect(fb, bx, by, bsize, bsize, RGB(0, 0, 0));

    /* Centre the occupied cells, not the bounding box. */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (tetris_piece_cell(block, 0, i, j) == 0)
                continue;
            if (i < minr) minr = i;
            if (i > maxr) maxr = i;
            if (j < minc) minc = j;
            if (j > maxc) maxc = j;
        }
    }
    x0 = bx + (bsize - (maxc - minc + 1) * TILE) / 2 - minc * TILE;
    y0 = by + (bsize - (maxr - minr + 1) * TILE) / 2 - minr * TILE;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            const int cell = tetris_piece_cell(block, 0, i, j);

            if (cell != 0)
                draw_tile(fb, x0 + j * TILE, y0 + i * TILE, asset_tile[cell - 1]);
        }
    }
}

static void draw_panel(uint32_t *fb, const tetris_t *t, const render_info_t *info)
{
    const int xl = PANEL_X + PANEL_PAD;
    const int xr = RENDER_W - PANEL_PAD;
    const int box = 4 * TILE + 4;
    char buf[24];
    int y;

    for (y = FIELD_Y; y < RENDER_H; y++) {
        const uint32_t c = 34U - 16U * (uint32_t)(y - FIELD_Y) / (RENDER_H - FIELD_Y);

        fill_rect(fb, PANEL_X, y, PANEL_W, 1, RGB(c, c, c));
    }
    fill_rect(fb, FIELD_W, FIELD_Y, 1, RENDER_H - FIELD_Y, C_FRAME);

    draw_text(fb, &font_small, xl, 24, "NEXT", C_LABEL);
    draw_preview(fb, PANEL_X + (PANEL_W - box) / 2, 37, box, t->next);

    draw_text(fb, &font_small, xl, 110, "SCORE", C_LABEL);
    draw_value(fb, xr, 122, t->score, C_WHITE);

    draw_text(fb, &font_small, xl, 144, "LINES", C_LABEL);
    draw_value(fb, xr, 156, t->lines, C_WHITE);

    draw_text(fb, &font_small, xl, 178, "LEVEL", C_LABEL);
    draw_value(fb, xr, 190, t->level, C_WHITE);

    draw_text(fb, &font_small, xl, 212, "TIME", C_LABEL);
    draw_value_str(fb, xr, 224, fmt_time(buf, t->ticks), C_WHITE);

    draw_text(fb, &font_small, xl, 246, "GAME", C_LABEL);
    draw_value_str(fb, xr, 258,
                   fmt_join(buf, "#", info->games + (info->game_over ? 0U : 1U), ""),
                   C_WHITE);
}

static void draw_game_over(uint32_t *fb, const tetris_t *t, const render_info_t *info)
{
    const int xc = FIELD_X + FIELD_W / 2;
    char buf[24];

    blend_rect(fb, FIELD_X, FIELD_Y, FIELD_W, FIELD_H, RGB(200, 0, 0), 128U);
    /* By the time the AI loses, the field is full of tiles. A dark card
     * keeps the text readable on top of them. */
    blend_rect(fb, FIELD_X + 4, 90, FIELD_W - 8, 138, RGB(0, 0, 0), 190U);
    fill_rect(fb, FIELD_X + 4, 90, FIELD_W - 8, 1, C_GOLD);
    fill_rect(fb, FIELD_X + 4, 227, FIELD_W - 8, 1, C_GOLD);

    draw_text_center(fb, &font_huge, xc, 100, "GAME OVER", C_WHITE);

    draw_text_center(fb, &font_small, xc, 138, "SCORE", C_WHITE);
    draw_text_center(fb, &font_big, xc, 150, fmt_u32(buf, t->score), C_WHITE);

    if (info->new_record) {
        if ((info->anim_ms / 400U) % 2U == 0U)
            draw_text_center(fb, &font_big, xc, 186, "NEW RECORD!", C_GOLD);
    } else {
        draw_text_center(fb, &font_small, xc, 180,
                         info->human ? "YOUR BEST" : "ALL-TIME BEST", C_GOLD);
        draw_text_center(fb, &font_big, xc, 192, fmt_u32(buf, info->best_score),
                         C_GOLD);
        if (!info->human)
            draw_text_center(fb, &font_small, xc, 210,
                             fmt_join(buf, "", info->best_lines, " lines"), C_WHITE);
    }
}

/* Where to touch, for the first seconds of a human game. Matches the
 * zones in app.c: the field in thirds, the panel for the hard drop. */
static void draw_hint(uint32_t *fb)
{
    const int third = FIELD_W / 3;
    const int y = RENDER_H - 46;

    blend_rect(fb, third, FIELD_Y, 1, FIELD_H, C_WHITE, 90U);
    blend_rect(fb, 2 * third, FIELD_Y, 1, FIELD_H, C_WHITE, 90U);
    blend_rect(fb, FIELD_X, y - 6, FIELD_W, 28, RGB(0, 0, 0), 150U);
    draw_text_center(fb, &font_big, third / 2, y, "<", C_WHITE);
    draw_text_center(fb, &font_big, third + third / 2, y, "ROT", C_WHITE);
    draw_text_center(fb, &font_big, 2 * third + third / 2, y, ">", C_WHITE);
    blend_rect(fb, PANEL_X, y - 6, PANEL_W, 28, RGB(0, 0, 0), 150U);
    draw_text_center(fb, &font_big, PANEL_X + PANEL_W / 2, y, "DROP", C_WHITE);
}

void render_frame(uint32_t *fb, const tetris_t *t, const render_info_t *info)
{
    draw_title_bar(fb, t, info);
    draw_field(fb, t);
    draw_panel(fb, t, info);
    if (info->hint)
        draw_hint(fb);
    if (info->game_over)
        draw_game_over(fb, t, info);
}
