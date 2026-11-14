# T_FrozenSealHalo (RGBA): colours frozen-assist/ring-edge T_FrozenSealHalo_RE (L) cyan -> white with alpha = glow.
import numpy as np, sys
from PIL import Image
L = np.asarray(Image.open(sys.argv[1]).convert('L')).astype(np.float32) / 255
cyan = np.array([0.25, 0.84, 1.0]); white = np.array([0.85, 0.97, 1.0])
k = np.clip(L * 1.4, 0, 1)[..., None] ** 2
rgb = cyan * (1 - k) + white * k
a = np.clip(L * 1.15, 0, 1)
Image.fromarray(np.dstack([rgb * 255, a[..., None] * 255]).astype(np.uint8), 'RGBA').save(sys.argv[2])
