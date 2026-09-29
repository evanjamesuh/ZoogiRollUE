import { create } from "zustand";
import {
  bindPowerAudioMute,
  installPowerAudioGestureResume,
  playBindWrap as playBindWrapSound,
  playShadowPulse as playShadowPulseSound,
  playExplosion as playExplosionSound,
  playPowerUnlock as playPowerUnlockSound,
  playRicochetPing as playRicochetPingSound,
  playStunEnd as playStunEndSound,
  playStunZap as playStunZapSound,
  playWolfDash as playWolfDashSound,
} from "../powerSounds";

type IntensityLevel = "calm" | "active" | "intense";

type SoundType = 
  | "collision_enemy" 
  | "collision_mushroom" 
  | "collision_tree" 
  | "collision_snowman"
  | "fall_player"
  | "fall_enemy" 
  | "fall_orb"
  | "pull_back"
  | "launch"
  | "orb_collect"
  | "level_up";

interface AudioState {
  backgroundMusic: HTMLAudioElement | null;
  menuMusic: HTMLAudioElement | null;
  hitSound: HTMLAudioElement | null;
  successSound: HTMLAudioElement | null;
  isMuted: boolean;
  
  intensity: IntensityLevel;
  targetVolume: number;
  currentVolume: number;
  audioContext: AudioContext | null;
  gainNode: GainNode | null;
  
  setBackgroundMusic: (music: HTMLAudioElement) => void;
  setMenuMusic: (music: HTMLAudioElement) => void;
  setHitSound: (sound: HTMLAudioElement) => void;
  setSuccessSound: (sound: HTMLAudioElement) => void;
  
  toggleMute: () => void;
  playMenuMusic: () => void;
  stopMenuMusic: () => void;
  playHit: (playbackRate?: number, volume?: number) => void;
  playSuccess: () => void;
  playSound: (type: SoundType, intensity?: number) => void;
  playPowerUnlock: () => void;
  playWolfDash: () => void;
  playExplosion: () => void;
  playStunZap: () => void;
  playStunEnd: () => void;
  playRicochetPing: (kind?: "arm" | "hit") => void;
  playBindWrap: () => void;
  playShadowPulse: () => void;
  
  setIntensity: (level: IntensityLevel) => void;
  updateIntensityFromGameState: (playerSpeed: number, recentCollisions: number, nearbyEnemies: number) => void;
  initAudioContext: () => void;
}

const INTENSITY_CONFIG = {
  calm: { volume: 0.15, playbackRate: 0.95 },
  active: { volume: 0.25, playbackRate: 1.0 },
  intense: { volume: 0.35, playbackRate: 1.08 }
};

