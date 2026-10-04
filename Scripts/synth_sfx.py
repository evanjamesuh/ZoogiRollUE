#!/usr/bin/env python3
"""Zoogi Roll anime-style SFX, synthesized from scratch (numpy/scipy). Oct 4, 2026.

All sounds are ORIGINAL: oscillators, filtered noise, envelopes and a small synthetic reverb. No samples from any
recording are used. The Naruto/Boruto reference clips were only *measured* (by the movie team) for style and timing:
  - impacts: fast attack (onset +10..+30 dB), spectral centroid ~0.9-1.6 kHz, 40-60 % of the energy below 150 Hz on the
    big hits, decays 0.25 s (hits) to 2-3.5 s (big booms / explosions),
  - whooshes build over ~200-300 ms into the hit,
  - a held pose / micro-pause (a beat of near-silence) right before every big hit.
Loudness: each file is normalized to about -16 LUFS max momentary (EBU R128, 400 ms) with true peak <= -1 dBTP,
measured with ffmpeg's ebur128 filter.

Usage: python3 synth_sfx.py <out_dir>
"""
import os, re, subprocess, sys, tempfile
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 48000
RNG = np.random.default_rng(20261004)


def t_axis(dur):
    return np.arange(int(dur * SR)) / SR


def noise(dur):
    return RNG.standard_normal(int(dur * SR))


def bp(x, lo, hi, order=2):
    sos = signal.butter(order, [lo, hi], btype='band', fs=SR, output='sos')
    return signal.sosfilt(sos, x)


def lp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f, btype='low', fs=SR, output='sos'), x)


def hp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f, btype='high', fs=SR, output='sos'), x)


def sweep_filter(x, f0, f1, q=2.0, kind='band', curve=1.0, block=256):
    """Time-varying resonant filter (block-wise state-variable filter), centre frequency f0 -> f1."""
    out = np.zeros_like(x)
    low = band = 0.0
    n = len(x)
    for s in range(0, n, block):
        frac = (s / max(1, n - 1)) ** curve
        f = f0 * (f1 / f0) ** frac
        g = 2 * np.sin(np.pi * min(f, SR * 0.24) / SR)
        damp = 1.0 / q
        seg = x[s:s + block]
        o = np.empty_like(seg)
        for i, v in enumerate(seg):
            high = v - low - damp * band
            band += g * high
            low += g * band
            o[i] = band if kind == 'band' else (low if kind == 'low' else high)
        out[s:s + block] = o
    return out


def env_ad(n, attack, decay, curve=4.0):
    t = np.arange(n) / SR
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    d = np.exp(-np.maximum(t - attack, 0) * curve / max(decay, 1e-4))
    return a * d


def env_rise(n, power=2.0):
    return (np.arange(n) / max(1, n - 1)) ** power


def osc(freqs, kind='sine'):
    """freqs: array of instantaneous Hz."""
    ph = 2 * np.pi * np.cumsum(freqs) / SR
    if kind == 'sine':
        return np.sin(ph)
    if kind == 'saw':
        return 2 * ((ph / (2 * np.pi)) % 1.0) - 1
    if kind == 'square':
        return np.sign(np.sin(ph))
    raise ValueError(kind)


def reverb(x, seconds=0.8, mix=0.25, bright=6000):
    """Small synthetic room: convolution with exponentially decaying filtered noise (no recorded IR)."""
    n = int(seconds * SR)
    ir = RNG.standard_normal(n) * np.exp(-np.arange(n) / SR * 6.9 / seconds)
    ir = lp(ir, bright)
    ir[: int(0.008 * SR)] *= np.linspace(0, 1, int(0.008 * SR))
    ir /= np.sqrt(np.sum(ir ** 2))
    wet = signal.fftconvolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    return dry * (1 - mix) + wet * mix


def pad(x, dur):
    n = int(dur * SR)
    return np.concatenate([x, np.zeros(max(0, n - len(x)))])[:n] if len(x) < n else x


def place(dst, src, at):
    i = int(at * SR)
    end = min(len(dst), i + len(src))
    dst[i:end] += src[: end - i]
    return dst


