# Upcoming: ideas chosen for after 2.1

Ideas picked out for Arctic Scream's next versions, on the FM-1 and the VIBE FM alike (one engine, both threads). Not
built yet. The longer list of what waits, and why, is in `DEVELOPER.md` ("Ideas for later", "Pictures"). As with
Demake and ZEN-1, an idea taken from another firmware is written anew, never copied, and credited in `LICENSING.md`
when it lands.

## Status (2026-11-13, branch `feature/2.2`)

Built, tested (host, stress 1..3, ARM, web, the device names in a Linux container) and documented in `MANUAL.md`:

| | Idea | Where | |
| --- | --- | --- | --- |
| 1 | The ice holds its breath | `jiehkki.c` `ih_tick`, `hall.c` `rv_hold` | built |
| 2 | A drone at power-on | JIEHKKI's START **ALWAYS** (`jk_always`; project EXT 23 bit 4) instead of a SYSTEM setting: no new settings version, and the piece is JIEHKKI's own | built |
| 3 | The keys as a display | `kl_flash`, `kl_keys` (no LIGHTS setting: it lights only what JIEHKKI and RUOHCAT do, both opt-in) | built |
| 4 | JIEHKKI's dusk | `jk_dusk` | built |
| 5 | PRESETS glides | there since 2.0 (`pgl_load`: the source's knobs glide, a new source's notes cross over). ENV's glide was tried and dropped: the engine reads ENV as a note starts, so gliding it only made a dip (drone_test caught it) | as it was |
| 6 | The year turns: JAHKI | `year.c`, LFO's fifth page; project version 10 (EXT 32 B) | built |
| 7 | SHIMMER moves with the weather | part of JAHKI (`rv_shq`) | built |
| | The factory sounds' fingerprints | `tests/sound_prints.py`, in `tests/run_host.sh` | built |
| | Pictures that move | `pictures.c` `pic_anim_rows`, `gen_pictures.py` `ANIM` | built (all 38) |
| | A picture for every factory sound (wished: no two alike) | 22 more scenes in `draw_pictures.py`; HOME keeps its stars (JAHKI's SKY only in the screensaver) | built |
| | GOVVA, the pictures as a screensaver (wished on the FM-1) | SCREEN OFF's last, `ui.c` `ui_govva`; no text over the pictures | built |
| 10 | PRESETS: listen, then go back | there already: a browse is one undo (`undo_take(UK_SND \| sel)`), EDIT + OCT-; now in MANUAL and `drone_test.py` | as it was |

On the FM-1 (beta1): the JAHKI page at 65 % CPU, 69 % peak (the whole picture moved once per strip of the screen, and
GUOVSSAHAS lags while it plays). beta2: only the strip's rows moved, the rows with nothing moving copied as they are;
GOVVA ~11 frames a second. beta2 on the FM-1: the same 65 %: that number is the audio's alone (VOICES 44, SPACE 8),
the screen is not in it; what was playing, not the pictures. With JAHKI on, GUOVSSAHAS shows the seasons (GOVVA).

## From Arcturus S6 (noted 2026-11-13)

[Arcturus S6](https://github.com/zeptosecond-dsp/Arcturus-S6) (Zeptosecond DSP, GPL-3.0-only, on Dinghy): a
self-playing wavetable instrument, four PPG-style oscillators moved by Serge-style function generators, sample and
holds and logic gates patched into each other. Its big patch matrix is its own identity and stays out of Arctic
Scream; these three fit.

### 1. The ice holds its breath (a moment frozen)

Arcturus lets a gate freeze its reverb: the texture is caught, held, let go. For Arctic Scream: RUOHČAT (and JIEHKKI,
while it plays) now and then freeze the room for a few seconds, SPACE's tail (and GRAIN, which has FREEZE already)
held still while the tracks play on over it, then let go in a slow release. How often by RUOHČAT's amount, never
while FREEZE is held by hand. Sketch: a freeze gate in the SPACE reverb's feedback (input muted, feedback at 1, the
damping kept); a random wait of 20..90 s, a hold of 3..10 s, a fade back over 2 s. CPU: none beyond the reverb's.

### 2. A drone at power-on (SYSTEM: START)

Arcturus rolls a gentle random drone at every power-on: it sounds at once. For Arctic Scream, a setting in SYSTEM,
START: AS LEFT (today's behaviour) or DRONE: at power-on a calm GENERATE'd drone on track 1 (a DRONE or PAD sound, a
fifth or an octave, LATCH on, slow), new each time, nothing saved until touched. Fits ZEN-1's "the Arctic plays"
(`DEVELOPER.md`) as its first step. On the VIBE: the same, after the welcome card's first start.

### 3. The keys as a display

Arcturus's key LEDs follow what is on screen (an envelope travelling along the keys, a gate, an LFO's dot). Arctic
Scream lights the white keys for FREEZE and the held layers today; when nothing is held they could breathe with the
music instead: a slow glow travelling with the wind (BIEGGA's gusts), JIEHKKI's parts lighting the key of the note
they play, the ice's answers (2.1) a key that flashes when the ice replies. A setting in SYSTEM (LIGHTS: ON, OFF)
for those who play in the dark. FM-1 only (the VIBE's pads have their own colours: the same idea on the pads later).

## From PaperLagoon and DEEP FIELD (noted 2026-11-13)

[PaperLagoon](https://github.com/hacha/paperlagoon) (hacha, GPL-3.0-only, on fm1-base): environmental music that
plays itself, steered from outside. [DEEP FIELD](https://github.com/WisestWizard/deepfield) (WisestWizard,
MPL-2.0, on fm1-base): a drone machine, every key a voice that swells in and stays. Their own cores (PaperLagoon's
worlds and offerings, DEEP FIELD's thirteen models) stay theirs; these fit.

### 4. JIEHKKI's dusk (after PaperLagoon's sleep)

PaperLagoon's sleep timer ends in a dusk: its last minute sparser, quieter, a little slower, then rest. JIEHKKI now
plays up to 8 hours (2.1): music to sleep to. Its last part (the last tenth, 1..15 minutes) a dusk: fewer parts
coming in, the long ones lingering, the tempo of its changes slower, the level falling, the last voice let go into
SPACE's tail, then silence (no cut). ENDLESS has none.

### 5. PRESETS glides (after DEEP FIELD's morph)

DEEP FIELD changes preset under a held chord by crossing each voice into the new sound. Arctic Scream cuts to the new
sound today. Turning PRESETS while a track sounds (latched or held) could glide: the new sound's parameters reached
over ~1.5 s (filters, levels, effects sends; the source swapped under a short crossfade where it must), the notes kept.
The picture card (2.1) shows meanwhile. A setting (SYSTEM: PRESETS CUT / GLIDE) if a cut is wanted for playing live.

### 6. The year turns: jahkegearddu (after PaperLagoon's cycles)

PaperLagoon's music leans on hidden quantities moved by three slow cycles of unequal, wandering length (about 1, 3
and 8 minutes). For Arctic Scream the cycles are the Arctic's own: in Sámi culture the year (jahkegearddu, the year's
wheel, North Sámi) has eight seasons (gávcci jagiáiggi), each named for how weather and land behave:

| | North Sámi | | Roughly |
| --- | --- | --- | --- |
| 1 | dálvi | winter | Dec..Feb |
| 2 | giđđadálvi | spring-winter | Feb..Mar |
| 3 | giđđa | spring | Apr..May |
| 4 | giđđageassi | spring-summer | May..Jun |
| 5 | geassi | summer | Jun..Aug |
| 6 | čakčageassi | autumn-summer | Aug..Sep |
| 7 | čakča | autumn | Oct..Nov |
| 8 | čakčadálvi | autumn-winter | Nov..Dec |

(Weather is dálki, the state of snow and land siivu. Pite Sámi has the same eight: dálvve, gïjrradálvve, gïjrra,
gïjrragiesse, giesse, tjaktjagiesse, tjaktja, tjaktjadálvve. The spellings to be checked against Neahttadigisánit
or a Sámi speaker before any of them goes onto the screen.)

The idea: a slow year turning under the music (a cycle of minutes to hours, set in SYSTEM, or still), passing
through the eight seasons. Each leans the four tracks a little: dálvi dark, low, slow, SPACE long, the polar night;
giđđadálvi brighter, crust and glare, BIEGGA's wind; geassi the midnight sun, high, open, still; čakča rain, fog,
lower. Never one knob to one thing, and small enough that the sounds stay themselves. JIEHKKI can follow it (a
night's piece crossing a season or two). The season's name shown on HOME, its picture in SKY (2.1's pictures).
This is "polar night / midnight sun" from `DEVELOPER.md`, grown into the whole year.

### 7. SHIMMER moves with the weather

SHIMMER is in already (FX > AIR). With 6, the seasons lean it (geassi's light and the aurora's nights more, čakča's
fog less), and the frozen moments of 1 catch it rising.

## Development: the factory sounds' fingerprints (after DEEP FIELD)

DEEP FIELD renders every factory preset to WAV and fails if a render's hash differs from the golden file; a meant
change rewrites the file. For Arctic Scream: `tests/render_sounds.sh` plays each of the 38 factory sounds (and the
demos A1..A6) through the host build for a few seconds on a fixed note, hashes the WAVs and compares them with
`tests/golden.txt`. A change to the engine that alters a sound by accident fails the test; on the VIBE the same
engine must give the same hashes (one engine, two threads). Build this first: it guards everything above.

## Pictures that move (wished 2026-11-13)

The user, of 2.1's drawn pictures: when PRESETS turns, the card could play a short animation (~2.5 s) of its
picture instead of standing still. Not video (16 pictures x 30 frames would not fit in the FM-1's flash): the
picture stays, and a little of it moves, drawn by the firmware each frame:

- snow and drift falling and blowing across (blowing, whiteout, vidda), stars twinkling (the night ones);
- the aurora's curtains rippling (tundra_aurora), its green shifting in brightness along the curtain;
- water rippling under the horizon (fjord, under_ice's light from above), the midnight sun's glitter on the sea;
- a light that blinks or turns (lighthouse's beam, station's mast light, the hut's window flickering, the flare
  burning and its smoke drifting);
- ice dripping (icicles), the cave's figure fading in and out (ice_cave), the figures on the tundra walking (voices).

`tools/gen_pictures.py` marks each picture's moving parts (palette entries for the shimmer, a horizon row for the
water, regions for the lights), `pictures.c` animates them over the still picture. Photographs get the generic ones
(snow, stars, a slow pan). Costs to measure on the FM-1 first: redrawing the card each frame over SPI.

## From Adrift (noted 2026-11-14)

[Adrift](https://github.com/romainguy/adrift) (Romain Guy, MPL-2.0, on felucca-bsp): a 16-voice physical-modelling
synth, each note an exciter played into a key-tracked bucket-brigade delay loop that rings at the note's pitch. Its
six voice groups, its many presets of plucked and bowed instruments and its own engine stay its own; these three
fit, written anew and credited in `LICENSING.md` when they land.

### 8. A resonator of ice (a tenth source)

Adrift's heart: a short burst (a pluck, noise, a bowed swell) sent into a delay loop tuned to the note, its feedback
deciding how long it sings, its sign (inverted: a pipe's hollow, odd harmonics) and an all-pass's dispersion (bells,
metal bars) its colour. For Arctic Scream a source of the ice's own: ice that sings when it is struck or bowed, glass
bowls, a frozen string, the long tones of a lake freezing. Fits next to Holvut (feedback) and BIEGGA (noise at the
note): the drone's sources so far are made, not struck. Knobs (a sketch): STRIKE (bow .. strike: the exciter's
shape), RING (feedback, up to self-oscillation for an endless drone), HOLLOW (the loop's sign, blended), BRITTLE
(dispersion: glass to metal). CPU to measure first: a fractional delay and an all-pass a voice.

### 9. Breathing and staggered entries (an envelope that loops, a delay before it)

Adrift's exciter envelope can loop while a key is held (attack-decay, or decay-attack-decay), and each group can wait
0..2 s before it sounds (one key, a strummed chord). For Arctic Scream: ENV's LOOP (OFF, A-D, D-A-D) makes a held
drone breathe on its own, slow swells like breath or wind, at ENV's own times; and a track's ONSET (0..2 s, longer for
drones, up to ~8 s) lets the four tracks come in one after another from a single chord, as JIEHKKI's tracks do. On
the ENV page; saved with the sound.

### 10. PRESETS: listen, then keep or go back

Adrift plays a preset as you turn to it, and OCT+ keeps it, OCT- goes back to the sound you had (all of it, as it
was). Arctic Scream's PRESETS changes the sound at once (and since 2.0 glides there). An undo for it: after turning
PRESETS, the sound before is kept; EDIT + OCT- (the undo that exists, DICE's and ACCIDENT's) brings it back, all of
it. OCT- alone stays the octave. Nothing new to learn for those who never want it.

## From SHRUB (noted 2026-11-14)

[SHRUB](https://shrub.audio/) (Steve Duda, Xfer; closed source, all rights reserved): a stripped-down wavetable synth
for the FM-1, its synth names its own. Nothing taken from it but ideas, written anew; no code, names or looks. Arctic
Scream's own menus and screens stay as they are.

### 11. CHAOS: movement that never repeats

SHRUB offers two strange attractors (a Lorenz and a Rössler) as modulation sources: smooth, never-repeating wiggles,
an X and a Y each. For drones the best kind of slow movement there is. In Arctic Scream: a CHAOS shape on LFO (its
RATE the attractor's speed, its DEPTH as now), and a choice for DRONE's walks (BORGA: random walks, or CHAOS). Fixed
point, a step of the equations a tick (control rate, not per sample): little CPU.

### 12. Knob moves that loop (a motion recorder)

SHRUB's REC records a knob as it is turned and plays the move back in a loop, a lane a knob, with the lane's length.
For Arctic Scream: hold REC (or a combination of it) and turn a knob, the move kept and looped over the pattern's length
(or longer: up to 64 bars, for a filter opening over minutes), a few lanes a track. Saved with the project; cleared
from SEQ's tools. Arctic Scream's step locks stay what they are; this is the hand's own slow move, as recorded.

### 13. Notes that spread themselves (voice steps)

SHRUB gives each new note the next of up to eight steps, each with its own pan and fine tune, round and round: a
chord spreads across the stereo field, every other note a hair apart. For Arctic Scream a SPREAD on the track (OFF,
2..8 steps; pan width and detune), made for held chords and DRONE's choir. Beside BORGA's drifting voices, not instead.

### 14. A CPU guard: a new note that would not fit takes a voice over softly

SHRUB watches its CPU; when a new note would not fit, it takes over a sounding voice (no click) instead of the audio
breaking up. Heavy sounds simply play fewer notes. For Arctic Scream: measure each block's time (as SYSTEM 2's CPU
does), and over a margin, steal the quietest or oldest voice with a short fade. A safety net for the heavy layers
(SHIMMER, GRAIN, four drones with RUOHCAT).

### 15. USB LEVEL: the speaker off while a computer listens

USB LEVEL (SYSTEM 3, 1.4) sets how loud the computer hears the FM-1. SHRUB adds a choice that quiets the speaker and
the jack while USB plays, so the sound is not heard twice when monitoring through the computer. For Arctic Scream: a
SPK OFF step on USB LEVEL (or a setting beside it).

### 16. The second core: load balancing

Both Adrift and SHRUB run on both of the chip's cores; ECHOFIELD renders half its grains on the second (see
`DEVELOPER.md`, "The second core"). Arctic Scream uses one. For the big overhaul: share the load, the tracks split
across the two cores (T1, T2 here; T3, T4 there) or the effects (SPACE, SHIMMER, GRAIN) moved over, the screen and
the main loop never waiting on the audio. Its headroom pays for 8, 11 and 12. Boot the core, share the buffers safely,
measure both (SYSTEM 2's CPU per core), and keep a fallback to one core if the second fails to start.
