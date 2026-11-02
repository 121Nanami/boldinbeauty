/* ui.js: shared BoldInBeauty UI kit in the trailer's HUD look: icons, panels, mock pages. */
(function (root) {
  var C = IG.C, el = IG.el;
  var UI = {};
  /* SVG icons, drawn in the brand palette. size in px. */
  var ICON = {
    gold: '<polygon points="50,6 88,28 88,72 50,94 12,72 12,28" fill="#151925" stroke="#FF3D8D" stroke-width="7"/><text x="50" y="66" text-anchor="middle" font-family="Archivo Black" font-size="44" fill="#F7F4FA">G</text>',
    diamond: '<polygon points="50,6 90,40 50,94 10,40" fill="#151925" stroke="#64E8FF" stroke-width="7"/><polyline points="10,40 90,40" stroke="#64E8FF" stroke-width="4" fill="none"/><polyline points="32,40 50,94 68,40 50,6 32,40" stroke="#64E8FF" stroke-width="3" fill="none" opacity=".7"/>',
    bytes: '<rect x="18" y="18" width="64" height="64" fill="#151925" stroke="#7657FF" stroke-width="7"/><g stroke="#7657FF" stroke-width="6">' +
      '<line x1="34" y1="4" x2="34" y2="18"/><line x1="50" y1="4" x2="50" y2="18"/><line x1="66" y1="4" x2="66" y2="18"/><line x1="34" y1="82" x2="34" y2="96"/><line x1="50" y1="82" x2="50" y2="96"/><line x1="66" y1="82" x2="66" y2="96"/>' +
      '<line x1="4" y1="34" x2="18" y2="34"/><line x1="4" y1="50" x2="18" y2="50"/><line x1="4" y1="66" x2="18" y2="66"/><line x1="82" y1="34" x2="96" y2="34"/><line x1="82" y1="50" x2="96" y2="50"/><line x1="82" y1="66" x2="96" y2="66"/></g>' +
      '<text x="50" y="62" text-anchor="middle" font-family="JetBrains Mono" font-weight="700" font-size="30" fill="#F7F4FA">01</text>',
    heart: '<ellipse cx="50" cy="16" rx="26" ry="7" fill="none" stroke="#64E8FF" stroke-width="4"/><path d="M50 90 L14 54 C2 40 10 22 28 22 C38 22 46 30 50 38 C54 30 62 22 72 22 C90 22 98 40 86 54 Z" fill="#FF3D8D" stroke="#F7F4FA" stroke-width="4"/>',
    ticket: '<path d="M8 26 H92 V42 A8 8 0 0 0 92 58 V74 H8 V58 A8 8 0 0 0 8 42 Z" fill="#151925" stroke="#FF3D8D" stroke-width="6"/><line x1="66" y1="30" x2="66" y2="70" stroke="#FF3D8D" stroke-width="4" stroke-dasharray="6 5"/><text x="36" y="60" text-anchor="middle" font-family="Archivo Black" font-size="26" fill="#F7F4FA">R</text>',
    star: '<polygon points="50,4 62,36 96,38 69,59 79,93 50,73 21,93 31,59 4,38 38,36" fill="#FF3D8D" stroke="#F7F4FA" stroke-width="4"/>',
    mail: '<rect x="8" y="22" width="84" height="58" fill="#151925" stroke="#64E8FF" stroke-width="6"/><polyline points="8,22 50,56 92,22" fill="none" stroke="#64E8FF" stroke-width="6"/>',
    gear: '<circle cx="50" cy="50" r="22" fill="none" stroke="#F7F4FA" stroke-width="8"/><g stroke="#F7F4FA" stroke-width="12">' +
      '<line x1="50" y1="6" x2="50" y2="22"/><line x1="50" y1="78" x2="50" y2="94"/><line x1="6" y1="50" x2="22" y2="50"/><line x1="78" y1="50" x2="94" y2="50"/><line x1="19" y1="19" x2="30" y2="30"/><line x1="70" y1="70" x2="81" y2="81"/><line x1="81" y1="19" x2="70" y2="30"/><line x1="19" y1="81" x2="30" y2="70"/></g>',
    helmet: '<path d="M14 62 C14 30 30 12 50 12 C70 12 86 30 86 62 L86 76 L62 76 L62 60 L38 60 L38 76 L14 76 Z" fill="#151925" stroke="#64E8FF" stroke-width="6"/><rect x="30" y="40" width="40" height="10" fill="#FF3D8D"/>',
    chest: '<path d="M22 14 L40 22 L60 22 L78 14 L92 34 L80 44 L80 88 L20 88 L20 44 L8 34 Z" fill="#151925" stroke="#64E8FF" stroke-width="6"/><line x1="50" y1="26" x2="50" y2="84" stroke="#FF3D8D" stroke-width="5"/>',
    legs: '<path d="M24 10 H76 L80 92 H58 L50 40 L42 92 H20 Z" fill="#151925" stroke="#64E8FF" stroke-width="6"/>',
    weapon: '<polygon points="82,8 92,18 40,70 30,60" fill="#F7F4FA" stroke="#64E8FF" stroke-width="4"/><rect x="18" y="58" width="26" height="9" fill="#FF3D8D" transform="rotate(45 31 62)"/><line x1="12" y1="88" x2="30" y2="70" stroke="#7657FF" stroke-width="10" stroke-linecap="round"/>',
    box: '<polygon points="50,8 90,28 90,72 50,92 10,72 10,28" fill="#151925" stroke="#7657FF" stroke-width="6"/><polyline points="10,28 50,48 90,28" fill="none" stroke="#7657FF" stroke-width="5"/><line x1="50" y1="48" x2="50" y2="92" stroke="#7657FF" stroke-width="5"/><text x="50" y="40" text-anchor="middle" font-family="Archivo Black" font-size="22" fill="#FF3D8D">?</text>',
    lock: '<rect x="20" y="44" width="60" height="46" fill="#151925" stroke="#F7F4FA" stroke-width="6"/><path d="M32 44 V30 A18 18 0 0 1 68 30 V44" fill="none" stroke="#F7F4FA" stroke-width="6"/>',
    check: '<polyline points="14,52 40,78 88,22" fill="none" stroke="#64E8FF" stroke-width="12" stroke-linecap="square"/>'
  };
  UI.icon = function (name, size, parent, css) {
    var d = el("div", "ig-icon", parent, '<svg viewBox="0 0 100 100" width="' + size + '" height="' + size + '" style="overflow:visible">' + ICON[name] + "</svg>",
      Object.assign({ position: "absolute", width: size + "px", height: size + "px" }, css || {}));
    return d;
  };
  UI.ICON = ICON;
  /* HUD panel: steel body, cyan hairline, rose spine on the left */
  UI.panel = function (parent, x, y, w, h, opts) {
    opts = opts || {};
    var p = el("div", "ig-panel", parent, null, { position: "absolute", left: x + "px", top: y + "px", width: w + "px", height: h + "px",
      background: opts.bg || "rgba(21,25,37,.92)", border: "2px solid rgba(100,232,255," + (opts.line || .5) + ")",
      borderLeft: opts.spine === false ? "2px solid rgba(100,232,255,.5)" : (opts.spineW || 8) + "px solid " + (opts.spine || C.rose),
      clipPath: opts.cut === false ? "none" : "polygon(0 0, calc(100% - 26px) 0, 100% 26px, 100% 100%, 0 100%)" });
    if (opts.title) el("div", "mono", p, opts.title, { position: "absolute", left: "22px", top: "14px", fontSize: "24px", color: C.cy });
    return p;
  };
  UI.chip = function (parent, x, y, icon, val, w) {
    var c = el("div", "ig-chip", parent, null, { position: "absolute", left: x + "px", top: y + "px", width: (w || 230) + "px", height: "56px",
      background: "rgba(21,25,37,.9)", border: "2px solid rgba(100,232,255,.45)", clipPath: "polygon(14px 0, 100% 0, calc(100% - 14px) 100%, 0 100%)" });
    UI.icon(icon, 38, c, { left: "18px", top: "8px" });
    var v = el("div", "mono", c, val, { position: "absolute", right: "26px", top: "12px", fontSize: "26px", fontWeight: 700, color: C.wh, letterSpacing: ".04em" });
    c.val = v; return c;
  };
  UI.TABS = ["OPERATOR", "TEAM", "LOBBY", "SHOP", "REVOLUTION", "CAMPAIGN", "SUMMON"];
  UI.nav = function (parent, active, y) {
    var n = el("div", "ig-nav", parent, null, { position: "absolute", left: "0", top: (y == null ? 960 : y) + "px", width: "1920px", height: "120px", background: "linear-gradient(0deg, rgba(9,11,18,.98), rgba(21,25,37,.9))", borderTop: "2px solid rgba(100,232,255,.45)" });
    var tabs = UI.TABS.map(function (t, i) {
      var on = t === active;
      var b = el("div", "voice", n, t, { position: "absolute", left: (60 + i * 260) + "px", top: "26px", width: "240px", textAlign: "center", fontSize: "54px", color: on ? C.wh : "rgba(247,244,250,.45)" });
      if (on) el("div", "", n, null, { position: "absolute", left: (60 + i * 260 + 40) + "px", top: "92px", width: "160px", height: "8px", background: C.rose, transform: "skewX(-30deg)", boxShadow: "0 0 16px " + C.rose });
      return b;
    });
    n.tabs = tabs; return n;
  };
  UI.topbar = function (parent, title, jp) {
    var t = el("div", "", parent, null, { position: "absolute", left: 0, top: 0, width: "1920px", height: "110px" });
    el("div", "voice", t, "<span class='bib-skew'>" + title + "</span>", { position: "absolute", left: "70px", top: "18px", fontSize: "86px", color: C.wh });
    if (jp) el("div", "jp", t, jp, { position: "absolute", left: "76px", top: "100px", fontSize: "22px", color: C.vio, letterSpacing: ".4em" });
    UI.chip(t, 900, 26, "gold", "24,800"); UI.chip(t, 1150, 26, "diamond", "300"); UI.chip(t, 1400, 26, "bytes", "1,250"); UI.chip(t, 1650, 26, "heart", "12", 210);
    return t;
  };
  UI.bg = function (parent, hx, hy) {
    el("div", "", parent, null, { position: "absolute", inset: 0, background: "radial-gradient(ellipse at " + (hx || 30) + "% " + (hy || 60) + "%, rgba(118,87,255,.24), rgba(9,11,18,0) 62%), radial-gradient(ellipse at 85% 15%, rgba(255,61,141,.12), rgba(9,11,18,0) 50%), #090B12" });
    el("div", "", parent, null, { position: "absolute", inset: 0, backgroundImage: "linear-gradient(rgba(100,232,255,.045) 1px, transparent 1px), linear-gradient(90deg, rgba(100,232,255,.045) 1px, transparent 1px)", backgroundSize: "80px 80px" });
  };
  /* Mock pages used to demo transitions */
  UI.page = function (parent, kind) {
    var p = el("div", "ig-page", parent, null, { position: "absolute", inset: 0, overflow: "hidden" });
    if (kind === "lobby") {
      UI.bg(p, 25, 55);
      el("div", "jp", p, "司令官", { position: "absolute", left: "560px", top: "250px", fontSize: "360px", color: "transparent", WebkitTextStroke: "2px rgba(118,87,255,.35)" });
      var rg = IG.ring(p, 120, 18, C.rose, { left: "1520px", top: "180px" });
      UI.topbar(p, "LOBBY", "ロビー");
      var n1 = UI.panel(p, 70, 200, 620, 230, { title: "NOTICE" });
      el("div", "voice", n1, "SEASON PASS", { position: "absolute", left: "22px", top: "60px", fontSize: "92px", color: C.wh });
      el("div", "mono", n1, "50 TIERS // FREE + PAID TRACKS", { position: "absolute", left: "24px", top: "164px", fontSize: "24px", color: "rgba(247,244,250,.75)" });
      var q = UI.panel(p, 70, 460, 620, 460, { title: "QUESTS", spine: C.vio });
      ["DAILY: CLEAR 3 STAGES", "DAILY: SUMMON ONCE", "WEEKLY: CLIMB THE ARK", "WEEKLY: WIN 5 ARENA FIGHTS"].forEach(function (s, i) {
        el("div", "mono", q, s, { position: "absolute", left: "24px", top: (70 + i * 90) + "px", fontSize: "26px", color: C.wh });
        el("div", "", q, null, { position: "absolute", left: "24px", top: (110 + i * 90) + "px", width: "560px", height: "8px", background: "rgba(100,232,255,.2)" });
        el("div", "", q, null, { position: "absolute", left: "24px", top: (110 + i * 90) + "px", width: (560 * [1, .6, .3, .8][i]) + "px", height: "8px", background: i ? C.cy : C.rose });
      });
      var m = UI.panel(p, 1290, 520, 560, 400, { title: "MAIL // FRIENDS // GANGS", spine: C.cy });
      ["MAIL", "FRIENDS", "GANGS"].forEach(function (s, i) {
        el("div", "voice", m, s, { position: "absolute", left: "24px", top: (70 + i * 105) + "px", fontSize: "84px", color: i ? C.wh : C.rose });
      });
      UI.nav(p, "LOBBY");
    } else {
      UI.bg(p, 70, 40);
      UI.topbar(p, "SUMMON", "リクルート");
      var hero = UI.panel(p, 70, 180, 1200, 740, { spine: C.rose, title: "FEATURED BANNER" });
      el("div", "", hero, null, { position: "absolute", left: "-100px", top: "220px", width: "1500px", height: "260px", background: "rgba(255,61,141,.9)", transform: "rotate(-12deg)" });
      el("div", "voice", hero, "<span class='bib-skew'>RECRUIT</span>", { position: "absolute", left: "60px", top: "180px", fontSize: "300px", color: C.wh, textShadow: "8px 7px 0 #7657FF" });
      el("div", "jp", hero, "オペレーター", { position: "absolute", left: "70px", top: "520px", fontSize: "64px", color: C.cy, letterSpacing: ".2em" });
      var ring = IG.ring(hero, 150, 22, C.wh, { left: "880px", top: "110px" });
      ["STANDARD", "FEATURED", "DAILY"].forEach(function (s, i) {
        var b = UI.panel(p, 1310, 180 + i * 130, 540, 110, { spine: i === 1 ? C.rose : "rgba(100,232,255,.5)" });
        el("div", "voice", b, s, { position: "absolute", left: "26px", top: "16px", fontSize: "76px", color: i === 1 ? C.wh : "rgba(247,244,250,.55)" });
      });
      var s1 = el("div", "", p, null, { position: "absolute", left: "1310px", top: "600px", width: "540px", height: "130px", background: C.st, border: "3px solid " + C.cy, clipPath: "polygon(20px 0,100% 0,calc(100% - 20px) 100%,0 100%)" });
      el("div", "voice", s1, "SUMMON X1", { position: "absolute", left: 0, width: "540px", textAlign: "center", top: "22px", fontSize: "90px", color: C.wh });
      var s10 = el("div", "", p, null, { position: "absolute", left: "1310px", top: "760px", width: "540px", height: "150px", background: C.rose, clipPath: "polygon(20px 0,100% 0,calc(100% - 20px) 100%,0 100%)" });
      el("div", "voice", s10, "SUMMON X10", { position: "absolute", left: 0, width: "540px", textAlign: "center", top: "28px", fontSize: "100px", color: C.wh });
      UI.nav(p, "SUMMON");
    }
    return p;
  };
  root.UI = UI;
})(window);
