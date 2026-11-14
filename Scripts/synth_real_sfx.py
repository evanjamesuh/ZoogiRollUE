# Nov 1 b59 (Claude): physically modelled "realistic" impact layers (modal synthesis, numpy only) for Evan's
# "add the most realistic sounds for everything". Heavy ball-on-ball clacks, glass orb clinks, wall knocks, a landing thud and a
# rolling rumble loop. Original synthesis, project-owned. Output: SourceArt/Audio/Real/*.wav (48 kHz 16-bit mono).
import numpy as np, wave, os, sys
SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'Audio', 'Real')
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(58)

def t_(dur): return np.arange(int(SR * dur)) / SR
def fft_filter(x, lo=None, hi=None, order=4):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); g = np.ones_like(f)
    if hi: g *= 1 / np.sqrt(1 + (f / hi) ** (2 * order))
    if lo: g *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** (2 * order))
    return np.fft.irfft(X * g, len(x))
def room(x, taps=((0.0071, 0.32), (0.0127, 0.22), (0.0213, 0.14), (0.0337, 0.08))):
    y = x.copy()
    for d, a in taps:
        n = int(d * SR); z = np.zeros_like(x); z[n:] = x[:-n] * a; y += fft_filter(z, hi=3500)
    return y
def norm(x, peak=0.85):
    x = x - np.mean(x); m = np.max(np.abs(x)) or 1; return x / m * peak
def fade(x, ms=4):
    n = int(SR * ms / 1000); x[-n:] *= np.linspace(1, 0, n); return x
def save(name, x):
    x = np.clip(x, -1, 1); d = (x * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
    print(name, '%.3f s' % (len(x) / SR))
def click(dur_ms, t):
    n = int(SR * dur_ms / 1000); p = np.zeros_like(t); win = 0.5 - 0.5 * np.cos(np.linspace(0, 2 * np.pi, n))
    p[:n] = win * rng.normal(0, 1, n); return p
def modes(t, freqs, amps, taus, start=0.0):
    y = np.zeros_like(t)
    for f, a, tau in zip(freqs, amps, taus):
        ph = rng.uniform(0, 2 * np.pi); y += a * np.sin(2 * np.pi * f * t + ph) * np.exp(-t / tau)
    return y

# 1) heavy ball-on-ball clack (resin / steel-cored mech balls ~1 m): bright click + sphere modes + heft thump
for i in range(6):
    t = t_(0.42); f0 = rng.uniform(620, 980)
    ratios = np.array([1, 1.52, 2.03, 2.46, 2.93, 3.61, 4.4]) * rng.uniform(0.97, 1.03, 7)
    y = modes(t, f0 * ratios, [1, .7, .5, .38, .26, .17, .1], [.075, .055, .042, .033, .025, .018, .013])
    y += 2.2 * fft_filter(click(1.1, t), lo=1200)
    y += 0.9 * np.sin(2 * np.pi * rng.uniform(85, 130) * t) * np.exp(-t / 0.028)
    save('BallClack_%02d' % (i + 1), fade(norm(room(y))))
# 2) glass orb clink: sharp tick + inharmonic glass partials with a longer ring
for i in range(6):
    t = t_(0.7); f0 = rng.uniform(2100, 3100)
    ratios = np.array([1, 2.32, 4.25, 6.63]) * rng.uniform(0.98, 1.02, 4)
    y = modes(t, f0 * ratios, [1, .55, .3, .15], [.22, .12, .07, .04])
    y += 1.6 * fft_filter(click(0.6, t), lo=3000)
    y += 0.25 * np.sin(2 * np.pi * rng.uniform(380, 520) * t) * np.exp(-t / 0.02)
    save('OrbClink_%02d' % (i + 1), fade(norm(room(y), 0.7)))
# 3) ball into a stone / wood wall: dull knock, low thud, gritty noise
for i in range(5):
    t = t_(0.45)
    y = 1.2 * np.sin(2 * np.pi * rng.uniform(70, 105) * t) * np.exp(-t / 0.05)
    y += modes(t, rng.uniform(260, 420) * np.array([1, 1.7, 2.6]), [.6, .35, .2], [.04, .028, .02])
    n = rng.normal(0, 1, len(t)) * np.exp(-t / 0.03); y += 0.9 * fft_filter(n, hi=1400)
    y += 0.8 * fft_filter(click(1.8, t), hi=5000)
    save('WallKnock_%02d' % (i + 1), fade(norm(room(y))))
# 4) landing thud (after a hop / arc)
for i in range(3):
    t = t_(0.6)
    y = 1.4 * np.sin(2 * np.pi * (rng.uniform(55, 75) * (1 + 0.6 * np.exp(-t / 0.02))) * t) * np.exp(-t / 0.09)
    n = rng.normal(0, 1, len(t)) * np.exp(-t / 0.06); y += 0.7 * fft_filter(n, hi=900)
    y += modes(t, rng.uniform(700, 900) * np.array([1, 1.52, 2.03]), [.25, .15, .1], [.05, .035, .025])
    save('Land_%02d' % (i + 1), fade(norm(room(y))))
# 5) rolling rumble loop (4 s, seamless): low rumble + faint surface grit ticks
L = 4.0; t = t_(L + 0.5)
br = np.cumsum(rng.normal(0, 1, len(t))); br -= np.convolve(br, np.ones(4801) / 4801, 'same')
y = fft_filter(br, lo=35, hi=240) * 1.0
grit = np.zeros_like(t); idx = rng.integers(0, len(t), int(40 * (L + 0.5)))
grit[idx] = rng.uniform(0.2, 1, len(idx)); y += 0.08 * fft_filter(np.convolve(grit, np.exp(-np.arange(200) / 30), 'same'), lo=900, hi=4000) * np.max(np.abs(y))
n = int(0.5 * SR); body = y[:int(L * SR)].copy(); xf = np.linspace(0, 1, n)
body[:n] = body[:n] * xf + y[int(L * SR):int(L * SR) + n] * (1 - xf)
save('RollLoop_Real', norm(body, 0.6))
