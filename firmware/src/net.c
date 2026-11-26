/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.9) NETWORK (HOME held, the fifth SYSTEM page): Wi-Fi, Ableton Link and Bluetooth MIDI on the FM-1's own radio
 * (platform.c radio_arctic.c: Optimist's stacks as fm1-base carries them). arctic.c includes this file after clock.c.
 *   WIFI       OFF, JOIN (the networks it was given, the best one heard: tools/arctic_wifi.py join SSID PASSWORD over
 *              USB), HOTSPOT (the FM-1 is the network: its name and password on the page; a phone or a laptop joins
 *              it, no router needed: on stage)
 *   LINK       OFF, ON: CLOCK is LINK (SYSTEM 2's CLOCK shows it too): the tempo, the beat and PLAY / STOP shared with
 *              Ableton Live, Note and every Link app on the network. Its pulses go into the clock as a MIDI clock's
 *              would (clock.c ck_msg), each at its own time: the sequencer and the arps play on the session's beat;
 *              PLAY starts at the session's next bar (alone: the next beat), and STOP from anyone stops all; the
 *              tempo knobs set the session's tempo, and another peer's comes here
 *   BLUETOOTH  OFF, ON: Bluetooth MIDI ("Arctic Scream XXXX"), in as from USB (notes, CCs; not its clock)
 * The radio starts when one of them is first turned on (or at power-on, kept so), and its first second is silent
 * (platform.c radio_quiet). A build without the radio (the host, the browser) shows the page and does nothing. */

#ifndef ARCTIC_RADIO
#define ARCTIC_RADIO 0                          /* (the host, the browser: no radio) */
#endif
/* (NW_*, NW_NAME, nw_wifi, nw_link, nw_bt: arctic.c, by SYSTEM's knobs) */
static int32_t nw_wifi_was = -1, nw_link_was = -1, nw_bt_was = -1;
#define RT_LINK 2u                              /* (platform.c RT_USB, RT_TRS: the realtime source CK_LINK reads) */

#if ARCTIC_RADIO
/* ---- Link's pulses into the clock (main loop): 24 a beat, each at its time on the timer (plat_link_t24), up to
 * LK_AHEAD ahead; the session's start at its next bar (FA just before that pulse), its stop at once (FC) */
#define LK_AHEAD 30000                          /* us */
static struct {
    int64_t b;                                  /* the next pulse's beat (whole, in micro-beats) */
    uint32_t k;                                 /* .. and its 24th in it */
    int64_t fa_ub;                              /* the start's beat (-1: none waiting) */
    uint8_t run, play;                          /* pulses going; the session's start / stop as last seen */
    int32_t mpb;                                /* the session's tempo as last seen (us a beat) */
    int32_t bpm;                                /* seq_bpm as last told to the session */
} lk = {.fa_ub = -1};
static void lk_put(uint32_t st, int64_t us)
{
    uint32_t f = irq_save();
    rt_in_at(st, RT_LINK, plat_link_t24(us));
    irq_restore(f);
}
static int64_t lk_next_bar(int64_t ub)          /* the first bar line (4 beats) at or after ub */
{
    int64_t ph = plat_link_phase(ub, 4000000);
    return ph ? ub - ph + 4000000 : ub;
}
static void lk_tick(void)
{
    int64_t now, b, ub, t;
    int32_t mpb;
    uint32_t n;
    int play;
    if (ck_mode != CK_LINK || !plat_link_on()) {
        lk.run = 0, lk.fa_ub = -1;
        return;
    }
    now = plat_link_now();
    if ((b = plat_link_beat_at(now)) < 0) return;
    mpb = plat_link_mpb();
    if (mpb > 0 && mpb != lk.mpb) {             /* another peer's tempo: ours */
        lk.mpb = mpb;
        seq_bpm = clamp((60000000 + mpb / 2) / mpb, 20, 300), lk.bpm = seq_bpm;
    } else if (seq_bpm != lk.bpm) {             /* ours turned: the session's */
        lk.bpm = seq_bpm;
        plat_link_set_tempo(60000000 / clamp(seq_bpm, 20, 300)), lk.mpb = plat_link_mpb();
    }
    if (!lk.run) {                              /* from the next pulse after now */
        int64_t ph = plat_link_phase(b, 1000000);
        lk.b = b - ph, lk.k = (uint32_t)(ph * 24 / 1000000) + 1u;
        if (lk.k >= 24u) lk.k = 0, lk.b += 1000000;
        lk.run = 1, lk.play = (uint8_t)plat_link_playing();
    }
    play = plat_link_playing();
    if (play && !lk.play) lk.fa_ub = lk_next_bar(b);   /* the session starts: we at its next bar */
    if (!play && lk.play) lk.fa_ub = -1, lk_put(0xFCu, now);   /* .. stops: we too */
    lk.play = (uint8_t)play;
    for (n = 0; n < 8u; n++) {                  /* the pulses due within LK_AHEAD */
        ub = lk.b + (int64_t)((lk.k * 1000000u + 12u) / 24u);
        t = plat_link_time_at(ub);
        if (t > now + LK_AHEAD) break;
        if (t < now - 200000) {                 /* (far behind: the main loop stood; on from now) */
            lk.run = 0;
            return;
        }
        if (lk.fa_ub >= 0 && ub >= lk.fa_ub) lk_put(0xFAu, t), lk.fa_ub = -1;   /* (START, then its downbeat) */
        lk_put(0xF8u, t);
        if (++lk.k >= 24u) lk.k = 0, lk.b += 1000000;
    }
}
static int lk_play(int on)                      /* PLAY / STOP in LINK: the session's (1: taken; 0: no session, as ever) */
{
    if (ck_mode != CK_LINK || !plat_link_on()) return 0;
    plat_link_set_playing(on);
    return 1;
}
static void nw_apply(void)                      /* the knobs to the radio */
{
    int32_t w = clamp(nw_wifi, 0, NNW - 1), l = clamp(nw_link, 0, 1), bt = clamp(nw_bt, 0, 1);
    if ((w || bt) && !radio_wanted) radio_on();
    if (w != nw_wifi_was) {
        nw_wifi_was = w;
        if (radio_wanted) plat_wifi_mode(w == NW_JOIN ? WIFI_MODE_AUTO : w == NW_HOTSPOT ? WIFI_MODE_AP : WIFI_MODE_OFF);
    }
    if (l != nw_link_was) {
        nw_link_was = l;
        if (radio_wanted || !l) plat_link_enable(l);
        lk.bpm = 0;                             /* (alone: our tempo goes to the session) */
    }
    if (bt != nw_bt_was) {
        nw_bt_was = bt;
        if (radio_wanted) plat_ble_enable(bt);
    }
}
static int nw_midi_in(uint8_t *st, uint8_t *d1, uint8_t *d2)   /* Bluetooth MIDI in (1: a message), as midi_get */
{
    uint32_t pkt, from;
    if (!radio_wanted || !nw_bt || !ble_midi_in(&pkt, &from)) return 0;
    nw_ble_in_n++, nw_ble_in_t = fm1_ms;
    *st = (uint8_t)(pkt >> 8), *d1 = (uint8_t)(pkt >> 16) & 0x7Fu, *d2 = (uint8_t)(pkt >> 24) & 0x7Fu;
    return *st >= 0x80u && *st < 0xF0u;         /* (channel messages; the clock is USB's and the jack's) */
}
static int nw_status(char *a, char *b, char *c, char *d)   /* the page's four lines; 1: the radio is in this build */
{
    char *p;
    uint32_t ip;
    int st;
    *a = *b = *c = *d = 0;
    if (!radio_wanted) { put_s(a, "THE RADIO IS OFF"); return 1; }
    if (radio_quiet) { put_s(a, "THE RADIO IS STARTING"); return 1; }
    if (nw_wifi == NW_HOTSPOT) {
        char nm[34], key[PLAT_WIFI_AP_KEY_MAX];
        int n = plat_wifi_ap_name(nm), ap = plat_wifi_ap_state();
        nm[n > 0 && n < 33 ? n : 0] = 0;
        p = put_s(a, ap == WIFI_AP_UP ? "HOTSPOT " : "HOTSPOT STARTING ");
        put_s(p, nm);
        if (plat_wifi_ap_default_key(key)) put_s(put_s(b, "PASSWORD "), key);
        else put_s(b, "PASSWORD AS TYPED");
        put_u(put_s(c, "PHONES JOINED "), (uint32_t)plat_wifi_ap_clients());
    } else if (nw_wifi == NW_JOIN) {
        st = plat_wifi_state();
        if (st == WIFI_CONNECTED && (ip = plat_wifi_ip()) != 0) {
            p = put_s(a, "JOINED "), put_s(p, plat_wifi_ssid());
            p = put_u(b, ip & 255u), *p++ = '.', p = put_u(p, ip >> 8 & 255u), *p++ = '.';
            p = put_u(p, ip >> 16 & 255u), *p++ = '.', put_u(p, ip >> 24);
        } else if (st == WIFI_FAILED) {
            put_s(a, "NOT JOINED"), put_s(b, plat_wifi_why());
        } else {
            char s[34];
            put_s(a, st == WIFI_JOINING || st == WIFI_ADDRESS ? "JOINING " : plat_wifi_saved(0, s) ? "LOOKING FOR NETWORKS" :
                  "NO NETWORK GIVEN YET");
            if (st == WIFI_JOINING || st == WIFI_ADDRESS) put_s(a + 8, plat_wifi_ssid());
            else if (!plat_wifi_saved(0, s)) put_s(b, "USB: ARCTIC_WIFI.PY JOIN");
        }
    } else
        put_s(a, "WIFI OFF");
    if (nw_link && plat_link_on()) {
        p = put_u(put_s(c, "LINK "), plat_link_peers());
        p = put_s(p, plat_link_peers() == 1u ? " PEER " : " PEERS ");
        put_u(p, (uint32_t)clamp(seq_bpm, 20, 300));
    } else if (nw_link)
        put_s(c, "LINK: WAITING FOR A NETWORK");
    put_s(d, !nw_bt ? "BLUETOOTH OFF" : plat_ble_connected() ? "BLUETOOTH MIDI CONNECTED" : "BLUETOOTH MIDI WAITING");
    return 1;
}
/* SysEx F0 7D 4E 57 cmd ..: the network from a computer (tools/arctic_wifi.py). 1 join: SSID 0 PASSWORD 0; 2 forget
 * all; 3 hotspot password: PASSWORD 0 (0 length: a new one made); every answer F0 7D 4E 57 cmd rc, then the status
 * lines (0 between) */
static int nw_frame(const uint8_t *f, uint32_t n)
{
    static uint8_t out[160];
    char a[48], b[48], c[48], d[48], ssid[33], pass[64];
    uint32_t i, k, o = 0;
    int rc = -1;
    if (n < 4u || f[0] != 0x7Du || f[1] != 0x4Eu || f[2] != 0x57u) return 0;
    if (f[3] == 1u) {
        for (i = 4u, k = 0; i < n && f[i] && k < 32u; i++) ssid[k++] = (char)f[i];
        ssid[k] = 0;
        for (i++, k = 0; i < n && f[i] && k < 63u; i++) pass[k++] = (char)f[i];
        pass[k] = 0;
        if (ssid[0]) {
            if (!radio_wanted) radio_on();
            if (nw_wifi != NW_JOIN) nw_wifi = NW_JOIN, nw_apply();
            rc = plat_wifi_join(ssid, pass[0] ? pass : 0);
        }
        for (k = 0; k < sizeof pass; k++) pass[k] = 0;
    } else if (f[3] == 2u && radio_wanted) {
        char s[34];
        while (plat_wifi_saved(0, s)) plat_wifi_forget(0);
        rc = 0;
    } else if (f[3] == 3u && radio_wanted) {
        for (i = 4u, k = 0; i < n && f[i] && k < 63u; i++) pass[k++] = (char)f[i];
        pass[k] = 0;
        if (k) rc = plat_wifi_ap_set(0, 0, pass);
        else plat_wifi_ap_forget(), rc = 0;
        for (k = 0; k < sizeof pass; k++) pass[k] = 0;
    } else if (f[3] == 4u)
        rc = 0;                                 /* (the status only) */
    nw_status(a, b, c, d);
    if (f[3] == 4u && radio_wanted) {           /* (status: the station's frames, Protected or not, a test line) */
        char *q = put_u(put_s(d, "RX PROT "), nw_rx_prot);
        put_u(put_s(q, " PLAIN "), nw_rx_plain);
    }
    out[o++] = 0x7Du, out[o++] = 0x4Eu, out[o++] = 0x57u, out[o++] = f[3], out[o++] = (uint8_t)(rc < 0 ? 1 : 0);
    {
        const char *L[4] = {a, b, c, d};
        for (k = 0; k < 4u; k++) {
            for (i = 0; L[k][i] && o < sizeof out - 2u; i++) out[o++] = (uint8_t)(L[k][i] & 0x7F);
            out[o++] = 0;
        }
    }
    sysex_send(out, o);
    return 1;
}
static void nw_boot(void)                       /* power-on: the radio again if it was left on */
{
    own_store_load();
    nw_wifi = wn_st.mode == WN_MODE_AUTO ? NW_JOIN : wn_st.mode == WN_MODE_AP ? NW_HOTSPOT : NW_OFF;
    nw_bt = (set.sys4 >> 5) & 1u, nw_link = ck_mode == CK_LINK;
    nw_apply();
}
#else
static void lk_tick(void) {}
static int lk_play(int on) { (void)on; return 0; }
static void nw_apply(void) {}
static void nw_boot(void) { nw_bt = (set.sys4 >> 5) & 1u, nw_link = ck_mode == CK_LINK; }
static int nw_midi_in(uint8_t *st, uint8_t *d1, uint8_t *d2) { (void)st, (void)d1, (void)d2; return 0; }
static int nw_status(char *a, char *b, char *c, char *d)
{
    put_s(a, "NO RADIO IN THIS BUILD"), *b = *c = *d = 0;
    return 0;
}
static int nw_frame(const uint8_t *f, uint32_t n) { (void)f, (void)n; return 0; }
#endif
static void nw_tick(void)                       /* main loop, each pass */
{
    if (nw_link != nw_link_was) {               /* LINK: CLOCK LINK, or back to INT */
        if (nw_link) sy_clk = CK_LINK;
        else if (sy_clk == CK_LINK) sy_clk = CK_INT;
    } else if ((sy_clk == CK_LINK) != (nw_link != 0))   /* (CLOCK turned on SYSTEM 2: LINK follows) */
        nw_link = sy_clk == CK_LINK;
    nw_apply();
#if !ARCTIC_RADIO
    nw_wifi_was = nw_wifi, nw_link_was = nw_link, nw_bt_was = nw_bt;
#endif
    lk_tick();
}
