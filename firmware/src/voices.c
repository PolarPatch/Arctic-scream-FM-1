/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (env_tick, amp_at)
 * GR!MM: Copyright (C) 2026 GR!MM */
/* Arctic Scream's tracks and voices.
 *   NT tracks, each with a source (an engine: VA, FM, SAMPLE, VOICE, WHEEL, PHYS, PHASE), every
 *   engine's four parameters, LEVEL, PAN and the
 *   sends DLY, REV and GRN
 *   (the main loop writes them, 32-bit volatiles: docs/RULES.md);
 *   NV voices shared by all the tracks: a voice plays a note for a track, with the track's engine and parameters
 *   as they are at each block;
 *   the envelope: Felucca's voice.c env_tick (hugelton/Felucca: Q24, a step every CTL frames, a linear attack, an
 *   exponential release; dsp.c amp_at's ramp over each block), with each track's ENV (ATTACK ~1 ms .. ~10 s, DECAY
 *   ~10 ms .. ~10 s, SUSTAIN 0 .. 100 %, RELEASE ~10 ms .. ~20 s; the times exponential in the knob) and a ~10 ms
 *   fade for a voice handed to another note (no click); an engine's own envelope (FM's DECAY, a PHYS strike) runs
 *   inside it;
 *   voices_render: every sounding voice into its track's buffer (mono, 44.1 kHz), for grimm.c's mixer.
 * Another engine: its begin, block and text functions and its names in an engine_t (as analog.c's VA_ENGINE, fm.c's
 * FM_ENGINE), an ENG_ and an entry in ENGINES; a track then plays it by its eng. A voice keeps the engine it began with;
 * a track's engine changing hands its sounding notes over through the fade (env_tick). */

#define CTL 32                                  /* frames a control step: Felucca's CTL, signed as Felucca's (fm6_core.c
                                                 * divides signed gain steps by it: unsigned, a falling gain wraps) */
#define CTL_LOG2 5
#define NV 6u                                   /* voices, shared by the tracks */
#define NT 4u                                   /* tracks */
#define NPARAM 4u                               /* an engine's parameters: KNOB 1..4 on the SOUND page */
#define VPH 8u                                  /* a voice's phases for its engine (SUPERSAW: 7) */
#define FADE_STEP 1198373                       /* Q24 a control step: a stolen voice is silent in 14 steps (~10 ms) */
#define REL_END (1 << 15)                       /* Q24: a release ends here, 54 dB down (a voice's last ~16 LSB, -66 dBFS;
                                                 * Felucca's 1 << 12 rendered the last ~2.7 s of a 6 s release unheard) */
enum { ST_OFF, ST_ATK, ST_SUS, ST_REL, ST_FADE };
enum { ENG_VA, ENG_FM, ENG_SAMPLE, ENG_VOICE, ENG_WHEEL, ENG_PHYS, ENG_PHASE, ENG_HOWL, ENG_BIEGGA, ENG_JIEKNA, NENG };   /* (2.0:
                                                 * Biegga; 2.2: Jiekna, the singing ice) */

typedef struct {
    volatile uint32_t req;                      /* main loop: + 1 after want_* */
    volatile uint8_t want_note, want_trk, want_on;   /* main loop: start this note for this track / release */
    volatile uint8_t stage, trk, note, eng;     /* audio: ST_*, the track, the note and the engine it plays */
    volatile int32_t env;                       /* audio: Q24 */
    uint32_t seen;                              /* audio: the req it acted on */
    int32_t amp, rng;                           /* the block's end amplitude (Q15); a random state for the engine */
    uint32_t ph[VPH];                           /* the engine's phases */
    int32_t s[4];                               /* .. and its other state (VA: the filter) */
} voice_t;

typedef struct {
    const char *name;                           /* shown in the SOUND page's header and on the SOURCE page */
    const char *knob[NPARAM];                   /* its four parameters' names */
    int32_t max[NPARAM];                        /* their ranges (0 .. max) */
    void (*begin)(voice_t *v);                  /* a note from silence */
    void (*block)(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1);   /* CTL frames, added */
    void (*text)(const volatile int32_t *p, uint32_t k, char *s);   /* parameter k as the screen shows it */
} engine_t;

