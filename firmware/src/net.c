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
 * MIDI out goes to the radio too: what USB-MIDI sends, and (THRU) the notes that come in over USB or the jack, so a
 * computer playing the FM-1 plays a phone's app through it as well
 * The radio starts when one of them is first turned on (or at power-on, kept so), and its first second is silent
 * (platform.c radio_quiet). A build without the radio (the host, the browser) shows the page and does nothing. */

#ifndef ARCTIC_RADIO
#define ARCTIC_RADIO 0                          /* (the host, the browser: no radio) */
#endif
/* (NW_*, NW_NAME, nw_wifi, nw_link, nw_bt: arctic.c, by SYSTEM's knobs) */
static int32_t nw_wifi_was = -1, nw_link_was = -1, nw_bt_was = -1;
#ifndef NET_MIDI
#define NET_MIDI 1                              /* (MIDI over Wi-Fi: nm_*; a build may leave it out) */
#endif
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
    if ((w || bt) && !radio_wanted) {
        if (fm1_ms < 3000u) return;             /* (at power-on: from the main loop, 3 s in, never inside the start:
                                                 * its calibration there reset the FM-1, twice: the USB rescue) */
        radio_on();
    }
    if (w != nw_wifi_was) {
        nw_wifi_was = w;
        if (radio_wanted) {
            plat_link_enable(0);                /* (Link opens its port and joins its group again on the new mode:
                                                 * FM-1 Again saw no peers in station mode when it came before) */
            plat_wifi_mode(w == NW_JOIN ? WIFI_MODE_AUTO : w == NW_HOTSPOT ? WIFI_MODE_AP : WIFI_MODE_OFF);
            nw_link_was = -1;
        }
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
    nw_ble_out = (uint8_t)bt;                   /* (the radio starts its Bluetooth anyway: nothing goes out with it OFF) */
    if (!bt && radio_wanted && plat_ble_connected() && (fm1_ms & 1023u) == 0u)
        plat_ble_enable(0);                     /* (OFF given while it was starting: again, till it takes) */
}
/* ---- (2.1.9) MIDI over Wi-Fi (RTP-MIDI, Apple's "Network Session"), from FM-1 Again (app/net.h, GPL-3.0-or-later,
 * PolarPatch). With an address (JOIN's network, or HOTSPOT's own), the FM-1 looks for a session on the network (an
 * mDNS question for _apple-midi._udp, sent from its own port so the answer comes back to it: no group joined, Link
 * keeps its own) and invites the first it finds: a phone's (iOS: an app's Network Session), a computer's (macOS: Audio
 * MIDI Setup's network session). A computer may invite it too (its address, port 5004). Then what goes out on USB-MIDI
 * goes there too (radio_arctic.c nw_out_tap), and what comes from it plays as MIDI in does. No journal. */
#define NM_CTL 5004u                            /* the session's two ports: control, then data */
#define NM_DATA 5005u
#define NM_MDNS 0xE00000FBu                     /* 224.0.0.251:5353 */
enum { NM_OFF, NM_SEARCH, NM_INV_CTL, NM_INV_DATA, NM_UP };
static struct {
    uint8_t st, tries, open;
    uint32_t ip, addr;                          /* the peer's address; ours when the ports were opened */
    uint16_t pctl, pdata, seq;                  /* its ports; our RTP sequence */
    uint32_t token, ssrc, at, heard, ck_at;     /* (ms: the last thing sent, heard from it, the last sync) */
    uint16_t qid;
} nm;
static const char NM_NAME[] = "Arctic Scream";
#define NM_RING 64u
static uint32_t nm_i_buf[NM_RING], nm_i_w, nm_i_r;   /* in (from wifi_poll), to nw_midi_in: both the main loop */
static uint8_t nm_buf[512], nm_tmp[256], nm_inst[256];   /* (main loop only) */

static uint8_t *nm_u16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; return p + 2; }
static uint8_t *nm_u32(uint8_t *p, uint32_t v) { p = nm_u16(p, v >> 16); return nm_u16(p, v & 0xFFFFu); }
static uint32_t nm_g16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t nm_g32(const uint8_t *p) { return nm_g16(p) << 16 | nm_g16(p + 2); }
static uint64_t nm_now(void)                    /* the session's clock: 100 us steps (shifts: no 64-bit multiply) */
{
    uint64_t t = fm1_ms;
    return (t << 3) + (t << 1);
}

