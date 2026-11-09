#!/usr/bin/env python3
"""Makes the mod assets that are derived from game data, from your own CDs.

They stay on this Mac (mods/race/local and mods/menu/local are in
.gitignore): nothing of the game goes into git.

  mods/race/local/
    bonus_z3.tga       the BMW Z3's paint: none of the CDs has a texture
                       for its model, so a plain BMW blue sheet painted in
                       the model's own UV layout (glass, lights, underside)
    bonus_dl.tga       the DeLorean's paint: A2 Racer IV's L1racer2 sheet
                       (the same delorean.d3d, so the UVs fit), else Europe
                       Racer's, else a plain silver sheet like the Z3's
    L0/L1/L2_anim.ini  LEAGUE_BONUS and TOURNAMENT_BONUS use those sheets
  mods/menu/local/
    CAR_FER/CAR_Z3/CAR_DEL.bmp   the bonus cars alone on an orange
                                 background (297x227, like CAR_*.bmp)
    TRL_CAR/TRL_HIP/TRL_JET.bmp  the trailers, small, for the trailer button

  tools/make_mod_assets.py [--game-dir iso/data] [--a2 DIR] [--er DIR]

--a2 / --er: the data folder with rcs.ind of A2 Racer IV / Europe Racer
(default: ~/Projects/Games/A2 Racer IV/iso/Data and
~/Projects/Games/Europe Racer/iso/EuropeRacer/res, when there).
Needs Pillow; pure Python, no numpy.
"""
import argparse
import bisect
import io
import math
import os
import struct
import sys
import zlib

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOME = os.path.expanduser("~")


# ------------------------------------------------------------------ archives

def find_nocase(d, name):
    try:
        for n in os.listdir(d):
            if n.lower() == name.lower():
                return os.path.join(d, n)
    except OSError:
        pass
    return None


class Archive:
    """RED rcs.ind/rcs.img: u16 count, then name[20] + u32 offset."""

    def __init__(self, d):
        ind = find_nocase(d, "rcs.ind")
        img = find_nocase(d, "rcs.img")
        if not ind or not img:
            raise FileNotFoundError(d)
        data = open(ind, "rb").read()
        n = struct.unpack("<H", data[:2])[0]
        self.entries = {}
        offs = []
        for i in range(n):
            e = data[2 + 24 * i: 26 + 24 * i]
            name = e[:20].split(b"\0")[0].decode("latin1").lower()
            off = struct.unpack("<I", e[20:24])[0]
            self.entries.setdefault(name, off)
            offs.append(off)
        self.f = open(img, "rb")
        size = self.f.seek(0, 2)
        self.offs = sorted(set(offs)) + [size]

    def has(self, name):
        return name.lower() in self.entries

    def read(self, name):
        o = self.entries[name.lower()]
        self.f.seek(o)
        b = self.f.read(self.offs[bisect.bisect_right(self.offs, o)] - o)
        # Europe Racer: zlib with a 4-byte length in front.
        if name.lower().endswith(".tga") and len(b) > 3 and b[2] not in (2, 10):
            for start in (4, 0):
                try:
                    return zlib.decompress(b[start:])
                except zlib.error:
                    pass
        return b

    def image(self, name):
        return Image.open(io.BytesIO(self.read(name))).convert("RGB")


# --------------------------------------------------------------------- models

def load_d3d(b):
    """.d3d: i32 -1, u32 vertices, u32 faces; vertex = 12 floats (pos,
    normal, uv, 4 more) + 1 byte; face = 4 u16 (3 indices, material) + 1."""
    nv, nf = struct.unpack("<II", b[4:12])
    o = 12
    verts, faces = [], []
    for _ in range(nv):
        verts.append(struct.unpack("<12f", b[o:o + 48]))
        o += 49
    for _ in range(nf):
        faces.append(struct.unpack("<4H", b[o:o + 8]))
        o += 9
    return verts, faces


