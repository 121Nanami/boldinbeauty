#!/usr/bin/env python3
"""Generate src/ui-<id>.html (transparent 5 s piece) and src/sheet-<NN>.html (showcase) from the UIP defs in shared/uip*.js."""
import os, re, json
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
defs = []
for f in sorted(os.listdir(os.path.join(ROOT, 'shared'))):
    if f.startswith('uip') and f.endswith('.js'):
        for m in re.finditer(r'def\("([\w-]+)", "(\d\d)", (\d+), "([^"]+)"', open(os.path.join(ROOT, 'shared', f)).read()):
            defs.append((m.group(2), int(m.group(3)), m.group(1), m.group(4)))
defs.sort()
for sh, n, pid, title in defs:
    open(os.path.join(ROOT, 'src', 'ui-%s.html' % pid), 'w').write(
        '<!--META {"dur":5,"alpha":true,"kind":"ui","sheet":"%s","n":%d,"title":%s} -->\n<div id="L" class="fill"></div>\n<script>UIP["%s"].fn(tl, document.getElementById("L"));</script>\n' % (sh, n, json.dumps(title), pid))
HOLD = {}
for sh, n, pid, title in defs:
    open(os.path.join(ROOT, 'src', 'ui-%s-linger.html' % pid), 'w').write(
        '<!--META {"dur":%s,"alpha":true,"kind":"linger","sheet":"%s","n":%d,"title":%s} -->\n<div id="L" class="fill"></div>\n<script>(function(){var L=document.getElementById("L"),s=IG.sub();UIP["%s"].fn(s,L);LINGER(tl,L,s,{id:"%s",hold:%s});})();</script>\n' % (round(HOLD.get(pid, 3.8) + 16.2, 2), sh, n, json.dumps(title + " (linger)"), pid, pid, HOLD.get(pid, 3.8)))
sheets = sorted(set(d[0] for d in defs))
TITLE, PIECE = 2.0, 5.0
for sh in sheets:
    items = [d for d in defs if d[0] == sh]
    dur = TITLE + PIECE * len(items)
    ids = json.dumps([d[2] for d in items])
    open(os.path.join(ROOT, 'src', 'sheet-%s.html' % sh), 'w').write('''<!--META {"dur":%s,"kind":"sheet","sheet":"%s"} -->
<div id="bgL" class="fill"></div>
<div id="pieces" class="fill"></div>
<div id="title" class="fill"></div>
<div id="lab" class="abs mono" style="left:60px;top:40px;font-size:24px;color:#F7F4FA;opacity:.8"></div>
<div id="num" class="abs mono" style="right:60px;top:40px;font-size:24px;color:#FF3D8D;font-weight:700"></div>
<script>
(function () {
  var ids = %s, SH = "%s", T0 = %s, P = %s;
  UI.bg(document.getElementById("bgL"), 50, 55);
  var ti = document.getElementById("title");
  IG.el("div", "", ti, null, { position: "absolute", inset: 0, background: "#090B12" });
  var k = IG.el("div", "mono", ti, "BOLDINBEAUTY // MOTION UI PACK // SHEET " + SH, { position: "absolute", left: "160px", top: "390px", fontSize: "30px", color: C.cy });
  var t = IG.el("div", "voice", ti, "<span class='bib-skew'>" + UIP.SHEETS[SH] + "</span>", { position: "absolute", left: "150px", top: "440px", fontSize: "220px", color: C.wh });
  var n = IG.el("div", "mono", ti, ids.length + " ANIMATED PIECES // 5 S EACH // 4K // TRANSPARENT MASTERS INCLUDED", { position: "absolute", left: "160px", top: "680px", fontSize: "26px", color: C.rose });
  IG.rise(tl, k, 0.1); IG.slam(tl, t, 0.2); IG.rise(tl, n, 0.5);
  tl.to(ti, { opacity: 0, duration: 0.2 }, T0 - 0.2);
  var LK = [[0, ""]], NK = [[0, ""]];
  ids.forEach(function (id, i) {
    var L = IG.el("div", "", "#pieces", null, { position: "absolute", inset: 0 });
    var at = T0 + i * P;
    IG.span(tl, L, at, at + P);
    var s = IG.sub(); UIP[id].fn(s, L); tl.add(s, at);
    LK.push([at, "SHEET " + SH + " // " + UIP[id].title]); NK.push([at, SH + "." + (i + 1 < 10 ? "0" : "") + (i + 1)]);
  });
  IG.textAt(tl, "#lab", LK, DUR); IG.textAt(tl, "#num", NK, DUR);
})();
</script>
''' % (dur, sh, ids, sh, TITLE, PIECE))
print(len(defs), 'pieces,', len(sheets), 'sheets')
