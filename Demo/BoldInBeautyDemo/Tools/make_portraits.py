"""Operator portraits for the UI: Art/Portraits/<name>.png (full body, front view, 768 px high) and
<name>_head.png (256x256 head crop). The hub looks them up by those file names (lower case operator name).

Three modes:

  --placeholders
      Draws a flat silhouette per operator (colour from the name, name written under it). Used until the real
      designs are cut, so the UI always has something to show.

  --sheets DIR [--only luna,judge] [--panels panels.json]
      Cuts the real designs. DIR holds the owner's four-view sheets, <name>-v1.png (death is death-v2.png): front,
      left, right and back views side by side. The front view is assumed to be the first of four equal columns; if
      a sheet differs, give its column (0-3) in a JSON file {"name": column} via --panels. The figure is found by
      its difference from the corner background colour, and the head crop is taken from the top of that figure.
      LOOK AT THE RESULTS (--contact sheet.png makes a contact sheet) before committing them.

  --xlsx FILE [--sheet "Front facing"] [--only luna,judge]
      Reads the owner's workbook of front-facing designs (one picture per row, the operator's name in a text cell
      of the same row). The .xlsx is opened read-only as a zip: each picture's anchor cell (xl/drawings) is matched
      to the name cell of that row (xl/worksheets + xl/sharedStrings). The background is kept. Check by eye.

    pip install pillow
    python3 Demo/BoldInBeautyDemo/Tools/make_portraits.py --placeholders
    python3 Demo/BoldInBeautyDemo/Tools/make_portraits.py --xlsx ~/Downloads/"BoldInBeauty — 20 character designs.xlsx" --contact /tmp/portraits.png
    python3 Demo/BoldInBeautyDemo/Tools/make_portraits.py --sheets ~/Documents/BoldInBeauty-Project/art/character-regeneration/2026-10-03/turnarounds --contact /tmp/portraits.png
"""

import argparse
import colorsys
import hashlib
import io
import json
import os
import posixpath
import re
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "Art", "Portraits")

# Roster order and rarity (BIBCore.cpp); the design sheets use these names.
OPERATORS = [
    ("myrcia", 3), ("fujita", 3), ("ishii", 3), ("shiraishi", 3), ("arita", 3),
    ("luna", 4), ("plack", 4), ("kuu", 4),
    ("alchemist", 5), ("blood", 5), ("stargazer", 5), ("gambler", 5), ("weather", 5), ("judge", 5),
    ("narrator", 6), ("jester", 6), ("media", 6), ("timekeeper", 6), ("death", 6), ("adapt", 6),
]
FULL_HEIGHT = 768
HEAD = 256
MAX_BYTES = 500 * 1024
# Head crops picked by eye where the automatic one (top 30% of the figure) misses the head:
# name -> (centre x, top, side), fractions of the cut full-body picture's height (x of its width).
HEAD_OVERRIDES = {
    "death": (0.52, 0.10, 0.21),  # the scythe blade above her head makes the figure box too tall
}
# Some workbook pictures have the operator's name written in their top-left corner (Ishii, Jester, Media).
LABEL_CORNER = (0.35, 0.07)


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:  # older Pillow
        return ImageFont.load_default()


def name_colour(name, stars):
    digest = hashlib.sha256(name.encode()).digest()
    hue = digest[0] / 255.0
    r, g, b = colorsys.hsv_to_rgb(hue, 0.45 + 0.1 * (stars - 3), 0.85)
    return int(r * 255), int(g * 255), int(b * 255)


def placeholder(name, stars):
    """Flat silhouette, 512x768 (full) and 256x256 (head)."""
    colour = name_colour(name, stars) + (255,)
    shade = tuple(int(c * 0.65) for c in colour[:3]) + (255,)
    full = Image.new("RGBA", (512, FULL_HEIGHT), (0, 0, 0, 0))
    d = ImageDraw.Draw(full)
    d.rounded_rectangle((126, 300, 386, 700), radius=60, fill=shade)
    d.rounded_rectangle((176, 330, 336, 700), radius=40, fill=colour)
    d.ellipse((176, 90, 336, 290), fill=(238, 214, 190, 255))
    d.ellipse((176, 90, 336, 220), fill=shade)
    d.rectangle((226, 280, 286, 320), fill=(238, 214, 190, 255))
    d.text((256, 730), name.upper(), fill=(240, 240, 250, 255), font=font(30), anchor="mm")
    head = full.crop((96, 60, 416, 380)).resize((HEAD, HEAD), Image.LANCZOS)
    return full, head


