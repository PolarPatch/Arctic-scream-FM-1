// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 GR!MM
// GR!MM's editor without its page: the editor protocol (docs/EDITOR_PROTOCOL.md) over a transport (Web MIDI to the
// FM-1, or the "virtual FM-1": grimm.wasm, the firmware's own code, stepped a millisecond at a time), the tracks' sounds
// live (TRACK, sent as SLOOP's editor sends its SETs: one at a time, the latest kept), the sound records,
// the backup file, and a WAV made a sample slot (a port of Felucca's tools/sampleio.py, GPL-3.0, and of
// tools/fm1_sample_upload.py: the same bytes). web/editor.js (the page) and web/test_web.mjs use it.

export const HEAD = [0x7d, 0x41, 0x53];   // "AS": Arctic Scream's (GR!MM's 7D 47 52)
export const CMD = { SMP_BEGIN: 11, SMP_WRITE: 12, SMP_END: 13, SMP_ERASE: 14, SMP_INFO: 15, SMP_FACTORY: 16, INFO: 20, GET: 21, PUT: 22,
                     TRACK: 23, SEQ: 24 };
export const PROTOS = [1, 2, 3];                           // the editor protocols this page knows (2: TRACK, 3: SEQ)
export const OBJ = { SET: 0, SLOT: 1, SND: 16, FACT: 80 };
const BEGIN = 0, DATA = 1, COMMIT = 2, ABORT = 3;
export const RC = {
  1: "the FM-1 did not accept the request (arguments)",
  2: "the FM-1 refused it: the data is damaged or not valid",
  3: "the FM-1 is playing or still sounding: stop the sequencer, let every note and tail end, then try again",
  4: "the FM-1's flash write failed: try again",
  5: "the transfer timed out on the FM-1: try again",
};
const SMP_RC = { 1: "bad arguments", 2: "flash erase failed", 3: "flash write failed", 4: "out of order",
                 5: "header or data CRC wrong", 6: RC[3], 7: "no flash" };
export class EditorError extends Error {
  constructor(msg, rc) { super(msg); this.rc = rc; }
}

// ------------------------------------------------------------------ bytes
const CRC_T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
export function crc32(b) {                                  // zlib's
  let c = 0xffffffff;
  for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 255] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
export function pack7(b) {                                  // groups of up to 7 bytes, each after their top bits
  const out = [];
  for (let i = 0; i < b.length; i += 7) {
    const g = b.slice(i, i + 7);
    let top = 0;
    g.forEach((x, j) => { top |= ((x >> 7) & 1) << j; });
    out.push(top, ...Array.from(g, (x) => x & 0x7f));
  }
  return out;
}
export function unpack7(b) {
  const out = [];
  for (let i = 0; i < b.length;) {
    const top = b[i++], g = b.slice(i, i + 7);
    g.forEach((x, j) => out.push(x | (((top >> j) & 1) << 7)));
    i += g.length;
  }
  return Uint8Array.from(out);
}
export const hex = (b) => Array.from(b, (x) => x.toString(16).toUpperCase().padStart(2, "0")).join(" ");
export const u35 = (v) => [0, 1, 2, 3, 4].map((k) => Math.floor(v / 2 ** (7 * k)) & 0x7f);
export const rd35 = (b, i = 0) => b.slice(i, i + 5).reduce((a, x, k) => a + x * 2 ** (7 * k), 0);

// ------------------------------------------------------------------ the client
// transport.request(frame) -> Promise of the reply's bytes after its cmd; frame: 7D 41 53 cmd args (no F0 / F7)
export class Client {
  constructor(transport) { this.t = transport; this.chain = Promise.resolve(); }
  wait(ms) {                                               // time for the FM-1 (the virtual one: its clock run on)
    if (this.t.step) { this.t.step(ms); return Promise.resolve(); }
    return new Promise((res) => setTimeout(res, ms));
  }
  req(cmd, args = [], timeout = 3000, quiet = false) {     // one at a time, in the order asked (quiet: not in the log)
    const p = this.chain.then(() => this.t.request([...HEAD, cmd, ...args], timeout, quiet));
    this.chain = p.catch(() => {});
    return p;
  }

