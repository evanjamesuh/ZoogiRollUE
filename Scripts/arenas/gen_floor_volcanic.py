"""Volcanic Pit floor: near-black basalt rune fortress disc (T_VolcanicFloor 4096^2 RGB) + lava-line glow mask
(T_VolcanicGlow 2048^2 L). Layout follows buzzy-arenas/03-volcanic-pit/UPGRADE_SPEC.md (redrawn procedurally):
  boundary r 1536-1550 | outer ring r 850 (10 cm) + thin inner ring r 780 | X spokes 45/135/225/315 from the core to the edge
  molten core r 200 (sun sigil, dark sockets under the 3 centre orbs) | thin sockets at the 15 ring orbs | a few lava cracks
Run: python3 gen_floor_volcanic.py <outdir>
"""
import sys, os
import numpy as np
from PIL import Image, ImageFilter
from arena_tex import *

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)

ORB_RING = [(500 * np.cos(np.radians(i * 24.0)), 500 * np.sin(np.radians(i * 24.0))) for i in range(15)]
CENTRE = [(90 * np.cos(np.radians(30 + 120 * k)), 90 * np.sin(np.radians(30 + 120 * k))) for k in range(3)]
SPAWNS = [(1100 * np.cos(np.radians(a)), 1100 * np.sin(np.radians(a))) for a in (0, 90, 180, 270)]
SPOKES = (45, 135, 225, 315)


def seg_dist(d, x0, y0, x1, y1, X=None, Y=None):
    X = d.x if X is None else X; Y = d.y if Y is None else Y
    vx, vy = x1 - x0, y1 - y0
    t = np.clip(((X - x0) * vx + (Y - y0) * vy) / (vx * vx + vy * vy + 1e-6), 0, 1)
    return np.hypot(X - (x0 + t * vx), Y - (y0 + t * vy))


def cracks(d, seed, count, rmin, rmax, avoid):
    rng = np.random.default_rng(seed)
    out = np.zeros((d.n, d.n), np.float32)
    for c in range(count):
        a = rng.uniform(0, 2 * np.pi); r0 = rng.uniform(rmin, rmax)
        x, y = r0 * np.cos(a), r0 * np.sin(a)
        for s in range(rng.integers(2, 4)):
            ang = rng.uniform(0, 2 * np.pi); px, py = x, y
            w0 = rng.uniform(1.6, 2.6)
            for k in range(rng.integers(4, 9)):
                ang += rng.uniform(-0.6, 0.6)
                L = rng.uniform(14, 34)
                qx, qy = px + L * np.cos(ang), py + L * np.sin(ang)
                c0 = int((min(px, qx) - 6 + R_CM) / d.cm_per_px); c1 = int((max(px, qx) + 6 + R_CM) / d.cm_per_px) + 1
                r0_ = int((min(py, qy) - 6 + R_CM) / d.cm_per_px); r1_ = int((max(py, qy) + 6 + R_CM) / d.cm_per_px) + 1
                c0, r0_ = max(c0, 0), max(r0_, 0)
                X = d.x[r0_:r1_, c0:c1]; Y = d.y[r0_:r1_, c0:c1]
                dd = seg_dist(d, px, py, qx, qy, X, Y)
                w = max(w0 * (1 - k * 0.12), 0.6)
                out[r0_:r1_, c0:c1] = np.maximum(out[r0_:r1_, c0:c1], line_mask(dd, w, 0.7))
                px, py = qx, qy
    return out * avoid


def glow_layout(d):
    g = np.zeros((d.n, d.n), np.float32)
    g = np.maximum(g, line_mask(d.r - 1543, 7.0, 1.4))                     # boundary 1536-1550
    # Checklist #7: inner double arcs at ~0.55 R, broken where the spokes cross.
    arcs = np.maximum(ring(d, 832, 4.5), ring(d, 868, 4.5))
    for sd in SPOKES:
        arcs *= 1 - spoke(d, sd, 45, 700, 1000)
    g = np.maximum(g, arcs * 0.9)
    # Checklist #8: X spokes as double parallel channels (8-10 cm lines, 14 cm apart).
    for sd in SPOKES:
        ar = np.radians(sd)
        perp = -d.x * np.sin(ar) + d.y * np.cos(ar)
        along = d.x * np.cos(ar) + d.y * np.sin(ar)
        inside = smooth(198, 206, along) * (1 - smooth(1530, 1538, along))
        dbl = np.maximum(line_mask(perp - 9.0, 4.0, 1.0), line_mask(perp + 9.0, 4.0, 1.0)) * inside
        g = np.maximum(g, dbl * 0.85)
    core = np.clip(1 - d.r / 200.0, 0, 1)
    g = np.maximum(g, (0.18 + 0.22 * core ** 0.7) * (d.r < 200))
    g = np.maximum(g, ring(d, 200, 5) * 1.0)
    g = np.maximum(g, ring(d, 150, 3) * 0.7)
    # Checklist #3: heptagram {7/3} sigil.
    pts = [(140 * np.cos(np.radians(90 + k * 360.0 / 7)), 140 * np.sin(np.radians(90 + k * 360.0 / 7))) for k in range(7)]
    for k in range(7):
        (x0, y0), (x1, y1) = pts[k], pts[(k + 3) % 7]
        g = np.maximum(g, line_mask(seg_dist(d, x0, y0, x1, y1), 2.2, 0.8) * 0.75)
    g = np.maximum(g, (1 - smooth(26, 34, d.r)) * 0.95)
    for i in range(12):
        g = np.maximum(g, spoke(d, i * 30.0 + 15.0, 2.5, 160, 192) * 0.6)
    for (x, y) in CENTRE:
        rr = np.hypot(d.x - x, d.y - y)
        g = g * (0.15 + 0.85 * smooth(44, 50, rr))
        g = np.maximum(g, circle_at(d, x, y, 52, 2.5) * 0.7)
    for (x, y) in ORB_RING:
        g = np.maximum(g, circle_at(d, x, y, 52, 3.0) * 0.45)
    lines = (np.abs(d.r - 850) < 40) | (np.abs(d.r - 500) < 70) | (d.r < 300) | (d.r > 1460)
    for s in SPOKES:
        lines |= spoke(d, s, 40, 0, 1600) > 0.01
    for a in (0, 90, 180, 270):
        lines |= spoke(d, a, 110, 0, 1600) > 0.01
    avoid = (~lines).astype(np.float32)
    cr = cracks(d, 9, 9, 380, 1380, avoid)
    g = np.maximum(g, cr * 0.5)
    return np.clip(g, 0, 1), cr


