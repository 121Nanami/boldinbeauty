#!/usr/bin/env python3
"""Exports the bb-Showreel motion pack for the game (owner request 2026-10-09: "a file filled with stylised transitions, ui etc to
use in game ... they are all animated ... make sure to use them all").

For every piece (49 UI pieces + 5 page transitions) it:
  1. builds the piece's HyperFrames project from Art/MotionPack/source with the game rules (Art/MotionPack/game.js): sample values
     in the pack ("800 / 4,800", "21 HITS", "FLOOR 42", sample mail and quests) are drawn transparent;
  2. renders it at 1920x1080, 12 fps (the game's own anime frame rate), transparent PNG frames (npx hyperframes);
  3. crops to the area the piece ever draws, drops repeated frames and packs the rest into 2048x2048 atlas pages;
  4. writes Saved/MotionPack/<id>/page_NN.png and Content/BIBMotion/motion.json (atlas layout, timeline, clips, value slots).
Then Tools/motion_import.py (editor) imports the pages as /Game/BIBMotion/<id>/T_<id>_NN. Run both with Tools/import_motion_pack.sh.

Needs: node + npx hyperframes (the pack's toolchain), ffmpeg, Python with numpy (e.g. the motion-studio .venv).
    python3 Tools/motion_export.py [piece ids...]       (no ids = all)
"""
import importlib.util
import glob
import json
import math
import os
import re
import shutil
import subprocess
import sys

try:
    import numpy as np
except ImportError:  # pragma: no cover
    sys.exit("motion_export.py needs numpy: run it with the motion-studio .venv python, or `python3 -m pip install numpy`.")

DEMO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACK = os.path.join(DEMO, "Art", "MotionPack")
SRC = os.path.join(PACK, "source")              # the original pack: VFX (sheets 09-18) still build from here
DREAM = os.path.join(PACK, "source-dream")      # BB Dream (2026-10-16): UI pieces and page transitions build from here
WORK = os.path.join(DEMO, "Saved", "MotionWork")
OUT = os.path.join(DEMO, "Saved", "MotionPack")
MANIFEST = os.path.join(DEMO, "Content", "BIBMotion", "motion.json")
FPS = 13   # the BB stepped rate (owner, 2026-10-16)
PAGE = 2048


def R(**kw):
    return kw


# ------------------------------------------------------------------------------------------------ catalogue
# hide: game-mode rules (text regex / inline left+top / CSS selector, remove = hide the element entirely)
# clips: name -> [start, end] seconds of the piece; slots: real values the game draws (1080p rect, font, size, colour, align,
# window [t0, t1] in piece seconds); anchor: the 1080p point that follows a screen position when the game anchors the piece.
# BB Dream text colours for the values the game draws (plum on the pastel panels; the old neon set was white / rose / cyan / violet)
W = "#5B4A78"; ROSE = "#D46F9E"; CY = "#8C7BA8"; VIO = "#7D68B8"; DIM = "#A99CBE"


def slot(name, x, y, w, h, font="mono", size=26, colour=W, align="left", window=None, shadow=None):
    s = {"name": name, "rect": [x, y, w, h], "font": font, "size": size, "colour": colour, "align": align}
    if window:
        s["window"] = window
    if shadow:
        s["shadow"] = shadow
    return s


