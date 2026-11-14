# Nov 6 b163 (Claude): "Finish the sound scape" (Arena Life Plan: ambient soundscape per map + crowd reactions). Original synthesis,
# numpy only, project-owned (nothing bought or downloaded): per-map 30 s seamless stereo loops, a crowd bed, a final-round drumroll and
# one-shot accents / reactions. Output: SourceArt/Audio/Amb/*.wav (48 kHz 16-bit; loops stereo, one-shots mono).
import numpy as np, wave, os
SR = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'SourceArt', 'Audio', 'Amb'); os.makedirs(OUT, exist_ok=True)
SFX = os.path.join(HERE, '..', 'SourceArt', 'Audio', 'SFX')
rng = np.random.default_rng(1106)
def T(d): return np.arange(int(SR * d)) / SR
def filt(x, lo=None, hi=None, order=4):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); g = np.ones_like(f)
    if hi: g *= 1 / np.sqrt(1 + (f / hi) ** (2 * order))
    if lo: g *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** (2 * order))
    return np.fft.irfft(X * g, len(x))
def bp(x, fc, q=8.0):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); bw = fc / q
    return np.fft.irfft(X * np.exp(-0.5 * ((f - fc) / bw) ** 2), len(x))
def colored(n, alpha, r=None):   # alpha 0 white, 1 pink, 2 brown
    r = r or rng; X = np.fft.rfft(r.normal(0, 1, n)); f = np.fft.rfftfreq(n, 1 / SR); f[0] = f[1]
    y = np.fft.irfft(X / f ** (alpha / 2), n); return y / (np.std(y) + 1e-9)
def lfo(n, rates, r=None):
    r = r or rng; t = np.arange(n) / SR; y = np.zeros(n)
    for k in rates: y += np.sin(2 * np.pi * k * t + r.uniform(0, 6.28)) / len(rates)
    return y
def hall(x, secs=2.0, wet=0.3, hi=4000, seed=1):
    r = np.random.default_rng(seed); n = int(SR * secs); ir = r.normal(0, 1, n) * np.exp(-np.arange(n) / (SR * secs / 6.5))
    ir = filt(ir, lo=80, hi=hi); ir[:int(SR * 0.015)] = 0; ir /= np.sqrt(np.sum(ir ** 2))
    L = len(x) + n; Y = np.fft.irfft(np.fft.rfft(np.pad(x, (0, n)), L) * np.fft.rfft(ir, L), L)
    return np.pad(x, (0, n)) * (1 - wet) + Y * wet * 2.5
def norm(x, peak=0.9): x = x - np.mean(x, axis=-1, keepdims=True) if x.ndim > 1 else x - np.mean(x); m = np.max(np.abs(x)) or 1; return x / m * peak
def fade(x, a=0.005, b=0.03):
    na, nb = int(SR * a), int(SR * b); x = x.copy(); x[..., :na] *= np.linspace(0, 1, na); x[..., -nb:] *= np.linspace(1, 0, nb); return x
