#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.4) TOUCH (SYSTEM 4, KNOB 3) in host mode (tests/host.c's "touch K VEL PRESS": what key K would read):
  - OFF (the start): the readings change nothing
  - VELOCITY: a key at 127 as with TOUCH off; a soft one quieter, by about the curve's 24 dB from 127 to 1
  - a MIDI note's velocity too, while TOUCH is on
  - VEL+PRESS: pressing harder deepens the track's LFO; with no pressure, as VELOCITY
    python3 tests/touch_test.py build/host/arctic"""
import array
import math
import subprocess
import sys
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else "build/host/arctic"
OUT = Path("build/touchtest")
OUT.mkdir(parents=True, exist_ok=True)
HOME, LFO = 8, 5                                # buttons (platform.c)
K1, K2, K3, K4 = 2, 3, 4, 5                                   # encoders
bad = 0


def check(what, ok, detail=""):
    global bad
    print(f"  {'ok' if ok else 'FAIL':4s}  touch: {what}" + (f" ({detail})" if detail and not ok else ""))
    bad += not ok


def run(name, ev, end=8000):
    (OUT / f"{name}.txt").write_text("".join(f"{t} {e}\n" for t, e in sorted(ev, key=lambda x: x[0])) + f"{end} end\n")
    subprocess.run([BIN, str(OUT / name), str(OUT / f"{name}.txt")], capture_output=True, check=True)
    return array.array("h", (OUT / f"{name}.wav").read_bytes()[44:])


def rms(a, t0, t1):
    s = a[2 * int(44100 * t0):2 * int(44100 * t1)]
    return math.sqrt(sum(x * x for x in s) / max(1, len(s)))


def mode(n):                                    # SYSTEM 4 (HOME held, then HOME three times), KNOB 3 to n
    return [(500, f"down {HOME}"), (1200, f"up {HOME}"), (1500, f"btn {HOME}"), (1800, f"btn {HOME}"),
            (2100, f"btn {HOME}")] + ([(2400, f"enc {K3} {n}")] if n else [])


KEY = [(3000, "keys 0x80"), (6500, "keys 0")]   # (key 7, C4: a note held 3.5 s)
plain = run("plain", mode(0) + KEY)
off = run("off", mode(0) + [(100, "touch 7 20 0")] + KEY)
check("OFF: the readings change nothing", off == plain)
hard = run("hard", mode(1) + [(100, "touch 7 127 0")] + KEY)
check("VELOCITY: a key at 127 as with TOUCH off", hard == plain)
soft = run("soft", mode(1) + [(100, "touch 7 40 0")] + KEY)
db = 20 * math.log10(rms(soft, 3.5, 6.0) / rms(hard, 3.5, 6.0))
check("VELOCITY: a key at 40 about 16.6 dB quieter", -19.0 < db < -14.0, f"{db:.1f} dB")
none = run("none", mode(1) + KEY)
db = 20 * math.log10(rms(none, 3.5, 6.0) / rms(hard, 3.5, 6.0))
check("VELOCITY: no reading in time, played at 100", -6.5 < db < -3.5, f"{db:.1f} dB")

MIDI = [(3000, "midi 0x90 60 30"), (6500, "midi 0x80 60 0")]
m_off = run("midi_off", mode(0) + MIDI)
m_on = run("midi_on", mode(1) + MIDI)
db = 20 * math.log10(rms(m_on, 3.5, 6.0) / rms(m_off, 3.5, 6.0))
check("a MIDI note at 30 quieter while TOUCH is on (off: as ever)", -21.5 < db < -16.5, f"{db:.1f} dB")

LFO_ON = [(2600, f"btn {LFO}"), (2650, f"enc {K1} 60"), (2700, f"enc {K2} 20"), (2750, f"enc {K4} -7")]   # (the
#                                               track's LFO: faster, DEPTH 20, SINE)
p0 = run("press0", mode(2) + LFO_ON + [(100, "touch 7 127 0")] + KEY)
v0 = run("vel_lfo", mode(1) + LFO_ON + [(100, "touch 7 127 0")] + KEY)
p1 = run("press1", mode(2) + LFO_ON + [(100, "touch 7 127 100")] + KEY)
check("VEL+PRESS: no pressure, as VELOCITY", p0 == v0)
check("VEL+PRESS: pressed harder, the LFO deeper", p1 != p0)
sys.exit(1 if bad else 0)
