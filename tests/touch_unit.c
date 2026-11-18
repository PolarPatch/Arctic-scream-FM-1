/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.2) TOUCH's readings (firmware/src/touch.c) fed by hand, as hal/fm1_touch.h would: a key touched, seated, its
 * velocity from the driven readings on the log scale (against the formula in floats), the LEDs and the drive while
 * held, the release, a graze, the pressure, the top row's probe, and the keys held back until their velocity.
 *   cc -O2 -Wall -o build/host/touch_unit tests/touch_unit.c -lm && build/host/touch_unit */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#define TT_TEST 1
#include "../firmware/src/touch.c"

static int bad;
static void check(const char *what, int ok, double a, double b)
{
    printf("  %-4s  touch unit: %s", ok ? "ok" : "FAIL", what);
    if (!ok) printf(" (%g, %g)", a, b);
    printf("\n");
    bad += !ok;
}
static double want(double d)                    /* the method in floats */
{
    double t;
    if (d <= TT_FIRM) return 127;
    if (d >= TT_LIGHT) return 1;
    t = log2((d - TT_DBASE) / (TT_FIRM - TT_DBASE)) / log2((double)(TT_LIGHT - TT_DBASE) / (TT_FIRM - TT_DBASE));
    return 1 + 126 * pow(1 - t, 0.7);
}
static void press(uint32_t k, uint32_t d0, uint32_t d1)   /* touched, two driven readings, seated */
{
    tch_sample(k, 850, 0);
    if (tch_drive(k)) tch_sample(k, d0, 1);
    tch_sample(k, 300, 0);
    if (tch_drive(k)) tch_sample(k, d1, 1);
}
static void lift(uint32_t k)
{
    tch_sample(k, 1023, 0);
    tch_sample(k, 1023, 0);
}

int main(void)
{
    uint32_t d, worst = 0, ms, k;
    double e = 0;
    check("open: no velocity, LEDs as they are, no drive", tch_vel(3) == 0 && tch_led_ok(3) && !tch_drive(3), 0, 0);
    tch_sample(3, 1023, 0);
    tch_sample(3, 850, 0);
    check("touched: the next reading driven, its column dark", tch_drive(3) && !tch_led_ok(3), 0, 0);
    tch_sample(3, 560, 1);
    check("one driven reading: no velocity yet", tch_vel(3) == 0 && !tch_drive(3), 0, 0);
    tch_sample(3, 300, 0);
    tch_sample(3, 545, 1);
    check("two: the velocity from the lowest", tch_vel(3) == (uint32_t)lround(want(545)), tch_vel(3), want(545));
    lift(3);
    check("let go: open, no velocity", tch_vel(3) == 0 && tch_led_ok(3), 0, 0);
    for (d = 400; d <= 1000; d += 3) {          /* the scale against the floats */
        uint32_t v;
        press(5, d, d);
        v = tch_vel(5);
        if (fabs(v - want(d)) > e) e = fabs(v - want(d)), worst = d;
        lift(5);
    }
    check("the scale: within 1.5 of the formula, 530 .. 896 (the table coarse where the curve is steep)", e <= 1.5, e, worst);
    press(6, 520, 520);
    check("firmer than FIRM: 127", tch_vel(6) == 127u, tch_vel(6), 127);
    lift(6);
    press(6, 950, 950);
    check("lighter than LIGHT: 1", tch_vel(6) == 1u, tch_vel(6), 1);
    lift(6);
    tch_sample(7, 850, 0);
    tch_sample(7, 1023, 0);
    check("a graze: open again", tch_led_ok(7) && !tch_drive(7), 0, 0);

    tch_mode = TCH_PRESS;                       /* the pressure: held at 650, then pressed to 450, then eased */
    press(8, 650, 650);
    for (ms = 10; ms < 400u; ms += 5) tch_sample(8, 300, 0), tch_sample(8, 650, 1), tch_update(ms);
    check("held still: no pressure", tch_pv[8] == 0, tch_pv[8], 0);
    for (; ms < 600u; ms += 5) tch_sample(8, 300, 0), tch_sample(8, 450, 1), tch_update(ms);
    check("pressed harder: pressure", tch_pv[8] > 100, tch_pv[8], 100);
    for (; ms < 800u; ms += 5) tch_sample(8, 300, 0), tch_sample(8, 650, 1), tch_update(ms);
    check("eased: none again", tch_pv[8] < 3, tch_pv[8], 0);
    lift(8);
    tch_update(ms + 5);
    check("let go: none", tch_pv[8] == 0, tch_pv[8], 0);

    for (k = 0; k < 8u; k++) tch_probe(22, 100, 0);   /* the top row: open at 100, then a press peaking 43 above */
    tch_probe(22, 120, 1);
    check("probe: one reading pressed, no velocity yet", tch_vel(22) == 0, tch_vel(22), 0);
    tch_probe(22, 143, 1);
    check("probe: 43 above its open level: 127", tch_vel(22) == 127u, tch_vel(22), 127);
    tch_probe(22, 100, 0);
    tch_probe(22, 120, 1), tch_probe(22, 115, 1);
    check("probe: 20 above: soft", tch_vel(22) == 21u, tch_vel(22), 21);

    tch_mode = TCH_VEL;                         /* the keys held back for their velocity */
    tch_shown = tch_wait = 0;
    check("a key down, no reading: held back", tch_keys(1u << 9, 1000) == 0, 0, 0);
    check(".. 15 ms on: shown, at 100", tch_keys(1u << 9, 1015) == 1u << 9 && tch_v[9] == 100u, tch_v[9], 100);
    press(10, 600, 600);
    check("a key with its velocity: shown at once", tch_keys(1u << 9 | 1u << 10, 1016) == (1u << 9 | 1u << 10) &&
          tch_v[10] == tch_vel(10), tch_v[10], tch_vel(10));
    check("a key down, held back,", tch_keys(1u << 11, 1020) == 0, 0, 0);
    check(".. let go before its velocity: down now,", tch_keys(0, 1022) == 1u << 11, 0, 0);
    check(".. and up the next time", tch_keys(0, 1023) == 0, 0, 0);
    tch_mode = TCH_OFF;
    check("TOUCH off: the keys as they are, every voice full", tch_keys(5, 1030) == 5u && tch_gain(1) == 0, 0, 0);
    tch_mode = TCH_VEL;
    check("the level: 127 full, 1 about 24 dB down", tch_gain(127) == 0 && tch_gain(1) == 16, tch_gain(1), 16);
    return bad ? 1 : 0;
}