def crackle(dur, rate, lo=1500, hi=7000, decay=0.004, density_curve=None):
    """Sparse random clicks (debris, embers, sparks)."""
    n = int(dur * SR)
    out = np.zeros(n)
    t = 0.0
    while True:
        r = rate if density_curve is None else rate * density_curve(t / dur)
        t += RNG.exponential(1.0 / max(r, 1e-3))
        if t >= dur:
            break
        k = int(decay * SR * RNG.uniform(0.5, 2.0))
        click = RNG.standard_normal(k) * np.exp(-np.arange(k) / max(1, k / 5))
        place(out, click * RNG.uniform(0.3, 1.0), t)
    return bp(out, lo, hi)


def trim_tail(x, thresh_db=-70):
    a = np.abs(x)
    th = np.max(a) * 10 ** (thresh_db / 20)
    idx = np.where(a > th)[0]
    if len(idx) == 0:
        return x
    end = min(len(x), idx[-1] + int(0.02 * SR))
    y = x[:end].copy()
    f = int(0.01 * SR)
    y[-f:] *= np.linspace(1, 0, f)
    return y


# ---------------------------------------------------------------- sounds

def dash():
    # Fast whoosh: band-passed noise sweeping 500 Hz -> 3.8 kHz, ~230 ms build (the refs' whooshes rise ~200-300 ms), quick tail.
    d = 0.55
    n = noise(d)
    x = sweep_filter(n, 450, 3800, q=3.0, curve=0.7)
    e = np.concatenate([env_rise(int(0.23 * SR), 2.2), np.exp(-np.arange(int(0.32 * SR)) / SR * 14)])[: len(x)]
    air = hp(noise(d), 5000) * e * 0.18
    tone = osc(np.linspace(260, 520, len(x))) * e * 0.08  # faint doppler-ish swish
    return reverb(lp(x * e + air + tone, 9000), 0.4, 0.15)  # tamed top end


def hit_crack():
    # Zoogi-vs-Zoogi hit, per the team brief (TEAM-NOTES s.4): weight in a soft low thump on the contact frame,
    # a mid "boing/squish", a modest mid crack for the anime snap and one tiny bright tick. Soft-clipped, small room.
    d = 0.5
    t = t_axis(d)
    thump = osc(70 + 110 * np.exp(-t * 32)) * np.exp(-t * 18) * 0.85
    boing = osc((230 + 90 * np.exp(-t * 25)) * (1 + 0.03 * np.sin(2 * np.pi * 16 * t))) * np.exp(-t * 16) * 0.45
    squish = bp(noise(d), 400, 1300) * np.exp(-t * 38) * 0.6
    crack = bp(noise(d), 1200, 4200) * np.exp(-t * 85) * 0.75
    tick = osc(np.full(len(t), 3150.0)) * np.exp(-t * 90) * 0.12
    y = np.tanh((thump + boing + squish + crack) * 1.5) + tick
    return reverb(lp(y, 9000), 0.5, 0.16, 4000)


def crit_boom():
    # Big crit / KO hit. Lands on the same frame as the impact flash (no delay; the held-pose beat is the Inhale gap
    # before the launch plus a ~4.4 dB music duck). Distorted low boom, a sub drop 110 -> 32 Hz over ~1.2 s,
    # a mid punch, one small bright crack layer and a long low rumble (refs: >35 % of energy < 150 Hz, 2-3.5 s tails).
    d = 2.6
    t = t_axis(d)
    sub = osc(32 + 78 * np.exp(-t * 2.6)) * np.exp(-t * 1.9)
    boom = lp(noise(d), 260, 4) * np.exp(-t * 3.2) * 2.2
    punch = bp(noise(d), 600, 2000) * np.exp(-t * 28) * 0.8
    crack = bp(noise(d), 2500, 6000) * np.exp(-t * 60) * 0.35
    rumble = lp(noise(d), 120, 4) * env_ad(len(t), 0.05, 2.2, 3.0) * 1.6
    zap = crackle(0.5, 90, 1500, 5000, 0.003, lambda u: 1 - u) * 0.5  # power-peak crackle accent
    y = np.tanh((sub * 1.1 + boom + punch + crack) * 1.4) + rumble
    place(y, zap, 0.02)
    return reverb(y, 1.4, 0.22, 3500)


