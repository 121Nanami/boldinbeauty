/* uip3.js: UI pack sheets 05 (progression), 06 (arcade) and 07 (mobile controls: custom joysticks and touch buttons). */
(function (root) {
  var C = IG.C, el = IG.el, H = IG.hash, UIP = root.UIP, A = UIP.A, T = UIP.T, cut = UIP.cut;
  UIP.SHEETS["05"] = "PROGRESSION"; UIP.SHEETS["06"] = "ARCADE"; UIP.SHEETS["07"] = "MOBILE CONTROLS";
  function def(id, sheet, n, title, fn) { UIP[id] = { sheet: sheet, n: n, title: title, fn: fn }; }
  var NS = "http://www.w3.org/2000/svg";
  function svg(parent, w, h, vb, css) { var s = document.createElementNS(NS, "svg"); s.setAttribute("width", w); s.setAttribute("height", h); s.setAttribute("viewBox", vb); s.style.position = "absolute"; s.style.overflow = "visible"; for (var k in css) s.style[k] = css[k]; parent.appendChild(s); return s; }
  function sv(parent, tag, attrs) { var e = document.createElementNS(NS, tag); for (var k in attrs) e.setAttribute(k, attrs[k]); parent.appendChild(e); return e; }

  /* 34 CAMPAIGN MAP */
  def("campaign-map", "05", 1, "CAMPAIGN NODE MAP", function (tl, L) {
    T(L, "voice", "CHAPTER 01", { left: "140px", top: "120px", fontSize: "120px", color: C.wh });
    T(L, "mono", "STAGE 1-1 TO 1-8 // MINI-BOSS EVERY STAGE // BOSS AT THE END", { left: "146px", top: "245px", fontSize: "26px", color: C.cy });
    var pts = [[220, 760], [420, 640], [620, 720], [820, 560], [1020, 640], [1220, 500], [1420, 600], [1640, 440]];
    var s = svg(L, 1920, 1080, "0 0 1920 1080", { left: "0", top: "0" });
    var d = "M" + pts.map(function (p) { return p[0] + " " + p[1]; }).join(" L");
    var path = sv(s, "path", { d: d, fill: "none", stroke: C.cy, "stroke-width": 4, "stroke-dasharray": "14 10" });
    var mask = sv(s, "path", { d: d, fill: "none", stroke: C.rose, "stroke-width": 8 });
    var len = 0; for (var i = 1; i < pts.length; i++) len += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    mask.setAttribute("stroke-dasharray", len + " " + len);
    tl.fromTo(path, { opacity: 0 }, { opacity: 0.6, duration: 0.4 }, 0.1);
    tl.fromTo(mask, { attr: { "stroke-dashoffset": len } }, { attr: { "stroke-dashoffset": len * (1 - 4 / 7) }, duration: 1.6, ease: "power2.inOut" }, 0.6);
    pts.forEach(function (p, i) {
      var boss = i === pts.length - 1, cur = i === 4, done = i < 4;
      var sz = boss ? 120 : 76;
      var n = A(L, { left: (p[0] - sz / 2) + "px", top: (p[1] - sz / 2) + "px", width: sz + "px", height: sz + "px", transform: "rotate(45deg)", background: done ? C.rose : "rgba(21,25,37,.95)", border: "4px solid " + (boss ? C.rose : cur ? C.wh : done ? C.rose : "rgba(100,232,255,.6)") });
      T(L, "mono", boss ? "BOSS" : "1-" + (i + 1), { left: (p[0] - 60) + "px", width: "120px", textAlign: "center", top: (p[1] + sz / 2 + 18) + "px", fontSize: "26px", color: boss ? C.rose : C.wh, fontWeight: 700 });
      tl.fromTo(n, { scale: 0 }, { scale: 1, duration: 0.25, ease: "back.out(3)" }, 0.2 + i * 0.08);
      if (cur) { tl.to(n, { scale: 1.25, boxShadow: "0 0 40px " + C.wh, duration: 0.4, yoyo: true, repeat: 5, ease: "sine.inOut" }, 2.2); }
      if (i < 4) tl.fromTo(n, { backgroundColor: "rgba(21,25,37,.95)" }, { backgroundColor: C.rose, duration: 0.15, immediateRender: false }, 0.6 + i * 0.4);
    });
    var tag = T(L, "mono", "CURRENT // STAGE 1-5", { left: "940px", top: "420px", fontSize: "28px", color: C.wh, fontWeight: 700 });
    IG.rise(tl, tag, 2.3);
    tl.to(L.children, { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 35 SEASON PASS */
  def("season-pass", "05", 2, "SEASON PASS TRACK", function (tl, L) {
    T(L, "voice", "SEASON PASS", { left: "120px", top: "120px", fontSize: "120px", color: C.wh });
    T(L, "mono", "50 TIERS // FREE + PAID TRACKS", { left: "126px", top: "245px", fontSize: "26px", color: C.cy });
    var strip = A(L, { left: "120px", top: "330px", width: "4000px", height: "560px" });
    T(L, "mono", "FREE", { left: "40px", top: "430px", fontSize: "26px", color: C.cy });
    T(L, "mono", "PAID", { left: "40px", top: "680px", fontSize: "26px", color: C.rose });
    var icons = ["gold", "bytes", "ticket", "heart", "diamond", "box"];
    for (var i = 0; i < 16; i++) {
      var x = i * 230, tier = 9 + i;
      T(strip, "title", String(tier), { left: (x + 70) + "px", top: "0px", fontSize: "50px", color: tier <= 12 ? C.rose : "rgba(247,244,250,.6)" });
      [0, 1].forEach(function (row) {
        var c = A(strip, { left: x + "px", top: (70 + row * 250) + "px", width: "200px", height: "210px", background: "rgba(21,25,37,.95)", border: "3px solid " + (row ? C.rose : "rgba(100,232,255,.6)") });
        UI.icon(icons[(i + row * 3) % 6], 100, c, { left: "50px", top: "36px" });
        if (row && tier > 11) UI.icon("lock", 44, c, { left: "148px", top: "8px" });
        if (tier <= 11) { var ck = A(c, { left: "0", top: "0", width: "200px", height: "210px", background: "rgba(9,11,18,.6)" }); UI.icon("check", 90, ck, { left: "55px", top: "60px" }); }
        if (tier === 12 && !row) { c.id = "claimMe" + (UIP.k = (UIP.k || 0) + 1); tl.to(c, { boxShadow: "0 0 40px " + C.cy, borderColor: C.wh, duration: 0.3, yoyo: true, repeat: 3 }, 1.6);
          var ck2 = A(c, { left: "0", top: "0", width: "200px", height: "210px", background: "rgba(9,11,18,.6)", opacity: 0 }); UI.icon("check", 90, ck2, { left: "55px", top: "60px" });
          tl.fromTo(ck2, { opacity: 0, scale: 1.6 }, { opacity: 1, scale: 1, duration: 0.25, ease: "back.out(3)", immediateRender: false }, 3.0); }
      });
    }
    var prog = A(L, { left: "120px", top: "950px", width: "1680px", height: "10px", background: "rgba(100,232,255,.2)" });
    var pf = A(prog, { left: "0", top: "0", height: "10px", width: "0px", background: C.rose });
    tl.fromTo(strip, { x: 400, opacity: 0 }, { x: 0, opacity: 1, duration: 0.6, ease: "expo.out" }, 0.1);
    tl.to(strip, { x: -380, duration: 1.0, ease: "power3.inOut" }, 3.4);
    tl.to(pf, { width: 1680 * 12 / 50, duration: 1.2, ease: "power2.out" }, 0.6);
    tl.to(L.children, { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 36 QUEST COMPLETE */
  def("quest-complete", "05", 3, "QUEST COMPLETE // CLAIM", function (tl, L) {
    var rows = [["DAILY: CLEAR 3 STAGES", 2, 3, "gold", "x800"], ["DAILY: SUMMON ONCE", 0, 1, "diamond", "x20"], ["WEEKLY: CLIMB THE ARK", 6, 10, "bytes", "x250"]];
    rows.forEach(function (r, i) {
      var y = 270 + i * 190, row = A(L, { left: "300px", top: y + "px", width: "1320px", height: "160px", background: "rgba(21,25,37,.95)", border: "2px solid rgba(100,232,255,.45)", borderLeft: "10px solid " + C.vio, clipPath: cut(20) });
      T(row, "voice", r[0], { left: "34px", top: "20px", fontSize: "70px", color: C.wh });
      var tr = A(row, { left: "36px", top: "112px", width: "700px", height: "12px", background: "rgba(100,232,255,.2)" });
      var f = A(tr, { left: "0", top: "0", height: "12px", width: (700 * r[1] / r[2]) + "px", background: C.cy });
      var c = T(row, "mono", r[1] + "/" + r[2], { left: "760px", top: "100px", fontSize: "28px", color: C.wh, fontWeight: 700 });
      UI.icon(r[3], 70, row, { left: "900px", top: "44px" }); T(row, "mono", r[4], { left: "985px", top: "66px", fontSize: "30px", color: C.wh, fontWeight: 700 });
      var btn = A(row, { left: "1110px", top: "40px", width: "170px", height: "80px", border: "3px solid rgba(247,244,250,.35)", clipPath: cut(12) });
      var bl = T(btn, "voice", "GO", { left: "0", width: "170px", textAlign: "center", top: "8px", fontSize: "60px", color: "rgba(247,244,250,.6)" });
      tl.fromTo(row, { opacity: 0, x: -200 }, { opacity: 1, x: 0, duration: 0.35, ease: "expo.out" }, 0.1 + i * 0.1);
      if (i === 0) {
        tl.to(f, { width: 700, background: C.rose, duration: 0.5, ease: "power2.out" }, 1.0);
        IG.textAt(tl, c, [[0, "2/3"], [1.4, "3/3"]], 5);
        tl.to(row, { borderLeftColor: C.rose, duration: 0.1 }, 1.5);
        tl.to(btn, { backgroundColor: C.rose, borderColor: C.rose, duration: 0.15 }, 1.6);
        IG.textAt(tl, bl, [[0, "GO"], [1.6, "CLAIM"], [2.6, "✓"]], 5); tl.to(bl, { color: C.wh, duration: 0.1 }, 1.6);
        tl.to(btn, { scale: 1.1, duration: 0.25, yoyo: true, repeat: 1 }, 1.8);
        tl.to(btn, { scale: 0.9, duration: 0.07 }, 2.5); tl.to(btn, { scale: 1, duration: 0.3, ease: "back.out(3)" }, 2.57);
        var st = T(L, "title", "COMPLETE", { left: "1150px", top: "200px", fontSize: "72px", color: C.rose, border: "6px solid " + C.rose, padding: "0 16px", background: "rgba(9,11,18,.85)", opacity: 0 });
        tl.fromTo(st, { opacity: 0, scale: 3, rotation: -12 }, { opacity: 1, scale: 1, rotation: -12, duration: 0.2, ease: "power4.in", immediateRender: false }, 2.65);
        IG.shake(tl, row, 2.85, 0.3, 10, 4);
        tl.to(st, { opacity: 0, duration: 0.2 }, 4.6);
      }
      tl.to(row, { opacity: 0, x: 200, duration: 0.3 }, 4.5 + i * 0.05);
    });
  });

  /* 37 THE ARK floor counter */
  def("ark-floor", "05", 4, "THE ARK // ENDLESS TOWER FLOOR", function (tl, L) {
    var stack = A(L, { left: "760px", top: "0px", width: "400px", height: "1080px", overflow: "hidden" });
    var col = A(stack, { left: "0", top: "0", width: "400px", height: "3000px" });
    for (var i = 0; i < 14; i++) {
      var f = 48 - i, y = 120 + i * 130;
      var b = A(col, { left: "40px", top: y + "px", width: "320px", height: "110px", background: "rgba(21,25,37,.95)", border: "2px solid " + (f === 42 ? C.rose : "rgba(100,232,255,.5)") });
      T(b, "mono", "FLOOR " + f, { left: "24px", top: "34px", fontSize: "34px", color: f <= 41 ? "rgba(247,244,250,.5)" : C.wh, fontWeight: 700 });
      if (f <= 41) UI.icon("check", 40, b, { left: "260px", top: "34px" });
    }
    var big = T(L, "title", "41", { left: "1220px", top: "380px", fontSize: "240px", color: C.wh, textShadow: "9px 8px 0 #FF3D8D" });
    T(L, "voice", "FLOOR", { left: "1230px", top: "300px", fontSize: "90px", color: C.rose });
    T(L, "mono", "THE ARK // NEVER RESETS", { left: "1234px", top: "640px", fontSize: "26px", color: C.cy });
    T(L, "jp", "アーク", { left: "300px", top: "380px", fontSize: "200px", color: "transparent", WebkitTextStroke: "2px rgba(118,87,255,.5)" });
    tl.fromTo(col, { y: 500 }, { y: -560, duration: 1.4, ease: "power3.out" }, 0.1);
    tl.to(col, { y: -430, duration: 0.8, ease: "expo.inOut" }, 2.0);
    IG.textAt(tl, big, [[0, "41"], [2.5, "42"]], 5);
    tl.fromTo(big, { scale: 1 }, { scale: 1.25, duration: 0.12, yoyo: true, repeat: 1, immediateRender: false }, 2.5);
    IG.shake(tl, L, 2.5, 0.3, 10, 5);
    tl.fromTo(L, { opacity: 0 }, { opacity: 1, duration: 0.2 }, 0);
    tl.to(L, { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 38 ARENA RANK UP */
  def("arena-rank", "05", 5, "ARENA RANK UP", function (tl, L) {
    var ranks = [["BRONZE", "#7657FF"], ["SILVER", "#64E8FF"], ["GOLD", "#FF3D8D"], ["DIAMOND", "#64E8FF"], ["PLATINUM", "#F7F4FA"], ["CHAMPIONS", "#FF3D8D"]];
    var s = svg(L, 360, 400, "-100 -110 200 220", { left: "780px", top: "180px" });
    var sh = sv(s, "polygon", { points: "0,-100 86,-50 86,50 0,100 -86,50 -86,-50", fill: "#151925", stroke: ranks[0][1], "stroke-width": 8 });
    var inner = sv(s, "polygon", { points: "0,-60 52,-30 52,30 0,60 -52,30 -52,-30", fill: "none", stroke: ranks[0][1], "stroke-width": 3, opacity: .7 });
    var chev = sv(s, "polyline", { points: "-34,10 0,-20 34,10", fill: "none", stroke: C.wh, "stroke-width": 10 });
    var nm = T(L, "voice", "BRONZE", { left: "0", width: "1920px", textAlign: "center", top: "620px", fontSize: "150px", color: C.wh });
    var ru = T(L, "voice", "<span class='bib-skew'>RANK UP</span>", { left: "0", width: "1920px", textAlign: "center", top: "780px", fontSize: "110px", color: C.rose, opacity: 0 });
    var tr = T(L, "mono", "", { left: "0", width: "1920px", textAlign: "center", top: "120px", fontSize: "30px", color: C.cy });
    tl.fromTo(s, { scale: 0, rotation: -90 }, { scale: 1, rotation: 0, duration: 0.4, ease: "back.out(2)" }, 0.1);
    IG.rise(tl, nm, 0.3);
    var keys = [[0, "BRONZE"]], tk = [[0, "TROPHIES 0"]];
    [1, 2, 3, 4, 5].forEach(function (r, j) {
      var t = 0.9 + j * 0.62;
      tl.to(s, { scale: 0.7, duration: 0.08 }, t - 0.08);
      tl.to(s, { scale: 1, rotation: "+=360", duration: 0.4, ease: "back.out(2)" }, t);
      tl.to([sh, inner], { attr: { stroke: ranks[r][1] }, duration: 0.05 }, t);
      keys.push([t, ranks[r][0]]); tk.push([t, "TROPHIES " + (r * 100)]);
    });
    IG.textAt(tl, nm, keys, 5); IG.textAt(tl, tr, tk, 5);
    tl.fromTo(ru, { opacity: 0, scale: 2 }, { opacity: 1, scale: 1, duration: 0.25, ease: "back.out(2)", immediateRender: false }, 3.9);
    tl.to(sh, { attr: { fill: "#3a0f22" }, duration: 0.2 }, 3.9);
    IG.shake(tl, L, 3.9, 0.4, 14, 3);
    tl.to(L.children, { opacity: 0, duration: 0.25 }, 4.65);
  });

  /* 39 MAIL INBOX */
  def("mail-inbox", "05", 6, "MAIL INBOX", function (tl, L) {
    var ic = A(L, { left: "300px", top: "380px", width: "220px", height: "220px" });
    UI.icon("mail", 220, ic, { left: "0", top: "0" });
    var badge = A(ic, { left: "170px", top: "-10px", width: "80px", height: "80px", borderRadius: "50%", background: C.rose, border: "4px solid " + C.bk });
    var bn = T(badge, "title", "0", { left: "0", width: "72px", textAlign: "center", top: "12px", fontSize: "40px", color: C.wh });
    var list = [["SYSTEM", "WELCOME BACK, COMMANDER"], ["GANG", "SEASON REWARDS ARE IN"], ["SYSTEM", "MAINTENANCE COMPENSATION"]];
    list.forEach(function (m, i) {
      var r = A(L, { left: "680px", top: (300 + i * 150) + "px", width: "940px", height: "126px", background: "rgba(21,25,37,.95)", border: "2px solid rgba(100,232,255,.45)", borderLeft: "10px solid " + (i === 1 ? C.vio : C.cy) });
      T(r, "mono", m[0], { left: "30px", top: "16px", fontSize: "24px", color: i === 1 ? C.vio : C.cy });
      T(r, "voice", m[1], { left: "30px", top: "46px", fontSize: "64px", color: C.wh });
      UI.icon(["gold", "ticket", "diamond"][i], 64, r, { left: "840px", top: "30px" });
      tl.fromTo(r, { opacity: 0, x: 120 }, { opacity: 1, x: 0, duration: 0.3, ease: "expo.out" }, 1.6 + i * 0.15);
      tl.to(r, { opacity: 0.45, duration: 0.2 }, 3.0 + i * 0.25);
    });
    tl.fromTo(ic, { scale: 0 }, { scale: 1, duration: 0.3, ease: "back.out(2)" }, 0.1);
    IG.textAt(tl, bn, [[0, "0"], [0.6, "1"], [0.9, "2"], [1.2, "3"], [3.0, "2"], [3.25, "1"], [3.5, "0"]], 5);
    tl.fromTo(badge, { scale: 0 }, { scale: 1, duration: 0.2, ease: "back.out(3)" }, 0.6);
    [0.9, 1.2].forEach(function (t) { tl.to(badge, { scale: 1.3, duration: 0.08, yoyo: true, repeat: 1 }, t); });
    tl.to(ic, { rotation: 8, duration: 0.06, yoyo: true, repeat: 5 }, 0.9);
    tl.to(badge, { scale: 0, duration: 0.2 }, 3.6);
    tl.to(L.children, { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* 40 BATTLE POWER */
  def("battle-power", "05", 7, "BATTLE POWER", function (tl, L) {
    var p = UI.panel(L, 560, 380, 800, 300, { title: "BATTLE POWER", spine: C.rose });
    var n = T(p, "title", "48,210", { left: "30px", top: "70px", fontSize: "150px", color: C.wh, textShadow: "7px 6px 0 #FF3D8D" });
    var d = T(p, "mono", "", { left: "36px", top: "240px", fontSize: "30px", color: C.cy, fontWeight: 700 });
    var sp = A(L, { left: "0", top: "0", width: "1920px", height: "1080px" });
    tl.fromTo(p, { opacity: 0, x: -100 }, { opacity: 1, x: 0, duration: 0.35, ease: "expo.out" }, 0.1);
    IG.count(tl, n, 48210, 53975, 1.2, 1.4, IG.commas, "power2.inOut");
    IG.textAt(tl, d, [[0, ""], [1.2, "▲ +5,765 // EQUIPMENT UPGRADED"]], 5);
    for (var i = 0; i < 10; i++) {
      var s = A(sp, { left: (600 + H(i, 2) * 700) + "px", top: (400 + H(i, 3) * 200) + "px", width: "16px", height: "16px", background: i % 2 ? C.rose : C.cy, transform: "rotate(45deg)" });
      tl.fromTo(s, { opacity: 0, scale: 0 }, { opacity: 1, scale: 1, y: -120 - H(i, 4) * 100, duration: 0.5, ease: "power2.out" }, 2.5 + i * 0.03);
      tl.to(s, { opacity: 0, duration: 0.3 }, 3.0 + i * 0.03);
    }
    tl.to(p, { scale: 1.05, duration: 0.15, yoyo: true, repeat: 1 }, 2.6);
    tl.to(p, { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* ---------------- SHEET 06: ARCADE ---------------- */
  def("night-shift", "06", 1, "NIGHT SHIFT // POWER & DOORS", function (tl, L) {
    var p = UI.panel(L, 260, 220, 1400, 640, { title: "NIGHT SHIFT // CAM 04", spine: C.vio });
    var cam = A(p, { left: "30px", top: "70px", width: "820px", height: "520px", background: "repeating-linear-gradient(0deg, rgba(100,232,255,.06) 0 2px, rgba(9,11,18,0) 2px 5px), #0d1018", border: "2px solid rgba(100,232,255,.4)" });
    var rec = T(cam, "mono", "● REC", { left: "20px", top: "16px", fontSize: "26px", color: C.rose, fontWeight: 700 });
    var clk = T(cam, "mono", "02:00 AM", { right: "20px", top: "16px", fontSize: "26px", color: C.wh });
    var st = T(cam, "voice", "", { left: "0", width: "820px", textAlign: "center", top: "200px", fontSize: "110px", color: "rgba(247,244,250,.12)" });
    var nz = IG.canvas("ns-" + (UIP.k = (UIP.k || 0) + 1), 820, 520, cam);
    IG.clock(tl, 5, function (t) { nz.clearRect(0, 0, 820, 520); var f = Math.floor(t * 30); for (var i = 0; i < 300; i++) { nz.fillStyle = "rgba(247,244,250," + (0.1 * H(i, f)).toFixed(3) + ")"; nz.fillRect(H(i, f + 1) * 820, H(i, f + 2) * 520, 3, 2); }
      if (t > 2.2 && t < 2.6) { nz.fillStyle = "rgba(247,244,250,.35)"; nz.fillRect(0, 0, 820, 520); } }, 0);
    IG.textAt(tl, st, [[0, "HALLWAY"], [2.2, "SIGNAL LOST"], [2.6, "HALLWAY"]], 5);
    tl.to(rec, { opacity: 0, duration: 0.01, repeat: 9, yoyo: true, repeatDelay: 0.25 }, 0);
    T(p, "mono", "POWER", { left: "890px", top: "80px", fontSize: "28px", color: C.cy });
    var pw = T(p, "title", "100%", { left: "890px", top: "120px", fontSize: "110px", color: C.wh });
    var pbar = A(p, { left: "890px", top: "250px", width: "460px", height: "28px", border: "2px solid " + C.cy });
    var pf = A(pbar, { left: "0", top: "0", bottom: "0", width: "456px", background: C.cy });
    var doors = ["LEFT DOOR", "RIGHT DOOR"].map(function (n, i) {
      var d = A(p, { left: (890 + i * 240) + "px", top: "330px", width: "220px", height: "240px", border: "3px solid " + C.cy, background: "rgba(21,25,37,.9)" });
      T(d, "mono", n, { left: "0", width: "220px", textAlign: "center", top: "16px", fontSize: "24px", color: C.wh });
      d.s = T(d, "voice", "OPEN", { left: "0", width: "220px", textAlign: "center", top: "90px", fontSize: "80px", color: C.cy });
      d.bar = A(d, { left: "0", top: "0", width: "220px", height: "0px", background: "repeating-linear-gradient(-45deg, rgba(255,61,141,.85) 0 16px, rgba(9,11,18,.85) 16px 32px)" });
      return d;
    });
    tl.fromTo(p, { opacity: 0, scale: 0.95 }, { opacity: 1, scale: 1, duration: 0.3 }, 0);
    IG.count(tl, pw, 100, 37, 0.4, 4.0, function (v) { return Math.round(v) + "%"; }, "none");
    tl.to(pf, { width: 456 * 0.37, duration: 4.0, ease: "none" }, 0.4);
    tl.to(pf, { background: C.rose, duration: 0.2 }, 3.2);
    [[0, 1.0, 2.2], [1, 2.8, 3.9]].forEach(function (q) {
      var d = doors[q[0]];
      tl.to(d.bar, { height: 240, duration: 0.15, ease: "power4.in" }, q[1]); tl.to(d, { borderColor: C.rose, duration: 0.1 }, q[1]);
      IG.shake(tl, d, q[1] + 0.15, 0.2, 8, q[0] + 2);
      IG.textAt(tl, d.s, [[0, "OPEN"], [q[1], "CLOSED"], [q[2], "OPEN"]], 5);
      tl.to(d.bar, { height: 0, duration: 0.2, ease: "power2.out" }, q[2]); tl.to(d, { borderColor: C.cy, duration: 0.1 }, q[2]);
    });
    tl.to(p, { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("rhythm-lanes", "06", 2, "RHYTHM // 5 LANES & JUDGEMENT", function (tl, L) {
    var lanes = A(L, { left: "610px", top: "60px", width: "700px", height: "960px", background: "rgba(9,11,18,.85)", borderLeft: "2px solid rgba(100,232,255,.4)", borderRight: "2px solid rgba(100,232,255,.4)" });
    for (var i = 1; i < 5; i++) A(lanes, { left: (i * 140) + "px", top: "0", width: "1px", height: "960px", background: "rgba(100,232,255,.25)" });
    var hit = A(lanes, { left: "0", top: "820px", width: "700px", height: "8px", background: C.rose, boxShadow: "0 0 20px " + C.rose });
    var keys = [];
    for (i = 0; i < 5; i++) keys.push(A(lanes, { left: (i * 140 + 20) + "px", top: "850px", width: "100px", height: "60px", border: "3px solid " + C.cy, background: "rgba(21,25,37,.9)" }));
    var notes = [[0, 0.7], [2, 0.9], [4, 1.1], [1, 1.4], [3, 1.6], [2, 1.9], [0, 2.2], [4, 2.2], [1, 2.6], [3, 2.8], [2, 3.1], [2, 3.3], [0, 3.6], [4, 3.8]];
    var judge = T(L, "voice", "", { left: "0", width: "1920px", textAlign: "center", top: "430px", fontSize: "150px", color: C.wh, textShadow: "6px 5px 0 #FF3D8D" });
    var combo = T(L, "title", "", { left: "1360px", top: "430px", fontSize: "120px", color: C.wh });
    T(L, "mono", "COMBO", { left: "1366px", top: "400px", fontSize: "26px", color: C.cy });
    var jk = [[0, ""]], ck = [[0, "0"]];
    notes.forEach(function (n, k) {
      var nt = A(lanes, { left: (n[0] * 140 + 15) + "px", top: "-60px", width: "110px", height: "34px", background: n[0] === 2 ? C.rose : C.cy, boxShadow: "0 0 14px " + (n[0] === 2 ? C.rose : C.cy) });
      tl.fromTo(nt, { y: 0 }, { y: 870, duration: 0.9, ease: "none" }, n[1] - 0.9);
      tl.set(nt, { opacity: 0 }, n[1]);
      tl.to(keys[n[0]], { backgroundColor: "rgba(255,61,141,.6)", duration: 0.05, yoyo: true, repeat: 1 }, n[1]);
      jk.push([n[1], k % 5 === 3 ? "GREAT" : "PERFECT"]); ck.push([n[1], String(k + 1)]);
      tl.fromTo(judge, { scale: 1.4 }, { scale: 1, duration: 0.12, immediateRender: false }, n[1]);
    });
    gsap.set(lanes.querySelectorAll("div"), {});
    IG.textAt(tl, judge, jk, 5); IG.textAt(tl, combo, ck, 5);
    tl.fromTo(lanes, { opacity: 0, scaleY: 0.2 }, { opacity: 1, scaleY: 1, duration: 0.35, ease: "expo.out" }, 0);
    tl.to([lanes, judge, combo], { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("boss-rush", "06", 3, "BOSS RUSH // ROUND COUNTER", function (tl, L) {
    var k = T(L, "mono", "BOSS RUSH // BOSS AFTER BOSS UNTIL YOU FALL", { left: "0", width: "1920px", textAlign: "center", top: "260px", fontSize: "30px", color: C.cy });
    var r = T(L, "voice", "<span class='bib-skew'>ROUND</span>", { left: "560px", top: "320px", fontSize: "260px", color: C.wh });
    var n = T(L, "title", "1", { left: "1160px", top: "300px", fontSize: "300px", color: C.rose, textShadow: "12px 10px 0 #7657FF" });
    var slab = A(L, { left: "-100px", top: "690px", width: "2120px", height: "90px", background: C.rose, transform: "rotate(-3deg)" });
    var cl = T(L, "voice", "", { left: "0", width: "1920px", textAlign: "center", top: "690px", fontSize: "80px", color: C.bk });
    IG.rise(tl, k, 0.1); IG.slam(tl, r, 0.2); IG.slam(tl, n, 0.4, { x: 200, s: 1.8 });
    IG.textAt(tl, n, [[0, "1"], [1.8, "2"], [3.4, "3"]], 5);
    IG.textAt(tl, cl, [[0, ""], [1.4, "BOSS DEFEATED"], [3.0, "BOSS DEFEATED"]], 5);
    [1.4, 3.0].forEach(function (t, i) {
      tl.fromTo(slab, { scaleX: 0 }, { scaleX: 1, duration: 0.2, ease: "expo.out", immediateRender: i === 0 }, t);
      tl.fromTo(cl, { opacity: 0 }, { opacity: 1, duration: 0.1, immediateRender: i === 0 }, t + 0.05);
      tl.to([slab, cl], { opacity: 0, duration: 0.2 }, t + 0.35); tl.set([slab, cl], { opacity: 1 }, t + 0.6);
      tl.fromTo(n, { scale: 2.4, opacity: 0 }, { scale: 1, opacity: 1, duration: 0.25, ease: "back.out(2)", immediateRender: false }, t + 0.4);
      IG.shake(tl, L, t + 0.6, 0.3, 14, i + 3);
    });
    tl.to([k, r, n], { opacity: 0, duration: 0.25 }, 4.6);
  });

  /* ---------------- SHEET 07: MOBILE CONTROLS ---------------- */
  /* stick path: deterministic thumb motion (angle, magnitude) over time */
  function stickPath(t) {
    var keys = [[0, 0, 0], [0.6, 0, 0], [1.0, -0.4, 1], [1.6, 0.3, 1], [2.0, 1.6, 1], [2.6, 2.6, 0.9], [3.0, 3.6, 1], [3.4, 4.6, 0.6], [3.9, 5.9, 1], [4.3, 5.9, 0]];
    for (var i = 1; i < keys.length; i++) if (t <= keys[i][0]) { var a = keys[i - 1], b = keys[i], u = (t - a[0]) / (b[0] - a[0]); u = u * u * (3 - 2 * u); return [a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u]; }
    return [5.9, 0];
  }
  function touchDot(parent) { return A(parent, { left: "0", top: "0", width: "70px", height: "70px", marginLeft: "-35px", marginTop: "-35px", borderRadius: "50%", background: "rgba(247,244,250,.25)", border: "3px solid rgba(247,244,250,.7)", opacity: 0 }); }

  def("joystick-ring", "07", 1, "JOYSTICK A // ROSE RING", function (tl, L) {
    var cx = 960, cy = 540, R = 230;
    var base = IG.ring(L, R, 10, C.rose, { left: (cx - R - 20) + "px", top: (cy - R - 20) + "px" });
    var inner = IG.ring(L, R * 0.42, 3, "rgba(100,232,255,.6)", { left: (cx - R * 0.42 - 6) + "px", top: (cy - R * 0.42 - 6) + "px" }); inner.circle.style.filter = "none"; inner.circle.setAttribute("stroke-dasharray", "10 10");
    var s = svg(L, 1920, 1080, "0 0 1920 1080", { left: "0", top: "0" });
    var wedge = sv(s, "path", { fill: "rgba(255,61,141,.25)", stroke: C.rose, "stroke-width": 3 });
    var ticks = []; for (var i = 0; i < 4; i++) { var a = i * Math.PI / 2; ticks.push(sv(s, "polygon", { points: (cx + Math.cos(a) * (R + 40)) + "," + (cy + Math.sin(a) * (R + 40)) + " " + (cx + Math.cos(a + 0.1) * (R + 18)) + "," + (cy + Math.sin(a + 0.1) * (R + 18)) + " " + (cx + Math.cos(a - 0.1) * (R + 18)) + "," + (cy + Math.sin(a - 0.1) * (R + 18)), fill: C.cy })); }
    var knob = A(L, { left: (cx - 85) + "px", top: (cy - 85) + "px", width: "170px", height: "170px", borderRadius: "50%", background: "radial-gradient(circle at 40% 35%, #ff7ab0, #FF3D8D 55%, #a8174f)", border: "4px solid " + C.wh, boxShadow: "0 0 40px rgba(255,61,141,.7)" });
    var lab = T(L, "mono", "", { left: (cx - 300) + "px", width: "600px", textAlign: "center", top: (cy + R + 70) + "px", fontSize: "28px", color: C.cy });
    var tdot = touchDot(L);
    tl.fromTo([base.svg, inner.svg, s], { opacity: 0, scale: 0.6 }, { opacity: 1, scale: 1, duration: 0.35, ease: "back.out(2)", transformOrigin: "50% 50%" }, 0.1);
    IG.pop(tl, knob, 0.25);
    IG.clock(tl, 5, function (t) {
      var p = stickPath(t), a = p[0], m = p[1], kx = Math.cos(a) * m * R * 0.62, ky = Math.sin(a) * m * R * 0.62;
      gsap.set(knob, { x: kx, y: ky, scale: m > 0.05 ? 0.92 : 1 }); gsap.set(tdot, { x: cx + kx, y: cy + ky, opacity: t > 0.8 && t < 4.3 ? 1 : 0 });
      if (m > 0.2) { var w = 0.55, r0 = R - 6, r1 = R * 0.5; wedge.setAttribute("d", "M" + (cx + Math.cos(a - w) * r1) + " " + (cy + Math.sin(a - w) * r1) + " L" + (cx + Math.cos(a - w) * r0) + " " + (cy + Math.sin(a - w) * r0) + " A" + r0 + " " + r0 + " 0 0 1 " + (cx + Math.cos(a + w) * r0) + " " + (cy + Math.sin(a + w) * r0) + " L" + (cx + Math.cos(a + w) * r1) + " " + (cy + Math.sin(a + w) * r1) + " A" + r1 + " " + r1 + " 0 0 0 " + (cx + Math.cos(a - w) * r1) + " " + (cy + Math.sin(a - w) * r1) + " Z"); wedge.style.opacity = Math.min(1, m); }
      else wedge.style.opacity = 0;
      var deg = ((a * 180 / Math.PI) % 360 + 360) % 360;
      lab.textContent = m > 0.2 ? (m > 0.95 ? "RUN // " : "WALK // ") + Math.round(deg) + "°" : "IDLE // DEAD ZONE";
    }, 0);
    tl.to([base.svg, inner.svg, s, knob, lab], { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("joystick-hex", "07", 2, "JOYSTICK B // HEX 8-WAY", function (tl, L) {
    var cx = 960, cy = 540, R = 240;
    var s = svg(L, 1920, 1080, "0 0 1920 1080", { left: "0", top: "0" });
    var hexPts = []; for (var i = 0; i < 6; i++) { var a = Math.PI / 6 + i * Math.PI / 3; hexPts.push((cx + Math.cos(a) * R) + "," + (cy + Math.sin(a) * R)); }
    sv(s, "polygon", { points: hexPts.join(" "), fill: "rgba(21,25,37,.75)", stroke: C.cy, "stroke-width": 5 });
    var segs = [];
    for (i = 0; i < 8; i++) {
      var a0 = i * Math.PI / 4 - Math.PI / 8 + 0.04, a1 = a0 + Math.PI / 4 - 0.08, r0 = R * 0.55, r1 = R * 0.86;
      segs.push(sv(s, "path", { d: "M" + (cx + Math.cos(a0) * r0) + " " + (cy + Math.sin(a0) * r0) + " L" + (cx + Math.cos(a0) * r1) + " " + (cy + Math.sin(a0) * r1) + " A" + r1 + " " + r1 + " 0 0 1 " + (cx + Math.cos(a1) * r1) + " " + (cy + Math.sin(a1) * r1) + " L" + (cx + Math.cos(a1) * r0) + " " + (cy + Math.sin(a1) * r0) + " A" + r0 + " " + r0 + " 0 0 0 " + (cx + Math.cos(a0) * r0) + " " + (cy + Math.sin(a0) * r0) + " Z", fill: "rgba(100,232,255,.08)", stroke: "rgba(100,232,255,.5)", "stroke-width": 2 }));
    }
    var knob = A(L, { left: (cx - 80) + "px", top: (cy - 80) + "px", width: "160px", height: "160px", clipPath: "polygon(25% 3%, 75% 3%, 100% 50%, 75% 97%, 25% 97%, 0 50%)", background: "linear-gradient(160deg, #8ff1ff, #64E8FF 50%, #2bb3cc)" });
    A(knob, { left: "50px", top: "50px", width: "60px", height: "60px", border: "4px solid rgba(9,11,18,.6)", clipPath: "polygon(25% 3%, 75% 3%, 100% 50%, 75% 97%, 25% 97%, 0 50%)" });
    var dirs = ["E", "SE", "S", "SW", "W", "NW", "N", "NE"];
    var lab = T(L, "mono", "", { left: (cx - 300) + "px", width: "600px", textAlign: "center", top: (cy + R + 50) + "px", fontSize: "28px", color: C.cy });
    var tdot = touchDot(L);
    tl.fromTo([s, knob], { opacity: 0, scale: 0.6, rotation: -30 }, { opacity: 1, scale: 1, rotation: 0, duration: 0.4, ease: "back.out(2)", transformOrigin: "50% 50%" }, 0.1);
    IG.clock(tl, 5, function (t) {
      var p = stickPath(t), a = p[0], m = p[1], kx = Math.cos(a) * m * R * 0.6, ky = Math.sin(a) * m * R * 0.6;
      gsap.set(knob, { x: kx, y: ky }); gsap.set(tdot, { x: cx + kx, y: cy + ky, opacity: t > 0.8 && t < 4.3 ? 1 : 0 });
      var k = ((Math.round(a / (Math.PI / 4)) % 8) + 8) % 8;
      segs.forEach(function (g, i) { var on = m > 0.3 && i === k; g.setAttribute("fill", on ? "rgba(100,232,255,.75)" : "rgba(100,232,255,.08)"); g.setAttribute("stroke", on ? C.wh : "rgba(100,232,255,.5)"); });
      lab.textContent = m > 0.3 ? "DIR // " + dirs[k] : "DIR // —";
    }, 0);
    tl.to([s, knob, lab], { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("joystick-tactical", "07", 3, "JOYSTICK C // TACTICAL TRAIL", function (tl, L) {
    var cx = 960, cy = 540, R = 230;
    var s = svg(L, 1920, 1080, "0 0 1920 1080", { left: "0", top: "0" });
    sv(s, "circle", { cx: cx, cy: cy, r: R, fill: "rgba(9,11,18,.55)", stroke: "rgba(247,244,250,.5)", "stroke-width": 2 });
    sv(s, "circle", { cx: cx, cy: cy, r: R * 0.25, fill: "none", stroke: "rgba(100,232,255,.5)", "stroke-width": 2, "stroke-dasharray": "6 6" });
    for (var i = 0; i < 72; i++) { var a = i * Math.PI / 36, l = i % 9 === 0 ? 28 : 12; sv(s, "line", { x1: cx + Math.cos(a) * R, y1: cy + Math.sin(a) * R, x2: cx + Math.cos(a) * (R - l), y2: cy + Math.sin(a) * (R - l), stroke: i % 9 === 0 ? C.rose : "rgba(247,244,250,.55)", "stroke-width": i % 9 === 0 ? 4 : 2 }); }
    sv(s, "line", { x1: cx - R - 40, y1: cy, x2: cx + R + 40, y2: cy, stroke: "rgba(100,232,255,.3)", "stroke-width": 1.5 });
    sv(s, "line", { x1: cx, y1: cy - R - 40, x2: cx, y2: cy + R + 40, stroke: "rgba(100,232,255,.3)", "stroke-width": 1.5 });
    var trail = sv(s, "polyline", { points: "", fill: "none", stroke: C.rose, "stroke-width": 5, "stroke-linecap": "round", opacity: .8 });
    var needle = sv(s, "line", { x1: cx, y1: cy, x2: cx, y2: cy, stroke: C.cy, "stroke-width": 3 });
    var knob = A(L, { left: (cx - 60) + "px", top: (cy - 60) + "px", width: "120px", height: "120px", borderRadius: "50%", border: "5px solid " + C.wh, background: "rgba(21,25,37,.85)" });
    A(knob, { left: "40px", top: "40px", width: "30px", height: "30px", borderRadius: "50%", background: C.rose });
    var ang = T(L, "mono", "", { left: (cx + R + 50) + "px", top: (cy - 60) + "px", fontSize: "30px", color: C.wh, fontWeight: 700 });
    var mag = T(L, "mono", "", { left: (cx + R + 50) + "px", top: (cy - 10) + "px", fontSize: "26px", color: C.cy });
    var tdot = touchDot(L);
    tl.fromTo([s, knob], { opacity: 0, scale: 1.3 }, { opacity: 1, scale: 1, duration: 0.4, ease: "expo.out", transformOrigin: "50% 50%" }, 0.1);
    IG.rise(tl, ang, 0.4); IG.rise(tl, mag, 0.45);
    IG.clock(tl, 5, function (t) {
      var p = stickPath(t), a = p[0], m = p[1], kx = Math.cos(a) * m * R * 0.7, ky = Math.sin(a) * m * R * 0.7;
      gsap.set(knob, { x: kx, y: ky }); gsap.set(tdot, { x: cx + kx, y: cy + ky, opacity: t > 0.8 && t < 4.3 ? 1 : 0 });
      var pts = []; for (var k = 0; k < 14; k++) { var q = stickPath(Math.max(0, t - k * 0.03)); pts.push((cx + Math.cos(q[0]) * q[1] * R * 0.7).toFixed(1) + "," + (cy + Math.sin(q[0]) * q[1] * R * 0.7).toFixed(1)); }
      trail.setAttribute("points", pts.join(" "));
      needle.setAttribute("x2", cx + Math.cos(a) * R * m); needle.setAttribute("y2", cy + Math.sin(a) * R * m);
      ang.textContent = "ANG " + String(Math.round((((a * 180 / Math.PI) % 360) + 360) % 360)).padStart(3, "0") + "°"; mag.textContent = "MAG " + m.toFixed(2);
    }, 0);
    tl.to([s, knob, ang, mag], { opacity: 0, duration: 0.25 }, 4.6);
  });

  function btn(parent, x, y, sz, label, col, fs) {
    var b = A(parent, { left: (x - sz / 2) + "px", top: (y - sz / 2) + "px", width: sz + "px", height: sz + "px", borderRadius: "50%", background: "rgba(21,25,37,.85)", border: "4px solid " + col, boxShadow: "0 0 18px " + col + "77" });
    b.lab = T(b, "voice", label, { left: "0", width: sz + "px", textAlign: "center", top: (sz / 2 - (fs || 44) * 0.55) + "px", fontSize: (fs || 44) + "px", color: C.wh });
    return b;
  }
  function press(tl, b, t, col) {
    tl.to(b, { scale: 0.84, duration: 0.06 }, t); tl.to(b, { scale: 1, duration: 0.3, ease: "back.out(3)" }, t + 0.06);
    tl.fromTo(b, { boxShadow: "0 0 70px " + col }, { boxShadow: "0 0 18px " + col + "77", duration: 0.4, immediateRender: false }, t);
  }
  def("mobile-actions", "07", 4, "TOUCH ACTION CLUSTER", function (tl, L) {
    var cx = 1250, cy = 640;
    var atk = btn(L, cx, cy, 250, "ATTACK", C.rose, 70);
    var m = [btn(L, cx - 270, cy + 40, 140, "MOVE 1", C.cy), btn(L, cx - 230, cy - 170, 140, "MOVE 2", C.cy), btn(L, cx - 60, cy - 290, 140, "MOVE 3", C.cy)];
    var sp = btn(L, cx + 190, cy - 230, 160, "SPECIAL", C.vio, 46), ult = btn(L, cx + 300, cy - 30, 170, "ULTIMATE", C.rose, 44);
    var all = [atk].concat(m, [sp, ult]);
    all.forEach(function (b, i) { tl.fromTo(b, { opacity: 0, scale: 0.3 }, { opacity: 1, scale: 1, duration: 0.3, ease: "back.out(2.5)" }, 0.1 + i * 0.06); });
    var tdot = touchDot(L);
    var seq = [[atk, 0.9, C.rose], [atk, 1.15, C.rose], [atk, 1.4, C.rose], [m[0], 1.8, C.cy], [m[1], 2.2, C.cy], [m[2], 2.6, C.cy], [sp, 3.1, C.vio], [ult, 3.7, C.rose]];
    seq.forEach(function (q) {
      press(tl, q[0], q[1], q[2]);
      var r = q[0].getBoundingClientRect ? null : null;
      tl.set(tdot, { x: parseFloat(q[0].style.left) + parseFloat(q[0].style.width) / 2, y: parseFloat(q[0].style.top) + parseFloat(q[0].style.height) / 2, opacity: 1 }, q[1] - 0.05);
      tl.set(tdot, { opacity: 0 }, q[1] + 0.18);
    });
    // cooldown dims on used moves
    [m[0], m[1], m[2], sp].forEach(function (b, i) { tl.to(b, { opacity: 0.4, duration: 0.1 }, seq[3 + i][1] + 0.2); tl.to(b, { opacity: 1, duration: 0.2 }, seq[3 + i][1] + 1.0); });
    var burst = T(L, "voice", "<span class='bib-skew'>ULTIMATE</span>", { left: "300px", top: "380px", fontSize: "170px", color: C.rose, textShadow: "7px 6px 0 #7657FF", opacity: 0 });
    IG.slam(tl, burst, 3.75, { x: -200, s: 1.6, ir: false }); tl.to(burst, { opacity: 0, duration: 0.2 }, 4.4);
    tl.to(all, { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("mobile-switch-dodge", "07", 5, "TOUCH SWITCH & DODGE", function (tl, L) {
    var sw = btn(L, 760, 560, 200, "SWITCH", C.cy, 56);
    var dots = [0, 1, 2].map(function (i) { return A(L, { left: (712 + i * 40) + "px", top: "690px", width: "20px", height: "20px", transform: "rotate(45deg)", background: i === 0 ? C.rose : "rgba(247,244,250,.35)" }); });
    var dg = btn(L, 1160, 560, 200, "DODGE", C.vio, 58);
    var bl = btn(L, 1400, 400, 150, "BLOCK", C.wh, 46);
    var tdot = touchDot(L);
    [sw, dg, bl].forEach(function (b, i) { tl.fromTo(b, { opacity: 0, scale: 0.3 }, { opacity: 1, scale: 1, duration: 0.3, ease: "back.out(2.5)" }, 0.1 + i * 0.08); });
    dots.forEach(function (d, i) { tl.fromTo(d, { opacity: 0 }, { opacity: 1, duration: 0.2 }, 0.3 + i * 0.05); });
    [[1.0, 1], [2.0, 2]].forEach(function (q) {
      press(tl, sw, q[0], C.cy); tl.to(sw, { rotation: "+=360", duration: 0.4, ease: "back.out(2)" }, q[0]);
      dots.forEach(function (d, i) { tl.to(d, { backgroundColor: i === q[1] ? C.rose : "rgba(247,244,250,.35)", duration: 0.1 }, q[0]); });
      tl.set(tdot, { x: 760, y: 560, opacity: 1 }, q[0] - 0.05); tl.set(tdot, { opacity: 0 }, q[0] + 0.2);
    });
    // dodge: swipe gesture with afterimages
    var arrow = T(L, "voice", "SWIPE ›››", { left: "1060px", top: "700px", fontSize: "56px", color: C.vio, opacity: 0 });
    tl.fromTo(arrow, { opacity: 0, x: -40 }, { opacity: 1, x: 40, duration: 0.5, ease: "power2.out", immediateRender: false }, 2.8);
    tl.to(arrow, { opacity: 0, duration: 0.2 }, 3.4);
    tl.set(tdot, { x: 1100, y: 560, opacity: 1 }, 2.9); tl.to(tdot, { x: 1300, duration: 0.25, ease: "power2.in" }, 2.95); tl.set(tdot, { opacity: 0 }, 3.25);
    press(tl, dg, 3.0, C.vio);
    for (var g = 0; g < 3; g++) { var gh = A(L, { left: "1060px", top: "460px", width: "200px", height: "200px", borderRadius: "50%", border: "4px solid " + C.vio, opacity: 0 });
      tl.fromTo(gh, { opacity: 0.6 - g * 0.15, x: 0 }, { opacity: 0, x: 80 + g * 70, duration: 0.45, immediateRender: false }, 3.0 + g * 0.04); }
    press(tl, bl, 3.8, C.wh);
    tl.set(tdot, { x: 1400, y: 400, opacity: 1 }, 3.75); tl.set(tdot, { opacity: 0 }, 4.1);
    tl.to([sw, dg, bl].concat(dots), { opacity: 0, duration: 0.25 }, 4.6);
  });

  def("mobile-hud-layout", "07", 6, "MOBILE HUD LAYOUT // FULL", function (tl, L) {
    var ph = A(L, { left: "210px", top: "190px", width: "1500px", height: "700px", borderRadius: "70px", border: "14px solid #151925", background: "radial-gradient(ellipse at 50% 60%, rgba(118,87,255,.25), rgba(9,11,18,0) 70%), #0b0e16", overflow: "hidden", boxShadow: "0 0 0 3px rgba(100,232,255,.35)" });
    var sc = A(ph, { left: "0", top: "0", width: "1472px", height: "672px", transform: "scale(1)", transformOrigin: "0 0" });
    // top HUD: hp + squad
    var hp = A(sc, { left: "60px", top: "40px", width: "420px", height: "22px", border: "2px solid rgba(100,232,255,.5)", transform: "skewX(-20deg)" });
    var hpf = A(hp, { left: "0", top: "0", height: "18px", width: "416px", background: C.rose });
    [0, 1, 2].forEach(function (i) { A(sc, { left: (60 + i * 80) + "px", top: "80px", width: "64px", height: "64px", border: "3px solid " + (i ? "rgba(100,232,255,.5)" : C.rose), background: "rgba(21,25,37,.9)" }); T(sc, "mono", "0" + (i + 1), { left: (74 + i * 80) + "px", top: "96px", fontSize: "24px", color: C.wh, fontWeight: 700 }); });
    T(sc, "mono", "❚❚", { left: "1380px", top: "36px", fontSize: "30px", color: C.wh });
    // left stick
    var st = IG.ring(sc, 110, 6, C.rose, { left: "150px", top: "330px" });
    var kn = A(sc, { left: "222px", top: "402px", width: "100px", height: "100px", borderRadius: "50%", background: C.rose, border: "3px solid " + C.wh });
    // right cluster
    var a = btn(sc, 1250, 500, 150, "ATTACK", C.rose, 40), b1 = btn(sc, 1080, 540, 86, "1", C.cy, 40), b2 = btn(sc, 1100, 410, 86, "2", C.cy, 40), b3 = btn(sc, 1200, 330, 86, "3", C.cy, 40), b4 = btn(sc, 1350, 360, 96, "SP", C.vio, 40);
    var u = btn(sc, 1390, 230, 100, "ULT", C.rose, 40), d = btn(sc, 940, 580, 90, "DODGE", C.vio, 26), s = btn(sc, 960, 450, 90, "SWAP", C.cy, 28);
    tl.fromTo(ph, { opacity: 0, rotation: -6, scale: 0.85 }, { opacity: 1, rotation: 0, scale: 1, duration: 0.5, ease: "expo.out" }, 0);
    [a, b1, b2, b3, b4, u, d, s].forEach(function (q, i) { tl.fromTo(q, { opacity: 0, scale: 0.3 }, { opacity: 1, scale: 1, duration: 0.25, ease: "back.out(2.5)" }, 0.4 + i * 0.05); });
    tl.fromTo(st.svg, { opacity: 0 }, { opacity: 1, duration: 0.3 }, 0.4);
    IG.clock(tl, 5, function (t) { var p = stickPath(t); gsap.set(kn, { x: Math.cos(p[0]) * p[1] * 70, y: Math.sin(p[0]) * p[1] * 70 }); }, 0);
    [[a, 1.2], [a, 1.45], [b2, 2.0], [s, 2.6], [u, 3.4]].forEach(function (q) { press(tl, q[0], q[1], C.rose); });
    tl.to(hpf, { width: 300, duration: 0.1 }, 2.2);
    var lb = T(L, "mono", "LEFT: MOVE // RIGHT: ATTACK, MOVES 1-3, SPECIAL, ULTIMATE, DODGE, SWAP", { left: "0", width: "1920px", textAlign: "center", top: "930px", fontSize: "26px", color: C.cy });
    IG.rise(tl, lb, 0.8);
    tl.to([ph, lb], { opacity: 0, duration: 0.25 }, 4.6);
  });
})(window);
