/*
 * See fx.h. Amounts are 0..256, colours 0x00RRGGBB, all in integers.
 */
#include "fx.h"
#include "assets.h"
#include <string.h>

/* Length of the effect per number of lines, index 1..4. */
static const uint32_t k_fx_ms[5] = { 0U, 0U, 720U, 720U, 1280U };

void fx_lines_cleared(fx_t *fx, int lines, uint32_t now_ms)
{
    if (lines < 2)   /* a single is too common, it would never stop */
        return;
    if (lines > 4)
        lines = 4;
    if (fx->lines > lines && now_ms - fx->start_ms < k_fx_ms[fx->lines])
        return;
    fx->start_ms = now_ms;
    fx->lines = (uint8_t)lines;
}

/* --------------------------------------------------------------------- */
/* Envelopes and colour operations                                       */
/* --------------------------------------------------------------------- */

/* 0 up to t0, eased up to 256 at t1, held to t2, eased down to 0 at t3. */
static uint32_t ramp(uint32_t t, uint32_t t0, uint32_t t1, uint32_t t2,
                     uint32_t t3)
{
    uint32_t x;

    if (t <= t0 || t >= t3)
        return 0U;
    if (t < t1)
        x = (t - t0) * 256U / (t1 - t0);
    else if (t <= t2)
        return 256U;
    else
        x = (t3 - t) * 256U / (t3 - t2);
    /* smoothstep, x * x * (3 - 2x) on 0..256 */
    return x * x * (768U - 2U * x) >> 16;
}

static uint32_t ch(uint32_t c, int shift)
{
    return (c >> shift) & 0xFFU;
}

static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b)
{
    return (r << 16) | (g << 8) | b;
}

/* a of the way from c to d, per channel. */
static uint32_t mix(uint32_t c, uint32_t d, uint32_t a)
{
    uint32_t out = 0U;
    int s;

    for (s = 0; s <= 16; s += 8) {
        const uint32_t x = ch(c, s);
        const uint32_t y = ch(d, s);

        out |= ((x * (256U - a) + y * a) >> 8) << s;
    }
    return out;
}

/* "Screen" blend: lightens c towards the colour t, also greys and black,
 * which make up most of the picture. */
static uint32_t screen(uint32_t c, uint32_t t)
{
    uint32_t out = 0U;
    int s;

    for (s = 0; s <= 16; s += 8)
        out |= (255U - (255U - ch(c, s)) * (255U - ch(t, s)) / 255U) << s;
    return out;
}

/* Hue turned by phase (0..767 is a full turn), as a blend between the
 * colour and its channels rotated by one (red to green) and by two. Not a
 * true hue rotation, but cheap and it goes round the wheel. */
static uint32_t hue(uint32_t c, uint32_t phase)
{
    const uint32_t r = ch(c, 16), g = ch(c, 8), b = ch(c, 0);
    const uint32_t p[3] = { c, rgb(b, r, g), rgb(g, b, r) };
    const uint32_t s = (phase >> 8) % 3U;

    return mix(p[s], p[(s + 1U) % 3U], phase & 0xFFU);
}

/* How much of a text colour c is, 0..256: light and grey, as the white
 * values and the grey labels are. Tiles, also the whitened ones of a
 * flashing line, and the gold best score have too much colour, the
 * background is too dark. */
static uint32_t textness(uint32_t c)
{
    const uint32_t r = ch(c, 16), g = ch(c, 8), b = ch(c, 0);
    const uint32_t hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const uint32_t lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const uint32_t luma = (r * 77U + g * 150U + b * 29U) >> 8;
    uint32_t w;

    if (luma <= 95U || hi - lo >= 16U)
        return 0U;
    w = (luma >= 140U) ? 256U : (luma - 95U) * 256U / 45U;
    return w * (16U - (hi - lo)) / 16U;
}

/* --------------------------------------------------------------------- */
/* Palette                                                               */
/* --------------------------------------------------------------------- */

#define FX_WARM    0xFFBE46U   /* the light of a double */
#define FX_COOL    0x46BEFFU   /* ... and of a triple   */
#define FX_INK     0x2A1A08U   /* white text on it */
#define FX_RAINBOW 0xFF6464U   /* base of the turning light of a tetromino */
#define FX_WHITE   0xFFFFFFU

