"""Text that is drawn into the menu pictures, set again as real text.

The titles ("Moeilijkheid") and labels ("Snelheid", "Type:") are part of
the screen pictures, 9 to 30 pixels high; no upscale model makes such
letters clean. tools/upscale_menu.py therefore erases them from the 4x
picture and sets them again in the menu's font, Gill Sans Ultra Bold,
with the colour, weight, shadow and bevel measured from the original.
Which text sits where is in tools/menu_text.tsv. The font is not in git:
it is looked up in the Office folders and the font folders of this Mac;
without it the pictures keep their upscaled text.
"""
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

FONT_NAMES = ("GillSansUltraBold.ttf", "GILSANUB.TTF", "Gill Sans Ultra Bold.ttf")
FONT_DIRS = (
    "/Applications/Microsoft Word.app/Contents/Resources/DFonts",
    "/Applications/Microsoft PowerPoint.app/Contents/Resources/DFonts",
    "/Applications/Microsoft Excel.app/Contents/Resources/DFonts",
    os.path.expanduser("~/Library/Fonts"),
    "/Library/Fonts",
)


def find_font():
    for d in FONT_DIRS:
        for n in FONT_NAMES:
            p = os.path.join(d, n)
            if os.path.exists(p):
                return p
    return None


def load_defs(path):
    """picture name (lower case) -> [(colour, box, text)]."""
    defs = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            pic, colour, box, text = line.rstrip("\n").split("\t")
            defs.setdefault(pic.lower(), []).append((colour, tuple(int(v) for v in box.split(",")), text))
    return defs


def _lum(c):
    return (c[0] * 299 + c[1] * 587 + c[2] * 114) // 1000


def _is_fill(c, colour):
    r, g, b = c
    if colour == "geel":
        return r > 215 and g > 165 and b < 110
    return r > 200 and g > 190 and b > 110 and g - b > 25


