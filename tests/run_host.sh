#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# GR!MM changes (file names): Copyright (C) 2026 GR!MM
# Run firmware/src/grimm.c on this computer (tests/host.c): build/host/grimm.wav and the screens as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
python3 tools/gen_words.py firmware/words.tsv web/words firmware/gen/ann_words.h >/dev/null   # (1.5: the reader's words)
MACHO=                                          # (macOS: Mach-O has no ".pool" section; the pool is plain .bss there)
[ "$(uname -s)" = Darwin ] && MACHO="-Dsection(x)=used"
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-2.0}\"" ${MACHO:+"$MACHO"} -Ifirmware/src -Ifirmware/gen -o build/host/grimm firmware/src/grimm.c
GRIMM_MASTER=1022 build/host/grimm build/host/grimm   # (MASTER as web/test_web.mjs turns it: 1022, Q12 4080)
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob("grimm-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
# saves across versions (tests/compat.c): the project Arctic Scream 1.2 saved (tests/data/a1.2-saves.bin) loads as 1.2 loaded it
mkdir -p build/compat
GRIMM_SAVE_IN=tests/data/a1.2-saves.bin GRIMM_STATE_OUT=build/compat/a1.2-state.txt build/host/grimm build/compat/run \
    tests/data/compat.txt > /dev/null
if cmp -s tests/data/a1.2-state.txt build/compat/a1.2-state.txt; then
    echo "saves  Arctic Scream 1.2's project loads as in 1.2"
else
    echo "saves  FAILED: Arctic Scream 1.2's project loads otherwise (diff tests/data/a1.2-state.txt build/compat/a1.2-state.txt)"
    exit 1
fi
# the update loader's screen (tests/ldr_screen_test.c): UPDATE MODE, WRITING and its bar, DONE, FAILED
cc -O2 -Wall -Ifirmware/gen -o build/host/ldr_screen_test tests/ldr_screen_test.c
build/host/ldr_screen_test
# (2.0) the update path (tests/update_test.c): ota.c's step and the loader's against a served package, an older
# build's flash first (the app cut short, packed by tools/fm1pkg_make.py); needs build/ from tools/build.py and the SDK
if [ -f build/arctic-scream.fwsc ] && [ -f build/grimm.bin ] && [ -d "${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}" ]; then
    head -c 60000 build/grimm.bin > build/host/old_app.bin
    AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}" python3 tools/fm1pkg_make.py build/host/old_app.bin build/loader/ota.bin build/host/old.fwsc > /dev/null
    python3 -c "import sys; sys.path.insert(0, 'tools'); import fm1_install as f
for a, b in (('build/host/old.fwsc', 'build/host/old.img'), ('build/arctic-scream.fwsc', 'build/host/new.img')):
    open(b, 'wb').write(f.logical_image(open(a, 'rb').read()))"
    cc -O1 -Wall -Wno-unused-function -Ifirmware/hal -Ifirmware/src -o build/host/update_test tests/update_test.c
    build/host/update_test build/host/old.img build/host/new.img
else
    echo "update_test: skipped (run tools/build.py first; the AC79 SDK for the older package)"
fi
# the flash chips (tests/flash_id_test.c): the newer FM-1s' UC25HQ80 saves as the first ones' P25Q80
cc -O2 -Wall -Ifirmware/hal -o build/host/flash_id_test tests/flash_id_test.c
build/host/flash_id_test
# MIDI. The TRS jack's parser (tests/midi_uart_test.c); the clock in and out, MIDI CHANNEL's CH n > SEL
# (tests/clock_test.py: host mode's "clock", "trs" and "probe")
cc -O2 -Wall -Wno-unused-function -Ifirmware/hal -o build/host/midi_uart_test tests/midi_uart_test.c
build/host/midi_uart_test
python3 tests/clock_test.py build/host/grimm
# the effects (tests/fx_test.py): the track filter's TYPE and POS, MASTER, DRIVE, their saves
python3 tests/fx_test.py build/host/grimm
# the quick wins (tests/quick_test.py)
python3 tests/quick_test.py build/host/grimm
# Arctic Scream 2.0: MONO, COLOUR's black and white (tests/mono_test.py)
python3 tests/mono_test.py build/host/grimm
# Arctic Scream 2.0: the drone machine: JIEHKKI, RUOHCAT, TUNING, Biegga, GUOVSSAHAS (tests/drone_test.py)
python3 tests/drone_test.py build/host/grimm
# LATCH: every factory sound, SYSTEM 3 both ways, EDIT, SEL + OCT+, slots, the project, PHYS (tests/latch_test.py)
python3 tests/latch_test.py build/host/grimm
# GENERATE (FX held + the black keys): in the SCALE, the pattern unheard and unchanged (tests/gen_test.py)
python3 tests/gen_test.py build/host/grimm
# the gain staging: no clip at any MASTER, SPACE and DELAY under their knee (tests/level_test.py)
python3 tests/level_test.py build/host/grimm
# the USB audio ring (firmware/src/uac_ring.h) in time: no dropout at any CPU load
cc -O2 -Wall -Ifirmware/src -o build/host/uac_ring_test tests/uac_ring_test.c
build/host/uac_ring_test
# no flash step (autosave, the settings' save) while a computer reads the USB audio: it stopped the key
# scan, the LEDs and the stream ~50 ms (tests/rc11_test.py)
python3 tests/rc11_test.py build/host/grimm
# (1.5) the screen reader: what it says, when, and nothing before a reader asks (tests/reader_test.py)
python3 tests/reader_test.py build/host/grimm
# (1.5.2) PANEL OUT: the panel for an app, and nothing before an app asks (tests/panel_out_test.py)
python3 tests/panel_out_test.py build/host/grimm
