"""Meadow Arena floor: carved sandstone rune disc (T_MeadowFloor 4096^2 RGB) + glow mask (T_MeadowGlow 2048^2 L).
Layout follows buzzy-arenas/01-meadow-arena/UPGRADE_SPEC.md (redrawn procedurally; no pixels copied from the art).
  boundary glow r 1536-1550 (14 cm) | orb-ring line r 500 (6 cm) | ring r 900 (6 cm) | spokes 45/135/225/315 (5 cm)
  hub sigil r 220 | sockets at the 15 ring orbs (r 500, every 24 deg) + 3 centre orbs (r 90, 30/150/270 deg)
Run: python3 gen_floor_meadow.py <outdir>
"""
import sys, os
import numpy as np
from arena_tex import *

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)

ORB_RING = [(500 * np.cos(np.radians(i * 24.0)), 500 * np.sin(np.radians(i * 24.0))) for i in range(15)]
CENTRE = [(90 * np.cos(np.radians(30 + 120 * k)), 90 * np.sin(np.radians(30 + 120 * k))) for k in range(3)]
SPAWNS = [(1100 * np.cos(np.radians(a)), 1100 * np.sin(np.radians(a))) for a in (0, 90, 180, 270)]
SPOKES = (45, 135, 225, 315)


def glow_layout(d):
    """Returns the glow mask (0..1) and a 'groove' mask (where the base is carved for the inlay)."""
    g = np.zeros((d.n, d.n), np.float32)
    g = np.maximum(g, line_mask(d.r - 1543, 7.0, 1.4) * 1.0)              # boundary 1536-1550
    g = np.maximum(g, ring(d, 500, 6) * 0.75)                                # orb-ring line
    g = np.maximum(g, ring(d, 900, 6) * 0.75)
    for s in SPOKES:
        g = np.maximum(g, spoke(d, s, 5, 262, 1536) * 0.65)
    # Hub sigil (flush): outer ring, inner ring, centre-orb triangle, small ticks.
    g = np.maximum(g, ring(d, 220, 5) * 0.8)
    g = np.maximum(g, ring(d, 150, 3.5) * 0.6)
    g = np.maximum(g, ring(d, 40, 3.5) * 0.6)
    for k in range(3):
        (x0, y0), (x1, y1) = CENTRE[k], CENTRE[(k + 1) % 3]
        # chord between two centre sockets, trimmed by the socket circles
        vx, vy = x1 - x0, y1 - y0
        L = np.hypot(vx, vy)
        t = np.clip(((d.x - x0) * vx + (d.y - y0) * vy) / (L * L), 0.0, 1.0)
        dd = np.hypot(d.x - (x0 + t * vx), d.y - (y0 + t * vy))
        tri = line_mask(dd, 1.6, 0.8)
        tri *= smooth(50, 54, np.hypot(d.x - x0, d.y - y0)) * smooth(50, 54, np.hypot(d.x - x1, d.y - y1))
        g = np.maximum(g, tri * 0.5)
    for i in range(24):
        a = i * 15.0 + 7.5
        g = np.maximum(g, spoke(d, a, 3, 160, 208) * (0.55 if i % 2 == 0 else 0.35))
    # Orb sockets: thin ring just outside each orb (orb radius 40).
    for (x, y) in ORB_RING + CENTRE:
        g = np.maximum(g, circle_at(d, x, y, 52, 3.5) * 0.55)
    groove = np.clip(g * 1.6, 0, 1)
    return g, groove


