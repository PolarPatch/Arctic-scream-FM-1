# Arctic Scream p0larpatch: developer notes

What has been done to turn GR!MM into Arctic Scream p0larpatch, where it lives in the code, and how to build, test
and release it. For building from scratch see [docs/BUILDING.md](docs/BUILDING.md); for the editor's protocol see
[docs/EDITOR_PROTOCOL.md](docs/EDITOR_PROTOCOL.md).

## Where it comes from

- A GPL-3.0 fork of [GR!MM](https://github.com/GRiMMBIENT/grimm) 1.0 (git remote `upstream`; its tag is kept
  locally as `grimm-v1.0`). GR!MM is built on Felucca (the platform), Dinghy, SLOOP, X0X, msfa, DaisySP, Rings and
  CrispyZebra; all their credits stay in [LICENSING.md](LICENSING.md).
- Holvut (the HOWL source in the code) follows the design of Synthux's [Audrey II](https://github.com/Synthux-Academy/Audrey-II) and Audrey Touch
  (MIT). It is written from scratch, in integers, and no code is copied.
- The DRONE page follows the idea of [Jangada](https://github.com/zednaked/jangada)'s DRONE page (zednaked, GPL-3.0:
  EVOL, TENS, RAMP). It is written from scratch, and no code is copied. Its knob names are North Sami, checked in the
  dictionary at [sanit.oahpa.no](https://sanit.oahpa.no/).
- The colours come from PolarPatch's [BlueWatch](https://github.com/PolarPatch/BlueWatch) cyberpunk theme.
- The PolarPatch logo (`assets/polarpatch-logo.png`, `firmware/gen/polarpatch_logo.h`) is PolarPatch's, not under
  the GPL.

## The platform in short

- The FM-1 is a JieLi AC79 with no FPU, so **all firmware code is integer-only**.
- Unity build: `firmware/src/grimm.c` includes every other `.c` file.
- RAM is tight: about 2 KB of the 96 KB main RAM is free. The bigger buffers live in the `.pool` section (~43 KB
  free there).
- The same `grimm.c` runs in three places:
  - on the FM-1;
  - as a host program (`-DDINGHY_HOST`) for tests and screenshots;
  - as WebAssembly (`-DGRIMM_WEB`, `web/fm1_web.c`) for the demo and the editor.

## What changed, by area

### Identity

| | Arctic Scream | GR!MM |
|---|---|---|
| Firmware ID | `FM-1_977` (`platform.c` `FELUCCA_ID`, `tools/build.py` `PRODUCT`) | `FM-1_966` |
| SysEx head | `7D 41 53` ("AS") | `7D 47 52` |
| Package | `build/arctic-scream.fwsc`, release `arctic-scream-X.Y.fwsc` | |
| USB names | "Arctic Scream" by "PolarPatch" (`usb.c` STR1/STR2) | |
| Saves | magic "ADPK", own tracks (56 B, `store.c`) | not read by Arctic Scream |

- `FM-1_977` is not in the Firmware Hub's catalogue (`https://thefirmwarehub.com/catalog.json`).
- The 9NN form lets the installers resume an interrupted update.
- The update loader keeps Felucca's names ("Felucca Update", `ota-FM-1_900`).
- macOS may keep showing the old MIDI port name "Felucca" until the device is removed in Audio MIDI Setup.

### Sound

- **Holvut** (North Sami: to howl), the eighth source (`ENG_HOWL`; its name on the screen "Holvut", `howl.c`
  `HOWL_ENGINE`):
  - Code: `firmware/src/howl.c` and `howl.h`. Its state shares the PHYS slot (`phys.c` `ph_slot`), so it costs no RAM
    of its own.
  - The loop: noise → Karplus-Strong string → DRIVE saturation → DAMP low-pass → ~70 Hz high-pass → two all-passes →
    × FEEDBACK → BODY's delay line → back into the string.
  - FEEDBACK runs from −60 to +12 dB and wakes the loop at about two thirds. A key adds a 50 ms burst.
  - Output level is `VOICE_LV * 3 / 4`, because chords had clipped.
- **DRONE**, LFO's second page (`PG_DRONE`; `firmware/src/drone.c`):
  - **BORGA** (snowdrift) 0–100 %: four slow random walks a track (a new target every 4–8 s, 5/8 of the last plus a
    random step, folded at the ends, glided through two one-poles of ~2.4 s). Walk 0: the track filter's CUTOFF (when
    it is not open); walk 1 and 2: the source's knobs as `DR_MAP` says (Holvut: FEEDBACK ±5 % around the threshold,
    BODY, DAMP); walk 3: the REV send. Each voice also gets its own slow detune (±3 cents, `vc_fine` in `vinc`) and
    breath (its level down to 80 %, in `voices_render`).
  - **STOARBMA** (storm) 0–100 %: the tension. It opens the filter (+40), adds resonance (+30), pushes the knobs as
    `DR_MAP` says (Holvut: FEEDBACK +12 %, DRIVE +40 %, DAMP −30 %), spreads the voices to ±12 cents, and makes the
    walks up to twice as deep and three times as fast.
  - **GOHCCAT** (to wake) OFF, 1, 2, 4, 8, 16, 32 bars: the time to reach STOARBMA from where the tension is, on the
    tempo (OFF: ~0.3 s). A drone woken from silence starts at 0.
  - The fourth knob is LATCH, as on SOURCE.
  - Everything is added on top of the knobs (`lfo_params`, `trk_filter`, `mix_tracks`). At BORGA and STOARBMA 0 the
    output is bit for bit as before (checked against the v1.0 build).
  - Runs in the audio ISR every 8th control step (~172 Hz). State in the pool; main RAM +208 B.
  - Saved without a format change, in spare bits: a sound's bytes 12 and 15–17 (`dr_spk` / `dr_suk`), a track's bytes
    32, 37 and 46 (`pk_track`). Older saves read as DRONE off. See `docs/EDITOR_PROTOCOL.md`.
  - The screen: four aurora ribbons (the walks), and a STOARBMA bar with its mark (`ui.c` `pic_drone`). The slabs show
    the whole Sami names in a narrow 7 px font (`ui_text_n`).