  async info() {
    let r;
    for (let i = 0; ; i++) {                                 // (the first message after opening a port can be lost)
      try { r = await this.req(CMD.INFO, [], 1500); break; } catch (e) { if (i >= 2) throw e; }
    }
    const j = r.indexOf(0, 2), i = j + 1;
    if (j < 0 || r.length < i + 15) throw new EditorError(`a short INFO from the FM-1 (${r.length} bytes): see the MIDI LOG`, 95);
    return { proto: r[0], fmt: r[1], firmware: String.fromCharCode(...r.slice(2, j)), sounds: r[i], soundSize: r[i + 1],
             factory: r[i + 2], slotSize: rd35(r, i + 3), setSize: rd35(r, i + 8), sampleSlots: r[i + 13],
             samplesUsed: r[i + 14] };
  }
  async track(t, quiet = false) {                          // track t's sound now: { sel (the FM-1's track), rec, name (as
    const r = await this.req(CMD.TRACK, [0, t], 1500, quiet);  // its HOME shows it; "" none) }
    if (r[3]) throw new EditorError(RC[r[3]] || `rc ${r[3]}`, r[3]);
    const rec = unpack7(r.slice(4, 4 + SND7));
    if (rec.length !== SND) throw new EditorError(`track ${t + 1}: ${rec.length} bytes back, not ${SND}`, 99);
    return { sel: r[2], rec, name: String.fromCharCode(...r.slice(4 + SND7)) };
  }
  async trackSet(t, rec, name = "") {                      // track t := rec, live (no flash), named so on its HOME;
    const nm = Array.from(String(name).slice(0, 15), (c) => (c.charCodeAt(0) >= 32 && c.charCodeAt(0) < 127 ? c.charCodeAt(0) : 32));
    const r = await this.req(CMD.TRACK, [1, t, ...pack7(rec), ...nm], 1500, true);   // -> the FM-1's track
    if (r[3]) throw new EditorError(RC[r[3]] || `rc ${r[3]}`, r[3]);
    return r[2];
  }
  async select(t) { return (await this.req(CMD.TRACK, [2, t], 1500))[2]; }
  // SEQ (protocol 3): track t's sequence, live as SEQ's knobs -> { sel, bpm, playing, pos (the step playing, -1 none),
  // bytes (SEQ_PK: READ only) }
  async seqOp(args, quiet) {
    const r = await this.req(CMD.SEQ, args, 1500, quiet);
    if (r[3]) throw new EditorError(RC[r[3]] || `rc ${r[3]}`, r[3]);
    return { sel: r[2], bpm: r[4] | r[5] << 7, playing: !!r[6], pos: r[7] - 1, bytes: r.length > 8 ? unpack7(r.slice(8)) : null };
  }
  async seq(t, quiet = false) {
    const s = await this.seqOp([0, t], quiet);
    if (!s.bytes || s.bytes.length !== SEQ_PK) throw new EditorError(`sequence ${t + 1}: ${s.bytes ? s.bytes.length : 0} bytes back, not ${SEQ_PK}`, 99);
    return s;
  }
  seqSet(t, bytes) { return this.seqOp([1, t, ...pack7(bytes)], true); }
  play(on) { return this.seqOp([3, 0, on ? 1 : 0], true); }
  tempo(bpm) { const b = Math.max(20, Math.min(300, bpm | 0)); return this.seqOp([2, 0, b & 0x7f, b >> 7], true); }
  async get(obj, size, progress) {
    const out = new Uint8Array(size);
    for (let off = 0; off < size; off += 256) {
      const c = Math.min(256, size - off), r = await this.req(CMD.GET, [obj, ...u35(off), c & 0x7f, c >> 7]);
      if (r[0] !== obj || r[1]) throw new EditorError(RC[r[1]] || `rc ${r[1]}`, r[1]);
      const d = unpack7(r.slice(9));
      if (d.length !== c) throw new EditorError(`object ${obj}: ${d.length} bytes back, not ${c}`, 99);
      out.set(d, off);
      if (progress) progress(off + c, size);
    }
    return out;
  }
  async put(obj, data, fmt, progress) {
    const step = async (args, timeout) => {
      const r = await this.req(CMD.PUT, args, timeout);
      if (r[2]) {
        await this.req(CMD.PUT, [ABORT, obj]).catch(() => {});
        throw new EditorError(RC[r[2]] || `rc ${r[2]}`, r[2]);
      }
    };
    await step([BEGIN, obj, fmt, ...u35(data.length), ...u35(crc32(data))]);
    for (let off = 0; off < data.length; off += 256) {
      await step([DATA, obj, ...u35(off), ...pack7(data.slice(off, off + 256))]);
      if (progress) progress(Math.min(off + 256, data.length), data.length);
    }
    await step([COMMIT, obj], 10000);
  }
  async smpInfo() {
    const r = await this.req(CMD.SMP_INFO, [], 2000), out = [];
    let i = 1;
    for (let k = 0; k < r[0]; k++) {
      const st = r[i], j = r.indexOf(0, i + 1);
      if (j < 0 || j + 7 > r.length) { out.push({ state: -1, name: "", n: 0, cap: 0 }); continue; }   // (cut short: GR!MM
                                                             // before 26b2 with four long names)
      out.push({ state: st, name: String.fromCharCode(...r.slice(i + 1, j)), n: r[j + 1] | r[j + 2] << 7 | r[j + 3] << 14,
                 cap: r[j + 4] | r[j + 5] << 7 | r[j + 6] << 14, root: null });
      i = j + 7;
    }
    if (r.length >= i + out.length) out.forEach((s, k) => { s.root = r[i + k]; });   // (26c: the roots; before: none)
    return out;
  }
  async smpFactory() {                                     // all four slots their factory samples again (~5 s), checked
    const r = await this.req(CMD.SMP_FACTORY, [], 30000);   // -> the slots (as smpInfo)
    if (r[0]) throw new EditorError(`restore: ${SMP_RC[r[0]] || `rc ${r[0]}`}`, r[0] === 6 ? 3 : r[0]);
    for (let i = 0; i < 200; i++) {
      const s = await this.smpInfo();
      if (!s.some((x) => x.state === 2)) return s;
      await this.wait(50);
    }
    return this.smpInfo();
  }
  async smpUpload(slot, hdr, data, progress) {             // as tools/fm1_sample_upload.py: BEGIN, WRITE x n, END
    const ok = (r, what) => { const rc = r[r.length - 1]; if (rc) throw new EditorError(`${what}: ${SMP_RC[rc] || `rc ${rc}`}`, rc === 6 ? 3 : rc); };
    ok(await this.req(CMD.SMP_BEGIN, [slot], 5000), "begin");
    for (let off = 0; off < data.length; off += 256) {
      const o = 512 + off;
      ok(await this.req(CMD.SMP_WRITE, [slot, o & 0x7f, (o >> 7) & 0x7f, (o >> 14) & 0x7f, ...pack7(data.slice(off, off + 256))]),
         `write at ${o}`);
      if (progress) progress(Math.min(off + 256, data.length), data.length);
    }
    ok(await this.req(CMD.SMP_END, [slot, ...pack7(hdr)], 5000), "end");
    for (let i = 0; i < 100; i++) {                          // the FM-1 checks and indexes it (well under a second)
      const s = await this.smpInfo();
      if (s[slot].state !== 2) return s;
      await this.wait(50);
    }
    return this.smpInfo();
  }
  async smpErase(slot) {
    const r = await this.req(CMD.SMP_ERASE, [slot], 10000);
    if (r[1]) throw new EditorError(`erase: ${SMP_RC[r[1]] || `rc ${r[1]}`}`, r[1] === 6 ? 3 : r[1]);
  }
}

