#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""The 64-bit multiplies the FM-1's CPU may get wrong (Arctic Scream p0larpatch 1.4).

On the FM-1 a 32x32->64-bit multiply (`r5_r4 = r4 * r14 (s)`) inlined among 16-bit loads (`r5 = h[...]`) now and
then leaves a register one bit off, and the firmware crashes (felucca-bsp docs/CPU.md). This disassembles
build/grimm.elf (after ./build.sh) and lists each function where such a multiply is within WINDOW instructions of a
16-bit load. The fix: dsp.c's mul64_sh (out of line), or the product split in two 32-bit ones (fm6_mulg).

  python3 tools/check_mul64.py [WINDOW]     (default 16; exit 1 if any is found)

Needs the toolchain as ./build.sh does (JIELI_TOOLCHAIN, Docker on a Mac)."""
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
os.environ.setdefault("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain"))
import build  # noqa: E402

MUL64 = re.compile(r"r\d+_r\d+ \+?= r\d+ \* r\d+")
LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):\s*$")


def main():
    window = int(sys.argv[1]) if len(sys.argv) > 1 else 16
    elf = build.SRC / "build" / "grimm.elf"
    if not elf.exists():
        raise SystemExit("check_mul64: no build/grimm.elf (run ./build.sh first)")
    syms = []
    for line in build.tc("common/bin/objdump", "-t", elf).splitlines():
        p = line.split()
        if " F " in line and len(p) >= 6:
            syms.append((int(p[0], 16), int(p[-2], 16), p[-1]))
    code, fn = [], None                          # (the label it is under, its address, the instruction)
    for line in build.tc("common/bin/objdump", "-d", elf).splitlines():
        m = LABEL.match(line)
        if m:
            fn = m.group(1)
        elif "\t" in line and ":" in line:
            code.append((fn, line.split(":")[0].strip(), line.split("\t")[-1].strip()))
    found = {}
    for j, (f, a, ins) in enumerate(code):
        if not MUL64.search(ins):
            continue
        near = [k for k in range(max(0, j - window), min(len(code), j + window + 1))
                if code[k][0] == f and "= h[" in code[k][2]]
        if near:
            addr = int(a, 16)
            name = next((n for s, size, n in syms if s <= addr < s + size), f)
            found.setdefault(name, []).append(a)
    for name, addrs in sorted(found.items()):
        print(f"  {name}: {len(addrs)} ({', '.join(addrs)})")
    print(f"check_mul64: {sum(map(len, found.values()))} multiplies beside 16-bit loads (within {window})")
    sys.exit(1 if found else 0)


if __name__ == "__main__":
    main()
