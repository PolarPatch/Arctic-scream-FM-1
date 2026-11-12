#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 1.5.3's drone machine in host mode (tests/host.c, "T probe": tests/probe.c's jkprobe line):
  - JIEHKKI (LFO x 3): START with nothing on plays its own notes, a track each (T1 at once, the others coming in), rising out of
    silence and heard from its first seconds; LENGTH 5 MIN
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


def run(name, ev, end, env=None, plain=True):     # (plain: Arctic Scream's VA tracks; else the factory's, ICE WRAITH)
    s = OUT / f"{name}.txt"
    s.write_text("".join(f"{t} {l}\n" for t, l in sorted(ev, key=lambda e: e[0])) + f"{end} end\n")
    e = dict(os.environ, GRIMM_PLAIN="1") if plain else dict(os.environ)
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
out, a, jk, notes = run("piece", ev + P(6900, 9000, 30000, 200000, 320000, 330500, 334000), 336000)
check("LENGTH 5 MIN, START: T1 at once, the others to come", [n.split(":")[0] for n in notes[9000]] == ["1"],
      notes[9000])
check(".. heard from its first seconds", rms(a, 11, 13) > rms(a, 60, 70) * 0.25, (rms(a, 11, 13), rms(a, 60, 70)))
check("LENGTH 5 MIN, START: its own notes, one a track", jk[30000]["run"] == 1 and jk[30000]["own"] == 1 and
      sorted(n.split(":")[0] for n in notes[30000]) == ["1", "2", "3", "4"], (jk[30000], notes[30000]))
check(".. rising out of silence", rms(a, 7, 8) < rms(a, 60, 70) * 0.5, (rms(a, 7, 8), rms(a, 60, 70)))
check(".. heard in its middle", rms(a, 150, 160) > 300, rms(a, 150, 160))
check(".. dying out over its last minutes", rms(a, 290, 295) < rms(a, 200, 210) * 0.5, (rms(a, 290, 295), rms(a, 200, 210)))
check(".. silent at its end (5 min)", rms(a, 309, 328) < 3, rms(a, 309, 328))
check(".. its notes let go, START OFF, the screen dark", not notes[320000] and jk[320000]["go"] == 0 and
      jk[320000]["ended"] == 1 and jk[320000]["scr"] == 1, (notes[320000], jk[320000]))
check(".. a key wakes it (the first touch only wakes, as ever), the next one heard", jk[330500]["ended"] == 0 and
      jk[330500]["scr"] == 0 and rms(a, 332, 334) > 50, (jk[330500], rms(a, 332, 334)))

# turned OFF early: its notes let go, the gain back
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (6800, f"enc {K2} 60"), (7000, f"enc {K4} 1"),
      (130000, f"enc {K4} -1")]
out, a, jk, notes = run("off", ev + P(129000, 132000, 150000), 151000)
check("DRIFT 100 %: the filters moved", jk[129000]["cut"] > 0, jk[129000])
check("turned OFF: its notes let go, the output's gain back", notes[129000] and not notes[132000] and
      jk[132000]["og"] == 32767 and jk[132000]["run"] == 0, (notes[129000], notes[132000], jk[132000]))
check(".. the filters back as set", jk[150000]["cut"] < jk[129000]["cut"] / 10 + 30, (jk[129000]["cut"], jk[150000]["cut"]))

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

# RUOHCAT's page (LFO x 4) with a sample: a 1.5 s tone of 440 Hz into slot 1 (as tools/fm1_sample_upload.py sends it),
# SAMPLE SLOT 1, SHARE 100 %, AMOUNT 100 %: the sample heard (440 Hz the loudest), nothing else of the ice
sys.path.insert(0, "tools")
import fm1_sample_upload as up                  # noqa: E402
wav = OUT / "tone440.wav"
w = wave.open(str(wav), "w")
w.setparams((1, 2, 22050, 0, "NONE", "not compressed"))
w.writeframes(struct.pack("<%dh" % 33075, *(int(12000 * math.sin(2 * math.pi * 440 * i / 22050)) for i in range(33075))))
w.close()
hdr, data, _, _ = up.build(wav, "TONE440", 69)
hx = lambda b: " ".join(f"{x:02X}" for x in b)
ev, t = [(4000, "sysex " + hx(up.HDR + [up.CMD_BEGIN, 0]))], 4000   # (after the boot screen)
for off in range(0, len(data), 256):
    o, t = up.DATA_OFF + off, t + 5
    ev.append((t, "sysex " + hx(up.HDR + [up.CMD_WRITE, 0, o & 0x7F, (o >> 7) & 0x7F, (o >> 14) & 0x7F] +
                               up.pack7(data[off:off + 256]))))
