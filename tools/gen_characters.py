#!/usr/bin/env python3
"""Generates the alternate playable vessel sprites (24x24, facing +x) to match
the hand-authored `player.png` wizard in detail. The initial "player" vessel is
intentionally NOT regenerated here.

    character_2.png  Dust Gunslinger  (cowboy + revolver)
    character_3.png  Iron Sentinel    (knight + sword)
    character_4.png  Shadow Rogue     (hooded + dagger)
    character_5.png  Wild Ranger      (archer + bow)
    character_6.png  Unknown Vessel   (locked shadow silhouette)

    python tools/gen_characters.py assets
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gen_hub_tiles import Canvas, write_png  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"
N = 24

# Dash rift/spark accent per vessel (index 2..6); reused by gen_dash_sprites.
ACCENTS = {
    "gunslinger": (216, 170, 90),
    "knight":     (120, 165, 235),
    "rogue":      (200, 95, 205),
    "ranger":     (110, 195, 95),
    "unknown":    (178, 120, 240),
}


def P(c, x, y, col):
    a = col[3] if len(col) > 3 else 255
    c.blend(x, y, col[0], col[1], col[2], a)


def box(c, x, y, w, h, col):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            P(c, xx, yy, col)


def hline(c, x0, x1, y, col):
    for x in range(x0, x1 + 1):
        P(c, x, y, col)


def vline(c, x, y0, y1, col):
    for y in range(y0, y1 + 1):
        P(c, x, y, col)


def outline(c, col=(24, 18, 26, 255)):
    """Wrap opaque pixels with a dark border for a crisp pixel-art silhouette."""
    todo = []
    for y in range(c.h):
        for x in range(c.w):
            if c.buf[(y * c.w + x) * 4 + 3] > 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1),
                           (1, 1), (1, -1), (-1, 1), (-1, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < c.w and 0 <= ny < c.h and \
                        c.buf[(ny * c.w + nx) * 4 + 3] > 180:
                    todo.append((x, y))
                    break
    for (x, y) in todo:
        P(c, x, y, col)


# ----------------------------------------------------------------- gunslinger
def gunslinger():
    c = Canvas(N, N)
    hat = (120, 86, 52); hat_d = (86, 60, 34); band = (70, 48, 28)
    skin = (224, 176, 134); skin_d = (190, 142, 104)
    bandana = (182, 54, 46); bandana_d = (140, 38, 34)
    coat = (176, 142, 96); coat_d = (132, 104, 66); coat_h = (206, 176, 126)
    pants = (74, 64, 80); boot = (58, 44, 34)
    gun = (78, 80, 94); gun_h = (140, 142, 156)
    # hat: crown + wide brim
    box(c, 7, 1, 7, 4, hat)
    hline(c, 7, 13, 1, hat_d)
    hline(c, 5, 16, 5, hat)        # brim
    hline(c, 5, 16, 6, hat_d)
    hline(c, 7, 13, 4, band)       # hat band
    # face
    box(c, 8, 6, 6, 4, skin)
    vline(c, 13, 6, 9, skin_d)
    P(c, 9, 7, (40, 30, 26))       # eye
    # neckerchief
    hline(c, 7, 14, 10, bandana)
    hline(c, 8, 13, 11, bandana_d)
    # coat / torso
    box(c, 6, 11, 9, 7, coat)
    vline(c, 6, 11, 17, coat_d)
    vline(c, 10, 12, 16, coat_d)   # coat seam
    hline(c, 6, 14, 12, coat_h)    # shoulder light
    # gun belt + buckle
    hline(c, 6, 14, 16, band)
    P(c, 10, 16, (210, 180, 90))
    # legs / boots
    box(c, 7, 18, 3, 4, pants)
    box(c, 11, 18, 3, 4, pants)
    hline(c, 7, 9, 21, boot)
    hline(c, 11, 13, 21, boot)
    # right arm + revolver pointing +x
    box(c, 14, 12, 3, 3, coat)
    box(c, 16, 12, 3, 3, gun)      # cylinder/grip block
    hline(c, 18, 21, 12, gun)      # barrel
    hline(c, 18, 21, 13, gun_h)
    P(c, 16, 15, gun_h)            # grip butt
    P(c, 17, 11, gun_h)            # hammer
    outline(c, (30, 22, 18, 255))
    return c


# ---------------------------------------------------------------------- knight
def knight():
    c = Canvas(N, N)
    steel = (150, 162, 184); steel_d = (104, 116, 140); steel_h = (200, 210, 226)
    plume = (210, 70, 78); plume_d = (160, 46, 54)
    gold = (216, 178, 92)
    blade = (212, 220, 232); blade_d = (150, 162, 182)
    # plume
    vline(c, 10, 0, 3, plume); vline(c, 11, 0, 2, plume_d)
    # helmet
    box(c, 7, 3, 7, 6, steel)
    hline(c, 7, 13, 3, steel_h)
    box(c, 8, 5, 5, 2, (40, 44, 56))   # visor slit
    P(c, 9, 5, (120, 200, 255))        # eye glint
    # body armor
    box(c, 6, 9, 9, 9, steel)
    vline(c, 6, 9, 17, steel_d)
    vline(c, 14, 9, 17, steel_d)
    hline(c, 6, 14, 9, steel_h)        # pauldron light
    box(c, 9, 11, 3, 4, steel_d)       # chest emblem recess
    P(c, 10, 12, gold)
    # legs
    box(c, 7, 18, 3, 4, steel_d)
    box(c, 11, 18, 3, 4, steel_d)
    # sword held upright on the right
    vline(c, 18, 3, 14, blade)
    vline(c, 19, 3, 14, blade_d)
    hline(c, 16, 21, 14, gold)         # crossguard
    box(c, 17, 15, 2, 3, (96, 64, 36)) # grip
    P(c, 18, 2, blade_d)               # tip
    outline(c, (26, 30, 40, 255))
    return c


# ----------------------------------------------------------------------- rogue
def rogue():
    c = Canvas(N, N)
    hood = (78, 60, 104); hood_d = (52, 40, 74); hood_h = (110, 88, 144)
    cloak = (66, 52, 92); cloak_d = (44, 34, 64)
    face = (40, 32, 50)
    blade = (200, 206, 220); blade_d = (140, 146, 164)
    # hood
    box(c, 7, 2, 8, 6, hood)
    hline(c, 7, 14, 2, hood_d)
    hline(c, 6, 15, 7, hood_h)
    box(c, 9, 5, 5, 3, face)           # shadowed face
    P(c, 10, 6, (150, 230, 220))       # glowing eye
    P(c, 12, 6, (150, 230, 220))
    # cloaked body
    box(c, 6, 8, 9, 10, cloak)
    vline(c, 6, 8, 17, cloak_d)
    vline(c, 14, 8, 17, cloak_d)
    vline(c, 9, 9, 17, cloak_d)        # fold
    hline(c, 6, 14, 8, hood_h)
    # legs
    box(c, 7, 18, 3, 4, cloak_d)
    box(c, 11, 18, 3, 4, cloak_d)
    # reverse-grip dagger in the right hand
    box(c, 14, 11, 2, 3, hood_d)       # hand
    vline(c, 16, 10, 16, blade)
    vline(c, 17, 11, 15, blade_d)
    hline(c, 15, 17, 13, (120, 90, 60))  # guard
    outline(c, (22, 16, 30, 255))
    return c


# ---------------------------------------------------------------------- ranger
def ranger():
    c = Canvas(N, N)
    cap = (66, 110, 60); cap_d = (44, 78, 42); cap_h = (96, 150, 86)
    feather = (220, 200, 96)
    skin = (224, 176, 134); skin_d = (190, 142, 104)
    tunic = (96, 132, 74); tunic_d = (66, 96, 52); tunic_h = (130, 170, 100)
    belt = (96, 66, 40); pants = (78, 64, 46)
    bow = (132, 92, 52); bow_h = (176, 130, 80); string = (220, 224, 210)
    # cap + feather
    box(c, 7, 2, 7, 4, cap)
    hline(c, 7, 13, 2, cap_h)
    hline(c, 7, 13, 5, cap_d)
    vline(c, 14, 0, 3, feather); P(c, 15, 1, feather)
    # face
    box(c, 8, 6, 6, 4, skin)
    vline(c, 13, 6, 9, skin_d)
    P(c, 10, 7, (40, 34, 30))
    # tunic
    box(c, 6, 10, 9, 8, tunic)
    vline(c, 6, 10, 17, tunic_d)
    hline(c, 6, 14, 10, tunic_h)
    hline(c, 6, 14, 16, belt)
    # quiver strap
    for i in range(6):
        P(c, 7 + i, 11 + i, tunic_d)
    # legs
    box(c, 7, 18, 3, 4, pants)
    box(c, 11, 18, 3, 4, pants)
    # bow on the right, drawn vertical curve with string
    for y in range(4, 19):
        dx = int(2.4 * (1 - ((y - 11) / 7.0) ** 2))
        if dx < 0:
            dx = 0
        P(c, 18 + dx, y, bow)
        P(c, 18 + dx + 1, y, bow_h)
    vline(c, 18, 5, 18, string)         # string
    P(c, 15, 11, (120, 96, 60))         # arrow nock at hand
    hline(c, 15, 18, 11, (150, 120, 78))
    outline(c, (26, 34, 22, 255))
    return c


# --------------------------------------------------------------------- unknown
def unknown():
    c = Canvas(N, N)
    # A deliberately obscured locked vessel: a dark hooded wraith with a faint
    # violet rim and cold glowing eyes.
    dark = (34, 30, 46); dark_d = (20, 18, 30); rim = (120, 92, 168)
    box(c, 7, 2, 8, 7, dark)            # hood
    hline(c, 7, 14, 2, dark_d)
    box(c, 6, 9, 10, 11, dark)          # cloak
    vline(c, 6, 9, 19, dark_d)
    vline(c, 15, 9, 19, dark_d)
    box(c, 7, 20, 3, 2, dark_d)
    box(c, 11, 20, 3, 2, dark_d)
    # rim light down the left edge + hood crown
    for y in range(3, 19):
        P(c, 6, y, rim if y % 2 == 0 else dark_d)
    hline(c, 8, 13, 2, rim)
    # cold eyes
    P(c, 9, 6, (170, 150, 240)); P(c, 12, 6, (170, 150, 240))
    P(c, 9, 6, (210, 200, 255))
    outline(c, (14, 12, 22, 255))
    return c


BUILDERS = {
    "character_2.png": ("gunslinger", gunslinger),
    "character_3.png": ("knight", knight),
    "character_4.png": ("rogue", rogue),
    "character_5.png": ("ranger", ranger),
    "character_6.png": ("unknown", unknown),
}


def sprite(kind):
    for _, (k, fn) in BUILDERS.items():
        if k == kind:
            return fn()
    raise KeyError(kind)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, (_, fn) in BUILDERS.items():
        write_png(os.path.join(OUT, name), fn())


if __name__ == "__main__":
    main()