def build(n_base=4096, n_glow=2048):
    d = Disc(n_base)
    courses = [262, 340, 420, 500, 590, 680, 770, 930, 1030, 1130, 1230, 1330, 1470, 1550]
    rnd, grout, edge = flagstones(d, courses, seed=31, min_block_cm=120, max_block_cm=210, grout_cm=3.0)
    hub = d.r < 262
    hr, hg, he = flagstones(d, [0, 80, 170, 262], seed=37, min_block_cm=80, max_block_cm=120, grout_cm=2.6)
    rnd = np.where(hub, hr, rnd); grout = np.where(hub, hg, grout); edge = np.where(hub, he, edge)

    A, B = hexc("#423936"), hexc("#2F2826")
    groutC = hexc("#141010")
    t = rnd[..., None]
    col = lerp(A, B, t)
    col = np.where(hub[..., None], lerp(hexc("#2A2321"), hexc("#221C1B"), t), col)
    mott = fbm(d.n, 24, 4, seed=15) - 0.5
    grain = value_noise(d.n, 1024, seed=19) - 0.5
    col = col * (1.0 + 0.22 * mott[..., None] + 0.10 * grain[..., None])
    tilt = (np.cos(d.a * 3.0 + rnd * 40.0) * 0.5 + 0.5) * smooth(0.0, 30.0, edge)
    col = col * (0.93 + 0.12 * tilt[..., None])
    bev = line_mask(edge - 4.0, 2.5, 1.5)
    col = col * (1.0 + 0.28 * bev[..., None])
    col = lerp(col, groutC, grout[..., None])
    for (x, y) in SPAWNS:
        rr = np.hypot(d.x - x, d.y - y)
        col = lerp(col, col * 0.7, line_mask(rr - 100, 3.0)[..., None])
    glyphs = glyph_band(d, 1392, 1462, 72, seed=13, stroke_cm=3.4, skip_deg=SPOKES)
    glyphs = np.maximum(glyphs, glyph_band(d, 268, 300, 28, seed=17, stroke_cm=2.6, skip_deg=SPOKES))
    rb = ring(d, 1392, 2.5) + ring(d, 1462, 2.5) + ring(d, 304, 2.0)
    # Checklist #2: carved brick course with radial ticks around the core (base only, no glow), r 330-450.
    rb = rb + ring(d, 330, 2.0) + ring(d, 450, 2.0)
    for i in range(48):
        rb = rb + spoke(d, i * 7.5 + 3.75, 1.6, 330, 450) * (1.0 if i % 2 else 0.6)
    carve = np.clip(glyphs + rb, 0, 1)
    col = lerp(col, col * 0.5, carve[..., None])

    dg = Disc(n_base)
    g, cr = glow_layout(dg)
    del dg
    halo = np.asarray(Image.fromarray((g * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(10)), np.float32) / 255.0
    col = lerp(col, hexc("#6A2E1C"), np.clip(halo * 0.9, 0, 0.55)[..., None])
    col = lerp(col, hexc("#7A2810"), np.clip(g * 1.6, 0, 1)[..., None] * 0.8)
    col = np.where((d.r > 1470)[..., None], col * 0.85, col)
    col = np.where((d.r > 1550)[..., None], lerp(A, B, 0.6) * 0.8, col)
    lum = (0.2126 * col[..., 0] + 0.7152 * col[..., 1] + 0.0722 * col[..., 2])[d.r < 1550]
    print("base mean value: %.3f" % lum.mean())
    save_rgb(col, os.path.join(OUT, "T_VolcanicFloor.png"))
    del col, rnd, grout, edge, mott, grain, halo, g

    d2 = Disc(n_glow)
    g2, _ = glow_layout(d2)
    save_gray(g2, os.path.join(OUT, "T_VolcanicGlow.png"))
    print("written", OUT)


if __name__ == "__main__":
    build()
