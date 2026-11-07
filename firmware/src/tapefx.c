/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GR!MM */
/* GR!MM's TAPE effect: the whole mix (the tracks and every effect's return) through a worn tape, the last stage before
 * MASTER. GR!MM's own, from DELAY's wear (wear.c: the wow and flutter, the saturation; the design after X0X's TAPE, see
 * tape.c):
 *   WOW         the read of a short line (~5 ms) moves with DELAY's wow (0.5 and 0.19 Hz, not quite periodic) and its
 *               flutter (6.1 Hz): up to ~+-2 % of pitch, a seasick transport (Arctic Scream 1.3: 3 x GR!MM's)
 *   SATURATION  DELAY's tanh, driven 1x .. 12x (1.3: was 4x), the level made up after it, blended in with the knob: 0 = clean. One
 *               curve (tf_curve), made by the main loop when the knob moves (tapefx_poll), crossed over to from the
 *               last within a block: no step in the sound, and nothing heavy in the audio ISR
 *   TONE        a one-pole low-pass, darker as it turns: open .. ~500 Hz (1.3: was ~1.8 kHz)
 *   HISS        noise (a draw every two frames: a little darker than white), under the low-pass (TONE darkens it
 *               too): up to ~ -29 dB rms (1.3: was -41)
 * All four at 0: bypassed, nothing runs. Turning on or off crosses over within a block (from the dry once the line has
 * filled, ~2.5 ms): no click. The stages that are at 0 are left out. The main loop sets the four and calls tapefx_poll;
 * the audio ISR calls tapefx_render on the mix, after SPACE. */

#define TF_LEN 512u                             /* the line, a side (a power of two) */
#define TF_BASE (240 << 8)                      /* the read, ~5.4 ms back (Q8 samples): the wow moves it around that */
#define TF_FILL 112u                            /* turning on: the dry alone this long, then the cross-over */
#define TF_NC 768u                              /* SATURATION's curve: x -98304 .. 98304 in steps of 256 */
#define TF_CQ 2                                 /* its values kept >> TF_CQ (int16: |y| <= |x| <= 98304, so <= 24576;
                                                 * the rounding: at most 2 in 98304, under one bit of the output) */

static int32_t tf_line[2][TF_LEN];
static int32_t tf_dry[2u * VB];                 /* (a block turning on or off: the mix as it came) */
static int16_t tf_curve[2][TF_NC + 1u];         /* SATURATION as one curve: drive, tanh, make-up, blend (tf_make): the
                                                 * one in use (tf.ci) and the next / the last */
static struct {
    uint32_t w, p1, p2, p3, rng, on;            /* the write index; the wow's phases; the hiss; on (the last block) */
    int32_t m, lpl, lpr, csat, ci;              /* the wow at the last block's end (Q8 samples); the low-pass, L R;
                                                 * the SATURATION the curve in use is for, and which one it is */
} tf;

/* the parameters, 0..127, written by the main loop (all 0: bypassed) */
static volatile int32_t tf_wow, tf_sat, tf_tone, tf_hiss;
static const uint16_t TF_TONE_HZ[128] = {   /* TONE's low-pass corner, Hz: -ln(1 - tc / 32768) x 44100 / 2 pi of the
                                                 * coefficient tapefx_render uses (0: off; 20000: at or above it, open) */
    0, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000,
    20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000,
    19747, 19318, 18897, 18492, 18097, 17710, 17338, 16973, 16616, 16271, 15931, 15601, 15278, 14964, 14655, 14354,
    14058, 13768, 13485, 13207, 12935, 12667, 12405, 12148, 11895, 11646, 11402, 11162, 10926, 10694, 10466, 10241,
    10020, 9802, 9588, 9377, 9169, 8964, 8762, 8563, 8367, 8173, 7982, 7793, 7608, 7424, 7243, 7064,
    6888, 6713, 6541, 6371, 6203, 6037, 5873, 5710, 5550, 5391, 5234, 5079, 4926, 4774, 4624, 4475,
    4328, 4183, 4039, 3897, 3755, 3616, 3478, 3340, 3205, 3071, 2937, 2806, 2675, 2546, 2418, 2291,
    2165, 2040, 1916, 1793, 1672, 1552, 1432, 1314, 1196, 1080, 964, 850, 736, 623, 511, 400};
