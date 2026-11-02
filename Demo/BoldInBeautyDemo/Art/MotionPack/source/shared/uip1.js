/* uip1.js: UI pack pieces, sheets 01 (navigation & core) and 02 (economy).
   Each piece: UIP[id] = { sheet, n, title, fn(tl, L) } builds a 5 s demo inside layer L (1920x1080, transparent).
   Initial states use gsap.set or fromTo (immediateRender) so pieces can be nested at any offset. */
(function (root) {
  var C = IG.C, el = IG.el, H = IG.hash;
  var UIP = root.UIP = root.UIP || {};
  UIP.SHEETS = UIP.SHEETS || {};
  UIP.SHEETS["01"] = "NAVIGATION & CORE"; UIP.SHEETS["02"] = "ECONOMY & REWARDS";
  function def(id, sheet, n, title, fn) { UIP[id] = { sheet: sheet, n: n, title: title, fn: fn }; }
  function A(p, css) { return el("div", "", p, null, Object.assign({ position: "absolute" }, css)); }
  function T(p, cls, txt, css) { return el("div", cls, p, txt, Object.assign({ position: "absolute" }, css)); }
  UIP.A = A; UIP.T = T;
  var cut = function (n) { return "polygon(" + n + "px 0, 100% 0, calc(100% - " + n + "px) 100%, 0 100%)"; };
  UIP.cut = cut;

  /* 01 NAV BAR: seven hub tabs, selection slides tab to tab */
  def("nav-bar", "01", 1, "HUB NAV BAR", function (tl, L) {
    var bar = A(L, { left: "0", top: "870px", width: "1920px", height: "150px", background: "linear-gradient(0deg, rgba(9,11,18,.98), rgba(21,25,37,.94))", borderTop: "2px solid rgba(100,232,255,.5)" });
    var glow = A(bar, { left: "0", top: "0", width: "240px", height: "150px", background: "linear-gradient(0deg, rgba(255,61,141,.28), rgba(255,61,141,0))" });
    var und = A(bar, { left: "0", top: "126px", width: "180px", height: "10px", background: C.rose, transform: "skewX(-30deg)", boxShadow: "0 0 18px " + C.rose });
    var tabs = UI.TABS.map(function (t, i) {
      return T(bar, "voice", t, { left: (60 + i * 260) + "px", top: "36px", width: "240px", textAlign: "center", fontSize: "62px", color: "rgba(247,244,250,.45)" });
    });
    var jp = ["オペレーター", "チーム", "ロビー", "ショップ", "レボリューション", "キャンペーン", "リクルート"];
    var sub = T(bar, "jp", "", { left: "0", top: "104px", width: "240px", textAlign: "center", fontSize: "18px", color: C.cy, letterSpacing: ".2em" });
    tl.fromTo(bar, { yPercent: 100 }, { yPercent: 0, duration: 0.45, ease: "expo.out" }, 0);
    tabs.forEach(function (b, i) { tl.fromTo(b, { opacity: 0, y: 30 }, { opacity: 1, y: 0, duration: 0.25, ease: "back.out(2)" }, 0.15 + i * 0.04); });
    var seq = [2, 4, 6, 0, 3], t0 = 0.6, keys = [];
    seq.forEach(function (k, j) {
      var t = t0 + j * 0.85, x = 60 + k * 260;
      tl.to(und, { x: x + 30, duration: 0.32, ease: "expo.out" }, t);
      tl.to(glow, { x: x, duration: 0.32, ease: "expo.out" }, t);
      tl.to(sub, { x: x, duration: 0.32, ease: "expo.out" }, t);
      tabs.forEach(function (b, i) { tl.to(b, { color: i === k ? C.wh : "rgba(247,244,250,.45)", scale: i === k ? 1.12 : 1, duration: 0.2 }, t); });
      keys.push([t, jp[k]]);
    });
    gsap.set(und, { x: 60 + 2 * 260 + 30 }); gsap.set(glow, { x: 60 + 2 * 260 }); gsap.set(sub, { x: 60 + 2 * 260 });
    keys.unshift([0, ""]); IG.textAt(tl, sub, keys, 5);
    tl.to(bar, { yPercent: 100, duration: 0.35, ease: "expo.in" }, 4.6);
  });

  /* 02 PRIMARY BUTTON: idle shimmer, hover glow, press squash, ripple, confirm */
  def("btn-primary", "01", 2, "PRIMARY BUTTON STATES", function (tl, L) {
    var b = A(L, { left: "660px", top: "450px", width: "600px", height: "150px", background: C.rose, clipPath: cut(26), overflow: "hidden" });
    var sh = A(b, { left: "-200px", top: "-20px", width: "120px", height: "200px", background: "linear-gradient(90deg, rgba(247,244,250,0), rgba(247,244,250,.6), rgba(247,244,250,0))", transform: "skewX(-20deg)" });
    var lab = T(b, "voice", "CONFIRM", { left: "0", width: "600px", textAlign: "center", top: "26px", fontSize: "110px", color: C.wh });
    var ok = T(b, "voice", "✓ DONE", { left: "0", width: "600px", textAlign: "center", top: "26px", fontSize: "110px", color: C.bk, opacity: 0 });
    var fr = A(L, { left: "644px", top: "434px", width: "632px", height: "182px", border: "3px solid " + C.cy, clipPath: cut(30), opacity: 0 });
    var rip = A(b, { left: "300px", top: "75px", width: "20px", height: "20px", marginLeft: "-10px", marginTop: "-10px", borderRadius: "50%", background: "rgba(247,244,250,.7)", opacity: 0 });
    var st = T(L, "mono", "", { left: "660px", top: "640px", fontSize: "26px", color: C.cy });
    IG.textAt(tl, st, [[0, "STATE: IDLE"], [1.4, "STATE: HOVER"], [2.4, "STATE: PRESSED"], [3.0, "STATE: CONFIRMED"]], 5);
    IG.pop(tl, b, 0.05, 0.4); IG.rise(tl, st, 0.3);
    tl.fromTo(sh, { x: 0 }, { x: 980, duration: 0.8, ease: "power2.inOut" }, 0.5);
    tl.fromTo(fr, { opacity: 0, scale: 1.08 }, { opacity: 1, scale: 1, duration: 0.25, ease: "power3.out", immediateRender: false }, 1.4);
    tl.to(b, { boxShadow: "0 0 50px rgba(255,61,141,.8)", y: -6, duration: 0.25 }, 1.4);
    tl.to(b, { scaleX: 1.04, scaleY: 0.86, y: 6, duration: 0.08, ease: "power2.in" }, 2.4);
    tl.to(b, { scaleX: 1, scaleY: 1, y: 0, duration: 0.4, ease: "elastic.out(1.2, 0.4)" }, 2.5);
    tl.fromTo(rip, { opacity: 0.8, scale: 0 }, { opacity: 0, scale: 40, duration: 0.6, ease: "power2.out", immediateRender: false }, 2.45);
    tl.to(b, { backgroundColor: C.wh, duration: 0.15 }, 3.0); tl.to(lab, { opacity: 0, y: -40, duration: 0.15 }, 3.0);
    tl.fromTo(ok, { opacity: 0, y: 40 }, { opacity: 1, y: 0, duration: 0.25, ease: "back.out(2)", immediateRender: false }, 3.05);
    tl.to([b, fr, st], { opacity: 0, scale: 0.9, duration: 0.25, ease: "power2.in" }, 4.6);
  });

  /* 03 SECONDARY / BACK / CLOSE buttons */
  def("btn-secondary", "01", 3, "SECONDARY, BACK & CLOSE", function (tl, L) {
    var s = A(L, { left: "560px", top: "440px", width: "460px", height: "130px", border: "3px solid " + C.cy, background: "rgba(21,25,37,.9)", clipPath: cut(22) });
    T(s, "voice", "CANCEL", { left: "0", width: "460px", textAlign: "center", top: "22px", fontSize: "92px", color: C.wh });
    var bk = A(L, { left: "300px", top: "440px", width: "200px", height: "130px", border: "3px solid rgba(247,244,250,.6)", background: "rgba(21,25,37,.9)", clipPath: "polygon(40px 0, 100% 0, 100% 100%, 40px 100%, 0 50%)" });
    T(bk, "voice", "BACK", { left: "40px", width: "160px", textAlign: "center", top: "28px", fontSize: "80px", color: C.wh });
    var x = A(L, { left: "1080px", top: "440px", width: "130px", height: "130px", border: "3px solid " + C.rose, background: "rgba(21,25,37,.9)" });
    T(x, "title", "✕", { left: "0", width: "130px", textAlign: "center", top: "22px", fontSize: "80px", color: C.rose, letterSpacing: "0" });
    var hl = A(L, { left: "0", top: "0", width: "10px", height: "130px", background: C.cy, opacity: 0 });
    [bk, s, x].forEach(function (e, i) { tl.fromTo(e, { opacity: 0, y: 60 }, { opacity: 1, y: 0, duration: 0.3, ease: "back.out(2)" }, 0.1 + i * 0.1); });
    // hover sweep across each, then press
    [[bk, 300, 1.0], [s, 560, 2.0], [x, 1080, 3.0]].forEach(function (q, i) {
      tl.set(hl, { x: q[1] - 20, y: 440 }, q[2]);
      tl.fromTo(hl, { opacity: 0 }, { opacity: 1, duration: 0.1, immediateRender: false }, q[2]);
      tl.to(q[0], { backgroundColor: i === 2 ? "rgba(255,61,141,.35)" : "rgba(100,232,255,.18)", duration: 0.15 }, q[2]);
      tl.to(q[0], { scale: 0.92, duration: 0.07 }, q[2] + 0.45); tl.to(q[0], { scale: 1, duration: 0.3, ease: "back.out(3)" }, q[2] + 0.52);
      tl.to(q[0], { backgroundColor: "rgba(21,25,37,.9)", duration: 0.3 }, q[2] + 0.8);
      tl.to(hl, { opacity: 0, duration: 0.1 }, q[2] + 0.85);
    });
    tl.to(x, { rotation: 90, duration: 0.3, ease: "back.out(2)" }, 3.45);
    tl.to([bk, s, x], { opacity: 0, y: 40, duration: 0.25, stagger: 0.05, ease: "power2.in" }, 4.5);
  });

  /* 04 TOGGLE + SLIDER (settings) */
  def("settings-controls", "01", 4, "SETTINGS TOGGLES & SLIDERS", function (tl, L) {
    var p = UI.panel(L, 460, 260, 1000, 560, { title: "SETTINGS // PREFERENCES" });
    var rows = [["MUSIC", 0.7], ["SFX", 0.45], ["TEXT SIZE", 0.5]];
    rows.forEach(function (r, i) {
      var y = 80 + i * 110;
      T(p, "voice", r[0], { left: "40px", top: y + "px", fontSize: "64px", color: C.wh });
      var tr = A(p, { left: "380px", top: (y + 30) + "px", width: "460px", height: "10px", background: "rgba(100,232,255,.25)" });
      var fl = A(tr, { left: "0", top: "0", height: "10px", width: (460 * r[1]) + "px", background: C.cy });
      var kn = A(tr, { left: (460 * r[1] - 18) + "px", top: "-14px", width: "36px", height: "36px", background: C.wh, transform: "rotate(45deg)", boxShadow: "0 0 12px " + C.cy });
      var v = T(p, "mono", Math.round(r[1] * 100) + "%", { left: "870px", top: (y + 18) + "px", fontSize: "30px", color: C.wh, fontWeight: 700 });
      var to = [0.9, 0.15, 0.75][i], tt = 1.0 + i * 0.6;
      tl.to(fl, { width: 460 * to, duration: 0.6, ease: "power3.inOut" }, tt);
      tl.to(kn, { left: 460 * to - 18, duration: 0.6, ease: "power3.inOut" }, tt);
      tl.to(kn, { scale: 1.3, duration: 0.15, yoyo: true, repeat: 1 }, tt);
      IG.count(tl, v, r[1] * 100, to * 100, tt, 0.6, function (q) { return Math.round(q) + "%"; }, "power3.inOut");
    });
    var togs = [["VIBRATION", true], ["DAMAGE NUMBERS", false]];
    togs.forEach(function (g, i) {
      var y = 420, x = 40 + i * 480;
      T(p, "voice", g[0], { left: x + "px", top: y + "px", fontSize: "50px", color: C.wh });
      var tg = A(p, { left: (x + (i ? 330 : 230)) + "px", top: (y + 6) + "px", width: "110px", height: "50px", border: "3px solid " + C.cy, background: g[1] ? "rgba(255,61,141,.85)" : "rgba(21,25,37,.9)" });
      var k = A(tg, { left: g[1] ? "58px" : "4px", top: "4px", width: "42px", height: "36px", background: C.wh });
      var t = 3.0 + i * 0.5;
      tl.to(k, { left: g[1] ? 4 : 58, duration: 0.25, ease: "back.out(2)" }, t);
      tl.to(tg, { backgroundColor: g[1] ? "rgba(21,25,37,.9)" : "rgba(255,61,141,.85)", duration: 0.2 }, t);
    });
    tl.fromTo(p, { opacity: 0, scale: 0.9 }, { opacity: 1, scale: 1, duration: 0.35, ease: "back.out(1.6)" }, 0);
    tl.to(p, { opacity: 0, scale: 0.95, duration: 0.25, ease: "power2.in" }, 4.6);
  });

  /* 05 LOADING: rose ring spinner with percentage and scramble */
  def("loading", "01", 5, "LOADING SPINNER", function (tl, L) {
    var w = A(L, { left: "0", top: "0", width: "1920px", height: "1080px" });
    var rg = IG.ring(w, 120, 18, C.rose, { left: "804px", top: "324px" });
    var r2 = IG.ring(w, 160, 3, C.cy, { left: "794px", top: "314px" }); r2.circle.style.filter = "none"; r2.circle.setAttribute("stroke-dasharray", "20 30");
    var pc = T(w, "title", "0%", { left: "0", width: "1920px", textAlign: "center", top: "432px", fontSize: "90px", color: C.wh });
    var lb = T(w, "mono", "", { left: "0", width: "1920px", textAlign: "center", top: "740px", fontSize: "30px", color: C.cy });
    var tip = T(w, "mono", "TIP: SWITCH OPERATORS TO KEEP THE PRESSURE ON", { left: "0", width: "1920px", textAlign: "center", top: "800px", fontSize: "24px", color: "rgba(247,244,250,.6)" });
    tl.fromTo(rg.circle, { attr: { "stroke-dashoffset": rg.L } }, { attr: { "stroke-dashoffset": 0 }, duration: 4.0, ease: "power1.inOut" }, 0.3);
    tl.fromTo(rg.svg, { rotation: 0 }, { rotation: 360, duration: 4.4, ease: "none" }, 0);
    tl.fromTo(r2.svg, { rotation: 0 }, { rotation: -540, duration: 5, ease: "none" }, 0);
    IG.count(tl, pc, 0, 100, 0.3, 4.0, function (v) { return Math.round(v) + "%"; }, "power1.inOut");
    IG.scramble(tl, lb, "LOADING // NEO TOKYO", 0.2, 0.6, 5);
    IG.rise(tl, tip, 0.6);
    tl.fromTo(w, { opacity: 0 }, { opacity: 1, duration: 0.2 }, 0);
    tl.to(rg.svg, { scale: 1.25, duration: 0.15, yoyo: true, repeat: 1 }, 4.3);
    tl.to(w, { opacity: 0, duration: 0.3 }, 4.65);
  });

  /* 06 MODAL: dialog opens, buttons, closes */
  def("modal", "01", 6, "MODAL DIALOG", function (tl, L) {
    var dim = A(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: "rgba(9,11,18,.6)", opacity: 0 });
    var m = UI.panel(L, 510, 290, 900, 500, { title: "CONFIRM // SUMMON" });
    IG.brackets(m, 14, 40, C.cy, 3);
    T(m, "voice", "USE 1 REGULAR TICKET?", { left: "40px", top: "90px", fontSize: "90px", color: C.wh });
    UI.icon("ticket", 90, m, { left: "44px", top: "210px" });
    T(m, "mono", "OWNED: 10  >  9", { left: "160px", top: "238px", fontSize: "30px", color: C.cy });
    var no = A(m, { left: "40px", top: "360px", width: "380px", height: "100px", border: "3px solid " + C.cy, clipPath: cut(18) });
    T(no, "voice", "CANCEL", { left: "0", width: "380px", textAlign: "center", top: "14px", fontSize: "76px", color: C.wh });
    var yes = A(m, { left: "460px", top: "360px", width: "380px", height: "100px", background: C.rose, clipPath: cut(18) });
    T(yes, "voice", "SUMMON", { left: "0", width: "380px", textAlign: "center", top: "14px", fontSize: "76px", color: C.wh });
    tl.fromTo(dim, { opacity: 0 }, { opacity: 1, duration: 0.3 }, 0);
    tl.fromTo(m, { opacity: 0, scaleY: 0.02, scaleX: 1.1 }, { opacity: 1, scaleY: 1, scaleX: 1, duration: 0.4, ease: "expo.out" }, 0.1);
    tl.to(yes, { boxShadow: "0 0 40px rgba(255,61,141,.9)", duration: 0.3, yoyo: true, repeat: 3 }, 1.4);
    tl.to(yes, { scale: 0.92, duration: 0.07 }, 3.2); tl.to(yes, { scale: 1, duration: 0.3, ease: "back.out(3)" }, 3.27);
    tl.to(m, { scaleY: 0.02, opacity: 0, duration: 0.3, ease: "expo.in" }, 3.8);
    tl.to(dim, { opacity: 0, duration: 0.3 }, 4.0);
  });

  /* 07 NOTICE TOASTS: stacked notices slide in, timer bars, dismiss */
  def("notice-toast", "01", 7, "NOTICE TOASTS", function (tl, L) {
    var msgs = [["NOTICE", "SEASON PASS UPDATED", C.rose], ["MAIL", "YOU HAVE 3 NEW MESSAGES", C.cy], ["QUEST", "DAILY QUEST COMPLETE", C.vio]];
    msgs.forEach(function (m, i) {
      var t = A(L, { left: "1180px", top: (220 + i * 170) + "px", width: "680px", height: "140px", background: "rgba(21,25,37,.95)", border: "2px solid rgba(100,232,255,.45)", borderLeft: "10px solid " + m[2], clipPath: cut(20) });
      T(t, "mono", m[0], { left: "30px", top: "18px", fontSize: "24px", color: m[2] });
      T(t, "voice", m[1], { left: "30px", top: "50px", fontSize: "66px", color: C.wh });
      var bar = A(t, { left: "0", bottom: "0", height: "5px", width: "680px", background: m[2] });
      var ti = 0.2 + i * 0.5;
      tl.fromTo(t, { x: 760 }, { x: 0, duration: 0.45, ease: "expo.out" }, ti);
      tl.fromTo(bar, { width: 680 }, { width: 0, duration: 3.2 - i * 0.3, ease: "none" }, ti + 0.3);
      tl.to(t, { x: 760, duration: 0.35, ease: "expo.in" }, ti + 3.5 - i * 0.3);
    });
  });

  /* 08 PAGE HEADER */
  def("page-header", "01", 8, "PAGE HEADER", function (tl, L) {
    var h = A(L, { left: "0", top: "300px", width: "1920px", height: "420px" });
    var slab = A(h, { left: "-200px", top: "120px", width: "1400px", height: "170px", background: C.rose, transform: "rotate(-12deg)", transformOrigin: "0 50%" });
    var t = T(h, "voice", "<span class='bib-skew'>CAMPAIGN</span>", { left: "160px", top: "60px", fontSize: "300px", color: C.wh, textShadow: "10px 9px 0 #7657FF" });
    var jp = T(h, "jp", "キャンペーン", { left: "170px", top: "340px", fontSize: "40px", color: C.cy, letterSpacing: ".4em" });
    var ln = A(h, { left: "170px", top: "330px", width: "900px", height: "4px", background: C.cy });
    var code = T(h, "mono", "", { left: "1180px", top: "250px", fontSize: "28px", color: C.wh });
    tl.fromTo(slab, { scaleX: 0 }, { scaleX: 1, duration: 0.35, ease: "expo.out" }, 0.1);
    IG.slam(tl, t, 0.2, { x: -300 });
    tl.fromTo(ln, { scaleX: 0, transformOrigin: "0 50%" }, { scaleX: 1, duration: 0.5, ease: "expo.out" }, 0.5);
    IG.scramble(tl, jp, "キャンペーン", 0.6, 0.5, 3);
    IG.scramble(tl, code, "CH 01 // 13 CHAPTERS", 0.8, 0.6, 4);
    tl.to(t, { x: 30, duration: 3.5, ease: "none" }, 0.6);
    tl.to(slab, { scaleX: 0, transformOrigin: "100% 50%", duration: 0.3, ease: "expo.in" }, 4.4);
    tl.to([t, jp, ln, code], { opacity: 0, x: -60, duration: 0.3, ease: "power2.in" }, 4.5);
  });

  /* ---------------- SHEET 02: ECONOMY ---------------- */
  /* 09 CURRENCY BAR with deltas */
  def("currency-bar", "02", 1, "CURRENCY BAR", function (tl, L) {
    var names = [["gold", 24800, 25600, "+800"], ["diamond", 300, 250, "-50"], ["bytes", 1250, 1500, "+250"], ["heart", 12, 14, "+2"]];
    names.forEach(function (n, i) {
      var c = UI.chip(L, 420 + i * 290, 480, n[0], IG.commas(n[1]), 260);
      tl.fromTo(c, { opacity: 0, y: -60 }, { opacity: 1, y: 0, duration: 0.3, ease: "back.out(2)" }, 0.1 + i * 0.07);
      var t = 1.2 + i * 0.6;
      var d = T(L, "mono", n[3], { left: (560 + i * 290) + "px", top: "560px", fontSize: "34px", fontWeight: 700, color: n[3][0] === "+" ? C.cy : C.rose });
      tl.fromTo(d, { opacity: 0, y: 20 }, { opacity: 1, y: 0, duration: 0.2, ease: "back.out(2)" }, t);
      tl.to(d, { opacity: 0, y: -50, duration: 0.6, ease: "power2.in" }, t + 0.6);
      IG.count(tl, c.val, n[1], n[2], t + 0.1, 0.6, IG.commas, "power2.out");
      tl.to(c, { scale: 1.1, duration: 0.1, yoyo: true, repeat: 1 }, t + 0.1);
      tl.to(c, { opacity: 0, y: -60, duration: 0.25, ease: "power2.in" }, 4.5 + i * 0.05);
    });
  });

  /* 10 REWARD POPUP */
  def("reward-popup", "02", 2, "REWARD POPUP", function (tl, L) {
    var ban = A(L, { left: "-100px", top: "250px", width: "2120px", height: "190px", background: C.rose, transform: "rotate(-4deg)" });
    var t = T(L, "voice", "<span class='bib-skew'>REWARDS</span>", { left: "0", width: "1920px", textAlign: "center", top: "240px", fontSize: "220px", color: C.wh, textShadow: "8px 7px 0 #7657FF" });
    var items = [["gold", "x800", C.rose], ["bytes", "x250", C.vio], ["heart", "x2", C.rose], ["ticket", "x1", C.cy], ["diamond", "x50", C.cy]];
    items.forEach(function (it, i) {
      var tile = A(L, { left: (430 + i * 220) + "px", top: "540px", width: "190px", height: "220px", background: "rgba(21,25,37,.95)", border: "3px solid " + it[2], boxShadow: "0 0 26px " + it[2] });
      UI.icon(it[0], 110, tile, { left: "40px", top: "30px" });
      T(tile, "mono", it[1], { left: "0", width: "190px", textAlign: "center", top: "160px", fontSize: "32px", fontWeight: 700, color: C.wh });
      tl.fromTo(tile, { opacity: 0, scale: 0.2, rotation: -20 }, { opacity: 1, scale: 1, rotation: 0, duration: 0.35, ease: "back.out(2.2)" }, 0.9 + i * 0.15);
      tl.to(tile, { opacity: 0, y: 60, duration: 0.25, ease: "power2.in" }, 4.4 + i * 0.03);
    });
    var tap = T(L, "mono", "TAP TO CONTINUE", { left: "0", width: "1920px", textAlign: "center", top: "830px", fontSize: "28px", color: C.wh });
    tl.fromTo(ban, { scaleX: 0 }, { scaleX: 1, duration: 0.35, ease: "expo.out" }, 0.1);
    IG.slam(tl, t, 0.3, { x: 0, s: 1.6 });
    tl.fromTo(tap, { opacity: 0 }, { opacity: 1, duration: 0.4, yoyo: true, repeat: 3 }, 2.0);
    tl.to([ban, t], { opacity: 0, duration: 0.25 }, 4.5);
  });

  /* 11 SHOP CARD purchase */
  def("shop-card", "02", 3, "SHOP ITEM PURCHASE", function (tl, L) {
    var c = A(L, { left: "720px", top: "200px", width: "480px", height: "680px", background: "rgba(21,25,37,.96)", border: "2px solid rgba(100,232,255,.5)", borderTop: "10px solid " + C.vio, clipPath: "polygon(0 0, calc(100% - 30px) 0, 100% 30px, 100% 100%, 0 100%)" });
    T(c, "mono", "SHOP // SUPPLY", { left: "30px", top: "24px", fontSize: "24px", color: C.cy });
    UI.icon("ticket", 220, c, { left: "130px", top: "90px" });
    T(c, "voice", "REGULAR TICKET", { left: "0", width: "480px", textAlign: "center", top: "330px", fontSize: "76px", color: C.wh });
    T(c, "mono", "X10 BUNDLE", { left: "0", width: "480px", textAlign: "center", top: "420px", fontSize: "28px", color: "rgba(247,244,250,.7)" });
    var buy = A(c, { left: "40px", top: "520px", width: "400px", height: "110px", background: C.rose, clipPath: cut(18) });
    UI.icon("diamond", 60, buy, { left: "60px", top: "25px" });
    T(buy, "voice", "1,500", { left: "140px", top: "14px", fontSize: "84px", color: C.wh });
    var stamp = T(L, "title", "PURCHASED", { left: "690px", top: "440px", fontSize: "84px", color: C.cy, border: "7px solid " + C.cy, padding: "4px 18px", background: "rgba(9,11,18,.85)", opacity: 0 });
    tl.fromTo(c, { opacity: 0, y: 80, rotationY: 60 }, { opacity: 1, y: 0, rotationY: 0, duration: 0.5, ease: "expo.out" }, 0);
    tl.to(c, { y: -16, boxShadow: "0 30px 60px rgba(118,87,255,.45)", borderColor: C.cy, duration: 0.3 }, 1.0);
    tl.to(buy, { scale: 0.92, duration: 0.07 }, 2.0); tl.to(buy, { scale: 1, duration: 0.3, ease: "back.out(3)" }, 2.07);
    tl.fromTo(stamp, { opacity: 0, scale: 3, rotation: -12 }, { opacity: 1, scale: 1, rotation: -12, duration: 0.2, ease: "power4.in", immediateRender: false }, 2.4);
    IG.shake(tl, c, 2.6, 0.3, 10, 3);
    tl.to(c, { opacity: 0.5, duration: 0.3 }, 2.7);
    tl.to([c, stamp], { opacity: 0, y: 60, duration: 0.3, ease: "power2.in" }, 4.5);
  });

  /* 12 LEVEL UP */
  def("level-up", "02", 4, "LEVEL UP", function (tl, L) {
    var x = IG.canvas("lvsp-" + (UIP.k = (UIP.k || 0) + 1), 1920, 1080, L);
    IG.clock(tl, 1.4, function (t) { x.clearRect(0, 0, 1920, 1080); if (t > 0 && t < 1.38) IG.speed(x, t, { col: "rgba(255,61,141,.55)", n: 60, r0: 260 + 600 * t }); }, 0.3);
    var t = T(L, "voice", "<span class='bib-skew'>LEVEL UP</span>", { left: "0", width: "1920px", textAlign: "center", top: "250px", fontSize: "240px", color: C.wh, textShadow: "9px 8px 0 #FF3D8D" });
    var lv = T(L, "title", "LV 11", { left: "0", width: "1920px", textAlign: "center", top: "500px", fontSize: "140px", color: C.rose, textShadow: "8px 7px 0 #7657FF" });
    var stats = [["HP", "+120"], ["ATK", "+34"], ["DEF", "+18"]];
    stats.forEach(function (s, i) {
      var r = T(L, "mono", s[0] + "  <b style='color:#64E8FF'>" + s[1] + "</b>", { left: (700 + i * 200) + "px", top: "700px", fontSize: "34px", color: C.wh });
      IG.rise(tl, r, 1.4 + i * 0.12);
      tl.to(r, { opacity: 0, duration: 0.2 }, 4.5);
    });
    IG.slam(tl, t, 0.3, { x: 0, s: 2 });
    tl.fromTo(lv, { opacity: 0 }, { opacity: 1, duration: 0.2 }, 0.6);
    IG.textAt(tl, lv, [[0, "LV 11"], [1.0, "LV 12"]], 5);
    tl.fromTo(lv, { scale: 1 }, { scale: 1.3, duration: 0.1, yoyo: true, repeat: 1, immediateRender: false }, 1.0);
    tl.to([t, lv], { opacity: 0, scale: 0.9, duration: 0.25 }, 4.5);
  });

  /* 13 RESOURCE FLOATERS fly to the top bar */
  def("resource-gain", "02", 5, "RESOURCE GAIN FLOATERS", function (tl, L) {
    var g = [["gold", "+800 GOLD", 760, 600], ["bytes", "+250 BYTES", 960, 680], ["heart", "+2 ANGEL HEARTS", 1160, 600]];
    var bar = [UI.chip(L, 900, 60, "gold", "24,800"), UI.chip(L, 1150, 60, "bytes", "1,250"), UI.chip(L, 1400, 60, "heart", "12", 210)];
    bar.forEach(function (b, i) { tl.fromTo(b, { opacity: 0, y: -40 }, { opacity: 1, y: 0, duration: 0.3 }, 0.1 + i * 0.05); });
    g.forEach(function (q, i) {
      var f = A(L, { left: (q[2] - 150) + "px", top: q[3] + "px", width: "300px", height: "200px" });
      UI.icon(q[0], 100, f, { left: "100px", top: "0px" });
      T(f, "voice", q[1], { left: "0", width: "300px", textAlign: "center", top: "110px", fontSize: "56px", color: C.wh });
      var t0 = 0.6 + i * 0.3;
      tl.fromTo(f, { opacity: 0, scale: 0.3, y: 60 }, { opacity: 1, scale: 1, y: 0, duration: 0.35, ease: "back.out(2.5)" }, t0);
      var tx = [900, 1150, 1400][i] + 60 - (q[2] - 150) - 100, ty = 60 - q[3];
      tl.to(f, { x: tx, y: ty, scale: 0.35, duration: 0.55, ease: "power3.in" }, 2.0 + i * 0.25);
      tl.to(f, { opacity: 0, duration: 0.08 }, 2.55 + i * 0.25);
      tl.to(bar[i], { scale: 1.15, duration: 0.1, yoyo: true, repeat: 1 }, 2.55 + i * 0.25);
      IG.count(tl, bar[i].val, [24800, 1250, 12][i], [25600, 1500, 14][i], 2.55 + i * 0.25, 0.5, IG.commas);
    });
    tl.to(bar, { opacity: 0, y: -40, duration: 0.25 }, 4.6);
  });
})(window);
