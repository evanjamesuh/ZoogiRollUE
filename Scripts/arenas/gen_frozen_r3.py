"""Frozen Ring round 3 textures (Oct 6, 10 PM). Inputs: the round-2 T_FrozenGlow / T_FrozenFloor (art-derived) and the
Animations & Powers pack (reskin-incoming/02-frozen-ring/animations). Outputs into SourceArt/Arenas/Frozen:
  T_FrozenGlow   art glow mask + clean procedural rune geometry (rings, 4 sockets, 8-point star, swirl arcs, glyph ticks)
  T_FrozenFloor  art floor lifted toward the art's pale ice (less saturated blue)
  T_FrozenCloud  soft anime cloud puff (RGBA) for the cloud sea
  T_FrozenDisp_S1/S2/S3/Titan  display decals (RGBA, colour baked) from the pack crack masks + glow emissive (R/G/B = severity)
  T_FrozenDisp_Crater  pack crater base colour + emissive rim baked
  T_FrozenDisp_Break / _Burst / _RuneRing  pillar-rise floor-break ring, ice/snow burst, rune glow ring (pack sprites)
  T_FrozenDisp_Ember / _Frost / _Storm  power overlays (pack MeltDrip / Refreeze+Rime / Arc)
  + the raw pack textures (17 destruction + BreakRing / RuneRing / Mist) copied for later material work.
usage: python3 gen_frozen_r3.py <r2 backup dir> <pack animations dir> <out dir>"""
import sys, os, math, shutil
import numpy as np
import cv2
from PIL import Image

BAK, PACK, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
N = 2048
HALF = 1550.0
rng = np.random.default_rng(3)


def P(x, y):
    return (int(round((x / (2 * HALF) + 0.5) * N * 4)), int(round((y / (2 * HALF) + 0.5) * N * 4)))  # 2-bit subpixel


def save(arr, name, mode=None):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode).save(os.path.join(OUT, name + '.png'))
    print('wrote', name)


# ---------------------------------------------------------------- rune glow
g = np.zeros((N, N), np.float32)
px = N / (2 * HALF)


def circ(r, w, v=1.0, c=(0, 0)):
    cv2.circle(g, P(*c), int(r * px * 4), v, max(1, int(w * px)), cv2.LINE_AA, 2)


def line(a, b, w, v=1.0):
    cv2.line(g, P(*a), P(*b), v, max(1, int(w * px)), cv2.LINE_AA, 2)


for r, w, v in [(1522, 22, 1.0), (1492, 6, 0.6), (980, 9, 0.9), (940, 5, 0.6), (700, 7, 0.8), (600, 5, 0.6), (345, 8, 0.9), (330, 4, 0.6), (232, 6, 0.8), (150, 6, 0.9), (112, 4, 0.7)]:
    circ(r, w, v)
pol = lambda r, a: (r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))
for k in range(4):  # 4 sockets on the medallion ring (classic start pads, r 980)
    circ(118, 9, 1.0, pol(960, k * 90)); circ(88, 4, 0.6, pol(960, k * 90))
for k in range(8):  # 8 small rune circles
    circ(48, 5, 0.8, pol(470, 22.5 + k * 45))
for rot in (0, 45):  # 8-point star from two squares inscribed in the medallion ring
    pts = [pol(940, rot + 90 * k) for k in range(4)]
    for k in range(4): line(pts[k], pts[(k + 1) % 4], 6, 0.75)
for k in range(8):  # spokes hub -> medallion
    line(pol(345, k * 45), pol(600, k * 45), 5, 0.7)
for k in range(72):  # glyph ticks between the 600 / 700 rings
    a = k * 5
    if k % 3 == 0:
        line(pol(612, a), pol(688, a), 5, 0.7)
    else:
        line(pol(630, a), pol(670, a + 1.2), 4, 0.5)
