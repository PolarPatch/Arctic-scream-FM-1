/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1) A picture for every sound (after Demake's, Sequencist's, whose every sound has its landscape; written anew):
 * firmware/gen/pictures.h (tools/gen_pictures.py, from assets/pictures/: drawn by tools/draw_pictures.py, or the
 * user's own photographs), PIC_W x PIC_H cells of 16 colours each, run-length coded. Which one a sound shows: its own
 * (PIC_SND, by the factory sound's name), else its category's (PIC_CAT). HOME shows it as a card a moment while
 * PRESETS turns (ui.c pic_home); the VIBE shows it big (Arctic Scream VIBE's port/home.c).
 *
 * pic_rows decodes a picture a row at a time into a row of cells (no buffer for the whole of it: the FM-1's RAM is
 * short), each row to a function of the caller's. */
#include "pictures.h"

#define PC_CARD_MS 2500u                        /* the card on HOME this long after the last turn */
static uint32_t pc_until, pc_id;                /* main loop: HOME's card, until then; its picture */
static char pc_name[16];                        /* .. and the sound's name on it */

static void pic_rows(uint32_t id, void (*row)(uint32_t y, const uint8_t *cells))
{
    const uint8_t *z = PIC_DATA[id % NPIC];
    uint8_t cells[PIC_W];
    uint32_t y = 0, x = 0, n, i;
    while (y < PIC_H) {                         /* (a control byte: c + 1 bytes as they are, or the next one c - 0x7E
                                                 * times; a byte is two cells, the left in its low nibble) */
        uint32_t c = *z++, rep = c >= 0x80u;
        n = rep ? c - 0x7Eu : c + 1u;
        for (i = 0; i < n && y < PIC_H; i++) {
            uint32_t b = rep ? z[0] : z[i];
            cells[x++] = (uint8_t)(b & 15u), cells[x++] = (uint8_t)(b >> 4);
            if (x >= PIC_W) row(y++, cells), x = 0;
        }
        z += rep ? 1u : n;
    }
}

static uint32_t pic_of_track(uint32_t t)        /* the picture of track t's sound: its own, else its category's */
{
    char nm[24];
    uint32_t i;
    nm[0] = 0;
    trk_sname(t, nm);
    for (i = 0; i < NPIC_SND; i++)
        if (same(nm, PIC_SND[i].snd)) return PIC_SND[i].pic;
    return PIC_CAT[(uint32_t)brw_cat[t] % (sizeof PIC_CAT)];
}

static void pic_card(uint32_t t)                /* main loop (PRESETS turned): HOME shows track t's picture a moment */
{
    if (pc_mode != PC_CARD) return;             /* (SKY: HOME shows it always; OFF: never) */
    pc_id = pic_of_track(t);
    trk_sname(t, pc_name);
    pc_until = fm1_ms + PC_CARD_MS;
}
static int pic_card_on(void) { return (int32_t)(pc_until - fm1_ms) > 0; }
