# Meadow r7: draws the Buzzy Meadow floor seal (measured off topdown.png, 1 art px ~ 4.37 cm) as a glow mask.
# Texture spans the knock-off disc: r_px = r_cm / 1550 * 2048 (4096 px square). Run from the repo root.
import math
from PIL import Image, ImageDraw, ImageFilter
import numpy as np
N = 4096; K = 2048 / 1550.0; C = N / 2
im = Image.new("L", (N, N), 0); d = ImageDraw.Draw(im)
def ring(r, w): R = r * K; d.ellipse((C - R, C - R, C + R, C + R), outline=255, width=int(w))
def disc_ring(cx, cy, r, w): R = r * K; x = C + cx * K; y = C + cy * K; d.ellipse((x - R, y - R, x + R, y + R), outline=255, width=int(w))
def line(r0, r1, deg, w):
    a = math.radians(deg); d.line((C + math.cos(a) * r0 * K, C + math.sin(a) * r0 * K, C + math.cos(a) * r1 * K, C + math.sin(a) * r1 * K), fill=255, width=int(w))
ring(1330, 22)
ring(790, 12)
ring(480, 8)
for deg in (0, 90, 180, 270): line(480, 1330, deg, 10)
for deg in (45, 135, 225, 315): line(790, 1330, deg, 8)
for deg in range(0, 360, 45):
    a = math.radians(deg + 22.5); disc_ring(math.cos(a) * 790, math.sin(a) * 790, 90, 8)
for deg in (45, 135, 225, 315):
    a = math.radians(deg); disc_ring(math.cos(a) * 1150, math.sin(a) * 1150, 110, 9)
disc_ring(0, 0, 130, 8)
core = im.filter(ImageFilter.GaussianBlur(2))
halo = im.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(28))
c = np.asarray(core, float) / 255; h = np.asarray(halo, float) / 255; h = h / max(h.max(), 1e-3)
o = np.clip(np.maximum(np.clip(c * 1.4, 0, 1), h * 0.45), 0, 1)
Image.fromarray((o * 255).astype("uint8")).save("SourceArt/Arenas/MeadowSeal/T_MeadowGlow.png")
Image.fromarray((o * 255).astype("uint8")).resize((512, 512)).save("/tmp/seal7.png")
