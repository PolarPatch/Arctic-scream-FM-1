// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 PolarPatch
// Arctic Scream p0larpatch: the demo page around the panel (panel.js): the demos A1-A4
// (web/demos.json, made by web/gen_demos.mjs) and the sequencer's state beside the panel.

import "./glitch.js";                                    // (the glitching titles)

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
