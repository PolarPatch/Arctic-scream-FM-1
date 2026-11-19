/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.4) TOUCH (SYSTEM 4): velocity and pressure from the FM-1's own keys, no change to the hardware. The method and
 * its numbers are Charles Vestal's (fm1-base lib/sense.c and lib/touch.c, from fm1-velocity, CTL-1 and MONK-1, MPL-2.0:
 * measured on an FM-1 over 830 presses), written anew here in integers (the chip has no FPU); the readings come from
 * hal/fm1_touch.h (the timer ISR, every ~3.3 ms a key).
 *
 * The 20 keys on the analog rows (F3 .. D5, 0 .. 19): a reading of 1023 is open, falling as the key's carbon pill is
 * pressed. Under TT_TOUCH the pill has started to conduct (the key touched); from then on every other reading is taken
 * with the row driven (tch_drive), which reads the pill itself: TT_DBASE (the diode and the 595) plus a resistance that
 * falls as a power of the force, so its log is linear in the log of the force.
 *   velocity   the lowest driven reading of the first TT_WAIT after the touch, on a log scale between TT_FIRM (127)
 *              and TT_LIGHT (1), then a curve of exponent 0.7 (regular playing lands at about 45 / 63 / 80)
 *   pressure   pressing harder than the key rests: its zero follows the driven reading down while the key settles
 *              (TT_SETTLE), then creeps after it, and comes back up at once when the key is eased; the lean is the
 *              pill's resistance below its rest in octaves, TT_DEAD of noise, TT_SPAN more is 127; smoothed ~30 ms
 * The top row (C#5 .. G5, 20 .. 26) has no converter: its column's low level read through its LED line (tch_probe);
 * a pressed key lifts it the more the harder it lands. Each key keeps its open level and the peak above it since the
 * press: 0 .. TT_TOP above it, a curve of 2.4 (velocity only; no pressure).
 *
 * The notes still come from the digital scan (fm1_in.notes): a key down waits for its velocity (tch_keys) at most
 * TT_WAIT_MS, then plays at what there is (100 with no reading). Velocity scales a voice's level (VG: 24 dB from 127 to
 * 1), a MIDI note's too while TOUCH is on; pressure deepens the track's LFO (to twice its DEPTH) and is HOWL's
 * pressure, as MIDI's. Off (the start), nothing is read and nothing changes. */

#define TT_TOUCH 900u                           /* a pulled-up reading under it: touched */
#define TT_SEAT 400u                            /* .. under it: seated */
#define TT_OPEN 600u                            /* lifting: over it twice running, open */
#define TT_WAIT 2u                              /* driven readings after the touch the velocity waits for (~6.6 ms) */
#define TT_DBASE 185u                           /* a driven reading with no pill: the diode and the 595 */
#define TT_FIRM 585u                            /* the driven reading that is 127 (CTL-1's 530, tuned by playing; on
                                                 * PolarPatch's FM-1 a firm hit read ~600, a light one ~865) */
#define TT_LIGHT 885u                           /* .. and 1 (CTL-1's 896) */
#define TT_TOP 43u                              /* the top row: the probe's rise above its open level that is 127 */
#define TT_WAIT_MS 15u                          /* a key down waits this long for its velocity at most */
#define TT_SETTLE 250u                          /* ms: the pressure's zero follows the key down while it settles */
#define TT_DEAD 2621                            /* 0.04 octave (Q16): noise */
#define TT_SPAN 10486                           /* 0.16 octave (Q16): 127 (MONK-1's 0.46 reached 37 at the firmest on
                                                 * PolarPatch's FM-1; its most sensitive setting is 0.12) */
#define TT_NA 20u                               /* the keys on the analog rows */
enum { TS_OPEN, TS_TOUCH, TS_DOWN, TS_LIFT };
enum { TCH_OFF, TCH_VEL, TCH_PRESS };           /* SYSTEM 4's TOUCH */

