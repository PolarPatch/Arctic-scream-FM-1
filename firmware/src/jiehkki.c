/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream 1.5.3's JIEHKKI page (LFO, tapped a third time; North Sami: a glacier, sanit.oahpa.no): a piece the
 * FM-1 plays on its own for as long as LENGTH says, moving as slowly as a glacier, then dying out.
 *   LENGTH     5, 10, 15, 25, 30, 60 MIN, ENDLESS: the piece's time
 *   DRIFT      0 .. 100 %: how much moves while it plays: the tracks' notes (its own; twice as often in its
 *              opening), their presence in the mix (a track rests a while, comes back), their filters (opening over
 *              the first minutes, closing as it dies out). 0: nothing moves, the piece only rises and dies out: a
 *              sleep timer
 *   RUOHCAT    (to creak, crack: sanit.oahpa.no) 0 .. 100 %: the ice around the drone, a crack, a drip, a deep boom now
 *              and then, each its own, from far apart (1 %, a minute or so) to a storm of them (100 %); heard whenever
 *              it is up, the piece playing or not. 0: none
 *   START      OFF, ON: the piece. Turned ON with nothing sounding, it plays its own notes, one a track (from the
 *              SCALE: ROOT low on T1, its fifth, a third and a fifth or second over it; SCALE OFF: as NATURAL
 *              MINOR), the tracks coming in one after another as it rises out of silence (10 .. 30 s); with notes
 *              sounding, it takes them as they are (and moves the mix alone). At its end
 *              (LENGTH) all of it has faded out: the notes are let go, the sequencer stops, GENERATE stops, the
 *              screen goes dark, and the FM-1 stays silent until a hand (a key, a button, a knob, a MIDI note) wakes
 *              it. Turned OFF: its notes let go, the mix as set again. ENDLESS: no end; its own notes still move
 * The main loop decides (jk_tick, every JK_TICK ms): the notes go through nq (as a key's, the ids JK_ID ..), the
 * gains and the filters' moves through jk_og, jk_tg, jk_cut; the audio ISR ramps them (jk_out, mix_tracks, trk_filter)
 * and plays RUOHCAT's sounds (ru_render: noise through band-passes, a falling sine, a slow low one; into the mix and
 * the REV bus). LENGTH, DRIFT and RUOHCAT are the project's (store.c, version 9); START is not saved.
 *
 * The RUOHCAT page (LFO, tapped a fourth time): the ice's sounds with your own samples among them (a joik heard far off
 * over the drone, the wind, water):
 *   AMOUNT     RUOHCAT's knob, as on JIEHKKI: how often
 *   SAMPLE     OFF, SLOT 1 .. 4, ALL: which sample slots it plays from (ALL: any that holds one)
 *   SHARE      0 .. 100 %: of its sounds, how many are the samples (100 %: the samples alone)
 *   VARY       0 .. 100 %: 0, a sample whole, from its start, as recorded; up, from further in, shorter, now and then a
 *              fourth, a fifth or an octave down or up
 * A sample comes in over 20 ms and goes over its last 100 ms, panned, with a share into SPACE; two at most at once.
 * SAMPLE, SHARE and VARY are the project's too (store.c's EXT). */

#define JK_TICK 50u                             /* ms between the main loop's looks */
#define JK_ENDLESS 6                            /* (JK_NLEN: grimm.c) */
static const uint8_t JK_MIN[JK_NLEN] = {5, 10, 15, 25, 30, 60, 0};   /* LENGTH's minutes (0: ENDLESS) */
static const char *const JK_LEN_NAME[JK_NLEN] = {"5 MIN", "10 MIN", "15 MIN", "25 MIN", "30 MIN", "60 MIN", "ENDLESS"};
#define JK_ID 202u                              /* its notes' origin (lat_id): JK_ID + 2 t + (0, 1) */
static const uint8_t JK_LO[NT] = {33, 43, 45, 52}, JK_HI[NT] = {52, 62, 64, 76};   /* a track's notes' range */

