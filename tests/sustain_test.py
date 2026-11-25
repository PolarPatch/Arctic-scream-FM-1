#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.7) SUSTAIN in host mode (tests/run_host.sh): CC 64 in (and on the SMK-37 its pedal, tests/smk_test.py)
  down: a key or a MIDI note let go sounds on; up: they go; a note played again meanwhile: one voice, not two;
  ALL NOTES OFF: what it held goes too
  python3 tests/sustain_test.py [build/host/arctic]"""
import os
import subprocess
import sys
from pathlib import Path

EXE = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/host/sustain")
OUT.mkdir(parents=True, exist_ok=True)
C4, E4 = 0x80, 0x800
bad = 0

b = bytearray(171)
b[167], b[169], b[170] = 4, 1, 2                # (SYSTEM 3's LATCH: ALWAYS OFF)
(OUT / "set.bin").write_bytes(bytes(b))
ENV = dict(os.environ, ARCTIC_SETTINGS_IN=str(OUT / "set.bin"), ARCTIC_PLAIN="1")


def run(name, ev, end):
    p = OUT / f"{name}.txt"
    p.write_text("".join(f"{t} {s}\n" for t, s in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    out = subprocess.run([EXE, str(OUT / name), str(p)], capture_output=True, text=True, check=True, env=ENV).stdout
    return {int(ln.split()[1]): [x for x in ln.split(" notes", 1)[1].split(" fx ")[0].split()]
            for ln in out.splitlines() if ln.startswith("probe ")}


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  sustain: {what}" + (f" ({detail})" if not ok else ""))
    bad += not ok


P = lambda *ts: [(t, "probe") for t in ts]
n = run("keys", [(2000, f"keys {C4:#x}"), (2100, "keys 0")] + P(2300) +
        [(3000, "midi 0xB0 64 127"), (3100, f"keys {C4:#x}"), (3200, "keys 0"), (3300, f"keys {E4:#x}"), (3400, "keys 0")] +
        P(3600) + [(3700, "midi 0xB0 64 0")] + P(3900), 4000)
check("no pedal: a key let go goes", n[2300] == [], n[2300])
check("CC 64 down: two keys let go sound on", len(n[3600]) == 2, n[3600])
check("CC 64 up: they go", n[3900] == [], n[3900])
n = run("midi", [(2000, "midi 0xB0 64 100"), (2100, "midi 0x90 60 100"), (2200, "midi 0x80 60 0"), (2300, "midi 0x90 60 100"),
                 (2400, "midi 0x80 60 0")] + P(2600) + [(2700, "midi 0xB0 123 0")] + P(2900) +
        [(3000, "midi 0xB0 64 100"), (3100, "midi 0x90 62 90"), (3200, "midi 0x80 62 0"), (3300, "midi 0xB0 64 10")] + P(3500), 3600)
check("a MIDI note let go and played again under it: one voice, not two", len(n[2600]) == 1, n[2600])
check("ALL NOTES OFF: what it held goes", n[2900] == [], n[2900])
check("CC 64 under 64: up", n[3500] == [], n[3500])
sys.exit(1 if bad else 0)
