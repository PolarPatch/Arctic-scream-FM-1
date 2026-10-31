#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# GR!MM changes (file names): Copyright (C) 2026 GR!MM
# Run firmware/src/grimm.c on this computer (tests/host.c): build/host/grimm.wav and the screens as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
MACHO=                                          # (macOS: Mach-O has no ".pool" section; the pool is plain .bss there)
[ "$(uname -s)" = Darwin ] && MACHO="-Dsection(x)=used"
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-1.3}\"" ${MACHO:+"$MACHO"} -Ifirmware/src -Ifirmware/gen -o build/host/grimm firmware/src/grimm.c
build/host/grimm build/host/grimm
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
