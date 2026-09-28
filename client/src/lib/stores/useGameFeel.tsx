import { create } from "zustand";

interface ScreenShake {
  intensity: number;
  duration: number;
  startTime: number;
}

interface HitEffect {
  id: string;
  position: [number, number, number];
  type: "collision" | "knockoff" | "ability" | "collect";
  timestamp: number;
}

interface FireBurst {
  id: string;
  position: [number, number, number];
  timestamp: number;
}

interface WallSpark {
  id: string;
  position: [number, number, number];
  normal: [number, number, number];
  intensity: number;
  timestamp: number;
}

interface CartoonExplosion {
  id: string;
  position: [number, number, number];
  timestamp: number;
  showBoom: boolean;
}

interface CartoonStarburst {
  id: string;
  position: [number, number, number];
  color: string;
  timestamp: number;
}

interface CartoonSpark {
  id: string;
  position: [number, number, number];
  direction: [number, number, number];
  color: string;
  timestamp: number;
}

interface CollisionBurst {
  id: string;
  position: [number, number, number];
  intensity: number;
  color1: string;
  color2: string;
  timestamp: number;
}

interface GameFeelState {
  screenShake: ScreenShake | null;
  slowMotion: { scale: number; duration: number; startTime: number } | null;
  hitEffects: HitEffect[];
  flashScreen: { color: string; duration: number; startTime: number } | null;
  fireBursts: FireBurst[];
  wallSparks: WallSpark[];
  cartoonExplosions: CartoonExplosion[];
  cartoonStarbursts: CartoonStarburst[];
  cartoonSparks: CartoonSpark[];
  collisionBursts: CollisionBurst[];
  
  triggerScreenShake: (intensity?: number, duration?: number) => void;
  triggerSlowMotion: (scale?: number, duration?: number) => void;
  triggerHitEffect: (position: [number, number, number], type: HitEffect["type"]) => void;
  triggerFlashScreen: (color?: string, duration?: number) => void;
  triggerHaptic: (type: "light" | "medium" | "heavy" | "success" | "warning" | "error") => void;
  triggerFireBurst: (position: [number, number, number]) => void;
  triggerWallSpark: (position: [number, number, number], normal: [number, number, number], intensity: number) => void;
  triggerCartoonExplosion: (position: [number, number, number], showBoom?: boolean) => void;
  triggerCartoonStarburst: (position: [number, number, number], color?: string) => void;
  triggerCartoonSparks: (position: [number, number, number], direction: [number, number, number], color?: string) => void;
  triggerCollisionBurst: (position: [number, number, number], intensity: number, color1?: string, color2?: string) => void;
  clearHitEffect: (id: string) => void;
  clearFireBurst: (id: string) => void;
  clearWallSpark: (id: string) => void;
  clearCartoonExplosion: (id: string) => void;
  clearCartoonStarburst: (id: string) => void;
  clearCartoonSpark: (id: string) => void;
  clearCollisionBurst: (id: string) => void;
  update: () => void;
}

// Effect throttling to prevent WebGL context loss
const MAX_CONCURRENT_EFFECTS = 10;
const EFFECT_COOLDOWN_MS = 80;
let lastEffectTime = 0;

const canTriggerEffect = (state: GameFeelState): boolean => {
  const now = Date.now();
  const totalEffects = state.hitEffects.length + state.fireBursts.length + 
    state.wallSparks.length + state.cartoonExplosions.length + 
    state.cartoonStarbursts.length + state.cartoonSparks.length +
    state.collisionBursts.length;
  
  if (totalEffects >= MAX_CONCURRENT_EFFECTS) return false;
  if (now - lastEffectTime < EFFECT_COOLDOWN_MS) return false;
  
  lastEffectTime = now;
  return true;
};

