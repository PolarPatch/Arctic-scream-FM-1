/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.8) DRIFT (HOME, tapped a fourth time: MIXER 3; after TheGrizard's After Images' DRIFT, its ambient mixer:
 * written anew for Arctic Scream): the mix moving by itself, slowly, so a drone never settles.
 *   AMOUNT     0 .. 100 %: how far the tracks' levels wander. 0: DRIFT off, the mix exactly as set. Up, now and then a
 *              new mix: one sounding track near full, the others somewhere under it (at AMOUNT 100 down to ~ -16 dB),
 *              sometimes one left almost alone with the others far off. T1 never goes far: the ground under it all
 *   PACE       0 .. 100 %: how often a new mix comes: ~2 min at 0, ~15 s at 100 (each a little apart). The levels
 *              always glide there over seconds, never a step
 *   WANDER     0 .. 100 %: how far each track walks round the stereo field from its PAN (T1 half as far)
 *   DISTANCE   0 .. 100 %: a track fading goes into the distance, not only quieter: more of it into SPACE (its REV
 *              send up) and its track filter darker (LP down, HP up: thinner), as far as it has gone
 * Only sounding tracks are given the top of the mix (one sounding alone: it stays full). With JIEHKKI playing, its own
 * presence moves and DRIFT's multiply. The main loop decides (df_tick, every DF_TICK ms); the audio ISR ramps the
 * gains (df_trk) and takes the pan, the send and the filter's move as they are (mix_tracks, tf_tgt). AMOUNT, PACE,
 * WANDER and DISTANCE are the project's (store.c, version 13). */

#define DF_TICK 50u                             /* ms between the main loop's looks */
static volatile int32_t df_amt, df_pace = 50, df_wand = 50, df_dist = 50;   /* the knobs: OFF, 50 %, 50 %, 50 % */
static volatile int32_t df_tg[NT] = {32767, 32767, 32767, 32767};   /* main loop -> audio: a track's gain (Q15) */
static volatile int32_t df_pan[NT], df_rev[NT], df_cut[NT];   /* .. its pan's move (-63 .. 63), its REV send's (0 .. 64),
                                                 * its filter's (CUTOFF << 8) */
static volatile uint32_t df_seen;               /* audio -> main loop: the tracks that sounded (bit t) */
static int32_t df_tga[NT] = {32767, 32767, 32767, 32767};   /* audio: as ramped */
static struct {
    uint32_t last, next, pan_next[NT], heard[NT], rng;
    int32_t g[NT], g_t[NT], p[NT], p_t[NT];     /* the level (Q15) and the walk (-32767 .. 32767): now, to glide to */
} df = {.rng = 0x6C8E9CF5u, .g = {32767, 32767, 32767, 32767}, .g_t = {32767, 32767, 32767, 32767}};

