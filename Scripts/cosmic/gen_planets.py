# Cosmic backdrop texture generator (procedural, numpy + Pillow + scipy). Run from Scripts/cosmic: python gen_nebula.py 4096 2048 && python finish_nebula.py && python gen_planets.py
# Output goes to ./tex; copy the PNGs to SourceArt/Cosmic and run Scripts/make_cosmic_backdrop.py (headless) to import.
import numpy as np
from PIL import Image
exec(open('gen_nebula.py').read().split("out = np.zeros")[0].split("W, H =")[0])  # imports only
src = open('gen_nebula.py').read()
ns = {}
exec(src[:src.index("def hexc")].replace("W, H = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (4096, 2048)", "W, H = 0, 0"), ns)
perlin, fbm, smooth = ns['perlin'], ns['fbm'], ns['smooth']
def hexc(h): return np.array([int(h[i:i + 2], 16) / 255.0 for i in (1, 3, 5)], np.float32)

def planet(N, R, bands, light, ring=None, atmo=None, seed=0, crater=False):
    y, x = np.mgrid[0:N, 0:N].astype(np.float32)
    px = (x + 0.5) / N * 2 - 1; py = 1 - (y + 0.5) / N * 2
    rgba = np.zeros((N, N, 4), np.float32)
    r2 = (px / R) ** 2 + (py / R) ** 2
    inside = r2 < 1
    pz = np.sqrt(np.clip(1 - r2, 0, 1))
    nx, ny, nz = px / R, py / R, pz
    # planet tilt for bands
    tilt = 0.35
    by = ny * np.cos(tilt) - nx * np.sin(tilt)
    bx = nx * np.cos(tilt) + ny * np.sin(tilt)
    p = np.stack([bx * 2, by * 2, nz * 2])
    if crater:
        n = fbm(p * 1.5, 5, f=1.5, off=seed) * 0.5 + 0.5
        t = np.clip(n, 0, 1)
        base = bands[0] * (1 - t[..., None]) + bands[1] * t[..., None]
        cr = smooth(0.62, 0.7, fbm(p * 3, 3, f=2.0, off=seed + 9) * 0.5 + 0.5)
        base = base * (1 - 0.35 * cr[..., None])
    else:
        warp = fbm(p, 4, f=1.5, off=seed) * 0.25
        lat = by + warp
        t = (np.sin(lat * 9.0) * 0.5 + 0.5) * 0.6 + (fbm(np.stack([lat * 6, lat * 6, lat * 6]), 3, f=1.0, off=seed + 3) * 0.5 + 0.5) * 0.4
        idx = np.clip(t, 0, 0.999) * (len(bands) - 1)
        i0 = idx.astype(int); fr = (idx - i0)[..., None]
        B = np.array(bands)
        base = B[i0] * (1 - fr) + B[np.minimum(i0 + 1, len(bands) - 1)] * fr
    L = np.array(light, np.float32); L /= np.linalg.norm(L)
    ndl = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
    shade = 0.04 + 0.96 * ndl ** 0.8
    col = base * shade[..., None]
    rim = (1 - pz) ** 3
    if atmo is not None:
        col = col + atmo * (rim * (0.3 + 0.7 * ndl))[..., None] * 1.2
    rgba[..., :3] = np.where(inside[..., None], col, 0)
    edge = np.clip((1 - np.sqrt(r2)) * N * R * 0.5, 0, 1)
    rgba[..., 3] = edge
    if atmo is not None:  # soft halo outside the disc
        d = np.sqrt(r2)
        halo = np.clip(1 - (d - 1) / 0.08, 0, 1) ** 2 * (d >= 1)
        hl = np.clip((px * L[0] + py * L[1]) / (np.sqrt(px * px + py * py) + 1e-6) * 0.5 + 0.6, 0, 1)
        rgba[..., :3] += (atmo * (halo * hl * 0.8)[..., None]) * (1 - rgba[..., 3:4])
        rgba[..., 3] = np.maximum(rgba[..., 3], halo * hl * 0.8)
    if ring is not None:
        ri, ro, rcol, incl, rot = ring
        # ring plane: ellipse in screen space
        ca, sa = np.cos(rot), np.sin(rot)
        rx = px * ca + py * sa; ry = (-px * sa + py * ca) / np.sin(incl)
        rr = np.sqrt(rx * rx + ry * ry)
        band = smooth(ri, ri + 0.02, rr) * (1 - smooth(ro - 0.02, ro, rr))
        tex = 0.55 + 0.45 * np.sin(rr * 140) * np.sin(rr * 37 + 1.0)
        gap = 1 - 0.8 * smooth(0.0, 0.01, 0.012 - np.abs(rr - (ri + ro) * 0.52))
        ra = band * np.clip(tex, 0, 1) * gap * 0.85
        # behind-planet part hidden: ring point is behind when ry > 0 (upper half after rotation) and inside disc
        behind = (ry * np.sign(np.sin(incl)) > 0) & inside
        ra = np.where(behind, 0, ra)
        # planet shadow on the ring (rough)
        rcolv = rcol * (0.35 + 0.65 * np.clip((rx * L[0] + 1) * 0.6, 0.2, 1))[..., None]
        a0 = rgba[..., 3:4]
        rgba[..., :3] = rgba[..., :3] * (1 - ra[..., None]) + rcolv * ra[..., None]
        rgba[..., 3] = np.clip(a0[..., 0] + ra * (1 - a0[..., 0]), 0, 1)
    return rgba

def save(rgba, path):
    # premultiplied colour stays as is (planet is unlit, alpha blends it), straight alpha export
    a = np.clip(rgba[..., 3:4], 1e-4, 1)
    c = np.clip(rgba[..., :3] / np.where(rgba[..., 3:4] > 0.002, a, 1), 0, 1)
    img = np.dstack([c, np.clip(rgba[..., 3:4], 0, 1)])
    Image.fromarray((img * 255 + 0.5).astype(np.uint8), 'RGBA').save(path)

A = planet(1024, 0.52, [hexc('#3a1c6e'), hexc('#7b3fd1'), hexc('#c46bd8'), hexc('#ff9b5a'), hexc('#ffd8a0'), hexc('#8a4be0'), hexc('#2b1a5a')],
           light=(-0.6, 0.45, 0.65), ring=(0.62, 0.95, hexc('#d9b8ff'), 0.28, 0.35), atmo=hexc('#b48cff'), seed=4)
save(A, 'tex/T_CosmicPlanetA.png')
B = planet(512, 0.8, [hexc('#1c6f8f'), hexc('#7fe6ff')], light=(-0.5, 0.5, 0.7), atmo=hexc('#59d8ff'), seed=12, crater=True)
save(B, 'tex/T_CosmicPlanetB.png')
# preview on dark bg
for n in ('A', 'B'):
    im = Image.open(f'tex/T_CosmicPlanet{n}.png'); bg = Image.new('RGBA', im.size, (8, 8, 30, 255)); bg.alpha_composite(im); bg.convert('RGB').save(f'tex/prev_planet{n}.jpg')
print('ok')