def save(name, x, rate=SR):
    x = np.clip(x, -1, 1)
    ch = 2 if x.ndim > 1 else 1
    d = (x.T if ch == 2 else x)
    d = (d * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w: w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(d.tobytes())
    print(name, '%.2f s' % (x.shape[-1] / rate), 'stereo' if ch == 2 else 'mono')
LOOP = 30.0; XF = 2.5
def looped(make):   # make(n) -> (2, n) for n = LOOP + XF seconds; returns a seamless (2, LOOP) loop
    n = int(SR * (LOOP + XF)); m = int(SR * LOOP); k = int(SR * XF); x = make(n)
    out = x[:, :m].copy(); w = np.linspace(0, 1, k); out[:, :k] = x[:, :k] * w + x[:, m:m + k] * (1 - w); return out
def place(bed, snd, at, gain=1.0, pan=0.0):   # pan -1..1 onto a stereo bed, wraps around the loop end
    n = bed.shape[1]; L = np.cos((pan + 1) * np.pi / 4); R = np.sin((pan + 1) * np.pi / 4)
    idx = (np.arange(len(snd)) + at) % n
    np.add.at(bed[0], idx, snd * gain * L); np.add.at(bed[1], idx, snd * gain * R)
# ---------------- building blocks ----------------
def wind(n, lo=180, hi=1100, gust=0.6, seed=0):
    r = np.random.default_rng(seed); out = np.zeros((2, n))
    for c in range(2):
        x = filt(colored(n, 1, r), lo=lo, hi=hi); env = 0.55 + gust * 0.45 * lfo(n, [0.05, 0.11, 0.023], r); out[c] = x * np.clip(env, 0.1, None)
    return out
def howl(n, seed=0):
    r = np.random.default_rng(seed); out = np.zeros((2, n))
    for c in range(2):
        base = colored(n, 1, r); y = np.zeros(n)
        for fc, q in ((340, 14), (520, 18), (760, 20), (1100, 16)):
            y += bp(base, fc * r.uniform(0.95, 1.05), q) * (0.5 + 0.5 * lfo(n, [0.04, 0.09, 0.17], r)) ** 2
        out[c] = y / (np.std(y) + 1e-9)
    return out
def chirp(r):
    d = r.uniform(0.05, 0.13); t = T(d); f0 = r.uniform(2400, 4200); f1 = f0 * r.uniform(1.15, 1.6) if r.random() < 0.6 else f0 * r.uniform(0.6, 0.85)
    f = f0 + (f1 - f0) * (t / d) + 300 * np.sin(2 * np.pi * r.uniform(25, 60) * t)
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.sin(np.pi * t / d) ** 2; return y
def bird_phrase(r):
    parts = []
    for i in range(r.integers(2, 6)): parts += [chirp(r), np.zeros(int(SR * r.uniform(0.03, 0.12)))]
    return np.concatenate(parts) * r.uniform(0.6, 1.0)
def bubble(r, big=False):
    d = r.uniform(0.04, 0.09) * (2.2 if big else 1); t = T(d); f0 = r.uniform(140, 380) * (0.5 if big else 1)
    f = f0 * (1 + 1.6 * t / d); y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (d * 0.35))
    if big: y += 0.6 * filt(r.normal(0, 1, len(t)), hi=300) * np.exp(-t / 0.05)
    return y
def bell(r, f):
    d = r.uniform(1.6, 2.8); t = T(d); y = np.zeros_like(t)
    for m, a, tau in ((1, 1, 1.0), (2.76, 0.5, 0.5), (5.40, 0.25, 0.25), (8.93, 0.12, 0.14)): y += a * np.sin(2 * np.pi * f * m * t) * np.exp(-t / (d * tau * 0.4))
    return y * np.minimum(1, t / 0.002)
def msum(xs):
    xs = list(xs); n = max(len(x) for x in xs); return sum(np.pad(x, (0, n - len(x))) for x in xs)
PENTA = [523.25, 587.33, 659.25, 783.99, 880.0, 1046.5, 1174.7]
def babble(n, voices=40, seed=0, lo=250, hi=2600):
    r = np.random.default_rng(seed); out = np.zeros((2, n))
    for v in range(voices):
        fc = r.uniform(lo, hi); src = bp(r.normal(0, 1, n), fc, r.uniform(3, 6))
        gate = np.clip(lfo(n, [r.uniform(2.5, 5.5), r.uniform(1, 2.2), r.uniform(0.2, 0.5)], r) * 1.6 + 0.2, 0, 1) ** 1.5
        pan = r.uniform(-0.9, 0.9); y = src * gate / (np.std(src) + 1e-9)
        out[0] += y * np.cos((pan + 1) * np.pi / 4); out[1] += y * np.sin((pan + 1) * np.pi / 4)
    return out / voices ** 0.5
def drip(r):
    d = r.uniform(0.04, 0.07); t = T(d); f0 = r.uniform(900, 1800); f = f0 * (1 + 1.4 * t / d)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.012) * np.minimum(1, t / 0.001)