PIECES = {
    # ---- 01 navigation & core
    "nav-bar": R(clips={"open": [0.0, 1.2]}),
    "btn-primary": R(clips={"press": [2.35, 3.5]}, anchor=[960, 525]),
    "btn-secondary": R(clips={"back": [0.95, 1.85], "cancel": [1.95, 2.85], "close": [2.95, 3.85]}, anchor=[400, 505],
                       anchors={"back": [400, 505], "cancel": [790, 505], "close": [1145, 505]}),
    "settings-controls": R(hide=[R(text=r"^\d+%$")], clips={"open": [0.0, 1.0]}),
    "loading": R(hide=[R(top="432px"), R(top="740px"), R(top="800px")], clips={"loop": [0.3, 4.3]},
                 slots=[slot("label", 0, 740, 1920, 40, "mono", 30, CY, "center"), slot("tip", 0, 800, 1920, 32, "mono", 24, DIM, "center")]),
    "modal": R(hide=[R(text=r"^USE "), R(text=r"^OWNED")], clips={"confirm": [0.0, 4.1]},
               slots=[slot("question", 550, 380, 840, 96, "display", 90, W, "left", [0.4, 3.8]),
                      slot("owned", 670, 530, 700, 36, "mono", 30, CY, "left", [0.4, 3.8])]),
    "notice-toast": R(hide=[R(text=r"^(SEASON PASS UPDATED|YOU HAVE 3 NEW MESSAGES|DAILY QUEST COMPLETE)$"), R(text=r"^(NOTICE|MAIL|QUEST)$")],
                      clips={"toasts": [0.0, 4.2]},
                      slots=[s for i, c in enumerate([ROSE, CY, VIO]) for s in (
                          slot("kind%d" % i, 1210, 238 + i * 170, 600, 28, "mono", 24, c, "left", [0.65 + i * 0.5, 3.7 + i * 0.2]),
                          slot("text%d" % i, 1210, 270 + i * 170, 620, 70, "display", 66, W, "left", [0.65 + i * 0.5, 3.7 + i * 0.2]))]),
    "page-header": R(hide=[R(left="160px", top="60px"), R(left="170px", top="340px"), R(left="1180px", top="250px")], clips={"open": [0.0, 1.6], "out": [4.3, 4.9]},
                     slots=[slot("title", 160, 360, 1500, 300, "display", 300, W, "left", [0.25, 4.5], VIO), slot("code", 1180, 550, 700, 34, "mono", 28, W, "left", [0.8, 4.5])]),
    # ---- 02 economy & rewards
    "currency-bar": R(hide=[R(text=r"^[\d,]+$"), R(text=r"^[+-][\d,]+$")], clips={"show": [0.0, 1.2], "hide": [4.4, 4.9]},
                      slots=[slot("v%d" % i, 420 + i * 290, 492, 234, 32, "mono", 26, W, "right", [0.1 + i * 0.07, 4.6]) for i in range(4)]),
    "reward-popup": R(hide=[R(text=r"^x[\d,]+$")], clips={"rewards": [0.0, 3.8]},
                      slots=[slot("r%d" % i, 430 + i * 220, 700, 190, 40, "mono", 32, W, "center", [1.1 + i * 0.15, 4.4]) for i in range(5)]),
    "shop-card": R(hide=[R(text=r"^REGULAR TICKET$"), R(text=r"^X10 BUNDLE$"), R(text=r"^[\d,]+$")], clips={"buy": [0.0, 3.4]},
                   slots=[slot("name", 720, 530, 480, 80, "display", 76, W, "center", [0.3, 4.5]), slot("sub", 720, 620, 480, 32, "mono", 26, DIM, "center", [0.3, 4.5]),
                          slot("price", 860, 734, 300, 84, "display", 84, W, "left", [0.3, 4.5])]),
    "level-up": R(hide=[R(text=r"^LV \d+$"), R(text=r"^(HP|ATK|DEF)$"), R(sel="b")], clips={"up": [0.0, 2.6]},
                  slots=[slot("level", 0, 500, 1920, 150, "numeral", 140, ROSE, "center", [0.6, 4.5], VIO), slot("stats", 560, 700, 800, 40, "mono", 34, W, "center", [1.4, 4.5])]),
    "resource-gain": R(hide=[R(text=r"^\+"), R(text=r"^[\d,]+$")], clips={"gain": [0.5, 3.4]},
                       slots=[slot("v0", 900, 72, 204, 32, "mono", 26, W, "right", [0.1, 4.6]), slot("v1", 1150, 72, 204, 32, "mono", 26, W, "right", [0.1, 4.6]),
                              slot("v2", 1400, 72, 184, 32, "mono", 26, W, "right", [0.1, 4.6])] +
                             [slot("g%d" % i, x - 150, y + 110, 300, 60, "display", 56, W, "center", [0.75 + i * 0.3, 2.0 + i * 0.25]) for i, (x, y) in enumerate([(760, 600), (960, 680), (1160, 600)])]),
    # ---- 03 summon
    "banner-select": R(clips={"open": [0.0, 1.9]}),
    "summon-charge": R(clips={"charge": [0.9, 3.7]}),
    "reveal-rare": R(clips={"reveal": [0.0, 2.6]}),
    "reveal-epic": R(clips={"reveal": [0.0, 2.6]}),
    "reveal-legendary": R(clips={"reveal": [0.0, 2.8]}),
    "reveal-secret": R(clips={"reveal": [0.0, 3.0]}),
    "summon-x10": R(clips={"cards": [0.0, 0.95]}),
    "ticket-use": R(hide=[R(text=r"^x\d+$")], clips={"use": [0.0, 2.6]}, slots=[slot("count", 0, 730, 1920, 120, "numeral", 110, W, "center", [0.3, 4.6])]),
    # ---- 04 combat HUD
    "hp-bar": R(hide=[R(text=r"^[\d,]+ / [\d,]+$"), R(text=r"^\+[\d,]+$")], clips={"low": [2.0, 3.1]}),
    "squad-switch": R(clips={"switch": [1.25, 1.95]}),
    "skill-buttons": R(clips={"intro": [0.0, 0.75]}),
    "ultimate-ready": R(clips={"ready": [2.3, 3.5]}),
    "damage-numbers": R(hide=[R(text=r"^[\d,]+$")], clips={"crit": [1.85, 2.75]}, anchor=[960, 460],
                        slots=[slot("value", 760, 380, 400, 160, "numeral", 150, ROSE, "center", [1.9, 2.75], VIO)]),
    "boss-intro": R(hide=[R(text=r"^CHAPTER \d+$"), R(text=r"^x\d+$")], clips={"intro": [0.0, 3.6]},
                    slots=[slot("chapter", 1160, 140, 400, 30, "mono", 26, CY, "right", [2.0, 4.6])]),
    "miniboss-warning": R(clips={"alert": [0.0, 2.2]}),
    "combo-counter": R(hide=[R(text=r"^\d+$")], clips={"burst": [0.1, 0.8]},
                      slots=[slot("count", 1240, 300, 560, 220, "numeral", 220, W, "left", [0.1, 4.2], ROSE)]),
    "defense-prompts": R(clips={"block": [0.15, 1.45], "parry": [1.65, 2.95], "dodge": [3.05, 4.35]}),
    "victory": R(hide=[R(text=r"^STAGE CLEAR")], clips={"win": [0.0, 3.2]}, slots=[slot("rewards", 0, 700, 1920, 36, "mono", 30, W, "center", [1.9, 4.6])]),
    "defeat": R(clips={"lose": [0.0, 3.0]}),
    "target-lock": R(hide=[R(left="1180px", top="410px")], clips={"lock": [0.1, 1.3]}, anchor=[960, 540],
                     slots=[slot("dist", 1180, 410, 500, 30, "mono", 26, W, "left", [0.7, 4.6])]),
    # ---- 05 progression
    "campaign-map": R(hide=[R(text=r"^CHAPTER \d+$"), R(text=r"^STAGE 1-1"), R(text=r"^1-\d$"), R(text=r"^CURRENT")], clips={"open": [0.0, 2.8]},
                      slots=[slot("title", 140, 120, 900, 120, "display", 120, W), slot("sub", 146, 245, 1600, 30, "mono", 26, CY),
                             slot("current", 940, 420, 700, 32, "mono", 28, W, "left", [2.3, 4.6])]),
    "season-pass": R(hide=[R(text=r"^\d+$")], clips={"open": [0.0, 2.0]}),
    "quest-complete": R(hide=[R(text=r"^(DAILY|WEEKLY):"), R(text=r"^\d+/\d+$"), R(text=r"^x\d+$")], clips={"claim": [0.0, 3.2]},
                        slots=[slot("q%d" % i, 334, 290 + i * 190, 780, 72, "display", 70, W, "left", [0.45 + i * 0.1, 4.5]) for i in range(3)]),
    "ark-floor": R(hide=[R(text=r"^\d+$"), R(text=r"^FLOOR \d+$")], clips={"floor": [0.0, 3.2]},
                   slots=[slot("floor", 1220, 380, 700, 250, "numeral", 240, W, "left", [0.2, 4.6], ROSE)]),
    "arena-rank": R(hide=[R(text=r"^(BRONZE|SILVER|GOLD|DIAMOND|PLATINUM|CHAMPIONS)$"), R(text=r"^TROPHIES")], clips={"rank": [0.0, 4.6]},
                    slots=[slot("rank", 0, 620, 1920, 150, "display", 150, W, "center", [0.3, 4.6]), slot("score", 0, 120, 1920, 34, "mono", 30, CY, "center", [0.3, 4.6])]),
    "mail-inbox": R(hide=[R(text=r"^(SYSTEM|GANG)$"), R(text=r"^(WELCOME BACK, COMMANDER|SEASON REWARDS ARE IN|MAINTENANCE COMPENSATION)$"), R(text=r"^\d$")], clips={"open": [0.0, 3.0]},
                    slots=[s for i in range(3) for s in (slot("kind%d" % i, 710, 316 + i * 150, 300, 28, "mono", 24, CY, "left", [1.9 + i * 0.15, 4.6]),
                                                         slot("subject%d" % i, 710, 346 + i * 150, 800, 66, "display", 64, W, "left", [1.9 + i * 0.15, 4.6]))] +
                          [slot("badge", 470, 382, 72, 50, "numeral", 40, W, "center", [0.6, 3.6])]),
    "battle-power": R(hide=[R(text=r"^[\d,]+$"), R(text=r"^▲")], clips={"power": [0.0, 3.2]},
                      slots=[slot("power", 590, 450, 760, 150, "numeral", 150, W, "left", [0.2, 4.6], ROSE), slot("delta", 596, 620, 760, 34, "mono", 30, CY, "left", [1.2, 4.6])]),
    # ---- 06 arcade
    "night-shift": R(clips={"intro": [0.0, 1.5]}),
    "rhythm-lanes": R(clips={"intro": [0.0, 2.0]}),
    "boss-rush": R(hide=[R(text=r"^\d$")], clips={"round": [0.0, 1.4]}, slots=[slot("round", 1160, 300, 500, 320, "numeral", 300, ROSE, "left", [0.4, 4.6], VIO)]),
    # ---- 07 mobile controls (the knob follows the real thumb in game, so the pack's own knob and readouts are hidden)
    "joystick-ring": R(hide=[R(left="875px", top="455px", remove=True), R(sel='[style*="width: 70px"]', remove=True), R(sel='path[fill="rgba(255,61,141,.25)"]', remove=True),
                             R(text=r"^(IDLE|WALK|RUN)")], clips={"intro": [0.0, 0.5], "idle": [0.5, 0.6]}, anchor=[960, 540]),
    "joystick-hex": R(hide=[R(left="880px", top="460px", remove=True), R(sel='[style*="width: 70px"]', remove=True), R(text=r"^DIR")], clips={"intro": [0.0, 0.55], "idle": [0.55, 0.65]}, anchor=[960, 540]),
    "joystick-tactical": R(hide=[R(left="900px", top="480px", remove=True), R(sel='[style*="width: 70px"]', remove=True), R(sel="polyline", remove=True), R(sel='line[stroke-width="3"]', remove=True),
                                 R(text=r"^(ANG|MAG)")], clips={"intro": [0.0, 0.55], "idle": [0.55, 0.65]}, anchor=[960, 540]),
    "mobile-actions": R(clips={"intro": [0.0, 0.7]}, anchor=[1250, 640]),
    "mobile-switch-dodge": R(clips={"switch": [0.95, 1.45], "dodge": [2.85, 3.5], "block": [3.75, 4.2]}, anchor=[760, 560],
                             anchors={"switch": [760, 560], "dodge": [1160, 560], "block": [1400, 400]}),
    "mobile-hud-layout": R(clips={"tutorial": [0.0, 4.6]}),
    # ---- 08 the screens that had no piece (Set 2, 2026-10-09)
    "story-dialogue": R(hide=[R(text=r"^CONTACT NAME$"), R(text=r"^\[PLACEHOLDER"), R(text=r"^PLACEHOLDER TEXT"), R(text=r"^PORTRAIT")],
                        clips={"open": [0.0, 0.6], "hold": [1.0, 1.1]},
                        slots=[slot("name", 490, 750, 1100, 80, "display", 76, ROSE, "left", [0.3, 4.6]), slot("line", 492, 850, 1150, 40, "mono", 28, W, "left", [0.4, 4.6])]),
    "objective-tracker": R(hide=[R(text=r"^\d+ / \d+$"), R(text=r"^(CLEAR THE ANGELS|SECTION CLEARED)$")], clips={"show": [0.0, 0.9], "clear": [3.2, 4.4]},
                           slots=[slot("count", 1290, 128, 300, 90, "numeral", 84, W, "left", [0.2, 4.6]), slot("objective", 1290, 230, 560, 64, "display", 60, ROSE, "left", [0.2, 4.6])]),
    "gauges-a": R(hide=[R(text=r"^\d+$"), R(text=r"^\d+ / 10$"), R(text=r"^BUFFER")], clips={"show": [0.0, 2.2]}),
    "gauges-b": R(hide=[R(text=r"^\d+%?$")], clips={"show": [0.0, 2.6]}),
    "judge-mode-wheel": R(hide=[R(text=r"^MODE:")], clips={"judge": [0.0, 1.4], "lawyer": [1.4, 2.4], "witness": [2.4, 3.3], "locked": [3.3, 4.4]},
                          slots=[slot("mode", 0, 70, 1920, 40, "mono", 34, W, "center", [0.1, 4.6])]),
    "global-exchange-panel": R(hide=[R(text=r"^\d+(\.\d+)?x$")], clips={"open": [0.0, 0.9]}),
    # ---- page transitions (fully cover the screen at the pack's MID time)
    "tr1": R(kind="transition", clips={"play": [0.0, 0.8]}),
    "tr2": R(kind="transition", clips={"play": [0.0, 1.0]}),
    "tr3": R(kind="transition", clips={"play": [0.0, 1.2]}),
    "tr4": R(kind="transition", clips={"play": [0.0, 1.35]}),
    "tr5": R(kind="transition", clips={"play": [0.0, 1.6]}),
}