export const useAudio = create<AudioState>((set, get) => ({
  backgroundMusic: null,
  menuMusic: null,
  hitSound: null,
  successSound: null,
  isMuted: true,
  
  intensity: "calm",
  targetVolume: 0.15,
  currentVolume: 0.15,
  audioContext: null,
  gainNode: null,
  
  setBackgroundMusic: (music) => set({ backgroundMusic: music }),
  setMenuMusic: (music) => set({ menuMusic: music }),
  setHitSound: (sound) => set({ hitSound: sound }),
  setSuccessSound: (sound) => set({ successSound: sound }),
  
  playMenuMusic: () => {
    const { menuMusic, isMuted } = get();
    if (menuMusic && !isMuted) {
      menuMusic.loop = true;
      menuMusic.volume = 0.3;
      menuMusic.play().catch(err => console.log("Menu music play prevented:", err));
    }
  },
  
  stopMenuMusic: () => {
    const { menuMusic } = get();
    if (menuMusic) {
      menuMusic.pause();
      menuMusic.currentTime = 0;
    }
  },
  
  initAudioContext: () => {
    const { audioContext } = get();
    if (audioContext) return;
    
    try {
      const ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
      const gain = ctx.createGain();
      gain.connect(ctx.destination);
      gain.gain.value = 0.15;
      set({ audioContext: ctx, gainNode: gain });
    } catch (e) {
      console.log("Web Audio API not supported");
    }
  },
  
  toggleMute: () => {
    const { isMuted, backgroundMusic, menuMusic } = get();
    const newMutedState = !isMuted;
    
    set({ isMuted: newMutedState });
    
    if (backgroundMusic) {
      if (newMutedState) {
        backgroundMusic.pause();
      } else {
        const { intensity } = get();
        backgroundMusic.volume = INTENSITY_CONFIG[intensity].volume;
        requestAnimationFrame(() => {
          backgroundMusic.play().catch(err => {
            console.log("Audio play prevented:", err);
          });
        });
      }
    }
    
    if (menuMusic) {
      if (newMutedState) {
        menuMusic.pause();
      } else {
        menuMusic.volume = 0.3;
        menuMusic.loop = true;
        requestAnimationFrame(() => {
          menuMusic.play().catch(err => {
            console.log("Menu music play prevented:", err);
          });
        });
      }
    }
    
    console.log(`Sound ${newMutedState ? 'muted' : 'unmuted'}`);
  },
  
  setIntensity: (level) => {
    const { backgroundMusic, isMuted, intensity: currentIntensity } = get();
    
    if (level === currentIntensity) return;
    
    const config = INTENSITY_CONFIG[level];
    set({ 
      intensity: level,
      targetVolume: config.volume
    });
    
    if (backgroundMusic && !isMuted) {
      const startVolume = backgroundMusic.volume;
      const targetVol = config.volume;
      const startRate = backgroundMusic.playbackRate;
      const targetRate = config.playbackRate;
      
      let step = 0;
      const steps = 20;
      const interval = setInterval(() => {
        step++;
        const t = step / steps;
        const eased = t * t * (3 - 2 * t);
        
        backgroundMusic.volume = startVolume + (targetVol - startVolume) * eased;
        backgroundMusic.playbackRate = startRate + (targetRate - startRate) * eased;
        
        if (step >= steps) {
          clearInterval(interval);
          set({ currentVolume: targetVol });
        }
      }, 25);
    }
  },
  
  updateIntensityFromGameState: (playerSpeed, recentCollisions, nearbyEnemies) => {
    const { setIntensity, intensity: currentIntensity } = get();
    
    let score = 0;
    score += Math.min(playerSpeed * 50, 25);
    score += recentCollisions * 8;
    score += nearbyEnemies * 5;
    
    let newIntensity: IntensityLevel;
    if (score >= 35) {
      newIntensity = "intense";
    } else if (score >= 12) {
      newIntensity = "active";
    } else {
      newIntensity = "calm";
    }
    
    if (newIntensity !== currentIntensity) {
      setIntensity(newIntensity);
    }
  },
  
  playHit: (playbackRate = 1, volume = 0.3) => {
    const { hitSound } = get();
    if (hitSound) {
      hitSound.currentTime = 0;
      hitSound.playbackRate = playbackRate;
      hitSound.volume = volume;
      hitSound.play().catch(() => {});
    }
  },
  
  playSuccess: () => {
    const { successSound } = get();
    if (successSound) {
      successSound.currentTime = 0;
      successSound.play().catch(error => {
        console.log("Success sound play prevented:", error);
      });
    }
  },
  
  playSound: (type: SoundType, intensity: number = 1) => {
    const { initAudioContext } = get();
    initAudioContext();
    
    let ctx = get().audioContext;
    if (!ctx) {
      try {
        ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
        set({ audioContext: ctx });
      } catch (e) {
        console.log("Cannot create AudioContext");
        return;
      }
    }
    
    if (ctx.state === 'suspended') {
      ctx.resume().catch(() => {});
    }
    
    try {
      const masterGain = ctx.createGain();
      masterGain.connect(ctx.destination);
      masterGain.gain.value = 0.7 * intensity;
      
      const now = ctx.currentTime;
      
      switch (type) {
        case "collision_enemy": {
          const osc = ctx.createOscillator();
          const gain = ctx.createGain();
          osc.connect(gain);
          gain.connect(masterGain);
          osc.frequency.setValueAtTime(150, now);
          osc.frequency.exponentialRampToValueAtTime(80, now + 0.15);
          gain.gain.setValueAtTime(0.6, now);
          gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
          osc.start(now);
          osc.stop(now + 0.2);
          break;
        }
        case "collision_mushroom": {
          const osc = ctx.createOscillator();
          const gain = ctx.createGain();
          osc.connect(gain);
          gain.connect(masterGain);
          osc.frequency.setValueAtTime(400, now);
          osc.frequency.exponentialRampToValueAtTime(800, now + 0.1);
          osc.frequency.exponentialRampToValueAtTime(400, now + 0.2);
          gain.gain.setValueAtTime(0.4, now);
          gain.gain.exponentialRampToValueAtTime(0.01, now + 0.25);
          osc.start(now);
          osc.stop(now + 0.25);
          break;
        }
        case "collision_tree": {
          const noise = ctx.createBufferSource();
          const bufferSize = ctx.sampleRate * 0.2;
          const buffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate);
          const data = buffer.getChannelData(0);
          for (let i = 0; i < bufferSize; i++) {
            data[i] = (Math.random() * 2 - 1) * Math.exp(-i / (bufferSize * 0.3));
          }
          noise.buffer = buffer;
          const filter = ctx.createBiquadFilter();
          filter.type = "lowpass";
          filter.frequency.value = 800;
          noise.connect(filter);
          filter.connect(masterGain);
          noise.start(now);
          break;
        }
        case "collision_snowman": {
          const noise = ctx.createBufferSource();
          const bufferSize = ctx.sampleRate * 0.15;
          const buffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate);
          const data = buffer.getChannelData(0);
          for (let i = 0; i < bufferSize; i++) {
            data[i] = (Math.random() * 2 - 1) * Math.exp(-i / (bufferSize * 0.2));
          }
          noise.buffer = buffer;
          const filter = ctx.createBiquadFilter();
          filter.type = "highpass";
          filter.frequency.value = 2000;
          const gain = ctx.createGain();
          gain.gain.value = 0.3;
          noise.connect(filter);
          filter.connect(gain);
          gain.connect(masterGain);
          noise.start(now);
          break;
        }
        case "fall_player":
        case "fall_enemy": {
          const osc = ctx.createOscillator();
          const gain = ctx.createGain();
          osc.connect(gain);
          gain.connect(masterGain);
          osc.frequency.setValueAtTime(600, now);
          osc.frequency.exponentialRampToValueAtTime(100, now + 0.5);
          gain.gain.setValueAtTime(0.5, now);
          gain.gain.exponentialRampToValueAtTime(0.01, now + 0.5);
          osc.start(now);
          osc.stop(now + 0.5);
          break;
        }
        case "fall_orb": {
          const osc = ctx.createOscillator();
          const gain = ctx.createGain();
          osc.connect(gain);
          gain.connect(masterGain);
          osc.type = "sine";
          osc.frequency.setValueAtTime(1200, now);
          osc.frequency.exponentialRampToValueAtTime(200, now + 0.2);
          gain.gain.setValueAtTime(0.3, now);
          gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
          osc.start(now);
          osc.stop(now + 0.2);
          break;
        }
        case "pull_back": {
          // Rubber band stretching sound - creaking tension with rising pitch
          const osc1 = ctx.createOscillator();
          const osc2 = ctx.createOscillator();
          const gain1 = ctx.createGain();
          const gain2 = ctx.createGain();
          const filter = ctx.createBiquadFilter();
          
          // Main stretching tone - rises with pull intensity
          osc1.connect(gain1);
          gain1.connect(filter);
          filter.connect(masterGain);
          osc1.type = "sawtooth";
          const baseFreq = 80 + intensity * 180;
          osc1.frequency.setValueAtTime(baseFreq, now);
          osc1.frequency.linearRampToValueAtTime(baseFreq * 1.5, now + 0.12);
          gain1.gain.setValueAtTime(0.18 * intensity, now);
          gain1.gain.linearRampToValueAtTime(0.22 * intensity, now + 0.08);
          gain1.gain.exponentialRampToValueAtTime(0.01, now + 0.15);
          
          // Creaking overtone for rubber texture
          osc2.connect(gain2);
          gain2.connect(filter);
          osc2.type = "triangle";
          osc2.frequency.setValueAtTime(baseFreq * 3, now);
          osc2.frequency.linearRampToValueAtTime(baseFreq * 4, now + 0.12);
          gain2.gain.setValueAtTime(0.08 * intensity, now);
          gain2.gain.exponentialRampToValueAtTime(0.01, now + 0.12);
          
          filter.type = "bandpass";
          filter.frequency.value = 400 + intensity * 600;
          filter.Q.value = 2;
          
          osc1.start(now);
          osc2.start(now);
          osc1.stop(now + 0.15);
          osc2.stop(now + 0.15);
          break;
        }
        case "launch": {
          // Rubber band snap/release sound - quick twang with harmonics
          const osc1 = ctx.createOscillator();
          const osc2 = ctx.createOscillator();
          const osc3 = ctx.createOscillator();
          const gain1 = ctx.createGain();
          const gain2 = ctx.createGain();
          const gain3 = ctx.createGain();
          const filter = ctx.createBiquadFilter();
          
          // Main snap - quick high to low sweep
          osc1.connect(gain1);
          gain1.connect(filter);
          filter.connect(masterGain);
          osc1.type = "sawtooth";
          osc1.frequency.setValueAtTime(800, now);
          osc1.frequency.exponentialRampToValueAtTime(120, now + 0.08);
          osc1.frequency.exponentialRampToValueAtTime(80, now + 0.2);
          gain1.gain.setValueAtTime(0.5, now);
          gain1.gain.exponentialRampToValueAtTime(0.15, now + 0.05);
          gain1.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
          
          // Twang harmonic
          osc2.connect(gain2);
          gain2.connect(masterGain);
          osc2.type = "sine";
          osc2.frequency.setValueAtTime(1200, now);
          osc2.frequency.exponentialRampToValueAtTime(200, now + 0.12);
          gain2.gain.setValueAtTime(0.25, now);
          gain2.gain.exponentialRampToValueAtTime(0.01, now + 0.15);
          
          // Impact thump
          osc3.connect(gain3);
          gain3.connect(masterGain);
          osc3.type = "sine";
          osc3.frequency.setValueAtTime(150, now);
          osc3.frequency.exponentialRampToValueAtTime(60, now + 0.08);
          gain3.gain.setValueAtTime(0.35, now);
          gain3.gain.exponentialRampToValueAtTime(0.01, now + 0.1);
          
          filter.type = "lowpass";
          filter.frequency.setValueAtTime(3000, now);
          filter.frequency.exponentialRampToValueAtTime(500, now + 0.15);
          
          osc1.start(now);
          osc2.start(now);
          osc3.start(now);
          osc1.stop(now + 0.25);
          osc2.stop(now + 0.18);
          osc3.stop(now + 0.12);
          break;
        }
        case "orb_collect": {
          const osc1 = ctx.createOscillator();
          const osc2 = ctx.createOscillator();
          const gain = ctx.createGain();
          osc1.connect(gain);
          osc2.connect(gain);
          gain.connect(masterGain);
          osc1.frequency.setValueAtTime(880, now);
          osc2.frequency.setValueAtTime(1320, now + 0.05);
          gain.gain.setValueAtTime(0.3, now);
          gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
          osc1.start(now);
          osc2.start(now + 0.05);
          osc1.stop(now + 0.15);
          osc2.stop(now + 0.2);
          break;
        }
        case "level_up": {
          [523, 659, 784, 1047].forEach((freq, i) => {
            const osc = ctx.createOscillator();
            const gain = ctx.createGain();
            osc.connect(gain);
            gain.connect(masterGain);
            osc.frequency.value = freq;
            const start = now + i * 0.12;
            gain.gain.setValueAtTime(0.3, start);
            gain.gain.exponentialRampToValueAtTime(0.01, start + 0.3);
            osc.start(start);
            osc.stop(start + 0.3);
          });
          break;
        }
      }
    } catch (e) {
      console.log("Sound synthesis error:", e);
    }
  },

  playPowerUnlock: () => {
    playPowerUnlockSound();
  },
  playWolfDash: () => {
    playWolfDashSound();
  },
  playExplosion: () => {
    playExplosionSound();
  },
  playStunZap: () => {
    playStunZapSound();
  },
  playStunEnd: () => {
    playStunEndSound();
  },
  playRicochetPing: (kind: "arm" | "hit" = "arm") => {
    playRicochetPingSound(kind);
  },
  playBindWrap: () => {
    playBindWrapSound();
  },
  playShadowPulse: () => {
    playShadowPulseSound();
  }
}));

bindPowerAudioMute(() => useAudio.getState().isMuted);
installPowerAudioGestureResume();