def face_normal(a, b, c):
    u = [b[k] - a[k] for k in range(3)]
    v = [c[k] - a[k] for k in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    ln = math.sqrt(sum(x * x for x in n)) or 1.0
    return [x / ln for x in n]


# ------------------------------------------------------------------ painting

def paint_sheet(verts, faces, classify, size=256, background=(40, 80, 170)):
    """A texture sheet in the model's own UV layout: every triangle filled
    with the colour classify(centroid, normal) gives it, lit a little from
    above so the flat colours still show the shape."""
    img = Image.new("RGB", (size, size), background)
    d = ImageDraw.Draw(img)
    light = (0.3, 0.85, 0.4)
    order = []
    for f in faces:
        a, b, c = (verts[i] for i in f[:3])
        n = face_normal(a[:3], b[:3], c[:3])
        cen = [(a[k] + b[k] + c[k]) / 3 for k in range(3)]
        col = classify(cen, n)
        shade = 0.8 + 0.25 * max(0.0, sum(n[k] * light[k] for k in range(3)))
        col = tuple(max(1, min(255, int(x * shade))) for x in col)
        uv = [(v[6] * (size - 1), v[7] * (size - 1)) for v in (a, b, c)]
        area = abs((uv[1][0] - uv[0][0]) * (uv[2][1] - uv[0][1]) - (uv[2][0] - uv[0][0]) * (uv[1][1] - uv[0][1]))
        order.append((area, uv, col))
    # Big triangles first, small details (lights) on top.
    for _, uv, col in sorted(order, key=lambda t: -t[0]):
        d.polygon(uv, fill=col, outline=col)
    return img


def near(p, q, r):
    return sum((p[k] - q[k]) ** 2 for k in range(3)) <= r * r


def z3_paint(cen, n):
    x, y, z = cen
    blue, glass, black = (34, 78, 168), (28, 34, 44), (18, 18, 20)
    if n[1] < -0.6:
        return black  # underside
    # Lights at Car_cfg [light_positions] 16 (BMW Z3).
    if near((abs(x), y, z), (6.46, 6.21, 21.12), 3.0):
        return (235, 235, 220)
    if near((abs(x), y, z), (5.98, 7.15, -21.51), 3.0):
        return (190, 20, 20)
    # Side windows, windscreen and rear window: the band below the roof.
    if 9.9 <= y <= 12.8 and -13.5 <= z <= 5.0 and n[1] < 0.9:
        return glass
    if y < 3.0 and abs(z) > 17:
        return (40, 40, 44)  # bumpers' lower edge
    return blue


def silver_paint(cen, n):
    x, y, z = cen
    silver, glass, black = (176, 180, 186), (28, 34, 44), (18, 18, 20)
    if n[1] < -0.6:
        return black
    if z > 21 and 4.5 < y < 8.5:
        return (230, 230, 215)
    if z < -21 and 5.5 < y < 9.5:
        return (170, 25, 25)
    if 9.5 <= y <= 13.5 and -10 <= z <= 6 and n[1] < 0.9:
        return glass
    return silver


def save_tga(img, path):
    """Uncompressed 24-bit TGA, bottom-up, like the game's own."""
    w, h = img.size
    hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 0)
    px = img.transpose(Image.FLIP_TOP_BOTTOM).tobytes("raw", "BGR")
    with open(path, "wb") as f:
        f.write(hdr + px)


# ------------------------------------------------------------------ rendering

class Mesh:
    def __init__(self, verts, faces, tex, offset=(0, 0, 0), mirror=False):
        self.verts, self.faces, self.tex = verts, faces, tex
        self.offset, self.mirror = offset, mirror


