/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream p0larpatch: HOME's WARP and MORPH (KNOB 3 and 4), a track's sound twisted and bent, smoothly. Both sit
 * on top of the knobs as DRONE's walks do (drone.c), the knobs never moved: at 0 the sound is exactly as set. Nothing
 * jumps: every move glides, the stepped knobs (MODEL, WAVE, PATCH, SLOT, ..) are left as set, and nothing goes silent.
 *   WARP   0 .. 127: eight layers that come in one after another, each over its eighth of the knob, so the sound grows
 *          from still to wild as it is turned: 1 the voices drift apart (a choir, +-12 cents); 2 the track filter
 *          breathes open (+-25 of 127); 3 the source's knobs wander (a quarter of their range); 4 the source is pushed the way DRONE's storm pushes it (Holvut:
 *          FEEDBACK and DRIVE up, DAMP a little open); 5 a slow vibrato (+-40 cents); 6 the filter's resonance rises and it sweeps wider; 7 the
 *          voices pull far apart (+-57 cents) and the wandering grows deeper and faster; 8 a tape's flutter, the pitch sagging and
 *          rising (+-1 semitone), the push to the full
 *   MORPH  0 .. 127: one knob through all four of the source's at once (its stepped ones aside), on a path of its own
 *          (each track and source its own, always the same), and around that place a slow wandering that grows with
 *          it: turned up, the sound bends through combinations of all four and never quite stands still
 * A knob the storm pushes (DRONE's DR_MAP) moves only that way, so Holvut's howl never falls asleep.
 * Audio ISR, every CH_DIV-th control step (ctl_hook: chaos_ctl); lfo_params adds ch_k to a voice's four, tf_tgt ch_cut,
 * trk_filter ch_res, voices_render a voice's ch_fine (vinc, as DRONE's dr_fine). */

#define CH_DIV 4u                               /* a step every 4 control steps: ~345 a second */
#define CH_NPT 4                                /* MORPH: its path's points past 0 a knob */
#define CH_NW 5u                                /* walks a track: 0 the filter, 1 .. 3 the source's, 4 MORPH's place and
                                                 * the pitch's sag */
#define CH_CENT 19                              /* a cent in tune_f units, x 8 (4096 (2^(1/1200) - 1) = 2.37) */

static struct {
    uint32_t rng, div, vib[NT], flut[NT];
    int32_t tgt[NT][CH_NW], s1[NT][CH_NW], w[NT][CH_NW];   /* the walks: target, glide, now (Q15, +-) */
    uint16_t cnt[NT][CH_NW];
    int32_t k[NT][NPARAM];                      /* the four's offsets, gliding (Q15 of the range) */
    int32_t ft[NV], f1[NV], f[NV];              /* a voice's own drift (Q15, +-) */
    uint16_t fc[NV];
} ch __attribute__((section(".pool")));
static int16_t ch_k[NT][NPARAM];                /* audio: WARP's and MORPH's on a track's four (knob units) */
static int32_t ch_cut[NT], ch_res[NT];          /* .. on its filter (CUTOFF << 8), RESONANCE */
static uint8_t ch_any[NT];                      /* .. ch_k not all 0 */

static int32_t ch_hash(uint32_t x)              /* MORPH's points: -32768 .. 32767, always the same for x */
{
    x ^= x >> 16, x *= 0x7FEB352Du, x ^= x >> 15, x *= 0x846CA68Bu, x ^= x >> 16;
    return (int32_t)(x >> 16) - 32768;
}
static int32_t ch_morph(uint32_t t, uint32_t e, uint32_t k, int32_t x)   /* knob k's place on MORPH's path at x (Q8
                                                 * points, 0 .. CH_NPT << 8), Q15: from 0 at 0, smoothly */
{
    int32_t i, f, a, b;
    uint32_t s = t * 0x9E3779B9u ^ e * 0x85EBCA6Bu ^ k * 0xC2B2AE35u;
    x = clamp(x, 0, (CH_NPT << 8) - 1), i = x >> 8, f = x & 255;
    a = i ? ch_hash(s + (uint32_t)i) : 0, b = ch_hash(s + (uint32_t)i + 1u);
    f = f * f * (768 - 2 * f) >> 16;            /* (smoothstep, 0 .. 256) */
    return a + ((b - a) * f >> 8);
}
static int32_t ch_layer(int32_t x, int32_t i) { return clamp((x - i * 128) * 2, 0, 256); }   /* WARP x (0 .. 1024):
                                                 * layer i's share, 0 .. 256, over its eighth */
