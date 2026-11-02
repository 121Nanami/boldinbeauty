"""Draws the placeholder UI icons into Demo/BoldInBeautyDemo/Art/Icons/*.png (128x128, transparent).

Every icon is a flat, simple shape so it can be swapped later: drop a PNG with the same file name into Art/Icons
and re-import (Tools/build_assets.sh does it). Currency icons are coloured; UI glyphs (navigation, lobby, shop...)
are white so the UI can tint them.

    pip install pillow
    python3 Demo/BoldInBeautyDemo/Tools/make_icons.py [--sheet contact_sheet.png] [--only name,name]
"""

import math
import os
import sys

from PIL import Image, ImageDraw

S = 4                      # supersampling
N = 128 * S
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "Icons")

WHITE = (245, 247, 252, 255)
SHADE = (170, 178, 196, 255)
GOLD = (255, 200, 40, 255)
GOLD_D = (178, 118, 10, 255)
CYAN = (60, 210, 235, 255)
CYAN_D = (20, 120, 150, 255)
PINK = (255, 120, 170, 255)
PINK_D = (190, 50, 100, 255)
BLUE = (80, 140, 255, 255)
PURPLE = (170, 100, 255, 255)
GREEN = (80, 210, 120, 255)
RED = (235, 80, 80, 255)
DARK = (24, 28, 40, 255)
CLEAR = (0, 0, 0, 0)


class Icon:
    def __init__(self):
        self.img = Image.new("RGBA", (N, N), CLEAR)
        self.d = ImageDraw.Draw(self.img)

    @staticmethod
    def p(v):
        return v * S

    def pts(self, points):
        return [(x * S, y * S) for x, y in points]

    def ellipse(self, box, fill=None, outline=None, w=0):
        self.d.ellipse([c * S for c in box], fill=fill, outline=outline, width=w * S)

    def rect(self, box, fill=None, outline=None, w=0, r=0):
        b = [c * S for c in box]
        if r:
            self.d.rounded_rectangle(b, radius=r * S, fill=fill, outline=outline, width=w * S)
        else:
            self.d.rectangle(b, fill=fill, outline=outline, width=w * S)

    def poly(self, points, fill=None, outline=None, w=0):
        self.d.polygon(self.pts(points), fill=fill)
        if outline:
            self.d.line(self.pts(list(points) + [points[0]]), fill=outline, width=w * S, joint="curve")

    def line(self, points, fill=WHITE, w=6):
        self.d.line(self.pts(points), fill=fill, width=w * S, joint="curve")

    def arc(self, box, start, end, fill=WHITE, w=6):
        self.d.arc([c * S for c in box], start, end, fill=fill, width=w * S)

    def save(self, name):
        os.makedirs(OUT, exist_ok=True)
        self.img.resize((128, 128), Image.LANCZOS).save(os.path.join(OUT, name + ".png"))


