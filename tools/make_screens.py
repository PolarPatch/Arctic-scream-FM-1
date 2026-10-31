#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream p0larpatch: the manual's screens (docs/manual/screens/*.png), taken from the firmware itself.
Each screen is a short run of tests/host.c (the same firmware/src/grimm.c the FM-1 runs): a script presses the
buttons, turns the knobs and plays the keys, and the screen as it is at the end is saved, 240 x 240 drawn 4 x.
Run from the repository (Pillow needed):  python3 tools/make_screens.py [name ...]"""
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "manual", "screens")
HOST = os.path.join(ROOT, "build", "screens", "grimm")

# the panel's ids (firmware/src/platform.c)
OCTD, OCTU, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC = range(14)
ESELECT, EALGO, K1, K2, K3, K4, EPRESETS = range(7)
WHITE = [0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24, 26]   # the white keys, from F2


def key(n):                                  # "C3" -> the key's bit (0 = F2)
    order = "C C# D D# E F F# G G# A A# B".split()
    return order.index(n[:-1]) + 12 * (int(n[-1]) + 1) - 41


class S:
    """a script for tests/host.c"""

    def __init__(self, t=4000):
        self.t, self.lines, self.mask = t, [], 0

    def w(self, ms):
        self.t += ms
        return self

    def _l(self, s):
        self.lines.append((self.t, s))

    def tap(self, *bs):
        for b in bs:
            self._l(f"btn {b}")
            self.t += 300
        return self

    def down(self, b):
        self._l(f"down {b}")
        self.t += 600
        return self

    def up(self, b):
        self._l(f"up {b}")
        self.t += 300
        return self

    def turn(self, e, d):                    # (steps of 8 count as they are; the rest a step at a time, slowly)
        s = 1 if d > 0 else -1
        for _ in range(abs(d) // 8):
            self._l(f"enc {e} {8 * s}")
            self.t += 30
        for _ in range(abs(d) % 8):
            self._l(f"enc {e} {s}")
            self.t += 100
        self.t += 150
        return self

    def keys(self, *ns, hold=150):
        m = 0
        for n in ns:
            m |= 1 << (key(n) if isinstance(n, str) else n)
        self._l(f"keys {self.mask | m}")
        self.t += hold
        self._l(f"keys {self.mask}")
        self.t += 150
        return self

    def hold(self, *ns):                     # keys held down until the end
        for n in ns:
            self.mask |= 1 << (key(n) if isinstance(n, str) else n)
        self._l(f"keys {self.mask}")
        self.t += 150
        return self


def va(s):                                   # T1 (ICE WRAITH, Holvut) to VA, LATCH OFF; back on its SOUND page
    return s.tap(EDIT, EDIT).turn(K1, -7).turn(K2, -1).tap(EDIT)


SHOTS = {
    "boot": lambda: S(1600),
    "home": lambda: S().keys("C3").keys("G3").w(2500),
    "holvut": lambda: S().keys("C3").w(800).tap(EDIT).turn(K1, 24).w(6000),
    "source": lambda: S().tap(EDIT, EDIT).w(800),
    "va": lambda: va(S()).hold("C3", "D#3", "G3").w(1500),
    "fm-algo": lambda: S().tap(EDIT, EDIT).turn(K1, -6).tap(EDIT, EDIT).turn(K3, 2).w(800),
    "fm-op": lambda: S().tap(EDIT, EDIT).turn(K1, -6).tap(EDIT, EDIT).turn(K3, 2).tap(EDIT).w(800),
    "env": lambda: S().keys("C3").tap(ENV).w(1500),
    "lfo": lambda: S().keys("C3").tap(LFO).turn(K2, 24).w(1500),
    "drone": lambda: S().keys("C3").keys("G3").tap(LFO, LFO).turn(K1, 64).turn(K2, 48).turn(K3, 3).w(3000),
    "scale": lambda: S().tap(SEL).turn(K2, 40).w(800),
    "chord": lambda: S().tap(SEL, SEL).turn(K1, 3).w(800),
    "arp": lambda: S().tap(ARP).w(800),
    "seq": lambda: va(S()).tap(SEQ, REC).keys("C3").keys("D#3").keys("G3").keys("A#3").keys("C4").keys("G3")
                          .keys("D#3").keys("F3").tap(REC, PLAY).w(1900),
    "save": lambda: S().tap(SAVE).w(800),
    "song": lambda: S().tap(GLO).w(800),
    "mixer": lambda: S().keys("C3").tap(HOME).w(1500),
    "tape": lambda: S().keys("C3").tap(FX).turn(K1, 40).w(2000),
    "grain": lambda: S().keys("C3").tap(FX, FX).turn(K4, 50).w(2000),
    "delay": lambda: S().keys("C3").tap(FX, FX, FX).turn(K4, 50).turn(K2, 60).w(2000),
    "space": lambda: S().keys("C3").tap(FX, FX, FX, FX).turn(K3, 40).w(2000),
    "perform": lambda: S().keys("C3").w(500).down(FX).w(1200),
    "system": lambda: S().down(HOME).w(900).up(HOME).w(500),
    "system2": lambda: S().down(HOME).w(900).up(HOME).tap(HOME).w(500),
}


def build():
    os.makedirs(os.path.dirname(HOST), exist_ok=True)
    subprocess.run(["cc", "-O2", "-w", "-DDINGHY_HOST", "-DAPP_VER=\"1.2\"", "-Dsection(x)=used",
                    "-Ifirmware/src", "-Ifirmware/gen", "-o", HOST, "firmware/src/grimm.c"], cwd=ROOT, check=True)


def shot(name, s):
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "s")
        with open(out + ".txt", "w") as f:
            f.write("".join(f"{t} {l}\n" for t, l in sorted(s.lines, key=lambda x: x[0])) + f"{s.t} end\n")
        subprocess.run([HOST, out, out + ".txt", "4"], check=True, stdout=subprocess.DEVNULL)
        last = sorted(p for p in os.listdir(tmp) if p.endswith(".ppm"))[-1]
        im = Image.open(os.path.join(tmp, last)).convert("RGB")
    im.resize((960, 960), Image.NEAREST).save(os.path.join(OUT, name + ".png"), optimize=True)
    print(name)


if __name__ == "__main__":
    build()
    for n in sys.argv[1:] or SHOTS:
        shot(n, SHOTS[n]())
