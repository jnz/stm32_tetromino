#!/usr/bin/env python3
"""Generate web/tetromino.html, the standalone browser page, from
web/src/tetromino.html and the artwork and the music in art/.

    python tools/gen_web.py               # from the repository root

The page is one file that works offline and from anywhere, so the
background goes into it as path data and the music as base64. The template keeps
placeholders instead, which keeps it small enough to work on:

  @BASI_SVG@   art/basi.svg, the background behind the field, as the
               list of its path data
  @SOUND@      the entries of the SOUND table, one per file of art/sound/
               (the music of the browser version at
               https://zwiener.org/tetromino.html; the sound effects
               are synthesized in the page)

Run it after every change to the template or to the files in art/. The
output is committed, so opening the page needs neither Python nor a
build.
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


def main():
    with open(TEMPLATE, encoding="utf-8") as f:
        page = f.read()
    for mark in ("@BASI_SVG@", "@SOUND@"):
        if page.count(mark) != 1:
            sys.exit("%s: expected %s exactly once" % (TEMPLATE, mark))

    sound = "\n".join('  %s: "%s",' % (name, b64(os.path.join(ROOT, "art", "sound", fn)))
                      for name, fn in SOUNDS)
    basi = ",\n".join(json.dumps(d) for d in svg_paths(BACKGROUND))
    page = page.replace("@BASI_SVG@", basi).replace("@SOUND@", sound)

    with open(OUTPUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(page)
    print("%s: %d bytes" % (os.path.relpath(OUTPUT, ROOT), len(page.encode("utf-8"))))


if __name__ == "__main__":
    main()
