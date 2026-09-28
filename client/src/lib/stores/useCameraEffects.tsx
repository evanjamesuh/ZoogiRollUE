import { create } from "zustand";

// Mobile safety limits
const MOBILE_MAX_SHAKE = 0.4;
const MOBILE_MAX_FREEZE = 0.2;

export interface CameraEffect {
  id: string;
  priority: number;
  duration: number;
  startTime: number;
  persistent?: boolean; // If true, doesn't auto-expire (must be cleared manually)
  effects: {
    zoom?: number;           // FOV zoom (negative = zoom in, positive = zoom out)
    tilt?: number;           // Camera tilt in degrees
    offset?: { x: number; y: number; z: number };
    lockTarget?: "player" | "enemy" | "nearest" | "falling";
    shake?: number;          // Shake intensity (capped at 0.4 for mobile)
    timeScale?: number;      // Slow motion (0.3 = 30% speed)
    recoil?: number;         // Launch recoil effect
    snap?: boolean;          // Instant snap to position
    freezeFrame?: number;    // Freeze time in seconds (capped at 0.2 for mobile)
  };
}

export interface AbilityCameraHook {
  priority: number;
  duration: number;
  effects: CameraEffect["effects"];
}

export const ABILITY_CAMERA_HOOKS: Record<string, AbilityCameraHook> = {
  "Dash Attack": {
    priority: 20,
    duration: 0.6,
    effects: {
      zoom: -4,
      tilt: 8,
      timeScale: 0.7,
      recoil: 0.3
    }
  },
  "Instant Explosion": {
    priority: 25,
    duration: 1.0,
    effects: {
      zoom: -8,
      tilt: 5,
      shake: 0.6,
      timeScale: 0.5,
      snap: true
    }
  },
  "Laser Trajectory": {
    priority: 20,
    duration: 0.8,
    effects: {
      zoom: -6,
      tilt: 3,
      lockTarget: "enemy",
      timeScale: 0.6
    }
  },
  "Static Shock": {
    priority: 20,
    duration: 0.7,
    effects: {
      zoom: -5,
      shake: 0.4,
      timeScale: 0.65,
      snap: true
    }
  },
  "Bandage Bind": {
    priority: 20,
    duration: 1.1,
    effects: {
      zoom: -6,
      tilt: 10,
      lockTarget: "enemy",
      timeScale: 0.7,
      shake: 0.15
    }
  },
  "Flame Burst": {
    priority: 22,
    duration: 0.9,
    effects: {
      zoom: -7,
      tilt: 6,
      shake: 0.5,
      timeScale: 0.55
    }
  },
  "Shadow Stun": {
    priority: 22,
    duration: 0.8,
    effects: {
      zoom: -5,
      tilt: 12,
      lockTarget: "enemy",
      timeScale: 0.6,
      snap: true
    }
  },
  "Phase Shift": {
    priority: 18,
    duration: 0.6,
    effects: {
      zoom: -3,
      timeScale: 0.8
    }
  },
  "Ground Slam": {
    priority: 25,
    duration: 1.2,
    effects: {
      zoom: -10,
      tilt: -5,
      shake: 0.8,
      timeScale: 0.4,
      recoil: 0.5
    }
  },
  "Bone Bounce": {
    priority: 18,
    duration: 0.5,
    effects: {
      zoom: -3,
      tilt: 4,
      timeScale: 0.75
    }
  },
  "Spike Damage": {
    priority: 20,
    duration: 0.7,
    effects: {
      zoom: -5,
      shake: 0.4,
      timeScale: 0.65,
      snap: true
    }
  },
  "Slippery": {
    priority: 15,
    duration: 0.4,
    effects: {
      zoom: -2,
      timeScale: 0.85
    }
  },
  "Ice Trail": {
    priority: 18,
    duration: 0.6,
    effects: {
      zoom: -4,
      tilt: 3,
      timeScale: 0.7
    }
  },
  "Toxic Trail": {
    priority: 20,
    duration: 0.8,
    effects: {
      zoom: -5,
      tilt: 8,
      shake: 0.2,
      timeScale: 0.65
    }
  },
  "Gravity Well": {
    priority: 28,
    duration: 1.4,
    effects: {
      zoom: -12,
      tilt: -8,
      shake: 0.3,
      timeScale: 0.35,
      lockTarget: "nearest"
    }
  }
};