static void chaos_ctl(void)                     /* audio, a control step: the walks, WARP's layers, MORPH's place */
{
    uint32_t t, k, i;
    if (++ch.div < CH_DIV) return;
    ch.div = 0;
    for (t = 0; t < NT; t++) {
        int32_t wp = clamp(trk[t].warp, 0, 127), m = clamp(trk[t].morph, 0, 127), x = wp * 1024 / 127;
        int32_t L[8], sm, pos, push, wd, o, mx;
        uint32_t e = (uint32_t)clamp(trk[t].eng, 0, NENG - 1);
        for (i = 0; i < 8u; i++) L[i] = ch_layer(x, (int32_t)i);
        sm = 40 + L[2] * 40 / 256 + L[6] * 200 / 256 + m;   /* (the walks: ~2 s; faster with WARP, and MORPH) */
        for (i = 0; i < CH_NW; i++) {
            if (!ch.cnt[t][i]) {
                ch.tgt[t][i] = dr_fold((ch.tgt[t][i] * 5 >> 3) + dr_bip(&ch.rng) * 3 / 4);
                ch.cnt[t][i] = (uint16_t)((700u + dr_rnd(&ch.rng) % 1400u) * 256u / (256u + (uint32_t)(L[2] + L[6] * 3)));
            } else
                ch.cnt[t][i]--;
            ch.s1[t][i] = dr_glide(ch.s1[t][i], ch.tgt[t][i], sm);
            ch.w[t][i] = dr_glide(ch.w[t][i], ch.s1[t][i], sm);
        }
        ch.vib[t] += 6200000u + (uint32_t)L[7] * 24000u;   /* (the vibrato: 0.5 .. ~1 Hz) */
        ch.flut[t] += 77000000u;                /* (the flutter: ~6.2 Hz) */
        push = (L[3] * 3 + L[7] * 2) * 128 / 5; /* (the storm's push, Q15: 60 % with layer 4, the full with 8) */
        wd = (L[2] * 25 + L[6] * 15) * 128 / 100;   /* (the wandering, Q15: 25 % of the range, then 40 %) */
        pos = m * CH_NPT * 256 / 127 + (ch.w[t][4] * m >> 15) * 96 / 127;   /* (MORPH's place, and its wandering) */
        ch_any[t] = 0;
        for (k = 0; k < NPARAM; k++) {
            int32_t tp = DR_MAP[e][k].tp, j, v;
            mx = ENGINES[e]->max[k];
            if (mx < 32) { ch.k[t][k] = 0, ch_k[t][k] = 0; continue; }   /* (a stepped knob: as set) */
            j = ch.w[t][1u + k % 3u];           /* (four knobs, three walks: the fourth the first's, the other way) */
            if (k == 3u) j = -j;
            v = mulq15(j, wd) + (m ? mulq15(ch_morph(t, e, k, pos), 19660) : 0);   /* (MORPH: 60 % of the range) */
            if (DR_MAP[e][k].w < DR_NW) v = v * DR_MAP[e][k].sw / 25;
            else if (tp > 0) v = v * 3 / 5;     /* (one only the storm pushes (Holvut's DRIVE): less) */   /* (a knob DRONE walks: only as far as it
                                                 * dares, as BORGA's swing: Holvut's FEEDBACK and DAMP a little) */
            if (tp) v = v < 0 ? -v : v;         /* (a knob the storm pushes: up only, louder; one it pulls down
                                                 * (Holvut's DAMP): up only too, darker: opened, it hisses) */
            v += tp * push / (tp < 0 ? 400 : 100);   /* (the push; pulled down, a quarter: no hiss) */
            ch.k[t][k] += (v - ch.k[t][k]) >> 5;   /* (glides, ~0.1 s: a turn of the knob never jumps) */
            o = (ch.k[t][k] * mx + (ch.k[t][k] < 0 ? -16384 : 16384)) >> 15;
            if (e == ENG_HOWL && k == 0u && o > 0)  /* (Holvut's FEEDBACK: never pushed into its overblowing steps,
                                                 * 120 .. 127, where the howl leaps and falls back; set there, left there) */
                o = clamp(119 - trk[t].p[e][0], 0, o);
            ch_k[t][k] = (int16_t)clamp(o, -mx, mx);
            ch_any[t] |= o != 0;
        }
        o = ch.w[t][0] * (L[1] * 25 + L[5] * 20) >> 15;   /* (+-25, then +-45 of 127, << 8) */
        ch_cut[t] = o > 0 ? o : o / 4;          /* (it breathes open; closing, a little: darker, the sound would fade) */
        ch_res[t] = (L[5] * 45) >> 8;
        for (k = 0; k < NV; k++) {              /* the voices: their own drift, the vibrato, the flutter, the sag */
            int32_t c8;
            if (vc[k].trk != t) continue;
            if (!wp || vc[k].stage == ST_OFF) { ch.ft[k] = ch.f1[k] = ch.f[k] = 0, ch_fine[k] = 0; continue; }
            if (!ch.fc[k]) {
                ch.ft[k] = dr_fold((ch.ft[k] * 5 >> 3) + dr_bip(&ch.rng));
                ch.fc[k] = (uint16_t)(500u + dr_rnd(&ch.rng) % 900u);
            } else
                ch.fc[k]--;
            ch.f1[k] = dr_glide(ch.f1[k], ch.ft[k], 60), ch.f[k] = dr_glide(ch.f[k], ch.f1[k], 60);
            c8 = (ch.f[k] * (L[0] * 12 + L[6] * 45) >> 15)                         /* (cents x 256: the drift) */
               + (wear_sine(ch.vib[t] + k * 0x2A000000u) * (L[4] * 40) >> 15)   /* (the vibrato, +-40) */
               + (wear_sine(ch.flut[t] + k * 0x51000000u) * (L[7] * 20) >> 15)  /* (the flutter, +-20) */
               + (ch.w[t][4] * (L[7] * 100) >> 15);                            /* (the sag, +-100) */
            ch_fine[k] = c8 * CH_CENT >> 11;    /* (cents x 256 -> tune_f units) */
        }
    }
}