// ------------------------------------------------------------------ transports
export class MidiTransport {                                 // Web MIDI (needs SysEx permission)
  constructor(input, output, log = null) {
    this.input = input, this.output = output, this.wait = null, this.log = log, this.sent = "";
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d[0] !== 0xf0) return;
      if (this.log && !(this.wait && this.wait.quiet)) this.log("in", d, input.name);
      if (d[d.length - 1] !== 0xf7 || !this.wait || hex(d) === this.sent) return;   // (our own frame back: a loop port)
      if (d[1] !== HEAD[0] || d[2] !== HEAD[1] || d[3] !== HEAD[2] || d[4] !== this.wait.cmd) return;
      const w = this.wait;
      this.wait = null;
      clearTimeout(w.timer);
      w.res(Array.from(d.subarray(5, d.length - 1)));
    };
  }
  request(frame, timeout = 3000, quiet = false) {
    if (this.wait) return Promise.reject(new EditorError("one request at a time", 98));
    return new Promise((res, rej) => {
      const f = [0xf0, ...frame, 0xf7];
      this.wait = { cmd: frame[3], res, quiet, timer: setTimeout(() => { this.wait = null; rej(new EditorError("no answer from the FM-1", 97)); }, timeout) };
      this.sent = hex(f);
      if (this.log && !quiet) this.log("out", f, this.output.name);
      this.output.send(f);
    });
  }
  close() { this.input.onmidimessage = null; }
}
// the FM-1's ports: an output and an input that answer INFO as GR!MM. Its USB name is "Arctic Scream" (before 1.0's
// rename: "Felucca", the platform's): ports named so, or FM-1, are tried first. An answer is an INFO reply, never our own INFO back (a loop port, as Bitwig's
// "to Rack" ports, sends every frame back unchanged: step 26b's page took that for the FM-1)
export async function findFM1(access, log = null) {
  const outs = [...access.outputs.values()], ins = [...access.inputs.values()];
  const fm = (p) => /arctic|felucca|fm-?1|m-?vave|gr!?mm/i.test(p.name || "") ? 0 : 1;
  outs.sort((a, b) => fm(a) - fm(b));
  ins.sort((a, b) => fm(a) - fm(b));
  for (const out of outs) {
    const got = await new Promise((res) => {
      const timer = setTimeout(() => done(null), 800);
      const done = (v) => { clearTimeout(timer); ins.forEach((i) => { i.onmidimessage = null; }); res(v); };
      ins.forEach((i) => {
        i.onmidimessage = (e) => {
          const d = e.data;
          if (d[0] !== 0xf0) return;
          if (log) log("in", d, i.name);
          if (d.length >= 20 && d[1] === HEAD[0] && d[2] === HEAD[1] && d[3] === HEAD[2] && d[4] === CMD.INFO) done(i);
        };
      });
      const f = [0xf0, ...HEAD, CMD.INFO, 0xf7];
      if (log) log("out", f, out.name);
      try { out.send(f); } catch { done(null); }
    });
    if (got) return { input: got, output: out };
  }
  return null;
}
// the virtual FM-1: grimm.wasm (the firmware's code, the browser build), its main loop run until it answers
export class VirtualFM1 {
  static async create(wasmBytes) {
    const v = new VirtualFM1();
    let ex = null, rewinding = false;
    const { instance } = await WebAssembly.instantiate(wasmBytes, {
      env: {
        web_yield() {
          if (rewinding) { ex.asyncify_stop_rewind(); rewinding = false; return; }
          const h = new Int32Array(ex.memory.buffer, v.data, 2);
          h[0] = v.data + 8;
          h[1] = v.data + v.size;
          ex.asyncify_start_unwind(v.data);
        },
        web_ticks: () => 0,
      },
    });
    v.ex = ex = instance.exports;
    if (ex._initialize) ex._initialize();
    v.data = ex.web_async_buf();
    v.size = ex.web_async_size();
    v.started = false;
    v.run = () => {
      if (v.started) { rewinding = true; ex.asyncify_start_rewind(v.data); }
      v.started = true;
      ex.web_run();
      if (ex.asyncify_get_state() === 1) ex.asyncify_stop_unwind();
    };
    ex.web_master(0);
    ex.web_want_frames(0);
    v.step(100);
    return v;
  }
  warmUp() { while (this.ex.web_now_ms() < 4000) this.step(50); }   // (as after power-on: GR!MM writes only once silent,
                                                             // ~3.5 s in)
  step(ms) {                                                 // the device's milliseconds (its sound thrown away)
    for (let i = 0; i < ms; i++) {
      this.run();
      const n = this.ex.web_avail();
      if (n) { this.ex.web_want_frames(n); this.ex.web_take(); this.ex.web_want_frames(0); }
    }
  }
  meta() {
    const ex = this.ex, p = ex.web_meta(), m = new Uint8Array(ex.memory.buffer, p);
    return JSON.parse(new TextDecoder().decode(m.subarray(0, m.indexOf(0))));
  }
  text(fn, ...args) {
    const ex = this.ex, p = ex[fn](...args), m = new Uint8Array(ex.memory.buffer, p);
    return new TextDecoder().decode(m.subarray(0, m.indexOf(0)));
  }
  async request(frame, timeout = 3000, quiet = false) {
    const ex = this.ex;
    if (this.log && !quiet) this.log("out", [0xf0, ...frame, 0xf7], "VIRTUAL FM-1");
    new Uint8Array(ex.memory.buffer, ex.web_sysex_in_buf(), frame.length).set(frame);
    ex.web_sysex_out_clear();
    ex.web_sysex_in(frame.length);
    for (let ms = 0; ms < timeout && !ex.web_sysex_out_len(); ms++) this.step(1);
    const n = ex.web_sysex_out_len();
    if (!n) throw new EditorError("no answer from the virtual FM-1", 97);
    const d = Array.from(new Uint8Array(ex.memory.buffer, ex.web_sysex_out_buf(), n));
    ex.web_sysex_out_clear();
    if (this.log && !quiet) this.log("in", d, "VIRTUAL FM-1");
    if (d[4] !== frame[3]) throw new EditorError("a wrong answer from the virtual FM-1", 96);
    return d.slice(5, d.length - 1);
  }
  close() {}
}