def star_points(cx, cy, r_out, r_in, n=5, rot=-90):
    pts = []
    for i in range(n * 2):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians(rot + i * 180 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def person(i, cx, cy, s=1.0, fill=WHITE):
    i.ellipse((cx - 13 * s, cy - 30 * s, cx + 13 * s, cy - 4 * s), fill=fill)
    i.rect((cx - 24 * s, cy + 2 * s, cx + 24 * s, cy + 40 * s), fill=fill, r=int(14 * s))


# ------------------------------------------------------------------ currencies (coloured)
def gold(i):
    i.ellipse((10, 10, 118, 118), GOLD, GOLD_D, 6)
    i.ellipse((26, 26, 102, 102), (255, 224, 110, 255), GOLD_D, 4)
    i.poly(star_points(64, 64, 26, 11), GOLD, GOLD_D, 3)


def bytes_(i):
    for x in (40, 58, 76):
        i.line([(x, 14), (x, 114)], CYAN_D, 6)
    for y in (40, 58, 76):
        i.line([(14, y), (114, y)], CYAN_D, 6)
    i.rect((26, 26, 102, 102), CYAN, CYAN_D, 5, r=10)
    i.rect((46, 46, 82, 82), DARK, CYAN_D, 3, r=4)
    i.line([(54, 64), (74, 64)], CYAN, 4)


def hearts(i):
    i.arc((34, 4, 94, 28), 0, 360, GOLD, 5)
    i.ellipse((14, 28, 68, 82), PINK, PINK_D, 4)
    i.ellipse((60, 28, 114, 82), PINK, PINK_D, 4)
    i.poly([(16, 62), (112, 62), (64, 118)], PINK, PINK_D, 4)
    i.ellipse((16, 30, 66, 80), PINK)
    i.ellipse((62, 30, 112, 80), PINK)
    i.poly([(18, 62), (110, 62), (64, 114)], PINK)
    i.ellipse((28, 40, 44, 56), (255, 200, 225, 255))


def diamond(i):
    i.poly([(30, 22), (98, 22), (122, 52), (64, 118), (6, 52)], CYAN, CYAN_D, 4)
    i.poly([(30, 22), (64, 52), (6, 52)], (130, 235, 250, 255))
    i.poly([(98, 22), (64, 52), (122, 52)], (60, 175, 215, 255))
    i.poly([(30, 22), (98, 22), (64, 52)], (190, 248, 255, 255))
    i.poly([(6, 52), (64, 52), (64, 118)], (40, 150, 195, 255))
    i.poly([(122, 52), (64, 52), (64, 118)], (30, 125, 170, 255))
    i.line([(30, 22), (64, 52), (98, 22)], CYAN_D, 2)
    i.line([(6, 52), (122, 52)], CYAN_D, 2)


def ticket(colour):
    def draw(i):
        i.rect((8, 30, 120, 98), colour, WHITE, 3, r=10)
        i.ellipse((-8, 52, 16, 76), CLEAR)
        i.ellipse((112, 52, 136, 76), CLEAR)
        i.line([(86, 38), (86, 90)], WHITE, 3)
        i.poly(star_points(48, 64, 20, 8), WHITE)
    return draw


def mileage(i):
    i.poly([(36, 70), (60, 70), (46, 122), (30, 100), (6, 106)], RED)
    i.poly([(68, 70), (92, 70), (122, 106), (98, 100), (82, 122)], (210, 60, 60, 255))
    i.ellipse((22, 6, 106, 90), GOLD, GOLD_D, 5)
    i.ellipse((36, 20, 92, 76), (255, 224, 110, 255), GOLD_D, 3)
    i.poly(star_points(64, 48, 18, 8), GOLD_D)


def arcade_token(i):
    i.ellipse((10, 10, 118, 118), PURPLE, (100, 50, 180, 255), 6)
    i.ellipse((28, 28, 100, 100), (130, 70, 220, 255), (100, 50, 180, 255), 3)
    i.line([(64, 42), (64, 86)], WHITE, 8)
    i.line([(42, 64), (86, 64)], WHITE, 8)


def star(i):
    i.poly(star_points(64, 68, 58, 24), GOLD, GOLD_D, 4)


def xp(i):
    i.poly(star_points(64, 68, 58, 24), GREEN, (30, 120, 60, 255), 4)
    i.line([(44, 66), (58, 80), (86, 50)], WHITE, 8)


# ------------------------------------------------------------------ navigation (white glyphs)
def nav_operator(i):
    person(i, 64, 62, 1.35)


def nav_team(i):
    person(i, 28, 70, 0.7, SHADE)
    person(i, 100, 70, 0.7, SHADE)
    person(i, 64, 62, 0.95)


def nav_lobby(i):
    i.poly([(8, 62), (64, 12), (120, 62)], WHITE)
    i.rect((24, 58, 104, 114), WHITE, r=4)
    i.rect((52, 78, 76, 114), DARK, r=3)


def nav_shop(i):
    i.arc((38, 8, 90, 64), 180, 360, WHITE, 8)
    i.poly([(16, 44), (112, 44), (104, 116), (24, 116)], WHITE)
    i.ellipse((44, 56, 56, 68), DARK)
    i.ellipse((72, 56, 84, 68), DARK)


def nav_revolution(i):
    i.line([(30, 10), (30, 118)], WHITE, 9)
    i.poly([(34, 14), (112, 34), (34, 66)], WHITE)
    i.ellipse((16, 106, 44, 128), SHADE)


def nav_campaign(i):
    i.line([(18, 18), (110, 110)], WHITE, 11)
    i.line([(110, 18), (18, 110)], WHITE, 11)
    i.line([(6, 40), (40, 6)], SHADE, 9)
    i.line([(122, 40), (88, 6)], SHADE, 9)
    i.ellipse((4, 98, 26, 122), SHADE)
    i.ellipse((102, 98, 124, 122), SHADE)


def nav_summon(i):
    i.poly(star_points(64, 64, 60, 24, 8), WHITE)
    i.ellipse((44, 44, 84, 84), DARK)
    i.ellipse((54, 54, 74, 74), WHITE)


# ------------------------------------------------------------------ lobby (white glyphs)
def quest(i):
    i.rect((24, 10, 104, 118), WHITE, r=10)
    i.line([(40, 40), (88, 40)], DARK, 6)
    i.line([(40, 62), (88, 62)], DARK, 6)
    i.line([(40, 84), (70, 84)], DARK, 6)
    i.line([(76, 96), (86, 106), (104, 82)], GREEN, 8)


def mail(i):
    i.rect((8, 26, 120, 102), WHITE, r=10)
    i.line([(14, 34), (64, 74), (114, 34)], DARK, 6)


def notice(i):
    i.poly([(14, 46), (14, 82), (44, 82), (104, 112), (104, 16), (44, 46)], WHITE)
    i.rect((30, 80, 50, 118), SHADE, r=4)
    i.arc((96, 36, 124, 92), -70, 70, SHADE, 6)


def event(i):
    i.rect((14, 52, 114, 116), WHITE, r=6)
    i.rect((8, 36, 120, 60), SHADE, r=6)
    i.rect((56, 36, 72, 116), RED)
    i.arc((28, 6, 66, 40), 90, 360, RED, 8)
    i.arc((62, 6, 100, 40), 180, 90 + 360, RED, 8)


def arcade(i):
    i.rect((6, 34, 122, 104), WHITE, r=26)
    i.line([(34, 56), (34, 84)], DARK, 8)
    i.line([(20, 70), (48, 70)], DARK, 8)
    i.ellipse((78, 54, 94, 70), RED)
    i.ellipse((96, 68, 112, 84), BLUE)


def seasonpass(i):
    i.poly([(10, 100), (4, 36), (36, 62), (64, 20), (92, 62), (124, 36), (118, 100)], GOLD, GOLD_D, 4)
    i.rect((10, 100, 118, 118), GOLD, GOLD_D, 4, r=4)
    i.ellipse((56, 70, 72, 86), WHITE)


def profile(i):
    i.rect((8, 22, 120, 106), WHITE, r=10)
    i.ellipse((22, 38, 58, 74), DARK)
    i.rect((20, 74, 60, 94), DARK, r=8)
    i.line([(72, 48), (108, 48)], DARK, 6)
    i.line([(72, 66), (108, 66)], DARK, 6)
    i.line([(72, 84), (96, 84)], DARK, 6)


def settings(i):
    for k in range(8):
        a = math.radians(k * 45)
        cx, cy = 64 + 44 * math.cos(a), 64 + 44 * math.sin(a)
        i.ellipse((cx - 12, cy - 12, cx + 12, cy + 12), WHITE)
    i.ellipse((22, 22, 106, 106), WHITE)
    i.ellipse((44, 44, 84, 84), DARK)


def preferences(i):
    for y, x in ((30, 40), (64, 84), (98, 56)):
        i.line([(14, y), (114, y)], SHADE, 7)
        i.ellipse((x - 14, y - 14, x + 14, y + 14), WHITE)


# ------------------------------------------------------------------ revolution (white glyphs)
def building(base, roof=None):
    def draw(i):
        i.rect((18, 40, 110, 116), WHITE, r=4)
        if roof == "flag":
            i.line([(64, 10), (64, 40)], WHITE, 5)
            i.poly([(66, 12), (98, 22), (66, 32)], RED)
        elif roof == "tri":
            i.poly([(10, 44), (64, 8), (118, 44)], WHITE)
        for x in (30, 58, 86):
            i.rect((x, 56, x + 14, 70), DARK)
        i.rect((52, 84, 76, 116), DARK, r=3)
        base(i)
    return draw


def b_school(i):
    building(lambda _: None, "flag")(i)


def b_police(i):
    i.poly([(64, 8), (112, 26), (104, 80), (64, 120), (24, 80), (16, 26)], WHITE)
    i.poly(star_points(64, 62, 30, 13), BLUE)


def b_fire(i):
    i.poly([(64, 6), (92, 40), (112, 76), (96, 112), (64, 124), (32, 112), (16, 76), (40, 44), (50, 62)], (255, 140, 40, 255))
    i.poly([(64, 52), (82, 80), (74, 110), (64, 116), (54, 110), (46, 82)], (255, 220, 90, 255))


def b_hospital(i):
    i.rect((14, 14, 114, 114), WHITE, r=14)
    i.rect((52, 28, 76, 100), RED)
    i.rect((28, 52, 100, 76), RED)


def b_gym(i):
    i.line([(30, 64), (98, 64)], WHITE, 10)
    for x in (14, 36, 78, 100):
        i.rect((x, 36 if x in (36, 78) else 46, x + 14, 92 if x in (36, 78) else 82), SHADE if x in (14, 100) else WHITE, r=4)


def b_market(i):
    i.poly([(10, 50), (24, 16), (104, 16), (118, 50)], RED)
    for k in range(4):
        i.rect((10 + k * 27, 44, 36 + k * 27, 58), WHITE if k % 2 else SHADE, r=6)
    i.rect((18, 58, 110, 112), WHITE, r=3)
    i.rect((30, 72, 98, 94), DARK, r=3)


def b_workshop(i):
    i.line([(30, 100), (92, 38)], WHITE, 14)
    i.poly([(76, 14), (116, 40), (96, 62), (58, 40)], SHADE)
    i.ellipse((14, 86, 44, 116), SHADE)


def b_shelter(i):
    i.poly([(64, 8), (122, 112), (6, 112)], WHITE)
    i.poly([(64, 60), (86, 112), (42, 112)], DARK)


# ------------------------------------------------------------------ shops / misc
def s_armoury(i):
    i.poly([(64, 8), (114, 26), (106, 84), (64, 120), (22, 84), (14, 26)], WHITE)
    i.poly([(64, 20), (100, 33), (94, 78), (64, 106), (34, 78), (28, 33)], BLUE)


def s_trade(i):
    i.poly([(10, 40), (74, 40), (74, 22), (118, 54), (74, 86), (74, 68), (10, 68)], WHITE)
    i.poly([(118, 88), (54, 88), (54, 70), (10, 102), (54, 134), (54, 116), (118, 116)], SHADE)


def s_alley(i):
    i.rect((22, 10, 106, 118), (60, 56, 80, 255), WHITE, 4, r=6)
    i.rect((38, 30, 90, 118), DARK, r=4)
    i.ellipse((74, 70, 84, 80), GOLD)
    i.ellipse((50, 44, 78, 72), (130, 120, 160, 255))


def s_premium(i):
    diamond(i)
    i.line([(100, 14), (100, 42)], WHITE, 6)
    i.line([(86, 28), (114, 28)], WHITE, 6)


def m_lock(i):
    i.arc((32, 6, 96, 70), 180, 360, WHITE, 10)
    i.rect((20, 54, 108, 120), WHITE, r=10)
    i.ellipse((54, 76, 74, 96), DARK)
    i.rect((60, 88, 68, 106), DARK)


def m_check(i):
    i.line([(18, 68), (50, 100), (112, 28)], GREEN, 16)


def m_cross(i):
    i.line([(24, 24), (104, 104)], WHITE, 14)
    i.line([(104, 24), (24, 104)], WHITE, 14)


def m_back(i):
    i.line([(88, 18), (34, 64), (88, 110)], WHITE, 14)


def m_next(i):
    i.line([(40, 18), (94, 64), (40, 110)], WHITE, 14)


def m_plus(i):
    i.line([(64, 20), (64, 108)], WHITE, 14)
    i.line([(20, 64), (108, 64)], WHITE, 14)


def m_arena(i):
    i.poly([(30, 12), (98, 12), (92, 58), (64, 80), (36, 58)], GOLD, GOLD_D, 4)
    i.arc((6, 18, 40, 52), 90, 270, GOLD, 7)
    i.arc((88, 18, 122, 52), -90, 90, GOLD, 7)
    i.rect((54, 78, 74, 100), GOLD)
    i.rect((34, 100, 94, 118), GOLD, GOLD_D, 3, r=3)


def m_tower(i):
    i.rect((40, 12, 88, 40), WHITE, r=3)
    i.rect((30, 40, 98, 78), SHADE, r=3)
    i.rect((18, 78, 110, 118), WHITE, r=3)
    for x in (44, 60, 76):
        i.rect((x, 4, x + 8, 16), WHITE)


def m_book(i):
    i.rect((14, 16, 112, 112), WHITE, r=8)
    i.rect((22, 24, 104, 104), BLUE, r=4)
    i.line([(64, 24), (64, 104)], WHITE, 4)
    i.line([(34, 46), (54, 46)], WHITE, 4)
    i.line([(74, 46), (94, 46)], WHITE, 4)


def m_stage(i):
    i.line([(28, 8), (28, 120)], WHITE, 9)
    i.poly([(32, 12), (112, 38), (32, 66)], WHITE)


def m_dice(i):
    i.rect((14, 14, 114, 114), WHITE, r=18)
    for x, y in ((38, 38), (90, 38), (64, 64), (38, 90), (90, 90)):
        i.ellipse((x - 9, y - 9, x + 9, y + 9), DARK)


def m_cards(i):
    i.rect((10, 22, 76, 112), SHADE, r=10)
    i.rect((44, 14, 114, 104), WHITE, r=10)
    i.poly(star_points(79, 59, 22, 9), RED)


def m_coin(i):
    i.ellipse((10, 10, 118, 118), WHITE)
    i.ellipse((24, 24, 104, 104), DARK)
    i.line([(54, 50), (74, 50), (74, 78), (54, 78), (54, 50)], WHITE, 5)


# ------------------------------------------------------------------ 2026-10-06: arcade, social, settings
def gang_coin(i):
    i.ellipse((8, 8, 120, 120), PINK)
    i.ellipse((18, 18, 110, 110), PINK_D)
    i.poly(star_points(64, 66, 34, 15), WHITE)


def friends(i):
    person(i, 44, 58, 0.8, SHADE)
    person(i, 80, 64, 1.0, WHITE)


def gang(i):
    i.poly([(64, 8), (114, 26), (110, 74), (64, 120), (18, 74), (14, 26)], WHITE)
    i.poly([(64, 24), (98, 36), (96, 70), (64, 102), (32, 70), (30, 36)], DARK)
    person(i, 64, 66, 0.7, PINK)


def night(i):
    i.ellipse((16, 12, 112, 108), (250, 230, 160, 255))
    i.ellipse((42, 4, 128, 90), CLEAR)
    i.d.ellipse([c * S for c in (42, 4, 128, 90)], fill=CLEAR)
    i.rect((20, 92, 108, 120), SHADE, r=6)
    i.rect((30, 98, 54, 120), DARK)
    i.rect((74, 98, 98, 120), DARK)


def rhythm(i):
    for k, x in enumerate((18, 44, 70, 96)):
        i.rect((x, 12, x + 14, 116), SHADE if k % 2 else WHITE, r=4)
    i.ellipse((14, 74, 40, 100), PINK)
    i.ellipse((66, 40, 92, 66), CYAN)
    i.rect((8, 96, 120, 104), PINK)


def bossrush(i):
    i.poly([(14, 40), (34, 14), (54, 36), (64, 10), (74, 36), (94, 14), (114, 40), (106, 74), (22, 74)], GOLD)
    i.ellipse((30, 60, 98, 120), WHITE)
    i.ellipse((44, 78, 58, 92), DARK)
    i.ellipse((70, 78, 84, 92), DARK)
    i.rect((48, 102, 80, 110), DARK)


def halofall(i):
    # an angel under its halo, shot at from below
    i.ellipse((34, 8, 94, 26), None, GOLD, 6)
    i.poly([(64, 30), (84, 56), (64, 88), (44, 56)], WHITE)
    i.poly([(44, 50), (8, 34), (24, 66)], SHADE)
    i.poly([(84, 50), (120, 34), (104, 66)], SHADE)
    i.rect((60, 96, 68, 112), CYAN)
    i.rect((40, 112, 88, 124), PINK, r=4)


def memory(i):
    # two cards, one face down (BB pink), one face up
    i.rect((10, 22, 62, 106), PINK_D, r=8)
    i.rect((18, 30, 54, 98), PINK, r=4)
    i.rect((66, 22, 118, 106), WHITE, r=8)
    i.ellipse((78, 34, 106, 62), SHADE)
    i.rect((76, 66, 108, 96), SHADE, r=6)


def breach(i):
    # the bar, the open port and the cursor
    i.rect((6, 44, 122, 84), DARK, WHITE, 4, r=6)
    i.rect((70, 48, 100, 80), GREEN)
    i.rect((80, 48, 90, 80), GOLD)
    i.rect((40, 30, 48, 98), PINK)


def rally(i):
    # Ark Rally (2026-10-15): a trial bike mid-jump over a pink ramp
    i.poly([(4, 124), (60, 124), (60, 96)], PINK)
    i.ellipse((22, 50, 58, 86), None, WHITE, 7)
    i.ellipse((82, 34, 118, 70), None, WHITE, 7)
    i.line([(40, 68), (64, 54), (100, 52)], PINK, 10)
    i.line([(64, 54), (60, 30)], GOLD, 9)
    i.rect((50, 10, 72, 30), GOLD, r=6)
    i.line([(60, 34), (92, 36)], GOLD, 6)


def admin(i):
    i.poly([(64, 8), (112, 26), (106, 76), (64, 120), (22, 76), (16, 26)], WHITE)
    i.line([(42, 64), (58, 82), (88, 46)], DARK, 10)


def keys(i):
    i.rect((6, 30, 122, 104), WHITE, r=12)
    for row, y in enumerate((40, 60)):
        for x in range(16, 108, 20):
            i.rect((x, y, x + 14, y + 14), DARK, r=3)
    i.rect((30, 82, 98, 94), DARK, r=3)


def equipment(i):
    i.line([(24, 104), (96, 32)], WHITE, 12)
    i.poly([(88, 18), (112, 16), (110, 40)], WHITE)
    i.line([(30, 78), (50, 98)], SHADE, 10)
    i.ellipse((14, 100, 32, 118), SHADE)


def m_gift(i):
    event(i)


def m_pass(i):
    seasonpass(i)


ICONS = {
    # currencies
    "gold": gold, "bytes": bytes_, "hearts": hearts, "diamond": diamond,
    "ticket_regular": ticket(BLUE), "ticket_special": ticket(PURPLE), "ticket_daily": ticket(GREEN),
    "mileage": mileage, "arcade_token": arcade_token, "star": star, "xp": xp,
    # navigation
    "nav_operator": nav_operator, "nav_team": nav_team, "nav_lobby": nav_lobby, "nav_shop": nav_shop,
    "nav_revolution": nav_revolution, "nav_campaign": nav_campaign, "nav_summon": nav_summon,
    # lobby
    "quest": quest, "mail": mail, "notice": notice, "event": event, "arcade": arcade, "seasonpass": seasonpass,
    "profile": profile, "settings": settings, "preferences": preferences,
    # revolution
    "b_school": b_school, "b_police": b_police, "b_fire": b_fire, "b_hospital": b_hospital, "b_gym": b_gym,
    "b_market": b_market, "b_workshop": b_workshop, "b_shelter": b_shelter,
    # shops / misc
    "s_armoury": s_armoury, "s_trade": s_trade, "s_alley": s_alley, "s_premium": s_premium,
    "lock": m_lock, "check": m_check, "cross": m_cross, "back": m_back, "next": m_next, "plus": m_plus,
    "arena": m_arena, "tower": m_tower, "book": m_book, "stage": m_stage, "dice": m_dice, "cards": m_cards,
    "coin": m_coin,
    # 2026-10-06: arcade, friends, gangs, admin desk, key bindings
    "gang_coin": gang_coin, "friends": friends, "gang": gang, "night": night, "rhythm": rhythm, "bossrush": bossrush,
    "admin": admin, "keys": keys, "equipment": equipment,
    # 2026-10-07: arcade mini-games
    "halofall": halofall, "memory": memory, "breach": breach,
    # 2026-10-15: Ark Rally
    "rally": rally,
}


# ------------------------------------------------------------------ 2026-10-07: inventory, currencies and elements
ORANGE = (255, 150, 50, 255)
ORANGE_D = (180, 90, 20, 255)


def charge(i):
    i.rect((24, 20, 104, 118), DARK, GREEN, 6, r=12)
    i.rect((48, 8, 80, 22), GREEN, r=4)
    i.poly([(70, 30), (40, 74), (62, 74), (54, 108), (88, 60), (66, 60), (74, 30)], GOLD)


def element_core(i):
    hexa = [(64 + 52 * math.cos(math.radians(a)), 64 + 52 * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
    i.poly(hexa, PURPLE, (100, 50, 180, 255), 5)
    inner = [(64 + 28 * math.cos(math.radians(a)), 64 + 28 * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
    i.poly(inner, CYAN)
    i.ellipse((54, 54, 74, 74), WHITE)


def event_token(i):
    i.ellipse((10, 10, 118, 118), ORANGE, ORANGE_D, 6)
    i.ellipse((28, 28, 100, 100), (255, 190, 100, 255), ORANGE_D, 3)
    i.poly(star_points(64, 66, 26, 11), WHITE)


def ark_medal(i):
    i.poly([(34, 6), (58, 6), (70, 56), (46, 56)], BLUE)
    i.poly([(94, 6), (70, 6), (58, 56), (82, 56)], RED)
    i.ellipse((24, 44, 104, 124), (200, 210, 230, 255), (110, 120, 150, 255), 6)
    i.poly([(64, 58), (90, 74), (90, 102), (64, 116), (38, 102), (38, 74)], (150, 160, 190, 255))
    i.line([(64, 66), (64, 108)], WHITE, 6)


def boost(base):
    def draw(i):
        base(i)
        i.ellipse((70, 70, 126, 126), GREEN, (30, 120, 60, 255), 4)
        i.poly([(98, 78), (118, 100), (106, 100), (106, 118), (90, 118), (90, 100), (78, 100)], WHITE)
    return draw


def inventory(i):
    i.rect((18, 38, 110, 116), WHITE, r=14)
    i.rect((40, 14, 88, 44), CLEAR, WHITE, 8, r=10)
    i.rect((18, 62, 110, 72), SHADE)
    i.rect((54, 56, 74, 80), DARK, r=4)


def el_base(colour, dark):
    def draw(i, glyph):
        i.ellipse((6, 6, 122, 122), DARK, colour, 8)
        glyph(i, colour, dark)
    return draw


def g_fire(i, c, d):
    i.poly([(64, 18), (94, 62), (88, 98), (64, 110), (40, 98), (34, 62), (52, 72)], c)
    i.poly([(64, 58), (78, 84), (64, 100), (50, 84)], GOLD)


def g_water(i, c, d):
    i.poly([(64, 16), (92, 66), (36, 66)], c)
    i.ellipse((34, 46, 94, 106), c)
    i.ellipse((48, 66, 62, 80), WHITE)


def g_nature(i, c, d):
    i.poly([(30, 96), (40, 44), (88, 24), (98, 72), (52, 96)], c)
    i.line([(30, 100), (82, 40)], d, 6)


def g_volt(i, c, d):
    i.poly([(72, 14), (34, 70), (60, 70), (50, 114), (94, 54), (68, 54), (80, 14)], c)


def g_dark(i, c, d):
    i.ellipse((24, 24, 104, 104), c)
    i.ellipse((44, 18, 116, 90), DARK)


def g_halo(i, c, d):
    i.ellipse((22, 26, 106, 58), CLEAR, c, 8)
    i.poly(star_points(64, 82, 26, 10, n=4), c)


def g_magic(i, c, d):
    i.poly(star_points(64, 64, 44, 18), c)
    i.ellipse((56, 56, 72, 72), WHITE)


def g_glitch(i, c, d):
    for k, (x, y, w) in enumerate(((28, 30, 50), (46, 50, 58), (22, 70, 64), (40, 90, 46))):
        i.rect((x, y, x + w, y + 12), c if k % 2 == 0 else WHITE)


def g_none(i, c, d):
    i.line([(40, 64), (88, 64)], c, 10)


def element_icon(colour, dark, glyph):
    base = el_base(colour, dark)
    return lambda i: base(i, glyph)


ICONS.update({
    "charge": charge, "element_core": element_core, "event_token": event_token, "ark_medal": ark_medal,
    "boost_gold": boost(gold), "boost_bytes": boost(bytes_), "boost_hearts": boost(hearts), "inventory": inventory,
    "el_fire": element_icon((255, 98, 40, 255), (160, 50, 10, 255), g_fire),
    "el_water": element_icon((50, 140, 255, 255), (20, 70, 160, 255), g_water),
    "el_nature": element_icon((90, 215, 80, 255), (30, 120, 40, 255), g_nature),
    "el_volt": element_icon((255, 225, 50, 255), (170, 140, 10, 255), g_volt),
    "el_dark": element_icon((140, 80, 205, 255), (70, 30, 120, 255), g_dark),
    "el_halo": element_icon((255, 245, 190, 255), (190, 170, 90, 255), g_halo),
    "el_magic": element_icon((230, 90, 215, 255), (130, 30, 120, 255), g_magic),
    "el_glitch": element_icon((50, 255, 205, 255), (10, 140, 110, 255), g_glitch),
    "el_none": element_icon((150, 150, 160, 255), (80, 80, 90, 255), g_none),
})


def main():
    names = []
    only = sys.argv[sys.argv.index("--only") + 1].split(",") if "--only" in sys.argv else None
    for name, fn in ICONS.items():
        if only and name not in only:
            continue
        icon = Icon()
        fn(icon)
        icon.save(name)
        names.append(name)
    print("wrote %d icons to %s" % (len(names), os.path.normpath(OUT)))
    if "--sheet" in sys.argv:
        path = sys.argv[sys.argv.index("--sheet") + 1]
        cols = 9
        rows = (len(names) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * 136, rows * 136), (40, 44, 60, 255))
        for k, name in enumerate(names):
            tile = Image.open(os.path.join(OUT, name + ".png"))
            sheet.alpha_composite(tile, ((k % cols) * 136 + 4, (k // cols) * 136 + 4))
        sheet.save(path)
        print("contact sheet:", path)


if __name__ == "__main__":
    main()
