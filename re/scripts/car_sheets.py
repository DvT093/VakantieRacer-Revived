#!/usr/bin/env python3
"""Contact sheets of every car in a Davilex RED game (Vakantie Racer,
A2 Racer IV, Europe Racer): each car rendered three-quarter front with
tools/make_mod_assets.py's PIL renderer, body + four wheels, textured.

  python3 -I re/scripts/car_sheets.py vr|a2|er [--only NAME] [--out PNG]

Reads the CDs (read only); writes only the PNG (build/shots is gitignored).
"""
import argparse
import bisect
import io
import os
import struct
import sys
import zlib

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from make_mod_assets import Archive, Mesh, find_nocase, render  # noqa: E402

HOME = os.path.expanduser("~")
GAMES = {
    "vr": dict(title="Vakantie Racer", data=os.path.join(ROOT, "iso/data"),
               archives=["", ], cfg="Car_cfg.ini",
               anims=["L0_anim.ini", "gen_anim.ini", "Car_cfg.ini"],
               tables=["league0", "traffic"]),
    "a2": dict(title="A2 Racer IV", data=os.path.join(HOME, "Projects/Games/A2 Racer IV/iso/Data"),
               archives=[""], cfg="CAR_CFG.INI",
               anims=["L0_anim.ini", "gen_anim.ini", "e06_anim.ini", "CAR_CFG.INI"],
               extra={"damage": "damage.ini"},
               tables=["league0", "traffic"]),
    "er": dict(title="Europe Racer", data=os.path.join(HOME, "Projects/Games/Europe Racer/iso/EuropeRacer"),
               archives=["res", "Game"], cfg="Game/Ini/Cars.ini",
               anims=["Game/Ini/Cars.ini", "Game/Ini/gen_anim.ini"],
               tables=["cars"], extra={"damage": "Game/Ini/damage.ini"}),
}


class Arc(Archive):
    """Archive that also unpacks Europe Racer's zlib'd .d3d (u32 size + zlib)."""

    def read(self, name):
        b = super().read(name)
        if name.lower().endswith(".d3d") and len(b) > 6 and b[4] == 0x78:
            try:
                return zlib.decompress(b[4:])
            except zlib.error:
                pass
        return b


def mip_crop(img):
    """A2 Racer IV / Europe Racer sheets carry their mip chain to the right
    (768x512 = 512x512 + 256, 384x256 = 256 + 128): UVs cover the square."""
    w, h = img.size
    if w * 2 == h * 3:
        img = img.crop((0, 0, h, h))
    return key16(img)


def key16(img):
    """The colour key is compared in 16-bit (565) colour: snap texels that
    fall in the same 565 cell as (255,0,127) or black onto the exact key."""
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if r >= 248 and g < 4 and 120 <= b < 128:
                px[x, y] = (255, 0, 127)
            elif r < 8 and g < 4 and b < 8:
                px[x, y] = (0, 0, 0)
    return img


def parse_ini(path):
    secs, cur = {}, None
    for line in open(path, encoding="latin1"):
        line = line.split(";", 1)[0].strip()
        if line.startswith("[") and "]" in line:
            cur = secs.setdefault(line[1:line.index("]")].strip().lower(), [])
        elif "=" in line and cur is not None:
            k, v = line.split("=", 1)
            cur.append((k.strip(), v.strip()))
    return secs


def load_d3d(b):
    nv, nf = struct.unpack("<II", b[4:12])
    o, V, F = 12, [], []
    for _ in range(nv):
        V.append(struct.unpack("<12f", b[o:o + 48]))
        o += 49
    for _ in range(nf):
        F.append(struct.unpack("<4H", b[o:o + 8]))
        o += 9
    if o != len(b):
        raise ValueError("size %d != %d" % (o, len(b)))
    return V, F


