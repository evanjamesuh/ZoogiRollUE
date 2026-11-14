"""Shared kit textures: T_StoneDetail (1024^2 tiling grain, mean 0.5) and T_WaterStreaks (256x1024 tiling streaks)."""
import sys, os
import numpy as np
from arena_tex import value_noise, fbm, save_gray

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)

# Stone grain: soft blotches + fine speckle + a few pits, all tileable.
n = 1024
a = fbm(n, 8, 5, seed=101, wrap=True)
b = value_noise(n, 256, seed=202, wrap=True)
rng = np.random.default_rng(3)
pits = (rng.random((n, n)) > 0.9985).astype(np.float32)
from PIL import Image, ImageFilter
pits = np.asarray(Image.fromarray((pits * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
g = 0.5 + 0.28 * (a - a.mean()) + 0.12 * (b - b.mean()) - 0.6 * pits
save_gray(np.clip(g, 0, 1), os.path.join(OUT, "T_StoneDetail.png"))

# Water streaks: vertical, tileable in both directions (columns of stretched noise).
w, h = 256, 1024
cols = value_noise(1024, 64, seed=7, wrap=True)          # square tileable noise
img = Image.fromarray(cols).resize((w, h * 4), Image.BICUBIC)  # stretch vertically (x4) -> streaky
s = np.asarray(img, np.float32)[:h, :]
s = np.vstack([s[:h // 2], s[:h // 2][::-1]])            # mirror to keep vertical tiling seamless
fine = value_noise(1024, 256, seed=9, wrap=True)
fine = np.asarray(Image.fromarray(fine).resize((w, h), Image.BICUBIC), np.float32)
st = np.clip((s - 0.35) * 1.8, 0, 1) * 0.75 + fine * 0.25
save_gray(np.clip(st, 0, 1), os.path.join(OUT, "T_WaterStreaks.png"))
print("shared textures written")
