# Procedural textures for the Awaken FX (launch ring, shock chain, void portal). Run with numpy + Pillow; output tex/*.png.
# Everything here is generated from scratch (no KriptoFX texture is used).
import numpy as np, math, os
from PIL import Image, ImageFilter
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tex'); os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)

def save(name, *ch):
    ch = [np.clip(c, 0, 1) for c in ch]
    while len(ch) < 3: ch.append(np.zeros_like(ch[0]))
    a = np.stack([(c * 255 + 0.5).astype(np.uint8) for c in ch[:3]], -1)
    Image.fromarray(a, 'RGB').save(os.path.join(OUT, name + '.png'))

def blur(a, r):
    im = Image.fromarray((np.clip(a, 0, 1) * 65535).astype(np.uint16).astype(np.int32), 'I')
    f = np.asarray(Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(r))).astype(float) / 255
    return f

def tile_noise(n, octaves=5, base=4, seed=0):
    r = np.random.default_rng(seed); out = np.zeros((n, n))
    amp = 1.0; tot = 0
    for o in range(octaves):
        f = base * 2 ** o
        g = r.random((f, f))
        # bicubic-ish periodic upsample
        im = Image.fromarray((g * 255).astype(np.uint8)).resize((f, f))
        big = np.tile(g, (3, 3))
        up = np.asarray(Image.fromarray((big * 255).astype(np.uint8)).resize((n * 3, n * 3), Image.BICUBIC)).astype(float) / 255
        out += amp * up[n:2 * n, n:2 * n]; tot += amp; amp *= 0.55
    out /= tot
    return (out - out.min()) / (out.max() - out.min())

N = 512
yy, xx = np.mgrid[0:N, 0:N]
u = (xx + 0.5) / N * 2 - 1; v = (yy + 0.5) / N * 2 - 1
r = np.sqrt(u * u + v * v); ang = (np.arctan2(v, u) + math.pi) / (2 * math.pi)

# 1. soft radial glow
save('T_ZAFX_Glow', np.exp(-(r / 0.42) ** 2) * (r < 1))

# 2. tileable fbm noise (pan in materials) + streaky vertical noise
noise = tile_noise(256, 5, 4, 1); save('T_ZAFX_Noise', noise, tile_noise(256, 5, 8, 2), tile_noise(256, 4, 2, 3))

# 3. fire ring band: R = ring line (thin hot core + glow + short licks), G = sweep order by angle, B = scorch band + cracks
R0 = 0.86
noise_a = tile_noise(512, 5, 6, 4)
jit = (np.interp(ang * 512, np.arange(512), noise_a[0]) - 0.5) * 0.025
d = r - (R0 + jit)
core = np.exp(-(d / 0.0065) ** 2)
glow = np.exp(-(d / 0.03) ** 2) * 0.45
licks = np.zeros_like(r)
for k in range(140):
    a0 = rng.random(); w = 0.002 + rng.random() * 0.004; L = 0.02 + rng.random() * 0.06
    da = np.abs(((ang - a0 + 0.5) % 1) - 0.5) * 2 * math.pi * R0
    side = 1 if rng.random() < 0.65 else -1
    t = np.clip(d * side / L, 0, 1)
    licks += np.exp(-(da / (w * (1 - t) + 1e-4)) ** 2) * (1 - t) * (d * side > -0.004) * (0.4 + 0.6 * rng.random())
ringR = np.clip(core + glow + licks * 0.8, 0, 1)
cr = np.zeros_like(r)
for k in range(90):  # scorch cracks crossing the ring
    a0 = rng.random() * 2 * math.pi; rr = R0 + (rng.random() - 0.5) * 0.12
    x, y = math.cos(a0) * rr, math.sin(a0) * rr; th = rng.random() * 2 * math.pi
    for s in range(int(10 + rng.random() * 25)):
        th += (rng.random() - 0.5) * 0.9; x += math.cos(th) * 0.006; y += math.sin(th) * 0.006
        px, py = int((x + 1) / 2 * N), int((y + 1) / 2 * N)
        if 0 <= px < N and 0 <= py < N: cr[py, px] = 1