class Game:
    def __init__(self, key):
        g = GAMES[key]
        self.g = g
        self.arcs = []
        for sub in g["archives"]:
            d = os.path.join(g["data"], sub)
            for ind in ("rcs.ind", "rc.ind", "rcslang.ind"):
                p = find_nocase(d, ind)
                if not p:
                    continue
                base = os.path.splitext(os.path.basename(p))[0]
                try:
                    a = Arc.__new__(Arc)
                    _init_named(a, d, base)
                    self.arcs.append(a)
                except FileNotFoundError:
                    pass
        self.secs = {}
        for f in g["anims"]:
            p = os.path.join(g["data"], f) if os.path.exists(os.path.join(g["data"], f)) else \
                find_nocase(os.path.join(g["data"], os.path.dirname(f)), os.path.basename(f))
            for k, v in parse_ini(p).items():
                self.secs.setdefault(k, v)
        # Europe Racer lists part anims inline: <i>=lin_a00_anim,LIN_A00.d3d,lincoln.pcx
        for sec in list(self.secs.values()):
            for k, v in sec:
                f = [x.strip() for x in v.split(",")]
                if len(f) == 3 and f[0].lower().endswith("_anim") and f[1].lower().endswith(".d3d"):
                    self.secs.setdefault(f[0].lower(), [("0", f[1] + "," + f[2])])
        self.cfgs = {}
        for k, f in g.get("extra", {}).items():
            p = find_nocase(os.path.join(g["data"], os.path.dirname(f)), os.path.basename(f))
            if p:
                self.cfgs[k] = parse_ini(p)
        self.cfg = parse_ini(os.path.join(g["data"], g["cfg"]) if os.path.exists(os.path.join(g["data"], g["cfg"]))
                             else find_nocase(g["data"], g["cfg"]))

    def has(self, name):
        return any(a.has(name) for a in self.arcs)

    def read(self, name):
        for a in self.arcs:
            if a.has(name):
                return a.read(name)
        raise KeyError(name)

    def texture(self, name):
        base = os.path.splitext(name)[0]
        for ext in (".tga", ".pcx"):
            if self.has(base + ext):
                return mip_crop(Image.open(io.BytesIO(self.read(base + ext))).convert("RGB")), base + ext
        return None, None

    def wheel_row(self, cartype):
        for k, v in self.cfg.get("wheel_positions", []):
            if k == str(cartype):
                return [float(x) for x in v.split(",")[:6]]
        return None

    def rows(self):
        out, seen = [], set()
        for t in self.g["tables"]:
            for k, v in self.cfg.get(t, []):
                if not k.isdigit():
                    continue
                f = [x.strip() for x in v.split(",")]
                ref, anim, cartype = f[0], f[1], int(f[5])
                if anim.lower() in seen:
                    continue
                seen.add(anim.lower())
                out.append(dict(table=t, idx=int(k), ref=ref, anim=anim, cartype=cartype, driver=f[2],
                                dam=f[8] if len(f) > 8 else None))
        return out

    def parts(self, anim):
        sec = dict(self.secs.get(anim.lower(), []))
        get = lambda i: [x.strip() for x in sec[str(i)].split(",")] if str(i) in sec else None
        return get(0), get(1)


def _init_named(self, d, base):
    """Archive.__init__ for <base>.ind/<base>.img (RC, RcsLang too)."""
    ind = find_nocase(d, base + ".ind")
    img = find_nocase(d, base + ".img")
    if not ind or not img:
        raise FileNotFoundError(d)
    data = open(ind, "rb").read()
    n = struct.unpack("<H", data[:2])[0]
    if 2 + 24 * n != len(data):
        raise FileNotFoundError(ind)
    self.entries, offs = {}, []
    for i in range(n):
        e = data[2 + 24 * i: 26 + 24 * i]
        name = e[:20].split(b"\0")[0].decode("latin1").lower()
        off = struct.unpack("<I", e[20:24])[0]
        self.entries.setdefault(name, off)
        offs.append(off)
    self.f = open(img, "rb")
    size = self.f.seek(0, 2)
    self.offs = sorted(set(offs)) + [size]


