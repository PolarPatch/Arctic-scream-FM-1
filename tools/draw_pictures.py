#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""The sounds' pictures, drawn: Arctic scenes in full colour at 464 x 272 into assets/pictures/<id>.png, for
tools/gen_pictures.py to scale down and dither into the firmware. They stand in until the photographs come (a photo
dropped into assets/pictures/ under the same name takes a drawing's place: draw only what is missing, --all to draw
every one again). Each scene is written here, from shapes and noise; nothing is taken from anyone's picture.

    <python with Pillow> tools/draw_pictures.py [--all] [id ...]
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

W, H = 464, 272
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "pictures")


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def sky(im, stops):                             # a vertical gradient: [(y 0..1, colour), ...]
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / (H - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                d.line((0, y, W, y), fill=lerp(c0, c1, (t - t0) / max(1e-6, t1 - t0)))
                break


def stars(im, rnd, n, y1, bright=(230, 240, 255)):
    d = ImageDraw.Draw(im)
    for _ in range(n):
        x, y = rnd.randrange(W), rnd.randrange(int(y1))
        c = lerp((70, 80, 110), bright, rnd.random() ** 2)
        d.rectangle((x, y, x + (1 if rnd.random() < 0.85 else 2), y + 1), fill=c)


def ridge(rnd, base, amp, rough, n=9):          # a mountain line: points across the width
    pts, h = [], 0.0
    for i in range(n + 1):
        h = h * 0.4 + rnd.uniform(-1, 1)
        pts.append((i * W / n, base - amp * (0.5 + 0.5 * math.sin(i * 1.7 + rnd.random())) + h * rough))
    out = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):   # (jagged in between)
        for k in range(8):
            t = k / 8
            out.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + rnd.uniform(-rough, rough) * 0.35))
    out.append(pts[-1])
    return out


def fill_ridge(im, pts, col, bottom=H):
    ImageDraw.Draw(im).polygon(pts + [(W, bottom), (0, bottom)], fill=col)


def aurora(im, rnd, y0, height, cols, n=3, alpha=0.75):
    """curtains of light: folds along a wave, rays hanging down from a bright hem, fading upward"""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    px = layer.load()
    for k in range(n):
        ph, fr, amp = rnd.uniform(0, 6.3), rnd.uniform(1.2, 2.6), rnd.uniform(8, 26)
        base = y0 + k * height * 0.22
        c0, c1 = cols[k % len(cols)], cols[(k + 1) % len(cols)]
        for x in range(W):
            hem = base + amp * math.sin(x / W * fr * 6.28 + ph) + 6 * math.sin(x / 23.0 + ph * 2)
            ray = 0.55 + 0.45 * math.sin(x * 0.31 + ph) * math.sin(x * 0.07 + k)
            for y in range(max(0, int(hem - height)), min(H, int(hem) + 3)):
                t = (hem - y) / height              # 0 at the hem, 1 at the top
                if t < 0:
                    v = max(0.0, 1 + t * 0.8)
                else:
                    v = max(0.0, 1 - min(1.0, t)) ** 1.6 * max(0.0, ray)
                c = lerp(c0, c1, min(1.0, max(0.0, t)))
                r, g, b = px[x, y]
                px[x, y] = (min(255, r + int(c[0] * v)), min(255, g + int(c[1] * v)), min(255, b + int(c[2] * v)))
    layer = layer.filter(ImageFilter.GaussianBlur(1.6))
    return _add(im, layer, alpha)


def _add(im, layer, a):
    import numpy as np
    x = np.asarray(im).astype(float) + np.asarray(layer).astype(float) * a
    return Image.fromarray(x.clip(0, 255).astype("uint8"))


def reflect(im, y_line, dim=0.55, ripple=2):    # the water: the picture above y_line mirrored below it, rippled
    top = im.crop((0, 0, W, y_line)).transpose(Image.FLIP_TOP_BOTTOM)
    px = im.load()
    t = top.load()
    for y in range(y_line, H):
        sy = y - y_line
        if sy >= top.size[1]:
            break
        off = int(ripple * math.sin(y * 0.9))
        for x in range(W):
            r, g, b = t[min(W - 1, max(0, x + off)), sy]
            fade = dim * (1 - sy / (H - y_line) * 0.5)
            px[x, y] = (int(r * fade), int(g * fade), int(b * fade + 6))


def snow(im, rnd, n, size=(1, 2), col=(235, 240, 250), wind=0.0):
    d = ImageDraw.Draw(im)
    for _ in range(n):
        x, y, s = rnd.randrange(W), rnd.randrange(H), rnd.randint(*size)
        d.line((x, y, x + wind * s * 3, y + s), fill=col, width=s)


def disc(im, cx, cy, r, col, glow=0):          # a light: its disc, and a soft glow around it (blurred, added)
    if glow:
        layer = Image.new("RGB", (W, H), (0, 0, 0))
        ImageDraw.Draw(layer).ellipse((cx - r * 1.6, cy - r * 1.6, cx + r * 1.6, cy + r * 1.6), fill=col)
        im.paste(_add(im, layer.filter(ImageFilter.GaussianBlur(glow / 2.5)), 0.9))
    ImageDraw.Draw(im).ellipse((cx - r, cy - r, cx + r, cy + r), fill=col)


NIGHT = [(0, (4, 6, 22)), (0.6, (16, 26, 58)), (1, (30, 46, 80))]


def p_glacier(rnd):                             # DRONE: a glacier's front under the polar night
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 160, 150)
    im = aurora(im, rnd, 90, 70, [(40, 220, 140), (90, 90, 220)], n=2, alpha=0.5)
    fill_ridge(im, ridge(rnd, 150, 40, 10), (30, 40, 66))
    d = ImageDraw.Draw(im)                      # the glacier's wall: blue ice, its cracks
    d.polygon([(70, 168), (400, 160), (420, 205), (60, 210)], fill=(120, 170, 210))
    for x in range(72, 410, 9):
        d.line((x, 166 + rnd.randint(-2, 2), x + rnd.randint(-4, 4), 208), fill=(70, 120, 170), width=2)
    d.polygon([(70, 168), (400, 160), (390, 150), (90, 154)], fill=(200, 225, 240))
    d.rectangle((0, 208, W, H), fill=(10, 18, 34))
    reflect(im, 208, 0.45)
    return im


