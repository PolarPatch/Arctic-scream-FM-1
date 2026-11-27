/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.9) The radio on Arctic Scream: Bluetooth MIDI and Wi-Fi (Ableton Link, the FM-1 as an access point) on
 * Optimist's own stacks (Henri David), as fm1-base carries them (Charles Vestal; MPL-2.0, Link GPL-3.0-only):
 * firmware/radio/ holds their files unchanged (radio/README.md says from where), this file wires them to Arctic
 * Scream's platform (platform.c: Dinghy's, from SLOOP / Felucca) in place of fm1-base's own (device/plat_fm1.c,
 * main_fm1.c). Nothing of JieLi's is linked; no task, no shim: the stacks run in the radio's two interrupts (priority
 * 2, under the audio's 3 and the timer's 4) and the main loop (radio_poll, from platform_poll).
 *   OFF until asked (radio_on): an FM-1 that never turns it on runs as before, the radio asleep
 *   the render is held (silence) for the first second of the radio's start: its calibration draws a lot of current
 *   and what is read from the flash meanwhile may come back wrong (fm1-base radio.c radio_quiet)
 *   what the radio keeps (its address; the networks: SSID and the WPA2 key, never the password; the access point's
 *   name and key) sits in the settings sector beside the settings: RS_BLE_AT, RS_WIFI_AT (settings_save keeps them) */

#define FM1B_BLE 1
#define FM1B_BLE_OWN 1
#define FM1B_BLE_CENTRAL 0
#define FM1B_WIFI 1
#define FM1B_WIFI_OWN 1
#define FM1B_LINK 1
#define FM1B_WIFI_AP 1
#define FM1_RF_WIFI_IQ 1
#define BLE_DEVICE_NAME "Arctic Scream"          /* "Arctic Scream XXXX": this FM-1's own four digits */
enum { OBJ_BLE = 1, OBJ_WIFI = 2 };
#define clamp plat_clamp                         /* (plat.h's own; dsp.c's is Arctic Scream's) */
#include "plat.h"
#undef clamp

/* ---- plat.h: what the stacks ask of the platform */
uint32_t plat_ticks24(void) { return fm1_ticks(); }
uint32_t plat_ms(void) { return fm1_ms; }
uint32_t plat_lock(void) { return irq_save(); }   /* (it nests: hal/fm1_flash.h) */
void plat_unlock(uint32_t f) { irq_restore(f); }
static uint8_t flash_uid[16];
static uint8_t flash_uid_ok;
const uint8_t *plat_chip_uid(void)
{
    if (!flash_ok) return 0;
    if (!flash_uid_ok) {
        uint32_t f = plat_lock();
        flash_uid_ok = FL_FAR(fl_uid_ram)(flash_uid) == 0;
        plat_unlock(f);
    }
    return flash_uid_ok ? flash_uid : 0;
}
int plat_dev_id(void) { return plat_uid_hash16(plat_chip_uid()); }
static uint32_t nw_ble_in_n, nw_ble_in_t;     /* (Bluetooth MIDI in: counted, its last time; net.c) */
uint32_t plat_midi_count(int port, int out) { return port == PORT_BLE && !out ? nw_ble_in_n : 0u; }
uint32_t plat_midi_ago(int port, int out) { return port == PORT_BLE && !out && nw_ble_in_n ? fm1_ms - nw_ble_in_t : 0xFFFFFFFFu; }