static volatile struct {                        /* the ISR's */
    uint8_t st, dnext, nd, up, ready;
    uint16_t d, dmin, vel_d;                    /* driven: the last, the lowest since the touch, the velocity's */
    uint16_t v;                                 /* the last pulled-up reading (SYSTEM 4's test line) */
} tt_k[TT_NA];
static volatile struct { uint16_t base16, peak; uint8_t n, down; } tt_p[7];   /* the top row's probe */
static volatile uint8_t tch_mode;               /* TOUCH: TCH_ (main loop; sys_sync) */
static volatile uint8_t id_vel[256];            /* a note's velocity as a voice's level (tch_gain), by its origin id:
                                                 * KEY_ID 0 .. 26, MIDI_ID 32 .. 159, (shot.c) 208 .. 223 (the main loop writes it before
                                                 * the ask; key_ask takes it) */
static uint32_t tch_shown, tch_wait;            /* main loop: the keys tch_keys shows down; those waiting */
static uint32_t tch_t0[27];
static uint8_t tch_v[27];                       /* main loop: each key's velocity as it was shown (0: none) */
static int32_t tt_base[TT_NA], tt_s[TT_NA];     /* main loop: the pressure's zero (log2, Q16), its smoothed 0 .. 32767 */
static uint32_t tt_on, tt_at[TT_NA], tt_ms;
static uint8_t tch_pv[27];                      /* main loop: each key's pressure, 0 .. 127 */
static uint8_t tch_last_k = 0xFF, tch_last_v;   /* (SYSTEM 4: the last key, its velocity) */

/* ---- the ISR's side (hal/fm1_touch.h; tests/touch_test.c) */
#if defined FM1_TOUCH_HAL || defined TT_TEST
static void tch_sample(uint32_t k, uint32_t v, uint32_t driven)
{
    if (k >= TT_NA) return;
    if (driven) {                               /* the pill: no change of state (a driven graze reads open too) */
        tt_k[k].dnext = 0;
        if (tt_k[k].st == TS_OPEN) return;
        tt_k[k].d = (uint16_t)v;
        if (tt_k[k].st != TS_LIFT && v < tt_k[k].dmin) tt_k[k].dmin = (uint16_t)v;
        if (tt_k[k].nd < 255u && ++tt_k[k].nd == TT_WAIT) tt_k[k].vel_d = tt_k[k].dmin, tt_k[k].ready = 1;
        return;
    }
    tt_k[k].v = (uint16_t)v;
    switch (tt_k[k].st) {
    case TS_OPEN:
        if (v < TT_TOUCH) tt_k[k].st = TS_TOUCH, tt_k[k].dmin = 1023, tt_k[k].nd = 0, tt_k[k].ready = 0;
        break;
    case TS_TOUCH:
        if (v >= TT_TOUCH) tt_k[k].st = TS_OPEN;   /* (a graze) */
        else if (v < TT_SEAT) tt_k[k].st = TS_DOWN;
        break;
    case TS_DOWN:
        if (v >= TT_SEAT) tt_k[k].st = TS_LIFT, tt_k[k].up = v >= TT_OPEN;   /* (this one counts: two running) */
        break;
    default:                                    /* TS_LIFT */
        if (v < TT_SEAT) tt_k[k].st = TS_DOWN;
        else if (v < TT_OPEN) tt_k[k].up = 0;
        else if (++tt_k[k].up >= 2u) tt_k[k].st = TS_OPEN, tt_k[k].ready = 0;
        break;
    }
    tt_k[k].dnext = tt_k[k].st != TS_OPEN;      /* (a held key: the next reading driven) */
}
static int tch_drive(uint32_t k) { return k < TT_NA && tt_k[k].dnext; }
static int tch_led_ok(uint32_t k) { return k >= TT_NA || tt_k[k].st == TS_OPEN; }
static void tch_probe(uint32_t k, uint32_t v, uint32_t pressed)
{
    uint32_t i = k - TT_NA;
    if (i >= 7u) return;
    if (!tt_p[i].base16) tt_p[i].base16 = (uint16_t)(v << 4);
    if (!pressed) {                             /* open: its level, smoothed */
        tt_p[i].down = 0, tt_p[i].peak = 0, tt_p[i].n = 0;
        tt_p[i].base16 = (uint16_t)((int32_t)tt_p[i].base16 + (((int32_t)v << 4) - (int32_t)tt_p[i].base16) / 8);
    } else {
        uint32_t b = tt_p[i].base16 >> 4, r = v > b ? v - b : 0u;
        tt_p[i].down = 1;
        if (tt_p[i].n < 255u) tt_p[i].n++;
        if (r > tt_p[i].peak) tt_p[i].peak = (uint16_t)r;
    }
}
#endif

