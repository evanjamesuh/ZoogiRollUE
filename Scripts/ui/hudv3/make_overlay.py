# hud-v3 overlay: Evan's hud_v3_target.png as one full-frame image; arena + pill strip are transparent holes,
# live values (numbers, names, portraits, cards) are inpainted out so the game draws them on top.
import sys, numpy as np, cv2
from PIL import Image
src = sys.argv[1] if len(sys.argv) > 1 else '/workspace/ui-polish/stone-ref/hud_v3_target.png'
out = sys.argv[2] if len(sys.argv) > 2 else 'tex3/overlay.png'
im = np.array(Image.open(src).convert('RGB'))
H, W = im.shape[:2]
mask = np.zeros((H, W), np.uint8)
def R(x0, y0, x1, y1): mask[y0:y1, x0:x1] = 255
for dy in (0, 251):
    for dx in (0, 1140):
        R(90+dx, 108+dy, 166+dx, 128+dy)   # name
        R(178+dx, 102+dy, 216+dx, 138+dy)  # score
        R(90+dx, 127+dy, 174+dx, 146+dy)   # cpu / turn chips
        R(90+dx, 146+dy, 224+dx, 167+dy)   # orbs / kos / hand
        R(28+dx, 193+dy, 112+dx, 210+dy)   # episode text
R(30, 232, 192, 277)                       # action box text
R(160, 25, 212, 45); R(222, 25, 254, 45)   # round / orbs values
R(636, 33, 742, 48); R(622, 49, 756, 70)   # active player + map
R(52, 703, 93, 740)                        # deck count
R(1272, 682, 1346, 730)                    # energy text
R(1098, 753, 1162, 768)                    # discard label
res = cv2.inpaint(im, mask, 5, cv2.INPAINT_TELEA)
# portraits -> dark well; card recess -> dark stone (cards drawn live)
rng = np.random.default_rng(1)
def flat(x0, y0, x1, y1, c):
    n = rng.normal(0, 4, (y1-y0, x1-x0, 1))
    res[y0:y1, x0:x1] = np.clip(np.array(c)[None, None, :] + n, 0, 255).astype(np.uint8)
for dy in (0, 251):
    for dx in (0, 1140): flat(28+dx, 109+dy, 83+dx, 165+dy, (14, 18, 24))
flat(380, 584, 966, 763, (22, 21, 20))
# action box / emblem slots -> plain panel stone; the game draws action_box (acting Zoogi) or emblem itself
for dy in (0, 255):
    for dx in (0, 1140):
        y0, y1, x0, x1 = 236 + dy, 304 + dy, 16 + dx, 220 + dx
        ring = np.concatenate([res[y0-4:y0, x0:x1].reshape(-1, 3), res[y1:y1+4, x0:x1].reshape(-1, 3)])
        flat(x0, y0, x1, y1, tuple(int(v) for v in np.median(ring, 0)))
# CAM / MENU labels -> plain plate (drawn live, bigger, with icons): per-row fill from the plate's own label-free ends
for (x0, x1, l0, l1, r0, r1) in [(1136, 1224, 1129, 1134, 1226, 1231), (1264, 1340, 1258, 1263, 1341, 1346)]:
    for y in range(14, 39):
        c = np.median(np.concatenate([res[y, l0:l1], res[y, r0:r1]]), 0)
        res[y, x0:x1] = np.clip(c + rng.normal(0, 2, (x1 - x0, 3)), 0, 255).astype(np.uint8)
a = np.full((H, W), 255, np.uint8)
a[68:560, 242:1134] = 0      # arena window
a[0:35, 425:952] = 0         # phase pills (drawn live, active one glows)
a[28:74, 596:782] = 0        # player + map plate (drawn live)
Image.fromarray(np.dstack([res, a])).save(out)
print('ok', out)