typedef struct {
    volatile int32_t eng;                       /* ENG_: what its notes play */
    volatile int32_t p[NENG][NPARAM];           /* every engine's four parameters (kept when the engine changes) */
    volatile int32_t level, dly, rev, grn, pan; /* MIXER 1 and 2: 0..127 (PAN 64: the middle) */
    volatile int32_t latch;                     /* SOURCE: LATCH, 0 OFF (a key sounds while held) / 1 ON */
    volatile int32_t env[4];                    /* ENV: ATTACK, DECAY, SUSTAIN, RELEASE, 0..127 */
    volatile int32_t chord, ch_type, ch_voice;  /* SEL: chord mode (0 OFF / 1 ON), CHORD TYPE, VOICING (scale.c) */
    volatile int32_t arp, arp_mode, arp_rate, arp_oct, arp_gate;   /* ARP: on (0 / 1), MODE, RATE, OCTAVES 1..4, GATE
                                                 * 10..100 % (arp.c) */
    volatile int32_t lfo_rate, lfo_depth, lfo_dest, lfo_shape;   /* LFO: RATE 0..127, DEPTH 0..100 %, DESTINATION,
                                                 * SHAPE (lfo.c) */
    volatile int32_t cut, res, fx;              /* TRACK: the track filter's CUTOFF (127: open, bypassed), RESONANCE;
                                                 * FX: its sends 0..200 % (100: as MIXER 1 sets them) (grimm.c) */
    volatile int32_t fm_alg, fm_fb, fmo[6][5];  /* FM: the track's own patch (FM ALGO, FM OP): ALGO 0..31, FEEDBACK
                                                 * 0..7; an operator's (OP1 first) RATIO 0..31, FINE, LEVEL, ATTACK,
                                                 * DECAY 0..99 (fm.c; a built-in PATCH's: fm_builtin) */
    volatile int32_t dr_borga, dr_storm, dr_rise;   /* DRONE: BORGA 0..100 %, STOARBMA 0..100 %, GOHCCAT 0 OFF .. 6
                                                 * (32 bars) (drone.c) */
    volatile int32_t ftype, fpos;               /* MIXER 2: the track filter's TYPE (FT_: LP, HP, BP), POS (0 PRE:
                                                 * the sends hear it filtered; 1 POST: only the dry sound filtered) */
    volatile int32_t warp, morph;               /* HOME: WARP, MORPH 0..127 (chaos.c) */
    volatile int32_t hw_x[4];                   /* (Arctic Scream 1.4) Holvut's LOOP page: GATE, BLOOM, LOW CUT, EXCITE
                                                 * 0..127 (howl.c; 0: as before it) */
    volatile int32_t e_loop, e_onset, spread, detune;   /* (2.2) ENV 2: LOOP (EL_), ONSET 0..100 (x 80 ms), SPREAD,
                                                 * DETUNE 0..100 % (the voice steps); 0: as before them */
} track_t;
enum { EL_OFF, EL_PULSE, EL_BREATHE, NELOOP };
static const char *const EL_NAME[NELOOP] = {"OFF", "PULSE", "BREATHE"};   /* (2.2) ENV 2's LOOP: PULSE (after Adrift's A-D: up over ATTACK, down
                                                 * over DECAY to nothing, again), BREATHE (D-A-D: down to SUSTAIN, up
                                                 * again), while the note is held; written anew */
enum { HX_GATE, HX_BLOOM, HX_LCUT, HX_EXCITE };
#define TRK_DEF {ENG_VA, {{0, 10, 90, 30}, {0, 64, 64, 20}, {0, 0, 127, 0}, {0, 12, 64, 10}, {4, 0, 40, 0}, {0, 64, 80, 64}, {0, 0, 60, 0}, {96, 64, 50, 40}, {64, 64, 64, 16}, {90, 100, 40, 30}}, 100, 0, 100, 0, 64, 0, \
                 {60, 85, 51, 105}, 0, 0, 0, 0, 0, 4, 1, 50, 40, 30, 9, 0, 84, 24, 100, 0, 0, {{0}}}
