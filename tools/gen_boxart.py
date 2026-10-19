#!/usr/bin/env python3
"""Original launcher artwork: a farm at night under a big harvest moon.

Drawn from scratch with shapes (no game graphics, no cover art), so it can be
kept in the repository. Output: launcher_assets/img/boxart.tga (portrait).
Run:  python3 tools/gen_boxart.py
"""
import math, random, os
from PIL import Image, ImageDraw, ImageFilter

W, H = 512, 704          # final size (portrait, ~ the launcher's hero slot)
S = 3                    # supersampling for smooth edges
w, h = W * S, H * S
rnd = random.Random(1996)

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

img = Image.new("RGB", (w, h))
d = ImageDraw.Draw(img)

# --- sky: deep indigo at the top, warm dusk glow at the horizon -------------
horizon = int(h * 0.60)
top, mid, low = (12, 14, 38), (34, 40, 78), (112, 84, 96)
for y in range(horizon + 40 * S):
    t = y / (horizon + 40 * S)
    c = lerp(top, mid, t / 0.65) if t < 0.65 else lerp(mid, low, (t - 0.65) / 0.35)
    d.line([(0, y), (w, y)], fill=c)

# --- stars --------------------------------------------------------------------
for _ in range(170):
    x, y = rnd.randrange(w), rnd.randrange(int(horizon * 0.85))
    r = rnd.choice([1, 1, 1, 2, 2, 3]) * S * 0.5
    b = rnd.randint(150, 255)
    d.ellipse([x - r, y - r, x + r, y + r], fill=(b, b, min(255, b + 10)))
for (fx, fy) in [(0.14, 0.10), (0.83, 0.07), (0.70, 0.30), (0.08, 0.36)]:
    x, y, L = fx * w, fy * h, 9 * S          # a few twinkling cross stars
    d.line([(x - L, y), (x + L, y)], fill=(255, 250, 220), width=S)
    d.line([(x, y - L), (x, y + L)], fill=(255, 250, 220), width=S)

# --- the harvest moon, with a soft glow --------------------------------------
mx, my, mr = int(w * 0.60), int(h * 0.30), int(w * 0.25)
glow = Image.new("L", (w, h), 0)
gd = ImageDraw.Draw(glow)
gd.ellipse([mx - mr * 1.7, my - mr * 1.7, mx + mr * 1.7, my + mr * 1.7], fill=80)
glow = glow.filter(ImageFilter.GaussianBlur(mr * 0.40))
img.paste(Image.new("RGB", (w, h), (255, 214, 120)), (0, 0), glow)
d = ImageDraw.Draw(img)
moon = Image.new("RGB", (2 * mr, 2 * mr))
md = ImageDraw.Draw(moon)
for yy in range(2 * mr):                   # warm gradient across the disc
    md.line([(0, yy), (2 * mr, yy)], fill=lerp((255, 226, 140), (242, 160, 52), yy / (2 * mr)))
for (cx, cy, cr) in [(0.35, 0.35, 0.13), (0.62, 0.55, 0.09), (0.45, 0.72, 0.07),
                     (0.72, 0.30, 0.06), (0.25, 0.60, 0.05)]:
    md.ellipse([(cx - cr) * 2 * mr, (cy - cr) * 2 * mr, (cx + cr) * 2 * mr, (cy + cr) * 2 * mr],
               fill=(244, 184, 88))
moon = moon.filter(ImageFilter.GaussianBlur(S * 1.5))
mask = Image.new("L", (2 * mr, 2 * mr), 0)
ImageDraw.Draw(mask).ellipse([0, 0, 2 * mr - 1, 2 * mr - 1], fill=255)
img.paste(moon, (mx - mr, my - mr), mask)
d = ImageDraw.Draw(img)

# --- a thin cloud across the moon --------------------------------------------
cloud = Image.new("L", (w, h), 0)
cd = ImageDraw.Draw(cloud)
for (fx, fy, rx, ry) in [(0.50, 0.40, 0.20, 0.022), (0.66, 0.415, 0.16, 0.018), (0.38, 0.43, 0.12, 0.015)]:
    cd.ellipse([(fx - rx) * w, (fy - ry) * h, (fx + rx) * w, (fy + ry) * h], fill=150)
cloud = cloud.filter(ImageFilter.GaussianBlur(6 * S))
img.paste(Image.new("RGB", (w, h), (70, 62, 98)), (0, 0), cloud)
d = ImageDraw.Draw(img)