ev.append((t + 50, "sysex " + hx(up.HDR + [up.CMD_END, 0] + up.pack7(hdr))))
ev += [(6200, f"btn {LFO}"), (6300, f"btn {LFO}"), (6400, f"btn {LFO}"), (6500, f"btn {LFO}"), (6700, f"enc {K2} 1"),
       (6800, f"enc {K3} 50"), (6900, f"enc {K1} 100")]
out, a, jk, notes = run("ruo_smp", ev + P(7000), 27000)
spec = [0.0] * 3
x = a[2 * 8 * 44100:2 * 27 * 44100:2]
for k, f in enumerate((440, 300, 3000)):        # (the tone; beside it; a crack's band)
    c = sum(v * math.cos(2 * math.pi * f * i / 44100) for i, v in enumerate(x))
    q = sum(v * math.sin(2 * math.pi * f * i / 44100) for i, v in enumerate(x))
    spec[k] = math.hypot(c, q) / len(x)
check("RUOHCAT page: SLOT 1, SHARE 100 %: the sample heard", rms(a, 8, 27) > 100 and spec[0] > 8 * spec[1] and
      spec[0] > 8 * spec[2], (rms(a, 8, 27), spec))
check("RUOHCAT page: never clipping", peak(a) < 32000, peak(a))

# (1.5.3) PRESETS on a latched drone: ICE WRAITH (Holvut) on C3, then POLAR NIGHT (VA): the two cross over (the old one
# fading as the new one's ATTACK rises); then POLAR STILL (Holvut again): its knobs glide. Never a hole, never a jump
for what, step, most in (("another source: the notes cross over", 1, 6), ("the same source: the knobs glide", -2, 8)):   # (Holvut
    # itself: its loop settles anew as BODY arrives, ~0.5 s: before 1.5.3 a hole to -16 dB and 9.5 dB steps)
    out, a, jk, notes = run(f"xf{step}", [(6200, "keys 128"), (6400, "keys 0"), (16000, f"enc 6 {step}")], 20000, plain=False)
    seq = [rms(a, 15.5 + i * 0.1, 15.6 + i * 0.1) for i in range(25)]   # (the 2 s it crosses over in; after it,
    jump = max(abs(20 * math.log10((seq[i + 1] + 1) / (seq[i] + 1))) for i in range(24))   # POLAR NIGHT's own swell)
    check(f"PRESETS on a drone, {what}: no hole (under -6 dB of before), no step over {most} dB in 0.1 s",
          min(seq[5:]) > 0.5 * min(seq[:5]) and jump < most, ([round(x) for x in seq], round(jump, 1)))

# (2.0) DICE: SEQ held + G3 (HUSH) .. G4 (MACHINE): a new part on T1 in that mood; EDIT held + OCT-: undone. ACCIDENT:
# EDIT held + G4: T1's settings moved (its knobs gliding there); undone the same way
SEQB, OCTD = 11, 0
def probes(out):
    pr = {}
    for m in re.finditer(r"^probe (\d+) .*? steps (\d+) .*? ph (\S+) .*? keep (\S+)", out, re.M):
        pr[int(m.group(1))] = (int(m.group(2)), m.group(3).split(",")[0], m.group(4).split(",")[0])
    return pr
ev = [(6200, "probe"), (6400, f"down {SEQB}"), (6600, f"keys {1 << 14}"), (6700, "keys 0"), (6900, f"up {SEQB}"), (7000, "probe"),
      (7200, f"down {SEQB}"), (7400, f"keys {1 << 26}"), (7500, "keys 0"), (7700, f"up {SEQB}"), (7800, "probe"),
      (8000, f"down {EDIT}"), (8200, f"btn {OCTD}"), (8400, f"up {EDIT}"), (8600, "probe"),
      (9000, f"down {EDIT}"), (9200, f"keys {1 << 26}"), (9300, "keys 0"), (9500, f"up {EDIT}"), (11500, "probe"),
      (12000, f"down {EDIT}"), (12200, f"btn {OCTD}"), (12400, f"up {EDIT}"), (12600, "probe")]
out, a, jk, notes = run("dice", ev, 13000)
pr = probes(out)
check("DICE: SEQ + G3, HUSH: one or two quiet notes; SEQ + G4, MACHINE: a dense part", pr[6200][0] == 0 and
      1 <= pr[7000][0] <= 2 and pr[7800][0] >= 6, {k: v[0] for k, v in pr.items()})
check("DICE: EDIT + OCT- undoes the last one (HUSH's part back)", pr[8600][1] == pr[7000][1], (pr[7000][1], pr[8600][1]))
check("ACCIDENT: EDIT + G4 moves T1's settings; EDIT + OCT- puts them back", pr[11500][2] != pr[8600][2] and
      pr[12600][2] == pr[8600][2], (pr[8600][2], pr[11500][2], pr[12600][2]))

if fails:
    sys.exit(1)
print("drone: JIEHKKI, RUOHCAT (its samples too), TUNING, Biegga, GUOVSSAHAS, PRESETS' crossing over, DICE, ACCIDENT ok")