def _median(cols):
    if not cols:
        return (0, 0, 0)
    return tuple(sorted(c[i] for c in cols)[len(cols) // 2] for i in range(3))


def _layer(font_path, text, size, track, stroke, scale_box, s):
    """The text as an L mask (full scale), placed with its ink box on
    `scale_box` (x0, y0 at full scale); returns (mask, ink width, height)."""
    font = ImageFont.truetype(font_path, size)
    widths = [font.getlength(ch) for ch in text]
    w = int(sum(widths) + track * (len(text) - 1) + 4 * stroke + size)
    h = int(size * 2 + 4 * stroke)
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    x = 2 * stroke
    for ch, cw in zip(text, widths):
        d.text((x, size // 2), ch, font=font, fill=255, stroke_width=stroke, stroke_fill=255)
        x += cw + track
    return m, m.getbbox()


def _fit(font_path, text, mask_w, mask_h, stroke):
    """Font size and tracking whose ink box is mask_w x mask_h (full scale)."""
    size = max(6, int(mask_h))
    for _ in range(4):
        m, bb = _layer(font_path, text, size, 0, stroke, None, 1)
        if not bb:
            return size, 0
        ih = bb[3] - bb[1]
        size = max(6, int(round(size * mask_h / max(1, ih))))
    m, bb = _layer(font_path, text, size, 0, stroke, None, 1)
    natural = bb[2] - bb[0]
    track = (mask_w - natural) / max(1, len(text) - 1)
    return size, track


def set_text(orig, hd, colour, box, text, font_path, fill_bg):
    """Erases `text` (found in `box` of the original picture `orig`) from
    the upscaled `hd` and sets it again. `fill_bg(img, keep_mask)` fills
    the masked-out pixels from their surroundings. Returns the new hd."""
    s = hd.size[0] // orig.size[0]
    px = orig.load()
    x0, y0, x1, y1 = box
    m = Image.new("L", orig.size, 0)
    mp = m.load()
    for y in range(max(0, y0), min(orig.size[1], y1)):
        for x in range(max(0, x0), min(orig.size[0], x1)):
            if _is_fill(px[x, y], colour):
                mp[x, y] = 255
    bb = m.getbbox()
    if not bb:
        return hd
    area = sum(1 for v in m.getdata() if v)
    # Colours: the letters' own (inside, away from the edge) and the
    # shadow's (where the letters, moved down right, land outside them).
    inner = m.filter(ImageFilter.MinFilter(3))
    fill_cols = [px[x, y] for y in range(bb[1], bb[3]) for x in range(bb[0], bb[2]) if inner.getpixel((x, y))]
    if len(fill_cols) < 5:
        fill_cols = [px[x, y] for y in range(bb[1], bb[3]) for x in range(bb[0], bb[2]) if mp[x, y]]
    fill = _median(fill_cols)
    best = None
    for dx in range(0, 4):
        for dy in range(1, 4):
            sh = ImageChops.subtract(ImageChops.offset(m, dx, dy), m)
            cols = [px[x, y] for y in range(bb[1], min(orig.size[1], bb[3] + 4))
                    for x in range(bb[0], min(orig.size[0], bb[2] + 4)) if sh.getpixel((x, y))]
            if len(cols) < 5:
                continue
            l = sum(_lum(c) for c in cols) / len(cols)
            if best is None or l < best[0]:
                best = (l, dx, dy, _median(cols))
    _, sdx, sdy, shadow = best or (0, 1, 1, (40, 40, 40))
    # Erase letters and shadow: fill from around, scaled up smoothly.
    region = ImageChops.lighter(m, ImageChops.offset(m, sdx, sdy))
    region = ImageChops.lighter(region, ImageChops.offset(m, max(0, sdx - 1), max(0, sdy - 1)))
    region = region.filter(ImageFilter.MaxFilter(5))
    clean = fill_bg(orig, ImageChops.invert(region))
    reg_big = region.resize(hd.size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(s * 0.6))
    out = Image.composite(clean.resize(hd.size, Image.BICUBIC), hd, reg_big)
    # The letters: size, tracking and weight (stroke) fitted to the original.
    tw, th = (bb[2] - bb[0]) * s, (bb[3] - bb[1]) * s
    choice = None
    for stroke in range(0, s + 2):
        size, track = _fit(font_path, text, tw, th, stroke)
        lay, lbb = _layer(font_path, text, size, track, stroke, None, s)
        if not lbb:
            continue
        ink = lay.crop(lbb).resize((max(1, (lbb[2] - lbb[0]) // s), max(1, (lbb[3] - lbb[1]) // s)), Image.BOX)
        got = sum(1 for v in ink.getdata() if v > 110)
        diff = abs(got - area)
        if choice is None or diff < choice[0]:
            choice = (diff, lay, lbb)
    _, lay, lbb = choice
    # Into place: ink box on the original's ink box (full scale).
    layer = Image.new("L", hd.size, 0)
    ink = lay.crop(lbb).resize((tw, th), Image.LANCZOS)
    layer.paste(ink, (bb[0] * s, bb[1] * s))
    shadow_l = ImageChops.offset(layer, sdx * s, sdy * s).filter(ImageFilter.GaussianBlur(s * 0.35))
    out = Image.composite(Image.new("RGB", hd.size, shadow), out, shadow_l)
    # Face with a soft bevel: light from the top left like the menu's art.
    hi = ImageChops.subtract(layer, ImageChops.offset(layer, s // 2, s // 2)).filter(ImageFilter.GaussianBlur(1))
    lo = ImageChops.subtract(layer, ImageChops.offset(layer, -(s // 2), -(s // 2))).filter(ImageFilter.GaussianBlur(1))
    face = Image.new("RGB", hd.size, fill)
    face = Image.composite(Image.new("RGB", hd.size, tuple(min(255, c + 40) for c in fill)), face,
                           hi.point(lambda v: v // 2))
    face = Image.composite(Image.new("RGB", hd.size, tuple(int(c * 0.8) for c in fill)), face,
                           lo.point(lambda v: v // 2))
    return Image.composite(face, out, layer)
