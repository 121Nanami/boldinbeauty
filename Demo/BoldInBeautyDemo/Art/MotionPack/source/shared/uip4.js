/* uip4.js: Set 2 sheet 08, the screens the VFX doc lists as having no motion piece yet. */
(function (root) {
  var C = IG.C, H = IG.hash, UIP = root.UIP, A = UIP.A, T = UIP.T, cut = UIP.cut;
  UIP.SHEETS["08"] = "MISSING SCREENS";
  function def(id, sheet, n, title, fn) { UIP[id] = { sheet: sheet, n: n, title: title, fn: fn }; }
  var NS = "http://www.w3.org/2000/svg";
  function svg(parent, w, h, vb, css) { var s = document.createElementNS(NS, "svg"); s.setAttribute("width", w); s.setAttribute("height", h); s.setAttribute("viewBox", vb); s.style.position = "absolute"; s.style.overflow = "visible"; for (var k in css) s.style[k] = css[k]; parent.appendChild(s); return s; }
  function sv(parent, tag, attrs) { var e = document.createElementNS(NS, tag); for (var k in attrs) e.setAttribute(k, attrs[k]); parent.appendChild(e); return e; }
  function arcPath(cx, cy, r, a0, a1) { var x0 = cx + Math.cos(a0) * r, y0 = cy + Math.sin(a0) * r, x1 = cx + Math.cos(a1) * r, y1 = cy + Math.sin(a1) * r; return "M" + x0 + " " + y0 + " A" + r + " " + r + " 0 " + (a1 - a0 > Math.PI ? 1 : 0) + " 1 " + x1 + " " + y1; }

  /* 01 STORY DIALOGUE */
  def("story-dialogue", "08", 1, "STORY DIALOGUE", function (tl, L) {
    var box = UI.panel(L, 240, 700, 1440, 290, { title: "STORY" });
    var slot = A(box, { left: "34px", top: "56px", width: "180px", height: "200px", border: "3px dashed rgba(100,232,255,.5)", background: "repeating-linear-gradient(-12deg, rgba(100,232,255,.08) 0 10px, transparent 10px 20px)" });
    T(slot, "mono", "PORTRAIT<br>SLOT", { left: "0", width: "180px", textAlign: "center", top: "70px", fontSize: "22px", color: C.cy, lineHeight: "1.3" });
    var nm = T(box, "voice", "CONTACT NAME", { left: "250px", top: "50px", fontSize: "76px", color: C.rose });
    var l1 = T(box, "mono", "", { left: "252px", top: "150px", fontSize: "30px", color: C.wh }), l2 = T(box, "mono", "", { left: "252px", top: "150px", fontSize: "30px", color: C.wh });
    var nx = T(box, "mono", "T TO CONTINUE ▸", { right: "40px", bottom: "20px", fontSize: "24px", color: C.cy });
    var ph = T(L, "mono", "PLACEHOLDER TEXT: THE OWNER WRITES THE STORY", { left: "240px", top: "660px", fontSize: "22px", color: "rgba(247,244,250,.55)" });
    tl.fromTo(box, { opacity: 0, y: 80 }, { opacity: 1, y: 0, duration: 0.4, ease: "expo.out" }, 0);
    IG.slam(tl, nm, 0.3, { x: -160 });
    IG.typeOn(tl, l1, "[PLACEHOLDER DIALOGUE LINE ONE.]", 0.6, 1.4, true);
    tl.to(l1, { opacity: 0, duration: 0.15 }, 2.9);
    IG.typeOn(tl, l2, "[PLACEHOLDER DIALOGUE LINE TWO.]", 3.1, 1.2, true); tl.fromTo(l2, { opacity: 0 }, { opacity: 1, duration: 0.05, immediateRender: false }, 3.1);
    tl.to(nx, { opacity: 0.15, duration: 0.35, yoyo: true, repeat: 7 }, 1.0);
    IG.rise(tl, ph, 0.5);
    tl.to([box, ph], { opacity: 0, y: 40, duration: 0.3 }, 4.6);
  });

  /* 02 OBJECTIVE TRACKER */
  def("objective-tracker", "08", 2, "OBJECTIVE / SECTION COUNTER", function (tl, L) {
    var p = UI.panel(L, 1260, 70, 600, 250, { title: "SECTION", spine: C.cy });
    var sc = T(p, "title", "2 / 4", { left: "30px", top: "58px", fontSize: "84px", color: C.wh });
    var ob = T(p, "voice", "CLEAR THE ANGELS", { left: "30px", top: "160px", fontSize: "60px", color: C.rose });
    var pips = [];
    for (var i = 0; i < 6; i++) pips.push(A(p, { left: (330 + i * 40) + "px", top: "90px", width: "26px", height: "26px", border: "3px solid " + C.cy, transform: "rotate(45deg)" }));
    var cl = T(L, "voice", "<span class='bib-skew'>SECTION CLEAR</span>", { left: "0", width: "1920px", textAlign: "center", top: "430px", fontSize: "190px", color: C.wh, textShadow: "9px 8px 0 #FF3D8D" });
    var dr = T(L, "mono", "DOOR UNLOCKED ▸", { left: "0", width: "1920px", textAlign: "center", top: "640px", fontSize: "34px", color: C.cy });
    tl.fromTo(p, { opacity: 0, x: 200 }, { opacity: 1, x: 0, duration: 0.4, ease: "expo.out" }, 0);
    pips.forEach(function (d, i) { var t = 0.9 + i * 0.4; tl.to(d, { backgroundColor: C.rose, borderColor: C.rose, scale: 1.3, duration: 0.12, yoyo: true, repeat: 1 }, t); tl.set(d, { backgroundColor: C.rose, borderColor: C.rose }, t + 0.26); });
    IG.textAt(tl, ob, [[0, "CLEAR THE ANGELS"], [3.3, "SECTION CLEARED"]], 5);
    IG.slam(tl, cl, 3.3, { x: 0, s: 1.7, d: 0.22 }); IG.rise(tl, dr, 3.8); IG.shake(tl, L, 3.4, 0.3, 10, 4);
    tl.to([p, cl, dr], { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 03 + 04 RESOURCE GAUGES (8 operator gauges) */
  function gaugeBox(L, x, y, name, col) { var g = A(L, { left: x + "px", top: y + "px", width: "720px", height: "260px", background: "rgba(21,25,37,.92)", border: "2px solid rgba(100,232,255,.4)", borderLeft: "8px solid " + col }); T(g, "voice", name, { left: "28px", top: "12px", fontSize: "56px", color: C.wh }); return g; }
  function fillTo(tl, el, prop, from, to, t, d) { var o = {}; o[prop] = to; var f = {}; f[prop] = from; tl.fromTo(el, f, Object.assign({ duration: d || 0.9, ease: "power2.out", immediateRender: false }, o), t); }
  def("gauges-a", "08", 3, "RESOURCE GAUGES A // BLOOD, SOULS, GRIT, DATA", function (tl, L) {
    var gs = [gaugeBox(L, 190, 180, "BLOOD", C.rose), gaugeBox(L, 1010, 180, "SOULS", C.vio), gaugeBox(L, 190, 500, "GRIT", C.cy), gaugeBox(L, 1010, 500, "DATA", C.cy)];
    gs.forEach(function (g, i) { tl.fromTo(g, { opacity: 0, y: 60 }, { opacity: 1, y: 0, duration: 0.35, ease: "back.out(1.6)" }, 0.1 + i * 0.1); });
    // blood: slanted bar + drips + glow at 80+
    var bt = A(gs[0], { left: "28px", top: "120px", width: "660px", height: "56px", border: "2px solid rgba(100,232,255,.5)", transform: "skewX(-20deg)", background: "rgba(9,11,18,.6)" });
    var bf = A(bt, { left: "0", top: "0", height: "52px", width: "0px", background: "linear-gradient(180deg,#ff6aa9,#FF3D8D 60%,#c41f63)" });
    var bn = T(gs[0], "title", "0", { right: "30px", top: "30px", fontSize: "64px", color: C.wh }), bg = T(gs[0], "mono", "+15% DAMAGE", { left: "28px", top: "196px", fontSize: "26px", color: C.rose, opacity: 0 });
    fillTo(tl, bf, "width", 0, 560, 0.6, 1.6); IG.count(tl, bn, 0, 85, 0.6, 1.6, null, "power2.out"); tl.to(bg, { opacity: 1, duration: 0.2 }, 1.9); tl.to(bt, { boxShadow: "0 0 36px " + C.rose, duration: 0.3, yoyo: true, repeat: 5 }, 1.9);
    // souls: 10 pips
    var sp = []; for (var i = 0; i < 10; i++) sp.push(A(gs[1], { left: (30 + i * 66) + "px", top: "120px", width: "46px", height: "46px", borderRadius: "50%", border: "3px solid " + C.vio }));
    var sn = T(gs[1], "title", "0", { left: "28px", top: "186px", fontSize: "50px", color: C.wh });
    sp.forEach(function (d, k) { if (k < 7) { tl.to(d, { backgroundColor: C.vio, boxShadow: "0 0 18px " + C.vio, scale: 1.25, duration: 0.12, yoyo: true, repeat: 1 }, 0.7 + k * 0.2); tl.set(d, { backgroundColor: C.vio, boxShadow: "0 0 18px " + C.vio }, 0.95 + k * 0.2); } });
    IG.textAt(tl, sn, [[0, "0 / 10"]].concat(Array.apply(null, Array(7)).map(function (_, k) { return [0.7 + k * 0.2, (k + 1) + " / 10"]; })), 5);
    // grit: 6 plates
    var gp = []; for (i = 0; i < 6; i++) gp.push(A(gs[2], { left: (28 + i * 112) + "px", top: "120px", width: "100px", height: "90px", border: "3px solid " + C.cy, clipPath: "polygon(0 0,100% 0,100% 70%,50% 100%,0 70%)" }));
    gp.forEach(function (d, k) { if (k < 4) { tl.to(d, { backgroundColor: C.cy, duration: 0.1 }, 0.8 + k * 0.3); tl.fromTo(d, { y: -14 }, { y: 0, duration: 0.2, ease: "back.out(3)", immediateRender: false }, 0.8 + k * 0.3); } });
    // data: buffer of 10 cells
    var dc = []; for (i = 0; i < 10; i++) dc.push(A(gs[3], { left: (28 + i * 66) + "px", top: "124px", width: "54px", height: "54px", border: "3px solid " + C.cy }));
    dc.forEach(function (d, k) { if (k < 6) { tl.to(d, { backgroundColor: k % 3 ? C.cy : C.rose, duration: 0.08 }, 0.9 + k * 0.15); tl.fromTo(d, { scale: 1.4 }, { scale: 1, duration: 0.15, immediateRender: false }, 0.9 + k * 0.15); } });
    T(gs[3], "mono", "BUFFER 6 / 10", { left: "28px", top: "200px", fontSize: "26px", color: C.cy });
    tl.to(gs, { opacity: 0, y: 40, duration: 0.3, stagger: 0.04 }, 4.5);
  });
  def("gauges-b", "08", 4, "RESOURCE GAUGES B // HEAT, MOMENTUM, STARLIGHT, SPEED", function (tl, L) {
    var gs = [gaugeBox(L, 190, 180, "HEAT", C.rose), gaugeBox(L, 1010, 180, "MOMENTUM", C.cy), gaugeBox(L, 190, 500, "STARLIGHT", C.wh), gaugeBox(L, 1010, 500, "SPEED", C.vio)];
    gs.forEach(function (g, i) { tl.fromTo(g, { opacity: 0, y: 60 }, { opacity: 1, y: 0, duration: 0.35, ease: "back.out(1.6)" }, 0.1 + i * 0.1); });
    // heat: semicircle gauge + needle
    var s1 = svg(gs[0], 300, 160, "-150 -150 300 160", { left: "380px", top: "70px" });
    sv(s1, "path", { d: arcPath(0, 0, 120, Math.PI, 2 * Math.PI), fill: "none", stroke: "rgba(100,232,255,.3)", "stroke-width": 20 });
    var hf = sv(s1, "path", { d: arcPath(0, 0, 120, Math.PI, 2 * Math.PI), fill: "none", stroke: C.rose, "stroke-width": 20 }); hf.setAttribute("stroke-dasharray", 377); hf.setAttribute("stroke-dashoffset", 377);
    var nd = sv(s1, "line", { x1: 0, y1: 0, x2: -100, y2: 0, stroke: C.wh, "stroke-width": 6, "stroke-linecap": "round" });
    var hn = T(gs[0], "title", "0%", { left: "28px", top: "120px", fontSize: "76px", color: C.wh });
    tl.fromTo(hf, { attr: { "stroke-dashoffset": 377 } }, { attr: { "stroke-dashoffset": 377 * 0.2 }, duration: 1.6, ease: "power2.out", immediateRender: false }, 0.6);
    var no = { a: 0 }; function needle() { nd.setAttribute("x2", Math.cos(Math.PI + no.a * Math.PI) * 100); nd.setAttribute("y2", Math.sin(Math.PI + no.a * Math.PI) * 100); } needle();
    tl.fromTo(no, { a: 0 }, { a: 0.8, duration: 1.6, ease: "power2.out", immediateRender: false, onUpdate: needle }, 0.6); IG.count(tl, hn, 0, 80, 0.6, 1.6, function (v) { return Math.round(v) + "%"; });
    tl.to(gs[0], { boxShadow: "0 0 40px rgba(255,61,141,.7)", duration: 0.3, yoyo: true, repeat: 4 }, 2.3);
    // momentum chevrons
    var ch = []; for (var i = 0; i < 8; i++) ch.push(T(gs[1], "title", "›", { left: (30 + i * 80) + "px", top: "90px", fontSize: "150px", color: "rgba(100,232,255,.25)", letterSpacing: "0" }));
    ch.forEach(function (c, k) { if (k < 6) { tl.to(c, { color: k > 3 ? C.rose : C.cy, duration: 0.1 }, 0.7 + k * 0.22); tl.fromTo(c, { x: -20 }, { x: 0, duration: 0.2, immediateRender: false }, 0.7 + k * 0.22); } });
    // starlight: 5 star pips
    var st = []; for (i = 0; i < 5; i++) { var q = A(gs[2], { left: (50 + i * 130) + "px", top: "110px", width: "90px", height: "90px", opacity: 0.25 }); q.innerHTML = '<svg viewBox="0 0 100 100" width="90" height="90"><polygon points="50,4 62,36 96,38 69,59 79,93 50,73 21,93 31,59 4,38 38,36" fill="#F7F4FA" stroke="#64E8FF" stroke-width="4"/></svg>'; st.push(q); }
    st.forEach(function (q, k) { if (k < 4) { tl.to(q, { opacity: 1, duration: 0.12 }, 0.8 + k * 0.4); tl.fromTo(q, { scale: 0.3, rotation: -90 }, { scale: 1, rotation: 0, duration: 0.3, ease: "back.out(3)", immediateRender: false }, 0.8 + k * 0.4); } });
    // speed: speedometer arc + count
    var s4 = svg(gs[3], 300, 160, "-150 -150 300 160", { left: "400px", top: "70px" });
    sv(s4, "path", { d: arcPath(0, 0, 120, Math.PI, 2 * Math.PI), fill: "none", stroke: "rgba(100,232,255,.3)", "stroke-width": 20 });
    var sf = sv(s4, "path", { d: arcPath(0, 0, 120, Math.PI, 2 * Math.PI), fill: "none", stroke: C.vio, "stroke-width": 20 }); sf.setAttribute("stroke-dasharray", 377); sf.setAttribute("stroke-dashoffset", 377);
    var sn = T(gs[3], "title", "0", { left: "28px", top: "110px", fontSize: "96px", color: C.wh });
    T(gs[3], "mono", "+25 PER SECOND UNHIT", { left: "28px", top: "214px", fontSize: "22px", color: C.cy });
    tl.fromTo(sf, { attr: { "stroke-dashoffset": 377 } }, { attr: { "stroke-dashoffset": 377 * 0.35 }, duration: 2.4, ease: "power1.in", immediateRender: false }, 0.6); IG.count(tl, sn, 0, 650, 0.6, 2.4, null, "power1.in");
    tl.to(gs, { opacity: 0, y: 40, duration: 0.3, stagger: 0.04 }, 4.5);
  });

  /* 05 JUDGE MODE WHEEL */
  def("judge-mode-wheel", "08", 5, "JUDGE MODE WHEEL", function (tl, L) {
    var YL = "#FFD23F", RD = "#E5252A", BL = "#3D7BFF";
    var s = svg(L, 700, 700, "-350 -350 700 700", { left: "610px", top: "190px" });
    var arcs = [["JUDGE", YL, -90], ["LAWYER", RD, 30], ["WITNESS", BL, 150]], paths = [], labels = [];
    arcs.forEach(function (a, i) { var a0 = (a[2] - 50) * Math.PI / 180, a1 = (a[2] + 50) * Math.PI / 180; var p = sv(s, "path", { d: arcPath(0, 0, 230, a0, a1), fill: "none", stroke: a[1], "stroke-width": 70, "stroke-linecap": "butt", opacity: 0.35 }); paths.push(p); });
    sv(s, "circle", { r: 150, fill: "rgba(21,25,37,.95)", stroke: "rgba(100,232,255,.5)", "stroke-width": 3 });
    var gv = sv(s, "g", {}); sv(gv, "rect", { x: -70, y: -50, width: 100, height: 60, fill: YL, transform: "rotate(-30)" }); sv(gv, "rect", { x: -10, y: -10, width: 16, height: 100, fill: YL, transform: "rotate(-30)" });
    var cx = [[0, -345], [380, 190], [-380, 190]]; arcs.forEach(function (a, i) { var l = T(L, "voice", a[0], { left: (960 + cx[i][0] - 150) + "px", width: "300px", textAlign: "center", top: (540 + cx[i][1] - 30) + "px", fontSize: "56px", color: a[1] }); labels.push(l); });
    var lock = T(L, "mono", "CONTEMPT OF COURT // WITNESS ONLY // 30 S", { left: "0", width: "1920px", textAlign: "center", top: "910px", fontSize: "30px", color: RD, opacity: 0 });
    var md = T(L, "mono", "MODE: JUDGE", { left: "0", width: "1920px", textAlign: "center", top: "70px", fontSize: "34px", color: C.wh });
    tl.fromTo([s, md], { opacity: 0, scale: 0.7 }, { opacity: 1, scale: 1, duration: 0.4, ease: "back.out(1.8)" }, 0);
    labels.forEach(function (l, i) { IG.rise(tl, l, 0.3 + i * 0.1); });
    var seq = [[0, 0.5], [1, 1.5], [2, 2.5]];
    seq.forEach(function (q) { var t = q[1]; paths.forEach(function (p, i) { tl.to(p, { opacity: i === q[0] ? 1 : 0.3, attr: { "stroke-width": i === q[0] ? 86 : 70 }, duration: 0.18 }, t); }); tl.to(gv.children, { attr: { fill: arcs[q[0]][1] }, duration: 0.15 }, t); tl.to(s, { rotation: "+=6", duration: 0.2 }, t); });
    IG.textAt(tl, md, [[0, "MODE: JUDGE"], [1.5, "MODE: LAWYER"], [2.5, "MODE: WITNESS"], [3.4, "MODE: WITNESS (LOCKED)"]], 5);
    tl.to(lock, { opacity: 1, duration: 0.15 }, 3.4); tl.to(paths[0], { opacity: 0.1, duration: 0.2 }, 3.4); tl.to(paths[1], { opacity: 0.1, duration: 0.2 }, 3.4); IG.shake(tl, s, 3.4, 0.3, 8, 3);
    tl.to([s, md, lock].concat(labels), { opacity: 0, duration: 0.3 }, 4.6);
  });

  /* 06 NARRATOR GLOBAL EXCHANGE PANEL */
  def("global-exchange-panel", "08", 6, "NARRATOR // GLOBAL EXCHANGE PANEL", function (tl, L) {
    var PU = "#7657FF", OG = "#FF8A2B", RD = "#E5252A";
    var p = UI.panel(L, 460, 190, 1000, 700, { title: "GLOBAL EXCHANGE", spine: OG });
    var rem = T(p, "title", "4.75x", { right: "36px", top: "20px", fontSize: "76px", color: OG, textShadow: "5px 4px 0 " + RD });
    T(p, "mono", "REMAINING", { right: "40px", top: "100px", fontSize: "22px", color: C.wh });
    var rows = [["DAMAGE", PU, [1, 3, 2]], ["HEALTH", OG, [1, 1, 2]], ["SPEED", RD, [1, 1.5, 3]], ["ULTIMATE", PU, [1, 1, 1]], ["COOLDOWN", OG, [1, 1, 1]]], fills = [], vals = [];
    rows.forEach(function (r, i) { var y = 160 + i * 96; T(p, "voice", r[0], { left: "34px", top: y + "px", fontSize: "56px", color: C.wh }); var tr = A(p, { left: "290px", top: (y + 14) + "px", width: "560px", height: "34px", border: "2px solid rgba(100,232,255,.5)", background: "rgba(9,11,18,.6)" }); var f = A(tr, { left: "0", top: "0", height: "30px", width: "0", background: r[1] }); var v = T(p, "title", "1.0x", { left: "870px", top: (y + 4) + "px", fontSize: "44px", color: C.wh }); fills.push(f); vals.push(v); for (var k = 1; k < 5; k++) A(tr, { left: (k * 112) + "px", top: "0", width: "2px", height: "30px", background: "rgba(9,11,18,.8)" }); });
    tl.fromTo(p, { opacity: 0, scale: 0.9 }, { opacity: 1, scale: 1, duration: 0.4, ease: "back.out(1.6)" }, 0);
    var stages = [0.5, 1.9, 3.3];
    stages.forEach(function (t, si) {
      rows.forEach(function (r, i) { var from = r[2][Math.max(0, si - 1)], to = r[2][si]; tl.fromTo(fills[i], { width: 112 * from * (si ? 1 : 0.0001) }, { width: 112 * to, duration: 0.6, ease: "power3.out", immediateRender: false }, t + i * 0.06); if (si === 0) tl.set(fills[i], { width: 0 }, 0); IG.count(tl, vals[i], from, to, t + i * 0.06, 0.6, function (v) { return v.toFixed(1) + "x"; }); });
    });
    IG.textAt(tl, rem, [[0, "4.75x"], [0.5, "4.75x"], [1.9, "3.25x"], [3.3, "0.75x"]], 5);
    tl.to(rem, { scale: 1.2, duration: 0.12, yoyo: true, repeat: 1 }, 1.9); tl.to(rem, { scale: 1.2, duration: 0.12, yoyo: true, repeat: 1 }, 3.3);
    tl.to(p, { opacity: 0, scale: 0.95, duration: 0.3 }, 4.6);
  });
})(window);
