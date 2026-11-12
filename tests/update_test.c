/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* The update path on this computer (2.0): the app's step (firmware/src/ota.c) and the update loader's
 * (firmware/loader/ldr_core.c) against a host in this file that serves a real package as tools/fm1_install.py does
 * (cmd 0x30 reads on the package's logical image, "success" at 0xE0000000 and 0xF0000000), on a flash of 1 MiB that
 * starts as another package's flash.bin, as if that firmware were installed. After Dinghy's update tests (zvenson/dinghy
 * tests/ota_test.c, ldr_test.c: the idea of serving a package to the code itself); written anew, none of their code.
 *   update_test OLD.img NEW.img      (logical images: tools/fm1_install.py's logical_image, run_host.sh makes them)
 * Checked:
 *   step 1   the package's loader staged at 0xE0000 as served, the update record at 0xE4F00, the restart asked;
 *            no "success" from the host: nothing committed, the staging erased; the app area never touched
 *   step 2   the app area as the new package's, the flash head and everything above it as before, the update records
 *            gone; a byte damaged on the way once: the pass runs again and the result is the same; no "success": the
 *            app written but the record kept (the host can resume); another flash chip: nothing written */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "FM-1_977"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000];                   /* the device's flash */
static uint8_t *img;                            /* the package being served (its logical image) */
static size_t img_len;
static uint32_t now_ms, reads, committed, rec_cleared, bad_writes, known = 1;
static uint32_t damage_at = 0xFFFFFFFFu, no_success;   /* serve one byte damaged once (its address); never "success" */
static uint32_t lo_ok = 0x4000u, hi_ok = 0x100000u;    /* where writes may go now */
static uint8_t reply[1200];                     /* the host's answer waiting (7-bit, between F0 and F7) */
static uint32_t reply_n;

static uint32_t t_pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--)
        for (acc |= (uint32_t)*in++ << nb, nb += 8u; nb >= 7u; nb -= 7u, acc >>= 7) out[o++] = (uint8_t)(acc & 0x7Fu);
    if (nb) out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}
static uint32_t t_unpack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--)
        for (acc |= (uint32_t)(*in++ & 0x7Fu) << nb, nb += 7u; nb >= 8u; nb -= 8u, acc >>= 8) out[o++] = (uint8_t)acc;
    return o;
}

/* ---- the host: a read request in, its answer out (as fm1_install.py's response()) */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], b[600];
    uint32_t addr, len, k, s = 0;
    if (n < 3u || p[0] != 0xF0 || p[n - 1u] != 0xF7 || t_unpack7(p + 1, n - 2u, u) != 15u || u[2] != 0x30) return 0;
    addr = (uint32_t)u[7] | (uint32_t)u[8] << 8 | (uint32_t)u[9] << 16 | (uint32_t)u[10] << 24;
    len = (uint32_t)u[11] | (uint32_t)u[12] << 8;
    if (addr == 0xE0000000u || addr == 0xF0000000u) {
        if (no_success) return 0;               /* (the host never confirms: the device's read times out) */
        len = 8;
    } else if (len > 512u || addr + len > img_len) {
        printf("update_test: the device asked for %#x + %u, outside the package\n", addr, len);
        exit(1);
    }
    b[0] = 0, b[1] = 0x59, b[2] = 0x30, b[3] = (uint8_t)(len + 8u), b[4] = (uint8_t)((len + 8u) >> 8), b[5] = 0, b[6] = u[6];
    for (k = 0; k < 4u; k++) b[7u + k] = (uint8_t)(addr >> (8u * k));
    b[11] = (uint8_t)len, b[12] = (uint8_t)(len >> 8), b[13] = 0;
    if (addr >= 0xE0000000u) memcpy(b + 14, "success", 8);
    else memcpy(b + 14, img + addr, len);
    if (damage_at >= addr && damage_at < addr + len) b[14u + damage_at - addr] ^= 0x5Au, damage_at = 0xFFFFFFFFu;
    for (k = 6; k < 14u + len; k++) s += b[k];
    b[14u + len] = (uint8_t)~s;
    reply_n = t_pack7(b, 15u + len, reply);
    reads++;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!reply_n) return 0;
    *p = reply, *n = reply_n;
    return 1;
}
static void ota_frame_done(void) { reply_n = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }

