/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 PolarPatch */
/* PANEL OUT: what the hands do on the FM-1's panel, for an app that draws it (a visualiser, a tutorial, a video):
 * which buttons and keys are down, the octave, each knob as it turns. Free for any FM-1 firmware to copy (MIT, this
 * file alone); the protocol is docs/PANEL_OUT.md (CC0). Written for Arctic Scream 1.5.2, asked for by Polar Signal's
 * app; its manner (silent until asked, a listener not heard for 5 s gone, never waiting on USB) is that of the screen
 * reader's announce.c (fm1-base, Charles Vestal, MPL-2.0), none of its code.
 *
 * SysEx over USB-MIDI, F0 7D 50 4F cmd args.. F7 ("PO"):
 *   app -> device      1 HELLO ver      listen (again every ~2 s: an app not heard for 5 s is gone)
 *                      2 BYE            stop
 *                      3 STATE          send STATE now
 *   device -> app      1 HELLO ver, key 0's MIDI note, octave + 64, keys, buttons, the firmware's name (ASCII to F7)
 *                      16 STATE buttons (2 x 7 bits), keys (4 x 7 bits), octave + 64   on every change, and after HELLO
 *                      17 KNOB id, 0, steps + 64 (1 .. 127: turned, its detents since the last)
 *                           or id, 1, value (0 .. 127: where it is, MASTER)
 *   knob ids: 0 .. 3 KNOB 1 .. 4, 4 SELECT, 5 ALGORITHM, 6 PRESETS, 7 MASTER
 * Nothing is built or sent while no app listens: a DAW on the same port never sees a byte. A message the USB queue has
 * no room for waits for the next tick (a knob's steps add up meanwhile); a knob is sent at most every 20 ms.
 *
 * The includer supplies, before the #include: po_wire_send(p, n) (one F0..F7 message; 0 = sent, else not sent at all),
 * PO_NAME (the HELLO answer); it calls po_frame() with each received SysEx (the bytes between F0 and F7), po_knob() as
 * a knob turns, and po_tick() from its main loop with the panel as it is. */
#include <stdint.h>
#ifndef PO_NAME
#define PO_NAME "FM-1"
#endif
#define PO_VER 1u
#define PO_H0 0x7Du
#define PO_H1 0x50u
#define PO_H2 0x4Fu
#define PO_GONE_MS 5000u
#define PO_KNOB_MS 20u
#define PO_KNOBS 8u
enum { PO_HELLO = 1, PO_BYE = 2, PO_STATE = 3, PO_SAY_STATE = 16, PO_KNOB = 17 };
enum { PO_REL = 0, PO_ABS = 1 };

static int po_wire_send(const uint8_t *p, uint32_t n);

static struct {
    uint8_t on, hello, state;                   /* listening; a HELLO to answer; STATE to send */
    uint32_t heard, sent_b, sent_k, sent_o;     /* the last HELLO; the panel as last sent */
    int32_t steps[PO_KNOBS], abs_v[PO_KNOBS];   /* a knob's detents not yet sent; MASTER's place (-1 sent) */
    uint32_t knob_t[PO_KNOBS];
} po;

static int po_on(void) { return po.on; }

static int po_send(const uint8_t *a, uint32_t n)   /* F0 7D 50 4F a.. F7 */
{
    uint8_t f[48];
    uint32_t i;
    if (n + 6u > sizeof f) return -1;
    f[0] = 0xF0, f[1] = PO_H0, f[2] = PO_H1, f[3] = PO_H2;
    for (i = 0; i < n; i++) f[4u + i] = a[i] & 0x7Fu;
    f[4u + n] = 0xF7;
    return po_wire_send(f, n + 5u);
}

/* a knob turned d detents (id: 0 .. 6) */
static void po_knob(uint32_t id, int32_t d)
{
    if (po.on && id < PO_KNOBS) po.steps[id] += d;
}

/* each main loop pass: the panel now (buttons: 14 bits; keys: 27 bits; oct: -3 .. 3; key0: key 0's MIDI note now;
 * master: 0 .. 127) */
static void po_tick(uint32_t now, uint32_t buttons, uint32_t keys, int32_t oct, uint32_t key0, int32_t master)
{
    uint8_t a[40];
    uint32_t k, n;
    if (po.on && now - po.heard > PO_GONE_MS) po.on = 0;
    if (!po.on) return;
    if (po.hello) {
        const char *s = PO_NAME;
        a[0] = PO_HELLO, a[1] = PO_VER, a[2] = (uint8_t)key0, a[3] = (uint8_t)(oct + 64), a[4] = 27, a[5] = 14;
        for (n = 6; *s && n < sizeof a; s++) a[n++] = (uint8_t)*s;
        if (po_send(a, n) == 0) po.hello = 0;
        else return;
    }
    oct = oct < -3 ? -3 : oct > 3 ? 3 : oct;
    if (po.state || buttons != po.sent_b || keys != po.sent_k || (uint32_t)(oct + 64) != po.sent_o) {
        a[0] = PO_SAY_STATE, a[1] = (uint8_t)(buttons & 0x7Fu), a[2] = (uint8_t)(buttons >> 7 & 0x7Fu);
        for (k = 0; k < 4u; k++) a[3u + k] = (uint8_t)(keys >> (7u * k) & 0x7Fu);
        a[7] = (uint8_t)(oct + 64);
        if (po_send(a, 8) == 0) po.state = 0, po.sent_b = buttons, po.sent_k = keys, po.sent_o = (uint32_t)(oct + 64);
    }
    master = master < 0 ? 0 : master > 127 ? 127 : master;
    if (master != po.abs_v[7]) po.abs_v[7] = master, po.steps[7] = 1;   /* (MASTER: its place, as a pending send) */
    for (k = 0; k < PO_KNOBS; k++) {
        int32_t d = po.steps[k];
        if (!d || now - po.knob_t[k] < PO_KNOB_MS) continue;
        a[0] = PO_KNOB, a[1] = (uint8_t)k;
        if (k == 7u) a[2] = PO_ABS, a[3] = (uint8_t)po.abs_v[7];
        else a[2] = PO_REL, a[3] = (uint8_t)(64 + (d < -63 ? -63 : d > 63 ? 63 : d));
        if (po_send(a, 4) != 0) break;          /* (no room: the rest next pass) */
        po.steps[k] = k == 7u ? 0 : d - (d < -63 ? -63 : d > 63 ? 63 : d);
        po.knob_t[k] = now;
    }
}

/* a received SysEx (between F0 and F7): 1 if it was PANEL OUT's */
static int po_frame(const uint8_t *p, uint32_t n, uint32_t now)
{
    uint32_t k;
    if (n < 4u || p[0] != PO_H0 || p[1] != PO_H1 || p[2] != PO_H2) return 0;
    switch (p[3]) {
    case PO_HELLO:
        if (!po.on) {                           /* a new app: everything as it is, MASTER's place too */
            for (k = 0; k < PO_KNOBS; k++) po.steps[k] = 0, po.knob_t[k] = now - PO_KNOB_MS;
            po.abs_v[7] = -1;
        }
        po.on = 1, po.heard = now, po.hello = 1, po.state = 1;
        break;
    case PO_BYE:
        po.on = 0;
        break;
    case PO_STATE:
        po.state = 1;
        break;
    default:
        break;
    }
    return 1;
}