def vowel_drone(n, f0, seed=0):
    r = np.random.default_rng(seed); t = np.arange(n) / SR
    vib = 1 + 0.004 * np.sin(2 * np.pi * 5.1 * t); ph = np.cumsum(f0 * vib) / SR
    saw = 2 * (ph % 1) - 1; saw = filt(saw, hi=3000)
    morph = 0.5 + 0.5 * lfo(n, [0.06, 0.03], r); y = np.zeros(n)
    for (fa, fb, a) in ((700, 400, 1.0), (1200, 850, 0.6), (2500, 2300, 0.25)):   # "ah" <-> "oh" formants
        y += a * (bp(saw, fa, 6) * morph + bp(saw, fb, 6) * (1 - morph))
    return y / (np.std(y) + 1e-9)
def car_pass(r, d=5.0):
    n = int(SR * d); t = np.arange(n) / SR; env = np.exp(-((t - d / 2) / (d / 5)) ** 2)
    rumble = filt(r.normal(0, 1, n), lo=60, hi=900) * env
    f = r.uniform(70, 110) * (1.08 - 0.16 * (t / d)); eng = filt(np.sign(np.sin(2 * np.pi * np.cumsum(f) / SR)), hi=1400) * env * 0.35
    y = rumble / (np.std(rumble) + 1e-9) + eng; pan = np.linspace(-0.9, 0.9, n) * (1 if r.random() < 0.5 else -1)
    return np.stack([y * np.cos((pan + 1) * np.pi / 4), y * np.sin((pan + 1) * np.pi / 4)])
def buzz(n, seed=0):
    r = np.random.default_rng(seed); t = np.arange(n) / SR
    y = sum(np.sin(2 * np.pi * 60 * k * t) / k for k in range(2, 16)); y = filt(y, lo=100, hi=2500)
    flick = 0.7 + 0.3 * (lfo(n, [7.3, 11.1, 0.4], r) > -0.6); return y * flick / (np.std(y) + 1e-9)
def crackle(r, d=0.4):
    n = int(SR * d); y = np.zeros(n)
    for i in range(r.integers(6, 18)):
        p = r.integers(0, n - 400); L = r.integers(40, 300); y[p:p + L] += r.normal(0, 1, L) * np.exp(-np.arange(L) / (L / 3))
    return filt(y, lo=800, hi=9000)
def creak(r):
    d = r.uniform(0.5, 1.0); n = int(SR * d); t = np.arange(n) / SR
    rate = r.uniform(25, 60) * (1 + 1.8 * np.sin(np.pi * t / d) ** 2); ph = np.cumsum(rate) / SR; clicks = np.zeros(n)
    idx = np.nonzero(np.diff(np.floor(ph)) > 0)[0]; clicks[idx] = 1
    y = filt(np.convolve(clicks, np.exp(-np.arange(200) / 30), 'same'), lo=180, hi=2200) * np.sin(np.pi * t / d)
    return y
def snare(r):
    d = 0.14; t = T(d); y = filt(r.normal(0, 1, len(t)), lo=1200, hi=8000) * np.exp(-t / 0.035) + 0.5 * np.sin(2 * np.pi * 190 * t) * np.exp(-t / 0.03)
    return y
# ---------------- per-map loops ----------------
def bed(n): return np.zeros((2, n))
def amb_meadow(n):
    r = np.random.default_rng(10); x = wind(n, seed=1) * 0.35
    for i in range(int(n / SR * 0.55)): place(x, bird_phrase(r), int(r.integers(0, n)), r.uniform(0.15, 0.35), r.uniform(-0.8, 0.8))
    x += filt(colored(n, 1, r), lo=2500, hi=7000)[None, :] * 0.03   # leaves / insects air
    return x
def amb_frozen(n):
    r = np.random.default_rng(11); x = howl(n, seed=2) * 0.32 + wind(n, 300, 2400, 0.8, seed=3) * 0.18
    for i in range(int(n / SR * 0.12)): place(x, hall(creak(r), 1.6, 0.4, seed=i), int(r.integers(0, n)), r.uniform(0.2, 0.4), r.uniform(-0.7, 0.7))
    return x