def inhale():
    # Wind-up before a launch: a breath-in swell (reverse-envelope noise, low-pass opening 300 -> 3 kHz, rising tone),
    # hard cut, then 0.11 s silence so the launch whoosh lands after a beat. 0.75 s total.
    d = 0.64
    x = sweep_filter(noise(d), 300, 3000, q=1.6, kind='low', curve=1.6)
    e = env_rise(len(x), 2.6)
    tone = osc(np.linspace(140, 420, len(x)) * (1 + 0.01 * np.sin(np.arange(len(x)) / SR * 2 * np.pi * 9))) * e * 0.2
    y = (x * 0.9 + tone) * e
    y[-int(0.006 * SR):] *= np.linspace(1, 0, int(0.006 * SR))
    return np.concatenate([y, np.zeros(int(0.11 * SR))])


def awaken_charge():
    # Infuse power-up: a low charge-hum drone (55/110 Hz) growing under a detuned saw chord sweeping up (200 -> 760 Hz)
    # with accelerating tremolo, a little shimmer, and a short sparkle + chime at the release. ~1.5 s.
    d = 1.15
    t = t_axis(d)
    f = 200 * (760 / 200) ** (t / d) ** 1.4
    chord = sum(osc(f * m * (1 + det), 'saw') for m, det in [(1, 0), (1, 0.007), (1.5, -0.004), (2, 0.003)]) / 4
    chord = lp(chord, 2400)
    trem = 0.6 + 0.4 * np.sin(2 * np.pi * np.cumsum(4 + 22 * (t / d) ** 2) / SR)
    hum = (osc(np.full(len(t), 55.0)) * 0.8 + osc(np.full(len(t), 110.5)) * 0.4) * (0.7 + 0.3 * np.sin(2 * np.pi * 6 * t))
    shimmer = bp(noise(d), 3500, 8000) * (t / d) ** 2 * 0.1
    y = (chord * trem * 0.8 + hum * 0.4 + shimmer) * env_rise(len(t), 1.5)
    tail = 0.4
    tt = t_axis(tail)
    spark = (crackle(tail, 90, 2500, 7000, 0.003) * 0.6 + osc(np.full(len(tt), 1046.5)) * 0.25
             + osc(np.full(len(tt), 1568.0)) * 0.15) * np.exp(-tt * 8)
    boom = lp(noise(tail), 200, 4) * np.exp(-tt * 10) * 1.2
    out = np.concatenate([y, np.zeros(len(tt))])
    place(out, spark + boom, d - 0.02)
    return reverb(out, 0.9, 0.25, 4500)


def guard_clang():
    # Shield: a soft low "whomp" (swelling low-passed noise + 120 Hz thud) under a metal clang (inharmonic partials,
    # top partials kept quiet so it rings rather than stings), plus a short click.
    d = 1.1
    t = t_axis(d)
    parts = [(392, 1.0, 3.0), (941, 0.7, 4.5), (1453, 0.45, 6.0), (2087, 0.25, 8.0), (2741, 0.15, 10.0), (3420, 0.08, 13.0)]
    y = sum(a * np.sin(2 * np.pi * f * t + RNG.uniform(0, 6.28)) * np.exp(-t * k) for f, a, k in parts)
    y *= 1 + 0.15 * np.sin(2 * np.pi * 7 * t)
    click = bp(noise(d), 1500, 5000) * np.exp(-t * 250) * 0.9
    thud = osc(120 + 40 * np.exp(-t * 30)) * np.exp(-t * 14) * 0.9
    whomp = lp(noise(d), 300, 4) * env_ad(len(t), 0.02, 0.35, 3.0) * 1.6
    return reverb(y * 0.45 + click + thud + whomp, 0.8, 0.22, 5000)


