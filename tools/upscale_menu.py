#!/usr/bin/env python3
"""Upscales every menu picture 4x with Real-ESRGAN, for the upgraded menu.

Reads your own CD (and the mod pictures made from it), writes PNGs to
mods/menu/local/hd/ (in .gitignore, like the rest of mods/menu/local):

  <name>.png        the picture, 4x (realesrgan-x4plus)
  <name>.alpha.png  the same with its transparent colour as real alpha:
                    the keyed pixels get their neighbours' colour first (so
                    no magenta fringes), the picture and the alpha mask are
                    upscaled separately and put together

Names are what the menu looks for (src/menu/engine/hd.h): a bitmap file
without extension ("mainscre"), a picture inside a form
("TfrmMainScreen.imgLogo"), a button state ("defBtn.UpLink"). Sources:
  iso/data/MenuData/*.bmp, mods/menu/*.bmp, mods/menu/local/*.bmp
  the forms in Menu.exe (vracer-menu --dump-pictures)
Button states get alpha on the background around the button (the colour
of the bottom-left pixel, as far as it reaches from the edge), so a
button looks right on any background.

Only what changed is upscaled again (cache in build/menu_hd/cache, by the
hash of the prepared picture). Real-ESRGAN (realesrgan-ncnn-vulkan
v0.2.5.0) is downloaded to build/tools/ when it is not there.

  tools/upscale_menu.py [--game-dir iso/data] [--menu BIN] [--force]

Needs Pillow; no numpy. Runs on the GPU (Metal via MoltenVK): one heavy
job, do not run it next to a build.
"""
import argparse
import hashlib
import io
import os
import shutil
import stat
import subprocess
import sys
import urllib.request
import re
import zipfile

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageMath

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python3 -I leaves it out
import menu_lamps  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ESRGAN_URL = ("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/"
              "realesrgan-ncnn-vulkan-20220424-macos.zip")
ESRGAN_DIR = os.path.join(ROOT, "build", "tools", "realesrgan-ncnn-vulkan")
MODEL = "realesrgan-x4plus"
WORK = os.path.join(ROOT, "build", "menu_hd")
OUT = os.path.join(ROOT, "mods", "menu", "local", "hd")
SCALE = 4
# Lamp pictures: drawn round from scratch (menu_lamps.py), not upscaled.
LAMP_SPRITES = re.compile(r"^(L_[a-z]+|.*\.imgBulb|.*\.Light(On|Off)|defLight\..*)$", re.I)


def log(*a):
    print("upscale_menu:", *a, flush=True)


