#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 2.1.5's SCREEN MIRROR in host mode (tests/host.c's "mirror" lines; firmware/src/mirror.c), the screen
as an app reads it (the Big Screen protocol):
  - nothing is sent before an app's hello, nor for a hello longer than six bytes
  - every message well formed (F0 7D 46 53 .. F7, 70 bytes at most, 7-bit, runs inside the screen)
  - the rows the mirror counts as sent are the screen's, pixel for pixel, however slow USB is (a message a
    millisecond, three, as fast as it goes); the header and the slabs all there; only rows still moving may wait
  - the sound's name after each hello; 02: the name alone, no screen
  - 3 s after the last hello, silent
    python3 tests/mirror_test.py build/host/arctic"""
import os
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/mirrortest")
OUT.mkdir(parents=True, exist_ok=True)
HELLO, AGAIN, NAME = "sysex 7D 46 53 00", "sysex 7D 46 53 01", "sysex 7D 46 53 02"
fails = 0


def check(name, ok, info=""):
    global fails
    print(f"  {'ok  ' if ok else 'FAIL'}  mirror: {name}" + ("" if ok else f"  ({info})"))
    fails += not ok


def run(name, ev, end, rate=0):
    p = OUT / f"{name}.txt"
    p.write_text("".join(f"{t} {s}\n" for t, s in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    env = dict(os.environ, ARCTIC_MR_RATE=str(rate))
    out = subprocess.run([BIN, str(OUT / name), str(p)], capture_output=True, text=True, check=True, env=env).stdout
    names = [(int(m.group(1)), m.group(2)) for m in re.finditer(r"^\s*(\d+) ms  mirror name: (.*)$", out, re.M)]
    m = re.search(r"^mirror: (\d+) messages, (\d+) bytes, the last at (\d+) ms, (\d+) bad, (\d+) pixels differ", out, re.M)
    w = re.search(r"^mirror: (\d+) rows waiting for a repair(.*)$", out, re.M)
    rows = [tuple(map(int, r.split(".."))) for r in w.group(2).split()] if w else []
    return names, (tuple(map(int, m.groups())) if m else None), rows


names, got, _ = run("quiet", [(4000, "btn 6"), (5000, "enc 2 5"), (6000, "sysex 7D 46 53 00 00")], 9000)
check("nothing sent before a hello, nor for a longer one", not names and not got, (names[:1], got))

PLAY = [(t, HELLO) for t in range(4000, 15001, 1000)] + [
    (5000, "btn 6"), (6000, "enc 2 5"), (7000, "enc 0 1"), (8000, "btn 3")]
for rate in (0, 1, 3):
    names, got, rows = run(f"play{rate}", [(3000, AGAIN)] + PLAY, 16000, rate)
    speed = f"{rate} a ms" if rate else "as fast as it goes"
    check(f"{speed}: the screen sent ({got[0] if got else 0} messages), none malformed", bool(got) and got[0] > 100 and
          got[3] == 0, got)
    check(f"{speed}: the rows sent are the screen's, pixel for pixel", bool(got) and got[4] == 0, got)
    still = [r for r in rows if r[0] < 40 or r[1] >= 150]
    check(f"{speed}: the header and the slabs all there (only the moving rows {rows} may wait)", not still, still)
    if rate == 0:
        check("the sound's name after each hello", len(names) >= 12 and all(n.startswith("T1 ") for t, n in names),
              names[:3])

names, got, _ = run("name", [(t, NAME) for t in range(3000, 9001, 1000)], 10000)
check("02: the name alone, no screen", len(names) >= 6 and not got, (names[:2], got))

names, got, _ = run("gone", [(3000, AGAIN), (4000, HELLO), (5000, HELLO)], 12000)
check("3 s after the last hello: silent", bool(got) and got[2] < 5000 + 3000 + 50, got)

print("mirror: all ok" if not fails else f"mirror: {fails} FAILED")
sys.exit(1 if fails else 0)
