/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* (2.2) HELP (SYSTEM 4): a knob turned, a line under the picture for a moment saying what it does (after HADAL-1's
 * CONTROL HELP TEXT, Polystellarflow's: written anew). The lines are short (26 characters at most: the picture's width
 * in the small font); a knob without one says nothing. The SOUND page's by its source. ui.c draws it (ui_help_bar). */

#define HL_MS 2500u                             /* how long a line stays */
static uint32_t hl_until;                       /* the line shown till then */
static const char *hl_txt;

static const char *const HELP_TXT[NPAGE][4] = {
    [PG_ENV] = {"HOW EACH NOTE STARTS", "HOW FAST IT FALLS", "THE LEVEL IT HOLDS", "HOW IT DIES AWAY"},
    [PG_ENV2] = {"ENVELOPE GOES ROUND, HELD", "EACH NOTE WAITS THIS LONG", "NOTES SPREAD L TO R", "EACH PLACE A LITTLE OFF"},
    [PG_LFO] = {"HOW FAST IT MOVES", "HOW MUCH, 0: OFF", "WHAT IT MOVES", "ITS SHAPE, CHAOS NEVER SAME"},
    [PG_DRONE] = {"SLOW RANDOM WALKS", "THE STORM, THE TENSION", "HOW LONG THE STORM RISES", "NOTES STAY AFTER THE KEY"},
    [PG_JIEHKKI] = {"HOW LONG THE PIECE PLAYS", "HOW MUCH MOVES IN IT", "THE ICE: CRACKS, DRIPS", "ON: IT PLAYS BY ITSELF"},
    [PG_RUOHCAT] = {"HOW OFTEN THE ICE SOUNDS", "WHICH OF YOUR SAMPLES", "HOW MANY ARE YOUR SAMPLES", "SHORTER, SHIFTED, MOVED"},
    [PG_JAHKI] = {"HOW LONG A WHOLE YEAR IS", "WHERE THE YEAR IS NOW", "HOW FAR SEASONS LEAN", "SEASONS ON THE SCREENSAVER"},
    [PG_HOME] = {"THE TRACK'S LEVEL", "THE TRACK'S FILTER", "MORE LIFE, STILL TO WILD", "THE SOUND SLOWLY CHANGING"},
    [PG_MIX1] = {"THE TRACK'S LEVEL", "HOW MUCH INTO DELAY", "HOW MUCH INTO SPACE", "HOW MUCH INTO GRAIN"},
    [PG_MIX2] = {"LEFT OR RIGHT", "FILTER: LOW, HIGH, BAND", "FILTER BEFORE OR AFTER FX", "THE FILTER'S RING"},
    [PG_SPACE] = {"HOW BIG THE ROOM", "HOW DARK THE TAIL", "HOW MUCH REVERB", "A GAP BEFORE THE TAIL"},
    [PG_DELAY] = {"THE TIME BETWEEN ECHOES", "HOW MANY ECHOES", "ECHOES WEARING DARKER", "HOW MUCH DELAY"},
    [PG_GRAIN] = {"HOW LONG THE GRAINS", "HOW MANY GRAINS", "HOW FAR THEY SCATTER", "HOW MUCH GRAIN"},
    [PG_TAPE] = {"A SLOW PITCH WOBBLE", "TAPE WARMTH", "BRIGHTER OR DARKER", "TAPE NOISE"},
    [PG_AIR] = {"ECHOES LEFT AND RIGHT", "THE TAIL AN OCTAVE UP", 0, 0},
    [PG_MASTER] = {"THE WHOLE MIX'S FILTER", "ITS RING", "LOW, HIGH OR BAND", "WHERE IN THE CHAIN"},
    [PG_DRIVE] = {"HOW HARD IT IS DRIVEN", "THE KIND OF DRIVE", "BRIGHTER OR DARKER", "HOW MUCH OF IT"},
    [PG_SCALE] = {"THE SCALE'S ROOT", "WHICH NOTES THE KEYS PLAY", "PURE TUNINGS FOR DRONES", 0},
    [PG_ARP] = {"THE NOTES' ORDER", "HOW FAST", "OVER HOW MANY OCTAVES", "HOW LONG EACH NOTE"},
    [PG_SYS4] = {"THE SPEAKER'S OWN SOUND", "THESE LINES, ON OR OFF", 0, 0},
};
static const char *help_sound(uint32_t e, uint32_t k)   /* the SOUND page's, by its source (0: none) */
{
    static const char *const JN[4] = {"BOWED .. STRUCK", "HOW LONG IT SINGS", "STRING OR HOLLOW PIPE", "GLASS, BELL, METAL"};
    static const char *const BG[4] = {"HOW HARD IT GUSTS", "THE WIND'S WHISTLE", "A FAR STORM .. HISS", "SNOW TICKING"};
    static const char *const HW[4] = {"HOW HARD IT HOWLS", "THE LOOP'S BODY", "HOW DARK", "HOW HARD IT IS DRIVEN"};
    return e == ENG_JIEKNA ? JN[k & 3u] : e == ENG_BIEGGA ? BG[k & 3u] : e == ENG_HOWL ? HW[k & 3u] : 0;
}
static void help_knob(uint32_t pg, uint32_t k)  /* knob k + 1 turned on page pg: its line, if HELP is on */
{
    const char *t;
    if (!help_on || pg >= NPAGE || k > 3u) return;
    t = pg == PG_SOUND ? help_sound(sel_eng(), k) : HELP_TXT[pg][k];
    if (!t) return;
    hl_txt = t, hl_until = fm1_ms + HL_MS;
}
/* (2.2) the guide at the very first start (after HADAL-1's first steps, written anew): three pages, any key or button
 * the next, the last one gone. On the FM-1 only (the host's tests, the browser and VIBE have none: VIBE its welcome) */
#define GD_N 3u
static uint32_t gd_page;                        /* 1 .. GD_N: showing; 0 none */
static const char *const GD_TXT[GD_N][6] = {
    {"WELCOME TO", "ARCTIC SCREAM", "", "A DRONE MACHINE", "FOR THE FM-1", "ANY KEY: ON"},
    {"HOLD A LOW KEY:", "ICE WRAITH HOWLS", "", "TURN PRESETS", "FOR OTHER SOUNDS", "ANY KEY: ON"},
    {"LFO THREE TIMES:", "JIEHKKI, A PIECE", "THAT PLAYS ITSELF", "", "HOLD HOME: SYSTEM", "ANY KEY: PLAY"},
};
static int gd_take(uint32_t pressed)            /* a press while the guide shows: the next page (1: taken) */
{
    if (!gd_page || !pressed) return 0;
    gd_page = gd_page < GD_N ? gd_page + 1u : 0u;
    return 1;
}
static int help_up(void) { return hl_txt && fm1_ms - hl_until > 0x80000000u; }   /* (a line showing) */
