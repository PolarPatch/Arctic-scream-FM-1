#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""(2.1.9) Arctic Scream's Wi-Fi from a computer, over USB (SysEx F0 7D 4E 57 ..: firmware/src/net.c nw_frame).
The password goes down the cable to the FM-1 (or the SMK-37), which keeps only the WPA2 key made from it.

  arctic_wifi.py join SSID PASSWORD   join a network (WIFI turns to JOIN); kept, joined again at every start
  arctic_wifi.py forget               forget every network kept
  arctic_wifi.py hotspot PASSWORD     the hotspot's own password (8..63 characters); "" for a new random one
  arctic_wifi.py status               what the NETWORK page says
  [--port NAME]                       the MIDI port (part of its name; default: the first Arctic Scream / Felucca one)

Needs mido with python-rtmidi."""
import argparse
import sys
import time

try:
    import mido
except ImportError:
    sys.exit("needs mido and python-rtmidi (pip install mido python-rtmidi)")

HEAD = [0x7D, 0x4E, 0x57]


def port_name(names, want):
    for n in names:
        if want and want.lower() in n.lower():
            return n
        if not want and any(k in n.lower() for k in ("arctic scream", "felucca", "fm-1")):
            return n
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["join", "forget", "hotspot", "status"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--port")
    a = ap.parse_args()
    data = []
    if a.cmd == "join":
        if len(a.args) != 2:
            sys.exit("join SSID PASSWORD")
        ssid, pw = a.args
        if not ssid or len(ssid.encode()) > 32 or not (8 <= len(pw) <= 63) or not (ssid + pw).isascii():
            sys.exit("SSID 1..32 characters, the password 8..63 (ASCII)")
        data = [1, *ssid.encode(), 0, *pw.encode(), 0]
    elif a.cmd == "forget":
        data = [2]
    elif a.cmd == "hotspot":
        pw = a.args[0] if a.args else ""
        if pw and not (8 <= len(pw) <= 63 and pw.isascii()):
            sys.exit("the hotspot's password: 8..63 characters (ASCII)")
        data = [3, *pw.encode(), 0]
    else:
        data = [4]
    out_n = port_name(mido.get_output_names(), a.port)
    in_n = port_name(mido.get_input_names(), a.port)
    if not out_n or not in_n:
        sys.exit("no Arctic Scream found on USB-MIDI")
    with mido.open_input(in_n) as inp, mido.open_output(out_n) as out:
        while inp.poll():
            pass
        out.send(mido.Message("sysex", data=HEAD + data))
        t0 = time.time()
        while time.time() - t0 < 3:
            m = inp.poll()
            if m is None:
                time.sleep(0.01)
                continue
            if m.type == "sysex" and list(m.data[:3]) == HEAD and m.data[3] == data[0]:
                ok = m.data[4] == 0
                lines = bytes(m.data[5:]).split(b"\0")
                print("ok" if ok else "refused")
                for ln in lines[:4]:
                    if ln:
                        print("  " + ln.decode("ascii", "replace"))
                return 0 if ok else 1
    sys.exit("no answer (a build without the radio?)")


if __name__ == "__main__":
    sys.exit(main())
