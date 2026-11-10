/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream 1.5.3's Biegga source (North Sami: wind, sanit.oahpa.no; the same word in Lule Sami): wind over the
 * tundra, written for this firmware. A voice is noise, shaped:
 *
 *   noise -> two band-passes at the note and its octave (the howl: the wind singing in a gap) -> + the noise through a
 *   one-pole low-pass (the rush) -> x the gust -> + SNOW's grains
 *
 * The note sets the howl's pitch, so a chord of Biegga is a chord the wind sings; the gust, a slow random walk of the
 * voice's own, swells and drops the sound and bends the howl up as it blows harder.
 *   GUST   how hard and how often it gusts: 0 a steady wind, 127 gusts that come and go every second or so, from a lull
 *          (a quarter) to full, the howl bent up to ~2 semitones as they blow
 *   HOWL   the howl's share and how narrow it sings: 0 a breath of rush alone, 127 a whistle (the band ~20 x narrower)
 *   TONE   the rush's low-pass, ~150 Hz .. ~10 kHz: a far storm .. snow hissing past
 *   SNOW   grains hitting the ice: none .. ~85 a second, short clicks of high noise
 * Fixed point, the trapezoidal SVF's band-pass (dsp.c tsvf_any); the state in the voice (s[], ph[]). */

#define BG_LV 32767                             /* a voice's level (Q15; with BG_UP, ~4 dB under a VA chord) */
#define BG_UP 3                                 /* (the sound x 3 / 2 before the knee) */
static const uint16_t BG_K[9] = {8192, 5115, 3193, 1994, 1245, 777, 485, 303, 189};   /* HOWL 0, 16 .. 128: the bands'
                                                 * damping (Q12: 2 .. 0.046) */
static const uint16_t BG_C[9] = {4096, 5202, 6553, 8314, 10526, 13312, 16834, 21299, 26951};   /* .. their gain back
                                                 * (Q12: x sqrt(2 / damping), a narrow band as loud) */
#define BG_GUST s[3]                            /* the gust now (Q15) */
static int32_t bg_tab(const uint16_t *t, int32_t h)   /* t at HOWL h (0 .. 127), between its points */
{
    int32_t i = h >> 4, f = h & 15;
    return t[i] + ((t[i + 1] - t[i]) * f >> 4);
}
static int32_t bg_noise(voice_t *v)            /* -16384 .. 16383 */
{
    uint32_t s = (uint32_t)v->rng;
    s ^= s << 13, s ^= s >> 17, s ^= s << 5;
    v->rng = (int32_t)s;
    return (int32_t)s >> 17;
}

static void bg_begin(voice_t *v)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) v->s[k] = 0;
    for (k = 0; k < VPH; k++) v->ph[k] = 0;
    v->BG_GUST = 24000;
    v->ph[0] = 24000, v->ph[1] = 1;             /* (the gust's target; blocks to its next) */
    if (!v->rng) v->rng = 0x6F4A7C15;
}

static void bg_block(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1)
{
    int32_t g = clamp(p[0], 0, 127), h = clamp(p[1], 0, 127), tn = clamp(p[2], 0, 127), sn = clamp(p[3], 0, 127);
    int32_t gust = v->BG_GUST, k, comp, cut, a, bg, rg, ag, i, damp = amp1 - amp0;
    int32_t ic1 = v->s[0], ic2 = v->s[1], lp = v->s[2], ic3 = (int32_t)v->ph[2], ic4 = (int32_t)v->ph[3];
    int32_t se = (int32_t)v->ph[4], sp = (int32_t)v->ph[5];
    uint32_t th = (uint32_t)(sn * sn * 2);      /* (SNOW: a grain's chance a sample, of 2^24) */
    tsvf_t c1, c2;
    if (!--v->ph[1]) {                          /* the gust: a new target every ~0.7 .. 2.2 s (GUST 127 .. 0) */
        uint32_t nb = 3000u - (uint32_t)g * 16u;
        v->ph[1] = nb / 2u + ((uint32_t)bg_noise(v) & 0x7FFFu) * nb / 32768u;
        v->ph[0] = (uint32_t)(32767 - (((bg_noise(v) & 0x7FFF) * g * 258) >> 15));
    }
    gust += ((int32_t)v->ph[0] - gust) >> 8;    /* (~0.2 s) */
    ag = 8192 + gust * 3 / 4;                   /* (a lull: a quarter) */
    if (!g) ag = 32767 * 3 / 4 + 8192 / 4;      /* (no gust: steady) */
    k = bg_tab(BG_K, h), comp = bg_tab(BG_C, h);
    cut = 13916 + (VP16(v) - 1104) * 1197 / 64 + (((gust - 16384) * g) >> 12) + vc_fine * 5 / 4 + tune_f * 5 / 4;
    tsvf_coef_k(&c1, cut, k);
    tsvf_coef_k(&c2, cut + 3592 + 40, k);       /* (its octave, a little sharp: the two beat) */
    a = 700 + tn * tn * 23 / 16;                /* (TONE: the rush's one-pole, Q15) */
    bg = h * 258, rg = (32767 - h * 180) * (192 - tn) / 128;   /* (the rush: louder dark, softer bright) */
    for (i = 0; i < (int32_t)CTL; i++) {
        int32_t z = bg_noise(v), b, y;
        b = tsvf_any(&c1, k, FT_BP, z >> 3, &ic1, &ic2) + tsvf_any(&c2, k, FT_BP, z >> 3, &ic3, &ic4) / 3;
        b = (b * comp) >> 9;
        lp += ((z - lp) * a) >> 15;
        y = (mulq15(b, bg) + mulq15(lp, rg));
        y = mulq15(y, ag);
        if (th && ((uint32_t)v->rng & 0xFFFFFFu) < th) se = 14000;   /* (SNOW: a grain) */
        if (se) {
            y += mulq15(z - sp, se) >> 1;       /* (high noise, ~1 ms) */
            se -= (se >> 5) + 1;
            if (se < 0) se = 0;
        }
        sp = z;
        y = soft_knee(y * BG_UP / 2, 20000);
        mono[i] += mulq15(mulq15(y, amp_at(amp0, damp, i)), BG_LV);
    }
    v->s[0] = ic1, v->s[1] = ic2, v->s[2] = lp, v->ph[2] = (uint32_t)ic3, v->ph[3] = (uint32_t)ic4;
    v->ph[4] = (uint32_t)se, v->ph[5] = (uint32_t)sp, v->BG_GUST = gust;
}

static void bg_text(const volatile int32_t *p, uint32_t k, char *s)
{
    int32_t v = clamp(p[k], 0, 127);
    if (k == 2u) { put_s(put_u(s, CUTOFF_HZ[clamp(28 + v * 75 / 127, 0, 127)]), " Hz"); return; }   /* (TONE: ~150 Hz
                                                 * .. ~10 kHz, as its one-pole) */
    put_s(put_u(s, (uint32_t)(v * 100 / 127)), "%");
}

static const engine_t BIEGGA_ENGINE = {"Biegga", {"GUST", "HOWL", "TONE", "SNOW"}, {127, 127, 127, 127}, bg_begin,
                                       bg_block, bg_text};
