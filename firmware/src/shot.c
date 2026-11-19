/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.1.4) ONE-SHOTS: hold GLO and press one of the 19 keys GLO's mutes and solos leave free (the black keys F#2 ..
 * D#3 and every key from F3 up): a short gesture plays once over the music, 1 .. 8 s, and fades by itself. The idea
 * (a menu of melodic effects on the keys, played once and gone) follows Polystellarflow's ZENITH-1 (MPL-2.0), written
 * anew; the gestures are my own.
 * A gesture plays on the selected track, with its own sound: notes of the SCALE (MINOR PENTATONIC with SCALE OFF) up
 * from its ROOT near C4 (OCT moves it), each with a velocity of its own (voices.c's gen_vel, as GENERATE's) and a
 * length, and the quickest attack; a SAMPLE track plays its sample on them. They take the track's voices as GENERATE's notes do. One at a time:
 * a new one takes the old one's place (its notes let go). Nothing saved.
 * The main loop (shot_poll, every pass) times the notes and asks the audio ISR (NQ_SHON, NQ_SHOFF: ctl_hook) to play
 * them as notes of their own (lat_id SHOT_ID ..: past GENERATE's). */

#define SHOT_ID 208u                            /* the notes' origins: SHOT_ID .. + SHOT_NV - 1 */
#define SHOT_NV 16u
#define SHOT_NEV 40u
#define NSHOT 19u
static const char *const SHOT_NAME[NSHOT] = {"SNOWFALL", "STRUM", "RISE", "CASCADE", "ARP", "RAINDROPS", "ECHO NOTE",
    "FIFTHS", "TWINKLE", "UNFOLD", "GLIMMER", "CALL AND REPLY", "METEOR", "BOUNCE", "TRILL", "LULLABY", "PEAL",
    "HARP", "CHIMES"};
static const uint8_t SHOT_KEY[NSHOT] = {1, 3, 5, 8, 10, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
static struct { uint16_t t, len; int8_t deg; uint8_t vel; } sh_ev[SHOT_NEV];
static uint32_t sh_n, sh_i, sh_t0, sh_trk, sh_base, sh_rng = 0x9E3779B9u;
static uint32_t sh_slot;                        /* the next note's origin (round the SHOT_NV) */
static uint32_t sh_off[SHOT_NV];                /* when each sounding one is let go (0: none) */
static volatile int8_t sh_now = -1;             /* the gesture playing (-1: none; the screen, the LEDs) */

static uint32_t sh_rnd(uint32_t n)
{
    sh_rng ^= sh_rng << 13, sh_rng ^= sh_rng >> 17, sh_rng ^= sh_rng << 5;
    return n ? sh_rng % n : 0u;
}
static uint32_t sh_mask(void) { return SC_MASK[sc_scale > 0 && sc_scale < NSCALE ? sc_scale : 12]; }
static uint32_t sh_per_oct(void)                /* the scale's notes an octave */
{
    uint32_t m = sh_mask(), n = 0;
    for (; m; m &= m - 1u) n++;
    return n ? n : 1u;
}
static uint32_t sh_note(int32_t deg)            /* deg notes of the scale from the base (its ROOT) */
{
    uint32_t m = sh_mask(), r = (uint32_t)clamp(sc_root, 0, 11);
    int32_t n = (int32_t)sh_base, s = deg < 0 ? -1 : 1;
    for (; deg; deg -= s)
        do n += s; while (!(m >> ((uint32_t)(n + 132 - (int32_t)r) % 12u) & 1u));
    return (uint32_t)clamp(n, 12, 120);
}
static void sh_add(uint32_t t, int32_t deg, uint32_t vel, uint32_t len)
{
    if (sh_n >= SHOT_NEV) return;
    len = len < 200u ? 200u : len;              /* (a note long enough to be heard on any source) */
    sh_ev[sh_n].t = (uint16_t)(t > 65535u ? 65535u : t), sh_ev[sh_n].len = (uint16_t)(len > 65535u ? 65535u : len);
    sh_ev[sh_n].deg = (int8_t)clamp(deg, -40, 40), sh_ev[sh_n].vel = (uint8_t)clamp((int32_t)vel, 1, 127);
    sh_n++;
}
static void sh_sort(void)                       /* (by time: a few, insertion) */
{
    uint32_t i, j;
    for (i = 1; i < sh_n; i++)
        for (j = i; j && sh_ev[j].t < sh_ev[j - 1u].t; j--) {
            __typeof__(sh_ev[0]) x = sh_ev[j];
            sh_ev[j] = sh_ev[j - 1u], sh_ev[j - 1u] = x;
        }
}
static void sh_build(uint32_t g)                /* gesture g's notes: time (ms), degree, velocity, length (ms) */
{
    uint32_t o = sh_per_oct(), beat = 60000u / (uint32_t)clamp(seq_bpm, 20, 300), i, t = 0;
    int32_t d, c[3] = {0, 2, 4};                /* (the triad on the root: its 1st, 3rd, 5th) */
    sh_n = 0;
    switch (g) {
    case 0:                                     /* SNOWFALL: soft high notes drifting down */
        for (i = 0, d = 2 * (int32_t)o + 4; i < 9u; i++, d -= 1 + (int32_t)sh_rnd(2)) sh_add(t, d, 40 + sh_rnd(30), 900), t += 280 + sh_rnd(420);
        break;
    case 1:                                     /* STRUM: down from high, then again softer */
        for (i = 0; i < 7u; i++) sh_add(i * 45u, c[(6u - i) % 3u] + (int32_t)o * (int32_t)((6u - i) / 3u), 100 - 6 * i, 2600);
        for (i = 0; i < 7u; i++) sh_add(1300u + i * 60u, c[(6u - i) % 3u] + (int32_t)o * (int32_t)((6u - i) / 3u), 60 - 4 * i, 2600);
        break;
    case 2:                                     /* RISE: up the scale, quickly */
        for (i = 0; i < 11u; i++) sh_add(i * 70u, (int32_t)i, 60 + 5 * i, 500);
        break;
    case 3:                                     /* CASCADE: down the scale */
        for (i = 0; i < 13u; i++) sh_add(i * 95u, 2 * (int32_t)o - (int32_t)i, 105 - 5 * i, 700);
        break;
    case 4:                                     /* ARP: up and down the triad, sixteenths at the tempo */
        for (i = 0; i < 16u; i++) {
            uint32_t k = i % 6u, s = k < 4u ? k : 6u - k;   /* (0 1 2 3 2 1) */
            sh_add(i * beat / 4u, c[s % 3u] + (int32_t)o * (int32_t)(s / 3u), 85 - (i & 3u ? 15 : 0) - i, beat / 5u);
        }
        break;
    case 5:                                     /* RAINDROPS: drops here and there */
        for (i = 0; i < 12u; i++) sh_add(sh_rnd(6000), (int32_t)o + (int32_t)sh_rnd(2 * o), 35 + sh_rnd(50), 150);
        break;
    case 6:                                     /* ECHO NOTE: one note echoing in time, fading */
        d = (int32_t)o + c[sh_rnd(3)];
        for (i = 0; i < 7u; i++) sh_add(i * beat * 3u / 4u, d, 110 - 15 * i, beat / 2u);
        break;
    case 7:                                     /* FIFTHS: climbing in fifths */
        for (i = 0; i < 5u; i++) sh_add(i * 320u, 4 * (int32_t)i, 70 + 8 * i, 1400);
        break;
    case 8:                                     /* TWINKLE: high notes of the triad, here and there */
        for (i = 0; i < 9u; i++) sh_add(sh_rnd(5000), 2 * (int32_t)o + c[sh_rnd(3)] + (int32_t)o * (int32_t)sh_rnd(2), 40 + sh_rnd(45), 400);
        break;
    case 9:                                     /* UNFOLD: the triad a note at a time, ringing on */
        for (i = 0; i < 5u; i++) sh_add(i * 550u, c[i % 3u] + (int32_t)o * (int32_t)(i / 3u), 75, 5200 - i * 550u);
        break;
    case 10:                                    /* GLIMMER: the triad at once, then higher, and higher */
        for (t = 0; t < 3u; t++)
            for (i = 0; i < 3u; i++) sh_add(t * 650u + i * 12u, c[i] + (int32_t)(o * t), 95 - 20 * t, 1800);
        break;
    case 11:                                    /* CALL AND REPLY: a phrase up high, answered lower */
        {
            int32_t p[4] = {4, 3, 5, 2};
            for (i = 0; i < 4u; i++) p[i] += (int32_t)sh_rnd(3) - 1;
            for (i = 0; i < 4u; i++) sh_add(i * 260u, (int32_t)o + p[i], 85, 380);
            for (i = 0; i < 4u; i++) sh_add(1700u + i * 300u, p[3u - i] - (i == 3u ? p[3] : 0), 70, i == 3u ? 1600 : 420);
        }
        break;
    case 12:                                    /* METEOR: a streak up three octaves, then one high note */
        for (i = 0; i < 3u * o; i++) sh_add(i * 22u, (int32_t)i - (int32_t)o, 40 + 2 * i, 120);
        sh_add(3u * o * 22u + 250u, 2 * (int32_t)o + c[sh_rnd(3)], 110, 3000);
        break;
    case 13:                                    /* BOUNCE: a note bouncing like a ball */
        for (i = 0, d = 600; i < 12u && d > 30; i++, d = d * 3 / 4) sh_add(t, (int32_t)o, 115 - 8 * i, 160), t += (uint32_t)d;
        break;
    case 14:                                    /* TRILL: two notes side by side, slowing down */
        for (i = 0, d = 70; i < 18u; i++, d += 9) sh_add(t, (int32_t)o + (int32_t)(i & 1u), 80 - 2 * i, (uint32_t)d), t += (uint32_t)d;
        break;
    case 15:                                    /* LULLABY: a slow melody, a step or two at a time */
        for (i = 0, d = (int32_t)o; i < 8u; i++, d += (int32_t)sh_rnd(5) - 2) sh_add(i * 480u, d, 70, i == 7u ? 1800 : 520);
        break;
    case 16:                                    /* PEAL: bells ringing down, the order changing */
        {
            uint8_t b[5] = {4, 3, 2, 1, 0};
            for (t = 0; t < 3u; t++) {
                uint32_t a = sh_rnd(4), x = b[a];
                b[a] = b[a + 1u], b[a + 1u] = (uint8_t)x;   /* (a pair swapped each round: change ringing) */
                for (i = 0; i < 5u; i++) sh_add(t * 1400u + i * 260u, (int32_t)o + b[i], 95 - 10 * t, 1200);
            }
        }
        break;
    case 17:                                    /* HARP: up the triad, two octaves */
        for (i = 0; i < 7u; i++) sh_add(i * 65u, c[i % 3u] + (int32_t)o * (int32_t)(i / 3u), 70 + 4 * i, 2400);
        break;
    default:                                    /* CHIMES: wind in the chimes, in bursts */
        for (i = 0; i < 14u; i++) {
            t = (i / 4u) * 1100u + sh_rnd(500);
            sh_add(t, (int32_t)o + c[sh_rnd(3)] + (int32_t)o * (int32_t)sh_rnd(2), 45 + sh_rnd(50), 1500);
        }
        break;
    }
    sh_sort();
}
static void shot_stop(void)                     /* the one playing: its notes let go */
{
    uint32_t i;
    for (i = 0; i < SHOT_NV; i++)
        if (sh_off[i]) nq_note(NQ_SHOFF, sh_trk, 0, SHOT_ID + i), sh_off[i] = 0;
    sh_n = sh_i = 0, sh_now = -1;
}
static void shot_play(uint32_t g, uint32_t t)   /* gesture g on track t, from now */
{
    int32_t b = 60 + 12 * set.oct, r = clamp(sc_root, 0, 11);
    shot_stop();
    sh_trk = t;
    sh_base = (uint32_t)(b - 6 + ((r - (b - 6)) % 12 + 12) % 12);   /* (the ROOT in the octave about C4 + OCT) */
    sh_build(g);
    sh_t0 = fm1_ms, sh_now = (int8_t)g;
    hd_say(SHOT_NAME[g]);
}
static int shot_key(uint32_t k)                 /* GLO held + key k: its gesture (0: not one of theirs) */
{
    uint32_t g;
    for (g = 0; g < NSHOT && SHOT_KEY[g] != k; g++)
        ;
    if (g == NSHOT) return 0;
    shot_play(g, sel);
    return 1;
}
static void shot_poll(uint32_t ms)             /* every main-loop pass: the notes due, the notes ending */
{
    uint32_t i, e = ms - sh_t0;
    for (i = 0; i < SHOT_NV; i++)
        if (sh_off[i] && (int32_t)(ms - sh_off[i]) >= 0) nq_note(NQ_SHOFF, sh_trk, 0, SHOT_ID + i), sh_off[i] = 0;
    for (; sh_i < sh_n && sh_ev[sh_i].t <= e; sh_i++) {
        uint32_t s = sh_slot, id = SHOT_ID + s;
        sh_slot = (sh_slot + 1u) % SHOT_NV;
        if (sh_off[s]) nq_note(NQ_SHOFF, sh_trk, 0, id);
        id_vel[id] = vel_gain(sh_ev[sh_i].vel);
        nq_note(NQ_SHON, sh_trk, sh_note(sh_ev[sh_i].deg), id);
        sh_off[s] = (ms + sh_ev[sh_i].len) | 1u;
    }
    if (sh_now >= 0 && sh_i >= sh_n) {         /* (all played: over once the last has ended) */
        for (i = 0; i < SHOT_NV && !sh_off[i]; i++)
            ;
        if (i == SHOT_NV) sh_now = -1;
    }
}