def render(meshes, size, yaw, pitch, background, ss=3, margin=0.08, fill=0.92):
    """Software render, supersampled ss times. Black texels are see-through
    (the game's colour key), and so is the pink (255, 0, 127) the sheets
    use inside the wheel arches."""
    W, H = size[0] * ss, size[1] * ss
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    light = (-0.45, 0.8, 0.4)

    def xform(p):
        x, y, z = p
        x, z = x * cy + z * sy, -x * sy + z * cy
        y, z = y * cp - z * sp, y * sp + z * cp
        return x, y, z

    tris = []
    for m in meshes:
        tex = m.tex.load() if m.tex else None
        tw, th = m.tex.size if m.tex else (1, 1)
        for f in m.faces:
            pts, uvs = [], []
            for i in f[:3]:
                v = m.verts[i]
                x = -v[0] if m.mirror else v[0]
                p = (x + m.offset[0], v[1] + m.offset[1], v[2] + m.offset[2])
                pts.append(p)
                uvs.append((v[6], v[7]))
            n = face_normal(*pts)
            if m.mirror:
                n = [-k for k in n]
            lam = 0.62 + 0.45 * abs(sum(n[k] * light[k] for k in range(3)))
            tris.append(([xform(p) for p in pts], uvs, tex, tw, th, lam))
    # Fit: perspective from a camera at distance D.
    allp = [p for t in tris for p in t[0]]
    rad = max(math.sqrt(p[0] ** 2 + p[1] ** 2 + p[2] ** 2) for p in allp)
    D = rad * 4.0

    def proj(p):
        k = D / (D - p[2])
        return p[0] * k, p[1] * k, p[2]

    tris = [([proj(p) for p in t[0]],) + t[1:] for t in tris]
    xs = [p[0] for t in tris for p in t[0]]
    ys = [p[1] for t in tris for p in t[0]]
    sc = min(W * fill / (max(xs) - min(xs)), H * (1 - 2 * margin) / (max(ys) - min(ys)))
    ox = W / 2 - (max(xs) + min(xs)) / 2 * sc
    oy = H / 2 + (max(ys) + min(ys)) / 2 * sc
    out = background.resize((W, H)) if isinstance(background, Image.Image) else Image.new("RGB", (W, H), background)
    P = out.load()
    zb = [[-1e9] * W for _ in range(H)]
    for pts, uvs, tex, tw, th, lam in tris:
        S = [(p[0] * sc + ox, oy - p[1] * sc, p[2]) for p in pts]
        A, B, C = S
        den = (B[1] - C[1]) * (A[0] - C[0]) + (C[0] - B[0]) * (A[1] - C[1])
        if abs(den) < 1e-9:
            continue
        x0, x1 = max(0, int(min(A[0], B[0], C[0]))), min(W - 1, int(max(A[0], B[0], C[0])) + 1)
        y0, y1 = max(0, int(min(A[1], B[1], C[1]))), min(H - 1, int(max(A[1], B[1], C[1])) + 1)
        for py in range(y0, y1 + 1):
            gy = py + 0.5
            row = zb[py]
            for px in range(x0, x1 + 1):
                gx = px + 0.5
                l0 = ((B[1] - C[1]) * (gx - C[0]) + (C[0] - B[0]) * (gy - C[1])) / den
                if l0 < 0:
                    continue
                l1 = ((C[1] - A[1]) * (gx - C[0]) + (A[0] - C[0]) * (gy - C[1])) / den
                l2 = 1 - l0 - l1
                if l1 < 0 or l2 < 0:
                    continue
                z = l0 * A[2] + l1 * B[2] + l2 * C[2]
                if z <= row[px]:
                    continue
                if tex:
                    u = l0 * uvs[0][0] + l1 * uvs[1][0] + l2 * uvs[2][0]
                    v = l0 * uvs[0][1] + l1 * uvs[1][1] + l2 * uvs[2][1]
                    c = tex[min(tw - 1, max(0, int(u * tw))), min(th - 1, max(0, int(v * th)))]
                    if c == (0, 0, 0) or c == (255, 0, 127):
                        continue
                else:
                    c = (128, 128, 128)
                row[px] = z
                P[px, py] = (min(255, int(c[0] * lam)), min(255, int(c[1] * lam)), min(255, int(c[2] * lam)))
    return out.resize(size, Image.LANCZOS)


def orange_background(w, h):
    """Like the CD's CAR_*.bmp: yellow top left into orange, soft rays."""
    img = Image.new("RGB", (w, h))
    P = img.load()
    for y in range(h):
        for x in range(w):
            t = min(1.0, math.hypot(x - w * 0.15, y - h * 0.1) / (w * 1.05))
            r = 255
            g = int(222 - (222 - 160) * t)
            b = int(30 * (1 - t))
            ang = math.atan2(y - h * 1.1, x - w * 0.25)
            if math.sin(ang * 14) > 0.6:
                g = min(255, g + 10)
            P[x, y] = (r, g, b)
    return img.filter(ImageFilter.GaussianBlur(1.2))


def wheel_meshes(wie, tex, row):
    """Four wheels at a Car_cfg wheel_positions row (front x,y,z rear x,y,z)."""
    v, f = wie
    fx, fy, fz, rx, ry, rz = row
    return [Mesh(v, f, tex, (fx, fy, fz)), Mesh(v, f, tex, (-fx, fy, fz), True),
            Mesh(v, f, tex, (rx, ry, rz)), Mesh(v, f, tex, (-rx, ry, rz), True)]


def wheel_row(car_cfg_text, cartype):
    sec = car_cfg_text.split("[wheel_positions]", 1)[1].split("[", 1)[0]
    for line in sec.splitlines():
        line = line.split(";", 1)[0]
        if "=" in line and line.split("=", 1)[0].strip() == str(cartype):
            return [float(x) for x in line.split("=", 1)[1].split(",")[:6]]
    return None