# --- rolling hills, far to near ----------------------------------------------
def hill(base, amp, freq, phase, color):
    pts = [(0, h)]
    for x in range(0, w + S, S * 4):
        y = base + amp * math.sin(x / w * math.pi * freq + phase) \
                 + amp * 0.35 * math.sin(x / w * math.pi * freq * 2.7 + phase * 1.9)
        pts.append((x, y))
    pts.append((w, h))
    d.polygon(pts, fill=color)

hill(h * 0.585, h * 0.025, 2.2, 0.6, (38, 52, 54))
hill(h * 0.625, h * 0.030, 1.6, 2.1, (34, 62, 40))

# --- barn on the middle hill -------------------------------------------------
bx, by, bw, bh = int(w * 0.13), int(h * 0.625), int(w * 0.25), int(h * 0.11)
roof = by - bh
red, dark = (128, 40, 36), (84, 28, 28)
d.rectangle([bx, by - bh * 0.62, bx + bw, by + 6 * S], fill=red)
d.polygon([(bx - 8 * S, by - bh * 0.60), (bx + bw * 0.5, roof - bh * 0.18),
           (bx + bw + 8 * S, by - bh * 0.60)], fill=dark)
door_w = bw * 0.36
d.rectangle([bx + bw / 2 - door_w / 2, by - bh * 0.38, bx + bw / 2 + door_w / 2, by + 6 * S], fill=dark)
d.line([(bx + bw / 2 - door_w / 2, by - bh * 0.38), (bx + bw / 2 + door_w / 2, by + 6 * S)],
       fill=(205, 190, 160), width=2 * S)
d.line([(bx + bw / 2 + door_w / 2, by - bh * 0.38), (bx + bw / 2 - door_w / 2, by + 6 * S)],
       fill=(205, 190, 160), width=2 * S)
wx, wy, ws = bx + bw / 2, by - bh * 0.78, bw * 0.09     # lit loft window
d.rectangle([wx - ws, wy - ws, wx + ws, wy + ws], fill=(255, 214, 120))
lit = Image.new("L", (w, h), 0)
ImageDraw.Draw(lit).ellipse([wx - ws * 4, wy - ws * 4, wx + ws * 4, wy + ws * 4], fill=70)
lit = lit.filter(ImageFilter.GaussianBlur(ws * 1.5))
img.paste(Image.new("RGB", (w, h), (255, 200, 110)), (0, 0), lit)
d = ImageDraw.Draw(img)

# --- silo next to the barn ---------------------------------------------------
sx, sw = bx + bw + 6 * S, bw * 0.22
d.rectangle([sx, by - bh * 0.95, sx + sw, by + 6 * S], fill=(118, 112, 120))
d.pieslice([sx, by - bh * 0.95 - sw / 2, sx + sw, by - bh * 0.95 + sw / 2], 180, 360, fill=(90, 86, 96))

# --- a round tree on the right ----------------------------------------------
tx, ty = int(w * 0.84), int(h * 0.60)
d.rectangle([tx - 5 * S, ty, tx + 5 * S, ty + h * 0.05], fill=(48, 34, 26))
for (ox, oy, r) in [(0, -0.035, 0.075), (-0.045, -0.005, 0.055), (0.045, -0.005, 0.055)]:
    d.ellipse([tx + ox * w - r * w, ty + oy * h - r * w, tx + ox * w + r * w, ty + oy * h + r * w],
              fill=(30, 70, 40))

# --- near field with crop rows -----------------------------------------------
hill(h * 0.70, h * 0.018, 1.2, 4.0, (52, 92, 44))
field_top = h * 0.73
for i in range(9):                         # rows converge toward the barn
    t0 = i / 8
    x_far = w * (0.30 + 0.40 * t0)
    x_near = w * (-0.35 + 1.70 * t0)
    d.line([(x_far, field_top), (x_near, h)], fill=(78, 58, 36), width=int(10 * S))
for i in range(9):                         # little sprouts along the rows
    t0 = i / 8
    for k in range(1, 9):
        u = k / 9
        y = field_top + (h - field_top) * u
        x = w * (0.30 + 0.40 * t0) + (w * (-0.35 + 1.70 * t0) - w * (0.30 + 0.40 * t0)) * u
        s = (3 + 9 * u) * S
        d.ellipse([x - s * 1.2, y - s, x, y], fill=(120, 190, 80))
        d.ellipse([x, y - s, x + s * 1.2, y], fill=(150, 210, 96))

