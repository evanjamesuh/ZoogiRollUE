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


# ======================================================================================= variants 02/03 (Oct 4, 4:45 PM)
def particles(seed, n, t, color, rise=True, size=(2, 6), alpha=220, spread_y=1.0, speed=1.0, drift=0.0):
    """Ember / dust / spark particles; deterministic per seed, advanced by time t (s)."""
    rnd = random.Random(seed)
    lay = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(lay)
    for _ in range(n):
        x0 = rnd.uniform(0, W); y0 = rnd.uniform(0, H * spread_y + H * (1 - spread_y)); v = rnd.uniform(120, 420) * speed
        r = rnd.uniform(*size); ph = rnd.uniform(0, 6.28)
        y = (y0 - v * t) % (H + 40) - 20 if rise else (y0 + v * t) % (H + 40) - 20
        x = x0 + math.sin(t * 3 + ph) * 18 + drift * t * v * 0.3
        a = int(alpha * (0.5 + 0.5 * math.sin(t * 9 + ph)))
        d.ellipse([x - r, y - r, x + r, y + r], fill=color + (a,))
    return lay.filter(ImageFilter.GaussianBlur(1.0))

def heat_shimmer(img, t, amp=4.0, freq=0.035):
    a = np.asarray(img)
    ys = np.arange(H)
    shift = (amp * np.sin(ys * freq + t * 14)).astype(int)
    out = np.empty_like(a)
    for y in range(H):
        out[y] = np.roll(a[y], shift[y], axis=0)
    return Image.fromarray(out)

def halftone_tint(img, color, strength=0.35, cell=8):
    g = np.asarray(ImageOps.grayscale(img)).astype(np.float32) / 255.0
    yy, xx = np.mgrid[0:H, 0:W]
    cx = (xx % cell) - cell / 2; cy = (yy % cell) - cell / 2
    dots = (np.hypot(cx, cy) < (g * cell * 0.62)).astype(np.float32)
    base = np.asarray(img).astype(np.float32)
    col = np.array(color, np.float32)[None, None, :]
    out = base * (1 - strength) + (dots[..., None] * col) * strength
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))

def grade(img, sat=1.0, bright=1.0, contrast=1.0, tint=None, tint_amt=0.0):
    if sat != 1.0: img = ImageEnhance.Color(img).enhance(sat)
    if contrast != 1.0: img = ImageEnhance.Contrast(img).enhance(contrast)
    if bright != 1.0: img = ImageEnhance.Brightness(img).enhance(bright)
    if tint: img = Image.blend(img, Image.new('RGB', (W, H), tint), tint_amt)
    return img

