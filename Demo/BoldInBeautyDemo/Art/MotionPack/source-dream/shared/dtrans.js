/* dtrans.js: the five BB Dream page transitions. Replaces TR.t1-t5 from trans.js with the same contract:
   TR.tN(tl, layer, at) covers the whole screen at at + TR.MID[N] (swap pages there) and is clear at at + TR.DUR[N].
   Pastel cel bands, thin plum ink, round shapes; no tech words. Colours are hand-set (IG.rawEl skips the neon mapping). */
(function (root) {
  var TR = root.TR, RE = IG.rawEl, INK = "#5B4A78";
  TR.NAME = ["", "Milk Veil", "Ribbons", "Cloud Bloom", "Sparkle Iris", "Full Dream"];
  var BAND = {
    blush: ["#FFEAF2", "#FFD9E8", "#F3BCD3"], baby: ["#EAF3FF", "#CFE3FF", "#B5CCEE"], lilac: ["#E9E0FB", "#CDBBF5", "#B6A2E6"],
    mint: ["#EAF8EF", "#D5F2DF", "#B8DEC6"], cream: ["#FFFFFF", "#FFF8F0", "#F1E6EC"]
  };
  var ORDER = ["blush", "baby", "lilac", "mint", "cream"];
  function cel(name, dir) { var b = BAND[name]; return "linear-gradient(" + (dir || "to bottom") + "," + b[0] + " 0 22%," + b[1] + " 22% 74%," + b[2] + " 74%)"; }
  function box(p, css) { return RE("div", "", p, null, Object.assign({ position: "absolute" }, css)); }
  var P4 = "M0-11C1.4-3 3-1.4 11 0 3 1.4 1.4 3 0 11-1.4 3-3 1.4-11 0-3-1.4-1.4-3 0-11Z";
  function sparkle(p, size, fill, css) {
    return RE("div", "", p, '<svg viewBox="-12 -12 24 24" width="' + size + '" height="' + size + '" style="overflow:visible"><path d="' + P4 + '" fill="' + fill +
      '" stroke="' + INK + '" stroke-width="1.2" vector-effect="non-scaling-stroke"/></svg>', Object.assign({ position: "absolute", width: size + "px", height: size + "px" }, css || {}));
  }
  function motes(p, n, seed) {
    var cols = ["#FFD9E8", "#CFE3FF", "#CDBBF5", "#D5F2DF", "#FFF8F0"];
    var a = [];
    for (var i = 0; i < n; i++) a.push(box(p, { left: (IG.hash(i, seed) * 1920) + "px", top: (IG.hash(i, seed + 1) * 1080) + "px", width: "14px", height: "14px",
      borderRadius: "50%", background: cols[i % 5], filter: "blur(1.5px)", opacity: 0 }));
    return a;
  }

  /* T1 MILK VEIL: a round-edged cream veil drifts across; a little pill fills while the page swaps. */
  TR.t1 = function (tl, L, at) {
    var v = box(L, { left: "-260px", top: "-60px", width: "2440px", height: "1200px", background: cel("cream", "to right"), borderRadius: "260px", border: "4px solid " + INK });
    var pill = box(v, { left: "1110px", top: "620px", width: "220px", height: "26px", borderRadius: "99px", background: "#FFFFFF", border: "3px solid " + INK, overflow: "hidden" });
    var fill = box(pill, { left: 0, top: 0, width: "0px", height: "26px", background: cel("blush") });
    var s1 = sparkle(v, 54, "#FFE6EF", { left: "1360px", top: "560px" });
    gsap.set(v, { xPercent: -102 });
    tl.fromTo(v, { xPercent: -102 }, { xPercent: 0, duration: 0.4, ease: "power2.inOut", immediateRender: false }, at);
    tl.fromTo(fill, { width: 0 }, { width: 220, duration: 0.35, ease: "power1.inOut", immediateRender: false }, at + 0.15);
    tl.fromTo(s1, { scale: 0, rotation: -40 }, { scale: 1, rotation: 0, duration: 0.25, ease: "back.out(1.5)", immediateRender: false }, at + 0.25);
    tl.to(v, { xPercent: 102, duration: 0.4, ease: "power2.inOut" }, at + 0.4);
  };

  /* T2 RIBBONS: six cel-banded pastel ribbons glide in from alternate sides, then glide on. */
  TR.t2 = function (tl, L, at) {
    var wrap = box(L, { left: "-240px", top: "-210px", width: "2400px", height: "1500px", transform: "rotate(-3deg)" });
    for (var i = 0; i < 6; i++) {
      var r = box(wrap, { left: 0, top: (i * 250) + "px", width: "2400px", height: "252px", borderRadius: "999px", background: cel(ORDER[i % 5]), border: "4px solid " + INK });
      var from = i % 2 ? 112 : -112;
      gsap.set(r, { xPercent: from });
      tl.fromTo(r, { xPercent: from }, { xPercent: 0, duration: 0.3, ease: "power2.out", immediateRender: false }, at + i * 0.025);
      tl.to(r, { xPercent: -from, duration: 0.4, ease: "power2.in" }, at + 0.52 + i * 0.015);
    }
  };

  /* T3 CLOUD BLOOM: round pastel clouds bloom from twelve points until they meet, then fold back in. */
  TR.t3 = function (tl, L, at) {
    var k = 0, cs = [];
    for (var y = 0; y <= 2; y++) for (var x = 0; x <= 3; x++) {
      var c = box(L, { left: (x * 640 - 540) + "px", top: (y * 540 - 540) + "px", width: "1080px", height: "1080px", borderRadius: "50%",
        background: "radial-gradient(circle at 38% 32%," + BAND[ORDER[k % 5]][0] + " 0 20%," + BAND[ORDER[k % 5]][1] + " 22% 70%," + BAND[ORDER[k % 5]][2] + " 72%)", border: "4px solid " + INK });
      cs.push(c); k++;
    }
    var core = box(L, { left: "660px", top: "240px", width: "600px", height: "600px", borderRadius: "50%", background: cel("cream"), border: "4px solid " + INK });
    cs.push(core);
    var sp = sparkle(L, 120, "#FFE6EF", { left: "900px", top: "480px" });
    cs.forEach(function (c, i) {
      var o = IG.hash(i, 31) * 0.2;
      gsap.set(c, { scale: 0 });
      tl.fromTo(c, { scale: 0 }, { scale: 1, duration: 0.36, ease: "back.out(1.3)", immediateRender: false }, at + o);
      tl.to(c, { scale: 0, duration: 0.3, ease: "power2.in" }, at + 0.7 + (0.2 - o));
    });
    tl.fromTo(sp, { scale: 0, rotation: -60, opacity: 1 }, { scale: 1, rotation: 0, duration: 0.3, ease: "back.out(1.5)", immediateRender: false }, at + 0.42);
    tl.to(sp, { scale: 0, rotation: 40, duration: 0.2, ease: "power2.in" }, at + 0.72);
  };

  /* T4 SPARKLE IRIS: a blush sparkle grows past the edges of the screen, holds, then dissolves into motes. */
  TR.t4 = function (tl, L, at) {
    var b = sparkle(L, 200, "#CDBBF5", { left: "860px", top: "440px" });
    var a = sparkle(L, 200, "#FFD9E8", { left: "860px", top: "440px" });
    var core = box(L, { left: "0", top: "0", width: "1920px", height: "1080px", background: cel("blush"), opacity: 0 });
    var ring = box(L, { left: "880px", top: "460px", width: "160px", height: "160px", borderRadius: "50%", border: "16px solid #F3A6C8", boxShadow: "0 0 0 3px " + INK + ", inset 0 0 0 3px " + INK + ", 0 0 30px rgba(205,187,245,.9)", opacity: 0 });
    var m = motes(L, 30, 77);
    gsap.set([a, b], { scale: 0 });
    tl.fromTo(b, { scale: 0, rotation: 45 }, { scale: 26, rotation: 0, duration: 0.5, ease: "power3.in", immediateRender: false }, at);
    tl.fromTo(a, { scale: 0, rotation: -20 }, { scale: 48, rotation: 0, duration: 0.55, ease: "power3.in", immediateRender: false }, at);
    tl.set(core, { opacity: 1 }, at + 0.55);
    tl.fromTo(ring, { scale: 0.4, opacity: 0 }, { scale: 1, opacity: 1, duration: 0.25, ease: "back.out(1.5)", immediateRender: false }, at + 0.45);
    tl.to([a, b], { opacity: 0, duration: 0.01 }, at + 0.56);
    tl.to(ring, { scale: 1.6, opacity: 0, duration: 0.3, ease: "power2.in" }, at + 0.8);
    tl.to(core, { opacity: 0, filter: "blur(18px)", duration: 0.55, ease: "power2.in" }, at + 0.78);
    m.forEach(function (e, i) {
      tl.fromTo(e, { opacity: 0, y: 0 }, { opacity: 0.9, y: -40, duration: 0.25, immediateRender: false }, at + 0.7 + IG.hash(i, 5) * 0.2);
      tl.to(e, { opacity: 0, y: -120, duration: 0.3 }, at + 1.0 + IG.hash(i, 6) * 0.05);
    });
  };

  /* T5 FULL DREAM: clouds bloom, the brand ring and wordmark settle in the middle, then the whole sky lifts away. */
  TR.t5 = function (tl, L, at) {
    var sky = box(L, { left: 0, top: 0, width: "1920px", height: "1080px" });
    var k = 0, cs = [];
    for (var y = 0; y <= 3; y++) for (var x = 0; x <= 4; x++) {
      var c = box(sky, { left: (x * 480 - 380) + "px", top: (y * 380 - 380) + "px", width: "760px", height: "760px", borderRadius: "50%",
        background: "radial-gradient(circle at 38% 32%," + BAND[ORDER[k % 5]][0] + " 0 20%," + BAND[ORDER[k % 5]][1] + " 22% 70%," + BAND[ORDER[k % 5]][2] + " 72%)", border: "4px solid " + INK });
      cs.push(c); k++;
    }
    var veil = box(sky, { left: 0, top: 0, width: "1920px", height: "1080px", background: "radial-gradient(ellipse at 50% 48%, #FFF8F0 0 30%, rgba(255,248,240,0) 62%)", opacity: 0 });
    var ring = box(sky, { left: "820px", top: "290px", width: "280px", height: "280px", borderRadius: "50%", border: "26px solid #F3A6C8",
      boxShadow: "0 0 0 4px " + INK + ", inset 0 0 0 4px " + INK + ", 0 0 40px rgba(205,187,245,.9)", opacity: 0 });
    var word = RE("div", "voice", sky, "BoldInBeauty", { position: "absolute", left: "0", width: "1920px", textAlign: "center", top: "620px", fontSize: "118px", color: INK,
      textShadow: "0 0 3px #FFF8F0, 0 0 3px #FFF8F0, 0 0 30px rgba(205,187,245,.95)", opacity: 0 });
    var sps = [sparkle(sky, 70, "#FFE6EF", { left: "760px", top: "300px" }), sparkle(sky, 46, "#E9E0FB", { left: "1120px", top: "520px" })];
    cs.forEach(function (c, i) {
      gsap.set(c, { scale: 0 });
      tl.fromTo(c, { scale: 0 }, { scale: 1, duration: 0.34, ease: "back.out(1.3)", immediateRender: false }, at + IG.hash(i, 17) * 0.3);
    });
    tl.fromTo(veil, { opacity: 0 }, { opacity: 1, duration: 0.3, immediateRender: false }, at + 0.35);
    tl.fromTo(ring, { scale: 0.5, opacity: 0, rotation: -40 }, { scale: 1, opacity: 1, rotation: 0, duration: 0.35, ease: "back.out(1.4)", immediateRender: false }, at + 0.42);
    tl.fromTo(word, { opacity: 0, y: 24 }, { opacity: 1, y: 0, duration: 0.3, ease: "power2.out", immediateRender: false }, at + 0.55);
    sps.forEach(function (s, i) { tl.fromTo(s, { scale: 0, rotation: -50 }, { scale: 1, rotation: 0, duration: 0.25, ease: "back.out(1.6)", immediateRender: false }, at + 0.6 + i * 0.08); });
    gsap.set(sky, { yPercent: 0 });
    tl.to(sky, { yPercent: -125, duration: 0.6, ease: "power3.in" }, at + 0.98);
  };
})(window);