export const useGameFeel = create<GameFeelState>((set, get) => ({
  screenShake: null,
  slowMotion: null,
  hitEffects: [],
  flashScreen: null,
  fireBursts: [],
  wallSparks: [],
  cartoonExplosions: [],
  cartoonStarbursts: [],
  cartoonSparks: [],
  collisionBursts: [],
  
  triggerScreenShake: (intensity = 0.5, duration = 200) => {
    set({
      screenShake: {
        intensity,
        duration,
        startTime: Date.now()
      }
    });
  },
  
  triggerSlowMotion: (scale = 0.2, duration = 500) => {
    set({
      slowMotion: {
        scale,
        duration,
        startTime: Date.now()
      }
    });
  },
  
  triggerHitEffect: (position, type) => {
    const id = `hit-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      hitEffects: [...state.hitEffects, { id, position, type, timestamp: Date.now() }]
    }));
    
    setTimeout(() => {
      get().clearHitEffect(id);
    }, 1000);
  },
  
  triggerFlashScreen: (color = "#ffffff", duration = 100) => {
    set({
      flashScreen: {
        color,
        duration,
        startTime: Date.now()
      }
    });
  },
  
  triggerHaptic: (type) => {
    if (typeof navigator !== "undefined" && "vibrate" in navigator) {
      const patterns: Record<string, number | number[]> = {
        light: 25,
        medium: 60,
        heavy: 120,
        success: [30, 60, 50],
        warning: [50, 40, 50],
        error: [80, 40, 80, 40, 80]
      };
      
      try {
        navigator.vibrate(patterns[type] || 40);
      } catch (e) {
      }
    }
  },
  
  clearHitEffect: (id) => {
    set((state) => ({
      hitEffects: state.hitEffects.filter(e => e.id !== id)
    }));
  },
  
  triggerFireBurst: (position) => {
    if (!canTriggerEffect(get())) return;
    const id = `fire-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      fireBursts: [...state.fireBursts, { id, position, timestamp: Date.now() }]
    }));
    
    setTimeout(() => {
      get().clearFireBurst(id);
    }, 1500);
  },
  
  triggerWallSpark: (position, normal, intensity) => {
    if (!canTriggerEffect(get())) return;
    const id = `spark-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      wallSparks: [...state.wallSparks, { id, position, normal, intensity, timestamp: Date.now() }]
    }));
    
    setTimeout(() => {
      get().clearWallSpark(id);
    }, 800);
  },
  
  clearFireBurst: (id) => {
    set((state) => ({
      fireBursts: state.fireBursts.filter(e => e.id !== id)
    }));
  },
  
  clearWallSpark: (id) => {
    set((state) => ({
      wallSparks: state.wallSparks.filter(e => e.id !== id)
    }));
  },
  
  triggerCartoonExplosion: (position, showBoom = true) => {
    if (!canTriggerEffect(get())) return;
    const id = `cartoon-exp-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      cartoonExplosions: [...state.cartoonExplosions, { id, position, timestamp: Date.now(), showBoom }]
    }));
    
    setTimeout(() => {
      get().clearCartoonExplosion(id);
    }, 1200);
  },
  
  triggerCartoonStarburst: (position, color = "#ffff00") => {
    if (!canTriggerEffect(get())) return;
    const id = `starburst-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      cartoonStarbursts: [...state.cartoonStarbursts, { id, position, color, timestamp: Date.now() }]
    }));
    
    setTimeout(() => {
      get().clearCartoonStarburst(id);
    }, 600);
  },
  
  triggerCartoonSparks: (position, direction, color = "#ffaa00") => {
    if (!canTriggerEffect(get())) return;
    const id = `cspark-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      cartoonSparks: [...state.cartoonSparks, { id, position, direction, color, timestamp: Date.now() }]
    }));
    
    setTimeout(() => {
      get().clearCartoonSpark(id);
    }, 800);
  },
  
  clearCartoonExplosion: (id) => {
    set((state) => ({
      cartoonExplosions: state.cartoonExplosions.filter(e => e.id !== id)
    }));
  },
  
  clearCartoonStarburst: (id) => {
    set((state) => ({
      cartoonStarbursts: state.cartoonStarbursts.filter(e => e.id !== id)
    }));
  },
  
  clearCartoonSpark: (id) => {
    set((state) => ({
      cartoonSparks: state.cartoonSparks.filter(e => e.id !== id)
    }));
  },
  
  triggerCollisionBurst: (position, intensity, color1 = "#ff7700", color2 = "#ffffff") => {
    if (!canTriggerEffect(get())) return;
    const id = `collision-burst-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    set((state) => ({
      collisionBursts: [...state.collisionBursts, { id, position, intensity, color1, color2, timestamp: Date.now() }]
    }));
    
    // Faster cleanup to prevent accumulation
    setTimeout(() => {
      get().clearCollisionBurst(id);
    }, 600);
  },
  
  clearCollisionBurst: (id) => {
    set((state) => ({
      collisionBursts: state.collisionBursts.filter(e => e.id !== id)
    }));
  },
  
  update: () => {
    const now = Date.now();
    const { screenShake, slowMotion, flashScreen } = get();
    
    if (screenShake && now - screenShake.startTime > screenShake.duration) {
      set({ screenShake: null });
    }
    
    if (slowMotion && now - slowMotion.startTime > slowMotion.duration) {
      set({ slowMotion: null });
    }
    
    if (flashScreen && now - flashScreen.startTime > flashScreen.duration) {
      set({ flashScreen: null });
    }
  }
}));

export const triggerCollisionFeel = (impactStrength: number = 0.5) => {
  const gameFeel = useGameFeel.getState();
  gameFeel.triggerScreenShake(impactStrength * 0.8, 150);
  gameFeel.triggerHaptic(impactStrength > 0.7 ? "heavy" : impactStrength > 0.4 ? "medium" : "light");
};

export const triggerKnockoffFeel = (position: [number, number, number]) => {
  const gameFeel = useGameFeel.getState();
  gameFeel.triggerScreenShake(1.0, 300);
  gameFeel.triggerSlowMotion(0.3, 400);
  gameFeel.triggerHitEffect(position, "knockoff");
  gameFeel.triggerFlashScreen("#ff4444", 80);
  gameFeel.triggerHaptic("success");
};

export const triggerAbilityFeel = (position: [number, number, number], intensity: number = 0.7) => {
  const gameFeel = useGameFeel.getState();
  gameFeel.triggerScreenShake(intensity, 200);
  gameFeel.triggerHitEffect(position, "ability");
  gameFeel.triggerHaptic("heavy");
};

export const triggerCollectFeel = (position: [number, number, number]) => {
  const gameFeel = useGameFeel.getState();
  gameFeel.triggerHitEffect(position, "collect");
  gameFeel.triggerHaptic("light");
};

export const triggerLaunchFeel = () => {
  const gameFeel = useGameFeel.getState();
  gameFeel.triggerScreenShake(0.3, 100);
  gameFeel.triggerHaptic("medium");
};

export const triggerWallHitFeel = (impactSpeed: number = 0.5) => {
  const gameFeel = useGameFeel.getState();
  const intensity = Math.min(impactSpeed * 3, 2.5);
  gameFeel.triggerScreenShake(intensity, 350);
  gameFeel.triggerHaptic("heavy");
  if (intensity > 1.0) {
    gameFeel.triggerHaptic("heavy");
  }
};
