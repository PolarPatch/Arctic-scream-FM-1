/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 PolarPatch */
/* (2.1.5) SCREEN MIRROR: the FM-1's screen, row by row, for an app that shows it big (FM-1 Mirror, FM-1 Live), for
 * whoever cannot read a 1.54-inch display. The Big Screen protocol of Patrick F. Kellogg (pfkellogg/fm1_open, branch
 * screen-out: BIG_SCREEN.md and screen_mirror/README.md; its screen part MIT), written anew from its description; none
 * of its code. Free for any FM-1 firmware to copy (MIT, this file alone).
 *
 * SysEx over USB-MIDI, F0 7D 46 53 cmd .. F7 ("FS"):
 *   app -> device      F0 7D 46 53 00 F7     keep sending the screen (about once a second)
 *                      F0 7D 46 53 01 F7     the same, and the whole screen again (a new viewer)
 *                      F0 7D 46 53 02 F7     keep sending the sound's name only, no screen
 *   device -> app      F0 7D 46 53 01 rl rh cl ch {n c0 c1 c2}.. F7
 *                          one row's pixels from column c (row, column: 7 bits low, then high); each run a count
 *                          1 .. 127 and its RGB565 colour as bits 0 .. 6, 7 .. 13, 14 .. 15; at most 70 bytes a message
 *                      F0 7D 46 53 02 name.. F7   the sound playing (ASCII), after each hello and when it changes
 * Only what is painted goes out, as it is painted; 3 s after the last hello, nothing. With nobody asking nothing is
 * built or sent, so playing is as before. A hello longer than six bytes is not one (the protocol's rule).
 * The rows wait in a ring (MR_BUF) for room in the USB queue; never waiting on it. A row the ring has no room for is
 * dropped and noted (a pass that is not a repair keeps a quarter of the ring back for them). Once the ring is half
 * empty, a repair: everything drawn again (mr_redraw), and of that pass only
 * the noted rows go out (a band of them, the band halved while it does not fit, doubled while it does). Every other
 * row's paints in that pass are fingerprinted together and kept: one that differs from the last (it changed just then,
 * or the paints the viewer has since are not a repair's) is noted for the next. A paint the same as one the viewer
 * has on top of its row already (a slab drawn again as it was) is not sent again. So a new viewer's whole screen (every row noted) and a busy screen's dropped
 * rows come through a share at a time, however small the ring, and what the viewer sees is the screen in the end.
 *
 * The includer supplies, before the #include: mr_wire_send(p, n) (one F0..F7 message; 0 = sent, else not sent at all),
 * mr_redraw() (everything drawn again); it calls mr_frame() with each received SysEx (the bytes between F0 and F7),
 * mr_blit() / mr_fill() wherever pixels go to the panel, mr_tick() from its main loop after the screen is drawn,
 * mr_send() whenever it may (while the screen's DMA runs), mr_name() with the sound's name. */
#include <stdint.h>
#ifndef MR_BUF
#define MR_BUF 8192u                            /* the ring (a power of two) */
#endif
#define MR_MSG 70u                              /* a message's bytes, F0 .. F7, at most */
#define MR_HEAD 9u                              /* F0 7D 46 53 01 rl rh cl ch */
#define MR_RUNS ((MR_MSG - MR_HEAD - 1u) / 4u)  /* runs a message (15) */
#define MR_GONE_MS 3000u
#define MR_AGAIN_MS 250u                        /* (a repair at most every 0.25 s) */

static int mr_wire_send(const uint8_t *p, uint32_t n);
static void mr_redraw(void);

static uint8_t mr_ring[MR_BUF] __attribute__((section(".pool")));
static uint32_t mr_print_was[240] __attribute__((section(".pool")));   /* a row's paints in the last repair it had,
                                                 * fingerprinted together */
static uint32_t mr_print_now[240] __attribute__((section(".pool")));   /* .. in this repair so far (0: none) */
#define MR_LAST 3u                              /* a row's last paints sent, newest last: one painted again the same, and
                                                 * nothing sent since over it, is not sent again (a slab redrawn as it was) */