// live values, as SLOOP's editor sends its SETs (isod89/sloop-fm1 web/editor.html, userSet): per key one in flight and
// the latest waiting, so a drag never builds a backlog; gap ms between two. send(key) -> a Promise
export class LiveSender {
  constructor(send, gap = 15) { this.send = send, this.gap = gap, this.want = new Set(), this.running = null, this.onError = null; }
  set(key) {
    this.want.add(key);
    if (!this.running) this.running = this.run();
    return this.running;
  }
  async run() {
    while (this.want.size) {
      const key = this.want.values().next().value;
      this.want.delete(key);
      try { await this.send(key); } catch (e) { if (this.onError) this.onError(e, key); }
      if (this.gap) await new Promise((r) => setTimeout(r, this.gap));
    }
    this.running = null;
  }
  get idle() { return !this.running; }
}

// ------------------------------------------------------------------ sounds (64 B records)
export const SND = 64, SND7 = 74;                          // a sound; in pack7
export function emptyRecord() { return new Uint8Array(SND).fill(0xff); }
export const isEmpty = (r) => r.every((x) => x === 0xff);
export function recordOk(r) {
  const dv = new DataView(r.buffer, r.byteOffset, SND);
  return r[0] === 0x54 && r[1] < 9 && r[3] < 8 && dv.getUint32(60, true) === crc32(r.subarray(0, 60));
}
export function parseSound(r, meta) {                        // a record -> a sound (FM: its patch, or the PATCH's as built)
  const fm = r[3] === 1 && r[18] & 0x80 ? r.slice(18, 50) : meta.fmPatches[Math.min(r[4], meta.fmPatches.length - 1)].fm;
  const name = String.fromCharCode(...r.slice(50, 60).filter((c) => c >= 32 && c < 127)).trim();
  return { cat: r[1], num: r[2], eng: r[3], k: [...r.slice(4, 8)], env: [...r.slice(8, 12)], latch: r[12],
           chordOn: r[13] >> 7, voicing: (r[13] >> 4) & 7, chord: r[13] & 15, lfo: [...r.slice(14, 18)],
           fm: { alg: fm[0] & 31, fb: fm[1] & 7, ops: [0, 1, 2, 3, 4, 5].map((i) => [...fm.slice(2 + i * 5, 7 + i * 5)]) }, name };
}
export function buildRecord(s) {
  const r = new Uint8Array(SND);
  r[0] = 0x54, r[1] = s.cat, r[2] = Math.max(1, Math.min(99, s.num || 1)), r[3] = s.eng;
  r.set(s.k, 4), r.set(s.env, 8), r[12] = s.latch ? 1 : 0;
  r[13] = (s.chordOn ? 0x80 : 0) | (s.voicing & 7) << 4 | (s.chord & 15);
  r.set(s.lfo, 14);
  if (s.eng === 1) {                                         // FM: its own patch (bit 7: there is one)
    r[18] = 0x80 | (s.fm.alg & 31), r[19] = s.fm.fb & 7;
    s.fm.ops.forEach((o, i) => r.set(o, 20 + i * 5));
  }
  const nm = (s.name || "").toUpperCase().replace(/[^\x20-\x7e]/g, "").slice(0, 10);
  for (let i = 0; i < nm.length; i++) r[50 + i] = nm.charCodeAt(i);
  new DataView(r.buffer).setUint32(60, crc32(r.subarray(0, 60)), true);
  return r;
}
export function soundTitle(r, meta) {                        // as the FM-1 names it ("PAD 03"), the editor's name first
  if (isEmpty(r)) return "";
  const s = parseSound(r, meta);
  return s.name || `${meta.cats[s.cat]} ${String(s.num).padStart(2, "0")}`;
}