static track_t trk[NT] = {TRK_DEF, TRK_DEF, TRK_DEF, TRK_DEF};   /* VA: SAW, DETUNE 10, CUTOFF 90, RESONANCE 30
                                                 * (Felucca's ANALOG); FM: BELL as made, DETUNE 20; SAMPLE: slot 1,
                                                 * the whole sample, forward; VOICE: A, SHIFT 0, BUZZ 64, BREATH 10
                                                 * (Felucca's); WHEEL: 3BAR, PERC OFF, CLICK 40, DRIVE 0 (Felucca's);
                                                 * PHYS: MODAL, STRUCTURE 64, DAMP 80, bowed at 50 %; PHASE: SAW,
                                                 * DCW 60, one line (Felucca's); HOWL: FEEDBACK 96 (awake), BODY 64, DAMP 50, DRIVE 40; (2.0) BIEGGA:
                                                 * GUST, HOWL, TONE half way, SNOW 16; (2.2) JIEKNA: STRIKE 90, RING
                                                 * 100, HOLLOW 40, BRITTLE 30; LEVEL 100,
                                                 * DLY 0, REV 100, GRN 0, PAN the middle; LATCH OFF; ENV: ATTACK
                                                 * ~0.2 s, DECAY 1 s, SUSTAIN 40 %, RELEASE ~5 s; chord mode OFF (SCALE, CLOSE); ARP OFF (UP,
                                                 * 1/16, 1 octave, GATE 50 %); LFO DEPTH 30 % (slow, TRK CUTOFF,
                                                 * SINE); the filter at CUTOFF 84, RESONANCE 24 (Arctic Scream
                                                 * p0larpatch: darker, and every knob of TRACK, ENV, LFO heard from the
                                                 * start); FX 100 %; FM: its patch, BELL's (fm_builtin at
                                                 * power-on) */
static voice_t vc[NV];
static int32_t vc_xf[NV], vc_xq[NV], vc_xe[NV];   /* (2.0) a voice crossing over to its track's new source
                                                 * (restart_track): how far along a control step (Q24; 0 not crossing),
                                                 * how far now, its level when it began (its level: that x 1 - the cube
                                                 * of how far, the new sound's attack's mirror: the two hold the sum) */
static uint32_t vblock;                         /* audio: + 1 every control block (fm.c: once a block a track) */
/* (2.2) ENV 2: ONSET, a note's wait before it sounds; the voice steps (after SHRUB's voice control, written anew): each
 * new note of a track takes the next of four steps, a place in the stereo field (SPREAD: its share in the track's side
 * signal, sbuf: mixed L + side, R - side) and a detune (DETUNE), round and round */
static uint32_t vc_wait[NV];                    /* audio: control steps left before the note sounds (ONSET) */
static uint32_t vc_held[NV], vc_gate[NV];       /* audio: (ONSET) a note let go while it waited: how long it was held,
                                                 * then how long it still plays (control steps; 0: as held) */
static uint8_t vc_vs[NV], vs_next[NT];          /* audio: a voice's step; a track's next */
static const int16_t VS_PAN[4] = {-32767, 32767, -16384, 16384}, VS_DET[4] = {2, -2, -1, 1};
static uint32_t sb_used;                        /* audio: the tracks with a side signal this render (bit t) */

/* the screen's numbers (the engines' text functions use them too) */
static char *put_u(char *s, uint32_t v)         /* v in decimal at s; returns the end */
{
    char t[10];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *s++ = t[--n];
    *s = 0;
    return s;
}
static char *put_s(char *s, const char *a)     /* returns the end */
{
    while (*a) *s++ = *a++;
    *s = 0;
    return s;
}

/* the amplitude at frame i of a block that ramps from amp0 by damp (dsp.c amp_at: / CTL as a shift, rounded towards 0) */
static inline int32_t amp_at(int32_t amp0, int32_t damp, int32_t i)
{
    int32_t x = damp * i;
    return amp0 + ((x + ((x >> 31) & (int32_t)(CTL - 1))) >> CTL_LOG2);
}

/* the LFO's (lfo.c, the audio ISR a control step): a track's pitch offset (1/16 semitones: VP16, a voice's pitch, every
 * engine's), and a voice's parameters, one of them moved (else the track's own) */
static volatile int32_t lfo_p16[NT];
#define VP16(v) ((int32_t)(v)->note * 16 + lfo_p16[(v)->trk % NT])
static const volatile int32_t *lfo_params(uint32_t v, uint32_t t, uint32_t e);
/* SYSTEM's TUNE (main loop): every source's pitch times 1 + tune_f / 4096 (-50 .. +50 cents: -117 .. +120; finer than
 * VP16's 6.25-cent steps); vinc: a pitch's phase increment, tuned */
