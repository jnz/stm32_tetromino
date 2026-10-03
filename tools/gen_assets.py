#!/usr/bin/env python3
"""Generate game/assets.c and game/assets.h from the browser version's
artwork in art/ (https://zwiener.org/tetromino.html).

    python tools/gen_assets.py            # from the repository root

Run once whenever the artwork, the tile size or the fonts change. The
output is committed, so building the firmware needs neither Python nor
Pillow.

What is generated:

  tiles       art/tetromino_blocks.png holds the seven 25x25 tiles
              exactly as the browser renders them. They are scaled down to
              TILE pixels and sorted into the colour order of tetromino.js
              (index 1 = I ... 7 = Z), matched on the top left pixel, which
              is the "high" colour of colortableH in the JavaScript.
  flash       Brightened copy of each tile, for complete lines while they
              flash before being removed (negative map values in the game).
  ghost       Grey tile, blended over the field with a per frame alpha at
              the landing position of the falling piece.
  background  art/basi.png, scaled from the 10x22 tile canvas of the
              browser (250x550) to 10x22 tiles of TILE pixels. It is grey
              only, so one byte per pixel is enough.
  fonts       DejaVu Sans Bold, anti-aliased, 8 bit coverage per pixel,
              printable ASCII. DejaVu is under a free license (Bitstream
              Vera derivative), unlike the Arial the browser version used.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

TILE = 15
COLS = 10
ROWS = 22

# colortableH of tetromino.js, index 1..7 = I J L O S T Z
COLORTABLE_H = [
    (198, 223, 126), (105, 134, 174), (231, 193, 131), (189, 107, 166),
    (231, 214, 131), (112, 198, 112), (231, 131, 131),
]

# (C name, pixel size). Pixel size, not point size: the panel has no DPI.
FONTS = [("font_small", 10), ("font_big", 14), ("font_huge", 19)]

FIRST_CHAR = 32
LAST_CHAR = 126

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def find_font(explicit):
    if explicit:
        return explicit
    try:
        import matplotlib
        p = os.path.join(os.path.dirname(matplotlib.__file__), "mpl-data",
                         "fonts", "ttf", "DejaVuSans-Bold.ttf")
        if os.path.isfile(p):
            return p
    except ImportError:
        pass
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"):
        if os.path.isfile(p):
            return p
    sys.exit("DejaVuSans-Bold.ttf not found, pass --font")


def argb(rgb):
    r, g, b = rgb[:3]
    return 0xFF000000 | (r << 16) | (g << 8) | b


def tiles(path):
    sheet = Image.open(path).convert("RGB")
    n = sheet.width // 25
    by_index = {}
    for k in range(n):
        t = sheet.crop((25 * k, 0, 25 * k + 25, 25))
        corner = t.getpixel((0, 0))
        dist = [sum((a - b) ** 2 for a, b in zip(corner, c)) for c in COLORTABLE_H]
        idx = dist.index(min(dist))
        if dist[idx] > 300:
            sys.exit("tile %d does not match any colour of tetromino.js" % k)
        if idx in by_index:
            sys.exit("two tiles match colour index %d" % (idx + 1))
        by_index[idx] = t.resize((TILE, TILE), Image.LANCZOS)
    if len(by_index) != 7:
        sys.exit("expected 7 tiles, got %d" % len(by_index))
    return [by_index[i] for i in range(7)]


def flash_tile(t):
    white = Image.new("RGB", t.size, (255, 255, 255))
    return Image.blend(t, white, 0.55)


def ghost_tile(normal):
    # Average luminance pattern of all seven tiles, shifted to a mean of
    # 120 (the ghost colour of tetromino.js). Keeps the bevel of the real
    # tiles, so the ghost reads as the same kind of block.
    acc = [0.0] * (TILE * TILE)
    for t in normal:
        for i, p in enumerate(t.convert("L").getdata()):
            acc[i] += p / len(normal)
    mean = sum(acc) / len(acc)
    out = Image.new("RGB", (TILE, TILE))
    out.putdata([tuple([max(0, min(255, int(round(v - mean + 120))))] * 3)
                 for v in acc])
    return out


def background(path):
    im = Image.open(path).convert("L")
    return im.resize((COLS * TILE, ROWS * TILE), Image.LANCZOS)


def font(path, px):
    f = ImageFont.truetype(path, px)
    ascent, descent = f.getmetrics()
    glyphs = []
    top, bottom = ascent + descent, 0
    for c in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(c)
        l, t, r, b = f.getbbox(ch)
        adv = int(round(f.getlength(ch)))
        xoff = min(0, l)
        w = max(adv, r) - xoff
        im = Image.new("L", (max(w, 1), ascent + descent), 0)
        ImageDraw.Draw(im).text((-xoff, 0), ch, font=f, fill=255)
        bb = im.getbbox()
        if bb:
            top = min(top, bb[1])
            bottom = max(bottom, bb[3])
        glyphs.append((ch, xoff, w, adv, im))
    out = []
    for ch, xoff, w, adv, im in glyphs:
        im = im.crop((0, top, im.width, bottom))
        out.append((ch, xoff, w, adv, list(im.getdata())))
    # Distance from the top of the cropped box to the baseline, for
    # aligning fonts of different size on one line.
    return out, bottom - top, ascent - top


def c_array_u32(vals, per_line=6):
    lines = []
    for i in range(0, len(vals), per_line):
        lines.append("    " + ", ".join("0x%08X" % v for v in vals[i:i + per_line]) + ",")
    return "\n".join(lines)


def c_array_u8(vals, per_line=16):
    lines = []
    for i in range(0, len(vals), per_line):
        lines.append("    " + ",".join("%d" % v for v in vals[i:i + per_line]) + ",")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--font", help="path to DejaVuSans-Bold.ttf")
    args = ap.parse_args()

    tpl = os.path.join(ROOT, "art")
    normal = tiles(os.path.join(tpl, "tetromino_blocks.png"))
    flash = [flash_tile(t) for t in normal]
    ghost = ghost_tile(normal)
    bg = background(os.path.join(tpl, "basi.png"))
    fontpath = find_font(args.font)
    fonts = [(name, px) + font(fontpath, px) for name, px in FONTS]

    h = []
    h.append("/* Generated by tools/gen_assets.py, do not edit. */")
    h.append("#ifndef TETRIS_ASSETS_H")
    h.append("#define TETRIS_ASSETS_H")
    h.append("#include <stdint.h>")
    h.append("")
    h.append("#define ASSET_TILE   %dU   /* tile edge in pixels */" % TILE)
    h.append("#define ASSET_BG_W   %dU" % (COLS * TILE))
    h.append("#define ASSET_BG_H   %dU" % (ROWS * TILE))
    h.append("")
    h.append("/* One glyph of an anti-aliased font. The bitmap is width x the")
    h.append(" * font height, one coverage byte per pixel, row major. xoff is")
    h.append(" * where the bitmap starts relative to the pen position, advance")
    h.append(" * is how far the pen moves on. */")
    h.append("typedef struct {")
    h.append("    uint32_t offset;   /* into the font's bitmap array */")
    h.append("    int8_t   xoff;")
    h.append("    uint8_t  width;")
    h.append("    uint8_t  advance;")
    h.append("} asset_glyph_t;")
    h.append("")
    h.append("typedef struct {")
    h.append("    const uint8_t       *bitmap;")
    h.append("    const asset_glyph_t *glyph;   /* FIRST..LAST char */")
    h.append("    uint8_t              height;")
    h.append("    uint8_t              baseline; /* from the top of the box */")
    h.append("} asset_font_t;")
    h.append("")
    h.append("#define ASSET_FONT_FIRST  %d" % FIRST_CHAR)
    h.append("#define ASSET_FONT_LAST   %d" % LAST_CHAR)
    h.append("")
    h.append("/* ARGB8888, index 0..6 = colour 1..7 of the game (I J L O S T Z). */")
    h.append("extern const uint32_t asset_tile[7][ASSET_TILE * ASSET_TILE];")
    h.append("extern const uint32_t asset_tile_flash[7][ASSET_TILE * ASSET_TILE];")
    h.append("/* Grey level per pixel, blended over the field with a variable alpha. */")
    h.append("extern const uint8_t  asset_tile_ghost[ASSET_TILE * ASSET_TILE];")
    h.append("/* Grey level per pixel of the whole 10x22 tile field. */")
    h.append("extern const uint8_t  asset_bg[ASSET_BG_H * ASSET_BG_W];")
    h.append("")
    for name, px, _, _, _ in fonts:
        h.append("extern const asset_font_t %s;   /* DejaVu Sans Bold %d px */" % (name, px))
    h.append("")
    h.append("#endif /* TETRIS_ASSETS_H */")

    c = []
    c.append("/* Generated by tools/gen_assets.py, do not edit.")
    c.append(" *")
    c.append(" * Tiles and background from art/, fonts rendered from DejaVu Sans")
    c.append(" * Bold (Bitstream Vera license, see the DejaVu project). */")
    c.append('#include "assets.h"')
    c.append("")
    c.append("const uint32_t asset_tile[7][ASSET_TILE * ASSET_TILE] = {")
    for t in normal:
        c.append("  {")
        c.append(c_array_u32([argb(p) for p in t.getdata()]))
        c.append("  },")
    c.append("};")
    c.append("")
    c.append("const uint32_t asset_tile_flash[7][ASSET_TILE * ASSET_TILE] = {")
    for t in flash:
        c.append("  {")
        c.append(c_array_u32([argb(p) for p in t.getdata()]))
        c.append("  },")
    c.append("};")
    c.append("")
    c.append("const uint8_t asset_tile_ghost[ASSET_TILE * ASSET_TILE] = {")
    c.append(c_array_u8([p[0] for p in ghost.getdata()]))
    c.append("};")
    c.append("")
    c.append("const uint8_t asset_bg[ASSET_BG_H * ASSET_BG_W] = {")
    c.append(c_array_u8(list(bg.getdata()), 30))
    c.append("};")
    for name, px, glyphs, height, baseline in fonts:
        bitmap = []
        table = []
        for ch, xoff, w, adv, data in glyphs:
            table.append("    { %6dU, %3d, %3d, %3d },  /* '%s' */" % (
                len(bitmap), xoff, w, adv,
                {"\\": "backslash", "'": "quote"}.get(ch, ch)))
            bitmap.extend(data)
        c.append("")
        c.append("static const uint8_t %s_bitmap[%d] = {" % (name, len(bitmap)))
        c.append(c_array_u8(bitmap, 24))
        c.append("};")
        c.append("")
        c.append("static const asset_glyph_t %s_glyph[%d] = {" % (name, len(table)))
        c.extend(table)
        c.append("};")
        c.append("")
        c.append("const asset_font_t %s = { %s_bitmap, %s_glyph, %d, %d };" % (
            name, name, name, height, baseline))
    c.append("")

    for fn, lines in (("assets.h", h), ("assets.c", c)):
        p = os.path.join(ROOT, "game", fn)
        with open(p, "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        print("wrote", p)


if __name__ == "__main__":
    main()
