/**
 * Short Web Audio one-shots for the star-coin powers.
 * One AudioContext is created on the first user gesture (or the first play
 * call during a gesture) and reused. Playback follows useAudio's mute flag
 * via bindPowerAudioMute; until that is bound, sounds stay silent.
 */

type MuteGetter = () => boolean;

let isPowerMuted: MuteGetter = () => true;
let context: AudioContext | null = null;
let gestureInstalled = false;
let noiseCache: AudioBuffer | null = null;

export function bindPowerAudioMute(getter: MuteGetter): void {
  isPowerMuted = getter;
}

function audioCtor(): typeof AudioContext | null {
  if (typeof window === "undefined") return null;
  const w = window as unknown as {
    AudioContext?: typeof AudioContext;
    webkitAudioContext?: typeof AudioContext;
  };
  return w.AudioContext || w.webkitAudioContext || null;
}

function getOrCreateContext(): AudioContext | null {
  if (context) return context;
  const Ctor = audioCtor();
  if (!Ctor) return null;
  try {
    context = new Ctor();
  } catch {
    return null;
  }
  return context;
}

export function installPowerAudioGestureResume(): void {
  if (gestureInstalled || typeof document === "undefined") return;
  gestureInstalled = true;

  const remove = () => {
    document.removeEventListener("pointerdown", onGesture);
    document.removeEventListener("keydown", onGesture);
    document.removeEventListener("touchend", onGesture);
  };

  const onGesture = () => {
    const ctx = getOrCreateContext();
    if (!ctx) return;
    if (ctx.state === "running") {
      remove();
      return;
    }
    if (ctx.state === "suspended") {
      void ctx.resume().then(() => {
        if (ctx.state === "running") remove();
      }).catch(() => {});
    }
  };

  document.addEventListener("pointerdown", onGesture, { passive: true });
  document.addEventListener("keydown", onGesture);
  document.addEventListener("touchend", onGesture, { passive: true });
}

function noiseBuffer(ctx: AudioContext, seconds: number): AudioBuffer {
  if (noiseCache && noiseCache.sampleRate === ctx.sampleRate && noiseCache.duration >= seconds - 0.001) {
    return noiseCache;
  }
  const length = Math.max(1, Math.floor(ctx.sampleRate * Math.max(seconds, 0.5)));
  const buffer = ctx.createBuffer(1, length, ctx.sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < length; i++) data[i] = Math.random() * 2 - 1;
  noiseCache = buffer;
  return buffer;
}

async function startSound(build: (ctx: AudioContext, when: number) => void): Promise<void> {
  if (isPowerMuted()) return;
  const ctx = getOrCreateContext();
  if (!ctx) return;
  if (ctx.state === "suspended") {
    try {
      await ctx.resume();
    } catch {
      return;
    }
  }
  if (isPowerMuted() || ctx.state !== "running") return;
  try {
    build(ctx, ctx.currentTime);
  } catch (err) {
    console.log("Power sound synthesis error:", err);
  }
}

function master(ctx: AudioContext, level: number): GainNode {
  const gain = ctx.createGain();
  gain.gain.value = level;
  gain.connect(ctx.destination);
  return gain;
}

function tone(
  ctx: AudioContext,
  dest: AudioNode,
  type: OscillatorType,
  freqAt: (osc: OscillatorNode, t: number) => void,
  gainAt: (gain: GainNode, t: number) => void,
  start: number,
  stop: number
): void {
  const osc = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.type = type;
  osc.connect(gain);
  gain.connect(dest);
  freqAt(osc, start);
  gainAt(gain, start);
  osc.start(start);
  osc.stop(stop);
}

/** Bright rising chime when a star coin unlocks a power. */
export function playPowerUnlock(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.42);
    const notes = [784, 1046, 1318, 1568];
    notes.forEach((freq, i) => {
      const start = t + i * 0.055;
      tone(
        ctx,
        out,
        "sine",
        (osc, when) => {
          osc.frequency.setValueAtTime(freq, when);
        },
        (gain, when) => {
          gain.gain.setValueAtTime(0.0001, when);
          gain.gain.exponentialRampToValueAtTime(i === notes.length - 1 ? 0.28 : 0.2, when + 0.02);
          gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.2);
        },
        start,
        start + 0.22
      );
    });
  });
}

