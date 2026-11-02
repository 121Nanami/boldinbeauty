/* game.js: the in-game export of the bb-Showreel motion pack (2026-10-09).
   The pack's pieces carry sample values ("800 / 4,800", "21 HITS", "FLOOR 42", fake mail and quests). In game mode those texts
   are drawn transparent, so the game can draw the real value in the same place (the slots in Tools/motion_export.py).
   Rules match an element by its text (a regex tested on every rendered frame) or by its inline position (left/top). */
(function (root) {
  root.BIBGAME = true;
  var RULES = root.GAME_RULES || {};
  function own(e) {
    var s = "";
    for (var i = 0; i < e.childNodes.length; i++) if (e.childNodes[i].nodeType === 3) s += e.childNodes[i].nodeValue;
    return s.trim();
  }
  function hit(e, r) {
    /* BB Dream writes sample text in sentence case and "x10" as "×10": match without case, × as x */
    if (r.text && new RegExp(r.text, "i").test(own(e).replace(/×/g, "x"))) return true;
    if (r.left !== undefined && e.style.left === r.left && (r.top === undefined || e.style.top === r.top)) return true;
    if (r.sel && e.matches && e.matches(r.sel)) return true;
    return false;
  }
  function hide(e) {
    e.style.color = "transparent"; e.style.textShadow = "none"; e.style.webkitTextStroke = "0"; e.style.borderColor = e.style.borderColor;
    e.setAttribute("data-bib-slot", "1");
  }
  root.GAME = {
    strip: function (id, L) {
      var rules = RULES[id] || [];
      if (!rules.length) return;
      function pass() {
        var all = L.querySelectorAll("*");
        for (var i = 0; i < all.length; i++) {
          var e = all[i];
          if (e.getAttribute("data-bib-slot")) continue;
          for (var k = 0; k < rules.length; k++) {
            var r = rules[k];
            if (hit(e, r)) { if (r.remove) e.style.visibility = "hidden"; else hide(e); break; }
          }
        }
      }
      pass();
      // texts that are filled in later (counters, scrambles) are caught on every seek: a clock tween on the piece's own timeline
      // (overriding the timeline's onUpdate would stop the render from advancing it)
      var tl = root.__timelines && root.__timelines.main;
      if (tl && root.IG && IG.clock) IG.clock(tl, root.DUR || 5, function () { pass(); }, 0);
    }
  };
})(window);
