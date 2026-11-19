#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.4) TIDE and GUARD (LFO's sixth page) in host mode:
  - NOW turned to OUT: the sound ebbs from then, at 0.15 dB a second
  - TIDE OFTEN (a build with minutes as seconds, -DTD_SPEED=60): an ebb comes by itself, goes down about DEPTH's
    25 dB (70 %), stays, and comes back all the way
  - GUARD 100: a chord swelling over a held note is held down after a few seconds; GUARD 0 leaves it
  (TIDE OFF and GUARD 0, the start, leave the sound as it was: tests/sound_prints.py)
    python3 tests/tide_test.py build/host/arctic build/host/arctic_tide60"""
import array
import math
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
FAST = sys.argv[2] if len(sys.argv) > 2 else "build/host/arctic_tide60"
OUT = Path("build/tidetest")
OUT.mkdir(parents=True, exist_ok=True)
LFO = 5
K1, K2, K3, K4 = 2, 3, 4, 5
bad = 0


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  tide: {what}" + (f" ({detail})" if detail and not ok else ""))
    bad += not ok


def run(name, ev, end, b=BIN):
    (OUT / f"{name}.txt").write_text("".join(f"{t} {e}\n" for t, e in sorted(ev, key=lambda x: x[0])) + f"{end} end\n")
    out = subprocess.run([b, str(OUT / name), str(OUT / f"{name}.txt")], capture_output=True, text=True, check=True).stdout
    pr = {int(m.group(1)): (int(m.group(2)), int(m.group(3)), int(m.group(4)))
          for m in re.finditer(r"^probe (\d+) .* tide (\d+) (-?\d+) (-?\d+)", out, re.M)}
    return pr, array.array("h", (OUT / f"{name}.wav").read_bytes()[44:])


def rms(a, t0, t1):
    s = a[2 * int(44100 * t0):2 * int(44100 * t1)]
    return math.sqrt(sum(x * x for x in s) / max(1, len(s))) + 1e-9


def db(a, b):
    return 20 * math.log10(a / b)


PAGE = [(1000 + 150 * i, f"btn {LFO}") for i in range(6)]   # (LFO tapped six times: TIDE)
HOLD = [(3000, "keys 0x80")]                    # (a note held throughout)

ref = run("ref", PAGE + HOLD, 38000)[1]
pr, a = run("now", PAGE + HOLD + [(8000, f"enc {K4} 1"), (37500, "probe")], 38000)
d = db(rms(a, 36.0, 37.5), rms(ref, 36.0, 37.5))   # (against the same note left alone: about 4.4 dB)
check("NOW to OUT: ebbing, about 0.15 dB a second", pr[37500][0] == 1 and -6.5 < d < -2.5, f"{d:.1f} dB {pr[37500]}")

probes = [(t, "probe") for t in range(4000, 40000, 250)]
pr, a = run("often", PAGE + HOLD + [(2200, f"enc {K1} 3")] + probes, 40000, FAST)
sts = [pr[t][0] for t in sorted(pr)]
ts = sorted(pr)
out = []                                        # (the first ebb's time out)
for t in ts:
    if pr[t][0] == 2: out.append(t)
    elif out: break
back = [t for t in ts if out and t > out[-1] and pr[t][0] == 0][:8]
deep = min(pr[t][1] for t in pr) / 10886.0
check("OFTEN: an ebb by itself, out, and back in", bool(out) and bool(back), sts[::8])
if out and back:
    lo = rms(a, out[0] / 1000 + 0.1, out[-1] / 1000)
    hi = rms(a, back[0] / 1000 + 0.3, back[-1] / 1000)
    check("OFTEN: down about DEPTH's 25 dB, and back to the level it left", -27.5 < deep < -24.0 and
          -28 < db(lo, hi) < -21, f"{deep:.1f} {db(lo, hi):.1f}")

CHORD = [(20000, "keys 0x80"), (20010, f"keys {0x80 | 0x200 | 0x800 | 0x4000 | 0x10000}")]
g0 = run("guard0", PAGE + HOLD + CHORD, 32000)[1]
pr, g1 = run("guard100", PAGE + HOLD + CHORD + [(2400, f"enc {K3} 100"), (31000, "probe")], 32000)
d = db(rms(g1, 22.0, 25.0), rms(g0, 22.0, 25.0))
check("GUARD 100: the chord held down a few seconds on; GUARD 0 lets it be", d < -1.0 and pr[31000][2] < 0, f"{d:.1f} dB")
same = db(rms(g1, 10.0, 19.0), rms(g0, 10.0, 19.0))
check("GUARD: the held note before it, as it was", abs(same) < 0.3, f"{same:.2f} dB")
sys.exit(1 if bad else 0)
