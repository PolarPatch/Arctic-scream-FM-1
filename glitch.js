// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 PolarPatch
// Arctic Scream p0larpatch: the glitching titles (.gl, data-t) of the demo page and the cheat sheet.

const still = matchMedia("(prefers-reduced-motion: reduce)").matches;

// ------------------------------------------------------------------ the glitching titles
// Each title gets three ghosts of its text (ice, plasma, violet). Now and then a ghost is torn into view in a thin
// band, a few at once when a title scrolls in; the ghosts lean toward the pointer. After the CONFIG title of
// PolarPatch's BlueWatch; the timing here is random, its own.
const titles = [...document.querySelectorAll(".gl")];
for (const t of titles) {
  for (let i = 0; i < 3; i++) {
    const g = document.createElement("i");
    g.textContent = t.dataset.t;
    g.setAttribute("aria-hidden", "true");
    t.appendChild(g);
  }
}
const band = (g, on) => {
  if (!on) { g.style.clipPath = "inset(100% 0 0 0)"; g.style.translate = ""; return; }
  const top = Math.random() * 82, h = 4 + Math.random() * 18, dx = (Math.random() < 0.5 ? -1 : 1) * (0.03 + Math.random() * 0.09);
  g.style.translate = `${dx.toFixed(3)}em 0`;               // (torn sideways too, a few pixels to a tenth of a letter)
  g.style.clipPath = `inset(${top.toFixed(1)}% 0 ${Math.max(0, 100 - top - h).toFixed(1)}% 0)`;
};
function tear(t, n = 1) {                                  // n tears of a random ghost, each 40-140 ms
  const gs = t.querySelectorAll("i");
  let k = 0;
  const one = () => {
    const g = gs[Math.floor(Math.random() * gs.length)];
    band(g, true);
    setTimeout(() => { band(g, false); if (++k < n) setTimeout(one, 20 + Math.random() * 80); }, 40 + Math.random() * 100);
  };
  one();
}
if (!still) {
  const seen = new Set();
  const io = new IntersectionObserver((es) => {
    for (const e of es) {
      if (e.isIntersecting) { seen.add(e.target); tear(e.target, 5); } else seen.delete(e.target);
    }
  }, { threshold: 0.6 });
  titles.forEach((t) => io.observe(t));
  const idle = () => {                                     // the titles in view, now and then
    for (const t of seen) if (Math.random() < 0.8) tear(t, 2 + Math.floor(Math.random() * 4));
    setTimeout(idle, 250 + Math.random() * 900);
  };
  idle();
  if (matchMedia("(hover: hover)").matches) {
    let raf = 0;
    addEventListener("pointermove", (e) => {
      if (raf) return;
      raf = requestAnimationFrame(() => {
        raf = 0;
        for (const t of seen) {
          const r = t.getBoundingClientRect(), dx = e.clientX - (r.left + r.width / 2), dy = e.clientY - (r.top + r.height / 2);
          const d = Math.hypot(dx, dy) || 1, k = 14 * Math.max(0, 1 - d / 700) ** 2;
          t.style.setProperty("--gx", (dx / d * k).toFixed(2) + "px");
          t.style.setProperty("--gy", (dy / d * k * 0.5).toFixed(2) + "px");
        }
      });
    }, { passive: true });
  }
}
