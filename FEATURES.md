# Arctic Scream p0larpatch: features

A complete overview of what Arctic Scream p0larpatch does, version 1.5.1. For the short version see the
[README](README.md); for every button on one page, the [cheat sheet](https://polarpatch.github.io/Arctic-scream-FM-1/cheatsheet.html);
for how it is built, [DEVELOPER.md](DEVELOPER.md).

## What it is

Arctic Scream p0larpatch is a free, open-source (GPL-3.0) drone firmware for the M-VAVE FM-1 by PolarPatch. It is built
on [GR!MM](https://github.com/GRiMMBIENT/grimm) and turns the FM-1 into an instrument for dark ambient drones and
feedback screams: slow, cold sounds that hum for minutes, and a voice that wakes on its own and howls.

| | |
| --- | --- |
| Version | 1.5.1 |
| Firmware identity | `FM-1_977` (USB name "Arctic Scream" by "PolarPatch") |
| Package | `arctic-scream-1.5.1.fwsc`, built and attested on GitHub |
| Source | <https://github.com/PolarPatch/Arctic-scream-FM-1> |
| Demo, editor, cheat sheet | <https://polarpatch.github.io/Arctic-scream-FM-1/> |
| Licence | GPL-3.0 (the PolarPatch logo excepted) |

## At a glance

- Four tracks, six voices shared between them.
- Eight sound sources, among them **Holvut**, a feedback loop that howls by itself.
- **DRONE**: a sound that lives on its own (BORGA, STOARBMA, GOHCCAT).
- 47 factory sounds with arctic names, and 64 slots for your own.
- Effects on seven pages (TAPE, GRAIN, DELAY, SPACE, MASTER, DRIVE, AIR) and eight performance tricks.
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
- **FEEDBACK** (−60 to +12 dB) wakes it. **BODY** (1–92 ms) bends it, gliding when turned and wandering a little by
  itself. **DAMP** (18 kHz to 100 Hz) turns the scream into a growl. **DRIVE** (1× to 7×) tears it.
- **Overblowing:** FEEDBACK's last steps (120–127) also lift a high-pass inside the loop, so the howl leaps up the
  note's overtones and screams higher. Below 120 the sound is as before.
- **Pressure:** MIDI aftertouch (channel or poly) on a track's channel pushes its Holvut's FEEDBACK toward the top.
  Press harder and the note feeds back more, then overblows; ease off and it falls back.
- **The LOOP page** (1.4; EDIT tapped again on a Holvut track: SOUND, LOOP, SOURCE). All four at 0 (OFF) sound as
  before. Saved with the sound and the project.
  - **GATE:** once the key is let go, the loop stops feeding itself: all the way up it dies into its own tail, in
    between it fades but lingers.
  - **BLOOM:** what GRAIN, DELAY and SPACE add to the mix goes back into the loop, an FX loop inside the feedback: the
    scream grows inside its own effects.
  - **LOWCUT:** the loop's high-pass, two poles, ~70 Hz to ~1.9 kHz: the howl thins and climbs; with DAMP, a band
    that screams.
  - **EXCITE:** the track's sample (SAMPLE's SLOT, START and LENGTH, looped) is fed into the string, so a sound of your
    own wakes and colours the howl. The slab shows the slot (`S2 45%`, or `EMPTY`).
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
| DELAY | TIME (20 ms–2.6 s) | FEEDBACK (to 150 %) | WEAR | MIX |
| SPACE | SIZE (0.4 s–~32 s, then INF) | DAMP | MIX | PRE-DELAY (0–250 ms) |
| MASTER (1.3) | CUTOFF | RESONANCE | TYPE (LP / HP / BP) | POSITION (PRE FX / POST REVERB / END) |
| DRIVE (1.3) | AMOUNT (0: off) | TYPE (SOFT / FUZZ / CLIP / FOLD) | TONE | MIX |
| AIR (1.4) | PING (PING-PONG, 0: off) | SHIMMR (SHIMMER, 0: off) | | |

- **Runaway echo**: DELAY FEEDBACK past 100 % lets the echo grow and saturate.
- **AIR** (1.4), FX's last page. **PING-PONG** throws DELAY's echoes left, right, left, as wide as it is turned (the
  loop runs at twice TIME, so TIME goes up to 1.3 s while it is on). **SHIMMER** takes SPACE's tail an octave up and back
  into the reverb, so it rises as it fades. Both are saved with the project; older projects load with them off.
- **Hold FX ~0.3 s: PERFORM.** While FX is held, the white keys play eight tricks: FREEZE (a toggle: holds the grain
  cloud), REV THROW, ECHO THROW, TAPE STOP, REVERSE, FILTER DOWN, FILTER UP, STUTTER.
- **DRIVE**: AMOUNT drives the whole mix 1x to ~8x into the TYPE's curve; each TYPE about as loud as the others at the
  same AMOUNT, at most ~6 dB louder, then a limiter. **MASTER**: a stereo filter on the whole mix, at its POSITION.
- **Hold FX + a black key: GENERATE.** That track plays by itself, random notes (or chords) of its SCALE, on the tempo
  or the MIDI clock: DRIFT (long, soft), PULSE (a melody on eighths), STORM (dense sixteenths). Each track its own MODE,
  OCTAVE and RANGE; several tracks share the six voices fairly; the pattern runs on silently underneath. Not saved.
- Signal flow: each track → its filter (PRE or POST its sends) → LEVEL and PAN → out. Its sends go to GRAIN, DELAY and
  SPACE. Then MASTER (at its POSITION), DRIVE, TAPE over everything, the tricks, MASTER's END, and the output stage:
  6 dB of headroom, the MASTER knob and a limiter, so nothing clips. On the speaker and headphones a side limiter
  (1.4) holds the difference between left and right under ~−17 dBFS, against the headphone amplifier's crackle on wide,
  loud sound; a mono sound never changes, and the USB recording is not touched.

## HOME and the mixer

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| HOME | VOLUME | CUTOFF | WARP (1.3) | MORPH (1.3) |
| MIXER 1 | LEVEL | DLY SEND | REV SEND | GRAIN SEND |
| MIXER 2 | PAN | TYPE (LP / HP / BP) | POS (PRE / POST) | RESONANCE |

HOME draws each track's wave as it plays. The header shows the track, its sound and the tempo.

- **WARP** (1.3): eight layers that come in one after another as you turn it, so the sound grows smoothly from still to wild: the voices drift apart like a choir; the filter breathes open; the source's knobs wander; the source is pushed the way DRONE's storm pushes it; a slow vibrato; more resonance and wider sweeps; the voices pull far apart and wander faster; at the top a tape's flutter and a sagging pitch. Nothing jumps and nothing goes silent.
- **MORPH** (1.3): one knob through all four of the source's knobs at once (on PHYS: STRUCTURE, DAMP and BOW; the stepped ones, such as MODEL, stay as set), on a path of its own for each track and source, always the same, with a slow wandering around it that grows as you turn it up. At 0 the sound is as set.
- While WARP or MORPH is up, the track's level is held where it was, so neither makes it suddenly louder or quieter; both sit on top of the knobs (the knobs themselves never move) and are saved with the project.

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
- **Autosave**: the work saves itself when stopped and silent, and comes back at power-on as it was left. Under a
  latched drone it saves after 30 s left alone (1.5); a slot or a sound you save is written at once, with a ~50 ms
  gap in the sound.
- **Undo / redo**: EDIT + OCT− / OCT+. **Hold REC ~1 s** clears the selected track's pattern (undoable).
- **Hold SEL + OCT+**: the selected track's LATCH on or off, from any page.
- Arctic Scream keeps its own saves, separate from other FM-1 firmwares. Samples already on the FM-1 stay.

## Settings (hold HOME)

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| SYSTEM | COLOUR (ICE, AURORA, VIOLET, PLASMA) | MIDI CHANNEL | TUNE (±50 cents) | SCREEN OFF |
| SYSTEM 2 | MIDI LEARN | SEQ MIDI OUT | SAMPLES (CLEAR) | CLOCK (INT, INT+OUT, USB, TRS) |
| SYSTEM 3 | LATCH (FROM SOUND, ALWAYS OFF) | USB LEVEL (−12 to +24 dB, 1.4) | | |

- **MIDI**: notes in and out over USB on each track's channel (or ALL > SEL, or CH n > SEL: channel n alone to the
  selected track). MIDI in on the TRS jack too, with a TRS-to-DIN adapter. MIDI LEARN links up to 32 CCs to any knob.
  SEQ MIDI OUT also sends the sequencer's, the arpeggiator's and GENERATE's notes.