static void nm_word(const char *w, uint16_t ours, uint16_t port, uint32_t token)   /* IN, OK, BY */
{
    uint8_t b[16 + sizeof NM_NAME], *p = b;
    p = nm_u16(p, 0xFFFFu);
    *p++ = (uint8_t)w[0];
    *p++ = (uint8_t)w[1];
    p = nm_u32(p, 2u);
    p = nm_u32(p, token);
    p = nm_u32(p, nm.ssrc);
    if (w[0] != 'B') {
        memcpy(p, NM_NAME, sizeof NM_NAME);
        p += sizeof NM_NAME;
    }
    plat_net_udp_send(ours, nm.ip, port, b, (uint32_t)(p - b));
}
static void nm_ck(uint32_t count, const uint8_t *in)   /* a clock sync: count 0 starts it, 1 and 2 answer */
{
    uint8_t b[36], *p = b;
    uint64_t t = nm_now();
    int i;
    p = nm_u16(p, 0xFFFFu);
    *p++ = 'C';
    *p++ = 'K';
    p = nm_u32(p, nm.ssrc);
    *p++ = (uint8_t)count;
    *p++ = 0, *p++ = 0, *p++ = 0;
    if (in) memcpy(p, in, 24);
    else memset(p, 0, 24);
    for (i = 0; i < 8; i++) p[8u * count + (uint32_t)i] = (uint8_t)(t >> (56 - 8 * i));
    plat_net_udp_send(NM_DATA, nm.ip, nm.pdata, b, sizeof b);
}
static void nm_lost(void)
{
    nm.st = nm.open ? NM_SEARCH : NM_OFF;
    nm.at = 0;
    nm_live = 0;
    no_r = no_w;
}
/* a name in an mDNS message, read through its pointers into o as it goes on the wire; its length, 0 bad */
static uint32_t nm_name(const uint8_t *m, uint32_t n, uint32_t at, uint8_t *o, uint32_t room, uint32_t *after)
{
    uint32_t len = 0, hops = 0;
    int jumped = 0;
    while (at < n) {
        uint32_t l = m[at];
        if ((l & 0xC0u) == 0xC0u) {
            if (at + 1 >= n || ++hops > 8) return 0;
            if (!jumped && after) *after = at + 2;
            jumped = 1;
            at = (l & 0x3Fu) << 8 | m[at + 1];
            continue;
        }
        if (l > 63u || at + 1 + l > n || len + 1 + l + 1 > room) return 0;
        memcpy(o + len, m + at, 1 + l);
        len += 1 + l;
        at += 1 + l;
        if (!l) {
            if (!jumped && after) *after = at;
            return len;
        }
    }
    return 0;
}
static void nm_ask(const uint8_t *name, uint32_t nlen, uint32_t type)   /* an mDNS question, answered to our port */
{
    uint8_t b[300], *p = b;
    if (nlen + 16 > sizeof b) return;
    p = nm_u16(p, nm.qid);
    p = nm_u16(p, 0);
    p = nm_u16(p, 1);
    p = nm_u32(p, 0);
    p = nm_u16(p, 0);
    memcpy(p, name, nlen);
    p += nlen;
    p = nm_u16(p, type);
    p = nm_u16(p, 0x8001u);                     /* IN, an answer to us alone */
    plat_net_udp_send(NM_CTL, NM_MDNS, 5353, b, (uint32_t)(p - b));
}
static void nm_mdns(uint32_t src, const uint8_t *m, uint32_t n)   /* an answer: the session's port (SRV), or ask it */
{
    uint32_t i, at = 12, cnt, ilen = 0;
    if (n < 12 || nm_g16(m) != nm.qid || !(m[2] & 0x80u) || nm.st != NM_SEARCH) return;
    for (i = nm_g16(m + 4); i; i--) {           /* the questions again */
        if (!nm_name(m, n, at, nm_tmp, sizeof nm_tmp, &at)) return;
        at += 4;
    }
    cnt = nm_g16(m + 6) + nm_g16(m + 8) + nm_g16(m + 10);
    for (i = 0; i < cnt && at < n; i++) {
        uint32_t type, rl;
        if (!nm_name(m, n, at, nm_tmp, sizeof nm_tmp, &at) || at + 10 > n) return;
        type = nm_g16(m + at);
        rl = nm_g16(m + at + 8);
        at += 10;
        if (at + rl > n) return;
        if (type == 33u && rl >= 6u) {          /* SRV: the control port; the address is the one that answered */
            nm.ip = src;
            nm.pctl = (uint16_t)nm_g16(m + at + 4);
            nm.pdata = (uint16_t)(nm.pctl + 1u);
            nm.st = NM_INV_CTL, nm.tries = 0, nm.at = 0;
            return;
        }
        if (type == 12u && !ilen)               /* PTR: the session's own name, to ask its SRV */
            ilen = nm_name(m, n, at, nm_inst, sizeof nm_inst, 0);
        at += rl;
    }
    if (ilen) nm_ask(nm_inst, ilen, 33u);
}
static void nm_midi_in(const uint8_t *b, uint32_t n)   /* the MIDI in a data packet, queued for nw_midi_in */
{
    uint32_t at, len, end, st = 0;
    int first = 1, z;
    if (n < 13 || (b[0] & 0xC0u) != 0x80u) return;
    at = 12u + 4u * (b[0] & 15u);
    if (at >= n) return;
    z = b[at] & 0x20;
    if (b[at] & 0x80u) {
        if (at + 1 >= n) return;
        len = (b[at] & 15u) << 8 | b[at + 1];
        at += 2;
    } else
        len = b[at++] & 15u;
    end = at + len < n ? at + len : n;
    while (at < end) {
        uint32_t d, c, k;
        if (!first || z)                        /* a delta time: up to four bytes */
            for (k = 0; k < 4u && at < end; k++)
                if (!(b[at++] & 0x80u)) break;
        first = 0;
        if (at >= end) break;
        if (b[at] & 0x80u) st = b[at++];
        if (st == 0xF0u || st == 0xF7u) {       /* SysEx: past it */
            while (at < end && b[at] != 0xF7u && b[at] != 0xF0u && b[at] != 0xF4u) at++;
            at++;
            st = 0;
            continue;
        }
        if (!st) break;
        c = st >= 0xF8u || st == 0xF6u ? 0u : st == 0xF2u || st < 0xC0u || (st & 0xF0u) == 0xE0u ? 2u : 1u;
        if (at + c > end) break;
        d = st << 8;
        if (c > 0u) d |= (uint32_t)b[at] << 16;
        if (c > 1u) d |= (uint32_t)b[at + 1] << 24;
        at += c;
        if (st >= 0xF0u && st < 0xF8u) st = 0;
        if (nm_i_w - nm_i_r < NM_RING) nm_i_buf[nm_i_w++ % NM_RING] = d;
    }
}
static void nm_rx(void *ctx, uint32_t src, uint16_t sport, const uint8_t *p, uint32_t n, int64_t at_us)
{
    uint32_t data = (uint32_t)(uintptr_t)ctx;
    (void)at_us;
    if (!data && sport == 5353u) {
        nm_mdns(src, p, n);
        return;
    }
    if (n >= 4 && p[0] == 0xFF && p[1] == 0xFF) {   /* the session's words */
        if (p[2] == 'I' && p[3] == 'N' && n >= 16) {   /* invited: yes (one session: a new one takes its place) */
            if (!data || src != nm.ip) {
                nm.ip = src, nm.pctl = sport, nm.pdata = (uint16_t)(sport + 1u);
                nm.st = NM_INV_DATA, nm.tries = 0, nm.at = fm1_ms, nm.heard = fm1_ms;
                nm_live = 0;
            } else {
                nm.pdata = sport, nm.st = NM_UP, nm.ck_at = 0, nm.heard = fm1_ms;
                nm_live = 1;
            }
            nm_word("OK", data ? NM_DATA : NM_CTL, sport, nm_g32(p + 8));
            return;
        }
        if (src != nm.ip) return;
        nm.heard = fm1_ms;
        if (p[2] == 'O' && p[3] == 'K' && n >= 16 && nm_g32(p + 8) == nm.token) {
            if (nm.st == NM_INV_CTL)
                nm.st = NM_INV_DATA, nm.tries = 0, nm.at = 0;
            else if (nm.st == NM_INV_DATA && data)
                nm.st = NM_UP, nm.ck_at = 0, nm_live = 1;
        } else if ((p[2] == 'N' && p[3] == 'O') || (p[2] == 'B' && p[3] == 'Y'))
            nm_lost();
        else if (p[2] == 'C' && p[3] == 'K' && n >= 36 && data && p[8] < 2u)
            nm_ck(p[8] + 1u, p + 12);
        return;
    }
    if (data && src == nm.ip && nm.st == NM_UP) {
        nm.heard = fm1_ms;
        nm_midi_in(p, n);
    }
}
static void nm_send(void)                       /* what went out since the last pass: one RTP-MIDI packet */
{
    uint8_t *b = nm_buf, *p = b + 14;
    uint32_t len, w = no_w;
    while (no_r != w) {
        uint32_t pk = no_buf[no_r % NO_RING], st = pk >> 8 & 0xFFu, c;
        if (st >= 0x80u && (st < 0xF0u || st >= 0xF8u)) {
            c = st >= 0xF8u ? 0u : (st & 0xF0u) == 0xC0u || (st & 0xF0u) == 0xD0u ? 1u : 2u;
            if (p + 2 + c > b + sizeof nm_buf) break;
            if (p != b + 14) *p++ = 0;          /* (a delta time of none) */
            *p++ = (uint8_t)st;
            if (c > 0u) *p++ = (uint8_t)(pk >> 16 & 0x7Fu);
            if (c > 1u) *p++ = (uint8_t)(pk >> 24 & 0x7Fu);
        }
        no_r++;
    }
    len = (uint32_t)(p - (b + 14));
    if (!len) return;
    b[0] = 0x80, b[1] = 0x61;
    nm_u16(b + 2, nm.seq++);
    nm_u32(b + 4, (uint32_t)nm_now());
    nm_u32(b + 8, nm.ssrc);
    b[12] = (uint8_t)(0x80u | len >> 8);        /* the long form always: B set, J, Z, P clear */
    b[13] = (uint8_t)len;
    plat_net_udp_send(NM_DATA, nm.ip, nm.pdata, b, 14u + len);
}
static void nm_tick(void)                       /* main loop (nw_tick) */
{
    uint32_t now = fm1_ms, addr = radio_wanted && nw_wifi ? plat_wifi_addr() : 0u;
    if (!nm.ssrc) nm.ssrc = 0x41530000u ^ now * 2654435761u;
    if (nm.open && addr != nm.addr) {           /* the network gone or changed: the ports go too */
        if (nm.st == NM_UP) nm_word("BY", NM_CTL, nm.pctl, nm.token);
        plat_net_udp_close(NM_CTL);
        plat_net_udp_close(NM_DATA);
        nm.open = 0;
        nm_lost();
    }
    if (addr && !nm.open) {
        if (plat_net_udp_open(NM_CTL, nm_rx, (void *)0) == 0 && plat_net_udp_open(NM_DATA, nm_rx, (void *)1) == 0) {
            nm.open = 1, nm.addr = addr, nm.st = NM_SEARCH, nm.at = 0;
        } else {
            plat_net_udp_close(NM_CTL);
            return;
        }
    }
    switch (nm.st) {
    case NM_SEARCH:                             /* ask every 2 s */
        if (!nm.at || now - nm.at >= 2000u) {
            static const uint8_t Q[] = "\013_apple-midi\004_udp\005local";
            nm.at = now;
            nm.qid = (uint16_t)(now | 1u);
            nm_ask(Q, sizeof Q, 12u);
        }
        break;
    case NM_INV_CTL:
    case NM_INV_DATA:                           /* invite, once a second, five times */
        if (!nm.at || now - nm.at >= 1000u) {
            if (nm.tries++ >= 5u) {
                nm_lost();
                break;
            }
            nm.at = now;
            if (nm.tries == 1u && nm.st == NM_INV_CTL) nm.token = now * 2246822519u + 1u;
            if (nm.st == NM_INV_CTL) nm_word("IN", NM_CTL, nm.pctl, nm.token);
            else nm_word("IN", NM_DATA, nm.pdata, nm.token);
        }
        break;
    case NM_UP:
        nm_send();
        if (!nm.ck_at || now - nm.ck_at >= 5000u) {   /* a sync every 5 s: its answers say it is there */
            nm.ck_at = now;
            nm_ck(0, 0);
        }
        if (now - nm.heard > 30000u) nm_lost(); /* nothing heard for 30 s: gone */
        break;
    default:
        break;
    }
}

