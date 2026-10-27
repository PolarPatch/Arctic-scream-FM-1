// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 PolarPatch
// Arctic Scream p0larpatch: the demo page's four demos (A1-A4), as the editor's own frames (docs/EDITOR_PROTOCOL.md:
// TRACK sets a track's sound, SEQ its sequence, the tempo, PLAY) and the keys to latch. The sounds are the factory's,
// read from the firmware itself (web/dist/grimm.wasm, after web/build_web.sh), with DRONE turned up, and MANDALORIA,
// a sound of PolarPatch's own. The editor loads the same songs (song: the sounds, the sequences, the latched keys).
//   node web/gen_demos.mjs            -> web/demos.json (the page's), build/demos/*.txt (tests/host.c scripts: hear them)
import { readFileSync, writeFileSync, mkdirSync } from "fs";
import { VirtualFM1, HEAD, pack7, parseSound, buildRecord, buildSeq, emptyStep } from "./editor_core.js";

const vf = await VirtualFM1.create(readFileSync("web/dist/grimm.wasm"));
const meta = vf.meta();
const fact = (name) => {
  const f = meta.factory.find((x) => x.name === name);
  if (!f) throw new Error("no factory sound " + name);
  return parseSound(Uint8Array.from(f.rec), meta);
};
const MANDALORIA = Uint8Array.from(Buffer.from(
  "54000107603f444e381f3d300100231f040400000000000000000000000000000000000000000000000000000000000000004d414e44414c4f524941ff4ad1cb", "hex"));

const NOTE = { C: 0, "C#": 1, D: 2, "D#": 3, E: 4, F: 5, "F#": 6, G: 7, "G#": 8, A: 9, "A#": 10, B: 11 };
const midi = (n) => NOTE[n.slice(0, -1)] + 12 * (+n.slice(-1) + 1);
const key = (n) => midi(n) - 41;                           // the panel's key (0 = F2)