static uint32_t df_rnd(uint32_t n)
{
    df.rng ^= df.rng << 13, df.rng ^= df.rng >> 17, df.rng ^= df.rng << 5;
    return (uint32_t)(((uint64_t)df.rng * n) >> 32);
}
static void df_deal(uint32_t now, uint32_t live)   /* a new mix: where each track's level goes */
{
    int32_t a = clamp(df_amt, 0, 100), pace = clamp(df_pace, 0, 100), lo, f, t, n = 0, top, solo;
    uint32_t gap = 120000u - (uint32_t)pace * 1050u;   /* (~120 s .. ~15 s) */
    df.next = now + gap * 3u / 4u + df_rnd(gap / 2u);
    for (t = 0; t < (int32_t)NT; t++) n += (int32_t)(live >> t & 1u);
    if (n < 2) {                                /* (one track sounding, or none: it stays as set) */
        for (t = 0; t < (int32_t)NT; t++) df.g_t[t] = 32767;
        return;
    }
    top = (int32_t)df_rnd((uint32_t)n);         /* (the one near full: among the sounding) */
    for (t = 0; t < (int32_t)NT; t++)
        if (live >> t & 1u && !top--) break;
    top = t;
    solo = df_rnd(100) < 30u;                   /* (now and then: that one almost alone) */
    for (t = 0; t < (int32_t)NT; t++) {
        lo = solo ? 1600 + (int32_t)df_rnd(8000u) : 5000 + (int32_t)df_rnd(27767u);   /* (the far ones: ~ -26 .. -12 dB;
                                                 * else ~ -16 .. 0 dB) */
        if (t == top) lo = 32767 - (int32_t)df_rnd(2000u);
        if (t == 0) lo = lo > 20000 ? lo : 20000;   /* (T1, the ground: never under ~ -4 dB) */
        f = 32767 - (int32_t)((int64_t)(32767 - lo) * a / 100);   /* (AMOUNT: how far of the way) */
        df.g_t[t] = f;
    }
}
static void df_tick(uint32_t now)               /* main loop, each pass */
{
    int32_t a = clamp(df_amt, 0, 100), w = clamp(df_wand, 0, 100), dist = clamp(df_dist, 0, 100), t;
    uint32_t live = 0, seen;
    if (now - df.last < DF_TICK) return;
    df.last = now;
    seen = df_seen, df_seen = 0;
    for (t = 0; t < (int32_t)NT; t++) {
        if (seen >> t & 1u) df.heard[t] = now;
        if (df.heard[t] && now - df.heard[t] < 4000u) live |= 1u << t;
    }
    if (!a) {                                   /* (off: everything back to as set, gliding) */
        for (t = 0; t < (int32_t)NT; t++) df.g_t[t] = 32767, df.p_t[t] = 0;
        df.next = now;
    } else {
        if ((int32_t)(now - df.next) >= 0) df_deal(now, live);
        for (t = 0; t < (int32_t)NT; t++)
            if ((int32_t)(now - df.pan_next[t]) >= 0) {   /* (the walk: a new place every 5 .. 15 s) */
                df.pan_next[t] = now + 5000u + df_rnd(10000u);
                df.p_t[t] = (int32_t)df_rnd(65535u) - 32767;
            }
    }
    for (t = 0; t < (int32_t)NT; t++) {         /* (glide: ~6 s the level, ~8 s the walk) */
        int32_t far, pv;
        df.g[t] += (df.g_t[t] - df.g[t]) / 128 + (df.g_t[t] > df.g[t] ? 1 : df.g_t[t] < df.g[t] ? -1 : 0);
        df.p[t] += (df.p_t[t] - df.p[t]) / 160 + (df.p_t[t] > df.p[t] ? 1 : df.p_t[t] < df.p[t] ? -1 : 0);
        df_tg[t] = df.g[t];
        pv = df.p[t] / 520 * w / 100;           /* (-63 .. 63; AMOUNT 0: the walk glides home) */
        df_pan[t] = t == 0 ? pv / 2 : pv;
        far = 32767 - df.g[t];                  /* (how far off it has gone, Q15) */
        df_rev[t] = far * dist / 100 * 64 / 32767;   /* (at the farthest: up to + 64 into SPACE) */
        df_cut[t] = (int32_t)((int64_t)far * dist / 100 * (40 << 8) / 32767);   /* (.. and up to 40 steps darker) */
        if (df_rev[t] > 64) df_rev[t] = 64;
        if (df_cut[t] > 40 << 8) df_cut[t] = 40 << 8;
    }
}
static int32_t df_trk(uint32_t t)               /* audio, a block: track t's gain (Q15), ramped */
{
    int32_t g = df_tga[t], d = df_tg[t] - g;
    g += d > 64 ? 64 : d < -64 ? -64 : d;
    return df_tga[t] = g;
}
static int32_t df_cut_of(uint32_t t)            /* the filter's move by its TYPE: LP darker, HP thinner, else none */
{
    uint32_t ty = (uint32_t)clamp(trk[t].ftype, 0, NFTYPE - 1);
    return ty == FT_LP ? -df_cut[t] : ty == FT_HP ? df_cut[t] / 2 : 0;
}