for k in range(6):  # swirl arcs (log spiral) between the medallion and the boundary
    a0 = k * 60 + 10
    pts = [pol(1020 + 420 * t, a0 + 70 * t) for t in np.linspace(0, 1, 40)]
    for i in range(len(pts) - 1): line(pts[i], pts[i + 1], 10 - 6 * i / 40, 0.75)
    pts = [pol(1060 + 300 * t, a0 + 22 + 50 * t) for t in np.linspace(0, 1, 30)]
    for i in range(len(pts) - 1): line(pts[i], pts[i + 1], 5, 0.5)
soft = cv2.GaussianBlur(g, (0, 0), 6)
proc = np.clip(g + soft * 0.6, 0, 1)
orig = np.asarray(Image.open(os.path.join(BAK, 'T_FrozenGlow.png')).convert('L')).astype(np.float32) / 255
if orig.shape[0] != N: orig = cv2.resize(orig, (N, N))
glow = np.maximum(proc, orig * 0.45)
save(glow * 255, 'T_FrozenGlow', 'L')

# ---------------------------------------------------------------- floor lift (paler, the art's icy white-cyan)
F = np.asarray(Image.open(os.path.join(BAK, 'T_FrozenFloor.png')).convert('RGB')).astype(np.float32)
L = F.mean(2, keepdims=True)
F = L + (F - L) * 0.78                      # desaturate a little
F = F * 0.86 + np.array([232, 246, 255], np.float32) * 0.14
F = F + proc[..., None] * np.array([40, 70, 80], np.float32) * 0.6  # runes read in the albedo too
save(F, 'T_FrozenFloor', 'RGB')

# ---------------------------------------------------------------- cloud puff
S = 1024
yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
dens = np.zeros((S, S), np.float32)
shade = np.zeros((S, S), np.float32)
for i in range(26):
    cx = rng.uniform(0.18, 0.82) * S; cy = rng.uniform(0.35, 0.65) * S + (abs(cx - S / 2) / S) * 0.25 * S
    r = rng.uniform(0.08, 0.2) * S * (1.2 - abs(cx - S / 2) / S)
    d = np.clip(1 - np.hypot(xx - cx, yy - cy) / r, 0, 1)
    dens = np.maximum(dens, d)
    shade = np.maximum(shade, np.clip(1 - np.hypot(xx - cx + r * 0.25, yy - cy + r * 0.35) / r, 0, 1))
a = np.clip(dens * 3.2, 0, 1)
a = cv2.GaussianBlur(a, (0, 0), 3)
lit = np.clip(shade * 2.2, 0, 1)
col = np.stack([0.80 + 0.2 * lit, 0.86 + 0.14 * lit, 0.95 + 0.05 * lit], -1) * 255
save(np.dstack([col, a[..., None] * 255]), 'T_FrozenCloud', 'RGBA')


# ---------------------------------------------------------------- pack-derived display decals
def ld(rel):
    return np.asarray(Image.open(os.path.join(PACK, rel)).convert('RGBA' if 'RGBA' in rel or 'Pow' in rel or 'FX' in rel or 'Form' in rel or 'Crater_Base' in rel else 'RGB')).astype(np.float32) / 255


def sev_disp(mask, emis, ch, falloff=None):
    m = mask[..., ch]; core = mask[..., 3]; e = emis[..., ch]
    rim = ((m > 0.2) & (m < 0.75)).astype(np.float32)
    inner = np.clip((m - 0.6) / 0.4, 0, 1)
    dark = np.array([0.043, 0.133, 0.2]); cyan = np.array([0.37, 0.88, 1.0]); hot = np.array([0.91, 1.0, 1.0])
    col = dark * rim[..., None] + cyan * inner[..., None]
    col = col * (1 - core[..., None]) + hot * core[..., None]
    col = np.maximum(col, cyan * e[..., None] * 0.9)
    alpha = np.clip(np.maximum(np.maximum(rim * 0.85, inner), np.maximum(core, e * 0.8)), 0, 1)
    if falloff is not None: alpha *= falloff
    return np.dstack([col * 255, alpha[..., None] * 255])