def p_tundra_aurora(rnd):                       # PAD: the polar night, the aurora over the tundra
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 220, 200)
    im = aurora(im, rnd, 120, 110, [(30, 230, 150), (120, 80, 230), (40, 200, 220)], n=3)
    fill_ridge(im, ridge(rnd, 220, 14, 5, 12), (14, 20, 34))
    fill_ridge(im, ridge(rnd, 236, 8, 3, 14), (22, 30, 46))
    snow(im, rnd, 40, (1, 1), (180, 200, 230))
    return im


def p_icicles(rnd):                             # BELL: icicles under a shelf of ice, glinting
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (10, 20, 46)), (1, (40, 70, 110))])
    stars(im, rnd, 60, 272)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, 46), fill=(170, 205, 230))
    d.rectangle((0, 40, W, 50), fill=(120, 165, 205))
    x = 6
    while x < W:
        w, l = rnd.randint(6, 16), rnd.randint(30, 190)
        d.polygon([(x, 48), (x + w, 48), (x + w / 2 + rnd.uniform(-2, 2), 48 + l)], fill=(150, 200, 235))
        d.line((x + 2, 50, x + w / 2, 46 + l * 0.8), fill=(230, 245, 255), width=1)
        x += w + rnd.randint(2, 14)
    for _ in range(14):                         # (glints)
        gx, gy = rnd.randrange(W), rnd.randrange(60, 220)
        d.line((gx - 4, gy, gx + 4, gy), fill=(255, 255, 255)), d.line((gx, gy - 4, gx, gy + 4), fill=(255, 255, 255))
    return im


def figure(d, x, y, h, col):                    # a lone figure, far off
    d.ellipse((x - h * 0.09, y - h, x + h * 0.09, y - h * 0.82), fill=col)
    d.polygon([(x - h * 0.16, y - h * 0.8), (x + h * 0.16, y - h * 0.8), (x + h * 0.22, y), (x - h * 0.22, y)], fill=col)


def p_voices(rnd):                              # VOICE: figures far off on the tundra, under a pale sky
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (36, 40, 74)), (0.55, (120, 110, 150)), (0.7, (190, 160, 170)), (1, (210, 220, 230))])
    im = aurora(im, rnd, 70, 60, [(80, 220, 170), (150, 120, 230)], n=2, alpha=0.35)
    fill_ridge(im, ridge(rnd, 190, 10, 4, 10), (150, 160, 185))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 196, W, H), fill=(205, 215, 230))
    for i, x in enumerate((150, 196, 236, 300)):
        figure(d, x, 205 + i % 2 * 3, 26 - i * 2, (30, 34, 54))
    snow(im, rnd, 60, (1, 2), (240, 245, 255), 0.4)
    return im


def p_hut(rnd):                                 # OTHER: a hut with a light on the tundra
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 170, 190)
    fill_ridge(im, ridge(rnd, 170, 30, 8), (26, 34, 58))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 200, W, H), fill=(170, 185, 210))
    d.polygon([(190, 200), (190, 170), (230, 150), (270, 170), (270, 200)], fill=(70, 40, 40))
    d.polygon([(184, 172), (230, 146), (276, 172), (270, 176), (230, 154), (190, 176)], fill=(230, 235, 245))
    d.rectangle((212, 178, 228, 192), fill=(255, 200, 90))
    disc(im, 220, 185, 2, (255, 220, 120), glow=30)
    for i in range(20):                         # (the light on the snow)
        d.line((200 - i * 3, 202 + i, 250 + i * 3, 202 + i), fill=lerp((230, 190, 120), (170, 185, 210), i / 20))
    snow(im, rnd, 50, (1, 1))
    return im


def p_flare(rnd):                               # KEYS, SIGNAL FLARE: a red flare rising over the sea ice
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (8, 6, 20)), (1, (40, 20, 40))])
    stars(im, rnd, 120, 200)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 210, W, H), fill=(60, 50, 70))
    for i in range(12):
        d.polygon([(i * 44 + rnd.randint(0, 20), 210), (i * 44 + 30, 206 - rnd.randint(0, 6)), (i * 44 + 46, 210)],
                  fill=(90, 80, 100))
    d.line((240, 210, 250, 90), fill=(255, 140, 100), width=1)
    disc(im, 250, 84, 5, (255, 90, 70), glow=70)
    for y in range(212, H, 2):                  # (its red on the ice)
        d.line((180, y, 320, y), fill=lerp((140, 50, 60), (60, 50, 70), (y - 212) / 60))
    return im


def p_blowing(rnd):                             # TEXTURE, WIND OVER ICE: snow blowing across bare ice
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (60, 70, 100)), (0.6, (140, 150, 175)), (1, (190, 200, 215))])
    d = ImageDraw.Draw(im)
    d.rectangle((0, 180, W, H), fill=(150, 175, 200))
    for _ in range(40):
        y = rnd.randrange(150, H)
        x = rnd.randrange(-100, W)
        d.line((x, y, x + rnd.randint(60, 200), y - rnd.randint(0, 6)), fill=(230, 238, 248), width=rnd.randint(1, 3))
    snow(im, rnd, 260, (1, 2), (245, 248, 255), 2.5)
    return im.filter(ImageFilter.GaussianBlur(0.6))


