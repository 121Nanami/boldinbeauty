#!/usr/bin/env python3
"""Package rendered BB Dream masters into delivery folders, with one PNG contact sheet per UI sheet (frames over the milk ground)."""
import json, os, re, shutil, subprocess, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'out')
SHEETS = {'01': 'Navigation_Core', '02': 'Economy_Rewards', '03': 'Summon', '04': 'Combat_HUD', '05': 'Progression',
          '06': 'Arcade', '07': 'Mobile_Controls', '08': 'Missing_Screens'}
def meta(pid):
    m = re.search(r'<!--META\s*(\{.*?\})\s*-->', open(os.path.join(ROOT, 'src', pid + '.html')).read(), re.S)
    return json.loads(m.group(1))
def ff(*a): subprocess.run(['ffmpeg', '-loglevel', 'error', '-y'] + list(a), check=True)

ui = os.path.join(ROOT, 'UI_Pack_Dream_4K'); tr = os.path.join(ROOT, 'Page_Transitions_Dream_4K'); sc = os.path.join(ROOT, 'Showcase_Sheets_4K')
for d in (ui, tr, sc): os.makedirs(d, exist_ok=True)
missing = []
pieces = sorted(f[:-5] for f in os.listdir(os.path.join(ROOT, 'src')) if f.startswith('ui-') and not f.endswith('-linger.html'))
by_sheet = {}
for pid in pieces:
    m = meta(pid); sh = m['sheet']; by_sheet.setdefault(sh, []).append((m['n'], pid))
    dst = os.path.join(ui, 'Sheet_%s_%s' % (sh, SHEETS[sh])); os.makedirs(dst, exist_ok=True)
    for name in (pid, pid + '-linger'):
        src = os.path.join(OUT, 'alpha', name + '.mov')
        if os.path.exists(src): shutil.move(src, os.path.join(dst, '%s_%02d_%s.mov' % (sh, m['n'], name[3:])))
        else: missing.append(name)
for i in range(1, 6):
    src = os.path.join(OUT, 'alpha', 'tr%d.mov' % i)
    if os.path.exists(src): shutil.move(src, os.path.join(tr, 'Transition_%02d.mov' % i))
    else: missing.append('tr%d' % i)
for name in ['sheet-0%d' % i for i in range(1, 9)] + ['tr-show']:
    src = os.path.join(OUT, 'silent', name + '.mp4')
    if os.path.exists(src): shutil.move(src, os.path.join(sc, name + '.mp4'))
    else: missing.append(name)
# contact sheets: each piece at 3.4 s, composited over milk, tiled 4 across
for sh, items in sorted(by_sheet.items()):
    items.sort(); tmp = tempfile.mkdtemp(dir=OUT); k = 0
    for n, pid in items:
        src = os.path.join(ui, 'Sheet_%s_%s' % (sh, SHEETS[sh]), '%s_%02d_%s.mov' % (sh, n, pid[3:]))
        if not os.path.exists(src): continue
        k += 1
        raw = os.path.join(tmp, 'raw.png')
        ff('-ss', '3.4', '-i', src, '-frames:v', '1', '-vf', 'scale=960:540', raw)
        ff('-f', 'lavfi', '-i', 'color=c=0xFBF6FF:s=960x540:d=1', '-i', raw, '-filter_complex', '[0:v][1:v]overlay=format=auto,format=rgb24', '-frames:v', '1', os.path.join(tmp, '%02d.png' % k))
    if k:
        rows = (k + 3) // 4
        os.remove(os.path.join(tmp, 'raw.png')) if os.path.exists(os.path.join(tmp, 'raw.png')) else None
        ff('-framerate', '1', '-i', os.path.join(tmp, '%02d.png'), '-vf', 'tile=4x%d:padding=8:color=0xE9E0FB' % rows, '-frames:v', '1',
           os.path.join(ui, 'Sheet_%s_%s' % (sh, SHEETS[sh]), 'Sheet_%s_%s_contact.png' % (sh, SHEETS[sh])))
    shutil.rmtree(tmp)
print('missing:', missing if missing else 'none')
