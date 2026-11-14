/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.2) MOTION: a knob's move, recorded by hand and played back in a loop (after SHRUB's motion sequencer, Steve Duda's,
 * its lanes of recorded knob moves; written anew, none of its code). Hold REC and turn a knob: from that turn on, how it
 * moves is taken (a point every 50 ms, up to two minutes), till REC is let go; then the move plays over and over, its
 * length the time it took, the knob moving by itself. A slow filter opening over a minute, a reverb swelling: by hand,
 * once.
 *   - four lanes, the project's (any track's knobs): a new move on a knob that has one replaces it; else a free lane;
 *     else the oldest
 *   - REC held + OCT-: the selected track's lanes cleared (said)
 *   - not on SYSTEM, SAVE, PERFORM, SEQ's, SONG's or SOURCE's pages, FM's PATCH, LATCH and START: what jumps or acts
 * A lane keeps 256 points over its length (the move taken at 50 ms, then spread over them), the knob gliding between.
 * The lanes are saved with the slots' stream (store.c: MO_AT), the project saved with each knob at its lane's first
 * value (mo_home: what plays never makes a save of its own, nor wears the flash). Main loop only. */

#define MO_STEP 50u                             /* ms a point, while recording */
#define MO_REC 2400u                            /* points at most: two minutes */
static uint8_t mo_buf[MO_REC] __attribute__((section(".pool")));
static uint32_t mo_rec, mo_n, mo_t0, mo_tn;     /* the lane recording (+ 1; 0: none); its points; its start, next */
static uint32_t mo_t[MO_N];                     /* a lane's loop: where it began */
static int32_t mo_was[MO_N];                    /* (mo_home: the knob's value, kept) */

static int mo_ok(uint32_t pg, uint32_t k)       /* a knob that can move by itself */
{
    if (pg == PG_SYSTEM || pg == PG_SYS2 || pg == PG_SYS3 || pg == PG_SYS4 || PAGES[pg].btn == BTN_SAVE || pg == PG_PERFORM ||
        PAGES[pg].btn == BTN_SEQ || pg == PG_SONG || pg == PG_SOURCE || pg == PG_TRACK)
        return 0;
    if ((pg == PG_SOUND && sel_eng() == ENG_FM && k == 0u) || (pg == PG_DRONE && k == 3u) || (pg == PG_JIEHKKI && k == 3u))
        return 0;                               /* (PATCH, LATCH, START) */
    return knob_param(pg, k) != 0;
}
static volatile int32_t *mo_ptr(const mo_lane_t *L, int32_t *mn, int32_t *mx)   /* the lane's knob, its range */
{
    uint32_t s = sel;
    int32_t op = fm_op;
    volatile int32_t *p;
    if (L->pg == PG_SOUND) {                    /* (that source's, the track's source now or not) */
        uint32_t e = L->x % NENG;
        *mn = 0, *mx = ENGINES[e]->max[L->k & 3u];
        return &trk[L->t & 3u].p[e][L->k & 3u];
    }
    sel = L->t & 3u;
    if (L->pg == PG_FMOP || (L->pg == PG_FMALGO && L->k == 3u)) fm_op = clamp((int32_t)L->x, 0, 5);
    p = knob_param(L->pg % NPAGE, L->k & 3u);
    *mn = knob_min(L->pg % NPAGE, L->k & 3u), *mx = knob_max(L->pg % NPAGE, L->k & 3u);
    sel = s, fm_op = op;
    return p;
}
static uint32_t mo_get(const mo_lane_t *L)      /* the knob now, 0 .. 127 over its range */
{
    int32_t mn, mx;
    volatile int32_t *p = mo_ptr(L, &mn, &mx);
    return p && mx > mn ? (uint32_t)((clamp(*p, mn, mx) - mn) * 127 / (mx - mn)) : 0u;
}
static int32_t mo_val(const mo_lane_t *L, uint32_t v, volatile int32_t **pp)   /* v (0 .. 127) as the knob's value */
{
    int32_t mn, mx;
    *pp = mo_ptr(L, &mn, &mx);
    return mn + ((int32_t)v * (mx - mn) + 63) / 127;
}

#if !defined DINGHY_HOST && !defined GRIMM_WEB && !defined FM1_PLATFORM_FILE
#define MO_HOLD() irq_save()                    /* (on the FM-1: the audio waits the few us of a save's packing) */
#define MO_LET(f) irq_restore(f)
#else
#define MO_HOLD() 0u
#define MO_LET(f) ((void)(f))
#endif
static uint32_t mo_irq;
static void mo_home(int on)                     /* (store.c, around packing the project) the lanes' knobs at their
                                                 * first value (on), then back */
{
    uint32_t i;
    volatile int32_t *p;
    if (on) mo_irq = MO_HOLD();
    for (i = 0; i < MO_N; i++) {
        int32_t v;
        if (!mo[i].on || mo_rec == i + 1u) continue;
        v = mo_val(&mo[i], mo[i].home, &p);
        if (!p) continue;
        if (on) mo_was[i] = *p, *p = v;
        else *p = mo_was[i];
    }
    if (!on) MO_LET(mo_irq);
}