def p_fjord(rnd):                               # DEEP FJORD, MANDALORIA: a fjord between steep mountains, the moon in it
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (6, 10, 30)), (0.6, (24, 40, 78)), (1, (40, 60, 96))])
    stars(im, rnd, 150, 120)
    disc(im, 232, 64, 13, (235, 235, 215), glow=40)
    d = ImageDraw.Draw(im)
    d.polygon([(0, 40), (60, 70), (120, 110), (170, 160), (0, 160)], fill=(18, 24, 44))
    d.polygon([(W, 30), (W - 70, 64), (W - 130, 120), (W - 175, 160), (W, 160)], fill=(16, 22, 40))
    d.polygon([(0, 40), (30, 50), (60, 70), (50, 76), (20, 60)], fill=(200, 210, 230))
    d.polygon([(W, 30), (W - 40, 46), (W - 70, 64), (W - 60, 72), (W - 20, 50)], fill=(200, 210, 230))
    fill_ridge(im, ridge(rnd, 158, 18, 4, 10), (30, 40, 66), 160)
    d.rectangle((0, 160, W, H), fill=(8, 14, 30))
    reflect(im, 160, 0.6, 3)
    return im


def p_whiteout(rnd):                            # WHITEOUT: a snowstorm, the horizon gone
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (180, 188, 200)), (0.5, (215, 220, 228)), (1, (200, 206, 216))])
    d = ImageDraw.Draw(im)
    figure(d, 330, 200, 22, (160, 166, 180))   # (someone almost gone)
    snow(im, rnd, 900, (1, 3), (250, 252, 255), 1.6)
    return im.filter(ImageFilter.GaussianBlur(1.1))


def p_midnight_sun(rnd):                        # MIDNIGHT SUN, POLAR STILL: the sun low over a glassy sea
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (60, 80, 140)), (0.4, (190, 130, 160)), (0.62, (250, 190, 140)), (0.63, (40, 60, 100)), (1, (30, 44, 80))])
    disc(im, 300, 158, 16, (255, 220, 160), glow=60)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 170, W, H), fill=(40, 56, 96))
    fill_ridge(im, ridge(rnd, 170, 8, 3, 12), (60, 60, 100), 172)
    for y in range(174, H, 3):                  # (the sun's road on the water)
        w = 6 + (y - 174) * 0.6
        d.line((300 - w + rnd.uniform(-3, 3), y, 300 + w, y), fill=lerp((255, 210, 150), (90, 90, 130), (y - 174) / 98))
    return im


def p_ice_cave(rnd):                            # Holvut: an ice cave with a pale figure in it
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (4, 10, 24)), (1, (10, 30, 56))])
    d = ImageDraw.Draw(im)
    d.ellipse((90, 30, 374, 330), fill=(40, 110, 160))     # (the cave's mouth, light from behind)
    d.ellipse((130, 60, 334, 320), fill=(90, 170, 210))
    d.ellipse((170, 90, 294, 310), fill=(150, 210, 235))
    im = im.filter(ImageFilter.GaussianBlur(6))
    d = ImageDraw.Draw(im)
    for _ in range(30):                         # (icicles from the roof)
        x = rnd.randrange(0, W)
        l = rnd.randint(10, 60)
        d.polygon([(x, 0), (x + 8, 0), (x + 4, l)], fill=(20, 50, 80))
    figure(d, 232, 238, 70, (225, 235, 245))
    d.ellipse((222, 172, 228, 176), fill=(20, 40, 60)), d.ellipse((236, 172, 242, 176), fill=(20, 40, 60))
    d.rectangle((0, 238, W, H), fill=(30, 70, 100))
    return im


def p_station(rnd):                             # DEAD SIGNAL: an abandoned weather station, its mast bent, one light
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (20, 20, 30)), (0.7, (50, 50, 64)), (1, (80, 80, 96))])
    d = ImageDraw.Draw(im)
    d.rectangle((0, 200, W, H), fill=(120, 124, 140))
    d.rectangle((150, 166, 230, 202), fill=(50, 46, 52)), d.polygon([(144, 168), (190, 150), (236, 168)], fill=(70, 66, 74))
    d.line((300, 202, 300, 90), fill=(40, 40, 50), width=3), d.line((300, 90, 330, 70), fill=(40, 40, 50), width=3)
    for y in range(100, 200, 14):
        d.line((292, y, 308, y + 10), fill=(40, 40, 50))
    disc(im, 330, 70, 2, (255, 80, 60), glow=24)
    d.rectangle((180, 176, 190, 186), fill=(120, 120, 60))
    snow(im, rnd, 200, (1, 2), (200, 205, 215), 1.2)
    return im


def p_under_ice(rnd):                           # UNDER THE ICE: looking up through the sea ice, bubbles
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (140, 200, 230)), (0.25, (40, 110, 160)), (1, (4, 16, 40))])
    d = ImageDraw.Draw(im)
    for _ in range(9):                          # (the cracks in the ice above, light through them)
        x = rnd.randrange(W)
        d.line((x, 0, x + rnd.randint(-60, 60), 40), fill=(230, 245, 255), width=2)
    for _ in range(10):                         # (light rays down)
        x = rnd.randrange(W)
        d.polygon([(x, 0), (x + 14, 0), (x + 60, H), (x + 30, H)], fill=(70, 140, 180))
    im = im.filter(ImageFilter.GaussianBlur(3))
    d = ImageDraw.Draw(im)
    for _ in range(40):
        x, y, r = rnd.randrange(W), rnd.randrange(60, H), rnd.randint(1, 4)
        d.ellipse((x - r, y - r, x + r, y + r), outline=(180, 220, 240))
    return im


def p_iceberg(rnd):                             # ICEBERG GONG: an iceberg alone in dark water
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (10, 14, 34)), (0.6, (40, 50, 86)), (1, (60, 70, 110))])
    stars(im, rnd, 100, 140)
    d = ImageDraw.Draw(im)
    d.polygon([(150, 168), (190, 110), (220, 120), (250, 80), (290, 116), (320, 168)], fill=(210, 230, 245))
    d.polygon([(250, 80), (290, 116), (320, 168), (270, 168)], fill=(140, 180, 215))
    d.rectangle((0, 168, W, H), fill=(6, 12, 28))
    d.polygon([(140, 168), (330, 168), (300, 240), (170, 236)], fill=(20, 60, 90))   # (what is under)
    reflect(im, 168, 0.35, 2)
    return im


