# PANEL OUT

What the hands do on an M-VAVE FM-1's panel, sent to an app over USB-MIDI: which buttons and keys are down, the
octave, and each knob as it turns. An app can then draw the panel as it is played (a visualiser, a tutorial, a video).

This document is dedicated to the public domain ([CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)): any
firmware or app may use the protocol. The reference code, `firmware/src/panel_out.c`, is MIT on its own (the rest of
Arctic Scream is GPL-3.0), so other FM-1 firmwares can copy that file as it is. First in Arctic Scream 1.5.2.

## Rules

- SysEx, non-commercial ID: `F0 7D 50 4F cmd args… F7` ("PO"). Every data byte is 7-bit.
- **Silent until asked.** The device sends nothing until an app sends HELLO. An app sends HELLO again every ~2 s; a
  device that has not heard one for 5 s stops. A DAW on the same port never sees a byte while no app listens.
- The device never waits for USB: a message with no room in its queue goes out on a later pass. A knob's steps add up
  meanwhile, so none are lost. A knob is sent at most every 20 ms.
- It only reports. An app cannot change anything on the device through PANEL OUT.

## App → device

| cmd | Name | Args | |
| --- | --- | --- | --- |
| 1 | HELLO | ver (1) | Start listening, or keep listening. Answered with HELLO, then STATE |
| 2 | BYE | | Stop |
| 3 | STATE | | Send STATE now |

## Device → app

| cmd | Name | Args |
| --- | --- | --- |
| 1 | HELLO | ver (1), key 0's MIDI note now, octave + 64, keys (27), buttons (14), the firmware's name in ASCII up to F7 |
| 16 | STATE | buttons (2 bytes × 7 bits, bit 0 first), keys (4 bytes × 7 bits, bit 0 first), octave + 64 |
| 17 | KNOB | id, mode, value. Mode 0: relative, value = detents since the last + 64 (1 … 127). Mode 1: absolute, value 0 … 127 |

STATE is sent after each HELLO and whenever a button, a key or the octave changes.

### Buttons (bit = id)

| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OCT− | OCT+ | FX | SCL (SELECT) | ENV | LFO | EDIT | GLO | HOME | SAVE | ARP | SEQ | PLAY | REC |

### Keys

Bit 0 is the lowest key (bottom left); bit 26 the highest. Key *k* plays MIDI note `key0 + k`, where `key0` comes from
HELLO and follows the octave. In Arctic Scream `key0` is 41 (F2) at octave 0. Other firmwares may map keys differently
and say so in their HELLO.

### Knobs

| id | Knob | Mode |
| --- | --- | --- |
| 0 … 3 | KNOB 1 … 4 | relative |
| 4 | SELECT | relative |
| 5 | ALGORITHM | relative |
| 6 | PRESETS | relative |
| 7 | MASTER | absolute (sent after HELLO and when it moves) |

## Example

```
app    F0 7D 50 4F 01 01 F7                                  HELLO
device F0 7D 50 4F 01 01 29 40 1B 0E 41 72 63 … F7           HELLO: v1, key 0 = 41, octave 0, 27 keys, 14 buttons, "Arctic Scream 1.5.2"
device F0 7D 50 4F 10 00 00 00 00 00 00 40 F7                STATE: nothing down, octave 0
device F0 7D 50 4F 11 07 01 7F F7                            KNOB: MASTER at 127
device F0 7D 50 4F 10 40 00 00 00 00 00 40 F7                STATE: EDIT down
device F0 7D 50 4F 11 00 00 45 F7                            KNOB: KNOB 1 turned 5 detents up
```
