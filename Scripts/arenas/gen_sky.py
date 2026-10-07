"""Painted day/dusk panoramas for the Buzzy arenas (4096x2048 equirect, seamless in yaw). Procedural only.
The lower half is a ground plane seen from altitude (perspective-correct, so it never seams), fading into haze at the
horizon; layered hill ridges sit on the horizon; soft anime cumulus above. Usage: python3 gen_sky.py <outdir> meadow
"""
import sys, os
import numpy as np
from PIL import Image, ImageFilter
from scipy.ndimage import map_coordinates
from arena_tex import value_noise, fbm, hexc, smooth, save_rgb

W, H = 4096, 2048


def periodic_1d(n, cells, seed, octaves=4):
    rng = np.random.default_rng(seed)
    out = np.zeros(n, np.float32)
    amp, tot = 1.0, 0.0
    x = np.arange(n) / n
    for o in range(octaves):
        c = cells * 2 ** o
        pts = rng.random(c + 1).astype(np.float32); pts[-1] = pts[0]
        xi = x * c
        i0 = np.floor(xi).astype(int); f = xi - i0
        f = f * f * (3 - 2 * f)
        out += amp * (pts[i0] * (1 - f) + pts[(i0 + 1)] * f)
        tot += amp; amp *= 0.5
    return out / tot


PEAKS = []