def p_lighthouse(rnd):                          # FOG HORN: a lighthouse in fog
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (60, 66, 80)), (1, (120, 126, 138))])
    d = ImageDraw.Draw(im)
    d.polygon([(250, 70), (270, 70), (280, 200), (240, 200)], fill=(70, 70, 80))
    d.rectangle((246, 58, 274, 72), fill=(40, 40, 50))
    d.polygon([(260, 64), (0, 30), (0, 100)], fill=(200, 200, 170))
    d.polygon([(260, 64), (W, 40), (W, 90)], fill=(170, 170, 150))
    disc(im, 260, 64, 5, (255, 250, 210), glow=40)
    d.rectangle((0, 200, W, H), fill=(50, 56, 68))
    fog = Image.new("RGB", (W, H), (150, 156, 166))
    return Image.blend(im.filter(ImageFilter.GaussianBlur(2)), fog, 0.35)


def p_vidda(rnd):                               # BIEGGA, RIDDU: wind over the vidda, a storm coming in
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (40, 50, 80)), (0.5, (90, 100, 130)), (1, (160, 170, 190))])
    d = ImageDraw.Draw(im)
    for _ in range(14):                         # (clouds racing)
        y = rnd.randrange(10, 140)
        x = rnd.randrange(-80, W)
        d.ellipse((x, y, x + rnd.randint(80, 200), y + rnd.randint(10, 26)), fill=(70, 80, 110))
    im = im.filter(ImageFilter.GaussianBlur(4))
    fill_ridge(im, ridge(rnd, 190, 20, 6, 10), (60, 70, 96))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 200, W, H), fill=(180, 190, 205))
    snow(im, rnd, 300, (1, 2), (235, 240, 250), 3.0)
    return im


# ---- (2.2) a picture for every factory sound: 22 more scenes, so no two sounds share one


def tree(d, x, y, h, col):                      # a spruce, a dark triangle of tiers
    for k in range(4):
        w = h * (0.34 - k * 0.07)
        top = y - h * (0.35 + k * 0.2)
        d.polygon([(x - w, top + h * 0.3), (x + w, top + h * 0.3), (x, top)], fill=col)
    d.line((x, y, x, y - h * 0.2), fill=col, width=max(1, int(h / 14)))


def birch(d, x, y, h, col, rnd):                # a bare birch, bent by the wind
    lean = rnd.uniform(-6, 6)
    d.line((x, y, x + lean, y - h), fill=col, width=2)
    for k in range(5):
        by = y - h * (0.35 + k * 0.13)
        dx = rnd.choice((-1, 1)) * rnd.uniform(6, 16)
        d.line((x + lean * (y - by) / h, by, x + lean * (y - by) / h + dx, by - rnd.uniform(6, 14)), fill=col)


def reindeer(d, x, y, s, col):                  # a reindeer side on, its antlers
    d.ellipse((x - 9 * s, y - 13 * s, x + 9 * s, y - 5 * s), fill=col)
    for lx in (-6, -3, 4, 7):
        d.line((x + lx * s, y - 7 * s, x + lx * s, y), fill=col, width=max(1, int(s)))
    d.polygon([(x + 7 * s, y - 11 * s), (x + 13 * s, y - 17 * s), (x + 15 * s, y - 15 * s), (x + 10 * s, y - 9 * s)], fill=col)
    d.ellipse((x + 12 * s, y - 18 * s, x + 17 * s, y - 14 * s), fill=col)
    for a in (-1, 1):
        d.line((x + 13 * s, y - 18 * s, x + 13 * s + a * 5 * s, y - 26 * s), fill=col)
        d.line((x + 13 * s + a * 3 * s, y - 22 * s, x + 13 * s + a * 7 * s, y - 23 * s), fill=col)


def p_snow_veil(rnd):                           # SNOW VEIL: snow falling soft over a spruce forest, the moon behind it
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (30, 36, 60)), (0.6, (70, 80, 110)), (1, (110, 120, 145))])
    disc(im, 330, 70, 18, (210, 215, 225), glow=70)
    d = ImageDraw.Draw(im)
    for row, (y, h, c) in enumerate(((190, 60, (40, 52, 70)), (215, 80, (24, 34, 50)), (250, 110, (12, 18, 30)))):
        for x in range(-10, W + 20, 26 + row * 10):
            tree(d, x + rnd.randint(-8, 8), y, h * rnd.uniform(0.7, 1.1), c)
    d.rectangle((0, 250, W, H), fill=(170, 180, 200))
    im = Image.blend(im, Image.new("RGB", (W, H), (120, 128, 150)), 0.18)
    snow(im, rnd, 500, (1, 2), (230, 235, 245), 0.2)
    return im.filter(ImageFilter.GaussianBlur(0.5))


def p_lichen(rnd):                              # LICHEN: the tundra close, lichen and stones, reindeer far off, low sun
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (90, 120, 170)), (0.45, (220, 190, 160)), (0.55, (240, 210, 170)), (1, (200, 180, 150))])
    disc(im, 90, 110, 12, (255, 235, 190), glow=50)
    fill_ridge(im, ridge(rnd, 128, 10, 3, 12), (110, 110, 120), 140)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 136, W, H), fill=(120, 120, 90))
    for _ in range(260):                        # (lichen: pale green-grey, rust, cream)
        x, y = rnd.randrange(W), rnd.randrange(140, H)
        r = 2 + (y - 136) * rnd.uniform(0.04, 0.12)
        c = rnd.choice(((190, 200, 160), (170, 180, 140), (200, 120, 60), (225, 215, 180), (140, 150, 110)))
        d.ellipse((x - r, y - r * 0.6, x + r, y + r * 0.6), fill=c)
    for _ in range(9):
        x, y = rnd.randrange(W), rnd.randrange(170, H)
        d.ellipse((x, y, x + rnd.randint(20, 46), y + rnd.randint(10, 20)), fill=(90, 92, 100))
    for i, x in enumerate((300, 326, 350)):
        reindeer(d, x, 140 - i % 2, 0.7, (60, 50, 50))
    return im


