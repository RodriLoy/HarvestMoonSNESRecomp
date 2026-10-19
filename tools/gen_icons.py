#!/usr/bin/env python3
"""Item icons for the pause menu (src/hm_icons.h): 16x16 pixel art.

Original drawings, made here from simple shapes (lines, boxes, polygons)
and then given a dark outline the way SNES sprites are, so they sit
naturally on the menu's parchment. Nothing is taken from the game.
Run:  python3 tools/gen_icons.py      (writes src/hm_icons.h and a preview)
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "hm_icons.h")

C = dict(
    k=(42, 26, 14), w=(150, 92, 46), W=(204, 142, 78), s=(150, 160, 174),
    S=(222, 228, 236), d=(92, 100, 112), g=(224, 168, 46), G=(255, 228, 122),
    h=(156, 106, 20), b=(58, 120, 216), B=(156, 208, 255), n=(30, 62, 122),
    r=(216, 64, 46), R=(255, 138, 106), y=(240, 208, 64), Y=(255, 244, 160),
    e=(76, 154, 58), E=(154, 216, 106), p=(232, 210, 160), P=(192, 160, 106),
    o=(122, 74, 34), m=(242, 160, 192), v=(154, 106, 208), t=(255, 255, 255),
    c=(240, 236, 220), q=(110, 70, 150),
)


def new():
    return Image.new("RGBA", (16, 16), (0, 0, 0, 0))


def L(d, pts, col, width=1):
    d.line(pts, fill=C[col] + (255,), width=width)


def P(d, pts, col):
    d.polygon(pts, fill=C[col] + (255,))


def R(d, box, col):
    d.rectangle(box, fill=C[col] + (255,))


def E(d, box, col):
    d.ellipse(box, fill=C[col] + (255,))


def handle(d, a, b):
    L(d, [a, b], "w", 2)
    L(d, [(a[0], a[1] - 1), (b[0], b[1] - 1)], "W", 1)


def outline(im):
    src = im.copy()
    for y in range(16):
        for x in range(16):
            if src.getpixel((x, y))[3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < 16 and 0 <= yy < 16 and src.getpixel((xx, yy))[3]:
                    im.putpixel((x, y), C["k"] + (255,))
                    break
    return im


def metal(gold):
    return ("g", "G", "h") if gold else ("s", "S", "d")


def sickle(gold=False):
    m, M, dk = metal(gold)
    im = new(); d = ImageDraw.Draw(im)
    handle(d, (3, 13), (6, 10))
    d.arc((5, 1, 15, 11), 150, 330, fill=C[m] + (255,), width=3)
    d.arc((6, 2, 14, 10), 170, 300, fill=C[M] + (255,), width=1)
    return outline(im)


def hoe(gold=False):
    m, M, dk = metal(gold)
    im = new(); d = ImageDraw.Draw(im)
    handle(d, (2, 14), (11, 4))
    P(d, [(9, 2), (14, 2), (14, 6), (12, 6)], m)
    L(d, [(10, 2), (13, 2)], M)
    L(d, [(13, 3), (13, 6)], dk)
    return outline(im)


def hammer(gold=False):
    m, M, dk = metal(gold)
    im = new(); d = ImageDraw.Draw(im)
    handle(d, (3, 14), (9, 7))
    R(d, (6, 2, 14, 6), m)
    L(d, [(6, 2), (14, 2)], M)
    R(d, (13, 2, 14, 6), dk)
    return outline(im)


def ax(gold=False):
    m, M, dk = metal(gold)
    im = new(); d = ImageDraw.Draw(im)
    handle(d, (3, 14), (10, 4))
    P(d, [(8, 2), (12, 1), (15, 4), (14, 9), (10, 7)], m)
    L(d, [(12, 2), (14, 4)], M)
    L(d, [(14, 5), (14, 8)], M)
    return outline(im)


def can(gold=False):
    a, A, dk = ("g", "G", "h") if gold else ("b", "B", "n")
    im = new(); d = ImageDraw.Draw(im)
    R(d, (2, 6, 10, 13), a)
    L(d, [(3, 7), (3, 12)], A)
    L(d, [(10, 10), (14, 4)], a, 2)
    R(d, (13, 2, 15, 4), dk)
    d.arc((2, 2, 9, 9), 180, 360, fill=C[dk] + (255,), width=1)
    return outline(im)


def seedbag(col, col2):
    im = new(); d = ImageDraw.Draw(im)
    P(d, [(4, 5), (11, 5), (13, 14), (2, 14)], "p")
    L(d, [(11, 6), (12, 13)], "P")
    R(d, (5, 3, 10, 4), "P")
    E(d, (5, 8, 10, 12), col)
    d.point((6, 9), fill=C[col2] + (255,))
    return outline(im)


def milker():
    im = new(); d = ImageDraw.Draw(im)
    P(d, [(3, 6), (12, 6), (11, 14), (4, 14)], "s")
    R(d, (4, 7, 11, 8), "c")
    L(d, [(4, 6), (11, 6)], "S")
    for x in (5, 8, 11):
        L(d, [(x, 5), (x - 1, 1)], "d")
    return outline(im)


def brush():
    im = new(); d = ImageDraw.Draw(im)
    R(d, (2, 7, 13, 10), "W")
    L(d, [(2, 7), (13, 7)], "w")
    for x in range(3, 13, 2):
        L(d, [(x, 11), (x, 13)], "y")
    return outline(im)


def bell():
    im = new(); d = ImageDraw.Draw(im)
    P(d, [(6, 3), (9, 3), (12, 11), (3, 11)], "g")
    L(d, [(6, 4), (5, 10)], "G")
    R(d, (2, 11, 13, 12), "h")
    E(d, (6, 12, 9, 15), "h")
    R(d, (7, 1, 8, 2), "h")
    return outline(im)


def bottle(col, light):
    im = new(); d = ImageDraw.Draw(im)
    R(d, (6, 1, 9, 3), "o")
    R(d, (5, 4, 10, 5), col)
    R(d, (3, 6, 12, 14), col)
    R(d, (5, 8, 10, 11), "c")
    L(d, [(4, 7), (4, 13)], light)
    return outline(im)


def potion():
    im = new(); d = ImageDraw.Draw(im)
    R(d, (6, 1, 9, 2), "o")
    R(d, (6, 3, 9, 5), "B")
    E(d, (2, 5, 13, 15), "m")
    E(d, (4, 7, 7, 10), "t")
    return outline(im)


def paint():
    im = new(); d = ImageDraw.Draw(im)
    R(d, (3, 5, 12, 14), "s")
    L(d, [(4, 6), (4, 13)], "S")
    E(d, (3, 3, 12, 7), "r")
    R(d, (9, 6, 10, 10), "r")
    d.arc((3, 0, 12, 8), 180, 360, fill=C["d"] + (255,), width=1)
    return outline(im)


def sprinkler():
    im = new(); d = ImageDraw.Draw(im)
    R(d, (7, 8, 8, 15), "d")
    E(d, (4, 5, 11, 9), "s")
    for p in ((2, 3), (13, 3), (4, 1), (11, 1), (1, 6), (14, 6)):
        d.point(p, fill=C["B"] + (255,))
        d.point((p[0], p[1] + 1), fill=C["b"] + (255,))
    return outline(im)


def beans():
    im = new(); d = ImageDraw.Draw(im)
    for (x, y) in ((2, 7), (7, 3), (8, 9)):
        E(d, (x, y, x + 5, y + 4), "e")
        d.point((x + 1, y + 1), fill=C["E"] + (255,))
    return outline(im)


def diamond():
    im = new(); d = ImageDraw.Draw(im)
    P(d, [(4, 3), (11, 3), (14, 7), (7, 14), (1, 7)], "b")
    P(d, [(4, 3), (7, 3), (6, 7), (1, 7)], "B")
    L(d, [(1, 7), (14, 7)], "n")
    return outline(im)


def feather():
    im = new(); d = ImageDraw.Draw(im)
    P(d, [(12, 1), (14, 3), (6, 12), (3, 12), (4, 9)], "b")
    L(d, [(13, 2), (3, 13)], "B")
    L(d, [(4, 11), (1, 14)], "c")
    return outline(im)


def feed(col, col2):
    im = new(); d = ImageDraw.Draw(im)
    R(d, (3, 4, 12, 14), "P")
    L(d, [(4, 5), (4, 13)], "p")
    R(d, (3, 3, 12, 4), "o")
    E(d, (5, 7, 10, 12), col)
    d.point((7, 9), fill=C[col2] + (255,))
    return outline(im)


# Game item ids ($0921) -> icon.
ICONS = [
    (0x01, sickle()), (0x02, hoe()), (0x03, hammer()), (0x04, ax()),
    (0x05, seedbag("y", "Y")), (0x06, seedbag("r", "R")), (0x07, seedbag("o", "W")),
    (0x08, seedbag("v", "t")), (0x09, bottle("e", "E")), (0x0A, potion()),
    (0x0B, bell()), (0x0C, seedbag("e", "E")), (0x0D, paint()), (0x0E, milker()),
    (0x0F, brush()), (0x10, can()), (0x11, sickle(True)), (0x12, hoe(True)),
    (0x13, hammer(True)), (0x14, ax(True)), (0x15, sprinkler()), (0x16, beans()),
    (0x17, diamond()), (0x18, feather()), (0x19, feed("y", "Y")), (0x1A, feed("e", "E")),
]


def main():
    out = ["/* Generated by tools/gen_icons.py -- do not edit. Original 16x16 item",
           " * icons, ARGB (alpha 0 = transparent), indexed by the game's item id. */",
           "#pragma once", "#include <stdint.h>", "",
           "#define HM_ICON_COUNT 0x1B",
           "static const uint32_t kHmIcons[HM_ICON_COUNT][256] = {"]
    by_id = dict(ICONS)
    sheet = Image.new("RGBA", (16 * len(ICONS), 16), (244, 226, 180, 255))
    for i in range(0x1B):
        im = by_id.get(i)
        if im is None:
            out.append("  {0},")
            continue
        px = []
        for y in range(16):
            for x in range(16):
                r, g, b, a = im.getpixel((x, y))
                px.append("0x%08X" % ((0xFF << 24 | r << 16 | g << 8 | b) if a else 0))
        out.append("  { /* item 0x%02X */" % i)
        for y in range(16):
            out.append("    " + ",".join(px[y * 16:(y + 1) * 16]) + ",")
        out.append("  },")
    out.append("};")
    for k, (i, im) in enumerate(ICONS):
        sheet.paste(im, (k * 16, 0), im)
    sheet.resize((sheet.width * 4, 64), Image.NEAREST).save(
        os.path.join(HERE, "..", "build", "icons_preview.png"))
    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