// ------------------------------------------------------------------ sequences (store.c's pk_seq: SEQ_PK B)
export const SEQ_PK = 260, SEQ_N = 64;
export const SEQ_DIV = ["1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"], SEQ_DIV_T = [96, 64, 48, 32, 24, 16, 12, 8];
export const SEQ_MODE = ["FORWARD", "BACKWARD", "PINGPONG", "RANDOM"];
export function parseSeq(b) {                                // -> { steps, div, swing, mode, st: [{ note, chance, ratchet, len, chord }] }
  const st = [];
  for (let k = 0; k < SEQ_N; k++) {
    const i = 4 + k * 4, v = b[i] | b[i + 1] << 8 | b[i + 2] << 16;
    st.push({ note: v & 127, chance: Math.min(100, v >> 7 & 127), ratchet: (v >> 14 & 3) + 1, len: (v >> 16 & 63) + 1,
              chord: b[i + 3] & 0x80 ? b[i + 3] : 0 });
  }
  return { steps: b[0], div: b[1], swing: b[2], mode: b[3], st };
}
export function buildSeq(q) {
  const b = new Uint8Array(SEQ_PK), c = (v, lo, hi) => Math.max(lo, Math.min(hi, v | 0));
  b[0] = c(q.steps, 1, SEQ_N), b[1] = c(q.div, 0, 7), b[2] = c(q.swing, 0, 127), b[3] = c(q.mode, 0, 3);
  q.st.forEach((s, k) => {
    const v = c(s.note, 0, 127) | c(s.chance, 0, 100) << 7 | (c(s.ratchet, 1, 4) - 1) << 14 | (c(s.len, 1, SEQ_N) - 1) << 16;
    b.set([v & 255, v >> 8 & 255, v >> 16 & 255, s.chord & 0x80 ? s.chord : 0], 4 + k * 4);
  });
  return b;
}
export const emptyStep = () => ({ note: 0, chance: 100, ratchet: 1, len: 1, chord: 0 });
export function seqToMidi(q, bpm, name = "ARCTIC SCREAM") {   // a Standard MIDI File (type 0, 96 ticks a quarter: the FM-1's)
  const ev = [], tps = SEQ_DIV_T[q.div] || 24;
  for (let k = 0; k < q.steps; k++) {
    const s = q.st[k];
    if (!s.note) continue;
    const t0 = k * tps, hit = Math.floor(tps / s.ratchet);
    for (let r = 0; r < s.ratchet; r++) {
      const on = t0 + r * hit, off = s.ratchet > 1 ? on + Math.max(1, hit >> 1) : t0 + s.len * tps - 1;
      ev.push([on, 1, s.note], [off, 0, s.note]);
    }
  }
  ev.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const vlq = (v) => { const o = [v & 127]; while ((v >>= 7)) o.unshift(v & 127 | 128); return o; };
  const us = Math.round(60000000 / bpm), trk = [0, 0xff, 0x51, 3, us >> 16 & 255, us >> 8 & 255, us & 255];
  const nm = Array.from(name.slice(0, 32), (c) => c.charCodeAt(0) & 127);
  trk.push(0, 0xff, 0x03, nm.length, ...nm);
  let at = 0;
  for (const [t, on, n] of ev) { trk.push(...vlq(t - at), on ? 0x90 : 0x80, n, on ? 100 : 0); at = t; }
  trk.push(...vlq(Math.max(0, q.steps * tps - at)), 0xff, 0x2f, 0);
  const u32 = (v) => [v >>> 24, v >> 16 & 255, v >> 8 & 255, v & 255];
  return Uint8Array.from([0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0, 96, 0x4d, 0x54, 0x72, 0x6b, ...u32(trk.length), ...trk]);
}
export function midiToSeq(buf, q) {                          // a MIDI file's notes onto q's grid (its DIVISION; the first
  const b = new Uint8Array(buf), u16 = (i) => b[i] << 8 | b[i + 1], u32 = (i) => (b[i] << 24 | b[i + 1] << 16 | b[i + 2] << 8 | b[i + 3]) >>> 0;
  if (String.fromCharCode(...b.slice(0, 4)) !== "MThd") throw new Error("NOT A MIDI FILE");   // track with notes, one a step)
  const ppq = u16(12);
  if (ppq & 0x8000) throw new Error("SMPTE TIME IS NOT SUPPORTED");
  let i = 8 + u32(4), notes = [];
  while (i + 8 <= b.length && !notes.length) {
    const end = i + 8 + u32(i + 4);
    let p = i + 8, t = 0, run = 0;
    const open = {};
    const vlq = () => { let v = 0, c; do { c = b[p++]; v = v << 7 | c & 127; } while (c & 128); return v; };
    while (p < end) {
      t += vlq();
      let st = b[p];
      if (st & 0x80) p++, run = st; else st = run;
      const hi = st & 0xf0;
      if (st === 0xff) { p++; const n = vlq(); p += n; continue; }
      if (st === 0xf0 || st === 0xf7) { const n = vlq(); p += n; continue; }
      if (hi === 0x90 || hi === 0x80) {
        const n = b[p], v = b[p + 1];
        p += 2;
        if (hi === 0x90 && v) open[n] = t;
        else if (open[n] !== undefined) { notes.push([open[n], t, n]); delete open[n]; }
      } else p += hi === 0xc0 || hi === 0xd0 ? 1 : 2;
    }
    i = end;
  }
  if (!notes.length) throw new Error("NO NOTES IN THE FILE");
  const tps = (SEQ_DIV_T[q.div] || 24) * ppq / 96, out = { ...q, st: Array.from({ length: SEQ_N }, emptyStep) };
  let last = 0;
  notes.sort((a, b) => a[0] - b[0] || b[2] - a[2]);
  for (const [on, off, n] of notes) {
    const k = Math.round(on / tps);
    if (k >= SEQ_N || out.st[k].note || Math.abs(on / tps - k) > 0.2) continue;   // (a chord: its highest note; off the
                                                             // grid, a ratchet's hits: not a step of their own)
    out.st[k] = { note: n, chance: 100, ratchet: 1, len: Math.max(1, Math.min(SEQ_N, Math.round((off - on) / tps))), chord: 0 };
    last = Math.max(last, k + 1);
  }
  out.steps = Math.max(1, Math.min(SEQ_N, Math.ceil(last / 4) * 4 || last));
  return out;
}

