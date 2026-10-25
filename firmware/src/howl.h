/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PolarPatch */
/* the HOWL source's state, a voice's (howl.c): in the voice's PHYS slot (phys.c ph_slot, a union: a voice plays one source
 * at a time), no RAM of its own */
#define HW_SL 1024u                             /* the string's line: notes down to ~43 Hz (lower: an octave up) */
#define HW_BL 768u                              /* BODY's line: up to ~17 ms */
#define HW_A1 89u                               /* the diffusion's two all-passes (Audrey's reverb in the loop, small) */
#define HW_A2 131u

typedef struct {
    int16_t str[HW_SL], body[HW_BL], ap1[HW_A1], ap2[HW_A2];
    uint32_t sw;                                /* the string's write index (masked) */
    uint16_t bw, a1, a2;                        /* BODY's and the all-passes' */
    int32_t dcx, dcy, sl, lp, hl;               /* the string's DC block and damping; DAMP's low-pass; the high-pass */
    int32_t bd;                                 /* BODY's delay now, frames Q8 (gliding) */
    int32_t wd, wt;                             /* the wander on it: now, its target (Q8) */
    uint32_t wn, rng, kick;                     /* blocks to the wander's next target; the noise; a note's burst left */
} howl_t;
