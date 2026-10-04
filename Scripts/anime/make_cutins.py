"""Build the three anime cut-in clips (Critical / Infuse / KO) from Aura-generated stills.
Each clip: 1280x720, 24 fps, H.264 High yuv420p. Pure 2D compositing (PIL + numpy) piped into ffmpeg.
Usage: python3 make_cutins.py <aura_png_dir> <out_dir>"""
import sys, math, random, subprocess, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageOps, ImageEnhance

W, H, FPS = 1280, 720, 24
SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)

def load(name, ycenter=0.5):
    im = Image.open(os.path.join(SRC, name + '.png')).convert('RGB')
    w, h = im.size
    ch = int(w * 9 / 16)
    y0 = int(max(0, min(h - ch, ycenter * h - ch / 2)))
    return im.crop((0, y0, w, y0 + ch))  # 1536x864

def view(src, zoom=1.0, cx=0.5, cy=0.5, dx=0.0, dy=0.0, rot=0.0):
    """Virtual camera: zoom>=1 into src around (cx,cy) (0..1), shake offset dx,dy in output px, roll in degrees."""
    sw, sh = src.size
    x0, y0, cw, chh = cam_rect(src, zoom, cx, cy, dx, dy)
    if rot:
        pad = 1.6
        big = src.transform((int(W * pad), int(H * pad)), Image.EXTENT,
                            (x0 - cw * (pad - 1) / 2, y0 - chh * (pad - 1) / 2, x0 + cw * (1 + (pad - 1) / 2), y0 + chh * (1 + (pad - 1) / 2)), Image.BICUBIC)
        big = big.rotate(rot, Image.BICUBIC)
        l = (big.width - W) // 2; t = (big.height - H) // 2
        return big.crop((l, t, l + W, t + H))
    return src.transform((W, H), Image.EXTENT, (x0, y0, x0 + cw, y0 + chh), Image.BICUBIC)

def cam_rect(src, zoom, cx, cy, dx=0.0, dy=0.0):
    sw, sh = src.size
    cw, chh = sw / zoom, sh / zoom
    x0 = cx * sw - cw / 2 - dx * cw / W
    y0 = cy * sh - chh / 2 - dy * chh / H
    x0 = max(0, min(sw - cw, x0)); y0 = max(0, min(sh - chh, y0))
    return x0, y0, cw, chh

def to_screen(src, zoom, cx, cy, px, py):
    """Source pixel -> output pixel for an unshaken, unrolled camera."""
    x0, y0, cw, chh = cam_rect(src, zoom, cx, cy)
    return (px - x0) * W / cw, (py - y0) * H / chh

def radial_lines(seed, n=140, inner=0.32, color=(255, 255, 255), alpha=200, cx=W/2, cy=H/2, width=(2, 9)):
    rnd = random.Random(seed)
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
    R = math.hypot(W, H)
    for _ in range(n):
        a = rnd.uniform(0, 2 * math.pi); r0 = R * rnd.uniform(inner, inner + 0.25); w = rnd.uniform(*width) / R
        p = [(cx + math.cos(a) * r0, cy + math.sin(a) * r0),
             (cx + math.cos(a - w * 6) * R, cy + math.sin(a - w * 6) * R),
             (cx + math.cos(a + w * 6) * R, cy + math.sin(a + w * 6) * R)]
        d.polygon(p, fill=color + (int(alpha * rnd.uniform(0.5, 1.0)),))
    return lay

def straight_lines(seed, n=60, vertical=False, color=(255, 255, 255), alpha=170, clear=None):
    rnd = random.Random(seed)
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
    for _ in range(n):
        th = rnd.uniform(1, 6); ln = rnd.uniform(0.25, 0.8)
        if vertical:
            x = rnd.uniform(0, W)
            if clear and abs(x - clear[0]) < clear[1]: continue
            y = rnd.uniform(-H * 0.3, H); d.rectangle([x, y, x + th, y + ln * H], fill=color + (int(alpha * rnd.uniform(.4, 1)),))
        else:
            y = rnd.uniform(0, H)
            if clear and abs(y - clear[0]) < clear[1]: continue
            x = rnd.uniform(-W * 0.3, W); d.rectangle([x, y, x + ln * W, y + th], fill=color + (int(alpha * rnd.uniform(.4, 1)),))
    return lay

def ring(r, width, color, alpha, cx=W/2, cy=H/2):
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color + (alpha,), width=int(width))
    return lay.filter(ImageFilter.GaussianBlur(2))

def star_glint(cx, cy, size, alpha, color=(255, 250, 220)):
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
    for ang, s in ((0, 1.0), (90, 1.0), (45, 0.45), (135, 0.45)):
        a = math.radians(ang); L = size * s
        for k, wdt in ((1.0, 3), (0.6, 7)):
            d.line([(cx - math.cos(a) * L * k, cy - math.sin(a) * L * k), (cx + math.cos(a) * L * k, cy + math.sin(a) * L * k)], fill=color + (int(alpha * (0.5 if wdt > 3 else 1)),), width=wdt)
    d.ellipse([cx - size * .12, cy - size * .12, cx + size * .12, cy + size * .12], fill=color + (alpha,))
    return lay.filter(ImageFilter.GaussianBlur(1.2))