static volatile int32_t jk_len = 2, jk_drift = 40, jk_ruo, jk_go;   /* the knobs: 15 MIN, 40 %, 0, OFF */
static volatile int32_t jk_og = 32767, jk_tg[NT] = {32767, 32767, 32767, 32767}, jk_cut[NT];   /* main loop -> audio:
                                                 * the output's gain, a track's (Q15), its filter's move (CUTOFF << 8) */
static volatile uint32_t jk_snap;              /* main loop -> audio: + 1, the output's gain to jk_og at once (a piece
                                                 * of its own begins: no blip of its first notes before the rise) */
static int32_t jk_oga = 32767, jk_tga[NT] = {32767, 32767, 32767, 32767};   /* audio: as ramped */
static struct {
    uint32_t run, own, t0, len, tick, ended, end_t, next_move, next_g[NT], enter[NT];
    uint8_t note[NT], id[NT], want[NT];         /* its own notes now (0: none), their ids; a note to come in (enter) */
    int32_t cut_t[NT], cut[NT];                 /* a filter's move: where it goes, where it is (<< 8) */
    uint32_t rng;
} jk = {.rng = 0x2545F491u};
static volatile uint32_t jk_sleep;              /* main loop: the piece is over, the screen to go dark (grimm.c) */

static uint32_t jk_rnd(uint32_t n)
{
    jk.rng ^= jk.rng << 13, jk.rng ^= jk.rng >> 17, jk.rng ^= jk.rng << 5;
    return (uint32_t)(((uint64_t)jk.rng * n) >> 32);
}
static int jk_in(int32_t n)                     /* in the scale (SCALE OFF: NATURAL MINOR on ROOT) */
{
    return sc_scale > 0 ? sc_in(n) : 0x5ADu >> ((uint32_t)(n + 132 - clamp(sc_root, 0, 11)) % 12u) & 1u;
}
static int32_t jk_step(int32_t n, int32_t d)    /* d notes of the scale on from n */
{
    int32_t s = d > 0 ? 1 : -1;
    for (; d; d -= s)
        do n += s; while (n > 0 && n < 127 && !jk_in(n));
    return n;
}
static int32_t jk_fit(uint32_t t, int32_t n)    /* n into track t's range, by octaves, in the scale */
{
    while (n > JK_HI[t]) n -= 12;
    while (n < JK_LO[t]) n += 12;
    while (!jk_in(n) && n > 0) n--;
    return n;
}
static uint32_t jk_len_ms(void)
{
    return (uint32_t)JK_MIN[clamp(jk_len, 0, JK_NLEN - 1)] * 60000u;
}
static uint32_t jk_left(uint32_t now)           /* ms left of the piece (ENDLESS, not playing: 0) */
{
    uint32_t L = jk.len, e = now - jk.t0;
    return jk.run && L && e < L ? L - e : 0;
}

static void jk_play(uint32_t t, int32_t n)      /* track t's own note to n (the new one on, then the old one let go) */
{
    uint32_t id = JK_ID + 2u * t + (jk.id[t] == JK_ID + 2u * t);
    if (n == jk.note[t]) return;
    if (n > 0) nq_note(NQ_ON, t, (uint32_t)n, id);
    if (jk.note[t]) nq_note(NQ_OFF, t, jk.note[t], jk.id[t]);
    jk.note[t] = (uint8_t)(n > 0 ? n : 0), jk.id[t] = (uint8_t)id;
}
static void jk_notes_off(void)
{
    uint32_t t;
    for (t = 0; t < NT; t++) jk_play(t, 0), jk.want[t] = 0;
}
static uint32_t jk_open(uint32_t L)            /* its opening (ms): the tracks come in, the filters open, the notes move
                                                 * twice as often: L / 6, 4 min at most (ENDLESS: 4 min) */
{
    return L && L / 6u < 240000u ? L / 6u : 240000u;
}
static uint32_t jk_fo(uint32_t L)              /* its dying out (ms): L / 5, 1 .. 8 min */
{
    uint32_t fo = L / 5u;
    return fo < 60000u ? 60000u : fo > 480000u ? 480000u : fo;
}
static void jk_calm(void)                       /* the mix as set: every gain up, the filters back */
{
    uint32_t t;
    for (t = 0; t < NT; t++) jk_tg[t] = 32767, jk.cut_t[t] = 0;
}