export const KNOCKOFF_CAMERA_HOOK: AbilityCameraHook = {
  priority: 50,
  duration: 1.2,
  effects: {
    zoom: 18,
    tilt: 15,
    shake: 0.4,
    timeScale: 0.3,
    lockTarget: "falling",
    freezeFrame: 0.15
  }
};

export const COLLISION_CAMERA_HOOK: AbilityCameraHook = {
  priority: 15,
  duration: 0.4,
  effects: {
    zoom: -3,
    shake: 0.3,
    timeScale: 0.8
  }
};

// Aim camera effect for trajectory preview
export const AIM_CAMERA_HOOK: AbilityCameraHook = {
  priority: 5,
  duration: 999, // Persistent until cleared
  effects: {
    zoom: -8,
    tilt: 8,
    offset: { x: 0, y: 1.5, z: 0 }
  }
};

// Launch camera effect
export const LAUNCH_CAMERA_HOOK: AbilityCameraHook = {
  priority: 15,
  duration: 0.4,
  effects: {
    shake: 0.2,
    recoil: 0.3
  }
};

// Target focus effect - pans camera to look at a specific world position
export const TARGET_FOCUS_CAMERA_HOOK: AbilityCameraHook = {
  priority: 18, // Higher than launch but lower than abilities
  duration: 1.2,
  effects: {
    zoom: -5,
    tilt: 5,
    timeScale: 0.7
  }
};

interface CameraEffectsState {
  activeEffects: CameraEffect[];
  baseZoom: number;
  baseTilt: number;
  freezeTimeLeft: number;
  targetWorldPosition: { x: number; y: number; z: number } | null;
  
  computedZoom: number;
  computedTilt: number;
  computedTimeScale: number;
  computedShake: number;
  computedOffset: { x: number; y: number; z: number };
  computedLockTarget: "player" | "enemy" | "nearest" | "falling" | null;
  computedTargetPosition: { x: number; y: number; z: number } | null;
  isFrozen: boolean;
  
  pushEffect: (abilityName: string, customDuration?: number) => void;
  pushCustomEffect: (hook: AbilityCameraHook) => string;
  pushKnockoffEffect: () => void;
  pushCollisionEffect: (intensity?: number) => void;
  pushAimEffect: () => string;
  pushLaunchEffect: () => void;
  pushTargetFocusEffect: (position: [number, number, number]) => void;
  clearEffect: (id: string) => void;
  clearByPriority: (minPriority: number) => void;
  clearAllEffects: () => void;
  update: (delta: number) => void;
}