R = ld('destruction/textures/T_FrozenCrack_Radial_RGBA.png'); RE = ld('destruction/textures/T_FrozenGlowCrack_Radial_E.png')
T = ld('destruction/textures/T_FrozenCrack_Tile_RGBA.png'); TE = ld('destruction/textures/T_FrozenGlowCrack_Tile_E.png')
TT = ld('destruction/textures/T_FrozenCrack_Titan_RGBA.png'); TTE = ld('destruction/textures/T_FrozenGlowCrack_Titan_E.png')
yy, xx = np.mgrid[0:1024, 0:1024].astype(np.float32)
rad = np.hypot(xx - 512, yy - 512) / 512
fall = np.clip((1 - rad) / 0.35, 0, 1)
save(sev_disp(R, RE, 0), 'T_FrozenDisp_S1', 'RGBA')
s2 = sev_disp(R, RE, 1); s2t = sev_disp(T, TE, 1, fall * 0.8)
s2[..., 3] = np.maximum(s2[..., 3], s2t[..., 3]); m = s2t[..., 3:] > s2[..., 3:] - 1; s2[..., :3] = np.where(s2t[..., 3:] > 10, np.maximum(s2[..., :3], s2t[..., :3]), s2[..., :3])
save(s2, 'T_FrozenDisp_S2', 'RGBA')
s3 = sev_disp(R, RE, 2); s3t = sev_disp(T, TE, 2, fall)
s3[..., 3] = np.maximum(s3[..., 3], s3t[..., 3]); s3[..., :3] = np.where(s3t[..., 3:] > 10, np.maximum(s3[..., :3], s3t[..., :3]), s3[..., :3])
save(s3, 'T_FrozenDisp_S3', 'RGBA')
save(sev_disp(TT, TTE, 2, np.clip((1 - rad) / 0.2, 0, 1)), 'T_FrozenDisp_Titan', 'RGBA')
CB = ld('destruction/textures/T_FrozenCrater_BaseColor.png')
CE = np.asarray(Image.open(os.path.join(PACK, 'destruction/textures/T_FrozenCrater_Emissive.png')).convert('L')).astype(np.float32) / 255
cr = CB.copy(); cr[..., :3] = cr[..., :3] * 0.8 + np.array([0.1, 0.76, 0.9]) * CE[..., None] * 0.9
save(cr * 255, 'T_FrozenDisp_Crater', 'RGBA')
# pillar-rise floor-break ring: R crack (A coverage) -> cyan-white seams with a dark rim
BR = np.asarray(Image.open(os.path.join(PACK, 'forming/textures/T_FrozenForm_BreakRing.png')).convert('RGBA')).astype(np.float32) / 255
c = BR[..., 0] * BR[..., 3]
br = np.dstack([np.array([0.55, 0.93, 1.0]) * np.clip(c * 1.4, 0, 1)[..., None] + np.array([0.04, 0.13, 0.2]) * (1 - c[..., None]), np.clip(c * 1.6 + BR[..., 3] * 0.15, 0, 1)[..., None]])
save(cv2.resize(br, (1024, 1024)) * 255, 'T_FrozenDisp_Break', 'RGBA')