cr = blur(cr, 0.8) * 3
scorch = np.clip(np.exp(-(d / 0.05) ** 2) * 0.75 + cr * 0.8, 0, 1)
inner = np.clip(1 - r / R0, 0, 1) ** 0.6 * (r < R0)          # soft inner floor glow (added in material with InnerGlow)
save('T_ZAFX_Ring', ringR, ang, scorch)
save('T_ZAFX_RingInner', inner)

# 4. flame lick sprite mask (vertical, base at bottom)
fy, fx = np.mgrid[0:256, 0:128]
fu = (fx + 0.5) / 128 * 2 - 1; fv = 1 - (fy + 0.5) / 256         # fv: 0 bottom .. 1 top
width = 0.75 * (1 - fv) ** 0.8 * np.clip(fv * 6, 0, 1) + 0.02
flame = np.clip(1 - np.abs(fu) / width, 0, 1) ** 1.3 * np.clip(1 - fv, 0, 1) ** 0.6
save('T_ZAFX_Flame', flame)

# 5. bolt cross-section (U across the strip): white core + violet glow falloff, V uniform
bu = (np.arange(64) + 0.5) / 64 * 2 - 1
prof = np.clip(np.exp(-(bu / 0.09) ** 2) + 0.45 * np.exp(-(bu / 0.4) ** 2), 0, 1)
save('T_ZAFX_Bolt', np.tile(prof, (16, 1)), np.tile(np.exp(-(bu / 0.09) ** 2), (16, 1)))

# 6. scorch splat for lightning hits: R = glow core, G = reveal (radius), B = dark splat with spiky streaks
spl = np.exp(-(r / 0.28) ** 2) * 0.9
for k in range(46):
    a0 = rng.random() * 2 * math.pi; L = 0.35 + rng.random() * 0.6; w = 0.012 + rng.random() * 0.025
    du = u * math.cos(a0) + v * math.sin(a0); dv = -u * math.sin(a0) + v * math.cos(a0)
    t = np.clip(du / L, 0, 1)
    spl += np.exp(-(dv / (w * (1 - t) + 0.002)) ** 2) * (du > 0) * (1 - t) ** 0.7
spl = np.clip(spl * (0.75 + 0.5 * noise_a), 0, 1) * (r < 1)
hot = np.exp(-(r / 0.12) ** 2)
save('T_ZAFX_Splat', hot, np.clip(1 - r, 0, 1), spl)

# 7. portal rim: thick soft ring with wisps (R), wobble noise ring (G)
pn = tile_noise(512, 5, 5, 9)
wob = (np.interp(ang * 512, np.arange(512), pn[100]) - 0.5) * 0.06
pd = r - (0.80 + wob)
rim = np.exp(-(pd / 0.045) ** 2) + 0.55 * np.exp(-(np.maximum(pd, 0) / 0.11) ** 2) * (pd > 0) + 0.25 * np.exp(-(np.minimum(pd, 0) / 0.12) ** 2) * (pd < 0)
wisp = np.clip(pn * 1.6 - 0.5, 0, 1) * np.exp(-(pd / 0.14) ** 2)
inside = np.clip((0.80 + wob - r) / 0.05, 0, 1)
save('T_ZAFX_PortalRim', np.clip(rim * 0.8 + wisp * 0.7, 0, 1), inside, pn)

# 8. star field (tileable): sharp points of different sizes + faint nebula in G
st = np.zeros((512, 512))
for k in range(700):
    x, y = rng.integers(0, 512, 2); b = rng.random() ** 3
    st[y, x] = max(st[y, x], 0.35 + 0.65 * b)
    if b > 0.6:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)): st[(y + dy) % 512, (x + dx) % 512] = max(st[(y + dy) % 512, (x + dx) % 512], b * 0.5)
neb = tile_noise(512, 5, 3, 12)
save('T_ZAFX_Stars', np.clip(st + blur(st, 1.2) * 0.8, 0, 1), np.clip(neb * 1.4 - 0.45, 0, 1))

# 9. round spark dot
save('T_ZAFX_Dot', np.exp(-(r / 0.35) ** 2) * (r < 1))
print('ok', sorted(os.listdir(OUT)))
