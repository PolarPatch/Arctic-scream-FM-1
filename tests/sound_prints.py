#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.2) The factory sounds' fingerprints (after DEEP FIELD's golden renders, written anew): the host build turns
PRESETS through every factory sound on track 1, plays each a moment on one key and lets it ring, and the sound of each
is hashed and compared with tests/sound_prints.txt. A change to the engine that alters a sound by accident fails here;
a change meant to alter one is taken in with --golden (and said in the commit). The VIBE's engine is this one: the
same prints there.

    tests/sound_prints.py [--golden]          (needs build/host/grimm: tests/run_host.sh builds it)
"""
import hashlib
import os
import struct
import subprocess
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLD = os.path.join(ROOT, "tests", "sound_prints.txt")
OUT = os.path.join(ROOT, "build", "prints")
N = 38                                          # the factory sounds (store.c's SND_NFACT)
T0, STEP, KEY = 7000, 3000, 1 << 12             # (after the boot screen; a sound every 3 s; one key)


def main():
    os.makedirs(OUT, exist_ok=True)
    script = []
    for i in range(N):
        t = T0 + i * STEP
        script += ["%d enc 6 1" % t, "%d keys %d" % (t + 300, KEY), "%d keys 0" % (t + 2000)]
    script.append("%d end" % (T0 + N * STEP))
    sp = os.path.join(OUT, "prints.txt")
    open(sp, "w").write("\n".join(script) + "\n")
    env = dict(os.environ, GRIMM_MASTER="1022")
    subprocess.run([os.path.join(ROOT, "build", "host", "grimm"), os.path.join(OUT, "p"), sp], env=env, check=True,
                   stdout=subprocess.DEVNULL)
    w = wave.open(os.path.join(OUT, "p.wav"))
    rate, ch, frames = w.getframerate(), w.getnchannels(), w.readframes(w.getnframes())
    bpf = ch * w.getsampwidth()
    got = []
    for i in range(N):
        a = (T0 + i * STEP + 300) * rate // 1000 * bpf
        b = (T0 + (i + 1) * STEP) * rate // 1000 * bpf
        seg = frames[a:b]
        peak = max(abs(v) for v in struct.unpack("<%dh" % (len(seg) // 2), seg)) if seg else 0
        got.append("%02d %s %5d" % (i + 1, hashlib.sha1(seg).hexdigest()[:16], peak))
    if "--golden" in sys.argv:
        open(GOLD, "w").write("# tests/sound_prints.py: a factory sound a line (PRESETS turned i times from the first\n"
                              "# start's ICE WRAITH): its print, its peak\n" + "\n".join(got) + "\n")
        print("prints  %d sounds taken in (%s)" % (N, GOLD))
        return
    want = [l.rstrip("\n") for l in open(GOLD) if not l.startswith("#")]
    bad = [(g, w_) for g, w_ in zip(got, want) if g != w_]
    if bad or len(want) != N:
        for g, w_ in bad:
            print("prints  sound %s changed: %s (was %s)" % (g[:2], g[3:], w_[3:]))
        print("prints  FAILED: %d of %d sounds differ (meant? tests/sound_prints.py --golden)" % (len(bad), N))
        sys.exit(1)
    print("prints  the %d factory sounds sound as they did" % N)


if __name__ == "__main__":
    main()