/* ---- the flash: NOR (a write clears bits), 4 KiB erases; a write outside [lo_ok, hi_ok) counted */
static int t_erase(uint32_t off)
{
    if (off & 0xFFFu || off >= sizeof nor) return -1;
    if (off < lo_ok || off >= hi_ok) bad_writes++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int t_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t k;
    if (off + n > sizeof nor) return -1;
    if (off < lo_ok || off + n > hi_ok) bad_writes++;
    for (k = 0; k < n; k++) nor[off + k] &= ((const uint8_t *)p)[k];
    return 0;
}
static int t_read(uint32_t off, void *p, uint32_t n)
{
    if (off + n > sizeof nor) return -1;
    memcpy(p, nor + off, n);
    return 0;
}
static int ota_erase(uint32_t off) { return t_erase(off); }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { return t_prog(off, p, n); }
static int ota_fread(uint32_t off, void *p, uint32_t n) { return t_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code) { (void)step, (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm, committed++; }
#include "../firmware/src/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n) { return t_read(off, p, n); }
static int ldr_erase(uint32_t off) { return t_erase(off); }
static int ldr_prog(uint32_t off, const void *p, uint32_t n) { return t_prog(off, p, n); }
static void ldr_record_clear(void) { rec_cleared++; }
static int ldr_flash_known(void) { return (int)known; }
static uint32_t prog_last, prog_total;
static void ldr_progress(uint32_t done, uint32_t total) { prog_last = done, prog_total = total; }
#include "../firmware/loader/ldr_core.c"

static int fails;
static void check(int ok, const char *what)
{
    printf("  %s  update: %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}
static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *p;
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END), *n = (size_t)ftell(f), fseek(f, 0, SEEK_SET);
    if (!(p = malloc(*n)) || fread(p, 1, *n, f) != *n) { perror(path); exit(1); }
    fclose(f);
    return p;
}
static int all_ff(uint32_t a, uint32_t n)
{
    while (n--) if (nor[a++] != 0xFFu) return 0;
    return 1;
}

static uint8_t *old_img, *new_img, before[sizeof nor];
static size_t old_len, new_len;
static uint32_t new_fl;                         /* the new package's flash.bin, in its image */
static void install_old(void)                   /* the flash as the old package left it, no update pending */
{
    uint8_t hdr[0x400];
    uint32_t fl_off, fl_len, o_off, o_len, crc;
    img = old_img, img_len = old_len;
    if (ota_ufw(hdr, &fl_off, &fl_len, &o_off, &o_len, &crc)) { printf("update_test: the old package's header\n"); exit(1); }
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, old_img + fl_off, fl_len < LDR_APP_HI ? fl_len : LDR_APP_HI);
    img = new_img, img_len = new_len, reply_n = 0, damage_at = 0xFFFFFFFFu, no_success = 0, known = 1;
    committed = rec_cleared = bad_writes = 0, ota_deadline = 0;   /* (ota_read's deadline: a session's own) */
}

