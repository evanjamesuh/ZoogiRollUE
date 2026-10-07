"""Frozen Ring textures derived from Evan's Buzzy art (Oct 6).
T_FrozenFloor / T_FrozenGlow: the top-down court rectified (ellipse fit on the cyan boundary ring -> circle, r 1540 cm of a
+-1550 cm square; image column = +X, row = +Y), painted obelisks and the hub crystal inpainted (3D pieces stand there),
pillar / obelisk sockets added. T_FrozenSky: equirect panorama from the hero sky / aurora / mountains + top-down cloud sea.
Tiles: ice (hex ice patch), snow, stone (arch / pillar stone) detail, rank-coded crack network. Scar decal from damaged.png.
Usage: python3 gen_frozen.py <buzzy target dir> <incoming dir> <out dir>"""
import sys, os, math, shutil
import numpy as np
import cv2
from PIL import Image, ImageFilter

TGT, INC, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(OUT, exist_ok=True)
N = 2048
HALF = 1550.0
RING = 1540.0


def load(name):
    return np.asarray(Image.open(os.path.join(TGT, name)).convert('RGB')).astype(np.float32)


def rectify(S, cx, cy, ax, ay, n=N, half=HALF, ring=RING):
    pxcm = ax / ring
    u = ((np.arange(n) + 0.5) / n * 2 - 1) * half
    X, Y = np.meshgrid(u, u)
    sx = (cx + X * pxcm).astype(np.float32)
    sy = (cy + Y * pxcm * (ay / ax)).astype(np.float32)
    return cv2.remap(S, sx, sy, cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT)


def to_px(x, y, n=N):  # world cm -> texture px
    return (x / (2 * HALF) + 0.5) * n, (y / (2 * HALF) + 0.5) * n


# ------------------------------------------------------------------ floor
td = load('topdown.png')
F = rectify(td, 1371, 780, 622, 560)
F = np.clip(F, 0, 255)
s = N / 1024.0
mask = np.zeros((N, N), np.uint8)
# painted obelisks (1024-space boxes from the rectified art) -> inpaint
for (x0, x1, y0, y1) in [(387, 442, 267, 382), (577, 632, 252, 372), (722, 772, 422, 512), (632, 682, 622, 732), (382, 437, 657, 767), (297, 352, 522, 632)]:
    cxp, cyp = (x0 + x1) / 2 * s, (y0 + y1) / 2 * s
    cv2.ellipse(mask, (int(cxp), int(cyp)), (int((x1 - x0) / 2 * s * 1.15), int((y1 - y0) / 2 * s * 1.08)), 0, 0, 360, 255, -1)
# hub crystal
cv2.ellipse(mask, (int(512 * s), int(497 * s)), (int(33 * s), int(50 * s)), 0, 0, 360, 255, -1)
Fi = cv2.inpaint(F.astype(np.uint8), mask, 9, cv2.INPAINT_TELEA).astype(np.float32)
# soften the inpainted areas with a matched local blur so they read as ice, not smears
blur = cv2.GaussianBlur(Fi, (0, 0), 6)
mk = cv2.GaussianBlur(mask.astype(np.float32) / 255.0, (0, 0), 5)[..., None]
Fi = Fi * (1 - mk * 0.5) + blur * (mk * 0.5)

# lift the painted obelisk shadow bands (real pillars cast real shadows now): illumination = large closing of luma
Lr = Fi.mean(2)
k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (61, 61))
illum = cv2.GaussianBlur(cv2.morphologyEx(Lr, cv2.MORPH_CLOSE, k), (0, 0), 20)
ratio = np.clip(Lr / np.maximum(illum, 1), 0, 1.2)
yy0, xx0 = np.mgrid[0:N, 0:N]
rr0 = np.hypot((xx0 + 0.5) / N * 2 * HALF - HALF, (yy0 + 0.5) / N * 2 * HALF - HALF)
band = np.clip((0.93 - ratio) / 0.12, 0, 1) * np.clip((1150 - rr0) / 80.0, 0, 1)
band = cv2.morphologyEx(band.astype(np.float32), cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (25, 25)))
band = cv2.GaussianBlur(band, (0, 0), 6)[..., None]
gain = np.clip(illum / np.maximum(Lr, 1), 1, 1.6)[..., None]
Fi = Fi * (1 - band * 0.85) + np.clip(Fi * gain * 0.97, 0, 255) * (band * 0.85)
# glow mask: thin, bright, cyan rune lines only (snow edges and ice-plate edges are kept faint)
L = Fi.mean(2)
Lb = cv2.GaussianBlur(L, (0, 0), 8)
cyan = np.clip((Fi[..., 2] - Fi[..., 0] - 25) / 40.0, 0, 1)
line = np.clip((L - Lb - 1) / 12.0, 0, 1)
bright = np.clip((L - 150) / 50.0, 0, 1)
yy, xx = np.mgrid[0:N, 0:N]
rr = np.hypot((xx + 0.5) / N * 2 * HALF - HALF, (yy + 0.5) / N * 2 * HALF - HALF)
inner = np.clip((1180 - rr) / 60.0, 0, 1)
glow = np.clip(line * cyan * bright * (0.25 + 0.75 * inner) * 1.8, 0, 1)
glow = cv2.GaussianBlur(glow, (0, 0), 0.8)
# boundary ring always glows
glow = np.maximum(glow, np.clip(1 - np.abs(rr - 1528) / 22.0, 0, 1) * 0.95)
glow = np.maximum(glow, np.clip(1 - np.abs(rr - 1500) / 9.0, 0, 1) * 0.5)

