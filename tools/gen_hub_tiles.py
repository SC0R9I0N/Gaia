#!/usr/bin/env python3
"""Generates the hub sandstone tileset PNGs (pure stdlib, no PIL).

Outputs into assets/:
  hub_floor.png    96x96  seamless sandstone brick floor
  hub_wall.png     64x64  seamless sandstone brick wall
  hub_lantern.png  512x64 animated wall lantern, 8 square frames
  hub_pillar.png   64x144 fluted sandstone column
  hub_urn.png      48x56  sandstone urn decoration
  hub_banner.png   48x96  hanging banner decoration
"""
import zlib, struct, math, random, os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"


def clamp(v, lo=0, hi=255):
    return lo if v < lo else hi if v > hi else int(v)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        # flat RGBA buffer
        self.buf = bytearray(w * h * 4)

    def _idx(self, x, y):
        return (y * self.w + x) * 4

    def blend(self, x, y, r, g, b, a, wrap=False):
        if wrap:
            x %= self.w
            y %= self.h
        elif x < 0 or x >= self.w or y < 0 or y >= self.h:
            return
        if a <= 0:
            return
        i = self._idx(x, y)
        sa = a / 255.0
        dr, dg, db, da = self.buf[i], self.buf[i+1], self.buf[i+2], self.buf[i+3]
        daf = da / 255.0
        oa = sa + daf * (1 - sa)
        if oa <= 0:
            self.buf[i:i+4] = bytes(4)
            return
        self.buf[i]   = clamp((r * sa + dr * daf * (1 - sa)) / oa)
        self.buf[i+1] = clamp((g * sa + dg * daf * (1 - sa)) / oa)
        self.buf[i+2] = clamp((b * sa + db * daf * (1 - sa)) / oa)
        self.buf[i+3] = clamp(oa * 255)

    def rect(self, x, y, w, h, col, wrap=False):
        r, g, b, a = col
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.blend(xx, yy, r, g, b, a, wrap)

    def hline(self, x0, x1, y, col, wrap=False):
        r, g, b, a = col
        for xx in range(x0, x1):
            self.blend(xx, y, r, g, b, a, wrap)

    def vline(self, x, y0, y1, col, wrap=False):
        r, g, b, a = col
        for yy in range(y0, y1):
            self.blend(x, yy, r, g, b, a, wrap)

    def disc(self, cx, cy, rx, ry, col, soft=0.0):
        r, g, b, a = col
        for yy in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for xx in range(int(cx - rx) - 1, int(cx + rx) + 2):
                nx = (xx + 0.5 - cx) / rx
                ny = (yy + 0.5 - cy) / ry
                d = nx * nx + ny * ny
                if d <= 1.0:
                    aa = a
                    if soft > 0:
                        edge = 1.0 - d
                        aa = a * min(1.0, edge / soft) if edge < soft else a
                    self.blend(xx, yy, r, g, b, aa)

    def glow(self, cx, cy, radius, col, power=2.0):
        r, g, b, a = col
        rad = int(radius) + 1
        for yy in range(int(cy - rad), int(cy + rad) + 1):
            for xx in range(int(cx - rad), int(cx + rad) + 1):
                d = math.hypot(xx + 0.5 - cx, yy + 0.5 - cy) / radius
                if d < 1.0:
                    self.blend(xx, yy, r, g, b, a * ((1.0 - d) ** power))

    def rows(self):
        out = []
        stride = self.w * 4
        for y in range(self.h):
            out.append(bytes(self.buf[y * stride:(y + 1) * stride]))
        return out


def write_png(path, canvas):
    def chunk(typ, data):
        c = typ + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = bytearray()
    for row in canvas.rows():
        raw.append(0)
        raw.extend(row)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", canvas.w, canvas.h, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))
    print("wrote", path, f"{canvas.w}x{canvas.h}")


def shade(col, d):
    return (clamp(col[0] + d), clamp(col[1] + d), clamp(col[2] + d), 255)


