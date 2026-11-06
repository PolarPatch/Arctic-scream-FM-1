/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* The screen reader (Arctic Scream p0larpatch 1.5): what the screen shows, said to a reader on the computer
 * (fm1-reader, https://charlesvestal.github.io/fm1-reader/: a page that speaks it or hands it to VoiceOver, NVDA,
 * JAWS, TalkBack). announce.c (fm1-base's, MPL-2.0, Charles Vestal) keeps the protocol; this says what changed:
 *   a new page, track or source    the page, the track and its source, then its four knobs (settles 200 ms)
 *   a knob turned                  its name and its value as the screen shows it (settles 120 ms)
 *   a list (SOUNDS, PROJECTS), SEQ's cursor    the row or the step, with its place ("5 of 64")
 *   the header's messages          hd_say (grimm.c): at once; a warning heard whole (ANN_KEEP)
 *   PLAY, STOP, REC; SAVES WHEN SILENT, MIDI LEARN's prompts, a save error
 *   WHERE (the reader asks)        all of it, the picture's caption and the header's right side too
 * The screen's own words; how to say them is the words file (firmware/words.tsv: "FDBK" feedback, "T#" track #),
 * on the site, not in flash. Main loop only: never the audio ISR. */

static char rd_buf[ANN_MAX + 1];                /* the text being built */
static struct {
    uint32_t ctx, row, started, play, rec, learn, err, said;
    char val[4][24];                            /* each knob's value as last said */
} rd;

static char *rd_s(char *e, const char *s)       /* s after e, within rd_buf */
{
    while (*s && e < rd_buf + ANN_MAX) *e++ = *s++;
    *e = 0;
    return e;
}
static char *rd_u(char *e, uint32_t v) { char t[12]; put_u(t, v); return rd_s(e, t); }
static uint32_t rd_hash(const char *s, uint32_t h) { while (*s) h = (h ^ (uint8_t)*s++) * 16777619u; return h; }

static int rd_own_text(uint32_t pg)             /* knob_text says this page's values (with their units) */
{
    switch (pg) {
    case PG_SOUND: case PG_SOURCE: case PG_ENV: case PG_SCALE: case PG_CHORD: case PG_SONG: case PG_LFO:
    case PG_DRONE: case PG_ARP: case PG_HOME: case PG_MIX1: case PG_MIX2: case PG_TAPE: case PG_LOOP: case PG_AIR:
    case PG_MASTER: case PG_DRIVE: case PG_GRAIN: case PG_DELAY: case PG_SPACE: return 1;
    default: return 0;
    }
}
static const char *rd_name(uint32_t k, const slab_t *S)   /* knob k's name: the whole one (the slab's is cut short) */
{
    const char *n = knob_name(page, k);
    if (page == PG_SOUND && sel_eng() == ENG_PHYS && (k == 1u || k == 3u)) return S->lab;   /* (PHYS: by MODEL) */
    if (PAGES[page].btn == BTN_SEQ && sel_held) return S->lab;   /* (the step's locks) */
    return n && n[0] ? n : S->lab;
}
static void rd_value(uint32_t k, const slab_t *S, char *v)   /* knob k's value as the screen means it */
{
    const char *s = S->val;
    v[0] = 0;
    if (rd_own_text(page) && !(PAGES[page].btn == BTN_SEQ && sel_held)) {
        knob_text(page, k, v);
        if (v[0] && k == 2u && (page == PG_LOOP || (page == PG_SOUND && sel_eng() == ENG_HOWL)) &&
            v[ui_len(v) - 1] >= '0' && v[ui_len(v) - 1] <= '9')
            put_s(v + ui_len(v), " HZ");        /* (LOWCUT, Holvut's DAMP: hertz, "1.9K" kilohertz) */
        if (v[0]) return;
    }
    while (s[0] == '0' && s[1] >= '0' && s[1] <= '9') s++;   /* ("045": 45) */
    put_s(v, s);
}
static char *rd_knob(char *e, uint32_t k)       /* "FEEDBACK 74%"; nothing for a knob with no job here */
{
    slab_t S;
    char v[24];
    ui_slab_make(k, &S);
    if ((S.lab[0] == '-' && !S.lab[1]) || (!knob_param(page, k) && page != PG_PERFORM)) return e;
    rd_value(k, &S, v);
    e = rd_s(e, rd_name(k, &S));
    if (v[0]) e = rd_s(rd_s(e, " "), v);
    return e;
}
static char *rd_page(char *e)                   /* "LOOP, T1 Holvut (2/3)": the page, the track and its source, its
                                                 * place in its button's round */
{
    const page_t *P = &PAGES[page];
    uint32_t p, n = 0, i = 0;
    e = rd_s(e, page == PG_PATTERN ? "PATTERN SETTINGS" : P->name);
    if (P->own || page == PG_HOME) {
        char t[24];
        e = rd_u(rd_s(e, ", T"), sel + 1u);
        e = rd_s(e, " ");
        if (page != PG_HOME || trk_sname(sel, t) == t) put_s(t, ENGINES[sel_eng()]->name);
        e = rd_s(e, t);
    }
    if (page == PG_FMOP || page == PG_FMALGO) e = rd_u(rd_s(e, ", OP "), (uint32_t)clamp(fm_op, 0, 5) + 1u);
    for (p = 0; p < NPAGE; p++)
        if (PAGES[p].btn == P->btn && pg_on(p)) {
            n++;
            if (pg_on(page) && pg_key(p) <= pg_key(page)) i++;
        }
    if (i && n > 1u) e = rd_s(rd_u(rd_s(rd_u(rd_s(e, " ("), i), "/"), n), ")");   /* ("(2/3)": page 2 of 3) */
    return e;
}
static char *rd_row(char *e)                    /* a list's row, or the step SEQ's cursor is on: "SLOT 5 DRONE 01
                                                 * Holvut, 5 OF 64"; "" for a page without */
{
    if (page == PG_SOUNDS) {
        int32_t n = 0, at = 0, i;
        uint32_t s = (uint32_t)clamp(sv_slot, 0, SND_N - 1);
        const uint8_t *r = snd_rec(s);
        char t[24];
        for (i = 0; i < (int32_t)SND_N; i++)
            if (sv_shown((uint32_t)i)) { n++; if (i == (int32_t)s) at = n; }
        e = rd_u(rd_s(e, "SLOT "), s + 1u);
        if (r) {
            snd_name(s, t);
            e = rd_s(rd_s(rd_s(rd_s(e, " "), t), ", "), ENGINES[r[3]]->name);
        } else
            e = rd_s(e, " EMPTY");
        if (at) e = rd_u(rd_s(rd_u(rd_s(e, ", "), (uint32_t)at), " OF "), (uint32_t)n);
        return e;
    }
    if (page == PG_PROJECTS) {
        char t[2] = {(char)('A' + (pj_slot & 3)), 0};
        e = rd_s(rd_s(e, "SLOT "), t);
        e = rd_s(e, seq_recall == (pj_slot & 3) ? " NEXT BAR" : slot_used((uint32_t)pj_slot & 3u) ? " USED" : " EMPTY");
        if ((pj_slot & 3) == slot_cur) e = rd_s(e, ", IN USE");
        e = rd_u(rd_s(e, ", "), (uint32_t)(pj_slot & 3) + 1u);
        return rd_s(e, " OF 4");
    }
    if (page == PG_GRID)
        return rd_u(rd_s(rd_u(rd_s(e, "STEP "), (uint32_t)seq[sel].cursor + 1u), " OF "), (uint32_t)seq_steps(&seq[sel]));
    return e;
}
static char *rd_knobs(char *e)                  /* the four knobs: "GATE OFF, BLOOM 45%, ..." */
{
    uint32_t k, first = 1;
    for (k = (page == PG_SOUNDS || page == PG_PROJECTS) ? 1u : 0u; k < 4u; k++) {   /* (a list's SLOT: its row) */
        char *b = e, *c;
        if (!first) e = rd_s(e, ", ");
        c = e;
        e = rd_knob(e, k);
        if (e == c) e = b, *e = 0;              /* (none: no comma either) */
        else first = 0;
    }
    return e;
}
static void rd_where(void)                      /* everything on the screen */
{
    char *e = rd_page(rd_buf), *b;
    if (ui_cap[0][0] || ui_cap[1][0]) {
        e = rd_s(rd_s(e, ". "), ui_cap[0]);
        if (ui_cap[1][0]) e = rd_s(rd_s(e, ui_cap[0][0] ? ", " : ""), ui_cap[1]);
    }
    b = e = rd_s(e, ". ");
    e = rd_row(e);
    if (e != b) e = rd_s(e, ". ");
    if (page == PG_HOME) { char t[16]; tempo_text(t, ""); e = rd_s(rd_s(rd_s(e, "TEMPO "), t), ". "); }
    rd_knobs(e);
    ann_say(rd_buf, 0, ANN_NOW);
}

static void speak(void)
{
    uint32_t k, ctx, row, touched = fm1_ms - t_hand < 600u;
    char v[24];
    slab_t S;
    int where = ann_where();
    if (!ann_on() || fm1_ms - ui_boot_t < UI_BOOT_MS) { rd.started = 0; return; }   /* (not on the boot screen) */
    ctx = rd_hash(PAGES[page].name, page * 131u + sel * 7u + (PAGES[page].own ? sel_eng() * 17u : 0u) +
                  (uint32_t)fm_op * 3u + sel_held);
    for (k = 0; k < 4u; k++) { ui_slab_make(k, &S); ctx = rd_hash(S.lab, ctx); }
    if (!rd.started || where) {           /* a new reader, or WHERE: all of it */
        rd.started = 1, rd.ctx = ctx, rd.play = playing, rd.rec = rec_on;
        rd_where();
        goto keep;
    }
    if (ctx != rd.ctx) {                        /* a new page, track, source: its name, its row, its knobs */
        char *e = rd_s(rd_page(rd_buf), ". "), *b = e;
        rd.ctx = ctx;
        e = rd_row(e);
        if (e != b) e = rd_s(e, ". ");
        rd_knobs(e);
        ann_later(rd_buf, ANN_NOW, 200u, fm1_ms);
        goto keep;
    }
    row = 0;                                    /* a list's row, SEQ's cursor: moved */
    rd_row(rd_buf);
    row = rd_hash(rd_buf, 1u);
    if (rd_buf[0] && row != rd.row && touched && page != PG_GRID) ann_later(rd_buf, ANN_NOW, 120u, fm1_ms);
    if (rd_buf[0] && row != rd.row && touched && page == PG_GRID) {   /* (the step and its knobs) */
        char *e = rd_s(rd_row(rd_buf), ". ");
        rd_knobs(e);
        ann_later(rd_buf, ANN_NOW, 120u, fm1_ms);
    }
    rd.row = row;
    for (k = 0; k < 4u; k++) {                  /* a knob turned: its name and value */
        ui_slab_make(k, &S);
        rd_value(k, &S, v);
        if (!same(v, rd.val[k]) && touched && !((page == PG_SOUNDS || page == PG_PROJECTS) && k == 0u)) {
            rd_buf[0] = 0;
            rd_knob(rd_buf, k);
            if (rd_buf[0]) ann_later(rd_buf, ANN_NOW, 120u, fm1_ms);
        }
    }
    if (playing != rd.play) ann_say(playing ? "PLAY" : "STOP", 0, ANN_NOW);   /* the transport */
    if (rec_on != rd.rec) ann_say(rec_on ? "RECORD ON" : "RECORD OFF", 0, ANN_NOW);
    rd.play = playing, rd.rec = rec_on;
    if (sv_wait && sv_said != rd.said) ann_say("SAVES WHEN SILENT", 0, 0);   /* (the header's own) */
    if (learn_st != rd.learn && learn_st) ann_say(learn_st == 1u ? "TURN A KNOB" : "MOVE A CC", 0, ANN_NOW);
    if (as_err && !rd.err) ann_say("SAVE ERROR: RETRYING", 0, ANN_NOW | ANN_KEEP);
keep:
    rd.said = sv_wait ? sv_said : rd.said, rd.learn = learn_st, rd.err = as_err;
    rd_buf[0] = 0, rd_row(rd_buf), rd.row = rd_hash(rd_buf, 1u);
    for (k = 0; k < 4u; k++) { ui_slab_make(k, &S); rd_value(k, &S, rd.val[k]); }
}

#ifdef DINGHY_HOST
/* host mode: GRIMM_LISTEN=1 a reader listening from the start (a HELLO every 2 s); GRIMM_SPEAKDUMP=1 every page, each
 * source's, its knobs through their range, the lists' rows: "KIND<tab>text" for fm1-reader's tools/audit.mjs
 * (tools/reader_audit.sh), then the program ends */
static void rd_dump(void)
{
    uint32_t p, e, k, s0 = sel, pg0 = page;
    int32_t v, v0, mx, st;
    for (p = 0; p < NPAGE; p++) {
        if (!PAGES[p].built || p == PG_TRACK || p == PG_PERFORM) continue;
        for (e = 0; e < NENG; e++) {
            int32_t e0 = trk[sel].eng;
            trk[sel].eng = (int32_t)e;
            page = p;
            if ((p == PG_LOOP && e != ENG_HOWL) || ((p == PG_FMALGO || p == PG_FMOP) && e != ENG_FM) ||
                (p != PG_SOUND && p != PG_LOOP && p != PG_FMALGO && p != PG_FMOP && e)) { trk[sel].eng = e0; continue; }
            rd_s(rd_s(rd_page(rd_buf), ". "), "");
            rd_knobs(rd_buf + ui_len(rd_buf));
            printf("SCREEN\t%s\n", rd_buf);
            rd_buf[0] = 0, rd_row(rd_buf);
            if (rd_buf[0]) printf("ROW\t%s\n", rd_buf);
            for (k = 0; k < 4u; k++) {
                volatile int32_t *q = knob_param(p, k);
                if (!q) continue;
                v0 = *q, mx = knob_max(p, k), st = mx > 32 ? mx / 32 : 1;
                for (v = 0; v <= mx; v += st) {
                    *q = v;
                    rd_buf[0] = 0, rd_knob(rd_buf, k);
                    if (rd_buf[0]) printf("KNOB\t%s\n", rd_buf);
                }
                *q = v0;
            }
            trk[sel].eng = e0;
        }
    }
    page = PG_HOME;
    rd_where();
    page = pg0, sel = s0;
    exit(0);
}
static void rd_host(void)
{
    static uint32_t hello_t, init, listen, dump;
    static const uint8_t HELLO[5] = {0x7D, 0x53, 0x52, 1, 1};
    if (!init) init = 1, listen = getenv("GRIMM_LISTEN") != 0, dump = getenv("GRIMM_SPEAKDUMP") != 0;
    if (dump && fm1_ms > 6500u) { ann.on = 1; rd_dump(); }
    if (listen && (!hello_t || fm1_ms - hello_t >= 2000u)) hello_t = fm1_ms | 1u, ann_frame(HELLO, 5, fm1_ms);
}
#endif
