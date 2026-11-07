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
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-1.5}\"" ${MACHO:+"$MACHO"} -Ifirmware/src -Ifirmware/gen -o build/host/grimm firmware/src/grimm.c
GRIMM_MASTER=1022 build/host/grimm build/host/grimm   # (GR!MM V2: MASTER as web/test_web.mjs turns it: 1022, Q12 4080)
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
# GR!MM V2: MIDI. The TRS jack's parser (tests/midi_uart_test.c); the clock in and out, MIDI CHANNEL's CH n > SEL
# (tests/clock_test.py: host mode's "clock", "trs" and "probe")
cc -O2 -Wall -Wno-unused-function -Ifirmware/hal -o build/host/midi_uart_test tests/midi_uart_test.c
build/host/midi_uart_test
python3 tests/clock_test.py build/host/grimm
# GR!MM V2: the effects (tests/fx_test.py): the track filter's TYPE and POS, MASTER, DRIVE, their saves
python3 tests/fx_test.py build/host/grimm
# GR!MM V2: the quick wins (tests/quick_test.py)
python3 tests/quick_test.py build/host/grimm
# LATCH: every factory sound, SYSTEM 3 both ways, EDIT, SEL + OCT+, slots, the project, PHYS (tests/latch_test.py)
python3 tests/latch_test.py build/host/grimm
# GR!MM V2: GENERATE (FX held + the black keys): in the SCALE, the pattern unheard and unchanged (tests/gen_test.py)
python3 tests/gen_test.py build/host/grimm
# GR!MM V2: the gain staging: no clip at any MASTER, SPACE and DELAY under their knee (tests/level_test.py)
python3 tests/level_test.py build/host/grimm
# GR!MM V2 rc9: the USB audio ring (firmware/src/uac_ring.h) in time: no dropout at any CPU load
cc -O2 -Wall -Ifirmware/src -o build/host/uac_ring_test tests/uac_ring_test.c
build/host/uac_ring_test
# GR!MM V2.0 rc11: no flash step (autosave, the settings' save) while a computer reads the USB audio: it stopped the key
# scan, the LEDs and the stream ~50 ms (tests/rc11_test.py)
python3 tests/rc11_test.py build/host/grimm
# (1.5) the screen reader: what it says, when, and nothing before a reader asks (tests/reader_test.py)
python3 tests/reader_test.py build/host/grimm
