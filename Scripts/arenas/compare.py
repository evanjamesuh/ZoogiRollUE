"""Before/after contact sheet: python3 compare.py <before_dir> <after_dir> <out.jpg> <slug>"""
import sys, glob, os
from PIL import Image, ImageDraw
b, a, out, slug = sys.argv[1:5]
views = ["aerial", "tactical", "standby", "hero", "low34", "rim", "hub"]
W, H = 640, 360
sheet = Image.new("RGB", (W * 2 + 30, (H + 24) * len(views) + 30), (20, 20, 24))
d = ImageDraw.Draw(sheet)
d.text((10, 8), "BEFORE", fill=(255, 255, 255)); d.text((W + 30, 8), "AFTER", fill=(255, 255, 255))
for i, v in enumerate(views):
    for j, src in enumerate((b, a)):
        f = glob.glob(os.path.join(src, "arena_%s_%d_%s.png" % (slug, i, v)))
        if not f: continue
        im = Image.open(f[0]).convert("RGB").resize((W, H), Image.LANCZOS)
        sheet.paste(im, (10 + j * (W + 10), 30 + i * (H + 24)))
    d.text((10, 30 + i * (H + 24) + H + 4), v, fill=(200, 200, 200))
sheet.save(out, quality=88)
print(out)
