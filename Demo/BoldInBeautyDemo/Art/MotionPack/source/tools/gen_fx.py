#!/usr/bin/env python3
"""Generate src/fx<SS>-<NN>-<slug>.html (transparent 4K masters) and src/fxsheet-<SS>.html (labelled sheet videos) from tools/fx_defs.py."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fx_defs as D
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYS = ("col", "col2", "sh", "pulse", "rcol")
FONTS = '<div style="position:absolute;left:0;top:0;opacity:0.01;font-size:24px"><span style="font-family:\'League Gothic\'">A</span><span style="font-family:\'Archivo Black\'">A</span><span style="font-family:\'JetBrains Mono\'">A</span></div>'
def res(v, pal):
    if isinstance(v, str) and not v.startswith("#"):
        if v in pal: return pal[v]
        if v in D.TOK: return D.TOK[v]
    return v
def spec(p):
    pal = D.PAL.get(p["who"], {}) if p["who"] else {}
    layers = []
    for L in p["layers"]:
        L = dict(L)
        for k in KEYS:
            if k in L: L[k] = res(L[k], pal)
        if "cols" in L: L["cols"] = [res(c, pal) for c in L["cols"]]
        layers.append(L)
    return dict(id=p["id"], title=p["title"], dur=p["dur"], layers=layers, enemy=p["enemy"], cas=p["cas"], note=p["note"], label=p["label"], who=D.NAME.get(p["who"], "SHARED") if p["who"] else "SHARED", slot=p["slot"], hit=p["hit"], sfx=p["sfx"], n=p["n"], sheet=p["sheet"])
def piece_html(p):
    s = spec(p)
    meta = {"dur": p["dur"], "alpha": True, "kind": "fx", "sheet": p["sheet"], "n": p["n"], "title": p["title"], "slug": p["slug"], "who": p["who"], "label": p["label"], "note": p["note"]}
    return "<!--META %s -->\n%s\n<script>\n(function () {\n  var SPEC = %s;\n  var x = IG.canvas('fxc');\n  IG.clock(tl, DUR, function (t) { x.clearRect(0, 0, 1920, 1080); FX.drawPiece(x, SPEC, t); });\n})();\n</script>\n" % (json.dumps(meta), FONTS, json.dumps(s))
GAP, T0 = 0.5, 2.4
def sheet_html(sh, items):
    title, sub = D.SHEETS[sh]; specs = [spec(p) for p in items]
    starts, t = [], T0
    for s in specs: starts.append(round(t, 3)); t += s["dur"] + GAP
    dur = round(t + 0.3, 2)
    hits = [[starts[i], specs[i]["hit"], specs[i]["sfx"]] for i in range(len(specs))]
    meta = {"dur": dur, "kind": "fxsheet", "sheet": sh, "hits": hits, "count": len(specs), "title": title}
    return dur, "<!--META %s -->\n%s\n<div id=\"bgL\" class=\"fill\"></div>\n<div id=\"title\" class=\"fill\"></div>\n<div id=\"hd\" class=\"abs voice\" style=\"left:70px;top:44px;font-size:74px;color:#F7F4FA\"></div>\n<div id=\"sub\" class=\"abs mono\" style=\"left:72px;top:128px;font-size:26px;color:#64E8FF\"></div>\n<div id=\"badge\" class=\"abs mono\" style=\"right:70px;top:56px;font-size:28px;color:#FF3D8D;font-weight:700\"></div>\n<div id=\"num\" class=\"abs mono\" style=\"right:70px;top:100px;font-size:22px;color:#F7F4FA;opacity:.7\"></div>\n<div id=\"cap\" class=\"abs mono\" style=\"left:72px;bottom:56px;font-size:26px;color:#F7F4FA\"></div>\n<div id=\"pal\" class=\"abs mono\" style=\"right:70px;top:132px;font-size:20px;color:#F7F4FA;opacity:.7\"></div>\n<script>\n(function () {\n  var SPECS = %s, ST = %s, T0 = %s, SH = %s, TITLE = %s, SUB = %s, N = SPECS.length;\n  UI.bg(document.getElementById('bgL'), 50, 55);\n  var ti = document.getElementById('title');\n  IG.el('div', '', ti, null, { position: 'absolute', inset: 0, background: '#090B12' });\n  var k = IG.el('div', 'mono', ti, 'BOLDINBEAUTY // MOTION PACK SET 2 // SHEET ' + SH, { position: 'absolute', left: '160px', top: '380px', fontSize: '30px', color: C.cy });\n  var t = IG.el('div', 'voice', ti, \"<span class='bib-skew'>\" + TITLE + '</span>', { position: 'absolute', left: '150px', top: '430px', fontSize: (TITLE.length > 22 ? 110 : 190) + 'px', color: C.wh, whiteSpace: 'normal', width: '1700px', lineHeight: '0.95' });\n  var n = IG.el('div', 'mono', ti, SUB + ' // ' + N + ' PIECES // TRANSPARENT 4K MASTERS INCLUDED', { position: 'absolute', left: '160px', top: '800px', fontSize: '24px', color: C.ro });\n  IG.rise(tl, k, 0.1); IG.slam(tl, t, 0.2); IG.rise(tl, n, 0.5); tl.to(ti, { opacity: 0, duration: 0.2 }, T0 - 0.25);\n  var HK = [[0, '']], SK = [[0, '']], BK = [[0, '']], NK = [[0, '']], CK = [[0, '']], PK = [[0, '']];\n  SPECS.forEach(function (s, i) { var a = ST[i]; HK.push([a, s.title.toUpperCase()]); SK.push([a, s.who.toUpperCase() + ' // ' + s.slot.toUpperCase()]); BK.push([a, s.label]); NK.push([a, SH + '.' + (i + 1 < 10 ? '0' : '') + (i + 1) + ' / ' + N]); CK.push([a, s.note.toUpperCase()]); PK.push([a, s.sheet >= '12' ? (s.label === 'SOURCE' ? 'OWNER-DESCRIBED LOOK' : 'LOOK DERIVED FROM ABILITY TEXT // PALETTE ' + (s.label === 'PROPOSED' || s.who !== 'SHARED' ? 'PLACEHOLDER' : 'OPEN')) : '']); });\n  IG.textAt(tl, '#hd', HK, DUR); IG.textAt(tl, '#sub', SK, DUR); IG.textAt(tl, '#badge', BK, DUR); IG.textAt(tl, '#num', NK, DUR); IG.textAt(tl, '#cap', CK, DUR); IG.textAt(tl, '#pal', PK, DUR);\n  var x = IG.canvas('fxs');\n  IG.clock(tl, DUR, function (t) {\n    x.clearRect(0, 0, 1920, 1080);\n    for (var i = 0; i < N; i++) { var lt = t - ST[i]; if (lt >= 0 && lt < SPECS[i].dur + 0.5) { FX.dress(x, SPECS[i], lt); FX.drawPiece(x, SPECS[i], lt); break; } }\n  });\n})();\n</script>\n" % (json.dumps(meta), FONTS, json.dumps(specs), json.dumps(starts), T0, json.dumps(sh), json.dumps(title), json.dumps(sub))
if __name__ == "__main__":
    allp = D.PIECES
    for p in allp: open(os.path.join(ROOT, "src", p["id"] + ".html"), "w").write(piece_html(p))
    sheets = sorted(set(p["sheet"] for p in allp)); tot = 0
    for sh in sheets:
        dur, h = sheet_html(sh, [p for p in allp if p["sheet"] == sh]); open(os.path.join(ROOT, "src", "fxsheet-%s.html" % sh), "w").write(h); tot += dur; print("sheet", sh, dur, "s")
    open(os.path.join(ROOT, "out", "fx_ids.txt"), "w").write(" ".join(p["id"] for p in allp)); open(os.path.join(ROOT, "out", "fxsheet_ids.txt"), "w").write(" ".join("fxsheet-%s" % s for s in sheets))
    print(len(allp), "pieces,", round(tot / 60, 1), "min of sheets")
