/* SPDX-License-Identifier: GPL-3.0-only
 * Arctic Scream p0larpatch (PolarPatch, 2026). The method after felucca-bsp's analog rows (Leo Kuroshita, Hügelton
 * Instruments; Charles Vestal's fm1-velocity, as fm1-ctl and fm1-monk read them; MPL-2.0: boards/fm1/fm1_input.h) and
 * Ruido (Miguel Hernández Domenech, GPL-3.0-only), written anew for this hal. */
/* The keys' readings for velocity and pressure (src/touch.c makes them into those). The keys' rubber has a carbon pill
 * under it, its resistance falling as it is pressed; the rows PA7 and PA8 are also the SARADC's channels 0 and 1.
 * While fm1_tch.on, each tick starts a conversion on the column it has just latched and takes it at the next, before
 * the rows are read: a frame (11 ticks, ~1.1 ms) reads row PA7, the next PA8, the next the top row, so each key is read
 * every ~3.3 ms:
 *   PA7, PA8      the 20 keys F3 .. D5 (0 .. 19), 1023 open, falling as the pill is pressed. A held key (tch_drive) is
 *                 read with its row driven high by the weaker driver (HD0) instead of pulled up: the pull-up sees
 *                 little but the matrix diode, the driver reads the pill. The row an input again at the next tick
 *   PA8, column 0 (OCT-: a button) the MASTER knob, every FM1_TCH_BATT-th time the battery instead: the main loop
 *                 cannot read them while the scan owns the converter (fm1_tch.master, .batt)
 *   the top row   (keys 20 .. 26, PA6, no converter of its own) through its LED line PA10: PA10 an input with its
 *                 pull-up, PA6 driven, so a pressed key lifts the column's low level, the more the harder (tch_probe)
 * A lit LED in the column lifts its keys' readings: a held key's column stays dark on the ticks that read it
 * (tch_led_ok), so a held key's column glows a little less. A conversion takes ~57 us of the tick's 100.
 *
 *   fm1_touch_harvest()   timer ISR, before fm1_input_tick
 *   fm1_touch_start()     timer ISR, after fm1_input_tick
 *   fm1_touch_on(on)      main loop: the readings on or off (on: MASTER and the battery from here) */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"
#include "fm1_adc.h"
#include "fm1_input.h"

static void tch_sample(uint32_t k, uint32_t v, uint32_t driven);   /* (src/touch.c: the app's side, from the ISR) */
static int tch_drive(uint32_t k);
static void tch_probe(uint32_t k, uint32_t v, uint32_t pressed);
static int tch_led_ok(uint32_t k);

#define FM1_TCH_BATT 64u                        /* the knob's slots per battery reading (~0.2 s) */
#define FM1_TCH_CON 0x200Fu                     /* the converter's clock and start delay (felucca-bsp's 7, 2) */
#define FM1_TCH_GIVEUP 64u                      /* conversions in a row never done: the readings off (the main loop's) */
enum { FT_NONE, FT_KEY, FT_DRIVEN, FT_PROBE, FT_MASTER, FT_BATT };
static volatile struct {
    uint8_t on, phase, what, pin, key, batt_n, dead;
    int16_t master, batt;                       /* -1: none yet */
    uint32_t timeouts;
} fm1_tch = {0, 0, 0, 0, 0, 0, 0, -1, -1, 0};

static void fm1__tch_drive(uint32_t pin, uint32_t hd0)   /* a row driven high by a weak output */
{
    uint32_t m = 1u << pin;
    if (hd0)
        FM1_PR(FM1_PA, FM1_HD0) |= m;
    else
        FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_OUT) |= m;
    FM1_PR(FM1_PA, FM1_DIR) &= ~m;
}
static void fm1__tch_release(uint32_t pin)      /* an input with its pull-up again */
{
    uint32_t m = 1u << pin;
    FM1_PR(FM1_PA, FM1_DIR) |= m;
    FM1_PR(FM1_PA, FM1_OUT) &= ~m;
    FM1_PR(FM1_PA, FM1_HD0) &= ~m;
}
static void fm1__tch_pa10(int probe)            /* PA10 (row PA6's LED line): an input with a pull-up, or an output */
{
    uint32_t m = 1u << 10;
    if (probe) {
        FM1_PR(FM1_PA, FM1_OUT) &= ~m;
        FM1_PR(FM1_PA, FM1_PU) |= m;
        FM1_PR(FM1_PA, FM1_DIR) |= m;
    } else {
        FM1_PR(FM1_PA, FM1_PU) &= ~m;
        FM1_PR(FM1_PA, FM1_DIR) &= ~m;
    }
}
static void fm1__tch_go(uint32_t ch)
{
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = (FM1_TCH_CON & 0xF00Fu) | 0x40u | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    fm1_adc_isr = 1;
}

