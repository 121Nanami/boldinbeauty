/* dream.js: the BB dream-core skin (board v2, approved by GreyHat 2026-10-16).
   Loaded right after ig.js and before ui.js / uip*.js / trans.js, so every piece that builds through IG.el,
   GSAP tweens or a 2D canvas is re-skinned at runtime: neon colours -> pastel by role, cut corners -> pills,
   hard offset shadows -> soft halos, ALL CAPS -> sentence case, slams / glitches / scrambles -> soft motion.
   Deterministic: no Math.random, no Date. */
(function (root) {
  var P = {
    milk: "#FBF6FF", cream: "#FFF8F0", blush: "#FFD9E8", rose: "#F3A6C8", lilac: "#CDBBF5", baby: "#CFE3FF",
    mint: "#D5F2DF", plum: "#5B4A78", plum2: "#8C7BA8", moss: "#BDCC8F", hollow: "#0F051A", vig: "#2A0F1E"
  };
  /* colour families: [fill, line, text] */
  var FAM = {
    dark:   ["#FFF8F0", "#5B4A78", "#5B4A78"],
    light:  ["#FFFDF8", "#5B4A78", "#5B4A78"],
    cyan:   ["#CFE3FF", "#93B2E6", "#8C7BA8"],
    blue:   ["#CFE3FF", "#93B2E6", "#6F84BE"],
    violet: ["#CDBBF5", "#A994E6", "#7D68B8"],
    rose:   ["#F3A6C8", "#E58DB5", "#D46F9E"],
    red:    ["#F4AFB5", "#E8909A", "#C9677A"],
    orange: ["#F9CBA8", "#EEAE84", "#C47F52"],
    yellow: ["#F8E7A8", "#E8CC78", "#A98A35"],
    green:  ["#D5F2DF", "#9ED3B0", "#5E9C78"],
    grey:   ["#EDE6F4", "#A99CBE", "#8C7BA8"]
  };
  var ROLE = { fill: 0, line: 1, text: 2 };
  function hex2(h) { return ("0" + Math.round(h).toString(16)).slice(-2); }
  function toRGB(s) {
    var m;
    if (s.charAt(0) === "#") {
      var h = s.slice(1);
      if (h.length === 3 || h.length === 4) h = h.split("").map(function (c) { return c + c; }).join("");
      return [parseInt(h.substr(0, 2), 16), parseInt(h.substr(2, 2), 16), parseInt(h.substr(4, 2), 16), h.length === 8 ? parseInt(h.substr(6, 2), 16) / 255 : 1];
    }
    m = s.match(/rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)/);
    return m ? [+m[1], +m[2], +m[3], m[4] == null ? 1 : +m[4]] : null;
  }
  function hsl(r, g, b) {
    r /= 255; g /= 255; b /= 255;
    var mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 2, d = mx - mn, s = 0, h = 0;
    if (d) {
      s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
      h = mx === r ? ((g - b) / d + (g < b ? 6 : 0)) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; h *= 60;
    }
    return [h, s, l];
  }
  function family(rgb) {
    var x = hsl(rgb[0], rgb[1], rgb[2]), h = x[0], s = x[1], l = x[2];
    if (l < 0.3) return "dark";
    if (l > 0.86 && s < 0.6) return "light";
    if (s < 0.18) return l < 0.5 ? "dark" : "grey";
    if (h >= 170 && h < 205) return "cyan";
    if (h >= 205 && h < 245) return "blue";
    if (h >= 245 && h < 300) return "violet";
    if (h >= 300 || h < 4) return (h >= 345 || h < 4) && s > 0.75 && l < 0.55 ? "red" : "rose";
    if (h < 18) return "red";
    if (h < 42) return "orange";
    if (h < 72) return "yellow";
    return "green";
  }
  function mapOne(s, role) {
    var rgb = toRGB(s); if (!rgb) return s;
    var c = toRGB(FAM[family(rgb)][ROLE[role] || 0]);
    var a = rgb[3];
    return a >= 1 ? "#" + hex2(c[0]) + hex2(c[1]) + hex2(c[2]) : "rgba(" + c[0] + "," + c[1] + "," + c[2] + "," + (+a.toFixed(3)) + ")";
  }
  var COL_RE = /#[0-9a-fA-F]{8}\b|#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3,4}\b|rgba?\([^)]*\)/g;
  function mapColors(str, role) {
    if (typeof str !== "string") return str;
    return str.replace(COL_RE, function (m) { return mapOne(m, role); });
  }

  /* ---- copy: ALL CAPS -> sentence case, keeping acronyms and proper names ---- */
  var KEEP = { HP: 1, MP: 1, SP: 1, UI: 1, UCN: 1, KO: 1, VS: 1, OK: 1, XP: 1, EXP: 1, PVP: 1, AI: 1, ID: 1, SSR: 1, SR: 1, UR: 1, MVP: 1, LV: 0 };
  var PROPER = ["night shift", "boss rush", "halo fall", "neon memory", "firewall breach", "ark rally", "season pass", "global exchange",
    "ark", "judge", "lawyer", "witness", "narrator", "rhythm", "arena", "gold", "bytes", "angel", "angels", "luna", "media", "tokyo", "neo"];
  function sentence(seg) {
    var out = seg.toLowerCase().replace(/^(\s*[^a-z]*)([a-z])/, function (m, a, b) { return a + b.toUpperCase(); });
    out = out.replace(/\b[a-z]+\b/g, function (w) { var U = w.toUpperCase(); return KEEP[U] ? U : w; });
    PROPER.forEach(function (p) {
      out = out.replace(new RegExp("\\b" + p + "\\b", "g"), function (m) { return m.replace(/\b[a-z]/g, function (c) { return c.toUpperCase(); }); });
    });
    out = out.replace(/\bx(\d+)\b/g, "×$1").replace(/\blv\.?\s?(\d)/g, "Lv $1");
    return out;
  }
  function recase(t) {
    if (typeof t !== "string" || !/[A-Z]{2}/.test(t) || /[a-z]/.test(t.replace(/<[^>]*>|&[a-z]+;/g, ""))) return t;
    return t.replace(/(<[^>]*>)|([^<]+)/g, function (m, tag, txt) {
      if (tag) return tag;
      return txt.split(/\s*\/\/\s*/).map(sentence).join(" · ");
    });
  }

  /* ---- CSS value rewriting by property ---- */
  var TEXTP = { color: 1, webkitTextStroke: 0, WebkitTextStroke: 0 };
  var LINEP = /border|outline|stroke|boxShadow|filter|textShadow|caret/i;
  function px(v) { var n = parseFloat(v); return isNaN(n) ? null : n; }
  var HALO = "0 0 2px #FFF8F0, 0 0 2px #FFF8F0, 0 0 24px rgba(205,187,245,.85)";
  function css(k, v, el) {
    if (v == null || typeof v !== "string") return v;
    if (k === "clipPath") {
      if (/polygon/.test(v)) { el._pill = true; return "none"; }
      return v;
    }
    if (k === "textShadow") return /^0 0 /.test(v) ? HALO : HALO;
    if (k === "transform") {
      return v.replace(/rotate\((-?[\d.]+)deg\)/g, function (m, a) { a = +a; return Math.abs(a) <= 15 ? "rotate(" + (a * 0.3).toFixed(2) + "deg)" : m; })
              .replace(/skewX\((-?[\d.]+)deg\)/g, function (m, a) { return "skewX(" + (+a * 0.15).toFixed(2) + "deg)"; });
    }
    if (k === "filter" && /brightness\(/.test(v)) {
      v = v.replace(/brightness\(([\d.]+)\)/g, function (m, b) { b = +b; return b < 1 ? "opacity(" + (0.35 + 0.65 * b).toFixed(3) + ") saturate(" + (0.4 + 0.6 * b).toFixed(3) + ")" : m; });
    }
    if (k === "fontFamily") return v.replace(/League Gothic|Archivo Black|Bebas Neue/g, "Fraunces").replace(/JetBrains Mono|Noto Sans JP/g, "M PLUS Rounded 1c");
    if (k === "backgroundImage" && /linear-gradient\(rgba\(100,232,255/.test(v)) return "none"; /* tech grid */
    if (TEXTP[k]) return mapColors(v, "text");
    if (LINEP.test(k)) return mapColors(v, "line").replace(/(\d+)px solid/, function (m, n) { return Math.max(2, Math.min(+n, 6)) + "px solid"; });
    if (/^background|^fill$/.test(k)) return mapColors(v, "fill");
    return mapColors(v, "fill");
  }
  /* html strings (svg icons, spans) */
  function html(h, cls) {
    if (typeof h !== "string") return h;
    h = h.replace(/(fill|stroke|stop-color|color)\s*=\s*"([^"]*)"/g, function (m, a, v) {
      var rgb = toRGB(v);
      if (a === "fill" && rgb && family(rgb) === "light") return 'fill="#FFE6EF"';
      return a + '="' + mapColors(v, a === "stroke" ? "line" : a === "color" ? "text" : "fill") + '"';
    });
    h = h.replace(/(<text[^>]*?)fill="([^"]*)"/g, function (m, a, v) { return a + 'fill="' + mapColors(v, "text") + '"'; });
    h = h.replace(/font-family="(League Gothic|Archivo Black)"/g, 'font-family="Fraunces"').replace(/font-family="JetBrains Mono"/g, 'font-family="M PLUS Rounded 1c"');
    h = h.replace(/(style="[^"]*)"/g, function (m, s) { return mapColors(s, /color:/.test(s) ? "text" : "fill") + '"'; });
    h = h.replace(/class='bib-skew'|class="bib-skew"/g, "");
    return recase(h);
  }
  var FONT_SCALE = { voice: 0.66, title: 0.92, mono: 1.12, jp: 1 };

  /* ---- IG.el: the chokepoint every piece builds through ---- */
  var IG = root.IG, el0 = IG.el;
  IG.el = function (tag, cls, parent, h, c) { return IG.raw(function () { return elD(tag, cls, parent, h, c); }); };
  function elD(tag, cls, parent, h, c) {
    var d = el0(tag, cls, parent, html(h, cls), null);
    if (c) for (var k in c) {
      var v = c[k];
      if (k === "fontSize" && cls) { var s = FONT_SCALE[(cls.match(/voice|title|mono|jp/) || [])[0]]; if (s && px(v) != null) v = (px(v) * s).toFixed(1) + "px"; }
      d.style[k] = css(k, v, d);
    }
    if (c) shape(d, c);
    if (cls && /\b(voice|title)\b/.test(cls)) {
      /* cream outline painted under the plum fill: reads over dark scenes and pastel ones alike */
      var fs = px(d.style.fontSize) || 60;
      d.style.webkitTextStroke = Math.max(3, fs * 0.075).toFixed(1) + "px #FFF8F0";
      d.style.paintOrder = "stroke fill";
      d.style.textShadow = "0 0 " + Math.max(10, fs * 0.25).toFixed(0) + "px rgba(205,187,245,.9)";
    }
    if (cls && /\b(mono|jp)\b/.test(cls) && !(c && c.textShadow)) d.style.textShadow = "0 0 2px #FFF8F0, 0 0 2px #FFF8F0, 0 0 6px rgba(255,248,240,.9)";
    return d;
  }
  /* BB shape language: solid fills get three cel bands, a thin plum ink edge and round corners */
  function mixRGB(a, b, k) { return [0, 1, 2].map(function (i) { return Math.round(a[i] + (b[i] - a[i]) * k); }); }
  function rgbs(c, a) { return "rgba(" + c[0] + "," + c[1] + "," + c[2] + "," + a + ")"; }
  function shape(d, c) {
    var h = px(c.height), w = px(c.width), bg = c.background || c.backgroundColor, explicitR = c.borderRadius;
    var full = w != null && h != null && w >= 1900 && h >= 1000;
    var solid = typeof bg === "string" && !/gradient|url\(/.test(bg) && toRGB(mapColors(bg, "fill"));
    if (solid && !full && h != null && h >= 18 && solid[3] >= 0.55) {
      var hi = mixRGB(solid, [255, 255, 255], 0.55), lo = mixRGB(solid, [91, 74, 120], 0.2), a = solid[3];
      d.style.background = "linear-gradient(to bottom," + rgbs(hi, a) + " 0 24%," + rgbs(solid, a) + " 24% 72%," + rgbs(lo, a) + " 72%)";
      if (!c.border && !c.borderTop && !c.borderLeft && !c.outline && h >= 34 && (w == null || w >= 34)) d.style.border = "3px solid #5B4A78";
      d._pill = true;
    }
    if ((d._pill || (bg && !full && h != null)) && !explicitR) {
      d.style.borderRadius = h != null && h <= 170 ? "999px" : (Math.min(48, (h || 200) * 0.12)) + "px";
    }
  }
  IG.dream = { P: P, map: mapColors, recase: recase, css: css };

  /* ---- direct style / attribute writes (pieces that skip IG.el, and GSAP's per-frame sets) ----
     Guarded: only colours that are still neon, near-black or stark white get mapped, so pastel values
     (already mapped, or GSAP interpolations between pastels) pass through untouched. */
  var BY = 0;
  IG.raw = function (fn) { BY++; try { return fn(); } finally { BY--; } };
  function neon(s) {
    var rgb = toRGB(s); if (!rgb) return false;
    var x = hsl(rgb[0], rgb[1], rgb[2]);
    return (x[1] >= 0.75 && x[2] >= 0.3 && x[2] <= 0.78) || x[2] < 0.28 || (x[2] > 0.93 && x[1] < 0.5) || (x[1] >= 0.6 && x[2] < 0.5);
  }
  function guarded(str, role) {
    if (typeof str !== "string") return str;
    return str.replace(COL_RE, function (m) { return neon(m) ? mapOne(m, role) : m; });
  }
  IG.dream.guarded = guarded;
  var SP = root.CSSStyleDeclaration.prototype;
  var HOOK = { background: "fill", backgroundColor: "fill", backgroundImage: "fill", color: "text", borderColor: "line", borderLeftColor: "line",
    borderTopColor: "line", borderRightColor: "line", borderBottomColor: "line", border: "line", borderLeft: "line", borderTop: "line", borderRight: "line",
    borderBottom: "line", boxShadow: "line", outline: "line", outlineColor: "line", fill: "fill", stroke: "line", filter: "line", webkitTextStroke: "line" };
  Object.keys(HOOK).forEach(function (k) {
    var dsc = Object.getOwnPropertyDescriptor(SP, k); if (!dsc || !dsc.set) return;
    Object.defineProperty(SP, k, { configurable: true, enumerable: dsc.enumerable, get: dsc.get, set: function (v) {
      dsc.set.call(this, BY || typeof v !== "string" ? v : guarded(v, HOOK[k])); } });
  });
  var setP = SP.setProperty;
  SP.setProperty = function (n, v, p) { return setP.call(this, n, BY || typeof v !== "string" ? v : guarded(v, /^color$/.test(n) ? "text" : /border|stroke|shadow|outline|filter/.test(n) ? "line" : "fill"), p); };
  var EP = root.Element.prototype, sa = EP.setAttribute;
  EP.setAttribute = function (n, v) {
    if (!BY && typeof v === "string") {
      if (n === "fill" || n === "stop-color") v = guarded(v, "fill");
      else if (n === "stroke") v = guarded(v, "line");
      else if (n === "style") v = guarded(v, "fill");
    }
    return sa.call(this, n, v);
  };
  /* hand-authored dream art (dtrans.js, linger.js) bypasses every mapping */
  IG.rawEl = function () { var a = arguments; return IG.raw(function () { return el0.apply(null, a); }); };

  /* ---- GSAP: remap colours / transforms in tween vars, calm big overshoots ---- */
  function fixVars(v) {
    if (!v || typeof v !== "object") return v;
    var o = {};
    for (var k in v) {
      var x = v[k];
      if (k === "ease" && typeof x === "string") { x = x.replace(/back\.(out|inOut)\(([\d.]+)\)/, function (m, a, n) { return "back." + a + "(" + Math.min(+n, 1.5) + ")"; }); }
      else if (k === "attr" && x && typeof x === "object") { var a = {}; for (var j in x) a[j] = typeof x[j] === "string" ? mapColors(x[j], j === "stroke" ? "line" : "fill") : x[j]; x = a; }
      else if (k === "rotation" && typeof x === "number" && Math.abs(x) <= 15) x = x * 0.3;
      else if (k === "skewX" && typeof x === "number") x = x * 0.15;
      else if (typeof x === "string" && k !== "ease") x = css(k, x, {});
      o[k] = x;
    }
    return o;
  }
  var gs = root.gsap, TP = gs.core.Timeline.prototype;
  ["to", "from", "set"].forEach(function (m) { var f = TP[m]; TP[m] = function (t, v, p) { return f.call(this, t, fixVars(v), p); }; });
  var ft = TP.fromTo; TP.fromTo = function (t, a, b, p) { return ft.call(this, t, fixVars(a), fixVars(b), p); };
  var gset = gs.set; gs.set = function (t, v) { return gset.call(gs, t, fixVars(v)); };

  /* ---- canvas: remap styles; text drawn in text colours; fonts swapped ---- */
  var CP = root.CanvasRenderingContext2D.prototype;
  function hook(prop, role) {
    var d = Object.getOwnPropertyDescriptor(CP, prop);
    Object.defineProperty(CP, prop, { configurable: true, get: function () { return d.get.call(this); },
      set: function (v) { if (prop === "fillStyle") this._srcFill = v; d.set.call(this, typeof v === "string" ? mapColors(v, role) : v); } });
  }
  var FS = Object.getOwnPropertyDescriptor(CP, "fillStyle");
  hook("fillStyle", "fill"); hook("strokeStyle", "line"); hook("shadowColor", "line");
  var fd = Object.getOwnPropertyDescriptor(CP, "font");
  Object.defineProperty(CP, "font", { configurable: true, get: function () { return fd.get.call(this); }, set: function (v) {
    var sc = /League Gothic/.test(v) ? 0.66 : 1;
    v = v.replace(/([\d.]+)px/, function (m, n) { return (n * sc).toFixed(1) + "px"; })
         .replace(/"?League Gothic"?|"?Archivo Black"?/g, '"Fraunces"').replace(/"?JetBrains Mono"?|"?Noto Sans JP"?/g, '"M PLUS Rounded 1c"');
    fd.set.call(this, v); } });
  ["fillText", "strokeText"].forEach(function (m) {
    var f = CP[m]; CP[m] = function (t, x, y, w) {
      var src = this._srcFill, swap = m === "fillText" && typeof src === "string";
      if (swap) FS.set.call(this, mapColors(src, "text"));
      var r = f.call(this, recase(String(t)), x, y, w);
      if (swap) FS.set.call(this, mapColors(src, "fill"));
      return r;
    };
  });
  var acs = root.CanvasGradient.prototype.addColorStop;
  root.CanvasGradient.prototype.addColorStop = function (o, c) { return acs.call(this, o, mapColors(c, "fill")); };

  /* ---- motion helpers: soft versions with the same signatures and timing ---- */
  var hash = IG.hash;
  IG.slam = function (tl, e, at, o) {
    o = o || {};
    tl.fromTo(e, { opacity: 0, x: (o.x == null ? -120 : o.x) * 0.15, y: (o.y || 0) + 24, scale: 0.97 },
      { opacity: 1, x: 0, y: 0, scale: 1, duration: Math.max(o.d || 0.32, 0.42), ease: "back.out(1.3)", immediateRender: o.ir == null ? true : o.ir }, at);
  };
  IG.pop = function (tl, e, at, d) { tl.fromTo(e, { opacity: 0, scale: 0.7, y: 14 }, { opacity: 1, scale: 1, y: 0, duration: Math.max(d || 0.35, 0.4), ease: "back.out(1.5)" }, at); };
  /* glitch -> memory smear: one soft blurred double, two steps, then clean */
  IG.glitch = function (tl, target, at, dur, amp, seed) {
    var s = 1 / 13;
    tl.set(target, { x: (amp || 20) * 0.35, filter: "blur(3px)", opacity: 0.8 }, at);
    tl.set(target, { x: -(amp || 20) * 0.15, filter: "blur(1.5px)", opacity: 0.9 }, at + s);
    tl.set(target, { x: 0, filter: "none", opacity: 1 }, at + 2 * s);
  };
  /* scramble -> letters float in (no glyph noise) */
  IG.scramble = function (tl, e, text, at, dur, seed) {
    e = IG.$(e); text = recase(text); var p = { v: 0 };
    function draw(v) { var k = Math.round(v * text.length); e.textContent = text.slice(0, k); }
    tl.fromTo(p, { v: 0 }, { v: 1, duration: dur, ease: "none", immediateRender: false, onUpdate: function () { draw(p.v); } }, at);
    draw(0); return p;
  };
  var typeOn0 = IG.typeOn; IG.typeOn = function (tl, e, text, at, dur, caret) { return typeOn0(tl, e, recase(text), at, dur, false); };
  var textAt0 = IG.textAt; IG.textAt = function (tl, e, keys, total) { return textAt0(tl, e, keys.map(function (k) { return [k[0], recase(k[1])]; }), total); };
  /* shake -> gentle sway */
  IG.shake = function (tl, target, at, dur, amp, seed) {
    var o = { t: 0 }; amp = (amp || 10) * 0.22;
    tl.fromTo(o, { t: 0 }, { t: 1, duration: dur, ease: "none", immediateRender: false, onUpdate: function () {
      var k = (1 - o.t) * amp; gs.set(target, { x: Math.sin(o.t * 9) * k, y: Math.cos(o.t * 7) * k * 0.5 }); } }, at);
    tl.set(target, { x: 0, y: 0 }, at + dur + 0.001);
  };
  IG.flash = function (tl, e, at, peak, d) { tl.fromTo(e, { opacity: Math.min(peak || 0.9, 0.55) }, { opacity: 0, duration: Math.max(d || 0.25, 0.4), ease: "power2.out", immediateRender: false }, at); };
  /* rain -> slow pastel petals */
  IG.rain = function (x, t, o) {
    o = o || {}; var N = Math.round((o.n || 140) * 0.35), W = x.W, H = x.H, sd = o.seed || 1, cols = ["#FFD9E8", "#CFE3FF", "#CDBBF5", "#D5F2DF"];
    for (var i = 0; i < N; i++) {
      var sp = 40 + 50 * hash(i, sd), x0 = hash(i, sd + 2) * W, y0 = hash(i, sd + 3) * (H + 100);
      var y = ((y0 + sp * t) % (H + 100)) - 50, xx = x0 + Math.sin(t * 0.8 + i) * 30, r = 3 + 5 * hash(i, 4);
      x.globalAlpha = (o.a || 1) * (0.35 + 0.4 * hash(i, 5));
      x.beginPath(); x.ellipse(xx, y, r, r * 0.6, t + i, 0, 7); x.fillStyle = cols[i % 4]; x.fill();
    }
    x.globalAlpha = 1;
  };
  IG.grain = function (x, t, amt, seed) {
    var f = Math.floor(t * 13), W = x.W, H = x.H; amt = (amt || 0.06) * 0.6;
    for (var i = 0; i < 700; i++) {
      x.fillStyle = hash(i + f * 977, seed || 5) > 0.5 ? "rgba(255,248,240," + (amt * hash(i, f + 3)).toFixed(3) + ")" : "rgba(91,74,120," + (amt * 0.8).toFixed(3) + ")";
      x.fillRect(hash(i, f * 3 + 1) * W, hash(i, f * 3 + 2) * H, 2, 2);
    }
  };
  /* speed lines -> soft sparkle rays */
  IG.speed = function (x, t, o) {
    o = o || {}; var cx = o.cx == null ? x.W / 2 : o.cx, cy = o.cy == null ? x.H / 2 : o.cy, N = Math.round((o.n || 90) * 0.3), f = Math.floor(t * 13);
    x.lineCap = "round";
    for (var i = 0; i < N; i++) {
      var a = hash(i, (o.seed || 1)) * Math.PI * 2 + f * 0.01, r0 = (o.r0 || 380) + 60 * hash(i, 7), len = 80 + 160 * hash(i, 9);
      x.strokeStyle = "rgba(205,187,245,.45)"; x.lineWidth = 2 + 3 * hash(i, 11);
      x.beginPath(); x.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); x.lineTo(cx + Math.cos(a) * (r0 + len), cy + Math.sin(a) * (r0 + len)); x.stroke();
    }
  };
  /* brackets -> soft rounded corner arcs */
  var br0 = IG.brackets;
  IG.brackets = function (parent, inset, size, col, w) {
    var bs = br0(parent, inset, size, col || "#93B2E6", Math.min(w || 3, 4));
    var rad = ["borderTopLeftRadius", "borderTopRightRadius", "borderBottomLeftRadius", "borderBottomRightRadius"];
    bs.forEach(function (b, i) { b.style[rad[i]] = (size || 56) * 0.6 + "px"; });
    return bs;
  };
  /* brand ring: pastel tube with a thin ink edge */
  var ring0 = IG.ring;
  IG.ring = function (parent, r, sw, col, c) {
    var g = ring0(parent, r, sw, col, c);
    var stroke = mapColors(col || "#FF3D8D", "fill");
    g.circle.setAttribute("stroke", stroke);
    g.circle.style.filter = "drop-shadow(0 0 1.5px #5B4A78) drop-shadow(0 0 " + Math.max(6, sw) + "px rgba(205,187,245,.8))";
    return g;
  };
  /* text helpers keep sentence case even when set later */
  var count0 = IG.count; IG.count = count0;
})(window);
