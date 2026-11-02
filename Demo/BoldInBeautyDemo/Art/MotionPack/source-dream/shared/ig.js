/* ig.js: shared motion helpers for the BoldInBeauty in-game motion pack.
   Everything is deterministic: no Math.random, no Date. All motion lives on one paused GSAP timeline (tl).
   Compositions are authored at 1920x1080 and rendered at 4K (deviceScaleFactor 2); canvases are created at 2x. */
(function (root) {
  var C = { bk: "#090B12", st: "#151925", rose: "#FF3D8D", vio: "#7657FF", cy: "#64E8FF", wh: "#F7F4FA" };
  function hash(i, s) {
    var x = Math.imul((i | 0) + 0x9E3779B9, 0x85EBCA6B) ^ Math.imul((s | 0) + 0x27D4EB2F, 0xC2B2AE35);
    x ^= x >>> 15; x = Math.imul(x, 0x2C1B3C6D); x ^= x >>> 12; x = Math.imul(x, 0x297A2D39); x ^= x >>> 15;
    return (x >>> 0) / 4294967296;
  }
  function rng(seed) { var i = 0; return function () { return hash(i++, seed); }; }
  function clamp(x, a, b) { return x < a ? a : x > b ? b : x; }
  function lerp(a, b, t) { return a + (b - a) * t; }
  function inv(a, b, x) { return clamp((x - a) / (b - a), 0, 1); }
  function oexp(t) { return t >= 1 ? 1 : 1 - Math.pow(2, -10 * t); }
  function ocub(t) { return 1 - Math.pow(1 - t, 3); }
  function $(s, p) { return typeof s === "string" ? (p || document).querySelector(s) : s; }
  function $$(s, p) { return Array.prototype.slice.call((p || document).querySelectorAll(s)); }
  function el(tag, cls, parent, html, css) {
    var d = document.createElement(tag || "div"); if (cls) d.className = cls; if (html != null) d.innerHTML = html;
    if (css) for (var k in css) d.style[k] = css[k]; if (parent) $(parent).appendChild(d); return d;
  }
  /* hi-dpi canvas: returns ctx in 1920x1080-style CSS units */
  function canvas(id, w, h, parent, css) {
    var c = document.getElementById(id);
    if (!c) { c = document.createElement("canvas"); c.id = id; (parent ? $(parent) : document.getElementById("root")).appendChild(c); }
    w = w || 1920; h = h || 1080; c.width = w * 2; c.height = h * 2; c.style.width = w + "px"; c.style.height = h + "px";
    c.style.position = "absolute"; c.style.left = c.style.left || "0px"; c.style.top = c.style.top || "0px";
    c.setAttribute("data-layout-allow-overflow", ""); c.setAttribute("data-layout-allow-occlusion", "");
    if (css) for (var k in css) c.style[k] = css[k];
    var x = c.getContext("2d"); x.setTransform(2, 0, 0, 2, 0, 0); x.W = w; x.H = h; x.cv = c; return x;
  }
  function clock(tl, dur, fn, at) {
    var o = { t: 0 };
    tl.fromTo(o, { t: 0 }, { t: dur, duration: dur, ease: "none", immediateRender: false, onUpdate: function () { fn(o.t); } }, at || 0);
    try { fn(0); } catch (e) {}
    return o;
  }
  var GLY = "ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝ0123456789<>/\\|+-=#";
  function scramble(tl, e, text, at, dur, seed) {
    e = $(e); var p = { v: 0 }, n = text.length, lead = 4; seed = seed || 3;
    function draw(v) {
      if (v >= 1) { e.textContent = text; return; }
      var front = Math.floor(v * (n + lead)), step = Math.floor(v * dur * 30), out = "";
      for (var i = 0; i < n; i++) {
        var ch = text.charAt(i);
        if (i < front - lead || ch === " ") out += i < front ? ch : "";
        else if (i < front) out += GLY.charAt(Math.floor(hash(i * 131 + step, seed) * GLY.length));
      }
      e.textContent = out;
    }
    tl.fromTo(p, { v: 0 }, { v: 1, duration: dur, ease: "none", immediateRender: false, onUpdate: function () { draw(p.v); } }, at);
    draw(0);
    return p;
  }
  function typeOn(tl, e, text, at, dur, caret) {
    e = $(e); var p = { v: 0 };
    function draw() { var k = Math.round(p.v * text.length); e.textContent = text.slice(0, k) + (caret && p.v < 1 ? "_" : ""); }
    tl.fromTo(p, { v: 0 }, { v: 1, duration: dur, ease: "none", immediateRender: false, onUpdate: draw }, at);
    draw(); return p;
  }
  function count(tl, e, from, to, at, dur, fmt, ease) {
    e = $(e); var p = { v: from }; fmt = fmt || function (v) { return String(Math.round(v)); };
    function draw() { e.textContent = fmt(p.v); }
    tl.fromTo(p, { v: from }, { v: to, duration: dur, ease: ease || "power2.out", immediateRender: false, onUpdate: draw }, at);
    draw(); return p;
  }
  function commas(v) { return String(Math.round(v)).replace(/\B(?=(\d{3})+(?!\d))/g, ","); }
  /* deterministic jitter shake (camera / element) */
  function shake(tl, target, at, dur, amp, seed) {
    seed = seed || 7; var o = { t: 0 };
    tl.fromTo(o, { t: 0 }, { t: 1, duration: dur, ease: "none", immediateRender: false, onUpdate: function () {
      var k = Math.pow(1 - o.t, 2) * amp, f = Math.floor(o.t * dur * 30);
      root.gsap.set(target, { x: (hash(f, seed) - 0.5) * 2 * k, y: (hash(f, seed + 99) - 0.5) * 2 * k });
    } }, at);
    tl.set(target, { x: 0, y: 0 }, at + dur + 0.001);
  }
  /* glitch burst: rapid x / skew / opacity jumps */
  function glitch(tl, target, at, dur, amp, seed) {
    seed = seed || 11; var n = Math.max(2, Math.round(dur * 30));
    for (var i = 0; i < n; i++) {
      var h1 = hash(i, seed), h2 = hash(i, seed + 1), h3 = hash(i, seed + 2);
      tl.set(target, { x: (h1 - 0.5) * 2 * amp, skewX: (h2 - 0.5) * 30, opacity: h3 > 0.85 ? 0.25 : 1 }, at + i / 30);
    }
    tl.set(target, { x: 0, skewX: 0, opacity: 1 }, at + n / 30);
  }
  function slam(tl, e, at, o) {
    o = o || {};
    tl.fromTo(e, { opacity: 0, x: o.x == null ? -120 : o.x, y: o.y || 0, scale: o.s == null ? 1.15 : o.s },
      { opacity: 1, x: 0, y: 0, scale: 1, duration: o.d || 0.32, ease: o.ease || "back.out(1.7)", immediateRender: o.ir == null ? true : o.ir }, at);
  }
  function rise(tl, e, at, d, dy) { tl.fromTo(e, { opacity: 0, y: dy == null ? 26 : dy }, { opacity: 1, y: 0, duration: d || 0.3, ease: "power3.out" }, at); }
  function pop(tl, e, at, d) { tl.fromTo(e, { opacity: 0, scale: 0.3 }, { opacity: 1, scale: 1, duration: d || 0.35, ease: "back.out(2.2)" }, at); }
  function out(tl, e, at, d) { tl.to(e, { opacity: 0, duration: d || 0.15, ease: "power2.in" }, at); }
  function flash(tl, e, at, peak, d) { tl.fromTo(e, { opacity: peak || 0.9 }, { opacity: 0, duration: d || 0.25, ease: "power2.out", immediateRender: false }, at); }
  /* show a scene container only between a and b */
  function span(tl, e, a, b) { if (a > 0) { root.gsap.set(e, { autoAlpha: 0 }); tl.set(e, { autoAlpha: 1 }, a); } if (b != null) tl.set(e, { autoAlpha: 0 }, b); }
  /* split text into letter spans */
  function letters(e) {
    e = $(e); var t = e.textContent; e.textContent = "";
    return t.split("").map(function (ch) { return el("span", "lt", e, ch === " " ? "&nbsp;" : ch, { display: "inline-block" }); });
  }
  /* rain streaks on a ctx at time t */
  function rain(x, t, o) {
    o = o || {}; var N = o.n || 140, W = x.W, H = x.H, sd = o.seed || 1, ang = o.ang == null ? 0.21 : o.ang;
    x.lineCap = "round";
    for (var i = 0; i < N; i++) {
      var sp = (o.sp || 900) + 700 * hash(i, sd), x0 = hash(i, sd + 2) * (W + 400), y0 = hash(i, sd + 3) * (H + 300);
      var y = ((y0 + sp * t) % (H + 300)) - 150, xx = x0 - ang * (y + 150);
      var len = 40 + 70 * hash(i, 4), a = (o.a || 1) * (0.10 + 0.22 * hash(i, 5));
      x.strokeStyle = hash(i, 6) > 0.82 ? "rgba(255,61,141," + a.toFixed(3) + ")" : "rgba(100,232,255," + (a * 0.8).toFixed(3) + ")";
      x.lineWidth = 1.2 + 1.4 * hash(i, 7);
      x.beginPath(); x.moveTo(xx, y); x.lineTo(xx - len * ang, y + len); x.stroke();
    }
  }
  /* film grain / static: deterministic per frame */
  function grain(x, t, amt, seed) {
    var f = Math.floor(t * 30), W = x.W, H = x.H; amt = amt || 0.06;
    for (var i = 0; i < 900; i++) {
      var a = hash(i + f * 977, seed || 5);
      x.fillStyle = a > 0.5 ? "rgba(247,244,250," + (amt * hash(i, f + 3)).toFixed(3) + ")" : "rgba(9,11,18," + (amt * 1.4).toFixed(3) + ")";
      x.fillRect(hash(i, f * 3 + 1) * W, hash(i, f * 3 + 2) * H, 2 + 3 * hash(i, f), 2);
    }
  }
  /* speed lines radiating from (cx,cy) */
  function speed(x, t, o) {
    o = o || {}; var cx = o.cx == null ? x.W / 2 : o.cx, cy = o.cy == null ? x.H / 2 : o.cy, N = o.n || 90, f = Math.floor(t * 30);
    for (var i = 0; i < N; i++) {
      var a = hash(i, f + (o.seed || 1)) * Math.PI * 2, r0 = (o.r0 || 380) + 200 * hash(i, f + 7), len = 300 + 900 * hash(i, f + 9);
      x.strokeStyle = o.col || "rgba(247,244,250,.55)"; x.lineWidth = 1 + 5 * hash(i, f + 11);
      x.beginPath(); x.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); x.lineTo(cx + Math.cos(a) * (r0 + len), cy + Math.sin(a) * (r0 + len)); x.stroke();
    }
  }
  /* corner brackets inside a parent */
  function brackets(parent, inset, size, col, w) {
    inset = inset == null ? 28 : inset; size = size || 56; col = col || C.cy; w = w || 3;
    var pos = [["left", "top"], ["right", "top"], ["left", "bottom"], ["right", "bottom"]];
    return pos.map(function (p) {
      var b = el("div", "ig-brk", parent, null, { position: "absolute", width: size + "px", height: size + "px" });
      b.style[p[0]] = inset + "px"; b.style[p[1]] = inset + "px";
      b.style["border" + p[0][0].toUpperCase() + p[0].slice(1)] = w + "px solid " + col;
      b.style["border" + p[1][0].toUpperCase() + p[1].slice(1)] = w + "px solid " + col;
      return b;
    });
  }
  /* rose tube ring svg (brand symbol). returns {svg, circle, C} */
  function ring(parent, r, sw, col, css) {
    var ns = "http://www.w3.org/2000/svg", s = document.createElementNS(ns, "svg"), R = r + sw * 2;
    s.setAttribute("viewBox", (-R) + " " + (-R) + " " + 2 * R + " " + 2 * R); s.setAttribute("width", 2 * R); s.setAttribute("height", 2 * R);
    s.style.position = "absolute"; s.style.overflow = "visible"; if (css) for (var k in css) s.style[k] = css[k];
    var g = document.createElementNS(ns, "circle"); g.setAttribute("r", r); g.setAttribute("fill", "none"); g.setAttribute("stroke", col || C.rose);
    g.setAttribute("stroke-width", sw); g.setAttribute("stroke-linecap", "round"); g.setAttribute("transform", "rotate(-90)");
    g.style.filter = "drop-shadow(0 0 " + sw * 1.5 + "px " + (col || C.rose) + ")";
    var L = 2 * Math.PI * r; g.setAttribute("stroke-dasharray", L + " " + L); s.appendChild(g); $(parent).appendChild(s);
    return { svg: s, circle: g, L: L };
  }
  function drawRing(tl, rg, at, d, from, to) {
    tl.fromTo(rg.circle, { attr: { "stroke-dashoffset": rg.L * (1 - (from || 0)) } }, { attr: { "stroke-dashoffset": rg.L * (1 - (to == null ? 1 : to)) }, duration: d || 0.6, ease: "power3.out", immediateRender: from != null }, at);
  }
  /* seek-safe text schedule: keys = [[time, text], ...] sorted */
  function textAt(tl, e, keys, total) {
    e = $(e); var last = null;
    clock(tl, total, function (t) { var v = ""; for (var i = 0; i < keys.length; i++) if (t >= keys[i][0]) v = keys[i][1]; if (v !== last) { e.textContent = v; last = v; } }, 0);
  }
  /* keep the timeline exactly DUR long */
  function seal(tl, dur) { tl.set({}, {}, dur); }
  root.IG = { C: C, hash: hash, rng: rng, clamp: clamp, lerp: lerp, inv: inv, oexp: oexp, ocub: ocub, $: $, $$: $$, el: el, canvas: canvas, clock: clock,
    scramble: scramble, typeOn: typeOn, count: count, commas: commas, shake: shake, glitch: glitch, slam: slam, rise: rise, pop: pop, out: out, flash: flash,
    span: span, letters: letters, rain: rain, grain: grain, speed: speed, brackets: brackets, ring: ring, drawRing: drawRing, seal: seal, textAt: textAt, sub: function () { return root.gsap.timeline(); } };
})(window);
