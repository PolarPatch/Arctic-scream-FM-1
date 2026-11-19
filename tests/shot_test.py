#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.4) ONE-SHOTS in host mode: GLO held + each of the 19 keys its mutes and solos leave free plays its gesture
on the selected track (sounding, its notes in the list), and it ends by itself (no note of it left on); GLO held + a
mute key still mutes, and plays none; a gesture over another: the first one's notes let go
    python3 tests/shot_test.py build/host/arctic"""
import array
import math
import re
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/shottest")
OUT.mkdir(parents=True, exist_ok=True)
GLO = 7
FREE = [1, 3, 5, 8, 10] + list(range(13, 27))
bad = 0


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  shot: {what}" + (f" ({detail})" if detail and not ok else ""))
    bad += not ok


def run(name, ev, end):
    (OUT / f"{name}.txt").write_text("".join(f"{t} {e}\n" for t, e in sorted(ev, key=lambda x: x[0])) + f"{end} end\n")
    out = subprocess.run([BIN, str(OUT / name), str(OUT / f"{name}.txt")], capture_output=True, text=True, check=True).stdout
    pr = {}
    for ln in out.splitlines():
        m = re.match(r"probe (\d+) .* shot (-?\d+) tide [-\d ]+ lit \w+ voices (.*)$", ln)
        if m:
            vs = [tuple(int(x) for x in v.split("/")) for v in m.group(3).split()]
            pr[int(m.group(1))] = (int(m.group(2)), sum(1 for v in vs if v[3]))
    return pr, array.array("h", (OUT / f"{name}.wav").read_bytes()[44:])


def rms(a, t0, t1):
    s = a[2 * int(44100 * t0):2 * int(44100 * t1)]
    return math.sqrt(sum(x * x for x in s) / max(1, len(s)))


def tap(k, t):
    return [(t, f"down {GLO}"), (t + 200, f"keys {1 << k}"), (t + 300, "keys 0"), (t + 400, f"up {GLO}")]


quiet = []
for g, k in enumerate(FREE):
    pr, a = run(f"g{g}", tap(k, 3000) + [(3700, "probe"), (16000, "probe")], 16100)
    ok = pr[3700][0] == g and pr[16000][0] == -1 and pr[16000][1] == 0 and rms(a, 3.2, 11.0) > 30
    if not ok:
        quiet.append((g, pr, round(rms(a, 3.2, 11.0))))
check("each of the 19: plays on the track, sounding, and ends by itself", not quiet, quiet[:3])

pr, a = run("mute", tap(0, 3000) + [(3700, "probe")], 3800)
check("GLO + key 1 still mutes T1, no gesture", pr[3700][0] == -1)

pr, a = run("over", tap(13, 3000) + tap(26, 3600) + [(4200, "probe"), (16000, "probe")], 16100)
check("a gesture over another: the new one plays, all let go in the end", pr[4200][0] == 18 and pr[16000] == (-1, 0), pr)
sys.exit(1 if bad else 0)