# ---- Set 2 VFX (sheets 09-18): every piece of tools/fx_defs.py. Anchored in game on the angel (statuses, angel tells) or the
# caster (abilities; mirrored when the caster faces left on screen); the 1080p frame's body reference is 300 px tall.
def _fx_catalogue():
    sys.path.insert(0, os.path.join(SRC, "tools"))
    import fx_defs
    out = {}
    for p in fx_defs.PIECES:
        ats = [L.get("at", "mid") for L in p["layers"]]
        status = any(L.get("fx") == "status" for L in p["layers"])
        if status or (("en" in ats) and ("cas" not in ats) and p["sheet"] in ("10", "11", "09")):
            mode, anchor = "enemy", [1300, 700]
        elif "cas" in ats:
            mode, anchor = "caster", [620, 700]
        else:
            mode, anchor = "centre", [960, 700]
        dur = float(p["dur"])
        ult = "Ultimate" in (p["slot"] or "")
        if status:
            clips = {"loop": [1.5, min(dur, 3.0)]}
        else:
            clips = {"play": [0.0, min(dur, 5.0 if ult else 3.0)]}
        out[p["id"]] = R(kind="fx", clips=clips, anchor=anchor, mode=mode, who=p["who"] or "", slot=p["slot"] or "", status=status)
    return out