static volatile int32_t tune_f;
static uint32_t vc_at;                          /* audio: the frame in the render where the voice's block begins (HOWL's
                                                 * BLOOM reads the effects' return there) */
static int32_t vc_fine;                         /* audio: the voice being rendered's own detune, as tune_f (DRONE's
                                                 * BORGA, STOARBMA: dr_fine; 0 between voices) */
static int32_t dr_fine[NV], dr_gain[NV] = {32767, 32767, 32767, 32767, 32767, 32767};   /* audio: drone.c's, a voice's
                                                 * detune (tune_f units) and breath (its level, Q15) */
static int32_t dr_gain_was[NV] = {32767, 32767, 32767, 32767, 32767, 32767};   /* (the last block's: ramped) */
static int32_t ch_fine[NV];                     /* audio: chaos.c's, a voice's (WARP: its drift, vibrato, flutter, as
                                                 * tune_f) */
static int32_t tn_fine(uint32_t n);             /* (scale.c: SCALE's TUNING, a note's) */
static inline uint32_t vinc(int32_t p16)
{
    uint32_t inc = pitch_inc((uint32_t)clamp(p16, 0, 2047));
    return inc + (uint32_t)((int32_t)(inc >> 12) * (tune_f + vc_fine));
}

/* a voice's scratch in the pool, for the source it plays (a voice plays one at a time, and each begins anew on it):
 * SAMPLE's windows (sampler.c sp_w), Holvut's BODY line (howl.c) */
#define VX_POOL 4096u
static int16_t vx_pool[NV][VX_POOL] __attribute__((section(".pool")));

#include "analog.c"                             /* the engines */
#include "fm.c"
#include "sampler.c"
#include "formant.c"
#include "wheel.c"
#include "phys.c"
#include "phase.c"
#include "howl.c"                               /* (Arctic Scream p0larpatch: the feedback loop, after Audrey II) */
#include "biegga.c"                             /* (2.0: the wind) */
#include "jiekna.c"                             /* (2.2: the singing ice) */
static const engine_t *const ENGINES[NENG] = {&VA_ENGINE, &FM_ENGINE, &SAMPLE_ENGINE, &FORMANT_ENGINE, &WHEEL_ENGINE,
                                              &PHYS_ENGINE, &PHASE_ENGINE, &HOWL_ENGINE, &BIEGGA_ENGINE,
                                              &JIEKNA_ENGINE};   /* (by ENG_) */

/* GENERATE's notes (gen.c): a voice's volume (0: full; else x / 256) and attack (0: the track's ATTACK; else
 * GEN_ATK's k - 1), asked with the note (gen_vel, gen_atk: the next note_on's), taken as it begins */
static uint8_t vc_vel[NV], vc_atk[NV], vc_wvel[NV], vc_watk[NV], gen_vel, gen_atk;
static uint8_t gen_lo, gen_hi;                  /* the next note_on's range, GENERATE's (0 0: none): its chord's
                                                 * notes past it folded in by octaves (grimm.c note_on) */
static const int32_t GEN_ATK[8] = {2434783, 608696, 202899, 81159, 40580, 20290, 10145, 4870};   /* ATTACK steps
                                                 * (Q24 a control step): 5, 20, 60, 150, 300, 600, 1200, 2500 ms */

/* main loop: voice v plays note for track t (a sounding voice fades out first; the same note of the same track
 * sounding goes on up from where it is) */
