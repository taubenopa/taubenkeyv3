#!/usr/bin/env python3
"""Erzeugt das Installer-Artwork (Nachtstadt, Neon, Tauben) - keine Binärdateien im Repo nötig.
   Aufruf:  python installer/make_art.py     (braucht: pip install pillow)"""
import math
import os
import random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
INK, NIGHT = (10, 12, 21), (18, 22, 42)
CREAM, AMBER, MINT = (244, 234, 213), (255, 194, 71), (55, 230, 160)
MAGENTA, VIOLET, CYAN = (255, 61, 154), (160, 102, 255), (79, 216, 255)
PIGEON, PIGEON_D = (141, 151, 173), (80, 88, 110)


def load_font(size, bold=True):
    for p in ("C:/Windows/Fonts/arialbd.ttf", "C:/Windows/Fonts/impact.ttf",
              "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/Library/Fonts/Arial Bold.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def vgradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return img


def glow(img, cx, cy, radius, color, strength):
    layer = Image.new("RGB", img.size, (0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=tuple(int(c * strength) for c in color))
    layer = layer.filter(ImageFilter.GaussianBlur(radius / 2.2))
    return _add(img, layer)


def _add(a, b):
    from PIL import ImageChops
    return ImageChops.add(a, b)


def skyline(img, y_base, seed, max_h, tint=(30, 35, 58)):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    x = 0
    while x < img.width:
        bw, bh = rnd.randint(14, 34), rnd.randint(max_h // 4, max_h)
        d.rectangle((x, y_base - bh, x + bw, y_base), fill=tint)
        for wy in range(y_base - bh + 5, y_base - 4, 8):
            for wx in range(x + 3, x + bw - 3, 6):
                if rnd.random() < 0.3:
                    d.rectangle((wx, wy, wx + 2, wy + 3), fill=rnd.choice([AMBER, CYAN]))
        x += bw + rnd.randint(0, 4)


def pigeon(d, cx, cy, s, flip=False, wing=1.0, crown=False):
    """Seitenansicht, Körpermitte (cx, cy), Länge ~ s. wing: 1 = Flügel oben, -1 = unten."""
    f = -1 if flip else 1
    P = lambda x, y: (cx + f * x * s, cy + y * s)
    # Flügel (hinten)
    def wing_poly(col, dx):
        tip_y = -0.80 * wing if wing > 0 else 0.55
        pts = [P(dx, -0.12), P(dx + 0.14, -0.35 * wing), P(dx + 0.05, tip_y * 0.8), P(dx - 0.14, tip_y),
               P(dx - 0.26, -0.30 * wing), P(dx - 0.28, -0.12)]
        d.polygon(pts, fill=col)
    wing_poly(PIGEON_D, -0.04)
    d.polygon([P(-0.34, -0.08), P(-0.84, -0.04), P(-0.80, 0.13), P(-0.34, 0.12)], fill=PIGEON_D)   # Schwanz
    d.ellipse((*P(-0.46, -0.22), *P(0.46, 0.24)) if not flip else (*P(0.46, -0.22), *P(-0.46, 0.24)), fill=PIGEON)
    # Schillerhals
    a, b = P(0.10, -0.31), P(0.52, 0.07)
    d.ellipse((min(a[0], b[0]), a[1], max(a[0], b[0]), b[1]), fill=(53, 224, 161))
    a, b = P(0.28, -0.27), P(0.52, 0.07)
    d.ellipse((min(a[0], b[0]), a[1], max(a[0], b[0]), b[1]), fill=(177, 77, 255))
    a, b = P(0.34, -0.41), P(0.61, -0.15)                                                          # Kopf
    d.ellipse((min(a[0], b[0]), a[1], max(a[0], b[0]), b[1]), fill=(152, 162, 184))
    d.polygon([P(0.58, -0.32), P(0.73, -0.26), P(0.58, -0.22)], fill=(240, 224, 200))              # Schnabel
    ex, ey = P(0.50, -0.32)
    r = 0.04 * s
    d.ellipse((ex - r, ey - r, ex + r, ey + r), fill=(255, 138, 0))
    d.ellipse((ex - r / 2, ey - r / 2, ex + r / 2, ey + r / 2), fill=(0, 0, 0))
    if crown:
        d.polygon([P(0.40, -0.40), P(0.39, -0.54), P(0.44, -0.47), P(0.47, -0.57), P(0.50, -0.47), P(0.55, -0.54), P(0.54, -0.40)], fill=AMBER)
    wing_poly((165, 175, 197), 0.0)


def text_chroma(img, xy, text, font, anchor="la"):
    d = ImageDraw.Draw(img)
    x, y = xy
    d.text((x + 3, y + 3), text, font=font, fill=MAGENTA, anchor=anchor)
    d.text((x - 2, y - 1), text, font=font, fill=CYAN, anchor=anchor)
    d.text((x, y), text, font=font, fill=CREAM, anchor=anchor)


def stars(img, seed, n):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    for _ in range(n):
        x, y = rnd.randrange(img.width), rnd.randrange(int(img.height * 0.6))
        d.point((x, y), fill=tuple(int(c * rnd.uniform(0.3, 0.8)) for c in CREAM))


def wizard_big(path):                       # Inno Setup: 164 x 314
    w, h = 164, 314
    img = vgradient(w, h, INK, (28, 22, 52))
    img = glow(img, w - 20, 10, 140, AMBER, 0.45)
    img = glow(img, 20, h, 120, MAGENTA, 0.35)
    stars(img, 3, 60)
    skyline(img, h - 40, 11, 90)
    d = ImageDraw.Draw(img)
    d.rectangle((0, h - 40, w, h - 36), fill=MAGENTA)
    d.rectangle((0, h - 36, w, h - 34), fill=CREAM)
    pigeon(d, 82, 118, 78, flip=False, wing=1.0, crown=True)
    pigeon(d, 36, 64, 30, flip=True, wing=-1.0)
    pigeon(d, 128, 40, 26, flip=False, wing=1.0)
    text_chroma(img, (w // 2, 190), "Tauben", load_font(30), "ma")
    text_chroma(img, (w // 2, 222), "Key", load_font(30), "ma")
    d = ImageDraw.Draw(img)
    d.text((w // 2, 275), "von taubenopa", font=load_font(12), fill=AMBER, anchor="ma")
    img.save(path, "BMP")


def wizard_small(path):                     # Inno Setup: 55 x 58
    img = vgradient(55, 58, INK, (28, 22, 52))
    img = glow(img, 45, 6, 40, AMBER, 0.5)
    d = ImageDraw.Draw(img)
    pigeon(d, 27, 30, 30, wing=1.0, crown=True)
    d.rectangle((0, 54, 55, 57), fill=MAGENTA)
    img.save(path, "BMP")


def mac_background(path):                   # productbuild: 1240 x 800
    w, h = 1240, 800
    img = vgradient(w, h, INK, (28, 22, 52))
    img = glow(img, w - 160, 40, 520, AMBER, 0.4)
    img = glow(img, 80, h, 420, MAGENTA, 0.3)
    stars(img, 5, 240)
    skyline(img, h - 70, 21, 260)
    d = ImageDraw.Draw(img)
    d.rectangle((0, h - 70, w, h - 62), fill=MAGENTA)
    d.rectangle((0, h - 62, w, h - 58), fill=CREAM)
    pigeon(d, 930, 300, 300, wing=1.0, crown=True)
    pigeon(d, 560, 360, 100, flip=True, wing=-1.0)
    pigeon(d, 1090, 120, 90, wing=1.0)
    text_chroma(img, (70, 70), "TaubenKey", load_font(96), "la")
    d = ImageDraw.Draw(img)
    d.text((76, 190), "von taubenopa", font=load_font(34), fill=AMBER)
    img.save(path, "PNG")


if __name__ == "__main__":
    os.makedirs(os.path.join(HERE, "art"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "macos", "resources"), exist_ok=True)
    wizard_big(os.path.join(HERE, "art", "wizard.bmp"))
    wizard_small(os.path.join(HERE, "art", "wizard_small.bmp"))
    mac_background(os.path.join(HERE, "macos", "resources", "background.png"))
    print("Artwork erzeugt.")
