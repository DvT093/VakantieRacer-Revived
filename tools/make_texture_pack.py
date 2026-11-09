#!/usr/bin/env python3
"""HD-texturepakket voor de race, uit je eigen CD (RED-engine, generiek).

Elke 24-bit .tga uit rcs.IMG wordt 4x zo groot gemaakt met Real-ESRGAN
(model 4xNomos8kSC) en als <hash>.png in build/texture_pack/ gezet. SwARc
herkent een texture aan zijn pixels (zie SwARc texture_pack.h) en tekent
dan de HD-versie, zolang Upgraded aan staat (F9).

De hash is FNV-1a 64 over zeven u32 (breedte, hoogte, 16 bits, maskers
R5G6B5) plus de pixels zoals de game ze in de texture zet: 24-bit afgekapt
naar R5G6B5, de colour key 255,0,127 wordt 0x0000 en echt zwart 0x0800
(zodat het geen key is). Nagemeten tegen SwARc's dump (VRACER_TEX_DUMP).

Voorbewerking, per texture:
- key: 255,0,127 plus de bijna-magenta rand die ertegenaan ligt (van het
  verkleinen bij de maker; het origineel toont die roze) wordt doorzichtig;
- doorzichtige texels krijgen de kleur van hun buren (anders roze randen);
- 16 px rand: de overkant (wrap) voor bladen zonder key, anders de rand
  herhaald; tools/texture_pack.ini kan dat per texture omdraaien;
- 4x met Real-ESRGAN, rand eraf; de alpha apart, als zwart-wit plaatje ook
  door Real-ESRGAN (natuurlijker bladranden dan Lanczos), drempel 0,5.
Uitzonderingen (tools/texture_pack.ini): hele textures overslaan, of
stukken (kleine tekst, gezichten) origineel houden (zacht vergroot, zoals
de bilineaire filter van het spel ze toont).

Alleen wat ontbreekt of veranderd is wordt opnieuw gemaakt. Real-ESRGAN en
het model haalt het script zelf op in build/tools/ (sha256 gecontroleerd).

Gebruik: python3 -I tools/make_texture_pack.py [--only naam,naam] [--force]
"""

import argparse
import bisect
import configparser
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

from PIL import Image, ImageChops, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = "2"  # verhogen als de verwerking verandert: alles opnieuw

ESRGAN_ZIP = ("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/"
              "realesrgan-ncnn-vulkan-20220424-macos.zip",
              "e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a")
MODEL = "4xNomos8kSC"
MODEL_FILES = {
    MODEL + ".param": ("https://raw.githubusercontent.com/upscayl/custom-models/main/models/4xNomos8kSC.param",
                       "4576ed5c2fc5fa250d3c3d585ef02248f26abdfc1867088078f501fe71e5d61e"),
    MODEL + ".bin": ("https://raw.githubusercontent.com/upscayl/custom-models/main/models/4xNomos8kSC.bin",
                     "da16e3880d87b177b7c6b659bbd880f8a101b868eb9ebc08d69eaa6d3edc4517"),
}
SCALE = 4
PAD = 16
KEY = (255, 0, 127)


def find_nocase(d, name):
    for f in os.listdir(d):
        if f.lower() == name.lower():
            return os.path.join(d, f)
    return None


class Archive:
    """RED rcs.ind/rcs.IMG: u16 aantal, dan naam[20] + u32 offset."""

    def __init__(self, d):
        ind = open(find_nocase(d, "rcs.ind"), "rb").read()
        self.img = open(find_nocase(d, "rcs.img"), "rb").read()
        n = struct.unpack("<H", ind[:2])[0]
        self.entries = []
        for i in range(n):
            e = ind[2 + 24 * i:26 + 24 * i]
            self.entries.append((e[:20].split(b"\0")[0].decode("latin1"), struct.unpack("<I", e[20:24])[0]))
        self.offs = sorted({o for _, o in self.entries} | {len(self.img)})

    def data(self, off):
        return self.img[off:self.offs[bisect.bisect_right(self.offs, off)]]


