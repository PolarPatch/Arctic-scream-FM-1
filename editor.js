// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 GR!MM
// Arctic Scream p0larpatch changes (the look, SOUND TOOLS, the sequencer page): Copyright (C) 2026 PolarPatch
// The editor page (web/editor.html). As SLOOP's editor (isod89/sloop-fm1, web/, GPL-3.0) works on the sound that
// plays: the sound page shows the four tracks' sounds (T1..T4) and every move of a control reaches the track at once,
// on the FM-1 (editor.c's TRACK, live, no flash) and in the firmware's own engine here (grimm.wasm in an AudioWorklet,
// worklet.js: its screen and its keys). STORE IN SLOT saves it into a user slot (flash: only by that button, the FM-1
// stopped and silent). SOUND TOOLS: undo, redo, compare with the sound as loaded, A / B and a morph between them,
// randomize; all here, the result sent as any edit. The sequencer page: the four tracks' sequences, live (editor.c's
// SEQ, protocol 3), as a grid, a piano roll or a list, MIDI files in and out. The library lists the user's and the
// factory sounds (LOAD TO T1..T4), samples the sample slots, backup everything. The protocol, the records, the WAV
// conversion: editor_core.js. Without an FM-1 the same, with the "virtual FM-1" (grimm.wasm here) in its place.
import { Client, MidiTransport, VirtualFM1, LiveSender, findFM1, HEAD, CMD, OBJ, PROTOS, EditorError, emptyRecord, isEmpty,
         recordOk, parseSound, buildRecord, soundTitle, makeBackup, checkBackup, restoreBackup, makeSlot, noteFromName,
         sampleName, noteName, pack7, hex, SEQ_N, SEQ_DIV, SEQ_MODE, parseSeq, buildSeq, emptyStep, seqToMidi,
         midiToSeq } from "./editor_core.js";
const RISE = ["OFF", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "32 BARS"];   // DRONE's GOHCCAT

const $ = (id) => document.getElementById(id);
function el(tag, attrs, parent, text) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (k === "cls") e.className = v;
    else if (k === "style") e.style.cssText = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v);
  }
  if (text !== undefined) e.textContent = text;
  if (parent) parent.appendChild(e);
  return e;
}
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const KC = ["var(--peri)", "var(--teal)", "var(--amber)", "var(--pink)"];     // the four knobs' colours (the FM-1's own)
const EC = ["var(--ice)", "var(--aurora)", "var(--amber)", "var(--plasma)"];  // ENV's
const NAME = "ARCTIC SCREAM";
const fwName = (v) => `p0larpatch v${v}`;                   // (the firmware as INFO names it: its version)

let wasm = null, vf = null, vfc = null, meta = null;        // grimm.wasm; its instance here (texts, the virtual FM-1)
let conn = null;                                             // { client, info, kind: "fm1" | "virtual", transport, live, seq }
let busy = false;
const user = Array.from({ length: 64 }, () => emptyRecord()), dirty = new Array(64).fill(false);
let pick = { fact: true, i: 0 }, readAt = "";                // the sound chosen in the library
const tracks = [null, null, null, null], loaded = ["", "", "", ""], from = ["", "", "", ""];   // T1..T4: the sound,
let selT = 0, touchedAt = 0;                                 // as loaded (REVERT), where it came from; the last edit
const label = ["", "", "", ""], labelEng = [0, 0, 0, 0];   // a track's sound's name as the FM-1's HOME shows it (the
                                                             // factory or user sound loaded, the editor's), for its source

// ------------------------------------------------------------------ messages, progress, busy, the MIDI log
function say(text, kind = "") { const m = $("msg"); m.textContent = text; m.className = kind; }
function progress(done, total) {
  const p = $("prog");
  p.style.visibility = total ? "visible" : "hidden";
  p.firstChild.style.width = total ? `${Math.round(done / total * 100)}%` : "0";
}
async function job(what, fn) {                               // one write or read at a time, its result said
  if (busy) { say("WAIT: SOMETHING IS STILL RUNNING", "err"); return; }
  busy = true;
  refreshButtons();
  try {
    const r = await fn();
    if (r !== false) say(typeof r === "string" ? r : `${what}: DONE`, "ok");
  } catch (e) {
    say(`${what}: ${e instanceof EditorError ? e.message : (e && e.message) || e}`.toUpperCase(), "err");
  } finally {
    busy = false;
    progress(0, 0);
    refreshButtons();
  }
}
const errText = (e) => (e instanceof EditorError ? e.message : (e && e.message) || String(e)).toUpperCase();
function download(name, data, type = "application/json") {
  const a = el("a", { href: URL.createObjectURL(new Blob([data], { type })), download: name });
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}
const stamp = () => new Date().toISOString().replace(/[-:]/g, "").replace("T", "-").slice(0, 15);
const logLines = [];
let logDrawn = false;
function midiLog(dir, bytes, port) {                         // every SysEx frame sent and received (the live ones not)
  const t = new Date().toTimeString().slice(0, 8);
  logLines.push(`${t} ${dir === "out" ? "->" : "<-"} ${port}: ${hex(bytes)}`);
  if (logLines.length > 400) logLines.splice(0, logLines.length - 400);
  if (!logDrawn && !$("log").hidden) { logDrawn = true; requestAnimationFrame(drawLog); }
}
function drawLog() { logDrawn = false; const l = $("log"); l.textContent = logLines.join("\n") || "(NOTHING YET)"; l.scrollTop = l.scrollHeight; }
$("b-log").onclick = () => { const l = $("log"); l.hidden = !l.hidden; $("b-log").classList.toggle("on", !l.hidden); if (!l.hidden) drawLog(); };
$("b-logclear").onclick = () => { logLines.length = 0; drawLog(); };

// ------------------------------------------------------------------ sections
const TITLES = { editor: "sound", seq: "sequencer", sounds: "library", samples: "samples", backup: "backup", connect: "connection" };
let page = "connect";
function show(s) {
  if (!TITLES[s]) s = "connect";
  page = s;
  document.querySelectorAll("nav a").forEach((a) => a.classList.toggle("on", a.dataset.s === s));
  document.querySelectorAll("section").forEach((x) => x.classList.toggle("on", x.id === "s-" + s));
  $("title").textContent = TITLES[s];
  if (s === "samples" && conn) sampleInfo();
  if (s === "seq") { drawSeqTracks(); drawSeq(); }
  if (s === "editor") { drawTracks(); drawEditor(); }
}
document.querySelectorAll("nav a").forEach((a) => a.addEventListener("click", () => { show(a.dataset.s); history.replaceState(null, "", "#" + a.dataset.s); }));