def frame(img, n, i):
    s = img.shape[0] // n
    return img[(i // n) * s:(i // n + 1) * s, (i % n) * s:(i % n + 1) * s]


SH = ld('destruction/textures/T_FrozenFX_Shards_4x4.png'); WI = ld('destruction/textures/T_FrozenFX_Wisp_8x8.png')
burst = np.zeros((1024, 1024, 4), np.float32)


def stamp(dst, spr, cx, cy, size, ang):
    spr = cv2.resize(spr, (int(size), int(size)))
    M = cv2.getRotationMatrix2D((size / 2, size / 2), ang, 1.0)
    spr = cv2.warpAffine(spr, M, (int(size), int(size)))
    x0, y0 = int(cx - size / 2), int(cy - size / 2)
    x1, y1 = max(0, x0), max(0, y0); x2, y2 = min(1024, x0 + int(size)), min(1024, y0 + int(size))
    if x2 <= x1 or y2 <= y1: return
    s = spr[y1 - y0:y2 - y0, x1 - x0:x2 - x0]; d = dst[y1:y2, x1:x2]
    a = s[..., 3:]
    d[..., :3] = d[..., :3] * (1 - a) + s[..., :3] * a; d[..., 3:] = np.maximum(d[..., 3:], a)


for i in range(40):  # snow wisps ring
    an = rng.uniform(0, 2 * math.pi); rr = rng.uniform(200, 420)
    stamp(burst, frame(WI, 8, rng.integers(16, 40)), 512 + rr * math.cos(an), 512 + rr * math.sin(an), rng.uniform(140, 260), rng.uniform(0, 360))
for i in range(46):  # ice shards flung outward
    an = rng.uniform(0, 2 * math.pi); rr = rng.uniform(140, 470)
    stamp(burst, frame(SH, 4, rng.integers(0, 16)), 512 + rr * math.cos(an), 512 + rr * math.sin(an), rng.uniform(50, 120), -math.degrees(an) + rng.uniform(-30, 30))
burst[..., 3] *= np.clip((1 - rad) / 0.12, 0, 1)
save(burst * 255, 'T_FrozenDisp_Burst', 'RGBA')
RR = ld('forming/textures/T_FrozenForm_RuneRing_4x4.png'); fr = frame(RR, 4, 6).copy()
fr[..., :3] = np.array([0.1, 0.76, 0.9]) * 0.6 + fr[..., :3] * 0.4
save(cv2.resize(fr, (512, 512)) * 255, 'T_FrozenDisp_RuneRing', 'RGBA')
# power overlays
MD = ld('destruction/textures/T_FrozenPow_MeltDrip.png')
em = np.dstack([np.array([1.0, 0.42, 0.17]) * MD[..., 0:1] + np.array([0.75, 0.88, 0.95]) * (1 - MD[..., 0:1]), (MD[..., 3:] * (0.35 + 0.65 * MD[..., 0:1])) * fall[..., None]])
save(em * 255, 'T_FrozenDisp_Ember', 'RGBA')
RF = ld('destruction/textures/T_FrozenPow_Refreeze.png'); RI = ld('destruction/textures/T_FrozenPow_Rime_4x4.png')
rime = cv2.resize(frame(RI, 4, 15), (1024, 1024))
fa = np.clip(np.maximum(RF[..., 0] * RF[..., 3], rime[..., 3]) * fall, 0, 1)
save(np.dstack([np.array([0.92, 0.98, 1.0]) * np.ones_like(fa)[..., None] * 255, fa[..., None] * 255]), 'T_FrozenDisp_Frost', 'RGBA')
AR = ld('destruction/textures/T_FrozenPow_Arc_4x4.png')
st = np.zeros((1024, 1024, 4), np.float32)
for i in range(8):
    an = i * 45 + rng.uniform(-10, 10)
    stamp(st, frame(AR, 4, i), 512 + 230 * math.cos(math.radians(an)), 512 + 230 * math.sin(math.radians(an)), 420, -an)
save(st * 255, 'T_FrozenDisp_Storm', 'RGBA')
# raw pack textures (kept for the MF_FrozenDamage material work)
for f in sorted(os.listdir(os.path.join(PACK, 'destruction/textures'))):
    shutil.copy(os.path.join(PACK, 'destruction/textures', f), os.path.join(OUT, f))
for f in ('T_FrozenForm_BreakRing.png', 'T_FrozenForm_RuneRing_4x4.png', 'T_FrozenForm_Mist_4x4.png'):
    shutil.copy(os.path.join(PACK, 'forming/textures', f), os.path.join(OUT, f))
print('DONE')