static void jk_start(uint32_t now)
{
    int32_t r = clamp(sc_root, 0, 11), n1 = jk_fit(0, 36 + r);
    uint32_t t, gap;
    jk.run = 1, jk.t0 = now, jk.len = jk_len_ms(), jk.ended = 0;
    jk.own = !tracks_latched() && !playing;     /* (nothing sounding: its own notes, out of silence) */
    gap = jk.len ? jk.len / 60u : 12000u;       /* (the tracks coming in: 5 .. 12 s apart) */
    gap = gap < 5000u ? 5000u : gap > 12000u ? 12000u : gap;
    jk.next_move = now + 3u * gap + 8000u + jk_rnd(8000u);
    jk_calm();
    if (jk.own) {
        jk_og = 0;                              /* (risen from silence: jk_arc) */
        jk_snap = jk_snap + 1u;
        jk_play(0, n1);
        jk.want[1] = (uint8_t)jk_fit(1, jk_step(n1, 4));   /* (the fifth: four notes of the scale up) */
        jk.want[2] = (uint8_t)jk_fit(2, jk_step(n1 + 12, 2));   /* (a third, an octave up) */
        jk.want[3] = (uint8_t)jk_fit(3, jk_step(n1 + 24, jk_rnd(2) ? 4 : 1));   /* (a fifth or second over it) */
        for (t = 1; t < NT; t++) jk.enter[t] = now + t * gap - gap / 4u + jk_rnd(gap / 2u);
    }
}
static void jk_stop(void)                       /* turned OFF: its notes out, the mix as set */
{
    jk_notes_off();
    jk.run = 0;
    jk_calm();
    jk_og = 32767;
}
static void jk_end(uint32_t now)                /* LENGTH played out: silence till a hand wakes it */
{
    uint32_t t;
    jk_notes_off();
    for (t = 0; t < NT; t++) nq_put(NQ_RELEASE, t, 0);   /* (the latched notes, the arps) */
    if (playing) playing = 0, nq_put(NQ_STOP, 0, (uint32_t)slot_cur & 3u);
    gen_on = 0;
    jk.run = 0, jk.ended = 1, jk.end_t = now, jk_go = 0;
    jk_og = 0;
    jk_sleep = 1;
}

static int32_t jk_arc_at(uint32_t e, uint32_t L, uint32_t own)   /* the output's gain (Q15) e ms into a piece L long
                                                 * (0: ENDLESS): risen out of silence (its own notes: over L / 16,
                                                 * 10 .. 30 s, quick at first, heard from its first seconds), dying out
                                                 * (jk_fo) */
{
    uint32_t fi = L ? L / 16u : 30000u, fo = jk_fo(L);
    int32_t g = 32767, x;
    fi = fi < 10000u ? 10000u : fi > 30000u ? 30000u : fi;
    if (own && e < fi) x = (int32_t)((uint64_t)e * 32767u / fi), g = x * (65534 - x) >> 15;
    if (L && e + fo > L) {
        x = e >= L ? 0 : (int32_t)((uint64_t)(L - e) * 32767u / fo);
        x = x * x >> 15;
        if (x < g) g = x;
    }
    return g;
}
static int32_t jk_arc(uint32_t now) { return jk_arc_at(now - jk.t0, jk.len, jk.own); }