def sky(theme, out):
    PEAKS.clear()
    u = (np.arange(W, dtype=np.float32) + 0.5) / W
    v = (np.arange(H, dtype=np.float32) + 0.5) / H
    az = (u - 0.5) * 2 * np.pi                    # matches material: u = atan2(y,x)/2pi + 0.5
    el = (0.5 - v) * np.pi                        # +pi/2 zenith .. -pi/2 nadir
    AZ = az[None, :].repeat(H, 0); EL = el[:, None].repeat(W, 1)
    T = THEMES[theme]
    img = np.zeros((H, W, 3), np.float32)

    # ---- sky gradient + sun glow
    t = np.clip(EL / (np.pi / 2), 0, 1) ** 0.6
    skyc = hexc(T["horizon"]) * (1 - t[..., None]) + hexc(T["zenith"]) * t[..., None]
    sd = np.array([np.cos(np.radians(T["sun_el"])) * np.cos(np.radians(T["sun_az"])),
                   np.cos(np.radians(T["sun_el"])) * np.sin(np.radians(T["sun_az"])), np.sin(np.radians(T["sun_el"]))])
    dirs = np.stack([np.cos(EL) * np.cos(AZ), np.cos(EL) * np.sin(AZ), np.sin(EL)], -1)
    cosang = np.clip((dirs * sd).sum(-1), -1, 1)
    glow = np.exp((cosang - 1) * 14.0)
    skyc = skyc + hexc(T["sun"])[None, None, :] * (glow[..., None] * T["sun_str"])
    img[:] = skyc

    # ---- clouds (upper band), tileable in u
    cn = fbm(W, 6, 6, seed=T["seed"], wrap=True)[:H, :]            # use top part of a square tile
    cn2 = fbm(W, 24, 4, seed=T["seed"] + 5, wrap=True)[:H, :]
    band = smooth(0.02, 0.10, EL) * (1 - smooth(0.45, 0.85, EL))
    dens = np.clip((cn * 0.8 + cn2 * 0.3 - T["cloud_cut"]) * 7.0, 0, 1) * band
    dens = np.asarray(Image.fromarray((dens * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2)), np.float32) / 255
    # light from above: compare with density a little lower -> top-lit edges
    shift = np.roll(dens, -10, axis=0)
    lit = np.clip(0.6 + (dens - shift) * 3.0, 0.35, 1.0)
    ccol = hexc(T["cloud_shadow"]) * (1 - lit[..., None]) + hexc(T["cloud"]) * lit[..., None]
    img = img * (1 - dens[..., None] * T["cloud_alpha"]) + ccol * (dens[..., None] * T["cloud_alpha"])

    # ---- ground plane seen from altitude (below the horizon)
    below = EL < 0
    hgt = 1.0
    dist = hgt / np.maximum(np.tan(-EL), 1e-3)                   # ground distance
    gx = np.cos(AZ) * dist; gy = np.sin(AZ) * dist
    G = 1024
    tiles = [fbm(G, 8, 5, seed=T["seed"] + 11, wrap=True), fbm(G, 32, 3, seed=T["seed"] + 12, wrap=True),
             value_noise(G, 128, seed=T["seed"] + 13, wrap=True)]
    def sample(tile, scale):
        cx = (gx * scale * G) % G; cy = (gy * scale * G) % G
        return map_coordinates(tile, [cy.ravel(), cx.ravel()], order=1, mode="wrap").reshape(gx.shape)
    sel = below
    f1 = np.where(sel, sample(tiles[0], 0.12), 0)
    f2 = np.where(sel, sample(tiles[1], 0.10), 0)
    f3 = np.where(sel, sample(tiles[1], 0.45), 0)
    f4 = np.where(sel, sample(tiles[0], 0.045), 0)
    g0, g1, g2 = hexc(T["ground_a"]), hexc(T["ground_b"]), hexc(T["ground_c"])
    k = np.clip((f1 - 0.35) * 2.5, 0, 1)[..., None]
    gcol = g0 * (1 - k) + g1 * k
    fields = (f2 > 0.62)[..., None]
    gcol = np.where(fields, gcol * 0.6 + g2 * 0.4, gcol)
    trees = (f3 > T["tree_cut"])[..., None]
    gcol = np.where(trees, gcol * 0.55 + hexc(T["tree"]) * 0.2, gcol)
    # rivers: thin bands along iso-lines of a smooth noise
    if T.get("river"):
        rv = np.abs(f4 - 0.5)
        rmask = (1 - smooth(0.003, 0.008, rv)) * smooth(-0.02, -0.25, EL)
        gcol = gcol * (1 - rmask[..., None]) + hexc(T["river"]) * rmask[..., None]
    # haze toward the horizon
    hz = np.clip(1 - np.exp(-dist * T.get("haze_k", 0.30)), 0, 1) ** 1.1
    gcol = gcol * (1 - hz[..., None]) + hexc(T["haze"]) * hz[..., None]
    img = np.where(below[..., None], gcol, img)

    # ---- hill ridges on the horizon (far -> near), with waterfalls on the mid ridge
    for i, R in enumerate(T["ridges"]):
        prof = periodic_1d(W, R["cells"], T["seed"] + 40 + i)
        prof = (prof - prof.min()) / max(prof.max() - prof.min(), 1e-6)     # full 0..1 range: real hills, not a flat band
        top_deg = R["base"] + R["amp"] * prof
        for (paz, ph, pw) in R.get("peaks", ()):                  # volcano cones: (azimuth deg, height deg, half-width deg)
            dz = np.abs(((np.degrees(az) - paz + 180) % 360) - 180)
            cone = R["base"] + ph * np.clip(1 - dz / pw, 0, 1) ** 1.3
            crater = ph * 0.06 * np.clip(1 - dz / (pw * 0.12), 0, 1)
            top_deg = np.maximum(top_deg, cone - crater)
            if R.get("vent"):
                PEAKS.append((paz, R["base"] + ph * 0.94))
        top_el = np.radians(top_deg)                              # ridge top elevation per column
        bot_el = np.radians(R["bottom"])
        mask = ((EL <= top_el[None, :]) & (EL >= bot_el)).astype(np.float32)
        edge = np.clip((top_el[None, :] - EL) / np.radians(0.25), 0, 1)
        mask *= edge
        shade = np.clip((top_el[None, :] - EL) / np.radians(R["amp"] + 2.0), 0, 1)
        rc = hexc(R["top"]) * (1 - shade[..., None]) + hexc(R["foot"]) * shade[..., None]
        tex = fbm(W, 64, 3, seed=T["seed"] + 60 + i, wrap=True)[:H, :]
        rc = rc * (0.92 + 0.16 * tex[..., None])
        if R.get("falls"):
            rng = np.random.default_rng(T["seed"] + 70 + i)
            for fcol in rng.choice(W, R["falls"], replace=False):
                wcol = int(W * 0.0015)
                ftop = top_el[fcol] - np.radians(0.5)
                fbot = ftop - np.radians(rng.uniform(2.0, 4.0))
                cols = slice(max(fcol - wcol, 0), fcol + wcol)
                fm = ((EL[:, cols] < ftop) & (EL[:, cols] > fbot)).astype(np.float32)
                rc[:, cols] = rc[:, cols] * (1 - fm[..., None] * 0.85) + hexc(T.get("fall_col", "#F2FAFF")) * fm[..., None] * 0.85
        a = mask[..., None] * R.get("alpha", 1.0)
        img = img * (1 - a) + rc * a
    # ---- smoke plumes rising from the volcano vents (soft, wind-bent, dark), then a glow at each vent
    for (paz, pel) in PEAKS:
        azd = np.degrees(AZ); eld = np.degrees(EL)
        hgt = np.clip(eld - pel, 0, None)
        cx = paz + hgt * T.get("plume_lean", 0.6)
        wdt = 2.2 + hgt * 0.8
        dx = np.abs(((azd - cx + 180) % 360) - 180)
        nz = fbm(W, 32, 4, seed=int(paz) + 900, wrap=True)[:H, :]
        m = np.clip(1 - dx / (wdt * (0.75 + 0.5 * nz)), 0, 1) * (eld > pel - 0.5) * np.clip(1 - hgt / T.get("plume_h", 40.0), 0, 1)
        m = smooth(0.0, 0.6, m) * T.get("plume_alpha", 0.8)
        pc = hexc(T["plume"]) * (0.8 + 0.4 * nz[..., None])
        img = img * (1 - m[..., None]) + pc * m[..., None]
        vg = np.exp(-(((azd - paz + 180) % 360 - 180) ** 2 + ((eld - pel) * 2.0) ** 2) / 1.5)
        img = img + hexc(T["vent"])[None, None, :] * vg[..., None] * 0.9
    if T.get("embers"):
        rng = np.random.default_rng(T["seed"] + 1)
        k = T["embers"]
        ys = rng.integers(int(H * 0.30), int(H * 0.52), k); xs = rng.integers(0, W, k)
        img[ys, xs] = np.maximum(img[ys, xs], hexc("#FFB060"))
    save_rgb(img, os.path.join(out, "T_%sSky.png" % T["key"]))
    print("sky written", theme)


