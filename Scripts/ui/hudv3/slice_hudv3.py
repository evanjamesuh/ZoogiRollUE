# Slices Evan's HUD v3 target (hud_v3_target.png, 1376x768, his own art) into Content/UI/HudV3/*.png.
# Coordinates are in target pixels; everything is upscaled 2x (Lanczos + light unsharp) before cutting.
# Text the game draws live is masked (bright / saturated glyph pixels inside a box) and inpainted.
import numpy as np, cv2, sys, os
from PIL import Image, ImageFilter
REF = os.environ.get('ZOOGI_HUDV3_REF', '/workspace/ui-polish/stone-ref/hud_v3_target.png')
OUT = sys.argv[1] if len(sys.argv) > 1 else '/workspace/ui-polish/hud-v3/tex/'
os.makedirs(OUT, exist_ok=True)
K = 2
src = Image.open(REF).convert('RGB')
big = src.resize((src.width * K, src.height * K), Image.LANCZOS).filter(ImageFilter.UnsharpMask(1.2, 60, 2))
A = np.asarray(big).copy()

def B(b): return tuple(int(round(v * K)) for v in b)
def text_mask(img, box, thr=150, sat=None, dil=3):
    x0, y0, x1, y1 = B(box); c = img[y0:y1, x0:x1].astype(int)
    l = c.mean(2); s = c.max(2) - c.min(2)
    m = (l > thr) if sat is None else ((l > thr) | (s > sat))
    m = cv2.dilate(m.astype(np.uint8) * 255, np.ones((dil, dil), np.uint8), iterations=2)
    full = np.zeros(img.shape[:2], np.uint8); full[y0:y1, x0:x1] = m; return full
def inpaint(img, masks, r=5):
    m = np.zeros(img.shape[:2], np.uint8)
    for k in masks: m = np.maximum(m, k)
    return cv2.inpaint(img, m, r, cv2.INPAINT_TELEA)
def rect_mask(img, box):
    x0, y0, x1, y1 = B(box); m = np.zeros(img.shape[:2], np.uint8); m[y0:y1, x0:x1] = 255; return m
def save(img, box, name, alpha=None):
    x0, y0, x1, y1 = B(box); c = img[y0:y1, x0:x1]
    if alpha is not None: c = np.dstack([c, alpha])
    Image.fromarray(c.astype(np.uint8)).save(OUT + name); print(name, c.shape[1], c.shape[0])