PIECES.update(_fx_catalogue())

TPL_HEAD_EXTRA = '<script>window.GAME_RULES = %s;</script>\n<script src="assets/game.js"></script>\n'


def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def build_project(pid, src_name):
    """Same wrapper as the pack's tools/build.py, plus the game rules and GAME.strip after the piece builds."""
    root = SRC if PIECES[pid].get("kind") == "fx" else DREAM
    spec = importlib.util.spec_from_file_location("build_" + os.path.basename(root).replace("-", "_"), os.path.join(root, "tools", "build.py"))
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    build.ROOT = root
    proj = os.path.join(root, "projects", src_name)
    meta = build.build(src_name)
    html = open(os.path.join(proj, "index.html")).read()
    rules = {pid: PIECES[pid].get("hide", [])}
    html = html.replace('<link rel="stylesheet" href="assets/bib.css" />', TPL_HEAD_EXTRA % json.dumps(rules) + '<link rel="stylesheet" href="assets/bib.css" />', 1)
    strip = '<script>GAME.strip(%s, document.getElementById("L"));</script>\n' % json.dumps(pid)
    html = html.replace('<script>IG.seal(tl, DUR);</script>', strip + '<script>IG.seal(tl, DUR);</script>', 1)
    open(os.path.join(proj, "index.html"), "w").write(html)
    shutil.copy(os.path.join(PACK, "game.js"), os.path.join(proj, "assets", "game.js"))
    # renders go to the work folder, not into the copied source
    dst = os.path.join(WORK, "projects", src_name)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.move(proj, dst)
    return dst, meta