static void voice_start(uint32_t v, uint32_t t, uint32_t note)
{
    vc_wvel[v] = gen_vel, vc_watk[v] = gen_atk; /* (GENERATE's; 0 0: the voice as ever) */
    vc[v].want_note = (uint8_t)note;
    vc[v].want_trk = (uint8_t)t;
    vc[v].want_on = 1;
    vc[v].req++;
}
static void voice_release(uint32_t v)
{
    vc[v].want_on = 0;
    vc[v].req++;
}
static void voice_fade(uint32_t v)              /* audio: out in ~10 ms (the fade), no note after it */
{
    vc[v].want_on = 0;
    if (vc[v].stage != ST_OFF) vc[v].stage = ST_FADE;
}
#define XF_STEP 8117                            /* (full to silent in ~1.5 s: 2^24 / (1.5 s / 32 frames)) */
#define XF_SLOW 2029                            /* (.. in ~6 s) */
static void voice_cross(uint32_t v, int32_t atk)   /* audio: out with the source it began with, no note after: over the
                                                 * new sound's ATTACK (atk, its step), ~1.5 s at least, ~6 s at most */
{
    vc[v].want_on = 0;
    if (vc[v].stage != ST_OFF)
        vc[v].stage = ST_REL, vc_xf[v] = atk > XF_STEP ? XF_STEP : atk < XF_SLOW ? XF_SLOW : atk, vc_xq[v] = 0,
        vc_xe[v] = vc[v].env;
}
static int voice_idle(uint32_t v) { return vc[v].stage == ST_OFF && vc[v].req == vc[v].seen; }
/* (2.0) the attack's curve: its level the cube of how far along it is (vc_atp, Q24), so a note flows in, a drone
 * swelling up out of nothing (half way: -18 dB; a straight line, before: -6 dB, there almost at once) */
static int32_t vc_atp[NV];
static int32_t atk_cube(int32_t p)              /* Q24 -> Q24 */
{
    int32_t t = p >> 12;
    return ((t * t) >> 12) * t;
}
static int32_t atk_root(int32_t e)              /* the cube's inverse: from a level, how far along (Q24) */
{
    int32_t t = 0, b;
    for (b = 2048; b; b >>= 1)
        if (atk_cube((t + b) << 12) <= e) t += b;
    return t << 12;
}
static int32_t voice_level(uint32_t v) { return vc[v].env; }

static void voice_begin(voice_t *v)             /* a note from silence */
{
    v->note = v->want_note;
    v->trk = v->want_trk < NT ? v->want_trk : 0u;
    if (!v->rng)
        v->rng = 1234567 + (int32_t)(v - vc);
    v->eng = (uint8_t)(trk[v->trk].eng >= 0 && trk[v->trk].eng < NENG ? trk[v->trk].eng : 0);
    ENGINES[v->eng]->begin(v);
    v->env = 0;
    vc_atp[v - vc] = 0;
    v->stage = ST_ATK;
    vc_vel[v - vc] = vc_wvel[v - vc], vc_atk[v - vc] = vc_watk[v - vc];   /* (GENERATE's volume, attack) */
    vc_vs[v - vc] = (uint8_t)(vs_next[v->trk]++ & 3u);   /* (2.2: the voice steps; ONSET: x 80 ms in control steps) */
    vc_wait[v - vc] = (uint32_t)clamp(trk[v->trk].e_onset, 0, 100) * 110u;
    vc_held[v - vc] = vc_gate[v - vc] = 0;
}

/* the tracks' ENV as steps, from the knobs, once a render: ATTACK a linear step (Q24 a control step), DECAY and RELEASE
 * the share of the way left a control step (Q16: 60 dB in the time), SUSTAIN (Q24). The times: ATTACK 1 ms x
 * 2^(v 20 / 192) (~9.6 s at 127), DECAY 10 ms x 2^(v 15 / 192) (~9.7 s), RELEASE 10 ms x 2^(v 16.5 / 192) (~19 s) */
typedef struct { int32_t atk, sus; uint32_t dec, rel; } env_c_t;
static env_c_t env_c[NT];
static uint32_t env_k(uint32_t t_ms_q16)        /* 60 dB in t: 6.9 / (t / 0.726 ms) of the way a control step, Q16 */
{
    uint32_t k = (uint32_t)((((uint64_t)328516u) << 16) / t_ms_q16);   /* (64 / 32: the chip's divide) */
    return k > 52429u ? 52429u : k ? k : 1u;
}
static void env_coef(env_c_t *c, const volatile int32_t *e)
{
    uint32_t ta = pow2_q16(clamp(e[0], 0, 127) * 20), td = 10u * pow2_q16(clamp(e[1], 0, 127) * 15);
    uint32_t tr = 10u * pow2_q16((clamp(e[3], 0, 127) * 33) >> 1);   /* (ms, Q16) */
    uint32_t a = (uint32_t)((((uint64_t)12173913u) << 16) / ta);       /* 2^24 / (t / 0.726 ms) */
    c->atk = (int32_t)(a > (1u << 24) ? 1u << 24 : a ? a : 1u);
    c->dec = env_k(td);
    c->rel = env_k(tr);
    c->sus = clamp(e[2], 0, 127) * 132104;     /* (2^24 / 127) */
}

