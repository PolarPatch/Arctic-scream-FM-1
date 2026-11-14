#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream 2.0's drone machine in host mode (tests/host.c, "T probe": tests/probe.c's jkprobe line):
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
out, a, jk, notes = run("piece", ev + P(6900, 9000, 30000, 200000, 240000, 270000, 285000, 300000, 320000, 330500,
                                         334000), 336000)
check("LENGTH 5 MIN, START: T1 at once, the others to come", [n.split(":")[0] for n in notes[9000]] == ["1"],
      notes[9000])
check(".. heard from its first seconds", rms(a, 11, 13) > rms(a, 60, 70) * 0.25, (rms(a, 11, 13), rms(a, 60, 70)))
check("LENGTH 5 MIN, START: its own notes, one a track", jk[30000]["run"] == 1 and jk[30000]["own"] == 1 and
      sorted(n.split(":")[0] for n in notes[30000]) == ["1", "2", "3", "4"], (jk[30000], notes[30000]))
check(".. rising out of silence", rms(a, 7, 8) < rms(a, 60, 70) * 0.5, (rms(a, 7, 8), rms(a, 60, 70)))
check(".. heard in its middle", rms(a, 150, 160) > 300, rms(a, 150, 160))
check(".. dying out over its last minutes", rms(a, 290, 295) < rms(a, 200, 210) * 0.5, (rms(a, 290, 295), rms(a, 200, 210)))
lit = {int(m.group(1)): int(m.group(2), 16) for m in re.finditer(r"^probe (\d+) .* lit ([0-9a-f]+)", out, re.M)}
want = sum(1 << (int(n.split(":")[1]) - 41) for n in notes[30000] if 41 <= int(n.split(":")[1]) < 68)
check("(2.2) the keys light the piece's own notes (every track's on the keys)", lit[30000] == want and want,
      (hex(lit[30000]), notes[30000]))
trk = lambda ms: sorted(n.split(":")[0] for n in notes[ms])
check("(2.2) its dusk: the tracks go one by one, the highest first, T1 alone at the last",
      trk(240000) == ["1", "2", "3", "4"] and trk(270000) == ["1", "2", "3"] and trk(285000) == ["1", "2"] and
      trk(300000) == ["1"], [trk(m) for m in (240000, 270000, 285000, 300000)])
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
# (2.2) the ice holds its breath: RUOHCAT 100 %, a held chord into SPACE; SPACE held a while now and then, let go again
ev = [(6000, "keys 145"), (6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}")] + \
     [(6800 + 20 * i, f"enc {K3} 5") for i in range(20)]
out, a, jk, notes = run("hold", ev + P(*range(8000, 120000, 500)), 120000)
held = [t for t in sorted(jk) if jk[t]["hold"]]
spans = [(t, u) for t, u in zip([held[0]] + [h for h, g in zip(held[1:], held) if h - g > 500], [g for h, g in zip(held[1:], held) if h - g > 500] + [held[-1]])] if held else []
check("(2.2) RUOHCAT 100 %: SPACE held now and then (3 .. 10 s each), let go between",
      2 <= len(spans) and all(2500 <= u - t <= 10500 for t, u in spans if u < 119000) and len(held) < len(jk) / 2, spans)


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

# (2.2) JAHKI (LFO x 5): YEAR 8 MIN (from 1 HOUR, 2 down), DEPTH 100 %: the seasons turn (a minute each), leaning
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (6800, f"btn {LFO}"), (7000, f"btn {LFO}"),
      (7200, f"enc {K1} -2"), (7400, f"enc {K3} 100"), (136000, f"enc {K2} 3")]
out, a, jk, notes = run("year", ev + P(6000, 9000, 40000, 70000, 100000, 135000, 137000), 145000, {"GRIMM_SAVE_OUT": str(OUT / "saves_year.bin")})
check("(2.2) JAHKI: at power-on as never set: 1 HOUR, DALVI, DEPTH 0, nothing leaning",
      jk[6000]["yr"] == 3 and jk[6000]["ys"] == 0 and jk[6000]["lean"] == 0 and jk[6000]["shq"] == 16 and jk[6000]["ycut"] == 0, jk[6000])
check(".. YEAR 8 MIN, DEPTH 100 %: DALVI leans dark and long (SPACE + 14, the filters down)",
      jk[9000]["yr"] == 1 and jk[9000]["ys"] == 0 and jk[9000]["lean"] >= 13 and jk[9000]["ycut"] < -2000, jk[9000])
