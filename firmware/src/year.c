/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.2) JAHKI (North Sami: a year; the words here from Giellatekno's dictionaries, satni.uit.no): the Arctic's year
 * turning slowly under the music, through the Sami year's eight seasons (jagiaigi), each named for how the weather and
 * the land behave, each leaning the sound a little (after PaperLagoon's slow cycles, hacha's, that keep its music
 * moving; written anew, its cycles the year's own):
 *   DALVI winter (the polar night: dark, SPACE long), GIDDADALVI spring-winter (crust and glare: brighter), GIDDA
 *   spring, GIDDAGEASSI early summer, GEASSI summer (the midnight sun: open, bright, SHIMMER rising), CAKCAGEASSI late
 *   summer, CAKCA autumn (rain and fog: lower, SHIMMER less), CAKCADALVI early winter (darker, SPACE growing).
 * The JAHKI page (LFO, tapped a fifth time):
 *   YEAR       STILL, 8, 24 MIN, 1, 2, 8, 24 HOURS: a whole year's time (STILL: SEASON stays)
 *   SEASON     where the year is now; turned, the year is there (it goes on from it)
 *   DEPTH      0 .. 100 %: how far the seasons lean the sound (0: not at all, the year only turns)
 *   SKY        OFF, ON: HOME's sky the season's picture and its name (pictures.c), whatever PICTURE says
 * What leans: the tracks' filters (with JIEHKKI's moves, jiehkki.c's jk_glide), SPACE's SIZE (hall.c's rv_lean: SIZE
 * as set, its tail longer or shorter), SHIMMER (rv_shq: as set, more or less of it; 0 stays 0). Between two seasons,
 * between their leans. Never the knobs or the project: a year at DEPTH 0 is the sound as it was. The project keeps
 * the four and where the year is (store.c, version 10): where it was when the project was last saved for another
 * reason, or SEASON turned (yr_kept): the year moving on is never a change to save (the flash would wear). Main loop:
 * yr_tick, from jk_tick. */
static const uint16_t YR_MIN[YR_NLEN] = {0, 8, 24, 60, 120, 480, 1440};   /* YEAR's minutes (0: STILL) */
static const char *const YR_LEN_NAME[YR_NLEN] = {"STILL", "8 MIN", "24 MIN", "1 HOUR", "2 HOURS", "8 HOURS", "24 HOURS"};
static const char *const YR_NAME[YR_NS] = {"DALVI", "GIDDADALVI", "GIDDA", "GIDDAGEASSI", "GEASSI", "CAKCAGEASSI",
                                           "CAKCA", "CAKCADALVI"};
static const char *const YR_SHORT[YR_NS] = {"DALVI", "G.DALV", "GIDDA", "G.GEAS", "GEASSI", "C.GEAS", "CAKCA", "C.DALV"};
static const char *const YR_SAY[YR_NS] = {"winter", "spring winter", "spring", "early summer", "summer", "late summer",
                                          "autumn", "early winter"};
static const char *const YR_PIC[YR_NS] = {"tundra_aurora", "blowing", "icicles", "fjord", "midnight_sun", "vidda",
                                          "lighthouse", "whiteout"};   /* (its picture, by pictures.h's name) */
static const int8_t YR_LEAN[YR_NS][3] = {      /* a season's lean at DEPTH 100 %: the filters (CUTOFF steps), SPACE's
                                                 * SIZE (steps), SHIMMER (/ 16) */
    {-10, 14, 16}, {4, 6, 21}, {6, 0, 16}, {10, -4, 19}, {14, -8, 26}, {6, -2, 19}, {-4, 4, 10}, {-8, 10, 13}};

static uint32_t yr_ph, yr_t, yr_seen = 0xFFu;   /* the year now (2^32 a year, a season its top 3 bits); the last
                                                 * pass; SEASON as last set by the year */
static int32_t yr_cut;                          /* the filters' lean (CUTOFF << 8: jiehkki.c's jk_glide adds it) */
static uint32_t yr_kept;                        /* where the year is, as the project keeps it (256 a year) */

static void yr_at(uint32_t pos8)                /* (store.c) a project loaded: the year where it was (256 a year) */
{
    yr_ph = (pos8 & 255u) << 24;
    yr_season = (int32_t)(yr_ph >> 29), yr_seen = (uint32_t)yr_season, yr_kept = pos8 & 255u;
}
static uint32_t yr_pos8(void) { return yr_kept; }   /* (store.c) where the project keeps it */
static void yr_keep(void) { yr_kept = yr_ph >> 24; }   /* (store.c's autosave, saving anyway) where it is now */

static void yr_tick(uint32_t now)               /* the year on; the leans from where it is */
{
    uint32_t dt = now - yr_t, L = (uint32_t)YR_MIN[clamp(yr_len, 0, YR_NLEN - 1)] * 60000u, s, n, f, d;
    int32_t c, z, h;
    yr_t = now;
    if ((uint32_t)clamp(yr_season, 0, YR_NS - 1) != yr_seen)   /* (SEASON turned: the year there, its start) */
        yr_ph = (uint32_t)clamp(yr_season, 0, YR_NS - 1) << 29, yr_keep();
    if (L && dt < 1000u) yr_ph += (uint32_t)((uint64_t)dt * 0x100000000ull / L);
    s = yr_ph >> 29, n = (s + 1u) % YR_NS, f = yr_ph >> 21 & 255u;   /* (this season, the next, how far: 0 .. 255) */
    yr_season = (int32_t)s, yr_seen = s;
    d = (uint32_t)clamp(yr_depth, 0, 100);
    c = YR_LEAN[s][0] * 256 + (YR_LEAN[n][0] - YR_LEAN[s][0]) * (int32_t)f;   /* (Q8, between the two) */
    z = YR_LEAN[s][1] * 256 + (YR_LEAN[n][1] - YR_LEAN[s][1]) * (int32_t)f;
    h = YR_LEAN[s][2] * 256 + (YR_LEAN[n][2] - YR_LEAN[s][2]) * (int32_t)f;
    yr_cut = c * (int32_t)d / 100;
    rv_lean = z * (int32_t)d / 25600;
    rv_shq = 16 + (h - 16 * 256) * (int32_t)d / 25600;
}