def hblur(img, px):
    """Horizontal motion blur (whip pan)."""
    if px < 2: return img
    a = np.asarray(img).astype(np.float32); acc = np.zeros_like(a); k = int(px)
    for i in range(0, k, max(1, k // 12)):
        acc += np.roll(a, i - k // 2, axis=1)
    acc /= len(range(0, k, max(1, k // 12)))
    return Image.fromarray(acc.astype(np.uint8))

def vblur(img, px):
    if px < 2: return img
    a = np.asarray(img).astype(np.float32); acc = np.zeros_like(a); k = int(px); n = 0
    for i in range(0, k, max(1, k // 12)):
        acc += np.roll(a, i - k // 2, axis=0); n += 1
    return Image.fromarray((acc / n).astype(np.uint8))

def panels(imgs, t, gap=10, slant=90, border=(10, 10, 10)):
    """3 slanted comic panels (manga split), each fed its own image; panels slide in with t (0..1)."""
    out = Image.new('RGB', (W, H), border)
    xs = [0, W / 3, 2 * W / 3, W]
    for i, im in enumerate(imgs):
        mask = Image.new('L', (W, H), 0); d = ImageDraw.Draw(mask)
        x0, x1 = xs[i], xs[i + 1]
        off = (1 - ease_out(min(1, max(0, t * 3 - i * 0.6)))) * H * (1 if i % 2 else -1)
        poly = [(x0 + slant + gap / 2, 0 + off), (x1 + slant - gap / 2, 0 + off), (x1 - slant - gap / 2, H + off), (x0 - slant + gap / 2, H + off)]
        if i == 0: poly[0] = (-10, off); poly[3] = (-10, H + off)
        if i == 2: poly[1] = (W + 10, off); poly[2] = (W + 10, H + off)
        d.polygon(poly, fill=255)
        out.paste(im, (0, 0), mask)
    return out

# ---------------------------------------------------------------- Critical_02 (2.0 s): overhead neon floor, spinning top-down approach -> ripple shockwave, magenta/cyan
def critical2():
    A = load('anime_crit2_a', 0.5); B = load('anime_crit2_b', 0.5)
    rnd = random.Random(31); N = 48; cut = 20; out = []
    for i in range(N):
        if i < cut:
            k = i / cut
            img = view(A, zoom=1.8 + 0.1 * k, cx=0.5, cy=0.5, rot=25 * (1 - k) ** 2)  # overhead camera spins and settles
            img = add(img, straight_lines(400 + i // 2, n=40, vertical=False, color=(90, 240, 255), alpha=int(80 + 80 * k)))
            img = chroma(img, 3 + 6 * k)
            if i >= cut - 2: img = flash(img, 0.5 + 0.25 * (i - cut + 2), (255, 120, 230))
        else:
            k = (i - cut) / (N - cut)
            dx, dy = shake(rnd, 22 * (1 - k) ** 2)
            img = view(B, zoom=1.4 + 0.18 * (1 - ease_out(k)), cx=0.5, cy=0.5, dx=dx, dy=dy, rot=-6 * k)
            for j, (col, sp) in enumerate((((255, 90, 220), 1.0), ((90, 240, 255), 0.75), ((255, 255, 255), 0.5))):
                rr = 40 + 900 * ease_out(min(1, k * (1.2 + j * 0.4)))
                img = add(img, ring(rr, 14 - j * 3, col, int(230 * max(0, 1 - k * (1.1 + j * 0.3)))))
            if i < cut + 2: img = flash(img, 0.9 - 0.4 * (i - cut), (255, 200, 245))
            if (i - cut) in (5, 9): img = ImageOps.invert(img)  # two single-frame negative pops
        out.append(vignette(img, 0.7))
    encode('Anime_Any_Any_Critical_02', out)

# ---------------------------------------------------------------- Critical_03 (2.0 s): emerald lightning, lateral dolly + smash zoom, green strobe, ends on 3 manga panels
def critical3():
    A = load('anime_crit3_a', 0.5)
    rnd = random.Random(37); N = 48; out = []
    for i in range(N):
        t = i / FPS
        if i < 14:  # lateral dolly from left toward the impact, halftone, ink look
            k = i / 14
            img = view(A, zoom=1.9, cx=0.26 + 0.24 * k, cy=0.5 + 0.04 * math.sin(k * 3))
            img = halftone_tint(img, (60, 255, 140), 0.25)
            img = add(img, straight_lines(500 + i // 2, n=50, vertical=False, color=(150, 255, 190), alpha=150))
        elif i < 30:  # smash zoom into contact + strobe
            k = (i - 14) / 16
            dx, dy = shake(rnd, 26 * (1 - k))
            img = view(A, zoom=2.2 - 1.0 * ease_out(k), cx=0.5, cy=0.5, dx=dx, dy=dy)
            img = add(img, radial_lines(600 + i, n=120, inner=0.25, color=(120, 255, 160), alpha=200))
            if (i - 14) % 4 == 0: img = flash(img, 0.75, (170, 255, 190))
            elif (i - 14) % 4 == 1: img = impact_frame(img, invert=True)
        else:  # three slanted panels: wide, contact close-up, cracked ball close-up
            k = (i - 30) / (N - 30)
            p1 = view(A, zoom=1.15 + 0.05 * k, cx=0.5, cy=0.5)
            p2 = grade(view(A, zoom=3.0, cx=0.5, cy=0.5), contrast=1.2)
            p3 = halftone_tint(view(A, zoom=2.4, cx=0.68, cy=0.5), (60, 255, 140), 0.35)
            img = panels([p1, p2, p3], 0.25 + 0.75 * k)
        out.append(vignette(img, 0.5))
    encode('Anime_Any_Any_Critical_03', out)

# ---------------------------------------------------------------- Infuse_02 Ember (2.0 s): low-angle crane up through a fire ring -> ablaze, heat shimmer, rising embers
def infuse2():
    A = load('anime_inf2_a', 0.5); B = load('anime_inf2_b', 0.5)
    rnd = random.Random(41); N = 48; cut = 22; out = []
    for i in range(N):
        t = i / FPS
        if i < cut:
            k = i / cut
            img = view(A, zoom=1.2 + 0.12 * k, cx=0.5, cy=0.62 - 0.2 * ease_in(k))  # crane up
            img = heat_shimmer(img, t, amp=2 + 3 * k)
            img = add(img, particles(700, 90, t, (255, 160, 60), rise=True, size=(1.5, 4)))
            img = grade(img, bright=0.9 + 0.2 * k)
            if i >= cut - 3: img = flash(img, 0.3 * (i - cut + 4), (255, 170, 80))
        else:
            k = (i - cut) / (N - cut)
            dx, dy = shake(rnd, 8 * (1 - k) + 2)
            img = view(B, zoom=1.3 - 0.15 * ease_out(k), cx=0.5, cy=0.5, dx=dx, dy=dy)
            img = heat_shimmer(img, t, amp=4)
            img = add(img, particles(800, 140, t, (255, 190, 90), rise=True, size=(1.5, 5), speed=1.6))
            img = add(img, ring(60 + 700 * ((k * 2) % 1), 10, (255, 140, 40), int(210 * (1 - (k * 2) % 1))))
            if k < 0.12: img = flash(img, 0.85 * (1 - k / 0.12), (255, 200, 120))
        out.append(vignette(img, 0.65))
    encode('Anime_Any_Any_Infuse_02', out)

# ---------------------------------------------------------------- Infuse_03 Titan (2.0 s): slow lateral orbit-pan as stones gather -> camera-drop SLAM, dust, amber/stone grade
def infuse3():
    A = load('anime_inf3_a', 0.45); B = load('anime_inf3_b', 0.4)
    rnd = random.Random(43); N = 48; cut = 26; out = []
    for i in range(N):
        t = i / FPS
        if i < cut:
            k = i / cut
            img = view(A, zoom=1.3, cx=0.38 + 0.24 * k, cy=0.42, rot=-3 + 6 * k)  # pan + gentle roll (orbit feel)
            img = add(img, particles(900, 60, t, (210, 180, 130), rise=True, size=(2, 5), speed=0.4, alpha=150))
            img = grade(img, sat=0.85, tint=(120, 90, 50), tint_amt=0.08)
            if i >= cut - 4: img = view(A, zoom=1.3 + 0.35 * (i - cut + 4) / 4, cx=0.55, cy=0.38)  # quick push-in before the slam
        else:
            k = (i - cut) / (N - cut)
            amp = 46 * (1 - k) ** 2.5 if k > 0.04 else 46
            dx, dy = shake(rnd, amp * 0.4); dy += amp * (1 if i % 2 else -1)  # vertical-dominant slam shake
            img = view(B, zoom=1.18 - 0.08 * ease_out(k), cx=0.5, cy=0.45, dx=dx, dy=dy)
            img = add(img, particles(950, 120, t * 0.6, (200, 170, 120), rise=False, size=(2, 6), speed=0.5, alpha=170, drift=1))
            img = add(img, ring(80 + 1000 * ease_out(k), 30 * (1 - k) + 6, (230, 190, 120), int(200 * (1 - k)), cy=H * 0.72))
            img = grade(img, sat=0.9, contrast=1.1)
            if i < cut + 2: img = flash(img, 0.7, (255, 220, 150))
        out.append(vignette(img, 0.6))
    encode('Anime_Any_Any_Infuse_03', out)

# ---------------------------------------------------------------- KO_02 (2.5 s): dutch-angle ricochet, whip pan, then a slow pull-back into the void, fade to black
def ko2():
    A = load('anime_ko2_a', 0.3); B = load('anime_ko2_b', 0.5)
    rnd = random.Random(47); N = 60; cut = 26; out = []
    for i in range(N):
        t = i / FPS
        if i < cut:
            k = i / cut
            dx, dy = shake(rnd, 30 * (1 - k) + 4)
            img = view(A, zoom=1.75 - 0.15 * k, cx=0.45, cy=0.5, dx=dx, dy=dy, rot=-10 + 4 * k)  # dutch tilt
            if i < 3: img = flash(img, 0.8 - 0.25 * i, (255, 120, 230))
            img = chroma(img, 8 * (1 - k))
            if i >= cut - 5: img = hblur(img, 40 + 120 * (i - cut + 5) / 5)  # whip pan out
        else:
            k = (i - cut) / (N - cut)
            z = 1.7 - 0.65 * ease_out(k); img = view(B, zoom=z, cx=0.497, cy=0.42)  # pull back: the void opens up
            if i < cut + 4: img = hblur(img, 160 - 40 * (i - cut))  # whip pan in
            if 0.35 < k < 0.8:
                s = math.sin((k - 0.35) / 0.45 * math.pi)
                gx, gy = to_screen(B, z, 0.497, 0.42, 763, 363); img = add(img, star_glint(gx, gy, 15 + 60 * s, int(220 * s), color=(170, 255, 250)))
            if k > 0.8: img = Image.blend(img, Image.new('RGB', (W, H)), (k - 0.8) / 0.2 * 0.85)
        out.append(vignette(img, 0.6))
    encode('Anime_Any_Any_KO_02', out)

# ---------------------------------------------------------------- KO_03 (2.5 s): time-stop freeze (desaturated, slow creep, ticking pulses) -> color snaps back, tilt-down plunge with vertical lines, white splash ring
def ko3():
    A = load('anime_ko3_a', 0.5); B = load('anime_ko3_b', 0.5)
    N = 60; cut = 30; out = []
    for i in range(N):
        t = i / FPS
        if i < cut:
            k = i / cut
            img = view(A, zoom=1.15 + 0.12 * k, cx=0.5, cy=0.5)  # very slow creep while time is stopped
            if i < 2: img = impact_frame(img, invert=(i == 0))
            elif i % 8 in (0, 1):  # "tick" pulse rings around the ball
                img = add(img, ring(120 + 40 * (i % 8), 6, (190, 255, 120), 200))
            if i >= cut - 3:  # color snaps back
                img = flash(img, 0.25 * (i - cut + 4), (220, 255, 200))
        else:
            k = (i - cut) / (N - cut)
            img = view(B, zoom=1.15 + 0.5 * ease_in(k), cx=0.5, cy=0.42 + 0.12 * ease_in(k))  # tilt down + fall in
            img = vblur(img, 30 * (1 - k))
            img = add(img, straight_lines(1000 + i, n=60, vertical=True, color=(200, 255, 170), alpha=150, clear=(W / 2, 70)))
            if k > 0.72:
                kk = (k - 0.72) / 0.28
                img = add(img, ring(30 + 520 * ease_out(kk), 16 * (1 - kk) + 3, (255, 255, 255), int(255 * (1 - kk)), cy=H * 0.62))
                if kk < 0.2: img = flash(img, 0.6 * (1 - kk / 0.2))
            if i < cut + 2: img = flash(img, 0.7 - 0.3 * (i - cut), (220, 255, 200))
        out.append(vignette(img, 0.6))
    encode('Anime_Any_Any_KO_03', out)

B_Y0 = [0]
KO_HEAD = (float(os.environ.get('KO_HX', 1157)), float(os.environ.get('KO_HY', 77)))
def _ko_b_y0():
    im = Image.open(os.path.join(SRC, 'anime_ko_b.png')); w, h = im.size; ch = int(w * 9 / 16)
    B_Y0[0] = int(max(0, min(h - ch, 0.30 * h - ch / 2)))
_ko_b_y0()

which = sys.argv[3:] or ['critical', 'infuse', 'ko']
for w in which: globals()[w]()
