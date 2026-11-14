"""Lava kit textures (tiling, 1024^2 grayscale): T_LavaNoise (heat fbm) and T_LavaCrust (cooled crust plates: 1 inside a
plate, 0 along the glowing cracks between plates). Shared by the lava sea and the lava falls (M_ArenaLava)."""
import sys, os
import numpy as np
from scipy.spatial import cKDTree
from arena_tex import fbm, value_noise, smooth, save_gray

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)
n = 1024
heat = 0.65 * fbm(n, 6, 5, seed=301, wrap=True) + 0.35 * fbm(n, 24, 3, seed=302, wrap=True)
heat = (heat - heat.min()) / (heat.max() - heat.min())
save_gray(heat, os.path.join(OUT, "T_LavaNoise.png"))

rng = np.random.default_rng(303)
pts = rng.random((70, 2))
tiled = np.concatenate([pts + [dx, dy] for dx in (-1, 0, 1) for dy in (-1, 0, 1)])
tree = cKDTree(tiled)
ax = (np.arange(n) + 0.5) / n
wx = fbm(n, 8, 3, seed=304, wrap=True) - 0.5
wy = fbm(n, 8, 3, seed=305, wrap=True) - 0.5
X = ax[None, :] + wx * 0.04
Y = ax[:, None] + wy * 0.04
dist, _ = tree.query(np.stack([X.ravel(), Y.ravel()], -1), k=2)
f = (dist[:, 1] - dist[:, 0]).reshape(n, n)
plates = smooth(0.004, 0.03, f)
plates *= 0.75 + 0.25 * value_noise(n, 64, seed=306, wrap=True)
save_gray(np.clip(plates, 0, 1), os.path.join(OUT, "T_LavaCrust.png"))
print("lava textures written")