def p_snowdrift(rnd):                           # SNOWDRIFT: drifts carved by the wind at dusk, snow streaming off them
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (40, 46, 90)), (0.5, (150, 120, 160)), (0.62, (240, 170, 150)), (1, (240, 200, 180))])
    d = ImageDraw.Draw(im)
    for k in range(6):                          # (the drifts, back to front, a lit edge and a blue shadow)
        y = 160 + k * 22
        pts = [(x, y - 18 * math.sin(x / (60 + k * 13) + k * 1.3) ** 2 - rnd.uniform(0, 3)) for x in range(0, W + 8, 8)]
        d.polygon(pts + [(W, H), (0, H)], fill=lerp((150, 150, 200), (215, 220, 240), k / 5))
        d.line(pts, fill=(255, 225, 205), width=2)
    for _ in range(30):
        y, x = rnd.randrange(140, 250), rnd.randrange(-60, W)
        d.line((x, y, x + rnd.randint(30, 90), y - rnd.randint(2, 8)), fill=(250, 235, 230))
    return im.filter(ImageFilter.GaussianBlur(0.7))


def p_permafrost(rnd):                          # PERMAFROST: the frozen ground in polygons under the stars
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 200, 120)
    fill_ridge(im, ridge(rnd, 112, 12, 4, 12), (20, 26, 44), 120)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 116, W, H), fill=(50, 60, 84))
    for row in range(9):                        # (the polygons, smaller far off: their cracks of ice)
        y0 = 116 + (row ** 1.6) * 3.4
        y1 = 116 + ((row + 1) ** 1.6) * 3.4
        n = 14 - row
        for i in range(-1, n + 1):
            x0 = i * W / n + rnd.uniform(-6, 6) + (row % 2) * W / n / 2
            d.line((x0, y0, x0 + rnd.uniform(-8, 8), y1), fill=(150, 175, 205), width=1 + row // 3)
        d.line((0, y1, W, y1 + rnd.uniform(-2, 2)), fill=(130, 155, 190), width=1 + row // 3)
    return im


def p_cracking_ice(rnd):                        # CRACKING ICE: a lake's black ice, white cracks running, the moon
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (8, 10, 26)), (1, (30, 40, 70))])
    stars(im, rnd, 120, 90)
    disc(im, 120, 40, 10, (230, 230, 220), glow=36)
    fill_ridge(im, ridge(rnd, 96, 14, 4, 12), (14, 18, 34), 100)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 98, W, H), fill=(12, 20, 36))
    for _ in range(7):                          # (cracks: branching white lines across the dark ice)
        x, y = rnd.uniform(0, W), rnd.uniform(110, H)
        for _ in range(4):
            a = rnd.uniform(-0.5, 0.5) + rnd.choice((0, math.pi))
            px, py = x, y
            for _ in range(rnd.randint(5, 12)):
                nx, ny = px + math.cos(a) * rnd.uniform(8, 22), py + math.sin(a) * rnd.uniform(2, 8)
                d.line((px, py, nx, ny), fill=(200, 225, 245), width=1)
                px, py, a = nx, ny, a + rnd.uniform(-0.6, 0.6)
    d.line((60, 160, 300, 220), fill=(240, 250, 255), width=2)
    return im


def p_solar_wind(rnd):                          # SOLAR WIND: the aurora's corona straight overhead, rays from a point
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (10, 6, 26)), (1, (20, 20, 50))])
    stars(im, rnd, 180, H)
    layer = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(layer)
    cx, cy = 232, 110
    for k in range(70):
        a = k / 70 * 6.283 + rnd.uniform(-0.04, 0.04)
        l = rnd.uniform(90, 260)
        c = lerp((50, 230, 140), (200, 70, 160), rnd.random() ** 1.5)
        d.line((cx + math.cos(a) * 18, cy + math.sin(a) * 12, cx + math.cos(a) * l, cy + math.sin(a) * l * 0.7), fill=c,
               width=rnd.randint(2, 6))
    im = _add(im, layer.filter(ImageFilter.GaussianBlur(3)), 0.9)
    disc(im, cx, cy, 4, (200, 255, 220), glow=40)
    fill_ridge(im, ridge(rnd, 258, 8, 3, 14), (6, 8, 16))
    return im


def p_ice_bell(rnd):                            # ICE BELL: a small wooden chapel in the snow, its bell, one window lit
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (12, 16, 40)), (0.7, (40, 56, 96)), (1, (70, 86, 120))])
    stars(im, rnd, 160, 150)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 206, W, H), fill=(190, 200, 222))
    d.rectangle((180, 150, 290, 210), fill=(90, 40, 36))             # (the chapel)
    d.polygon([(170, 152), (235, 112), (300, 152)], fill=(225, 232, 245))
    d.rectangle((300, 120, 330, 210), fill=(80, 36, 32))             # (its bell tower)
    d.polygon([(294, 122), (315, 80), (336, 122)], fill=(225, 232, 245))
    d.rectangle((306, 128, 324, 146), fill=(20, 16, 24))
    d.ellipse((310, 132, 320, 144), fill=(200, 160, 80))
    d.rectangle((222, 170, 238, 192), fill=(255, 205, 110))
    disc(im, 230, 181, 2, (255, 220, 140), glow=26)
    for x in (40, 80, 400, 440):
        tree(ImageDraw.Draw(im), x, 214, 60, (14, 22, 34))
    snow(im, rnd, 90, (1, 2), (235, 240, 250), 0.3)
    return im