export const useCameraEffects = create<CameraEffectsState>((set, get) => ({
  activeEffects: [],
  baseZoom: 0,
  baseTilt: 0,
  freezeTimeLeft: 0,
  targetWorldPosition: null,
  
  computedZoom: 0,
  computedTilt: 0,
  computedTimeScale: 1,
  computedShake: 0,
  computedOffset: { x: 0, y: 0, z: 0 },
  computedLockTarget: null,
  computedTargetPosition: null,
  isFrozen: false,
  
  pushEffect: (abilityName: string, customDuration?: number) => {
    const hook = ABILITY_CAMERA_HOOKS[abilityName];
    if (!hook) return;
    
    const id = `ability-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: hook.priority,
      duration: (customDuration ?? hook.duration) * 1000,
      startTime: Date.now(),
      effects: { ...hook.effects }
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
    }, effect.duration);
  },
  
  pushCustomEffect: (hook: AbilityCameraHook): string => {
    const id = `custom-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: hook.priority,
      duration: hook.duration * 1000,
      startTime: Date.now(),
      effects: { ...hook.effects }
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
    }, effect.duration);
    
    return id;
  },
  
  pushKnockoffEffect: () => {
    const id = `knockoff-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: KNOCKOFF_CAMERA_HOOK.priority,
      duration: KNOCKOFF_CAMERA_HOOK.duration * 1000,
      startTime: Date.now(),
      effects: { ...KNOCKOFF_CAMERA_HOOK.effects }
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
    }, effect.duration);
  },
  
  pushCollisionEffect: (intensity = 1) => {
    const id = `collision-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const scaledEffects = {
      ...COLLISION_CAMERA_HOOK.effects,
      zoom: (COLLISION_CAMERA_HOOK.effects.zoom || 0) * intensity,
      shake: (COLLISION_CAMERA_HOOK.effects.shake || 0) * intensity
    };
    
    const effect: CameraEffect = {
      id,
      priority: COLLISION_CAMERA_HOOK.priority,
      duration: COLLISION_CAMERA_HOOK.duration * 1000,
      startTime: Date.now(),
      effects: scaledEffects
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
    }, effect.duration);
  },
  
  pushAimEffect: () => {
    const id = `aim-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: AIM_CAMERA_HOOK.priority,
      duration: AIM_CAMERA_HOOK.duration * 1000,
      startTime: Date.now(),
      persistent: true, // Won't auto-expire
      effects: { ...AIM_CAMERA_HOOK.effects }
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    return id; // Return ID so caller can clear it later
  },
  
  pushLaunchEffect: () => {
    // Clear aim effects first (AIM has priority 5, so clear all <= 5)
    get().clearByPriority(AIM_CAMERA_HOOK.priority);
    
    const id = `launch-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: LAUNCH_CAMERA_HOOK.priority,
      duration: LAUNCH_CAMERA_HOOK.duration * 1000,
      startTime: Date.now(),
      effects: { ...LAUNCH_CAMERA_HOOK.effects }
    };
    
    set((state) => ({
      activeEffects: [...state.activeEffects, effect]
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
    }, effect.duration);
  },
  
  pushTargetFocusEffect: (position: [number, number, number]) => {
    const id = `target-focus-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const effect: CameraEffect = {
      id,
      priority: TARGET_FOCUS_CAMERA_HOOK.priority,
      duration: TARGET_FOCUS_CAMERA_HOOK.duration * 1000,
      startTime: Date.now(),
      effects: { ...TARGET_FOCUS_CAMERA_HOOK.effects }
    };
    
    // Store the target world position
    set((state) => ({
      activeEffects: [...state.activeEffects, effect],
      targetWorldPosition: { x: position[0], y: position[1], z: position[2] }
    }));
    
    setTimeout(() => {
      get().clearEffect(id);
      // Clear target position when effect ends
      set({ targetWorldPosition: null, computedTargetPosition: null });
    }, effect.duration);
  },
  
  clearEffect: (id: string) => {
    set((state) => ({
      activeEffects: state.activeEffects.filter(e => e.id !== id)
    }));
  },
  
  clearByPriority: (maxPriority: number) => {
    // Clear all effects with priority <= maxPriority (keeps higher priority effects)
    set((state) => ({
      activeEffects: state.activeEffects.filter(e => e.priority > maxPriority)
    }));
  },
  
  clearAllEffects: () => {
    set({ activeEffects: [], freezeTimeLeft: 0, isFrozen: false });
  },
  
  update: (delta: number) => {
    const { activeEffects, baseZoom, baseTilt, freezeTimeLeft } = get();
    const now = Date.now();
    
    // Handle freeze frame (delta is in seconds, freezeTimeLeft is in seconds)
    if (freezeTimeLeft > 0) {
      const newFreezeTime = Math.max(0, freezeTimeLeft - delta);
      set({ 
        freezeTimeLeft: newFreezeTime,
        isFrozen: newFreezeTime > 0
      });
      if (newFreezeTime > 0) return; // Skip update while frozen
    }
    
    let zoom = baseZoom;
    let tilt = baseTilt;
    let timeScale = 1;
    let shake = 0;
    let offset = { x: 0, y: 0, z: 0 };
    let lockTarget: "player" | "enemy" | "nearest" | "falling" | null = null;
    let newFreezeTime = 0;
    
    let highestPriority = 0;
    
    const sortedEffects = [...activeEffects].sort((a, b) => b.priority - a.priority);
    
    sortedEffects.forEach((effect) => {
      const elapsed = now - effect.startTime;
      // For persistent effects, don't decay
      const progress = effect.persistent ? 0 : Math.min(elapsed / effect.duration, 1);
      const easeOut = 1 - Math.pow(progress, 2);
      
      if (effect.priority >= highestPriority) {
        if (effect.effects.lockTarget) {
          lockTarget = effect.effects.lockTarget;
          highestPriority = effect.priority;
        }
      }
      
      zoom += (effect.effects.zoom || 0) * easeOut;
      tilt += (effect.effects.tilt || 0) * easeOut;
      
      // Accumulate offsets
      if (effect.effects.offset) {
        offset.x += (effect.effects.offset.x || 0) * easeOut;
        offset.y += (effect.effects.offset.y || 0) * easeOut;
        offset.z += (effect.effects.offset.z || 0) * easeOut;
      }
      
      // Shake takes max value (capped at mobile limit)
      const effectShake = Math.min((effect.effects.shake || 0) * easeOut, MOBILE_MAX_SHAKE);
      shake = Math.max(shake, effectShake);
      
      // TimeScale takes min value
      if (effect.effects.timeScale !== undefined) {
        const effectTimeScale = effect.effects.timeScale + (1 - effect.effects.timeScale) * (1 - easeOut);
        timeScale = Math.min(timeScale, effectTimeScale);
      }
      
      // Freeze frame (capped at mobile limit) - only trigger once at effect start
      if (effect.effects.freezeFrame && elapsed < 50) { // Only trigger freeze once in first 50ms
        newFreezeTime = Math.min(effect.effects.freezeFrame, MOBILE_MAX_FREEZE);
      }
    });
    
    // Pass through target position if set
    const { targetWorldPosition } = get();
    
    set({
      computedZoom: zoom,
      computedTilt: tilt,
      computedTimeScale: timeScale,
      computedShake: shake,
      computedOffset: offset,
      computedLockTarget: lockTarget,
      computedTargetPosition: targetWorldPosition,
      freezeTimeLeft: newFreezeTime > 0 ? newFreezeTime : freezeTimeLeft,
      isFrozen: newFreezeTime > 0 || freezeTimeLeft > 0
    });
  }
}));

export const triggerAbilityCameraEffect = (abilityName: string) => {
  useCameraEffects.getState().pushEffect(abilityName);
};

export const triggerKnockoffCameraEffect = () => {
  useCameraEffects.getState().pushKnockoffEffect();
};

export const triggerCollisionCameraEffect = (intensity?: number) => {
  useCameraEffects.getState().pushCollisionEffect(intensity);
};

export const triggerAimCameraEffect = () => {
  return useCameraEffects.getState().pushAimEffect();
};

export const triggerLaunchCameraEffect = () => {
  useCameraEffects.getState().pushLaunchEffect();
};

export const clearCameraEffectById = (id: string) => {
  useCameraEffects.getState().clearEffect(id);
};

export const clearAimCameraEffect = () => {
  useCameraEffects.getState().clearByPriority(AIM_CAMERA_HOOK.priority);
};

export const triggerTargetFocusCameraEffect = (position: [number, number, number]) => {
  useCameraEffects.getState().pushTargetFocusEffect(position);
};

export const ARC_PEAK_CAMERA_HOOK: AbilityCameraHook = {
  priority: 35,
  duration: 1.25,
  effects: {
    freezeFrame: 0.05,
    timeScale: 0.3,
    zoom: -15,
    tilt: 5
  }
};

export const ARC_PEAK_FREEZE_ONLY_HOOK: AbilityCameraHook = {
  priority: 35,
  duration: 1.25,
  effects: {
    freezeFrame: 0.05,
    timeScale: 0.3,
    zoom: 0,
    tilt: 0
  }
};

let currentArcPeakEffectId: string | null = null;
let currentArcPeakFreezeOnlyId: string | null = null;

export const triggerArcPeakCameraEffect = () => {
  currentArcPeakEffectId = useCameraEffects.getState().pushCustomEffect(ARC_PEAK_CAMERA_HOOK);
};

export const triggerArcPeakFreezeOnly = () => {
  currentArcPeakFreezeOnlyId = useCameraEffects.getState().pushCustomEffect(ARC_PEAK_FREEZE_ONLY_HOOK);
};

export const clearArcPeakCameraEffect = () => {
  if (currentArcPeakEffectId) {
    useCameraEffects.getState().clearEffect(currentArcPeakEffectId);
    currentArcPeakEffectId = null;
  }
  if (currentArcPeakFreezeOnlyId) {
    useCameraEffects.getState().clearEffect(currentArcPeakFreezeOnlyId);
    currentArcPeakFreezeOnlyId = null;
  }
  useCameraEffects.setState({ 
    freezeTimeLeft: 0, 
    isFrozen: false,
    targetWorldPosition: null,
    computedTargetPosition: null
  });
};
