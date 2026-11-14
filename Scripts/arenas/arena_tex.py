"""Shared helpers for the Buzzy arena textures (procedural, PIL + numpy; no third-party art).

World mapping used by M_ZoogiArenaFloor: U = X / (2R) + 0.5, V = Y / (2R) + 0.5 with R = 1550 cm (the knockout radius),
so image column = X, image row = Y and an angle measured from +X toward +Y (UE yaw) is atan2(row - c, col - c).
"""
import numpy as np
from PIL import Image, ImageFilter

R_CM = 1550.0


class Disc:
    """Polar coordinate grids for an N x N floor texture covering [-R, R]^2 cm."""
    def __init__(self, n):
        self.n = n
        self.cm_per_px = 2 * R_CM / n
        ax = (np.arange(n, dtype=np.float32) + 0.5) * self.cm_per_px - R_CM
        self.x = ax[None, :].repeat(n, 0)          # columns = X
        self.y = ax[:, None].repeat(n, 1)          # rows = Y
        self.r = np.hypot(self.x, self.y)
        self.a = np.arctan2(self.y, self.x)        # -pi..pi, UE yaw in radians
        self.a360 = np.degrees(self.a) % 360.0


def value_noise(n, cells, seed, wrap=False):
    """Smooth value noise in [0,1], n x n, `cells` random lattice points per side (bicubic upsampled)."""
    rng = np.random.default_rng(seed)
    g = rng.random((cells, cells)).astype(np.float32)
    if wrap:
        g = np.pad(g, 2, mode="wrap")
        big = n + 4 * n // cells
        im = Image.fromarray(g).resize((big, big), Image.BICUBIC)
        o = 2 * n // cells
        out = np.asarray(im, dtype=np.float32)[o:o + n, o:o + n]
    else:
        out = np.asarray(Image.fromarray(g).resize((n, n), Image.BICUBIC), dtype=np.float32)
    return np.clip(out, 0, 1)


def fbm(n, base_cells, octaves, seed, wrap=False, gain=0.5):
    acc = np.zeros((n, n), np.float32)
    amp, tot, cells = 1.0, 0.0, base_cells
    for o in range(octaves):
        acc += amp * value_noise(n, min(cells, n), seed + 31 * o, wrap)
        tot += amp
        amp *= gain
        cells *= 2
    return acc / tot


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def line_mask(dist_cm, half_width_cm, soft_cm=1.2):
    """Anti-aliased band where |dist| < half width."""
    return 1.0 - smooth(half_width_cm - soft_cm, half_width_cm + soft_cm, np.abs(dist_cm))


def ring(d, radius, width):
    return line_mask(d.r - radius, width * 0.5)


def spoke(d, deg, width, r0, r1):
    a = np.radians(deg)
    ux, uy = np.cos(a), np.sin(a)
    along = d.x * ux + d.y * uy
    across = -d.x * uy + d.y * ux
    m = line_mask(across, width * 0.5)
    m *= smooth(r0 - 1, r0 + 1, along) * (1 - smooth(r1 - 1, r1 + 1, along))
    return m


def circle_at(d, cx, cy, radius, width):
    return line_mask(np.hypot(d.x - cx, d.y - cy) - radius, width * 0.5)


def hash2(a, b, seed=0):
    """Deterministic pseudo-random in [0,1) for integer arrays a, b."""
    h = (a.astype(np.int64) * 73856093) ^ (b.astype(np.int64) * 19349663) ^ (seed * 83492791)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFF).astype(np.float32) / 65536.0


