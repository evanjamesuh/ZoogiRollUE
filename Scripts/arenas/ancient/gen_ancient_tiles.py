# Oct 31: Ancient Collision Meadows tile textures (stylized, seamless, 1024 px = one M_AncientTile repeat). Usage: gen_ancient_tiles.py <outdir>
# Marble-Clash-style chunky tiles with bevels (Evan: "tile like that"), palette from the 3 refs in C:\dev\art\ancient_meadows.
import sys, os, numpy as np
from PIL import Image
N = 1024
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
rng = np.random.default_rng(31)

def noise(cells, seed, oct=4):
    """tileable value noise in [0,1]"""
    r = np.random.default_rng(seed); acc = np.zeros((N, N)); amp = 1.0; tot = 0
    for o in range(oct):
        c = cells * (2 ** o)
        g = r.random((c, c))
        x = np.linspace(0, c, N, endpoint=False); i0 = np.floor(x).astype(int); f = x - i0; f = f * f * (3 - 2 * f)
        i1 = (i0 + 1) % c
        a = g[np.ix_(i0, i0)]; b = g[np.ix_(i0, i1)]; cc = g[np.ix_(i1, i0)]; d = g[np.ix_(i1, i1)]
        fx = f[None, :]; fy = f[:, None]
        acc += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy); tot += amp; amp *= 0.5
    return acc / tot

def hexc(h): h = h.lstrip('#'); return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float) / 255.0

def tiles(n, colors, grout, bevel=0.07, gw=0.022, jitter=0.06, checker=True, offset_rows=False, seed=1, grain=0.10, crack=0.0, moss=None):
    """n x n tiles per texture; returns HxWx3 float"""
    y, x = np.mgrid[0:N, 0:N] / N * n
    if offset_rows: x = x + 0.5 * (np.floor(y) % 2)
    ix, iy = np.floor(x).astype(int) % n, np.floor(y).astype(int) % n
    fx, fy = x - np.floor(x), y - np.floor(y)
    r = np.random.default_rng(seed); tone = r.uniform(-jitter, jitter, (n + 1, n + 1))
    base = np.zeros((N, N, 3))
    pick = ((ix + iy) % 2) if checker else (r.integers(0, len(colors), (n + 1, n + 1))[iy, ix] % len(colors))
    for k, c in enumerate(colors): base[pick == k] = hexc(c)
    base *= (1 + tone[iy, ix])[..., None]
    # bevel: lit top-left, shaded bottom-right; grout lines
    e = np.minimum.reduce([fx, fy, 1 - fx, 1 - fy])
    lit = np.clip((bevel - np.minimum(fx, fy)) / bevel, 0, 1) * (np.minimum(fx, fy) <= np.minimum(1 - fx, 1 - fy))
    sh = np.clip((bevel - np.minimum(1 - fx, 1 - fy)) / bevel, 0, 1) * (np.minimum(1 - fx, 1 - fy) < np.minimum(fx, fy))
    base *= (1 + 0.22 * lit - 0.28 * sh)[..., None]
    nz = noise(8, seed + 7) - 0.5; nz2 = noise(32, seed + 9) - 0.5
    base *= (1 + grain * (nz * 1.2 + nz2))[..., None]
    if crack > 0:
        cr = noise(6, seed + 3, 2); line = np.abs(cr - 0.5) < 0.006 * crack
        base[line] *= 0.72
    if moss:
        m = noise(6, seed + 11, 4); mm = np.clip((m - 0.62) * 6, 0, 1) * np.clip((0.12 - e) / 0.12 + 0.3, 0, 1)
        base = base * (1 - mm[..., None] * 0.8) + hexc(moss) * mm[..., None] * 0.8
    g = e < gw
    base[g] = hexc(grout) * (1 + 0.15 * nz[g])[..., None]
    return np.clip(base, 0, 1)