_vig = None
def vignette(img, strength=0.55):
    global _vig
    if _vig is None:
        y, x = np.mgrid[0:H, 0:W]; r = np.hypot((x - W/2) / (W/2), (y - H/2) / (H/2))
        _vig = np.clip(1 - strength * np.clip(r - 0.55, 0, None) ** 1.5, 0, 1)[..., None]
    return Image.fromarray((np.asarray(img).astype(np.float32) * _vig).astype(np.uint8))

def add(base, lay):
    """Additive blend an RGBA layer."""
    b = np.asarray(base).astype(np.float32); l = np.asarray(lay).astype(np.float32)
    out = b + l[..., :3] * (l[..., 3:4] / 255.0)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))

def over(base, lay):
    return Image.alpha_composite(base.convert('RGBA'), lay).convert('RGB')

def flash(img, amt, color=(255, 255, 255)):
    return Image.blend(img, Image.new('RGB', (W, H), color), max(0, min(1, amt)))

def chroma(img, px):
    if px < 1: return img
    r, g, b = img.split()
    r = ImageChops.offset(r, int(px), 0); b = ImageChops.offset(b, -int(px), 0)
    return Image.merge('RGB', (r, g, b))

def impact_frame(img, invert=True):
    g = ImageOps.grayscale(img); g = ImageOps.autocontrast(g, cutoff=5)
    g = g.point(lambda v: 255 if v > 110 else 0)
    if invert: g = ImageOps.invert(g)
    return g.convert('RGB')

def ease_out(t): return 1 - (1 - t) ** 3
def ease_in(t): return t ** 3

def shake(rnd, amp):
    return rnd.uniform(-amp, amp), rnd.uniform(-amp, amp)

def encode(name, frames):
    path = os.path.join(OUT, name + '.mp4')
    p = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}', '-r', str(FPS), '-i', '-',
                          '-c:v', 'libx264', '-profile:v', 'high', '-pix_fmt', 'yuv420p', '-preset', 'slow', '-crf', '16',
                          '-maxrate', '9M', '-bufsize', '18M', '-g', str(FPS), '-movflags', '+faststart', '-an', path], stdin=subprocess.PIPE)
    for f in frames:
        p.stdin.write(f.tobytes())
    p.stdin.close(); p.wait()
    print('wrote', path, len(frames), 'frames')