check(".. a minute a season: GIDDADALVI, then GIDDA (the leans between)", jk[70000]["ys"] == 1 and jk[135000]["ys"] == 2 and
      jk[40000]["lean"] < jk[9000]["lean"], [(jk[t]["ys"], jk[t]["lean"], jk[t]["ycut"]) for t in (9000, 40000, 70000, 135000)])
check(".. SEASON turned by hand: the year there (GIDDA, 3 on: CAKCAGEASSI, late summer: SHIMMER up)", jk[137000]["ys"] == 5 and jk[137000]["shq"] > 16,
      jk[137000])
out, a, jk, notes = run("year_on", P(3000), 3500, {"GRIMM_SAVE_IN": str(OUT / "saves_year.bin")})
check(".. power-on: YEAR, DEPTH and the season turned to, as saved (version 10; the year moving on is no save)",
      jk[3000]["yr"] == 1 and jk[3000]["yd"] == 100 and jk[3000]["ys"] == 5, jk[3000])

# (2.2) START ALWAYS: saved with the project; at power-on the piece begins on its own (its own notes, rising)
sv2 = OUT / "saves_always.bin"
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (7000, f"enc {K4} 2")]
out, a, jk, notes = run("always", ev + P(9000), 50000, {"GRIMM_SAVE_OUT": str(sv2)})
check("(2.2) START ALWAYS: it plays now", jk[9000]["go"] == 2 and jk[9000]["run"] == 1, jk[9000])
out, a, jk, notes = run("always_on", P(2000, 40000), 41000, {"GRIMM_SAVE_IN": str(sv2)})
check(".. power-on: START ALWAYS again, the piece begins on its own", jk[2000]["go"] == 2 and jk[40000]["run"] == 1 and
      jk[40000]["own"] == 1 and len(notes[40000]) == 4 and rms(a, 30, 40) > 100, (jk[2000], jk[40000], notes[40000]))

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

# (2.2) GOVVA: SCREEN OFF's very last; 1 min untouched: the moving picture (the screen lit); a knob back
ev = [(6200, f"down {HOME}"), (6900, f"up {HOME}"), (7200, f"enc {K4} 6"), (70000, f"enc {K1} 1")]
out, a, jk, notes = run("govva", ev + P(60000, 69000, 71000), 72000)
check("(2.2) GOVVA: not before 1 min", jk[60000]["aur"] == 0, jk[60000])
check(".. after 1 min untouched, the picture (lit)", jk[69000]["aur"] == 2 and jk[69000]["scr"] == 1, jk[69000])
check(".. a knob turned, back", jk[71000]["aur"] == 0 and jk[71000]["scr"] == 0, jk[71000])
# .. JAHKI on (DEPTH up), SCREEN OFF at GUOVSSAHAS: its seasons, never the aurora
ev = [(6200, f"btn {LFO}"), (6400, f"btn {LFO}"), (6600, f"btn {LFO}"), (6800, f"btn {LFO}"), (7000, f"btn {LFO}"),
      (7200, f"enc {K3} 50"), (7600, f"down {HOME}"), (8300, f"up {HOME}"), (8600, f"enc {K4} 5")]
out, a, jk, notes = run("govva_year", ev + P(130000), 131000)
check(".. JAHKI on, GUOVSSAHAS set: the seasons instead of the aurora", jk[130000]["aur"] == 2 and jk[130000]["scr"] == 1, jk[130000])

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

# (2.0) PRESETS on a latched drone: ICE WRAITH (Holvut) on C3, then POLAR NIGHT (VA): the two cross over (the old one
# fading as the new one's ATTACK rises); then POLAR STILL (Holvut again): its knobs glide. Never a hole, never a jump
for what, step, most in (("another source: the notes cross over", 1, 6), ("the same source: the knobs glide", -2, 8)):   # (Holvut
    # itself: its loop settles anew as BODY arrives, ~0.5 s: before 2.0 a hole to -16 dB and 9.5 dB steps)
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

# (2.2) PRESETS undone: a whole browse is one undo, EDIT + OCT- brings back the sound before it (all of it)
ev = [(6200, "probe"), (6400, "enc 6 1"), (6700, "enc 6 1"), (7000, "enc 6 1"), (7400, "probe"),
      (7800, f"down {EDIT}"), (8000, f"btn {OCTD}"), (8200, f"up {EDIT}"), (8600, "probe")]
out, a, jk, notes = run("presets_undo", ev, 9000)
pr = probes(out)
check("PRESETS: turned three times, then EDIT + OCT-: the sound before the browse back", pr[7400][2] != pr[6200][2] and
      pr[8600][2] == pr[6200][2], (pr[6200][2], pr[7400][2], pr[8600][2]))

