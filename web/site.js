// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 PolarPatch
// Arctic Scream p0larpatch: the demo page around the panel (panel.js): the demos A1-A5
// (web/demos.json, made by web/gen_demos.mjs) and the sequencer's state beside the panel.

import "./glitch.js";                                    // (the glitching titles)
import { Client, makeSlot, sampleName, noteFromName, noteName, SAMPLE } from "./editor_core.js";

const $ = (id) => document.getElementById(id);

// ------------------------------------------------------------------ the sources' waves
// Each wave is drawn from x 2 to 72, so three copies 70 apart make one endless wave; under the pointer it rolls left.
document.querySelectorAll("#sources .card svg").forEach((svg, i) => {
  const p = svg.querySelector("path"), g = document.createElementNS("http://www.w3.org/2000/svg", "g");
  g.setAttribute("class", "roll");
  g.style.setProperty("--spd", (0.9 + (i * 0.37) % 1.2).toFixed(2) + "s");
  for (const dx of [-70, 0, 70]) {
    const c = p.cloneNode();
    c.setAttribute("transform", `translate(${dx} 0)`);
    g.appendChild(c);
  }
  p.replaceWith(g);
});

// ------------------------------------------------------------------ the sequencer's lanes
const COL = ["var(--ice)", "var(--aurora)", "var(--violet)", "var(--plasma)"];
const lanes = [];
for (let t = 0; t < 4; t++) {
  const l = document.createElement("div");
  l.className = "lane";
  l.innerHTML = `<span>T${t + 1}</span><div class="cells" style="--c:${COL[t]}">${"<i></i>".repeat(16)}</div>`;
  $("lanes").appendChild(l);
  lanes.push([...l.querySelectorAll("i")]);
}
let lit = [-1, -1, -1, -1];
addEventListener("fm1-seq", (e) => {
  const s = e.detail, on = !!(s >> 28 & 1);
  $("r-play").textContent = on ? "PLAY" : "STOP";
  for (let t = 0; t < 4; t++) {
    const p = (s >> (7 * t) & 127) - 1, c = p < 0 ? -1 : p % 16;
    if (c === lit[t]) continue;
    if (lit[t] >= 0) lanes[t][lit[t]].classList.remove("on");
    if (c >= 0) lanes[t][c].classList.add("on");
    lit[t] = c;
  }
});

// ------------------------------------------------------------------ the demos
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
let demos = [], busy = false;
async function play(d, btn) {
  if (busy) return;
  busy = true;
  try {
    await window.fm1.start();
    document.querySelectorAll(".demo").forEach((b) => b.classList.toggle("on", b === btn));
    $("r-demo").textContent = d.name;
    $("r-tempo").textContent = d.tempo;
    for (const a of d.act) {                               // one at a time: the worklet takes a frame a block
      if (a.sx) window.fm1.sysex(a.sx);
      else if (a.panic) window.fm1.panic();
      else if (a.keys) await window.fm1.tap(a.keys);
      await wait(40);
    }
  } catch { /* (not switched on: panel.js says why) */ }
  busy = false;
}
fetch("demos.json").then((r) => r.json()).then((list) => {
  demos = list;
  for (const d of demos) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "demo";
    for (const [cls, text] of [["id", d.id], ["n", d.name], ["s", d.sub]]) {
      const s = document.createElement("span");
      s.className = cls;
      s.textContent = text;
      b.appendChild(s);
    }
    b.addEventListener("click", () => play(d, b));
    $("demos").appendChild(b);
  }
}).catch(() => { $("demos").textContent = "The demos could not be loaded."; });
$("d-stop").addEventListener("click", () => window.fm1.sysex([0x7d, 0x41, 0x53, 24, 3, 0, 0]));
$("d-off").addEventListener("click", () => window.fm1.panic());
$("hero-go").addEventListener("click", () => {
  $("play").scrollIntoView({ block: "start" });
  window.fm1.start().catch(() => {});
});