static void jk_glide(uint32_t now)              /* the filters' moves glide to where they go (~3 s; while it plays,
                                                 * opened over its opening, closed again as it dies out: + 10 at DRIFT
                                                 * 100 %), back to 0 once it is off */
{
    int32_t d = jk.run ? clamp(jk_drift, 0, 100) : 0, mid = 0, t;
    uint32_t e = now - jk.t0, op = jk_open(jk.len), fo = jk_fo(jk.len);
    if (d) {
        mid = e < op ? (int32_t)((uint64_t)e * 16u / op) : 16;   /* (0 .. 16) */
        if (jk.len && e + fo > jk.len) {
            int32_t c = e >= jk.len ? 0 : (int32_t)((uint64_t)(jk.len - e) * 16u / fo);
            if (c < mid) mid = c;
        }
    }
    for (t = 0; t < (int32_t)NT; t++) {
        jk.cut[t] += (jk.cut_t[t] + mid * d * 256 / 160 - jk.cut[t]) / 64;
        jk_cut[t] = jk.cut[t];
    }
}
static void jk_move(uint32_t now)               /* DRIFT: a note, a track's presence, the filters' targets */
{
    int32_t d = clamp(jk_drift, 0, 100), t, n;
    if (!d) { jk_calm(); return; }
    if (jk.own && (int32_t)(now - jk.next_move) >= 0) {   /* (a note: every ~4 min at 1 %, ~25 s at 100 %; in
                                                 * its opening, twice as often) */
        uint32_t m = 240000u - (uint32_t)d * 2150u;
        if (now - jk.t0 < jk_open(jk.len)) m /= 2u;
        jk.next_move = now + m * 6u / 10u + jk_rnd(m * 8u / 10u);
        int32_t k;
        t = jk_rnd(6) ? 1 + (int32_t)jk_rnd(3) : 0;
        n = jk.note[t];
        if (n) {
            if (t == 0) {                       /* (T1, the root: to its fourth or fifth and home again) */
                int32_t home = jk_fit(0, 36 + clamp(sc_root, 0, 11));
                n = n != home && jk_rnd(10) < 6 ? home : jk_fit(0, jk_step(home, jk_rnd(2) ? 3 : 4));
            } else {
                int32_t s = 1 + (int32_t)jk_rnd(2);
                n = jk_step(n, jk_rnd(2) ? s : -s);
                if (n < JK_LO[t] || n > JK_HI[t]) n = jk_step(jk.note[t], n < JK_LO[t] ? s : -s);
                for (k = 0; k < (int32_t)NT; k++)   /* (never on another track's note) */
                    if (k != t && jk.note[k] == n) n = jk_step(n, 1);
                n = clamp(n, JK_LO[t], JK_HI[t]);
            }
            for (k = 0; k < (int32_t)NT; k++)   /* (never on another track's note: this move left out) */
                if (k != t && jk.note[k] == n) n = jk.note[t];
            jk_play((uint32_t)t, n);
        }
    }
    for (t = 0; t < (int32_t)NT; t++) {
        if ((int32_t)(now - jk.next_g[t]) >= 0) {   /* (a track's presence, its filter: every 40 .. 120 s) */
            int32_t g = 32767 - (int32_t)jk_rnd((uint32_t)d * 200u);
            jk.next_g[t] = now + 40000u + jk_rnd(80000u);
            if (t && jk_rnd(250) < (uint32_t)d) g = 3000;   /* (a rest a while) */
            jk_tg[t] = g;
            jk.cut_t[t] = ((int32_t)jk_rnd(29) - 14) * d * 256 / 100;
        }
    }
}

/* ---- RUOHCAT: the main loop picks an event's time and its sound, the audio plays it */
enum { RU_CRACK, RU_DRIP, RU_BOOM, RU_SMP };
typedef struct {
    uint8_t type, hits;                         /* RU_; a crack's sub-cracks to come */
    uint16_t gap;                               /* .. samples between them (at most) */
    int32_t cut1, cut2, amp, pan, send;         /* band-passes (CUTOFF << 8), level (Q15), PAN (0 .. 32767), REV send */
    uint32_t inc;                               /* a drip's, a boom's pitch (phase step); a sample's speed (Q16 of its
                                                 * samples an output sample) */
    uint32_t slot, seg, len;                    /* a sample's slot, its first segment, its length (its samples) */
} ru_ev_t;
static volatile int32_t ru_smp, ru_share = 50, ru_vary = 30;   /* the RUOHCAT page's knobs: SAMPLE (0 OFF, 1 .. 4 a slot,
                                                 * 5 ALL), SHARE %, VARY % */
