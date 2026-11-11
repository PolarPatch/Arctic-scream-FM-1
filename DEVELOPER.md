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
  - Code: `firmware/src/howl.c` and `howl.h`. Its state shares the PHYS slot (`phys.c` `ph_slot`); only BODY's line
    (4096 frames a voice) lives apart, in the voice's pool scratch (`voices.c` `vx_pool`, 48 KB for the six), which
    SAMPLE's windows (`sampler.c` `sp_w`) share: a voice plays one source at a time, and each begins anew on it.
  - The loop: noise → Karplus-Strong string → DRIVE saturation → DAMP low-pass → high-pass (~70 Hz; overblown, two
    poles up to ~1.6 kHz) → four all-passes → × FEEDBACK → BODY's delay line → back into the string.
  - BODY runs from ~1 to ~92 ms: as 1.1's up to 100 (~9 ms), as 1.2's up to 115 (~22 ms), then on to ~92 ms.
  - FEEDBACK runs from −60 to +12 dB and wakes the loop at about two thirds. A key adds a 50 ms burst. Over 119 it
    also lifts the high-pass (`hw_hpc`), with up to 5× level back on the output (the overblown howl is thinner), so the
    howl leaps up its overtones; no preset goes past 112 (with DRONE's walk, 118), so they sound as before.
  - MIDI pressure (0xD0, or 0xA0's value, on the track's channel; `hw_press`) pushes FEEDBACK toward 127. FEEDBACK and
    the pressure glide (~6 ms, `hw_glide`).
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

- **GR!MM V2.0 in 1.3** (cherry-picked onto the branch with `-x`, so `git log` names each upstream commit; local tags
  `grimm-v1.0`, `grimm-v2.0` are GR!MM's own, kept apart from ours so a `v*` push never starts our release):
  - Taken: the RAM savings (steps as bytes, TAPE's curve in 16 bits), the CPU meter's split (`tools/fm1_cpu.py`), MIDI
    CHANNEL's CH n > SEL, TRS MIDI IN (`midi_uart.c`), the MIDI clock (`clock.c`), the track filter's TYPE and POS,
    MASTER and DRIVE (`master.c`), SYSTEM 3's LATCH, SEL + OCT+, REC held, the editor's settings and sample backups,
    the voice rule and PHYS at most four, GENERATE (`gen.c`), the gain staging, and from rc9–rc12: the USB audio ring
    (`uac_ring.h`), the saves under endless tails, no flash step while a computer records, the reset reasons, values
    loaded within range, the overload guard's order.
  - Not taken: the choirs, MIXER 1's six tiles and TAPE per track, PHASE's LINE 2, GLIDE, VELOCITY, the new boot and
    HOME pictures, the 24-bit output path (`PB`) and the rest of 4a25a02.
  - Ours, where they met: the editor's commands CPU and FX are **25** and **26** (24 is SEQ); the sample objects are
    **5–8** (our 44 factory sounds end at 123); the track filter's TYPE and POS sit in bits 4–6 of a track's byte 45 (ARP's
    RATE, 0..8; GR!MM's byte 32 is DRONE's BORGA here), no size change; MASTER and DRIVE in the working project, version
    **8** (ours; 7 is 1.0–1.2); the page enum keeps DRONE at 25, then MASTER 26, DRIVE 27, SYSTEM 3 28 (MIDI LEARN's
    links); DRIVE's gain is `drv_gain` (DRONE has a `dr_gain`); DRIVE's level table is made for our quieter mix
    (`tools/gen_drive_levels.py`, RMS 1000–4000).
  - GR!MM's tests run with `GRIMM_PLAIN=1` (host mode: the plain VA tracks GR!MM starts with, not ICE WRAITH) and our
    keys an octave down; `tests/compat.c`'s saves are Arctic Scream 1.2's (`tests/data/a1.2-*`).

### The screen reader (1.5)

- **What it is.** The FM-1 tells [fm1-reader](https://github.com/charlesvestal/fm1-reader)'s page what its screen
  shows, over SysEx (`F0 7D 53 52 ...`, its `PROTOCOL.md`). `firmware/src/announce.c` (fm1-base's, MPL-2.0) keeps the
  protocol; `firmware/src/reader.c` says what changed: a page, a knob, a list's row, the header's messages (`hd_say`),
  PLAY, STOP, REC. Nothing is sent before a reader's HELLO.
- **The words.** `firmware/words.tsv` says how the screen's text is spoken ("SHIMMR": shimmer). Every build runs
  `tools/gen_words.py`, which writes `web/words/<id>.json` and the link in `firmware/gen/ann_words.h`. A changed table
  makes a new file: commit it, publish it with the site (`build_web.sh` copies `web/words/` to `words/`), and never
  change or delete a published one: an FM-1 in the wild points at it.
- **Updating the shared files.** Change nothing in `announce.c` or `gen_words.py`; run fm1-reader's
  `tools/sync_announce.sh ../arctic-drones` (fm1-base next to it), then `tools/reader_audit.sh` again.
- **Cost.** ~6 KB of flash and ~580 B of RAM (`ANN_MAX` 160, so a SAY fits the SysEx queue of 64 packets as it is).
- **Checks.** `tools/reader_audit.sh` (0 errors; three warnings kept: "nothing to undo", "nothing to redo", "nothing
  copied" say what did not happen) and `tests/reader_test.py`. On the device: fm1-reader's
  `tools/fm1_reader.py --where --seconds 20` while turning knobs and changing pages.
- **Not yet.** The browser demo does not speak (`web/fm1_web.c`'s `ann_wire_send` refuses); fm1-reader's PAGES.md
  describes how.

## Build and test

```sh
# host build: screens and sound; PNGs with Pillow (not sips: test_web compares them)
cc -O2 -Wall -Wno-unused-function '-Dsection(x)=used' -DDINGHY_HOST -DAPP_VER='"1.3"' -Ifirmware/src -Ifirmware/gen \
   -o build/host/grimm firmware/src/grimm.c
build/host/grimm build/host/grimm        # (or: tests/run_host.sh)
build/host/grimm OUT script.txt FPS      # script mode: "3100 keys 0x80", "3500 btn 6", "5000 enc 2 -45", "end"

# web: the demo, the editor, the cheat sheet into web/dist, then the tests (wasm against host mode, the editor's core)
source ~/emsdk/emsdk_env.sh; bash web/build_web.sh
node web/test_web.mjs                     # must say "test_web: all ok"

# tests/run_host.sh also runs GR!MM's tests: clock, fx, quick, latch, gen, level, the USB ring, rc11, and (1.5) the
# screen reader's (all "passed")

# the screen reader's words (1.5): fm1-reader next to this repo (../fm1-reader); must say "0 errors"
sh tools/reader_audit.sh                  # --spoken: every message as it will be heard

# stress test, seeds 1-3: each must print STRESS OK (see tests/stress.sh for the build line)
python3 tests/stress_gen.py 1 build/stress/script.txt build/stress/checks.txt
STRESS_CHECKS=build/stress/checks.txt build/stress/grimm build/stress/s build/stress/script.txt

# the real firmware (JieLi toolchain; on macOS through Docker, linux/amd64)
./build.sh                                # -> build/arctic-scream.fwsc; checks RAM, pool and the image size
python3 tools/check_mul64.py              # must find 0: no 64-bit multiply beside 16-bit loads (see below)
```

On macOS, tests/stress.sh stops at its device check (`tests/device_names.py` runs the toolchain's clang without
Docker, and needs gcc's `-Wredundant-decls`). Run the stress build by hand with `'-Dsection(x)=used'` added, as
`tests/run_host.sh` does.

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
- **Ideas from Audrey Touch (noted 2026-11-02, waiting).** [AudreyTouch](https://github.com/Synthux-Academy/AudreyTouch)
  (MIT, Infrasonic Audio / Synthux Academy; already credited for Holvut's design) is Audrey II on the Simple Touch:
  pads, a line input, simpler to play. Its loop (`Source/FeedbackSynthEngine.cpp`, `Engine::Process`): a Karplus
  string fed by the body line's return, white noise at -90 dB and the input (left channel, its own gain); a fixed
  overdrive, LP and HP; the reverb inside the loop; written back through FEEDBACK x the envelope (1 in DRONE). Ideas,
  to be written from scratch as Holvut was (no code copied):
  - **A sample as the exciter (the first choice):** one of the four sample slots fed into Holvut's loop instead of
    straight out, so the howl grows over a choir, a voice or drums uploaded from the editor. The FM-1 has no audio input
    in this firmware (ALNK0 is output only), so this is its stand-in for Touch's line in.
  - **The computer's audio into the loop over USB:** a UAC playback interface (an isochronous OUT endpoint) so the
    FM-1 becomes a feedback effect for anything the computer plays. Large: the chip doing audio both ways at once is
    untested, and it touches the USB layout that 1.3.1 made work on macOS 10.14 to 15 (test those again).
  - **Reverb in the loop:** a share of SPACE back into Holvut, so the howl blooms as Touch's SIZE does (Holvut has only
    its four all-passes now).
  - **The envelope on FEEDBACK:** a setting where the loop dies when the key is let go, instead of howling on.
  - Touch's right switch (a random target each half cycle, slewed: 1-8 Hz or 0.01-0.5 Hz on BODY) is close to
    DRONE's BORGA; its pad scale 2 (0, 5, 6, 9, 10, 12, 13) is close to HORROR.
- **USB audio per track (asked for in issue #1, 2026-11-03).** Record each track on its own channels over USB.
  - The idea: 10 channels, each track's own stereo pair (after its filter, LEVEL and PAN, from `mix_tracks` in
    `grimm.c`) plus the master pair. The stems are dry: ECHO, REVERB, GRAIN and the master stage are shared and come
    after the mix. The master pair stays the full mix.
  - Bandwidth fits: 10 ch x 2 B x 49 samples = 980 B a frame at 48 kHz, under full speed's 1023.
  - Open questions:
    - Main RAM (~2 KB free): the ring grows fivefold, so it must move into the pool.
    - CPU: the 48 kHz resampler (`uac_tap48`) would run five times over. Measure with SYSTEM 2 first.
    - The JieLi SIE's EP4 FIFO size.
    - A setting to choose 2 or 10 channels, since 2 is simpler for most hosts.
    - The USB layout: test macOS 10.14 to 15 again.
- **The computer's audio into Holvut's loop over USB (waiting, 2026-11-03).** When it is built, support only the latest
  two or three macOS versions; macOS 12 and older are too far back.
- **Ideas from Book of the Dead II (DigDugDIY, noted 2026-11-03).** An analog feedback drone box with an FX send and
  return inside its loop. These are ideas only; it is hardware, so there is no code to look at.
  - **AURA (power starve):** a "starved" loop, where the gain sags and stutters at random, to make it chaotic and
    unpredictable. On Holvut this could be a random sag on FEEDBACK and DRIVE, deeper with the knob.
  - **STIRRING THE POT:** a momentary push to the gain structure. On the FM-1 this could be a key or button held for a
    FEEDBACK burst; aftertouch already does something close.
  - **CLOSE / OPEN BOOK:** stop or start the drone. LATCH plus GATE now cover this.
  - **THIRD EYE (self-oscillation amount)** is FEEDBACK. **The FX loop** is BLOOM, done in 1.4.
- **What ECHOFIELD showed (noted 2026-11-04).** [echofield-fm1](https://github.com/Philip-Bos/echofield-fm1) (MIT,
  Philip Bos) is a granular sampler for the FM-1 built on fm1-base. 1.4 took three of its ideas, written again in
  integers (no code copied; credited in the sources): PING-PONG and SHIMMER (FX's AIR page) and the side limiter on the
  output. Two more are bigger and wait:
  - **The FPU.** Its DESIGN.md says the chip (pi32v2, 360 MHz) has a single-precision FPU, and its whole engine is
    float (FRASIL's too, below). This firmware has assumed no FPU and no libgcc from the start (`grimm.c`'s rules, `tools/build.py` builds
    without float flags). Test it first: a small float loop built with the toolchain's FPU flags, timed on the FM-1
    against the integer version. If it holds, new effects could be written in float.
  - **The second core.** ECHOFIELD renders half its grains on the chip's second core (fm1-base's `cpu2`, with a
    fallback to one core if the second fails) and measured 28-54 % peaks while playing hard. This firmware uses one
    core. Moving the voices or the effects to the second core would be the biggest gain in headroom, and the biggest
    job: boot the core, share the buffers, and keep a fallback.
  - Its hardware notes also report that fm1-base's USB audio can splice whole 256-sample blocks. This firmware's own
    USB ring (1.3.1) is a different path, but it is worth a listen with a plain tone.
- **64-bit multiplies beside 16-bit loads (fixed in 1.4).** felucca-bsp's `docs/CPU.md` (found through FRASIL, below)
  reports that on the FM-1 a 32x32->64-bit multiply inlined among 16-bit loads now and then leaves a register one bit
  off, and the firmware crashes. fm1-radio's AAC decoder crashed within a minute that way. `tools/check_mul64.py`
  found nine such places here:
  - FM6's operators (`fm6_op_run`, `fm6_op_fb`): now `fm6_mulg`, two 32-bit products, the same bits.
  - PHYS's sympathetic strings (`px_symp_run`): the same split.
  - FORMANT's coefficients, PERFORM's history, the LFO's DRIFT shape and LOOP's BLOOM: now `dsp.c`'s `mul64_sh`, out
    of line.

  The stress test's sound is bit for bit the same (seeds 1-3), and both splits were checked against 64 bits over
  200 million random inputs each. Nobody had reported such a crash, but it is rare and random; FM6 was the most
  exposed. Not measured yet: FM6's CPU on the FM-1 after the change.
- **What FRASIL showed (noted 2026-11-05).** [frasil](https://github.com/styvesgoupil/frasil) (MPL-2.0, Styves
  Goupil) is an ambient drone firmware for the FM-1 built on fm1-base, much praised. Ideas only, nothing copied:
  - **FREEZE as a hold.** Hold FX: the reverb holds what is in it (loop gain to ~1, damping off, input faded out over
    ~0.1 s); let go and it thaws over DECAY. SPACE's SIZE 127 (INF) already does this, but only by turning the knob
    away from the size you had. A hold combo would make it a performance move.
  - **Its reverb** is an 8-line float FDN at the full 44.1 kHz with an allpass inside each line, RT60 set per line,
    shimmer with BRIGHT and a choice of interval. It needs ~128 KB of pool; this firmware has 2 KB left.
  - **Load shedding.** It measures the render as it goes and, near the budget, drops partials and then voices,
    letting the quietest released ones go first, so the audio never breaks up.
  - **ENV held: SWELL**, every drone voice swells together; and **COMPARE**, the scene as saved.
  - Its engine builds alone (`engine/`), with tests that compare render hashes between the simulator and the engine,
    and a bench that counts instructions against the FM-1's budget.
- **The key lights after OCT-/OCT+ (fixed in 1.4).** A YouTube comment on GR!MM's video said the key lights do not
  follow octave changes; this firmware had the same bug. A key's latched note (and an ARP key) was lit on the key it
  was played on, whatever the octave. Each note now keeps the octave it was played in (`lat_oct`, the arp's `oct`),
  and its light moves with OCT-/OCT+ (off the keys: none lit). The same comment asked for a sign when all six voices
  are in use; not done yet.
- **HOME's sky as northern lights, or snow and rain (prototype 2026-11-03, set aside).**
  - Where: the local branch `proto/home-aurora` (commit d9a734b), not pushed. It is off unless built with
    `HOME_SKY 1`.
  - What it does: four aurora curtains over HOME's stars, one per track, each as bright as its track sounds. A
    curtain is walked along its length and folds over itself; the hem ripples, the rays fade from green up to violet
    in seven posterized steps with Bayer at the edges, and a 5-wide blur softens it.
  - Source of the idea: [pixelfx](https://github.com/patch-gg/pixelfx) `src/fx/weather/aurora.js` (MIT, patch-gg),
    itself after Sprite Fusion's holy light (MIT, Hugo Duprez). Written again in integers, no code copied; credit
    both if it ships. The same gallery has snow, blizzard and rain.
  - Measured on the FM-1:
    - CPU NOW 23.4 %, PEAK 38.2 % (lightly played);
    - pool +3.3 KB (1.7 KB left), flash +1.8 KB;
    - about 9 frames a second, because the whole field takes ~42 ms on the panel's 12 MHz SPI.
  - Why it was set aside: the picture was not good enough. The strands came out streaky and the hem did not burn.
  - Next time: try 2 x 2 pixel cells (pixel-art look, a quarter of the work), a real bloom, fewer and wider curtains,
    or a screensaver instead of HOME so the speed matters less.
- Tune the HOME waveform look for ICE WRAITH, and a stronger HOWL burst.
- Move the redo buffer (`store.c` `redo_b`, 5 KB) into the pool now that the pool has room, to free main RAM.
- Store `wv_d` (HOME's drawn waves) as 8-bit values instead of 32-bit: saves ~1.2 KB of main RAM.
- SATURATION's curve (`tapefx.c` `tf_curve`) in 16 bits instead of 32: saves ~3 KB, but needs a listening check.
- Our own factory samples, through `tools/gen_samples.py`.
- Publish GitHub Pages automatically from `main` with a workflow, instead of by hand.