def figure_box(image):
    """Bounding box of everything that differs from the corner background colour."""
    rgb = image.convert("RGB")
    corners = [rgb.getpixel((0, 0)), rgb.getpixel((rgb.width - 1, 0)), rgb.getpixel((0, rgb.height - 1)), rgb.getpixel((rgb.width - 1, rgb.height - 1))]
    background = tuple(sorted(c[i] for c in corners)[1] for i in range(3))
    mask = Image.new("L", rgb.size, 0)
    px = rgb.load()
    mp = mask.load()
    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = px[x, y]
            if abs(r - background[0]) + abs(g - background[1]) + abs(b - background[2]) > 60:
                mp[x, y] = 255
    return mask.getbbox() or (0, 0, rgb.width, rgb.height)


def cut(sheet_path, column):
    sheet = Image.open(sheet_path).convert("RGBA")
    panel_width = sheet.width // 4
    return cut_figure(sheet.crop((column * panel_width, 0, (column + 1) * panel_width, sheet.height)))


def erase_label(picture):
    """Paints the top-left name label of a workbook picture with the background colour."""
    picture = picture.copy()
    w, h = int(picture.width * LABEL_CORNER[0]), int(picture.height * LABEL_CORNER[1])
    ImageDraw.Draw(picture).rectangle((0, 0, w, h), fill=picture.getpixel((0, 0)))
    return picture