static const char *const RU_SMP_NAME[RU_NSMP] = {"OFF", "SLOT 1", "SLOT 2", "SLOT 3", "SLOT 4", "ALL"};
static const char *const RU_SMP_SHORT[RU_NSMP] = {"OFF", "S1", "S2", "S3", "S4", "ALL"};
#define RS_N 2u                                 /* samples playing at once at most */
static volatile uint32_t ru_w;                  /* main loop: + 1 an event (ru_q[ru_w % 4]) */
static ru_ev_t ru_q[4];
static uint32_t ru_next;                        /* main loop: the next event's time */
static volatile uint32_t ru_hit;                /* audio: + 1 an event begun (the screen's flash) */
static uint8_t ru_last;                         /* main loop: the last event's type (the screen) */

static volatile uint32_t rs_busy;               /* audio: the samples playing now (the main loop: room for one more) */
static int ru_pick(ru_ev_t *E)                  /* main loop: a sample event into E (0: none to play, no room) */
{
    uint32_t k, n, ok = 0, v = (uint32_t)clamp(ru_vary, 0, 100), sg, nseg;
    int32_t semi = 0;
    static const int8_t SEMI[5] = {-12, -7, -5, 5, 7};
    if (ru_smp <= 0 || rs_busy >= RS_N) return 0;
    for (k = 0; k < SMP_SLOTS; k++)             /* (the slots it may play: ready ones) */
        if (smp[k].ready && smp[k].n >= SMP_SEG * 4u && (ru_smp >= RU_NSMP - 1 || (uint32_t)ru_smp == k + 1u)) ok |= 1u << k;
    if (!ok) return 0;
    do k = jk_rnd(SMP_SLOTS); while (!(ok >> k & 1u));
    n = smp[k].n, nseg = n / SMP_SEG;
    sg = v ? jk_rnd(nseg * v / 100u / 2u + 1u) : 0;   /* (VARY: from up to half way in) */
    E->type = RU_SMP, E->slot = k, E->seg = sg;
    E->len = n - sg * SMP_SEG;
    if (v) E->len -= (uint32_t)(((uint64_t)E->len * jk_rnd(v * 7u + 1u)) / 1000u);   /* (shorter: up to 70 %) */
    if (E->len < SMP_SEG * 4u) E->len = SMP_SEG * 4u;
    if (v && jk_rnd(140) < v) semi = SEMI[jk_rnd(5)];   /* (now and then a fourth, a fifth, an octave) */
    E->inc = pow2_q16(semi * 16) >> 1;          /* (22050 Hz: half a sample of it an output sample) */
    E->amp = 13000 + (int32_t)jk_rnd(5000);
    E->pan = 6000 + (int32_t)jk_rnd(20768);
    E->send = 10000 + (int32_t)jk_rnd(14000);
    return 1;
}

static void ru_tick(uint32_t now)
{
    int32_t d = clamp(jk_ruo, 0, 100);
    uint32_t mean, r;
    ru_ev_t *E;
    if (!d || jk.ended) { ru_next = now + 2000u; return; }
    if ((int32_t)(now - ru_next) < 0) return;
    mean = 60000u >> (d / 16);                  /* (1 %: ~a minute apart; 50 %: ~8 s; 100 %: under 1 s) */
    mean -= mean * (uint32_t)(d % 16) / 32u;
    ru_next = now + mean / 4u + jk_rnd(mean * 3u / 2u);
    E = &ru_q[ru_w % 4u];
    r = jk_rnd(100);
    E->type = r < 55u ? RU_CRACK : r < 85u ? RU_DRIP : RU_BOOM;
    if (jk_rnd(100) < (uint32_t)clamp(ru_share, 0, 100) && ru_pick(E)) {
        ru_last = RU_SMP;
        ru_w = ru_w + 1u;
        return;
    }
    if (ru_smp && ru_share >= 100) return;      /* (the samples alone: none ready, nothing) */
    E->pan = (int32_t)jk_rnd(32768);
    E->send = 12000 + (int32_t)jk_rnd(16000);
    switch (E->type) {
    case RU_CRACK:                              /* (a band at 2 .. 6 kHz, its body at 300 .. 800 Hz; 1 .. 6 cracks) */
        E->cut1 = (85 + (int32_t)jk_rnd(22)) << 8, E->cut2 = (46 + (int32_t)jk_rnd(20)) << 8;
        E->hits = (uint8_t)jk_rnd(6), E->gap = (uint16_t)(200u + jk_rnd(1600));
        E->amp = 4500 + (int32_t)jk_rnd(4500);
        break;
    case RU_DRIP:                               /* (1.3 .. 3.2 kHz, falling) */
        E->inc = (1300u + jk_rnd(1900)) * 97391u, E->amp = 1800 + (int32_t)jk_rnd(2400);
        break;
    default:                                    /* (40 .. 70 Hz, a few seconds) */
        E->inc = (40u + jk_rnd(30)) * 97391u, E->amp = 4000 + (int32_t)jk_rnd(4000);
        E->cut1 = (34 + (int32_t)jk_rnd(8)) << 8, E->send = 20000;
        break;
    }
    ru_last = E->type;
    ru_w = ru_w + 1u;
}

