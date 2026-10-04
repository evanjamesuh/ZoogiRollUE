# Cosmic backdrop texture generator (procedural, numpy + Pillow + scipy). Run from Scripts/cosmic: python gen_nebula.py 4096 2048 && python finish_nebula.py && python gen_planets.py
# Output goes to ./tex; copy the PNGs to SourceArt/Cosmic and run Scripts/make_cosmic_backdrop.py (headless) to import.
import numpy as np
from PIL import Image
import sys
W, H = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (4096, 2048)
rng = np.random.default_rng(7)
PERM = np.concatenate([rng.permutation(256)] * 2).astype(np.int32)
G = rng.normal(size=(256, 3)).astype(np.float32); G /= np.linalg.norm(G, axis=1, keepdims=True)
def fade(t): return t * t * t * (t * (t * 6 - 15) + 10)
def perlin(x, y, z):
    xi = np.floor(x).astype(np.int32); yi = np.floor(y).astype(np.int32); zi = np.floor(z).astype(np.int32)
    xf = (x - xi).astype(np.float32); yf = (y - yi).astype(np.float32); zf = (z - zi).astype(np.float32)
    xi &= 255; yi &= 255; zi &= 255
    u, v, w = fade(xf), fade(yf), fade(zf)
    def grad(ix, iy, iz, dx, dy, dz):
        h = PERM[PERM[PERM[ix] + iy] + iz]
        g = G[h]
        return g[..., 0] * dx + g[..., 1] * dy + g[..., 2] * dz
    n000 = grad(xi, yi, zi, xf, yf, zf); n100 = grad(xi + 1, yi, zi, xf - 1, yf, zf)
    n010 = grad(xi, yi + 1, zi, xf, yf - 1, zf); n110 = grad(xi + 1, yi + 1, zi, xf - 1, yf - 1, zf)
    n001 = grad(xi, yi, zi + 1, xf, yf, zf - 1); n101 = grad(xi + 1, yi, zi + 1, xf - 1, yf, zf - 1)
    n011 = grad(xi, yi + 1, zi + 1, xf, yf - 1, zf - 1); n111 = grad(xi + 1, yi + 1, zi + 1, xf - 1, yf - 1, zf - 1)
    x1 = n000 + u * (n100 - n000); x2 = n010 + u * (n110 - n010)
    x3 = n001 + u * (n101 - n001); x4 = n011 + u * (n111 - n011)
    y1 = x1 + v * (x2 - x1); y2 = x3 + v * (x4 - x3)
    return y1 + w * (y2 - y1)
def fbm(p, oct, f=1.0, lac=2.03, gain=0.5, off=0.0):
    s = 0; a = 1.0; norm = 0
    for i in range(oct):
        s = s + a * perlin(p[0] * f + off + i * 17.3, p[1] * f + off * 1.7 + i * 5.1, p[2] * f - off + i * 9.7)
        norm += a; a *= gain; f *= lac
    return s / norm
def ridged(p, oct, f=1.0, off=0.0):
    s = 0; a = 1.0; norm = 0
    for i in range(oct):
        n = 1.0 - np.abs(perlin(p[0] * f + off + i * 3.3, p[1] * f + i * 7.7, p[2] * f + off + i * 1.1))
        s = s + a * n * n; norm += a; a *= 0.55; f *= 2.1
    return s / norm
def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)
def hexc(h): return np.array([int(h[i:i + 2], 16) / 255.0 for i in (1, 3, 5)], np.float32)
BASE_DEEP = hexc('#05051a'); BASE_MID = hexc('#0d0b33')
PURPLE = hexc('#7a3cff'); BLUE = hexc('#2b4dff'); CYAN = hexc('#22c8ff'); MAGENTA = hexc('#ff2f9a'); ORANGE = hexc('#ff9a2e'); GOLD = hexc('#ffd36a')
out = np.zeros((H, W, 4), np.float32)
CH = 128
for r0 in range(0, H, CH):
    rows = np.arange(r0, min(H, r0 + CH))
    v = (rows + 0.5) / H; u = (np.arange(W) + 0.5) / W
    lat = (0.5 - v) * np.pi; lon = (u - 0.5) * 2 * np.pi
    LAT, LON = np.meshgrid(lat, lon, indexing='ij')
    d = np.stack([np.cos(LAT) * np.cos(LON), np.cos(LAT) * np.sin(LON), np.sin(LAT)]).astype(np.float32)
    # domain warp
    wq = fbm(d, 4, f=1.6, off=3.0); wr = fbm(d, 4, f=1.6, off=11.0); ws = fbm(d, 4, f=1.6, off=23.0)
    p = d + 0.55 * np.stack([wq, wr, ws])
    dens = fbm(p, 6, f=2.2, off=1.0) * 0.5 + 0.5
    dens = smooth(0.42, 0.78, dens)
    big = fbm(d, 3, f=0.9, off=40.0) * 0.5 + 0.5          # where the nebula lives at all
    region = smooth(0.38, 0.72, big)
    fil = ridged(p * 1.0, 5, f=3.0, off=5.0)               # wispy filaments
    fil = smooth(0.55, 0.95, fil)
    hue = np.clip(fbm(d + 0.3 * np.stack([wr, ws, wq]), 3, f=1.3, off=60.0) * 1.6 + 0.5, 0, 1)
    hue2 = np.clip(fbm(p, 3, f=1.8, off=80.0) * 1.7 + 0.5, 0, 1)
    dust = smooth(0.55, 0.8, fbm(p, 5, f=4.0, off=90.0) * 0.5 + 0.5)
    # colour ramp: purple->blue->cyan on one side, magenta->orange on the other
    t = hue[..., None]
    cool = np.where(t < 0.5, PURPLE + (BLUE - PURPLE) * (t / 0.5), BLUE + (CYAN - BLUE) * ((t - 0.5) / 0.5))
    warm = MAGENTA + (ORANGE - MAGENTA) * smooth(0.72, 0.97, hue2)[..., None]
    mixw = smooth(0.45, 0.75, hue2)[..., None]
    col = cool * (1 - mixw) + warm * mixw
    base = BASE_DEEP + (BASE_MID - BASE_DEEP) * (fbm(d, 3, f=1.2, off=100.0)[..., None] * 0.5 + 0.5)
    glow = (dens * region)[..., None]
    filam = (fil * region * (0.4 + 0.6 * dens))[..., None]
    c = base + col * glow * 0.85 + col * filam * 1.3 + GOLD * (filam * glow) * 0.35
    c = c * (1 - 0.6 * dust[..., None] * glow)
    # elevation shaping: board sits over the lower hemisphere -> calmer there, richest band around the horizon
    el = np.sin(LAT)[..., None]
    shape = 0.55 + 0.45 * np.exp(-(el / 0.55) ** 2)
    shape = np.where(el < 0, 0.45 + 0.55 * np.exp(-(el / 0.45) ** 2), shape)
    c = c * shape
    # near wisp mask for the parallax layer (alpha)
    near = smooth(0.6, 0.95, ridged(p + 0.2, 4, f=5.0, off=33.0)) * smooth(0.45, 0.7, fbm(d, 3, f=1.8, off=70.0) * 0.5 + 0.5)
    out[rows, :, :3] = c
    out[rows, :, 3] = near
    print(r0, flush=True)
np.save('tex/neb_raw.npy', out)
