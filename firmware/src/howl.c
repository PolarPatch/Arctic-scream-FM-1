/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic p0lar patches' HOWL source: a feedback loop that wakes on its own, after the design of Audrey II (Nick Donaldson,
 * Infrasonic Audio; Synthux Academy, MIT: github.com/Synthux-Academy/Audrey-II, FeedbackSynthEngine.cpp), written anew in
 * integers for the FM-1 (no FPU); none of its code is copied. A voice's loop:
 *
 *   a breath of noise (inaudible) + BODY's line -> a Karplus-Strong string at the note -> DRIVE's saturation -> DAMP's
 *   low-pass, a high-pass (~70 Hz) -> two all-passes (Audrey's reverb in the loop, small) -> x FEEDBACK -> BODY's line
 *
 * Nothing is struck: the string only hears the noise and what comes round, so the voice is near silent until FEEDBACK
 * lifts the loop's gain past one; then the sound grows out of the noise by itself and howls (the saturation holds it).
 * A key adds a short burst, so a note near the threshold provokes it. The filters in the loop pick which overtones scream;
 * BODY glides (~0.2 s, Audrey's smoothing), so turning it bends the whole sound, and a slow random wander moves it a
 * little all the time (after Audrey Touch's sample-and-hold on its body).
 *   FEEDBACK   the loop's gain, -60 .. +12 dB (it wakes around 2/3 of the way)
 *   BODY       the second line, ~1 .. ~17 ms: a resonance of its own beside the note's, metal at the short end
 *   DAMP       the loop's low-pass, ~18 kHz .. ~100 Hz: a scream .. a growl under the ice
 *   DRIVE      the saturation, 1x .. 7x: clean .. torn
 * State: howl_t (howl.h) in the voice's PHYS slot. */

#define HW_KICK 2205u                           /* a note's burst: 50 ms */
#define HW_NOISE 28                             /* the breath: >> this of the noise (~-90 dB) */

static howl_t *hw_of(voice_t *v) { return &ph_slot[v - vc].u.h; }
static uint32_t hw_rnd(howl_t *S)
{
    uint32_t s = S->rng;
    s ^= s << 13, s ^= s >> 17, s ^= s << 5;
    return S->rng = s;
}
static int32_t hw_body(int32_t body)            /* BODY: its delay, frames Q8, ~1 .. ~17 ms (44 x 2^(0 .. 4.08)) */
{
    return (int32_t)((44u * pow2_q16(clamp(body, 0, 127) * 784 / 127)) >> 8);
}
static int32_t hw_gain(int32_t fb)              /* FEEDBACK: -60 .. +12 dB, Q16 */
{
    return (int32_t)(pow2_q16(clamp(fb, 0, 127) * 2297 / 127) >> 10);
}
static uint32_t hw_cut(int32_t dmp)             /* DAMP: the low-pass's corner, Hz: ~18 kHz .. ~100 Hz */
{
    return (18000u * pow2_q16(-clamp(dmp, 0, 127) * 1440 / 127)) >> 16;
}

static void howl_begin(voice_t *v)              /* a note from silence: the loop empty, a burst to wake it */
{
    howl_t *S = hw_of(v);
    uint32_t i, *w = (uint32_t *)S;
    for (i = 0; i < sizeof *S / 4u; i++) w[i] = 0;
    ph_slot[v - vc].model = 0xFF;               /* (not a PHYS model: PHYS begins anew) */
    S->rng = (uint32_t)v->rng | 1u;
    S->kick = HW_KICK;
    S->bd = hw_body(trk[v->trk].p[ENG_HOWL][1]);
}

/* the saturation: tanh's rational fit x (27 + x^2) / (27 + 9 x^2), flat from 3; Q15 in and out */
static inline int32_t hw_sat(int32_t x)
{
    int32_t x12 = clamp(x >> 3, -12288, 12288), x2 = (x12 * x12) >> 12;
    int32_t y = x12 * ((110592 + x2) >> 4) / ((110592 + 9 * x2) >> 4);
    return clamp(y * 8, -32767, 32767);
}
static inline int32_t hw_ap(int16_t *b, uint16_t *ix, uint32_t n, int32_t x)   /* an all-pass, g 0.6 */
{
    int32_t d = b[*ix], y = d - ((x * 19661) >> 15);
    b[*ix] = (int16_t)clamp(x + ((y * 19661) >> 15), -32767, 32767);
    if (++*ix == n) *ix = 0;
    return y;
}

/* CTL frames of voice v, added to mono[] */
static void howl_block(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1)
{
    howl_t *S = hw_of(v);
    uint32_t inc = vinc(VP16(v)) >> 8, per, fc = hw_cut(p[2]), i;
    int32_t g = hw_gain(p[0]) >> 4, drv = 256 + clamp(p[3], 0, 127) * 12, dmp = amp1 - amp0;
    int32_t x = (int32_t)(fc * 4669u / 1000u), a = x * 16384 / (32768 + x);   /* (DAMP's one-pole, Q14) */
    int32_t tgt, bd0 = S->bd, dbd;
    per = 0xFFFFFFFFu / (inc ? inc : 1u);      /* the note's period, frames Q8 */
    while (per > (HW_SL - 4u) << 8) per >>= 1;  /* (below ~43 Hz: an octave up) */
    if (per < 4u << 8) per = 4u << 8;
    if (!S->wn) {                               /* the wander: a new target now and then (0.3 .. 1.5 s), +-4 % of BODY */
        int32_t b = hw_body(p[1]);
        S->wt = (int32_t)(hw_rnd(S) % 2001u) * (b / 25) / 1000 - b / 25;
        S->wn = 400u + hw_rnd(S) % 1700u;
    } else
        S->wn--;
    S->wd += (S->wt - S->wd) >> 5;
    tgt = clamp(hw_body(p[1]) + S->wd, 4 << 8, (int32_t)(HW_BL - 3u) << 8);
    S->bd += (tgt - S->bd) >> 8;                /* BODY glides (~0.2 s) */
    dbd = (S->bd - bd0) / (int32_t)CTL;
    for (i = 0; i < (uint32_t)CTL; i++) {
        uint32_t bdi = (uint32_t)(bd0 + dbd * (int32_t)i), bi = bdi >> 8, bf = bdi & 255u;
        uint32_t si = per >> 8, sf = per & 255u;
        int32_t r1 = (int32_t)S->bw - (int32_t)bi, r0, fbk, s, y, n;
        int32_t s1 = S->str[(S->sw - si) & (HW_SL - 1u)], s0 = S->str[(S->sw - si - 1u) & (HW_SL - 1u)];
        if (r1 < 0) r1 += (int32_t)HW_BL;
        r0 = r1 ? r1 - 1 : (int32_t)HW_BL - 1;
        fbk = S->body[r1] + (((S->body[r0] - S->body[r1]) * (int32_t)bf) >> 8);   /* what comes round */
        n = (int32_t)hw_rnd(S) >> HW_NOISE;     /* the breath */
        if (S->kick) {                          /* a note's burst, fading */
            n += (((int32_t)S->rng >> 20) * (int32_t)S->kick) / (int32_t)HW_KICK;
            S->kick--;
        }
        s = clamp(s1 + (((s0 - s1) * (int32_t)sf) >> 8) + fbk + n, -65535, 65535);   /* the string */
        S->dcy = clamp(s - S->dcx + ((S->dcy * 32604) >> 15), -65535, 65535);
        S->dcx = s;
        s = (S->dcy * 26214) >> 15;             /* (x 0.8, Audrey's) */
        S->sl += ((s - S->sl) * 11150) >> 14;   /* (its damping, ~8 kHz) */
        s = S->sl;
        S->str[S->sw++ & (HW_SL - 1u)] = (int16_t)clamp(s, -32767, 32767);
        y = hw_sat((s * drv) >> 8);             /* DRIVE */
        S->lp += ((y - S->lp) * a) >> 14;       /* DAMP */
        y = S->lp;
        S->hl += ((y - S->hl) * 327) >> 15;     /* (~70 Hz) */
        y -= S->hl;
        y = (y + hw_ap(S->ap2, &S->a2, HW_A2, hw_ap(S->ap1, &S->a1, HW_A1, y))) >> 1;   /* the diffusion */
        S->body[S->bw] = (int16_t)clamp((y * g) >> 12, -32767, 32767);   /* x FEEDBACK, round again */
        if (++S->bw == HW_BL) S->bw = 0;
        y = clamp(y, -32767, 32767);
        mono[i] += mulq15(mulq15(y, amp_at(amp0, dmp, (int32_t)i)), VOICE_LV) * 2;
    }
}

static void howl_text(const volatile int32_t *p, uint32_t k, char *s)   /* parameter k as the screen shows it */
{
    int32_t v = clamp(p[k], 0, 127);
    switch (k) {
    case 0: {                                   /* "-14DB", "+6DB" */
        int32_t db = -60 + (v * 72 + 63) / 127;
        if (db >= 0) *s++ = '+';
        else *s++ = '-', db = -db;
        put_s(put_u(s, (uint32_t)db), "DB");
        return;
    }
    case 1: {                                   /* "4.2MS" */
        uint32_t t = ((uint32_t)hw_body(v) * 10u / 256u + 22u) / 44u;
        s = put_u(s, t / 10u);
        *s++ = '.', *s++ = (char)('0' + t % 10u);
        put_s(s, "MS");
        return;
    }
    case 2: {                                   /* "2.1K", "450" */
        uint32_t f = hw_cut(v);
        if (f >= 1000u) {
            s = put_u(s, f / 1000u);
            *s++ = '.', *s++ = (char)('0' + f / 100u % 10u);
            put_s(s, "K");
        } else
            put_u(s, f);
        return;
    }
    default: put_s(put_u(s, ((uint32_t)v * 100u + 63u) / 127u), "%"); return;
    }
}

static const engine_t HOWL_ENGINE = {"HOWL", {"FEEDBACK", "BODY", "DAMP", "DRIVE"}, {127, 127, 127, 127}, howl_begin,
                                     howl_block, howl_text};