def p_crystal_harp(rnd):                        # CRYSTAL HARP: hoarfrost grown thick on wires against a pale sunrise
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (120, 150, 200)), (0.6, (230, 200, 200)), (1, (250, 225, 200))])
    d = ImageDraw.Draw(im)
    fill_ridge(im, ridge(rnd, 236, 10, 3, 10), (170, 170, 200))
    d.line((60, H, 70, 20), fill=(60, 54, 70), width=8)              # (two poles, wires strung between, down the sky)
    d.line((400, H, 392, 40), fill=(60, 54, 70), width=8)
    for k in range(6):
        y0, y1 = 40 + k * 26, 58 + k * 26
        pts = [(70 + (392 - 70) * t, y0 + (y1 - y0) * t + 40 * math.sin(t * math.pi)) for t in [i / 40 for i in range(41)]]
        d.line(pts, fill=(80, 80, 100), width=1)
        for px, py in pts:                      # (the frost on it: feathers of white)
            for _ in range(2):
                l = rnd.uniform(2, 7)
                a = rnd.uniform(-2.6, -0.5)
                d.line((px, py, px + math.cos(a) * l, py + math.sin(a) * l), fill=(250, 252, 255))
    return im


def p_aurora_choir(rnd):                        # AURORA CHOIR: many curtains over a frozen lake, mirrored in its ice
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 220, 150)
    im = aurora(im, rnd, 60, 70, [(40, 230, 140), (200, 80, 200), (60, 200, 230), (120, 240, 120)], n=5, alpha=0.8)
    fill_ridge(im, ridge(rnd, 150, 12, 4, 12), (14, 20, 34), 156)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 156, W, H), fill=(10, 16, 30))
    reflect(im, 156, 0.5, 1)
    return im


def p_blizzard_breath(rnd):                     # BLIZZARD BREATH: a reindeer herd turned from the storm
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (120, 126, 140)), (1, (190, 196, 206))])
    d = ImageDraw.Draw(im)
    d.rectangle((0, 190, W, H), fill=(200, 205, 214))
    for i in range(9):
        x, y, s = 60 + i * 42 + rnd.randint(-10, 10), 200 + rnd.randint(0, 30), rnd.uniform(1.1, 1.6)
        reindeer(d, x, y, s, lerp((50, 50, 60), (130, 134, 146), rnd.random() * 0.6))
    snow(im, rnd, 700, (1, 3), (245, 248, 252), 2.2)
    return im.filter(ImageFilter.GaussianBlur(0.9))


def p_northern_call(rnd):                       # NORTHERN CALL: a wolf on a ridge, calling to a full moon
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (6, 10, 30)), (0.7, (30, 44, 84)), (1, (50, 64, 100))])
    stars(im, rnd, 160, 180)
    disc(im, 300, 80, 34, (240, 238, 220), glow=90)
    d = ImageDraw.Draw(im)
    d.polygon([(0, H), (0, 210), (140, 186), (230, 200), (320, 230), (W, 240), (W, H)], fill=(10, 14, 26))
    x, y = 150, 186                             # (the wolf: body, legs, its head up)
    d.polygon([(x - 26, y - 14), (x + 8, y - 18), (x + 14, y - 10), (x - 22, y - 6)], fill=(10, 14, 26))
    for lx in (-22, -16, 2, 8):
        d.line((x + lx, y - 8, x + lx, y + 2), fill=(10, 14, 26), width=3)
    d.polygon([(x + 6, y - 16), (x + 16, y - 34), (x + 24, y - 38), (x + 18, y - 26), (x + 14, y - 12)], fill=(10, 14, 26))
    d.polygon([(x + 14, y - 34), (x + 16, y - 42), (x + 19, y - 36)], fill=(10, 14, 26))
    d.polygon([(x - 26, y - 12), (x - 40, y - 4), (x - 24, y - 8)], fill=(10, 14, 26))
    return im


def p_empty_horizon(rnd):                       # EMPTY HORIZON: sea ice flat to the edge of the world, a band of light
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (30, 40, 80)), (0.5, (90, 110, 160)), (0.6, (240, 200, 170)), (0.62, (200, 210, 230)), (1, (150, 165, 195))])
    d = ImageDraw.Draw(im)
    for y in range(170, H, 3):                  # (the ice, faint lines toward the eye)
        d.line((0, y, W, y), fill=lerp((210, 215, 235), (150, 165, 195), (y - 170) / 102))
    for _ in range(16):
        y = rnd.randrange(176, H)
        x = rnd.randrange(W)
        d.line((x, y, x + rnd.randint(10, 60), y), fill=(235, 240, 250))
    return im


def p_tundra_strings(rnd):                      # TUNDRA STRINGS: a power line crossing the tundra, poles to the horizon
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (40, 50, 90)), (0.55, (140, 130, 160)), (0.66, (220, 170, 150)), (1, (200, 190, 200))])
    fill_ridge(im, ridge(rnd, 170, 8, 3, 12), (90, 90, 120), 178)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 176, W, H), fill=(190, 195, 215))
    prev = None
    for k in range(9):                          # (the poles, nearer each time)
        t = k / 8
        x = 420 - t * t * 380
        h = 16 + t * t * 180
        base = 178 + t * t * 90
        d.line((x, base, x, base - h), fill=(40, 36, 46), width=max(1, int(t * 6)))
        d.line((x - h * 0.18, base - h * 0.92, x + h * 0.18, base - h * 0.92), fill=(40, 36, 46), width=max(1, int(t * 4)))
        if prev:
            for s in (-1, 0, 1):
                d.line((prev[0] + s * prev[1] * 0.17, prev[2] - prev[1] * 0.92, x + s * h * 0.17, base - h * 0.92),
                       fill=(50, 46, 60))
        prev = (x, h, base)
    return im


def p_icebreaker(rnd):                          # ICEBREAKER: a ship cutting through the ice at night, its lights on
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (8, 12, 30)), (1, (36, 46, 80))])
    stars(im, rnd, 120, 130)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 160, W, H), fill=(150, 170, 200))
    for _ in range(30):                         # (the broken floes)
        x, y = rnd.randrange(W), rnd.randrange(166, H)
        d.polygon([(x, y), (x + rnd.randint(14, 40), y + rnd.randint(-3, 3)), (x + rnd.randint(10, 30), y + rnd.randint(5, 12))],
                  fill=(200, 215, 235))
    d.polygon([(100, 200), (180, 230), (330, 236), (400, 200), (380, 186), (110, 186)], fill=(16, 18, 28))   # (its hull)
    d.polygon([(150, 186), (150, 160), (250, 160), (260, 140), (300, 140), (310, 186)], fill=(150, 40, 40))
    d.line((270, 140, 270, 96), fill=(30, 30, 40), width=3)
    for x in range(160, 300, 14):
        d.rectangle((x, 166, x + 6, 172), fill=(255, 220, 140))
    disc(im, 270, 94, 2, (255, 255, 230), glow=20)
    disc(im, 396, 196, 4, (255, 250, 220), glow=60)                  # (its searchlight on the ice ahead)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 236, W, H), fill=(10, 20, 36))
    return im


