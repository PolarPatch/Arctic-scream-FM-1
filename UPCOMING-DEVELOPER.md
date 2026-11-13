# Upcoming: ideas chosen for after 2.1

Ideas picked out for Arctic Scream's next versions, on the FM-1 and the VIBE FM alike (one engine, both threads). Not
built yet. The longer list of what waits, and why, is in `DEVELOPER.md` ("Ideas for later", "Pictures"). As with
Demake and ZEN-1, an idea taken from another firmware is written anew, never copied, and credited in `LICENSING.md`
when it lands.

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
