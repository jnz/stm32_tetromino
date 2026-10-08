#!/usr/bin/env python3
"""Generate the standalone browser pages from their templates in web/src/
and the artwork and the music in art/:

  web/tetromino.html   the game, from web/src/tetromino.html
  web/embed.html       only the field, the AI playing, for an iframe, from
                       web/src/embed.html and the game and AI of
                       web/src/tetromino.html

    python tools/gen_web.py               # from the repository root

Each page is one file that works offline and from anywhere, so the
background goes into it as path data and the music as base64. The
templates keep placeholders instead, which keeps them small enough to work
on:

  @BASI_SVG@   (tetromino.html) art/basi.svg, the background behind the
               field, as the list of its path data
  @SOUND@      the entries of the SOUND table, one per file of art/sound/
               (the music of the browser version at
               https://zwiener.org/tetromino.html; the sound effects
               are synthesized in the page)
  @CORE@       (embed.html) the core of tetromino.html: game, AI and
               effects, between its CORE BEGIN and CORE END

Run it after every change to the templates or to the files in art/. The
output is committed, so opening a page needs neither Python nor a build.
"""
import base64
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

TEMPLATE = os.path.join(ROOT, "web", "src", "tetromino.html")
OUTPUT = os.path.join(ROOT, "web", "tetromino.html")
EMBED_TEMPLATE = os.path.join(ROOT, "web", "src", "embed.html")
EMBED_OUTPUT = os.path.join(ROOT, "web", "embed.html")
BACKGROUND = os.path.join(ROOT, "art", "basi.svg")

# Name in the page's SOUND table, file in art/sound/.
SOUNDS = [
    ("music", "tetromino1.mp3"),
]


def b64(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("ascii")


def svg_paths(path):
    """The d attributes of the paths of a potrace SVG, whitespace squeezed.
    The page draws them with the transform of potrace's <g>, which it
    knows; check that it is that one."""
    with open(path, encoding="utf-8") as f:
        svg = f.read()
    if 'transform="translate(0.000000,2200.000000) scale(0.100000,-0.100000)"' not in svg:
        sys.exit("%s: expected potrace's transform for a 1000 x 2200 view box" % path)
    return [" ".join(d.split()) for d in re.findall(r'<path d="([^"]*)"', svg)]


def core_of(page):
    """The core of tetromino.html: from its "use strict" to the banner of
    CORE END, which it runs under Node too, so it needs no page."""
    a = page.find('"use strict";')
    e = page.find("/* CORE END")
    b = page.rfind("/* ====", 0, e)
    if a < 0 or e < 0 or b < a:
        sys.exit("%s: no core between \"use strict\" and CORE END" % TEMPLATE)
    return page[a:b].rstrip() + "\n"


def fill(template, marks, values):
    with open(template, encoding="utf-8") as f:
        page = f.read()
    for mark in marks:
        if page.count(mark) != 1:
            sys.exit("%s: expected %s exactly once" % (template, mark))
    for mark in marks:
        page = page.replace(mark, values[mark])
    return page


def write(path, page):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(page)
    print("%s: %d bytes" % (os.path.relpath(path, ROOT), len(page.encode("utf-8"))))


def main():
    with open(TEMPLATE, encoding="utf-8") as f:
        source = f.read()
    values = {
        "@BASI_SVG@": ",\n".join(json.dumps(d) for d in svg_paths(BACKGROUND)),
        "@SOUND@": "\n".join('  %s: "%s",' % (name, b64(os.path.join(ROOT, "art", "sound", fn)))
                             for name, fn in SOUNDS),
        "@CORE@": core_of(source),
    }
    write(OUTPUT, fill(TEMPLATE, ("@BASI_SVG@", "@SOUND@"), values))
    write(EMBED_OUTPUT, fill(EMBED_TEMPLATE, ("@CORE@",), values))


if __name__ == "__main__":
    main()
