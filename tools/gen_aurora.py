#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""GUOVSSAHAS's tables (ui.c ui_aurora): pixelfx's weather/aurora (https://github.com/patch-gg/pixelfx, MIT License,
Copyright (c) 2026 Patch; its banded light after Sprite Fusion's holy light, its noise after Sprite Fusion's fog,
https://www.spritefusion.com/pixel-art-vfx, MIT License, Copyright (c) 2026 Hugo Duprez), written anew for the FM-1:
what it computes once, here, so the screen draws it in fixed point.

  - the noise the curtains' rays and patches come from: only the nine rows of it they read (each a whole number)
  - a curtain's light from its hem up its rays (PROF), the glow's falloff, (1 - u)^1.3 for the strands
  - the starfield (seed 3) and the night sky's own specks, for the FM-1's window on it: 240 x 240, the effect's canvas
    (320 x 180) seen from x -120 .. 120, y -228 .. 12

  python3 tools/gen_aurora.py firmware/gen/aurora_tables.h
"""
import math
import sys
from pathlib import Path

M32 = 0xFFFFFFFF
W, H, X0, Y0 = 240, 240, -120, -228             # (ui.c AU_X0, AU_Y0: the window, multiples of 4: the dither's phase)
SEED = 3
BAYI = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]


def hsh(*ns):                                   # (pixelfx core/math.js hash, rand)
    h = 0x9E3779B9
    for n in ns:
        h = ((h ^ (int(n) & M32)) * 0x85EBCA6B) & M32
        h ^= h >> 13
        h = (h * 0xC2B2AE35) & M32
        h ^= h >> 16
    return h


def rand(*ns):
    return hsh(*ns) / 4294967296.0


def hexc(s):
    v = int(s.lstrip("#"), 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def mix(a, b, t):
    t = min(1.0, max(0.0, t))
    return tuple(int(a[i] + (b[i] - a[i]) * t) & 255 for i in range(3))


def rgb(c):
    return f"0x{c[0]:02X}{c[1]:02X}{c[2]:02X}u"


def field_rows(seed, rows_wanted, w=256, h=128, cells=(8, 16, 32)):
    out = {y: [0.0] * w for y in rows_wanted}
    amp, total = 0.5, 0.0
    for n in cells:
        cs = w / n
        rows = max(1, round(h / cs))
        lat = [[rand(a, b, seed, n) for a in range(n)] for b in range(rows)]
        at = lambda a, b: lat[b % rows][a % n]
        for y in rows_wanted:
            gy = (y / h) * rows
            iy = math.floor(gy)
            v = gy - iy
            sv = v * v * (3 - 2 * v)
            for x in range(w):
                gx = x / cs
                ix = math.floor(gx)
                u = gx - ix
                su = u * u * (3 - 2 * u)
                top = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * su
                bot = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * su
                out[y][x] += (top + (bot - top) * sv) * amp
        total += amp
        amp *= 0.5
    return {y: [v / total for v in r] for y, r in out.items()}


def arr(name, ctype, vals, per=16):
    out = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


def main():
    G = [hexc(c) for c in ["#081a1e", "#0d3a36", "#13664e", "#1fa06a", "#3fd88e", "#9af7bf", "#e6fff0"]]
    U = [hexc(c) for c in ["#120b2a", "#26164f", "#41208a", "#6a34c0", "#9c62e8", "#d2b4ff", "#f6eeff"]]
    WHITE = (255, 255, 255)
    L = ["/* GUOVSSAHAS's tables: tools/gen_aurora.py (pixelfx's weather/aurora, MIT, Copyright (c) 2026 Patch; after Sprite",
         " * Fusion's holy light and fog, MIT, Copyright (c) 2026 Hugo Duprez). Generated: do not edit. */",
         f"#define AU_W {W}", f"#define AU_H {H}", f"#define AU_X0 ({X0})", f"#define AU_Y0 ({Y0})"]
    # the noise: the rows sample() reads (10 + n * 30, 50 + n * 13, 80 + n * 9, n the curtain), 0 .. 255 of 256
    rows = [10 + n * 30 for n in range(3)] + [50 + n * 13 for n in range(3)] + [80 + n * 9 for n in range(3)]
    F = field_rows(SEED * 5 + 1, rows)
    flat = []
    for y in rows:
        flat += [min(255, max(0, round(v * 256))) for v in F[y]]
    L += ["/* the noise's rows: [r1 n0..2, r2 n0..2, patch n0..2][256], Q8 */"] + arr("AU_FIELD", "uint8_t", flat, 32)
    prof = []
    for i in range(201):
        v = i / 200
        prof.append(round(4096 * (2.6 if v < 0.035 else 1.15 - v if v < 0.22 else (1 - v) ** 1.5 * 0.9)))
    L += ["/* a curtain's light from its hem (0) up its rays (200), Q12 */"] + arr("AU_PROF", "uint16_t", prof)
    L += ["/* a glow's (1 - sqrt(d))^2, d 0 .. 255 of 256, Q12 */"] + \
        arr("AU_GLOW", "uint16_t", [round(4096 * (1 - math.sqrt(i / 256)) ** 2) for i in range(256)])
    L += ["/* a strand's fade, (1 - u)^1.3 at 1 - u = i / 64, Q12 */"] + \
        arr("AU_POW13", "uint16_t", [round(4096 * (i / 64) ** 1.3) for i in range(65)])
    # colours: G, U, their halves, and the ones the stars and strands use
    GM = [mix(G[i], U[i], 0.5) for i in range(7)]
    L += ["/* the palettes, 0xRRGGBB: G (green, the hem), U (violet, high up), GM (between) */"]
    L += arr("AU_G", "uint32_t", [rgb(c) for c in G], 7) + arr("AU_U", "uint32_t", [rgb(c) for c in U], 7)
    L += arr("AU_GM", "uint32_t", [rgb(c) for c in GM], 7)
    L += [f"#define AU_S_DIM {rgb(mix(U[3], WHITE, 0.2))}", f"#define AU_S_MID {rgb(mix(U[5], WHITE, 0.5))}",
          f"#define AU_S_SPK {rgb(mix(U[5], WHITE, 0.6))}", f"#define AU_STRAND_M {rgb(mix(G[5], U[5], 0.5))}"]
    # the night sky: five steps, dithered; its own specks
    top, low = hexc("#0d0b14"), hexc("#221d33")
    steps = [mix(top, low, min(1, s / 4)) for s in range(6)]
    L += ["/* the night sky's five steps, top to bottom (pixelfx's \"sky\" backdrop) */"] + arr("AU_SKY", "uint32_t", [rgb(c) for c in steps], 6)
    specks = []
    for y in range(H):
        t = y / (H - 1)
        for x in range(W):
            r = rand(x, y, 77)
            if r > 0.994:
                step = int(t * 5 + BAYI[((y & 3) << 2) | (x & 3)] / 16)
                specks.append((y, x, rgb(mix(steps[step], hexc("#cfc8ff"), 0.35 + (r - 0.994) * 90))))
    L += [f"#define AU_NSPECK {len(specks)}", "/* the sky's specks, by row: y, x, colour */",
          "static const struct { uint8_t y, x; uint32_t c; } AU_SPECK[AU_NSPECK] = {"]
    L += [f"    {{{y}, {x}, {c}}}," for y, x, c in specks] + ["};"]
    # the stars (pixelfx's, seed 3): those the window shows, with their glows' reach
    st = []
    for i in range(150):
        x = -160 + rand(SEED, i, 1) * 320
        y = -180 + rand(SEED, i, 2) * 175
        r = rand(SEED, i, 3)
        c = 1 + math.floor(rand(SEED, i, 4) * 4)
        ph = rand(SEED, i, 5)
        kind = 0 if r < 0.6 else 1 if r < 0.95 else 2
        col = "AU_S_DIM" if r < 0.6 else "AU_S_MID" if r < 0.9 else "0xFFFFFFu"
        if x < X0 - 12 or x >= X0 + W + 12:
            continue
        st.append((round((x - X0) * 256), round((y - Y0) * 256), kind, c, round(ph * 65536) & 0xFFFF, col))
    L += [f"#define AU_NSTAR {len(st)}",
          "/* the stars: x, y (the window's pixels, Q8), kind (0 a dim speck, 1 a star and its glow, 2 a sparkle), its",
          " * twinkle's turns a loop, its phase (of 65536), colour */",
          "static const struct { int32_t x, y; uint8_t kind, c; uint16_t ph; uint32_t col; } AU_STAR[AU_NSTAR] = {"]
    L += [f"    {{{a}, {b}, {k}, {c}, {p}, {col}}}," for a, b, k, c, p, col in st] + ["};"]
    # the three curtains, back to front (pixelfx's CURTAINS), in fixed point: a phase's turns of 2^32; per Q8 pixel of s,
    # a wave of length lam turns 2^24 / lam
    TAU = 2 * math.pi
    turns = lambda rad: round(rad / TAU * 2 ** 32) & M32
    CUR = [dict(y=-132, slope=-0.08, amp=8, lam=210, fold=14, flam=150, height=40, k=0.6, s0=-210, s1=210, rip=1.6, c=[1, 1, 2], hue=0.35),
           dict(y=-88, slope=0.22, amp=13, lam=250, fold=19, flam=165, height=82, k=1.5, s0=-195, s1=180, rip=2.4, c=[1, 2, 3], hue=0),
           dict(y=-38, slope=-0.05, amp=5, lam=150, fold=10, flam=110, height=20, k=0.45, s0=-210, s1=210, rip=1.4, c=[2, 1, 3], hue=0)]
    L += ["/* the curtains, back to front: hem y (px), its slope (Q16), its wave (amp px, a turn a lam: 2^24 / lam), its fold",
          " * (px, 2^24 / flam), its rays' height (px), brightness k (Q12), its ends s0, s1 (px), ripple (Q8), its cycles a loop",
          " * (fold, wave, the 64 px ripple), hue up its rays (Q12), the phases n * 2, n * 1.7, n (radians, as turns of 2^32) */",
          "static const struct { int32_t y, slope, amp, klam, fold, kflam, height, k, s0, s1, rip; uint8_t c0, c1, c2; int32_t hue;",
          "                      uint32_t ph0, ph1, ph2; } AU_CURT[3] = {"]
    for n, c in enumerate(CUR):
        L.append(f"    {{{c['y']}, {round(c['slope'] * 65536)}, {c['amp']}, {round(2 ** 24 / c['lam'])}, {c['fold']}, "
                 f"{round(2 ** 24 / c['flam'])}, {c['height']}, {round(c['k'] * 4096)}, {c['s0']}, {c['s1']}, "
                 f"{round(c['rip'] * 256)}, {c['c'][0]}, {c['c'][1]}, {c['c'][2]}, {round(c['hue'] * 4096)}, "
                 f"{turns(n * 2)}u, {turns(n * 1.7)}u, {turns(n)}u}},")
    L += ["};", f"#define AU_K64 {round(2 ** 24 / 64)}", f"#define AU_K23 {round(2 ** 24 / 23)}",
          f"#define AU_BREATH_PH {turns(-1.2)}u   /* (the sky's breath: -1.2 rad) */",
          "/* the dither's thresholds (Bayer, (b + 0.5) / 16, Q12), and the shading's (x 0.06) */"]
    L += arr("AU_BAY", "uint16_t", [round((b + 0.5) / 16 * 4096) for b in BAYI])
    L += arr("AU_TH", "uint16_t", [round((b + 0.5) / 16 * 0.06 * 4096) for b in BAYI])
    Path(sys.argv[1]).write_text("\n".join(L) + "\n")
    print(f"{sys.argv[1]}: {len(specks)} specks, {len(st)} stars")


if __name__ == "__main__":
    main()