// ------------------------------------------------------------------ backup (the same file as tools/fm1_editor_test.py)
export function backupObjects(info) {
  return [[OBJ.SET, info.setSize, "settings"], ...[0, 1, 2, 3].map((k) => [OBJ.SLOT + k, info.slotSize, `slot ${"ABCD"[k]}`]),
          ...Array.from({ length: info.sounds }, (_, k) => [OBJ.SND + k, info.soundSize, `sound ${k + 1}`])];
}
const b64 = (b) => { let s = ""; b.forEach((x) => { s += String.fromCharCode(x); }); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
export async function makeBackup(client, info, progress) {
  const list = backupObjects(info), objects = [];
  for (let i = 0; i < list.length; i++) {
    const [id, size, what] = list[i], d = await client.get(id, size);
    objects.push({ id, len: size, crc: crc32(d), data: b64(d) });
    if (progress) progress(i + 1, list.length, what);
  }
  return { format: "grimm-backup", version: 1, proto: info.proto, fmt: info.fmt, firmware: info.firmware,
           date: new Date().toISOString().replace("T", " ").slice(0, 19), objects };
}
export function checkBackup(b, info) {                       // -> [{id, data, what}] or throws (nothing written)
  if (!b || b.format !== "grimm-backup" || b.version !== 1) throw new EditorError("not a GR!MM backup file", 90);
  if (b.fmt !== info.fmt) throw new EditorError(`the backup's format is ${b.fmt}, the FM-1's is ${info.fmt}`, 90);
  const sizes = new Map(backupObjects(info).map(([id, size, what]) => [id, [size, what]]));
  return b.objects.map((o) => {
    const d = unb64(o.data), s = sizes.get(o.id);
    if (!s || d.length !== s[0] || crc32(d) !== o.crc) throw new EditorError(`object ${o.id} is damaged or not this firmware's`, 90);
    return { id: o.id, data: d, what: s[1] };
  });
}
export async function restoreBackup(client, objs, info, progress) {
  for (let i = 0; i < objs.length; i++) {
    await client.put(objs[i].id, objs[i].data, info.fmt);
    if (progress) progress(i + 1, objs.length, objs[i].what);
  }
}

// ------------------------------------------------------------------ a WAV -> a GR!MM sample slot
// tools/sampleio.py read_any_wav, resample, to_int16, ima_encode, user_slot, note_from_name, and the cut and name of
// tools/fm1_sample_upload.py, with the order of their float operations kept: the same bytes
export const SAMPLE = { RATE: 22050, DATA_OFF: 512, HDR_LEN: 480, MAX_DATA: 72 * 1024 - 512, MAGIC: 0x504d5347 };
SAMPLE.MAX_N = SAMPLE.MAX_DATA * 2;
const IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
  97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
  6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767];
const IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8];
const pyRound = (x) => {                                     // Python's round(): halves to even
  const f = Math.floor(x), d = x - f;
  return d > 0.5 ? f + 1 : d < 0.5 ? f : f % 2 === 0 ? f : f + 1;
};
export function readAnyWav(buf) {                            // PCM 8/16/24/32 or float, mixed to mono -> {sr, x}
  const d = new Uint8Array(buf), dv = new DataView(d.buffer, d.byteOffset, d.byteLength), str = (o) => String.fromCharCode(...d.slice(o, o + 4));
  if (d.length < 12 || str(0) !== "RIFF" || str(8) !== "WAVE") throw new Error("not a WAV file");
  const ch = {};
  for (let i = 12; i + 8 <= d.length;) {
    const id = str(i), n = dv.getUint32(i + 4, true);
    if (!(id in ch)) ch[id] = [i + 8, n];
    i += 8 + n + (n & 1);
  }
  if (!ch["fmt "]) throw new Error("a WAV without a fmt chunk");
  const fo = ch["fmt "][0];
  let tag = dv.getUint16(fo, true);
  const nch = dv.getUint16(fo + 2, true), sr = dv.getUint32(fo + 4, true), bits = dv.getUint16(fo + 14, true);
  if (tag === 0xfffe) tag = dv.getUint16(fo + 24, true);
  const bps = bits >> 3;
  if ((tag !== 1 && tag !== 3) || !bps || !nch) throw new Error(`an unsupported WAV format (tag ${tag}, ${bits} bit)`);
  const [o, n] = ch.data || [0, 0], frame = bps * nch, nfr = Math.floor(Math.min(n, d.length - o) / frame);
  const v = new Float64Array(nfr * nch);
  for (let i = 0; i < v.length; i++) {
    const p = o + i * bps;
    if (tag === 3) v[i] = bps === 4 ? dv.getFloat32(p, true) : dv.getFloat64(p, true);
    else if (bps === 2) v[i] = dv.getInt16(p, true) / 32768;
    else if (bps === 3) v[i] = ((d[p + 2] << 24) | (d[p + 1] << 16) | (d[p] << 8)) / 2147483648;
    else if (bps === 4) v[i] = dv.getInt32(p, true) / 2147483648;
    else v[i] = (d[p] - 128) / 128;
  }
  if (nch === 1) return { sr, x: Array.from(v) };
  const x = new Array(nfr);
  for (let f = 0; f < nfr; f++) {
    if (nch === 2) { x[f] = (v[2 * f] + v[2 * f + 1]) / 2; continue; }
    let acc = 0.0;
    for (let c = 0; c < nch; c++) acc += v[f * nch + c];
    x[f] = acc / nch;
  }
  return { sr, x };
}
export function resample(x, sr, to) {                        // a moving average over the ratio, then linear
  if (sr === to) return x;
  if (sr > to) {
    const k = Math.max(1, pyRound(sr / to));
    if (k === 2) x = x.length ? [...x.slice(0, -1).map((a, i) => (a + x[i + 1]) / 2), x[x.length - 1] / 2] : [];
    else if (k > 1) {
      const y = new Array(x.length);
      for (let i = 0; i < x.length; i++) {
        let a = x[i];
        for (let j = i + 1; j < i + k && j < x.length; j++) a += x[j];
        y[i] = a / k;
      }
      x = y;
    }
  }
  const step = sr / to, out = [], last = x.length - 1;
  for (let p = 0.0; p < last; p += step) {
    const i = Math.trunc(p), f = p - i;
    out.push(x[i] * (1 - f) + x[i + 1] * f);
  }
  return out;
}
const peak = (x) => x.reduce((m, v) => Math.max(m, Math.abs(v)), 1e-9);
export function toInt16(x) {
  const pk = peak(x);
  return x.map((v) => Math.max(-32768, Math.min(32767, Math.trunc(v / pk * 30000))));
}
const IMA_VD = new Int32Array(89 * 8), IMA_NX = new Uint8Array(89 * 8);   // (sampleio.py _ima_tables)
for (let i = 0; i < 89; i++)
  for (let c = 0; c < 8; c++) {
    const step = IMA_STEP[i];
    IMA_VD[i * 8 + c] = (step >> 3) + (c & 4 ? step : 0) + (c & 2 ? step >> 1 : 0) + (c & 1 ? step >> 2 : 0);
    IMA_NX[i * 8 + c] = Math.max(0, Math.min(88, i + IMA_IDX[c]));
  }