int main(int argc, char **argv)
{
    uint8_t hdr[0x400], rec[112], rec0[112];
    uint32_t fl_len, o_off, o_len, crc, len;
    int rc;
    if (argc != 3) { fprintf(stderr, "usage: update_test OLD.img NEW.img\n"); return 2; }
    old_img = slurp(argv[1], &old_len), new_img = slurp(argv[2], &new_len);
    img = new_img, img_len = new_len;
    if (ota_ufw(hdr, &new_fl, &fl_len, &o_off, &o_len, &crc) || fl_len < LDR_APP_HI) { printf("update_test: the new package\n"); return 1; }

    /* step 1: the app stages the loader */
    install_old();
    memcpy(before, nor, sizeof nor);
    lo_ok = OTA_AREA, hi_ok = OTA_AREA + OTA_AREA_LEN;
    rc = ota_session();
    len = o_len - 0x20u;
    check(rc == 0 && committed == 1, "step 1: the package checked, the loader staged, the restart asked");
    check(!memcmp(nor + OTA_AREA + 32u, new_img + o_off + 0x20u, len), "step 1: the staged loader is the package's, byte for byte");
    t_read(OTA_RES, rec, sizeof rec);
    check(ota_rd16(rec + 6) == 0x5441u && ota_rd16(rec) == ota_crc16(rec + 2, 78, 0) && ota_rd32(rec + 72) == OTA_AREA,
          "step 1: the update record at 0xE4F00 (its CRC, the loader's place)");
    memcpy(rec0, rec, sizeof rec);
    check(!bad_writes && !memcmp(nor, before, OTA_AREA), "step 1: nothing written below the staging (the app, the saves)");

    install_old();
    lo_ok = OTA_AREA, hi_ok = OTA_AREA + OTA_AREA_LEN, no_success = 1;
    rc = ota_session();
    check(rc == -13 && !committed && all_ff(OTA_AREA, OTA_AREA_LEN),
          "step 1, no \"success\" from the host: nothing committed, the staging erased");

    /* step 2: the loader writes the app area */
    install_old();
    memcpy(before, nor, sizeof nor);
    lo_ok = LDR_APP_LO, hi_ok = LDR_REC_HI;
    t_prog(OTA_RES, rec0, sizeof rec0);         /* (the update record, as step 1 left it) */
    rc = ldr_session();
    check(rc == 0, "step 2: the loader's session ends with \"success\"");
    check(!memcmp(nor + LDR_APP_LO, new_img + new_fl + LDR_APP_LO, LDR_APP_HI - LDR_APP_LO),
          "step 2: the app area is the new package's");
    check(!memcmp(nor, before, LDR_APP_LO) && !memcmp(nor + LDR_REC_HI, before + LDR_REC_HI, sizeof nor - LDR_REC_HI),
          "step 2: the flash head and the saves above 0xFC000 as they were");
    t_read(OTA_RES, rec, 8);
    check(rec_cleared && all_ff(OTA_RES & ~0xFFFu, 0x1000), "step 2: the update records gone (RAM and flash): the new app boots");
    check(prog_last == prog_total && prog_total == LDR_APP_HI - LDR_APP_LO, "step 2: the progress reaches its end (the screen's bar)");
    check(!bad_writes, "step 2: no write outside the app area and the records");

    install_old();
    lo_ok = LDR_APP_LO, hi_ok = LDR_REC_HI, damage_at = new_fl + 0x20000u;
    rc = ldr_session();
    check(rc == 0 && !memcmp(nor + LDR_APP_LO, new_img + new_fl + LDR_APP_LO, LDR_APP_HI - LDR_APP_LO),
          "step 2, a byte damaged on the way once: the pass runs again, the app area right");

    install_old();
    lo_ok = LDR_APP_LO, hi_ok = LDR_REC_HI, no_success = 1;
    t_prog(OTA_RES, rec0, sizeof rec0);
    {
        uint8_t r[8];
        rc = ldr_session();
        t_read(OTA_RES, r, 8);
        check(rc == LDR_ENOFIN && !all_ff(OTA_RES, 8), "step 2, no \"success\": the record kept, the host can resume");
    }

    install_old();
    memcpy(before, nor, sizeof nor);
    lo_ok = hi_ok = 0, known = 0;
    rc = ldr_session();
    check(rc == LDR_EFLASH && !memcmp(nor + LDR_APP_LO, before + LDR_APP_LO, LDR_APP_HI - LDR_APP_LO),
          "step 2, another flash chip: the app area untouched");

    if (fails) return 1;
    printf("update_test: the update path ok (%u reads served)\n", reads);
    return 0;
}
