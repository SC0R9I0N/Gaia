#!/usr/bin/env python3
"""Generates the player teleport/dash sprite sheets (pure stdlib, no PIL).

Each sheet is 120x24 = five 24x24 frames that read as a short-range blink:
a wizard silhouette dematerializes into a bright vertical arcane rift with
rising sparks (frames 0-2), then rematerializes (frames 3-4). Each of the six
playable vessels gets the same effect themed to its own accent colour.

    python tools/gen_dash_sprites.py assets
"""
import os
import sys
import math

sys.path.insert(0, os.path.dirname(__file__))
from gen_hub_tiles import Canvas, write_png, clamp  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"

FRAME = 24
FRAMES = 5

# Per-vessel accent colour (sampled from each character sprite, then saturated).
# Order matches dash.png, dash_2.png ... dash_6.png.
ACCENTS = [
    (95, 165, 255),    # Astral Wizard  — azure
    (255, 115, 55),    # Ember Witch    — ember orange
    (125, 210, 95),    # Moss Hermit    — moss green
    (90, 215, 245),    # Storm Adept    — electric cyan
    (205, 212, 255),   # Moon Seer      — silver lavender
    (185, 125, 240),   # Unknown Vessel — spectral violet
]

# presence = how solid the body is; rift = how intense the portal seam is.
SCHEDULE = [
    (0.82, 0.22),   # frame 0: starting to dissolve
    (0.45, 0.62),   # frame 1: half gone, rift swelling
    (0.10, 1.00),   # frame 2: peak blink — almost pure energy
    (0.45, 0.62),   # frame 3: reforming
    (0.86, 0.18),   # frame 4: nearly solid, glow fading
]


def scale(c, f):
    return (clamp(c[0] * f), clamp(c[1] * f), clamp(c[2] * f))


def mix(a, b, t):
    return (clamp(a[0] + (b[0] - a[0]) * t),
            clamp(a[1] + (b[1] - a[1]) * t),
            clamp(a[2] + (b[2] - a[2]) * t))


def wizard_pixels(accent):
    """Base 24x24 silhouette as {(x,y): (r,g,b)} — drawn in accent tones."""
    dark = scale(accent, 0.34)
    body = scale(accent, 0.64)
    hi = mix(accent, (255, 255, 255), 0.55)
    staff = scale(accent, 0.5)
    px = {}

    def setp(x, y, c):
        if 0 <= x < FRAME and 0 <= y < FRAME:
            px[(x, y)] = c

    cx = 12
    # pointed hat
    for y in range(1, 9):
        hw = int((y - 1) / 7.0 * 7)
        for x in range(cx - hw, cx + hw + 1):
            setp(x, y, body if (cx - hw < x < cx + hw) else dark)
    setp(cx, 1, hi)
    setp(cx, 2, hi)
    # brim
    for x in range(4, 21):
        setp(x, 8, body)
        setp(x, 9, dark)
    # face band
    for y in range(10, 13):
        for x in range(8, 16):
            setp(x, y, hi if y == 10 else body)
    # robe (widening trapezoid)
    for y in range(13, 22):
        t = (y - 13) / 8.0
        x0 = int(8 - t * 2)
        x1 = int(16 + t * 2)
        for x in range(x0, x1 + 1):
            setp(x, y, dark if (x == x0 or x == x1) else body)
    for x in range(5, 19):
        setp(x, 22, dark)
    # a small staff with a bright tip
    for y in range(6, 22):
        setp(19, y, staff)
    setp(19, 6, hi)
    setp(18, 6, hi)
    return px