def fill_plain(img, box, patch):
    # replace box interior with a tiled plain patch (mirrored, so no hard seams)
    x0, y0, x1, y1 = B(box); px0, py0, px1, py1 = B(patch); p = img[py0:py1, px0:px1]
    p2 = np.concatenate([p, p[:, ::-1]], 1); p2 = np.concatenate([p2, p2[::-1]], 0)
    reps = ((y1 - y0) // p2.shape[0] + 1, (x1 - x0) // p2.shape[1] + 1, 1)
    t = np.tile(p2, reps)[: y1 - y0, : x1 - x0]
    t = cv2.GaussianBlur(t, (0, 0), 0.6)
    out = img.copy(); out[y0:y1, x0:x1] = t; return out

def fill_flat(img, box, patch, amp=0.55):
    # flat stone face: patch median colour + the high-pass grain of the tray stone (no visible repeats / smears)
    x0, y0, x1, y1 = B(box); px0, py0, px1, py1 = B(patch)
    col = np.median(img[py0:py1, px0:px1].reshape(-1, 3), 0)
    st = A[B((180, 590, 350, 760))[1]:B((180, 590, 350, 760))[3], B((180, 590, 350, 760))[0]:B((180, 590, 350, 760))[2]].astype(float).mean(2)
    hp = st - cv2.GaussianBlur(st, (0, 0), 6)
    hp = np.tile(np.concatenate([hp, hp[::-1]], 0), (4, 4))[: y1 - y0, : x1 - x0]
    out = img.copy(); out[y0:y1, x0:x1] = np.clip(col[None, None, :] + amp * hp[..., None], 0, 255); return out
def rect_inpaint(img, boxes, r=4): return inpaint(img, [rect_mask(img, b) for b in boxes], r)
def feather_rect(h, w, box, f):
    m = np.zeros((h, w), np.uint8); x0, y0, x1, y1 = box; m[y0:y1, x0:x1] = 255
    return cv2.GaussianBlur(m, (0, 0), f)

# ---------------- top bar  (live text spots, in target px: round value 162..203 x 27..42, orbs value 224..250)
t = rect_inpaint(A, [(161, 26, 204, 43), (223, 26, 252, 43)])
save(t, (0, 0, 326, 52), 'top_title.png')
# pills strip: every cell rebuilt from the RESOLVE cell (plain stone pill); states are overlays
pc = rect_inpaint(A, [(758, 10, 832, 28)], 3)
cell = (738, 0, 843, 36); cx0, cy0, cx1, cy1 = B(cell)
plain_cell = pc[cy0:cy1, cx0:cx1]
strip = A.copy()
for i in range(5):
    x = B((423 + i * 105, 0, 0, 0))[0]
    strip[cy0:cy1, x:x + (cx1 - cx0)] = plain_cell
save(strip, (416, 0, 956, 36), 'pills_bar.png')            # pill i spans 430+105i .. 525+105i, y 6..30
save(pc, (746, 6, 841, 30), 'pill_off.png')
d = rect_inpaint(A, [(446, 10, 516, 28)], 3); save(d, (430, 6, 525, 30), 'pill_done.png')
aw = rect_inpaint(A, [(551, 10, 621, 28)], 3)
x0, y0, x1, y1 = B((508, 0, 664, 50)); c = aw[y0:y1, x0:x1].astype(float)
lum = c.mean(2); blue = c[..., 2] - c[..., 0]
glow = np.clip(np.maximum((lum - 70) * 2.0, blue * 2.5), 0, 255)
core = feather_rect(y1 - y0, x1 - x0, ((537 - 508) * K, 6 * K, (632 - 508) * K, 30 * K), 2)
alpha = np.maximum(glow, core).astype(np.uint8)
save(aw, (508, 0, 664, 50), 'pill_on.png', alpha)          # pill core at 537..632 x 6..30
# map / active-player plate under the pills (texts drawn live)
sp = rect_inpaint(A, [(635, 31, 747, 47), (624, 48, 756, 68)], 4)
save(sp, (596, 26, 784, 72), 'name_plate.png')             # player pill 636..745 x 31..47, map text centre (690, 58)
# right plate with CAM / MENU buttons baked (labels drawn live, UButtons draw only hover / press light)
rp = rect_inpaint(A, [(1140, 16, 1220, 35), (1268, 15, 1340, 36)], 4)
save(rp, (1094, 0, 1376, 54), 'top_right.png')             # CAM btn 1120..1240 x 9..43, MENU 1250..1357 x 9..43

# ---------------- arena frame: corner brackets (alpha = bracket triangle + the frame bands, never the scene)
from PIL import ImageDraw
def poly_alpha(box, polys, bands):
    x0, y0, x1, y1 = box
    m = Image.new('L', ((x1 - x0) * K, (y1 - y0) * K), 0); d = ImageDraw.Draw(m)
    for p in polys: d.polygon([((x - x0) * K, (y - y0) * K) for x, y in p], fill=255)
    for b in bands: d.rectangle([(b[0] - x0) * K, (b[1] - y0) * K, (b[2] - x0) * K - 1, (b[3] - y0) * K - 1], fill=255)
    return np.asarray(m.filter(ImageFilter.GaussianBlur(0.8)))
AX0, AY0, AX1, AY1 = 242, 68, 1134, 560                     # arena rect in the target
b = (226, 54, 280, 110); save(A, b, 'corner_tl.png', poly_alpha(b, [[(226, 54), (278, 54), (226, 108)]], [(226, 54, AX0, 110), (226, 54, 280, AY0)]))
b = (1096, 54, 1150, 110); save(A, b, 'corner_tr.png', poly_alpha(b, [[(1098, 54), (1150, 54), (1150, 108)]], [(AX1, 54, 1150, 110), (1096, 54, 1150, AY0)]))
b = (226, 522, 280, 578); save(A, b, 'corner_bl.png', poly_alpha(b, [[(226, 524), (226, 578), (278, 578)]], [(226, 522, AX0, 578), (226, AY1, 280, 578)]))
b = (1096, 522, 1150, 578); save(A, b, 'corner_br.png', poly_alpha(b, [[(1150, 524), (1150, 578), (1098, 578)]], [(AX1, 522, 1150, 578), (1096, AY1, 1150, 578)]))
save(A, (226, 250, AX0, 400), 'edge_l.png')
save(A, (AX1, 250, 1150, 400), 'edge_r.png')
save(A, (380, 52, 540, AY0), 'edge_t.png')
save(A, (380, AY1, 540, 576), 'edge_b.png')

# ---------------- team columns
gp = A.copy()
gp = fill_flat(gp, (16, 318, 220, 560), (60, 420, 180, 440)); gp = rect_inpaint(gp, [(80, 309, 148, 318)], 3)
save(gp, (8, 309, 228, 568), 'group_panel.png')             # outer stone panel, border ~8 px
save(A, (18, 319, 217, 342), 'header_bar.png')
save(A, (184, 64, 219, 97), 'gem_blue.png')
save(A, (1330, 64, 1366, 97), 'gem_orange.png')
save(A, (1330, 316, 1366, 346), 'gem_teal.png')
save(A, (82, 284, 146, 324), 'connector.png')
# neutral gem (tinted live with the Awaken stone colour): the connector diamond, desaturated + brightened
x0, y0, x1, y1 = B((96, 290, 132, 320)); g = A[y0:y1, x0:x1].astype(float); lum = g.mean(2)
yy, xx = np.mgrid[0:y1 - y0, 0:x1 - x0]; cxg, cyg = (x1 - x0) / 2, (y1 - y0) / 2
dia = np.clip((1 - (abs(xx - cxg) / cxg + abs(yy - cyg) / cyg)) * 12, 0, 1)
gw = np.clip(lum * 1.35 + 30, 0, 255)
Image.fromarray(np.dstack([gw, gw, gw, dia * 255]).astype(np.uint8)).save(OUT + 'gem_white.png'); print('gem_white.png')
cp = fill_flat(rect_inpaint(A, [(203, 394, 217, 410)], 3), (27, 355, 211, 476), (60, 420, 180, 440))
save(cp, (20, 349, 217, 482), 'card_panel.png')
pf = A.copy(); save(pf, (25, 353, 87, 415), 'portrait_frame.png')
save(rect_inpaint(A, [(97, 380, 121, 390)], 2), (92, 377, 126, 393), 'chip_purple.png')
save(rect_inpaint(A, [(134, 380, 166, 390)], 2), (130, 377, 171, 393), 'chip_gold.png')
save(rect_inpaint(A, [(35, 240, 85, 250)], 2), (30, 237, 89, 252), 'chip_blue.png')
save(rect_inpaint(A, [(32, 462, 186, 470)], 3), (27, 458, 188, 474), 'bar_track.png')
save(A, (30, 462, 104, 470), 'bar_fill.png')
save(rect_inpaint(A, [(28, 236, 214, 274)], 4), (20, 231, 217, 277), 'action_box.png')
save(A, (21, 488, 215, 559), 'emblem.png')

# ---------------- bottom tray
save(fill_flat(A, (53, 696, 90, 740), (53, 692, 90, 696), 0.25), (0, 578, 362, 768), 'tray_left.png')          # drawn at tray top + 12   # count box 51..92 x 691..744
tr = rect_inpaint(A, [(1270, 676, 1340, 709), (1255, 750, 1348, 758), (1104, 755, 1162, 768)], 4)
save(tr, (978, 578, 1376, 768), 'tray_right.png')         # drawn at tray top + 12          # disc 1268..1344 x 668..734, bar 1253..1351 x 749..759, orb (1230,628) r 60
x0, y0, x1, y1 = B((1166, 564, 1294, 692)); o = A[y0:y1, x0:x1]
yy, xx = np.mgrid[0:y1 - y0, 0:x1 - x0]; r = np.hypot(xx - (x1 - x0) / 2, yy - (y1 - y0) / 2)
oa = np.clip((60 * K - r) / (8 * K) * 255, 0, 255).astype(np.uint8)
Image.fromarray(np.dstack([o, oa])).save(OUT + 'orb_swirl.png'); print('orb_swirl.png')
# card tray recess: border ring from the art, cards removed (interior = the dark gap colour)
tm = A.copy(); x0, y0, x1, y1 = B((380, 588, 950, 768))
gap = A[B((942, 600, 949, 740))[1]:B((942, 600, 949, 740))[3], B((942, 600, 949, 740))[0]:B((942, 600, 949, 740))[2]].reshape(-1, 3).mean(0)
yy = np.linspace(0, 1, y1 - y0)[:, None, None]
tm[y0:y1, x0:x1] = np.clip(gap[None, None, :] * (0.85 + 0.25 * yy), 0, 255)
# rows 588..768 left / right borders from the art (x 360..382, 950..982); top edge 566..588 has no cards
save(tm, (360, 566, 982, 768), 'tray_mid.png')             # 9-slice: border ~22 px (top / sides)
gem = rect_inpaint(A, [(388, 592, 401, 606)], 3); save(gem, (380, 584, 410, 614), 'cost_gem.png')
p = A[B((180, 590, 350, 760))[1]:B((180, 590, 350, 760))[3], B((180, 590, 350, 760))[0]:B((180, 590, 350, 760))[2]]
p2 = np.concatenate([p, p[:, ::-1]], 1); p2 = np.concatenate([p2, p2[::-1]], 0)
Image.fromarray(p2).save(OUT + 'stone_tile.png'); print('stone_tile.png', p2.shape[1], p2.shape[0])