/* ---- the main loop's */
static int32_t tt_log2(uint32_t x)              /* log2(x), Q16, x >= 1 (a 33-point table between the octaves) */
{
    static const uint16_t L[33] = {0, 2909, 5732, 8473, 11136, 13727, 16248, 18704, 21098, 23433, 25711, 27936,
                                   30109, 32234, 34312, 36346, 38336, 40286, 42196, 44068, 45904, 47705, 49472, 51207,
                                   52911, 54584, 56229, 57845, 59434, 60997, 62534, 64047, 65535};
    int32_t e = 0;
    uint32_t f, i, r;
    if (!x) return 0;
    while (x >> (e + 1)) e++;
    f = e >= 16 ? x >> (e - 16) : x << (16 - e);   /* 1.0 .. 2.0, Q16 */
    f -= 65536u;
    i = f >> 11, r = f & 2047u;
    return (e << 16) + L[i] + (int32_t)(((L[i + 1u] - L[i]) * r) >> 11);
}
static uint32_t tt_vel_d(uint32_t d)            /* a driven reading's velocity, 1 .. 127 */
{
    static const uint16_t P[65] = {32767, 32408, 32047, 31684, 31320, 30953, 30585, 30215, 29843, 29469, 29093,
        28715, 28334, 27952, 27567, 27180, 26790, 26398, 26004, 25607, 25207, 24805, 24400, 23992, 23580, 23166, 22749,
        22328, 21904, 21476, 21045, 20610, 20170, 19727, 19279, 18827, 18371, 17909, 17442, 16969, 16491, 16007, 15517,
        15020, 14515, 14004, 13483, 12955, 12416, 11868, 11308, 10737, 10152, 9552, 8935, 8300, 7643, 6961, 6249, 5500,
        4705, 3847, 2896, 1783, 0};             /* (1 - t)^0.7, t = i / 64 */
    int32_t l0 = tt_log2(TT_FIRM - TT_DBASE), t, i, p;
    if (d <= TT_FIRM) return 127u;
    if (d >= TT_LIGHT) return 1u;
    t = ((tt_log2(d - TT_DBASE) - l0) << 12) / ((tt_log2(TT_LIGHT - TT_DBASE) - l0) >> 4);   /* 0 .. 1, Q16 (no 64-bit
                                                 * division on the chip) */
    t = t < 0 ? 0 : t > 65535 ? 65535 : t;
    i = t >> 10;
    p = P[i] + (((P[i + 1] - P[i]) * (t & 1023)) >> 10);
    return 1u + (uint32_t)((126 * p + 16384) >> 15);
}
static uint32_t tch_vel(uint32_t k)             /* key k's velocity for its press now, 0: no reading yet */
{
    static const uint8_t P24[TT_TOP + 1u] = {1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 19, 21, 24,
        26, 29, 32, 35, 39, 42, 46, 50, 54, 58, 63, 68, 73, 78, 83, 89, 95, 101, 107, 113, 120, 127};
#ifdef DINGHY_HOST
    if (k < 27u && host_tvel[k]) return host_tvel[k];
#endif
    if (k < TT_NA) return tt_k[k].ready ? tt_vel_d(tt_k[k].vel_d) : 0u;
    if (k < 27u && tt_p[k - TT_NA].down && tt_p[k - TT_NA].n >= 2u)
        return P24[tt_p[k - TT_NA].peak > TT_TOP ? TT_TOP : tt_p[k - TT_NA].peak];
    return 0;
}
/* the keys as the notes see them: a key down shown once its velocity is read (TT_WAIT_MS at most), a key let go
 * before that shown down this pass and up the next (TOUCH off: the keys as they are) */