/* ---- the radio's records in the settings sector: [magic][length][crc32] then the bytes, each at its own place */
#define RS_BLE_AT 0x800u
#define RS_WIFI_AT 0xA00u
#define RS_END 0xC00u
#define RS_MAGIC 0x41524431u                    /* "ARD1" */
static uint8_t rs_keep[RS_END - RS_BLE_AT];     /* (settings_save: the records read before the erase, written after) */
static int rs_at(uint32_t obj) { return obj == OBJ_BLE ? (int)RS_BLE_AT : obj == OBJ_WIFI ? (int)RS_WIFI_AT : -1; }
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    uint32_t h[3];
    int at = rs_at(obj);
    if (!flash_ok || at < 0 || st_read(SETTINGS_AT + (uint32_t)at, h, sizeof h) || h[0] != RS_MAGIC || h[1] > max ||
        h[1] > 0x200u - sizeof h)
        return -1;
    if (st_read(SETTINGS_AT + (uint32_t)at + sizeof h, dst, h[1]) || crc32(dst, h[1]) != h[2])
        return -1;
    return (int)h[1];
}
static int settings_rewrite(const void *set, uint32_t n);
static uint8_t st_last[256];                    /* the settings as last loaded or saved (a radio record's save keeps them) */
static uint32_t st_last_n;
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    uint32_t h[3];
    int at = rs_at(obj);
    if (!flash_ok || at < 0 || len > 0x200u - sizeof h) return -9;
    if (st_read(SETTINGS_AT + RS_BLE_AT, rs_keep, sizeof rs_keep)) return -9;
    h[0] = RS_MAGIC, h[1] = len, h[2] = crc32(src, len);
    memset(rs_keep + (uint32_t)at - RS_BLE_AT, 0xFF, 0x200u);
    memcpy(rs_keep + (uint32_t)at - RS_BLE_AT, h, sizeof h);
    memcpy(rs_keep + (uint32_t)at - RS_BLE_AT + sizeof h, src, len);
    return settings_rewrite(st_last_n ? st_last : 0, st_last_n);
}

uint32_t nw_rx_prot, nw_rx_plain;              /* (wifi_own.c wl_rx: keyed, data frames with / without the Protected bit) */
/* ---- the stacks */
static void plat_yield_radio(void);
void plat_yield(void) { plat_yield_radio(); }
/* the radio's start: while it calibrates (radio_quiet, ~1 s) the timer only counts (platform.c fm1_timer5_irq) and the
 * render rests: Arctic Scream's 10 kHz timer fetched its code from the flash all the time, and a flash read under the
 * calibration's current came back wrong: a jump to 0xFDD0 inside fm1_rfc_adc_peak (the USB rescue). Masking the timer
 * (fm1-base's way for JieLi's Wi-Fi) stalls Arctic Scream's main loop, which waits on its milliseconds */
#include "radio.c"
#include "wifi_own.c"

/* ---- Arctic Scream's side */
static uint8_t radio_wanted;                    /* the app asked for it (radio_on) */
static void radio_poll(void)                    /* platform_poll, every pass */
{
    if (!radio_wanted) return;
    ble_poll();
    wifi_poll();
}
static void plat_yield_radio(void) { fm1_wdt_feed(); }   /* (a wait inside the stacks: the watchdog; the main loop polls) */
/* (2.1.9) MIDI out over the radio: everything USB-MIDI sends (midi_out_event: the main loop and the render), channel
 * messages and the realtime ones, to Bluetooth MIDI (a phone or a computer connected) and, once a session is up, to
 * MIDI over Wi-Fi (net.c nm_*). The lock keeps the two senders apart; the main loop drains both */
#define NO_RING 64u
static uint32_t no_buf[NO_RING];
static volatile uint32_t no_w, no_r;
static volatile uint8_t nm_live;                /* (net.c: a Wi-Fi MIDI session is up) */
static volatile uint8_t nw_ble_out;             /* (net.c: BLUETOOTH on) */
static void nw_out_tap(uint32_t pkt)
{
    uint32_t f;
    if (!radio_wanted || (pkt & 15u) < 8u) return;   /* (SysEx and the rest stay on USB) */
    f = irq_save();
    if (nw_ble_out) ble_midi_tap(pkt, TO_BLE_HOST);
    if (nm_live && no_w - no_r < NO_RING) {
        no_buf[no_w % NO_RING] = pkt;
        RING_PUBLISH();
        no_w++;
    }
    irq_restore(f);
}
static void radio_on(void)                      /* main loop: the radio started, once (Bluetooth MIDI; Wi-Fi as its mode says) */
{
    if (radio_wanted) return;
    radio_wanted = 1;
    radio_boot_note();
    ble_start();
}
