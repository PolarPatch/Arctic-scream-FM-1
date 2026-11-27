#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.8) DRIFT (HOME's fourth page, MIXER 3: drift.c) in host mode:
  - its page is HOME's fourth; AMOUNT 0 (the start): every track's gain full, no walk (the sound as before:
    tests/sound_prints.py)
  - AMOUNT 100, PACE 100, four tracks sounding: the levels move, slowly (never a step), one track always near the top
    in time, T1 never far off; DISTANCE: a track far off sends more into SPACE, its filter darker
  - WANDER: the tracks walk round the stereo field; WANDER 0: they stay
  - one track sounding alone: it stays full
  - AMOUNT back to 0: everything glides back to as set
  - saved with the project (version 13): at power-on as set
    python3 tests/drift_test.py [build/host/arctic]"""
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/drifttest")
OUT.mkdir(parents=True, exist_ok=True)
HOME = 8
K1, K2, K3, K4 = 2, 3, 4, 5
bad = 0


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  drift: {what}" + (f" ({detail})" if detail != "" and not ok else ""))
    bad += not ok


def run(name, ev, end, env=None):
    import os
    (OUT / f"{name}.txt").write_text("".join(f"{t} {e}\n" for t, e in sorted(ev, key=lambda x: x[0])) + f"{end} end\n")
    out = subprocess.run([BIN, str(OUT / name), str(OUT / f"{name}.txt")], capture_output=True, text=True, check=True,
                         env=dict(os.environ, **(env or {}))).stdout
    pr = {}
    for m in re.finditer(r"^probe (\d+) .* drift (\d+) (-?\d+) ([-\d,]+) ([-\d,]+) (-?\d+),(-?\d+)", out, re.M):
        pr[int(m.group(1))] = dict(page=int(m.group(2)), amt=int(m.group(3)), g=[int(x) for x in m.group(4).split(",")],
                                   pan=[int(x) for x in m.group(5).split(",")], rev=int(m.group(6)), cut=int(m.group(7)))
    return pr


P = lambda *ts: [(t, "probe") for t in ts]
PAGE = [(1000 + 200 * i, f"btn {HOME}") for i in range(3)]   # (from HOME, tapped three times: DRIFT)
NOTES = [(1200, "midi 0x90 36 100"), (1300, "midi 0x91 48 100"), (1400, "midi 0x92 55 100"), (1500, "midi 0x93 64 100")]
FULL = [(2500, f"enc {K1} 100"), (2600, f"enc {K2} 100"), (2700, f"enc {K3} 100"), (2800, f"enc {K4} 100")]

pr = run("off", PAGE + NOTES + P(*range(3000, 30001, 1000)), 30500)
check("its page is HOME's fourth (DRIFT)", pr[3000]["page"] == 37, pr[3000]["page"])
check("AMOUNT 0: every gain full, no walk, no distance",
      all(p["g"] == [32767] * 4 and p["pan"] == [0] * 4 and p["rev"] == 0 and p["cut"] == 0 for p in pr.values()),
      next(iter(pr.values())))

T = list(range(3000, 180001, 250))
pr = run("full", PAGE + NOTES + FULL + P(*T), 180500)
g = [pr[t]["g"] for t in T]
lows = [min(x[k] for x in g[40:]) for k in range(4)]
check("AMOUNT 100: the levels move (some track down past -6 dB)", min(lows[1:]) < 16000, lows)
check("T1, the ground: never under ~ -4 dB", lows[0] >= 19500, lows[0])
step = max(abs(g[i + 1][k] - g[i][k]) for i in range(len(g) - 1) for k in range(4))
check("never a step: under ~ -1.5 dB a quarter second", step < 6000, step)
top = sum(max(x) > 28000 for x in g[120:]) / len(g[120:])
check("one track near the top most of the time (between the glides)", top > 0.6, round(top, 2))
pans = [pr[t]["pan"] for t in T]
check("WANDER 100: the tracks walk the stereo field (and T1 half as far)",
      max(abs(p[2]) for p in pans) > 25 and max(abs(p[0]) for p in pans) <= 32, (max(abs(p[2]) for p in pans), max(abs(p[0]) for p in pans)))
far = [(pr[t]["g"][1], pr[t]["rev"], pr[t]["cut"]) for t in T]
f0 = [x for x in far if x[0] < 12000]
check("DISTANCE 100: T2 far off, more into SPACE and darker", f0 and all(r > 30 and c > 15 * 256 for _, r, c in f0),
      f0[:3] if f0 else "T2 never far")

pr = run("still", PAGE + NOTES + FULL + [(2900, f"enc {K3} -100")] + P(*T[::4]), 180500)
check("WANDER 0: no walk", all(p["pan"] == [0] * 4 for p in pr.values()), [p["pan"] for p in list(pr.values())[:3]])

pr = run("alone", PAGE + NOTES[1:2] + FULL + P(*T[::4]), 180500)
check("one track sounding alone: everything stays full", all(p["g"] == [32767] * 4 for p in pr.values()),
      [p["g"] for p in pr.values() if p["g"] != [32767] * 4][:2])

T2 = list(range(3000, 150001, 1000))
pr = run("back", PAGE + NOTES + FULL + [(90000, f"enc {K1} -100")] + P(*T2), 150500)
check("AMOUNT back to 0: the mix glides back to as set (within 40 s)",
      pr[90000]["amt"] == 100 and pr[90000]["g"] != [32767] * 4 and pr[130000]["g"] == [32767] * 4 and
      all(abs(x) <= 1 for x in pr[130000]["pan"]) and pr[132000]["amt"] == 0, (pr[90000]["g"], pr[130000]["g"], pr[130000]["pan"]))

sv = OUT / "saves_drift.bin"
ev = PAGE + [(2500, f"enc {K1} 37"), (2600, f"enc {K2} -20"), (2700, f"enc {K3} 12"), (2800, f"enc {K4} -50")]
run("save", ev, 40000, {"ARCTIC_SAVE_OUT": str(sv)})
pr = run("load", PAGE + P(3000), 3500, {"ARCTIC_SAVE_IN": str(sv)})
check("saved with the project (version 13): at power-on AMOUNT 37", pr[3000]["amt"] == 37, pr[3000]["amt"])
pr = run("old", P(3000), 3500, {"ARCTIC_SAVE_IN": "tests/data/a1.2-saves.bin"})
check("a project from 1.2: DRIFT off", pr[3000]["amt"] == 0, pr[3000]["amt"])
sys.exit(1 if bad else 0)