/** Whoosh plus a short growl / howl for Wolfgang's clone dash. */
export function playWolfDash(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.5);
    const noise = ctx.createBufferSource();
    noise.buffer = noiseBuffer(ctx, 0.45);
    const filter = ctx.createBiquadFilter();
    filter.type = "bandpass";
    filter.Q.value = 0.7;
    filter.frequency.setValueAtTime(280, t);
    filter.frequency.exponentialRampToValueAtTime(1600, t + 0.18);
    filter.frequency.exponentialRampToValueAtTime(500, t + 0.36);
    const noiseGain = ctx.createGain();
    noiseGain.gain.setValueAtTime(0.0001, t);
    noiseGain.gain.exponentialRampToValueAtTime(0.45, t + 0.04);
    noiseGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.34);
    noise.connect(filter);
    filter.connect(noiseGain);
    noiseGain.connect(out);
    noise.start(t);
    noise.stop(t + 0.36);

    tone(
      ctx,
      out,
      "sawtooth",
      (osc, when) => {
        osc.frequency.setValueAtTime(140, when);
        osc.frequency.exponentialRampToValueAtTime(70, when + 0.32);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.16, when + 0.05);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.34);
      },
      t,
      t + 0.36
    );

    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(260, when);
        osc.frequency.linearRampToValueAtTime(440, when + 0.16);
        osc.frequency.linearRampToValueAtTime(220, when + 0.38);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.12, when + 0.14);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.4);
      },
      t,
      t + 0.42
    );

    const air = ctx.createBufferSource();
    air.buffer = noiseBuffer(ctx, 0.9);
    const airFilter = ctx.createBiquadFilter();
    airFilter.type = "lowpass";
    airFilter.frequency.setValueAtTime(880, t + 0.12);
    airFilter.frequency.exponentialRampToValueAtTime(220, t + 0.85);
    const airGain = ctx.createGain();
    airGain.gain.setValueAtTime(0.0001, t + 0.1);
    airGain.gain.exponentialRampToValueAtTime(0.05, t + 0.24);
    airGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.9);
    air.connect(airFilter);
    airFilter.connect(airGain);
    airGain.connect(out);
    air.start(t + 0.1);
    air.stop(t + 0.95);
  });
}

/** Low boom, noise burst, fast decay for Hotstreak's blast. */
export function playExplosion(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.55);
    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(110, when);
        osc.frequency.exponentialRampToValueAtTime(32, when + 0.38);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.7, when + 0.015);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.4);
      },
      t,
      t + 0.42
    );
    tone(
      ctx,
      out,
      "triangle",
      (osc, when) => {
        osc.frequency.setValueAtTime(180, when);
        osc.frequency.exponentialRampToValueAtTime(50, when + 0.16);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.35, when);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.16);
      },
      t,
      t + 0.18
    );

    const noise = ctx.createBufferSource();
    noise.buffer = noiseBuffer(ctx, 0.4);
    const filter = ctx.createBiquadFilter();
    filter.type = "lowpass";
    filter.frequency.setValueAtTime(1800, t);
    filter.frequency.exponentialRampToValueAtTime(180, t + 0.22);
    const noiseGain = ctx.createGain();
    noiseGain.gain.setValueAtTime(0.5, t);
    noiseGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.24);
    noise.connect(filter);
    filter.connect(noiseGain);
    noiseGain.connect(out);
    noise.start(t);
    noise.stop(t + 0.26);

    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(46, when);
        osc.frequency.exponentialRampToValueAtTime(28, when + 0.9);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.07, when + 0.12);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 1.05);
      },
      t + 0.08,
      t + 1.15
    );
  });
}

/** Electric crackle for Bolt's stun burst. */
export function playStunZap(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.4);
    const cracks = [
      { at: 0, freq: 1800, dur: 0.045 },
      { at: 0.03, freq: 3400, dur: 0.03 },
      { at: 0.055, freq: 2200, dur: 0.04 },
      { at: 0.09, freq: 4100, dur: 0.028 },
      { at: 0.13, freq: 1600, dur: 0.05 },
      { at: 0.18, freq: 2800, dur: 0.04 },
    ];
    for (const crack of cracks) {
      const start = t + crack.at;
      const noise = ctx.createBufferSource();
      noise.buffer = noiseBuffer(ctx, 0.4);
      const filter = ctx.createBiquadFilter();
      filter.type = "bandpass";
      filter.frequency.value = crack.freq;
      filter.Q.value = 4;
      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0.0001, start);
      gain.gain.exponentialRampToValueAtTime(0.55, start + 0.006);
      gain.gain.exponentialRampToValueAtTime(0.0001, start + crack.dur);
      noise.connect(filter);
      filter.connect(gain);
      gain.connect(out);
      noise.start(start);
      noise.stop(start + crack.dur + 0.01);
    }
    tone(
      ctx,
      out,
      "square",
      (osc, when) => {
        osc.frequency.setValueAtTime(520, when);
        osc.frequency.exponentialRampToValueAtTime(90, when + 0.2);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.08, when + 0.01);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.22);
      },
      t,
      t + 0.24
    );
  });
}