def decode_tga(d):
    """24-bit ongecomprimeerde TGA als RGB-plaatje (bovenste rij eerst)."""
    if len(d) < 18 or d[2] != 2 or d[16] != 24:
        return None
    w, h = d[12] | d[13] << 8, d[14] | d[15] << 8
    p = 18 + d[0]
    if len(d) < p + w * h * 3:
        return None
    im = Image.frombytes("RGB", (w, h), d[p:p + w * h * 3], "raw", "BGR")
    if not d[17] & 0x20:
        im = im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    return im


def pixels(im):
    get = getattr(im, "get_flattened_data", None)  # Pillow 12+, getdata gaat weg
    return list(get() if get else im.getdata())


def fnv(h, b):
    for x in b:
        h = ((h ^ x) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def game_hash(im):
    """De hash die SwARc berekent voor deze texture in het spel (R5G6B5)."""
    w, h = im.size
    out = bytearray(w * h * 2)
    i = 0
    for r, g, b in pixels(im):
        if (r, g, b) == KEY:
            v = 0
        else:
            v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3) or 0x0800
        out[i] = v & 0xFF
        out[i + 1] = v >> 8
        i += 2
    head = struct.pack("<7I", w, h, 16, 0xF800, 0x07E0, 0x001F, 0)
    return "%016x" % fnv(fnv(0xCBF29CE484222325, head), out)


def key_mask(im):
    """'L': 255 = zichtbaar, 0 = key (plus de bijna-magenta rand ertegenaan)."""
    w, h = im.size
    px = pixels(im)
    exact = [p == KEY for p in px]
    if not any(exact):
        return None
    near = [r > 200 and g < 60 and 90 < b < 170 for r, g, b in px]
    key = exact[:]
    # Bijna-magenta telt alleen als hij aan de key vastzit (een roze auto
    # blijft roze); een paar rondes voor rand van meer dan één pixel.
    for _ in range(3):
        grow = key[:]
        for y in range(h):
            for x in range(w):
                i = y * w + x
                if key[i] or not near[i]:
                    continue
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        xx, yy = x + dx, y + dy
                        if 0 <= xx < w and 0 <= yy < h and key[yy * w + xx]:
                            grow[i] = True
        key = grow
    m = Image.new("L", (w, h))
    m.putdata([0 if k else 255 for k in key])
    return m


def fill_transparent(im, mask):
    """Doorzichtige texels krijgen de kleur van hun zichtbare buren."""
    rgba = im.convert("RGBA")
    rgba.putalpha(mask)
    out = im.copy()
    known = mask.copy()
    for radius in (1, 2, 4, 8, 16, 32, 64, 128):
        pre = rgba.convert("RGBa").filter(ImageFilter.BoxBlur(radius))
        blurred = pre.convert("RGBA")
        a = blurred.getchannel("A").point(lambda v: 255 if v > 0 else 0)
        todo = ImageChops.subtract(a, known)
        out.paste(blurred.convert("RGB"), (0, 0), todo)
        known = ImageChops.lighter(known, a)
        if known.getextrema()[0] == 255:
            break
    return out


def pad(im, wrap):
    w, h = im.size
    out = Image.new("RGB", (w + 2 * PAD, h + 2 * PAD))
    if wrap:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                out.paste(im, (PAD + dx * w, PAD + dy * h))
    else:
        # Rand herhalen: de buitenste rij/kolom uitrekken.
        out.paste(im.crop((0, 0, w, 1)).resize((w, PAD)), (PAD, 0))
        out.paste(im.crop((0, h - 1, w, h)).resize((w, PAD)), (PAD, PAD + h))
        out.paste(im.crop((0, 0, 1, h)).resize((PAD, h)), (0, PAD))
        out.paste(im.crop((w - 1, 0, w, h)).resize((PAD, h)), (PAD + w, PAD))
        for cx, cy, sx, sy in ((0, 0, 0, 0), (PAD + w, 0, w - 1, 0), (0, PAD + h, 0, h - 1),
                               (PAD + w, PAD + h, w - 1, h - 1)):
            out.paste(im.getpixel((sx, sy)), (cx, cy, cx + PAD, cy + PAD))
        out.paste(im, (PAD, PAD))
    return out