def esrgan_binary():
    exe = os.path.join(ESRGAN_DIR, "realesrgan-ncnn-vulkan")
    if os.path.exists(exe) and os.path.isdir(os.path.join(ESRGAN_DIR, "models")):
        return exe
    log("Real-ESRGAN downloaden:", ESRGAN_URL)
    data = urllib.request.urlopen(ESRGAN_URL, timeout=120).read()
    tmp = ESRGAN_DIR + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for m in z.infolist():
            # Flat: the binary and models/ at the top, whatever the zip's folders.
            parts = m.filename.split("/")
            if "models" in parts:
                rel = "/".join(parts[parts.index("models"):])
            else:
                rel = parts[-1]
            if not rel or m.is_dir() or ".." in rel.split("/"):
                continue
            dst = os.path.join(tmp, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "wb") as f:
                f.write(z.read(m))
    shutil.rmtree(ESRGAN_DIR, ignore_errors=True)
    os.rename(tmp, ESRGAN_DIR)
    os.chmod(exe, os.stat(exe).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    subprocess.run(["xattr", "-dr", "com.apple.quarantine", ESRGAN_DIR], check=False)
    return exe


def find_nocase(d, name):
    try:
        for e in os.listdir(d):
            if e.lower() == name.lower():
                return os.path.join(d, e)
    except FileNotFoundError:
        pass
    return None


# ------------------------------------------------------------ preparing

def key_mask(img, key, edge):
    """L mask: 0 where the picture is transparent, 255 where it is not."""
    w, h = img.size
    if edge:
        # Flood fill from every border pixel of the key colour, with a
        # marker colour the picture does not use.
        used = set(c for _, c in (img.getcolors(1 << 24) or []))
        marker = next(c for c in ((1, 2, 3), (3, 2, 1), (0, 1, 2), (2, 1, 0)) if c not in used)
        m = img.copy()
        px = m.load()
        border = [(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)] + \
                 [(0, y) for y in range(h)] + [(w - 1, y) for y in range(h)]
        for p in border:
            if px[p] == key:
                ImageDraw.floodfill(m, p, marker, thresh=0)
        r, g, b = m.split()
        is_m = ImageMath.lambda_eval(
            lambda a: a["convert"]((a["r"] == marker[0]) & (a["g"] == marker[1]) & (a["b"] == marker[2]), "L"),
            r=r, g=g, b=b)
    else:
        r, g, b = img.split()
        is_m = ImageMath.lambda_eval(
            lambda a: a["convert"]((a["r"] == key[0]) & (a["g"] == key[1]) & (a["b"] == key[2]), "L"),
            r=r, g=g, b=b)
    return is_m.point(lambda v: 0 if v else 255)


def soft_edges(img, mask, key):
    """The opaque pixels at the edge are a blend of the figure and the key
    colour (the artist anti-aliased against it). Splits them: alpha from how
    far they are from the key towards the figure's colour next to them, and
    the key colour taken out of their colour. Returns (img, mask)."""
    w, h = img.size
    px, mk = img.load(), mask.load()
    out, om = img.copy(), mask.copy()
    opx, omk = out.load(), om.load()
    kr, kg, kb = key
    def keyed(x, y):
        return 0 <= x < w and 0 <= y < h and mk[x, y] == 0
    rim = set()
    for y in range(h):
        for x in range(w):
            if mk[x, y] and (keyed(x - 1, y) or keyed(x + 1, y) or keyed(x, y - 1) or keyed(x, y + 1)):
                rim.add((x, y))
    for (x, y) in rim:
        # The figure's own colour: opaque neighbours that are not rim.
        n, fr, fg, fb = 0, 0, 0, 0
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and 0 <= yy < h and mk[xx, yy] and (xx, yy) not in rim:
                    c = px[xx, yy]
                    fr, fg, fb, n = fr + c[0], fg + c[1], fb + c[2], n + 1
        if not n:
            continue
        f = (fr / n - kr, fg / n - kg, fb / n - kb)
        ff = f[0] * f[0] + f[1] * f[1] + f[2] * f[2]
        if ff < 40 * 40:
            continue  # figure close to the key colour: leave it
        c = px[x, y]
        d = (c[0] - kr, c[1] - kg, c[2] - kb)
        a = max(0.0, min(1.0, (d[0] * f[0] + d[1] * f[1] + d[2] * f[2]) / ff))
        if a >= 0.95:
            continue
        if a < 0.08:
            omk[x, y] = 0
            continue
        omk[x, y] = int(a * 255)
        opx[x, y] = tuple(max(0, min(255, int(k + dd / a))) for k, dd in zip(key, d))
    return out, om


def fill_keyed(img, mask):
    """The keyed pixels get the colour of the nearest opaque ones (pull-push:
    average down a pyramid, fill holes from the coarser level)."""
    if mask.getextrema() == (255, 255):
        return img
    levels = []
    rgb, a = Image.composite(img, Image.new("RGB", img.size), mask), mask
    while True:
        levels.append((rgb, a))
        if rgb.size == (1, 1):
            break
        nw, nh = max(1, (rgb.size[0] + 1) // 2), max(1, (rgb.size[1] + 1) // 2)
        rgb = rgb.resize((nw, nh), Image.BOX)
        a = a.resize((nw, nh), Image.BOX)
    filled = None
    for rgb, a in reversed(levels):
        # Un-premultiply: colour = rgb * 255 / a where a > 0.
        chans = [ImageMath.lambda_eval(lambda e: e["convert"](e["min"](e["c"] * 255 / e["max"](e["a"], 1), 255), "L"),
                                       c=c, a=a) for c in rgb.split()]
        col = Image.merge("RGB", chans)
        if filled is None:
            filled = col
        else:
            up = filled.resize(rgb.size, Image.BILINEAR)
            filled = Image.composite(col, up, a.point(lambda v: 255 if v else 0))
    return Image.composite(img, filled, mask.point(lambda v: 255 if v else 0))


def parse_key(spec, img):
    w, h = img.size
    if spec in ("bl", "edge", "auto"):
        return img.getpixel((0, h - 1))
    return (int(spec[0:2], 16), int(spec[2:4], 16), int(spec[4:6], 16))


# ------------------------------------------------------------ sources

def sources(game_dir, menu_bin):
    """name -> (path, key spec); later sources override earlier ones."""
    items = {}
    menudata = find_nocase(game_dir, "MenuData")
    dirs = [menudata, os.path.join(ROOT, "mods", "menu"), os.path.join(ROOT, "mods", "menu", "local")]
    for d in dirs:
        if not d or not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.lower().endswith(".bmp"):
                name = f[:-4]
                # A button state replaced by a mod: same rules as in the form.
                spec = "edge" if any(name.endswith("." + l) for l in
                                     ("UpLink", "DownLink", "FocusedLink", "HoveredLink", "DisabledLink")) else "auto"
                items[name.lower()] = (name, os.path.join(d, f), spec)
    forms = os.path.join(WORK, "forms")
    shutil.rmtree(forms, ignore_errors=True)
    exe = find_nocase(menudata, "Menu.exe") if menudata else None
    if exe and os.access(menu_bin, os.X_OK):
        subprocess.run([menu_bin, "--game-dir", game_dir, "--dump-pictures", forms], check=True)
        with open(os.path.join(forms, "pictures.tsv")) as f:
            for line in f:
                if line.startswith("#") or not line.strip():
                    continue
                name, _, _, key = line.rstrip("\n").split("\t")
                if name.lower() not in items:  # a mod file with that name wins
                    items[name.lower()] = (name, os.path.join(forms, name + ".bmp"), key)
    else:
        log("let op: geen", menu_bin, "(bouw vracer-menu): de plaatjes in de formulieren worden overgeslagen")
    return list(items.values())


def wants_alpha(img, spec):
    if spec == "-":
        return False
    if spec == "edge":
        # Tiny controls (check box, radio, slider needle) sit on their own colour.
        return min(img.size) > 20
    if spec != "auto":
        return True
    # Bitmap files: whole backgrounds (a screen, a popup) are never keyed;
    # a picture with hardly any pixels of its corner colour does not need it.
    w, h = img.size
    if (w, h) == (800, 600) or w >= 456:
        return False
    key = parse_key("bl", img)
    n = sum(c for c, col in (img.getcolors(1 << 24) or []) if col == key)
    return n >= 0.005 * w * h


# ------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", default=os.path.join(ROOT, "iso", "data"))
    ap.add_argument("--menu", default=os.path.join(ROOT, "out", "build", "release", "vracer-menu"))
    ap.add_argument("--force", action="store_true", help="alles opnieuw upscalen")
    args = ap.parse_args()
    if not os.path.isdir(args.game_dir):
        log("geen CD-data in", args.game_dir)
        return 1
    cache = os.path.join(WORK, "cache")
    os.makedirs(cache, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)

    # Prepare every input; jobs = the pictures Real-ESRGAN has to make.
    plan = []   # (name, rgb hash, mask hash or None, size)
    jobs = {}   # hash -> prepared PIL image
    def job(img):
        buf = io.BytesIO()
        img.save(buf, "PNG")
        h = hashlib.sha1(MODEL.encode() + buf.getvalue()).hexdigest()
        if args.force or not os.path.exists(os.path.join(cache, h + ".png")):
            jobs[h] = img
        return h
    sources_by_name = {}
    for name, path, spec in sources(args.game_dir, args.menu):
        sources_by_name[name] = path
        try:
            img = Image.open(path).convert("RGB")
        except Exception as e:  # noqa: BLE001
            log("overgeslagen", path, e)
            continue
        if LAMP_SPRITES.match(name):
            plan.append((name, ("lamp", img), "lamp" if wants_alpha(img, spec) or spec == "edge" else None, img.size))
            continue
        plain = job(img)
        alpha = None
        if wants_alpha(img, spec):
            key = parse_key(spec, img)
            mask = key_mask(img, key, spec == "edge")
            if mask.getextrema()[0] < 255:
                rgb, mask = soft_edges(img, mask, key)
                # The model gets the shape (any coverage) and makes its edge
                # crisp; the soft edge values are scaled smoothly (compose).
                hard = mask.point(lambda v: 255 if v else 0)
                alpha = (job(fill_keyed(rgb, mask)), job(Image.merge("RGB", (hard, hard, hard))), mask)
        plan.append((name, plain, alpha, img.size))

    if jobs:
        exe = esrgan_binary()
        tin, tout = os.path.join(WORK, "in"), os.path.join(WORK, "out")
        for d in (tin, tout):
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
        for h, img in jobs.items():
            img.save(os.path.join(tin, h + ".png"))
        log(f"{len(jobs)} plaatjes upscalen met {MODEL} (een paar minuten)")
        r = subprocess.run([exe, "-i", tin, "-o", tout, "-n", MODEL, "-s", str(SCALE), "-f", "png", "-m", "models"],
                           cwd=ESRGAN_DIR, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if r.returncode:
            log("Real-ESRGAN faalde:", r.stderr.strip().splitlines()[-3:])
            return 1
        for h in jobs:
            src = os.path.join(tout, h + ".png")
            if not os.path.exists(src):
                log("Real-ESRGAN maakte niets van", h)
                continue
            shutil.move(src, os.path.join(cache, h + ".png"))
        shutil.rmtree(tin, ignore_errors=True)
        shutil.rmtree(tout, ignore_errors=True)
    else:
        log("alles al upscaled")

    # Outputs from the cache (written only when they changed).
    wanted = set()
    def put(fname, img_or_path):
        dst = os.path.join(OUT, fname)
        wanted.add(fname.lower())
        if isinstance(img_or_path, str):
            if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(img_or_path):
                return
            shutil.copyfile(img_or_path, dst)
        else:
            img_or_path.save(dst)
    for name, plain, alpha, size in plan:
        if isinstance(plain, tuple):  # a lamp picture
            img = plain[1]
            # With alpha around the lamp in both: the rail shows through.
            put(name + ".png", menu_lamps.lamp_sprite(img, SCALE, True))
            if alpha:
                put(name + ".alpha.png", menu_lamps.lamp_sprite(img, SCALE, True))
            continue
        p = os.path.join(cache, plain + ".png")
        # Screens: the lamps baked into them drawn round over the upscale.
        lamps = menu_lamps.find_lamps(Image.open(sources_by_name[name]).convert("RGB")) if size[0] >= 400 else []
        if lamps:
            hd = Image.open(p).convert("RGB")
            for cx, cy, r, kind, colour in lamps:
                menu_lamps.paint_lamp(hd, cx * SCALE, cy * SCALE, r * SCALE, kind, colour)
            put(name + ".png", hd)
            continue
        if not os.path.exists(p):
            continue
        put(name + ".png", p)
        if alpha:
            rgb_p, mask_p = (os.path.join(cache, h + ".png") for h in alpha[:2])
            if not (os.path.exists(rgb_p) and os.path.exists(mask_p)):
                continue
            dst = os.path.join(OUT, name + ".alpha.png")
            if not args.force and os.path.exists(dst) and \
                    os.path.getmtime(dst) >= max(os.path.getmtime(rgb_p), os.path.getmtime(mask_p), os.path.getmtime(__file__)):
                wanted.add((name + ".alpha.png").lower())
                continue
            rgb = Image.open(rgb_p).convert("RGB")
            if min(size) < 16:
                # Tiny (status dots): the model makes the few pixels square;
                # a smooth resize keeps them round.
                m = alpha[2].resize(rgb.size, Image.BICUBIC)
            else:
                hard = Image.open(mask_p).convert("L").resize(rgb.size, Image.BILINEAR)
                # Clean edges: the model's grey ramps steeper, no haze.
                hard = hard.point(lambda v: 0 if v < 24 else 255 if v > 231 else int((v - 24) * 255 / 207))
                # Soft edge values (shadows, anti-aliasing), spread outwards
                # first so the smooth resize does not eat the shape's edge.
                soft = alpha[2]
                soft = Image.composite(soft, soft.filter(ImageFilter.MaxFilter(3)), soft.point(lambda v: 255 if v else 0))
                m = ImageChops.multiply(hard, soft.resize(rgb.size, Image.BICUBIC))
            rgba = rgb.copy()
            rgba.putalpha(m)
            put(name + ".alpha.png", rgba)
    for f in os.listdir(OUT):
        if f.lower().endswith(".png") and f.lower() not in wanted:
            os.remove(os.path.join(OUT, f))
    log(f"{len(plan)} plaatjes in {os.path.relpath(OUT, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