static void mo_knob(uint32_t pg, uint32_t k)    /* REC held, knob k + 1 turned on page pg: a lane begins (the first
                                                 * knob only) */
{
    mo_lane_t L;
    uint32_t i, j;
    if (mo_rec) return;
    if (!mo_ok(pg, k)) { hd_say("NO MOTION HERE"); return; }
    if (pg == PG_HOME && k == 0u) pg = PG_MIX1; /* (HOME's VOLUME: the track's LEVEL) */
    L.on = 0, L.pg = (uint8_t)pg, L.k = (uint8_t)k, L.t = (uint8_t)(PAGES[pg].own ? sel : 0u);
    L.x = (uint8_t)(pg == PG_SOUND ? sel_eng() : pg == PG_FMOP || (pg == PG_FMALGO && k == 3u) ? (uint32_t)fm_op : 0u);
    for (i = 0; i < MO_N; i++)                  /* (the same knob's lane, a free one, the oldest) */
        if (mo[i].on && mo[i].pg == L.pg && mo[i].k == L.k && mo[i].t == L.t && mo[i].x == L.x) break;
    if (i == MO_N)
        for (i = 0; i < MO_N && mo[i].on; i++)
            ;
    if (i == MO_N)
        for (i = 0, j = 1; j < MO_N; j++)
            if (mo_t[j] < mo_t[i]) i = j;
    mo[i].on = 0, mo[i].pg = L.pg, mo[i].k = L.k, mo[i].t = L.t, mo[i].x = L.x;
    mo_rec = i + 1u, mo_n = 0, mo_t0 = mo_tn = fm1_ms;
    {
        char *e = put_s(hd_msg, "MOTION: ");
        if (PAGES[pg].own) e = put_s(put_u(put_s(e, "T"), L.t + 1u), " ");
        put_s(e, knob_name(pg, k));
        hd_until = fm1_ms + 2000u;
    }
}
static void mo_clear(uint32_t t)                /* REC held + OCT-: track t's lanes gone */
{
    uint32_t i, n = 0;
    for (i = 0; i < MO_N; i++)
        if (mo[i].on && mo[i].t == t) mo[i].on = 0, n++, mo_dirty = 1;
    put_s(put_u(put_s(hd_msg, n ? "MOTION CLEARED: T" : "NO MOTION ON T"), t + 1u), "");
    hd_until = fm1_ms + 2000u;
}
static void mo_end(void)                        /* REC let go: the move spread over the lane's points, playing */
{
    mo_lane_t *L = &mo[mo_rec - 1u];
    uint32_t i, len = fm1_ms - mo_t0;
    mo_rec = 0;
    if (mo_n < 4u || len < 200u) { hd_say("MOTION: TOO SHORT"); return; }
    for (i = 0; i < MO_PTS; i++) {              /* (point i: the take at i / MO_PTS of the way, between its two) */
        uint32_t q = i * (mo_n - 1u) * 16u / MO_PTS, a = q >> 4, f = q & 15u;
        uint32_t b = a + 1u < mo_n ? a + 1u : a;
        L->v[i] = (uint8_t)((mo_buf[a] * (16u - f) + mo_buf[b] * f + 8u) >> 4);
    }
    L->home = L->v[0], L->len10 = (uint16_t)(len / 10u > 65535u ? 65535u : len / 10u), L->on = 1;
    mo_t[L - mo] = fm1_ms, mo_dirty = 1;
    hd_say("MOTION: PLAYING");
}
static void mo_tick(uint32_t rec_held)          /* main loop: the recording taken, the lanes played */
{
    uint32_t i;
    if (mo_rec) {
        if (!rec_held || mo_n >= MO_REC) { mo_end(); return; }
        while (fm1_ms - mo_tn < 0x80000000u && mo_n < MO_REC)   /* (a point every MO_STEP ms) */
            mo_buf[mo_n++] = (uint8_t)mo_get(&mo[mo_rec - 1u]), mo_tn += MO_STEP;
    }
    for (i = 0; i < MO_N; i++) {
        uint32_t len, pos, q, a, f, v;
        volatile int32_t *p;
        int32_t x;
        if (!mo[i].on || mo_rec == i + 1u) continue;
        len = (uint32_t)mo[i].len10 * 10u;
        pos = (fm1_ms - mo_t[i]) % len;
        q = pos * (MO_PTS * 16u) / len, a = q >> 4, f = q & 15u;   /* (pos < 655 s: in 32 bits) */
        v = (mo[i].v[a % MO_PTS] * (16u - f) + mo[i].v[(a + 1u) % MO_PTS] * f + 8u) >> 4;
        x = mo_val(&mo[i], v, &p);
        if (p && *p != x) *p = x;
    }
}
