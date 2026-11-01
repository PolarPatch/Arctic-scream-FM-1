/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream p0larpatch: HOME's CHAOS and MORPH (KNOB 3 and 4), a track's sound torn about and bent. Both sit on top
 * of the knobs as DRONE's walks do (drone.c), the knobs never moved: at 0 the sound is exactly as set.
 *   CHAOS  0 .. 127: the sound will not sit still. At random times, from about once a second to ~20 times a second as
 *          it is turned up, the source's four knobs jump to new places (up to 3/4 of their range, the stepped ones too:
 *          MODEL, WAVE, ..), pushed the way DRONE's storm pushes them (Holvut: FEEDBACK and DRIVE up, DAMP open; a
 *          knob the storm pushes moves only that way, so the howl never falls asleep), the
 *          track filter jumps and its resonance rises, and the voices leap octaves and fifths; past 3/4 any interval,
 *          a little out of tune, and short silences. Its depth grows as its square: low, a sound that stirs; full, havoc
 *   MORPH  0 .. 127: one knob through all four of the source's at once, on a path of its own (each track and source its
 *          own, always the same): turned up, the sound bends through combinations of all four, MODEL too
 * Audio ISR, every CH_DIV-th control step (ctl_hook: chaos_ctl); lfo_params adds ch_k to a voice's four, tf_tgt ch_cut,
 * trk_filter ch_res, VP16 a voice's ch_p16 (voices.c), voices_render its ch_gain. */

#define CH_DIV 4u                               /* a step every 4 control steps: ~345 a second */
#define CH_HZ 345u
#define CH_NPT 4                                /* MORPH: its path's points past 0 a knob */
static const int8_t CH_LEAP[8] = {12, -12, 7, -5, 19, -24, 24, 5};   /* CHAOS's leaps (semitones) below 3/4 */

static struct {
    uint32_t rng, cnt[NT];
    int32_t tgt[NT][NPARAM], cur[NT][NPARAM];   /* the four's jumps (Q15, +-), glided */
    int32_t cut_t[NT], cut_c[NT], res_t[NT], res_c[NT];
    uint32_t div;
} ch __attribute__((section(".pool")));
static int16_t ch_k[NT][NPARAM];                /* audio: CHAOS's and MORPH's on a track's four (knob units) */
static int32_t ch_cut[NT], ch_res[NT];          /* .. on its filter (CUTOFF << 8), RESONANCE */
static uint8_t ch_any[NT];                      /* .. ch_k not all 0 */

