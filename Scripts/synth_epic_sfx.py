# Nov 3 b103 (Claude): "make the game the most epic ... realistic rock and debris" (war drums = the feel, not literal drums).
# Original synthesis (numpy only, project-owned): cinematic trailer impacts (sub drop + body + metal ring), risers, rock smash,
# debris crumble tails and sub rumbles, all in a big hall. Output: SourceArt/Audio/Epic/*.wav (48 kHz 16-bit mono).
import numpy as np, wave, os
SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'Audio', 'Epic')
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(103)
def T(d): return np.arange(int(SR * d)) / SR
def filt(x, lo=None, hi=None, order=4):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); g = np.ones_like(f)
    if hi: g *= 1 / np.sqrt(1 + (f / hi) ** (2 * order))
    if lo: g *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** (2 * order))
    return np.fft.irfft(X * g, len(x))
def hall(x, secs=2.2, wet=0.35, hi=2500, seed=1):
    r = np.random.default_rng(seed); n = int(SR * secs); ir = r.normal(0, 1, n) * np.exp(-np.arange(n) / (SR * secs / 6.5))
    ir = filt(ir, lo=60, hi=hi); ir[:int(SR * 0.012)] = 0; ir /= np.sqrt(np.sum(ir ** 2))
    L = len(x) + n; X = np.fft.rfft(np.pad(x, (0, n)), L); Y = np.fft.irfft(X * np.fft.rfft(ir, L), L)
    return np.pad(x, (0, n)) * (1 - wet) + Y * wet * 3.0
