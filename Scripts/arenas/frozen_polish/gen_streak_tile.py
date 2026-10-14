# T_FrozenStreakTile: tileable grayscale painterly vertical striations (glacier / ice-cliff faces). Mean ~0.5 so
# M_FrozenPiece's Color * lerp(1, d*2, DetailStrength) keeps the base colour on average. Usage: gen_streak_tile.py <out.png>
import numpy as np, sys, cv2
from PIL import Image
N = 1024; rng = np.random.default_rng(11)
def wrapcols(n):
    g = rng.random(n + 3).astype(np.float32); g[-3:] = g[:3]
    return cv2.resize(g[None], (N * (n + 3) // n, 1), interpolation=cv2.INTER_CUBIC)[0, :N]
u = sum(a * wrapcols(n) for a, n in [(0.45, 9), (0.3, 23), (0.18, 57), (0.07, 140)])
# slow vertical variation (painted strokes fade along their length), tileable in v
v = np.zeros((N, N), np.float32)
for a, ny, nx in [(0.5, 3, 18), (0.3, 6, 40)]:
    g = rng.random((ny + 3, nx + 3)).astype(np.float32); g[-3:] = g[:3]; g[:, -3:] = g[:, :3]
    big = cv2.resize(g, (N * (nx + 3) // nx, N * (ny + 3) // ny), interpolation=cv2.INTER_CUBIC)[:N, :N]
    v += a * big
img = u[None, :] * 0.8 + v * 0.35
img = (img - img.min()) / (img.max() - img.min())
img = np.round(img * 6) / 6 * 0.6 + img * 0.4          # light posterise: painted bands, not noise
img = cv2.GaussianBlur(img, (0, 0), 1.2)
img = 0.5 + (img - img.mean()) * 0.9
Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8), 'L').save(sys.argv[1])
