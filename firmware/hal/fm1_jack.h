/* SPDX-License-Identifier: GPL-3.0-only
 * Arctic Scream p0larpatch (PolarPatch, 2026). The pin and its bounce after fm1-base (Henri David, MPL-2.0:
 * device/plat_hp.c), written anew for this hal. */
/* The headphone jack's switch: PB5, 0 with a plug in, 1 with nothing (measured there on an FM-1 with a probe of the
 * input registers). Only read: an input with its input enabled, its pulls as the boot left them. A plug bounces for
 * ~150 ms (several flips within 50 .. 150 ms), so a state counts once the pin has held it FM1_JACK_MS.
 *
 *   fm1_jack_init()      at start
 *   fm1_jack_poll(ms)    main loop, every pass
 *   fm1_jack             1 headphones in, 0 nothing plugged, -1 not read yet */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"

#define FM1_JACK_MS 100u
static volatile int8_t fm1_jack = -1;
static uint8_t fm1_jack_raw;
static uint32_t fm1_jack_t;

static void fm1_jack_init(void)
{
    FM1_PR(FM1_PB, FM1_DIR) |= 1u << 5;
    FM1_PR(FM1_PB, FM1_DIE) |= 1u << 5;
}

static void fm1_jack_poll(uint32_t ms)
{
    uint8_t r = (uint8_t)!(FM1_PR(FM1_PB, FM1_IN) >> 5 & 1u);   /* (1: a plug) */
    if (fm1_jack < 0)
        fm1_jack_raw = r, fm1_jack_t = ms, fm1_jack = (int8_t)r;
    else if (r != fm1_jack_raw)
        fm1_jack_raw = r, fm1_jack_t = ms;
    else if (r != (uint8_t)fm1_jack && ms - fm1_jack_t >= FM1_JACK_MS)
        fm1_jack = (int8_t)r;
}
