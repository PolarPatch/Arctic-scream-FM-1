#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""The sounds' pictures into the firmware: assets/pictures/<id>.png (a drawing from tools/draw_pictures.py, or a
photograph under the same name), each scaled to PIC_W x PIC_H cells (a cell 2 x 2 pixels on the FM-1: HOME's field),
16 colours of its own (median cut), dithered in order (Bayer 4 x 4, as COLOUR MONO's dots), packed 4 bits a cell and
run-length coded, into firmware/gen/pictures.h with which picture each category and each factory sound shows.

    <python with Pillow and numpy> tools/gen_pictures.py      (-> firmware/gen/pictures.h, build/pictures/*.png previews)

The run-length code: a control byte c < 0x80 is c + 1 bytes as they are, c >= 0x80 the next byte c - 0x7E times. A
byte is two cells, the left one in its low nibble.
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "pictures")
DST = os.path.join(ROOT, "firmware", "gen", "pictures.h")
PREVIEW = os.path.join(ROOT, "build", "pictures")
PIC_W, PIC_H = 116, 68

# which picture a category shows (store.c's CAT_ order), and a factory sound its own
CATS = ["glacier", "tundra_aurora", "icicles", "hut", "flare", "glacier", "blowing", "voices", "hut"]   # DRONE PAD BELL
#     ORGAN KEYS BASS TEXTURE VOICE OTHER
SOUNDS = {                                      # (2.2: each factory sound a picture of its own)
    "fjord": ["DEEP FJORD", "MANDALORIA"], "whiteout": ["WHITEOUT"], "midnight_sun": ["MIDNIGHT SUN"],
    "ice_cave": ["POLAR HOWL"], "station": ["DEAD SIGNAL"], "under_ice": ["UNDER THE ICE"], "iceberg": ["ICEBERG GONG"],
    "lighthouse": ["FOG HORN"], "vidda": ["BIEGGA"], "flare": ["SIGNAL FLARE"], "blowing": ["WIND OVER ICE"],
    "tundra_aurora": ["POLAR NIGHT"], "glacier": ["GLACIER PAD"], "icicles": ["BOWED ICICLE"],
    "voices": ["TUNDRA VOICES"], "hut": ["ARCTIC HUM"],
    "snow_veil": ["SNOW VEIL"], "lichen": ["LICHEN"], "snowdrift": ["SNOWDRIFT"], "permafrost": ["PERMAFROST"],
    "cracking_ice": ["CRACKING ICE"], "solar_wind": ["SOLAR WIND"], "ice_bell": ["ICE BELL"],
    "crystal_harp": ["CRYSTAL HARP"], "aurora_choir": ["AURORA CHOIR"], "blizzard_breath": ["BLIZZARD BREATH"],
    "northern_call": ["NORTHERN CALL"], "empty_horizon": ["EMPTY HORIZON"], "tundra_strings": ["TUNDRA STRINGS"],
    "icebreaker": ["ICEBREAKER"], "svalbard_cello": ["SVALBARD CELLO"], "north_strings": ["NORTH STRINGS"],
    "riddu": ["RIDDU"], "wendigo": ["WENDIGO"], "frozen_scream": ["FROZEN SCREAM"], "polar_still": ["POLAR STILL"],
    "iii": ["III"], "ice_wraith": ["ICE WRAITH"],
}
# (2.2) what moves in each picture while it shows (pictures.c animates it, nothing more is stored than this): in cells
# (116 x 68: the drawing's pixels / 4). The colours a part takes are found by pick() in the picture's own palette.
#   sh   shimmer: (kind CURTAIN, GLITTER or RAYS, rows, pick, high colour): the aurora's folds, a road of light on the
#        water, rays of light breathing (CURTAIN also sways its rows' cells sideways)
#   tw   twinkle: (rows, pick, n): the picked colours twinkling (n 0: glints, the ice's light), or n stars of
#        the firmware's own on them (the sky's dark: a star of the drawing is mostly lost at 116 x 68), each its phase
#   rp   ripple: rows of water, shifted a little each row
#   sc   scroll: (rows, tenths of a cell a second): clouds racing, drift on the ice
#   li   a light: (kind PULSE, FLICKER, BLINK or BEAM, x, y, radius, colour)
#   fd   fade: (x0, x1, y0, y1, pick, colour): a figure fading into what is around it, and back
#   dt   dots: (kind SNOW, DRIP, RISE or DRIFT, how many, x0, x1, y0, y1, tenths of a cell a second, wind, colour)
SH_KIND = {"CURTAIN": 1, "GLITTER": 2, "RAYS": 3}
LI_KIND = {"PULSE": 1, "FLICKER": 2, "BLINK": 3, "BEAM": 4}
DT_KIND = {"SNOW": 1, "DRIP": 2, "RISE": 3, "DRIFT": 4}
ANIM = {
    "glacier": dict(sh=("CURTAIN", (0, 36), dict(lum=(60, 255), frac=0.8, cnt=(40, 9999)), (70, 250, 165)),
                    tw=((0, 36), dict(lum=(0, 50)), 22), rp=(52, 67)),
    "tundra_aurora": dict(sh=("CURTAIN", (0, 47), dict(lum=(75, 255), cnt=(100, 9999), frac=0.7), (70, 250, 165)),
                          tw=((0, 47), dict(lum=(70, 255), cnt=(0, 60)), 0),
                          dt=("SNOW", 14, 0, 115, 0, 67, 25, 8, "light")),
    "icicles": dict(tw=((14, 60), dict(lum=(200, 255)), 0), dt=("DRIP", 8, 0, 115, 12, 62, 0, 0, "light")),
    "voices": dict(sh=("CURTAIN", (0, 22), dict(lum=(60, 255), green=15, frac=0.7), (110, 240, 190)),
                   fd=(34, 80, 41, 53, dict(lum=(0, 60)), (205, 215, 230)),
                   dt=("SNOW", 10, 0, 115, 0, 67, 20, 4, "light")),
    "hut": dict(li=("FLICKER", 55, 46, 9, (255, 200, 110)), tw=((0, 40), dict(lum=(0, 50)), 24),
                dt=("SNOW", 10, 0, 115, 0, 67, 18, 3, "light")),
    "flare": dict(li=("FLICKER", 62, 21, 13, (255, 90, 70)), tw=((0, 48), dict(lum=(0, 50)), 22),
                  dt=("RISE", 6, 59, 65, 2, 20, 14, 2, (160, 130, 150))),
    "blowing": dict(sc=((45, 67), 30), dt=("DRIFT", 16, 0, 115, 28, 67, 60, 0, "light")),
    "fjord": dict(sh=("GLITTER", (40, 67), dict(lum=(50, 140)), (240, 240, 220)), rp=(40, 67),
                  tw=((0, 28), dict(lum=(0, 40)), 18), li=("PULSE", 58, 16, 7, (240, 240, 215))),
    "whiteout": dict(fd=(78, 86, 41, 51, dict(lum=(0, 190), frac=0.5), (215, 220, 228)),
                     dt=("SNOW", 20, 0, 115, 0, 67, 30, 30, "light")),
    "midnight_sun": dict(sh=("GLITTER", (43, 67), dict(lum=(85, 140)), (255, 232, 180)),
                         rp=(43, 67), li=("PULSE", 75, 39, 8, (255, 225, 170))),
    "ice_cave": dict(fd=(48, 68, 26, 60, dict(lum=(200, 255), frac=0.6), (150, 210, 235)),
                     dt=("DRIP", 6, 0, 115, 0, 18, 0, 0, (120, 170, 200))),
    "station": dict(li=("BLINK", 82, 17, 6, (255, 80, 60)), dt=("SNOW", 14, 0, 115, 0, 67, 25, 30, "light")),
    "under_ice": dict(sh=("RAYS", (0, 67), dict(lum=(60, 190)), (170, 225, 250)),
                      dt=("RISE", 10, 0, 115, 15, 67, 20, 0, (180, 220, 240))),
    "iceberg": dict(rp=(42, 67), tw=((0, 34), dict(lum=(0, 50)), 20),
                    sh=("RAYS", (18, 42), dict(lum=(150, 255), frac=0.6), (240, 250, 255))),
    "lighthouse": dict(li=("BEAM", 65, 16, 64, (255, 250, 210))),
    "vidda": dict(sc=((2, 36), 20), dt=("DRIFT", 18, 0, 115, 30, 67, 70, 0, "light")),
    "snow_veil": dict(li=("PULSE", 82, 17, 6, (215, 220, 230)), dt=("SNOW", 16, 0, 115, 0, 67, 12, 3, "light")),
    "lichen": dict(sh=("RAYS", (0, 33), dict(lum=(150, 255)), (255, 240, 210)), li=("PULSE", 22, 27, 6, (255, 235, 190))),
    "snowdrift": dict(sc=((40, 67), 15), dt=("DRIFT", 16, 0, 115, 30, 67, 60, 0, "light")),
    "permafrost": dict(tw=((0, 28), dict(lum=(0, 50)), 24), dt=("SNOW", 8, 0, 115, 0, 67, 10, 4, "light")),
    "cracking_ice": dict(tw=((27, 67), dict(lum=(150, 255)), 0), li=("PULSE", 30, 10, 5, (230, 230, 220))),
    "solar_wind": dict(sh=("RAYS", (0, 63), dict(lum=(60, 255), cnt=(30, 9999)), (120, 255, 190)),
                       tw=((0, 63), dict(lum=(0, 40)), 20)),
    "ice_bell": dict(li=("PULSE", 57, 45, 6, (255, 210, 120)), tw=((0, 37), dict(lum=(0, 50)), 18),
                     dt=("SNOW", 10, 0, 115, 0, 67, 12, 3, "light")),
    "crystal_harp": dict(tw=((8, 60), dict(lum=(220, 255)), 0), dt=("SNOW", 6, 0, 115, 0, 67, 6, 4, "light")),
    "aurora_choir": dict(sh=("CURTAIN", (0, 38), dict(lum=(75, 255), cnt=(100, 9999), frac=0.6), (70, 250, 165)),
                         rp=(39, 67), tw=((0, 38), dict(lum=(0, 50)), 16)),
    "blizzard_breath": dict(dt=("DRIFT", 20, 0, 115, 0, 67, 70, 0, "light")),
    "northern_call": dict(li=("PULSE", 75, 20, 10, (240, 238, 220)), tw=((0, 44), dict(lum=(0, 50)), 22)),
    "empty_horizon": dict(sh=("GLITTER", (43, 67), dict(lum=(150, 255)), (255, 240, 220)),
                          dt=("SNOW", 6, 0, 115, 0, 67, 8, 6, "light")),
    "tundra_strings": dict(dt=("SNOW", 12, 0, 115, 0, 67, 14, 30, "light")),
    "icebreaker": dict(li=("BLINK", 67, 23, 4, (255, 255, 230)), rp=(59, 67), dt=("SNOW", 8, 0, 115, 0, 58, 12, 6, "light")),
    "svalbard_cello": dict(tw=((42, 47), dict(lum=(150, 255), red=60), 0), dt=("SNOW", 14, 0, 115, 0, 67, 12, 5, "light")),
    "north_strings": dict(sh=("CURTAIN", (0, 40), dict(lum=(60, 255), green=15, frac=0.7), (70, 250, 165)),
                          dt=("SNOW", 8, 0, 115, 0, 67, 12, 4, "light")),
    "riddu": dict(rp=(40, 67), sc=((0, 30), 25), dt=("DRIFT", 14, 10, 70, 20, 60, 50, 0, "light")),
    "wendigo": dict(fd=(56, 70, 10, 57, dict(lum=(140, 255), frac=0.5), (30, 36, 46)),
                    li=("BLINK", 62, 24, 2, (255, 60, 40)), dt=("SNOW", 8, 0, 115, 0, 67, 10, 3, "light")),
    "frozen_scream": dict(sh=("RAYS", (0, 67), dict(lum=(60, 200)), (200, 235, 255)),
                          dt=("SNOW", 12, 35, 80, 0, 67, 14, 2, "light")),
    "polar_still": dict(tw=((0, 14), dict(lum=(0, 140)), 10)),
    "iii": dict(li=("PULSE", 85, 15, 6, (245, 240, 225)), tw=((0, 45), dict(lum=(0, 50)), 22)),
    "ice_wraith": dict(fd=(50, 72, 14, 53, dict(lum=(110, 255), frac=0.5), (16, 26, 58)),
                       dt=("DRIFT", 14, 0, 115, 20, 67, 50, 0, "light")),
}
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], float) / 16.0 - 0.47


def convert(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size                              # (cropped to the cells' shape from the middle, then scaled)
    want = PIC_W / PIC_H
    if w / h > want:
        cw = int(h * want)
        im = im.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h))
    else:
        ch = int(w / want)
        im = im.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
    small = im.resize((PIC_W, PIC_H), Image.LANCZOS)
    mid = im.resize((PIC_W * 2, PIC_H * 2), Image.LANCZOS)   # (the palette from 4 x the cells, refined: small bright
                                                 # areas, the aurora's green, keep a colour of their own)
    pal = np.array(mid.quantize(16, method=Image.Quantize.MAXCOVERAGE, kmeans=8).getpalette()[:48], float).reshape(16, 3)
    px = np.asarray(small, float)
    spread = np.std(pal, axis=0).mean() * 0.9 + 8   # (how far the dots reach: a little more than the palette's steps)
    yy, xx = np.mgrid[0:PIC_H, 0:PIC_W]
    px = px + BAYER[yy % 4, xx % 4][..., None] * spread
    d = ((px[:, :, None, :] - pal[None, None, :, :]) ** 2).sum(-1)
    idx = d.argmin(-1).astype(np.uint8)
    return pal.astype(int), idx


def pick(pal, idx, rows, c):
    """the palette's colours a part takes (a mask of 16 bits): bright enough (lum), common or rare enough (cnt), most of
    their cells in the rows and columns (frac), greener or redder than the rest (green, red)"""
    lum = 0.3 * pal[:, 0] + 0.59 * pal[:, 1] + 0.11 * pal[:, 2]
    cnt = np.bincount(idx.ravel(), minlength=16)
    y0, y1 = rows
    x0, x1 = c.get("cols", (0, PIC_W - 1))
    inside = np.bincount(idx[y0:y1 + 1, x0:x1 + 1].ravel(), minlength=16)
    m = 0
    for i in range(16):
        if cnt[i] == 0 or not (c.get("lum", (0, 255))[0] <= lum[i] <= c.get("lum", (0, 255))[1]):
            continue
        if not (c.get("cnt", (0, 99999))[0] <= cnt[i] <= c.get("cnt", (0, 99999))[1]):
            continue
        if inside[i] < c.get("frac", 0.0) * cnt[i] or inside[i] == 0:
            continue
        if "green" in c and not pal[i, 1] > pal[i, 0] + c["green"]:
            continue
        if "red" in c and not pal[i, 0] > pal[i, 2] + c["red"]:
            continue
        m |= 1 << i
    return m


def anim(name, pal, idx):                       # the picture's line of PIC_ANIM (pictures.c's pic_anim_t)
    a = ANIM.get(name, {})
    lum = 0.3 * pal[:, 0] + 0.59 * pal[:, 1] + 0.11 * pal[:, 2]
    dark, light = int(np.argmin(lum)), int(np.argmax(lum))
    col = lambda c: rgb565(pal[light]) if c == "light" else rgb565(c)
    v = dict(sh=(0, 0, 0, 0, 0), tw=(0, 0, 0, 0), rp=(0, 0), sc=(0, 0, 0), li=(0, 0, 0, 0, 0), fd=(0, 0, 0, 0, 0, 0),
             dt=(0, 0, 0, 0, 0, 0, 0, 0, 0))
    if "sh" in a:
        k, rows, c, hi = a["sh"]
        v["sh"] = (pick(pal, idx, rows, c), SH_KIND[k], rows[0], rows[1], rgb565(hi))
    if "tw" in a:
        rows, c, n = a["tw"]
        v["tw"] = (pick(pal, idx, rows, c), rows[0], rows[1], n)
    if "rp" in a:
        v["rp"] = a["rp"]
    if "sc" in a:
        (y0, y1), sp = a["sc"]
        v["sc"] = (y0, y1, sp)
    if "li" in a:
        k, x, y, r, c = a["li"]
        v["li"] = (LI_KIND[k], x, y, r, rgb565(c))
    if "fd" in a:
        x0, x1, y0, y1, c, to = a["fd"]
        v["fd"] = (pick(pal, idx, (y0, y1), dict(c, cols=(x0, x1))), x0, x1, y0, y1, rgb565(to))
    if "dt" in a:
        k, n, x0, x1, y0, y1, sp, wind, c = a["dt"]
        v["dt"] = (DT_KIND[k], n, x0, x1, y0, y1, sp, wind, col(c))
    f = lambda t: ", ".join(str(int(x)) for x in t)
    return "    {{%s}, {%s}, {%s}, {%s}, {%s}, {%s}, {%s}, %d}   /* %s */" % (
        f(v["sh"]), f(v["tw"]), f(v["rp"]), f(v["sc"]), f(v["li"]), f(v["fd"]), f(v["dt"]), dark, name)


def rle(b):
    out, i, n = bytearray(), 0, len(b)
    while i < n:
        j = i
        while j + 1 < n and b[j + 1] == b[i] and j - i < 128:
            j += 1
        if j - i >= 1:                          # a run of 2..129
            out += bytes([0x7E + (j - i + 1), b[i]])
            i = j + 1
            continue
        k = i                                   # literals up to the next run of 2 (128 at most)
        while k < n and k - i < 128 and not (k + 1 < n and b[k + 1] == b[k]):
            k += 1
        out += bytes([k - i - 1]) + b[i:k]
        i = k
    return bytes(out)


def rgb565(c):
    r, g, b = (int(v) for v in c)
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


def main():
    names = sorted(f[:-4] for f in os.listdir(SRC) if f.endswith(".png"))
    for n in set(CATS) | set(SOUNDS):
        if n not in names:
            sys.exit("gen_pictures: no assets/pictures/%s.png (tools/draw_pictures.py draws it)" % n)
    os.makedirs(PREVIEW, exist_ok=True)
    out = ["/* generated by tools/gen_pictures.py from assets/pictures/ (a PNG each): do not edit */",
           "#define PIC_W %du" % PIC_W, "#define PIC_H %du" % PIC_H, "#define NPIC %du" % len(names)]
    total = 0
    pals, datas, anims = [], [], []
    for i, n in enumerate(names):
        pal, idx = convert(os.path.join(SRC, n + ".png"))
        packed = bytes((idx[y, x] | idx[y, x + 1] << 4) for y in range(PIC_H) for x in range(0, PIC_W, 2))
        z = rle(packed)
        total += len(z) + 32
        pals.append(", ".join("0x%04X" % rgb565(c) for c in pal))
        anims.append(anim(n, pal, idx))
        rows = [", ".join(str(v) for v in z[j:j + 32]) for j in range(0, len(z), 32)]
        out.append("static const uint8_t PIC_D%d[%d] = {\n    %s};   /* %s */" % (i, len(z), ",\n    ".join(rows), n))
        prev = Image.fromarray(pal[idx].astype(np.uint8)).resize((PIC_W * 2, PIC_H * 2), Image.NEAREST)
        prev.save(os.path.join(PREVIEW, n + ".png"))
    out.append("static const uint16_t PIC_PAL[NPIC][16] = {\n" + ",\n".join("    {%s}" % p for p in pals) + "};")
    out.append("static const pic_anim_t PIC_ANIM[NPIC] = {   /* (2.2) what moves: pictures.c */\n" + ",\n".join(anims) + "};")
    out.append("static const uint8_t *const PIC_DATA[NPIC] = {%s};" % ", ".join("PIC_D%d" % i for i in range(len(names))))
    out.append("static const char *const PIC_NAME[NPIC] = {%s};" % ", ".join('"%s"' % n for n in names))
    out.append("static const uint8_t PIC_CAT[%d] = {%s};   /* store.c's categories, in their order */" %
               (len(CATS), ", ".join(str(names.index(c)) for c in CATS)))
    pairs = sorted((s, names.index(p)) for p, ss in SOUNDS.items() for s in ss)
    out.append("#define NPIC_SND %du" % len(pairs))
    out.append("static const struct { const char *snd; uint8_t pic; } PIC_SND[NPIC_SND] = {\n" +
               ",\n".join('    {"%s", %d}' % p for p in pairs) + "};")
    open(DST, "w").write("\n".join(out) + "\n")
    print("gen_pictures: %d pictures, %d bytes (%s)" % (len(names), total, DST))


if __name__ == "__main__":
    main()