static void fm1_touch_harvest(void)
{
    uint32_t what = fm1_tch.what, v;
    if (!fm1_adc_isr)
        return;
    if (what == FT_DRIVEN || what == FT_PROBE)
        fm1__tch_release(fm1_tch.pin);
    if (what == FT_PROBE)
        fm1__tch_pa10(0);
    if (FM1_ADC_CON & 0x80u) {
        v = FM1_ADC_RES & 0x3FFu;
        fm1_tch.dead = 0;
        if (what == FT_KEY || what == FT_DRIVEN)
            tch_sample(fm1_tch.key, v, what == FT_DRIVEN);
        else if (what == FT_PROBE)
            tch_probe(fm1_tch.key, v, fm1_in.notes >> fm1_tch.key & 1u);
        else if (what == FT_MASTER)
            fm1_tch.master = (int16_t)v;
        else if (what == FT_BATT)
            fm1_tch.batt = (int16_t)v;
    } else {
        fm1_tch.timeouts++;
        if (++fm1_tch.dead >= FM1_TCH_GIVEUP)   /* (never done: given up, the main loop's again) */
            fm1_tch.on = 0;
    }
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    fm1_tch.what = FT_NONE;
    fm1_adc_isr = 0;
}

static void fm1_touch_start(void)
{
    uint32_t c = fm1__tick_col, id, k;
    if (!fm1_tch.on || fm1_adc_main || fm1_adc_isr)
        return;
    if (c == 0u)                                /* (a new frame: the next row's turn) */
        fm1_tch.phase = (uint8_t)(fm1_tch.phase >= 2u ? 0u : fm1_tch.phase + 1u);
    if (fm1_tch.phase == 2u) {                  /* the top row, through PA10 */
        if (c >= 7u)
            return;
        fm1_tch.key = (uint8_t)(FM1_KEYMAP[2][c] - 14);
        fm1__led_lines(fm1_led[c] & ~(1u << 2));
        fm1__tch_pa10(1);
        fm1__tch_drive(6u, 0u);
        fm1_tch.pin = 6, fm1_tch.what = FT_PROBE;
        fm1__tch_go(2u);
        return;
    }
    if (fm1_tch.phase == 1u && c < 2u) {        /* (OCT- and OCT+: no keys) the knob, now and then the battery */
        if (c)
            return;
        fm1_tch.what = FT_MASTER;
        if (++fm1_tch.batt_n >= FM1_TCH_BATT)
            fm1_tch.batt_n = 0, fm1_tch.what = FT_BATT;
        fm1__tch_go(fm1_tch.what == FT_BATT ? FM1_ADC_BATT : FM1_ADC_MASTER);
        return;
    }
    id = (uint32_t)FM1_KEYMAP[3u + fm1_tch.phase][c];
    k = id - 14u;
    fm1_tch.key = (uint8_t)k;
    fm1_tch.pin = (uint8_t)(fm1_tch.phase ? 8u : 7u);
    if (!tch_led_ok(k))
        fm1__led_lines(0);
    fm1_tch.what = FT_KEY;
    if (tch_drive(k)) {
        fm1__tch_drive(fm1_tch.pin, 1u);
        fm1_tch.what = FT_DRIVEN;
    }
    fm1__tch_go(fm1_tch.phase);
}

static void fm1_touch_on(int on)                /* main loop */
{
    if (on && !fm1_tch.on) {
        fm1_tch.dead = 0;
        fm1_tch.master = (int16_t)fm1_adc_read(FM1_ADC_MASTER);   /* (until the knob's slot comes round) */
        fm1_tch.batt = (int16_t)fm1_adc_read(FM1_ADC_BATT);
    }
    fm1_tch.on = (uint8_t)(on != 0);
}