- **Presets** (`store.c`):
  - Six HOWLs together at the end of DRONE: POLAR HOWL, WENDIGO, FROZEN SCREAM, WHITEOUT, DEAD SIGNAL, ICE WRAITH.
  - The Holvut sounds and SVALBARD CELLO have LATCH on.
  - The six Holvut sounds start with DRONE on: BORGA 50–75 %, STOARBMA 0, GOHCCAT 8 bars (`FSNDD`).
  - The 46 factory sounds have arctic names.
- **Default sound:** ICE WRAITH (`SND_DEFAULT`), on all four tracks at the first boot and after NEW (`snd_default()`
  in `grimm.c`).
- **HORROR scale** and **runaway echo:** DELAY feedback goes up to 150 %.
- **Keys:** an octave down, the first white key F2. The track defaults are darker. The knobs on HOME, EDIT and LFO all
  change the sound audibly from the start.
- **Room made:**
  - DELAY 3.0 s instead of 3.5 (`tape.c` `DL_LEN`, `DL_MS` in `tools/gen_tables.py`).
  - GRAIN ring 1.0 s instead of 1.25 (`GE_LEN`).
  - PERFORM history 0.6 s instead of 0.75 (`PF_LEN`).
- **Samples:**
  - No factory samples are built in: `SMP_FACT_N 0`. GR!MM's four took ~290 KB of flash.
  - Power-on leaves the slots alone. SYSTEM 2's SAMPLES and the editor's button are CLEAR and empty all four.
  - To build in our own samples later: put the WAVs in `assets/samples/`, list them in `SAMPLES` in
    `tools/gen_samples.py`, and run it.

### Screen (`firmware/src/ui.c`)

- **Colours:** BlueWatch only (`BW_*`), many per page. The knob colours are `UI_KC`: periwinkle, teal, amber, pink.
  SEQ is violet, cyan and pink.
- **Boot:** the PolarPatch logo, "Arctic scream", and moving aurora ribbons (`ui_boot_draw`, `bt_ribbons`).
- **HOME:**
  - glowing waveforms per track;
  - the slabs show the selected track's VOLUME (MUTE when muted), CUTOFF, RES and FX.