# (2.2) USB LEVEL's SPK OFF (SYSTEM 3, KNOB 2 past +24 DB): while a computer takes the sound over USB, the speaker and
# the jack quiet (no click: a ramp), the computer still gets it; after, the speaker back
ev = [(5000, f"down {HOME}"), (5700, f"up {HOME}"), (6000, f"btn {HOME}"), (6300, f"btn {HOME}"), (6600, f"enc {K2} 7"),
      (7000, f"keys {1 << 7}"), (8500, "probe"), (11500, "probe"), (13500, "probe"), (14000, "keys 0")]
out, a, jk, notes = run("spk_off", ev, 14200, env={"GRIMM_USB_FEED": "1", "GRIMM_USB_REC": "9000,12000"})
pk = {int(m.group(1)): int(m.group(2)) for m in re.finditer(r"^usbpk (\d+) (\d+)$", out, re.M)}
b, d, f = rms(a, 7.8, 8.8), rms(a, 9.5, 11.8), rms(a, 12.6, 13.6)
check("SPK OFF: the speaker quiet while the computer takes the sound, back after; USB has it",
      b > 100 and d < 0.01 * b and f > 0.3 * b and pk.get(11500, 0) > 1000, (round(b), round(d), round(f), pk))

# (2.2) LFO SHAPE CHAOS: a Lorenz attractor, smooth (no jump in a control step's worth), wide, both sides, never the
# same turn twice
ev = [(5000, f"btn {LFO}"), (5300, f"enc {K2} 100"), (5600, f"enc {K4} 7"), (5900, f"enc {K1} 20")] + \
    [(6000 + 100 * i, "probe") for i in range(400)]
out, a, jk, notes = run("chaos", ev, 46200)
v = [jk[t]["lfa"] for t in sorted(jk)]
step = max(abs(v[i + 1] - v[i]) for i in range(len(v) - 1))
turns = [i for i in range(1, len(v) - 1) if v[i - 1] < v[i] >= v[i + 1] and v[i] > 4000]
peaks = sorted({round(v[i] / 50) for i in turns})
check("LFO CHAOS: wide, both sides, smooth, its turns' tops nearly all different",
      jk[sorted(jk)[0]]["lsh"] == 7 and max(v) > 12000 and min(v) < -12000 and step < 16000 and len(turns) >= 3 and
      len(peaks) >= 0.8 * len(turns), (max(v), min(v), step, len(turns), len(peaks)))

# (2.2) ENV 2 (ENV twice): ONSET, a note's wait; LOOP PULSE, the envelope going round while the note is held; SPREAD,
# the notes taking places in the stereo field (L and R apart), nothing of it at 0
ENVB = 4
def side(a, t0, t1):                            # the RMS of L - R from t0 to t1 s
    x = a[2 * int(t0 * 44100):2 * int(t1 * 44100)]
    d = [x[i] - x[i + 1] for i in range(0, len(x) - 1, 2)]
    return math.sqrt(sum(v * v for v in d) / max(1, len(d)))
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K2} 25"), (7000, f"keys {1 << 7}"), (10800, "keys 0")]
out, a, jk, notes = run("onset", ev, 11000, env={})
check("ENV 2 ONSET 2 s: silent for its wait, then the note", rms(a, 7.1, 8.8) < 30 and rms(a, 9.3, 10.5) > 150,
      (round(rms(a, 7.1, 8.8)), round(rms(a, 9.3, 10.5))))
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K2} 25"), (7000, f"keys {1 << 7}"), (7400, "keys 0")]
out, a, jk, notes = run("onset_tap", ev, 12000)
check("ENV 2 ONSET: a note let go while it waits still plays after the wait", rms(a, 7.1, 8.8) < 30 and
      rms(a, 9.0, 10.0) > 60, (round(rms(a, 7.1, 8.8)), round(rms(a, 9.0, 10.0))))
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K1} 1"), (6000, f"keys {1 << 7}")]
out, a, jk, notes = run("loop", ev, 30000)
w = [rms(a, 10 + 0.25 * i, 10.25 + 0.25 * i) for i in range(76)]
check("ENV 2 LOOP PULSE: a held note goes round, down and up again, more than once",
      max(w) > 3 * (min(w) + 1) and sum(1 for i in range(1, 75) if w[i - 1] > w[i] < w[i + 1] and w[i] < 0.4 * max(w)) >= 2,
      [round(x) for x in w[::4]])
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K3} 100"), (6000, f"keys {1 << 7 | 1 << 11 | 1 << 14}")]
out, a, jk, notes = run("spread", ev, 10000)
out0, a0, jk0, notes0 = run("spread0", [(6000, f"keys {1 << 7 | 1 << 11 | 1 << 14}")], 10000)
check("ENV 2 SPREAD 100 %: three notes apart in the stereo field (twice the side of SPREAD 0's, its reverb's)",
      side(a, 8, 9.5) > 2 * side(a0, 8, 9.5),
      (round(side(a, 8, 9.5)), round(rms(a, 8, 9.5)), round(side(a0, 8, 9.5))))