THEMES = {
    "volcanic": dict(key="Volcanic", seed=700, zenith="#1E0B0A", horizon="#B8441C", sun="#FF7A2A", sun_az=200, sun_el=8, sun_str=0.55,
                     cloud="#5A3A32", cloud_shadow="#24120F", cloud_cut=0.47, cloud_alpha=0.8,
                     ground_a="#2A1612", ground_b="#3A1C14", ground_c="#4A2216", tree="#120806", tree_cut=0.72,
                     river="#FF6A1A", haze="#7A2A14", fall_col="#FF8A2A",
                     plume="#2E201E", vent="#FF7A2A", plume_lean=0.35, plume_h=38.0, plume_alpha=0.85, embers=900,
                     ridges=[dict(cells=3, base=0.0, amp=5.0, bottom=-1.5, top="#4A2620", foot="#6A3020", alpha=0.85,
                                  peaks=[(35, 24, 20), (160, 17, 14), (250, 27, 22), (320, 14, 12)], vent=True),
                             dict(cells=5, base=-1.0, amp=5.0, bottom=-2.5, top="#2E1714", foot="#4A2218", falls=9),
                             dict(cells=9, base=-2.5, amp=3.5, bottom=-4.5, top="#1C0E0C", foot="#3A1A12", falls=6)]),
    "meadow": dict(key="Meadow", seed=500, zenith="#4E8FDB", horizon="#D3ECF7", sun="#FFF1C8", sun_az=215, sun_el=48, sun_str=0.22,
                   cloud="#FFFFFF", cloud_shadow="#A9C3DD", cloud_cut=0.50, cloud_alpha=0.95,
                   ground_a="#78AE55", ground_b="#93C266", ground_c="#C7D27A", tree="#2F5E2A", tree_cut=0.70,
                   river="#9ED3EA", haze="#BFDCE6", haze_k=0.14,
                   ridges=[dict(cells=3, base=0.5, amp=13.0, bottom=-1.5, top="#9EC3CF", foot="#BCD9E0", alpha=0.8),
                           dict(cells=5, base=-0.5, amp=8.0, bottom=-2.5, top="#86BC7A", foot="#AFD0B0", falls=7),
                           dict(cells=8, base=-2.0, amp=4.5, bottom=-4.5, top="#6AA955", foot="#A3C9A0")]),
}

if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for th in sys.argv[2:]:
        sky(th, out)