function imaCode(pred, idx, x) {                             // the standard choice of code for x
  const step = IMA_STEP[idx];
  let diff = x - pred, code = 0;
  if (diff < 0) { code = 8; diff = -diff; }
  if (diff >= step) { code |= 4; diff -= step; }
  if (diff >= step >> 1) { code |= 2; diff -= step >> 1; }
  if (diff >= step >> 2) code |= 1;
  return code;
}
// GR!MM V2: each code the one whose sample and the next two decode closest (sampleio.py ima_encode, look: the same
// codes): the standard choice follows a steep edge too slowly, a crackle. The decoder is the same (samples.c ima_nib)
export function imaEncode(s) {
  let pred = 0, idx = 0;
  const nib = [], n = s.length;
  let P = 0, I = 0;
  const next = (p, i, c) => {                                // the decoder: into P, I
    const k = i * 8 + (c & 7);
    P = Math.max(-32768, Math.min(32767, c & 8 ? p - IMA_VD[k] : p + IMA_VD[k])), I = IMA_NX[k];
  };
  for (let i = 0; i < n; i++) {
    const x0 = s[i], x1 = i + 1 < n ? s[i + 1] : x0, x2 = i + 2 < n ? s[i + 2] : x1;
    let best = -1, bestC = 0;
    for (let c = 0; c < 16; c++) {
      next(pred, idx, c);
      const p1 = P, i1 = I, e1 = (p1 - x0) * (p1 - x0);
      if (best >= 0 && e1 >= best) continue;                 // (already no better: the errors after it only add)
      let b2 = -1;
      for (let c2 = 0; c2 < 16; c2++) {
        next(p1, i1, c2);
        const p2 = P, i2 = I;
        let e2 = e1 + (p2 - x1) * (p2 - x1);
        if (b2 >= 0 && e2 >= b2) continue;
        next(p2, i2, imaCode(p2, i2, x2));
        e2 += (P - x2) * (P - x2);
        if (b2 < 0 || e2 < b2) b2 = e2;
      }
      if (best < 0 || b2 < best) { best = b2; bestC = c; }
    }
    next(pred, idx, bestC);
    pred = P, idx = I;
    nib.push(bestC);
  }
  if (nib.length & 1) nib.push(0);
  const out = new Uint8Array(nib.length >> 1);
  for (let i = 0; i < out.length; i++) out[i] = nib[2 * i] | (nib[2 * i + 1] << 4);
  return out;
}
export function noteFromName(name, def = 60) {
  const m = /(?<![A-Za-z])([A-G])([#b]?)(-?\d)(?!\d)/.exec(name);
  if (!m) return def;
  const pc = "C D EF G A B".indexOf(m[1]) + ({ "#": 1, b: -1, "": 0 })[m[2]];
  return Math.max(0, Math.min(127, (parseInt(m[3], 10) + 1) * 12 + pc));
}
// a slot's name: 8 characters (the header's, and what the FM-1 shows); longer, its number at the end kept
// ("Sample-002" -> "SAMP-002"), else its start (tools/fm1_sample_upload.py slot_name)
export function sampleName(s) {
  s = String(s).toUpperCase().replace(/[^A-Z0-9 _\-.#!+]/g, "").trim();
  if (s.length > 8) {
    const m = /[ _\-.]?\d+$/.exec(s), tail = m && m[0].length < 8 ? m[0] : "";
    s = s.slice(0, 8 - tail.length).trimEnd() + tail;
  }
  return s || "SAMPLE";
}
export const noteName = (n) => ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12] + (Math.floor(n / 12) - 1);
export function makeSlot(buf, name, root) {                  // -> {hdr (480 B), data, secs, cut}
  let { sr, x } = readAnyWav(buf);
  x = resample(x, sr, SAMPLE.RATE);
  if (!x.length || peak(x) <= 1e-6) throw new Error("no audio in it (silent or empty)");
  const cut = x.length > SAMPLE.MAX_N;
  if (cut) {                                                 // what fits, with a 20 ms fade at the cut
    x = x.slice(0, SAMPLE.MAX_N);
    const f = Math.trunc(0.02 * SAMPLE.RATE);
    for (let i = 0; i < f; i++) x[SAMPLE.MAX_N - f + i] *= (f - i) / f;
  }
  const s = toInt16(x), data = imaEncode(s), hdr = new Uint8Array(SAMPLE.HDR_LEN), dv = new DataView(hdr.buffer);
  const nm = name.toUpperCase().replace(/[^\x20-\x7e]/g, "").slice(0, 8);
  dv.setUint32(0, SAMPLE.MAGIC, true), dv.setUint16(4, 1, true), hdr[6] = 1;
  for (let i = 0; i < nm.length; i++) hdr[8 + i] = nm.charCodeAt(i);
  dv.setUint32(16, data.length, true), dv.setUint32(20, crc32(data), true);
  const z = 32;                                              // the one zone: the whole keyboard
  dv.setUint32(z, 0, true), dv.setUint32(z + 4, s.length, true), dv.setUint32(z + 8, 0, true);
  dv.setUint32(z + 12, s.length - 1, true), dv.setUint32(z + 16, pyRound(SAMPLE.RATE / 44100 * 65536), true);
  dv.setInt16(z + 20, root * 16, true), dv.setInt16(z + 22, 0, true);
  hdr[z + 24] = 0, hdr[z + 25] = 0, hdr[z + 26] = 127, hdr[z + 27] = 0;
  return { hdr, data, secs: s.length / SAMPLE.RATE, cut };
}