def render(pid, proj):
    frames = os.path.join(WORK, "frames", pid)
    if os.path.exists(frames):
        shutil.rmtree(frames)
    r = run(["npx", "hyperframes", "render", "--format", "png-sequence", "--fps", str(FPS), "--resolution", "1080p", "-o", frames, "--quiet"], cwd=proj)
    pngs = sorted(glob.glob(os.path.join(frames, "**", "*.png"), recursive=True))
    if not pngs:
        print(r.stdout[-2000:])
        raise RuntimeError("render produced no frames for " + pid)
    return pngs


def sequence(pngs, name):
    """Numbered links to the frames (ffmpeg reads image sequences reliably; its concat demuxer drops images)."""
    seq = os.path.join(WORK, "seq_" + name)
    if os.path.exists(seq):
        shutil.rmtree(seq)
    os.makedirs(seq)
    for i, p in enumerate(pngs):
        os.symlink(os.path.abspath(p), os.path.join(seq, "f_%05d.png" % (i + 1)))
    return seq


def analyse(pngs, used=None):
    """Quarter-resolution alpha for every frame: the piece's bounding box and its repeated frames. Only frames in `used` (the
    clips the game plays) are kept; the others map to the nearest kept frame."""
    seq = sequence(pngs, "all")
    raw = subprocess.run(["ffmpeg", "-v", "error", "-framerate", str(FPS), "-i", os.path.join(seq, "f_%05d.png"), "-vf", "scale=480:270:flags=area",
                          "-f", "rawvideo", "-pix_fmt", "rgba", "-"], stdout=subprocess.PIPE, check=True).stdout
    arr = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 270, 480, 4)
    keep = sorted(used) if used else list(range(arr.shape[0]))
    keep = [i for i in keep if i < arr.shape[0]] or [0]
    alpha = arr[keep][..., 3]
    seen = (alpha > 6).any(axis=0)
    ys, xs = np.where(seen)
    if len(xs) == 0:
        return (0, 0, 16, 16), list(range(len(pngs))), [0] * len(pngs)
    pad = 2
    x0, x1 = max(0, xs.min() - pad), min(479, xs.max() + pad)
    y0, y1 = max(0, ys.min() - pad), min(269, ys.max() + pad)
    box = (int(x0) * 4, int(y0) * 4, int(x1 - x0 + 1) * 4, int(y1 - y0 + 1) * 4)
    unique, mapping = [], []
    last = None
    keepset = set(keep)
    for i in range(arr.shape[0]):
        if i not in keepset:
            mapping.append(max(0, len(unique) - 1))
            continue
        if last is not None and np.abs(arr[i].astype(np.int16) - arr[last].astype(np.int16)).max() <= 3:
            mapping.append(len(unique) - 1)
            continue
        unique.append(i)
        mapping.append(len(unique) - 1)
        last = i
    return box, unique, mapping