static volatile uint32_t tf_pub, tf_ack;        /* main loop: + 1, the other curve made (for tf_psat); audio: = tf_pub,
                                                 * crossed over to it */
static volatile int32_t tf_psat;

/* a SATURATION curve into c: DELAY's tanh (wear.c), driven 1x .. 12x, made up, blended in with the knob (0: x itself) */
static void tf_make(int16_t *c, int32_t sat)
{
    int32_t g = 4096 + sat * 355, ig = (int32_t)(134217728u / (uint32_t)g), sw = sat * 129, x, y;   /* sw: Q14 */
    uint32_t j;
    for (j = 0; j <= TF_NC / 2u; j++) {         /* (odd: one half, mirrored) */
        x = (int32_t)(j << 8);
        y = x + (((wear_sat(x, g, ig) - x) * sw) >> 14);
        c[TF_NC / 2u + j] = (int16_t)clamp((y + (1 << (TF_CQ - 1))) >> TF_CQ, -32767, 32767);
        c[TF_NC / 2u - j] = (int16_t)-c[TF_NC / 2u + j];
    }
}
/* main loop: the curve for SATURATION as it is now, into the one not in use, once the last has been taken (first call:
 * the curve in use, x itself) */
static void tapefx_poll(void)
{
    static uint32_t made;
    int32_t sat = clamp(tf_sat, 0, 127);
    if (!made) {
        tf_make(tf_curve[0], 0);
        made = 1;
    }
    if (tf_ack != tf_pub || sat == tf_psat)
        return;
    tf_make(tf_curve[tf.ci ^ 1], sat);
    tf_psat = sat;
    tf_pub++;
}
static inline int32_t tf_shape(const int16_t *c, int32_t x)   /* x through curve c, between its points */
{
    int32_t a = clamp(x + (int32_t)(TF_NC << 7), 0, (int32_t)(TF_NC << 8) - 1), i = a >> 8;
    int32_t y0 = (int32_t)c[i] * (1 << TF_CQ), y1 = (int32_t)c[i + 1] * (1 << TF_CQ);
    return y0 + (((y1 - y0) * (a & 255)) >> 8);
}