def p_svalbard_cello(rnd):                      # SVALBARD CELLO: a row of painted houses under the mountain, polar night
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (10, 14, 40)), (0.6, (36, 50, 96)), (1, (60, 76, 120))])
    stars(im, rnd, 120, 80)
    d = ImageDraw.Draw(im)
    d.polygon([(0, 70), (120, 40), (260, 70), (380, 30), (W, 60), (W, 200), (0, 200)], fill=(200, 210, 230))   # (snow on it)
    d.polygon([(0, 110), (140, 80), (260, 110), (370, 76), (W, 100), (W, 200), (0, 200)], fill=(70, 80, 110))
    d.rectangle((0, 200, W, H), fill=(180, 190, 210))
    cols = ((190, 50, 50), (230, 180, 60), (60, 120, 170), (80, 150, 100), (200, 110, 60), (150, 70, 120))
    for i in range(8):
        x = 20 + i * 54
        c = cols[i % len(cols)]
        d.rectangle((x, 160, x + 44, 206), fill=c)
        d.polygon([(x - 4, 162), (x + 22, 140), (x + 48, 162)], fill=(230, 236, 246))
        for wx in (x + 8, x + 26):
            lit = rnd.random() < 0.7
            d.rectangle((wx, 172, wx + 10, 184), fill=(255, 210, 120) if lit else (30, 30, 40))
    return im


def p_north_strings(rnd):                       # NORTH STRINGS: drying racks of fish on the shore, the aurora above
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 180, 150)
    im = aurora(im, rnd, 90, 80, [(40, 220, 150), (60, 180, 230)], n=2, alpha=0.6)
    fill_ridge(im, ridge(rnd, 170, 30, 8, 10), (20, 26, 44), 180)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 178, W, H), fill=(170, 182, 206))
    for k in range(3):                          # (the racks: posts, rails, rows of fish hanging)
        y = 186 + k * 26
        x0, x1 = 30 + k * 30, 440 - k * 10
        for x in range(int(x0), int(x1) + 1, 70):
            d.line((x, y + 30, x - 10, y - 50), fill=(50, 40, 40), width=3)
            d.line((x, y + 30, x + 10, y - 50), fill=(50, 40, 40), width=3)
        d.line((x0, y - 44, x1, y - 44), fill=(50, 40, 40), width=2)
        for x in range(int(x0) + 4, int(x1), 6):
            d.line((x, y - 44, x, y - 24 - rnd.randint(0, 6)), fill=(110, 90, 70), width=2)
    return im


def p_riddu(rnd):                               # RIDDU: the coast in a storm, a wave breaking on black rock
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (40, 46, 60)), (1, (90, 100, 116))])
    d = ImageDraw.Draw(im)
    for _ in range(14):
        y, x = rnd.randrange(0, 120), rnd.randrange(-80, W)
        d.ellipse((x, y, x + rnd.randint(80, 220), y + rnd.randint(14, 30)), fill=(60, 66, 82))
    im = im.filter(ImageFilter.GaussianBlur(5))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 160, W, H), fill=(30, 50, 66))
    for y in range(164, H, 8):                  # (the swell)
        d.line([(x, y + 4 * math.sin(x / 30 + y)) for x in range(0, W + 10, 10)], fill=(60, 86, 104), width=2)
    d.polygon([(0, H), (0, 170), (60, 160), (130, 190), (180, 230), (200, H)], fill=(14, 14, 20))   # (the rock)
    for _ in range(260):                        # (the breaking wave, its spray)
        a = rnd.uniform(-2.6, -0.4)
        r = rnd.uniform(0, 90) ** 1.1
        x, y = 140 + math.cos(a) * r, 190 + math.sin(a) * r
        s = rnd.randint(1, 4)
        d.ellipse((x, y, x + s, y + s), fill=(225, 235, 240))
    return im.filter(ImageFilter.GaussianBlur(0.6))


def p_wendigo(rnd):                             # WENDIGO: a dark forest of bare trees, and something tall between them
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (14, 20, 30)), (1, (40, 50, 64))])
    d = ImageDraw.Draw(im)
    d.rectangle((0, 220, W, H), fill=(90, 100, 116))
    for _ in range(26):
        birch(d, rnd.randrange(W), 230 + rnd.randint(-6, 20), rnd.randint(140, 240), (10, 12, 16), rnd)
    x, y = 250, 226                             # (it: thin, too tall, antlers)
    d.polygon([(x - 8, y), (x - 6, y - 90), (x - 12, y - 120), (x + 12, y - 120), (x + 6, y - 90), (x + 8, y)], fill=(170, 176, 186))
    d.ellipse((x - 9, y - 140, x + 9, y - 118), fill=(200, 205, 212))
    for a in (-1, 1):
        d.line((x + a * 6, y - 138, x + a * 22, y - 170), fill=(170, 176, 186), width=2)
        d.line((x + a * 14, y - 154, x + a * 28, y - 158), fill=(170, 176, 186), width=2)
    d.rectangle((x - 5, y - 132, x - 2, y - 129), fill=(255, 60, 40)), d.rectangle((x + 2, y - 132, x + 5, y - 129), fill=(255, 60, 40))
    snow(im, rnd, 60, (1, 1), (180, 190, 200))
    return im