#define RU_N 3                                  /* events sounding at once at most */
static struct {
    uint8_t on, type, hits;
    uint16_t gap, cnt;
    uint8_t sh1, sh2;                           /* its envelopes' falls: e -= e >> sh a sample (2^sh samples) */
    int32_t e1, e2, amp, gl, gr, send, n1, n2, n3, n4, lp;   /* (e1, e2: Q23, amp << 8) */
    uint32_t ph, inc, inc_lo;
    tsvf_t f1, f2;
    int32_t k1, k2;
} ru[RU_N];
static struct {                                /* audio: the samples playing */
    uint8_t on, slot;
    uint32_t pos, end, frac, step, att;         /* the next sample to decode, its end; Q16 between; samples played */
    int32_t pred, idx, y0, y1, amp, gl, gr, send;
} ru_sp[RS_N];
static uint32_t ru_r;                           /* audio: the events taken */
static int32_t ru_rng = 0x1B873593;

static void rs_begin(const ru_ev_t *E)
{
    uint32_t i, best = 0;
    const smp_slot_t *S = &smp[E->slot & 3u];
    if (!S->ready) return;
    for (i = 0; i < RS_N; i++) {                /* (a free one; none: the one furthest on goes) */
        if (!ru_sp[i].on) { best = i; break; }
        if (ru_sp[i].att > ru_sp[best].att) best = i;
    }
    i = best;
    ru_sp[i].on = 1, ru_sp[i].slot = (uint8_t)(E->slot & 3u);
    ru_sp[i].pos = E->seg * SMP_SEG, ru_sp[i].end = ru_sp[i].pos + E->len;
    if (ru_sp[i].end > S->n) ru_sp[i].end = S->n;
    ru_sp[i].pred = smp_ipred[E->slot & 3u][E->seg], ru_sp[i].idx = smp_iidx[E->slot & 3u][E->seg];
    ru_sp[i].frac = 0, ru_sp[i].step = E->inc, ru_sp[i].att = 0, ru_sp[i].y0 = ru_sp[i].y1 = 0;
    ru_sp[i].amp = E->amp, ru_sp[i].send = E->send;
    ru_sp[i].gl = 32767 - (E->pan >> 1), ru_sp[i].gr = 16384 + (E->pan >> 1);
    ru_hit++;
}
static void rs_render(int32_t *out, int32_t *rev, uint32_t n)   /* audio: the samples, read on from their slots */
{
    uint32_t k, i, busy = 0;
    for (k = 0; k < RS_N; k++) {
        const smp_slot_t *S = &smp[ru_sp[k].slot];
        if (!ru_sp[k].on) continue;
        if (!S->ready) { ru_sp[k].on = 0; continue; }   /* (its slot cleared, written over) */
        for (i = 0; i < n; i++) {
            int32_t y, g, left;
            for (ru_sp[k].frac += ru_sp[k].step; ru_sp[k].frac >= 65536u; ru_sp[k].frac -= 65536u) {
                ru_sp[k].y0 = ru_sp[k].y1;
                ru_sp[k].y1 = ru_sp[k].pos < ru_sp[k].end ? ima_dec(S->data, ru_sp[k].pos, &ru_sp[k].pred, &ru_sp[k].idx) : 0;
                ru_sp[k].pos++;
            }
            y = ru_sp[k].y0 + (((ru_sp[k].y1 - ru_sp[k].y0) * (int32_t)(ru_sp[k].frac >> 2)) >> 14);
            left = (int32_t)ru_sp[k].end - (int32_t)ru_sp[k].pos;   /* (its last 100 ms: out) */
            g = left <= 0 ? 0 : left < 2205 ? left * 14 : 32767;
            if (ru_sp[k].att < 882u) g = g * (int32_t)ru_sp[k].att / 882;   /* (its first 20 ms: in) */
            ru_sp[k].att++;
            y = mulq15(mulq15(y, g), ru_sp[k].amp);
            out[2u * i] += mulq15(y, ru_sp[k].gl), out[2u * i + 1u] += mulq15(y, ru_sp[k].gr);
            rev[i] += mulq15(y, ru_sp[k].send);
        }
        if (ru_sp[k].pos >= ru_sp[k].end + 2u) ru_sp[k].on = 0;
        busy += ru_sp[k].on;
    }
    rs_busy = busy;
}