/** Metallic ping when Lars arms Ricochet, and a brighter double ping on the bounce. */
export function playRicochetPing(kind: "arm" | "hit" = "arm"): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.38);
    const base = kind === "hit" ? 1320 : 880;
    const strikes = kind === "hit" ? [0, 0.07] : [0];
    strikes.forEach((offset, index) => {
      const start = t + offset;
      const freq = base * (index === 0 ? 1 : 0.84);
      tone(
        ctx,
        out,
        "sine",
        (osc, when) => {
          osc.frequency.setValueAtTime(freq, when);
          osc.frequency.exponentialRampToValueAtTime(freq * 0.72, when + 0.09);
        },
        (gain, when) => {
          gain.gain.setValueAtTime(0.0001, when);
          gain.gain.exponentialRampToValueAtTime(0.32, when + 0.008);
          gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.11);
        },
        start,
        start + 0.12
      );
      tone(
        ctx,
        out,
        "triangle",
        (osc, when) => {
          osc.frequency.setValueAtTime(freq * 2.63, when);
        },
        (gain, when) => {
          gain.gain.setValueAtTime(0.0001, when);
          gain.gain.exponentialRampToValueAtTime(0.14, when + 0.006);
          gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.07);
        },
        start,
        start + 0.08
      );
    });
  });
}

/** Low pulse when Nightshade freezes nearby marbles. */
export function playShadowPulse(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.4);
    const noise = ctx.createBufferSource();
    noise.buffer = noiseBuffer(ctx, 0.45);
    const filter = ctx.createBiquadFilter();
    filter.type = "lowpass";
    filter.frequency.setValueAtTime(900, t);
    filter.frequency.exponentialRampToValueAtTime(120, t + 0.35);
    const noiseGain = ctx.createGain();
    noiseGain.gain.setValueAtTime(0.0001, t);
    noiseGain.gain.exponentialRampToValueAtTime(0.5, t + 0.04);
    noiseGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.4);
    noise.connect(filter);
    filter.connect(noiseGain);
    noiseGain.connect(out);
    noise.start(t);
    noise.stop(t + 0.42);

    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(196, when);
        osc.frequency.exponentialRampToValueAtTime(55, when + 0.38);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.22, when + 0.05);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.42);
      },
      t,
      t + 0.44
    );
    tone(
      ctx,
      out,
      "triangle",
      (osc, when) => {
        osc.frequency.setValueAtTime(740, when);
        osc.frequency.exponentialRampToValueAtTime(220, when + 0.16);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.08, when + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.18);
      },
      t + 0.02,
      t + 0.22
    );
  });
}

/** Short cloth whoosh when Wraps binds nearby marbles. */
export function playBindWrap(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.42);
    const noise = ctx.createBufferSource();
    noise.buffer = noiseBuffer(ctx, 0.4);
    const filter = ctx.createBiquadFilter();
    filter.type = "bandpass";
    filter.Q.value = 0.85;
    filter.frequency.setValueAtTime(1400, t);
    filter.frequency.exponentialRampToValueAtTime(280, t + 0.28);
    const noiseGain = ctx.createGain();
    noiseGain.gain.setValueAtTime(0.0001, t);
    noiseGain.gain.exponentialRampToValueAtTime(0.55, t + 0.03);
    noiseGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.32);
    noise.connect(filter);
    filter.connect(noiseGain);
    noiseGain.connect(out);
    noise.start(t);
    noise.stop(t + 0.34);

    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(220, when);
        osc.frequency.exponentialRampToValueAtTime(90, when + 0.26);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.12, when + 0.04);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.28);
      },
      t,
      t + 0.3
    );
  });
}

/** Small shake-off blip when a stun timer finishes. */
export function playStunEnd(): void {
  void startSound((ctx, t) => {
    const out = master(ctx, 0.35);
    tone(
      ctx,
      out,
      "sine",
      (osc, when) => {
        osc.frequency.setValueAtTime(620, when);
        osc.frequency.exponentialRampToValueAtTime(340, when + 0.09);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.0001, when);
        gain.gain.exponentialRampToValueAtTime(0.22, when + 0.012);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.1);
      },
      t,
      t + 0.12
    );
    tone(
      ctx,
      out,
      "triangle",
      (osc, when) => {
        osc.frequency.setValueAtTime(980, when);
      },
      (gain, when) => {
        gain.gain.setValueAtTime(0.12, when);
        gain.gain.exponentialRampToValueAtTime(0.0001, when + 0.05);
      },
      t + 0.04,
      t + 0.1
    );
  });
}
