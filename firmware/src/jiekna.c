/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.2) Arctic Scream's JIEKNA source (North Sami jiekŋa: ice, Giellatekno's dictionaries, satni.uit.no; on the screen
 * without its ŋ, as SUOKNA): ice that sings when it is struck or bowed. A voice is a loop tuned to the note, a burst or
 * a bow of noise played into it (after Romain Guy's Adrift, its bucket-brigade resonators: a burst into a delay tuned to
 * the note; written anew, none of its code):
 *
 *   noise (STRIKE: a burst at the note's start .. a bow all along) -> + the loop -> the line (the note's period) ->
 *   an all-pass (BRITTLE: glass, a bell, a bar of metal) -> a one-pole (the ice's damping) -> x RING (its sign: HOLLOW)
 *   -> round again
 *
 *   STRIKE   0 a bow (the noise all along, soft: a glass rubbed, a frozen string bowed) .. 127 a strike (a burst at the
 *            note's start, gone in a few ms: a bell of ice, a stone on the lake)
 *   RING     how long it sings: 0 a knock .. 100 seconds .. 127 for ever (over 1: it sings on by itself, held in by a
 *            soft knee: an endless drone of ice)
 *   HOLLOW   0 .. 63 the loop as it is (every harmonic: a string); 64 .. 127 turned over (the odd ones: a pipe, the
 *            hollow tone of ice over water), the line halved to keep the note; within each half, darker .. brighter
 *   BRITTLE  the all-pass: 0 none (in tune: a string) .. 127 its overtones pulled apart (glass, a bell, metal)
 * The line in the voice's pool scratch (voices.c vx_pool); its state in s[] and ph[]. Fixed point. */

#define JN_BL VX_POOL                           /* the line (frames, a power of two) */
#define JN_LV 30000                             /* a voice's level (Q15) */
_Static_assert((JN_BL & (JN_BL - 1u)) == 0u, "the line a power of two");
#define JN_LP s[0]                              /* the damping's state */
#define JN_AX s[1]                              /* the all-pass's last in, */
#define JN_AY s[2]                              /* .. and out */
#define JN_HIT s[3]                             /* the strike's burst, fading (Q15) */
#define JN_W ph[0]                              /* where the line is written */
#define JN_BOW ph[1]                            /* the bow's noise, low-passed */
#define JN_DCX ph[2]                            /* the DC blocker's last in, */
#define JN_DCY ph[3]                            /* .. and out */

static int32_t jn_noise(voice_t *v)            /* -16384 .. 16383 */
{
    uint32_t s = (uint32_t)v->rng;
    s ^= s << 13, s ^= s >> 17, s ^= s << 5;
    v->rng = (int32_t)s;
    return (int32_t)s >> 17;
}

static void jn_begin(voice_t *v)
{
    uint32_t k, *w = (uint32_t *)vx_pool[v - vc];
    for (k = 0; k < JN_BL / 2u; k++) w[k] = 0;  /* (a note from silence: the line quiet) */
    for (k = 0; k < 4u; k++) v->s[k] = 0;
    for (k = 0; k < VPH; k++) v->ph[k] = 0;
    if (!v->rng) v->rng = 0x5A17C0DE;
    v->JN_HIT = clamp(trk[v->trk % NT].p[ENG_JIEKNA][0], 0, 127) * 258;   /* (STRIKE: the burst's height) */
}

static void jn_block(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1)
{
    int16_t *line = vx_pool[v - vc];
    int32_t st = clamp(p[0], 0, 127), rg = clamp(p[1], 0, 127), ho = clamp(p[2], 0, 127), br = clamp(p[3], 0, 127);
    int32_t inv = ho >= 64, tone = inv ? ho - 64 : ho, dmp = amp1 - amp0, i;
    int32_t g = 29000 + rg * rg * 255 / 1000;   /* (RING: x 0.885 .. ~1.01 a turn, Q15) */
    int32_t c = br * 154, b = 15000 + tone * 270;   /* (BRITTLE: the all-pass's, 0 .. 0.6; the damping's one-pole, Q15) */
    int32_t bow = (127 - st) * 3, lp = v->JN_LP, ax = v->JN_AX, ay = v->JN_AY, hit = v->JN_HIT;
    int32_t bw = (int32_t)v->JN_BOW, dx = (int32_t)v->JN_DCX, dy = (int32_t)v->JN_DCY;
    uint32_t inc = vinc(VP16(v)) >> 8, per, w = v->JN_W, pi, pf;
    per = 0xFFFFFFFFu / (inc ? inc : 1u);       /* the note's period, frames Q8 */
    if (inv) per >>= 1;                         /* (turned over: it sounds an octave down; half the line, the note) */
    per = per > 256u + (uint32_t)c / 64u ? per - 256u - (uint32_t)c / 64u : 256u;   /* (the filters' own delay, about) */
    while (per > (JN_BL - 4u) << 8) per >>= 1; /* (below ~11 Hz: an octave up) */
    if (per < 2u << 8) per = 2u << 8;
    pi = per >> 8, pf = per & 255u;
    for (i = 0; i < (int32_t)CTL; i++) {
        int32_t r1 = line[(w - pi) & (JN_BL - 1u)], r0 = line[(w - pi - 1u) & (JN_BL - 1u)];
        int32_t y = r1 + (((r0 - r1) * (int32_t)pf) >> 8), z = jn_noise(v), a, x;
        a = ((c * (y - ay)) >> 15) + ax;        /* BRITTLE: the all-pass */
        ax = y, ay = a;
        lp += ((a - lp) * b) >> 15;             /* the ice's damping */
        x = (lp * g) >> 15;                     /* RING */
        if (inv) x = -x;                        /* HOLLOW */
        bw += ((z - bw) * 6000) >> 15;          /* (the bow: soft noise, all along) */
        x += (bw * bow) >> 8;
        if (hit) x += (z * hit) >> 15, hit -= (hit >> 7) + 1;   /* (the strike: ~3 ms) */
        if (hit < 0) hit = 0;
        dy = x - dx + ((dy * 32752) >> 15), dx = x;   /* (no DC round the loop: ~2.5 Hz, so the note stays in tune) */
        x = soft_knee(dy, 20000);               /* (RING over 1: held in) */
        x = clamp(x, -32767, 32767);
        line[w++ & (JN_BL - 1u)] = (int16_t)x;
        mono[i] += mulq15(mulq15(x, amp_at(amp0, dmp, i)), JN_LV);
    }
    v->JN_LP = lp, v->JN_AX = ax, v->JN_AY = ay, v->JN_HIT = hit;
    v->JN_W = w, v->JN_BOW = (uint32_t)bw, v->JN_DCX = (uint32_t)dx, v->JN_DCY = (uint32_t)dy;
}

static void jn_text(const volatile int32_t *p, uint32_t k, char *s)
{
    int32_t v = clamp(p[k], 0, 127);
    if (k == 0u) { put_s(s, v < 20 ? "BOW" : v > 107 ? "STRIKE" : v < 64 ? "BOW + STRIKE" : "STRIKE + BOW"); return; }
    if (k == 2u) { put_s(put_u(put_s(s, v >= 64 ? "PIPE " : "STRING "), (uint32_t)((v & 63) * 100 / 63)), "%"); return; }
    if (k == 1u && v >= 121) { put_s(s, "ENDLESS"); return; }
    put_s(put_u(s, (uint32_t)(v * 100 / 127)), "%");
}

static const engine_t JIEKNA_ENGINE = {"Jiekna", {"STRIKE", "RING", "HOLLOW", "BRITTLE"}, {127, 127, 127, 127}, jn_begin,
                                       jn_block, jn_text};