def flagstones(d, rings, seed, min_block_cm=110.0, max_block_cm=190.0, grout_cm=2.6):
    """Concentric flagstone courses. rings: list of radii [r0, r1, ..., rk] (course boundaries, cm).
    Returns (stone_id_rand [0,1), grout mask [0,1], edge distance in cm)."""
    n = d.n
    rnd = np.zeros((n, n), np.float32)
    edge = np.full((n, n), 999.0, np.float32)
    rng = np.random.default_rng(seed)
    for k in range(len(rings) - 1):
        r0, r1 = rings[k], rings[k + 1]
        sel = (d.r >= r0) & (d.r < r1)
        if not sel.any():
            continue
        rm = 0.5 * (r0 + r1)
        circ = 2 * np.pi * rm
        blocks = max(6, int(circ / rng.uniform(min_block_cm, max_block_cm)))
        off = rng.uniform(0, 2 * np.pi)
        # Slightly irregular block widths: jitter each boundary angle.
        bounds = (np.arange(blocks + 1) / blocks) * 2 * np.pi
        jit = rng.uniform(-0.28, 0.28, blocks + 1) * (2 * np.pi / blocks)
        jit[0] = 0.0
        jit[-1] = 0.0
        bounds = bounds + jit
        aa = (d.a[sel] - off) % (2 * np.pi)
        idx = np.searchsorted(bounds, aa, side="right") - 1
        idx = np.where(aa < bounds[0], blocks - 1, idx) % blocks
        lo = bounds[idx]
        hi = bounds[idx + 1]
        aa2 = np.where(aa < lo, aa + 2 * np.pi, aa)
        da = np.minimum(aa2 - lo, hi - aa2) * d.r[sel]
        dr = np.minimum(d.r[sel] - r0, r1 - d.r[sel])
        edge[sel] = np.minimum(da, dr)
        rnd[sel] = hash2(np.full(idx.shape, k), idx, seed)
    grout = 1.0 - smooth(grout_cm * 0.5 - 0.7, grout_cm * 0.5 + 0.7, edge)
    return rnd, grout, edge


def glyph_band(d, r_in, r_out, count, seed, stroke_cm=3.0, skip_deg=()):
    """Band of simple rune glyphs (short straight strokes, ticks and small circles) between two radii."""
    rng = np.random.default_rng(seed)
    n = d.n
    out = np.zeros((n, n), np.float32)
    h = r_out - r_in
    rc = 0.5 * (r_in + r_out)
    for i in range(count):
        a0 = (i + 0.5) / count * 360.0
        if any(abs(((a0 - s + 180) % 360) - 180) < 6 for s in skip_deg):
            continue
        ar = np.radians(a0)
        cx, cy = rc * np.cos(ar), rc * np.sin(ar)
        tx, ty = -np.sin(ar), np.cos(ar)       # tangent
        rx, ry = np.cos(ar), np.sin(ar)        # radial
        w = h * 0.55
        # local window for speed
        pad = h
        c0 = int((cx - pad + R_CM) / d.cm_per_px); c1 = int((cx + pad + R_CM) / d.cm_per_px) + 1
        r0_ = int((cy - pad + R_CM) / d.cm_per_px); r1_ = int((cy + pad + R_CM) / d.cm_per_px) + 1
        c0, r0_ = max(c0, 0), max(r0_, 0)
        X = d.x[r0_:r1_, c0:c1] - cx
        Y = d.y[r0_:r1_, c0:c1] - cy
        lt = X * tx + Y * ty       # along tangent
        lr = X * rx + Y * ry       # along radial
        m = np.zeros_like(X)
        kind = rng.integers(0, 4)
        def seg(p0, p1):
            (x0, y0), (x1, y1) = p0, p1
            vx, vy = x1 - x0, y1 - y0
            L2 = vx * vx + vy * vy + 1e-6
            t = np.clip(((lt - x0) * vx + (lr - y0) * vy) / L2, 0, 1)
            dd = np.hypot(lt - (x0 + t * vx), lr - (y0 + t * vy))
            return line_mask(dd, stroke_cm * 0.5, 0.8)
        hw, hh = w * 0.5, h * 0.36
        m = np.maximum(m, seg((0, -hh), (0, hh)))
        if kind == 0:
            m = np.maximum(m, seg((-hw, hh), (0, 0))); m = np.maximum(m, seg((hw, hh), (0, 0)))
        elif kind == 1:
            m = np.maximum(m, seg((-hw, -hh * 0.2), (hw, -hh * 0.2)))
            m = np.maximum(m, line_mask(np.hypot(lt, lr - hh * 0.55) - hh * 0.3, stroke_cm * 0.5, 0.8))
        elif kind == 2:
            m = np.maximum(m, seg((-hw, -hh), (hw, hh)))
        else:
            m = np.maximum(m, seg((0, hh), (hw, hh * 0.3))); m = np.maximum(m, seg((0, -hh * 0.3), (-hw, -hh)))
        out[r0_:r1_, c0:c1] = np.maximum(out[r0_:r1_, c0:c1], m)
    return out


def to_u8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)


def save_rgb(rgb, path):
    Image.fromarray(to_u8(rgb), "RGB").save(path)


def save_gray(g, path):
    Image.fromarray(to_u8(g), "L").save(path)


def hexc(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], np.float32)


def lerp(a, b, t):
    return a + (b - a) * t
