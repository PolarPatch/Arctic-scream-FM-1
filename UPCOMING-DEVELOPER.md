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