def head_override(full, name):
    cx, top, side = HEAD_OVERRIDES[name]
    side_px = int(full.height * side)
    left = max(0, min(full.width - side_px, int(full.width * cx) - side_px // 2))
    head = full.crop((left, int(full.height * top), left + side_px, int(full.height * top) + side_px))
    return head.resize((HEAD, HEAD), Image.LANCZOS)


def cut_figure(panel):
    """Full body (FULL_HEIGHT high) and a HEAD x HEAD head-and-shoulders crop of the figure in one front view."""
    left, top, right, bottom = figure_box(panel)
    pad = 12
    figure = panel.crop((max(0, left - pad), max(0, top - pad), min(panel.width, right + pad), min(panel.height, bottom + pad)))
    scale = FULL_HEIGHT / figure.height
    full = figure.resize((max(1, int(figure.width * scale)), FULL_HEIGHT), Image.LANCZOS)
    # head and shoulders: the top 30% of the figure, as a square centred on it
    head_height = int(figure.height * 0.30)
    centre = figure.width // 2
    half = max(head_height, int(figure.width * 0.4)) // 2
    head = figure.crop((max(0, centre - half), 0, min(figure.width, centre + half), head_height))
    side = max(head.width, head.height)
    # pad with the picture's own background colour (transparent where the art has alpha)
    square = Image.new(panel.mode, (side, side), panel.getpixel((0, 0)))
    square.paste(head, ((side - head.width) // 2, 0))
    return full, square.resize((HEAD, HEAD), Image.LANCZOS)


XLSX_NS = {
    "m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    "rel": "http://schemas.openxmlformats.org/package/2006/relationships",
    "xdr": "http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing",
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
}


def _rels(book, part):
    """{rId: absolute zip path} for the relationships of one part."""
    folder, file = posixpath.split(part)
    rels_path = posixpath.join(folder, "_rels", file + ".rels")
    if rels_path not in book.namelist():
        return {}
    root = ET.fromstring(book.read(rels_path))
    return {r.get("Id"): posixpath.normpath(posixpath.join(folder, r.get("Target"))) for r in root.findall("rel:Relationship", XLSX_NS)}


def _cell_ref(ref):
    letters, digits = re.match(r"([A-Z]+)(\d+)", ref).groups()
    col = 0
    for ch in letters:
        col = col * 26 + ord(ch) - 64
    return int(digits) - 1, col - 1  # zero-based row, column (as in drawing anchors)


def xlsx_pictures(path, sheet_name):
    """[(row, name, PIL image)] for every picture on the sheet, named by the text cell(s) left of its anchor."""
    with zipfile.ZipFile(path) as book:
        workbook = ET.fromstring(book.read("xl/workbook.xml"))
        book_rels = _rels(book, "xl/workbook.xml")
        sheet_part = None
        for sheet in workbook.find("m:sheets", XLSX_NS):
            if sheet.get("name") == sheet_name:
                sheet_part = book_rels[sheet.get("{%s}id" % XLSX_NS["r"])]
        if not sheet_part:
            raise SystemExit("no sheet named %r in %s" % (sheet_name, path))
        shared = []
        if "xl/sharedStrings.xml" in book.namelist():
            for si in ET.fromstring(book.read("xl/sharedStrings.xml")).findall("m:si", XLSX_NS):
                shared.append("".join(t.text or "" for t in si.iter("{%s}t" % XLSX_NS["m"])))
        texts = {}
        for c in ET.fromstring(book.read(sheet_part)).iter("{%s}c" % XLSX_NS["m"]):
            v = c.find("m:v", XLSX_NS)
            if c.get("t") == "s" and v is not None:
                texts[_cell_ref(c.get("r"))] = shared[int(v.text)]
            elif c.get("t") == "inlineStr":
                texts[_cell_ref(c.get("r"))] = "".join(t.text or "" for t in c.iter("{%s}t" % XLSX_NS["m"]))
            elif c.get("t") == "str" and v is not None:
                texts[_cell_ref(c.get("r"))] = v.text or ""
        pictures = []
        for drawing in [p for p in _rels(book, sheet_part).values() if "/drawings/" in p]:
            media = _rels(book, drawing)
            for anchor in ET.fromstring(book.read(drawing)):
                start = anchor.find("xdr:from", XLSX_NS)
                blip = anchor.find(".//a:blip", XLSX_NS)
                if start is None or blip is None:
                    continue
                row = int(start.find("xdr:row", XLSX_NS).text)
                col = int(start.find("xdr:col", XLSX_NS).text)
                names = [texts[(row, c)].strip() for c in range(col) if texts.get((row, c), "").strip()]
                image = Image.open(io.BytesIO(book.read(media[blip.get("{%s}embed" % XLSX_NS["r"])])))
                image.load()
                pictures.append((row, names[-1] if names else "", image))
        return sorted(pictures, key=lambda p: p[0])


def save_small(image, path):
    """PNG under MAX_BYTES: lossless first, then a 256-colour palette if it is too big."""
    image.save(path, optimize=True)
    if os.path.getsize(path) > MAX_BYTES:
        image.quantize(256, method=Image.FASTOCTREE if image.mode == "RGBA" else Image.MEDIANCUT).save(path, optimize=True)


def sheet_file(directory, name):
    for suffix in ("-v1.png", "-v2.png", "-v3.png", ".png"):
        path = os.path.join(directory, name + suffix)
        if os.path.exists(path):
            return path
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--placeholders", action="store_true")
    parser.add_argument("--sheets")
    parser.add_argument("--xlsx", help="the owner's workbook of front-facing designs (.xlsx, read only)")
    parser.add_argument("--sheet", default="Front facing", help="worksheet that holds the front views (default %(default)s)")
    parser.add_argument("--only", default="")
    parser.add_argument("--panels", help="JSON file {name: column 0-3} for sheets whose front view is not the first column")
    parser.add_argument("--contact", help="write a contact sheet of the results to this path")
    args = parser.parse_args()
    if not args.placeholders and not args.sheets and not args.xlsx:
        parser.error("give --placeholders, --sheets DIR or --xlsx FILE")
    only = {n.strip() for n in args.only.split(",") if n.strip()}
    panels = json.load(open(args.panels)) if args.panels else {}
    os.makedirs(OUT, exist_ok=True)
    written = []
    from_xlsx = {}
    if args.xlsx:
        for row, label, image in xlsx_pictures(args.xlsx, args.sheet):
            key = label.lower()
            print("row %d: %-12s %dx%d %s" % (row + 1, label or "(no name)", image.width, image.height, image.mode))
            if key in from_xlsx:
                raise SystemExit("two pictures named %r" % label)
            from_xlsx[key] = image
        unknown = sorted(set(from_xlsx) - {n for n, _ in OPERATORS})
        if unknown:
            raise SystemExit("pictures with names that are not operators: %s" % ", ".join(unknown))
    for name, stars in OPERATORS:
        if only and name not in only:
            continue
        if args.xlsx:
            if name not in from_xlsx:
                print("missing picture for", name)
                continue
            image = from_xlsx[name]
            full, head = cut_figure(erase_label(image.convert("RGBA") if image.mode in ("RGBA", "LA", "P") else image.convert("RGB")))
            if name in HEAD_OVERRIDES:
                head = head_override(full, name)
        elif args.sheets:
            path = sheet_file(args.sheets, name)
            if not path:
                print("missing sheet for", name)
                continue
            full, head = cut(path, int(panels.get(name, 0)))
        else:
            full, head = placeholder(name, stars)
        save_small(full, os.path.join(OUT, name + ".png"))
        save_small(head, os.path.join(OUT, name + "_head.png"))
        written.append(name)
    print("wrote %d portraits to %s" % (len(written), os.path.normpath(OUT)))
    if args.contact and written:
        cols = 10
        rows = (len(written) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * 260, rows * 270), (40, 44, 60, 255))
        for index, name in enumerate(written):
            tile = Image.open(os.path.join(OUT, name + "_head.png")).convert("RGBA")
            sheet.alpha_composite(tile, ((index % cols) * 260 + 2, (index // cols) * 270 + 2))
            ImageDraw.Draw(sheet).text(((index % cols) * 260 + 6, (index // cols) * 270 + 255), name, fill=(255, 255, 255, 255), font=font(14))
        sheet.save(args.contact)
        print("contact sheet:", args.contact)


if __name__ == "__main__":
    main()