# ---------------------------------------------------------------- Critical (2.0 s): glint -> white flash -> golden starburst
def critical():
    A = load('anime_critical_a', 0.5); B = load('anime_critical_b', 0.5)
    rnd = random.Random(7); N = int(2.0 * FPS); cut = 18; out = []
    for i in range(N):
        t = i / FPS
        if i < cut:  # slow-motion push toward the contact point, lines tightening
            k = i / cut
            img = view(A, zoom=1.05 + 0.4 * k * k, cx=0.47 + 0.03 * k, cy=0.47, dx=2 * math.sin(i * 2.1) * k, dy=2 * math.cos(i * 1.7) * k)
            img = add(img, radial_lines(i // 2, n=110, inner=0.42 - 0.12 * k, alpha=int(90 + 110 * k)))
            img = add(img, star_glint(W * 0.5, H * 0.47, 40 + 200 * k * k, int(60 + 195 * k)))
            if i >= cut - 2: img = flash(img, 0.6 + 0.2 * (i - cut + 2))
        elif i < cut + 2:  # two pure white frames
            img = Image.new('RGB', (W, H), (255, 255, 255)) if i == cut else impact_frame(view(B, 1.25), invert=False)
        else:  # golden starburst: zoom punch out, heavy decaying shake, expanding ring
            k = (i - cut - 2) / (N - cut - 2)
            amp = 34 * (1 - k) ** 2; dx, dy = shake(rnd, amp)
            img = view(B, zoom=1.3 - 0.22 * ease_out(k), cx=0.5, cy=0.48, dx=dx, dy=dy, rot=rnd.uniform(-1.5, 1.5) * (1 - k))
            img = add(img, radial_lines(100 + i // 2, n=150, inner=0.22 + 0.15 * k, color=(255, 214, 90), alpha=int(220 * (1 - k * 0.6))))
            img = add(img, ring(60 + 900 * ease_out(k), 26 * (1 - k) + 4, (255, 236, 160), int(255 * (1 - k))))
            img = add(img, ring(30 + 520 * ease_out(min(1, k * 1.4)), 12, (255, 255, 255), int(200 * max(0, 1 - k * 1.4))))
            img = chroma(img, 10 * (1 - k) ** 2)
            if k < 0.12: img = flash(img, 0.45 * (1 - k / 0.12), (255, 240, 190))
        out.append(vignette(img, 0.6))
    encode('Anime_Any_Any_Critical_01', out)

# ---------------------------------------------------------------- Infuse (2.0 s): energy spirals in (rolling camera) -> blue flash -> aura pillar (tilt-up, rising lines)
def infuse():
    A = load('anime_infuse_a', 0.5); B = load('anime_infuse_b', 0.62)
    rnd = random.Random(11); N = int(2.0 * FPS); cut = 24; out = []
    for i in range(N):
        if i < cut:
            k = i / cut
            img = view(A, zoom=1.3 + 0.35 * ease_in(k), cx=0.5, cy=0.5, rot=-14 * k - 4 * k * k)  # roll inward with the spiral
            for j in range(3):  # converging rings
                rr = ((1 - ((k * 2.2 + j / 3) % 1)) ** 1.4) * 700 + 40
                img = add(img, ring(rr, 5, (120, 200, 255), int(170 * (1 - rr / 760))))
            pulse = 0.5 + 0.5 * math.sin(i * 1.3)
            img = ImageEnhance.Brightness(img).enhance(1.0 + 0.10 * pulse * k)
            if i >= cut - 3: img = flash(img, 0.35 * (i - cut + 4), (170, 225, 255))
        else:
            k = (i - cut) / (N - cut)
            dx, dy = shake(rnd, 10 * (1 - k) + 2)
            img = view(B, zoom=1.22 - 0.08 * k, cx=0.5, cy=0.58 - 0.16 * ease_out(k), dx=dx, dy=dy)  # tilt up the pillar
            img = add(img, straight_lines(200 + i, n=70, vertical=True, color=(150, 220, 255), alpha=150, clear=(W / 2, 90)))
            img = add(img, ring(80 + 600 * ((k * 2.5) % 1), 8, (200, 160, 255), int(200 * (1 - (k * 2.5) % 1)), cy=H * (0.78 - 0.1 * k)))
            if k < 0.15: img = flash(img, 0.8 * (1 - k / 0.15), (190, 235, 255))
            img = ImageEnhance.Color(img).enhance(1.12)
        out.append(vignette(img, 0.5))
    encode('Anime_Any_Any_Infuse_01', out)

# ---------------------------------------------------------------- KO (2.5 s): impact frames -> blast with horizontal speed lines -> skyward comet + star twinkle
def ko():
    A = load('anime_ko_a', 0.5); B = load('anime_ko_b', 0.30)
    rnd = random.Random(23); N = int(2.5 * FPS); cut = 30; out = []
    # comet head in B's 16:9 crop coordinates (found from the still), normalised
    hpx, hpy = KO_HEAD[0], KO_HEAD[1] - B_Y0[0]; hx, hy = hpx / 1536, hpy / 864
    for i in range(N):
        if i < 4:  # anime impact frames: invert / white / invert / mono
            base = view(A, zoom=1.35, cx=0.6, cy=0.55)
            img = [impact_frame(base, True), Image.new('RGB', (W, H), (255, 255, 255)), impact_frame(base, False), impact_frame(base, True)][i]
        elif i < cut:
            k = (i - 4) / (cut - 4)
            amp = 40 * (1 - k) ** 1.5 + 3; dx, dy = shake(rnd, amp)
            img = view(A, zoom=1.35 - 0.3 * ease_out(k), cx=0.6 - 0.1 * k, cy=0.55, dx=dx + 160 * ease_out(k), dy=dy, rot=4 * (1 - k) * (1 if i % 2 else -1))
            img = add(img, straight_lines(300 + i, n=80, vertical=False, color=(255, 240, 220), alpha=int(200 * (1 - k * 0.5)), clear=(H * 0.55, 120)))
            img = chroma(img, 14 * (1 - k) ** 2)
            if i < 7: img = flash(img, 0.5 - 0.15 * (i - 4), (255, 200, 120))
        else:
            k = (i - cut) / (N - cut)
            z = 1.0 + 1.0 * (k * k * (3 - 2 * k)); ccx = 0.5 + (hx - 0.5) * ease_out(k); ccy = 0.5 + (hy - 0.5) * ease_out(k)
            img = view(B, zoom=z, cx=ccx, cy=ccy)
            # project head into output space for the twinkle
            sx, sy = to_screen(B, z, ccx, ccy, hpx, hpy)
            if k > 0.45:
                tw = (k - 0.45) / 0.55; s = math.sin(tw * math.pi)
                img = add(img, star_glint(sx, sy, 20 + 110 * s, int(255 * s)))
            if i < cut + 3: img = flash(img, 1 - (i - cut) / 3)
        out.append(vignette(img, 0.6))
    encode('Anime_Any_Any_KO_01', out)

B_Y0 = [0]
KO_HEAD = (float(os.environ.get('KO_HX', 1157)), float(os.environ.get('KO_HY', 77)))
def _ko_b_y0():
    im = Image.open(os.path.join(SRC, 'anime_ko_b.png')); w, h = im.size; ch = int(w * 9 / 16)
    B_Y0[0] = int(max(0, min(h - ch, 0.30 * h - ch / 2)))
_ko_b_y0()

which = sys.argv[3:] or ['critical', 'infuse', 'ko']
for w in which: globals()[w]()
