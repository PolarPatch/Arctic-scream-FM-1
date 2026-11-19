/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.4) TIDE (LFO, tapped a sixth time): the whole sound ebbing now and then, far down for a few minutes, and coming
 * back, quicker than it left; and GUARD, a slow hand on the level that keeps a swell from towering over the music
 * around it. The ideas follow Tomorrow's Man's SAMSARA (the tide and the energy governor over AMB-1, MPL-2.0),
 * written anew; none of its code.
 *   TIDE    OFF, RARE (an ebb every 40 .. 70 minutes), SOMETIMES (15 .. 30), OFTEN (5 .. 10)
 *   DEPTH   0 .. 100 %: how far an ebb goes (100 %: 36 dB down, all but silent)
 *   GUARD   0 .. 100 %: the level heard over the last two seconds against the long one's (a minute down, ~30 s up: a
 *           sound that stays is the music's level in time; its first 4 s, the level it settles at): more than 2 dB over it,
 *           the excess taken off at GUARD's share, slowly (in at 1 dB a second, out at 0.3): a swell, a stacked chord,
 *           a howl rising, all kept about the music's own level; never lifted, only held down (at most 12 dB)
 *   NOW     IN, OUT: where the tide is; turned to OUT, an ebb now; to IN, back now
 * An ebb: down at 0.15 dB a second to DEPTH (a 25 dB ebb takes nearly three minutes), there one to three minutes, back
 * at 0.3. Both act on the whole output (the speaker, the jack, the USB recording) before MASTER, the effects' tails
 * too. Nothing else changes: TIDE OFF and GUARD 0 (the start) leave the sound as it was, bit for bit. Saved with the
 * project (store.c, in the extension's spare bytes: older projects have it off). The main loop times the tide
 * (td_tick); the audio ISR measures and applies (td_block). */

enum { TD_OFF, TD_RARE, TD_SOME, TD_OFTEN, TD_NMODE };
static const char *const TD_NAME[TD_NMODE] = {"OFF", "RARE", "SOMETIMES", "OFTEN"};
static const char *const TD_SHORT[TD_NMODE] = {"OFF", "RARE", "SOME", "OFTEN"};
static const uint16_t TD_GAP[TD_NMODE][2] = {{0, 0}, {40, 70}, {15, 30}, {5, 10}};   /* minutes between ebbs */
enum { TS_IN, TS_EBB, TS_OUT, TS_BACK };        /* the tide: in, going out, out, coming back */
#define TD_OCT 65536                            /* log2 units (Q16): an octave of level, 6.02 dB */
#define TD_DB(x) ((int32_t)((x) * 10886.0))     /* dB -> log2 Q16 (constants only) */
static volatile int32_t td_mode, td_depth = 70, td_guard, td_now;   /* the page's four (main loop) */
static uint32_t td_st, td_next, td_hold, td_t;  /* main loop: the tide's state; when the next ebb is due (ms); how
                                                 * long out; its last step */
static volatile int32_t td_lv;                  /* main loop -> audio: the tide's level (log2 Q16, <= 0) */
static uint32_t td_settle;                      /* audio: blocks measured since a start (to 690) */
static int32_t td_l3, td_l60 = -1, td_gov;      /* audio: the 3 s and the minute's level (log2 Q16; Q20 the minute's),
                                                 * GUARD's hand (log2 Q16, <= 0) */
static volatile int32_t td_gov_shown;           /* audio -> screen: GUARD's hand now */
static int32_t td_g = 4096;                     /* audio: the gain at the last block's end (Q12) */
static uint32_t td_rng = 0x2F6B1D9Cu;
#ifndef TD_SPEED
#define TD_SPEED 1u                             /* (tests: minutes as seconds at 60) */
#endif

static uint32_t td_rnd(uint32_t n)
{
    td_rng ^= td_rng << 13, td_rng ^= td_rng >> 17, td_rng ^= td_rng << 5;
    return n ? td_rng % n : 0u;
}
static int32_t td_log2(uint32_t x)              /* log2(x), Q16, x >= 1: the octave and a straight line within it
                                                 * (within 0.09 dB of it: enough for a meter) */
{
    int32_t e = 0;
    if (!x) return 0;
    while (x >> (e + 1)) e++;
    return (e << 16) + (int32_t)((e >= 16 ? x >> (e - 16) : x << (16 - e)) - 65536u);
}
static int32_t td_gain(int32_t l)               /* 2^(l / 65536), Q12, l <= 0 */
{
    static const uint16_t E[33] = {4096, 4008, 3922, 3838, 3756, 3675, 3596, 3519, 3444, 3370, 3298, 3227, 3158,
        3090, 3024, 2959, 2896, 2834, 2773, 2714, 2655, 2598, 2543, 2488, 2435, 2383, 2332, 2282, 2233, 2185, 2138, 2092,
        2048};                                  /* 2^(-i / 32), Q12 */
    uint32_t o, f, i;
    if (l >= 0) return 4096;
    l = -l;
    o = (uint32_t)l >> 16, f = (uint32_t)l & 65535u, i = f >> 11;
    if (o >= 12u) return 0;
    return (int32_t)((E[i] + (((int32_t)E[i + 1u] - (int32_t)E[i]) * (int32_t)(f & 2047u) >> 11)) >> o);
}
static void td_due(uint32_t ms)                 /* the next ebb's time, from now */
{
    uint32_t m = (uint32_t)clamp(td_mode, 0, TD_NMODE - 1), lo = TD_GAP[m][0], hi = TD_GAP[m][1];
    td_next = ms + (lo * 60000u + td_rnd((hi - lo) * 60000u + 1u)) / TD_SPEED;
}
static void td_tick(uint32_t ms)                /* main loop, every pass: the tide */
{
    int32_t lv = td_lv, bottom = -TD_DB(36.0) / 100 * clamp(td_depth, 0, 100), dt;
    static int32_t mode_was, now_was;
    if (td_mode != mode_was) {                  /* (switched: the next ebb from now; OFF, back) */
        mode_was = td_mode;
        if (td_mode != TD_OFF) td_due(ms);
        else if (td_st != TS_IN) td_st = TS_BACK;
    }
    if (td_now != now_was) {                    /* (NOW turned: out now, or back now) */
        now_was = td_now;
        if (td_now && (td_st == TS_IN || td_st == TS_BACK)) td_st = TS_EBB;
        else if (!td_now && td_st != TS_IN) td_st = TS_BACK;
    }
    if (ms - td_t < 50u) return;                /* (a step every 50 ms: the slopes whole numbers) */
    dt = (int32_t)(ms - td_t) * (int32_t)TD_SPEED;
    td_t = ms;
    switch (td_st) {
    case TS_IN:
        if (td_mode != TD_OFF && (int32_t)(ms - td_next) >= 0) td_st = TS_EBB;
        break;
    case TS_EBB:                                /* (0.15 dB a second) */
        lv -= TD_DB(0.15) * dt / 1000;
        if (lv <= bottom) lv = bottom, td_st = TS_OUT, td_hold = ms + (60000u + td_rnd(120001u)) / TD_SPEED;
        break;
    case TS_OUT:
        lv = lv < bottom ? lv + TD_DB(0.3) * dt / 1000 : bottom;   /* (DEPTH turned meanwhile: there) */
        if ((int32_t)(ms - td_hold) >= 0) td_st = TS_BACK;
        break;
    default:                                    /* TS_BACK (0.3 dB a second) */
        lv += TD_DB(0.3) * dt / 1000;
        if (lv >= 0) {
            lv = 0, td_st = TS_IN;
            if (td_mode != TD_OFF) td_due(ms);
        }
        break;
    }
    td_lv = lv;
    td_now = now_was = td_st == TS_EBB || td_st == TS_OUT;   /* (the knob shows where it is) */
}
/* audio, a block of n frames (L R, before MASTER): measured, then the tide and GUARD's hand on it (ramped over the
 * block); none of it while both are off and at rest */
static void td_block(int32_t *out, uint32_t n)
{
    uint32_t i, s = 0;
    int32_t l, g, gn, d, ex;
    if (!td_guard && !td_lv && !td_gov && td_g == 4096) {
        td_l60 = -1;                            /* (the meters start again when it is on) */
        return;
    }
    for (i = 0; i < 2u * n; i += 8u) {          /* the level: the mean of |x| (every 4th frame) */
        int32_t x = out[i];
        s += (uint32_t)(x < 0 ? -x : x) >> 4;
    }
    l = td_log2(s / (n / 4u ? n / 4u : 1u) + 1u);
    if (l >= TD_OCT * 4) {                      /* (the quiet below the floor is not the music's level: not measured) */
        if (td_l60 < 0) td_l3 = l, td_l60 = l << 4, td_settle = 0;   /* (a start: both here) */
        td_l3 += (l - td_l3) / 345;             /* (~2 s at 5.8 ms a block) */
        if (td_settle < 690u) td_settle++, td_l60 = td_l3 << 4;   /* (its first 4 s: the sound settling is its level) */
        else td_l60 += ((l << 4) - td_l60) / ((l << 4) > td_l60 ? 5170 : 10240);   /* (up in ~30 s, down in a
                                                 * minute: a sound that stays is the music's level in time) */
    }
    ex = td_l3 - (td_l60 >> 4) - TD_DB(2.0);
    ex = ex <= 0 || td_l60 < 0 ? 0 : (ex > 2 * TD_OCT ? 2 * TD_OCT : ex) * clamp(td_guard, 0, 100) / 100;   /* (the
                                                 * quiet below the floor: never held) */
    d = -ex - td_gov;                           /* (the hand toward it: in at 1 dB a second, out at 0.3) */
    td_gov += d < 0 ? (d < -63 ? -63 : d) : (d > 19 ? 19 : d);
    td_gov_shown = td_gov;
    gn = td_gain(td_lv + td_gov);
    g = td_g;
    for (i = 0; i < n; i++) {
        int32_t gi = g + (gn - g) * (int32_t)(i + 1u) / (int32_t)n;
        out[2u * i] = mul64_sh(out[2u * i], gi, 12), out[2u * i + 1u] = mul64_sh(out[2u * i + 1u], gi, 12);
    }
    td_g = gn;
}