static int32_t ch_hash(uint32_t x)              /* MORPH's points: -32768 .. 32767, always the same for x */
{
    x ^= x >> 16, x *= 0x7FEB352Du, x ^= x >> 15, x *= 0x846CA68Bu, x ^= x >> 16;
    return (int32_t)(x >> 16) - 32768;
}
static int32_t ch_morph(uint32_t t, uint32_t e, uint32_t k, int32_t m)   /* MORPH m (0 .. 127): knob k's place on its
                                                 * path, Q15; from 0 at 0 through CH_NPT points, smoothly */
{
    int32_t x = m * CH_NPT * 256 / 127, i = x >> 8, f = x & 255, a, b;
    uint32_t s = t * 0x9E3779B9u ^ e * 0x85EBCA6Bu ^ k * 0xC2B2AE35u;
    if (i >= CH_NPT) return ch_hash(s + CH_NPT);
    a = i ? ch_hash(s + (uint32_t)i) : 0, b = ch_hash(s + (uint32_t)i + 1u);
    f = f * f * (768 - 2 * f) >> 16;            /* (smoothstep, 0 .. 256) */
    return a + ((b - a) * f >> 8);
}
static void ch_jump(uint32_t t, int32_t c)      /* CHAOS c: a new place for the four, the filter, the voices */
{
    uint32_t k, r;
    for (k = 0; k < NPARAM; k++)
        if (dr_rnd(&ch.rng) % 10u < 7u) ch.tgt[t][k] = dr_bip(&ch.rng);
    ch.cut_t[t] = dr_bip(&ch.rng), ch.res_t[t] = (int32_t)(dr_rnd(&ch.rng) >> 17);
    for (k = 0; k < NV; k++) {
        if (vc[k].trk != t || vc[k].stage == ST_OFF) continue;
        r = dr_rnd(&ch.rng);
        ch_p16[k] = 0, ch_gain[k] = 32767;
        if ((int32_t)(r % 128u) < c * 3 / 4) {  /* (a leap: more often, the further it is turned) */
            if (c < 96) ch_p16[k] = CH_LEAP[r >> 8 & 7u] * 16;
            else ch_p16[k] = (int32_t)((r >> 12) % 769u) - 384;   /* (any, +-2 octaves, off the semitones) */
        }
        if (c > 90 && (int32_t)(r >> 20 & 127u) < (c - 90) * 3 / 4) ch_gain[k] = 0;   /* (a short silence) */
    }
    r = 800u + (uint32_t)(c * c) * 13u / 10u;   /* (jumps a second x 1000: 0.8 .. ~22) */
    ch.cnt[t] = CH_HZ * 1000u / r * (256u + (dr_rnd(&ch.rng) >> 23)) / 512u + 1u;   /* (x 1/2 .. 3/2) */
}
static void chaos_ctl(void)                     /* audio, a control step: CHAOS's jumps, MORPH's place, what they move */
{
    uint32_t t, k;
    if (++ch.div < CH_DIV) return;
    ch.div = 0;
    for (t = 0; t < NT; t++) {
        int32_t c = clamp(trk[t].chaos, 0, 127), m = clamp(trk[t].morph, 0, 127), amp = c * c / 127, o, mx;
        uint32_t e = (uint32_t)clamp(trk[t].eng, 0, NENG - 1);
        if (c && dr_sounds(t)) {
            if (!ch.cnt[t] || --ch.cnt[t] == 0) ch_jump(t, c);
        } else {
            c = amp = 0, ch.cnt[t] = 0;
            for (k = 0; k < NV; k++)
                if (vc[k].trk == t) ch_p16[k] = 0, ch_gain[k] = 32767;
        }
        ch_any[t] = 0;
        for (k = 0; k < NPARAM; k++) {          /* the four: CHAOS's jumps (glided ~10 ms) and push, MORPH's place */
            ch.cur[t][k] += (ch.tgt[t][k] - ch.cur[t][k]) >> 2;
            int32_t j = ch.cur[t][k], tp = DR_MAP[e][k].tp, w = m ? ch_morph(t, e, k, m) : 0;
            if (tp) j = tp > 0 ? (j < 0 ? -j : j) : (j > 0 ? -j : j), w = tp > 0 ? (w < 0 ? -w : w) : (w > 0 ? -w : w);
                                                /* (a knob the storm pushes: only that way, louder, never quieter:
                                                 * Holvut's FEEDBACK never under its threshold) */
            mx = ENGINES[e]->max[k];
            o = mulq15(j, amp * 196) + tp * 327 * c / 127 + mulq15(w, 28000);   /* (3/4 of the range; the push) */
            o = (o * mx + (o < 0 ? -16384 : 16384)) >> 15;
            ch_k[t][k] = (int16_t)clamp(o, -mx, mx);
            ch_any[t] |= o != 0;
        }
        ch.cut_c[t] += (ch.cut_t[t] - ch.cut_c[t]) >> 3, ch.res_c[t] += (ch.res_t[t] - ch.res_c[t]) >> 3;
        ch_cut[t] = (ch.cut_c[t] * 48 >> 7) * amp / 127;   /* (+-48 of 127, << 8) */
        ch_res[t] = (ch.res_c[t] * 70 >> 15) * amp / 127 + 20 * c / 127;   /* (up to + 90) */
    }
}