def damage_parts(game, dam):
    """A2 Racer IV: the body d3d is only the hull; panels, bumpers and the
    driver are damage objects (damage.ini), each a d3d around its pivot."""
    if not dam or "damage" not in game.cfgs:
        return []
    d = game.cfgs["damage"]
    models = {v.split(",")[0].strip().lower(): v.split(",")[5].strip().lower() for k, v in d.get("damage models", []) if k.isdigit()}
    cars = {v.split(",")[0].strip().lower(): [x.strip().lower() for x in v.split(",")[3:] if x.strip()]
            for k, v in d.get("damageable cars", []) if k.isdigit()}
    meshes = cars.get(models.get(dam.lower(), ""), [])
    if not meshes:
        return []
    out = []
    for k, v in d.get("damageable car objects", []):
        if not k.isdigit():
            continue
        f = [x.strip() for x in v.split(",")]
        if f[0].lower() == meshes[0]:
            sec = dict(game.secs.get(f[1].lower(), []))
            if "0" in sec:
                m, t = [x.strip() for x in sec["0"].split(",")]
                out.append((f[1], m, t, (float(f[2]), float(f[3]), float(f[4]))))
    return out


def car_meshes(game, body, wheel, cartype, notes, override=None, wrow=None, dam=None):
    meshes = []
    for name, m, t, off in damage_parts(game, dam):
        if "_bot" in m.lower():
            continue
        tx, _ = game.texture(t)
        if game.has(m):
            PV, PF = load_d3d(game.read(m))
            meshes.append(Mesh(PV, PF, tx, off))
    if override:
        tex, texname = mip_crop(Image.open(override).convert("RGB")), os.path.basename(override)
    else:
        tex, texname = game.texture(body[1])
    if tex is None:
        notes.append("textuur %s ontbreekt" % body[1])
    V, F = load_d3d(game.read(body[0]))
    meshes.append(Mesh(V, F, tex))
    ymin = min(v[1] for v in V)
    if wheel and wheel[0].lower() not in ("dummy.d3d", "dummy2.d3d") and game.has(wheel[0]):
        wt, _ = game.texture(wheel[1])
        if wt is not None:  # wheels are drawn opaque: black is tyre, not the colour key
            wt = wt.point(lambda c: max(c, 6))
        WV, WF = load_d3d(game.read(wheel[0]))
        row = wrow or game.wheel_row(cartype)
        if row:
            fx, fy, fz, rx, ry, rz = row
            fx, rx = abs(fx), abs(rx)
            if fy == 0 and ry == 0:  # A2/ER: height from the suspension, not the table
                fy = ry = max(v[1] for v in WV)
                notes.append("wielhoogte geschat (y=0 in tabel)") if False else None
            if fx > 40 or rx > 40:  # typo rows (Lamborghini 70.93)
                rx = fx = min(fx, rx)
            meshes += [Mesh(WV, WF, wt, (fx, fy, fz)), Mesh(WV, WF, wt, (-fx, fy, fz), True),
                       Mesh(WV, WF, wt, (rx, ry, rz)), Mesh(WV, WF, wt, (-rx, ry, rz), True)]
        else:
            notes.append("geen wiel-rij %d" % cartype)
    return meshes, texname, ymin


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("game", choices=GAMES)
    ap.add_argument("--only")
    ap.add_argument("--out")
    ap.add_argument("--names", help="ini-like file anim=label overrides")
    ap.add_argument("--extra", action="append", default=[], help="label:body.d3d:tex:wheel.d3d:wtex:cartype")
    ap.add_argument("--skip", action="append", default=[])
    ap.add_argument("--cell", default="400x300")
    ap.add_argument("--tex", action="append", default=[], help="ANIM=path.tga: other paint (marked on the sheet)")
    ap.add_argument("--wheels", action="append", default=[], help="ANIM=fx,fy,fz,rx,ry,rz")
    ap.add_argument("--mark", action="append", default=[], help="ANIM=note shown in red")
    a = ap.parse_args()
    game = Game(a.game)
    cw, ch = (int(x) for x in a.cell.split("x"))
    names = {}
    if a.names:
        for line in open(a.names, encoding="utf-8"):
            if "=" in line and not line.startswith("#"):
                k, v = line.rstrip("\n").split("=", 1)
                names[k.strip().lower()] = v.strip()
    jobs = []
    for r in game.rows():
        if r["anim"].lower() in [s.lower() for s in a.skip]:
            continue
        body, wheel = game.parts(r["anim"])
        if not body:
            print("skip %s: geen sectie %s" % (r["ref"], r["anim"]))
            continue
        jobs.append((r["anim"], r["ref"], body, wheel, r["cartype"], r["dam"]))
    for e in a.extra:
        label, b, bt, w, wt, ct = e.split(":")
        jobs.append((label, label, [b, bt], [w, wt] if w else None, int(ct), None))
    if a.only:
        jobs = [j for j in jobs if a.only.lower() in (j[0].lower(), j[1].lower())]

    bg = (206, 208, 212)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 17)
        small = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 13)
    except OSError:
        font = small = ImageFont.load_default()
    cells, report = [], []
    texo = dict(x.split("=", 1) for x in a.tex)
    texo = {k.lower(): v for k, v in texo.items()}
    wrows = {k.lower(): [float(x) for x in v.split(",")] for k, v in (x.split("=", 1) for x in a.wheels)}
    marks = {k.lower(): v for k, v in (x.split("=", 1) for x in a.mark)}
    for anim, ref, body, wheel, cartype, dam in jobs:
        notes = []
        if anim.lower() in marks:
            notes.append(marks[anim.lower()])
        try:
            meshes, texname, _ = car_meshes(game, body, wheel, cartype, notes, texo.get(anim.lower()), wrows.get(anim.lower()), dam)
            img = render(meshes, (cw, ch - 44), yaw=50, pitch=16, background=bg, ss=2, margin=0.06, fill=0.9)
        except Exception as ex:  # noqa: BLE001
            img, texname = Image.new("RGB", (cw, ch - 44), (90, 40, 40)), None
            notes.append("FOUT: %s" % ex)
        cell = Image.new("RGB", (cw, ch), (238, 239, 241))
        cell.paste(img, (0, 0))
        d = ImageDraw.Draw(cell)
        label = names.get(anim.lower(), ref)
        d.text((8, ch - 42), label, fill=(20, 20, 24), font=font)
        sub = "%s · %s%s" % (body[0], body[1], " · " + wheel[0] if wheel and "dummy" not in wheel[0].lower() else "")
        if notes:
            sub = "! " + "; ".join(notes)
        d.text((8, ch - 20), sub[:60], fill=(160, 30, 30) if notes else (80, 80, 90), font=small)
        cells.append(cell)
        report.append((label, anim, ref, body[0], body[1], texname, wheel[0] if wheel else "", cartype, notes))
        print("%-28s %-22s %-14s %-14s %s" % (label, anim, body[0], body[1], "; ".join(notes)), flush=True)
    cols = 5 if len(cells) > 12 else 4
    rows = (len(cells) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cw + (cols + 1) * 6, rows * ch + (rows + 1) * 6 + 40), (60, 62, 68))
    ImageDraw.Draw(sheet).text((10, 10), "%s - %d auto's" % (game.g["title"], len(cells)), fill="white", font=font)
    for i, c in enumerate(cells):
        sheet.paste(c, (6 + (i % cols) * (cw + 6), 46 + (i // cols) * (ch + 6)))
    out = a.out or os.path.join(ROOT, "build/shots", "autos_%s.png" % {"vr": "vakantieracer", "a2": "a2r4", "er": "europeracer"}[a.game])
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sheet.save(out)
    print("->", out)


if __name__ == "__main__":
    main()
