/* SPDX-License-Identifier: GPL-3.0-only
 * Arctic Scream p0larpatch (PolarPatch, 2026). The method after Ruido (Miguel Hernández Domenech,
 * github.com/miguelhd/ruido, GPL-3.0-only: platforms/fm1/fm1_main.c, fm1_panel_io.h), written anew for this hal. */
/* How hard a key is held (a test: ARCTIC_PRESS_TEST). The keys' rubber has a carbon pill under it, its resistance falls
 * as it is pressed. The rows PA7 and PA8 are also the SARADC's channels 0 and 1: while a held key's column is the one
 * the 595 pulls low, the row is driven high (the weaker drive, HD0) instead of pulled up, and the ADC reads what the pill
 * leaves of it: ~815 at a touch, ~528 pressed firmly (Ruido's figures), 1023 open. The note keys on rows PA7 and PA8:
 * 20 of the 27 (ids 14 .. 33); PA6's (34 .. 40) stay on and off only.
 *
 *   fm1_press_harvest()   timer ISR, before fm1_input_tick: a reading started a tick ago taken, the row an input again
 *   fm1_press_start()     timer ISR, after fm1_input_tick: the column just latched holds the key to read: start it
 *   fm1_press_raw[k]      note key k's last reading (0..1023), -1 none yet since it went down
 *
 * One key at a time, round the held ones, every FM1_PRESS_EVERY frames: its column's LEDs dark that tick (their
 * current through the 595 would lift the reading), so a held key's column glows a little less. The ADC is the MASTER
 * knob's and the battery's too: fm1_adc_read (main loop) says so in fm1_adc_main, waits for a reading under way,
 * and the ISR starts none meanwhile. */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"
#include "fm1_adc.h"
#include "fm1_input.h"

#ifndef FM1_PRESS_EVERY
#define FM1_PRESS_EVERY 3u                       /* frames between readings (a frame: ~1.1 ms) */
#endif
static volatile int16_t fm1_press_raw[27];
static uint8_t fm1_press_pin, fm1_press_key, fm1_press_next;
static uint32_t fm1_press_frame;

static void fm1_press_harvest(void)
{
    uint32_t m;
    int32_t v = -1;
    if (!fm1_adc_isr)
        return;
    m = 1u << fm1_press_pin;
    if (FM1_ADC_CON & 0x80u)
        v = (int32_t)(FM1_ADC_RES & 0x3FFu);
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    FM1_PR(FM1_PA, FM1_DIR) |= m;                /* an input again, pulled up, before the rows are read */
    FM1_PR(FM1_PA, FM1_OUT) &= ~m;
    FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_PU) |= m;
    FM1_PR(FM1_PA, FM1_DIE) |= m;
    fm1_adc_isr = 0;
    if (v >= 0 && (fm1_in.notes >> fm1_press_key & 1u))
        fm1_press_raw[fm1_press_key] = (int16_t)v;
}

static void fm1_press_start(void)
{
    uint32_t c = fm1__tick_col, k, i, r, m;
    for (k = 0; k < 27u; k++)                    /* (let go: no reading) */
        if (!(fm1_in.notes >> k & 1u)) fm1_press_raw[k] = -1;
    if (fm1_adc_main || fm1_adc_isr || fm1_in.frames - fm1_press_frame < FM1_PRESS_EVERY)
        return;
    for (i = 0; i < 20u; i++) {                  /* the next held key on PA7 / PA8, from where the last left off */
        k = (fm1_press_next + i) % 20u;
        if (fm1_in.notes >> k & 1u) break;
    }
    if (i == 20u)
        return;
    for (r = 3; r < 5u; r++)                     /* (its row and column) */
        if (FM1_KEYMAP[r][c] == (int8_t)(14u + k)) break;
    if (r == 5u)
        return;                                  /* (not its column yet: a later tick) */
    m = 1u << (r == 3u ? 7 : 8);
    fm1__led_lines(0);
    FM1_PR(FM1_PA, FM1_DIE) &= ~m;               /* the row driven high (HD0) through the pill to the column */
    FM1_PR(FM1_PA, FM1_PU) &= ~m;
    FM1_PR(FM1_PA, FM1_PD) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_HD0) |= m;
    FM1_PR(FM1_PA, FM1_OUT) |= m;
    FM1_PR(FM1_PA, FM1_DIR) &= ~m;
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = 0x204Fu | (r == 3u ? 0u : 1u) << 8;   /* (~57 us: read in the next tick) */
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
    fm1_press_pin = (uint8_t)(r == 3u ? 7 : 8);
    fm1_press_key = (uint8_t)k;
    fm1_press_next = (uint8_t)((k + 1u) % 20u);
    fm1_press_frame = fm1_in.frames;
    fm1_adc_isr = 1;
}
