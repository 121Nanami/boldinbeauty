/* linger.js: GreyHat's admiration rule. A linger track holds a piece after its entrance and, over 16 s, lets it go from sweet to hollow:
   0-4 s sweet · 4-8 s off · 8-12 s wrong · 12 s+ hollow (then dead still). The game plays this when the player idles on a screen and
   snaps back to the sweet piece on any input. Everything is computed from t (seek-safe), stepped by the 13 fps render.
   Approved whisper lines (board v2) only on victory, ultimate-ready and notice-toast; every other piece creeps without new words. */
(function (root) {
  var IG = root.IG, H = IG.hash, gs = root.gsap;
  function cl(x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
  function mix(a, b, k) {
    a = [parseInt(a.substr(1, 2), 16), parseInt(a.substr(3, 2), 16), parseInt(a.substr(5, 2), 16)];
    b = [parseInt(b.substr(1, 2), 16), parseInt(b.substr(3, 2), 16), parseInt(b.substr(5, 2), 16)]; k = cl(k);
    return "rgb(" + [0, 1, 2].map(function (i) { return Math.round(a[i] + (b[i] - a[i]) * k); }).join(",") + ")";
  }
  function pick(d, a) { return d < 0.3 ? a[0] : d < 0.6 ? a[1] : d < 0.85 ? a[2] : a[3]; }
  var LINES = {
    "victory": { find: /stage clear/i, lines: [null, "stage clear · you did so well", "stay a little longer", "you can stay forever"] },
    "ultimate-ready": { find: /ultimate ready/i, lines: [null, null, "ready when you are", "we’re ready"], over: true },
    "notice-toast": { find: /daily quest complete|season pass updated|new messages/i, last: true,
      lines: [null, "the lights stay on tonight", "we left the light on for you", "we always leave the light on"] }
  };
  var DROP = { "notice-toast": /later/i };

  root.LINGER = function (tl, L, s, o) {
    o = o || {}; var HOLD = o.hold || 3.8, DUR = o.dur || HOLD + 16.2, id = o.id;
    s.pause();
    var wrap = L; /* L already holds the piece */
    var mot = IG.rawEl("div", "", L.parentNode, null, { position: "absolute", left: 0, top: 0, width: "1920px", height: "1080px", pointerEvents: "none" });
    var vig = IG.rawEl("div", "", L.parentNode, null, { position: "absolute", left: "-96px", top: "-54px", width: "2112px", height: "1188px", pointerEvents: "none", opacity: 0,
      background: "radial-gradient(ellipse at center, rgba(42,15,30,0) 40%, rgba(42,15,30,.9) 100%)" });
    vig.style.background = "radial-gradient(ellipse at center, rgba(42,15,30,0) 40%, rgba(42,15,30,.9) 100%)";
    var MC = ["#FFD9E8", "#CFE3FF", "#D5F2DF", "#CDBBF5", "#FFF8F0"], motes = [];
    for (var i = 0; i < 26; i++) { var m = document.createElement("div"); m.style.cssText = "position:absolute;width:14px;height:14px;margin:-7px;border-radius:50%;opacity:0;filter:blur(1.5px);background:" + MC[i % 5]; mot.appendChild(m); motes.push(m); }
    var cache = null;
    function scan() {
      /* after the entrance: collect ink edges, display text, percentages and the lines to rewrite */
      var all = Array.prototype.slice.call(L.querySelectorAll("*")), r = { ink: [], text: [], pct: [], big: null, line: null, drop: null };
      var bigW = 0;
      all.forEach(function (e) {
        var cs = getComputedStyle(e);
        if (cs.visibility === "hidden" || cs.display === "none") return;
        if (parseFloat(cs.borderTopWidth) >= 2 && /91, 74, 120|5b4a78/i.test(cs.borderTopColor + cs.borderLeftColor)) r.ink.push([e, parseFloat(cs.borderTopWidth)]);
        var own = e.childNodes.length && Array.prototype.every.call(e.childNodes, function (n) { return n.nodeType === 3; });
        var disp = /\b(voice|title)\b/.test(e.className) && !e.querySelector("div,svg,canvas");
        if (!own && !disp) return;
        var t = e.textContent.trim(); if (!t) return;
        if (/\b(voice|title)\b/.test(e.className)) { r.text.push(e); var w = e.getBoundingClientRect().width; if (w > bigW) { bigW = w; r.big = e; } }
        if (/^\d{1,3}%$/.test(t)) r.pct.push(e);
        var spec = LINES[id];
        if (spec && spec.find.test(t) && (!r.line || spec.last)) r.line = e;
        if (DROP[id] && DROP[id].test(t)) r.drop = e;
      });
      r.text.forEach(function (e) { e._orig = e.textContent; e._col = getComputedStyle(e).color; });
      if (r.big) { var t = r.big.textContent; r.big.textContent = ""; r.big._sp = t.split("").map(function (ch) { var sp = document.createElement("span"); sp.style.display = "inline-block"; sp.textContent = ch === " " ? " " : ch; r.big.appendChild(sp); return sp; }); }
      if (r.line) r.line._orig = r.line.textContent;
      r.pct.forEach(function (e) { e._orig = e.textContent; });
      return r;
    }
    IG.clock(tl, DUR, function (t) { IG.raw(function () { frame(t); }); }, 0);
    function frame(t) {
      s.seek(Math.min(t, HOLD), false);
      if (t < HOLD - 0.01) { wrap.style.filter = ""; wrap.style.transform = ""; return; }
      if (!cache) cache = scan();
      var lt = t - HOLD, d = cl(lt / 13.3), still = d > 0.94, f = Math.floor(t * 13);
      /* bob, detached hollow shadow, colour drain */
      var bob = still ? 0 : Math.sin(lt * 2 * Math.PI / 3.2) * 6, lag = still ? 0 : Math.sin((lt - d * 1.4) * 2 * Math.PI / 3.2) * 6;
      var sh = cl((d - 0.3) / 0.4);
      var filt = "saturate(" + (1 - 0.6 * d).toFixed(3) + ") sepia(" + (0.35 * d).toFixed(3) + ") hue-rotate(" + (18 * d).toFixed(1) + "deg) brightness(" + (1 - 0.1 * d * d).toFixed(3) + ")";
      if (sh > 0) filt += " drop-shadow(" + (6 + 40 * d).toFixed(1) + "px " + (12 + 50 * d + lag - bob).toFixed(1) + "px " + (18 - 12 * d).toFixed(1) + "px rgba(15,5,26," + (0.4 * sh).toFixed(3) + "))";
      var smearPer = d > 0.5 ? 3.4 - 1.6 * d : 99, smear = !still && (lt % smearPer) < 2 / 13;
      if (smear) filt += " blur(" + (3 + d * 4).toFixed(1) + "px)";
      var hollow = d > 0.88 && f % 29 === 0 && !still;
      if (hollow) filt = "brightness(0.06) saturate(0) drop-shadow(0 0 6px rgba(15,5,26,.6))";
      wrap.style.filter = filt;
      wrap.style.transform = "translate(" + (smear ? 12 + 18 * d : 0) + "px," + bob.toFixed(2) + "px)";
      /* ink grows toward Hollow Ink */
      var ik = cl((d - 0.4) / 0.5);
      cache.ink.forEach(function (p) { p[0].style.borderColor = mix("#5B4A78", "#0F051A", ik); p[0].style.borderWidth = (p[1] + Math.pow(cl((d - 0.5) / 0.5), 1.5) * 6).toFixed(2) + "px"; });
      cache.text.forEach(function (e) { e.style.color = mix("#5B4A78", "#0F051A", cl((d - 0.5) / 0.4)); });
      /* letters slip; the last one turns around */
      if (cache.big && cache.big._sp) cache.big._sp.forEach(function (sp, i, a) {
        var dj = d > 0.3 && !still ? (H(i * 7 + Math.floor(f / 6), 3) - 0.5) * d * d * 16 : (d > 0.3 ? (H(i * 7, 3) - 0.5) * d * d * 16 : 0);
        sp.style.transform = "translateY(" + dj.toFixed(2) + "px)" + (i === a.length - 1 && d > 0.62 ? " scaleX(-1)" : "");
      });
      /* percentages keep counting */
      cache.pct.forEach(function (e) { var v = parseInt(e._orig, 10); e.textContent = (v + (d > 0.3 ? Math.floor(3 + (d - 0.3) / 0.7 * 9) : 0)) + "%"; });
      /* approved whisper lines */
      var spec = LINES[id];
      if (spec && cache.line) { var nl = pick(d, spec.lines); cache.line.textContent = nl || cache.line._orig; }
      if (cache.drop) cache.drop.style.opacity = (1 - cl((d - 0.6) / 0.25)).toFixed(3);
      /* motes: drift up, then gather into a ring that slows and stops */
      var ring = cl((d - 0.55) / 0.3);
      motes.forEach(function (m, k) {
        var p = still ? (H(k, 9) + 0.5) % 1 : (lt * 0.1 + H(k, 9)) % 1, fx = H(k, 40) * 1920, fy = 1080 - p * 1180;
        var ang = k / motes.length * Math.PI * 2 + (still ? 0 : lt * 0.25 * (1 - d)), rx = 960 + Math.cos(ang) * 300, ry = 540 + Math.sin(ang) * 300;
        m.style.left = (fx + (rx - fx) * ring).toFixed(1) + "px"; m.style.top = (fy + (ry - fy) * ring).toFixed(1) + "px";
        m.style.opacity = (((ring > 0 ? 0.5 + 0.5 * ring : Math.sin(p * Math.PI)) * 0.8) * cl(lt / 0.8)).toFixed(3);
        m.style.background = d > 0.6 ? mix(MC[k % 5], "#BDCC8F", d) : MC[k % 5];
      });
      vig.style.opacity = Math.pow(cl((d - 0.45) / 0.55), 1.4).toFixed(3);
      vig.style.transform = "scale(" + (1.12 - 0.12 * d + (still ? 0 : 0.02 * Math.sin(lt * 2 * Math.PI / 5.2))).toFixed(4) + ")";
    }
  };
})(window);