static uint32_t mr_last_h[240][MR_LAST] __attribute__((section(".pool")));
static uint8_t mr_last_x[240][MR_LAST] __attribute__((section(".pool")));
static uint8_t mr_last_w[240][MR_LAST] __attribute__((section(".pool")));
static uint8_t mr_nlast[240] __attribute__((section(".pool")));
static struct {
    uint8_t on, name_on, repair, say;           /* the screen asked for; the name; a repair's pass; the name to send */
    uint8_t hold, painted;                      /* a repair due: other paints wait (the ring drains for it); the repair's
                                                 * pass has painted */
    uint32_t heard, name_heard, fix_t;          /* the last hellos; the last repair */
    uint32_t w, r;                              /* the ring: each message its length, then its bytes */
    uint32_t band, next;                        /* rows a repair sends whole; where the next band starts */
    uint32_t drop[8], fix[8], touch[8], known[8], still[8];   /* (bit y) rows the viewer lacks; this repair's band; sent since
                                                 * the last repair; mr_print_was is the viewer's; drawn the same
                                                 * at the last two repairs (not moving: in the band first) */
    char name[24];                              /* the name last sent */
} mr = {.band = 240u};

#define MR_BIT(a, y) ((a)[(y) >> 5] >> ((y) & 31u) & 1u)
#define MR_SET(a, y) ((a)[(y) >> 5] |= 1u << ((y) & 31u))
#define MR_CLR(a, y) ((a)[(y) >> 5] &= ~(1u << ((y) & 31u)))

static int mr_on(void) { return mr.on; }

static uint32_t mr_room(void) { return MR_BUF - (mr.w - mr.r); }

/* one row's message into the ring (m: MR_HEAD + 4 x runs bytes, its F7 still to add), keeping room back; 0: no room */
static int mr_push(uint8_t *m, uint32_t n, uint32_t keep)
{
    uint32_t i;
    m[n++] = 0xF7;
    if (mr_room() < n + 1u + keep) return 0;
    mr_ring[mr.w++ % MR_BUF] = (uint8_t)n;
    for (i = 0; i < n; i++) mr_ring[mr.w++ % MR_BUF] = m[i];
    return 1;
}

static uint32_t mr_head(uint8_t *m, uint32_t y, uint32_t x)
{
    m[0] = 0xF0, m[1] = 0x7D, m[2] = 'F', m[3] = 'S', m[4] = 1;
    m[5] = (uint8_t)(y & 127u), m[6] = (uint8_t)(y >> 7), m[7] = (uint8_t)(x & 127u), m[8] = (uint8_t)(x >> 7);
    return MR_HEAD;
}

static uint32_t mr_px(const uint16_t *px, uint32_t i, uint16_t c)   /* (px swapped, the panel's byte order; 0: c) */
{
    return px ? (uint32_t)(px[i] >> 8 | (px[i] & 0xFFu) << 8) : c;
}

/* a paint's fingerprint: where, how wide, what */
static uint32_t mr_print(uint32_t x, uint32_t w, const uint16_t *px, uint16_t c)
{
    uint32_t h = 2166136261u ^ (x << 8 | w), i;
    if (!px) return ((h ^ c) * 16777619u) + 1u;   /* (a fill: unlike a row of pixels the same) */
    for (i = 0; i < w; i++) h = (h ^ px[i]) * 16777619u;
    return h;
}

/* the viewer has this paint on top of row y already (painted the same, nothing sent over it since) */
static int mr_had(uint32_t y, uint32_t x, uint32_t w, uint32_t h)
{
    uint32_t i, j, n = mr_nlast[y];
    for (i = n; i--;)
        if (mr_last_x[y][i] == x && mr_last_w[y][i] == w && mr_last_h[y][i] == h) {
            for (j = i + 1u; j < n; j++)        /* (a later one over it: not on top) */
                if (mr_last_x[y][j] < x + w && x < (uint32_t)mr_last_x[y][j] + mr_last_w[y][j]) return 0;
            return 1;
        }
    return 0;
}
static void mr_had_add(uint32_t y, uint32_t x, uint32_t w, uint32_t h)
{
    uint32_t i, n = mr_nlast[y];
    if (n == MR_LAST)
        for (i = 1, n--; i <= n; i++)
            mr_last_x[y][i - 1u] = mr_last_x[y][i], mr_last_w[y][i - 1u] = mr_last_w[y][i], mr_last_h[y][i - 1u] = mr_last_h[y][i];
    mr_last_x[y][n] = (uint8_t)x, mr_last_w[y][n] = (uint8_t)w, mr_last_h[y][n] = h;
    mr_nlast[y] = (uint8_t)(n + 1u);
}

