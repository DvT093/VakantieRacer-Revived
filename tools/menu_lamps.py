"""Round lamps for the upgraded menu, drawn at full resolution.

The menu's lamps are 7 pixels wide: the bulbs on the rails, the status dots
(L_*.bmp) and the empty grey sockets they sit in, baked into the screens.
An AI upscale makes those few pixels square and blotchy, so
tools/upscale_menu.py draws them as clean round lamps instead: lamp
pictures are drawn from scratch, and on the screens the lamps found in the
original picture are painted over the upscale at their place.
"""
from PIL import Image, ImageChops, ImageDraw, ImageFilter

SS = 4  # supersampling for smooth edges


def _lum(c):
    return (c[0] * 299 + c[1] * 587 + c[2] * 114) // 1000


def _is_lit(c):
    r, g, b = c
    return b > 170 and r < 150 and b - r > 90  # cyan/blue lamp glass


def _is_socket(c):
    r, g, b = c
    return max(c) - min(c) < 28 and _lum(c) < 135  # dark grey


def _components(img, test, box=None):
    w, h = img.size
    px = img.load()
    seen = set()
    out = []
    x0, y0, x1, y1 = box or (0, 0, w, h)
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x, y) in seen or not test(px[x, y]):
                continue
            stack, comp = [(x, y)], []
            seen.add((x, y))
            while stack:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if x0 <= nx < x1 and y0 <= ny < y1 and (nx, ny) not in seen and test(px[nx, ny]):
                        seen.add((nx, ny))
                        if len(comp) < 200:
                            stack.append((nx, ny))
            out.append(comp)
    return out


