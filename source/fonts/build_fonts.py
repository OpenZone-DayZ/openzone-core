#!/usr/bin/env python3
"""Rebuild the fonts of the OpenZone window in VPP into OpenZone_VPP/gui/fonts.

    python source/fonts/build_fonts.py              build what fonts.json lists
    python source/fonts/build_fonts.py --check      build in memory, compare with the files on disk
    python source/fonts/build_fonts.py --preview D  also write atlas and sample PNGs into D

The converter is the series' one and lives in the research repository
(openzone-research/source/fonts: fontbuild.py, fontkit/); this script only points it at this
repository's manifest, faces and output folder. The research repository stands beside this
one (the series' layout, README of E:\\openzone); OZ_FONT_CONVERTER names another place.

Needs Python 3.10+ with Pillow, and numpy for the distance-field files. The output is
deterministic: an unchanged manifest and unchanged TTFs give byte-identical files.
"""
import os
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CONVERTER = os.environ.get("OZ_FONT_CONVERTER") or os.path.join(
    os.path.dirname(REPO), "openzone-research", "source", "fonts")
if not os.path.isfile(os.path.join(CONVERTER, "build_fonts.py")):
    raise SystemExit("the series' font converter is not at %s (set OZ_FONT_CONVERTER)" % CONVERTER)
sys.path.insert(0, CONVERTER)

import build_fonts as series  # noqa: E402
import fontbuild  # noqa: E402

# This repository's faces, manifest and output; the converter's own code is untouched.
series.HERE = HERE
series.REPO = REPO
series.LICENCE_NOTE = (
    "The fonts in this folder (ozv_*.fnt with their ozv_*.edds atlases) are bitmap renderings\n"
    "of the typefaces below, made by source/fonts/build_fonts.py of the OpenZone Core\n"
    "repository with the converter of the OpenZone Research repository. They are Modified\n"
    "Versions under the SIL Open Font License 1.1 and do not use any Reserved Font Name. The\n"
    "original TTF files are in the OpenZone Core repository, source/fonts/.\n")

# The window's own signs, kept when the face has them and they fit the cell: the rail's
# chevrons, the status dots, the list arrows.
fontbuild.SYMBOLS = sorted(set(fontbuild.SYMBOLS) | {
    0x25B8, 0x25BE, 0x25B4, 0x25C2,     # small triangles: right, down, up, left
    0x25B6, 0x25BC, 0x25B2, 0x25C0,     # triangles
    0x2212, 0x2194, 0x21B5, 0x2261,     # minus, left-right arrow, return, three bars
    0x2264, 0x2265, 0x2260, 0x2248})    # at most, at least, not equal, about

# Two things the window's faces need that the converter does not offer, read from this
# repository's manifest and applied to what the converter built:
#   "track": N  -- N more atlas pixels after every glyph: letter spacing, which a TextWidget
#                  cannot do (the mockup spreads its small capitals by a pixel);
#   "ink": K    -- the coverage curve a * (K + 1) / (a * K + 1), the browser's "enhanced
#                  contrast": the engine draws a small size from a box-filtered mip of the
#                  atlas, a quarter lighter than a browser draws the same text (measured
#                  2026-11-13: 0.71 to 0.77 of the ink at 13 px from a 28-px cell).
_build_one = series.build_one


def build_one(o, charsets):
    built = _build_one(o, charsets)
    track = o.get("track")
    if track:
        for g in built.font.glyphs:
            g.post += track
    ink = o.get("ink")
    if ink:
        r, g2, b, a = built.atlas.split()
        lut = [min(255, int(round(255.0 * (v / 255.0) * (ink + 1.0) / ((v / 255.0) * ink + 1.0)))) for v in range(256)]
        from PIL import Image
        built.atlas = Image.merge("RGBA", (r, g2, b, a.point(lut)))
    return built


series.build_one = build_one

if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    args = sys.argv[1:]
    if "--manifest" not in args:
        args = ["--manifest", os.path.join(HERE, "fonts.json")] + args
    sys.exit(series.main(args))