static void ru_begin(const ru_ev_t *E)
{
    uint32_t i, best = 0;
    if (E->type == RU_SMP) { rs_begin(E); return; }
    for (i = 0; i < RU_N; i++) {                /* (a free one; none: the quietest goes) */
        if (!ru[i].on) { best = i; break; }
        if (ru[i].e1 < ru[best].e1) best = i;
    }
    i = best;
    ru[i].on = 1, ru[i].type = E->type, ru[i].hits = E->hits, ru[i].gap = E->gap, ru[i].cnt = E->gap;
    ru[i].amp = E->amp, ru[i].e1 = ru[i].e2 = E->amp << 8, ru[i].send = E->send;
    ru[i].gl = 32767 - (E->pan >> 1), ru[i].gr = 16384 + (E->pan >> 1);
    ru[i].n1 = ru[i].n2 = ru[i].n3 = ru[i].n4 = ru[i].lp = 0, ru[i].ph = 0, ru[i].inc = E->inc;
    if (E->type == RU_CRACK) {
        ru[i].k1 = 900, ru[i].k2 = 1800;
        tsvf_coef_k(&ru[i].f1, E->cut1, ru[i].k1), tsvf_coef_k(&ru[i].f2, E->cut2, ru[i].k2);
        ru[i].sh1 = (uint8_t)(8u + (E->gap > 1000u) + (E->gap > 1400u));   /* (a crack ~6 .. 23 ms) */
    } else if (E->type == RU_DRIP) {
        ru[i].inc_lo = E->inc / 2u + E->inc / 16u, ru[i].sh1 = 11;   /* (~46 ms) */
    } else {
        ru[i].inc_lo = E->inc * 3u / 4u, ru[i].sh1 = 16, ru[i].sh2 = 10;   /* (~1.5 s; its rise ~23 ms) */
        tsvf_coef_k(&ru[i].f1, E->cut1, 5000);
    }
    ru_hit++;
}

