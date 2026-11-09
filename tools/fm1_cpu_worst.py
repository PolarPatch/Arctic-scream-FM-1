#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""Arctic Scream's worst case on the FM-1 (1.5.2): every effect on and high, all four tracks holding chords (more notes
than the six voices: the voice budget full), and the editor's CPU (25) read every second; then the effects as they
were and the notes off.
  python3 tools/fm1_cpu_worst.py               20 s
  python3 tools/fm1_cpu_worst.py --seconds 60
Before: put the heaviest sounds you play on T1..T4 (HOWL, PHYS, FM ...), set MIDI CHANNEL to 1-4 (SYSTEM), and set
CPU PEAK to 0 (SYSTEM 2: OCT- + OCT+). It ends with NOW's highest reading, PEAK, and how deep the stacks went. Under
100 % the FM-1 keeps up; past it Arctic Scream lets go of a note (CPU FULL: NOTE DROPPED), nothing worse.
Needs mido and python-rtmidi, as tools/fm1_install.py.
"""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fm1_sample_upload import Link  # noqa: E402  (the FM-1's MIDI ports, the identity checked)
from fm1_cpu import CPU, rd35       # noqa: E402

FX = 26
# (index, value): TAPE, GRAIN, DELAY (TIME at its longest), SPACE, MASTER (BP, after the reverb), DRIVE (FOLD), PING-PONG,
# SHIMMER (docs/EDITOR_PROTOCOL.md, 26 FX)
WORST = [(0, 64), (1, 100), (2, 40), (3, 30), (4, 80), (5, 127), (6, 80), (7, 100), (8, 127), (9, 100), (10, 100),
         (11, 100), (12, 127), (13, 64), (14, 100), (15, 64), (16, 64), (17, 64), (18, 2), (19, 1), (20, 100), (21, 3),
         (22, 64), (23, 100), (28, 127), (29, 127)]
CHORDS = [[40, 47, 52], [52, 55, 59], [64, 67, 71], [76, 79, 83]]   # T1..T4: twelve notes for six voices


def fx_read(link):
    r = link.req(FX, [0], 2.0)
    if len(r) < 2 + 30 or r[1] != 0:
        raise SystemExit("no effects from the FM-1 (an Arctic Scream before 1.4?)")
    return r[2:2 + 30]


def fx_set(link, i, v):
    link.req(FX, [1, i, v], 2.0)


def cpu(link):
    r = link.req(CPU, [], 1.5)
    n = r[0]
    v = [rd35(r[1 + 5 * k:6 + 5 * k]) for k in range(len(r[1:]) // 5)]
    budget, now, peak = v[0], v[1], v[2]
    stacks = v[3 + n:3 + n + 4] if len(v) >= 3 + n + 4 else None
    return 100.0 * now / budget, 100.0 * peak / budget, stacks


def main():
    ap = argparse.ArgumentParser(description="Arctic Scream's worst case on the FM-1.",
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--seconds", type=int, default=20, help="how long to hold it (default 20)")
    a = ap.parse_args()
    link = Link()
    for _ in range(3):                          # (the first message after opening a port can be lost)
        try:
            was = fx_read(link)
            break
        except TimeoutError:
            pass
    else:
        sys.exit("no answer from the FM-1")
    top = 0.0
    try:
        for i, v in WORST:
            fx_set(link, i, v)
        for ch, chord in enumerate(CHORDS):
            for n in chord:
                link.o.send(link.mido.Message("note_on", channel=ch, note=n, velocity=110))
        for s in range(a.seconds):
            time.sleep(1.0)
            now, peak, _ = cpu(link)
            top = max(top, now)
            print(f"{s + 1:3d} s  NOW {now:5.1f}%  PEAK {peak:5.1f}%")
    finally:
        for ch, chord in enumerate(CHORDS):
            for n in chord:
                link.o.send(link.mido.Message("note_off", channel=ch, note=n, velocity=0))
        for i, v in enumerate(was):
            fx_set(link, i, v)
    now, peak, st = cpu(link)
    print(f"\nworst NOW {top:.1f}%, PEAK {peak:.1f}% of the 5.8 ms a block has (the effects back as they were)")
    if st and st[1] and st[3]:
        print(f"stacks: main loop {st[0]} of {st[1]} B ({100.0 * st[0] / st[1]:.0f}%), "
              f"interrupts {st[2]} of {st[3]} B ({100.0 * st[2] / st[3]:.0f}%)")
    else:
        print("stacks: not reported (Arctic Scream before 1.5.2)")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except TimeoutError as e:
        sys.exit(f"{e}: is Arctic Scream running, and no other app using the FM-1's MIDI port?")