int fx_palette(const fx_t *fx, uint32_t now_ms, uint32_t pal[256])
{
    const uint32_t t = now_ms - fx->start_ms;
    uint32_t env = 0U, light = 0U, light_c = FX_WARM;
    int k;

    memcpy(pal, asset_pal, 256U * sizeof pal[0]);
    if (fx->lines == 0U || t >= k_fx_ms[fx->lines])
        return 0;

    switch (fx->lines) {
    case 2:
    case 3:
        env = ramp(t, 0U, 120U, 280U, 720U);
        light = env * 160U / 256U;
        light_c = (fx->lines == 2) ? FX_WARM : FX_COOL;
        break;
    default:
        /* Lit up like the others, by a pastel light that goes once round
         * the colour wheel. The picture's own colours stay: turning them
         * as well looked psychedelic on a large screen. */
        env = ramp(t, 0U, 160U, 960U, 1280U);
        light = env * 150U / 256U;
        light_c = mix(hue(FX_RAINBOW, t * 768U / 1280U), FX_WHITE, 128U);
        break;
    }

    for (k = 0; k < 256; k++) {
        /* Text is told by its own colour, before anything changed it. */
        const uint32_t ink = textness(pal[k]);
        uint32_t c = pal[k];

        if (light != 0U)
            c = mix(c, screen(c, light_c), light);
        /* The text turns dark on the lit up picture. */
        if (ink != 0U)
            c = mix(c, FX_INK, ink * env / 256U);
        pal[k] = c;
    }
    return 1;
}

/* --------------------------------------------------------------------- */
/* Slam                                                                  */
/* --------------------------------------------------------------------- */

/* The way the picture moves, in thousandths of the amplitude, every 25 ms:
 * pulled down hard, then springing back past its place and settling, like
 * on a rubber band. At 20 frames per second every other point is shown. */
static const int16_t k_slam[] = {
    0, 700, 1000, 850, 550, 250, 0, -150, -200, -150, -80, -20, 0
};
#define SLAM_STEP_MS  25U
#define SLAM_MS       (SLAM_STEP_MS * (sizeof k_slam / sizeof k_slam[0] - 1U))

void fx_slam(fx_t *fx, int lines, int hard, int human, uint32_t now_ms)
{
    int amp;

    if (lines >= 4)
        amp = hard ? 80 : 50;
    else if (lines == 3)
        amp = hard ? 40 : 25;
    else if (human && hard)
        amp = 8;
    else
        return;
    /* A weaker one does not cut a stronger one short. */
    if (now_ms - fx->slam_ms < SLAM_MS && amp < fx->slam_amp)
        return;
    fx->slam_ms = now_ms;
    fx->slam_amp = (uint8_t)amp;
}

int fx_shake(const fx_t *fx, uint32_t now_ms)
{
    const uint32_t t = now_ms - fx->slam_ms;
    const uint32_t i = t / SLAM_STEP_MS;
    const int32_t f = (int32_t)(t % SLAM_STEP_MS);
    int32_t v;

    if (fx->slam_amp == 0U || t >= SLAM_MS)
        return 0;
    v = k_slam[i] + (k_slam[i + 1U] - k_slam[i]) * f / (int32_t)SLAM_STEP_MS;
    v *= fx->slam_amp;                      /* in 1/10000 pixel */
    return (int)((v + (v >= 0 ? 5000 : -5000)) / 10000);
}

/* --------------------------------------------------------------------- */
/* Shift                                                                 */
/* --------------------------------------------------------------------- */

/* 0, 1, 2, 1, 0, -1, -2, -1, ... for FX_SHIFT_MAX 2: one pixel per step,
 * the middle passed twice as often as the ends. */
static int triangle(uint32_t k)
{
    const uint32_t n = 4U * FX_SHIFT_MAX;
    const int i = (int)((k + FX_SHIFT_MAX) % n);

    return (i <= 2 * FX_SHIFT_MAX) ? i - FX_SHIFT_MAX : 3 * FX_SHIFT_MAX - i;
}

void fx_shift(uint32_t now_ms, int *dx, int *dy)
{
    /* x steps every period, y every full sweep of x: all positions in
     * (4 * FX_SHIFT_MAX)^2 steps, about two hours. Never more than one
     * pixel per axis at a time. */
    const uint32_t k = now_ms / FX_SHIFT_MS;

    *dx = triangle(k);
    *dy = triangle(k / (4U * FX_SHIFT_MAX));
}