/* WARP's and MORPH's level: what they do to the sound may make it louder or quieter, so while either is up the track's
 * level is held to where it was as they came up: its level now (rising within ~25 ms, falling over ~0.4 s) against the one it had at 0 (its last
 * ~1.5 s, then held; after a silence, the level it comes back at), the gain between them (0.25 .. 6 x; lifted only
 * while a key holds a note: a tail dying away is left to die) gliding (down at once, up over ~0.2 s, and across each block), in as either
 * knob comes up (all of it by a quarter of the knob). After the track filter (mix_tracks: ch_level, the track's block
 * in place) */
static int32_t ch_fast[NT], ch_ref[NT], ch_lg[NT] = {4096, 4096, 4096, 4096};   /* (levels x 64; the gain, Q12) */
static void ch_level(uint32_t t, int32_t *x, uint32_t n)
{
    int32_t a = 4 * (trk[t].warp > trk[t].morph ? trk[t].warp : trk[t].morph), lv = 0, g, g0 = ch_lg[t], dg;
    uint32_t i, held = 0;
    for (i = 0; i < NV; i++) held |= vc[i].trk == t && (vc[i].stage == ST_ATK || vc[i].stage == ST_SUS);
    for (i = 0; i < n; i += 4u) lv += (x[i] < 0 ? -x[i] : x[i]) >> 6;
    lv = lv / (int32_t)(n / 4u) * 64;
    ch_fast[t] += (lv - ch_fast[t]) >> (lv > ch_fast[t] ? 2 : 6);   /* (up within ~25 ms, down over ~0.4 s) */
    if (ch_fast[t] < 32 || ch_ref[t] < 32) ch_ref[t] = ch_fast[t];   /* (silent, or from silence: as it is) */
    else if (a <= 0) ch_ref[t] += (ch_fast[t] - ch_ref[t]) >> 8;   /* (off: its level the last ~1.5 s; on: held) */
    g = ch_fast[t] > 64 ? clamp((int32_t)(((int64_t)ch_ref[t] << 12) / ch_fast[t]), 1024, 24576) : 4096;
    if (!held && g > 4096) g = 4096;            /* (no key: no tail lifted) */
    a = clamp(a, 0, 127);
    g = 4096 + (g - 4096) * a / 127;
    ch_lg[t] = g0 + ((g - g0) >> (g < g0 ? 1 : 5));   /* (down at once, up over ~0.2 s: no burst) */
    if (g0 == 4096 && ch_lg[t] == 4096) return;
    dg = (ch_lg[t] - g0) * 256 / (int32_t)n;    /* (from the last block's gain to this one's, across the block) */
    for (i = 0; i < n; i++) x[i] = (int32_t)(((int64_t)x[i] * (g0 * 256 + dg * (int32_t)i)) >> 20);
}