# ---- gereedschap


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def fetch(url, digest, path):
    if os.path.exists(path) and sha256(path) == digest:
        return
    print("ophalen:", url)
    tmp = path + ".part"
    with urllib.request.urlopen(url) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f)
    if sha256(tmp) != digest:
        os.remove(tmp)
        sys.exit("fout: %s heeft een andere sha256 dan verwacht" % url)
    os.replace(tmp, path)


def tools(cache):
    """Map met realesrgan-ncnn-vulkan en models/ (opgehaald als nodig)."""
    d = os.path.join(cache, "realesrgan")
    exe = os.path.join(d, "realesrgan-ncnn-vulkan")
    os.makedirs(os.path.join(d, "models"), exist_ok=True)
    if not os.path.exists(exe):
        z = os.path.join(cache, "realesrgan.zip")
        fetch(*ESRGAN_ZIP, z)
        with zipfile.ZipFile(z) as zf:
            for n in zf.namelist():
                if n == "realesrgan-ncnn-vulkan":
                    with zf.open(n) as src, open(exe, "wb") as dst:
                        shutil.copyfileobj(src, dst)
        os.chmod(exe, 0o755)
    for name, (url, digest) in MODEL_FILES.items():
        fetch(url, digest, os.path.join(d, "models", name))
    return d