const sound = (name, drone, extra = {}) => {               // a factory sound, DRONE's values set, named on HOME
  const s = { ...(name === "MANDALORIA" ? parseSound(MANDALORIA, meta) : fact(name)), ...extra };
  if (drone) s.drone = { ...s.drone, ...drone };
  s.name = name;
  return buildRecord(s);
};
// a sequence: steps, DIVISION (0 1/4 .. 4 1/16), and per step "NOTE[/len][*chance][xratchet]" or "-"
const seq = (div, steps) => {
  const st = Array.from({ length: 64 }, emptyStep);
  steps.forEach((s, i) => {
    if (s === "-") return;
    const m = /^([A-G]#?\d)(?:\/(\d+))?(?:\*(\d+))?(?:x(\d))?$/.exec(s);
    if (!m) throw new Error("step " + s);
    st[i] = { note: midi(m[1]), len: +(m[2] || 1), chance: +(m[3] || 100), ratchet: +(m[4] || 1), chord: 0 };
  });
  return buildSeq({ steps: steps.length, div, swing: 0, mode: 0, st });
};
const EMPTY = buildSeq({ steps: 16, div: 4, swing: 0, mode: 0, st: Array.from({ length: 64 }, emptyStep) });

const F = {                                                 // the frames, after F0 (without F7)
  track: (t, rec, name) => [...HEAD, 23, 1, t, ...pack7(rec), ...[...name.slice(0, 15)].map((c) => c.charCodeAt(0))],
  select: (t) => [...HEAD, 23, 2, t],
  seq: (t, b) => [...HEAD, 24, 1, t, ...pack7(b)],
  tempo: (bpm) => [...HEAD, 24, 2, 0, bpm & 127, bpm >> 7],
  play: (on) => [...HEAD, 24, 3, 0, on ? 1 : 0],
};

const DEMOS = [
  { id: "A1", name: "MANDALORIA", sub: "Holvut on the edge, over a fjord", tempo: 72,
    tracks: [[0, "MANDALORIA", { borga: 40 }], [1, "DEEP FJORD", { borga: 30 }]],
    latch: [[0, ["C3"]], [1, ["F2"]]] },
  { id: "A2", name: "WHITEOUT", sub: "the storm rises over eight bars", tempo: 66,
    tracks: [[0, "WHITEOUT", { borga: 75, storm: 85, rise: 4 }], [1, "ICEBERG GONG"]],
    seqs: [[1, seq(0, ["D3/3", "-", "-", "-", "-", "-", "A3/2*80", "-", "-", "-", "F3/4", "-", "-", "-", "-", "-"])]],
    latch: [[0, ["D3"]]], play: true },
  { id: "A3", name: "NIGHT TERROR", sub: "close semitones and a dead signal", tempo: 92,
    tracks: [[0, "ICE PIANO"], [1, "DEAD SIGNAL", { borga: 55, storm: 30, rise: 3 }]],
    seqs: [[0, seq(2, ["C4", "C#4*80", "-", "G3", "F#4*70", "-", "C4x2", "-", "D#4", "E4*60", "-", "B3", "C4", "-",
                       "F#3*80", "-"])]],
    latch: [[1, ["F2"]]], play: true },
  { id: "A4", name: "AURORA", sub: "a choir under falling stars", tempo: 80,
    tracks: [[0, "POLAR NIGHT", { borga: 50 }], [1, "STARLIGHT"], [2, "SOLAR WIND", { borga: 40 }]],
    seqs: [[1, seq(2, ["C5*90", "-", "G4*60", "E5*80", "-", "B4*50", "-", "G5*40x2", "D5*80", "-", "A4*60", "-", "E5*70",
                       "-", "C5*50x2", "-"])]],
    latch: [[0, ["C3", "G3", "D4"]], [2, ["G3"]]], play: true },
];

const out = [];
for (const d of DEMOS) {
  const act = [{ sx: F.play(0) }, { panic: 1 }];           // what played before: stopped, its notes let go
  for (let t = 0; t < 4; t++) act.push({ sx: F.seq(t, EMPTY) });
  for (const [t, name, drone] of d.tracks) act.push({ sx: F.track(t, sound(name, drone), name) });
  for (const [t, b] of d.seqs || []) act.push({ sx: F.seq(t, b) });
  act.push({ sx: F.tempo(d.tempo) });
  for (const [t, ks] of d.latch || []) {
    act.push({ sx: F.select(t) });
    act.push({ keys: ks.reduce((m, n) => m | 1 << key(n), 0) });
  }
  act.push({ sx: F.select(0) });
  if (d.play) act.push({ sx: F.play(1) });
  const sq = [0, 1, 2, 3].map((t) => [...((d.seqs || []).find(([u]) => u === t) || [t, EMPTY])[1]]);
  const tr = d.tracks.map(([t, name, drone]) => [t, name, [...sound(name, drone)]]);
  const latch = (d.latch || []).map(([t, ks]) => [t, ks.reduce((m, n) => m | 1 << key(n), 0)]);
  out.push({ id: d.id, name: d.name, sub: d.sub, tempo: d.tempo, tracks: d.tracks.map(([t, n]) => [t, n]), act,
             song: { tr, sq, latch, play: !!d.play } });   // (the editor's: the same, as sounds and sequences)
}
writeFileSync("web/demos.json", JSON.stringify(out));
console.log("web/demos.json:", out.map((d) => `${d.id} ${d.name} (${d.act.length})`).join(", "));

// the same for tests/host.c: boot, then each demo alone for 24 s (its sound in build/demos/<id>.wav)
mkdirSync("build/demos", { recursive: true });
const hx = (f) => f.map((b) => b.toString(16).padStart(2, "0")).join(" ");
for (const d of out) {
  let t = 4000, s = "";
  for (const a of d.act) {
    if (a.sx) s += `${t} sysex ${hx(a.sx)}\n`;
    else if (a.keys) { s += `${t} keys ${a.keys}\n${t + 120} keys 0\n`; t += 120; }
    t += 60;
  }
  writeFileSync(`build/demos/${d.id}.txt`, s + `${t + 24000} end\n`);
}