# ---------------------------------------------------------------- brick tiles
def brick_tile(w, h, brick_w, brick_h, mortar, base, seed,
               bevel_hi=24, bevel_lo=-30, tone=14, wear=0.06):
    """Seamless running-bond sandstone bricks (wraps at edges)."""
    c = Canvas(w, h)
    rng = random.Random(seed)
    mort = shade(base, -52)
    # mortar background with grain
    for y in range(h):
        for x in range(w):
            n = rng.randint(-8, 8)
            c.blend(x, y, mort[0] + n, mort[1] + n, mort[2] + n, 255)
    rows = h // brick_h
    for row in range(rows):
        y0 = row * brick_h
        offset = (brick_w // 2) if (row % 2) else 0
        x = offset - brick_w
        while x < w + brick_w:
            bx, by = x + mortar, y0 + mortar
            bw, bh = brick_w - mortar, brick_h - mortar
            t = rng.randint(-tone, tone)
            bc = shade(base, t)
            for yy in range(by, by + bh):
                for xx in range(bx, bx + bw):
                    n = rng.randint(-6, 6)
                    # occasional darker pit / wear speck
                    if rng.random() < wear:
                        n -= rng.randint(10, 26)
                    c.blend(xx, yy, bc[0] + n, bc[1] + n, bc[2] + n, 255, wrap=True)
            hi = shade(bc, bevel_hi)
            lo = shade(bc, bevel_lo)
            # lit top + left, shadowed bottom + right
            for xx in range(bx, bx + bw):
                c.blend(xx, by, hi[0], hi[1], hi[2], 200, wrap=True)
                c.blend(xx, by + bh - 1, lo[0], lo[1], lo[2], 200, wrap=True)
            for yy in range(by, by + bh):
                c.blend(bx, yy, hi[0], hi[1], hi[2], 200, wrap=True)
                c.blend(bx + bw - 1, yy, lo[0], lo[1], lo[2], 200, wrap=True)
            # a hairline crack on some bricks
            if rng.random() < 0.30:
                cx0 = rng.randint(bx + 3, bx + bw - 4)
                cy0 = by + 1
                steps = rng.randint(bh // 2, bh - 2)
                xcur = cx0
                for s in range(steps):
                    c.blend(xcur, cy0 + s, lo[0] - 12, lo[1] - 12, lo[2] - 12, 150, wrap=True)
                    xcur += rng.choice((-1, 0, 0, 1))
            x += brick_w
    return c


# ---------------------------------------------------------------- lantern sheet
def lantern_sheet(frames=8, fs=64):
    sheet = Canvas(frames * fs, fs)
    metal = (58, 42, 28, 255)
    metal_hi = (120, 92, 58, 255)
    glass = (90, 60, 28)
    for f in range(frames):
        ox = f * fs
        ph = (f / frames) * 2 * math.pi
        flick = 0.5 + 0.5 * math.sin(ph)
        jit = math.sin(ph * 2.0)
        cx = ox + fs // 2

        # baked halo (this is what makes the glow pulse as frames cycle)
        sub = Canvas(fs, fs)
        sub.glow(fs // 2, 34, 30 + 5 * flick,
                 (255, 168, 70, int(70 + flick * 70)), power=2.2)
        for y in range(fs):
            for x in range(fs):
                i = (y * fs + x) * 4
                if sub.buf[i + 3]:
                    sheet.blend(ox + x, y, sub.buf[i], sub.buf[i+1],
                                sub.buf[i+2], sub.buf[i+3])

        # chain + ceiling mount
        sheet.rect(cx - 1, 2, 2, 8, metal)
        sheet.rect(cx - 5, 1, 10, 3, metal_hi)
        # top cap of the lantern
        sheet.rect(cx - 12, 12, 24, 6, metal)
        sheet.rect(cx - 12, 12, 24, 2, metal_hi)
        # glass housing (warm, brightening with the flame)
        gb = int(18 + flick * 26)
        for yy in range(18, 46):
            for xx in range(cx - 11, cx + 11):
                sheet.blend(xx, yy, glass[0] + gb, glass[1] + gb // 2,
                            glass[2], 235)
        # vertical frame posts + bottom
        sheet.vline(cx - 11, 18, 47, metal)
        sheet.vline(cx + 10, 18, 47, metal)
        sheet.vline(cx, 18, 47, (40, 28, 18, 90))
        sheet.rect(cx - 13, 46, 26, 6, metal)
        sheet.rect(cx - 13, 50, 26, 2, shade(metal, -14))
        # ring at very bottom
        sheet.rect(cx - 3, 52, 6, 3, metal_hi)

        # animated flame (leaf-shaped, layered)
        base_y = 44
        flame_h = 16 + 7 * flick
        sway = jit * 2.0

        def flame(w0, h0, col, alpha):
            for t in range(int(h0)):
                yy = base_y - t
                frac = t / h0
                width = w0 * math.sin(math.pi * (t + 0.5) / h0)
                fx = cx + sway * frac
                for dx in range(-int(width), int(width) + 1):
                    sheet.blend(int(fx) + dx, yy, col[0], col[1], col[2], alpha)

        flame(7, flame_h, (236, 120, 40), 235)
        flame(4.5, flame_h * 0.82, (255, 184, 74), 245)
        flame(2.2, flame_h * 0.6, (255, 246, 200), 255)
    return sheet


# ---------------------------------------------------------------- pillar
def pillar():
    w, h = 64, 144
    c = Canvas(w, h)
    # A paler, cooler limestone so columns stand out from the warm brick floor.
    base = (214, 198, 164, 255)
    rng = random.Random(7)

    def block(x0, y0, x1, y1, tint=0):
        bc = shade(base, tint)
        for y in range(y0, y1):
            for x in range(x0, x1):
                n = rng.randint(-7, 7)
                c.blend(x, y, bc[0] + n, bc[1] + n, bc[2] + n, 255)
        hi = shade(bc, 26)
        lo = shade(bc, -34)
        c.hline(x0, x1, y0, (hi[0], hi[1], hi[2], 220))
        c.hline(x0, x1, y1 - 1, (lo[0], lo[1], lo[2], 220))
        c.vline(x0, y0, y1, (hi[0], hi[1], hi[2], 220))
        c.vline(x1 - 1, y0, y1, (lo[0], lo[1], lo[2], 220))

    # top abacus, capital, shaft, base
    block(4, 0, 60, 9)
    block(7, 9, 57, 22, tint=-6)
    block(16, 22, 48, 120, tint=4)
    block(7, 120, 57, 138, tint=-6)
    block(3, 138, 61, 144)
    # flutes on the shaft (deeper grooves for clearer carved relief)
    for fx in range(20, 48, 6):
        c.vline(fx, 24, 119, (shade(base, -46)[0], shade(base, -46)[1], shade(base, -46)[2], 200))
        c.vline(fx + 1, 24, 119, (shade(base, 30)[0], shade(base, 30)[1], shade(base, 30)[2], 150))
    # crisp dark outline down the shaft sides so the silhouette reads on the floor
    out = (96, 78, 50, 220)
    c.vline(16, 22, 120, out)
    c.vline(47, 22, 120, out)
    return c


# ---------------------------------------------------------------- urn
def urn():
    w, h = 48, 56
    c = Canvas(w, h)
    # Terracotta, deliberately off-palette from the sandstone so urns read as
    # separate props rather than blending into the floor.
    base = (176, 102, 68, 255)
    rng = random.Random(11)
    cx = w // 2
    # body
    for y in range(14, 52):
        ry = 1.0 - ((y - 32) / 22.0) ** 2
        if ry <= 0:
            continue
        half = int(15 * math.sqrt(ry))
        for x in range(cx - half, cx + half):
            n = rng.randint(-7, 7)
            shadeside = -18 if x > cx + half - 4 else (22 if x < cx - half + 3 else 0)
            c.blend(x, y, base[0] + n + shadeside, base[1] + n + shadeside,
                    base[2] + n + shadeside, 255)
    # neck + rim
    c.rect(cx - 8, 8, 16, 7, shade(base, -6))
    c.rect(cx - 10, 6, 20, 3, shade(base, 24))
    c.rect(cx - 10, 12, 20, 2, shade(base, -28))
    # handles
    for sgn in (-1, 1):
        for t in range(8):
            a = math.pi * t / 7
            hx = cx + sgn * (13 + int(5 * math.sin(a)))
            hy = 20 + int(10 * (1 - math.cos(a)))
            c.blend(hx, hy, *shade(base, -22))
            c.blend(hx + sgn, hy, *shade(base, -22))
    # cream decorative band reads against the terracotta body
    c.hline(cx - 13, cx + 13, 30, (222, 202, 156, 200))
    c.hline(cx - 13, cx + 13, 34, (222, 202, 156, 200))
    return c


# ---------------------------------------------------------------- banner
def banner():
    w, h = 48, 96
    c = Canvas(w, h)
    cloth = (78, 52, 120)      # deep arcane violet
    cloth_hi = shade(cloth, 30)
    cloth_lo = shade(cloth, -28)
    rng = random.Random(5)
    # rod
    c.rect(4, 4, 40, 5, (54, 40, 26, 255))
    c.rect(4, 4, 40, 2, (110, 84, 52, 255))
    # cloth with a gentle wave + swallowtail bottom
    top, bot = 8, 84
    for y in range(top, bot):
        wave = int(2 * math.sin((y - top) * 0.18))
        x0, x1 = 9 + wave, 39 + wave
        for x in range(x0, x1):
            n = rng.randint(-6, 6)
            edge = -16 if (x > x1 - 3 or x < x0 + 2) else 0
            c.blend(x, y, cloth[0] + n + edge, cloth[1] + n + edge,
                    cloth[2] + n + edge, 255)
        # side trim
        c.blend(x0, y, *cloth_hi)
        c.blend(x1 - 1, y, *cloth_lo)
    # swallowtail notch at the bottom
    for y in range(bot, bot + 9):
        d = y - bot
        wave = int(2 * math.sin((y - top) * 0.18))
        for x in range(9 + wave, 39 + wave):
            mid = 24 + wave
            if abs(x - mid) < d:
                continue
            c.blend(x, y, cloth[0], cloth[1], cloth[2], 255)
    # gold trim top/bottom of cloth
    c.hline(9, 39, top, (214, 176, 92, 230))
    c.hline(9, 39, top + 1, (214, 176, 92, 160))
    # emblem: a glowing rune diamond
    ex, ey = 24, 44
    c.glow(ex, ey, 13, (150, 120, 255, 120), power=2.0)
    for t in range(-8, 9):
        span = 8 - abs(t)
        for x in range(ex - span, ex + span + 1):
            c.blend(x, ey + t, 224, 206, 255, 235)
    for t in range(-5, 6):
        span = 5 - abs(t)
        for x in range(ex - span, ex + span + 1):
            c.blend(x, ey + t, 120, 92, 210, 255)
    return c


def main():
    os.makedirs(OUT, exist_ok=True)
    floor = brick_tile(96, 96, 48, 24, 3, (199, 171, 122), seed=42,
                       bevel_hi=20, bevel_lo=-24, tone=12, wear=0.05)
    write_png(os.path.join(OUT, "hub_floor.png"), floor)

    # Walls are noticeably darker (and a touch cooler) than the floor so the
    # room's edges read clearly instead of blending into the brick floor.
    wall = brick_tile(64, 64, 32, 16, 2, (150, 120, 80), seed=99,
                      bevel_hi=34, bevel_lo=-42, tone=12, wear=0.04)
    write_png(os.path.join(OUT, "hub_wall.png"), wall)

    write_png(os.path.join(OUT, "hub_lantern.png"), lantern_sheet())
    write_png(os.path.join(OUT, "hub_pillar.png"), pillar())
    write_png(os.path.join(OUT, "hub_urn.png"), urn())
    write_png(os.path.join(OUT, "hub_banner.png"), banner())


if __name__ == "__main__":
    main()