/* one control step of the envelope (voice.c env_tick: ATK linear, REL exponential; Arctic Scream: the track's ENV, DECAY towards
 * SUSTAIN (it follows SUSTAIN when it moves), the fade) */
static int32_t env_tick(voice_t *v)
{
    const env_c_t *c = &env_c[v->trk % NT];
    int32_t e = v->env;
    if (v->req != v->seen) {                    /* the main loop's request */
        v->seen = v->req;
        vc_xf[v - vc] = 0;
        if (!v->want_on) {
            if (v->stage == ST_ATK && vc_wait[v - vc])   /* (2.2: let go while it waits: it still plays, as long as */
                vc_held[v - vc] |= 0x80000000u;        /* it was held, after the wait) */
            else if (v->stage == ST_ATK || v->stage == ST_SUS)
                v->stage = ST_REL;
        } else if (v->stage == ST_OFF) {
            voice_begin(v);
            e = 0;
        } else if (v->want_note == v->note && v->want_trk == v->trk && v->eng == trk[v->trk].eng && v->stage != ST_FADE) {
            v->stage = ST_ATK;                  /* the same note again (same engine): on up from where it is */
            vc_atp[v - vc] = atk_root(e);
            vc_vel[v - vc] = vc_wvel[v - vc], vc_atk[v - vc] = vc_watk[v - vc];
        } else {
            v->stage = ST_FADE;                 /* another note: out first, then begin */
        }
    }
    switch (v->stage) {
    case ST_ATK: {                              /* (2.0: along it in a straight line, its level the cube) */
        int32_t p;
        if (vc_wait[v - vc]) {                  /* (2.2: ONSET, still waiting) */
            uint32_t h = vc_held[v - vc];
            if (!(h >> 31)) vc_held[v - vc] = h + 1u;   /* (how long held) */
            if (!--vc_wait[v - vc] && h >> 31) vc_gate[v - vc] = (h & 0x7FFFFFFFu) + 1u;
            e = 0;
            break;
        }
        if (vc_gate[v - vc] && !--vc_gate[v - vc]) {   /* (its time held, played: let go) */
            v->stage = ST_REL;
            break;
        }
        p = vc_atp[v - vc] + (vc_atk[v - vc] ? GEN_ATK[(vc_atk[v - vc] - 1u) & 7u] : c->atk);   /* (V2:
                                                 * GENERATE's attack, or the track's) */
        if (p >= (1 << 24)) {
            p = 1 << 24;
            v->stage = ST_SUS;
        }
        vc_atp[v - vc] = p, e = atk_cube(p);
        break;
    }
    case ST_SUS: {                              /* DECAY to SUSTAIN, and SUSTAIN; (2.2) LOOP: PULSE down to nothing,
                                                 * BREATHE to SUSTAIN, and up again (from where it is) */
        int32_t lp = trk[v->trk % NT].e_loop, to = lp == EL_PULSE ? 0 : c->sus;
        int32_t d = mulq16(e - to, c->dec);
        if (vc_gate[v - vc] && !--vc_gate[v - vc]) {   /* (ONSET: its time held, played) */
            v->stage = ST_REL;
            break;
        }
        e -= d ? d : e > to ? 1 : e < to ? -1 : 0;
        if (lp > EL_OFF && lp < NELOOP && e - to < (1 << 24) / 48) {
            v->stage = ST_ATK;
            vc_atp[v - vc] = atk_root(e);
        }
        break;
    }
    case ST_REL:
        if (vc_xf[v - vc]) {                    /* (2.0: crossing over) */
            int32_t q = vc_xq[v - vc] += vc_xf[v - vc];
            if (q >= (1 << 24)) e = 0, v->stage = ST_OFF, vc_xf[v - vc] = 0;
            else e = (vc_xe[v - vc] >> 12) * (((1 << 24) - atk_cube(q)) >> 12);
            break;
        }
        e -= mulq16(e, c->rel);
        if (e < REL_END) {
            e = 0;
            v->stage = ST_OFF;
        }
        break;
    case ST_FADE:
        e -= FADE_STEP;
        if (e <= 0) {
            e = 0;
            v->stage = ST_OFF;
            if (v->want_on) {
                v->env = 0;
                voice_begin(v);
            }
        }
        break;
    default:
        e = 0;
        break;
    }
    v->env = e;
    return vc_vel[v - vc] ? ((e >> 9) * vc_vel[v - vc]) >> 8 : e >> 9;   /* Q15 (GENERATE's volume) */
}

