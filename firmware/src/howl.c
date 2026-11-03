/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream p0larpatch's HOWL source: a feedback loop that wakes on its own, after the design of Audrey II (Nick Donaldson,
 * Infrasonic Audio; Synthux Academy, MIT: github.com/Synthux-Academy/Audrey-II, FeedbackSynthEngine.cpp), written anew in
 * integers for the FM-1 (no FPU); none of its code is copied. A voice's loop:
 *
 *   a breath of noise (inaudible) + BODY's line -> a Karplus-Strong string at the note -> DRIVE's saturation -> DAMP's
 *   low-pass, a high-pass (~70 Hz; FEEDBACK's top end lifts it) -> four all-passes (Audrey's reverb in the loop, small)
 *   -> x FEEDBACK -> BODY's line
 *
 * Nothing is struck: the string only hears the noise and what comes round, so the voice is near silent until FEEDBACK
 * lifts the loop's gain past one; then the sound grows out of the noise by itself and howls (the saturation holds it).
 * A key adds a short burst, so a note near the threshold provokes it. The filters in the loop pick which overtones scream;
 * BODY glides (~0.2 s, Audrey's smoothing), so turning it bends the whole sound, and a slow random wander moves it a
 * little all the time (after Audrey Touch's sample-and-hold on its body).
 *   FEEDBACK   the loop's gain, -60 .. +12 dB (it wakes around 2/3 of the way); its last steps (120 .. 127) also
 *              lift the loop's high-pass, ~70 Hz .. ~1.6 kHz, so the howl overblows: it leaps up the note's overtones
 *              (after Audrey III's high-pass in the loop). MIDI pressure (channel or poly aftertouch, on the track's
 *              channel) pushes FEEDBACK toward its top: press harder, it screams and climbs (Audrey III's plates)
 *   BODY       the second line, ~1 .. ~92 ms: a resonance of its own beside the note's, metal at the short end; past
 *              ~9 ms (100) faster, to slow, beating tones at the long end, past ~22 ms (115) slow pulses (Audrey III's
 *              goes to 100 ms)
 *   DAMP       the loop's low-pass, ~18 kHz .. ~100 Hz: a scream .. a growl under the ice
 *   DRIVE      the saturation, 1x .. 7x: clean .. torn
 * The LOOP page (Arctic Scream 1.4, after Audrey Touch's loop and Book of the Dead's FX loop; written anew, none of
 * their code), the track's own (track_t hw_x), 0 each as before it:
 *   GATE       how far the loop stops feeding itself once the key is let go (Audrey Touch's envelope on the write
 *              back): 0 howls on through the release as before, 100 % lets it die into its own tail
 *   BLOOM      the effects' return (GRAIN, DELAY, SPACE: what they add to the mix, the last render's) fed into the
 *              loop, an FX loop inside the feedback: the howl blooms into the reverb and the echoes come round again
 *   LOWCUT     the loop's high-pass, ~70 Hz (as before) .. ~1.9 kHz, two poles: with DAMP, a band that screams
 *   EXCITE     the track's sample (the SAMPLE source's SLOT, START and LENGTH, as recorded, looped) fed into the
 *              string: the howl grows out of a voice, a choir, drums; OFF: the noise alone wakes it
 * State: howl_t (howl.h) in the voice's PHYS slot; BODY's line in the voice's pool scratch (voices.c vx_pool, shared with
 * SAMPLE's windows); hw_press: the tracks' MIDI pressure (grimm.c, the MIDI in). */

#define HW_KICK 2205u                           /* a note's burst: 50 ms */
#define HW_NOISE 28                             /* the breath: >> this of the noise (~-90 dB) */

_Static_assert(HW_BL <= VX_POOL, "BODY's line fits in a voice's pool scratch");
static uint8_t hw_press[NT];                    /* a track's MIDI pressure, 0 .. 127 (the main loop writes it) */
#define HW_RN 256u                              /* (the render's frames: VB) */
static int32_t hw_ret[HW_RN] __attribute__((section(".pool")));   /* BLOOM: the effects' return, mono, the last render's
                                                 * (grimm.c app_audio) */
static uint8_t hw_bloom_on;                     /* (a HOWL track's BLOOM is up: app_audio keeps hw_ret) */
typedef struct {                                /* EXCITE: a track's sample, read on as recorded (22.05 kHz, each sample
                                                 * twice), shared by the track's HOWL voices: CTL frames a control step */
    uint32_t vb, pos, start, end;               /* the step it holds; the next sample, the loop's first and end */
    int32_t pred, idx, a, b;                    /* the decoder; the last two samples */
    uint8_t slot, on, half;
    int16_t buf[CTL];
} hw_exc_t;
static hw_exc_t hw_ex[NT];
static howl_t *hw_of(voice_t *v) { return &ph_slot[v - vc].u.h; }
static uint32_t hw_rnd(howl_t *S)
{
    uint32_t s = S->rng;
    s ^= s << 13, s ^= s >> 17, s ^= s << 5;
    return S->rng = s;
}
static int32_t hw_body(int32_t body)            /* BODY: its delay, frames Q8, ~1 .. ~92 ms (44 x 2^(0 .. 6.53)): as 1.1's
                                                 * to 100 (~9 ms), as 1.2's to 115 (~22 ms), then on to ~92 ms (the
                                                 * sounds saved keep theirs) */
{
    int32_t b = clamp(body, 0, 127);
    int32_t d = b <= 100 ? b * 784 / 127 : b <= 115 ? 617 + (b - 100) * 435 / 27 : 858 + (b - 115) * 33;
    return (int32_t)((44u * pow2_q16(d)) >> 8);
}
static int32_t hw_gain(int32_t fb)              /* FEEDBACK (Q8): -60 .. +12 dB, Q16 */
{
    return (int32_t)(pow2_q16(clamp(fb, 0, 127 << 8) * 2297 / (127 << 8)) >> 10);
}
static int32_t hw_hpc(int32_t fb)               /* FEEDBACK (Q8): the high-pass's coefficient, Q15: ~70 Hz, and over 119
                                                 * up to ~1.6 kHz (2^4.51) */
{
    int32_t u = fb > 119 << 8 ? (clamp(fb, 0, 127 << 8) - (119 << 8)) * 866 / (8 << 8) : 0;
    return (int32_t)((327u * pow2_q16(u)) >> 16);
}
static int32_t hw_glide(int32_t x, int32_t t)   /* an eighth of the way a block, and there when near */
{
    return t - x > 7 || x - t > 7 ? x + ((t - x) >> 3) : t;
}
static int32_t hw_fb(const voice_t *v, int32_t fb)   /* FEEDBACK with the track's pressure (Q8): toward 127 */
{
    fb = clamp(fb, 0, 127);
    return (fb << 8) + (127 - fb) * 2 * (int32_t)hw_press[v->trk < NT ? v->trk : 0];
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
    for (w = (uint32_t *)vx_pool[v - vc], i = 0; i < HW_BL / 2u; i++) w[i] = 0;
    ph_slot[v - vc].model = 0xFF;               /* (not a PHYS model: PHYS begins anew) */
    S->rng = (uint32_t)v->rng | 1u;
    S->kick = HW_KICK;
    S->bd = hw_body(trk[v->trk].p[ENG_HOWL][1]);
    S->fb = hw_fb(v, trk[v->trk].p[ENG_HOWL][0]);
    S->hc = hw_hpc(S->fb);
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

static const int16_t *hw_excite(uint32_t t)    /* track t's sample this control step (0: none) */
{
    hw_exc_t *E = &hw_ex[t];
    sp_loop_t L;
    uint32_t i;
    if (E->vb == vblock && E->on) return E->buf;
    if (!sp_loop(trk[t].p[ENG_SAMPLE], &L)) {   /* (the slot empty: nothing; REVERSE: read forward all the same) */
        E->on = 0;
        return 0;
    }
    if (!E->on || E->slot != L.slot || E->pos >= E->end) {   /* (the loop from its segment's start: no decodes to skip) */
        uint32_t seg = L.l0 >> SMP_SEG_LOG2;
        E->slot = L.slot, E->on = 1, E->half = 0;
        E->start = E->pos = seg << SMP_SEG_LOG2, E->end = L.l0 + L.len;
        if (E->end > smp[L.slot].n) E->end = smp[L.slot].n;
        E->pred = smp_ipred[L.slot][seg], E->idx = smp_iidx[L.slot][seg], E->a = E->b = 0;
    }
    for (i = 0; i < (uint32_t)CTL; i++) {
        int32_t w;
        if (!E->half) {
            E->a = E->b;
            E->b = E->pos < E->end ? ima_dec(smp[E->slot].data, E->pos, &E->pred, &E->idx) : 0;
            E->pos++;
        }
        w = (int32_t)(E->pos - E->start < E->end - E->pos ? E->pos - E->start : E->end - E->pos);   /* (a fade at both ends,
                                                 * 256 samples: the loop never clicks into the string) */
        w = w > 256 ? 256 : w;
        E->buf[i] = (int16_t)((E->half ? (E->a + E->b) >> 1 : E->a) * w >> 8);
        E->half ^= 1u;
    }
    E->vb = vblock;
    return E->buf;
}

/* CTL frames of voice v, added to mono[] */
static void howl_block(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1)
{
    howl_t *S = hw_of(v);
    int16_t *body = vx_pool[v - vc];
    const volatile int32_t *X = trk[v->trk < NT ? v->trk : 0].hw_x;
    uint32_t inc = vinc(VP16(v)) >> 8, per, fc = hw_cut(p[2]), i;
    int32_t g, hc, hl, mk, drv = 256 + clamp(p[3], 0, 127) * 12, dmp = amp1 - amp0;
    int32_t x = (int32_t)(fc * 4669u / 1000u), a = x * 16384 / (32768 + x);   /* (DAMP's one-pole, Q14) */
    int32_t tgt, bd0 = S->bd, dbd, gate = clamp(X[HX_GATE], 0, 127), lc = clamp(X[HX_LCUT], 0, 127);
    int32_t bl = clamp(X[HX_BLOOM], 0, 127), ex = clamp(X[HX_EXCITE], 0, 127);
    const int16_t *exs = ex ? hw_excite(v->trk < NT ? v->trk : 0) : 0;
    const int32_t *ret = bl && hw_bloom_on && vc_at + (uint32_t)CTL <= HW_RN ? hw_ret + vc_at : 0;
    S->fb = hw_glide(S->fb, hw_fb(v, p[0]));    /* (FEEDBACK and the pressure glide: ~6 ms) */
    g = hw_gain(S->fb) >> 4;
    {                                           /* GATE: the key held (1) or let go (0), gliding (up ~3 ms, down ~45 ms) */
        int32_t held = v->stage == ST_REL || v->stage == ST_FADE ? 0 : 32767;
        S->gt += held > S->gt ? (held - S->gt) >> 2 : (held - S->gt) >> 6;
        g = (int32_t)((int64_t)g * (32767 - gate * (32767 - S->gt) / 127) >> 15);
    }
    hc = S->hc = hw_glide(S->hc, hw_hpc(S->fb));
    hl = lc ? (int32_t)(((uint32_t)hc * pow2_q16(lc * 920 / 127)) >> 16) : hc;   /* LOWCUT: up from it, ~1.9 kHz */
    hl = hl > 12880 ? 12880 : hl;
    ex = ex * ex * 2;                           /* (EXCITE, BLOOM: Q15 gains, squared: fine at the low end) */
    bl = bl * bl * 3 / 2;
    mk = 256 + (hc - 327) * 1024 / 7146;        /* (overblown, the high-pass thins it: up to 5x back, Q8) */
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
        fbk = body[r1] + (((body[r0] - body[r1]) * (int32_t)bf) >> 8);   /* what comes round */
        n = (int32_t)hw_rnd(S) >> HW_NOISE;     /* the breath */
        if (S->kick) {                          /* a note's burst, fading */
            n += (((int32_t)S->rng >> 20) * (int32_t)S->kick) / (int32_t)HW_KICK;
            S->kick--;
        }
        if (exs) n += (exs[i] * ex) >> 15;      /* (EXCITE: the sample into the string) */
        if (ret) n += (int32_t)clamp(((int64_t)ret[i] * bl) >> 11, -32767, 32767);   /* (BLOOM: the effects' return, at the
                                                 * output's level: up to ~12x, to the loop's) */
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
        S->hl += ((y - S->hl) * hl) >> 15;      /* (~70 Hz .. ~1.6 kHz; LOWCUT: on up to ~1.9 kHz) */
        y -= S->hl;
        if (lc) {                               /* (LOWCUT: a second pole, so it narrows) */
            S->hl2 += ((y - S->hl2) * hl) >> 15;
            y -= S->hl2;
        }
        S->h2 += ((y - S->h2) * (hc - 327)) >> 15;   /* (overblown: a second pole, steeper, so the howl leaps) */
        y -= S->h2;
        n = hw_ap(S->ap2, &S->a2, HW_A2, hw_ap(S->ap1, &S->a1, HW_A1, y));
        y = (y + hw_ap(S->ap4, &S->a4, HW_A4, hw_ap(S->ap3, &S->a3, HW_A3, n))) >> 1;   /* the diffusion */
        body[S->bw] = (int16_t)clamp((y * g) >> 12, -32767, 32767);   /* x FEEDBACK, round again */
        if (++S->bw == HW_BL) S->bw = 0;
        y = clamp((y * mk) >> 8, -32767, 32767);
        mono[i] += mulq15(mulq15(y, amp_at(amp0, dmp, (int32_t)i)), VOICE_LV * 3 / 4);   /* (a chord of it: room left) */
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

static const engine_t HOWL_ENGINE = {"Holvut", {"FEEDBACK", "BODY", "DAMP", "DRIVE"}, {127, 127, 127, 127}, howl_begin,
                                     howl_block, howl_text};