def amb_volcano(n):
    r = np.random.default_rng(12); t = np.arange(n) / SR
    rum = filt(colored(n, 2, r), hi=110) * 0.55 + 0.25 * np.sin(2 * np.pi * 38 * t) * (0.6 + 0.4 * lfo(n, [0.07, 0.13], r))
    x = np.stack([rum, filt(colored(n, 2, r), hi=110) * 0.55 + rum * 0.5])
    for i in range(int(n / SR * 3.5)): place(x, bubble(r, r.random() < 0.15), int(r.integers(0, n)), r.uniform(0.08, 0.22), r.uniform(-0.6, 0.6))
    x += wind(n, 150, 600, 0.4, seed=4) * 0.08
    return x
def amb_cosmos(n):
    r = np.random.default_rng(13); t = np.arange(n) / SR; y = np.zeros((2, n))
    for c in range(2):
        for f, a in ((55, 0.5), (82.6, 0.35), (110.3, 0.25), (164.9, 0.12)): y[c] += a * np.sin(2 * np.pi * f * (1 + c * 0.002) * t + r.uniform(0, 6)) * (0.7 + 0.3 * lfo(n, [0.03, 0.08], r))
        y[c] += filt(colored(n, 0, r), lo=2500, hi=7000) * 0.05 * (0.5 + 0.5 * lfo(n, [0.05], r))
    for i in range(int(n / SR * 0.2)):
        d = r.uniform(0.3, 0.7); tt = T(d); s = np.sin(2 * np.pi * np.cumsum(r.uniform(600, 1200) * (1 + tt / d)) / SR) * np.sin(np.pi * tt / d)
        place(y, hall(s, 2.5, 0.6, seed=30 + i), int(r.integers(0, n)), 0.06, r.uniform(-0.8, 0.8))
    return y
def amb_arabian(n):
    r = np.random.default_rng(14); x = babble(n, 36, seed=5) * 0.22 + wind(n, 200, 1600, 0.5, seed=6) * 0.16
    for i in range(int(n / SR * 0.5)):
        s = msum(bell(r, PENTA[r.integers(0, len(PENTA))] * r.choice([1, 2])) * r.uniform(0.4, 1) for _ in range(r.integers(1, 4)))
        place(x, s, int(r.integers(0, n)), 0.07, r.uniform(-0.7, 0.7))
    return x
def amb_tomb(n):
    r = np.random.default_rng(15); d1 = vowel_drone(n, 98.0, seed=7); d2 = vowel_drone(n, 146.8, seed=8); d3 = vowel_drone(n, 73.4, seed=9)
    ch = (d1 * 0.5 + d2 * 0.3 + d3 * 0.35) * (0.6 + 0.4 * lfo(n, [0.04, 0.07], r))
    chant = hall(filt(ch, hi=1600), 3.5, 0.7, hi=2000, seed=40)[:n] * 0.16
    x = np.stack([chant, np.roll(chant, int(SR * 0.013))]) + filt(colored(n, 2, r), hi=200)[None, :] * 0.05
    for i in range(int(n / SR * 1.1)): place(x, hall(drip(r), 2.2, 0.55, seed=50 + i), int(r.integers(0, n)), r.uniform(0.12, 0.3), r.uniform(-0.8, 0.8))
    return x
def amb_neon(n):
    r = np.random.default_rng(16); x = np.stack([buzz(n, 1), buzz(n, 2)]) * 0.05 + filt(colored(n, 1, r), lo=50, hi=500)[None, :] * 0.12
    k = 0
    while k < n:
        c = car_pass(r, r.uniform(3.5, 6.0)) * r.uniform(0.18, 0.4); m = min(c.shape[1], n - k); x[:, k:k + m] += c[:, :m]; k += int(SR * r.uniform(1.2, 4.0))
    for i in range(int(n / SR * 0.25)): place(x, crackle(r), int(r.integers(0, n)), 0.05, r.uniform(-0.6, 0.6))
    return x
