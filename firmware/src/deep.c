/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.6) Arctic Scream's DEEP source, on the screen "Luovos boazu" (North Sami: a free reindeer, one that roams loose,
 * not tied nor gathered into the fences with the herd; the name chosen by the user; Qvigstad's older Markasami notes give
 * a broader luovas, standard orthography in print): a drone from below, calm, drawn out, no howl. A voice is four sine generators that
 * play into each other in a ring, each drifting slowly on its own (after the idea of Caravel's DRONE, bonszaimusic's
 * GPL-3.0 FM-1 firmware, itself after the coupled oscillators of a LYRA-8; written anew, none of its code):
 *
 *   four sines (the note, and below it as DEPTH turns) -> each phase-moved by the one before it (COUPLE) -> rounded
 *   (COUPLE: sine .. a soft cubic) -> x its own slow breath -> summed -> two one-pole low-passes (TONE)
 *
 *   DEPTH    the four's tuning: 0 all at the note (a slow chorus) .. 42 an octave under and a fifth over .. 85 two
 *            octaves under .. 127 two under and a minor second over the note (the dark rub); between, they glide.
 *            A generator under ~22 Hz plays an octave up (nothing below hearing)
 *   COUPLE   0 four pure sines .. 127 each phase-moved up to ~0.45 of a turn by its neighbour, rounder and rougher:
 *            warmth and growl, never a howl (the move is bounded; nothing feeds back into the pitch)
 *   DRIFT    each generator's slow wander, its own rate (~24 .. ~10 s round): up to ~12 cents, and its level breathing
 *            down to half: the sound turns over by itself
 *   TONE     the two low-passes, ~190 Hz (a far rumble) .. ~6 kHz
 * The state in the voice: ph[0..3] the phases; ph[4..7] each one's last output (low 16 bits) and its drift's phase (high
 * 16); s[0], s[1] the low-passes. Fixed point, no tables of its own (sine_i, formant.c's). */

#define DP_LV 12000                             /* a voice's level (Q15: its four sines and their subs in a chord, under SPACE's knee) */
#define DP_MIN ((uint32_t)((22ull << 32) / FS)) /* a generator's lowest (~22 Hz) */
static const uint16_t DP_RAT[4][4] = {          /* DEPTH's four tunings, each generator's ratio to the note (Q12) */
    {4096, 4096, 4096, 4096},                   /* the note, four times (their detunes apart) */
    {2048, 4096, 4096, 6144},                   /* an octave under, the note twice, a fifth over */
    {1024, 2048, 4096, 6144},                   /* two octaves under, one, the note, a fifth over */
    {1024, 2048, 4096, 4369}};                  /* two under, one, the note, a minor second over (16 / 15) */
static const int8_t DP_DET[4] = {-5, 2, -2, 5}; /* their own detunes (tune_f units: ~2 cents) */

static void dp_begin(voice_t *v)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        v->ph[k] = k * 0x3C6EF372u;             /* (apart from the start: no click of four in phase) */
        v->ph[4u + k] = (k * 0x4000u + 0x1000u) << 16;   /* (their drifts a quarter apart, outputs 0) */
        v->s[k] = 0;
    }
}

static int32_t dp_tri(uint32_t ph16)            /* a triangle, -32768 .. 32767, a turn over 65536 */
{
    int32_t x = (int32_t)(ph16 & 0xFFFFu);
    return x < 32768 ? x * 2 - 32768 : 98303 - x * 2;
}

static void dp_block(voice_t *v, const volatile int32_t *p, int32_t *mono, int32_t amp0, int32_t amp1)
{
    int32_t dep = clamp(p[0], 0, 127), cpl = clamp(p[1], 0, 127), dft = clamp(p[2], 0, 127), ton = clamp(p[3], 0, 127);
    int32_t pos = dep * 3 * 256 / 127, si = pos >> 8, sf = pos & 255, dmp = amp1 - amp0, i, k;
    int32_t pm = cpl * 464, ch = cpl * 258, b = clamp(900 + ton * ton * 18 / 10, 900, 30000);
    int32_t g[4], y[4], l0 = v->s[0], l1 = v->s[1];
    uint32_t inc[4], base = vinc(VP16(v)), ph[4];
    if (si > 2) si = 2, sf = 256;
    for (k = 0; k < 4; k++) {
        uint32_t d = v->ph[4 + k] >> 16;
        int32_t r = DP_RAT[si][k] + ((DP_RAT[si + 1][k] - DP_RAT[si][k]) * sf >> 8), t, u;
        uint32_t f = (base >> 12) * (uint32_t)r + ((base & 4095u) * (uint32_t)r >> 12);   /* (base x r, no 64-bit
                                                 * multiply: tools/check_mul64.py) */
        d = (d + 2u + (uint32_t)k) & 0xFFFFu;   /* (the drift: ~0.04 .. ~0.1 Hz, each its own) */
        t = dp_tri(d), u = dp_tri(d + 16384u);
        if (f > 0x60000000u) f = 0x60000000u;
        f += (uint32_t)((int32_t)(f >> 12) * (DP_DET[k] + t * dft / 32768 * 28 / 127));   /* (its detune, its wander:
                                                 * ~12 cents) */
        inc[k] = f;
        while (inc[k] && inc[k] < DP_MIN) inc[k] <<= 1;
        g[k] = 32767 - (((u + 32768) * dft) >> 9);   /* (its breath: down to half) */
        y[k] = (int16_t)(v->ph[4 + k] & 0xFFFFu);
        v->ph[4 + k] = d << 16;
        ph[k] = v->ph[k];
    }
    for (i = 0; i < (int32_t)CTL; i++) {
        int32_t sum = 0, x[4];
        for (k = 0; k < 4; k++) {
            int32_t a;
            ph[k] += inc[k];
            a = sine_i(ph[k] + (uint32_t)(y[(k + 3) & 3] * pm));   /* (COUPLE: moved by the one before it) */
            a += (((3 * a - (((a * a) >> 15) * a >> 15)) / 2 - a) * ch) >> 15;   /* (rounded toward 1.5 x - 0.5 x^3) */
            x[k] = a;
            sum += (a * g[k]) >> 15;
        }
        for (k = 0; k < 4; k++) y[k] = x[k];
        sum >>= 2;
        l0 += ((sum - l0) * b) >> 15;           /* TONE */
        l1 += ((l0 - l1) * b) >> 15;
        mono[i] += mulq15(mulq15(clamp(l1, -32767, 32767), amp_at(amp0, dmp, i)), DP_LV);
    }
    for (k = 0; k < 4; k++) v->ph[k] = ph[k], v->ph[4 + k] |= (uint16_t)y[k];
    v->s[0] = l0, v->s[1] = l1;
}

static void dp_text(const volatile int32_t *p, uint32_t k, char *s)
{
    static const char *const SET[4] = {"UNISON", "SUB, FIFTH", "TWO SUBS", "DARK RUB"};
    int32_t v = clamp(p[k], 0, 127);
    if (k == 0u) {
        int32_t pos = v * 3 * 256 / 127, n = (pos + 128) >> 8;
        if (pos - n * 256 > -40 && pos - n * 256 < 40) { put_s(s, SET[n]); return; }
    }
    if (k == 1u && v == 0) { put_s(s, "PURE"); return; }
    put_s(put_u(s, (uint32_t)(v * 100 / 127)), "%");
}

static const engine_t DEEP_ENGINE = {"Luovos boazu", {"DEPTH", "COUPLE", "DRIFT", "TONE"}, {127, 127, 127, 127},
                                     dp_begin, dp_block, dp_text};
