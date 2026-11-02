/* fx1.js: canvas VFX engine for the bb-Showreel Set 2 (ability / status / combat effects).
   Pure functions of time (seek-safe, no Math.random). A piece = list of layers; each layer = recipe + params.
   Effects are drawn relative to a world centre C=(960,640); caster sits at CAS, enemy at EN. No figures are drawn:
   effects float on their own, and the sheet videos add a small anchor reticle + a wireframe angel dummy for scale. */
(function (root) {
  var H = IG.hash, TAU = Math.PI * 2, PI = Math.PI, cos = Math.cos, sin = Math.sin;
  var FX = root.FX = {}, R = FX.R = {};
  var W = "#F7F4FA", CY = "#64E8FF", RO = "#FF3D8D", VI = "#7657FF", BK = "#090B12";
  var POS = { cas: [-340, 60], en: [340, 60], mid: [0, 60], top: [0, -330] };
  FX.POS = POS;
  function cl(v, a, b) { return v < a ? a : v > b ? b : v; }
  function inv(a, b, v) { return cl((v - a) / (b - a), 0, 1); }
  function lerp(a, b, u) { return a + (b - a) * u; }
  function eo(u) { return 1 - Math.pow(1 - u, 3); }
  function eo4(u) { return 1 - Math.pow(1 - u, 4); }
  function eb(u) { var c = 1.8; return 1 + (c + 1) * Math.pow(u - 1, 3) + c * Math.pow(u - 1, 2); }
  var rc = {};
  function rgba(h, a) { var c = rc[h]; if (!c) { var n = parseInt(h.slice(1), 16); c = rc[h] = [(n >> 16) & 255, (n >> 8) & 255, n & 255]; } return "rgba(" + c[0] + "," + c[1] + "," + c[2] + "," + cl(a, 0, 1).toFixed(3) + ")"; }
  function glow(x, px, py, r, col, a) { if (r <= 1 || a <= 0) return; var g = x.createRadialGradient(px, py, 0, px, py, r); g.addColorStop(0, rgba(col, a)); g.addColorStop(1, rgba(col, 0)); x.fillStyle = g; x.fillRect(px - r, py - r, r * 2, r * 2); }
  function disc(x, px, py, r, col, a) { if (r <= 0) return; x.fillStyle = rgba(col, a == null ? 1 : a); x.beginPath(); x.arc(px, py, r, 0, TAU); x.fill(); }
  function circ(x, px, py, r, col, w, a) { if (r <= 0) return; x.strokeStyle = rgba(col, a == null ? 1 : a); x.lineWidth = w; x.beginPath(); x.arc(px, py, r, 0, TAU); x.stroke(); }
  function ell(x, px, py, rx, ry, col, a, fill, w) { if (rx <= 0 || ry <= 0) return; x.beginPath(); x.ellipse(px, py, rx, ry, 0, 0, TAU); if (fill) { x.fillStyle = rgba(col, a); x.fill(); } else { x.strokeStyle = rgba(col, a); x.lineWidth = w || 2; x.stroke(); } }
  function line(x, ax, ay, bx, by, col, w, a) { x.strokeStyle = rgba(col, a == null ? 1 : a); x.lineWidth = w; x.lineCap = "round"; x.beginPath(); x.moveTo(ax, ay); x.lineTo(bx, by); x.stroke(); }
  function poly(x, pts, col, a, stroke, w) { x.beginPath(); pts.forEach(function (p, i) { if (i) x.lineTo(p[0], p[1]); else x.moveTo(p[0], p[1]); }); x.closePath(); if (stroke) { x.strokeStyle = rgba(col, a); x.lineWidth = w || 2; x.stroke(); } else { x.fillStyle = rgba(col, a); x.fill(); } }
  function hexp(cx, cy, r, rot) { var p = []; for (var i = 0; i < 6; i++) p.push([cx + cos(rot + i * PI / 3) * r, cy + sin(rot + i * PI / 3) * r]); return p; }
  function ringPulse(x, ox, oy, tt, dur, r0, r1, col, w, flat) {
    var u = inv(0, dur, tt); if (tt < 0 || u >= 1) return; var rr = lerp(r0, r1, eo(u)), a = 1 - u;
    if (flat) ell(x, ox, oy, rr, rr * 0.3, col, a, false, w * a + 1); else circ(x, ox, oy, rr, col, w * a + 1, a);
  }
  function parts(x, t, o) {
    var n = o.n || 20, life = o.life || 0.8, s = o.seed || 1, t0 = o.t0 || 0, sp = o.spread || 0;
    for (var i = 0; i < n; i++) {
      var u;
      if (o.loop) u = (((t - t0) / life + i / n + H(i, s + 9)) % 1 + 1) % 1; else { u = (t - t0 - H(i, s) * sp) / life; if (u < 0 || u > 1) continue; }
      var a = lerp(o.a0 == null ? 0 : o.a0, o.a1 == null ? TAU : o.a1, H(i, s + 1)), v = (o.spd || 300) * (0.35 + 0.65 * H(i, s + 2)), tm = u * life;
      var d = v * life * (eo(u) * 0.6 + u * 0.4);
      var px = (o.ox || 0) + cos(a) * d + (o.wx ? (H(i, s + 3) - 0.5) * o.wx : 0), py = (o.oy || 0) + sin(a) * d + (o.g || 0) * 0.5 * tm * tm + (o.wy ? (H(i, s + 4) - 0.5) * o.wy : 0);
      var al = (o.fade === "in" ? Math.sin(u * PI) : 1 - u) * (o.al == null ? 1 : o.al), sz = (o.size || 6) * (o.shrink === false ? 1 : (1 - u * 0.7)) * (0.6 + 0.8 * H(i, s + 5));
      var col = o.cols ? o.cols[Math.floor(H(i, s + 6) * o.cols.length)] : (o.col || W), sh = o.shape;
      if (sh === "line") { var ln = sz * (o.len || 4); line(x, px, py, px - cos(a) * ln, py - sin(a) * ln, col, Math.max(1.5, sz * 0.4), al); }
      else if (sh === "sq") { x.save(); x.translate(px, py); x.rotate(H(i, s + 7) * TAU + u * 3); x.fillStyle = rgba(col, al); x.fillRect(-sz / 2, -sz / 2, sz, sz); x.restore(); }
      else if (sh === "dia") poly(x, [[px, py - sz], [px + sz * 0.7, py], [px, py + sz], [px - sz * 0.7, py]], col, al);
      else if (sh === "ring") circ(x, px, py, sz, col, 2, al);
      else disc(x, px, py, sz, col, al);
    }
  }
  function cres(x, cx, cy, r, a0, a1, w, col, al) {
    if (a1 - a0 < 0.02 || al <= 0) return; var n = 22, up = [], dn = [];
    for (var i = 0; i <= n; i++) { var f = i / n, a = lerp(a0, a1, f), th = w * Math.sin(f * PI); up.push([cx + cos(a) * (r + th / 2), cy + sin(a) * (r + th / 2)]); dn.push([cx + cos(a) * (r - th / 2), cy + sin(a) * (r - th / 2)]); }
    dn.reverse(); poly(x, up.concat(dn), col, al);
  }
  function bolt(x, ax, ay, bx, by, seed, col, w, al, jag) {
    var n = 12, pts = [[ax, ay]], dx = bx - ax, dy = by - ay, L = Math.hypot(dx, dy) || 1, nx = -dy / L, ny = dx / L;
    for (var i = 1; i < n; i++) { var f = i / n, o = (H(i, seed) - 0.5) * 2 * (jag || 40) * Math.sin(f * PI); pts.push([ax + dx * f + nx * o, ay + dy * f + ny * o]); }
    pts.push([bx, by]);
    [[w * 2.6, col, al * 0.3], [w, col, al * 0.8], [w * 0.4, W, al]].forEach(function (s) { x.strokeStyle = rgba(s[1], s[2]); x.lineWidth = s[0]; x.lineJoin = "round"; x.lineCap = "round"; x.beginPath(); pts.forEach(function (p, i) { if (i) x.lineTo(p[0], p[1]); else x.moveTo(p[0], p[1]); }); x.stroke(); });
  }
  function glyph(x, g, s, col, a) {
    x.save(); x.strokeStyle = rgba(col, a); x.fillStyle = rgba(col, a); x.lineWidth = Math.max(3, s * 0.12); x.lineCap = "round"; x.lineJoin = "round";
    if (g === "cross") { x.beginPath(); x.moveTo(-s, -s); x.lineTo(s, s); x.moveTo(s, -s); x.lineTo(-s, s); x.stroke(); }
    else if (g === "crosshair") { circ(x, 0, 0, s * 0.7, col, x.lineWidth, a); x.beginPath(); x.moveTo(-s * 1.2, 0); x.lineTo(-s * 0.35, 0); x.moveTo(s * 1.2, 0); x.lineTo(s * 0.35, 0); x.moveTo(0, -s * 1.2); x.lineTo(0, -s * 0.35); x.moveTo(0, s * 1.2); x.lineTo(0, s * 0.35); x.stroke(); }
    else if (g === "skull") { ell(x, 0, -s * 0.15, s * 0.8, s * 0.75, col, a, true); x.fillRect(-s * 0.45, s * 0.35, s * 0.9, s * 0.55); disc(x, -s * 0.32, -s * 0.15, s * 0.2, BK, a); disc(x, s * 0.32, -s * 0.15, s * 0.2, BK, a); }
    else if (g === "gavel") { x.rotate(-0.6); x.fillRect(-s * 0.9, -s * 0.55, s * 1.4, s * 0.8); x.fillRect(s * 0.1, s * 0.2, s * 0.22, s * 1.2); }
    else if (g === "clock") { circ(x, 0, 0, s, col, x.lineWidth, a); x.beginPath(); x.moveTo(0, 0); x.lineTo(0, -s * 0.7); x.moveTo(0, 0); x.lineTo(s * 0.5, s * 0.2); x.stroke(); }
    else if (g === "eye") { x.beginPath(); x.moveTo(-s * 1.2, 0); x.quadraticCurveTo(0, -s * 1.1, s * 1.2, 0); x.quadraticCurveTo(0, s * 1.1, -s * 1.2, 0); x.stroke(); disc(x, 0, 0, s * 0.35, col, a); }
    else if (g === "drop") { x.beginPath(); x.moveTo(0, -s); x.quadraticCurveTo(s * 0.9, s * 0.2, 0, s * 0.9); x.quadraticCurveTo(-s * 0.9, s * 0.2, 0, -s); x.fill(); }
    else if (g === "snow") { x.beginPath(); for (var k = 0; k < 3; k++) { var an = k * PI / 3; x.moveTo(cos(an) * s, sin(an) * s); x.lineTo(-cos(an) * s, -sin(an) * s); } x.stroke(); }
    else if (g === "flame") { x.beginPath(); x.moveTo(0, -s * 1.1); x.quadraticCurveTo(s * 0.9, -s * 0.1, s * 0.5, s * 0.8); x.quadraticCurveTo(0, s * 1.1, -s * 0.5, s * 0.8); x.quadraticCurveTo(-s * 0.9, -s * 0.1, 0, -s * 1.1); x.fill(); }
    else if (g === "bolt") { poly(x, [[s * 0.2, -s * 1.1], [-s * 0.6, s * 0.1], [-s * 0.05, s * 0.1], [-s * 0.3, s * 1.1], [s * 0.7, -s * 0.2], [s * 0.1, -s * 0.2]], col, a); }
    else if (g === "link") { x.strokeRect(-s * 1.0, -s * 0.4, s * 1.2, s * 0.8); x.strokeRect(-s * 0.2, -s * 0.4, s * 1.2, s * 0.8); }
    else if (g === "down") { poly(x, [[-s, -s * 0.5], [s, -s * 0.5], [0, s * 0.8]], col, a); }
    else if (g === "up") { poly(x, [[-s, s * 0.5], [s, s * 0.5], [0, -s * 0.8]], col, a); }
    else if (g === "star") { var p = []; for (var q = 0; q < 10; q++) { var r2 = q % 2 ? s * 0.45 : s; p.push([cos(-PI / 2 + q * PI / 5) * r2, sin(-PI / 2 + q * PI / 5) * r2]); } poly(x, p, col, a); }
    else if (g === "spiral") { x.beginPath(); for (var j = 0; j < 40; j++) { var aa = j * 0.4, rr = s * j / 40; x.lineTo(cos(aa) * rr, sin(aa) * rr); } x.stroke(); }
    else if (g === "heart") { x.beginPath(); x.moveTo(0, s * 0.9); x.bezierCurveTo(-s * 1.4, -s * 0.2, -s * 0.5, -s * 1.1, 0, -s * 0.3); x.bezierCurveTo(s * 0.5, -s * 1.1, s * 1.4, -s * 0.2, 0, s * 0.9); x.fill(); }
    else if (g === "pill") { x.save(); x.rotate(-0.6); ell(x, 0, 0, s, s * 0.45, col, a, true); x.fillStyle = rgba(BK, a * 0.35); x.fillRect(0, -s * 0.45, s, s * 0.9); x.restore(); }
    else if (g === "warn") { poly(x, [[0, -s], [s, s * 0.8], [-s, s * 0.8]], col, a, true, x.lineWidth); x.fillRect(-s * 0.06, -s * 0.3, s * 0.12, s * 0.6); }
    x.restore();
  }
  function ico(x, cx, cy, s, ax, ay, col, lw, a) {
    var PHI = 1.618, V = [[0, 1, PHI], [0, -1, PHI], [0, 1, -PHI], [0, -1, -PHI], [1, PHI, 0], [-1, PHI, 0], [1, -PHI, 0], [-1, -PHI, 0], [PHI, 0, 1], [-PHI, 0, 1], [PHI, 0, -1], [-PHI, 0, -1]];
    var E = []; for (var i = 0; i < 12; i++) for (var j = i + 1; j < 12; j++) { var dx = V[i][0] - V[j][0], dy = V[i][1] - V[j][1], dz = V[i][2] - V[j][2]; if (Math.abs(dx * dx + dy * dy + dz * dz - 4) < 0.01) E.push([i, j]); }
    var P = V.map(function (v) { var X = v[0], Y = v[1], Z = v[2], c = cos(ay), sn = sin(ay), X2 = X * c - Z * sn, Z2 = X * sn + Z * c; c = cos(ax); sn = sin(ax); var Y2 = Y * c - Z2 * sn, Z3 = Y * sn + Z2 * c, k = 4 / (6 + Z3); return [cx + X2 * s * k, cy + Y2 * s * k]; });
    x.strokeStyle = rgba(col, a); x.lineWidth = lw; x.beginPath(); E.forEach(function (e) { x.moveTo(P[e[0]][0], P[e[0]][1]); x.lineTo(P[e[1]][0], P[e[1]][1]); }); x.stroke();
  }
  FX.ico = ico; FX.glyph = glyph; FX.rgba = rgba; FX.glow = glow;

  /* ================= recipes ================= */
  R.strike = function (x, t, P) {
    var n = P.n || 3, gap = P.gap || 0.16, col = P.col || W, col2 = P.col2 || col, len = P.len || 520, w = P.w || 54, base = (P.ang == null ? -12 : P.ang) * PI / 180;
    for (var i = 0; i < n; i++) {
      var tt = t - i * gap; if (tt < 0 || tt > 0.75) continue;
      var u = inv(0, 0.2, tt), head = eo4(u), tail = eo(inv(0.06, 0.5, tt)), fin = P.fin && i === n - 1, dir = i % 2 ? -1 : 1, sc = fin ? 1.35 : 1, r = len * 0.62 * sc, al = 1 - inv(0.3, 0.75, tt);
      x.save(); x.rotate(base + (i - (n - 1) / 2) * (P.fan == null ? 0.22 : P.fan)); x.scale(dir, 1);
      cres(x, 0, r * 0.5, r, -PI / 2 - 1.0 + 2.0 * tail, -PI / 2 - 1.0 + 2.0 * head, w * sc, col, 0.95 * al);
      cres(x, 0, r * 0.5, r * 0.97, -PI / 2 - 1.0 + 2.0 * tail, -PI / 2 - 1.0 + 2.0 * head, w * 0.4 * sc, W, 0.9 * al);
      x.restore();
      parts(x, t, { t0: i * gap + 0.12, n: fin ? 22 : 10, life: 0.5, spd: fin ? 380 : 240, col: col2, shape: "line", len: 3, size: 5, seed: 7 + i, oy: -r * 0.4 });
      if (fin) ringPulse(x, 0, -r * 0.2, tt - 0.1, 0.5, 30, 260, col2, 10, false);
    }
  };
  R.dash = function (x, t, P) {
    var dist = P.dist || 800, col = P.col || W, col2 = P.col2 || col, dd = P.dd || 0.3, u = inv(0, dd, t), head = lerp(-dist / 2, dist / 2, eo(u)), h = P.h || 110, fade = 1 - inv(dd, dd + 0.6, t);
    x.save(); x.scale(P.dir || 1, 1);
    var tl_ = Math.max(-dist / 2 - 40, head - (P.trail || 560)), g = x.createLinearGradient(tl_, 0, head, 0); g.addColorStop(0, rgba(col, 0)); g.addColorStop(1, rgba(col, 0.85 * fade));
    x.fillStyle = g; x.beginPath(); x.moveTo(tl_, 0); x.lineTo(head, -h / 2); x.lineTo(head + 40, 0); x.lineTo(head, h / 2); x.closePath(); x.fill();
    for (var k = 0; k < 8; k++) { var y = (H(k, 3) - 0.5) * h * 1.7, a0 = head - 100 - H(k, 4) * 360; line(x, Math.max(a0, -dist / 2 - 40), y, head - H(k, 5) * 40, y, col2, 2.5, 0.6 * fade); }
    for (k = 0; k < 5; k++) { var px = head - 70 * (k + 1) - 22 * k; if (px < -dist / 2) continue; poly(x, [[px + 16, -h * 0.55], [px + 40, -h * 0.55], [px + 24, h * 0.55], [px, h * 0.55]], col2, 0.28 * fade / (k + 1)); }
    if (P.dust) parts(x, t, { t0: 0, n: 16, loop: false, life: 0.7, spd: 160, a0: -PI * 0.8, a1: -PI * 0.2, ox: -dist / 2, oy: 0, g: 200, size: 8, col: col2, seed: 5, spread: dd });
    x.restore();
  };
  R.blast = function (x, t, P) {
    var r = P.r || 300, col = P.col || W, col2 = P.col2 || col, D = P.D || 0.6, u = inv(0, D, t), nr = P.rings == null ? 2 : P.rings, flat = P.flat;
    if (t < 0 || u >= 1) return;
    for (var i = 0; i < nr; i++) { var uu = inv(i * 0.07, i * 0.07 + D, t), rr = r * eo(uu), a = 1 - uu; if (uu >= 1) continue; if (flat) ell(x, 0, 0, rr, rr * 0.3, i % 2 ? col2 : col, a, false, 22 * a + 2); else circ(x, 0, 0, rr, i % 2 ? col2 : col, 22 * a + 2, a); }
    var nrr = P.rays == null ? 14 : P.rays; for (i = 0; i < nrr; i++) { var an = i / nrr * TAU + 0.2, l0 = r * 0.25 * eo(u), l1 = r * 0.95 * eo(u); line(x, cos(an) * l0, sin(an) * l0 * (flat ? 0.3 : 1), cos(an) * l1, sin(an) * l1 * (flat ? 0.3 : 1), col2, 5 * (1 - u) + 1, 1 - u); }
    glow(x, 0, 0, r * 1.1 * (1 - u * 0.4), col, (1 - u) * 0.8); disc(x, 0, 0, r * 0.22 * (1 - u), W, 1 - u);
    parts(x, t, { n: 22, life: 0.7, spd: r * 1.3, col: col2, shape: "line", len: 3, size: 5, seed: 11 });
  };
  R.zone = function (x, t, P) {
    var rx = P.rx || 360, ry = P.ry || rx * 0.3, col = P.col || W, col2 = P.col2 || col, kind = P.kind || "up", g = eo(inv(0, P.grow || 0.4, t));
    ell(x, 0, 0, rx * g, ry * g, col, 0.2 * g, true); ell(x, 0, 0, rx * g, ry * g, col, 0.9 * g, false, 4);
    for (var k = 0; k < 3; k++) { var u = ((t * 0.7 + k / 3) % 1); ell(x, 0, 0, rx * u * g, ry * u * g, col2, (1 - u) * 0.5, false, 3); }
    if (kind === "cells") { for (var c = 0; c < 26; c++) { var cx = (H(c, 5) - 0.5) * rx * 1.6, cy = (H(c, 6) - 0.5) * ry * 1.6, pu = 0.5 + 0.5 * sin(t * 4 + c * 1.7); if (cx * cx / (rx * rx) + cy * cy / (ry * ry) < 0.9) poly(x, hexp(cx, cy, 26, 0), c % 3 ? col : col2, 0.2 + 0.5 * pu, true, 2); } return; }
    var o = { loop: true, n: P.n || 34, life: 1.0, wx: rx * 1.5, wy: ry * 0.6, ox: 0, oy: 0, col: col, cols: [col, col2], seed: 9, g: 0, size: 7, a0: -PI / 2 - 0.35, a1: -PI / 2 + 0.35, spd: 200 };
    if (kind === "bubbles") { o.shape = "ring"; o.spd = 150; o.size = 9; } else if (kind === "crystal") { o.shape = "dia"; o.spd = 90; o.size = 14; o.life = 1.4; } else if (kind === "rain") { o.a0 = PI / 2 - 0.1; o.a1 = PI / 2 + 0.1; o.oy = -420; o.spd = 800; o.shape = "line"; o.len = 5; o.size = 5; o.life = 0.5; } else if (kind === "embers") { o.spd = 260; o.size = 6; o.shape = "sq"; }
    parts(x, t, o);
  };
  R.beam = function (x, t, P) {
    var ch = P.charge == null ? 0.5 : P.charge, len = P.len || 1200, w = P.w || 90, hold = P.hold || 1.2, col = P.col || W, col2 = P.col2 || W;
    x.save(); x.rotate((P.ang || 0) * PI / 180);
    if (t < ch + 0.1) { var c = inv(0, ch, t); glow(x, 0, 0, 50 + 140 * c, col, 0.8); disc(x, 0, 0, 10 + 20 * c, W, 1); for (var i = 0; i < 18; i++) { var u = (t * 2.4 + i / 18) % 1, rr = 220 * (1 - u), an = H(i, 5) * TAU; disc(x, cos(an) * rr, sin(an) * rr, 5, col2, u); } }
    var tt = t - ch, env = eo(inv(0, 0.12, tt)) * (1 - inv(hold, hold + 0.3, tt));
    if (tt > 0 && env > 0) {
      var ww = w * env * (1 + 0.06 * sin(tt * 60)), g = x.createLinearGradient(0, -ww / 2, 0, ww / 2); g.addColorStop(0, rgba(col, 0)); g.addColorStop(0.3, rgba(col, 0.75)); g.addColorStop(0.5, rgba(W, 1)); g.addColorStop(0.7, rgba(col, 0.75)); g.addColorStop(1, rgba(col, 0));
      x.fillStyle = g; x.fillRect(0, -ww / 2, len, ww); glow(x, 0, 0, ww * 1.2, col, 0.7 * env); glow(x, len, 0, ww * 1.4, col2, 0.6 * env);
      parts(x, t, { loop: true, n: 18, life: 0.4, spd: 400, a0: -0.5, a1: 0.5, ox: len, oy: 0, col: col2, shape: "line", len: 3, size: 6, seed: 4 });
    }
    x.restore();
  };
  R.volley = function (x, t, P) {
    var n = P.n || 5, gap = P.gap == null ? 0.1 : P.gap, sp = P.sp || 1900, dist = P.dist || 680, col = P.col || W, col2 = P.col2 || W, kind = P.kind || "bolt", spread = P.spread == null ? 90 : P.spread, size = P.size || 14, fly = dist / sp;
    for (var i = 0; i < n; i++) {
      var tt = t - i * gap; if (tt < 0) continue; var u = tt / fly, y0 = n === 1 ? 0 : (i / (n - 1) - 0.5) * spread, y1 = y0 * (P.conv == null ? 0.3 : P.conv);
      if (u < 1) {
        var px = dist * u, py = lerp(y0, y1, u), ang = Math.atan2(y1 - y0, dist);
        x.save(); x.translate(px, py); x.rotate(ang);
        if (kind === "needle") { line(x, -90, 0, 0, 0, col, 3, 0.9); line(x, -20, 0, 14, 0, W, 3, 1); }
        else if (kind === "tracer") { line(x, -260, 0, 0, 0, col2, 3, 0.9); glow(x, 0, 0, 24, col, 0.8); }
        else if (kind === "star") { x.rotate(tt * 14); poly(x, [[0, -size * 1.5], [size * 0.4, -size * 0.4], [size * 1.5, 0], [size * 0.4, size * 0.4], [0, size * 1.5], [-size * 0.4, size * 0.4], [-size * 1.5, 0], [-size * 0.4, -size * 0.4]], col, 1); glow(x, 0, 0, size * 2.4, col, 0.5); }
        else if (kind === "card") { x.rotate(tt * 18); x.fillStyle = rgba(W, 1); x.fillRect(-size * 0.7, -size, size * 1.4, size * 2); x.strokeStyle = rgba(col, 1); x.lineWidth = 3; x.strokeRect(-size * 0.7, -size, size * 1.4, size * 2); disc(x, 0, 0, size * 0.3, col, 1); }
        else if (kind === "ember") { glow(x, 0, 0, size * 2.6, col, 0.6); disc(x, 0, 0, size, col2, 1); line(x, -150, 0, 0, 0, col, size, 0.5); }
        else if (kind === "ice") { poly(x, [[size * 1.6, 0], [0, -size * 0.6], [-size * 1.6, 0], [0, size * 0.6]], col, 1); glow(x, 0, 0, size * 2, col, 0.4); }
        else { line(x, -120, 0, 0, 0, col, size, 0.6); line(x, -70, 0, 6, 0, W, size * 0.4, 1); glow(x, 0, 0, size * 2.4, col, 0.7); }
        x.restore();
      } else { var ti = tt - fly; ringPulse(x, dist, y1, ti, 0.35, 10, P.ir || 70, col, 8); parts(x, t, { t0: i * gap + fly, n: 8, life: 0.4, spd: 180, ox: dist, oy: y1, col: col2, shape: "line", len: 3, size: 5, seed: 20 + i }); glow(x, dist, y1, 60 * (1 - inv(0, 0.3, ti)), col, 0.6); }
    }
  };
  function orb(x, px, py, s, kind, col, col2, t) {
    if (kind === "black") { glow(x, px, py, s * 2.6, col || VI, 0.4); disc(x, px, py, s, "#05060b", 1); circ(x, px, py, s, col || VI, 3, 0.9); x.save(); x.translate(px, py); x.rotate(-0.4 + t * 0.6); ell(x, 0, 0, s * 1.8, s * 0.5, col2 || "#FFD23F", 0.85, false, 3); x.restore(); }
    else if (kind === "white") { glow(x, px, py, s * 3.2, W, 0.55); disc(x, px, py, s, W, 1); for (var i = 0; i < 8; i++) { var an = i * PI / 4 + t; line(x, px + cos(an) * s * 1.2, py + sin(an) * s * 1.2, px + cos(an) * s * 2.2, py + sin(an) * s * 2.2, W, 3, 0.7); } }
    else { glow(x, px, py, s * 2.6, col, 0.6); disc(x, px, py, s, col2 || col, 1); }
  }
  FX.orb = orb;
  R.orbit = function (x, t, P) {
    var n = P.n || 1, r = P.r || 150, sp = P.sp || 3.2, kind = P.kind || "glow", col = P.col || W, col2 = P.col2, grow = eo(inv(0, 0.35, t)), fl = P.flat == null ? 0.55 : P.flat, s = P.s || 26;
    for (var i = 0; i < n; i++) {
      var a = t * sp + i * TAU / n + (P.ph || 0);
      for (var k = 6; k >= 1; k--) { var ak = a - k * 0.09, qx = cos(ak) * r * grow, qy = sin(ak) * r * fl * grow + (P.dy || 0); disc(x, qx, qy, s * (1 - k * 0.12), kind === "white" ? W : (col || VI), 0.12); }
      orb(x, cos(a) * r * grow, sin(a) * r * fl * grow + (P.dy || 0), s * grow, P.kinds ? P.kinds[i % P.kinds.length] : kind, col, col2, t);
    }
    if (P.pull) parts(x, t, { loop: true, n: 14, life: 0.6, spd: -200, a0: 0, a1: TAU, ox: 0, oy: P.dy || 0, col: col, shape: "line", len: 4, size: 5, seed: 3, al: 0.7 });
  };
  R.hole = function (x, t, P) {
    var s = P.s || 110, kind = P.kind || "black", g = eb(inv(0, P.grow || 0.4, t)), pul = 1 + 0.05 * sin(t * 8), col = P.col || VI, col2 = P.col2 || "#FFD23F";
    if (P.ext) { var ex = eo(inv(0, P.ext, t)); }
    for (var i = 0; i < 16; i++) { var u = (t * 1.1 + i / 16) % 1, rr = s * 4 * (1 - u) + s, an = H(i, 5) * TAU + (1 - u) * 5; disc(x, cos(an) * rr, sin(an) * rr * 0.6, 4, kind === "white" ? W : col2, u * 0.8); }
    orb(x, 0, 0, s * g * pul, kind, col, col2, t);
  };
  R.aura = function (x, t, P) {
    var col = P.col || W, col2 = P.col2 || col, r = P.r || 120, h = P.h || 280, g = eo(inv(0, 0.3, t));
    glow(x, 0, -h / 2, h * 0.8 * g, col, 0.18 * g); ell(x, 0, 0, r * 1.4 * g, r * 0.4 * g, col, 0.9, false, 4); ell(x, 0, -h / 2, r * g, h / 2 * g, col, 0.3 + 0.15 * sin(t * 6), false, 3);
    parts(x, t, { loop: true, n: P.n || 36, life: 0.9, spd: h * 1.2, a0: -PI / 2 - 0.2, a1: -PI / 2 + 0.2, wx: r * 2, oy: 0, cols: [col, col2], shape: "line", len: 5, size: 5, seed: 6 });
  };
  R.dome = function (x, t, P) {
    var rx = P.rx || 260, ry = P.ry || rx * 0.9, col = P.col || CY, g = eb(inv(0, 0.3, t)), hits = P.hits || [];
    x.save(); x.beginPath(); x.ellipse(0, 0, rx * g, ry * g, 0, PI, TAU); x.closePath(); x.fillStyle = rgba(col, 0.12); x.fill(); x.strokeStyle = rgba(col, 0.95); x.lineWidth = 4; x.stroke(); x.clip();
    for (var k = 1; k <= 4; k++) { x.beginPath(); x.ellipse(0, 0, rx * g, ry * g * k / 4, 0, PI, TAU); x.strokeStyle = rgba(col, 0.28); x.lineWidth = 2; x.stroke(); }
    for (k = -4; k <= 4; k++) { x.beginPath(); x.moveTo(k * rx * g / 4.5, 0); x.quadraticCurveTo(k * rx * g / 9, -ry * g * 1.1, 0, -ry * g); x.strokeStyle = rgba(col, 0.28); x.stroke(); }
    hits.forEach(function (h, i) { var uu = inv(0, 0.5, t - h); if (uu <= 0 || uu >= 1) return; var hx = (H(i, 31) - 0.5) * rx * 1.2, hy = -ry * 0.5 * (0.3 + H(i, 32)); circ(x, hx, hy, 90 * uu, W, 6 * (1 - uu) + 1, 1 - uu); glow(x, hx, hy, 80, col, 0.6 * (1 - uu)); });
    x.restore(); ell(x, 0, 0, rx * g, rx * 0.28 * g, col, 0.8, false, 4);
    if (P.pulse) ringPulse(x, 0, 0, (t * 1.2) % 1 - 0.0, 1, rx * 0.2, rx * 1.1, col, 4, true);
  };
  R.bolt = function (x, t, P) {
    var n = P.n || 3, gap = P.gap || 0.3, col = P.col || CY, area = P.area || 600;
    for (var i = 0; i < n; i++) {
      var tt = t - i * gap; if (tt < 0 || tt > 0.5) continue; var px = (H(i, P.seed || 3) - 0.5) * area, f = Math.floor(tt * 30), a = 1 - inv(0.1, 0.5, tt);
      bolt(x, px + (H(i, 7) - 0.5) * 120, -520, px, 0, i * 31 + f, col, P.w || 14, a, 80); glow(x, px, 0, 150, col, 0.6 * a); ringPulse(x, px, 0, tt - 0.05, 0.4, 20, 150, col, 8, true);
      parts(x, t, { t0: i * gap + 0.04, n: 10, life: 0.45, spd: 260, a0: PI, a1: TAU, ox: px, col: W, shape: "line", len: 3, size: 4, seed: 14 + i });
    }
  };
  R.chain = function (x, t, P) {
    var pts = P.pts || [[0, 0], [160, -60], [330, 40], [500, -80], [680, 30]], gap = P.gap || 0.12, col = P.col || CY, f = Math.floor(t * 30);
    for (var j = 1; j < pts.length; j++) {
      var tt = t - (j - 1) * gap; if (tt < 0 || tt > 0.6) continue; var a = 1 - inv(0.25, 0.6, tt);
      bolt(x, pts[j - 1][0], pts[j - 1][1], pts[j][0], pts[j][1], j * 17 + f, col, 8, a, 36); glow(x, pts[j][0], pts[j][1], 50, col, 0.6 * a); ringPulse(x, pts[j][0], pts[j][1], tt - 0.02, 0.3, 8, 60, col, 5);
    }
  };
  R.meteors = function (x, t, P) {
    var n = P.n || 4, gap = P.gap || 0.3, col = P.col || RO, col2 = P.col2 || W, fall = P.fall || 0.45, area = P.area || 500;
    for (var i = 0; i < n; i++) {
      var tt = t - i * gap; if (tt < 0) continue; var tx = (P.sx != null ? P.sx : 0) + (H(i, 9) - 0.5) * area, u = tt / fall;
      if (u < 1) { var sx = tx + 600, sy = -820, px = lerp(sx, tx, u), py = lerp(sy, 0, u); line(x, px + (sx - px) * 0.35, py + (sy - py) * 0.35, px, py, col, 24, 0.5); line(x, px + (sx - px) * 0.18, py + (sy - py) * 0.18, px, py, col2, 9, 0.9); glow(x, px, py, 80, col, 0.8); disc(x, px, py, 22, W, 1); }
      else { var ti = tt - fall; ringPulse(x, tx, 0, ti, 0.5, 20, P.ir || 220, col, 14, true); glow(x, tx, -20, 200 * (1 - inv(0, 0.4, ti)), col, 0.8); parts(x, t, { t0: i * gap + fall, n: 16, life: 0.6, spd: 360, a0: PI, a1: TAU, ox: tx, col: col2, shape: "line", len: 3, size: 5, seed: 40 + i, g: 400 }); }
    }
  };
  R.blink = function (x, t, P) {
    var n = P.n || 5, gap = P.gap || 0.18, col = P.col || CY, col2 = P.col2 || W, area = P.area || 420, rows = P.rows == null ? 90 : P.rows;
    function pp(i) { return [(H(i, 3) - 0.5) * area * 2, (H(i, 4) - 0.5) * rows * 2 - 40]; }
    for (var i = 1; i <= n; i++) {
      var tt = t - i * gap; if (tt < 0 || tt > 0.6) continue; var a = pp(i - 1), b = pp(i), al = 1 - inv(0.08, 0.6, tt);
      line(x, a[0], a[1], b[0], b[1], col, 10 * al + 2, al); line(x, a[0], a[1], b[0], b[1], col2, 3, al);
      ringPulse(x, b[0], b[1], tt, 0.35, 10, 80, col, 8); glow(x, b[0], b[1], 90, col, 0.7 * al);
      var an = -0.2 + H(i, 8) * 0.4; line(x, b[0] - cos(an) * 170, b[1] - sin(an) * 170 - 20, b[0] + cos(an) * 170, b[1] + sin(an) * 170 - 20, col2, 6, al);
      parts(x, t, { t0: i * gap, n: 8, life: 0.4, spd: 200, ox: b[0], oy: b[1], col: col, shape: "line", len: 3, size: 5, seed: 30 + i });
    }
  };
  R.tornado = function (x, t, P) {
    var h = P.h || 520, r = P.r || 150, col = P.col || W, col2 = P.col2 || col, g = eo(inv(0, 0.4, t));
    ell(x, 0, 0, r * 1.4 * g, r * 0.4 * g, col, 0.25, true); ell(x, 0, 0, r * 1.4 * g, r * 0.4 * g, col, 0.8, false, 3);
    for (var i = 0; i < 80; i++) {
      var u = ((t * 0.7 + i / 80) % 1), hh = u * h * g, rad = r * (0.3 + 1.2 * u) * g, an = t * 9 + i * 0.9 + u * 9, px = cos(an) * rad, py = -hh + sin(an) * rad * 0.28, back = sin(an) > 0 ? 1 : 0.55;
      disc(x, px, py, 5 + 5 * u, i % 3 ? col : col2, sin(u * PI) * 0.9 * back);
    }
    if (P.pull) parts(x, t, { loop: true, n: 14, life: 0.5, spd: -300, ox: 0, oy: -80, wy: 80, col: col, shape: "line", len: 5, size: 5, seed: 5, a0: 0, a1: TAU, al: 0.6 });
  };
  R.pillar = function (x, t, P) {
    var w = P.w || 120, col = P.col || W, warn = P.warn == null ? 0.5 : P.warn, g = eo(inv(0, warn, t));
    if (t < warn + 0.05) { ell(x, 0, 0, w * 1.6 * g, w * 0.5 * g, col, 0.25 * g, true); ell(x, 0, 0, w * 1.6 * g, w * 0.5 * g, col, 0.9, false, 4); }
    var tt = t - warn; if (tt < 0) return; var env = eo(inv(0, 0.1, tt)) * (1 - inv(P.hold || 0.5, (P.hold || 0.5) + 0.3, tt));
    if (env > 0) { var ww = w * env, gr = x.createLinearGradient(-ww / 2, 0, ww / 2, 0); gr.addColorStop(0, rgba(col, 0)); gr.addColorStop(0.5, rgba(W, 1)); gr.addColorStop(1, rgba(col, 0)); x.fillStyle = gr; x.fillRect(-ww / 2, -1000, ww, 1000); glow(x, 0, 0, 220 * env, col, 0.7); ringPulse(x, 0, 0, tt, 0.5, 30, 300, col, 14, true); }
  };
  R.chains = function (x, t, P) {
    var n = P.n || 6, col = P.col || W, tx = P.tx || 0, ty = P.ty || -60, g = P.pull == null ? 0.55 : P.pull;
    for (var i = 0; i < n; i++) {
      var an = i / n * TAU + 0.3, sx = cos(an) * (P.r || 520), sy = sin(an) * (P.r || 520) * 0.45 + ty, u = eo(inv(0.1 + i * 0.03, 0.1 + i * 0.03 + g, t)), ex = lerp(sx, tx, u), ey = lerp(sy, ty, u), hold = 1 - inv(g + 1.0, g + 1.3, t);
      var L = Math.hypot(ex - sx, ey - sy), links = Math.floor(L / 34);
      for (var k = 0; k <= links; k++) { var f = k / Math.max(1, links); x.save(); x.translate(lerp(sx, ex, f), lerp(sy, ey, f)); x.rotate(Math.atan2(ey - sy, ex - sx) + (k % 2 ? PI / 2 : 0)); ell(x, 0, 0, k % 2 ? 6 : 15, k % 2 ? 15 : 8, col, 0.9 * hold, false, 4); x.restore(); }
      glow(x, ex, ey, 40, col, 0.5 * hold);
    }
    ringPulse(x, tx, ty, t - 0.1, 0.6, 20, 200, col, 8);
  };
  R.rain = function (x, t, P) {
    var area = P.area || 700, col = P.col || CY, h = P.h || 520, ang = P.ang == null ? 0.18 : P.ang, g = eo(inv(0, 0.3, t)), n = P.n || 70;
    if (P.cloud) { for (var c = 0; c < 6; c++) { glow(x, (c / 5 - 0.5) * area * 0.9, -h, 130, col2c(P), 0.3 * g); } }
    for (var i = 0; i < n; i++) { var u = ((t * (P.sp || 2.2) + H(i, 5)) % 1), px = (H(i, 2) - 0.5) * area + u * h * ang, py = -h + u * h; if (P.snow) disc(x, px + sin(t * 3 + i) * 12, py, 5 + 4 * H(i, 3), W, 0.8 * g); else line(x, px, py, px - h * ang * 0.08, py - 40, i % 4 ? col : W, 3, 0.8 * g); }
    for (i = 0; i < 10; i++) { var u2 = ((t * 2.2 + i / 10) % 1); ell(x, (H(i, 8) - 0.5) * area, (H(i, 9) - 0.5) * 40, 70 * u2, 20 * u2, col, (1 - u2) * 0.6, false, 3); }
  };
  function col2c(P) { return P.col2 || P.col || CY; }
  R.mark = function (x, t, P) {
    var g = P.glyph || "crosshair", col = P.col || RO, s = P.s || 34, y = -(P.h || 0), a = eb(inv(0, 0.25, t)), bob = sin(t * 5) * 6;
    x.save(); x.translate(0, y + bob); x.scale(a, a); glow(x, 0, 0, s * 2.6, col, 0.35); glyph(x, g, s, col, 1); if (P.ring) { var cu = (t / (P.ring || 3)) % 1; x.strokeStyle = rgba(W, 0.9); x.lineWidth = 5; x.beginPath(); x.arc(0, 0, s * 1.6, -PI / 2, -PI / 2 + TAU * (P.rev ? cu : 1 - cu)); x.stroke(); } x.restore();
    if (P.pips) for (var i = 0; i < P.pips; i++) { var pu = eb(inv(0.2 + i * 0.25, 0.45 + i * 0.25, t)); disc(x, (i - (P.pips - 1) / 2) * 24, y + s * 2.1, 8 * pu, col, 1); }
  };
  R.halo = function (x, t, P) {
    var n = P.n || 1, col = P.col || W, r = P.r || 90, hy = -(P.h || 0), g = eo(inv(0, 0.4, t));
    for (var i = 0; i < n; i++) { var yy = hy - i * 36, rr = r * g * (1 - i * 0.12); ell(x, 0, yy, rr, rr * 0.28, col, 0.25, true); ell(x, 0, yy, rr, rr * 0.28, col, 0.95, false, 5); for (var k = 0; k < 24; k++) { var an = k / 24 * TAU + t * 1.2 * (i % 2 ? -1 : 1); if (sin(an) > 0) disc(x, cos(an) * rr, yy + sin(an) * rr * 0.28, 3, W, 0.9); } glow(x, 0, yy, rr * 1.6, col, 0.25); }
  };
  R.shards = function (x, t, P) {
    var n = P.n || 40, col = P.col || W, col2 = P.col2 || CY, w = P.w || 160, h = P.h || 260, up = P.up, D = P.D || 1.2;
    for (var i = 0; i < n; i++) { var u = inv(H(i, 3) * 0.3, H(i, 3) * 0.3 + D, t); if (u <= 0 || u >= 1) continue; var bx = (H(i, 4) - 0.5) * w, by = -H(i, 5) * h, vx = (H(i, 6) - 0.5) * (P.spd || 420), vy = up ? -120 - 300 * H(i, 7) : (H(i, 7) - 0.8) * 380, px = bx + vx * u, py = by + vy * u + (up ? 0 : 400 * u * u), sz = 18 * (1 - u * 0.6) * (0.6 + H(i, 8)), rot = H(i, 9) * TAU + u * 6;
      x.save(); x.translate(px, py); x.rotate(rot); poly(x, [[0, -sz], [sz * 0.9, sz * 0.7], [-sz * 0.9, sz * 0.7]], i % 3 ? col : col2, 0.9 * (1 - u)); poly(x, [[0, -sz], [sz * 0.9, sz * 0.7], [-sz * 0.9, sz * 0.7]], W, 0.8 * (1 - u), true, 1.5); x.restore(); }
  };
  R.glyphs = function (x, t, P) {
    var list = P.list || ["φ", "π", "∑", "√5", "∫", "Δ", "∞", "1.618"], n = P.n || 14, col = P.col || CY, w = P.w || 200;
    x.font = "700 30px 'JetBrains Mono', monospace"; x.textAlign = "center";
    for (var i = 0; i < n; i++) { var u = inv(H(i, 3) * 0.5, H(i, 3) * 0.5 + 1.6, t); if (u <= 0 || u >= 1) continue; x.fillStyle = rgba(i % 3 ? col : W, Math.sin(u * PI)); x.fillText(list[i % list.length], (H(i, 4) - 0.5) * w * 2, -40 - u * (P.h || 360) * (0.5 + H(i, 5))); }
  };
  R.text = function (x, t, P) {
    var u = eb(inv(0, 0.22, t)), s = P.s || 120, col = P.col || W; x.save(); x.translate(0, P.y || -200); x.scale(u, u); x.font = (P.font === "mono" ? "700 " : "") + s + "px " + (P.font === "mono" ? "'JetBrains Mono', monospace" : P.font === "title" ? "'Archivo Black', sans-serif" : "'League Gothic', sans-serif"); x.textAlign = "center"; x.textBaseline = "middle";
    x.fillStyle = rgba(P.sh || VI, 1); x.fillText(P.txt, 7, 6); x.fillStyle = rgba(col, 1); x.fillText(P.txt, 0, 0); x.restore();
  };
  R.flash = function (x, t, P) { var u = inv(0, P.D || 0.15, t); if (u >= 1) return; x.fillStyle = rgba(P.col || W, (P.a == null ? 0.8 : P.a) * (1 - u)); x.fillRect(-960, -640, 1920, 1080 + 200); };
  R.speed = function (x, t, P) { if (t > (P.D || 0.5)) return; IG.speed(x, t, { cx: 0, cy: -60, n: P.n || 70, r0: P.r0 || 260, col: rgba(P.col || W, 0.55 * (1 - inv(0, P.D || 0.5, t))) }); };
  R.ring = function (x, t, P) { for (var i = 0; i < (P.n || 1); i++) ringPulse(x, 0, P.y || 0, t - i * 0.1, P.D || 0.5, P.r0 || 20, P.r1 || 240, P.col || W, P.w || 12, P.flat); };
  R.burst = function (x, t, P) { parts(x, t, { n: P.n || 24, life: P.life || 0.7, spd: P.spd || 320, col: P.col || W, cols: P.cols, shape: P.shape || "line", len: 3, size: P.size || 6, seed: P.seed || 3, g: P.g || 0, ox: 0, oy: P.y || 0, a0: P.a0, a1: P.a1 }); };
  R.column = function (x, t, P) {
    var col = P.col || W, col2 = P.col2 || col, h = P.h || 700, w = P.w || 160, env = eo(inv(0, 0.25, t)) * (1 - inv(P.hold || 1, (P.hold || 1) + 0.4, t));
    if (env <= 0) return; var g = x.createLinearGradient(-w / 2, 0, w / 2, 0); g.addColorStop(0, rgba(col, 0)); g.addColorStop(0.5, rgba(col2, 0.8)); g.addColorStop(1, rgba(col, 0)); x.fillStyle = g; x.fillRect(-w / 2 * env, -h, w * env, h);
    for (var k = 0; k < 6; k++) { var u = (t * 1.4 + k / 6) % 1; ell(x, 0, -u * h, w * 0.7, w * 0.2, col2, (1 - u) * 0.9, false, 4); }
    ringPulse(x, 0, 0, t, 0.6, 30, 340, col, 14, true); glow(x, 0, 0, 200 * env, col, 0.6);
    parts(x, t, { loop: true, n: 30, life: 0.9, spd: 600, a0: -PI / 2 - 0.15, a1: -PI / 2 + 0.15, wx: w, col: col2, shape: "line", len: 5, size: 5, seed: 8 });
  };
  R.flower = function (x, t, P) {
    var n = P.n || 8, r = P.r || 280, col = P.col || RO, col2 = P.col2 || W, g = eb(inv(0, 0.7, t)), hy = -(P.h || 220);
    for (var i = 0; i < n; i++) { var an = i / n * TAU + t * 0.3; x.save(); x.translate(0, hy); x.rotate(an); ell(x, r * 0.55 * g, 0, r * 0.5 * g, r * 0.22 * g, col, 0.55, true); ell(x, r * 0.55 * g, 0, r * 0.5 * g, r * 0.22 * g, col2, 0.9, false, 3); x.restore(); }
    for (i = 0; i < n; i++) { var an2 = i / n * TAU - t * 0.5 + 0.4; x.save(); x.translate(0, hy); x.rotate(an2); ell(x, r * 0.3 * g, 0, r * 0.28 * g, r * 0.12 * g, col2, 0.8, true); x.restore(); }
    disc(x, 0, hy, r * 0.14 * g, W, 1); glow(x, 0, hy, r * 1.1 * g, col, 0.3);
    line(x, 0, hy, 0, 0, col2, 8, 0.8 * g);
  };
  R.dice = function (x, t, P) {
    var faces = P.faces || [3, 5, 2, 6, 4, 1], D = P.D || 1.4, u = inv(0, D, t), face = u < 1 ? faces[Math.floor(t * 14) % 6] : (P.final || 6), s = 80, col = P.col || W, res = P.rcol || RO;
    var y = u < 1 ? -Math.abs(sin(u * 9)) * 160 * (1 - u) - 40 : -40; x.save(); x.translate(0, y); x.rotate(u < 1 ? (1 - u) * 6 : 0);
    x.fillStyle = rgba(W, 1); x.fillRect(-s, -s, s * 2, s * 2); x.strokeStyle = rgba(col, 1); x.lineWidth = 8; x.strokeRect(-s, -s, s * 2, s * 2);
    var pp = { 1: [[0, 0]], 2: [[-1, -1], [1, 1]], 3: [[-1, -1], [0, 0], [1, 1]], 4: [[-1, -1], [1, -1], [-1, 1], [1, 1]], 5: [[-1, -1], [1, -1], [0, 0], [-1, 1], [1, 1]], 6: [[-1, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [1, 1]] }[face];
    pp.forEach(function (p) { disc(x, p[0] * s * 0.5, p[1] * s * 0.5, s * 0.14, u < 1 ? BK : res, 1); }); x.restore();
    if (u >= 1) { ringPulse(x, 0, -40, t - D, 0.5, 60, 260, res, 10); glow(x, 0, -40, 200 * (1 - inv(0, 0.5, t - D)), res, 0.5); }
  };
  R.coin = function (x, t, P) {
    var D = P.D || 1.2, u = inv(0, D, t), heads = P.heads !== false, col = P.col || RO, y = -Math.sin(u * PI) * 300 - 60, sc = u < 1 ? cos(u * 22) : 1;
    glow(x, 0, y, 120, col, 0.3); x.save(); x.translate(0, y); x.scale(1, Math.abs(sc) < 0.06 ? 0.06 : Math.abs(sc)); disc(x, 0, 0, 90, col, 1); circ(x, 0, 0, 74, W, 6, 0.9); x.fillStyle = rgba(W, 1); x.font = "90px 'Archivo Black', sans-serif"; x.textAlign = "center"; x.textBaseline = "middle"; x.fillText(u < 1 ? (sc > 0 ? "H" : "T") : (heads ? "H" : "T"), 0, 6); x.restore();
    if (u >= 1) ringPulse(x, 0, -60, t - D, 0.5, 60, 240, col, 10);
  };
  R.slots = function (x, t, P) {
    var syms = P.syms || ["7", "★", "◆", "♥", "☠", "🔔"], stops = P.stops || [0.9, 1.3, 1.7], res = P.res || ["7", "7", "7"], col = P.col || RO;
    x.fillStyle = rgba(BK, 0.9); x.fillRect(-270, -250, 540, 220); x.strokeStyle = rgba(col, 1); x.lineWidth = 8; x.strokeRect(-270, -250, 540, 220);
    for (var i = 0; i < 3; i++) { var stopped = t > stops[i], cx = (i - 1) * 170, sym = stopped ? res[i] : syms[Math.floor(t * 22 + i * 2) % syms.length]; x.fillStyle = rgba(stopped ? col : W, 1); x.font = "130px 'Archivo Black', sans-serif"; x.textAlign = "center"; x.textBaseline = "middle"; x.fillText(sym, cx, -140 + (stopped ? 0 : sin(t * 40 + i) * 14)); if (stopped) ringPulse(x, cx, -140, t - stops[i], 0.35, 20, 110, col, 6); }
    if (t > stops[2] + 0.1) { var a = 0.5 + 0.5 * sin(t * 18); glow(x, 0, -140, 300, col, 0.35 * a); }
  };
  R.roulette = function (x, t, P) {
    var D = P.D || 2.0, u = inv(0, D, t), rot = eo(u) * 18 + t * 0.3, N = 12, cols = [RO, BK, RO, BK, RO, BK, RO, BK, RO, BK, RO, CY], res = P.res == null ? 11 : P.res;
    x.save(); x.translate(0, -150); x.scale(1, 0.5);
    for (var i = 0; i < N; i++) { var a0 = rot + i * TAU / N, a1 = a0 + TAU / N; x.beginPath(); x.moveTo(0, 0); x.arc(0, 0, 210, a0, a1); x.closePath(); x.fillStyle = rgba(cols[i], 1); x.fill(); x.strokeStyle = rgba(W, 0.8); x.lineWidth = 3; x.stroke(); }
    circ(x, 0, 0, 40, W, 6, 1); x.restore();
    var ba = -rot * 0.0 + (u < 1 ? t * 11 : 11 * D) , br = u < 1 ? 200 - 40 * u : 160; disc(x, cos(ba) * br, -150 + sin(ba) * br * 0.5, 12, W, 1);
    if (u >= 1) { var cc = cols[res]; glow(x, 0, -150, 300, cc === BK ? W : cc, 0.4 * (0.6 + 0.4 * sin((t - D) * 14))); }
  };
  R.clock = function (x, t, P) {
    var r = P.r || 150, col = P.col || W, col2 = P.col2 || CY, g = eo(inv(0, 0.3, t)), y = -(P.h || 240), spd = P.spd == null ? 1 : P.spd;
    glow(x, 0, y, r * 2, col2, 0.25 * g); circ(x, 0, y, r * g, col, 6, 0.95); circ(x, 0, y, r * 0.88 * g, col2, 2, 0.6);
    for (var i = 0; i < 12; i++) { var an = i / 12 * TAU - PI / 2; line(x, cos(an) * r * 0.78 * g, y + sin(an) * r * 0.78 * g, cos(an) * r * 0.9 * g, y + sin(an) * r * 0.9 * g, col, 4, 0.9); }
    var ha = t * 6 * spd, ma = t * 0.5 * spd; line(x, 0, y, cos(ha - PI / 2) * r * 0.78 * g, y + sin(ha - PI / 2) * r * 0.78 * g, col2, 5, 1); line(x, 0, y, cos(ma - PI / 2) * r * 0.5 * g, y + sin(ma - PI / 2) * r * 0.5 * g, col, 8, 1); disc(x, 0, y, 9, W, 1);
    if (P.rewind) for (var k = 0; k < 4; k++) { x.strokeStyle = rgba(col2, 0.6 - k * 0.12); x.lineWidth = 4; x.beginPath(); x.arc(0, y, r * (1.15 + k * 0.1), -PI / 2 - 0.2, -PI / 2 + 1.6 - t * 3 % 1.6, true); x.stroke(); }
  };
  R.pips = function (x, t, P) { var n = P.n || 5, col = P.col || W, y = -(P.h || 300); for (var i = 0; i < n; i++) { var u = eb(inv(i * 0.18, i * 0.18 + 0.3, t)); x.save(); x.translate((i - (n - 1) / 2) * 60, y); x.scale(u, u); glyph(x, "star", 22, col, 1); glow(x, 0, 0, 40, col, 0.4); x.restore(); } };
  R.hexfield = function (x, t, P) { // data / firewall / glitch cells in a rectangle
    var w = P.w || 600, h = P.h || 160, col = P.col || CY, col2 = P.col2 || RO, g = eo(inv(0, 0.3, t)), n = P.n || 40;
    for (var i = 0; i < n; i++) { var u = ((t * 1.3 + H(i, 4)) % 1), cx = (H(i, 5) - 0.5) * w * g, cy = -H(i, 6) * h; poly(x, hexp(cx, cy, 14 + 18 * H(i, 7), u * 3), i % 4 ? col : col2, Math.sin(u * PI) * 0.8, true, 2); }
    line(x, -w / 2 * g, 0, w / 2 * g, 0, col, 4, 0.9); parts(x, t, { loop: true, n: 24, life: 0.8, spd: 260, a0: -PI * 0.7, a1: -PI * 0.3, wx: w, col: col, cols: [col, col2], shape: "sq", size: 8, seed: 12 });
  };
  R.glitchbar = function (x, t, P) { var f = Math.floor(t * 20), n = P.n || 8, col = P.col || CY, col2 = P.col2 || RO, w = P.w || 500; for (var i = 0; i < n; i++) { var y = (H(i, f) - 0.5) * (P.h || 360), l = 40 + 260 * H(i, f + 1), px = (H(i, f + 2) - 0.5) * w; x.fillStyle = rgba(i % 3 ? col : col2, 0.65); x.fillRect(px, y, l, 6 + 14 * H(i, f + 3)); } };
  R.grid = function (x, t, P) { var w = P.w || 480, col = P.col || CY, g = eo(inv(0, 0.3, t)); for (var i = -4; i <= 4; i++) { line(x, i * w / 8 * g, -420, i * w / 8 * g, 0, col, 1.5, 0.35); } for (i = 0; i < 6; i++) { line(x, -w / 2 * g, -i * 80, w / 2 * g, -i * 80, col, 1.5, 0.35); } };
  R.slabs = function (x, t, P) { // -12 degree slabs sweeping (verdict / curtain / ink)
    var n = P.n || 3, col = P.col || W; for (var i = 0; i < n; i++) { var u = eo(inv(i * 0.08, i * 0.08 + 0.3, t)); x.save(); x.rotate(-12 * PI / 180); x.fillStyle = rgba(i % 2 ? (P.col2 || col) : col, 0.85 * (1 - inv(P.hold || 0.8, (P.hold || 0.8) + 0.3, t))); x.fillRect(-900 + 1800 * (1 - u) * (i % 2 ? -1 : 1) * 0 - 900, -200 + i * 120 - (n - 1) * 60, 1800 * u, 90); x.restore(); }
  };
  R.pulsehex = function (x, t, P) { var n = P.n || 4, col = P.col || CY; for (var i = 0; i < n; i++) { var u = inv(i * 0.15, i * 0.15 + 0.8, t); if (u <= 0 || u >= 1) continue; poly(x, hexp(0, -(P.h || 0), (P.r || 300) * eo(u), PI / 6), col, 1 - u, true, 6 * (1 - u) + 1); } };

  /* ================= pieces ================= */
  var EXTRA = { al: 1 };
  FX.drawLayer = function (x, t, L, D) {
    var t0 = L.t0 || 0, t1 = L.t1 == null ? D : L.t1, lt = t - t0; if (lt < 0 || t > t1) return;
    var out = L.fo === 0 ? 0 : inv(t1 - (L.fo == null ? 0.3 : L.fo), t1, t), at = POS[L.at || "mid"] || POS.mid;
    x.save(); x.globalAlpha *= (1 - out) * (L.al == null ? 1 : L.al); x.translate(at[0] + (L.ox || 0), at[1] + (L.oy || 0)); if (L.sc) x.scale(L.sc, L.sc); if (L.flip) x.scale(-1, 1);
    R[L.fx](x, lt, L); x.restore();
  };
  FX.drawPiece = function (x, spec, t) {
    x.save(); x.translate(960, 640); (spec.layers || []).forEach(function (L) { FX.drawLayer(x, t, L, spec.dur); }); x.restore();
  };
  /* scene dressing for the sheet videos only (never in the transparent masters) */
  FX.dress = function (x, spec, t) {
    x.save(); x.translate(960, 640);
    ell(x, 0, 60, 760, 120, "#64E8FF", 0.05, true); ell(x, 0, 60, 760, 120, "#64E8FF", 0.18, false, 2);
    if (spec.cas !== false) { var c = POS.cas; x.save(); x.translate(c[0], c[1]); circ(x, 0, 0, 26, "#64E8FF", 3, 0.85); line(x, -40, 0, -16, 0, "#64E8FF", 3, 0.85); line(x, 16, 0, 40, 0, "#64E8FF", 3, 0.85); line(x, 0, -40, 0, -16, "#64E8FF", 3, 0.85); line(x, 0, 16, 0, 40, "#64E8FF", 3, 0.85); x.font = "700 20px 'JetBrains Mono', monospace"; x.fillStyle = rgba("#64E8FF", 0.8); x.textAlign = "center"; x.fillText("CASTER", 0, 70); x.restore(); }
    if (spec.enemy !== false) { var e = POS.en; ico(x, e[0], e[1] - 140, 120, 0.4 + t * 0.35, 0.6 + t * 0.5, "#F7F4FA", 2, 0.22); ico(x, e[0], e[1] - 140, 62, -t * 0.5, t * 0.7, "#64E8FF", 2, 0.22); x.font = "700 20px 'JetBrains Mono', monospace"; x.fillStyle = rgba("#64E8FF", 0.6); x.textAlign = "center"; x.fillText("ANGEL (SCALE DUMMY)", e[0], e[1] + 70); }
    x.restore();
  };
  FX.status = function (x, t, spec) { x.save(); x.translate(960, 640); (spec.layers || []).forEach(function (L) { FX.drawLayer(x, t, L, spec.dur); }); x.restore(); };
})(window);