static void nw_thru(uint8_t st, uint8_t d1, uint8_t d2)   /* MIDI THRU: a channel message from USB or the jack, on to
                                                 * the radio's outs (never back to USB: no loop) */
{
    if (st >= 0x80u && st < 0xF0u)
        nw_out_tap((uint32_t)(st >> 4) | (uint32_t)st << 8 | (uint32_t)(d1 & 0x7Fu) << 16 | (uint32_t)(d2 & 0x7Fu) << 24);
}
static int nw_midi_in(uint8_t *st, uint8_t *d1, uint8_t *d2)   /* Bluetooth MIDI and Wi-Fi MIDI in (1: a message), as
                                                 * midi_get */
{
    uint32_t pkt, from;
    if (!radio_wanted) return 0;
    if (nm_i_r != nm_i_w)
        pkt = nm_i_buf[nm_i_r++ % NM_RING];
    else if (!nw_bt || !ble_midi_in(&pkt, &from))
        return 0;
    else
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
    p = put_s(d, plat_ble_connected() ? (nw_bt ? "BT MIDI ON" : "BT OFF, STILL LINKED") : nw_bt ? "BT WAITING" : "BT OFF");
    if (nm.open) put_s(p, nm.st == NM_UP ? ", NET MIDI ON" : ", NET MIDI LOOKING");
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
    if (f[3] == 4u && radio_wanted && nw_wifi == NW_JOIN) {           /* (status: the station's frames, Protected or not, a test line) */
        char *q = put_u(put_s(d, "RX PROT "), nw_rx_prot);
        put_u(put_s(q, " PLAIN "), nw_rx_plain);
    }
    out[o++] = 0xF0u;                           /* (sysex_send: the whole frame, F0 .. F7) */
    out[o++] = 0x7Du, out[o++] = 0x4Eu, out[o++] = 0x57u, out[o++] = f[3], out[o++] = (uint8_t)(rc < 0 ? 1 : 0);
    {
        const char *L[4] = {a, b, c, d};
        for (k = 0; k < 4u; k++) {
            for (i = 0; L[k][i] && o < sizeof out - 3u; i++) out[o++] = (uint8_t)(L[k][i] & 0x7F);
            out[o++] = 0;
        }
    }
    out[o++] = 0xF7u;
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
static void nw_thru(uint8_t st, uint8_t d1, uint8_t d2) { (void)st, (void)d1, (void)d2; }
static int nw_status(char *a, char *b, char *c, char *d)
{
#ifdef DINGHY_HOST
    if (getenv("ARCTIC_NET_DEMO")) {            /* (host, for the videos: the page as the FM-1 shows it, made up) */
        char *p;
        *a = *b = *c = *d = 0;
        if (nw_wifi == NW_HOTSPOT) put_s(a, "HOTSPOT FM1-4207"), put_s(b, "PASSWORD polarnight");
        else put_s(a, nw_wifi ? "JOINED ARCTIC" : "WIFI OFF");
        if (nw_link) {
            p = put_s(c, "LINK 1 PEER ");
            put_u(p, (uint32_t)clamp(seq_bpm, 20, 300));
        }
        put_s(d, nw_bt ? "BT MIDI ON, NET MIDI ON" : "BT OFF, NET MIDI ON");
        return 1;
    }
#endif
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
#if ARCTIC_RADIO && NET_MIDI
    nm_tick();
#endif
}
