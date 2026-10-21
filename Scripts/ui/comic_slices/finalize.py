# Post-step after cut_slices.py: cap the long side at 1600 px (2x-safe for the overlay's 0.40 W / 0.56 W boxes at 1080p)
# and pad to multiples of 4 (transparent) so every texture compresses as BC7.
import glob, os
from PIL import Image
for p in sorted(glob.glob('/workspace/zoogi-unreal/comic-slices/*.png')):
    im = Image.open(p).convert('RGBA')
    w, h = im.size
    s = min(1.0, 1600.0 / max(w, h))
    if s < 1.0:
        im = im.resize((round(w * s), round(h * s)), Image.LANCZOS)
    w, h = im.size
    W, H = (w + 3) // 4 * 4, (h + 3) // 4 * 4
    if (W, H) != (w, h):
        c = Image.new('RGBA', (W, H), (0, 0, 0, 0)); c.paste(im, (0, 0)); im = c
    im.save(p, optimize=True)
    print(os.path.basename(p), im.size)