def amb_ancient(n):
    r = np.random.default_rng(17); x = wind(n, seed=11) * 0.28 + amb_volcano(n) * 0.45
    for i in range(int(n / SR * 0.35)): place(x, bird_phrase(r), int(r.integers(0, n)), r.uniform(0.1, 0.25), r.uniform(-0.8, 0.8))
    x += filt(colored(n, 1, r), lo=300, hi=1800)[None, :] * 0.02   # distant waterfalls
    return x
def amb_court(n):   # League Training Court (Training): morning meadow, light breeze, a few early fans
    r = np.random.default_rng(18); x = wind(n, seed=12) * 0.22 + babble(n, 10, seed=13, lo=300, hi=1800) * 0.07
    for i in range(int(n / SR * 0.4)): place(x, bird_phrase(r), int(r.integers(0, n)), r.uniform(0.12, 0.28), r.uniform(-0.8, 0.8))
    return x
def crowd_bed(n):
    r = np.random.default_rng(19); x = babble(n, 64, seed=21, lo=220, hi=3000) * (0.75 + 0.25 * lfo(n, [0.05, 0.13], r))[None, :]
    return np.stack([hall(x[0], 1.8, 0.35, seed=60)[:n], hall(x[1], 1.8, 0.35, seed=61)[:n]])
def drumroll():
    r = np.random.default_rng(20); n = int(SR * 4.0); y = np.zeros(n); t = 0.0
    while t < 4.0 - 0.15:
        s = snare(r) * r.uniform(0.7, 1.0); p = int(t * SR); y[p:p + len(s)] += s[:n - p]; t += 1 / 17.0 * r.uniform(0.93, 1.07)
    y = hall(y, 1.2, 0.25, seed=70)[:n]; k = int(SR * 0.3); y[:k] = y[:k] * np.linspace(0, 1, k) + y[-k:] * np.linspace(1, 0, k)
    return np.stack([y, np.roll(y, 37)])
LOOPS = [('AmbMeadow_Loop', amb_meadow, 0.7), ('AmbFrozen_Loop', amb_frozen, 0.7), ('AmbVolcano_Loop', amb_volcano, 0.75), ('AmbCosmos_Loop', amb_cosmos, 0.7),
         ('AmbArabian_Loop', amb_arabian, 0.7), ('AmbTomb_Loop', amb_tomb, 0.7), ('AmbNeon_Loop', amb_neon, 0.7), ('AmbAncient_Loop', amb_ancient, 0.72),
         ('AmbCourt_Loop', amb_court, 0.7), ('AmbCrowd_Loop', crowd_bed, 0.8)]
for name, fn, pk in LOOPS: save(name, norm(looped(fn), pk))
save('AmbDrumroll_Loop', norm(drumroll(), 0.85))
# ---------------- one-shots ----------------
r = np.random.default_rng(99)
for i in range(4): save('AmbBird_%02d' % (i + 1), norm(fade(bird_phrase(r)), 0.7))
for i in range(3): save('AmbIceCreak_%02d' % (i + 1), norm(fade(hall(creak(r), 1.6, 0.4, seed=i)), 0.8))
for i in range(3): save('AmbLavaPop_%02d' % (i + 1), norm(fade(hall(np.concatenate([bubble(r, True), bubble(r)]), 1.0, 0.2, seed=10 + i)), 0.8))
for i in range(3): save('AmbChime_%02d' % (i + 1), norm(fade(msum(bell(r, PENTA[k]) * 0.5 for k in r.choice(len(PENTA), 3, replace=False))), 0.7))
for i in range(3): save('AmbDrip_%02d' % (i + 1), norm(fade(hall(drip(r), 2.2, 0.55, seed=20 + i)), 0.7))
for i in range(2): save('AmbCarPass_%02d' % (i + 1), norm(fade(car_pass(r, 5.0).mean(axis=0)), 0.8))
for i in range(2): save('AmbZap_%02d' % (i + 1), norm(fade(crackle(r, 0.5)), 0.8))
for i in range(2):
    d = 0.6; tt = T(d); s = np.sin(2 * np.pi * np.cumsum(r.uniform(700, 1100) * (1 + 0.8 * tt / d)) / SR) * np.sin(np.pi * tt / d)
    save('AmbSpaceBlip_%02d' % (i + 1), norm(fade(hall(s, 2.5, 0.6, seed=30 + i)), 0.7))
