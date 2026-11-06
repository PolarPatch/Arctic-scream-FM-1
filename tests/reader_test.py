#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 1.5's screen reader in host mode (tests/host.c's "said" lines; firmware/src/reader.c, announce.c):
  - nothing is said before a reader's HELLO; the HELLO's answer names the firmware and links its words file
  - a new reader hears the whole screen once, after the boot screen; a WHERE again
  - a page changed: its name, track, source, place and knobs; a knob turned: its name and value
  - PLAY and STOP; a warning (an empty slot loaded) heard whole (ANN_KEEP); a list's row with its place
  - BYE: silent again; a reader not heard from for 5 s: silent
    python3 tests/reader_test.py build/host/grimm"""
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/grimm"
OUT = Path("build/readertest")
OUT.mkdir(parents=True, exist_ok=True)
fails = 0
HELLO, WHERE, BYE = "sysex 7D 53 52 01 01", "sysex 7D 53 52 03", "sysex 7D 53 52 02"


def check(name, ok, info=""):
    global fails
    print(f"  {'ok  ' if ok else 'FAIL'}  reader: {name}" + ("" if ok else f"  ({info})"))
    fails += not ok


def run(name, ev, end):
    p = OUT / f"{name}.txt"
    p.write_text("".join(f"{t} {s}\n" for t, s in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    out = subprocess.run([BIN, str(OUT / name), str(p)], capture_output=True, text=True, check=True).stdout
    said = [(int(m.group(1)), m.group(2) == " (keep)", m.group(3))
            for m in re.finditer(r"^\s*(\d+) ms  said( \(keep\))?: (.*)$", out, re.M)]
    hello = re.findall(r"reader HELLO: (.*)", out)
    return said, hello


def hellos(a, b):                                # a reader's HELLO every 2 s
    return [(t, HELLO) for t in range(a, b, 2000)]


said, hello = run("quiet", [(6500, "btn 6"), (7000, "enc 2 5")], 9000)
check("nothing said before a HELLO", not said and not hello, said[:2])

said, hello = run("main", hellos(1000, 20000) + [
    (7000, "btn 6"),                             # EDIT: SOUND (Holvut)
    (8000, "enc 2 6"),                           # FEEDBACK
    (9500, "btn 12"), (10500, "btn 12"),         # PLAY, STOP
    (11500, "btn 9"), (12000, "enc 6 0"), (12500, "btn 9"),   # SAVE: SOUNDS, PROJECTS
    (13500, "btn 1"),                            # OCT+: LOAD slot A (empty)
    (15000, WHERE)], 17000)
check("the HELLO's answer: the name and the words file's link",
      bool(hello) and hello[0].startswith("Arctic Scream"), hello[:1])
first = [s for s in said if s[0] < 5500]          # (the boot screen: 5.6 s)
check("not on the boot screen", not first, first[:1])
check("a new reader: the whole screen once (HOME, its track, tempo, knobs)",
      len([s for s in said if s[2].startswith("HOME, T1")]) == 1 and
      any("TEMPO" in s[2] and "VOLUME" in s[2] for s in said), [s[2] for s in said[:2]])
pg = [s for s in said if 7000 <= s[0] < 8000]
check("EDIT: the page, its track and source, its place, its knobs",
      any(s[2].startswith("SOUND, T1 Holvut (1/3).") and "FEEDBACK" in s[2] for s in pg), pg)
kn = [s for s in said if 8000 <= s[0] < 9500]
check("a knob turned: its name and value", any(re.match(r"FEEDBACK [-+]?\d+DB$", s[2]) for s in kn), kn)
check("PLAY, STOP", [s[2] for s in said if 9500 <= s[0] < 11000] == ["PLAY", "STOP"],
      [s for s in said if 9500 <= s[0] < 11000])
row = [s for s in said if 11500 <= s[0] < 13500]
check("SAVE: the list's row with its place", any(re.search(r"SLOT 1 EMPTY, 1 OF \d+", s[2]) for s in row), row)
check("PROJECTS: its slot, in use, 1 OF 4", any("SLOT A EMPTY, IN USE, 1 OF 4" in s[2] for s in row), row)
w = [s for s in said if 13500 <= s[0] < 14500]
check("a warning heard whole (ANN_KEEP)", any(s[1] and s[2] == "EMPTY: NOT LOADED" for s in w), w)
wh = [s for s in said if 15000 <= s[0] < 15500]
check("WHERE: the whole page again", any(s[2].startswith("PROJECTS") and "ACTION" in s[2] for s in wh), wh)

said, _ = run("bye", hellos(1000, 8000) + [(8500, BYE), (9500, "btn 6"), (10500, "enc 2 5")], 12000)
check("BYE: silent", not [s for s in said if s[0] > 8500], [s for s in said if s[0] > 8500][:2])
said, _ = run("gone", hellos(1000, 8000) + [(13500, "btn 6"), (14500, "enc 2 5")], 16000)
check("no HELLO for 5 s: silent", not [s for s in said if s[0] > 12000], [s for s in said if s[0] > 12000][:2])

print("reader test:", "passed" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
