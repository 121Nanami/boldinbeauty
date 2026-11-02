/* trans.js: the five official BoldInBeauty page transitions.
   TR.tN(tl, layer, at) builds transition N inside `layer` (a 1920x1080 absolute element) starting at `at`.
   Every transition fully covers the screen at `at + TR.MID[N]` (swap pages there) and is fully clear at `at + TR.DUR[N]`. */
(function (root) {
  var C = IG.C, el = IG.el, H = IG.hash;
  var TR = { DUR: [0, 0.8, 1.0, 1.2, 1.35, 1.6], MID: [0, 0.4, 0.45, 0.6, 0.55, 0.75], n: 0,
    NAME: ['', 'SIMPLE TECH', 'CYBER SLAB', 'STYLISED TECH', 'BB SIGNAL', 'FULL BB'] };
  function box(p, css) { return el("div", "", p, null, Object.assign({ position: "absolute" }, css)); }
  function cvs(p, id) { return IG.canvas(id, 1920, 1080, p); }

  /* T1 SIMPLE TECH: a steel panel slides across with a cyan leading edge, a load bar and a label. */
  TR.t1 = function (tl, L, at) {
    var pan = box(L, { left: 0, top: 0, width: "1920px", height: "1080px", background: C.st });
    var edge = box(pan, { right: "-4px", top: 0, width: "4px", height: "1080px", background: C.cy, boxShadow: "0 0 18px " + C.cy });
    var grid = box(pan, { inset: 0, backgroundImage: "linear-gradient(rgba(100,232,255,.06) 1px, transparent 1px), linear-gradient(90deg, rgba(100,232,255,.06) 1px, transparent 1px)", backgroundSize: "60px 60px" });
    var lab = el("div", "mono", pan, "LOADING", { position: "absolute", left: "860px", top: "500px", fontSize: "28px", color: C.wh });
    var trk = box(pan, { left: "860px", top: "548px", width: "200px", height: "4px", background: "rgba(100,232,255,.25)" });
    var bar = box(trk, { left: 0, top: 0, height: "4px", width: "0px", background: C.cy });
    gsap.set(pan, { xPercent: -101 });
    tl.fromTo(pan, { xPercent: -101 }, { xPercent: 0, duration: 0.4, ease: "power3.inOut", immediateRender: false }, at);
    tl.fromTo(bar, { width: 0 }, { width: 200, duration: 0.35, ease: "power2.inOut", immediateRender: false }, at + 0.15);
    tl.to(pan, { xPercent: 101, duration: 0.4, ease: "power3.inOut" }, at + 0.4);
    tl.set(edge, { right: "auto", left: "-4px" }, at + 0.4);
  };

  /* T2 CYBER: -12 degree slabs in steel / violet sweep in staggered, a glitch slice and a hex flash, then sweep out. */
  TR.t2 = function (tl, L, at) {
    var wrap = box(L, { left: "-400px", top: "-400px", width: "2720px", height: "1880px", transform: "rotate(-12deg)" });
    var N = 7, cols = [C.st, "#1d2233", C.st, "#241f45", C.st, "#1d2233", C.st];
    var slabs = [];
    for (var i = 0; i < N; i++) {
      var s = box(wrap, { left: 0, top: (i * 1880 / N) + "px", width: "2720px", height: (1880 / N + 2) + "px", background: cols[i] });
      box(s, { left: 0, top: 0, width: "2720px", height: "2px", background: i % 2 ? C.cy : C.vio, opacity: .8 });
      slabs.push(s);
      gsap.set(s, { xPercent: i % 2 ? 105 : -105 });
      tl.fromTo(s, { xPercent: i % 2 ? 105 : -105 }, { xPercent: 0, duration: 0.36, ease: "expo.out", immediateRender: false }, at + i * 0.02);
      tl.to(s, { xPercent: i % 2 ? -105 : 105, duration: 0.38, ease: "expo.in" }, at + 0.52 + i * 0.015);
    }
    var hex = box(L, { inset: 0, opacity: 0, backgroundImage: "radial-gradient(circle at 50% 50%, rgba(100,232,255,.25) 2px, transparent 3px)", backgroundSize: "34px 30px" });
    var tag = el("div", "mono", L, "SYNC // 0x3D8D", { position: "absolute", left: "96px", top: "980px", fontSize: "26px", color: C.cy, opacity: 0 });
    tl.fromTo(hex, { opacity: 0 }, { opacity: 1, duration: 0.08, immediateRender: false }, at + 0.36);
    tl.to(hex, { opacity: 0, duration: 0.25 }, at + 0.5);
    tl.fromTo(tag, { opacity: 0 }, { opacity: 0.9, duration: 0.05, immediateRender: false }, at + 0.3);
    tl.to(tag, { opacity: 0, duration: 0.05 }, at + 0.6);
    IG.glitch(tl, wrap, at + 0.42, 0.1, 30, 41);
  };

  /* T3 STYLISED TECH: rose halftone sweeps in, a steel field with brackets and a locking reticle, katakana scramble, iris reveal. */
  TR.t3 = function (tl, L, at) {
    var field = box(L, { inset: 0, background: C.bk, clipPath: "circle(0% at 50% 50%)" });
    var ht = box(L, { left: "-200px", top: "-200px", width: "2320px", height: "1480px", opacity: 0, backgroundImage: "radial-gradient(circle, " + C.rose + " 38%, transparent 40%)", backgroundSize: "26px 26px", transform: "rotate(-12deg)" });
    box(field, { inset: 0, backgroundImage: "radial-gradient(ellipse at 50% 50%, rgba(118,87,255,.35), rgba(9,11,18,0) 60%)" });
    box(field, { inset: 0, backgroundImage: "linear-gradient(rgba(100,232,255,.07) 1px, transparent 1px)", backgroundSize: "100% 4px" });
    IG.brackets(field, 60, 90, C.cy, 4);
    var ret = el("div", "", field, null, { position: "absolute", left: "810px", top: "390px", width: "300px", height: "300px", border: "3px solid " + C.cy, borderRadius: "50%" });
    var cross = el("div", "", ret, null, { position: "absolute", left: "-60px", top: "148px", width: "420px", height: "2px", background: C.cy });
    el("div", "", ret, null, { position: "absolute", left: "148px", top: "-60px", width: "2px", height: "420px", background: C.cy });
    var dot = el("div", "", ret, null, { position: "absolute", left: "130px", top: "130px", width: "40px", height: "40px", borderRadius: "50%", background: C.rose, boxShadow: "0 0 30px " + C.rose });
    var jp = el("div", "jp", field, "", { position: "absolute", left: "0", width: "1920px", textAlign: "center", top: "740px", fontSize: "64px", color: C.wh, letterSpacing: ".3em" });
    var code = el("div", "mono", field, "PAGE_SWAP // LOCK", { position: "absolute", left: "96px", top: "84px", fontSize: "26px", color: C.cy });
    // halftone sweep across along -12 degrees
    tl.fromTo(ht, { opacity: 1, xPercent: -110 }, { xPercent: 110, duration: 0.6, ease: "power2.inOut", immediateRender: false }, at);
    tl.set(ht, { opacity: 0 }, at + 0.61);
    tl.fromTo(field, { clipPath: "circle(0% at 50% 50%)" }, { clipPath: "circle(75% at 50% 50%)", duration: 0.32, ease: "power3.in", immediateRender: false }, at + 0.18);
    tl.fromTo(ret, { scale: 2.4, rotation: 90, opacity: 0 }, { scale: 1, rotation: 0, opacity: 1, duration: 0.3, ease: "expo.out", immediateRender: false }, at + 0.32);
    tl.fromTo(dot, { scale: 0 }, { scale: 1, duration: 0.2, ease: "back.out(3)", immediateRender: false }, at + 0.55);
    IG.scramble(tl, jp, "作戦開始", at + 0.36, 0.28, 9);
    tl.fromTo(field, { clipPath: "circle(75% at 50% 50%)" }, { clipPath: "circle(0% at 50% 50%)", duration: 0.5, ease: "expo.inOut", immediateRender: false }, at + 0.7);
  };

  /* T4 BB SIGNAL: rose slash, white impact frame, the rose ring draws and opens as an iris, katakana strips and speed lines. */
  TR.t4 = function (tl, L, at) {
    var cover = box(L, { inset: 0, background: C.bk, clipPath: "polygon(0% 0%, 0% 0%, 0% 100%, 0% 100%)" });
    box(cover, { inset: 0, background: "radial-gradient(ellipse at 50% 50%, rgba(255,61,141,.18), rgba(118,87,255,.18) 40%, rgba(9,11,18,0) 70%)" });
    var sp = IG.canvas("t4sp-" + (TR.n++), 1920, 1080, cover);
    IG.clock(tl, 0.9, function (t) { sp.clearRect(0, 0, 1920, 1080); if (t > 0) IG.speed(sp, at + t, { col: "rgba(255,61,141,.5)", n: 70, r0: 300 }); }, at + 0.2);
    var strips = [];
    for (var i = 0; i < 4; i++) {
      var s = el("div", "jp", cover, "オペレーター ネオ東京 司令官 天使 オペレーター ネオ東京 司令官 天使", { position: "absolute", left: "-200px", top: (120 + i * 250) + "px", fontSize: "34px", color: i % 2 ? C.cy : C.vio, opacity: .55, letterSpacing: ".4em" });
      strips.push(s); tl.fromTo(s, { x: i % 2 ? -300 : 0 }, { x: i % 2 ? 0 : -300, duration: 1.2, ease: "none", immediateRender: false }, at);
    }
    var rg = IG.ring(cover, 150, 22, C.rose, { left: "766px", top: "346px" });
    var word = el("div", "voice", cover, "<span class='bib-skew'>BOLDINBEAUTY</span>", { position: "absolute", left: 0, width: "1920px", textAlign: "center", top: "760px", fontSize: "96px", color: C.wh, letterSpacing: ".08em" });
    var slash = box(L, { left: "-300px", top: "470px", width: "2520px", height: "140px", background: C.rose, transform: "rotate(-12deg)", opacity: 0 });
    var white = box(L, { inset: 0, background: C.wh, opacity: 0 });
    var iris = box(L, { inset: 0, opacity: 0 });
    // slash crosses, cover wipes in behind it along the diagonal
    tl.fromTo(slash, { opacity: 1, xPercent: -100 }, { xPercent: 100, duration: 0.36, ease: "power3.inOut", immediateRender: false }, at);
    tl.set(slash, { opacity: 0 }, at + 0.37);
    tl.fromTo(cover, { clipPath: "polygon(0% 0%, 0% 0%, -25% 100%, -25% 100%)" }, { clipPath: "polygon(0% 0%, 125% 0%, 100% 100%, -25% 100%)", duration: 0.36, ease: "power3.inOut", immediateRender: false }, at + 0.02);
    tl.set(white, { opacity: 1 }, at + 0.38); tl.set(white, { opacity: 0 }, at + 0.38 + 2 / 30);
    IG.drawRing(tl, rg, at + 0.4, 0.3, 0, 1);
    tl.fromTo(rg.svg, { scale: 0.6, rotation: -90 }, { scale: 1, rotation: 0, duration: 0.35, ease: "back.out(1.8)", immediateRender: false }, at + 0.4);
    IG.slam(tl, word, at + 0.46, { x: 0, s: 1.4, d: 0.22 });
    IG.shake(tl, cover, at + 0.4, 0.3, 10, 3);
    // the ring opens into an iris that reveals the next page
    tl.to(rg.svg, { scale: 14, duration: 0.6, ease: "expo.in" }, at + 0.72);
    tl.to(word, { opacity: 0, scale: 1.5, duration: 0.25, ease: "power2.in" }, at + 0.72);
    tl.set(cover, { clipPath: "none" }, at + 0.7);
    var m = { r: 0 };
    tl.fromTo(m, { r: 0 }, { r: 120, duration: 0.62, ease: "expo.in", immediateRender: false, onUpdate: function () {
      var g = "radial-gradient(circle at 50% 50%, transparent " + m.r + "%, black " + (m.r + 0.5) + "%)";
      cover.style.webkitMaskImage = g; cover.style.maskImage = g;
    } }, at + 0.72);
    tl.set(cover, { opacity: 0 }, at + TR.DUR[4]);
  };

  /* T5 FULL BB: angel-geometry shards assemble into a cover, the wordmark slams with a violet offset and the ring locks into the O,
     a halftone pulse and a white impact frame, then the shards blast out along speed lines. */
  TR.t5 = function (tl, L, at) {
    var x = IG.canvas("t5-" + (TR.n++), 1920, 1080, L);
    // shard grid: triangles from a jittered lattice
    var cols = 9, rows = 6, pts = [], tris = [];
    for (var j = 0; j <= rows; j++) for (var i = 0; i <= cols; i++) {
      var px = i * 1920 / cols + ((i > 0 && i < cols) ? (H(i * 31 + j, 5) - 0.5) * 120 : 0);
      var py = j * 1080 / rows + ((j > 0 && j < rows) ? (H(i * 17 + j, 6) - 0.5) * 100 : 0);
      pts.push([px - 20 * (j / rows), py]);
    }
    for (j = 0; j < rows; j++) for (i = 0; i < cols; i++) {
      var a = j * (cols + 1) + i, b = a + 1, c = a + cols + 1, d = c + 1;
      tris.push([a, b, d]); tris.push([a, d, c]);
    }
    var cx = 960, cy = 540;
    var o = { t: 0 };
    function draw() {
      var t = o.t; x.clearRect(0, 0, 1920, 1080);
      var inP = IG.inv(0, 0.5, t), outP = IG.inv(0.95, 1.55, t);
      if (t <= 0 || t >= 1.6) return;
      if (outP > 0) IG.speed(x, at + t, { col: "rgba(247,244,250," + (0.5 * (1 - outP)).toFixed(2) + ")", n: 80, r0: 200 + 900 * outP });
      for (var k = 0; k < tris.length; k++) {
        var tr = tris[k], A = pts[tr[0]], B = pts[tr[1]], D = pts[tr[2]];
        var mx = (A[0] + B[0] + D[0]) / 3, my = (A[1] + B[1] + D[1]) / 3;
        var dly = H(k, 21) * 0.25, p = IG.ocub(IG.inv(dly, dly + 0.28, t)), q = Math.pow(IG.inv(0.95 + H(k, 22) * 0.15, 1.45 + H(k, 22) * 0.1, t), 2);
        if (p <= 0) continue;
        var dx = mx - cx, dy = my - cy, dist = Math.sqrt(dx * dx + dy * dy) + 1;
        var off = (1 - p) * (700 + 500 * H(k, 23)) + q * (1600 + 600 * H(k, 24));
        var rot = (1 - p) * (H(k, 25) - 0.5) * 3 + q * (H(k, 26) - 0.5) * 5;
        var sc = 1 - 0.25 * q;
        x.save(); x.translate(mx + dx / dist * off, my + dy / dist * off); x.rotate(rot); x.scale(sc, sc);
        x.beginPath(); x.moveTo(A[0] - mx, A[1] - my); x.lineTo(B[0] - mx, B[1] - my); x.lineTo(D[0] - mx, D[1] - my); x.closePath();
        var shade = H(k, 27);
        x.fillStyle = shade > 0.86 ? C.rose : shade > 0.68 ? "#241f45" : shade > 0.3 ? C.st : C.bk; x.fill();
        x.strokeStyle = shade > 0.8 ? "rgba(100,232,255,.9)" : "rgba(100,232,255,.35)"; x.lineWidth = 1.5; x.stroke();
        x.restore();
      }
    }
    tl.fromTo(o, { t: 0 }, { t: 1.6, duration: 1.6, ease: "none", immediateRender: false, onUpdate: draw }, at);
    tl.set(o, { t: 1.6, onComplete: draw }, at + 1.6);
    var ht = box(L, { inset: 0, opacity: 0, backgroundImage: "radial-gradient(circle, rgba(255,61,141,.55) 36%, transparent 40%)", backgroundSize: "22px 22px" });
    var mark = el("div", "", L, null, { position: "absolute", left: 0, top: "390px", width: "1920px", height: "300px", opacity: 0 });
    var w = el("div", "title", mark, "<span class='bib-skew'>B<span class='o'>O</span>LDINBEAUTY</span>", { position: "absolute", left: 0, width: "1920px", textAlign: "center", top: "60px", fontSize: "180px", color: C.rose, textShadow: "10px 9px 0 " + C.vio });
    var tg = el("div", "mono", mark, "AN ORIGINAL 3D ANIME GACHA GAME", { position: "absolute", left: 0, width: "1920px", textAlign: "center", top: "250px", fontSize: "28px", color: C.wh, letterSpacing: ".3em" });
    var white = box(L, { inset: 0, background: C.wh, opacity: 0 });
    tl.fromTo(ht, { opacity: 0 }, { opacity: 0.9, duration: 0.1, immediateRender: false }, at + 0.45);
    tl.to(ht, { opacity: 0, duration: 0.3 }, at + 0.6);
    tl.set(mark, { opacity: 1 }, at + 0.5);
    IG.slam(tl, w, at + 0.5, { x: -260, s: 1.3, d: 0.26, ir: false });
    IG.rise(tl, tg, at + 0.66, 0.2, 14);
    tl.set(white, { opacity: 1 }, at + 0.5); tl.set(white, { opacity: 0 }, at + 0.5 + 2 / 30);
    IG.shake(tl, mark, at + 0.52, 0.35, 14, 8);
    IG.glitch(tl, mark, at + 0.88, 0.08, 40, 77);
    tl.to(mark, { opacity: 0, scale: 1.25, duration: 0.15, ease: "power2.in" }, at + 0.95);
  };
  root.TR = TR;
})(window);
