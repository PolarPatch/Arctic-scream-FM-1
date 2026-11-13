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


SCENES = {
    "glacier": p_glacier, "tundra_aurora": p_tundra_aurora, "icicles": p_icicles, "voices": p_voices, "hut": p_hut,
    "flare": p_flare, "blowing": p_blowing, "fjord": p_fjord, "whiteout": p_whiteout, "midnight_sun": p_midnight_sun,
    "ice_cave": p_ice_cave, "station": p_station, "under_ice": p_under_ice, "iceberg": p_iceberg,
    "lighthouse": p_lighthouse, "vidda": p_vidda,
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