static uint32_t tch_keys(uint32_t raw, uint32_t ms)
{
    uint32_t k, v, show = tch_shown;
    if (tch_mode == TCH_OFF) {
        tch_shown = raw, tch_wait = 0;
        return raw;
    }
    for (k = 0; k < 27u; k++) {
        uint32_t b = 1u << k;
        if (raw & b) {
            if (show & b) continue;
            if (!(tch_wait & b)) tch_wait |= b, tch_t0[k] = ms;
            v = tch_vel(k);
            if (!v && ms - tch_t0[k] < TT_WAIT_MS) continue;
            show |= b, tch_wait &= ~b, tch_v[k] = (uint8_t)(v ? v : 100u);
            tch_last_k = (uint8_t)k, tch_last_v = tch_v[k];
        } else if (tch_wait & b) {              /* (let go while it waited: down now, up next time) */
            show |= b, tch_wait &= ~b, tch_v[k] = (uint8_t)(tch_vel(k) ? tch_vel(k) : 100u);
        } else {
            show &= ~b;
        }
    }
    tch_shown = show;
    return show;
}
/* a velocity as a voice's level (vc_vel's: x / 256, 0 full): 24 dB from 127 to 1 */
static uint8_t vel_gain(uint32_t vel)
{
    static const uint8_t VG[128] = {16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22, 23, 23, 24,
        25, 25, 26, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39, 40, 41, 41, 42, 43, 44,
        45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 58, 59, 60, 62, 63, 64, 66, 67, 69, 70, 72, 73, 75, 77, 78, 80, 82,
        84, 86, 87, 89, 91, 93, 95, 98, 100, 102, 104, 106, 109, 111, 114, 116, 119, 121, 124, 127, 130, 133, 136, 139,
        142, 145, 148, 151, 155, 158, 162, 165, 169, 173, 176, 180, 184, 188, 192, 197, 201, 206, 210, 215, 220, 224,
        229, 235, 240, 245, 250, 0};
    return VG[vel > 127u ? 127u : vel];
}
static uint8_t tch_gain(uint32_t vel) { return tch_mode == TCH_OFF ? 0u : vel_gain(vel); }   /* (TOUCH off: full) */
/* every main-loop pass: each held key's pressure, every 5 ms */
static void tch_update(uint32_t ms)
{
    uint32_t k;
    if (ms - tt_ms < 5u) return;
    tt_ms = ms;
    for (k = 0; k < 27u; k++) {
        int32_t l, x;
#ifdef DINGHY_HOST
        if (host_tvel[k]) {
            tch_pv[k] = tch_mode == TCH_PRESS && (tch_shown >> k & 1u) ? host_tpress[k] : 0u;
            continue;
        }
#endif
        if (k >= TT_NA || tch_mode != TCH_PRESS || tt_k[k].st != TS_DOWN || !tt_k[k].nd) {
            if (k < TT_NA) tt_on &= ~(1u << k), tt_s[k] = 0;
            tch_pv[k] = 0;
            continue;
        }
        l = tt_log2(tt_k[k].d > TT_DBASE + 1u ? tt_k[k].d - TT_DBASE : 1u);
        if (!(tt_on >> k & 1u)) tt_on |= 1u << k, tt_base[k] = l, tt_at[k] = ms;
        if (l > tt_base[k]) tt_base[k] = l;     /* (eased off: that is the rest now) */
        else tt_base[k] += ms - tt_at[k] < TT_SETTLE ? (l - tt_base[k]) / 4 : (l - tt_base[k]) * 52 / 65536;
        x = tt_base[k] - l - TT_DEAD;
        x = x < 0 ? 0 : x > TT_SPAN ? 32767 : x * 32767 / TT_SPAN;
        tt_s[k] += (x - tt_s[k]) * 17 / 100;
        tch_pv[k] = (uint8_t)((tt_s[k] * 127 + 16384) >> 15);
    }
}