# sockets: pillar (22.5 + 90k) and obelisk (67.5 + 90k) bases at r 680, plus the hub medallion ring
for k in range(8):
    a = math.radians(22.5 + 45 * k)
    px, py = to_px(math.cos(a) * 680, math.sin(a) * 680)
    rp = 70 / (2 * HALF) * N
    d = np.hypot(xx - px, yy - py)
    ringm = np.clip(1 - np.abs(d - rp) / 3.5, 0, 1)
    disc = np.clip((rp - d) / 3.0, 0, 1)
    Fi = Fi * (1 - disc[..., None] * 0.35) + disc[..., None] * np.array([150, 205, 235]) * 0.35
    Fi = Fi * (1 - ringm[..., None] * 0.6) + ringm[..., None] * np.array([200, 245, 255]) * 0.6
    glow = np.maximum(glow, ringm)
hub = np.clip(1 - np.abs(rr - 118) / 4.0, 0, 1)
glow = np.maximum(glow, hub * 0.8)
Image.fromarray(np.clip(Fi, 0, 255).astype(np.uint8)).save(os.path.join(OUT, 'T_FrozenFloor.png'))
Image.fromarray((glow * 255).astype(np.uint8)).save(os.path.join(OUT, 'T_FrozenGlow.png'))
print('floor ok')

# ------------------------------------------------------------------ sky panorama (equirect 4096 x 2048)
hero = load('hero.png')  # 2752 x 1536
H, W = hero.shape[:2]
PW, PH = 4096, 2048
pano = np.zeros((PH, PW, 3), np.float32)
# vertical gradient sky (zenith deep blue -> horizon pale)
el = (0.5 - (np.arange(PH) + 0.5) / PH) * 180.0  # elevation deg, +90 top
zen = np.array([60, 120, 200], np.float32); hor = np.array([205, 228, 245], np.float32)
t = np.clip(el / 60.0, 0, 1)[:, None]
pano[:] = (hor * (1 - t) + zen * t)[:, None, :]
# hero sky (aurora, sun) rows 0..0.18H over the full width -> elevation 42..12 deg, mirrored to loop
def loop(img):
    return np.concatenate([img, img[:, ::-1]], axis=1)