/* the audio ISR: n frames (a multiple of CTL, at most VB) of every sounding voice into tb[its track] (mono); returns
 * the tracks that sound (bit t), whose buffers it zeroed first; the others' are left as they were */
#define VB 256u
static int32_t sbuf[NT][VB];                    /* audio: (2.2) the tracks' side signals (SPREAD) */
static void ctl_hook(void);                     /* grimm.c: a control block's notes (the main loop's asks, the
                                                 * sequencer), before the voices */
static uint32_t voices_render(int32_t (*tb)[VB], uint32_t n)
{
    uint32_t b, i, k, used = 0, envd = 0;
    if (n > VB)
        return 0;
    sb_used = 0;
    for (b = 0; b < n; b += CTL, vblock++) {
        ctl_hook();
        for (k = 0; k < NV; k++)                /* nothing sounds and nothing is asked for: nothing to do */
            if (vc[k].stage != ST_OFF || vc[k].amp || vc[k].req != vc[k].seen) break;
        if (k == NV)
            continue;
        if (!envd) {                            /* the tracks' ENV, as the knobs are now (once a render) */
            for (k = 0; k < NT; k++)
                env_coef(&env_c[k], trk[k].env);
            envd = 1;
        }
        for (k = 0; k < NV; k++) {
            voice_t *v = &vc[k];
            int32_t amp0 = v->amp, amp1 = env_tick(v);
            uint32_t t = v->trk;
            v->amp = amp1;
            if (!amp0 && !amp1)
                continue;                       /* silent: costs nothing */
            if (!(used >> t & 1u)) {            /* the track's first sound this block */
                used |= 1u << t;
                for (i = 0; i < n; i++)
                    tb[t][i] = 0;
            }
            vc_fine = dr_fine[k] + ch_fine[k] + tn_fine(v->note);   /* (DRONE: its detune and breath, drone.c; WARP's,
                                                 * chaos.c; 2.0 SCALE's TUNING, scale.c) */
            if (trk[t].detune > 0)              /* (2.2: DETUNE, the voice's step: up to +-24 cents) */
                vc_fine += VS_DET[vc_vs[k] & 3u] * clamp(trk[t].detune, 0, 100) * 28 / 100;
            if (dr_gain[k] < 32767 || dr_gain_was[k] < 32767) amp0 = mulq15(amp0, dr_gain_was[k]), amp1 = mulq15(amp1, dr_gain[k]);
            dr_gain_was[k] = dr_gain[k];
            vc_at = b;
            if (trk[t].spread > 0) {            /* (2.2: SPREAD: the voice alone, then into the track and its side) */
                int32_t vb[CTL], *o = tb[t] + b, *sd = sbuf[t] + b;
                int32_t g = VS_PAN[vc_vs[k] & 3u] * clamp(trk[t].spread, 0, 100) / 100;
                if (!(sb_used >> t & 1u)) {
                    sb_used |= 1u << t;
                    for (i = 0; i < n; i++) sbuf[t][i] = 0;
                }
                for (i = 0; i < CTL; i++) vb[i] = 0;
                ENGINES[v->eng]->block(v, lfo_params(k, t, v->eng), vb, amp0, amp1);
                for (i = 0; i < CTL; i++) o[i] += vb[i], sd[i] += ((vb[i] >> 4) * g) >> 11;   /* (+-2^19 x Q15: in 32 bits) */
            } else
                ENGINES[v->eng]->block(v, lfo_params(k, t, v->eng), tb[t] + b, amp0, amp1);   /* (the engine it began
                                                 * with; the LFO's on one of its knobs) */
            vc_fine = 0;
        }
    }
    return used;
}
