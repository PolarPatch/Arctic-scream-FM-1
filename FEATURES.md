# Arctic Scream p0larpatch: features

A complete overview of what Arctic Scream p0larpatch does, version 1.1. For the short version see the
[README](README.md); for every button on one page, the [cheat sheet](https://polarpatch.github.io/Arctic-scream-FM-1/cheatsheet.html);
for how it is built, [DEVELOPER.md](DEVELOPER.md).

## What it is

Arctic Scream p0larpatch is a free, open-source (GPL-3.0) drone firmware for the M-VAVE FM-1 by PolarPatch. It is built
on [GR!MM](https://github.com/GRiMMBIENT/grimm) and turns the FM-1 into an instrument for dark ambient drones and
feedback screams: slow, cold sounds that hum for minutes, and a voice that wakes on its own and howls.

| | |
| --- | --- |
| Version | 1.1 |
| Firmware identity | `FM-1_977` (USB name "Arctic Scream" by "PolarPatch") |
| Package | `arctic-scream-1.1.fwsc`, built and attested on GitHub |
| Source | <https://github.com/PolarPatch/Arctic-scream-FM-1> |
| Demo, editor, cheat sheet | <https://polarpatch.github.io/Arctic-scream-FM-1/> |
| Licence | GPL-3.0 (the PolarPatch logo excepted) |

## At a glance

- Four tracks, six voices shared between them.
- Eight sound sources, among them **Holvut**, a feedback loop that howls by itself.
- **DRONE**: a sound that lives on its own (BORGA, STOARBMA, GOHCCAT).
- 47 factory sounds with arctic names, and 64 slots for your own.
- Four effects (TAPE, GRAIN, DELAY, SPACE) and eight performance tricks.
- A 64-step sequencer per track, an arpeggiator, chords, scales and song mode.
- An arctic cyberpunk screen with moving northern lights.
- A web editor and a browser demo that run the same code as the instrument.

## The tracks and voices

- **Four tracks**, T1–T4, chosen with ALGORITHM. Every page shows the selected track.
- **Six voices** shared by all tracks. A seventh note takes the oldest one, faded out in ~10 ms.
- Each track has its own source, the settings of every source, ENV, LATCH, chord, arpeggiator, LFO, DRONE, filter, level,
  pan and three effect sends.
- **Keys** play the selected track. The first white key is **F2**: everything sits an octave down, for drones.
- **OCT− / OCT+** move the keys by octaves (−2 to +2). Notes already sounding keep their pitch.
- **LATCH ON**: a key starts its note, the same key again stops it.

## Sound sources (EDIT)

EDIT opens SOUND (the source's four knobs), then SOURCE (the source and LATCH). An FM track also has FM ALGO and FM OP.

| Source | K1 | K2 | K3 | K4 | What it is |
| --- | --- | --- | --- | --- | --- |
| VA | WAVE | DETUNE | CUTOFF | RESONANCE | Virtual analog: saw, square, supersaw |
| FM | PATCH | BRIGHT | DECAY | DETUNE | Six-operator FM, 32 algorithms, its own editable patch |
| SAMPLE | SLOT | START | LENGTH | REVERSE | Your own WAVs, four slots, loaded from the editor |
| VOICE | VOWEL | SHIFT | BUZZ | BREATH | A singing formant voice |
| WHEEL | DRAWBARS | PERC | CLICK | DRIVE | A tonewheel organ |
| PHYS | MODEL | STRUCTURE | DAMP | BOW | Physical models: modal, string, membrane, sympathetic strings |
| PHASE | WAVE | WAVE2 | DCW | DETUNE | Phase distortion |
| **Holvut** | FEEDBACK | BODY | DAMP | DRIVE | A feedback loop that howls (below) |

**FM ALGO**: ALGO 1–32, FEEDBACK, OP (the operator, 1–6), FINE. **FM OP**: that operator's RATIO, LEVEL, ATTACK,
DECAY.

### Holvut, the feedback source

*Holvut* is North Sami for "to howl". It is new in Arctic Scream, inspired by the Synthux Audrey II and written from
scratch for the FM-1.

- A string and a resonant body sit inside a loop with saturation, filters and diffusion. Nothing is struck: the loop
  hears only a breath of noise and what comes round.
- Below the threshold the loop sleeps. Past it (about two thirds of FEEDBACK) it wakes, grows out of silence and howls
  by itself. The saturation keeps it from blowing up.
- Each key sends a short burst into the loop, so a note played near the threshold provokes it.
- **FEEDBACK** (−60 to +12 dB) wakes it. **BODY** (1–17 ms) bends it, gliding when turned and wandering a little by
  itself. **DAMP** (18 kHz to 100 Hz) turns the scream into a growl. **DRIVE** (1× to 7×) tears it.
- The howl's pitch comes mostly from the loop itself, not the key, so OCT− and OCT+ do not move it by an octave.
  [DEVELOPER.md](DEVELOPER.md) describes a possible change.

## DRONE: a sound that lives on its own (LFO, tapped twice)

New in 1.1. The idea follows Jangada's DRONE page; it is written from scratch. The knob names are North Sami.

| Knob | Meaning | What it does |
| --- | --- | --- |
| **BORGA** 0–100 % | snowdrift | Four slow random walks move the sound: the track filter's cutoff, the source's shape, its most natural knob (Holvut: FEEDBACK around the threshold) and the reverb's distance. Each voice also drifts a few cents and breathes in level, so a held chord sounds like a choir. It never repeats. |
| **STOARBMA** 0–100 % | storm | The tension. It opens the filter, raises resonance and drive, spreads the voices up to ±12 cents, and makes the walks deeper and faster. |
| **GOHCCAT** OFF, 1–32 bars | to wake | How long the storm takes to reach STOARBMA, in tempo. A drone woken from silence starts calm and builds. |
| **LATCH** | | As on SOURCE. |

- At 0 % the sound is exactly as set: nothing is added.
- The settings are saved with the sound. All seven Holvut presets start with BORGA on and GOHCCAT at 8 bars.
- The screen shows the four walks as aurora ribbons and the storm as a bar with its target mark.

## ENV and LFO

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| ENV | ATTACK (~1 ms–10 s) | DECAY (~10 ms–10 s) | SUSTAIN (default 40 %) | RELEASE (~10 ms–20 s) |
| LFO | RATE (60 s a cycle to 20 Hz) | DEPTH | DESTINATION | SHAPE |

- **DESTINATION**: PITCH (±1 octave), VOLUME, PAN, the source's four knobs by name, REV SEND, GRAIN SEND, the track's
  cutoff.
- **SHAPE**: SINE, TRIANGLE, SAW UP, SAW DOWN, SQUARE, RANDOM, DRIFT.
- The LFO never restarts with a note, so drones keep breathing.

## Scales, chords and the arpeggiator

- **SEL → SCALE**: ROOT and SCALE, shared by all tracks. Keys, MIDI in and recordings snap to it. OFF and fourteen
  scales, the last of them **HORROR**: close semitones, every note wrong in the right way.
- **SEL → CHORD**: CHORD TYPE (SCALE, MIN, MAJ, SUS2, SUS4, FIFTH, MIN7, MAJ7, DIM, ADD9, CLUSTER) and VOICING (CLOSE,
  OPEN, SPREAD, +OCT, DROP). **Hold SEL** for chord mode on or off: one key, a whole chord.
- **ARP**: MODE (UP, DOWN, UP-DOWN, RANDOM, AS PLAYED, CHORD), RATE (1/1–1/32 and triplets), OCTAVES 1–4, GATE.
  **Hold ARP** for the arpeggiator on or off. It follows the sequencer's clock while it plays.

## Sequencer and song

- **SEQ → GRID**: 64 steps a track. Per step: NOTE, CHANCE, RATCHET, LENGTH.
- **SEQ → PATTERN SETTINGS**: STEPS 1–64, DIVISION (1/4–1/32 and triplets), SWING, PLAY MODE.
- **PLAY** starts and stops all four tracks together. **REC**: stopped, each key fills a step; playing, notes land on the
  nearest step.
- **Step locks**: hold SEL and turn a knob to lock a sound value to the selected step. SEL + OCT− clears them.
- **Hold SEQ + a white key**: CLEAR, RANDOM, SHIFT ←, SHIFT →, COPY, PASTE, DOUBLE, MUTATE (all undoable).
- **GLO → SONG**: a chain of up to 16 steps, each a slot A–D and its repeats; MODE OFF, SONG, SONG LOOP. REC there
  records the slots you switch to.
- **Hold GLO + a white key**: keys 1–4 mute T1–T4, keys 5–8 solo them.
- Tempo 20–300 BPM (SELECT on HOME, or PRESETS on the sequencer pages).

## Effects (FX)

| Effect | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| TAPE (over the whole mix) | WOW | SATURATION | TONE | HISS |
| GRAIN | SIZE (10–390 ms) | DENSITY | SCATTER | MIX |
| DELAY | TIME (20 ms–3 s) | FEEDBACK (to 150 %) | WEAR | MIX |
| SPACE | SIZE (0.4 s–~32 s, then INF) | DAMP | MIX | PRE-DELAY (0–250 ms) |

- **Runaway echo**: DELAY FEEDBACK past 100 % lets the echo grow and saturate.
- **Hold FX ~0.3 s: PERFORM.** While FX is held, the white keys play eight tricks: FREEZE (a toggle: holds the grain
  cloud), REV THROW, ECHO THROW, TAPE STOP, REVERSE, FILTER DOWN, FILTER UP, STUTTER.
- Signal flow: each track → LEVEL and PAN → out. Its sends go to GRAIN, DELAY and SPACE. Then TAPE over everything,
  the tricks, and MASTER.

## HOME and the mixer

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| HOME | VOLUME | CUTOFF | RESONANCE | FX (the three sends together, 0–200 %) |
| MIXER 1 | LEVEL | DLY SEND | REV SEND | GRAIN SEND |
| MIXER 2 | PAN | | | |

HOME draws each track's wave as it plays. The header shows the track, its sound and the tempo.

## Sounds and saving (SAVE)

- **47 factory sounds** with arctic names, in PAD, DRONE, BELL, VOICE, ORGAN, KEYS, OTHER and TEXTURE. Among them
  POLAR NIGHT, PERMAFROST, GLACIER PAD, SVALBARD CELLO, NORTHERN LIGHT, WIND OVER ICE.
- **Seven Holvut presets** at the end of DRONE: POLAR HOWL, WENDIGO, FROZEN SCREAM, WHITEOUT, DEAD SIGNAL, III and
  **ICE WRAITH**, the sound every track starts with. All seven latch and have DRONE on. III is a horrorscape: three
  Holvut voices in a cluster chord, rubbing a semitone apart, dark and slowly waking.
- **PRESETS** (the knob) steps through the selected category's sounds, factory first, then your own.
- **SAVE → SOUNDS**: 64 slots for your own sounds (SLOT, CATEGORY, TYPE, ACTION LOAD / SAVE / CLEAR). A sound keeps its
  source, knobs, ENV, LATCH, chord, LFO, DRONE and (FM) its patch.
- **SAVE → PROJECTS**: slots A–D, each the four tracks, their sequences and locks (LOAD, SAVE, ERASE, NEW).
- **Hold SAVE + a white key**: keys 1–4 recall A–D (at the next bar when playing), keys 5–8 save into them.
- **Autosave**: the work saves itself when stopped and silent, and comes back at power-on as it was left.
- **Undo / redo**: EDIT + OCT− / OCT+.
- Arctic Scream keeps its own saves, separate from GR!MM's. Samples already on the FM-1 stay.

## Settings (hold HOME)

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| SYSTEM | COLOUR (ICE, AURORA, VIOLET, PLASMA) | MIDI CHANNEL | TUNE (±50 cents) | SCREEN OFF |
| SYSTEM 2 | MIDI LEARN | SEQ MIDI OUT | SAMPLES (CLEAR) | |

- **MIDI**: notes in and out over USB on each track's channel (or ALL > SEL). MIDI LEARN links up to 32 CCs to any knob.
  SEQ MIDI OUT also sends the sequencer's and the arpeggiator's notes.
- SYSTEM 2 shows the CPU load (NOW and PEAK).

## The screen

- An arctic cyberpunk palette after PolarPatch's BlueWatch: a colour per page and per knob.
- **Boot**: the PolarPatch logo over drifting northern lights.
- **HOME**: each track's wave glowing as it plays, under a starry sky.
- **Moving northern lights** (new in 1.1), a ribbon per knob:
  - **Holvut EDIT**: four ribbons that follow FEEDBACK, BODY, DAMP and DRIVE.
  - **DRONE**: the four walks and the storm bar.
  - **ENV**: the envelope as a ribbon, each stage in its colour, its light hanging down as a curtain.
  - **LFO**: the shape as a ribbon, a band around it as wide as DEPTH, and the dot where the LFO is.
  - **TAPE**: four ribbons that wobble, flatten, ripple and fray with the knobs.
  - **GRAIN**: grains of light torn off a dim ribbon; FREEZE stops them.
  - **DELAY**: the echoes stepping down and to the right; past 100 % they grow and burn magenta.
  - **SPACE**: an aurora curtain as long as the reverb.
  - **MIXER 1 / 2**: each track a curtain of four standing ribbons, or a ribbon across the stereo field with its crest
    at PAN.

## The web editor

Runs in Chrome or Edge and talks to the FM-1 over USB (SysEx), live. Without an FM-1 it runs the firmware inside the
page. <https://polarpatch.github.io/Arctic-scream-FM-1/editor.html>

- **Sound**: every parameter of the four tracks, live, with the instrument's screen and keys beside it; a card each for
  the source, ENV, sound settings, LFO, DRONE and the FM patch.
- **Sound tools**: undo and redo; compare with the sound as loaded; store A and B, toggle and morph between them;
  randomize (knobs, ENV, LFO, DRONE, all, or a new source) by an amount; a list of what changed.
- **Sequencer**: a step grid, a piano roll or a typed list; tempo, play, stop and a playhead; MIDI files in and out.
- **Library**: the factory sounds and your 64 slots; load to T1–T4, name, save, export and import (`.arcticsound`;
  GR!MM's `.grimmsound` read too).
- **Samples**: WAVs into the four sample slots.
- **Backup**: the settings, slots A–D and all sounds, saved and restored.

## The browser demo and cheat sheet

- **[Demo](https://polarpatch.github.io/Arctic-scream-FM-1/)**: the FM-1's panel in the browser, running the same code.
  Computer keys play the keys and buttons.
- **[Cheat sheet](https://polarpatch.github.io/Arctic-scream-FM-1/cheatsheet.html)**: every page and combo on one
  printable page, and how to summon the screams from the arctic tundra.

## Installing

1. Download `arctic-scream-X.Y.fwsc` from [Releases](https://github.com/PolarPatch/Arctic-scream-FM-1/releases).
2. `pip install mido python-rtmidi`, connect the FM-1 over USB, run
   `python3 tools/fm1_install.py arctic-scream-1.1.fwsc`.

- It installs over the original firmware, GR!MM or any other FM-1 firmware.
- An interrupted install resumes: the FM-1 comes back in update mode and the same command finishes it.
- Every release is built and signed on GitHub; check it with `gh attestation verify` (see the README).
- If anything goes wrong, the Rescue page on the [FM-1 Firmware Hub](https://thefirmwarehub.com/) brings the FM-1 back.

## Version history

| Version | Date | What came |
| --- | --- | --- |
| 1.0 | 2026-10-25 | The fork of GR!MM: HOWL (now Holvut), six screaming presets, the HORROR scale, runaway echo, keys an octave down, 46 arctic factory sounds, the new look, the web editor with sound tools and sequencer, the demo and cheat sheet |
| 1.1 | 2026-10-26 | DRONE (BORGA, STOARBMA, GOHCCAT); HOWL renamed Holvut; moving northern lights on ENV, LFO, FX and the mixers; SUSTAIN 40 % by default; DRONE in the editor |

## Credits

- **[GR!MM](https://github.com/GRiMMBIENT/grimm)** (GPL-3.0): the firmware this is built on, with Felucca, Dinghy,
  SLOOP, X0X, msfa, DaisySP, Rings and CrispyZebra.
- **[Audrey II](https://github.com/Synthux-Academy/Audrey-II)** and **Audrey Touch** (Nick Donaldson, Infrasonic Audio;
  Synthux Academy; MIT): the design behind Holvut and the HORROR scale. No code copied.
- **[Jangada](https://github.com/zednaked/jangada)** (zednaked, GPL-3.0): the idea behind DRONE. No code copied.
- **[BlueWatch](https://github.com/PolarPatch/BlueWatch)** by PolarPatch: the colour palette.
- North Sami words checked in the dictionary at [sanit.oahpa.no](https://sanit.oahpa.no/).
- Fonts: Press Start 2P (CodeMan38) and Terminus (Dimitar Zhekov), SIL Open Font License 1.1.

See [LICENSING.md](LICENSING.md) for the details. M-VAVE and FM-1 are trademarks of their owners; Arctic Scream
p0larpatch is not affiliated with or endorsed by them, nor by GR!MM.