sky = loop(hero[0:int(H * 0.18)])
e = lambda d: int((90 - d) / 180 * PH)
r0, r1 = e(42), e(12)
skyR = cv2.resize(sky, (PW, r1 - r0), interpolation=cv2.INTER_AREA)
fade = np.clip((np.arange(r1 - r0) / (r1 - r0)) / 0.3, 0, 1)[:, None, None]
pano[r0:r1] = pano[r0:r1] * (1 - fade) + skyR * fade
# mountain range: hero side columns only (no arena), rows 0.14..0.36H -> elevation 14..-6 deg
lm = hero[int(H * 0.14):int(H * 0.36), 0:int(W * 0.24)]
rm = hero[int(H * 0.14):int(H * 0.36), int(W * 0.80):]
mts = np.concatenate([lm, rm, lm[:, ::-1], rm[:, ::-1]], axis=1)
r2, r3 = e(14), e(-6)
mR = cv2.resize(mts, (PW, r3 - r2), interpolation=cv2.INTER_AREA)
mf = np.clip((np.arange(r3 - r2) / (r3 - r2)) / 0.22, 0, 1)[:, None, None]
pano[r2:r3] = pano[r2:r3] * (1 - mf) + mR * mf
# below: soft painted cloud sea (procedural fbm, art palette), mountain feet fading into it
rng = np.random.default_rng(5)
def fbm(h, w, oct=5):
    out = np.zeros((h, w), np.float32); amp = 1.0; tot = 0
    for o in range(oct):
        gh, gw = 4 * 2 ** o, 8 * 2 ** o
        g = rng.random((gh, gw + 1)).astype(np.float32); g[:, -1] = g[:, 0]
        up = cv2.resize(g, (w + w // gw, h), interpolation=cv2.INTER_CUBIC)[:, :w]
        out += up * amp; tot += amp; amp *= 0.5
    return out / tot
hh = PH - r3
c = fbm(hh, PW)
c = np.clip((c - 0.35) / 0.4, 0, 1)
c = cv2.GaussianBlur(c, (0, 0), 3)
deep = np.array([120, 160, 205], np.float32); cloud = np.array([236, 244, 252], np.float32)
low = deep * (1 - c[..., None]) + cloud * c[..., None]
lf = np.clip(np.arange(hh) / 70.0, 0, 1)[:, None, None]
pano[r3:] = pano[r3 - 1:r3] * (1 - lf) + low * lf
Image.fromarray(np.clip(pano, 0, 255).astype(np.uint8)).save(os.path.join(OUT, 'T_FrozenSky.png'))
print('sky ok')


# ------------------------------------------------------------------ tiles
def tileable_gray(crop, size=512, gain=1.0):
    g = cv2.cvtColor(crop.astype(np.uint8), cv2.COLOR_RGB2GRAY).astype(np.float32)
    g = cv2.resize(g, (size, size), interpolation=cv2.INTER_AREA)
    hp = g - cv2.GaussianBlur(g, (0, 0), size / 8)
    hp = hp / (hp.std() + 1e-3) * 0.16 * gain + 0.5
    # offset-blend for seams
    sh = np.roll(np.roll(hp, size // 2, 0), size // 2, 1)
    yy, xx = np.mgrid[0:size, 0:size] / size
    w = np.clip(np.minimum(np.minimum(xx, 1 - xx), np.minimum(yy, 1 - yy)) * 6, 0, 1)
    out = hp * w + sh * (1 - w)
    return np.clip(out, 0, 1)

hub = load('hub.png')
ice = tileable_gray(td[200:520, 1700:2020], gain=1.2)      # NE hex ice sheet
snow = tileable_gray(td[1350:1530, 1150:1450], gain=0.6)   # snow lip
stone = tileable_gray(hub[600:1100, 1900:2050], gain=0.9)   # pillar stone
for name, a in [('T_FrozenIceTile', ice), ('T_FrozenSnowTile', snow), ('T_FrozenStoneTile', stone)]:
    Image.fromarray((a * 255).astype(np.uint8)).save(os.path.join(OUT, name + '.png'))

# rank-coded crack network (Worley edges, tileable): 1.0 main cracks, 0.7 secondary, 0.45 hairlines
def worley_edges(size, cells, seed):
    rng = np.random.default_rng(seed)
    pts = rng.random((cells, cells, 2))
    yy, xx = np.mgrid[0:size, 0:size] / size * cells
    f1 = np.full((size, size), 9.0); f2 = np.full((size, size), 9.0)
    for oy in range(-1, 2):
        for ox in range(-1, 2):
            for cy in range(cells):
                for cx in range(cells):
                    px = cx + ox * cells + pts[cy, cx, 0]; py = cy + oy * cells + pts[cy, cx, 1]
                    if abs(px - cells / 2) > cells or abs(py - cells / 2) > cells: continue
                    d = np.hypot(xx - px, yy - py)
                    f2 = np.where(d < f1, f1, np.minimum(f2, d)); f1 = np.minimum(f1, d)
    return f2 - f1

S = 512
e1 = worley_edges(S, 4, 1); e2 = worley_edges(S, 8, 2); e3 = worley_edges(S, 16, 3)
crack = np.zeros((S, S), np.float32)
crack = np.maximum(crack, (e3 < 0.035) * 0.42)
crack = np.maximum(crack, (e2 < 0.045) * 0.68)
crack = np.maximum(crack, (e1 < 0.05) * 1.0)
crack = cv2.resize(crack, (1024, 1024), interpolation=cv2.INTER_LINEAR)
Image.fromarray((crack * 255).astype(np.uint8)).save(os.path.join(OUT, 'T_FrozenCrackTile.png'))
print('tiles ok')

# ------------------------------------------------------------------ decals
for d in ['Crack', 'Crater', 'Frost', 'GlowCrack', 'Shards']:
    shutil.copy(os.path.join(INC, 'T_FrozenDmg_%s.png' % d), os.path.join(OUT, 'T_FrozenDmg_%s.png' % d))
# art crater (damaged.png, right crater) with an alpha matte: dark soot + cyan cracks vs. local ice
dm = load('damaged.png')
cx, cy, R = int(1745), int(925), 300
crop = dm[cy - R:cy + R, cx - R:cx + R].copy()
Lc = crop.mean(2)
cyan_c = np.clip((crop[..., 1] + crop[..., 2]) / 2 - crop[..., 0] - 60, 0, 120) / 120.0
dark = np.clip((165 - Lc) / 90.0, 0, 1)
yy, xx = np.mgrid[0:2 * R, 0:2 * R]
rad = np.hypot(xx - R, yy - R) / R
alpha = np.clip(np.maximum(dark, cyan_c * 0.9) * np.clip((1 - rad) / 0.25, 0, 1), 0, 1)
alpha = cv2.GaussianBlur(alpha, (0, 0), 1.2)
rgba = np.dstack([crop, alpha * 255]).astype(np.uint8)
Image.fromarray(rgba, 'RGBA').resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, 'T_FrozenScar_Crater.png'))
print('decals ok')
