# Cuts Evan's Episode Engine comic art into transparent PNG slices/panels along their real borders (black gutters).
# Output: /workspace/zoogi-unreal/comic-slices/<name>.png (RGBA, trimmed, native resolution) + contact_sheet.jpg
import numpy as np, cv2, os
from PIL import Image, ImageDraw
OUT = '/workspace/zoogi-unreal/comic-slices'
REF = '/workspace/zoogi-unreal/episode-engine/refs_2026-10-08/'
ATT = '/home/box/agent-data/agents/f4f1f49a-5970-424b-8312-7120d45986ad/attachments/'
def load(p): return np.asarray(Image.open(p).convert('RGB'))
def save(name, rgb, mask, feather=1.2, border=0):
    m = (mask > 0).astype(np.uint8) * 255
    if border:  # thin black comic outline just inside the cut
        er = cv2.erode(m, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * border + 1, 2 * border + 1)))
        rgb = rgb.copy(); rgb[(m > 0) & (er == 0)] = (8, 8, 12)
    a = cv2.GaussianBlur(m, (0, 0), feather) if feather else m
    a[m == 0] = np.minimum(a[m == 0], 0)  # never bleed outside the cut
    ys, xs = np.nonzero(m)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    rgba = np.dstack([rgb, a])[y0:y1, x0:x1]
    Image.fromarray(rgba, 'RGBA').save(f'{OUT}/{name}.png')
    print(name, rgba.shape[1], 'x', rgba.shape[0])
def poly(shape, pts):
    m = np.zeros(shape[:2], np.uint8); cv2.fillPoly(m, [np.array(pts, np.int32)], 1); return m
def textmask(im, rect, white=200, grow=7, minh=40):
    # big callout word inside rect: white letters + their black outline (dilated), nothing else
    x0, y0, x1, y1 = rect
    m = np.zeros(im.shape[:2], np.uint8)
    sub = im[y0:y1, x0:x1]
    w = (sub.min(2) > white).astype(np.uint8)
    w = cv2.morphologyEx(w, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, st, _ = cv2.connectedComponentsWithStats(w, 8)  # letters only: drop small white specks (snow, glints)
    keep = np.zeros(n, bool); keep[1:] = st[1:, cv2.CC_STAT_HEIGHT] >= minh
    w = keep[lab].astype(np.uint8)
    w = cv2.dilate(w, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * grow + 1, 2 * grow + 1)))
    m[y0:y1, x0:x1] = w
    return m

# ---- 1. eye / reaction slices (black background, diagonal bands): component -> convex hull
im = load(REF + 'comic_slice_pack1_eyes.png')
blk = (im.max(2) < 30).astype(np.uint8)
op = cv2.morphologyEx(blk, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9)))
n, lab, st, cen = cv2.connectedComponentsWithStats(1 - op, 8)
comps = sorted([i for i in range(1, n) if st[i][4] > 50000], key=lambda i: st[i][4])
names = {}
for i in comps:
    # classify by mean hue of the saturated pixels (OpenCV hue 0-180)
    hsv = cv2.cvtColor(im, cv2.COLOR_RGB2HSV)[lab == i]
    hsv = hsv[hsv[:, 1] > 120]
    hue = np.median(hsv[:, 0])
    if 80 <= hue <= 130: nm = 'slice_eyes_blue'
    elif 35 <= hue < 80: nm = 'slice_eyes_green_scan'
    elif 9 <= hue < 35: nm = 'slice_eyes_orange_surprised'
    else: nm = 'slice_eyes_red_angry'
    print('eye comp', i, 'hue', hue, nm)
    names[nm] = i
for nm, i in names.items():
    hull = cv2.convexHull(np.column_stack(np.nonzero(lab == i))[:, ::-1].astype(np.int32))
    m = np.zeros(im.shape[:2], np.uint8); cv2.fillPoly(m, [hull], 1)
    m &= (1 - (lab != i) * (lab > 0) * 1).astype(np.uint8)  # never take a neighbour's pixels
    save(nm, im, m, border=0)

