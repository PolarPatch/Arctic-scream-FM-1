/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* The update loader's screen (firmware/loader/ldr_screen.c) on this computer: its moments drawn into a frame, saved
 * as build/host/ldr-<n>.ppm, and checked: every line fits the panel, the bar fills to its end, a pass run again starts
 * the bar again, nothing is drawn off the panel.
 *   cc -O2 -Wall -Ifirmware/gen -o build/host/ldr_screen_test tests/ldr_screen_test.c && build/host/ldr_screen_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t fb[240 * 240];
static int fails, off_panel;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    if (x + w > 240u || y + h > 240u)
        off_panel++;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
static void lcd_init(void) {}
static void lcd_sync(void) {}
#include "../firmware/loader/ldr_screen.c"

static void save(const char *name)
{
    char path[64];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "build/host/ldr-%s.ppm", name);
    if (!(f = fopen(path, "wb"))) { perror(path); exit(1); }
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}
static void check(int ok, const char *what)
{
    if (!ok) { printf("ldr_screen FAILED: %s\n", what); fails++; }
}
static uint32_t lit(uint32_t y, uint16_t c)          /* the bar's row: how many px have colour c */
{
    uint32_t i, n = 0;
    for (i = 0; i < 240u; i++)
        n += fb[y * 240u + i] == c;
    return n;
}

int main(void)
{
    static const char *const LINES[] = {"ARCTIC SCREAM", "WAITING FOR", "THE INSTALLER", "KEEP USB IN", "WRITING",
        "DO NOT SWITCH", "OFF OR UNPLUG", "DONE", "RESTARTING", "UNKNOWN FLASH", "NOT INSTALLED", "UPDATE FAILED",
        "INSTALL AGAIN"};
    const uint32_t total = 0x93000u - 0x4000u, bar = LS_BAR_Y + 2u;
    uint32_t i, d;
    for (i = 0; i < sizeof LINES / sizeof *LINES; i++)
        check(ls_width(LINES[i], 2) <= 240u, LINES[i]);
    ldr_screen();
    save("wait");
    for (d = 0x1000u; d <= total / 2u; d += 0x1000u)
        ldr_bar(d, total);
    save("write");
    check(lit(bar, LS_ICE) == (total / 2u / 0x1000u) * 0x1000u * LS_BAR_W / total, "the bar at half");
    ldr_bar(0x1000u, total);                          /* a pass run again: the bar starts again */
    check(lit(bar, LS_ICE) == 0x1000u * LS_BAR_W / total, "the bar after a pass run again");
    for (d = 0x1000u; d <= total; d += 0x1000u)
        ldr_bar(d, total);
    check(lit(bar, LS_ICE) == LS_BAR_W, "the bar at the end");
    ldr_result(LS_DONE);
    save("done");
    ldr_result(LS_FAILED);
    save("failed");
    check(lit(bar, LS_ICE) == 0 && lit(bar, LS_DIM) == 0, "FAILED clears the bar");
    ldr_screen();
    ldr_result(LS_FLASH);
    save("flash");
    check(!off_panel, "nothing drawn off the panel");
    if (fails)
        return 1;
    printf("ldr_screen: the update loader's screen ok (build/host/ldr-*.ppm)\n");
    return 0;
}