def find_lamps(img):
    """Lamps baked into a screen: [(cx, cy, r, kind, colour)] in pixels of
    the original; kind 'lit' (with its glass colour) or 'socket'. A lamp is
    a round spot about 7 pixels wide that differs from a ring around it:
    a dark grey hole in a lighter grey rail, or blue/cyan glass."""
    w, h = img.size
    px = img.load()
    lum = [[_lum(px[x, y]) for x in range(w)] for y in range(h)]
    ring = [(dx, dy) for dx in range(-5, 6) for dy in range(-5, 6) if 4.6 <= (dx * dx + dy * dy) ** 0.5 <= 5.6]
    core = [(dx, dy) for dx in range(-2, 3) for dy in range(-2, 3) if dx * dx + dy * dy <= 4]
    lamps = []
    taken = set()
    for y in range(5, h - 5):
        for x in range(5, w - 5):
            c = px[x, y]
            lit = _is_lit(c)
            if not lit and not _is_socket(c):
                continue
            if any((x + dx, y + dy) in taken for dx in range(-4, 5) for dy in range(-4, 5)):
                continue
            cs = [px[x + dx, y + dy] for dx, dy in core]
            rs = [px[x + dx, y + dy] for dx, dy in ring]
            if lit:
                if sum(_is_lit(q) for q in cs) < len(cs) - 3 or sum(_is_lit(q) for q in rs) > 3:
                    continue
            else:
                cl = sum(lum[y + dy][x + dx] for dx, dy in core) / len(core)
                rl = [lum[y + dy][x + dx] for dx, dy in ring]
                # All around lighter grey (a rail), the core all dark grey.
                if sum(_is_socket(q) for q in cs) < len(cs) or min(rl) < cl + 12:
                    continue
                if sum(1 for q in rs if max(q) - min(q) < 40) < len(rs) - 2:
                    continue
            # Centre: middle of the spot's extent around (x, y).
            # Centre: the weighted middle of the spot (how much each pixel
            # differs from the ring), sub-pixel, so a row of lamps lines up.
            test = _is_lit if lit else _is_socket
            ring_l = sum(lum[y + dy][x + dx] for dx, dy in ring) / len(ring)
            sw = sx = sy = 0.0
            for dy in range(-4, 5):
                for dx in range(-4, 5):
                    q = px[x + dx, y + dy]
                    if not test(q):
                        continue
                    wgt = abs(ring_l - lum[y + dy][x + dx]) + 1
                    sw += wgt
                    sx += wgt * (x + dx + 0.5)
                    sy += wgt * (y + dy + 0.5)
            cx, cy = sx / sw, sy / sw
            r = 3.5  # all the menu's lamps are 7 pixels wide; the found extent wobbles
            for dx in range(-4, 5):
                for dy in range(-4, 5):
                    taken.add((int(cx) + dx, int(cy) + dy))
            if lit:
                colour = sorted(cs, key=_lum)[len(cs) // 3]
            else:  # the rail around it, to cover the upscaled hole first
                colour = tuple(sum(q[i] for q in rs) // len(rs) for i in range(3))
            lamps.append((cx, cy, r, "lit" if lit else "socket", colour))
    return lamps


def _disc(size, cx, cy, r):
    m = Image.new("L", (size[0] * SS, size[1] * SS), 0)
    ImageDraw.Draw(m).ellipse([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], fill=255)
    return m.resize(size, Image.LANCZOS)


# The glass colours of the menu's lamps (L_*.bmp, the rail bulbs): every
# lamp is drawn in one of these, so equal lamps look equal everywhere.
GLASS = [(24, 248, 255), (27, 133, 255), (74, 255, 24), (255, 33, 24)]


def _glass(c):
    return min(GLASS, key=lambda g: sum((a - b) ** 2 for a, b in zip(g, c)))


def paint_lamp(dst, cx, cy, r, kind, colour):
    """Draws one lamp on RGB(A) image `dst` (centre and radius in its pixels).
    One light for all: from the top left (highlight top left on the glass;
    a hole is dark at the top left with its lit lip at the bottom right)."""
    w, h = dst.size
    pad = int(r * 2) + 4
    bx, by = int(cx - pad), int(cy - pad)
    size = (2 * pad, 2 * pad)
    lx, ly = cx - bx, cy - by
    rgb = dst.convert("RGB").crop((bx, by, bx + size[0], by + size[1]))
    alpha_out = dst.getchannel("A").crop((bx, by, bx + size[0], by + size[1])) if dst.mode == "RGBA" else None
    body = _disc(size, lx, ly, r)
    if kind == "socket":
        # Cover the upscaled hole with the rail colour around it.
        cover = _disc(size, lx, ly, r * 1.3).filter(ImageFilter.GaussianBlur(r * 0.12))
        rgb = Image.composite(Image.new("RGB", size, colour), rgb, cover)
        # A hole: dark, shaded from the top left, a light lip bottom right.
        # Inside: a diagonal gradient, dark at the top left (the shadow of
        # the edge) to lighter at the bottom right, like the original.
        grad = Image.linear_gradient("L").rotate(45, resample=Image.BICUBIC, expand=True)
        grad = grad.resize((int(r * 2.9), int(r * 2.9)), Image.BICUBIC)
        g = Image.new("L", size, 128)
        g.paste(grad, (int(lx - grad.size[0] / 2), int(ly - grad.size[1] / 2)))
        inner = Image.merge("RGB", [g.point(lambda v: 62 + v * 70 // 255)] * 3)
        lip = ImageChops.subtract(_disc(size, lx + r * 0.1, ly + r * 0.1, r * 1.06), body)
        rgb = Image.composite(Image.new("RGB", size, (235, 235, 238)), rgb, lip.point(lambda v: v * 2 // 5))
        rim = ImageChops.subtract(_disc(size, lx - r * 0.1, ly - r * 0.1, r * 1.06), body)
        rgb = Image.composite(Image.new("RGB", size, (70, 70, 74)), rgb, rim.point(lambda v: v * 2 // 5))
        rgb = Image.composite(inner, rgb, body)
    else:
        c = _glass(colour)
        dark = tuple(int(v * 0.55) for v in c)
        light = tuple(min(255, int(v * 0.6 + 255 * 0.4)) for v in c)
        glass = Image.new("RGB", size, dark)
        glass = Image.composite(Image.new("RGB", size, c), glass,
                                _disc(size, lx, ly + r * 0.1, r * 0.8).filter(ImageFilter.GaussianBlur(r * 0.3)))
        glass = Image.composite(Image.new("RGB", size, light), glass,
                                _disc(size, lx, ly + r * 0.15, r * 0.45).filter(ImageFilter.GaussianBlur(r * 0.3)))
        hl = _disc(size, lx - r * 0.3, ly - r * 0.35, r * 0.28).filter(ImageFilter.GaussianBlur(r * 0.12))
        glass = Image.composite(Image.new("RGB", size, (255, 255, 255)), glass, hl.point(lambda v: v * 9 // 10))
        rim = ImageChops.subtract(_disc(size, lx, ly, r * 1.1), body)
        rgb = Image.composite(Image.new("RGB", size, tuple(int(v * 0.35) for v in c)), rgb, rim.point(lambda v: v // 2))
        rgb = Image.composite(glass, rgb, body)
    if alpha_out is not None:
        a = ImageChops.lighter(alpha_out, _disc(size, lx, ly, r * 1.1))
        rgb = rgb.convert("RGBA")
        rgb.putalpha(a)
        dst.paste(rgb, (bx, by))
    else:
        dst.paste(rgb, (bx, by))


def lamp_sprite(img, scale, keyed):
    """HD version of a lamp picture (one or more 7x7 cells side by side):
    the lamps drawn from scratch, at `scale`. RGBA when `keyed`."""
    w, h = img.size
    cell = h
    px = img.load()
    if keyed:
        out = Image.new("RGBA", (w * scale, h * scale), (0, 0, 0, 0))
    else:
        # The background around the lamp (corner colour), upscaled smoothly.
        out = img.resize((w * scale, h * scale), Image.BICUBIC)
        bg = px[0, 0]
        out = Image.new("RGB", out.size, bg)
    key = px[0, h - 1]
    for i in range(max(1, w // cell)):
        cols = [px[x, y] for x in range(i * cell, (i + 1) * cell) for y in range(cell)]
        glass = [c for c in cols if c != key and (not keyed or c != key)]
        lit = [c for c in glass if max(c) - min(c) > 80 and _lum(c) > 60] or glass
        if not lit:
            continue
        lit.sort(key=_lum)
        colour = lit[len(lit) // 3]
        paint_lamp(out, (i * cell + cell / 2) * scale, cell / 2 * scale, cell / 2 * scale * 0.98, "lit", colour)
    return out