// ------------------------------------------------------------------ the engine: the firmware in an AudioWorklet (sound, screen)
let ac = null, node = null, engReady = null;
const scr = $("screen").getContext("2d"), img = scr.createImageData(240, 240);
function screenText(lines) {
  scr.fillStyle = "#000", scr.fillRect(0, 0, 240, 240);
  scr.fillStyle = "#22d3ee", scr.font = '8px "Press Start 2P", monospace', scr.textAlign = "center";
  lines.forEach((l, i) => scr.fillText(l, 120, 108 + i * 14));
}
function drawScreen(fb) {                                    // RGB565 (panel.js)
  const d = img.data;
  for (let i = 0; i < 240 * 240; i++) {
    const p = fb[i];
    d[i * 4] = ((p >> 11) & 31) * 255 / 31, d[i * 4 + 1] = ((p >> 5) & 63) * 255 / 63, d[i * 4 + 2] = (p & 31) * 255 / 31;
    d[i * 4 + 3] = 255;
  }
  scr.putImageData(img, 0, 0);
}
function engSend(frame) { if (node) node.port.postMessage({ type: "sysex", data: frame }); }   // (no F0 / F7)
function startEngine() {                                     // at once; the browser lets it sound after a click or a key
  if (engReady) { if (ac && ac.state !== "running") ac.resume(); return engReady; }
  engReady = (async () => {
    ac = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" });
    await ac.audioWorklet.addModule("worklet.js");
    node = new AudioWorkletNode(ac, "grimm", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
    node.connect(ac.destination);
    await new Promise((res, rej) => {
      node.port.onmessage = (e) => {
        const m = e.data;
        if (m.type === "ready") res();
        else if (m.type === "error") rej(new Error(m.message));
        else if (m.type === "frame") {
          if (m.fb) drawScreen(m.fb);
          if (m.seq !== undefined) engSeqState(m.seq);
        }
      };
      node.port.postMessage({ type: "load", wasm: wasm.slice(0), master: 760 });
    });
    for (let t = 0; t < 4; t++) engTrack(t), engSeq(t);      // (the page's four sounds and sequences, the page's track)
    engSend([...HEAD, CMD.TRACK, 2, selT]);
    engSend([...HEAD, CMD.SEQ, 2, 0, bpm & 0x7f, bpm >> 7]);
    await ac.resume();
  })().catch((e) => { screenText(["NO SOUND HERE:", String(e.message || e).slice(0, 26).toUpperCase()]); });
  return engReady;
}
addEventListener("pointerdown", startEngine, true);
addEventListener("keydown", startEngine, true);

// ------------------------------------------------------------------ connect
function setConn(c) {
  if (conn && conn.transport.close) conn.transport.close();
  conn = c;
  const s = $("conn");
  s.textContent = !c ? "NOT CONNECTED" : c.kind === "virtual" ? `VIRTUAL FM-1 · ${fwName(c.info.firmware)}` : `FM-1 · ${fwName(c.info.firmware)}`;
  s.classList.toggle("on", !!c);
  const t = $("info");
  t.textContent = "";
  if (c) {
    const i = c.info;
    [["CONNECTED TO", c.kind === "virtual" ? "THE VIRTUAL FM-1 (THIS PAGE)" : `THE FM-1 OVER USB (${c.transport.output.name})`],
     ["FIRMWARE", `Arctic Scream ${fwName(i.firmware)}`], ["EDITOR PROTOCOL", i.proto + (c.live ? " (LIVE EDITING" + (c.seq ? ", SEQUENCES)" : ")") : " (NO LIVE EDITING)")],
     ["USER SOUND SLOTS", i.sounds], ["FACTORY SOUNDS", i.factory], ["SAMPLE SLOTS IN USE", `${i.samplesUsed} OF ${i.sampleSlots}`]]
      .forEach(([k, v]) => { const r = el("tr", {}, t); el("td", {}, r, k); el("td", {}, r, String(v)); });
  }
  drawTracks();
  refreshButtons();
}
async function connectFM1() {
  if (!navigator.requestMIDIAccess) { say("THIS BROWSER HAS NO WEB MIDI: USE CHROME OR EDGE", "err"); return; }
  await job("CONNECT", async () => {
    say("ASKING FOR MIDI ACCESS…");
    const access = await navigator.requestMIDIAccess({ sysex: true });
    say("LOOKING FOR ARCTIC SCREAM ON THE MIDI PORTS…");
    const p = await findFM1(access, midiLog);
    if (!p) throw new EditorError("NO FM-1 WITH ARCTIC SCREAM FOUND (USB DATA CABLE? ANOTHER APP ON ITS MIDI PORT?) SEE THE MIDI LOG", 0);
    const transport = new MidiTransport(p.input, p.output, midiLog), client = new Client(transport), info = await client.info();
    if (!PROTOS.includes(info.proto)) throw new EditorError(`THE FM-1 SPEAKS EDITOR PROTOCOL ${info.proto}; THIS PAGE KNOWS ${PROTOS.join(", ")}`, 0);
    setConn({ client, info, kind: "fm1", transport, live: info.proto >= 2, seq: info.proto >= 3 });
    return afterConnect(true, `ON ${p.output.name}`);
  });
}
async function tryVirtual() {
  await job("CONNECT", async () => {
    say("STARTING THE VIRTUAL FM-1…");
    await new Promise((r) => setTimeout(r, 30));
    vf.warmUp();
    const info = await vfc.info();
    setConn({ client: vfc, info, kind: "virtual", transport: vf, live: true, seq: info.proto >= 3 });
    return afterConnect(false, "(THE VIRTUAL FM-1: THE FIRMWARE'S OWN CODE IN THIS PAGE, NOTHING IS KEPT)");
  });
}
// connected: the FM-1's four tracks and sequences into the page (the virtual one takes the page's), then its user slots
async function afterConnect(read, where) {
  if (conn.live && read) {
    say("READING THE FOUR TRACKS…");
    let sel = 0;
    for (let t = 0; t < 4; t++) {
      const r = await conn.client.track(t);
      tracks[t] = null;                                      // (another sound: not the page's name for it)
      sel = r.sel, tracks[t] = fromDevice(r.rec, t), loaded[t] = JSON.stringify(tracks[t]), from[t] = "ON THE FM-1";
      label[t] = r.name, labelEng[t] = tracks[t].eng;
      resetHistory(t);
      engTrack(t);
    }
    selT = sel, engSend([...HEAD, CMD.TRACK, 2, selT]);
    if (conn.seq) await readSeqs();
  } else if (conn.live) {
    for (let t = 0; t < 4; t++) await conn.client.trackSet(t, buildRecord(tracks[t]), homeName(t));
    await conn.client.select(selT);
    if (conn.seq) { for (let t = 0; t < 4; t++) await conn.client.seqSet(t, buildSeq(seqs[t])); await conn.client.tempo(bpm); }
  }
  await readSlots();
  drawTracks(), drawEditor(), drawSlots(), drawSeqTracks(), drawSeq();
  if (page === "connect") { show("editor"); history.replaceState(null, "", "#editor"); }   // (else: stay where you are)
  return `CONNECTED ${where}: ${fwName(conn.info.firmware)}` + (conn.live ? `, TRACK ${selT + 1} IN THE EDITOR (LIVE)` :
    ": ITS FIRMWARE HAS NO LIVE EDITING, SO THE SLIDERS PLAY ONLY HERE; SOUNDS, SAMPLES, BACKUP WORK") +
    (conn.live && !conn.seq ? " (NO SEQUENCES: AN OLDER FIRMWARE)" : "");
}
$("b-connect").onclick = $("b-connect2").onclick = connectFM1;
$("b-try").onclick = $("b-try2").onclick = tryVirtual;
$("b-disconnect").onclick = () => { setConn(null); say("DISCONNECTED: THE SOUNDS STAY HERE, THE SLIDERS PLAY ONLY HERE"); };

// ------------------------------------------------------------------ the tracks: live on the FM-1 and here
function fromDevice(rec, t) {                                // the FM-1's track: its sound, the page's name for it kept
  const s = parseSound(rec, meta), o = tracks[t];
  if (o) s.name = o.name, s.cat = o.cat, s.num = o.num;
  else s.name = "", s.num = 1;
  return s;
}
const homeName = (t) => (label[t] && labelEng[t] === tracks[t].eng ? label[t] : tracks[t].name);   // (what HOME shows)
const trackName = (t) => homeName(t) || `${meta.engines[tracks[t].eng].name} SOUND`;
const engTrack = (t) => engSend([...HEAD, CMD.TRACK, 1, t, ...pack7(buildRecord(tracks[t])), ...Array.from(homeName(t), (c) => c.charCodeAt(0) & 0x7f)]);
const live = new LiveSender(async (t) => { if (conn && conn.live) await conn.client.trackSet(t, buildRecord(tracks[t]), homeName(t)); }, 15);
live.onError = (e) => say(`LIVE: ${errText(e)}`, "err");
function pushTrack(t) {                                      // track t's sound: here, and on the FM-1 (live)
  engTrack(t);
  if (conn && conn.live) live.set(t);
}
function changed(fromTool = false) {                         // the sound in the editor moved
  touchedAt = performance.now();
  if (!fromTool) { cmp = null; $("b-compare").classList.remove("on"); }
  pushTrack(selT);
  scheduleCommit(selT);
  drawTitle();
}
async function selectTrack(t) {
  flushCommit();
  selT = t;
  engSend([...HEAD, CMD.TRACK, 2, t]);
  cmp = null;
  drawTracks(), drawEditor(), drawSeqTracks(), drawSeq();
  if (conn && conn.live) {
    await live.running;
    try { await conn.client.select(t); } catch (e) { say(`SELECT: ${errText(e)}`, "err"); }
  }
}
let polling = false;
setInterval(async () => {                                    // the FM-1's knobs and track: the page follows (polled)
  if (!conn || !conn.live || conn.kind !== "fm1" || busy || polling || !live.idle || document.hidden || page === "seq") return;
  if (performance.now() - touchedAt < 800 || /SELECT|INPUT/.test(document.activeElement.tagName) && document.activeElement.type !== "range") return;
  polling = true;
  try {
    let { sel, rec, name } = await conn.client.track(selT, true);
    if (sel !== selT) selT = sel, engSend([...HEAD, CMD.TRACK, 2, sel]), { rec, name } = await conn.client.track(sel, true), drawTracks();
    if (!live.idle || performance.now() - touchedAt < 800) return;
    const mine = buildRecord(tracks[selT]);
    if (rec.slice(3, 50).some((x, k) => x !== mine[3 + k]) || name !== homeName(selT)) {
      tracks[selT] = fromDevice(rec, selT), label[selT] = name, labelEng[selT] = tracks[selT].eng;
      scheduleCommit(selT);
      engTrack(selT), drawTracks(), drawEditor();
    }
  } catch { /* (the next poll) */ } finally { polling = false; }
}, 400);
function trackTitle(t) {
  const s = tracks[t];
  return !s ? "—" : homeName(t) ? `${homeName(t)} · ${meta.engines[s.eng].name}` : trackName(t);
}
function trackButtons(box, onPick) {
  box.textContent = "";
  for (let t = 0; t < 4; t++) {
    const x = el("button", { cls: "trk" + (t === selT ? " on" : "") }, box);
    el("b", {}, x, `T${t + 1}`), x.append(tracks[t] ? trackTitle(t) : "");
    x.onclick = () => onPick(t);
  }
}
function drawTracks() { trackButtons($("tracks"), selectTrack); drawTitle(); }
function drawTitle() {
  const s = tracks[selT];
  $("ed-title").textContent = `T${selT + 1}: ${s ? trackName(selT) : ""}` + (s && JSON.stringify(s) !== loaded[selT] ? " *" : "");
  $("ed-from").textContent = from[selT] + (conn && conn.live ? ` · live on the ${conn.kind === "virtual" ? "virtual " : ""}FM-1` : conn ? " · here only (no live editing)" : " · here only (not connected)");
  drawToolState();
}

// ------------------------------------------------------------------ library
function slotLabel(r, i, fact) {
  const n = String(i + 1).padStart(2, "0");
  if (!fact && isEmpty(r)) return [n, "— EMPTY —"];
  const s = parseSound(r, meta), name = fact ? meta.factory[i].name : soundTitle(r, meta);
  return [n, `${name} (${meta.engines[s.eng].name})`];
}
const pickRec = () => (pick.fact ? Uint8Array.from(meta.factory[pick.i].rec) : user[pick.i]);
const pickName = () => (pick.fact ? `FACTORY ${pick.i + 1}: ${meta.factory[pick.i].name}` :
  `SLOT ${String(pick.i + 1).padStart(2, "0")}` + (isEmpty(user[pick.i]) ? " (EMPTY)" : `: ${soundTitle(user[pick.i], meta)}`));
function drawSlots() {
  const u = $("user"), f = $("factory");
  u.textContent = f.textContent = "";
  user.forEach((r, i) => {
    const [n, t] = slotLabel(r, i, false), on = !pick.fact && pick.i === i;
    const d = el("div", { cls: "slot" + (isEmpty(r) ? " empty" : "") + (on ? " sel" : "") + (dirty[i] ? " dirty" : ""),
                          onclick: () => { pick = { fact: false, i }; drawSlots(); }, ondblclick: () => loadTo(selT) }, u);
    el("span", { cls: "n" }, d, n + " "), d.append(t);
  });
  meta.factory.forEach((x, i) => {
    const [n, t] = slotLabel(Uint8Array.from(x.rec), i, true);
    const d = el("div", { cls: "slot" + (pick.fact && pick.i === i ? " sel" : ""), onclick: () => { pick = { fact: true, i }; drawSlots(); },
                          ondblclick: () => loadTo(selT) }, f);
    el("span", { cls: "n" }, d, n + " "), d.append(t);
  });
  $("pickname").textContent = pickName();
  $("readat").textContent = readAt;
  const es = $("ed-slot"), keep = es.value;
  es.textContent = "";
  user.forEach((r, i) => el("option", { value: i }, es, `SLOT ${String(i + 1).padStart(2, "0")}${isEmpty(r) ? " (EMPTY)" : ": " + soundTitle(r, meta)}`));
  es.value = keep || 0;
  refreshButtons();
}
function refreshButtons() {
  const c = !!conn && !busy, u = !pick.fact, empty = u && isEmpty(user[pick.i]);
  ["b-readall", "b-store", "b-backup", "b-restore", "b-sinfo", "b-sfactory"].forEach((id) => { $(id).disabled = !c; });
  $("b-disconnect").disabled = !conn || busy;
  $("b-connect").disabled = $("b-try").disabled = $("b-connect2").disabled = $("b-try2").disabled = busy;
  for (let t = 0; t < 4; t++) $("b-load" + t).disabled = empty;
  $("b-export").disabled = empty;
  ["b-rename", "b-clear"].forEach((id) => { $(id).disabled = !u || empty; });
  $("b-import").disabled = !u;
  $("b-send").disabled = !c || !u || !dirty[pick.i];
  document.querySelectorAll(".smp button").forEach((b) => { b.disabled = !c; });
}
async function readSlots() {
  for (let i = 0; i < 64; i++) {
    user[i] = await conn.client.get(OBJ.SND + i, 64);
    dirty[i] = false;
    progress(i + 1, 64);
  }
  readAt = `read from the ${conn.kind === "virtual" ? "virtual " : ""}FM-1 at ${new Date().toLocaleTimeString()}`;
}
$("b-readall").onclick = () => job("READ", async () => {
  await readSlots();
  drawSlots();
  return `READ: ${user.filter((r) => !isEmpty(r)).length} OF 64 USER SLOTS HOLD A SOUND`;
});
async function writeSlot(i, rec) {                           // flash: a user slot, read back
  await conn.client.put(OBJ.SND + i, rec, conn.info.fmt, progress);
  const back = await conn.client.get(OBJ.SND + i, 64);
  if (back.some((x, k) => x !== rec[k])) throw new EditorError("THE SLOT DID NOT READ BACK AS SENT", 0);
  user[i] = rec, dirty[i] = false;
  drawSlots();
}
$("b-send").onclick = () => job(`SEND SLOT ${pick.i + 1}`, async () => {
  const i = pick.i;
  await writeSlot(i, user[i]);
  return `SLOT ${i + 1} SENT AND READ BACK: ${isEmpty(user[i]) ? "EMPTY" : soundTitle(user[i], meta)}`;
});
function loadTo(t) {                                         // the chosen sound onto track t, here and on the FM-1
  const r = pickRec();
  if (isEmpty(r)) return;
  const s = parseSound(r, meta);
  if (pick.fact) s.name = meta.factory[pick.i].name.slice(0, 10);
  tracks[t] = s, loaded[t] = JSON.stringify(s), from[t] = pickName();
  label[t] = pick.fact ? meta.factory[pick.i].name : soundTitle(r, meta), labelEng[t] = s.eng;   // (as PRESETS names it)
  resetHistory(t);
  pushTrack(t);
  selectTrack(t);
  show("editor");
  history.replaceState(null, "", "#editor");
  say(`${pickName()} ON TRACK ${t + 1}` + (conn && conn.live ? " (ON THE FM-1 TOO, NOT SAVED)" : " (HERE)"), "ok");
}
for (let t = 0; t < 4; t++) $("b-load" + t).onclick = () => loadTo(t);
let songs = [];
async function loadSong(d) {                                 // a demo song: its sounds and names, sequences and tempo on T1..T4
  const g = d.song;
  for (const [t, name, rec] of g.tr) {
    const s = parseSound(Uint8Array.from(rec), meta);
    s.name = name.slice(0, 10);
    tracks[t] = s, loaded[t] = JSON.stringify(s), from[t] = `DEMO ${d.id} ${d.name}`;
    label[t] = name, labelEng[t] = s.eng;
    resetHistory(t);
    pushTrack(t);
  }
  g.sq.forEach((b, t) => { seqs[t] = parseSeq(Uint8Array.from(b)); seqChanged(t, false); });
  bpm = d.tempo;
  engSend([...HEAD, CMD.SEQ, 2, 0, bpm & 0x7f, bpm >> 7]);
  if (conn && conn.seq) tempoLive.set(0);
  await startEngine();
  for (const [t, mask] of g.latch) {                         // (the drones, latched here; on the FM-1 its keys do it)
    await selectTrack(t);
    for (let k = 0; k < 32; k++) if (mask >> k & 1) press(k, true);
    await new Promise((r) => setTimeout(r, 120));
    for (let k = 0; k < 32; k++) if (mask >> k & 1) press(k, false);
  }
  await selectTrack(g.tr[0][0]);
  if (g.play && !sqPlaying) $("b-sqplay").onclick();
  drawTracks(), drawEditor(), drawSeqTracks(), drawSeq();
  const keys = g.latch.map(([t, m]) => `T${t + 1} ` + [...Array(32).keys()].filter((k) => m >> k & 1).map((k) => noteName(k + 41)).join(" "));
  say(`DEMO ${d.id} ${d.name} ON T1–T4` + (conn && conn.live ? " (ON THE FM-1 TOO, NOT SAVED" + (keys.length ? `; LATCH THERE: ${keys.join(", ")}` : "") + ")" : " (HERE)"), "ok");
}
function drawSongs() {
  const b = $("songs");
  b.textContent = "";
  for (const d of songs) {
    const x = el("div", { cls: "slot", title: d.tracks.map(([t, n]) => `T${t + 1} ${n}`).join(" · "), onclick: () => loadSong(d) }, b);
    el("span", { cls: "n" }, x, d.id + " "), x.append(`${d.name} · ${d.sub}`);
  }
}
$("b-rename").onclick = () => {
  const s = parseSound(user[pick.i], meta), n = prompt("A NAME (10 CHARACTERS: A-Z 0-9 SPACE); THE FM-1 ITSELF SHOWS \"" +
    `${meta.cats[s.cat]} ${String(s.num).padStart(2, "0")}\"`, s.name || "");
  if (n === null) return;
  s.name = n.toUpperCase().replace(/[^A-Z0-9 \-!#+.]/g, "").slice(0, 10);
  user[pick.i] = buildRecord(s), dirty[pick.i] = true;
  drawSlots();
  say(`SLOT ${pick.i + 1} RENAMED HERE (SEND TO FM-1 WRITES IT)`, "ok");
};
$("b-clear").onclick = () => {
  if (!confirm(`CLEAR SLOT ${pick.i + 1} HERE? (THE FM-1 KEEPS IT UNTIL YOU SEND)`)) return;
  user[pick.i] = emptyRecord(), dirty[pick.i] = true;
  drawSlots();
  say(`SLOT ${pick.i + 1} CLEARED HERE (SEND TO FM-1 WRITES IT)`, "ok");
};
$("b-export").onclick = () => exportSound(pickRec(), pick.fact ? meta.factory[pick.i].name : null);
$("b-import").onclick = () => $("f-import").click();
async function readSoundFile(f) {                            // (GR!MM's sound files read too: the same records)
  const j = JSON.parse(await f.text()), r = Uint8Array.from(j.rec || []);
  if (!/^(grimm|arctic)-sound$/.test(j.format) || r.length !== 64 || !recordOk(r)) throw new Error("NOT A SOUND FILE");
  return r;
}
$("f-import").onchange = async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  try {
    const r = await readSoundFile(f);
    user[pick.i] = r, dirty[pick.i] = true;
    drawSlots();
    say(`IMPORTED INTO SLOT ${pick.i + 1}: ${soundTitle(r, meta)} (SEND TO FM-1 WRITES IT)`, "ok");
  } catch (err) { say(`IMPORT: ${errText(err)}`, "err"); }
};
function exportSound(r, name) {
  const t = name || soundTitle(r, meta) || "SOUND";
  download(`${t.replace(/[^A-Z0-9]+/g, "_")}.arcticsound`, JSON.stringify({ format: "arctic-sound", version: 1, name: t, rec: [...r] }));
  say(`EXPORTED: ${t}`, "ok");
}

// ------------------------------------------------------------------ controls: a name in its colour, the value big, a slider
const knobText = (s, k) => vf.text("web_knob_text", s.eng, s.k[0], s.k[1], s.k[2], s.k[3], k);
function fill(r) { r.style.setProperty("--p", `${(r.value - r.min) / Math.max(1, r.max - r.min) * 100}%`); }
function control(parent, name, min, max, get, set, text, color) {
  const c = el("div", { cls: "ctl" }, parent);
  if (color) c.style.setProperty("--c", color);
  el("div", { cls: "lab" }, c, name);
  const v = el("div", { cls: "val" }, c), r = el("input", { type: "range", min, max, step: 1, "aria-label": name }, c);
  r.value = get();
  const upd = () => { v.textContent = text ? text(+r.value) : String(r.value); fill(r); };
  r.addEventListener("input", () => { set(+r.value); upd(); });
  upd();
  return r;
}
function choice(parent, name, names, get, set, color) {
  const c = el("div", { cls: "ctl" }, parent);
  if (color) c.style.setProperty("--c", color);
  el("div", { cls: "lab" }, c, name);
  const s = el("select", { "aria-label": name }, c);
  names.forEach((n, i) => el("option", { value: i }, s, n));
  s.value = get();
  s.addEventListener("change", () => set(+s.value));
  return s;
}
function card(parent, icon, title, color, small) {
  const g = el("div", { cls: "card" }, parent);
  if (color) g.style.setProperty("--c", color);
  const h = el("h2", {}, g);
  el("i", {}, h, icon), h.append(title);
  if (small) el("small", {}, h, small);
  return g;
}

// ------------------------------------------------------------------ the sound page
function drawEditor() {
  const ed = $("ed"), s = tracks[selT];
  ed.textContent = "";
  if (!s) return;
  const eng = meta.engines[s.eng], left = el("div", {}, ed), right = el("div", {}, ed);

  let g = card(left, "◈", eng.name, "var(--violet)", "source · its four knobs");
  let c = el("div", { cls: "ctls" }, g);
  for (let k = 0; k < 4; k++)
    control(c, eng.knobs[k], 0, eng.max[k], () => s.k[k], (v) => {
      s.k[k] = v;
      if (s.eng === 1 && k === 0) { setPatch(s, v); drawFM(); }   // (FM's PATCH: its operators as built, as on the FM-1)
      changed();
    }, () => knobText(s, k), KC[k]);

  g = card(left, "⌁", "env", "var(--ice)", "attack · decay · sustain · release");
  const cv = el("canvas", { cls: "env", width: 640, height: 140 }, g);
  c = el("div", { cls: "ctls" }, g);
  ["ATTACK", "DECAY", "SUSTAIN", "RELEASE"].forEach((n, k) =>
    control(c, n, 0, 127, () => s.env[k], (v) => { s.env[k] = v; drawEnv(cv, s); changed(); }, (v) => vf.text("web_env_text", k, v).toLowerCase(), EC[k]));
  drawEnv(cv, s);

  g = card(right, "◉", "sound", "var(--aurora)");
  c = el("div", { cls: "ctls" }, g);
  const cn = el("div", { cls: "ctl", style: "grid-column: span 2" }, c);
  el("div", { cls: "lab" }, cn, "name (the editor's)");
  const nm = el("input", { type: "text", maxlength: 10, style: "width:100%;margin-top:4px" }, cn);
  nm.value = s.name || "";
  nm.addEventListener("input", () => {                     // (10 characters the FM-1 shows: A-Z 0-9 space - ! # + .)
    const at = nm.selectionStart;
    nm.value = s.name = nm.value.toUpperCase().replace(/[^A-Z0-9 \-!#+.]/g, "").slice(0, 10);
    nm.setSelectionRange(at, at);
    label[selT] = s.name.trim(), labelEng[selT] = s.eng;     // (the FM-1's HOME shows it too)
    changed(), drawTracks();
  });
  choice(c, "category", meta.cats, () => s.cat, (v) => { s.cat = v; drawTitle(); drawTracks(); }, "var(--aurora)");
  choice(c, "latch", ["OFF", "ON"], () => (s.latch ? 1 : 0), (v) => { s.latch = v; changed(); }, "var(--ice)");
  choice(c, "source", meta.engines.map((e) => e.name), () => s.eng, (v) => {
    s.eng = v;
    for (let k = 0; k < 4; k++) s.k[k] = Math.min(s.k[k], meta.engines[v].max[k]);
    if (v === 1) setPatch(s, s.k[0]);
    changed(), drawEditor(), drawTracks();
  }, "var(--violet)");
  choice(c, "chord mode", ["OFF", "ON"], () => (s.chordOn ? 1 : 0), (v) => { s.chordOn = v; changed(); }, "var(--pink)");
  choice(c, "chord type", meta.chords, () => s.chord, (v) => { s.chord = v; changed(); }, "var(--pink)");
  choice(c, "voicing", meta.voicings, () => s.voicing, (v) => { s.voicing = v; changed(); }, "var(--pink)");

  g = card(right, "∿", "lfo", "var(--amber)");
  c = el("div", { cls: "ctls" }, g);
  control(c, "RATE", 0, 127, () => s.lfo[0], (v) => { s.lfo[0] = v; changed(); }, (v) => vf.text("web_lfo_text", v).toLowerCase(), "var(--amber)");
  control(c, "DEPTH", 0, 100, () => s.lfo[1], (v) => { s.lfo[1] = v; changed(); }, (v) => `${v} %`, "var(--gold)");
  choice(c, "destination", meta.lfoDest.map((d, i) => (i >= 3 && i <= 6 ? eng.knobs[i - 3] : d)), () => s.lfo[2], (v) => { s.lfo[2] = v; changed(); }, "var(--teal)");
  choice(c, "shape", meta.lfoShape, () => s.lfo[3], (v) => { s.lfo[3] = v; changed(); }, "var(--sky)");

  g = card(right, "❄", "drone", "var(--ice)");              // (the FM-1's DRONE page: LFO, tapped again)
  c = el("div", { cls: "ctls" }, g);
  s.drone = s.drone || { borga: 0, storm: 0, rise: 0 };
  control(c, "BORGA", 0, 100, () => s.drone.borga, (v) => { s.drone.borga = v; changed(); }, (v) => `${v} %`, "var(--peri)");
  control(c, "STOARBMA", 0, 100, () => s.drone.storm, (v) => { s.drone.storm = v; changed(); }, (v) => `${v} %`, "var(--teal)");
  choice(c, "gohccát", RISE, () => s.drone.rise, (v) => { s.drone.rise = v; changed(); }, "var(--amber)");
  el("p", { style: "grid-column: 1 / -1; margin: 4px 0 0; color: var(--muted); font-size: 11px" }, c,
    "borga (snowdrift): the sound wanders on its own · stoarbma (storm): the tension · gohccát (to wake): bars to reach it");
  drawFM();
  drawTitle();
}
function drawEnv(cv, s) {                                    // the ENV's shape (its times as the knobs' share, not to scale)
  const g = cv.getContext("2d"), W = cv.width, H = cv.height, [a, d, su, r] = s.env.map((v) => v / 127);
  const x1 = 8 + a * W * 0.24, x2 = x1 + 4 + d * W * 0.24, x3 = Math.max(x2 + 40, W * 0.72), x4 = x3 + 4 + r * (W - 16 - x3);
  const top = 12, bot = H - 10, ys = bot - su * (bot - top);
  g.clearRect(0, 0, W, H);
  g.strokeStyle = "#1c232c", g.lineWidth = 2;
  g.beginPath(), g.moveTo(0, bot + 1), g.lineTo(W, bot + 1), g.stroke();
  g.strokeStyle = "#22d3ee", g.lineWidth = 3, g.shadowColor = "#22d3ee", g.shadowBlur = 8;
  g.beginPath(), g.moveTo(8, bot), g.lineTo(x1, top), g.lineTo(x2, ys), g.lineTo(x3, ys), g.lineTo(x4, bot), g.stroke();
  g.shadowBlur = 0;
}
function setPatch(s, v) {
  const f = meta.fmPatches[Math.min(v, meta.fmPatches.length - 1)].fm;
  s.fm = { alg: f[0] & 31, fb: f[1] & 7, ops: [0, 1, 2, 3, 4, 5].map((i) => f.slice(2 + i * 5, 7 + i * 5)) };
}
const ratioText = (o) => { const r = (o[0] ? o[0] * 100 : 50) * (100 + o[1]) / 100; return (r >= 1000 ? (r / 100).toFixed(1) : (r / 100).toFixed(2)); };
function drawFM() {
  const f = $("fm"), s = tracks[selT];
  f.textContent = "";
  f.style.display = s && s.eng === 1 ? "" : "none";
  if (!s || s.eng !== 1) return;
  const h = el("h2", {}, f);
  el("i", {}, h, "⚛"), h.append("fm"), el("small", {}, h, "the track's own patch · fm algo · fm op");
  const top = el("div", { cls: "cols" }, f), a = el("div", { cls: "ctls three" }, top), b = el("div", {}, top);
  const cv = el("canvas", { id: "alg", width: 216, height: 104 }, b);
  control(a, "ALGO", 0, 31, () => s.fm.alg, (v) => { s.fm.alg = v; drawAlg(cv); changed(); }, (v) => String(v + 1).padStart(2, "0"), "var(--sky)");
  control(a, "FEEDBACK", 0, 7, () => s.fm.fb, (v) => { s.fm.fb = v; drawAlg(cv); changed(); }, null, "var(--violet)");
  const t = el("table", { cls: "ops" }, f), hr = el("tr", {}, t);
  ["", "RATIO", "", "FINE", "", "LEVEL", "", "ATTACK", "", "DECAY", ""].forEach((x) => el("td", { cls: "lab" }, hr, x));
  const OC = ["var(--sky)", "var(--teal)", "var(--amber)", "var(--aurora)", "var(--pink)"];
  s.fm.ops.forEach((o, i) => {
    const r = el("tr", {}, t);
    el("td", { cls: "o" }, r, `OP${i + 1}`);
    const vr = [];
    [[31, 0], [99, 1], [99, 2], [99, 3], [99, 4]].forEach(([mx, k]) => {
      const c = el("td", {}, r), v = el("td", { cls: "v" }, r), in_ = el("input", { type: "range", min: 0, max: mx, step: 1, "aria-label": `OP${i + 1}` }, c);
      in_.style.setProperty("--c", OC[k]);
      in_.value = o[k];
      vr.push(v);
      const upd = () => { v.textContent = k === 0 ? ratioText(o) : String(o[k]); if (k === 1) vr[0].textContent = ratioText(o); fill(in_); };
      in_.addEventListener("input", () => { o[k] = +in_.value; upd(); changed(); });
      upd();
    });
  });
  drawAlg(cv);
}
function drawAlg(cv) {                                       // ui.c's fm_algo_draw, here: the operators as wired
  const s = tracks[selT], g = cv.getContext("2d"), alg = meta.fmAlg[s.fm.alg], car = [], tg = [0, 0, 0, 0, 0, 0], bus = [0, 0, 0];
  let fbin = -1;
  for (let k = 0; k < 6; k++) {
    const f = alg[k], o = 5 - k, ib = (f >> 4) & 3, ob = f & 3;
    if (ib) for (let j = 0; j < 6; j++) if (bus[ib] >> j & 1) tg[j] |= 1 << o;
    if (f & 0x40) fbin = o;
    if (ob) bus[ob] = (f & 4 ? bus[ob] : 0) | 1 << o; else car.push(o);
  }
  const dep = [0, 0, 0, 0, 0, 0], x2 = [-1, -1, -1, -1, -1, -1];
  for (let r = 0; r < 6; r++) for (let o = 0; o < 6; o++) for (let j = 0; j < 6; j++) if (tg[o] >> j & 1 && dep[j] + 1 > dep[o]) dep[o] = dep[j] + 1;
  let slot = 0;
  const place = (o) => {
    let lo = 999, hi = -1;
    for (let j = 0; j < 6; j++) {
      let first = 0;
      while (first < 6 && !(tg[j] >> first & 1)) first++;
      if (first !== o || x2[j] >= 0) continue;
      place(j);
      lo = Math.min(lo, x2[j]), hi = Math.max(hi, x2[j]);
    }
    x2[o] = hi < 0 ? 2 * slot++ : (lo + hi) >> 1;
  };
  car.slice().sort((a, b) => a - b).forEach(place);
  for (let o = 0; o < 6; o++) {
    let a = 999, b = -1, n = 0;
    for (let j = 0; j < 6; j++) if (tg[o] >> j & 1) a = Math.min(a, x2[j]), b = Math.max(b, x2[j]), n++;
    if (n > 1) x2[o] = (a + b) >> 1;
    if (x2[o] < 0) x2[o] = 2 * slot++;
  }
  const C = getComputedStyle(document.documentElement), col = (v) => C.getPropertyValue(v).trim();
  const w = Math.floor(216 / Math.max(1, slot)), bw = Math.min(w - 6, 28), X = (o) => Math.floor(w / 2 + x2[o] * w / 2), Y = (o) => 70 - dep[o] * 22;
  g.fillStyle = col("--bg"), g.fillRect(0, 0, 216, 104);
  g.fillStyle = col("--muted");
  for (let o = 0; o < 6; o++) for (let j = 0; j < 6; j++) if (tg[o] >> j & 1) {
    const xa = X(o), xb = X(j), ym = Y(j) - 4;
    g.fillRect(xa - 1, Y(o) + 14, 2, ym - Y(o) - 14), g.fillRect(Math.min(xa, xb) - 1, ym, Math.abs(xa - xb) + 2, 2), g.fillRect(xb - 1, ym, 2, 4);
  }
  g.font = '8px "Press Start 2P", monospace';
  let lo = 999, hi = -1;
  for (let o = 0; o < 6; o++) {
    const x = X(o) - (bw >> 1), y = Y(o), c = car.includes(o);
    if (c) {
      lo = Math.min(lo, X(o)), hi = Math.max(hi, X(o));
      g.fillStyle = col("--label"), g.fillRect(X(o) - 1, y + 14, 2, 8);
      g.fillStyle = col("--sky"), g.fillRect(x, y, bw, 14), g.fillStyle = col("--bg"), g.fillRect(x + 2, y + 2, bw - 4, 10);
      g.fillStyle = col("--sky");
    } else { g.fillStyle = col("--part-hi"), g.fillRect(x, y, bw, 14); g.fillStyle = col("--label"); }
    g.fillText(String(o + 1), X(o) - 4, y + 11);
    if (o === fbin && s.fm.fb) { g.fillStyle = col("--violet"); g.fillRect(x + bw, y + 6, 4, 2), g.fillRect(x + bw + 2, y - 4, 2, 10), g.fillRect(X(o), y - 4, x + bw + 4 - X(o), 2); }
  }
  g.fillStyle = col("--label"), g.fillRect(lo - 1, 92, hi - lo + 2, 2);
  g.fillStyle = col("--muted"), g.fillText("OUT", 96, 103);
}
$("b-store").onclick = () => {                               // flash: the track's sound into a user slot
  const s = tracks[selT], i = +$("ed-slot").value;
  if (!s || !conn) return;
  if (!isEmpty(user[i]) && !confirm(`SLOT ${i + 1} HOLDS ${soundTitle(user[i], meta)}: REPLACE IT ON THE FM-1?`)) return;
  job(`STORE IN SLOT ${i + 1}`, async () => {
    let num = 0;                                             // (its number: one past the category's highest, as the FM-1)
    user.forEach((r, k) => { if (k !== i && !isEmpty(r) && r[1] === s.cat && r[2] > num) num = r[2]; });
    s.num = Math.min(99, num + 1);
    const rec = buildRecord(s);
    await writeSlot(i, rec);
    loaded[selT] = JSON.stringify(s), from[selT] = `SLOT ${String(i + 1).padStart(2, "0")}`;
    drawTitle();
    return `T${selT + 1} STORED IN SLOT ${i + 1} ON THE FM-1 AND READ BACK: ${soundTitle(rec, meta)}`;
  });
};
$("b-eexport").onclick = () => { if (tracks[selT]) exportSound(buildRecord(tracks[selT])); };
$("b-eimport").onclick = () => $("f-eimport").click();
$("f-eimport").onchange = async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  try {
    const s = parseSound(await readSoundFile(f), meta);
    tracks[selT] = s, loaded[selT] = JSON.stringify(s), from[selT] = f.name.toUpperCase();
    label[selT] = s.name, labelEng[selT] = s.eng;
    resetHistory(selT);
    pushTrack(selT), drawTracks(), drawEditor();
    say(`${f.name.toUpperCase()} ON TRACK ${selT + 1}`, "ok");
  } catch (err) { say(`IMPORT: ${errText(err)}`, "err"); }
};
$("b-revert").onclick = () => {
  if (!loaded[selT]) return;
  applySound(JSON.parse(loaded[selT]));
  say(`T${selT + 1}: AS LOADED (UNDO BRINGS THE EDIT BACK)`, "ok");
};

// ------------------------------------------------------------------ SOUND TOOLS: undo, redo, compare, A / B, morph, randomize
// a track's history: its sound as last committed (base) and the ones before (undo) and undone (redo); an edit commits
// once the controls rest (350 ms), so a drag is one step
const hist = [0, 1, 2, 3].map(() => ({ undo: [], redo: [], base: "", timer: 0 }));
let cmp = null, ab = { a: null, b: null, last: "" };          // compare: the sound it hides; A and B: sounds (JSON)
function resetHistory(t) { const h = hist[t]; clearTimeout(h.timer); h.timer = 0; h.undo = [], h.redo = [], h.base = JSON.stringify(tracks[t]); }
function commit(t) {
  const h = hist[t], now = JSON.stringify(tracks[t]);
  clearTimeout(h.timer), h.timer = 0;
  if (now === h.base) return;
  h.undo.push(h.base), h.redo = [], h.base = now;
  if (h.undo.length > 200) h.undo.shift();
  drawToolState();
}
function scheduleCommit(t) { const h = hist[t]; clearTimeout(h.timer); h.timer = setTimeout(() => commit(t), 350); }
function flushCommit() { if (hist[selT].timer) commit(selT); }
function applySound(s, commitIt = true) {                    // the sound in the editor := s (sent, drawn; one undo step)
  tracks[selT] = s;
  touchedAt = performance.now();
  pushTrack(selT);
  if (commitIt) commit(selT);
  drawTracks(), drawEditor();
}
$("b-undo").onclick = () => {
  flushCommit();
  const h = hist[selT];
  if (!h.undo.length) return;
  h.redo.push(h.base), h.base = h.undo.pop();
  cmp = null;
  applySound(JSON.parse(h.base), false);
};
$("b-redo").onclick = () => {
  const h = hist[selT];
  if (!h.redo.length) return;
  h.undo.push(h.base), h.base = h.redo.pop();
  applySound(JSON.parse(h.base), false);
};
addEventListener("keydown", (e) => {                         // (Ctrl/Cmd+Z, Shift: redo; not while typing a name)
  if (!(e.ctrlKey || e.metaKey) || e.key.toLowerCase() !== "z" || /INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName) &&
      document.activeElement.type !== "range" || page !== "editor") return;
  e.preventDefault();
  $(e.shiftKey ? "b-redo" : "b-undo").click();
});
$("b-compare").onclick = () => {                             // the sound as loaded, and back to the edit
  flushCommit();
  if (cmp) { const back = cmp; cmp = null; tracks[selT] = JSON.parse(back); pushTrack(selT); drawTracks(), drawEditor(); say("COMPARE: YOUR EDIT", "ok"); }
  else if (loaded[selT]) { cmp = JSON.stringify(tracks[selT]); tracks[selT] = JSON.parse(loaded[selT]); pushTrack(selT); drawTracks(), drawEditor(); say("COMPARE: THE SOUND AS LOADED (CLICK AGAIN FOR YOUR EDIT)", "ok"); }
  $("b-compare").classList.toggle("on", !!cmp);
};
function storeAB(w) {
  flushCommit();
  ab[w] = JSON.stringify(cmp ? JSON.parse(cmp) : tracks[selT]);
  say(`STORED IN ${w.toUpperCase()}: ${trackName(selT)}`, "ok");
  drawToolState();
}
$("b-sta").onclick = () => storeAB("a");
$("b-stb").onclick = () => storeAB("b");
$("b-ab").onclick = () => {
  if (!ab.a || !ab.b) return;
  ab.last = ab.last === "a" ? "b" : "a";
  cmp = null;
  applySound(JSON.parse(ab[ab.last]));
  $("t-morph").value = ab.last === "a" ? 0 : 100, fill($("t-morph"));
  say(`PLAYING ${ab.last.toUpperCase()}`, "ok");
};
function morph(a, b, m) {                                    // A..B at m (0..1): the numbers between, the choices A's then B's
  const lerp = (x, y) => Math.round(x + (y - x) * m), s = JSON.parse(JSON.stringify(m < 0.5 ? a : b));
  if (a.eng === b.eng) s.k = a.k.map((x, k) => lerp(x, b.k[k]));
  s.env = a.env.map((x, k) => lerp(x, b.env[k]));
  s.lfo[0] = lerp(a.lfo[0], b.lfo[0]), s.lfo[1] = lerp(a.lfo[1], b.lfo[1]);
  if (a.drone && b.drone) s.drone.borga = lerp(a.drone.borga, b.drone.borga), s.drone.storm = lerp(a.drone.storm, b.drone.storm);
  if (a.eng === 1 && b.eng === 1) s.fm.ops = a.fm.ops.map((o, i) => o.map((x, k) => lerp(x, b.fm.ops[i][k])));
  return s;
}
$("t-morph").addEventListener("input", () => {
  const r = $("t-morph");
  fill(r);
  if (!ab.a || !ab.b) return;
  cmp = null;
  tracks[selT] = morph(JSON.parse(ab.a), JSON.parse(ab.b), r.value / 100);
  touchedAt = performance.now();
  pushTrack(selT), scheduleCommit(selT), drawTitle();
  clearTimeout(r.t), r.t = setTimeout(drawEditor, 120);      // (the controls follow once the morph rests)
});
$("t-ramount").addEventListener("input", () => fill($("t-ramount")));
$("b-rand").onclick = () => {                                // a step from here, as far as AMOUNT says (or all new)
  flushCommit();
  const s = JSON.parse(JSON.stringify(tracks[selT])), sc = $("t-rscope").value, amt = $("t-ramount").value / 100;
  const nudge = (v, max) => clamp(Math.round(v + (Math.random() * 2 - 1) * amt * max), 0, max);
  if (sc === "src") {                                        // (a new source: not SAMPLE, whose slots may be empty)
    const pool = meta.engines.map((_, i) => i).filter((i) => meta.engines[i].name !== "SAMPLE");
    s.eng = pool[Math.floor(Math.random() * pool.length)];
    s.k = meta.engines[s.eng].max.map((m) => Math.floor(Math.random() * (m + 1)));
    if (s.eng === 1) setPatch(s, s.k[0]);
    s.env = s.env.map(() => Math.floor(Math.random() * 128));
    s.lfo[0] = Math.floor(Math.random() * 128), s.lfo[1] = Math.floor(Math.random() * 60);
  } else {
    if (sc === "knobs" || sc === "all") s.k = s.k.map((v, k) => nudge(v, meta.engines[s.eng].max[k]));
    if (sc === "env" || sc === "all") s.env = s.env.map((v) => nudge(v, 127));
    if (sc === "lfo" || sc === "all") s.lfo[0] = nudge(s.lfo[0], 127), s.lfo[1] = nudge(s.lfo[1], 100);
    if (sc === "drone" || sc === "all") s.drone.borga = nudge(s.drone.borga, 100), s.drone.storm = nudge(s.drone.storm, 100);
    if (s.eng === 1 && (sc === "knobs" || sc === "all") && s.k[0] !== tracks[selT].k[0]) setPatch(s, s.k[0]);
  }
  cmp = null;
  applySound(s);
  say(`RANDOMIZED (${$("t-rscope").selectedOptions[0].text.toUpperCase()}, ${$("t-ramount").value} %): UNDO TAKES IT BACK`, "ok");
};
function diffList() {                                        // what changed since the sound was loaded, in words
  const out = [], s = tracks[selT], l = loaded[selT] && JSON.parse(loaded[selT]);
  if (!s || !l) return out;
  const add = (what, a, b) => { if (String(a) !== String(b)) out.push([what, a, b]); };
  add("SOURCE", meta.engines[l.eng].name, meta.engines[s.eng].name);
  const eng = meta.engines[s.eng];
  for (let k = 0; k < 4; k++) add(eng.knobs[k], l.eng === s.eng ? knobText({ ...s, k: l.k }, k) : l.k[k], knobText(s, k));
  ["ATTACK", "DECAY", "SUSTAIN", "RELEASE"].forEach((n, k) => add(n, vf.text("web_env_text", k, l.env[k]), vf.text("web_env_text", k, s.env[k])));
  add("LFO RATE", vf.text("web_lfo_text", l.lfo[0]), vf.text("web_lfo_text", s.lfo[0]));
  add("LFO DEPTH", `${l.lfo[1]} %`, `${s.lfo[1]} %`);
  add("LFO DEST", meta.lfoDest[l.lfo[2]], meta.lfoDest[s.lfo[2]]);
  add("LFO SHAPE", meta.lfoShape[l.lfo[3]], meta.lfoShape[s.lfo[3]]);
  add("LATCH", l.latch ? "ON" : "OFF", s.latch ? "ON" : "OFF");
  const ld = l.drone || { borga: 0, storm: 0, rise: 0 }, sd = s.drone || ld;
  add("BORGA", `${ld.borga} %`, `${sd.borga} %`);
  add("STOARBMA", `${ld.storm} %`, `${sd.storm} %`);
  add("GOHCCÁT", RISE[ld.rise], RISE[sd.rise]);
  add("CHORD", l.chordOn ? meta.chords[l.chord] : "OFF", s.chordOn ? meta.chords[s.chord] : "OFF");
  add("NAME", l.name || "—", s.name || "—");
  if (s.eng === 1 && l.eng === 1 && JSON.stringify(s.fm) !== JSON.stringify(l.fm)) out.push(["FM PATCH", "as loaded", "edited"]);
  return out;
}
function drawToolState() {
  const h = hist[selT];
  if (!$("b-undo")) return;
  $("b-undo").disabled = !h.undo.length && !h.timer;
  $("b-redo").disabled = !h.redo.length;
  $("t-steps").textContent = `${h.undo.length} / ${h.redo.length} steps`;
  $("b-ab").disabled = !ab.a || !ab.b;
  $("t-morph").disabled = !ab.a || !ab.b;
  $("b-sta").classList.toggle("on", !!ab.a), $("b-stb").classList.toggle("on", !!ab.b);
  const d = $("diff");
  d.textContent = "";
  const list = diffList();
  if (!list.length) el("li", {}, d, "nothing: as loaded");
  list.forEach(([w, a, b]) => { const li = el("li", {}, d); el("b", {}, li, w + " "); li.append(`${a} → ${b}`); });
}

// ------------------------------------------------------------------ the sequencer page (SEQ, protocol 3; here: the engine's)
const seqs = [0, 1, 2, 3].map(() => ({ steps: 16, div: 4, swing: 0, mode: 0, st: Array.from({ length: SEQ_N }, emptyStep) }));
let bpm = 120, sqPlaying = false, sqPos = [-1, -1, -1, -1], selStep = 0, view = "grid", seqTouched = 0;
const seqLive = new LiveSender(async (t) => { if (conn && conn.seq) await conn.client.seqSet(t, buildSeq(seqs[t])); }, 15);
seqLive.onError = (e) => say(`SEQ: ${errText(e)}`, "err");
const engSeq = (t) => engSend([...HEAD, CMD.SEQ, 1, t, ...pack7(buildSeq(seqs[t]))]);
const remotePlay = () => conn && conn.kind === "fm1" && conn.seq;   // (the FM-1 plays; else this page's engine)
function seqChanged(t = selT, redraw = true) {
  seqTouched = performance.now();
  engSeq(t);
  if (conn && conn.seq) seqLive.set(t);
  if (redraw) drawSteps();
}
async function readSeqs() {
  for (let t = 0; t < 4; t++) {
    const r = await conn.client.seq(t);
    seqs[t] = parseSeq(r.bytes), bpm = r.bpm, sqPlaying = r.playing;
    engSeq(t);
  }
  engSend([...HEAD, CMD.SEQ, 2, 0, bpm & 0x7f, bpm >> 7]);
}
function engSeqState(v) {                                    // the worklet's playhead (when this page plays)
  if (remotePlay()) return;
  sqPlaying = !!(v >>> 28 & 1);
  for (let t = 0; t < 4; t++) sqPos[t] = (v >>> (7 * t) & 127) - 1;
  if (page === "seq") drawPlayhead();
}
let seqPolling = false;
setInterval(async () => {                                    // the FM-1's sequence: its playhead, its edits (polled)
  if (page !== "seq" || !remotePlay() || busy || seqPolling || !seqLive.idle || document.hidden) return;
  seqPolling = true;
  try {
    const r = await conn.client.seq(selT, true);
    sqPlaying = r.playing, sqPos = sqPos.map((p, t) => (t === selT ? r.pos : p));
    if (r.bpm !== bpm) bpm = r.bpm, drawSeqPattern();
    if (seqLive.idle && performance.now() - seqTouched > 1000 && !/INPUT|SELECT/.test(document.activeElement.tagName)) {
      const mine = buildSeq(seqs[selT]);
      if (r.bytes.some((x, k) => x !== mine[k])) { seqs[selT] = parseSeq(r.bytes); engSeq(selT); drawSeq(); }
    }
    drawPlayhead();
  } catch { /* (the next poll) */ } finally { seqPolling = false; }
}, 250);
function drawSeqTracks() { trackButtons($("strks"), selectTrack); }
function drawSeq() { drawSeqPattern(); drawSteps(); }
function drawSeqPattern() {
  const q = seqs[selT], c = $("sq-pat");
  c.textContent = "";
  control(c, "LEN", 1, SEQ_N, () => q.steps, (v) => { q.steps = v; $("sq-len").value = v; seqChanged(); }, (v) => `${v} step${v > 1 ? "s" : ""}`, "var(--violet)");
  choice(c, "DIV", SEQ_DIV, () => q.div, (v) => { q.div = v; seqChanged(); }, "var(--ice)");
  control(c, "SWING", 0, 127, () => q.swing, (v) => { q.swing = v; seqChanged(selT, false); }, (v) => `${Math.round(v * 50 / 127)} %`, "var(--pink)");
  choice(c, "PLAY MODE", SEQ_MODE, () => q.mode, (v) => { q.mode = v; seqChanged(); }, "var(--violet)");
  control(c, "TEMPO", 20, 300, () => bpm, (v) => {
    bpm = v;
    engSend([...HEAD, CMD.SEQ, 2, 0, v & 0x7f, v >> 7]);
    if (conn && conn.seq) tempoLive.set(0);
    seqTouched = performance.now();
  }, (v) => `${v} bpm`, "var(--ice)");
  $("sq-len").value = q.steps;
  drawPlayState();
}
const tempoLive = new LiveSender(async () => { if (conn && conn.seq) await conn.client.tempo(bpm); }, 30);
function drawPlayState() {
  $("b-sqplay").textContent = sqPlaying ? "■ stop" : "▶ play";
  $("b-sqplay").classList.toggle("on", sqPlaying);
  $("sq-state").textContent = `T${selT + 1} · ${remotePlay() ? "on the FM-1" : "played here"} · ${sqPlaying ? "playing" : "stopped"}`;
}
const noteText = (s) => (!s.note ? "·" : s.chord ? `${noteName(s.note)}♯` : noteName(s.note));
function parseNote(txt) {                                    // "C4", "f#3", "Db2", "60", "-" / "" (off) -> 0..127, or null
  const t = txt.trim().toUpperCase();
  if (!t || t === "-" || t === "·" || t === "OFF") return 0;
  if (/^\d+$/.test(t)) return clamp(+t, 0, 127);
  const m = /^([A-G])([#B]?)(-?\d)$/.exec(t);
  if (!m) return null;
  const n = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 }[m[1]] + (m[2] === "#" ? 1 : m[2] === "B" ? -1 : 0) + (+m[3] + 1) * 12;
  return n >= 1 && n <= 127 ? n : null;
}
function drawSteps() {
  const q = seqs[selT];
  $("sq-grid").hidden = view !== "grid", $("roll").hidden = view !== "roll";
  $("b-vgrid").classList.toggle("on", view === "grid"), $("b-vroll").classList.toggle("on", view === "roll");
  if (view === "grid") {
    const g = $("sq-grid");
    g.textContent = "";
    q.st.forEach((s, k) => {
      const d = el("div", { cls: "st" + (s.note ? " on" : "") + (s.chord ? " chord" : "") + (k >= q.steps ? " out" : "") + (k === selStep ? " sel" : "") +
                                (sqPlaying && sqPos[selT] === k ? " now" : ""), title: `step ${k + 1}` }, g);
      el("span", { cls: "i" }, d, String(k + 1)), el("span", { cls: "nn" }, d, noteText(s));
      if (s.note && s.ratchet > 1) el("span", { cls: "bars" }, d, "×" + s.ratchet);
      d.onclick = () => { selStep = k; drawSteps(); };
      d.ondblclick = () => { const st = q.st[k]; if (st.note) st.note = 0, st.chord = 0; else st.note = lastNote; selStep = k; seqChanged(); };
    });
  } else drawRoll();
  drawStepEditor();
  drawStepList();
  drawPlayState();
}
let lastNote = 60;
function drawPlayhead() {
  if (view === "grid") document.querySelectorAll("#sq-grid .st").forEach((d, k) => d.classList.toggle("now", sqPlaying && sqPos[selT] === k));
  else drawRoll();
  drawPlayState();
}
function drawStepEditor() {
  const q = seqs[selT], s = q.st[selStep], c = $("sq-step");
  $("sq-stitle").textContent = `step ${selStep + 1}${s.chord ? " · a chord step (recorded in chord mode)" : ""}`;
  c.textContent = "";
  const re = () => { seqChanged(selT, false); refreshStepCell(); };
  control(c, "NOTE", 0, 127, () => s.note, (v) => { s.note = v; if (!v) s.chord = 0; else lastNote = v; re(); }, (v) => (v ? `${noteName(v)} (${v})` : "off"), "var(--violet)");
  control(c, "CHANCE", 0, 100, () => s.chance, (v) => { s.chance = v; re(); }, (v) => `${v} %`, "var(--ice)");
  control(c, "RATCHET", 1, 4, () => s.ratchet, (v) => { s.ratchet = v; re(); }, (v) => `×${v}`, "var(--pink)");
  control(c, "LENGTH", 1, SEQ_N, () => s.len, (v) => { s.len = v; re(); }, (v) => `${v} step${v > 1 ? "s" : ""}`, "var(--violet)");
}
function refreshStepCell() {                                 // (a slider moved: the grid's cell and the list follow, cheaply)
  const q = seqs[selT], s = q.st[selStep];
  if (view === "grid") {
    const d = $("sq-grid").children[selStep];
    if (d) {
      d.classList.toggle("on", !!s.note), d.classList.toggle("chord", !!s.chord);
      d.querySelector(".nn").textContent = noteText(s);
      const b = d.querySelector(".bars");
      if (s.note && s.ratchet > 1) (b || el("span", { cls: "bars" }, d)).textContent = "×" + s.ratchet; else if (b) b.remove();
    }
  } else drawRoll();
  const row = $("sq-list").rows[selStep + 1];
  if (row) { const ins = row.querySelectorAll("input"); ins[0].value = s.note ? noteName(s.note) : "", ins[1].value = s.chance, ins[2].value = s.ratchet, ins[3].value = s.len; }
}
function drawStepList() {
  const q = seqs[selT], t = $("sq-list");
  t.textContent = "";
  const h = el("tr", {}, t);
  ["#", "note", "chance %", "ratchet", "length"].forEach((x) => el("th", {}, h, x));
  q.st.forEach((s, k) => {
    const r = el("tr", { cls: k >= q.steps ? "out" : "" }, t);
    el("td", {}, r, String(k + 1));
    const nt = el("input", { type: "text", value: s.note ? noteName(s.note) : "", placeholder: "·", "aria-label": `step ${k + 1} note` }, el("td", {}, r));
    nt.addEventListener("change", () => {
      const n = parseNote(nt.value);
      if (n === null) { say(`STEP ${k + 1}: "${nt.value}" IS NOT A NOTE (C4, F#3, 60, OR EMPTY FOR OFF)`, "err"); nt.value = s.note ? noteName(s.note) : ""; return; }
      s.note = n, s.chord = n ? s.chord : 0, selStep = k;
      if (n) lastNote = n;
      seqChanged();
    });
    [["chance", 0, 100], ["ratchet", 1, 4], ["len", 1, SEQ_N]].forEach(([f, lo, hi]) => {
      const i = el("input", { type: "number", min: lo, max: hi, value: s[f], "aria-label": `step ${k + 1} ${f}` }, el("td", {}, r));
      i.addEventListener("change", () => { s[f] = clamp(+i.value | 0, lo, hi); i.value = s[f]; selStep = k; seqChanged(); });
    });
  });
}
// the piano roll: the steps across, the notes up; a click sets a step's note (on it again: off), a drag draws
let rollLo = 48;
const ROWS = 36;
function rollRange() {
  const n = seqs[selT].st.filter((s) => s.note).map((s) => s.note);
  if (!n.length) return;
  const lo = Math.min(...n), hi = Math.max(...n);
  if (lo < rollLo || hi >= rollLo + ROWS) rollLo = clamp(Math.floor((lo + hi) / 2) - ROWS / 2, 0, 127 - ROWS);
}
function drawRoll() {
  const cv = $("roll"), q = seqs[selT], W = cv.width, H = cv.height, gx = 36, cw = (W - gx) / q.steps, rh = H / ROWS, g = cv.getContext("2d");
  g.fillStyle = "#060410", g.fillRect(0, 0, W, H);
  for (let r = 0; r < ROWS; r++) {                            // the rows: the black keys darker, every C named
    const n = rollLo + ROWS - 1 - r, y = r * rh, black = [1, 3, 6, 8, 10].includes(n % 12);
    g.fillStyle = black ? "#0a0d12" : "#0d1117", g.fillRect(gx, y, W - gx, rh);
    g.fillStyle = n % 12 === 0 ? "#45606e" : "#161b22", g.fillRect(gx, y + rh - 1, W - gx, 1);
    if (n % 12 === 0) { g.fillStyle = "#7fa8b8", g.font = "11px ui-monospace, monospace", g.fillText(noteName(n), 4, y + rh - 3); }
  }
  for (let k = 0; k <= q.steps; k++) { g.fillStyle = k % 4 ? "#161b22" : "#30363d"; g.fillRect(gx + k * cw, 0, 1, H); }
  if (sqPlaying && sqPos[selT] >= 0 && sqPos[selT] < q.steps) { g.fillStyle = "rgba(34, 211, 238, .14)"; g.fillRect(gx + sqPos[selT] * cw, 0, cw, H); }
  q.st.slice(0, q.steps).forEach((s, k) => {
    if (!s.note || s.note < rollLo || s.note >= rollLo + ROWS) return;
    const y = (rollLo + ROWS - 1 - s.note) * rh, w = Math.min(s.len, q.steps - k) * cw;
    g.fillStyle = s.chord ? "#f472b6" : "#a78bfa", g.globalAlpha = 0.25 + 0.75 * s.chance / 100;
    g.fillRect(gx + k * cw + 1, y + 1, w - 2, rh - 2);
    g.globalAlpha = 1;
    if (k === selStep) { g.strokeStyle = "#22d3ee", g.lineWidth = 2, g.strokeRect(gx + k * cw + 1, y + 1, w - 2, rh - 2); }
  });
}
{
  const cv = $("roll");
  let drawing = false, lastK = -1;
  const at = (e) => {
    const b = cv.getBoundingClientRect(), q = seqs[selT], x = (e.clientX - b.left) * cv.width / b.width, y = (e.clientY - b.top) * cv.height / b.height;
    const k = Math.floor((x - 36) / ((cv.width - 36) / q.steps)), n = rollLo + ROWS - 1 - Math.floor(y / (cv.height / ROWS));
    return k >= 0 && k < q.steps ? [k, clamp(n, 1, 127)] : null;
  };
  cv.addEventListener("pointerdown", (e) => {
    const p = at(e);
    if (!p) return;
    cv.setPointerCapture(e.pointerId);
    const [k, n] = p, s = seqs[selT].st[k];
    if (s.note === n) s.note = 0, s.chord = 0; else { s.note = n, lastNote = n; drawing = true; }
    selStep = k, lastK = k;
    seqChanged(selT, false), drawRoll(), drawStepEditor(), drawStepList();
  });
  cv.addEventListener("pointermove", (e) => {
    if (!drawing) return;
    const p = at(e);
    if (!p || p[0] === lastK) return;
    const [k, n] = p;
    seqs[selT].st[k].note = n, lastK = k, selStep = k;
    seqChanged(selT, false), drawRoll();
  });
  const end = () => { if (drawing) { drawing = false; drawStepEditor(), drawStepList(); } };
  cv.addEventListener("pointerup", end), cv.addEventListener("pointercancel", end);
  cv.addEventListener("wheel", (e) => { e.preventDefault(); rollLo = clamp(rollLo + (e.deltaY > 0 ? -2 : 2), 0, 127 - ROWS); drawRoll(); }, { passive: false });
}
$("b-vgrid").onclick = () => { view = "grid"; drawSteps(); };
$("b-vroll").onclick = () => { view = "roll"; rollRange(); drawSteps(); };
$("b-sqplay").onclick = async () => {
  const on = !sqPlaying;
  if (remotePlay()) {
    try { const r = await conn.client.play(on); sqPlaying = r.playing || on; } catch (e) { say(`PLAY: ${errText(e)}`, "err"); return; }
  } else {
    await startEngine();
    engSend([...HEAD, CMD.SEQ, 3, 0, on ? 1 : 0]);
    sqPlaying = on;
  }
  if (!on) sqPos = [-1, -1, -1, -1];
  drawPlayhead();
};
$("b-sqreload").onclick = () => {
  if (!conn || !conn.seq) { say("RELOAD: CONNECT FIRST (THE SEQUENCES ARE READ FROM THE FM-1)", "err"); return; }
  job("RELOAD", async () => { await readSeqs(); drawSeq(); return "THE FOUR SEQUENCES READ FROM THE FM-1"; });
};
$("b-sqclear").onclick = () => {
  if (!confirm(`CLEAR T${selT + 1}'S SEQUENCE? (ALL 64 STEPS OFF; THE FM-1'S TOO WHEN CONNECTED)`)) return;
  seqs[selT].st = Array.from({ length: SEQ_N }, emptyStep);
  seqChanged();
  say(`T${selT + 1}: SEQUENCE CLEARED`, "ok");
};
const setLen = (n) => { seqs[selT].steps = clamp(n, 1, SEQ_N); seqChanged(); drawSeqPattern(); };
$("b-sqless").onclick = () => setLen(seqs[selT].steps - 1);
$("b-sqmore").onclick = () => setLen(seqs[selT].steps + 1);
$("sq-len").addEventListener("change", () => setLen(+$("sq-len").value | 0));
$("b-mexport").onclick = () => {
  download(`arctic-scream-T${selT + 1}-${stamp()}.mid`, seqToMidi(seqs[selT], bpm, `${NAME} T${selT + 1}`), "audio/midi");
  say(`T${selT + 1}'S SEQUENCE EXPORTED AS A MIDI FILE (${bpm} BPM)`, "ok");
};
$("b-mimport").onclick = () => $("f-midi").click();
$("f-midi").onchange = async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  try {
    const q = midiToSeq(await f.arrayBuffer(), seqs[selT]);
    seqs[selT] = q, selStep = 0;
    rollRange();
    seqChanged(), drawSeqPattern();
    say(`${f.name.toUpperCase()} ON T${selT + 1}: ${q.st.filter((s) => s.note).length} NOTES, ${q.steps} STEPS OF ${SEQ_DIV[q.div]}`, "ok");
  } catch (err) { say(`IMPORT MIDI: ${errText(err)}`, "err"); }
};

// ------------------------------------------------------------------ play: the engine's keys (the selected track)
let keyMask = 0;
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26], BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
const NOTE = ["F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E"];
const KEYMAP = {};
"zxcvbnm,./".split("").forEach((c, i) => { KEYMAP[c] = WHITE[i]; });
"sdfhjl;'".split("").forEach((c, i) => { KEYMAP[c] = [1, 3, 5, 8, 10, 13, 15, 17][i]; });
"qwertyuio".split("").forEach((c, i) => { KEYMAP[c] = 18 + i; });
const keyEls = [];
function drawKeys() {
  const k = $("kbd"), ww = 100 / WHITE.length;
  WHITE.forEach((key, i) => {
    const e = el("div", { cls: "w" }, k);
    Object.assign(e.style, { left: `${i * ww}%`, width: `${ww}%` });
    if (NOTE[key % 12] === "C") el("span", {}, e, "C" + (3 + Math.floor((key + 5) / 12)));
    keyEls[key] = e;
  });
  BLACK.forEach((key, i) => {
    const e = el("div", { cls: "b" }, k);
    Object.assign(e.style, { left: `${(BLACK_AFTER[i] + 0.68) * ww}%`, width: `${ww * 0.64}%` });
    keyEls[key] = e;
  });
  keyEls.forEach((e, key) => {
    e.addEventListener("pointerdown", (ev) => { e.setPointerCapture(ev.pointerId); press(key, true); });
    e.addEventListener("pointerup", () => press(key, false));
    e.addEventListener("pointercancel", () => press(key, false));
  });
}
function press(key, down) {
  keyMask = down ? keyMask | 1 << key : keyMask & ~(1 << key);
  keyEls[key].classList.toggle("down", down);
  if (node) node.port.postMessage({ type: "keys", mask: keyMask >>> 0 });
}
addEventListener("keydown", (e) => {
  const k = KEYMAP[e.key.toLowerCase()];
  if (k === undefined || e.repeat || e.ctrlKey || e.metaKey || e.altKey || page !== "editor" ||
      /INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName) && document.activeElement.type !== "range") return;
  e.preventDefault();
  press(k, true);
});
addEventListener("keyup", (e) => { const k = KEYMAP[e.key.toLowerCase()]; if (k !== undefined && keyMask >> k & 1) press(k, false); });
$("b-panic").onclick = () => { if (node) node.port.postMessage({ type: "panic" }); say("ALL NOTES OFF (HERE)", "ok"); };