// ------------------------------------------------------------------ (2.0) the samples: the web's own, longer slots
// A WAV dropped on a slot goes into the firmware here as tools/fm1_sample_upload.py sends it (editor_core.js's Client
// over panel.js's request): cut to the slot's length as INFO gives it (the browser's ~95 s; the FM-1's 6.6 s). The
// firmware writes a slot only while nothing sounds: ALL NOTES OFF first, then asked again until the tails are gone
const client = new Client({ request: (f, timeout) => window.fm1.request(f, timeout) });
let slots = [], loading = false;
const slotRows = [];
for (let k = 0; k < 4; k++) {
  const b = document.createElement("button"), file = document.createElement("input");
  b.type = "button", b.className = "slot";
  b.innerHTML = `<span class="k">${k + 1}</span><span class="n">EMPTY</span><span class="s">DROP A WAV HERE, OR CLICK</span><span class="bar"><i></i></span>`;
  file.type = "file", file.accept = ".wav,audio/wav", file.hidden = true;
  b.addEventListener("click", () => { if (!loading) file.click(); });
  file.addEventListener("change", () => file.files[0] && load(k, file.files[0]));
  b.addEventListener("dragover", (e) => { e.preventDefault(); b.classList.add("hot"); });
  b.addEventListener("dragleave", () => b.classList.remove("hot"));
  b.addEventListener("drop", (e) => { e.preventDefault(); b.classList.remove("hot"); if (e.dataTransfer.files[0] && !loading) load(k, e.dataTransfer.files[0]); });
  $("smp-slots").append(b, file);
  slotRows.push(b);
}
const say = (k, n, s, part) => {
  const r = slotRows[k];
  if (n !== null) r.querySelector(".n").textContent = n;
  if (s !== null) r.querySelector(".s").textContent = s;
  r.querySelector(".bar i").style.width = part === undefined ? "0" : `${Math.round(part * 100)}%`;
};
function drawSlots() {
  slots.forEach((x, k) => {
    if (x.state === 1) say(k, x.name || "(NO NAME)", `${(x.n / SAMPLE.RATE).toFixed(1)} S${x.root !== null ? ` · ROOT ${noteName(x.root)}` : ""}`);
    else say(k, x.state === 2 ? "BEING CHECKED" : "EMPTY", "DROP A WAV HERE, OR CLICK");
  });
  if (slots[0] && slots[0].cap) $("smp-cap").textContent = `FOUR SLOTS OF ${Math.floor(slots[0].cap / SAMPLE.RATE)} S HERE (THE FM-1: 6.6 S)`;
}
async function load(k, f) {
  loading = true;
  slotRows.forEach((r) => r.classList.add("busy"));
  try {
    await window.fm1.start();
    say(k, f.name.toUpperCase(), "READING");
    if (!slots.length) slots = await client.smpInfo();
    const stem = f.name.replace(/\.[^.]*$/, ""), cap = slots[k] && slots[k].cap ? slots[k].cap : SAMPLE.MAX_N;
    const sl = makeSlot(await f.arrayBuffer(), sampleName(stem), noteFromName(stem, 60), cap);
    for (let t0 = Date.now(); ; ) {             // (the firmware writes only in silence: notes off, the tails let go)
      try {
        say(k, null, `LOADING ${sl.secs.toFixed(1)} S${sl.cut ? " (CUT TO FIT)" : ""}`, 0);
        slots = await client.smpUpload(k, sl.hdr, sl.data, (i, n) => say(k, null, null, i / n));
        break;
      } catch (e) {
        if (e.rc !== 3 || Date.now() - t0 > 30000) throw e;
        window.fm1.panic();
        say(k, null, "WAITING FOR SILENCE (ALL NOTES OFF)");
        await wait(1000);
      }
    }
    drawSlots();
  } catch (e) {
    say(k, "NOT LOADED", String((e && e.message) || e).toUpperCase());
  }
  loading = false;
  slotRows.forEach((r) => r.classList.remove("busy"));
}