# --- wooden fence across the foreground --------------------------------------
fy = h * 0.83
for x in range(-S * 10, w + S * 60, int(w / 7)):
    d.rectangle([x, fy - 26 * S, x + 11 * S, fy + 34 * S], fill=(120, 84, 52))
    d.polygon([(x, fy - 26 * S), (x + 5.5 * S, fy - 33 * S), (x + 11 * S, fy - 26 * S)], fill=(120, 84, 52))
d.rectangle([0, fy - 16 * S, w, fy - 8 * S], fill=(140, 100, 62))
d.rectangle([0, fy + 6 * S, w, fy + 14 * S], fill=(140, 100, 62))

# --- fireflies ---------------------------------------------------------------
ff = Image.new("L", (w, h), 0)
fd = ImageDraw.Draw(ff)
for _ in range(16):
    x, y = rnd.randrange(w), rnd.randrange(int(h * 0.62), int(h * 0.80))
    r = rnd.randint(4, 7) * S
    fd.ellipse([x - r, y - r, x + r, y + r], fill=200)
ff = ff.filter(ImageFilter.GaussianBlur(3 * S))
img.paste(Image.new("RGB", (w, h), (240, 240, 140)), (0, 0), ff)

# --- gentle vignette ---------------------------------------------------------
vig = Image.new("L", (w, h), 0)
ImageDraw.Draw(vig).rectangle([0, 0, w, h], outline=255, width=int(w * 0.06))
vig = vig.filter(ImageFilter.GaussianBlur(w * 0.05))
img.paste(Image.new("RGB", (w, h), (8, 10, 6)), (0, 0), vig.point(lambda v: v * 0.55))

out = img.resize((W, H), Image.LANCZOS)
here = os.path.dirname(os.path.abspath(__file__))
dst = os.path.join(here, "..", "launcher_assets", "img", "boxart.tga")
out.save(dst)                       # uncompressed 24-bit TGA (stb_image reads it)
print("wrote", os.path.normpath(dst))

# --- app icon: the moon over a sprout -----------------------------------------
def make_icon(n=256):
    s = n * 4
    ic = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    g = ImageDraw.Draw(ic)
    g.rounded_rectangle([0, 0, s - 1, s - 1], radius=s * 0.22, fill=(28, 34, 62, 255))
    for y in range(int(s * 0.62), s):             # field at the bottom
        g.line([(0, y), (s, y)], fill=(52, 92, 44, 255))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=s * 0.22, fill=255)
    cr = s * 0.30; cx, cy = s * 0.5, s * 0.40
    halo = Image.new("L", (s, s), 0)
    ImageDraw.Draw(halo).ellipse([cx - cr * 1.35, cy - cr * 1.35, cx + cr * 1.35, cy + cr * 1.35], fill=110)
    halo = halo.filter(ImageFilter.GaussianBlur(cr * 0.18))
    ic.paste(Image.new("RGBA", (s, s), (255, 214, 120, 255)), (0, 0), halo)
    g = ImageDraw.Draw(ic)
    g.ellipse([cx - cr, cy - cr, cx + cr, cy + cr], fill=(250, 196, 84, 255))
    g.ellipse([cx - cr * 0.55, cy - cr * 0.50, cx - cr * 0.15, cy - cr * 0.10], fill=(236, 172, 70, 255))
    g.ellipse([cx + cr * 0.20, cy + cr * 0.05, cx + cr * 0.50, cy + cr * 0.35], fill=(236, 172, 70, 255))
    g.rectangle([0, s * 0.70, s, s], fill=(52, 92, 44, 255))   # hill in front of the moon
    g.ellipse([-s * 0.2, s * 0.62, s * 1.2, s * 1.1], fill=(60, 104, 50, 255))
    st = s * 0.035                                       # sprout
    g.rectangle([s * 0.5 - st / 2, s * 0.60, s * 0.5 + st / 2, s * 0.80], fill=(120, 190, 80, 255))
    g.ellipse([s * 0.5 - s * 0.17, s * 0.55, s * 0.5, s * 0.66], fill=(150, 210, 96, 255))
    g.ellipse([s * 0.5, s * 0.52, s * 0.5 + s * 0.19, s * 0.64], fill=(120, 190, 80, 255))
    out = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    out.paste(ic, (0, 0), mask)
    return out.resize((n, n), Image.LANCZOS)

icon = make_icon(256)
img_dir = os.path.join(here, "..", "launcher_assets", "img")
icon.save(os.path.join(img_dir, "icon.png"))
icon.save(os.path.join(img_dir, "icon.ico"),
          sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("wrote icon.png / icon.ico")