# ---- 2. multi-panel (HALO BANK / SKY BUMPER / AIR REACTION / ORB LINE / PARTNER INTERCEPT)
im = load(ATT + 'a1d55ac177d5860090b76b401f45d0166bccb2b7715c925bd684aaae87ef06de.png')
S = im.shape[1] / 1024.0  # coords below picked on the 1024-wide preview
P = lambda pts: [(int(x * S), int(y * S)) for x, y in pts]
R = lambda r: tuple(int(v * S) for v in r)
gut = (im.max(2) < 45).astype(np.uint8)
gut = cv2.morphologyEx(gut, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))
n, lab, st, cen = cv2.connectedComponentsWithStats(1 - gut, 8)
def comp_at(x, y): return (lab == lab[int(y * S), int(x * S)]).astype(np.uint8)
def closed(m):  # fill speed-line holes inside a closed panel
    cs, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    out = np.zeros_like(m); cv2.drawContours(out, [max(cs, key=cv2.contourArea)], -1, 1, -1); return out
panels = {
    'panel_sky_bumper': (closed(comp_at(150, 290)), (20, 330, 260, 420)),
    'panel_orb_line': (closed(comp_at(900, 290)), (840, 335, 1003, 401)),
    'panel_partner_intercept': (closed(comp_at(500, 520)), (535, 460, 720, 565)),
}
# HALO BANK top wedge: the bottom gutter is broken by the word, so its polygon follows the gutter lines by hand.
halo = poly(im.shape, P([(272, 0), (411, 166), (652, 133), (757, 0)])) & (1 - gut)
panels['panel_halo_bank'] = (halo, (510, 95, 690, 165))
# AIR REACTION: the open centre between the four wedges (reticle + word), cut as a polygon along the wedge gutters.
air = poly(im.shape, P([(415, 188), (700, 168), (765, 205), (765, 250), (683, 326), (612, 404), (420, 440), (342, 330), (300, 238)])) & (1 - gut)
_n, _lab, _st, _ = cv2.connectedComponentsWithStats(air.astype(np.uint8), 8)
air = (_lab == 1 + int(np.argmax(_st[1:, cv2.CC_STAT_AREA]))).astype(np.uint8)  # drop the neighbour sliver past the gutter
panels['panel_air_reaction'] = (air, None)  # top edge kept under the HALO BANK word; right edge takes the whole AIR REACTION word
for nm, (m, tr) in panels.items():
    if tr: m = m | textmask(im, R(tr))
    save(nm, im, m, border=3)

# ---- 3. full-frame action panels (no gutters): diagonal-cut comic slices with a black border
def slant(shape, top, bot, sk):  # parallelogram band: left edge leans sk px
    h, w = shape[:2]
    return poly(shape, [(sk, top), (w, top), (w - sk, bot), (0, bot)])
im = load(ATT + '9863b0e22548d634ec39aa05e82f72bbed533e0c698143c5fe28e7b7bbb113e5.png')
save('panel_air_vs_stone', im, slant(im.shape, 0, im.shape[0], 140), border=6)
im = load(ATT + 'fbd8cc3d17c1c3a1fbb4a98172f4c09ebec42cd6eeb23c22f113bd9e5e631753.png')
h, w = im.shape[:2]
# drop the title strip (top 130 px) and cut a diagonal corner that removes the mechanics info box (bottom-left)
save('panel_sky_bumper_impact', im, poly(im.shape, [(140, 132), (w, 132), (w - 140, h), (1100, h), (720, 820), (0, 820)]), border=6)
im = load(REF + 'resonance_halo_detail.png')
h, w = im.shape[:2]
save('panel_halo_rebound', im, poly(im.shape, [(140, 170), (w, 170), (w - 140, h), (0, h)]), border=6)  # title/subtitle strip removed

# ---- contact sheet
files = sorted(f for f in os.listdir(OUT) if f.endswith('.png'))
th = []
for f in files:
    t = Image.open(f'{OUT}/{f}'); t.thumbnail((520, 300))
    bg = Image.new('RGB', (540, 340), (110, 110, 120))
    for yy in range(0, 340, 20):
        for xx in range(0, 540, 20):
            if (xx + yy) // 20 % 2: ImageDraw.Draw(bg).rectangle([xx, yy, xx + 19, yy + 19], fill=(140, 140, 150))
    bg.paste(t, (10, 10), t); ImageDraw.Draw(bg).text((10, 320), f, fill='white'); th.append(bg)
cols = 3; rows = (len(th) + cols - 1) // cols
sheet = Image.new('RGB', (cols * 540, rows * 340), 'black')
for i, t in enumerate(th): sheet.paste(t, ((i % cols) * 540, (i // cols) * 340))
sheet.save(f'{OUT}/contact_sheet.jpg', quality=88)
print('sheet', len(files))
