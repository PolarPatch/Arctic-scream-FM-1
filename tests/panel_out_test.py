#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 1.5.2's PANEL OUT in host mode (tests/host.c's "panel:" lines; firmware/src/panel_out.c):
  - nothing is sent before an app's HELLO
  - the HELLO's answer: the version, key 0's MIDI note, the octave, the keys, the buttons and the firmware's name;
    then STATE and MASTER's place
  - a button, a key: STATE as it changes; a knob: its steps (at most one message every 20 ms, none lost)
  - BYE: silent again; an app not heard from for 5 s: silent
    python3 tests/panel_out_test.py build/host/grimm"""
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/grimm"
OUT = Path("build/paneltest")
OUT.mkdir(parents=True, exist_ok=True)
HELLO, BYE = "sysex 7D 50 4F 01 01", "sysex 7D 50 4F 02"
fails = 0


def check(name, ok, info=""):
    global fails
    print(f"  {'ok  ' if ok else 'FAIL'}  panel: {name}" + ("" if ok else f"  ({info})"))
    fails += not ok


def run(name, ev, end):
    p = OUT / f"{name}.txt"
    p.write_text("".join(f"{t} {s}\n" for t, s in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    out = subprocess.run([BIN, str(OUT / name), str(p)], capture_output=True, text=True, check=True).stdout
    return [(int(m.group(1)), [int(x) for x in m.group(2).split()])
            for m in re.finditer(r"^\s*(\d+) ms  panel: (.*)$", out, re.M)]


got = run("quiet", [(6500, "btn 6"), (7000, "enc 2 5")], 9000)
check("nothing sent before a HELLO", not got, got[:2])

got = run("main", [(t, HELLO) for t in range(6000, 12000, 2000)] + [
    (6500, "btn 6"),                             # EDIT
    (6600, "keys 5"), (6800, "keys 0"),          # keys 0 and 2
    (7000, "enc 2 5"), (7005, "enc 2 3"),        # KNOB 1: 5, then 3 within 20 ms
    (7200, "enc 0 -2"),                          # SELECT
    (7400, "btn 1"),                             # OCT+
    (12500, BYE), (13000, "btn 6"), (13200, "enc 3 4")], 14000)
hello = [m for t, m in got if m[0] == 1]
name = bytes(hello[0][6:]).decode() if hello else ""
check("the HELLO's answer: version 1, key 0 F2 (41) at octave 0, 27 keys, 14 buttons, the name",
      bool(hello) and hello[0][1:6] == [1, 41, 64, 27, 14] and name.startswith("Arctic Scream"), hello[:1])
check("an answer to each HELLO", len(hello) == 3, len(hello))
st = [(t, m[1:]) for t, m in got if m[0] == 16]
check("STATE after the HELLO: nothing down, octave 0", bool(st) and st[0][1] == [0, 0, 0, 0, 0, 0, 64], st[:1])
check("EDIT down: bit 6", any(m[0] == 64 for t, m in st if 6500 <= t < 6600), [s for s in st if 6500 <= s[0] < 6600])
check("keys 0 and 2 down, then up", any(m[2] == 5 for t, m in st if 6600 <= t < 6800) and
      any(m[2] == 0 for t, m in st if t >= 6800), [s for s in st if 6600 <= s[0] < 6900])
check("OCT+: the octave + 1 (65)", any(m[6] == 65 for t, m in st if t >= 7400), [s for s in st if s[0] >= 7400][:2])
kn = [(t, m[1:]) for t, m in got if m[0] == 17]
k1 = [(t, m) for t, m in kn if m[0] == 0]
check("KNOB 1: 8 steps in all, as relative (steps + 64)", sum(m[2] - 64 for t, m in k1) == 8 and
      all(m[1] == 0 for t, m in k1), k1)
check("KNOB 1: no two messages within 20 ms", all(b[0] - a[0] >= 20 for a, b in zip(k1, k1[1:])), k1)
check("SELECT: id 4, -2", any(m[0] == 4 and m[2] == 62 for t, m in kn), [k for k in kn if k[1][0] == 4])
check("MASTER: its place after the HELLO (id 7, absolute, 127 here)", any(m == [7, 1, 127] for t, m in kn), kn[:3])
check("BYE: silent", not [g for g in got if g[0] > 12500], [g for g in got if g[0] > 12500][:2])

got = run("gone", [(6000, HELLO), (13000, "btn 6")], 14000)
check("an app not heard from for 5 s: silent", not [g for g in got if g[0] > 11500], [g for g in got if g[0] > 11500][:2])

if fails:
    sys.exit(1)
print("panel_out: PANEL OUT ok")
