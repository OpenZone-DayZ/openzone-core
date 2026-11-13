#!/usr/bin/env python3
"""Draw the small signs of the OpenZone window in VPP as white PNGs (source/icons/png).

Fira Sans, the window's face, has no ● ▸ ▾ ✓: these are drawn here and packed into the
imageset OpenZone_VPP/gui/imagesets/ozv_ui (white on clear, tinted by the widget's colour):

    python source/icons/make_icons.py
    python <dayz-modding skill>/scripts/build_imageset.py source/icons/png --name ozv_ui \
        --prefix OpenZone_VPP/gui/imagesets --out OpenZone_VPP/gui/imagesets

Needs Pillow. Each sign is drawn four times larger and reduced, 64 x 64 a cell.
"""
import math
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "png")
N = 64          # the cell
SS = 4          # drawn at 4x
W = (255, 255, 255, 255)


def canvas():
    im = Image.new("RGBA", (N * SS, N * SS), (255, 255, 255, 0))
    return im, ImageDraw.Draw(im)


def save(im, name):
    os.makedirs(OUT, exist_ok=True)
    im.resize((N, N), Image.LANCZOS).save(os.path.join(OUT, name + ".png"))


def s(v):
    return int(round(v * SS))


def dot():
    im, d = canvas()
    d.ellipse([s(8), s(8), s(56), s(56)], fill=W)
    save(im, "dot")


def ring():
    im, d = canvas()
    d.ellipse([s(8), s(8), s(56), s(56)], outline=W, width=s(7))
    save(im, "ring")


def tri_right():
    im, d = canvas()
    d.polygon([(s(20), s(12)), (s(48), s(32)), (s(20), s(52))], fill=W)
    save(im, "tri_right")


def tri_down():
    im, d = canvas()
    d.polygon([(s(12), s(20)), (s(52), s(20)), (s(32), s(48))], fill=W)
    save(im, "tri_down")


def stroke(d, pts, width):
    d.line([(s(x), s(y)) for x, y in pts], fill=W, width=s(width), joint="curve")
    r = s(width) / 2
    for x, y in (pts[0], pts[-1]):
        d.ellipse([s(x) - r, s(y) - r, s(x) + r, s(y) + r], fill=W)


def check():
    im, d = canvas()
    stroke(d, [(12, 33), (26, 47), (52, 17)], 8)
    save(im, "check")


def cross():
    im, d = canvas()
    stroke(d, [(16, 16), (48, 48)], 6)
    stroke(d, [(48, 16), (16, 48)], 6)
    save(im, "cross")


def caret():
    im, d = canvas()
    stroke(d, [(14, 24), (32, 42), (50, 24)], 7)
    save(im, "caret")


def chevron():
    im, d = canvas()
    stroke(d, [(24, 14), (42, 32), (24, 50)], 7)
    save(im, "chevron")


def search():
    im, d = canvas()
    d.ellipse([s(9), s(9), s(43), s(43)], outline=W, width=s(6))
    stroke(d, [(40, 40), (54, 54)], 6)
    save(im, "search")


def square():
    im, d = canvas()
    d.rectangle([s(6), s(6), s(58), s(58)], fill=W)
    save(im, "square")


def excl():
    """A report line taken over changed: the "!" of the TraderPlus import's board."""
    im, d = canvas()
    d.rounded_rectangle([s(26), s(6), s(38), s(42)], radius=s(6), fill=W)
    d.ellipse([s(25), s(46), s(39), s(60)], fill=W)
    save(im, "excl")


def dash():
    """A report line left out: the board's "–"."""
    im, d = canvas()
    d.rounded_rectangle([s(12), s(27), s(52), s(37)], radius=s(5), fill=W)
    save(im, "dash")


# The series' mark on VPP's toolbar button: the OZ-COM mark the radios carry
# (openzone-radio, models/assets/_kit/texkit.py, brand_mark_art): an open ring, a
# transmitter dot at its centre and a wave out through the gap. In its own colours, not
# white: VPP draws its button image white at 0.784 alpha, so the colours stay.
# VPP stretches a button's image over a box of 70 x 56 px on a 16:9 screen (its own icons
# are drawn so): the mark is centred in a canvas of those proportions, or its ring would
# come out an oval (measured on the stand, 1920 x 1080, 2026-11-13).
MARK = (160, 128)
MARK_ACCENT = (79, 181, 232, 255)   # OZ_Palette ACCENT, the radios' wave and dot


def round_arc(d, cx, cy, r, a0, a1, w, fill):
    """An arc w thick with round ends; r is its outer radius, angles as PIL's (from three
    o'clock, clockwise)."""
    d.arc((cx - r, cy - r, cx + r, cy + r), a0, a1, fill=fill, width=int(round(w)))
    for a in (a0, a1):
        rr = r - w / 2
        x, y = cx + rr * math.cos(math.radians(a)), cy + rr * math.sin(math.radians(a))
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=fill)


def oz_mark():
    mw, mh = MARK
    n = mh * SS
    im = Image.new("RGBA", (n * 2, n * 2), (255, 255, 255, 0))
    d = ImageDraw.Draw(im)
    r = n * 0.37
    w = r * 0.30
    c = n
    round_arc(d, c, c, r, -12, 282, w, W)                     # the gap at the top right
    dot = r * 0.34
    d.ellipse((c - dot, c - dot, c + dot, c + dot), fill=MARK_ACCENT)
    round_arc(d, c, c, r * 1.62, 292, 338, w * 0.85, MARK_ACCENT)
    # the drawing's own box (the wave makes it lean to the top right), centred in a canvas
    # of the button's proportions with a little air around it
    x0, y0, x1, y1 = im.getbbox()
    art = im.crop((x0, y0, x1, y1))
    ch = max(art.height / 0.92, art.width / 0.92 * mh / mw)
    cw = ch * mw / mh
    out = Image.new("RGBA", (int(cw), int(ch)), (255, 255, 255, 0))
    out.alpha_composite(art, (int((cw - art.width) / 2), int((ch - art.height) / 2)))
    os.makedirs(OUT, exist_ok=True)
    out.resize((mw, mh), Image.LANCZOS).save(os.path.join(OUT, "oz_mark.png"))


if __name__ == "__main__":
    for f in (dot, ring, tri_right, tri_down, check, cross, caret, chevron, search, square, excl, dash,
              oz_mark):
        f()
    print("icons in", OUT, sorted(os.listdir(OUT)))
