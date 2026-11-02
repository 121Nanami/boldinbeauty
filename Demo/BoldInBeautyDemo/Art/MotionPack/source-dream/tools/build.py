#!/usr/bin/env python3
"""Wrap src/<id>.html (META + style + markup + script) into a renderable HyperFrames project in projects/<id>/."""
import json, os, re, shutil, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TPL = """<!doctype html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=__W__, height=__H__" />
<title>BB Dream __ID__</title>
<script src="assets/gsap.min.js"></script>
<script src="assets/ig.js"></script>
<script src="assets/dream.js"></script>
<script src="assets/trans.js"></script>
<script src="assets/dtrans.js"></script>
<script src="assets/ui.js"></script>
__UIP__
<link rel="stylesheet" href="assets/bib.css" />
<style>
  /* Impact and Comic Sans are system fonts. Their font files are not redistributed in this public snapshot. */
</style>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  html, body { width: __W__px; height: __H__px; overflow: hidden; background: __BG__; }
  #root { position: relative; width: __W__px; height: __H__px; overflow: hidden; background: __BG__; }
  .abs { position: absolute; }
  #root > .fill, #root > div:not(.fill) { filter: drop-shadow(0 0 12px rgba(205,187,245,.55)); }
  .fill { position: absolute; left: 0; top: 0; width: __W__px; height: __H__px; }
</style>
__STYLE__
</head>
<body>
<div id="root" data-composition-id="main" data-start="0" data-width="__W__" data-height="__H__" data-duration="__DUR__" data-fps="13">
__BODY__
</div>
<script>
  window.__timelines = window.__timelines || {};
  var DUR = __DUR__, W = __W__, H = __H__, C = IG.C;
  var tl = gsap.timeline({ paused: true });
  window.__timelines["main"] = tl;
</script>
__SCRIPT__
<script>IG.seal(tl, DUR);</script>
</body>
</html>
"""
def build(pid):
    src = open(os.path.join(ROOT, 'src', pid + '.html')).read()
    m = re.search(r'<!--META\s*(\{.*?\})\s*-->', src, re.S)
    meta = json.loads(m.group(1)); src = src[m.end():]
    styles = ''.join(re.findall(r'<style>.*?</style>', src, re.S))
    scripts = ''.join(re.findall(r'<script>.*?</script>', src, re.S))
    body = re.sub(r'<style>.*?</style>|<script>.*?</script>', '', src, flags=re.S).strip()
    W, H = meta.get('w', 1920), meta.get('h', 1080)
    bg = ('transparent' if meta.get('alpha') else meta.get('bg', '#FBF6FF')) if not os.environ.get('PREVIEW_BG') else os.environ['PREVIEW_BG']
    uips = sorted(f for f in os.listdir(os.path.join(ROOT, 'shared')) if f.startswith('uip') and f.endswith('.js')) + ['linger.js']
    html = (TPL.replace('__UIP__', ''.join('<script src="assets/%s"></script>\n' % f for f in uips)).replace('__W__', str(W)).replace('__H__', str(H)).replace('__ID__', pid).replace('__BG__', bg)
            .replace('__DUR__', str(meta['dur'])).replace('__STYLE__', styles).replace('__BODY__', body).replace('__SCRIPT__', scripts))
    d = os.path.join(ROOT, 'projects', pid); os.makedirs(os.path.join(d, 'assets'), exist_ok=True)
    for f in ['gsap.min.js', 'ig.js', 'dream.js', 'linger.js', 'bib.css', 'trans.js', 'dtrans.js', 'ui.js'] + sorted(f for f in os.listdir(os.path.join(ROOT, 'shared')) if f.startswith('uip') and f.endswith('.js')): shutil.copy(os.path.join(ROOT, 'shared', f), os.path.join(d, 'assets', f))
    shutil.copytree(os.path.join(ROOT, 'shared', 'fonts'), os.path.join(d, 'assets', 'fonts'), dirs_exist_ok=True)
    open(os.path.join(d, 'index.html'), 'w').write(html)
    open(os.path.join(d, 'package.json'), 'w').write('{"name":"%s","private":true,"type":"module"}' % pid)
    open(os.path.join(d, 'meta.json'), 'w').write(json.dumps({'id': pid, 'name': pid}))
    open(os.path.join(d, 'hyperframes.json'), 'w').write(json.dumps({"$schema": "https://hyperframes.heygen.com/schema/hyperframes.json",
        "registry": "https://raw.githubusercontent.com/heygen-com/hyperframes/main/registry",
        "paths": {"blocks": "compositions", "components": "compositions/components", "assets": "assets"}, "media": {"autoProxy": True}}, indent=1))
    json.dump(meta, open(os.path.join(d, 'meta-src.json'), 'w'))
    return meta
if __name__ == '__main__':
    ids = sys.argv[1:] or [f[:-5] for f in sorted(os.listdir(os.path.join(ROOT, 'src'))) if f.endswith('.html')]
    for i in ids: print(i, build(i).get('dur'))
