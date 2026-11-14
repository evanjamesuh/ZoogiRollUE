# Slices Evan's ui_kit_sheet.png (2752x1536) and card_tablets_sheet.png (5504x3072) into Content/Data/HudV3.
# Boxes are given in "display" coords of a 1024-wide preview (kit) / 819-wide preview (tablets) and scaled here.
import numpy as np, cv2, sys, os
from PIL import Image
REF = os.environ.get('ZOOGI_HUDV3_REFDIR', '/workspace/ui-polish/stone-ref/')
OUT = sys.argv[1] if len(sys.argv) > 1 else '/workspace/ui-polish/hud-v3/tex2/'
os.makedirs(OUT, exist_ok=True)
kit = np.asarray(Image.open(REF + 'ui_kit_sheet.png').convert('RGB')).copy(); KS = 2752 / 1024
def disc(cx, cy, r, name, rin=0.0, size=128):
    X, Y, R = cx * KS, cy * KS, r * KS
    x0, y0, x1, y1 = int(X - R), int(Y - R), int(X + R), int(Y + R)
    c = kit[y0:y1, x0:x1]; h, w = c.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]; d = np.hypot(xx - w / 2, yy - h / 2) / (w / 2)
    a = np.clip((1 - d) * 40, 0, 1)
    if rin > 0: a *= np.clip((d - rin) * 40, 0, 1)
    im = Image.fromarray(np.dstack([c, a * 255]).astype(np.uint8)).resize((size, size), Image.LANCZOS); im.save(OUT + name); print(name)
def rect(box, name, inp=(), h=None):
    x0, y0, x1, y1 = [int(v * KS) for v in box]; img = kit
    if inp:
        m = np.zeros(kit.shape[:2], np.uint8)
        for b in inp: m[int(b[1] * KS):int(b[3] * KS), int(b[0] * KS):int(b[2] * KS)] = 255
        img = cv2.inpaint(kit, m, 5, cv2.INPAINT_TELEA)
    im = Image.fromarray(img[y0:y1, x0:x1])
    if h: im = im.resize((int(im.width * h / im.height), h), Image.LANCZOS)
    im.save(OUT + name); print(name)
def bracket(box, name, size=128):
    # prompt glyphs on the flat slate background: alpha = distance from the background colour
    x0, y0, x1, y1 = [int(v * KS) for v in box]; c = kit[y0:y1, x0:x1].astype(float)
    bg = np.median(np.concatenate([c[:4].reshape(-1, 3), c[-4:].reshape(-1, 3)]), 0)
    a = np.clip((np.abs(c - bg).sum(2) - 25) * 4, 0, 255)
    Image.fromarray(np.dstack([c, a]).astype(np.uint8)).resize((size, size), Image.LANCZOS).save(OUT + name); print(name)
# status frames (annulus only; the portrait is drawn inside)
disc(219, 76, 38, 'ring_cyan.png', 0.66); disc(300, 76, 38, 'ring_red.png', 0.66); disc(54, 162, 38, 'ring_bronze.png', 0.78)
# gauges: straight rune bars (empty / full) -> live fill by UV
rect((438, 199, 668, 221), 'rbar_empty.png', [(535, 203, 570, 217)], 48)
rect((438, 334, 668, 356), 'rbar_full.png', [(535, 338, 572, 352)], 48)
rect((436, 44, 650, 96), 'arc_cyan.png', (), 120)
# ring marks: filled / empty / cracked sockets
disc(755, 75, 34, 'mark_cyan.png'); disc(888, 75, 34, 'mark_fire.png'); disc(821, 75, 34, 'mark_empty.png'); disc(755, 285, 34, 'mark_cracked.png'); disc(955, 285, 34, 'mark_void.png')
# elemental icons: NOT used (Evan 4:06 PM: element visuals come from element_tablets.png)
if False:
  for n, x in (('ember', 60), ('titan', 150), ('storm', 245), ('frost', 340), ('phase', 435), ('void', 527)): disc(x, 487, 43, 'elem_' + n + '.png')
# interaction prompts
bracket((585, 420, 640, 475), 'prompt_cyan.png'); bracket((585, 495, 640, 550), 'prompt_gold.png')
bracket((940, 495, 998, 553), 'prompt_red.png'); bracket((870, 495, 928, 553), 'prompt_cyanbox.png'); bracket((800, 495, 858, 553), 'prompt_lock.png')

# ---------- card tablets: frame with cost number, name plate text and stars inpainted (drawn live)
tab = np.asarray(Image.open(REF + 'element_tablets.png').convert('RGB')).copy(); TS = 5504 / 819
names = ['earth', 'fire', 'storm', 'frost', 'water', 'nature', 'growth', 'void', 'solar', 'arcane']
cols = [(30, 165), (185, 320), (343, 478), (500, 635), (655, 790)]
for i, n in enumerate(names):
    r, c = divmod(i, 5); y0, y1 = (28, 218) if r == 0 else (238, 428); x0, x1 = cols[c]
    m = np.zeros(tab.shape[:2], np.uint8)
    for b in ((x0 + 8, y0 + 4, x0 + 30, y0 + 28), (x0 + 12, y1 - 68, x1 - 12, y1 - 24)):
        m[int(b[1] * TS):int(b[3] * TS), int(b[0] * TS):int(b[2] * TS)] = 255
    crop = (int(x0 * TS), int(y0 * TS), int(x1 * TS), int(y1 * TS))
    sub = tab[crop[1]:crop[3], crop[0]:crop[2]]; ms = m[crop[1]:crop[3], crop[0]:crop[2]]
    sub = cv2.inpaint(np.ascontiguousarray(sub), ms, 9, cv2.INPAINT_TELEA)
    Image.fromarray(sub).resize((360, 506), Image.LANCZOS).save(OUT + 'tablet_' + n + '.png'); print('tablet_' + n)
