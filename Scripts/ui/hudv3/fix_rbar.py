# hud-v3: the empty EPISODE rune track had a flat gray hourglass in the middle; continue the rune track through it
# and lift the runes a little so the empty bar reads at 0.
import numpy as np
from PIL import Image
p = 'w/Content/Data/HudV3/rbar_empty.png'
a = np.array(Image.open(p).convert('RGBA')).astype(np.float32)
y0, y1 = 9, 39
a[y0:y1, 196:306] = a[y0:y1, 84:194]
inner = a[y0:y1, 20:482, :3]
lum = inner.mean(2, keepdims=True)
inner += np.clip(lum - 55, 0, None) * np.array([0.25, 0.75, 0.95]) * 0.9   # runes glow faintly cyan
a[y0:y1, 20:482, :3] = np.clip(inner, 0, 255)
Image.fromarray(a.astype(np.uint8)).save(p)
print('ok')
