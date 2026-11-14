# hud-v3: pill_on.png was cut from the target with the plate's "• P1" text in its bottom-right corner; clear it.
import numpy as np
from PIL import Image
p = 'w/Content/Data/HudV3/pill_on.png'
a = np.array(Image.open(p).convert('RGBA'))
a[62:100, 255:312, 3] = 0
# soft edge so the glow doesn't end in a hard line
for i, x in enumerate(range(245, 255)): a[62:100, x, 3] = (a[62:100, x, 3] * (1 - (i + 1) / 11)).astype(np.uint8)
Image.fromarray(a).save(p); print('ok')