static void ru_render(int32_t *out, int32_t *rev, uint32_t n)   /* audio: RUOHCAT's sounds into the mix, the REV bus */
{
    uint32_t i, k, w = ru_w;
    if (w - ru_r > 4u) ru_r = w - 4u;
    while (ru_r != w) ru_begin(&ru_q[ru_r++ % 4u]);
    rs_render(out, rev, n);
    for (k = 0; k < RU_N; k++) {
        if (!ru[k].on) continue;
        for (i = 0; i < n; i++) {
            int32_t y, z = (int32_t)(noise32(&ru_rng) >> 1) >> 16;   /* (-16384 .. 16383) */
            switch (ru[k].type) {
            case RU_CRACK:
                y = tsvf_any(&ru[k].f1, ru[k].k1, FT_BP, z, &ru[k].n1, &ru[k].n2) +
                    (tsvf_any(&ru[k].f2, ru[k].k2, FT_BP, z, &ru[k].n3, &ru[k].n4) >> 1);
                y = mulq15(y, ru[k].e1 >> 8);
                ru[k].e1 -= (ru[k].e1 >> ru[k].sh1) + (ru[k].e1 > 0);
                if (ru[k].hits && !--ru[k].cnt) {   /* (the next crack, as loud or softer) */
                    ru[k].hits--, ru[k].cnt = (uint16_t)(ru[k].gap / 2u + (uint32_t)(noise32(&ru_rng) % ru[k].gap));
                    ru[k].e1 = (ru[k].amp * (8 + (int32_t)(noise32(&ru_rng) & 7u)) / 16) << 8;
                }
                break;
            case RU_DRIP:
                y = mulq15(sine_i(ru[k].ph), ru[k].e1 >> 8);
                ru[k].ph += ru[k].inc;
                if (ru[k].inc > ru[k].inc_lo) ru[k].inc -= ru[k].inc >> 9;
                ru[k].e1 -= (ru[k].e1 >> ru[k].sh1) + (ru[k].e1 > 0);
                break;
            default:                            /* (the slow one less the fast one: a soft rise, a long fall) */
                ru[k].lp = tsvf_lp(&ru[k].f1, z, &ru[k].n1, &ru[k].n2);
                y = mulq15(sine_i(ru[k].ph) + ru[k].lp, (ru[k].e1 - ru[k].e2) >> 8);
                ru[k].ph += ru[k].inc;
                if (ru[k].inc > ru[k].inc_lo && !(i & 15u)) ru[k].inc -= ru[k].inc >> 10;
                ru[k].e1 -= (ru[k].e1 >> ru[k].sh1) + (ru[k].e1 > 0);
                ru[k].e2 -= (ru[k].e2 >> ru[k].sh2) + (ru[k].e2 > 0);
                break;
            }
            out[2u * i] += mulq15(y, ru[k].gl), out[2u * i + 1u] += mulq15(y, ru[k].gr);
            rev[i] += mulq15(y, ru[k].send);
        }
        if (ru[k].e1 < 2048 && !ru[k].hits) ru[k].on = 0;   /* (under 8 of its Q15: gone) */
    }
}

/* audio: the output's gain (JIEHKKI's rise and fall), ramped a step a sample (a sweep ~0.7 s at most) */
static void jk_out(int32_t *out, uint32_t n)
{
    static uint32_t snap;
    int32_t g = jk_oga, t = clamp(jk_og, 0, 32767);
    uint32_t i;
    if (snap != jk_snap) snap = jk_snap, g = t;
    if (g == 32767 && t == 32767) return;
    for (i = 0; i < n; i++) {
        g += g < t ? 1 : g > t ? -1 : 0;
        out[2u * i] = mulq16(out[2u * i], (uint32_t)g << 1);   /* (32-bit: no 64-bit multiply in the ISR) */
        out[2u * i + 1u] = mulq16(out[2u * i + 1u], (uint32_t)g << 1);
    }
    jk_oga = g;
}
static int32_t jk_trk(uint32_t t)               /* audio, a block: track t's gain (Q15), ramped (~0.5 s at most) */
{
    int32_t g = jk_tga[t], d = jk_tg[t] - g;
    g += d > 48 ? 48 : d < -48 ? -48 : d;
    return jk_tga[t] = g;
}

/* main loop, each pass: the knob START, the piece, RUOHCAT; touched: a hand's (or a MIDI note's) last time */
static void jk_tick(uint32_t now, uint32_t touched)
{
    if (jk.ended && (int32_t)(touched - jk.end_t) > 0) {   /* (over and silent: a hand wakes it) */
        jk.ended = 0, jk_og = 32767;
        jk_calm();
    }
    if (now - jk.tick < JK_TICK) return;
    jk.tick = now;
    if (jk_go && !jk.run) jk_start(now);
    else if (!jk_go && jk.run) jk_stop();
    if (jk.run) {
        uint32_t t;
        jk.len = jk_len_ms();                   /* (LENGTH turned while it plays: from its start still) */
        for (t = 1; t < NT; t++)                /* (its own tracks coming in) */
            if (jk.want[t] && (int32_t)(now - jk.enter[t]) >= 0) jk_play(t, jk.want[t]), jk.want[t] = 0;
        jk_og = jk_arc(now);
        jk_move(now);
        if (jk.len && now - jk.t0 >= jk.len) jk_end(now);
    }
    jk_glide(now);
    ru_tick(now);
}
