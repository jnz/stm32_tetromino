#ifndef TETRIS_FX_H
#define TETRIS_FX_H
/*
 * Effects that work on the whole picture without touching a pixel of it:
 *
 *   palette   a line clear sets off a short change of the colours, applied
 *             to the palette (asset_pal) the picture is looked up through.
 *             The more lines, the more of it:
 *               1 line    nothing, it comes too often
 *               2 lines   the picture lights up warm, white text goes dark
 *               3 lines   the same in a cool blue
 *               4 lines   a pastel light going round the colour wheel
 *             All changes are fades, at most one dark-bright-dark swing per
 *             effect (well under the 3 flashes per second that photosensitive
 *             viewers may react to).
 *
 *   slam      a clear of three or four lines pulls the picture down and
 *             lets it spring back, like on a rubber band, more when a hard
 *             drop did it. A human's hard drop gets a slight one too.
 *
 *   shift     the picture wanders by up to FX_SHIFT_MAX pixels, one pixel
 *             every FX_SHIFT_MS. On an LCD that shows the same frame, labels
 *             and separator for months, this spreads their edges over a few
 *             pixels, against image retention ("burn in").
 *
 * Platform independent. The firmware hands the results to the LTDC (palette
 * into the CLUT, shift into the layer window), the simulator applies them
 * when it writes a frame.
 */
#include <stdint.h>

#define FX_SHIFT_MAX   2
#define FX_SHIFT_MS    (2U * 60U * 1000U)

typedef struct {
    uint32_t start_ms;
    uint8_t  lines;     /* lines of the running effect, 0 = none yet */
    uint8_t  slam_amp;  /* amplitude of the slam, tenths of a pixel */
    uint32_t slam_ms;   /* when it started */
} fx_t;

/* lines (1..4) were cleared at now_ms. Takes over from a running effect
 * unless that one is for more lines. */
void fx_lines_cleared(fx_t *fx, int lines, uint32_t now_ms);

/* The palette at now_ms into pal (256 entries, 0x00RRGGBB). 1 = an effect
 * is running and pal holds it, 0 = none, pal is asset_pal unchanged. */
int fx_palette(const fx_t *fx, uint32_t now_ms, uint32_t pal[256]);

/* lines were cleared at now_ms (0 for none), hard: by a hard drop, human:
 * by a person. Starts a slam where there is one for that. */
void fx_slam(fx_t *fx, int lines, int hard, int human, uint32_t now_ms);

/* How far the slam has pulled the picture down at now_ms, in pixels (up
 * is negative): -2 .. 8. */
int fx_shake(const fx_t *fx, uint32_t now_ms);

/* The picture's offset at now_ms, each in -FX_SHIFT_MAX..FX_SHIFT_MAX. */
void fx_shift(uint32_t now_ms, int *dx, int *dy);

#endif /* TETRIS_FX_H */
