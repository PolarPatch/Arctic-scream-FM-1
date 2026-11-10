#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 1.5.3's drone machine in host mode (tests/host.c, "T probe": tests/probe.c's jkprobe line):
  - JIEHKKI (LFO x 3): START with nothing on plays its own notes, a track each, rising out of silence; LENGTH 5 MIN
    dies out on time: silent, its notes let go, the screen dark; a key wakes it, and it sounds again; turned OFF early:
    its notes let go, the sound as set
  - RUOHCAT: the ice heard on its own, never clipping
  - TUNING (SEL, KNOB 3): JUST's minor third (6/5) 15.6 cents over EQUAL's, the root as it was
  - Biegga (the ninth source): heard, not clipping; its knobs, JIEHKKI's and TUNING saved with the project (version 9)
    and loaded at power-on; a slot keeps its Biegga; a project from 1.2 loads with them as at power-on
  - GUOVSSAHAS (SCREEN OFF's last): after 2 min untouched the aurora (the screen lit), a touch back
    python3 tests/drone_test.py build/host/grimm"""
import math
import os
import re
import struct
import subprocess
import sys
import wave
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/grimm"
OUT = Path("build/dronetest")
OUT.mkdir(parents=True, exist_ok=True)
fails = 0
LFO, EDIT, SEL, HOME, SAVE = 5, 6, 3, 8, 9      # buttons (platform.c)
K1, K2, K3, K4 = 2, 3, 4, 5                     # encoders


def check(name, ok, info=""):
    global fails
    print(f"  {'ok  ' if ok else 'FAIL'}  drone: {name}" + ("" if ok else f"  ({info})"))
    fails += not ok


def run(name, ev, end, env=None):
    s = OUT / f"{name}.txt"
    s.write_text("".join(f"{t} {l}\n" for t, l in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    e = dict(os.environ, GRIMM_PLAIN="1")
    e.update(env or {})
    out = subprocess.run([BIN, str(OUT / name), str(s)], capture_output=True, text=True, check=True, env=e).stdout
    w = wave.open(str(OUT / f"{name}.wav"))
    n = w.getnframes()
    a = struct.unpack("<%dh" % (2 * n), w.readframes(n))
    w.close()
    jk = {}
    for m in re.finditer(r"^jkprobe (\d+) (.*)$", out, re.M):
        f = m.group(2).split(" bg ")
        d = dict(zip(f[0].split()[::2], (int(x) for x in f[0].split()[1::2])))
        d["bg"] = [[int(x) for x in q.split(",")] for q in f[1].split()]
        jk[int(m.group(1))] = d
    notes = {int(m.group(1)): m.group(2).split() for m in re.finditer(r"^probe (\d+) .*? notes(.*?) fx ", out, re.M)}
    return out, a, jk, notes


def rms(a, t0, t1):                             # the output's RMS from t0 to t1 s
    x = a[2 * int(t0 * 44100):2 * int(t1 * 44100)]
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


def peak(a):
    return max(abs(v) for v in a) if a else 0


P = lambda *ts: [(t, "probe") for t in ts]

# JIEHKKI: LFO x 3, LENGTH 5 MIN (KNOB 1 from 15 MIN, 2 down), START (KNOB 4); a key after its end
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (6800, f"enc {K1} -1"), (6900, f"enc {K1} -1"),
      (7000, f"enc {K4} 1"), (330000, "keys 1"), (330300, "keys 0"), (331000, "keys 1"), (334000, "keys 0")]
out, a, jk, notes = run("piece", ev + P(6900, 30000, 200000, 320000, 330500, 334000), 336000)
check("LENGTH 5 MIN, START: its own notes, one a track", jk[30000]["run"] == 1 and jk[30000]["own"] == 1 and
      sorted(n.split(":")[0] for n in notes[30000]) == ["1", "2", "3", "4"], (jk[30000], notes[30000]))
check(".. rising out of silence", rms(a, 8, 12) < rms(a, 60, 70) * 0.7, (rms(a, 8, 12), rms(a, 60, 70)))
check(".. heard in its middle", rms(a, 150, 160) > 300, rms(a, 150, 160))
check(".. dying out over its last minutes", rms(a, 290, 295) < rms(a, 200, 210) * 0.5, (rms(a, 290, 295), rms(a, 200, 210)))
check(".. silent at its end (5 min)", rms(a, 309, 328) < 3, rms(a, 309, 328))
check(".. its notes let go, START OFF, the screen dark", not notes[320000] and jk[320000]["go"] == 0 and
      jk[320000]["ended"] == 1 and jk[320000]["scr"] == 1, (notes[320000], jk[320000]))
check(".. a key wakes it (the first touch only wakes, as ever), the next one heard", jk[330500]["ended"] == 0 and
      jk[330500]["scr"] == 0 and rms(a, 332, 334) > 50, (jk[330500], rms(a, 332, 334)))

# turned OFF early: its notes let go, the gain back
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (7000, f"enc {K4} 1"), (20000, f"enc {K4} -1")]
out, a, jk, notes = run("off", ev + P(19000, 22000), 23000)
check("turned OFF: its notes let go, the output's gain back", notes[19000] and not notes[22000] and
      jk[22000]["og"] == 32767 and jk[22000]["run"] == 0, (notes[19000], notes[22000], jk[22000]))

# with a note held (LATCH OFF, plain VA): it takes it, no notes of its own, no rise
ev = [(6200, "keys 1"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (6800, f"btn {LFO}"), (7000, f"enc {K4} 1")]
out, a, jk, notes = run("held", ev + P(15000), 16000)
check("START with a note on: its own none, the note as it is", jk[15000]["own"] == 0 and len(notes[15000]) == 1 and
      rms(a, 8, 9) > 100, (jk[15000], notes[15000], rms(a, 8, 9)))

# RUOHCAT 100 %, nothing else
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}")] + [(6800 + 20 * i, f"enc {K3} 5") for i in range(20)]
out, a, jk, notes = run("ruo", ev, 30000)
check("RUOHCAT 100 %: the ice heard, nothing played", rms(a, 8, 30) > 100 and peak(a) < 32000, (rms(a, 8, 30), peak(a)))


def freq(a, t0, t1, lo, hi):                    # the strongest frequency between lo and hi Hz (interpolated)
    import numpy as np
    x = np.array(a[2 * int(t0 * 44100):2 * int(t1 * 44100):2], dtype=float)
    x *= np.hanning(len(x))
    s = np.abs(np.fft.rfft(x, 8 * len(x)))
    f = np.fft.rfftfreq(8 * len(x), 1 / 44100)
    m = (f > lo) & (f < hi)
    i = np.argmax(s * m)
    return f[i]


# TUNING: D#3 (C minor's third) under EQUAL and JUST; C3 alike
try:
    import numpy  # noqa: F401
    have_np = True
except ImportError:
    have_np = False
if have_np:
    for nm, key in (("ds", 1 << 10), ("c", 1 << 7)):
        fr = []
        for tun in (0, 1):
            ev = [(6000, f"btn {EDIT}"), (6100, f"enc {K2} -10"), (6200, f"btn {SEL}")] + \
                 ([(6400, f"enc {K3} 1")] if tun else []) + [(7000, f"keys {key}"), (11000, "keys 0")]   # (DETUNE 0: one saw)
            out, a, jk, notes = run(f"tun_{nm}{tun}", ev, 11500)
            fr.append(freq(a, 8, 11, 100, 200))
        c = 1200 * math.log2(fr[1] / fr[0])
        if nm == "ds":
            check("TUNING JUST: the minor third 15.6 cents over EQUAL", abs(c - 15.64) < 1.5, (fr, c))
        else:
            check("TUNING JUST: ROOT as in EQUAL", abs(c) < 1.0, (fr, c))
else:
    print("  skip  drone: TUNING (no numpy)")

# Biegga: T1 to it (SOURCE: Holvut's next), a chord
BG = [(6200, f"btn {EDIT}"), (6400, f"btn {EDIT}")] + [(6600, f"enc {K1} 8")]   # (plain: VA, eight down to Biegga)
ev = BG + [(7000, "keys 133"), (12000, "keys 0")]
out, a, jk, notes = run("biegga", ev + P(9000), 13000)
check("Biegga: heard, not clipping", rms(a, 8, 12) > 300 and peak(a) < 32000, (rms(a, 8, 12), peak(a)))
check("Biegga: its knobs as at power-on (GUST, HOWL, TONE 64, SNOW 16)", jk[9000]["bg"][0] == [64, 64, 64, 16], jk[9000])

# saved: Biegga's GUST + 20, LENGTH 30 MIN, DRIFT 70, RUOHCAT 20, TUNING JUST; out; in again at power-on
sv = OUT / "saves.bin"
ev = BG + [(6800, f"btn {EDIT}"), (7000, f"enc {K1} 20"), (7400, f"btn {LFO}"), (7600, f"btn {LFO}"), (7800, f"btn {LFO}"),
           (8000, f"enc {K1} 2"), (8200, f"enc {K2} 30"), (8400, f"enc {K3} 20"), (8600, f"btn {SEL}"), (8800, f"enc {K3} 1")]
out, a, jk, notes = run("save", ev + P(9500), 40000, {"GRIMM_SAVE_OUT": str(sv)})
check("set: Biegga GUST 84, LENGTH 30 MIN, DRIFT 70, RUOHCAT 20, TUNING JUST", jk[9500]["bg"][0][0] == 84 and
      jk[9500]["len"] == 4 and jk[9500]["drift"] == 70 and jk[9500]["ruo"] == 20 and jk[9500]["tun"] == 1, jk[9500])
out, a, jk, notes = run("load", P(3000), 3500, {"GRIMM_SAVE_IN": str(sv)})
check("power-on: all of it as saved (version 9)", jk[3000]["bg"][0][0] == 84 and jk[3000]["len"] == 4 and
      jk[3000]["drift"] == 70 and jk[3000]["ruo"] == 20 and jk[3000]["tun"] == 1 and jk[3000]["go"] == 0, jk[3000])

# a slot keeps its Biegga: GUST 84 saved into slot A (SAVE held + white key 5), GUST back down, A recalled (SAVE held +
# white key 1)
ev = BG + [(6800, f"btn {EDIT}"), (7000, f"enc {K1} 20"), (7400, f"down {SAVE}"), (8000, "keys 128"), (8200, "keys 0"),
           (8400, f"up {SAVE}"), (9000, f"enc {K1} -40"), (9400, f"down {SAVE}"), (10000, "keys 1"), (10200, "keys 0"),
           (10400, f"up {SAVE}")]
out, a, jk, notes = run("slot", ev + P(9300, 11000), 11500)
check("slot A: its Biegga recalled (GUST 84, after 44)", jk[9300]["bg"][0][0] == 44 and jk[11000]["bg"][0][0] == 84,
      (jk[9300]["bg"][0], jk[11000]["bg"][0]))

# 1.2's project: as at power-on
out, a, jk, notes = run("old", P(3000), 3500, {"GRIMM_SAVE_IN": "tests/data/a1.2-saves.bin"})
check("Arctic Scream 1.2's project: Biegga, JIEHKKI, TUNING as at power-on", jk[3000]["bg"][0] == [64, 64, 64, 16] and
      jk[3000]["len"] == 2 and jk[3000]["drift"] == 40 and jk[3000]["ruo"] == 0 and jk[3000]["tun"] == 0, jk[3000])

# GUOVSSAHAS: SCREEN OFF (SYSTEM, KNOB 4) to its last; 2 min untouched; a button back
ev = [(6200, f"down {HOME}"), (6900, f"up {HOME}"), (7200, f"enc {K4} 5"), (130000, f"btn {EDIT}")]
out, a, jk, notes = run("aurora", ev + P(100000, 129000, 131000), 132000)
check("GUOVSSAHAS: not before 2 min", jk[100000]["aur"] == 0, jk[100000])
check("GUOVSSAHAS: after 2 min untouched, the aurora (lit)", jk[129000]["aur"] == 1 and jk[129000]["scr"] == 1, jk[129000])
check("GUOVSSAHAS: a touch, back", jk[131000]["aur"] == 0 and jk[131000]["scr"] == 0, jk[131000])

if fails:
    sys.exit(1)
print("drone: JIEHKKI, RUOHCAT, TUNING, Biegga, GUOVSSAHAS ok")
