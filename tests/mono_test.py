#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 2.0's MONO (SYSTEM: COLOUR's fifth) in host mode: turned to, the screen black and white alone; the
setting saved; the settings saved with it, the boot screen and HOME black and white from the start; back to ICE,
the colours again.
    python3 tests/mono_test.py build/host/grimm"""
import os
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/grimm"
OUT = Path("build/monotest")
OUT.mkdir(parents=True, exist_ok=True)
fails = 0


def check(name, ok, info=""):
    global fails
    print(f"  {'ok  ' if ok else 'FAIL'}  mono: {name}" + ("" if ok else f"  ({info})"))
    fails += not ok


def colours(ppm):                            # the colours a frame has
    b = ppm.read_bytes()
    head, n = [], 0
    while len(head) < 4:                     # P6 w h max
        j = b.index(b"\n", n)
        head += b[n:j].split()
        n = j + 1
    px = b[n:]
    return {px[i:i + 3] for i in range(0, len(px), 3)}


def run(name, ev, end, col=None):
    for p in OUT.glob(f"{name}-*.ppm"):
        p.unlink()
    s = OUT / f"{name}.txt"
    s.write_text("".join(f"{t} {l}\n" for t, l in sorted(ev)) + f"{end} end\n")
    env = dict(os.environ)
    if col is not None:                      # version 5's settings (172 B), COLOUR as given
        b = bytearray(172)
        b[2], b[167], b[170] = col, 5, 2
        (OUT / f"set_{name}.bin").write_bytes(bytes(b))
        env["GRIMM_SETTINGS_IN"] = str(OUT / f"set_{name}.bin")
    out = subprocess.run([BIN, str(OUT / name), str(s), "1"], capture_output=True, text=True, check=True,
                         env=env).stdout
    return out, sorted(OUT.glob(f"{name}-*.ppm"))


BW = {b"\x00\x00\x00", b"\xff\xff\xff"}
# SYSTEM (HOME held), KNOB 1 four steps: ICE .. MONO; then EDIT (SOUND), a key
out, fr = run("turn", [(6200, "down 8"), (6900, "up 8"), (7200, "enc 2 4"), (8000, "btn 6"), (8500, "keys 1"),
                       (9500, "keys 0")], 16000)
c = colours(fr[-1])
check("turned to MONO: the screen black and white alone (SOUND, a key played)", c <= BW and len(c) == 2, len(c))
c = colours(fr[5])
check("before: the colours (HOME)", len(c) > 2, len(c))
# saved as MONO: from the start
out, fr = run("saved", [(7000, "keys 1"), (8000, "keys 0")], 9000, col=4)
check("saved as MONO: the boot screen black and white", all(colours(f) <= BW for f in fr[1:5]),
      [len(colours(f)) for f in fr[1:5]])
check("saved as MONO: HOME black and white", colours(fr[-1]) <= BW, len(colours(fr[-1])))
# saved as MONO, KNOB 1 back to ICE
out, fr = run("back", [(6200, "down 8"), (6900, "up 8"), (7200, "enc 2 -4"), (8000, "btn 6")], 16000, col=4)
check("MONO, back to ICE: the colours again", len(colours(fr[-1])) > 2, len(colours(fr[-1])))
check("COLOUR turned: saved (172 B, once all is silent)", "settings saved (172 B)" in out)

if fails:
    sys.exit(1)
print("mono: MONO ok")
