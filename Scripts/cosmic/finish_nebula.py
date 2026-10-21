# Cosmic backdrop texture generator (procedural, numpy + Pillow + scipy). Run from Scripts/cosmic: python gen_nebula.py 4096 2048 && python finish_nebula.py && python gen_planets.py
# Output goes to ./tex; copy the PNGs to SourceArt/Cosmic and run Scripts/make_cosmic_backdrop.py (headless) to import.
import numpy as np
from PIL import Image
o = np.load('tex/neb_raw.npy'); H, W = o.shape[:2]
rng = np.random.default_rng(3)
c = o[..., :3] * 1.15
# baked stars (soft dots, widened in u by 1/cos(lat) so they stay round on the sphere)
N = 9000
z = rng.uniform(-1, 1, N); lon = rng.uniform(-np.pi, np.pi, N); lat = np.arcsin(z)
u = (lon / (2 * np.pi) + 0.5) * W; v = (0.5 - lat / np.pi) * H
b = rng.pareto(2.2, N) * 0.25 + 0.15
b = np.clip(b, 0, 2.5)
tints = np.array([[1, 1, 1], [0.75, 0.85, 1], [1, 0.85, 0.7], [0.85, 0.75, 1], [0.7, 1, 1]], np.float32)
tc = tints[rng.integers(0, len(tints), N)]
for i in range(N):
    sz = 0.7 + 0.9 * min(b[i], 1.5)              # sigma in texels
    sx = sz / max(np.cos(lat[i]), 0.08)
    r = int(np.ceil(3 * max(sx, sz)))
    x0, y0 = int(u[i]), int(v[i])
    ys = np.arange(y0 - r, y0 + r + 1); xs = np.arange(x0 - r, x0 + r + 1)
    ys = ys[(ys >= 0) & (ys < H)]
    if len(ys) == 0: continue
    gx = np.exp(-((xs + 0.5 - u[i]) / sx) ** 2 * 0.5); gy = np.exp(-((ys + 0.5 - v[i]) / sz) ** 2 * 0.5)
    g = gy[:, None] * gx[None, :] * b[i]
    c[np.ix_(ys, xs % W)] += g[..., None] * tc[i]
c = np.clip(c, 0, 1)
a = np.clip(o[..., 3], 0, 1)
img = np.dstack([c, a[..., None]])
Image.fromarray((img * 255 + 0.5).astype(np.uint8), 'RGBA').save('tex/T_CosmicNebula.png')
Image.fromarray((c * 255).astype(np.uint8)).resize((1024, 512), Image.LANCZOS).save('tex/T_CosmicNebula_prev.jpg', quality=90)
print('ok', W, H)