// ------------------------------------------------------------------ samples
let smpInfo = [];
async function sampleInfo() {
  if (!conn || busy) return;
  try { smpInfo = await conn.client.smpInfo(); } catch (e) { say(`SAMPLES: ${errText(e)}`, "err"); return; }
  drawSamples();
}
function drawSamples() {
  const box = $("smp");
  box.textContent = "";
  if (!conn) { el("p", { cls: "note" }, box, "Connect first (connect FM-1, or try)."); return; }
  smpInfo.forEach((s, k) => {
    const cd = el("div", { cls: "card", style: "--c:var(--sky)" }, box), d = el("div", { cls: "smp" }, cd);
    el("div", { cls: "big" }, d, String(k + 1));
    const r = el("div", {}, d);
    el("div", {}, r, s.state === 1 ? `${s.name || "(NO NAME)"} · ${(s.n / 22050).toFixed(2)} S` +
      (s.root !== null ? ` · ROOT ${noteName(s.root)}` : "") : s.state === 2 ? "BEING CHECKED…" :
      s.state < 0 ? "IN USE (ITS NAME DID NOT FIT IN THE FM-1'S ANSWER: AN OLDER FIRMWARE)" : "EMPTY");
    const drop = el("div", { cls: "drop" }, r, "DROP A WAV HERE, OR CLICK");
    const file = el("input", { type: "file", accept: ".wav,audio/wav" }, r);
    file.hidden = true;
    const row = el("div", { cls: "row" }, r);
    el("label", { cls: "k" }, row, "name");
    const nm = el("input", { type: "text", maxlength: 8, size: 9 }, row);   // (8: the slot's, what the FM-1 shows)
    nm.value = s.state === 1 ? s.name : "";
    nm.addEventListener("input", () => { nm.value = nm.value.toUpperCase().replace(/[^A-Z0-9 _\-.#!+]/g, "").slice(0, 8); });
    el("label", { cls: "k" }, row, "root");
    const root = el("select", {}, row);                      // (the note the sample plays at its own pitch)
    for (let n = 0; n < 128; n++) el("option", { value: n }, root, `${noteName(n)} (${n})`);
    root.value = s.state === 1 && s.root !== null ? s.root : 60;
    const up = el("button", { cls: "go" }, row, "UPLOAD"), er = el("button", {}, row, "ERASE");
    let buf = null;
    const take = async (f) => {
      if (!f) return;
      buf = await f.arrayBuffer();
      const stem = f.name.replace(/\.[^.]*$/, "");
      nm.value = sampleName(stem), root.value = noteFromName(stem, +root.value);
      drop.textContent = f.name.toUpperCase();
    };
    drop.onclick = () => file.click();
    file.onchange = () => take(file.files[0]);
    drop.ondragover = (e) => { e.preventDefault(); drop.classList.add("hot"); };
    drop.ondragleave = () => drop.classList.remove("hot");
    drop.ondrop = (e) => { e.preventDefault(); drop.classList.remove("hot"); take(e.dataTransfer.files[0]); };
    up.onclick = () => job(`UPLOAD SLOT ${k + 1}`, async () => {
      if (!buf) throw new EditorError("CHOOSE A WAV FIRST", 0);
      const sl = makeSlot(buf, sampleName(nm.value), Math.max(0, Math.min(127, +root.value | 0)));
      say(`UPLOADING ${(sl.data.length / 1024).toFixed(0)} KB…`);
      smpInfo = await conn.client.smpUpload(k, sl.hdr, sl.data, progress);
      drawSamples();
      if (smpInfo[k].state === 0 || smpInfo[k].state === 2) throw new EditorError("THE FM-1 DID NOT ACCEPT IT: UPLOAD AGAIN", 0);
      return `SLOT ${k + 1}: ${smpInfo[k].name || sampleName(nm.value)}, ${sl.secs.toFixed(2)} S${sl.cut ? " (CUT TO 6.64 S)" : ""}, CHECKED BY THE FM-1`;
    });
    er.onclick = () => {
      if (!confirm(`ERASE SAMPLE SLOT ${k + 1} ON THE FM-1?`)) return;
      job(`ERASE SLOT ${k + 1}`, async () => { await conn.client.smpErase(k); smpInfo = await conn.client.smpInfo(); drawSamples(); return `SLOT ${k + 1} ERASED`; });
    };
  });
  refreshButtons();
}
$("b-sinfo").onclick = sampleInfo;
$("b-sfactory").onclick = () => {                            // CLEAR (the factory state: Arctic Scream has no samples built in)
  if (!confirm("CLEAR ALL SAMPLES?\n\nThis EMPTIES all four sample slots on the FM-1. " +
               "Your own samples in them are gone: keep their WAV files.")) return;
  job("CLEAR ALL SAMPLES", async () => {
    say("EMPTYING THE FOUR SAMPLE SLOTS (ABOUT 5 S)…");
    try { smpInfo = await conn.client.smpFactory(); } catch (e) {
      if (e.rc === 97) throw new EditorError("NO ANSWER: THE FM-1'S FIRMWARE IS TOO OLD FOR THIS", 97);
      throw e;
    }
    drawSamples();
    return "ALL FOUR SAMPLE SLOTS EMPTY";
  });
};

// ------------------------------------------------------------------ backup
$("b-backup").onclick = () => job("BACKUP", async () => {
  const b = await makeBackup(conn.client, conn.info, (i, n, what) => { progress(i, n); say(`READING ${what.toUpperCase()}…`); });
  download(`arctic-scream-backup-${stamp()}.json`, JSON.stringify(b, null, 1));
  return "BACKUP MADE: ALL 64 SOUNDS, SLOTS A–D, THE SETTINGS (DOWNLOADED)";
});
$("b-restore").onclick = () => $("f-restore").click();
$("f-restore").onchange = async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  let objs;
  try { objs = checkBackup(JSON.parse(await f.text()), conn.info); } catch (err) { say(`RESTORE: ${errText(err)} (NOTHING WRITTEN)`, "err"); return; }
  if (!confirm(`RESTORE ${f.name}?\n\nThis REPLACES all 64 user sounds, slots A–D and the settings on the FM-1. A backup of what it holds now is made (downloaded) first.`)) return;
  await job("RESTORE", async () => {
    const before = await makeBackup(conn.client, conn.info, (i, n) => { progress(i, n); say("FIRST, A BACKUP OF WHAT THE FM-1 HOLDS NOW…"); });
    download(`arctic-scream-backup-before-restore-${stamp()}.json`, JSON.stringify(before, null, 1));
    await restoreBackup(conn.client, objs, conn.info, (i, n, what) => { progress(i, n); say(`WRITING ${what.toUpperCase()}…`); });
    for (let i = 0; i < objs.length; i++) {                  // read back: as written
      const d = await conn.client.get(objs[i].id, objs[i].data.length);
      if (d.some((x, k) => x !== objs[i].data[k])) throw new EditorError(`${objs[i].what.toUpperCase()} DID NOT READ BACK AS WRITTEN`, 0);
      progress(i + 1, objs.length);
    }
    for (const o of objs) if (o.id >= OBJ.SND && o.id < OBJ.SND + 64) { user[o.id - OBJ.SND] = o.data; dirty[o.id - OBJ.SND] = false; }
    drawSlots();
    return "RESTORED AND READ BACK: EVERYTHING AS IN THE FILE (THE BACKUP FROM BEFORE IS DOWNLOADED)";
  });
};

// ------------------------------------------------------------------ start: the four tracks and sequences as the firmware starts
(async () => {
  try {
    wasm = await (await fetch("grimm.wasm")).arrayBuffer();
    vf = await VirtualFM1.create(wasm.slice(0));
    vf.log = midiLog;
    vfc = new Client(vf);
    meta = vf.meta();
    for (let t = 0; t < 4; t++) {
      const r = await vfc.track(t, true);
      tracks[t] = fromDevice(r.rec, t), label[t] = r.name, labelEng[t] = tracks[t].eng;
      loaded[t] = JSON.stringify(tracks[t]), from[t] = "as Arctic Scream starts";
      resetHistory(t);
      if (meta.proto >= 3) { const q = await vfc.seq(t, true); seqs[t] = parseSeq(q.bytes), bpm = q.bpm; }
    }
  } catch (e) {
    say(`COULD NOT LOAD ARCTIC SCREAM (grimm.wasm): ${e.message || e}. OPEN THIS PAGE FROM A WEB SERVER, NOT A FILE.`.toUpperCase(), "err");
    return;
  }
  screenText(["CLICK ANYWHERE:", "ARCTIC SCREAM'S", "SOUND AND SCREEN"]);
  startEngine();
  drawKeys();
  drawSlots();
  fetch("demos.json").then((r) => r.json()).then((l) => { songs = l; drawSongs(); }).catch(() => { $("songs").textContent = "(the demo songs could not be loaded)"; });
  drawTracks();
  drawEditor();
  drawSamples();
  drawSeqTracks();
  fill($("t-morph")), fill($("t-ramount"));
  setConn(null);
  say("READY: CONNECT THE FM-1, OR TRY WITHOUT ONE. THE EDITOR WORKS HERE ALREADY.");
  const q = new URLSearchParams(location.search), h = location.hash.slice(1);
  show(h || (q.has("fact") ? "editor" : "connect"));
  if (q.has("fact")) { pick = { fact: true, i: +q.get("fact") || 0 }; drawSlots(); loadTo(0); }
  if (q.has("try")) await tryVirtual();
  if (h) show(h);
  if (h === "samples") await sampleInfo();
})();