- **The aurora pages** (`ui.c`, after GR!MM's pictures were replaced): every knob a ribbon of northern lights, moving
  a frame every `UI_ANIM_MS` (`ui_anim_page`, `hw_ph`; the region y 44–147 redrawn):
  - TAPE: four ribbons, one a knob (WOW wobbles, SATURATION squares off, TONE ripples, HISS frays); bypassed: still and dim.
  - GRAIN: a dim buffer ribbon and grains of light torn off it (DENSITY, SIZE, SCATTER, MIX); FREEZE stops them.
  - DELAY: the sound and its echoes as ribbons stepping down and right (TIME), smaller each time (FEEDBACK), more worn
    (WEAR), as bright as MIX; past 100 % they grow and burn magenta ("DELAY: RUNAWAY").
  - SPACE: an aurora curtain, its rays as long as SIZE (INF: to the ground), darker with DAMP, below the ribbon by
    PRE-DELAY, as bright as MIX.
  - LFO: the SHAPE as a ribbon, a band around it as wide as DEPTH, and the dot.
  - MIXER 1: each track four standing ribbons (LEVEL, DLY, REV, GRAIN); MIXER 2: each track a ribbon across the stereo
    field, its crest at PAN.
- **Holvut EDIT:**
  - Always four aurora ribbons instead of GR!MM's cells: FEEDBACK sets their height, BODY their length, DAMP their
    light, DRIVE their shape.
  - Animated on `PG_SOUND` (`hw_ph`).

### Web: demo, editor, cheat sheet (`web/`)

- **`index.html` / `panel.js`, the demo:**
  - The FM-1's panel in the browser, running the same firmware: flat grey buttons with a cyan edge when lit, keys
    with a violet edge, REC red only while recording.
  - The name over the keys: "Arctic scream – p0larpatch".
- **`cheatsheet.html`:** every page and combo on one printable page, plus how to summon the screams.
- **`editor.html` / `editor.js` / `editor_core.js`, the editor:**
  - **Look:** BlueWatch colours, PolarPatch logo, a left menu (track: sound, sequencer; sounds: library, samples;
    device: backup, connection). The firmware is shown as "p0larpatch v1.0".
  - **Sound:** every parameter live; the four knobs in the FM-1's colours; the ENV as a curve; the screen and keys of
    the engine running in the page.
  - **Sound tools:**
    - undo and redo, per track; an edit counts once the controls rest for 350 ms;
    - compare with the sound as loaded;
    - store A and store B, toggle between them, and morph A → B;
    - randomize knobs, ENV, LFO or everything, by an amount;
    - a list of what changed since the sound was loaded.
  - **Drone:** BORGA, STOARBMA and GOHCCÁT, live, in morph, randomize and the list of changes.
  - **Sequencer:**
    - pattern settings (LEN, DIV, SWING, PLAY MODE, TEMPO);
    - a 64-step grid, a piano roll and a typed step list;
    - per step: NOTE, CHANCE, RATCHET and LENGTH;
    - PLAY and STOP with a playhead;
    - MIDI files in and out (`seqToMidi`, `midiToSeq`).
  - Sound files are exported as `.arcticsound`. GR!MM's `.grimmsound` files are still read.
- **Firmware support for the editor:**
  - `firmware/src/editor.c` speaks protocol 3, adding command **24 SEQ**: READ, SET, TEMPO and PLAY.
  - `web/editor_web.c` exports `web_seq_state` for the playhead when the page itself plays.
  - `web/worklet.js` keeps only the newest SEQ SET per track while one is waiting.

## Build and test

```sh
# host build: screens and sound; PNGs with Pillow (not sips: test_web compares them)
cc -O2 -Wall -Wno-unused-function '-Dsection(x)=used' -DDINGHY_HOST -DAPP_VER='"1.1"' -Ifirmware/src -Ifirmware/gen \
   -o build/host/grimm firmware/src/grimm.c
build/host/grimm build/host/grimm        # (or: tests/run_host.sh)
build/host/grimm OUT script.txt FPS      # script mode: "3100 keys 0x80", "3500 btn 6", "5000 enc 2 -45", "end"

# web: the demo, the editor, the cheat sheet into web/dist, then the tests (wasm against host mode, the editor's core)
source ~/emsdk/emsdk_env.sh; bash web/build_web.sh
node web/test_web.mjs                     # must say "test_web: all ok"

# stress test, seeds 1-3: each must print STRESS OK (see tests/stress.sh for the build line)
python3 tests/stress_gen.py 1 build/stress/script.txt build/stress/checks.txt
STRESS_CHECKS=build/stress/checks.txt build/stress/grimm build/stress/s build/stress/script.txt

# the real firmware (JieLi toolchain; on macOS through Docker, linux/amd64)
./build.sh                                # -> build/arctic-scream.fwsc; checks RAM, pool and the image size
```

Notes for the web build:

- Serve `web/dist` (for example on `localhost:8765`) to try the pages. The editor's `?try` starts the virtual FM-1.
- Web MIDI with SysEx needs Chrome or Edge, and https or localhost.

## Install on an FM-1

```sh
pip install mido python-rtmidi
python3 tools/fm1_install.py --info                     # what it runs now (read-only)
python3 tools/fm1_install.py build/arctic-scream.fwsc --yes
```

- It installs from V15 (`FM-1_015`), from GR!MM or from any other firmware.
- If an install is cut off, the FM-1 comes back in update mode. Run the same command again to finish the write.

## Release and publish

- **Code:** `main` on https://github.com/PolarPatch/Arctic-scream-FM-1. The commits are authored as stevenilsen
  (GitHub noreply).
- **Releases:** push a tag `vX.Y`.
  - `.github/workflows/release.yml` calls the Firmware Hub's shared `zvenson/fm1-build/.github/workflows/build.yml@v1`,
    which builds `build/arctic-scream-X.Y.fwsc` on GitHub, attests it and publishes it as the release.
  - **Never upload or replace a package by hand**: the hub's "Built from source" key needs the attested file.
  - Verify a release with
    `gh attestation verify arctic-scream-X.Y.fwsc --repo PolarPatch/Arctic-scream-FM-1 --signer-workflow zvenson/fm1-build/.github/workflows/build.yml`.
  - The workflow is pinned to the tag `@v1`, not to a commit, because the hub only trusts `@v1`.
- **Pages:** https://polarpatch.github.io/Arctic-scream-FM-1/ is served from the `gh-pages` branch, which holds the
  contents of `web/dist`. It is updated by hand:
  1. build `web/dist` and run the tests;
  2. copy `web/dist` into a checkout of `gh-pages`;
  3. commit and push.
- **Pictures:**
  - `docs/screens/howl.png` is the 240 × 240 HOWL EDIT screen (from the host build), used for the hub.
  - `docs/screens/demo.png` is the demo's panel, used on the README.
  - The manual's screens, `docs/manual/screens/*.png` (960 × 960), come from the host build: `python3 tools/make_screens.py`
    (or a name, e.g. `drone`) takes them again after a change to the screens. `docs/manual/banner.png` is the
    manual's banner.

## Firmware Hub status (v1.0, 2026-10-25)

| Requirement | Status |
|---|---|
| GPL-3.0 with public source | done |
| `.fwsc` on a GitHub release, built and attested | done |
| Unique identity `FM-1_977` | done |
| Update loader included | done |
| Installed from V15 | passed |
| Installed from another firmware (GR!MM) | passed, with the release file |
| Leaving it: GR!MM installed over Arctic Scream | passed |
| Answers the identity request with `FM-1_977` over USB | passed |
| Interrupted update | passed: cut at 28 %, the FM-1 came back as `ota-FM-1_900`, the write finished |
| README says it does not read other firmwares' projects | done |
| Submission email to the hub | to be sent by PolarPatch |

## Ideas for later

- A generator after Unholy Union.
- **Holvut does not follow the keys or OCT−/OCT+ (found 2026-10-26, kept as it is for now).**
  - What: the loop howls on the partial its own outer loop favours. F2, F1 (OCT−) and F0 (OCT− twice) all howl around
    616 Hz; F3 around 353 Hz. VA and the other sources follow the octave correctly.
  - Why: two loops in one. The string (`HW_SL`, tuned to the note) sits inside an outer loop through DRIVE, DAMP, the
    high-pass, the two all-passes (`HW_A1`/`HW_A2`, fixed) and BODY's line (fixed ms). With FEEDBACK high the outer
    loop dominates and the note only nudges which partial wins. Below ~43 Hz the string line (`HW_SL` 1024) folds the
    note an octave up, so OCT− twice sounds as once.
  - Possible change: make the whole loop scale with the note's period. BODY as a share of the period (a prototype,
    `r = 32 + BODY × 480 / 127` in Q8, gave F2 → 436 Hz and F1 → 219 Hz, a clean octave; F3 was still off), and the
    all-pass lengths scaled too. Reaching ~22 Hz needs `HW_SL` 2048 and longer BODY/all-pass lines: move `ph_slot`
    (phys.c, ~24 KB) into the pool.
  - Cost: the character changes (more tonal, less wild), BODY becomes a ratio instead of ms, and the six presets need
    retuning by ear.
  - Also: OCT−/OCT+ only move notes played after the press; a latched drone keeps its pitch.
- Tune the HOME waveform look for ICE WRAITH, and a stronger HOWL burst.
- Move the redo buffer (`store.c` `redo_b`, 5 KB) into the pool now that the pool has room, to free main RAM.
- Store `wv_d` (HOME's drawn waves) as 8-bit values instead of 32-bit: saves ~1.2 KB of main RAM.
- SATURATION's curve (`tapefx.c` `tf_curve`) in 16 bits instead of 32: saves ~3 KB, but needs a listening check.
- Our own factory samples, through `tools/gen_samples.py`.
- Publish GitHub Pages automatically from `main` with a workflow, instead of by hand.