sv3 = OUT / "saves_env2.bin"
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K1} 2"), (5900, f"enc {K2} 10"), (6200, f"enc {K3} 50"),
      (6500, f"enc {K4} 30")]
out, a, jk, notes = run("env2_save", ev + P(7000), 40000, {"GRIMM_SAVE_OUT": str(sv3)})
out, a, jk2, notes = run("env2_load", P(3000), 3500, {"GRIMM_SAVE_IN": str(sv3)})
e2 = lambda d: (d["el"], d["eo"], d["es"], d["ed"])
check("ENV 2 saved with the project (version 11): at power-on as set (BREATHE, 0.8 s, 50 %, 30 %)",
      e2(jk[7000]) == (2, 10, 50, 30) and e2(jk2[3000]) == (2, 10, 50, 30), (e2(jk[7000]), e2(jk2[3000])))
# a slot keeps its ENV 2: SPREAD 40 into slot A, SPREAD to 0, A recalled: 40 again; a power-on: still in the slot
ev = [(5000, f"btn {ENVB}"), (5300, f"btn {ENVB}"), (5600, f"enc {K3} 40"), (7400, f"down {SAVE}"), (8000, "keys 128"),
      (8200, "keys 0"), (8400, f"up {SAVE}"), (9000, f"enc {K3} -40"), (9400, f"down {SAVE}"), (10000, "keys 1"),
      (10200, "keys 0"), (10400, f"up {SAVE}")]
sv4 = OUT / "saves_e2slot.bin"
out, a, jk, notes = run("e2slot", ev + P(9300, 11000), 40000, {"GRIMM_SAVE_OUT": str(sv4)})
ev = [(5000, f"down {SAVE}"), (5600, "keys 1"), (5800, "keys 0"), (6000, f"up {SAVE}")]
out, a, jk2, notes = run("e2slot_on", P(4000) + ev + P(7000), 7500, {"GRIMM_SAVE_IN": str(sv4)})
check("slot A: its ENV 2 recalled (SPREAD 40, after 0), and after a power-on",
      jk[9300]["es"] == 0 and jk[11000]["es"] == 40 and jk2[7000]["es"] == 40, (jk[9300]["es"], jk[11000]["es"], jk2[7000]["es"]))

# (2.2) the CPU guard: hot (host: GRIMM_CPU_HOT), a new note takes a release over instead of a free voice; cool, never
ev = [(6000, f"keys {1 << 7}"), (6500, "keys 0"), (7000, f"keys {1 << 9}"), (7500, "keys 0"), (8000, f"keys {1 << 11}"),
      (8500, "keys 0")]
out, a, jk, notes = run("cpu_hot", ev + P(9000), 9500, {"GRIMM_CPU_HOT": "1"})
out, a0, jk0, notes = run("cpu_cool", ev + P(9000), 9500)
check("CPU guard: hot, new notes take a release over (no voice added); cool, never; heard either way",
      jk[9000]["took"] >= 2 and jk0[9000]["took"] == 0 and rms(a, 8.0, 9.0) > 100, (jk[9000]["took"], jk0[9000]["took"], round(rms(a, 8, 9))))

# (2.2) Jiekna, the tenth source (the singing ice): on T1 (EDIT twice: SOURCE, + 9), C3 struck: heard, in tune with VA's
# C3, never clipping, its ring dying away with RING as set; RING up to the top: it sings on, held (no clip)
ev = [(5000, f"btn {EDIT}"), (5300, f"btn {EDIT}"), (5600, f"enc {K1} 9"), (7000, f"keys {1 << 7}"), (12000, "keys 0")]
out, a, jk, notes = run("jiekna", ev, 14000)
out, av, jk, notes = run("jiekna_va", [(7000, f"keys {1 << 7}"), (12000, "keys 0")], 14000)
if have_np:
    fj, fv = freq(a, 7.3, 9.3, 110, 160), freq(av, 7.3, 9.3, 110, 160)   # (its fundamental: the strongest there)
    check("Jiekna: heard, never clipping, C3 in tune with VA's (within 1.5 %)", rms(a, 7.2, 8) > 200 and peak(a) < 32000 and
          abs(fj / fv - 1) < 0.015, (round(rms(a, 7.2, 8)), peak(a), round(fj, 1), round(fv, 1)))