def pack(pid, pngs, box, unique, kind):
    x, y, w, h = box
    w, h = min(w, 1920 - x), min(h, 1080 - y)
    # Atlas cell size: full size for small pieces, scaled so big ones (transitions, banners) stay a few pages.
    limit = 680 if kind == "transition" else (400 if kind == "fx" else 900)
    scale = min(1.0, limit / max(w, h))
    cw, ch = max(2, int(round(w * scale / 2)) * 2), max(2, int(round(h * scale / 2)) * 2)
    cols, rows = max(1, PAGE // cw), max(1, PAGE // ch)
    per = cols * rows
    pages = int(math.ceil(len(unique) / per))
    out = os.path.join(OUT, pid)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    files = [pngs[i] for i in unique]
    if pages > 1:
        # ffmpeg's tile filter drops a partly filled last page: pad it with the last frame (never shown).
        files += [files[-1]] * (pages * cols * rows - len(files))
    seq = sequence(files, "unique")
    pcols = min(cols, len(unique))
    prows = min(rows, int(math.ceil(len(unique) / pcols)))
    # pages are padded to PAGE x PAGE so they compress (block formats need multiples of 4) and can carry mips for phones
    vf = "crop=%d:%d:%d:%d,scale=%d:%d:flags=lanczos,format=rgba,tile=%dx%d:color=0x00000000,pad=%d:%d:0:0:color=0x00000000" % (w, h, x, y, cw, ch, pcols, prows, PAGE, PAGE)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", str(FPS), "-i", os.path.join(seq, "f_%05d.png"), "-vf", vf, "-frames:v", str(pages), os.path.join(out, "page_%02d.png")], check=True)
    return {"cell": [cw, ch], "cols": pcols, "rows": prows, "pages": pages, "page": [PAGE, PAGE], "scale": scale, "box": [x, y, w, h]}


def main():
    ids = [a for a in sys.argv[1:] if not a.startswith("--")] or list(PIECES)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    manifest = {"fps": FPS, "pieces": {}}
    if os.path.exists(MANIFEST):
        manifest = json.load(open(MANIFEST))
        if manifest.get("fps") != FPS:   # frames at another rate would play at the wrong speed: start a fresh manifest
            manifest = {"fps": FPS, "pieces": {}}
    for pid in ids:
        spec = PIECES[pid]
        kind = spec.get("kind", "ui")
        src_name = pid if kind in ("transition", "fx") else "ui-" + pid
        frames_dir = os.path.join(WORK, "frames", pid)
        cached = sorted(glob.glob(os.path.join(frames_dir, "**", "*.png"), recursive=True))
        if "--repack" in sys.argv and cached:
            pngs, meta = cached, {"dur": len(cached) / FPS}
        else:
            proj, meta = build_project(pid, src_name)
            pngs = render(pid, proj)
        used = set()
        for start, end in spec.get("clips", {}).values():
            used.update(range(max(0, int(math.floor(start * FPS))), int(math.ceil(end * FPS)) + 1))
        box, unique, mapping = analyse(pngs, used)
        atlas = pack(pid, pngs, box, unique, kind)
        entry = {"kind": kind, "duration": meta.get("dur", len(pngs) / FPS), "frames": mapping, "unique": len(unique), "clips": spec.get("clips", {}),
                 "slots": spec.get("slots", []), "anchor": spec.get("anchor"), "anchors": spec.get("anchors", {}),
                 "mode": spec.get("mode", ""), "who": spec.get("who", ""), "slot": spec.get("slot", "")}
        entry.update(atlas)
        manifest["pieces"][pid] = entry
        print("%-20s %3d frames -> %3d unique, box %s, cell %s, %d page(s)" % (pid, len(pngs), len(unique), atlas["box"], atlas["cell"], atlas["pages"]), flush=True)
        json.dump(manifest, open(MANIFEST, "w"), indent=1)
    print("manifest:", MANIFEST)


if __name__ == "__main__":
    main()