/* the audio ISR: the mix (n frames, L R) in place */
static void tapefx_render(int32_t *out, uint32_t n)
{
    int32_t wow = clamp(tf_wow, 0, 127), sat = clamp(tf_sat, 0, 127), tone = clamp(tf_tone, 0, 127),
            hiss = clamp(tf_hiss, 0, 127);
    uint32_t on = (wow | sat | tone | hiss) != 0, i, w = tf.w, rng = tf.rng ? tf.rng : 0x9E3779B9u;
    int32_t glide, m1, D0, dD, hl = (hiss * hiss) >> 3, tc = 32767 - ((tone * tone * 1965) >> 10), l, r, shape;
    uint32_t fade = tf_pub != tf_ack, pub = tf_pub;
    const int16_t *c0, *c1;
    if (!on && !tf.on)
        return;                                 /* bypassed */
    if (n > VB)
        n = VB;
    if (on != tf.on) {                          /* turning on or off: the dry kept for the cross-over */
        for (i = 0; i < 2u * n; i++)
            tf_dry[i] = out[i];
        if (on) {                               /* (from an empty line, a clean wow) */
            for (i = 0; i < TF_LEN; i++)
                tf_line[0][i] = tf_line[1][i] = 0;
            tf.m = 0;
        }
    }
    if (fade) {                                 /* a new curve: crossed over to in this block */
        tf.ci ^= 1;
        tf.csat = tf_psat;
    }
    c1 = tf_curve[tf.ci];
    c0 = tf_curve[tf.ci ^ 1];
    shape = tf.csat || fade;

    /* the wow: the read glides over the block to this block's wow (wear.c; up to ~220 samples, Q8) */
    tf.p1 += DL_W1, tf.p2 += DL_W2, tf.p3 += DL_W3;
    m1 = wear_wow(tf.p1, tf.p2, tf.p3, wow * 161) * 3;
    D0 = TF_BASE + tf.m;
    dD = (TF_BASE + m1) - D0;
    tf.m = m1;
    glide = wow || dD || D0 != TF_BASE;
    {                                           /* WOW (through the line), SATURATION */
        int32_t *L0 = tf_line[0], *L1 = tf_line[1], acc = D0 * 256;
        for (i = 0; i < n; i++, acc += dD) {
            int32_t D = acc >> 8, yl, yr;
            uint32_t r0 = (w - (uint32_t)(D >> 8)) & (TF_LEN - 1u);
            L0[w] = out[2u * i];
            L1[w] = out[2u * i + 1u];
            w = (w + 1u) & (TF_LEN - 1u);
            yl = L0[r0], yr = L1[r0];
            if (glide) {                        /* (WOW 0, settled: the read stands still, on a whole sample) */
                uint32_t r1 = (r0 - 1u) & (TF_LEN - 1u);
                int32_t f = D & 255;
                yl += ((L0[r1] - yl) * f) >> 8;
                yr += ((L1[r1] - yr) * f) >> 8;
            }
            if (shape) {
                if (fade) {                     /* (the last curve to the new one over the block) */
                    int32_t k = (int32_t)i << 7, ol = tf_shape(c0, yl), orr = tf_shape(c0, yr);
                    yl = ol + mulq15(clamp(tf_shape(c1, yl) - ol, -65535, 65535), k);
                    yr = orr + mulq15(clamp(tf_shape(c1, yr) - orr, -65535, 65535), k);
                } else {
                    yl = tf_shape(c1, yl);
                    yr = tf_shape(c1, yr);
                }
            }
            out[2u * i] = yl;
            out[2u * i + 1u] = yr;
        }
    }
    tf.w = w;
    l = tf.lpl, r = tf.lpr;
    if (hiss || tone) {                         /* HISS (a draw a side for two frames: its top bits; a little darker
                                                 * than white), TONE (it darkens the hiss too) */
        int32_t hl0 = 0, hr0 = 0;
        for (i = 0; i < n; i++) {
            int32_t yl = out[2u * i], yr = out[2u * i + 1u];
            if (hiss) {
                if (!(i & 1u)) {
                    rng = rng * 1664525u + 1013904223u;
                    hl0 = (((int32_t)rng >> 16) * hl) >> 15;
                    rng = rng * 1664525u + 1013904223u;
                    hr0 = (((int32_t)rng >> 16) * hl) >> 15;
                }
                yl += hl0;
                yr += hr0;
            }
            if (tone) {                         /* the low-pass, Q15 (it may settle a step under 0: inaudible) */
                l += (clamp(yl - l, -65535, 65535) * tc) >> 15;
                r += (clamp(yr - r, -65535, 65535) * tc) >> 15;
                yl = l, yr = r;
            }
            out[2u * i] = yl;
            out[2u * i + 1u] = yr;
        }
    }
    if (!tone)                                  /* (TONE turned up later: from where the sound is) */
        l = out[2u * n - 2u], r = out[2u * n - 1u];
    tf.rng = rng, tf.lpl = l, tf.lpr = r;
    if (fade)
        tf_ack = pub;                           /* (the last curve free for the next) */

    if (on != tf.on) {                          /* the cross-over: on, from the dry once the line has filled; off, to it */
        int32_t k = on ? 0 : 32767, dk = on ? 32767 / (int32_t)(n > TF_FILL ? n - TF_FILL : 1u) : -32767 / (int32_t)n;
        for (i = 0; i < n; i++) {
            k = on && i < TF_FILL ? 0 : clamp(k + dk, 0, 32767);   /* (a running ramp: (i - TF_FILL) x 32767 would be a
                                                                     * constant the build takes for a ROM address) */
            out[2u * i] = tf_dry[2u * i] + mulq15(clamp(out[2u * i] - tf_dry[2u * i], -65535, 65535), k);
            out[2u * i + 1u] = tf_dry[2u * i + 1u] + mulq15(clamp(out[2u * i + 1u] - tf_dry[2u * i + 1u], -65535, 65535), k);
        }
        tf.on = on;
    }
}