- **MIDI clock**: follow one from USB or the TRS jack (START, CONTINUE, STOP), or send one over USB (INT+OUT).
- **USB LEVEL** (1.4): how loud a computer records the FM-1, in 6 dB steps; 0 dB is the volume knob all the way up,
  and a limiter after it keeps the louder steps from clipping.
- SYSTEM 2 shows the CPU load (NOW and PEAK) and USB REC DROPS (gaps in the USB recording); the header says CPU FULL:
  NOTE DROPPED when the FM-1 had to let a note go, and WATCHDOG RESET or LOW VOLTAGE RESET after such a restart.

## The screen

- An arctic cyberpunk palette after PolarPatch's BlueWatch: a colour per page and per knob.
- **Boot**: ARCTIC SCREAM out of the dark in a glitch storm, settling into the banner's torn rows, with p0larpatch and the four tracks' lines (about 5.6 s).
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
  the source, ENV, sound settings, LFO, DRONE, the FM patch and Holvut's LOOP (1.4).
- **Sound tools**: undo and redo; compare with the sound as loaded; store A and B, toggle and morph between them;
  randomize (knobs, ENV, LFO, DRONE, all, or a new source) by an amount; a list of what changed.
- **Sequencer**: a step grid, a piano roll or a typed list; tempo, play, stop and a playhead; MIDI files in and out.
- **Library**: the factory sounds and your 64 slots; load to T1–T4, name, save, export and import (`.arcticsound`;
  older `.grimmsound` files read too).
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
   `python3 tools/fm1_install.py arctic-scream-1.5.1.fwsc`.

