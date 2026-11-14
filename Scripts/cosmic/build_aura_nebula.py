"""Build the Aura nebula panorama (4096x2048 equirect, RGBA) from two Aura-generated 1536x1024 stills.
- the two stills sit side by side around the horizon, cross-faded at both seams (u=0.5 and the u=0/1 wrap)
- above/below, the stills are mirror-extended, then faded toward the poles (equirect stretch) and the lower hemisphere is dimmed
- uniform pole-corrected stars are added on top (same recipe as finish_nebula.py)
- alpha = the procedural near-wisps mask from the old T_CosmicNebula (the material's parallax layer)
Usage: python build_aura_nebula.py nebula_a.png nebula_b.png proc_T_CosmicNebula.png out.png"""
import sys
import numpy as np
from PIL import Image
W, H = 4096, 2048
OV = 512                     # seam overlap in px
a_p, b_p, proc_p, out_p = sys.argv[1:5]

def tile(p):
    im = Image.open(p).convert('RGB')
    tw = W // 2 + OV; th = int(round(im.height * tw / im.width))
    return np.asarray(im.resize((tw, th), Image.LANCZOS)).astype(np.float32) / 255.0

A, B = tile(a_p), tile(b_p)
th = A.shape[0]
canvas_h = H
# vertical placement: nebula band of the still (~40 % down) lands slightly above the horizon (v = 0.46)
band = 0.40
y0 = int(H * 0.46 - band * th)

def place(t):
    """Mirror-extend the tile vertically to cover the full panorama height."""
    full = np.zeros((H, t.shape[1], 3), np.float32)
    for y in range(H):
        sy = y - y0
        # reflect into [0, th)
        period = 2 * th
        m = sy % period
        if m >= th: m = period - 1 - m
        full[y] = t[m]
    return full

FA, FB = place(A), place(B)
half = W // 2
# B is the dimmer still: lift it toward A's level so the two halves read as one sky
la = FA.mean(); lb = FB.mean(); FB = FB * min(1.5, 0.9 * la / max(lb, 1e-4))
# cyclic weighted accumulation: A starts at x=-OV/2, B at x=half-OV/2; smoothstep ramps across each overlap
acc = np.zeros((H, W, 3), np.float32); wsum = np.zeros((1, W, 1), np.float32)
T = half + OV
r = np.linspace(0, 1, OV, dtype=np.float32); r = r * r * (3 - 2 * r)
wt = np.ones(T, np.float32); wt[:OV] = r; wt[-OV:] = r[::-1]
for F, x0 in ((FA, -OV // 2), (FB, half - OV // 2)):
    cols = (np.arange(T) + x0) % W
    np.add.at(acc, (slice(None), cols), F * wt[None, :, None])
    np.add.at(wsum, (slice(None), cols), wt[None, :, None])
out = acc / np.maximum(wsum, 1e-4)
# latitude shaping: fade toward poles, dim the lower hemisphere a bit more (board side)
v = (np.arange(H, dtype=np.float32) + 0.5) / H
lat = (0.5 - v) * np.pi
fade = np.clip(np.cos(lat) ** 1.6, 0, 1)
fade = np.where(lat < 0, fade * (0.55 + 0.45 * np.clip(1 + lat / 0.6, 0, 1)), fade)
out *= fade[:, None, None]
# overall level: dim backdrop, match the procedural panorama's mean luminance (x1.1)
proc = Image.open(proc_p)
pr = np.asarray(proc.convert('RGB')).astype(np.float32) / 255.0
lum = lambda c: (c[..., 0] * 0.2126 + c[..., 1] * 0.7152 + c[..., 2] * 0.0722).mean()
out *= min(1.6, 0.85 * lum(pr) / max(lum(out), 1e-4))
# soft highlight roll-off + slight desaturation so the bright cores never compete with the board
out = out / (1.0 + 0.9 * out)
out *= 1.25
g = out.mean(axis=2, keepdims=True); out = g + (out - g) * 0.85

# uniform, pole-corrected stars (same recipe as finish_nebula.py, fewer since the stills carry their own)
rng = np.random.default_rng(5)
N = 5000
z = rng.uniform(-1, 1, N); lon = rng.uniform(-np.pi, np.pi, N); latS = np.arcsin(z)
u = (lon / (2 * np.pi) + 0.5) * W; vv = (0.5 - latS / np.pi) * H
b = np.clip(rng.pareto(2.2, N) * 0.22 + 0.12, 0, 2.0)
tints = np.array([[1, 1, 1], [0.75, 0.85, 1], [1, 0.85, 0.7], [0.85, 0.75, 1], [0.7, 1, 1]], np.float32)
tc = tints[rng.integers(0, len(tints), N)]
for i in range(N):
    sz = 0.7 + 0.9 * min(b[i], 1.5); sx = sz / max(np.cos(latS[i]), 0.08)
    r = int(np.ceil(3 * max(sx, sz))); x0, yy0 = int(u[i]), int(vv[i])
    ys = np.arange(yy0 - r, yy0 + r + 1); xs = np.arange(x0 - r, x0 + r + 1); ys = ys[(ys >= 0) & (ys < H)]
    if len(ys) == 0: continue
    g = np.exp(-((ys + 0.5 - vv[i]) / sz) ** 2 * 0.5)[:, None] * np.exp(-((xs + 0.5 - u[i]) / sx) ** 2 * 0.5)[None, :] * b[i]
    out[np.ix_(ys, xs % W)] += g[..., None] * tc[i]
out = np.clip(out, 0, 1)
alpha = np.asarray(proc.split()[3]).astype(np.float32) / 255.0 if proc.mode == 'RGBA' else np.zeros((H, W), np.float32)
img = np.dstack([out, alpha[..., None]])
Image.fromarray((img * 255 + 0.5).astype(np.uint8), 'RGBA').save(out_p)
Image.fromarray((out * 255).astype(np.uint8)).resize((1024, 512), Image.LANCZOS).save(out_p.replace('.png', '_prev.jpg'), quality=90)
# seam check strip: wrap seam and middle seam side by side
s = np.concatenate([out[:, W - 300:], out[:, :300], np.zeros((H, 20, 3)), out[:, half - 300:half + 300]], 1)
Image.fromarray((np.clip(s * 2.5, 0, 1) * 255).astype(np.uint8)).resize((610, 1024)).save(out_p.replace('.png', '_seams.jpg'), quality=88)
print('ok mean lum proc %.4f aura %.4f' % (lum(pr), lum(out)))