def read_wav(p):
    with wave.open(p) as w:
        sr = w.getframerate(); ch = w.getnchannels(); x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
    if ch > 1: x = x.reshape(-1, ch).mean(axis=1)
    if sr != SR: x = np.interp(np.arange(0, len(x), sr / SR), np.arange(len(x)), x)
    return x
# crowd "awww" on a miss: the project's CrowdWave1 bent down in pitch + a falling vowel choir
try: cw = read_wav(os.path.join(SFX, 'CrowdWave1.wav'))
except Exception as e: cw = None; print('CrowdWave1 missing', e)
for i in range(2):
    d = 1.6; n = int(SR * d); t = np.arange(n) / SR
    choir = np.zeros(n)
    for v in range(14):
        f0 = r.uniform(170, 330) * (1.0 - 0.28 * (t / d) ** 1.3); ph = np.cumsum(f0 * (1 + 0.01 * np.sin(2 * np.pi * r.uniform(4, 6) * t))) / SR
        saw = 2 * (ph % 1) - 1; choir += (bp(saw, 750, 5) + 0.6 * bp(saw, 1150, 6) + 0.3 * bp(saw, 2500, 7)) * r.uniform(0.6, 1)
    choir = choir / (np.std(choir) + 1e-9) * np.sin(np.pi * np.minimum(1, t / 0.25) / 2) * np.exp(-np.maximum(0, t - 0.8) / 0.5)
    y = choir * 0.6
    if cw is not None:
        m = min(len(cw), int(SR * 1.4)); rate = 0.82 - 0.2 * np.linspace(0, 1, m) - 0.05 * i; pos = np.cumsum(rate); pos = pos[pos < len(cw) - 1][:n]
        g = np.interp(pos, np.arange(len(cw)), cw); g = filt(np.pad(g, (0, n - len(g))), hi=2600); y += g / (np.std(g) + 1e-9) * 0.5 * np.exp(-t / 1.0)
    save('CrowdGroan_%02d' % (i + 1), norm(fade(hall(y, 1.6, 0.35, seed=80 + i)), 0.8))
# the fall: a whoosh down past the edge, then the blast far below the arena with a long echo
for i in range(2):
    d = 1.0; n = int(SR * d); t = np.arange(n) / SR; base = r.normal(0, 1, n); y = np.zeros(n)
    for j, fc in enumerate(np.geomspace(2400, 280, 24)):
        seg = slice(int(j * n / 24), int((j + 1) * n / 24) + 1); y[seg] += bp(base, fc, 3)[seg]
    y = y / (np.std(y) + 1e-9) * np.sin(np.pi * t / d) ** 0.7
    save('FallWhoosh_%02d' % (i + 1), norm(fade(y), 0.8))
for i in range(2):
    d = 1.2; t = T(d); boom = np.sin(2 * np.pi * np.cumsum(55 - 25 * t / d) / SR) * np.exp(-t / 0.35) + 0.8 * filt(r.normal(0, 1, len(t)), hi=500) * np.exp(-t / 0.18)
    y = hall(boom, 3.5, 0.65, hi=1200, seed=90 + i); dly = int(SR * 0.42); echo = np.zeros(len(y) + 2 * dly); echo[:len(y)] += y; echo[dly:dly + len(y)] += 0.45 * y; echo[2 * dly:2 * dly + len(y)] += 0.2 * y
    save('UnderBlast_%02d' % (i + 1), norm(fade(filt(echo, hi=1500)), 0.8))
print('DONE')