def bricks(rows, per_row, c1, c2, grout, seed, moss=None):
    y, x = np.mgrid[0:N, 0:N] / N
    ry = np.floor(y * rows).astype(int)
    xx = x * per_row + 0.5 * (ry % 2)
    ix = np.floor(xx).astype(int) % per_row
    fx, fy = xx - np.floor(xx), y * rows - ry
    r = np.random.default_rng(seed); tone = r.uniform(-0.1, 0.1, (rows + 1, per_row + 1)); mix = r.random((rows + 1, per_row + 1))
    a, b = hexc(c1), hexc(c2)
    t = mix[ry % rows, ix][..., None]
    base = a * (1 - t) + b * t
    base = base * (1 + tone[ry % rows, ix])[..., None]
    e = np.minimum.reduce([fx * 2.0 / per_row * rows, fy, (1 - fx) * 2.0 / per_row * rows, 1 - fy])
    base *= (1 + 0.18 * np.clip((0.12 - fy) / 0.12, 0, 1) - 0.25 * np.clip((fy - 0.86) / 0.14, 0, 1))[..., None]
    nz = noise(10, seed + 5) - 0.5; base *= (1 + 0.16 * nz + 0.1 * (noise(48, seed + 6) - 0.5))[..., None]
    if moss:
        m = noise(5, seed + 8, 4); mm = np.clip((m - 0.6) * 5, 0, 1)
        base = base * (1 - mm[..., None] * 0.7) + hexc(moss) * mm[..., None] * 0.7
    base[e < 0.06] = hexc(grout)
    return np.clip(base, 0, 1)

def rock(c1, c2, seed):
    n1 = noise(4, seed, 5); n2 = noise(16, seed + 1, 3)
    t = np.clip(n1 * 1.3 - 0.15, 0, 1)[..., None]
    base = hexc(c1) * (1 - t) + hexc(c2) * t
    strata = 0.5 + 0.5 * np.sin((np.mgrid[0:N, 0:N][0] / N * 18 + n1 * 6) * np.pi)
    base *= (0.85 + 0.15 * strata + 0.2 * (n2 - 0.5))[..., None]
    return np.clip(base, 0, 1)

def save(name, a):
    Image.fromarray((a * 255).astype(np.uint8), 'RGB').save(os.path.join(OUT, name + '.png')); print('wrote', name)

os.makedirs(OUT, exist_ok=True)
save('T_AncHubTile',      tiles(4, ['#E3CFA3', '#D2BB8C'], '#8F7B5A', checker=False, jitter=0.07, seed=2, crack=1.0, moss='#7FA84A'))
save('T_AncEmeraldTile',  tiles(4, ['#86D152', '#68B83C'], '#4C8A2C', bevel=0.04, gw=0.012, jitter=0.03, seed=3, grain=0.16))
save('T_AncNorthTile',    tiles(4, ['#DB6A43', '#BF5234'], '#7A3220', seed=4, crack=0.6, moss='#6E9A3E'))
save('T_AncCitadelTile',  tiles(4, ['#6A9AE8', '#4F7FD2'], '#2C4C8C', seed=5, crack=0.5))
save('T_AncSunstoneTile', tiles(4, ['#F2BB72', '#E0A45C'], '#9E6A36', checker=False, seed=6, crack=1.2, moss='#8FA84A'))
save('T_AncBridgeTile',   bricks(8, 4, '#BDB5A9', '#A69E92', '#6E675D', 7, moss='#6F9A40'))
save('T_AncPolishTile',   tiles(2, ['#F6EDD6', '#EEE2C4'], '#C9B48A', bevel=0.03, gw=0.008, jitter=0.02, seed=8, grain=0.06))
save('T_AncWallTile',     bricks(6, 3, '#A99273', '#8E7859', '#5A4C3A', 9, moss='#5E8A38'))
save('T_AncWallBlueTile', bricks(6, 3, '#5B78B8', '#46619E', '#273A66', 10, moss='#4F7E3A'))
save('T_AncRockTile',     rock('#8A7560', '#5E4E3E', 11))
save('T_AncGrassTile',    tiles(1, ['#5FA444', '#5FA444'], '#5FA444', bevel=0.0, gw=0.0, jitter=0.0, seed=12, grain=0.3))