def upscale(tool_dir, src_dir, dst_dir):
    r = subprocess.run(["./realesrgan-ncnn-vulkan", "-i", src_dir, "-o", dst_dir, "-n", MODEL,
                        "-s", str(SCALE), "-m", "models", "-f", "png"],
                       cwd=tool_dir, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        sys.exit("fout: realesrgan-ncnn-vulkan (%d):\n%s" % (r.returncode, r.stderr[-2000:]))


# ---- uitzonderingen


def load_rules(path):
    cp = configparser.ConfigParser(allow_no_value=True, delimiters=("=",))
    cp.optionxform = str.lower
    cp.read(path)
    def names(sec):
        return {k.lower() for k in cp[sec]} if cp.has_section(sec) else set()
    keep = {}
    if cp.has_section("keep"):
        for k, v in cp["keep"].items():
            rects = []
            for part in (v or "").split(";"):
                nums = [int(x) for x in part.replace(",", " ").split()]
                if len(nums) == 4:
                    rects.append(tuple(nums))
            keep[k.lower()] = rects
    return {"skip": names("skip"), "wrap": names("wrap"), "clamp": names("clamp"), "keep": keep}


def signature(name, rules):
    return json.dumps([VERSION, MODEL, SCALE, PAD, name in rules["wrap"], name in rules["clamp"],
                       rules["keep"].get(name, [])])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", default=os.path.join(ROOT, "iso", "data"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "texture_pack"))
    ap.add_argument("--cache", default=os.path.join(ROOT, "build", "tools"))
    ap.add_argument("--rules", default=os.path.join(ROOT, "tools", "texture_pack.ini"))
    ap.add_argument("--only", help="alleen deze namen (komma's), bv. overlay.tga")
    ap.add_argument("--force", action="store_true", help="alles opnieuw maken")
    a = ap.parse_args()
    if os.environ.get("VRACER_SKIP_UPSCALE"):
        print("HD-textures overgeslagen (VRACER_SKIP_UPSCALE)")
        return
    if not os.path.isdir(a.game_dir) or not find_nocase(a.game_dir, "rcs.ind"):
        sys.exit("geen rcs.ind in %s (pak eerst je CD uit, zie CLAUDE.md)" % a.game_dir)
    rules = load_rules(a.rules)
    only = {n.strip().lower() for n in a.only.split(",")} if a.only else None
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.cache, exist_ok=True)
    stamps_path = os.path.join(a.out, ".stamps.json")
    try:
        stamps = json.load(open(stamps_path))
    except (OSError, ValueError):
        stamps = {}
    names_path = os.path.join(a.cache, "hashes.json")  # naam+offset -> hash, scheelt rekenwerk
    try:
        known = json.load(open(names_path))
    except (OSError, ValueError):
        known = {}

    arc = Archive(a.game_dir)
    todo, index, seen = [], [], set()
    for name, off in arc.entries:
        low = name.lower()
        if not low.endswith(".tga") or (only and low not in only):
            continue
        im = decode_tga(arc.data(off))
        if im is None or im.size[0] > 256 or im.size[1] > 256:
            continue  # laadschermen (640x480) zijn 2D, geen texture
        if low in rules["skip"]:
            continue
        ck = "%s@%d" % (low, off)
        h = known.get(ck) or game_hash(im)
        known[ck] = h
        if h in seen:
            continue
        seen.add(h)
        if all(p == KEY for p in pixels(im)):
            continue  # helemaal key: wordt tijdens het spel getekend (radar)
        index.append((low, h, im.size))
        sig = signature(low, rules)
        if a.force or stamps.get(h) != sig or not os.path.exists(os.path.join(a.out, h + ".png")):
            todo.append((low, h, im, sig))
    json.dump(known, open(names_path, "w"), indent=0)

    if todo:
        print("textures: %d te maken, %d al klaar" % (len(todo), len(index) - len(todo)))
        tool_dir = tools(a.cache)
        with tempfile.TemporaryDirectory(dir=a.cache) as tmp:
            src, dst = os.path.join(tmp, "in"), os.path.join(tmp, "out")
            os.makedirs(src)
            os.makedirs(dst)
            prep = {}
            for low, h, im, sig in todo:
                mask = key_mask(im)
                filled = fill_transparent(im, mask) if mask else im
                wrap = (mask is None or low in rules["wrap"]) and low not in rules["clamp"]
                pad(filled, wrap).save(os.path.join(src, h + ".png"))
                if mask:
                    pad(mask.convert("RGB"), wrap).save(os.path.join(src, h + "_a.png"))
                prep[h] = (low, filled, mask, sig)
            upscale(tool_dir, src, dst)
            for h, (low, im, mask, sig) in prep.items():  # im: met gevulde key
                w, ht = im.size
                big = Image.open(os.path.join(dst, h + ".png")).convert("RGB")
                big = big.crop((PAD * SCALE, PAD * SCALE, (PAD + w) * SCALE, (PAD + ht) * SCALE))
                for x, y, rw, rh in rules["keep"].get(low, []):
                    part = im.crop((x, y, x + rw, y + rh)).resize((rw * SCALE, rh * SCALE), Image.Resampling.BICUBIC)
                    big.paste(part, (x * SCALE, y * SCALE))
                if mask:
                    alpha = Image.open(os.path.join(dst, h + "_a.png")).convert("L")
                    alpha = alpha.crop((PAD * SCALE, PAD * SCALE, (PAD + w) * SCALE, (PAD + ht) * SCALE))
                    alpha = alpha.point(lambda v: 255 if v >= 128 else 0)
                    big = big.convert("RGBA")
                    big.putalpha(alpha)
                big.save(os.path.join(a.out, h + ".png"), optimize=False, compress_level=6)
                stamps[h] = sig
                print("  %s -> %s.png" % (low, h))
    # Weg wat niet meer bij een texture hoort (alleen bij een volledige run).
    if not only:
        keepset = {h for _, h, _ in index}
        for f in os.listdir(a.out):
            if f.endswith(".png") and f[:-4] not in keepset:
                os.remove(os.path.join(a.out, f))
                stamps.pop(f[:-4], None)
    json.dump(stamps, open(stamps_path, "w"), indent=0)
    # index.tsv: naam, hash, maat (voor wie een texture op naam zoekt, zoals
    # het pauzemenu het HUD-font). Bij --only aangevuld, niet vervangen.
    lines = {}
    idx_path = os.path.join(a.out, "index.tsv")
    if only and os.path.exists(idx_path):
        for line in open(idx_path):
            lines[line.split("\t")[0]] = line
    for low, h, (w, ht) in index:
        lines[low] = "%s\t%s\t%dx%d\n" % (low, h, w, ht)
    with open(idx_path, "w") as f:
        f.writelines(lines[k] for k in sorted(lines))
    print("OK: %d textures in %s" % (len(index), os.path.relpath(a.out, ROOT)))


if __name__ == "__main__":
    main()
