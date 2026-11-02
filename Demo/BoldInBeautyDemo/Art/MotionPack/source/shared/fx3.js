/* fx3.js: extra recipes: suck / push streaks, ground cracks, the button prop. */
(function (root) {
  var H = IG.hash, TAU = Math.PI * 2, PI = Math.PI, cos = Math.cos, sin = Math.sin, R = FX.R, rgba = FX.rgba, glow = FX.glow;
  var W = "#F7F4FA", BK = "#090B12";
  function cl(v, a, b) { return v < a ? a : v > b ? b : v; }
  function inv(a, b, v) { return cl((v - a) / (b - a), 0, 1); }
  function line(x, ax, ay, bx, by, col, w, a) { x.strokeStyle = rgba(col, a); x.lineWidth = w; x.lineCap = "round"; x.beginPath(); x.moveTo(ax, ay); x.lineTo(bx, by); x.stroke(); }
  R.suck = function (x, t, P) {
    var n = P.n || 36, r = P.r || 500, col = P.col || W, out = P.out, oy = P.oy || 0, flat = P.flat == null ? 0.55 : P.flat, g = inv(0, 0.25, t);
    for (var i = 0; i < n; i++) {
      var u = (t * (P.sp || 1.5) + i / n) % 1, a = H(i, 5) * TAU, rr = out ? 60 + (r - 60) * u : r * (1 - u) + 40, len = 70 * (out ? 1 - u : u + 0.3), px = cos(a) * rr, py = sin(a) * rr * flat + oy, dx = cos(a) * len, dy = sin(a) * len * flat;
      line(x, px, py, px + (out ? -dx : dx), py + (out ? -dy : dy), i % 3 ? col : (P.col2 || W), 3 + 2 * u, Math.sin(u * PI) * 0.9 * g);
    }
    if (P.core) glow(x, 0, oy, 140, col, 0.35 * g);
  };
  R.cracks = function (x, t, P) {
    var n = P.n || 9, col = P.col || W, r = P.r || 420, u = 1 - Math.pow(1 - inv(0, P.D || 0.35, t), 3);
    for (var i = 0; i < n; i++) {
      var a = i / n * TAU + H(i, 3) * 0.5, px = 0, py = 0, pts = [[0, 0]], segs = 7;
      for (var k = 1; k <= segs; k++) { a += (H(i * 9 + k, 4) - 0.5) * 0.9; var l = r / segs * (0.6 + 0.8 * H(i * 9 + k, 5)) * u; px += cos(a) * l; py += sin(a) * l * 0.3; pts.push([px, py]); }
      x.strokeStyle = rgba(col, 0.95 * (1 - inv(1.0, 1.6, t))); x.lineWidth = 5; x.lineJoin = "round"; x.beginPath(); pts.forEach(function (p, j) { if (j) x.lineTo(p[0], p[1]); else x.moveTo(p[0], p[1]); }); x.stroke();
      x.strokeStyle = rgba(W, 0.8 * (1 - inv(1.0, 1.6, t))); x.lineWidth = 2; x.stroke();
    }
    glow(x, 0, 0, r * 0.5 * (1 - u * 0.5), col, 0.5 * (1 - u));
  };
  R.button = function (x, t, P) {
    var press = P.press == null ? 1.0 : P.press, g = inv(0, 0.4, t), col = P.col || "#E5252A", y = -(P.h || 230);
    x.save(); x.translate(0, y + (1 - g) * 60); x.globalAlpha *= g;
    x.fillStyle = rgba("#151925", 1); x.fillRect(-190, 30, 380, 70); x.strokeStyle = rgba("#64E8FF", 0.9); x.lineWidth = 4; x.strokeRect(-190, 30, 380, 70);
    var dn = t > press ? 1 : 0, cap = dn ? 14 : 36;
    x.fillStyle = rgba(col, 1); x.beginPath(); x.ellipse(0, 30 - cap + 10, 120, 24, 0, 0, TAU); x.fill(); x.fillRect(-120, 30 - cap + 10, 240, cap); x.fillStyle = rgba(col, 1); x.beginPath(); x.ellipse(0, 30 - cap, 120, 24, 0, 0, TAU); x.fill(); x.fillStyle = rgba(W, 0.35); x.beginPath(); x.ellipse(-30, 30 - cap - 6, 50, 8, 0, 0, TAU); x.fill();
    x.fillStyle = rgba(W, 1); x.font = "700 30px 'JetBrains Mono', monospace"; x.textAlign = "center"; x.fillText(P.txt || "NUCLEAR BOMB", 0, 76);
    x.restore();
    if (t > press) { var u = inv(press, press + (P.D || 0.9), t); if (u < 1) { x.strokeStyle = rgba(P.pulse || "#FFD23F", 1 - u); x.lineWidth = 16 * (1 - u) + 2; x.beginPath(); x.arc(0, y, 80 + 1000 * u, 0, TAU); x.stroke(); glow(x, 0, y, 300 * (1 - u), P.pulse || "#FFD23F", 0.5 * (1 - u)); } }
  };
})(window);