/* a row of w pixels at (x, y), as it goes to the panel: px (swapped), or c for all of them */
static void mr_row(uint32_t x, uint32_t y, uint32_t w, const uint16_t *px, uint16_t c)
{
    uint8_t m[MR_MSG];
    uint32_t i = 0, n, runs = 0, v, k, keep = 0, ok = 1, h = mr_print(x, w, px, c);
    if (!mr.repair) {
        if (!MR_BIT(mr.drop, y) && mr_had(y, x, w, h)) return;   /* (as the viewer has it on top) */
        MR_SET(mr.touch, y);
        if (mr.hold) {                          /* (a repair due: this row whole with it, the ring left to drain) */
            MR_SET(mr.drop, y), mr_nlast[y] = 0;
            return;
        }
        keep = MR_BUF / 4u;                     /* (room kept for the repairs: a busy screen's rows dropped first) */
    } else {                                    /* (a repair's pass: the band out; the others fingerprinted) */
        uint32_t first = !mr_print_now[y];
        mr.painted = 1;
        mr_print_now[y] = (mr_print_now[y] * 31u + h) | 1u;
        if (!MR_BIT(mr.fix, y)) return;
        if (first) mr_nlast[y] = 0;             /* (the band's row: what the viewer has is this pass's paints) */
    }
    n = mr_head(m, y, x);
    while (i < w) {
        v = mr_px(px, i, c);
        for (k = 1; i + k < w && k < 127u && mr_px(px, i + k, c) == v; k++)
            ;
        if (runs == MR_RUNS) {                  /* (full: out, the next from here) */
            if (!(ok = mr_push(m, n, keep))) break;
            n = mr_head(m, y, x + i), runs = 0;
        }
        m[n++] = (uint8_t)k, m[n++] = (uint8_t)(v & 127u), m[n++] = (uint8_t)(v >> 7 & 127u), m[n++] = (uint8_t)(v >> 14);
        runs++, i += k;
    }
    if (!ok || !mr_push(m, n, keep)) MR_SET(mr.drop, y), mr_nlast[y] = 0;   /* (no room: this row whole at a repair) */
    else mr_had_add(y, x, w, h);
}

/* pixels to the panel: a block of rows (px swapped, as lcd_blit sends them), or a rectangle of one colour */
static void mr_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t j;
    if (!mr.on || !w || x >= 240u) return;
    for (j = 0; j < h && y + j < 240u; j++)
        mr_row(x, y + j, x + w > 240u ? 240u - x : w, px + j * w, 0);
}
static void mr_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t j;
    if (!mr.on || !w || x >= 240u) return;
    if (x + w > 240u) w = 240u - x;
    for (j = 0; j < h && y + j < 240u; j++)
        mr_row(x, y + j, w, 0, c);
}

/* a received SysEx (between F0 and F7): 1 if it was the mirror's */
static int mr_frame(const uint8_t *p, uint32_t n, uint32_t now)
{
    uint32_t k;
    if (n < 4u || p[0] != 0x7Du || p[1] != 'F' || p[2] != 'S') return 0;
    if (n != 4u || p[3] > 2u) return 1;         /* (not a hello: taken, nothing done) */
    mr.say = 1;                                 /* (the name after every hello) */
    if (p[3] == 2u) {
        mr.name_on = 1, mr.name_heard = now;
        return 1;
    }
    if (!mr.on || p[3] == 1u) {                 /* (a new viewer: every row, nothing old first) */
        for (k = 0; k < 8u; k++) mr.drop[k] = mr.touch[k] = k < 7u ? 0xFFFFFFFFu : 0xFFFFu;
        for (k = 0; k < 8u; k++) mr.known[k] = 0;
        for (k = 0; k < 240u; k++) mr_nlast[k] = 0;
        mr.w = mr.r = 0, mr.fix_t = now - MR_AGAIN_MS;
    }
    mr.on = 1, mr.heard = now;
    return 1;
}

/* what waits, out as the USB queue takes it (the main loop; also while the screen's DMA runs) */
static void mr_send(void)
{
    uint8_t m[MR_MSG];
    uint32_t n, i;
    while (mr.on && mr.w != mr.r) {
        n = mr_ring[mr.r % MR_BUF];
        for (i = 0; i < n; i++) m[i] = mr_ring[(mr.r + 1u + i) % MR_BUF];
        if (mr_wire_send(m, n)) break;
        mr.r += n + 1u;
    }
}

