# r6: lower the cumulus band of T_FrozenBrightSky so a low camera (ref_aerial / hero) sees blue sky + clouds just above the
# far mountains. Elevation remap: new 1..60 deg <- old 13..60 deg (cumulus bodies start right above the far mountains),
# soft blend 0..1.5 deg to the original horizon, below 0 deg (cloud sea) and above 60 deg unchanged.
# Usage: python gen_sky_r6.py <in T_FrozenBrightSky.png> <out.png>
import sys, numpy as np
from PIL import Image
src = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(np.float32)
H, W, _ = src.shape
row = np.arange(H) + 0.5
E = 90 - row / H * 180
Eold = np.where((E >= 1) & (E <= 60), 13 + (E - 1) * (47 / 59), E)
rold = np.clip((90 - Eold) / 180 * H - 0.5, 0, H - 1)
r0 = np.floor(rold).astype(int); r1 = np.minimum(r0 + 1, H - 1); f = (rold - r0)[:, None, None]
remap = src[r0] * (1 - f) + src[r1] * f
b = np.clip((E - 0.0) / 1.5, 0, 1)[:, None, None]  # 0 deg original horizon -> 1.5 deg remapped sky
out = src * (1 - b) + remap * b
Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(sys.argv[2])
print('ok', W, H)
