// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 GR!MM
// After Felucca's web/emu/worklet.js (hugelton/Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
// Instruments) and X0X's (charlesvestal/fm1-x0x), both GPL-3.0.
// Arctic Scream in an AudioWorklet: grimm.wasm is firmware/src/grimm.c on the fake FM-1 (web/fm1_web.c). Each render quantum
// asks it for 128 frames, which runs the device's clock forward; about 30 times a second the screen, the lights and
// the saved settings go to the page. The app's main loop never returns: it pauses in web_yield (below; Asyncify, see
// web/fm1_web.c) once the frames are there, and the next quantum resumes it where it stopped.

class Grimm extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.started = false;
    this.rewinding = false;
    this.sentDraws = -1;
    this.sentLeds = "";
    this.sentWrites = 0;
    this.lastFrame = -1;
    this.sx = [];                                            // the editor's SysEx frames (TRACK), one a quantum
    this.asks = [];                                          // (1.5.3) frames whose replies go back to the page
    this.asked = false;                                      // .. one of them in the firmware now
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const now = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
        const env = { web_yield: () => this.yieldNow(), web_ticks: () => (now() * 24000) >>> 0 };   // fm1_ticks: 24 MHz
        const { instance } = await WebAssembly.instantiate(m.wasm, { env });
        const ex = (this.ex = instance.exports);
        if (ex._initialize) ex._initialize();
        this.data = ex.web_async_buf();
        this.dataSize = ex.web_async_size();
        if (m.settings) {                                    // what the page kept, before the app boots
          const b = new Uint8Array(m.settings).subarray(0, 256);
          new Uint8Array(ex.memory.buffer, ex.web_settings(), b.length).set(b);
          ex.web_settings_set_len(b.length);
        }
        ex.web_master(m.master ?? 700);
        this.port.postMessage({ type: "ready" });
      } catch (err) {
        this.ex = null;
        this.port.postMessage({ type: "error", message: String((err && err.message) || err) });
      }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    if (m.type === "keys") ex.web_keys(m.mask >>> 0);
    else if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
    else if (m.type === "enc") ex.web_enc(m.enc, m.n | 0);
    else if (m.type === "master") ex.web_master(m.value | 0);
    else if (m.type === "midi") ex.web_midi(m.data[0], m.data[1], m.data[2]);
    else if (m.type === "request") {                        // (1.5.3) a frame whose reply the page waits for (SAMPLES:
      this.asks.push(m.data);                               // one at a time, panel.js request; sent once the queue is clear;
      this.asked = false;                                   // the one before answered, or given up on)
    }
    else if (m.type === "sysex") {                          // the editor: a frame without F0 / F7 (its reply dropped);
      const f = m.data, set = (q) => (q[3] === 23 || q[3] === 24) && q[4] === 1;   // (TRACK's and SEQ's SET)
      const k = set(f) ? this.sx.findIndex((q) => set(q) && q[3] === f[3] && q[5] === f[5]) : -1;
      if (k >= 0) this.sx[k] = f; else this.sx.push(f);      // (a track's sound or sequence waiting: the newest in its place)
    }
    else if (m.type === "panic" && ex.web_panic) ex.web_panic();
  }

  // Asyncify (https://kripken.github.io/blog/wasm/2019/07/16/asyncify.html): the first call of web_yield unwinds the
  // app back to run(); the next run() rewinds into it, and web_yield, called again, ends the rewind and returns
  yieldNow() {
    const ex = this.ex;
    if (this.rewinding) {
      ex.asyncify_stop_rewind();
      this.rewinding = false;
      return;
    }
    const h = new Int32Array(ex.memory.buffer, this.data, 2);
    h[0] = this.data + 8;
    h[1] = this.data + this.dataSize;
    ex.asyncify_start_unwind(this.data);
  }

  run() {
    const ex = this.ex;
    if (this.started) {
      this.rewinding = true;
      ex.asyncify_start_rewind(this.data);
    }
    this.started = true;
    ex.web_run();
    if (ex.asyncify_get_state() === 1) ex.asyncify_stop_unwind();
  }

  publish() {
    const ex = this.ex, msg = { type: "frame" }, transfer = [];
    const draws = ex.web_screen_draws();
    if (draws !== this.sentDraws) {
      this.sentDraws = draws;
      msg.fb = new Uint16Array(ex.memory.buffer, ex.web_fb(), 240 * 240).slice();
      transfer.push(msg.fb.buffer);
    }
    const leds = [ex.web_lit_buttons() >>> 0, ex.web_lit_keys() >>> 0];
    if (leds.join() !== this.sentLeds) {
      this.sentLeds = leds.join();
      msg.leds = leds;
    }
    const writes = ex.web_settings_writes();
    if (writes !== this.sentWrites) {
      this.sentWrites = writes;
      const n = ex.web_settings_len();
      if (n >= 0) msg.settings = new Uint8Array(ex.memory.buffer, ex.web_settings(), n).slice();
    }
    const sq = ex.web_seq_state ? ex.web_seq_state() >>> 0 : 0;   // (the sequencer: the editor's playhead)
    if (sq !== this.sentSeq) this.sentSeq = msg.seq = sq;
    if (msg.fb || msg.leds || msg.settings || msg.seq !== undefined) this.port.postMessage(msg, transfer);
  }

  pull(n) {                                                  // n frames of the FM-1's 44.1 kHz (at most 1024)
    const ex = this.ex;
    ex.web_want_frames(n);
    if (!ex.web_sysex_busy() && (this.sx.length || (this.asks.length && !this.asked))) {
      const ask = !this.sx.length, f = ask ? this.asks.shift() : this.sx.shift();
      new Uint8Array(ex.memory.buffer, ex.web_sysex_in_buf(), f.length).set(f);
      ex.web_sysex_in(f.length);
      ex.web_sysex_out_clear();
      this.asked = ask;
    }
    if (!ex.web_ready()) this.run();
    if (this.asked && ex.web_sysex_out_len()) {             // (its reply, F0 .. F7)
      this.asked = false;
      this.port.postMessage({ type: "reply", data: new Uint8Array(ex.memory.buffer, ex.web_sysex_out_buf(), ex.web_sysex_out_len()).slice() });
    }
    if (!this.asked) ex.web_sysex_out_clear();
    ex.web_take();
    return [new Float32Array(ex.memory.buffer, ex.web_out_l(), n), new Float32Array(ex.memory.buffer, ex.web_out_r(), n)];
  }

  // (1.5.2) a browser that will not run at 44.1 kHz (48 kHz, 96 kHz): the FM-1's frames resampled to its rate, a cubic
  // (Catmull-Rom) between them, so the pitch and the tempo stay the FM-1's. After ECHOFIELD's emulator playing in tune at
  // 48 kHz (Philip-Bos/echofield-fm1, MIT: the idea), written anew
  resample(out, n) {
    const step = 44100 / sampleRate;
    if (!this.rs) this.rs = { l: new Float32Array(4096), r: new Float32Array(4096), len: 2, pos: 1 };
    const rs = this.rs, need = Math.floor(rs.pos + (n - 1) * step) + 3 - rs.len;
    if (need > 0) {
      const [l, r] = this.pull(Math.min(need, 1024));
      rs.l.set(l, rs.len), rs.r.set(r, rs.len);
      rs.len += l.length;
    }
    const L = out[0], R = out[1], cr = (a, b, c, d, t) => b + 0.5 * t * (c - a + t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
    for (let k = 0; k < n; k++, rs.pos += step) {
      const i = Math.floor(rs.pos), t = rs.pos - i;
      L[k] = cr(rs.l[i - 1], rs.l[i], rs.l[i + 1], rs.l[i + 2], t);
      if (R) R[k] = cr(rs.r[i - 1], rs.r[i], rs.r[i + 1], rs.r[i + 2], t);
    }
    const drop = Math.floor(rs.pos) - 1;                     // (keep the frame before the next one)
    if (drop > 0) {
      rs.l.copyWithin(0, drop, rs.len), rs.r.copyWithin(0, drop, rs.len);
      rs.len -= drop, rs.pos -= drop;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    const out = outputs[0], n = out[0].length;
    if (sampleRate !== 44100) this.resample(out, n);
    else {
      const [l, r] = this.pull(n);
      out[0].set(l);
      if (out[1]) out[1].set(r);
    }
    if (currentTime - this.lastFrame >= 1 / 30) {
      this.lastFrame = currentTime;
      this.publish();
    }
    return true;
  }
}

registerProcessor("grimm", Grimm);
