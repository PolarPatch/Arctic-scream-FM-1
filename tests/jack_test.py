#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.2) SOUND AUTO in host mode (tests/host.c's "jack 1|0"): with the jack unread, and with headphones in, the sound
is the whole range, as before; with nothing plugged in, the speaker's curve (its lows taken away); a plug pulled out
in the middle turns the curve on from then on
    python3 tests/jack_test.py build/host/arctic"""
import array
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/jacktest")
OUT.mkdir(parents=True, exist_ok=True)
bad = 0


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  jack: {what}" + (f" ({detail})" if detail and not ok else ""))
    bad += not ok


def run(name, ev):
    (OUT / f"{name}.txt").write_text("".join(f"{t} {e}\n" for t, e in sorted(ev, key=lambda x: x[0])) + "6000 end\n")
    subprocess.run([BIN, str(OUT / name), str(OUT / f"{name}.txt")], capture_output=True, check=True)
    return array.array("h", (OUT / f"{name}.wav").read_bytes()[44:])


NOTE = [(1000, "keys 0x1"), (5500, "keys 0")]   # (the lowest key held)
none = run("unread", NOTE)
phones = run("phones", [(10, "jack 1")] + NOTE)
spk = run("speaker", [(10, "jack 0")] + NOTE)
pulled = run("pulled", [(10, "jack 1"), (3000, "jack 0")] + NOTE)
check("unread: as before; headphones in: the same", none == phones)
check("nothing plugged in: the speaker's curve", none != spk)
cut = 2 * 44100 * 29 // 10                       # (2.9 s: the audio is a block ahead of the 3 s)
check("a plug pulled out: the same until then", pulled[:cut] == phones[:cut])
check("and the speaker's curve after", pulled[cut + 2 * 44100 // 2:] != phones[cut + 2 * 44100 // 2:])
sys.exit(1 if bad else 0)