ev = [(5000, f"btn {EDIT}"), (5300, f"btn {EDIT}"), (5600, f"enc {K1} 9"), (5800, f"btn {EDIT}"), (6000, f"enc {K1} -90"),
      (6300, f"enc {K2} 27"), (7000, f"keys {1 << 7}"), (25000, "keys 0")]
out, a, jk, notes = run("jiekna_endless", ev, 26000)
check("Jiekna RING at the top, bowed: it sings on by itself, held (no clip, no dying away)",
      rms(a, 20, 24) > 0.5 * rms(a, 9, 13) and rms(a, 20, 24) > 200 and peak(a) < 32000,
      (round(rms(a, 9, 13)), round(rms(a, 20, 24)), peak(a)))

# (2.2) MOTION: REC held, HOME's CUTOFF turned up and down over ~3 s, REC let go: the move plays by itself, over and over
# (REC not left on, the pattern not cleared); saved: at power-on it plays again; REC held + OCT-: gone
RECB = 13
ev = [(6000, f"down {RECB}")] + [(6100 + 100 * i, f"enc {K2} {-3 if i < 15 else 3}") for i in range(30)] + [(9200, f"up {RECB}")]
sv5 = OUT / "saves_motion.bin"
out, a, jk, notes = run("motion", ev + P(*range(9500, 16000, 100), 44000, 120000), 121000, {"GRIMM_SAVE_OUT": str(sv5)})
c = [jk[t]["cut0"] for t in range(9500, 16000, 100)]
check("MOTION: the move plays by itself, down and up, again (its length ~3 s); REC off, a lane", max(c) - min(c) > 25 and
      jk[9500]["mo"] == 1 and "rec 0" in out.split("probe 9500")[1][:400], (min(c), max(c), c[::5], jk[9500]["mo"]))
check(".. what it plays never makes a save of its own (the flash not written again past the first save)",
      jk[120000]["sgen"] == jk[44000]["sgen"], (jk[44000]["sgen"], jk[120000]["sgen"]))
low = [t for t, x in zip(range(9500, 16000, 100), c) if x == min(c)]
check(".. it comes round about every 3 s", any(3000 <= b - a_ <= 3400 for a_ in low for b in low), low)
out, a, jk2, notes = run("motion_on", P(*range(3000, 9000, 100)) + [(9500, f"down {RECB}"), (9700, f"btn {OCTD}"),
                         (9900, f"up {RECB}")] + P(10500, 12000), 12500, {"GRIMM_SAVE_IN": str(sv5)})
c2 = [jk2[t]["cut0"] for t in range(3000, 9000, 100)]
check(".. power-on: it plays again; REC held + OCT-: gone, the knob still", max(c2) - min(c2) > 25 and jk2[3000]["mo"] == 1 and
      jk2[10500]["mo"] == 0 and jk2[10500]["cut0"] == jk2[12000]["cut0"], (min(c2), max(c2), jk2[10500]["mo"]))

# (2.2) the second core's way (host: GRIMM_CPU2): SPACE a block behind (5.8 ms), otherwise the same sound
ev = [(6000, f"keys {1 << 7 | 1 << 14}"), (6500, "keys 0")]
out, a1, jk, notes = run("cpu2_one", ev, 12000, plain=False)
out, a2, jk, notes = run("cpu2_two", ev, 12000, {"GRIMM_CPU2": "1"}, plain=False)
check("the second core's way: SPACE a block behind, its tail as loud (within 0.5 dB), never clipping",
      abs(20 * math.log10((rms(a2, 8, 11) + 1) / (rms(a1, 8, 11) + 1))) < 0.5 and peak(a2) < 32000 and rms(a2, 8, 11) > 100,
      (round(rms(a1, 8, 11)), round(rms(a2, 8, 11)), peak(a2)))

if fails:
    sys.exit(1)
print("drone: JIEHKKI, RUOHCAT (its samples too), TUNING, Biegga, GUOVSSAHAS, GOVVA, PRESETS' crossing over, DICE, ACCIDENT, PRESETS' undo, SPK OFF, LFO CHAOS, ENV 2, the CPU guard, Jiekna, MOTION, the second core's way ok")