/* row y not yet as the screen has it (dropped, or its repair not drawn): tests/host.c's check */
static int mr_stale(uint32_t y) { return (int)(MR_BIT(mr.drop, y) | (mr.repair && MR_BIT(mr.fix, y))); }

static void mr_repaired(void)                   /* a repair's pass drawn */
{
    uint32_t y, missed = 0, f;
    for (y = 0; y < 240u; y++) {
        f = MR_BIT(mr.fix, y);
        if (!mr_print_now[y]) {                 /* (not drawn (a dark screen): its band again next time) */
            if (f) MR_SET(mr.drop, y);
            continue;
        }
        if (f) {                                /* (sent whole: the viewer's, unless some did not fit) */
            if (MR_BIT(mr.drop, y)) missed = 1, MR_CLR(mr.known, y);
            else MR_SET(mr.known, y), MR_CLR(mr.touch, y);
        } else if (MR_BIT(mr.touch, y) || !MR_BIT(mr.known, y) || mr_print_now[y] != mr_print_was[y]) {
            MR_SET(mr.drop, y), MR_CLR(mr.known, y);   /* (not as the viewer has it, or not sure: whole, next) */
        }
        if (mr_print_now[y] == mr_print_was[y]) MR_SET(mr.still, y); else MR_CLR(mr.still, y);
        mr_print_was[y] = mr_print_now[y], mr_print_now[y] = 0;
    }
    mr.band = missed ? (mr.band > 16u ? mr.band / 2u : 8u) : (mr.band < 120u ? mr.band * 2u : 240u);
    mr.repair = 0;
}

/* the main loop, each pass, after the screen is drawn */
static void mr_tick(uint32_t now)
{
    uint32_t k, y, any = 0;
    if (mr.name_on && now - mr.name_heard >= MR_GONE_MS) mr.name_on = 0;
    if (mr.repair && (mr.painted || now - mr.fix_t >= 500u)) mr_repaired();   /* (its pass: the next that paints, as a
                                                 * screen that draws now and then (GOVVA) does) */
    if (!mr.on) return;
    if (now - mr.heard >= MR_GONE_MS) {         /* (nobody asking: gone, the ring emptied) */
        mr.on = 0, mr.w = mr.r = 0;
        return;
    }
    mr_send();
    if (mr.repair) return;
    for (k = 0; k < 8u; k++) any |= mr.drop[k];
    mr.hold = any && now - mr.fix_t >= MR_AGAIN_MS;
    if (mr.hold && mr_room() >= MR_BUF / 2u) {   /* (rows the viewer lacks, the ring
                                                 * half empty: a repair) */
        uint32_t pass, n = 0;
        for (k = 0; k < 8u; k++) mr.fix[k] = 0;
        for (pass = 0; pass < 2u; pass++)       /* (a band of them, round the screen: the still rows first) */
            for (k = 0, y = mr.next; k < 240u && n < mr.band; k++, y = (y + 1u) % 240u)
                if (MR_BIT(mr.drop, y) && (pass || MR_BIT(mr.still, y))) MR_SET(mr.fix, y), MR_CLR(mr.drop, y), n++;
        mr.next = y;
        mr.repair = 1, mr.painted = mr.hold = 0, mr.fix_t = now;
        mr_redraw();
    }
}

/* the sound playing, ASCII (every ~100 ms is plenty): sent after a hello, and when it changes */
static void mr_name(const char *s, uint32_t now)
{
    uint8_t m[6 + sizeof mr.name];
    uint32_t i, same = 1;
    (void)now;
    if (!mr.on && !mr.name_on) return;
    for (i = 0; i + 1u < sizeof mr.name && s[i]; i++)
        if (mr.name[i] != s[i]) same = 0;
    if (mr.name[i]) same = 0;
    if (same && !mr.say) return;
    m[0] = 0xF0, m[1] = 0x7D, m[2] = 'F', m[3] = 'S', m[4] = 2;
    for (i = 0; i + 1u < sizeof mr.name && s[i]; i++) m[5u + i] = (uint8_t)(s[i] & 0x7F);
    m[5u + i] = 0xF7;
    if (mr_wire_send(m, 6u + i)) return;        /* (no room: the next call) */
    for (i = 0; i + 1u < sizeof mr.name && s[i]; i++) mr.name[i] = s[i];
    mr.name[i] = 0, mr.say = 0;
}
