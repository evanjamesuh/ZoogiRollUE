# T_FrozenSky2: bright blue anime sky with soft painterly cumulus, light aurora + rainbow sun-flare streaks.
# Usage: python gen_sky_bright.py <SourceArt/Arenas/Frozen/T_FrozenSky.png> <out T_FrozenBrightSky.png>
# Keeps the r1 panorama's horizon mountain band and cloud sea (rows below elevation ~6 deg).
import numpy as np, cv2
from PIL import Image
W, H = 4096, 2048
src = np.asarray(Image.open(__import__('sys').argv[1]).convert('RGB')).astype(np.float32) / 255
v = (np.arange(H) + 0.5) / H
elev = 90 - v * 180                       # deg
E = np.repeat(elev[:, None], W, 1)
U = np.repeat(((np.arange(W) + 0.5) / W)[None, :], H, 0)
rng = np.random.default_rng(7)

def hexc(h): return np.array([int(h[i:i+2], 16) for i in (1, 3, 5)], np.float32) / 255
zen, mid, hor = hexc('#2C78D6'), hexc('#4E9BE6'), hexc('#86C4F2')
t = np.clip(E / 70, 0, 1)
sky = np.where((t < 0.35)[..., None], hor + (mid - hor) * (t / 0.35)[..., None], mid + (zen - mid) * np.clip((t - 0.35) / 0.65, 0, 1)[..., None] ** 0.8)

def wrapnoise(gh, gw, oct=6, pers=0.55):
    out = np.zeros((H, W), np.float32); amp = 1; tot = 0
    for o in range(oct):
        g = rng.random((gh * 2 ** o, gw * 2 ** o)).astype(np.float32)
        g = np.concatenate([g, g[:, :3]], 1)  # wrap in u
        big = cv2.resize(g, (int(W * (g.shape[1]) / (gw * 2 ** o)), H), interpolation=cv2.INTER_CUBIC)[:, :W]
        out += amp * big; tot += amp; amp *= pers
    return out / tot

n = wrapnoise(14, 18, 4, 0.5)
n2 = wrapnoise(28, 36, 3, 0.5)
# cumulus band: elev 4..48, densest 10..30; flattened bottoms (clouds sit on an invisible shelf)
band = np.clip((E - 4) / 8, 0, 1) * np.clip((52 - E) / 22, 0, 1)
dens = (n * 0.8 + n2 * 0.2) - 0.53 + 0.08 * band
mask = np.clip(dens / 0.09, 0, 1) * band
mask = cv2.GaussianBlur(mask, (0, 0), 5)
# lighting: top of a puff is lit (density gradient toward +elev = up = smaller row)
gy = np.zeros_like(dens); gy[1:-1] = dens[:-2] - dens[2:]   # positive where density grows upward... (row-1 is higher)
lit = np.clip(0.55 - gy * 40, 0, 1)
white, lav, blu = hexc('#FFFFFF'), hexc('#D9D4F4'), hexc('#A9BFEA')
core = np.clip(dens / 0.25, 0, 1)
ccol = white * lit[..., None] + (lav * (1 - core[..., None]) + blu * core[..., None]) * (1 - lit[..., None])
ccol = ccol * 0.65 + white * 0.35
out = sky * (1 - mask[..., None]) + ccol * mask[..., None]
# wispy high cirrus streaks
cir = wrapnoise(40, 10, 3); cir = np.clip((cir - 0.55) / 0.12, 0, 1) * np.clip((E - 30) / 10, 0, 1) * np.clip((70 - E) / 10, 0, 1)
cir = cv2.GaussianBlur(cir, (0, 0), 6)
out = out * (1 - 0.35 * cir[..., None]) + 0.35 * cir[..., None]
# rainbow sun flares: a few short spectral arcs/streaks near elev 18-40
def spectrum(x):  # x 0..1 -> rgb
    h = (x * 0.8 * 179).astype(np.uint8)
    hsv = np.stack([h, np.full_like(h, 200), np.full_like(h, 255)], -1)
    return cv2.cvtColor(hsv[None], cv2.COLOR_HSV2RGB)[0].astype(np.float32) / 255
for cu, ce, r, w, s in [(0.62, 26, 0.10, 0.012, 0.55), (0.13, 22, 0.08, 0.010, 0.4), (0.87, 30, 0.07, 0.010, 0.35)]:
    du = (U - cu + 0.5) % 1 - 0.5
    de = (E - ce) / 90
    rr = np.sqrt((du * 2) ** 2 + de ** 2)
    k = np.clip(1 - np.abs(rr - r) / w, 0, 1) * np.clip(1 - np.abs(du) / 0.06, 0, 1) * (de > -0.02)
    col = spectrum(np.clip((rr - r + w) / (2 * w), 0, 1).reshape(-1)).reshape(H, W, 3)
    out = out + col * (k * s)[..., None] * 0.6
    # soft glow + star burst at the flare centre
    g = np.exp(-((du * 2) ** 2 + de ** 2) / (2 * 0.012 ** 2))
    out = out + g[..., None] * 0.5 * s
# aurora from the original band, screened in lightly
aur = np.clip(src - sky, 0, 1) * (np.clip((E - 8) / 6, 0, 1) * np.clip((32 - E) / 6, 0, 1))[..., None]
out = 1 - (1 - out) * (1 - 0.45 * aur)
# below elevation ~7: original horizon mountains + cloud sea (cross-fade 7..12)
f = np.clip((E - 7) / 5, 0, 1)[..., None]
out = src * (1 - f) + out * f
out = np.clip(out, 0, 1)
Image.fromarray((out * 255).astype(np.uint8)).save(__import__('sys').argv[2])

print('ok')
