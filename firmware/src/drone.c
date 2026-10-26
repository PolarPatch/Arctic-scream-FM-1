/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream p0larpatch's DRONE page (LFO, tapped again): a track's sound that lives on its own while it sounds. The
 * idea after Jangada's DRONE page (zednaked, GPL-3.0: github.com/zednaked/jangada, EVOL, TENS, RAMP), written anew for
 * this firmware; none of its code is copied. The names are North Sami (sanit.oahpa.no):
 *   BORGA      (snowdrift) 0 .. 100 %: four slow random walks a track move the sound, never the same twice, never
 *              periodic: walk 0 the track filter's CUTOFF (when it is not open) with the source's brightness, walk 1
 *              its shape, walk 2 its own most natural knob (Holvut: FEEDBACK, around the threshold), walk 3 the REV
 *              send (near, far); and each voice its own slow detune (a few cents) and breath (its level): a held chord
 *              breathes as a choir. 0: none of it, the sound as it is set
 *   STOARBMA   (storm) 0 .. 100 %: the tension; it opens the filter, adds resonance, drives the source (its DR_MAP
 *              row: Holvut's FEEDBACK and DRIVE up, DAMP open), spreads the voices up to +-12 cents, and makes the walks
 *              deeper and faster (x 3)
 *   GOHCCAT    (to wake) OFF, 1, 2, 4, 8, 16, 32 bars: the time the tension takes to reach STOARBMA, on the tempo (OFF:
 *              ~0.3 s); a drone that wakes from silence starts at tension 0 and rises
 * Each walk takes a new target every 4 .. 8 s (tension: up to 3 x as often), 5/8 of the last plus a random step,
 * folded back at the ends, and glides to it through two one-poles (~2.4 s); the voices' detune and breath the same,
 * their own. The walks run only while the track sounds; from silence they start at 0: a drone begins as its sound is
 * set. Everything is added on top of the knobs (as the LFO's), the knobs never moved: BORGA and STOARBMA at 0 give the
 * sound exactly as it is set.
 * Audio ISR, a control step (ctl_hook: drone_ctl, every DR_DIV-th); lfo_params adds dr_k to a voice's four,
 * trk_filter dr_cut and dr_res, mix_tracks dr_rev, voices_render a voice's dr_fine (vinc) and dr_gain; ui.c draws the
 * walks and the tension. State in the pool (zeroed at boot). */

#define DR_NW 4u                                /* walks a track */
#define DR_DIV 8u                               /* a walk step every 8 control steps: ~172 a second */
#define DR_SM 79                                /* the one-poles a step, Q15: ~2.4 s (x 1 .. 3 with the tension) */
#define DR_T_MIN 689u                           /* a new target after 4 .. 8 s (steps) */
#define DR_T_RND 689u
#define DR_NRISE 7                              /* GOHCCAT: OFF, 1, 2, 4, 8, 16, 32 bars */
#define DR_FINE 7                               /* a voice's detune at BORGA 100 %: +-3 cents (tune_f units: ~0.43 cent) */
#define DR_FINE_T 28                            /* .. and at STOARBMA 100 %: +-12 cents */
#define DR_BREATH 6554                          /* a voice's breath at BORGA 100 %: down to 80 % (Q15) */
static volatile int32_t seq_bpm;                /* (seq.c: the tempo) */

/* what the walks and the tension move, by source, a knob each: {walk (DR_NONE: none), its swing at BORGA 100 % and
 * the tension's push at STOARBMA 100 % (% of the knob's range, + up)}; discrete knobs (WAVE, PATCH, SLOT, MODEL, ..)
 * stay as set */
#define DR_NONE 255u
typedef struct { uint8_t w; int8_t sw, tp; } dr_map_t;
static const dr_map_t DR_MAP[NENG][NPARAM] = {
    {{DR_NONE, 0, 0}, {1, 20, 30}, {2, 25, 35}, {DR_NONE, 0, 25}},          /* VA: WAVE, DETUNE, CUTOFF, RESONANCE */
    {{DR_NONE, 0, 0}, {2, 25, 40}, {DR_NONE, 0, 0}, {1, 20, 25}},           /* FM: PATCH, BRIGHT, DECAY, DETUNE */
    {{DR_NONE, 0, 0}, {DR_NONE, 0, 0}, {DR_NONE, 0, 0}, {DR_NONE, 0, 0}},   /* SAMPLE: as set (the filter, the voices) */
    {{2, 20, 0}, {DR_NONE, 0, 0}, {DR_NONE, 0, 35}, {1, 15, 20}},           /* VOICE: VOWEL, SHIFT, BUZZ, BREATH */
    {{DR_NONE, 0, 0}, {DR_NONE, 0, 0}, {1, 15, 20}, {2, 15, 45}},           /* WHEEL: DRAWBARS, PERC, CLICK, DRIVE */
    {{DR_NONE, 0, 0}, {DR_NONE, 0, 0}, {2, 20, 0}, {1, 20, 30}},            /* PHYS: MODEL, STRUCTURE, DAMP, BOW */
    {{DR_NONE, 0, 0}, {DR_NONE, 0, 0}, {2, 25, 40}, {1, 20, 25}},           /* PHASE: WAVE, WAVE2, DCW, DETUNE */
    {{2, 5, 12}, {1, 20, 0}, {0, 15, -30}, {DR_NONE, 0, 40}},               /* Holvut: FEEDBACK, BODY, DAMP, DRIVE */
};

typedef struct {
    uint32_t rng;
    int32_t tgt[DR_NW], s1[DR_NW], w[DR_NW];    /* a walk's target, its glide (Q15, +-32767): w the walk now */
    uint16_t cnt[DR_NW];                        /* steps to its next target */
    int32_t tens, goal, span;                   /* the tension now, where it goes (STOARBMA), the way there (Q23: 0 ..
                                                 * 32767 << 8): GOHCCAT's time for the whole way, however far */
    uint8_t on;                                 /* the track sounded at the last step */
} dr_trk_t;
typedef struct {
    int32_t ft, f1, f, gt, g1, g;               /* a voice's detune (target, glide, now: Q15) and breath (Q15 down) */
    uint16_t cnt;
} dr_vc_t;
static struct {
    dr_trk_t t[NT];
    dr_vc_t v[NV];
    uint32_t div;
} dr __attribute__((section(".pool")));
static int16_t dr_k[NT][NPARAM];                /* audio: the walks' and the tension's on a track's four (knob units) */
static int32_t dr_cut[NT], dr_res[NT], dr_rev[NT];   /* .. on its filter (CUTOFF << 8), RESONANCE, REV send */
static uint8_t dr_any[NT];                      /* .. dr_k not all 0 */

static uint32_t dr_rnd(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    return *s = x;
}
static int32_t dr_bip(uint32_t *s) { return (int32_t)(dr_rnd(s) >> 16) - 32768; }   /* -32768 .. 32767 */
static int32_t dr_fold(int32_t x) { return x > 32767 ? 65534 - x : x < -32767 ? -65534 - x : x; }
static int32_t dr_glide(int32_t a, int32_t to, int32_t k) { return a + (((to - a) * k) >> 15); }
static int32_t dr_tens15(uint32_t t) { return dr.t[t].tens >> 8; }   /* the tension now, Q15 (ui.c) */

static uint32_t dr_sounds(uint32_t t)           /* a voice of track t sounds */
{
    uint32_t k;
    for (k = 0; k < NV; k++)
        if (vc[k].trk == t && vc[k].stage != ST_OFF) return 1u;
    return 0u;
}
static int32_t dr_rise_step(int32_t rise, int32_t span)   /* GOHCCAT: the tension's step a walk step (Q23), span in its
                                                 * time; OFF: ~0.3 s */
{
    uint32_t bars;
    if (rise <= 0) return span / 52 + 1;
    bars = 1u << (uint32_t)(clamp(rise, 1, DR_NRISE - 1) - 1);   /* (a bar: 4 beats, 41344 / BPM walk steps) */
    return (int32_t)(((uint32_t)span * (uint32_t)clamp(seq_bpm, 20, 300)) / (bars * 41344u)) + 1;
}

static void drone_ctl(void)                     /* audio, a control step: the walks, the tension, what they move */
{
    uint32_t t, i, k;
    if (++dr.div < DR_DIV) return;
    dr.div = 0;
    for (t = 0; t < NT; t++) {
        dr_trk_t *D = &dr.t[t];
        int32_t bo = clamp(trk[t].dr_borga, 0, 100), st = clamp(trk[t].dr_storm, 0, 100), tq, dep, sm, e;
        int32_t goal = (st * 32767 / 100) << 8, step;
        uint32_t on = dr_sounds(t);
        if (on && !D->on) {                     /* (from silence: the walks from 0; GOHCCAT: the tension from 0) */
            for (i = 0; i < DR_NW; i++) D->tgt[i] = D->s1[i] = D->w[i] = 0, D->cnt[i] = (uint16_t)(i * 97u);
            D->tens = trk[t].dr_rise > 0 ? 0 : goal;
            D->goal = -1;
            D->rng += t * 0x632BE5ABu + 1u;
        }
        if (goal != D->goal) {                  /* (a new STOARBMA: the way from here, at least 1/16 of it all) */
            D->goal = goal;
            D->span = goal > D->tens ? goal - D->tens : D->tens - goal;
            if (D->span < (32767 << 8) / 16) D->span = (32767 << 8) / 16;
        }
        step = dr_rise_step(trk[t].dr_rise, D->span);
        D->on = (uint8_t)on;
        if (!on) {
            for (k = 0; k < NPARAM; k++) dr_k[t][k] = 0;
            dr_cut[t] = dr_res[t] = dr_rev[t] = 0, dr_any[t] = 0;
            continue;
        }
        D->tens = D->tens < goal ? (goal - D->tens < step ? goal : D->tens + step) :
                                   (D->tens - goal < step ? goal : D->tens - step);
        tq = D->tens >> 8;                      /* (Q15) */
        sm = DR_SM + DR_SM * 2 * tq / 32767;    /* (the tension: faster, deeper) */
        for (i = 0; i < DR_NW; i++) {
            if (!D->cnt[i]) {
                D->tgt[i] = dr_fold((D->tgt[i] * 5 >> 3) + dr_bip(&D->rng) * 3 / 4);
                D->cnt[i] = (uint16_t)((DR_T_MIN + dr_rnd(&D->rng) % DR_T_RND) * 32768u / (32768u + 2u * (uint32_t)tq));
            } else
                D->cnt[i]--;
            D->s1[i] = dr_glide(D->s1[i], D->tgt[i], sm);
            D->w[i] = dr_glide(D->w[i], D->s1[i], sm);
        }
        dep = bo * 327 * (32768 + tq) >> 15;    /* (BORGA, Q15; x 2 at full tension) */
        dep = dep > 32767 ? 32767 : dep;
        e = clamp(trk[t].eng, 0, NENG - 1);
        dr_any[t] = 0;
        for (k = 0; k < NPARAM; k++) {          /* the source's four: swing + push, % of the range */
            const dr_map_t *M = &DR_MAP[e][k];
            int32_t mx = ENGINES[e]->max[k], o = M->tp * tq / 100;   /* (Q15 x %) */
            if (M->w < DR_NW) o += mulq15(mulq15(D->w[M->w], dep), M->sw * 327);
            o = (o * mx + (o < 0 ? -16384 : 16384)) >> 15;
            dr_k[t][k] = (int16_t)o;
            dr_any[t] |= o != 0;
        }
        dr_cut[t] = trk[t].cut < 127 ? (mulq15(D->w[0], dep) * 16 >> 7) : 0;   /* (+-16 of 127, << 8; open: as it is) */
        dr_cut[t] += (40 * tq) >> 7;            /* (the tension: up to + 40) */
        dr_res[t] = 30 * tq >> 15;
        dr_rev[t] = mulq15(D->w[3], dep) * 12 >> 15;
    }
    for (k = 0; k < NV; k++) {                  /* the voices: their own detune and breath */
        dr_vc_t *V = &dr.v[k];
        uint32_t t = vc[k].trk % NT;
        int32_t bo = clamp(trk[t].dr_borga, 0, 100), tq = dr.t[t].tens >> 8, fa, ga;
        if (vc[k].stage == ST_OFF) {
            V->ft = V->f1 = V->f = V->gt = V->g1 = V->g = 0, V->cnt = (uint16_t)(k * 61u);
            dr_fine[k] = 0, dr_gain[k] = 32767;
            continue;
        }
        if (!V->cnt) {
            V->ft = dr_fold((V->ft * 5 >> 3) + dr_bip(&dr.t[t].rng));
            V->gt = (int32_t)(dr_rnd(&dr.t[t].rng) >> 17);   /* (0 .. 32767: how far down) */
            V->cnt = (uint16_t)(DR_T_MIN / 2u + dr_rnd(&dr.t[t].rng) % DR_T_RND);
        } else
            V->cnt--;
        V->f1 = dr_glide(V->f1, V->ft, DR_SM), V->f = dr_glide(V->f, V->f1, DR_SM);
        V->g1 = dr_glide(V->g1, V->gt, DR_SM), V->g = dr_glide(V->g, V->g1, DR_SM);
        fa = DR_FINE * bo / 100 + DR_FINE_T * tq / 32767;   /* (cents x ~2.3 at the ends) */
        ga = DR_BREATH * bo / 100;
        dr_fine[k] = V->f * fa / 32767;
        dr_gain[k] = 32767 - mulq15(V->g, ga);
    }
}
static char *dr_rise_text(char *s, int32_t v, const char *sp)   /* GOHCCAT: "OFF", "1 BAR", "16 BARS" (sp: " "; "" on
                                                 * a slab: "16BAR") */
{
    v = clamp(v, 0, DR_NRISE - 1);
    if (!v) return put_s(s, "OFF");
    s = put_s(put_u(s, 1u << (uint32_t)(v - 1)), sp);
    return put_s(s, *sp && v > 1 ? "BARS" : "BAR");
}
