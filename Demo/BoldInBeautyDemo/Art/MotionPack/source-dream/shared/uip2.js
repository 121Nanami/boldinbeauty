/* uip2.js: UI pack sheets 03 (summon) and 04 (combat HUD). No character art anywhere: summons reveal rarity crystals and stars. */
(function (root) {
  var C = IG.C, el = IG.el, H = IG.hash, UIP = root.UIP, A = UIP.A, T = UIP.T, cut = UIP.cut;
  UIP.SHEETS["03"] = "SUMMON"; UIP.SHEETS["04"] = "COMBAT HUD";
  function def(id, sheet, n, title, fn) { UIP[id] = { sheet: sheet, n: n, title: title, fn: fn }; }
  var RAR = { 3: ["RARE", C.cy, "#0f2a33"], 4: ["EPIC", C.vio, "#1d1640"], 5: ["LEGENDARY", C.rose, "#3a0f22"], 6: ["SECRET", C.wh, "#2a2440"] };
  function stars(parent, n, size, col, css) {
    var w = A(parent, Object.assign({ height: size + "px", width: (n * size * 1.05) + "px" }, css || {}));
    var arr = [];
    for (var i = 0; i < n; i++) arr.push(el("div", "", w, '<svg viewBox="0 0 100 100" width="' + size + '" height="' + size + '"><polygon points="50,4 62,36 96,38 69,59 79,93 50,73 21,93 31,59 4,38 38,36" fill="' + col + '" stroke="#090B12" stroke-width="5"/></svg>',
      { position: "absolute", left: (i * size * 1.05) + "px", top: "0" }));
    w.arr = arr; return w;
  }
  UIP.stars = stars;
  /* crystal: the summon "drop" object (a faceted angel-geometry gem) */
  function crystal(parent, size, col, css) {
    var s = el("div", "", parent, '<svg viewBox="-60 -80 120 160" width="' + size + '" height="' + (size * 4 / 3) + '" style="overflow:visible">' +
      '<polygon points="0,-76 52,-20 0,76 -52,-20" fill="' + col + '" opacity=".22" stroke="' + col + '" stroke-width="4"/>' +
      '<polyline points="-52,-20 0,-6 52,-20 M0,-6 0,76 M0,-76 0,-6" fill="none" stroke="' + col + '" stroke-width="3" opacity=".9"/>' +
      '<polygon points="0,-76 52,-20 0,-6" fill="' + col + '" opacity=".55"/></svg>', Object.assign({ position: "absolute", filter: "drop-shadow(0 0 24px " + col + ")" }, css || {}));
    return s;
  }
  UIP.crystal = crystal;

  /* 14 BANNER SELECT */
  def("banner-select", "03", 1, "BANNER SELECT", function (tl, L) {
    var names = [["STANDARD", C.cy], ["FEATURED", C.rose], ["DAILY", C.vio]];
    var tabs = names.map(function (n, i) {
      var b = A(L, { left: "1300px", top: (260 + i * 150) + "px", width: "500px", height: "120px", background: "rgba(21,25,37,.92)", border: "2px solid rgba(100,232,255,.4)", borderLeft: "10px solid rgba(247,244,250,.25)", clipPath: cut(18) });
      b.lab = T(b, "voice", n[0], { left: "30px", top: "18px", fontSize: "84px", color: "rgba(247,244,250,.5)" });
      tl.fromTo(b, { opacity: 0, x: 200 }, { opacity: 1, x: 0, duration: 0.3, ease: "expo.out" }, 0.1 + i * 0.08);
      return b;
    });
    var card = A(L, { left: "120px", top: "200px", width: "1120px", height: "680px", overflow: "hidden", border: "3px solid " + C.cy, background: "#151925" });
    var panes = names.map(function (n, i) {
      var p = A(card, { left: "0", top: "0", width: "1120px", height: "680px", background: "radial-gradient(ellipse at 70% 40%, " + n[1] + "55, rgba(21,25,37,0) 60%), #151925" });
      A(p, { left: "-200px", top: "300px", width: "1600px", height: "180px", background: n[1], opacity: .85, transform: "rotate(-12deg)" });
      T(p, "voice", "<span class='bib-skew'>" + n[0] + "</span>", { left: "60px", top: "90px", fontSize: "230px", color: C.wh, textShadow: "8px 7px 0 #090B12" });
      T(p, "mono", ["PERMANENT POOL", "LIMITED SPOTLIGHT", "ONE FREE PULL A DAY"][i], { left: "70px", top: "560px", fontSize: "30px", color: C.wh });
      IG.ring(p, 110, 18, i === 1 ? C.wh : n[1], { left: "800px", top: "80px" });
      return p;
    });
    tl.fromTo(card, { opacity: 0, x: -100 }, { opacity: 1, x: 0, duration: 0.4, ease: "expo.out" }, 0);
    gsap.set(panes[1], { xPercent: 100 }); gsap.set(panes[2], { xPercent: 100 });
    var order = [0, 1, 2], t0 = 0.6;
    order.forEach(function (k, j) {
      var t = t0 + j * 1.3;
      tabs.forEach(function (b, i) {
        tl.to(b, { borderLeftColor: i === k ? names[k][1] : "rgba(247,244,250,.25)", x: i === k ? -30 : 0, duration: 0.2 }, t);
        tl.to(b.lab, { color: i === k ? C.wh : "rgba(247,244,250,.5)", duration: 0.2 }, t);
      });
      if (j > 0) { tl.to(panes[order[j - 1]], { xPercent: -100, duration: 0.45, ease: "expo.inOut" }, t); tl.to(panes[k], { xPercent: 0, duration: 0.45, ease: "expo.inOut" }, t); }
    });
    tl.to(tabs.concat([card]), { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 15 SUMMON BUTTON: hold to charge, burst */
  def("summon-charge", "03", 2, "SUMMON BUTTON CHARGE", function (tl, L) {
    var rg = IG.ring(L, 230, 14, C.rose, { left: "690px", top: "270px" });
    var base = IG.ring(L, 230, 4, "rgba(100,232,255,.4)", { left: "710px", top: "290px" }); base.circle.style.filter = "none";
    var b = A(L, { left: "760px", top: "460px", width: "400px", height: "160px", background: C.rose, clipPath: cut(26) });
    T(b, "voice", "SUMMON", { left: "0", width: "400px", textAlign: "center", top: "22px", fontSize: "120px", color: C.wh });
    var hint = T(L, "mono", "HOLD TO SUMMON", { left: "0", width: "1920px", textAlign: "center", top: "830px", fontSize: "30px", color: C.cy });
    var white = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: C.wh, opacity: 0 });
    var x = IG.canvas("scsp-" + (UIP.k = (UIP.k || 0) + 1), 1920, 1080, L);
    IG.clock(tl, 1.5, function (t) { x.clearRect(0, 0, 1920, 1080); if (t > 0 && t < 1.48) IG.speed(x, t, { col: "rgba(247,244,250,.6)", n: 80, r0: 200 + 900 * t }); }, 3.1);
    IG.pop(tl, b, 0.1); IG.rise(tl, hint, 0.3);
    tl.fromTo([rg.svg, base.svg], { opacity: 0 }, { opacity: 1, duration: 0.2 }, 0.2);
    tl.fromTo(rg.circle, { attr: { "stroke-dashoffset": rg.L } }, { attr: { "stroke-dashoffset": 0 }, duration: 2.0, ease: "power2.in" }, 1.0);
    tl.to(b, { scale: 0.9, duration: 0.15 }, 1.0);
    IG.shake(tl, b, 1.8, 1.2, 8, 3);
    tl.to(rg.svg, { scale: 1.08, duration: 0.1, yoyo: true, repeat: 9, ease: "none" }, 2.0);
    tl.fromTo(white, { opacity: 0 }, { opacity: 1, duration: 0.1, immediateRender: false }, 3.0);
    tl.to(white, { opacity: 0, duration: 0.6, ease: "power2.out" }, 3.15);
    tl.to(rg.svg, { scale: 3, opacity: 0, duration: 0.5, ease: "expo.out" }, 3.05);
    tl.to([b, base.svg, hint], { opacity: 0, duration: 0.1 }, 3.05);
  });

  /* 16-19 RARITY REVEALS */
  function reveal(n) {
    return function (tl, L) {
      var R = RAR[n], col = R[1];
      var bg = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: "radial-gradient(ellipse at 50% 50%, " + col + "44, rgba(9,11,18,0) 60%)", opacity: 0 });
      IG.raw(function () { bg.style.background = "radial-gradient(ellipse at 50% 38%, #FFFFFF 0%, #FBF6FF 40%, #EDE3FB 100%)"; });
      var x = IG.canvas("rv" + n + "-" + (UIP.k = (UIP.k || 0) + 1), 1920, 1080, L);
      var cr = crystal(L, 240, col, { left: "840px", top: "220px" });
      var st = stars(L, n, 90, n === 6 ? C.rose : col, { left: (960 - n * 90 * 1.05 / 2) + "px", top: "600px" });
      var nm = T(L, "voice", "<span class='bib-skew'>" + R[0] + "</span>", { left: "0", width: "1920px", textAlign: "center", top: "700px", fontSize: n >= 5 ? "200px" : "170px", color: n === 6 ? C.rose : C.wh, textShadow: "8px 7px 0 " + (n === 6 ? C.vio : col) });
      var white = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: n === 6 ? C.rose : C.wh, opacity: 0 });
      // rays / speed burst grows with rarity
      IG.clock(tl, 4.4, function (t) {
        x.clearRect(0, 0, 1920, 1080); if (t <= 0.9) return;
        var k = Math.min(1, (t - 0.9) * 2), N = 10 + n * 6;
        x.save(); x.translate(960, 380); x.rotate(t * 0.4 * (n - 2));
        for (var i = 0; i < N; i++) { x.rotate(Math.PI * 2 / N); var c = n === 6 ? [C.rose, "#FFF8F0", C.vio, "#FFF8F0", C.cy, "#FFF8F0"][i % 6] : (i % 2 ? "#FFF8F0" : col);
          x.fillStyle = c; x.globalAlpha = (i % 2 ? 0.42 : 0.36) * k; x.beginPath(); x.moveTo(0, 0); x.lineTo(1400, -40 - n * 6); x.lineTo(1400, 40 + n * 6); x.closePath(); x.fill(); }
        x.restore(); x.globalAlpha = 1;
        if (n >= 5 && t < 2.2) IG.speed(x, t, { cx: 960, cy: 380, col: "rgba(247,244,250,.5)", n: 40 + n * 10, r0: 200 + 500 * (t - 0.9) });
      }, 0);
      tl.fromTo(cr, { y: 500, opacity: 0, scale: 0.4 }, { y: 0, opacity: 1, scale: 1, duration: 0.6, ease: "expo.out" }, 0.1);
      tl.fromTo(cr, { rotation: -8 }, { rotation: 8, duration: 0.12, repeat: 5, yoyo: true, ease: "none", immediateRender: false }, 0.5);
      if (n === 6) IG.glitch(tl, cr, 0.6, 0.3, 40, 66);
      tl.fromTo(white, { opacity: 0 }, { opacity: 1, duration: 0.05, immediateRender: false }, 0.9);
      tl.to(white, { opacity: 0, duration: n >= 5 ? 0.6 : 0.35, ease: "power2.out" }, 0.95);
      tl.to(bg, { opacity: 1, duration: 0.3 }, 0.9);
      tl.to(cr, { scale: 1.25, duration: 0.25, ease: "back.out(3)" }, 0.9);
      st.arr.forEach(function (s, i) { tl.fromTo(s, { scale: 0, rotation: -90, opacity: 0 }, { scale: 1, rotation: 0, opacity: 1, duration: 0.25, ease: "back.out(3)" }, 1.1 + i * 0.12); });
      IG.slam(tl, nm, 1.2 + n * 0.12, { x: 0, s: 1.8 });
      if (n >= 5) IG.shake(tl, L, 0.92, 0.5, n * 4, n);
      tl.to(cr, { y: -14, duration: 1.2, yoyo: true, repeat: 1, ease: "sine.inOut" }, 1.3);
      tl.to([cr, st, nm, bg], { opacity: 0, duration: 0.3 }, 4.6);
    };
  }
  def("reveal-rare", "03", 3, "RARITY REVEAL // 3 STAR RARE", reveal(3));
  def("reveal-epic", "03", 4, "RARITY REVEAL // 4 STAR EPIC", reveal(4));
  def("reveal-legendary", "03", 5, "RARITY REVEAL // 5 STAR LEGENDARY", reveal(5));
  def("reveal-secret", "03", 6, "RARITY REVEAL // 6 STAR SECRET", reveal(6));

  /* 20 X10 RESULTS */
  def("summon-x10", "03", 7, "X10 RESULTS GRID", function (tl, L) {
    var res = [3, 3, 4, 3, 5, 3, 4, 3, 3, 6];
    T(L, "voice", "RESULTS", { left: "130px", top: "110px", fontSize: "110px", color: C.wh });
    res.forEach(function (r, i) {
      var col = RAR[r][1], x = 130 + (i % 5) * 340, y = 260 + Math.floor(i / 5) * 360;
      var card = A(L, { left: x + "px", top: y + "px", width: "300px", height: "330px", perspective: "800px" });
      var back = A(card, { left: "0", top: "0", width: "300px", height: "330px", background: "repeating-linear-gradient(-12deg, #151925 0 18px, #1d2233 18px 36px)", border: "3px solid rgba(100,232,255,.5)" });
      IG.ring(back, 50, 10, C.rose, { left: "80px", top: "95px" });
      var face = A(card, { left: "0", top: "0", width: "300px", height: "330px", background: "radial-gradient(ellipse at 50% 40%, " + col + "55, #151925 70%)", border: "4px solid " + col, boxShadow: "0 0 " + (r * 6) + "px " + col, opacity: 0 });
      crystal(face, 110, col, { left: "95px", top: "40px" });
      stars(face, r, 36, r === 6 ? C.rose : col, { left: (150 - r * 36 * 1.05 / 2) + "px", top: "210px" });
      T(face, "voice", RAR[r][0], { left: "0", width: "300px", textAlign: "center", top: "254px", fontSize: "54px", color: C.wh });
      tl.fromTo(card, { opacity: 0, y: 80 }, { opacity: 1, y: 0, duration: 0.25, ease: "back.out(2)" }, 0.1 + i * 0.05);
      var t = 1.0 + i * 0.22 + (r >= 5 ? 0.15 : 0);
      tl.to(back, { scaleX: 0, duration: 0.12, ease: "power2.in" }, t);
      tl.fromTo(face, { scaleX: 0, opacity: 1 }, { scaleX: 1, duration: r >= 5 ? 0.3 : 0.15, ease: "back.out(2)", immediateRender: false }, t + 0.12);
      if (r >= 5) IG.shake(tl, card, t + 0.15, 0.3, 10, i);
      tl.to(card, { opacity: 0, duration: 0.2 }, 4.6);
    });
  });

  /* 21 TICKET USE */
  def("ticket-use", "03", 8, "TICKET CONSUME", function (tl, L) {
    var tk = A(L, { left: "660px", top: "330px", width: "600px", height: "360px" });
    var lft = A(tk, { left: "0", top: "0", width: "600px", height: "360px", clipPath: "inset(0 50% 0 0)" }); UI.icon("ticket", 600, lft, { left: "0", top: "-120px" });
    var rgt = A(tk, { left: "0", top: "0", width: "600px", height: "360px", clipPath: "inset(0 0 0 50%)" }); UI.icon("ticket", 600, rgt, { left: "0", top: "-120px" });
    var cnt = T(L, "title", "x10", { left: "0", width: "1920px", textAlign: "center", top: "730px", fontSize: "110px", color: C.wh });
    var lab = T(L, "mono", "REGULAR TICKET", { left: "0", width: "1920px", textAlign: "center", top: "260px", fontSize: "30px", color: C.rose });
    IG.pop(tl, tk, 0.1); IG.rise(tl, cnt, 0.3); IG.rise(tl, lab, 0.3);
    tl.to(tk, { rotation: -4, duration: 0.08, yoyo: true, repeat: 5 }, 1.2);
    tl.to(lft, { x: -120, y: 60, rotation: -18, opacity: 0, duration: 0.6, ease: "power2.in" }, 1.8);
    tl.to(rgt, { x: 120, y: 60, rotation: 18, opacity: 0, duration: 0.6, ease: "power2.in" }, 1.8);
    IG.textAt(tl, cnt, [[0, "x10"], [2.1, "x9"]], 5);
    tl.fromTo(cnt, { scale: 1 }, { scale: 1.3, color: C.rose, duration: 0.12, yoyo: true, repeat: 1, immediateRender: false }, 2.1);
    tl.to([cnt, lab], { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* ---------------- SHEET 04: COMBAT HUD ---------------- */
  /* 22 HEALTH BAR */
  def("hp-bar", "04", 1, "HEALTH BAR // DAMAGE, HEAL, LOW HP", function (tl, L) {
    var w = A(L, { left: "460px", top: "480px", width: "1000px", height: "120px" });
    T(w, "mono", "HP", { left: "0", top: "0", fontSize: "28px", color: C.cy });
    var num = T(w, "mono", "4,800 / 4,800", { right: "0", top: "0", fontSize: "28px", color: C.wh, fontWeight: 700 });
    var tr = A(w, { left: "0", top: "44px", width: "1000px", height: "46px", background: "rgba(21,25,37,.95)", border: "2px solid rgba(100,232,255,.5)", transform: "skewX(-20deg)" });
    var trail = A(tr, { left: "0", top: "0", height: "42px", width: "996px", background: C.wh });
    var fill = A(tr, { left: "0", top: "0", height: "42px", width: "996px", background: "linear-gradient(180deg, #ff6aa9, #FF3D8D 60%, #c41f63)" });
    for (var i = 1; i < 10; i++) A(tr, { left: (i * 100) + "px", top: "0", width: "2px", height: "42px", background: "rgba(9,11,18,.5)" });
    var warn = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", boxShadow: "inset 0 0 180px rgba(255,61,141,.9)", opacity: 0 });
    tl.fromTo(w, { opacity: 0, y: 40 }, { opacity: 1, y: 0, duration: 0.3 }, 0);
    var hp = { v: 4800 };
    function hit(t, to, d) { tl.to(fill, { width: 996 * to / 4800, duration: 0.05 }, t); tl.to(trail, { width: 996 * to / 4800, duration: 0.5, ease: "power2.in" }, t + 0.35); IG.shake(tl, w, t, 0.25, 12, t * 10); IG.count(tl, num, d, to, t, 0.2, function (v) { return IG.commas(v) + " / 4,800"; }); }
    hit(0.6, 3400, 4800); hit(1.3, 2100, 3400); hit(2.0, 800, 2100);
    tl.fromTo(warn, { opacity: 0 }, { opacity: 1, duration: 0.25, yoyo: true, repeat: 3, immediateRender: false }, 2.1);
    tl.to(fill, { width: 996 * 3600 / 4800, background: "linear-gradient(180deg, #8ff1ff, #64E8FF 60%, #2bb3cc)", duration: 0.7, ease: "power2.out" }, 3.3);
    tl.to(trail, { width: 996 * 3600 / 4800, duration: 0.7, ease: "power2.out" }, 3.3);
    IG.count(tl, num, 800, 3600, 3.3, 0.7, function (v) { return IG.commas(v) + " / 4,800"; });
    var heal = T(L, "voice", "+2,800", { left: "1200px", top: "380px", fontSize: "90px", color: C.cy, opacity: 0 });
    tl.fromTo(heal, { opacity: 0, y: 30 }, { opacity: 1, y: 0, duration: 0.2, immediateRender: false }, 3.3); tl.to(heal, { opacity: 0, y: -40, duration: 0.5 }, 3.9);
    tl.to(w, { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 23 SQUAD SWITCHER: 3 operators, 1 active */
  def("squad-switch", "04", 2, "SQUAD SWITCHER // 3 OPERATORS, 1 IN THE FIGHT", function (tl, L) {
    var slots = [];
    for (var i = 0; i < 3; i++) {
      var s = A(L, { left: (560 + i * 280) + "px", top: "620px", width: "240px", height: "240px", background: "rgba(21,25,37,.95)", border: "3px solid rgba(100,232,255,.5)", clipPath: "polygon(0 0, 100% 0, 100% 80%, 80% 100%, 0 100%)" });
      T(s, "title", "0" + (i + 1), { left: "26px", top: "18px", fontSize: "90px", color: "rgba(247,244,250,.85)" });
      stars(s, [5, 4, 3][i], 28, [C.rose, C.vio, C.cy][i], { left: "28px", top: "120px" });
      var hpT = A(s, { left: "26px", top: "176px", width: "186px", height: "12px", background: "rgba(100,232,255,.2)" });
      A(hpT, { left: "0", top: "0", height: "12px", width: [186, 140, 90][i] + "px", background: C.rose });
      slots.push(s);
      tl.fromTo(s, { opacity: 0, y: 80 }, { opacity: 1, y: 0, duration: 0.3, ease: "back.out(2)" }, 0.1 + i * 0.08);
    }
    var act = T(L, "mono", "ACTIVE", { left: "560px", top: "570px", fontSize: "28px", color: C.rose, fontWeight: 700 });
    var sw = A(L, { left: "1420px", top: "640px", width: "200px", height: "200px", borderRadius: "50%", border: "4px solid " + C.cy, background: "rgba(21,25,37,.9)" });
    T(sw, "voice", "SWITCH", { left: "0", width: "200px", textAlign: "center", top: "62px", fontSize: "70px", color: C.wh });
    tl.fromTo(sw, { opacity: 0, scale: 0.5 }, { opacity: 1, scale: 1, duration: 0.3, ease: "back.out(2)" }, 0.4);
    var x = IG.canvas("ssw-" + (UIP.k = (UIP.k || 0) + 1), 1920, 1080, L);
    var white = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: C.wh, opacity: 0 });
    function activate(k, t) {
      slots.forEach(function (s, i) { tl.to(s, { y: i === k ? -40 : 0, borderColor: i === k ? C.rose : "rgba(100,232,255,.5)", scale: i === k ? 1.08 : 1, duration: 0.25, ease: "back.out(2)" }, t); });
      tl.to(act, { x: k * 280, duration: 0.25, ease: "expo.out" }, t);
    }
    gsap.set(slots[0], { y: -40, scale: 1.08, borderColor: C.rose });
    [[1, 1.4], [2, 2.6], [0, 3.8]].forEach(function (q) {
      tl.to(sw, { scale: 0.88, duration: 0.07 }, q[1] - 0.1); tl.to(sw, { scale: 1, rotation: "+=360", duration: 0.35, ease: "back.out(2)" }, q[1] - 0.03);
      activate(q[0], q[1]);
      tl.fromTo(white, { opacity: 0.6 }, { opacity: 0, duration: 0.2, immediateRender: false }, q[1]);
    });
    IG.clock(tl, 5, function (t) { x.clearRect(0, 0, 1920, 1080); [1.4, 2.6, 3.8].forEach(function (s) { var u = t - s; if (u > 0 && u < 0.35) IG.speed(x, t, { cx: 960, cy: 540, col: "rgba(255,61,141," + (0.6 * (1 - u / 0.35)).toFixed(2) + ")", n: 50, r0: 300 }); }); }, 0);
    tl.to(slots.concat([sw, act]), { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 24 SKILL BUTTONS with cooldown sweeps */
  def("skill-buttons", "04", 3, "SKILLS // 3 MOVES, SPECIAL, ULTIMATE", function (tl, L) {
    var defs = [["MOVE 1", 160, 1120, 700, C.cy, 1.0], ["MOVE 2", 160, 1300, 520, C.cy, 1.6], ["MOVE 3", 160, 1500, 560, C.cy, 2.2], ["SPECIAL", 190, 1290, 800, C.vio, 2.8], ["ULTIMATE", 230, 1560, 820, C.rose, 3.6]];
    var ns = "http://www.w3.org/2000/svg";
    defs.forEach(function (d, i) {
      var sz = d[1], b = A(L, { left: (d[2] - 340) + "px", top: (d[3] - 260) + "px", width: sz + "px", height: sz + "px" });
      var cir = A(b, { left: "0", top: "0", width: sz + "px", height: sz + "px", borderRadius: "50%", background: "rgba(21,25,37,.95)", border: "4px solid " + d[4], boxShadow: "0 0 20px " + d[4] + "88" });
      T(b, "voice", d[0], { left: "0", width: sz + "px", textAlign: "center", top: (sz / 2 - 30) + "px", fontSize: (i > 2 ? 52 : 46) + "px", color: C.wh });
      var svg = document.createElementNS(ns, "svg"); svg.setAttribute("viewBox", "-50 -50 100 100"); svg.setAttribute("width", sz); svg.setAttribute("height", sz); svg.style.position = "absolute"; svg.style.left = "0"; svg.style.top = "0"; b.appendChild(svg);
      var pie = document.createElementNS(ns, "path"); pie.setAttribute("fill", "rgba(9,11,18,.72)"); svg.appendChild(pie);
      var cd = T(b, "mono", "", { left: "0", width: sz + "px", textAlign: "center", top: (sz / 2 + 20) + "px", fontSize: "28px", color: C.wh, fontWeight: 700 });
      tl.fromTo(b, { opacity: 0, scale: 0.4 }, { opacity: 1, scale: 1, duration: 0.3, ease: "back.out(2.5)" }, 0.1 + i * 0.07);
      var press = d[5], dur = 1.4 + i * 0.1, o = { p: 0 };
      tl.to(b, { scale: 0.86, duration: 0.07 }, press); tl.to(b, { scale: 1, duration: 0.3, ease: "back.out(3)" }, press + 0.07);
      tl.fromTo(cir, { boxShadow: "0 0 80px " + d[4] }, { boxShadow: "0 0 20px " + d[4] + "88", duration: 0.4, immediateRender: false }, press);
      tl.fromTo(o, { p: 1 }, { p: 0, duration: dur, ease: "none", immediateRender: false, onUpdate: function () {
        var a = o.p * Math.PI * 2;
        pie.setAttribute("d", o.p <= 0.001 ? "" : o.p >= 0.999 ? "M0 -50 A50 50 0 1 1 -0.01 -50 Z" : "M0 0 L0 -50 A50 50 0 " + (a > Math.PI ? 1 : 0) + " 1 " + (50 * Math.sin(a)).toFixed(2) + " " + (-50 * Math.cos(a)).toFixed(2) + " Z");
        cd.textContent = o.p > 0.001 ? (o.p * dur * 3).toFixed(1) : "";
      } }, press + 0.05);
      tl.to(b, { opacity: 0, scale: 0.6, duration: 0.2 }, 4.6 + i * 0.03);
    });
  });

  /* 25 ULTIMATE READY */
  def("ultimate-ready", "04", 4, "ULTIMATE CHARGE // READY", function (tl, L) {
    var rg = IG.ring(L, 150, 16, C.rose, { left: "766px", top: "246px" });
    var bg = IG.ring(L, 150, 16, "rgba(118,87,255,.35)", { left: "766px", top: "246px" }); bg.circle.style.filter = "none";
    L.insertBefore(bg.svg, rg.svg);
    var pc = T(L, "title", "0%", { left: "0", width: "1920px", textAlign: "center", top: "390px", fontSize: "80px", color: C.wh });
    var rd = T(L, "voice", "<span class='bib-skew'>ULTIMATE READY</span>", { left: "0", width: "1920px", textAlign: "center", top: "680px", fontSize: "170px", color: C.rose, textShadow: "8px 7px 0 #7657FF", opacity: 0 });
    var x = IG.canvas("ult-" + (UIP.k = (UIP.k || 0) + 1), 1920, 1080, L);
    tl.fromTo([rg.svg, bg.svg], { opacity: 0, scale: 0.6 }, { opacity: 1, scale: 1, duration: 0.3, ease: "back.out(2)" }, 0);
    tl.fromTo(rg.circle, { attr: { "stroke-dashoffset": rg.L } }, { attr: { "stroke-dashoffset": 0 }, duration: 2.2, ease: "power1.in" }, 0.2);
    IG.count(tl, pc, 0, 100, 0.2, 2.2, function (v) { return Math.round(v) + "%"; }, "power1.in");
    tl.to(rg.svg, { scale: 1.3, duration: 0.12, yoyo: true, repeat: 1 }, 2.4);
    IG.slam(tl, rd, 2.45, { x: 0, s: 1.8, ir: false });
    IG.shake(tl, L, 2.45, 0.4, 14, 5);
    IG.clock(tl, 2.5, function (t) { x.clearRect(0, 0, 1920, 1080); if (t > 0) { var k = (t % 0.8) / 0.8; x.strokeStyle = "rgba(255,61,141," + (0.8 * (1 - k)).toFixed(2) + ")"; x.lineWidth = 6; x.beginPath(); x.arc(960, 400, 170 + 260 * k, 0, 6.283); x.stroke(); } }, 2.45);
    tl.to(rg.svg, { rotation: 360, duration: 2.2, ease: "none" }, 2.5);
    tl.to([rg.svg, bg.svg, pc, rd], { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 26 DAMAGE NUMBERS */
  def("damage-numbers", "04", 5, "DAMAGE NUMBERS // CRITICAL", function (tl, L) {
    var hits = [[0.3, 1240, 840, 420, false], [0.6, 980, 900, 360, false], [0.9, 1420, 760, 380, false], [1.4, 1110, 820, 300, false], [1.9, 9876, 960, 380, true], [2.8, 1530, 700, 460, false], [3.1, 1720, 1200, 420, false], [3.5, 12450, 900, 320, true]];
    hits.forEach(function (h, i) {
      var crit = h[4];
      var d = T(L, crit ? "title" : "voice", IG.commas(h[1]), { left: (h[2] - 200) + "px", width: "400px", textAlign: "center", top: h[3] + "px", fontSize: crit ? "150px" : "96px", color: crit ? C.rose : C.wh, textShadow: crit ? "7px 6px 0 #7657FF" : "4px 4px 0 #090B12", opacity: 0 });
      if (crit) T(d, "mono", "CRITICAL", { left: "0", width: "400px", textAlign: "center", top: "-34px", fontSize: "30px", color: C.cy, fontWeight: 700 });
      var dx = (H(i, 3) - 0.5) * 220;
      tl.fromTo(d, { opacity: 0, scale: crit ? 2.6 : 1.6, x: 0, y: 0 }, { opacity: 1, scale: 1, duration: crit ? 0.2 : 0.14, ease: "back.out(3)", immediateRender: false }, h[0]);
      tl.to(d, { x: dx, y: -120, duration: 0.9, ease: "power1.out" }, h[0] + 0.15);
      tl.to(d, { opacity: 0, duration: 0.25 }, h[0] + 0.85);
      if (crit) IG.shake(tl, d, h[0], 0.3, 14, i);
    });
  });

  /* 27 BOSS INTRO BAR */
  def("boss-intro", "04", 6, "BOSS INTRO // HP BAR", function (tl, L) {
    var stripe = A(L, { left: "-100px", top: "400px", width: "2120px", height: "200px", background: "repeating-linear-gradient(-45deg, #FF3D8D 0 40px, #090B12 40px 80px)", transform: "rotate(-4deg)", opacity: 0 });
    var w = T(L, "voice", "<span class='bib-skew'>WARNING</span>", { left: "0", width: "1920px", textAlign: "center", top: "390px", fontSize: "220px", color: C.wh, textShadow: "0 0 30px #FF3D8D", opacity: 0 });
    var bar = A(L, { left: "360px", top: "120px", width: "1200px", height: "120px", opacity: 0 });
    T(bar, "voice", "BOSS", { left: "0", top: "0", fontSize: "72px", color: C.rose });
    T(bar, "mono", "CHAPTER 01", { right: "0", top: "20px", fontSize: "26px", color: C.cy });
    var tr = A(bar, { left: "0", top: "74px", width: "1200px", height: "34px", border: "2px solid " + C.rose, background: "rgba(21,25,37,.9)" });
    var segs = [];
    for (var i = 0; i < 3; i++) segs.push(A(tr, { left: (i * 399) + "px", top: "2px", width: "395px", height: "26px", background: [C.rose, C.vio, C.cy][i], transform: "scaleX(0)", transformOrigin: "0 50%" }));
    var x3 = T(bar, "title", "x3", { left: "1210px", top: "60px", fontSize: "56px", color: C.wh });
    tl.fromTo(stripe, { opacity: 0, scaleY: 0 }, { opacity: 1, scaleY: 1, duration: 0.2, ease: "expo.out" }, 0.1);
    tl.fromTo(stripe, { backgroundPosition: "0px 0px" }, { backgroundPosition: "226px 0px", duration: 2, ease: "none" }, 0.1);
    tl.fromTo(w, { opacity: 0 }, { opacity: 1, duration: 0.08, repeat: 7, yoyo: true, immediateRender: false }, 0.3);
    tl.to([stripe, w], { opacity: 0, scaleY: 0, duration: 0.2 }, 1.9);
    tl.to(bar, { opacity: 1, duration: 0.2 }, 2.0);
    segs.forEach(function (s, i) { tl.to(s, { scaleX: 1, duration: 0.45, ease: "power2.out" }, 2.1 + i * 0.35); });
    tl.fromTo(x3, { scale: 2.5, opacity: 0 }, { scale: 1, opacity: 1, duration: 0.25, ease: "back.out(3)" }, 3.2);
    tl.to(bar, { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 28 MINI-BOSS */
  def("miniboss-warning", "04", 7, "MINI-BOSS ALERT", function (tl, L) {
    var b = A(L, { left: "0", top: "420px", width: "1920px", height: "220px", background: "rgba(9,11,18,.85)", borderTop: "4px solid " + C.vio, borderBottom: "4px solid " + C.vio });
    var tx = T(b, "voice", "", { left: "0", width: "1920px", textAlign: "center", top: "20px", fontSize: "170px", color: C.wh });
    var tick = T(b, "jp", "警告 // MINI-BOSS APPROACHING // 警告 // MINI-BOSS APPROACHING // 警告 // MINI-BOSS APPROACHING // 警告", { left: "0", top: "-40px", fontSize: "24px", color: C.vio, letterSpacing: ".2em" });
    tl.fromTo(b, { scaleY: 0 }, { scaleY: 1, duration: 0.3, ease: "expo.out" }, 0.1);
    IG.scramble(tl, tx, "MINI-BOSS", 0.3, 0.6, 9);
    tl.fromTo(tick, { x: 0 }, { x: -800, duration: 4.5, ease: "none" }, 0);
    IG.glitch(tl, tx, 1.4, 0.2, 30, 3); IG.glitch(tl, tx, 2.8, 0.15, 30, 4);
    tl.to(b, { scaleY: 0, duration: 0.3, ease: "expo.in" }, 4.4);
  });

  /* 29 COMBO COUNTER */
  def("combo-counter", "04", 8, "COMBO COUNTER", function (tl, L) {
    var w = A(L, { left: "1240px", top: "300px", width: "560px", height: "360px" });
    var n = T(w, "title", "0", { left: "0", top: "0", fontSize: "220px", color: C.wh, textShadow: "8px 7px 0 #FF3D8D" });
    var h = T(w, "voice", "HITS", { left: "10px", top: "210px", fontSize: "90px", color: C.rose });
    var tb = A(w, { left: "10px", top: "320px", width: "420px", height: "10px", background: C.cy });
    tl.fromTo(w, { opacity: 0, x: 100 }, { opacity: 1, x: 0, duration: 0.25 }, 0.1);
    for (var i = 1; i <= 24; i++) {
      var t = 0.2 + i * 0.14 * (i > 12 ? 0.8 : 1);
      (function (i, t) {
        tl.fromTo(n, { scale: 1.35 }, { scale: 1, duration: 0.12, ease: "power2.out", immediateRender: false }, t);
        tl.fromTo(tb, { scaleX: 1 }, { scaleX: 0.3, transformOrigin: "0 50%", duration: 0.14, ease: "none", immediateRender: false }, t);
      })(i, t);
    }
    IG.textAt(tl, n, [[0, "0"]].concat(Array.apply(null, Array(24)).map(function (_, k) { var i = k + 1; return [0.2 + i * 0.14 * (i > 12 ? 0.8 : 1), String(i)]; })), 5);
    tl.to(tb, { scaleX: 0, duration: 0.6, ease: "none" }, 3.5);
    tl.to(w, { opacity: 0, x: 100, duration: 0.3 }, 4.2);
  });

  /* 30 BLOCK / PARRY / DODGE */
  def("defense-prompts", "04", 9, "BLOCK, PARRY & DODGE", function (tl, L) {
    var words = [["BLOCK", C.cy, 0.2], ["PARRY", C.rose, 1.7], ["DODGE", C.vio, 3.1]];
    words.forEach(function (w, i) {
      var g = A(L, { left: "0", top: "0", width: "1920px", height: "1080px" });
      var shield = A(g, { left: "860px", top: "320px", width: "200px", height: "240px", border: "8px solid " + w[1], borderRadius: i === 0 ? "10px 10px 100px 100px" : "50%", opacity: 0 });
      var t = T(g, "voice", "<span class='bib-skew'>" + w[0] + "</span>", { left: "0", width: "1920px", textAlign: "center", top: "600px", fontSize: "200px", color: C.wh, textShadow: "7px 6px 0 " + w[1], opacity: 0 });
      var slash = A(g, { left: "560px", top: "430px", width: "800px", height: "16px", background: C.wh, transform: "rotate(-30deg)", boxShadow: "0 0 30px " + w[1], opacity: 0 });
      tl.fromTo(shield, { opacity: 0, scale: 0.4 }, { opacity: 1, scale: 1, duration: 0.18, ease: "back.out(3)", immediateRender: false }, w[2]);
      IG.slam(tl, t, w[2] + 0.05, { x: i === 2 ? 300 : 0, s: 1.6, d: 0.2, ir: false });
      if (i === 1) { tl.fromTo(slash, { opacity: 1, scaleX: 0 }, { scaleX: 1, duration: 0.12, ease: "expo.out", immediateRender: false }, w[2]); tl.to(slash, { opacity: 0, duration: 0.3 }, w[2] + 0.2); IG.shake(tl, g, w[2], 0.3, 18, 4); }
      if (i === 2) { for (var k = 0; k < 3; k++) { var ghost = T(g, "voice", "<span class='bib-skew'>DODGE</span>", { left: "0", width: "1920px", textAlign: "center", top: "600px", fontSize: "200px", color: C.vio, opacity: 0 });
        tl.fromTo(ghost, { opacity: 0.5 - k * 0.15, x: -80 - k * 80 }, { opacity: 0, x: -200 - k * 80, duration: 0.5, immediateRender: false }, w[2] + 0.05); } }
      tl.to([shield, t], { opacity: 0, duration: 0.2 }, w[2] + 1.2);
    });
  });

  /* 31 VICTORY */
  def("victory", "04", 10, "VICTORY BANNER", function (tl, L) {
    var slab = A(L, { left: "-200px", top: "330px", width: "2320px", height: "300px", background: C.rose, transform: "rotate(-6deg)" });
    var t = T(L, "voice", "<span class='bib-skew'>VICTORY</span>", { left: "0", width: "1920px", textAlign: "center", top: "300px", fontSize: "320px", color: C.wh, textShadow: "12px 10px 0 #7657FF" });
    var st = stars(L, 3, 130, C.wh, { left: (960 - 3 * 130 * 1.05 / 2) + "px", top: "160px" });
    var rw = T(L, "mono", "STAGE CLEAR // +800 GOLD // +250 BYTES // +2 ANGEL HEARTS", { left: "0", width: "1920px", textAlign: "center", top: "700px", fontSize: "30px", color: C.wh });
    var white = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: C.wh, opacity: 0 });
    tl.fromTo(slab, { scaleX: 0 }, { scaleX: 1, duration: 0.35, ease: "expo.out" }, 0.1);
    IG.slam(tl, t, 0.25, { x: 0, s: 2.2, d: 0.22 });
    tl.fromTo(white, { opacity: 0.9 }, { opacity: 0, duration: 0.3, immediateRender: false }, 0.47);
    IG.shake(tl, L, 0.47, 0.4, 16, 2);
    st.arr.forEach(function (s, i) { tl.fromTo(s, { scale: 0, rotation: -120 }, { scale: 1, rotation: 0, duration: 0.3, ease: "back.out(3)" }, 1.0 + i * 0.25); });
    IG.rise(tl, rw, 1.9);
    tl.to([slab, t, st, rw], { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 32 DEFEAT */
  def("defeat", "04", 11, "DEFEAT BANNER", function (tl, L) {
    var t = T(L, "voice", "<span class='bib-skew'>DEFEAT</span>", { left: "0", width: "1920px", textAlign: "center", top: "280px", fontSize: "300px", color: "rgba(247,244,250,.85)" });
    var ln = A(L, { left: "560px", top: "600px", width: "800px", height: "4px", background: C.vio });
    var r1 = A(L, { left: "560px", top: "680px", width: "370px", height: "110px", border: "3px solid " + C.cy, clipPath: cut(18) });
    T(r1, "voice", "RETRY", { left: "0", width: "370px", textAlign: "center", top: "14px", fontSize: "84px", color: C.wh });
    var r2 = A(L, { left: "990px", top: "680px", width: "370px", height: "110px", border: "3px solid rgba(247,244,250,.5)", clipPath: cut(18) });
    T(r2, "voice", "RETURN", { left: "0", width: "370px", textAlign: "center", top: "14px", fontSize: "84px", color: C.wh });
    tl.fromTo(t, { opacity: 0, y: -60 }, { opacity: 1, y: 0, duration: 0.6, ease: "power2.out" }, 0.1);
    IG.glitch(tl, t, 0.8, 0.4, 40, 9); IG.glitch(tl, t, 2.6, 0.2, 25, 10);
    tl.to(t, { y: 20, color: "rgba(118,87,255,.9)", duration: 1.5, ease: "power1.in" }, 1.2);
    tl.fromTo(ln, { scaleX: 0 }, { scaleX: 1, duration: 0.5, ease: "expo.out" }, 0.9);
    IG.rise(tl, r1, 1.4); IG.rise(tl, r2, 1.5);
    tl.to([t, ln, r1, r2], { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 33 TARGET LOCK */
  def("target-lock", "04", 12, "TARGET LOCK RETICLE", function (tl, L) {
    var r = A(L, { left: "760px", top: "340px", width: "400px", height: "400px" });
    var br = IG.brackets(r, 0, 80, C.rose, 6);
    var c = A(r, { left: "150px", top: "150px", width: "100px", height: "100px", border: "4px solid " + C.cy, borderRadius: "50%" });
    A(r, { left: "196px", top: "120px", width: "8px", height: "40px", background: C.cy }); A(r, { left: "196px", top: "240px", width: "8px", height: "40px", background: C.cy });
    A(r, { left: "120px", top: "196px", width: "40px", height: "8px", background: C.cy }); A(r, { left: "240px", top: "196px", width: "40px", height: "8px", background: C.cy });
    var lab = T(L, "mono", "", { left: "1180px", top: "360px", fontSize: "30px", color: C.rose, fontWeight: 700 });
    var dist = T(L, "mono", "", { left: "1180px", top: "410px", fontSize: "26px", color: C.wh });
    tl.fromTo(r, { scale: 2.4, rotation: 45, opacity: 0, x: -300, y: 120 }, { scale: 1, rotation: 0, opacity: 1, x: 0, y: 0, duration: 0.6, ease: "expo.out" }, 0.1);
    tl.fromTo(c, { scale: 0 }, { scale: 1, duration: 0.25, ease: "back.out(3)" }, 0.6);
    IG.scramble(tl, lab, "TARGET LOCKED", 0.7, 0.4, 5);
    IG.count(tl, dist, 48, 12, 0.7, 3, function (v) { return "DIST " + v.toFixed(1) + " M"; }, "none");
    tl.to(br, { scale: 0.85, duration: 0.4, yoyo: true, repeat: 5, ease: "sine.inOut" }, 1.0);
    tl.to(r, { x: 40, y: -20, duration: 1.2, yoyo: true, repeat: 1, ease: "sine.inOut" }, 1.4);
    tl.to([r, lab, dist], { opacity: 0, scale: 0.8, duration: 0.25 }, 4.6);
  });
})(window);