def rock_crack():
    # Rock crack: a dry knock + a burst of gritty crackle (stone splitting) + a few pebble ticks.
    d = 0.6
    t = t_axis(d)
    knock = osc(70 + 90 * np.exp(-t * 40)) * np.exp(-t * 30) * 0.9
    split = crackle(0.18, 900, 900, 6000, 0.003)
    split = pad(split, d) * np.exp(-t * 12)
    grit = bp(noise(d), 1200, 4500) * np.exp(-t * 35) * 0.5
    ticks = crackle(d, 25, 2000, 7000, 0.002, lambda u: max(0.0, 1 - u))
    return reverb(np.tanh((knock + split * 1.4 + grit) * 1.3) + ticks * 0.6, 0.5, 0.15)


def rock_shatter():
    # Rock shatter: low thump + broadband crunch, then a debris rattle that thins out over ~1.6 s (pebbles bouncing).
    d = 2.0
    t = t_axis(d)
    thump = osc(45 + 70 * np.exp(-t * 18)) * np.exp(-t * 7) * 1.2
    crunch = lp(noise(d), 3500) * np.exp(-t * 14) * 1.3
    crunch2 = crackle(0.35, 1600, 700, 5000, 0.004)
    rattle = crackle(1.7, 70, 1500, 6500, 0.003, lambda u: (1 - u) ** 1.5 + 0.05)
    pebbles = crackle(1.7, 18, 500, 2500, 0.006, lambda u: (1 - u) ** 2)
    y = np.tanh((thump + crunch) * 1.2)
    place(y, crunch2 * 1.3, 0.0)
    place(y, rattle * 0.7, 0.12)
    place(y, pebbles * 0.6, 0.2)
    return reverb(y, 0.9, 0.18)


def sting_ember():
    # Fire burst: low "whoomp" (lowpassed noise, fast swell), roaring band noise and ember crackle.
    d = 1.1
    t = t_axis(d)
    whoomp = lp(noise(d), 500, 4) * env_ad(len(t), 0.04, 0.5, 3.5) * 2.0
    roar = sweep_filter(noise(d), 1800, 500, q=1.2, curve=0.6) * env_ad(len(t), 0.02, 0.7, 3.0)
    embers = crackle(d, 60, 2500, 8000, 0.002, lambda u: 1 - u)
    return reverb(np.tanh(whoomp + roar * 0.9) + embers * 0.8, 0.7, 0.2)


def sting_storm():
    # Electric zap: buzzy square at ~95 Hz with random FM jitter, bright arcing crackle, sharp attack.
    d = 0.8
    t = t_axis(d)
    jitter = lp(RNG.standard_normal(len(t)), 60) * 40
    buzz = osc(95 + jitter + 30 * np.exp(-t * 10), 'square') * env_ad(len(t), 0.003, 0.35, 3.0)
    buzz = bp(buzz, 150, 5000)
    arcs = crackle(d, 400, 2000, 7000, 0.0015, lambda u: np.exp(-u * 4))
    snap = bp(noise(d), 2000, 6000) * np.exp(-t * 120) * 0.8
    return reverb(np.tanh(buzz * 0.8 + arcs * 1.2 + snap), 0.5, 0.15)


def sting_frost():
    # Ice shatter: a soft low thud for weight, glassy partials (random 2-6 kHz, fast decays), tinkle crackle, light hiss.
    d = 1.2
    t = t_axis(d)
    glass = np.zeros(len(t))
    for _ in range(12):
        f = RNG.uniform(2000, 6000)
        at = RNG.uniform(0, 0.25)
        k = RNG.uniform(7, 16)
        tt = t_axis(d - at)
        place(glass, np.sin(2 * np.pi * f * tt) * np.exp(-tt * k) * RNG.uniform(0.2, 0.5), at)
    thud = osc(90 + 60 * np.exp(-t * 30)) * np.exp(-t * 14) * 1.0
    hit = bp(noise(d), 1500, 6000) * np.exp(-t * 60) * 0.7
    tinkle = crackle(d, 40, 3000, 8000, 0.002, lambda u: np.exp(-u * 3)) * 0.7
    air = bp(noise(d), 4000, 9000) * env_ad(len(t), 0.05, 0.9, 3.0) * 0.1
    return reverb(glass * 0.5 + thud + hit + tinkle + air, 1.0, 0.28, 6000)