def build(n_base=4096, n_glow=2048):
    # ---------------------------------------------------------------- base colour
    d = Disc(n_base)
    courses = [262, 365, 452, 548, 640, 735, 830, 970, 1075, 1180, 1285, 1385, 1470, 1550]
    rnd, grout, edge = flagstones(d, courses, seed=11)
    # Hub dais: radial wedge stones in two rings plus a centre disc.
    hub = d.r < 262
    hr, hg, he = flagstones(d, [0, 70, 160, 262], seed=23, min_block_cm=70, max_block_cm=110, grout_cm=2.2)
    rnd = np.where(hub, hr, rnd); grout = np.where(hub, hg, grout); edge = np.where(hub, he, edge)

    stoneA, stoneB = hexc("#C4AA84"), hexc("#A58964")
    hubA, hubB = hexc("#D2BD98"), hexc("#B9A07A")
    groutC = hexc("#6F5D45")
    moss = hexc("#5E7F35")

    t = rnd[..., None]
    col = lerp(stoneA, stoneB, t)
    col = np.where(hub[..., None], lerp(hubA, hubB, t), col)
    # Mottling + fine grain (two octaves of value noise), stronger on a few stones.
    mott = fbm(d.n, 24, 4, seed=5) - 0.5
    grain = value_noise(d.n, 1024, seed=9) - 0.5
    col = col * (1.0 + 0.16 * mott[..., None] + 0.06 * grain[..., None])
    # Per-stone tilt: a soft directional gradient inside each stone (reads as uneven, hand-laid slabs).
    tilt = (np.cos(d.a * 3.0 + rnd * 40.0) * 0.5 + 0.5) * smooth(0.0, 30.0, edge)
    col = col * (0.95 + 0.08 * tilt[..., None])
    # Weathered, slightly darker stone edges (carved bevel) and a soft highlight on the sun side.
    bev = 1.0 - smooth(0.0, 7.0, edge)
    col = col * (1.0 - 0.18 * bev[..., None])
    # Grout with patchy moss.
    mossy = smooth(0.55, 0.75, fbm(d.n, 32, 3, seed=77))
    gcol = lerp(groutC, moss, (mossy * 0.8)[..., None])
    col = lerp(col, gcol, grout[..., None])
    # Dais border band 220-262 (dark carved stone) and spawn-pad stone circles.
    band = smooth(218, 221, d.r) * (1 - smooth(259, 262, d.r))
    col = lerp(col, col * 0.78, band[..., None])
    for (x, y) in SPAWNS:
        rr = np.hypot(d.x - x, d.y - y)
        col = lerp(col, col * 0.8, line_mask(rr - 100, 3.0)[..., None])
        col = lerp(col, lerp(hubA, hubB, 0.4) * (1 + 0.1 * mott[..., None]), (rr < 97)[..., None] * 0.6)
    # Carved rune band between 1392 and 1462 (engraved, not glowing): darker glyphs.
    glyphs = glyph_band(d, 1392, 1462, 72, seed=3, stroke_cm=3.2, skip_deg=SPOKES)
    rb = ring(d, 1392, 2.5) + ring(d, 1462, 2.5)
    carve = np.clip(glyphs + rb, 0, 1)
    col = lerp(col, col * 0.62, carve[..., None])
    # A few old hairline cracks (static flavour; live battle damage is a later decal pass).
    rng = np.random.default_rng(42)
    crack = np.zeros((d.n, d.n), np.float32)
    for c in range(7):
        a = rng.uniform(0, 2 * np.pi); r0 = rng.uniform(600, 1400)
        x, y = r0 * np.cos(a), r0 * np.sin(a)
        for s in range(3):
            ang = rng.uniform(0, 2 * np.pi)
            px, py = x, y
            for k in range(rng.integers(4, 8)):
                ang += rng.uniform(-0.7, 0.7)
                L = rng.uniform(10, 26)
                qx, qy = px + L * np.cos(ang), py + L * np.sin(ang)
                vx, vy = qx - px, qy - py
                c0 = int((min(px, qx) - 4 + R_CM) / d.cm_per_px); c1 = int((max(px, qx) + 4 + R_CM) / d.cm_per_px) + 1
                r0_ = int((min(py, qy) - 4 + R_CM) / d.cm_per_px); r1_ = int((max(py, qy) + 4 + R_CM) / d.cm_per_px) + 1
                X = d.x[r0_:r1_, c0:c1]; Y = d.y[r0_:r1_, c0:c1]
                tt = np.clip(((X - px) * vx + (Y - py) * vy) / (vx * vx + vy * vy), 0, 1)
                dd = np.hypot(X - (px + tt * vx), Y - (py + tt * vy))
                crack[r0_:r1_, c0:c1] = np.maximum(crack[r0_:r1_, c0:c1], line_mask(dd, 0.7, 0.6) * (1 - k * 0.08))
                px, py = qx, qy
    col = lerp(col, col * 0.45, crack[..., None])

    # Carved inlay grooves where the glow lines run (the line reads as a carved channel even without emissive).
    dg = Disc(n_base)
    _, groove = glow_layout(dg)
    del dg
    col = lerp(col, hexc("#7E9E6A"), groove[..., None] * 0.65)
    # Checklist #3: the art carves 8 spokes; only the 4 diagonals glow (no glow under the spawn lanes), so the 4 cardinal
    # spokes are carved channels only, broken around the spawn pads.
    for a in (0, 90, 180, 270):
        sp = spoke(d, a, 4, 262, 1392)
        ax, ay = 1100 * np.cos(np.radians(a)), 1100 * np.sin(np.radians(a))
        sp *= smooth(104, 112, np.hypot(d.x - ax, d.y - ay))
        col = lerp(col, col * 0.6, sp[..., None])
    # Outer curb course (1470-1550) a touch darker so the edge reads.
    col = np.where((d.r > 1470)[..., None], col * 0.9, col)
    # Outside the disc: fill with curb colour (never visible; avoids dark mip bleeding at the rim).
    col = np.where((d.r > 1550)[..., None], lerp(stoneA, stoneB, 0.6) * 0.85, col)
    lum = (0.2126 * col[..., 0] + 0.7152 * col[..., 1] + 0.0722 * col[..., 2])[d.r < 1550]
    print("base mean luminance (linear-ish sRGB value): %.3f" % lum.mean())
    save_rgb(col, os.path.join(OUT, "T_MeadowFloor.png"))
    del col, rnd, grout, edge, mott, grain

    # ---------------------------------------------------------------- glow mask
    d2 = Disc(n_glow)
    g, _ = glow_layout(d2)
    save_gray(g, os.path.join(OUT, "T_MeadowGlow.png"))
    print("written", OUT)


if __name__ == "__main__":
    build()
