/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1) A picture for every sound (after Demake's, Sequencist's, whose every sound has its landscape; written anew):
 * firmware/gen/pictures.h (tools/gen_pictures.py, from assets/pictures/: drawn by tools/draw_pictures.py, or the
 * user's own photographs), PIC_W x PIC_H cells of 16 colours each, run-length coded. Which one a sound shows: its own
 * (PIC_SND, by the factory sound's name), else its category's (PIC_CAT). HOME shows it as a card a moment while
 * PRESETS turns (ui.c pic_home); the VIBE shows it big (Arctic Scream VIBE's port/home.c).
 *
 * pic_rows decodes a picture a row at a time into a row of cells (no buffer for the whole of it: the FM-1's RAM is
 * short), each row to a function of the caller's. */
typedef struct {                                /* (2.2) what moves in a picture (tools/gen_pictures.py's ANIM): */
    uint16_t sh[5];                             /*   shimmer: its colours (a mask), kind, rows y0 .. y1, its high colour */
    uint16_t tw[4];                             /*   twinkle: its colours, rows; n: n stars of our own on them, else they */
    uint8_t rp[2];                              /*   ripple: rows */
    int16_t sc[3];                              /*   scroll: rows, tenths of a cell a second */
    uint16_t li[5];                             /*   a light: kind, x, y, radius, colour */
    uint16_t fd[6];                             /*   fade: its colours, x0, x1, y0, y1, the colour it fades into */
    int32_t dt[9];                              /*   dots: kind, how many, x0, x1, y0, y1, tenths a second, wind, colour */
    uint8_t dark;                               /*   the palette's darkest */
} pic_anim_t;
#include "pictures.h"

#define PC_CARD_MS 2500u                        /* the card on HOME this long after the last turn */
static uint32_t pc_until, pc_id;                /* main loop: HOME's card, until then; its picture */
static char pc_name[16];                        /* .. and the sound's name on it */

static void pic_rows(uint32_t id, void (*row)(uint32_t y, const uint8_t *cells))
{
    const uint8_t *z = PIC_DATA[id % NPIC];
    uint8_t cells[PIC_W];
    uint32_t y = 0, x = 0, n, i;
    while (y < PIC_H) {                         /* (a control byte: c + 1 bytes as they are, or the next one c - 0x7E
                                                 * times; a byte is two cells, the left in its low nibble) */
        uint32_t c = *z++, rep = c >= 0x80u;
        n = rep ? c - 0x7Eu : c + 1u;
        for (i = 0; i < n && y < PIC_H; i++) {
            uint32_t b = rep ? z[0] : z[i];
            cells[x++] = (uint8_t)(b & 15u), cells[x++] = (uint8_t)(b >> 4);
            if (x >= PIC_W) row(y++, cells), x = 0;
        }
        z += rep ? 1u : n;
    }
}

static uint32_t pic_of_track(uint32_t t)        /* the picture of track t's sound: its own, else its category's */
{
    char nm[24];
    uint32_t i;
    nm[0] = 0;
    trk_sname(t, nm);
    for (i = 0; i < NPIC_SND; i++)
        if (same(nm, PIC_SND[i].snd)) return PIC_SND[i].pic;
    return PIC_CAT[(uint32_t)brw_cat[t] % (sizeof PIC_CAT)];
}

static void pic_card(uint32_t t)                /* main loop (PRESETS turned): HOME shows track t's picture a moment */
{
    if (pc_mode != PC_CARD) return;             /* (SKY: HOME shows it always; OFF: never) */
    pc_id = pic_of_track(t);
    trk_sname(t, pc_name);
#ifdef DINGHY_HOST
    if (getenv("GRIMM_PIC")) pc_id = (uint32_t)atoi(getenv("GRIMM_PIC")) % NPIC;   /* (host: a picture to film) */
#endif
    pc_until = fm1_ms + PC_CARD_MS;
}
static int pic_card_on(void) { return (int32_t)(pc_until - fm1_ms) > 0; }

/* ---- (2.2) Pictures that move: while a picture shows (the card, ~2.5 s; SKY, always) a little of it moves, from the
 * picture itself and a line of numbers (PIC_ANIM: tools/gen_pictures.py), never frames: the aurora's folds breathing and
 * swaying, the water rippling and glittering, stars twinkling, a light flickering, blinking or sweeping, a figure
 * fading into the snow and back, snow, drips, bubbles and drift. pic_anim_rows hands the caller a row of colours
 * (RGB565) at a time, as pic_rows does cells; t in ms. A frame costs a pass over the picture's cells (7888). */
enum { SH_CURTAIN = 1, SH_GLITTER, SH_RAYS };
enum { LI_PULSE = 1, LI_FLICKER, LI_BLINK, LI_BEAM };
enum { DT_SNOW = 1, DT_DRIP, DT_RISE, DT_DRIFT };
#define PA_DOTS 24u
static const pic_anim_t *pa;
static uint32_t pa_id, pa_t, pa_n;
static int (*pa_want)(uint32_t y);
static void (*pa_out)(uint32_t y, const uint16_t *col);
static uint8_t pa_dx[PA_DOTS], pa_dy[PA_DOTS];

static int32_t pa_sin(uint32_t ph)              /* a sine, ph 4096 a turn: Q12 (a parabola a half turn) */
{
    int32_t x = (int32_t)(ph & 2047u) - 1024, y = 4096 - (x * x >> 8);
    return ph & 2048u ? -y : y;
}
static uint32_t pa_hash(uint32_t a)
{
    a ^= a >> 16, a *= 0x7FEB352Du, a ^= a >> 15, a *= 0x846CA68Bu;
    return a ^ a >> 16;
}
static uint16_t pa_mix(uint16_t a, uint16_t b, int32_t q)   /* a -> b, q of 256 (RGB565) */
{
    int32_t r0 = a >> 11, g0 = a >> 5 & 63, b0 = a & 31;
    q = q < 0 ? 0 : q > 256 ? 256 : q;
    r0 += ((b >> 11) - r0) * q >> 8, g0 += ((b >> 5 & 63) - g0) * q >> 8, b0 += ((b & 31) - b0) * q >> 8;
    return (uint16_t)(r0 << 11 | g0 << 5 | b0);
}
static int pa_in(uint32_t y, uint32_t y0, uint32_t y1) { return y1 && y >= y0 && y <= y1; }

static void pa_dots(void)                       /* where the dots are at pa_t (each its own from a hash: no state) */
{
    const int32_t *d = pa->dt;
    uint32_t i, n = (uint32_t)d[1] < PA_DOTS ? (uint32_t)d[1] : PA_DOTS;
    int32_t w = d[3] - d[2] + 1, h = d[5] - d[4] + 1, t = (int32_t)(pa_t % 600000u);
    pa_n = 0;
    if (!d[0] || w <= 0 || h <= 0) return;
    for (i = 0; i < n; i++) {
        uint32_t r = pa_hash(i * 7919u + pa_id * 131u + 17u);
        int32_t sp = 128 + (int32_t)(r >> 16 & 127u), x = (int32_t)(r % (uint32_t)w), y = (int32_t)(r >> 8) % h, wb;
        int32_t fall = d[6] * sp * t / 256 / 10000;   /* (cells, d[6] tenths a second, sp 0.5 .. 1) */
        wb = pa_sin((uint32_t)t * 2u / 3u + (r >> 4)) * 3 / 8192;   /* (a sway of 1 .. 2 cells) */
        switch (d[0]) {
        case DT_SNOW:
            y = (y + fall) % h, x = ((x + d[7] * t / 10000 * sp / 256 + wb) % w + w) % w;
            break;
        case DT_DRIP: {                         /* (each falls from its tip, quicker as it goes, and again) */
            int32_t per = 1300 + (int32_t)(r >> 20 & 1023u), ph = (t + (int32_t)(r & 4095u)) % per;
            x = (int32_t)(r % (uint32_t)w), y = (h - 1) * ph / per * ph / per;
            break;
        }
        case DT_RISE:
            y = h - 1 - (y + fall) % h, x = ((x + wb + d[7] * t / 10000) % w + w) % w;
            break;
        case DT_DRIFT:                          /* (streaks along the wind: 3 cells, drawn in pa_row) */
            x = (x + fall * 2) % w, y = (y + wb / 2 + h) % h;
            break;
        }
        pa_dx[pa_n] = (uint8_t)(d[2] + x), pa_dy[pa_n] = (uint8_t)(d[4] + y), pa_n++;
    }
}

static void pa_row(uint32_t y, const uint8_t *cells)
{
    const uint16_t *pal = PIC_PAL[pa_id % NPIC];
    uint16_t out[PIC_W], lo = pal[pa->dark];
    uint32_t x, i, t = pa_t;
    int32_t scr = 0, rip = 0, sway = 0, rowm = 4096, li = 0;
    if (pa_want && !pa_want(y)) return;
    if (pa_in(y, (uint32_t)pa->sc[0], (uint32_t)pa->sc[1])) scr = pa->sc[2] * (int32_t)(t % 600000u) / 10000;
    if (pa_in(y, pa->rp[0], pa->rp[1])) rip = pa_sin(y * 587u + t * 3u / 5u) * 3 / 8192;   /* (-1 .. 1 a row) */
    if (pa->sh[1] == SH_CURTAIN && pa_in(y, pa->sh[2], pa->sh[3])) {
        sway = pa_sin(y * 117u - t * 17u / 20u) * 3 / 8192;   /* (the curtains sway: -1 .. 1, rows apart) */
        rowm = 3072 + pa_sin(y * 196u + t * 13u / 10u) / 4;   /* (and breathe a row at a time: 0.5 .. 1) */
    }
    switch (pa->li[0]) {                        /* the light's strength now: Q8, below 0 dark */
    case LI_PULSE: li = 140 + pa_sin(t * 3u / 10u) * 90 / 4096; break;
    case LI_FLICKER: {
        uint32_t k = t / 90u, f = t % 90u;
        int32_t a = (int32_t)(pa_hash(k + pa_id) & 255u), b = (int32_t)(pa_hash(k + 1u + pa_id) & 255u);
        li = 70 + (a + (b - a) * (int32_t)f / 90) * 3 / 4;
        break;
    }
    case LI_BLINK: { uint32_t ph = t % 1700u; li = ph < 650u ? 256 - (int32_t)ph * 256 / 650 : -110; break; }
    case LI_BEAM: li = pa_sin(t * 13u / 10u); break;   /* (Q12: the beam turning, left lit, then right) */
    }
    for (x = 0; x < PIC_W; x++) {
        int32_t sx = (int32_t)x;
        uint32_t c;
        uint16_t v;
        if (scr) sx = ((sx + scr) % (int32_t)PIC_W + (int32_t)PIC_W) % (int32_t)PIC_W;
        if (rip) sx = sx + rip < 0 ? 0 : sx + rip >= (int32_t)PIC_W ? (int32_t)PIC_W - 1 : sx + rip;
        c = cells[sx];
        if (sway) {
            int32_t s2 = sx + sway;
            if (s2 >= 0 && s2 < (int32_t)PIC_W && (pa->sh[0] >> cells[s2] & 1u)) c = cells[s2];
        }
        v = pal[c];
        if (pa->sh[1] && (pa->sh[0] >> c & 1u) && pa_in(y, pa->sh[2], pa->sh[3])) {
            int32_t w;
            if (pa->sh[1] == SH_CURTAIN) {      /* (the folds of light running along, the green breaking through) */
                int32_t s1 = pa_sin(x * 137u - t + (uint32_t)(pa_sin(x * 33u + t * 3u / 10u) * 391 >> 10));
                w = 159 + ((97 * s1 >> 12) * (rowm >> 4) >> 8);
                v = pa_mix(lo, v, w * 294 >> 8);
                v = pa_mix(v, pa->sh[4], (w - 179) * 410 >> 8 > 128 ? 128 : (w - 179) * 410 >> 8);
            } else if (pa->sh[1] == SH_GLITTER) {   /* (a road of light: a slow swell, sparks coming and going) */
                uint32_t k = pa_hash(x * 131u + y * 977u + t / 160u * 7919u);
                w = 200 + pa_sin(x * 90u - t / 2u + y * 300u) * 56 / 4096;
                v = (k & 255u) > 228u ? pa_mix(v, pa->sh[4], 190) : pa_mix(lo, v, w);
            } else {                            /* (rays breathing, slowly, across) */
                w = 200 + pa_sin((x * 2u + y) * 59u - t * 2u / 5u) * 56 / 4096;
                v = pa_mix(pa_mix(lo, v, w), pa->sh[4], w > 236 ? (w - 236) * 4 : 0);
            }
        }
        if (pa->tw[0] >> c & 1u && pa_in(y, pa->tw[1], pa->tw[2])) {   /* (each star its own phase) */
            uint32_t k = pa_hash(x + y * 131u + pa_id * 7u);
            if (!pa->tw[3])
                v = pa_mix(lo, v, 141 + pa_sin((k & 4095u) + t * 9u / 4u) * 115 / 4096);
            else if ((k >> 12) % ((uint32_t)(pa->tw[2] - pa->tw[1] + 1u) * PIC_W) < pa->tw[3])
                v = pa_mix(v, 0xEF7Fu, 110 + pa_sin((k & 4095u) + t * 9u / 4u) * 110 / 4096);
        }
        if (pa->fd[0] >> c & 1u && x >= pa->fd[1] && x <= pa->fd[2] && y >= pa->fd[3] && y <= pa->fd[4])
            v = pa_mix(v, pa->fd[5], (4096 - pa_sin(t * 32u / 25u + 1024u)) * 220 >> 13);   /* (gone at 3.2 s's half) */
        if (pa->li[0]) {
            int32_t dx = (int32_t)x - pa->li[1], dy = (int32_t)y - pa->li[2], r = pa->li[3], d2 = dx * dx + dy * dy, q;
            if (pa->li[0] == LI_BEAM) {         /* (a wedge each way from the lamp, one side lit as the other dims) */
                int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, side = dx < 0 ? li : -li;
                if (ax <= r && ay * 4 <= ax + 3 && side > 0)
                    v = pa_mix(v, pa->li[4], side * (r - ax) / r * 200 >> 12);
                if (d2 < 9) v = pa_mix(v, pa->li[4], 160 + (li < 0 ? -li : li) * 96 / 4096);
            } else if (d2 < r * r) {
                q = (r * r - d2) * 256 / (r * r);
                v = li >= 0 ? pa_mix(v, pa->li[4], q * li >> 8) : pa_mix(v, lo, q * -li >> 8);
            }
        }
        out[x] = v;
    }
    for (i = 0; i < pa_n; i++)                  /* the dots on this row (DRIFT: a streak of three) */
        if (pa_dy[i] == y) {
            uint32_t k, len = pa->dt[0] == DT_DRIFT ? 3u : 1u;
            for (k = 0; k < len && pa_dx[i] >= k; k++)
                out[pa_dx[i] - k] = pa_mix(out[pa_dx[i] - k], (uint16_t)pa->dt[8], 190 - (int32_t)k * 60);
        }
    pa_out(y, out);
}

static void pic_anim_rows(uint32_t id, uint32_t t, int (*want)(uint32_t y), void (*row)(uint32_t y, const uint16_t *col))
{
    pa = &PIC_ANIM[id % NPIC], pa_id = id % NPIC, pa_t = t, pa_want = want, pa_out = row;
    pa_dots();
    pic_rows(id, pa_row);
}
static uint32_t pic_card_t(void) { return fm1_ms - (pc_until - PC_CARD_MS); }   /* (2.2) the card's time, from 0 */