def render_frame(c, ox, accent, base, presence, rift, frame):
    cx = 12
    white = (255, 255, 255)

    # Ground glow pooling under the blink.
    if rift > 0.05:
        c.glow(ox + cx, 20, 9,
               (accent[0], accent[1], accent[2], int(70 * rift)), 2.0)

    # Vertical arcane rift: a bright seam, white at the core, accent at edges.
    half = 1.0 + rift * 4.5
    for y in range(FRAME):
        vint = math.sin(math.pi * (y + 0.5) / FRAME)
        x = int(cx - half - 1)
        while x <= int(cx + half + 1):
            hf = 1.0 - abs(x - cx) / (half + 1.0)
            if hf > 0.0:
                a = int(235 * rift * vint * hf)
                if a > 0:
                    col = mix(accent, white, hf)
                    c.blend(ox + x, y, col[0], col[1], col[2], a)
            x += 1
    if rift > 0.3:
        for y in range(FRAME):
            vint = math.sin(math.pi * (y + 0.5) / FRAME)
            c.blend(ox + cx, y, 255, 255, 255, int(205 * rift * vint))
        # a horizontal flash through the seam at the peak of the blink
        if rift > 0.9:
            for x in range(FRAME):
                hf = 1.0 - abs(x - cx) / 12.0
                if hf > 0:
                    c.blend(ox + x, 12, 255, 255, 255, int(150 * hf))

    # Body: dispersing outward into the seam and speckling away as it fades.
    spread = 1.0 - presence
    for (x, y), col in base.items():
        dx = round((x - cx) * spread * 1.1)
        dy = round((y - 12) * spread * 0.25)
        gate = (x * 3 + y * 5 + frame * 7) % 7
        if spread > 0.12 and gate < spread * 6.0:
            continue
        lx, ly = x + dx, y + dy
        if lx < 0 or lx >= FRAME or ly < 0 or ly >= FRAME:
            continue
        a = int(235 * presence)
        if a > 0:
            c.blend(ox + lx, ly, col[0], col[1], col[2], a)

    # Rising sparks — more of them mid-blink. Deterministic per (frame, k).
    pcount = int(9 * rift)
    for k in range(pcount):
        seed = k * 13 + frame * 29
        pxp = cx + ((seed * 7) % 11) - 5
        pyp = 3 + ((seed * 5) % 17)
        br = 150 + (seed % 90)
        if 0 <= pxp < FRAME and 0 <= pyp < FRAME:
            c.blend(ox + pxp, pyp, 255, 255, 255, min(255, br))
            c.blend(ox + pxp + 1, pyp, accent[0], accent[1], accent[2], br // 2)
            c.blend(ox + pxp, pyp + 1, accent[0], accent[1], accent[2], br // 2)


def silhouette_from_sprite(spr, accent):
    """Recolour an archetype's 24x24 sprite into accent-toned 'energy' pixels so
    its blink dissolves the actual character shape, not a generic wizard."""
    px = {}
    for y in range(spr.h):
        for x in range(spr.w):
            i = (y * spr.w + x) * 4
            if spr.buf[i + 3] < 60:
                continue
            bri = (spr.buf[i] + spr.buf[i + 1] + spr.buf[i + 2]) / 765.0
            col = scale(accent, 0.32 + bri * 1.0)
            if bri > 0.78:
                col = mix(col, (255, 255, 255), 0.45)
            px[(x, y)] = col
    return px


def make_sheet(accent, base):
    c = Canvas(FRAME * FRAMES, FRAME)
    for f in range(FRAMES):
        presence, rift = SCHEDULE[f]
        render_frame(c, f * FRAME, accent, base, presence, rift, f)
    return c


def main():
    os.makedirs(OUT, exist_ok=True)
    # The initial "player" vessel (Astral Wizard) keeps its wizard-silhouette
    # blink, regenerated identically — it is intentionally never restyled.
    azure = (95, 165, 255)
    write_png(os.path.join(OUT, "dash.png"),
              make_sheet(azure, wizard_pixels(azure)))

    # Alternate vessels dissolve their OWN archetype silhouette (gen_characters).
    import gen_characters as GC
    roster = [
        ("dash_2.png", "gunslinger"), ("dash_3.png", "knight"),
        ("dash_4.png", "rogue"), ("dash_5.png", "ranger"),
        ("dash_6.png", "unknown"),
    ]
    for name, kind in roster:
        accent = GC.ACCENTS[kind]
        base = silhouette_from_sprite(GC.sprite(kind), accent)
        write_png(os.path.join(OUT, name), make_sheet(accent, base))


if __name__ == "__main__":
    main()