# ---------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game-dir", default=os.path.join(ROOT, "iso", "data"))
    ap.add_argument("--a2", default=os.path.join(HOME, "Projects/Games/A2 Racer IV/iso/Data"))
    ap.add_argument("--er", default=os.path.join(HOME, "Projects/Games/Europe Racer/iso/EuropeRacer/res"))
    ap.add_argument("--skip-menu", action="store_true", help="only the race textures")
    a = ap.parse_args()

    vr = Archive(a.game_dir)
    race_out = os.path.join(ROOT, "mods", "race", "local")
    menu_out = os.path.join(ROOT, "mods", "menu", "local")
    os.makedirs(race_out, exist_ok=True)
    os.makedirs(menu_out, exist_ok=True)

    z3 = load_d3d(vr.read("z3.d3d"))
    dl = load_d3d(vr.read("delorean.d3d"))

    # Paint: Z3 (no texture for its model on any CD).
    z3_sheet = paint_sheet(*z3, z3_paint)
    save_tga(z3_sheet, os.path.join(race_out, "bonus_z3.tga"))
    print("bonus_z3.tga: BMW-blauw, eigen blad in de UV-indeling van z3.d3d")

    # Paint: DeLorean, from the first CD that has its sheet.
    dl_sheet, source = None, None
    for label, d in (("A2 Racer IV", a.a2), ("Europe Racer", a.er)):
        try:
            arc = Archive(d)
        except (FileNotFoundError, OSError):
            continue
        if arc.has("delorean.d3d") and arc.read("delorean.d3d") != vr.read("delorean.d3d"):
            print("  %s: delorean.d3d is anders dan op de Vakantie Racer-CD, overgeslagen" % label)
            continue
        if arc.has("l1racer2.tga"):
            dl_sheet, source = arc.image("l1racer2.tga"), label
            break
    if dl_sheet is None:
        dl_sheet, source = paint_sheet(*dl, silver_paint, background=(170, 174, 180)), "effen zilver"
    save_tga(dl_sheet, os.path.join(race_out, "bonus_dl.tga"))
    print("bonus_dl.tga: %s (%dx%d)" % (source, dl_sheet.size[0], dl_sheet.size[1]))

    for league in range(3):
        with open(os.path.join(race_out, "L%d_anim.ini" % league), "w") as f:
            f.write("; Gemaakt door tools/make_mod_assets.py: de lak van de Z3 en de DeLorean.\r\n"
                    "[LEAGUE_BONUS]\r\n0=Z3.d3d,bonus_z3.pcx\r\n\r\n"
                    "[TOURNAMENT_BONUS]\r\n0=DeLorean.d3d,bonus_dl.pcx\r\n")

    if a.skip_menu:
        return

    # Menu pictures.
    car_cfg = open(find_nocase(a.game_dir, "Car_cfg.ini"), encoding="latin1").read()
    mod_cfg = open(os.path.join(ROOT, "mods", "race", "Car_cfg.ini"), encoding="latin1").read()
    cars01 = vr.image("cars01.tga")
    bg = orange_background(297, 227)
    jobs = [
        ("CAR_FER.bmp", "ferrari.d3d", vr.image("cars01.tga"), "ferr_wie.d3d", wheel_row(car_cfg, 5)),
        ("CAR_Z3.bmp", "z3.d3d", z3_sheet, "z3_wie.d3d", wheel_row(mod_cfg, 6)),
        ("CAR_DEL.bmp", "delorean.d3d", dl_sheet, "delo_wie.d3d", wheel_row(mod_cfg, 24)),
    ]
    for out, body, tex, wie, row in jobs:
        meshes = [Mesh(*load_d3d(vr.read(body)), tex)]
        if row:
            meshes += wheel_meshes(load_d3d(vr.read(wie)), cars01, row)
        img = render(meshes, (297, 227), yaw=50, pitch=16, background=bg, fill=0.86)
        img.save(os.path.join(menu_out, out))
        print(out)

    l1 = vr.image("l1racer1.tga")
    l2 = vr.image("l1racer2.tga")
    key = (255, 0, 255)  # the button picture's transparent colour
    trailers = [
        ("TRL_CAR.bmp", "trailer1.d3d", l1, "tra1wiel.d3d", wheel_row(car_cfg, 22)),
        ("TRL_HIP.bmp", "trailer2.d3d", l1, "tra1wiel.d3d", wheel_row(car_cfg, 23)),
        ("TRL_JET.bmp", "trailer3.d3d", l2, "tra3wiel.d3d", wheel_row(car_cfg, 21)),
    ]
    for out, body, tex, wie, row in trailers:
        meshes = [Mesh(*load_d3d(vr.read(body)), tex)]
        if row:
            meshes += wheel_meshes(load_d3d(vr.read(wie)), cars01, row)
        img = render(meshes, (84, 50), yaw=60, pitch=14, background=key, ss=4, margin=0.04, fill=0.96)
        # Keep the key colour exact around the edges (no blended halo).
        px = img.load()
        for y in range(img.size[1]):
            for x in range(img.size[0]):
                r, g, b = px[x, y]
                if r > 200 and g < 90 and b > 200:
                    px[x, y] = key
        img.save(os.path.join(menu_out, out))
        print(out)


if __name__ == "__main__":
    sys.exit(main())