def norm(x, peak=0.9): x = x - np.mean(x); m = np.max(np.abs(x)) or 1; return x / m * peak
def tail_fade(x, ms=30): n = int(SR * ms / 1000); x[-n:] *= np.linspace(1, 0, n); return x
def save(name, x):
    x = np.clip(x, -1, 1); d = (x * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
    print(name, '%.2f s' % (len(x) / SR))
MEM = [1.0, 1.594, 2.136, 2.296, 2.653, 2.918, 3.156]      # circular membrane mode ratios
def drum(f0=62.0, dur=1.6, hard=1.0, shell=170.0, seed=0):
    r = np.random.default_rng(seed); t = T(dur)
    bend = 1 + 0.22 * hard * np.exp(-t / 0.045)              # the skin's pitch drops right after the strike
    y = np.zeros_like(t)
    for k, m in enumerate(MEM):
        ph = np.cumsum(2 * np.pi * f0 * m * bend / SR) + r.uniform(0, 6.28)
        tau = 0.55 / (1 + 0.9 * k) * (1.4 if k == 0 else 1.0)
        y += (1.0 if k == 0 else 0.55 / (k + 0.4)) * np.sin(ph) * np.exp(-t / tau)
    y += 0.35 * np.sin(2 * np.pi * shell * t) * np.exp(-t / 0.09)          # wooden shell
    n = int(SR * 0.008); stick = np.zeros_like(t); stick[:n] = r.normal(0, 1, n) * np.hanning(n * 2)[n:]
    y += hard * 0.9 * filt(stick, lo=150, hi=4500)                          # the bachi thwack
    air = r.normal(0, 1, len(t)) * np.exp(-t / 0.05); y += 0.25 * hard * filt(air, lo=60, hi=900)
    return y
def ka(seed=0):
    r = np.random.default_rng(seed); t = T(0.35); y = np.zeros_like(t)
    for f, a, tau in ((930, 1, .045), (1710, .6, .03), (2650, .4, .02), (4100, .25, .012)):
        y += a * np.sin(2 * np.pi * f * r.uniform(.97, 1.03) * t) * np.exp(-t / tau)
    n = int(SR * .004); y[:n] += r.normal(0, 1.2, n)
    return y
# ---- cinematic impacts (Evan: "don't take the war drum literally" - the epic feel: trailer hits, sub drops, risers)
def impact(seed, big=False):
    r = np.random.default_rng(seed); dur = 4.0 if big else 3.0; t = T(dur)
    f = (95 if big else 110) * np.exp(-t / 0.35) + (30 if big else 38)                  # the sub drop
    sub = np.sin(np.cumsum(2 * np.pi * f / SR)) * np.exp(-t / (1.6 if big else 1.1))
    body = np.tanh(3.0 * filt(r.normal(0, 1, len(t)), lo=35, hi=320) * np.exp(-t / (0.5 if big else 0.32))) * 0.8
    n = int(SR * 0.014); tr = np.zeros_like(t); tr[:n] = r.normal(0, 1, n) * np.linspace(1, 0, n); tr = filt(tr, lo=1800) * 1.4
    metal = np.zeros_like(t)
    for fr, a, tau in ((412, .5, 1.1), (657, .35, .9), (1233, .25, .7), (1871, .18, .5), (2604, .1, .35)):
        metal += a * np.sin(2 * np.pi * fr * r.uniform(.98, 1.02) * t + r.uniform(0, 6.28)) * np.exp(-t / tau)
    y = 1.3 * sub + body + tr + 0.35 * metal
    return norm(tail_fade(hall(y, 3.6 if big else 2.8, 0.38, hi=2200, seed=seed + 5)))
for i in range(3): save('EpicImpact_%02d' % (i + 1), impact(200 + i))
for i in range(2): save('EpicBig_%02d' % (i + 1), impact(220 + i, True))
for i in range(2):
    r = np.random.default_rng(240 + i); dur = 2.0; t = T(dur); k = t / dur
    noise = r.normal(0, 1, len(t)); y = np.zeros_like(t)
    seg = int(SR * 0.05)
    for s0 in range(0, len(t), seg):                                                       # band sweeping up
        lo = 300 + 4000 * (s0 / len(t)) ** 2; y[s0:s0 + seg] = filt(noise[s0:s0 + seg] if len(noise[s0:s0 + seg]) > 8 else np.zeros(seg)[:len(noise[s0:s0 + seg])], lo=lo, hi=lo * 3)[:len(y[s0:s0 + seg])]
    tone = np.sin(np.cumsum(2 * np.pi * (180 + 700 * k ** 2) / SR)) * 0.35
    y = (y * 0.7 + tone) * (k ** 2.2)
    save('EpicRiser_%02d' % (i + 1), norm(tail_fade(hall(y, 1.2, 0.2, seed=250 + i), 8), 0.8))
# ---- rock + debris
def pebbles(dur, rate, seed, lo=900, hi=5200):
    r = np.random.default_rng(seed); t = T(dur); y = np.zeros_like(t)
    tt = 0.0
    while True:
        tt += r.exponential(1 / (rate * np.exp(-tt / (dur * 0.35)) + 1e-3))
        if tt >= dur - 0.06: break
        f = r.uniform(lo, hi); n = int(SR * r.uniform(.01, .05)); tk = np.arange(n) / SR
        ev = np.sin(2 * np.pi * f * tk) * np.exp(-tk / r.uniform(.004, .015)) + 0.6 * r.normal(0, 1, n) * np.exp(-tk / .003)
        s = int(tt * SR); y[s:s + n] += ev[:len(y) - s] * r.uniform(.2, 1.0) * np.exp(-tt / dur)
    return y
for i in range(4):
    t = T(2.2); crumble = filt(rng.normal(0, 1, len(t)), lo=70, hi=500) * np.exp(-t / 0.5)
    y = pebbles(2.2, 70, seed=80 + i) * 0.8 + crumble * 0.6
    save('Debris_%02d' % (i + 1), norm(tail_fade(hall(y, 1.8, 0.25, hi=4000, seed=90 + i)), 0.8))
for i in range(4):
    t = T(2.6); r = np.random.default_rng(120 + i)
    crack = np.zeros_like(t); n = int(SR * .012); crack[:n] = r.normal(0, 1, n) * np.linspace(1, 0, n)
    crack = filt(crack, lo=600) * 1.6
    for f, a, tau in ((2300, .5, .02), (3900, .35, .012), (5600, .2, .008)): crack += a * np.sin(2 * np.pi * f * t) * np.exp(-t / tau)
    thud = 1.1 * np.sin(2 * np.pi * (70 + 8 * i) * t * (1 + .3 * np.exp(-t / .03))) * np.exp(-t / .22) + .6 * filt(r.normal(0, 1, len(t)), hi=300) * np.exp(-t / .15)
    y = crack + thud + pebbles(2.6, 55, seed=130 + i) * .7
    save('RockSmash_%02d' % (i + 1), norm(tail_fade(hall(y, 2.2, 0.3, seed=140 + i))))
for i in range(2):
    t = T(3.5); r = np.random.default_rng(150 + i)
    y = filt(r.normal(0, 1, len(t)), lo=22, hi=95, order=6) * (1 - np.exp(-t / .03)) * np.exp(-t / 1.0) * 2.5 + 0.7 * np.sin(2 * np.pi * 34 * t) * np.exp(-t / .5)
    save('Rumble_%02d' % (i + 1), norm(tail_fade(y), 0.85))
print('done')
