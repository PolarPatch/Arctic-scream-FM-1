/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* Arctic Scream 1.5.2: the update loader's screen (loader.c; tests/ldr_screen_test.c). The panel stayed black while
 * the loader waited and wrote: now ARCTIC SCREAM, UPDATE MODE and what it waits for, then WRITING with a bar and the
 * per cent, DONE, or what went wrong and what to do. After Skiff's loader screen (illuminus5/skiff,
 * firmware/loader/ldr_screen.c, GPL-3.0: the idea, its moments and drawing before USB starts), written anew: the
 * app's own pixfont (Press Start 2P), the start screen's colours, none of its code.
 * Everything here is in the loader's RAM image: the app area may be half written, so nothing is read from it. Drawn
 * before USB starts (USB is polled: the panel's init would leave the host's enumeration unanswered), then only the
 * lines that change. */
#include "grimm_pixfont.h"

#define LS_BG 0x0022u                   /* the start screen's night, #000410 */
#define LS_INK 0xFFFFu
#define LS_ICE 0x269Du                  /* #22D3EE */
#define LS_DIM 0x2104u                  /* the bar's track */
#define LS_BAR_X 30u
#define LS_BAR_Y 186u
#define LS_BAR_W 180u
#define LS_BAR_H 8u
#define LS_BODY_Y 112u                  /* three lines of 16 px, 20 apart */

static const uint16_t LS_STRIPE[4] = {0x269Du, 0x3DCAu, 0xA45Fu, 0xF8D1u};   /* ice, aurora, violet, plasma */

static uint32_t ls_width(const char *t, uint32_t s)
{
    uint32_t n = 0;
    while (t[n])
        n++;
    return n * 8u * s;
}
static void ls_text(uint32_t y, uint32_t s, uint16_t c, const char *t)   /* centred, s x s px a dot */
{
    uint32_t w = ls_width(t, s), x = w < 240u ? (240u - w) / 2u : 0u, r, i, b;
    for (; *t && x + 8u * s <= 240u; t++, x += 8u * s) {
        const uint8_t *g = PIXFONT[*t >= 32 && *t <= 126 ? *t - 32 : 0];
        for (r = 0; r < 8u; r++)
            for (i = 0; i < 8u; i = b) {        /* a row's runs of dots, one fill each */
                while (i < 8u && !(g[r] << i & 0x80u))
                    i++;
                for (b = i; b < 8u && (g[r] << b & 0x80u); b++)
                    ;
                if (b > i)
                    lcd_fill(x + i * s, y + r * s, (b - i) * s, s, c);
            }
    }
}
static void ls_body(const char *a, const char *b, const char *c)   /* the three lines under the title */
{
    lcd_fill(0, LS_BODY_Y, 240, 3u * 20u, LS_BG);
    ls_text(LS_BODY_Y, 2, LS_INK, a);
    ls_text(LS_BODY_Y + 20u, 2, LS_INK, b);
    ls_text(LS_BODY_Y + 40u, 2, LS_INK, c);
}
static void ls_bar_off(void) { lcd_fill(0, LS_BAR_Y, 240, 30, LS_BG); }

static void ldr_screen(void)                       /* UPDATE MODE: waiting for the installer */
{
    uint32_t k;
    lcd_init();
    lcd_fill(0, 0, 240, 240, LS_BG);
    ls_text(44, 2, LS_ICE, "ARCTIC SCREAM");
    for (k = 0; k < 4u; k++)
        lcd_fill(42u + k * 42u, 70, 30, 2, LS_STRIPE[k]);
    ls_text(84, 1, LS_ICE, "UPDATE MODE");
    ls_body("WAITING FOR", "THE INSTALLER", "KEEP USB IN");
    lcd_sync();
}

static uint32_t ls_bar_w, ls_pct = 101u;
static uint8_t ls_writing;
static void ldr_bar(uint32_t done, uint32_t total)  /* WRITING and the bar, as the app area is written */
{
    uint32_t w = done * LS_BAR_W / total, p = done * 100u / total;   /* (the app area: well below 2^32 / 180) */
    char s[6];
    if (!ls_writing || w < ls_bar_w) {             /* a write starts (again: a pass run once more) */
        ls_body("WRITING", "DO NOT SWITCH", "OFF OR UNPLUG");
        ls_bar_off();
        lcd_fill(LS_BAR_X, LS_BAR_Y, LS_BAR_W, LS_BAR_H, LS_DIM);
        ls_bar_w = 0, ls_pct = 101u, ls_writing = 1;
    }
    if (w > ls_bar_w) {
        lcd_fill(LS_BAR_X + ls_bar_w, LS_BAR_Y, w - ls_bar_w, LS_BAR_H, LS_ICE);
        ls_bar_w = w;
    }
    if (p != ls_pct) {                             /* "42 %" under the bar */
        uint32_t n = 0;
        if (p >= 100u)
            s[n++] = '1', s[n++] = '0', s[n++] = '0';
        else {
            if (p >= 10u)
                s[n++] = (char)('0' + p / 10u);
            s[n++] = (char)('0' + p % 10u);
        }
        s[n++] = ' ', s[n++] = '%', s[n] = 0;
        lcd_fill(0, LS_BAR_Y + 14u, 240, 8, LS_BG);
        ls_text(LS_BAR_Y + 14u, 1, LS_ICE, s);
        ls_pct = p;
    }
    lcd_sync();
}
enum { LS_DONE, LS_FLASH, LS_FAILED };
static void ldr_result(uint32_t what)              /* the session's end: what happened, what to do */
{
    if (what == LS_DONE)
        ls_body("DONE", "RESTARTING", "");
    else if (what == LS_FLASH)
        ls_body("UNKNOWN FLASH", "NOT INSTALLED", "RESTARTING");
    else {
        ls_body("UPDATE FAILED", "INSTALL AGAIN", "KEEP USB IN");
        ls_bar_off();
    }
    ls_writing = 0;
    lcd_sync();
}