- It installs over the original firmware or any other FM-1 firmware.
- An interrupted install resumes: the FM-1 comes back in update mode and the same command finishes it.
- Every release is built and signed on GitHub; check it with `gh attestation verify` (see the README).
- If anything goes wrong, the Rescue page on the [FM-1 Firmware Hub](https://thefirmwarehub.com/) brings the FM-1 back.
- Both flash chips the FM-1 ships with are known (since 1.5.1): the P25Q80 of the first units and the UC25HQ80 of those
  sold since autumn 2026. On a newer unit with 1.5 or older, install 1.5.1 once through the chip's download mode (the
  Rescue page's `#chip` steps); after that, updates install the normal way.

## Version history

| Version | Date | What came |
| --- | --- | --- |
| 1.0 | 2026-10-25 | The first release: HOWL (now Holvut), six screaming presets, the HORROR scale, runaway echo, keys an octave down, 46 arctic factory sounds, the new look, the web editor with sound tools and sequencer, the demo and cheat sheet |
| 1.1 | 2026-10-26 | DRONE (BORGA, STOARBMA, GOHCCAT); HOWL renamed Holvut; moving northern lights on ENV, LFO, FX and the mixers; SUSTAIN 40 % by default; DRONE in the editor |
| 1.5.1 | 2026-11-08 | The newer FM-1s' flash chip (UC25HQ80): saves, the USB rescue and updates on it as on the first chip (P25Q80); on a newer unit with 1.5 or older, install through the chip's download mode once |
| 1.5 | 2026-11-07 | The screen reader: the screen said to fm1-reader (pages, knobs, lists, steps, PLAY, STOP, REC, the header's messages); saves under a latched drone (a slot or a sound saved at once, the autosave after 30 s at rest); the editor keeps a project's effects and tempo in its file; TAPE's TONE shows its corner (OPEN, 10K .. 400) |
| 1.4 | 2026-11-06 | Holvut's LOOP page: GATE, BLOOM (the effects back into the loop), LOWCUT, EXCITE (the track's sample into the string); FX's AIR page: PING-PONG and SHIMMER; SYSTEM 3's USB LEVEL (−12 to +24 dB); a side limiter against the headphone amplifier's crackle; latched and ARP key lights follow OCT− and OCT+; no 64-bit multiplies beside 16-bit loads, which the FM-1's CPU now and then got wrong (FM, PHYS, VOICE, PERFORM, LFO, BLOOM), and a tool that checks it |
| 1.3.1 | 2026-11-01 | USB audio on macOS 10.14 to 15: the audio input and MIDI each with an audio control of their own (macOS's older USB audio driver stopped at MIDI in the list and never reached the input); the computer records at full level, the volume knob sets the speaker only |
| 1.3 | 2026-11-01 | Holvut overblows: FEEDBACK's last steps lift a high-pass in the loop, and the howl leaps up its overtones; MIDI aftertouch pushes FEEDBACK (press harder, it screams and climbs); BODY to ~92 ms; SAMPLE and Holvut share a voice's scratch memory. From GR!MM V2.0: GENERATE, MASTER and DRIVE, the track filter's TYPE and POS, MIDI clock in and out, TRS MIDI IN, CH n > SEL, SYSTEM 3's LATCH, SEL + OCT+, REC held to clear, 6 dB of headroom and a limiter, the voice rule, backups with the samples, the USB audio ring, saves under endless tails, reset reasons; ~6.5 KB of RAM freed. The editor: projects A–D to files, SETTINGS. Clearer page colours |
| 1.2 | 2026-10-31 | III, a Holvut horrorscape (47 factory sounds) and demo A5; Holvut deeper: BODY to ~45 ms, four all-passes in the loop; WHITEOUT retuned to keep its sound; the host tests run on macOS |

## Credits

- **[GR!MM](https://github.com/GRiMMBIENT/grimm)** (GPL-3.0): the firmware this is built on, with Felucca, Dinghy,
  SLOOP, X0X, msfa, DaisySP, Rings and CrispyZebra.
- **[Audrey II](https://github.com/Synthux-Academy/Audrey-II)** and **Audrey Touch** (Nick Donaldson, Infrasonic Audio;
  Synthux Academy; MIT): the design behind Holvut and the HORROR scale. No code copied.
- **[Jangada](https://github.com/zednaked/jangada)** (zednaked, GPL-3.0): the idea behind DRONE. No code copied.
- **[ECHOFIELD](https://github.com/Philip-Bos/echofield-fm1)** (Philip Bos, MIT): the ideas behind PING-PONG, SHIMMER
  and the side limiter (1.4). No code copied.
- **Book of the Dead II** (DigDugDIY): the FX loop in the feedback, LOOP's BLOOM (1.4). An idea only.
- **[BlueWatch](https://github.com/PolarPatch/BlueWatch)** by PolarPatch: the colour palette.
- North Sami words checked in the dictionary at [sanit.oahpa.no](https://sanit.oahpa.no/).
- Fonts: Press Start 2P (CodeMan38) and Terminus (Dimitar Zhekov), SIL Open Font License 1.1.

See [LICENSING.md](LICENSING.md) for the details. M-VAVE and FM-1 are trademarks of their owners; Arctic Scream
p0larpatch is not affiliated with or endorsed by them.