def sting_titan():
    # Heavy stone slam: very low thump (40-90 Hz), lowpassed crunch and a short ground rumble.
    d = 1.3
    t = t_axis(d)
    thump = osc(40 + 50 * np.exp(-t * 14)) * np.exp(-t * 5) * 1.4
    crunch = lp(noise(d), 900, 4) * np.exp(-t * 18) * 1.5
    grit = bp(noise(d), 1500, 4000) * np.exp(-t * 40) * 0.4
    rumble = lp(noise(d), 90, 4) * env_ad(len(t), 0.03, 0.9, 3.0) * 1.5
    return reverb(np.tanh((thump + crunch + grit) * 1.3) + rumble, 0.8, 0.15, 3000)


def sting_void():
    # Dark warp: a reverse swell into a falling sine (420 -> 55 Hz) through a slow flanger, phasey low drone.
    d = 1.4
    t = t_axis(d)
    swell_n = int(0.35 * SR)
    swell = lp(noise(0.35), 1200) * env_rise(swell_n, 3.0) * 0.8
    fall = osc(55 + 365 * np.exp(-t * 3.5)) * env_ad(len(t), 0.01, 1.0, 3.0)
    drone = osc(np.full(len(t), 48.0), 'saw')
    drone = lp(drone, 400) * env_ad(len(t), 0.1, 1.1, 3.0) * 0.5
    x = fall + drone
    # flanger: mix with a slowly modulated delay copy
    dly = (0.002 + 0.0015 * np.sin(2 * np.pi * 0.7 * t)) * SR
    idx = np.clip(np.arange(len(t)) - dly, 0, len(t) - 1)
    x = x + np.interp(idx, np.arange(len(t)), x) * 0.8
    out = np.concatenate([swell, x])
    return reverb(out, 1.2, 0.3, 2500)


SOUNDS = {
    'Dash': dash, 'HitCrack': hit_crack, 'CritBoom': crit_boom, 'Inhale': inhale, 'AwakenCharge': awaken_charge,
    'GuardClang': guard_clang, 'RockCrack': rock_crack, 'RockShatter': rock_shatter,
    'StingEmber': sting_ember, 'StingStorm': sting_storm, 'StingFrost': sting_frost, 'StingTitan': sting_titan, 'StingVoid': sting_void,
}
NO_TRIM = {'Inhale', 'CritBoom'}  # keep the built-in silences


def measure(x):
    """(max momentary LUFS, true peak dBTP) via ffmpeg ebur128; short files are padded with silence for the 400 ms window."""
    y = np.concatenate([np.zeros(int(0.05 * SR)), x, np.zeros(int(0.45 * SR))])
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
        wavfile.write(f.name, SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        name = f.name
    r = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-v', 'verbose', '-i', name, '-af', 'ebur128=peak=true:framelog=verbose', '-f', 'null', '-'],
                       capture_output=True, text=True)
    os.unlink(name)
    ms = [float(m) for m in re.findall(r'M:\s*(-?[\d.]+)', r.stderr)]
    tp = re.findall(r'Peak:\s*(-?[\d.]+|-inf)\s*dBFS', r.stderr)
    return (max(ms) if ms else -70.0), (float(tp[-1]) if tp and tp[-1] != '-inf' else -70.0)


def main(out):
    os.makedirs(out, exist_ok=True)
    report = []
    for name, fn in SOUNDS.items():
        x = fn()
        x = x - np.mean(x)
        if name not in NO_TRIM:
            x = trim_tail(x)
        x = x / (np.max(np.abs(x)) + 1e-9) * 0.5
        for _ in range(4):
            m, tp = measure(x)
            gain_db = -16.0 - m
            if tp + gain_db > -1.0:
                gain_db = -1.0 - tp
            x = x * 10 ** (gain_db / 20)
            if abs(gain_db) < 0.2:
                break
        m, tp = measure(x)
        wavfile.write(os.path.join(out, name + '.wav'), SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
        report.append((name, len(x) / SR, m, tp))
        print('%-13s %5.2fs  max momentary %6.1f LUFS  true peak %5.1f dBTP' % (name, len(x) / SR, m, tp))
    return report


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'SFX')