def p_frozen_scream(rnd):                       # FROZEN SCREAM: down into a crevasse, deep blue, snow falling in
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (230, 240, 250)), (0.12, (140, 200, 235)), (0.5, (30, 90, 150)), (1, (2, 8, 24))])
    d = ImageDraw.Draw(im)
    left = [(0, 0)] + [(150 + 40 * math.sin(y / 30) + rnd.uniform(-6, 6) + y * 0.25, y) for y in range(0, H + 8, 8)] + [(0, H)]
    right = [(W, 0)] + [(320 - 40 * math.sin(y / 34 + 1) + rnd.uniform(-6, 6) - y * 0.25, y) for y in range(0, H + 8, 8)] + [(W, H)]
    d.polygon(left, fill=(70, 140, 190)), d.polygon(right, fill=(60, 120, 175))
    for y in range(0, H, 10):                   # (the ice's layers in the walls)
        d.line((0, y, 140 + y * 0.25, y + 3), fill=(110, 175, 215))
        d.line((W, y + 5, 330 - y * 0.25, y + 8), fill=(100, 160, 205))
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    snow(im, rnd, 70, (1, 2), (240, 248, 255))
    return im


def p_polar_still(rnd):                         # POLAR STILL: a lake without a breath of wind, the mountains in it
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (40, 60, 120)), (0.5, (110, 130, 190)), (0.62, (210, 170, 190)), (1, (230, 200, 200))])
    stars(im, rnd, 40, 60)
    d = ImageDraw.Draw(im)
    d.polygon([(40, 140), (150, 50), (200, 80), (250, 40), (340, 110), (420, 90), (W, 140)], fill=(70, 80, 120))
    d.polygon([(150, 50), (170, 66), (180, 62), (200, 80), (190, 70), (160, 70)], fill=(230, 235, 245))
    d.polygon([(250, 40), (280, 64), (300, 70), (270, 66)], fill=(230, 235, 245))
    fill_ridge(im, ridge(rnd, 138, 6, 2, 14), (40, 46, 70), 140)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 140, W, H), fill=(20, 26, 46))
    reflect(im, 140, 0.85, 0)
    return im


def p_iii(rnd):                                 # III: three standing stones on a hill under the moon
    im = Image.new("RGB", (W, H))
    sky(im, [(0, (12, 10, 30)), (0.7, (50, 40, 80)), (1, (90, 70, 110))])
    stars(im, rnd, 180, 200)
    disc(im, 340, 60, 14, (245, 240, 225), glow=60)
    d = ImageDraw.Draw(im)
    d.ellipse((-60, 190, W + 60, 420), fill=(30, 26, 44))
    for x, h, w in ((170, 70, 20), (230, 94, 24), (290, 62, 18)):
        d.polygon([(x - w, 210), (x - w * 0.7, 210 - h), (x, 210 - h - 8), (x + w * 0.6, 210 - h + 4), (x + w, 210)],
                  fill=(70, 66, 86))
        d.line((x + w * 0.6, 210 - h + 4, x + w, 210), fill=(140, 136, 160), width=2)
    return im


def p_ice_wraith(rnd):                          # ICE WRAITH: a shape of blowing snow rising over the glacier at night
    im = Image.new("RGB", (W, H))
    sky(im, NIGHT)
    stars(im, rnd, 160, 160)
    fill_ridge(im, ridge(rnd, 170, 50, 10), (30, 40, 70))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 210), (W, 196), (W, H), (0, H)], fill=(110, 140, 180))
    layer = Image.new("RGB", (W, H))
    ld = ImageDraw.Draw(layer)
    for k in range(300):                        # (the wraith: a tall veil of snow, arms out, a hollow face)
        t = rnd.random()
        y = 210 - t * 150
        w = 10 + 30 * math.sin(t * math.pi) + (40 if 0.55 < t < 0.7 else 0)
        x = 240 + rnd.uniform(-w, w) + 20 * math.sin(t * 5)
        s = rnd.randint(2, 6)
        ld.ellipse((x, y, x + s, y + s), fill=(150, 180, 210))
    im = _add(im, layer.filter(ImageFilter.GaussianBlur(2.5)), 1.0)
    d = ImageDraw.Draw(im)
    d.ellipse((246, 72, 252, 80), fill=(20, 30, 50)), d.ellipse((260, 72, 266, 80), fill=(20, 30, 50))
    snow(im, rnd, 120, (1, 2), (220, 230, 245), 1.0)
    return im



SCENES = {
    "glacier": p_glacier, "tundra_aurora": p_tundra_aurora, "icicles": p_icicles, "voices": p_voices, "hut": p_hut,
    "flare": p_flare, "blowing": p_blowing, "fjord": p_fjord, "whiteout": p_whiteout, "midnight_sun": p_midnight_sun,
    "ice_cave": p_ice_cave, "station": p_station, "under_ice": p_under_ice, "iceberg": p_iceberg,
    "lighthouse": p_lighthouse, "vidda": p_vidda,
    "snow_veil": p_snow_veil, "lichen": p_lichen, "snowdrift": p_snowdrift, "permafrost": p_permafrost,
    "cracking_ice": p_cracking_ice, "solar_wind": p_solar_wind, "ice_bell": p_ice_bell, "crystal_harp": p_crystal_harp,
    "aurora_choir": p_aurora_choir, "blizzard_breath": p_blizzard_breath, "northern_call": p_northern_call,
    "empty_horizon": p_empty_horizon, "tundra_strings": p_tundra_strings, "icebreaker": p_icebreaker,
    "svalbard_cello": p_svalbard_cello, "north_strings": p_north_strings, "riddu": p_riddu, "wendigo": p_wendigo,
    "frozen_scream": p_frozen_scream, "polar_still": p_polar_still, "iii": p_iii, "ice_wraith": p_ice_wraith,
}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    every = "--all" in sys.argv
    os.makedirs(OUT, exist_ok=True)
    for name, fn in SCENES.items():
        if args and name not in args:
            continue
        dst = os.path.join(OUT, name + ".png")
        if os.path.exists(dst) and not every and not args:
            continue                            # (a photograph, or a drawing kept: left as it is)
        fn(random.Random(name)).save(dst)
        print("drawn", dst)


if __name__ == "__main__":
    main()
