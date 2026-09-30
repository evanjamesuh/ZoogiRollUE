export type VfxQuality = "high" | "low";

export interface ParticleBudget {
  smoke: number;
  fire: number;
  embers: number;
  wisps: number;
}

export interface BloomParams {
  intensity: number;
  levels: number;
  resolutionScale: number;
  radius: number;
}

let quality: VfxQuality = "high";
let locked = false;
const listeners = new Set<() => void>();

function publish(): void {
  if (typeof window === "undefined") return;
  (window as Window & { __vfxQuality?: VfxQuality }).__vfxQuality = quality;
}

function detectInitial(): { quality: VfxQuality; locked: boolean } {
  if (typeof window === "undefined") return { quality: "high", locked: false };
  const query = new URLSearchParams(window.location.search).get("vfx");
  if (query === "high" || query === "low") return { quality: query, locked: true };
  const mobile = window.matchMedia("(pointer: coarse)").matches || window.innerWidth < 900;
  return { quality: mobile ? "low" : "high", locked: false };
}

const initial = detectInitial();
quality = initial.quality;
locked = initial.locked;
publish();

export function getVfxQuality(): VfxQuality {
  return quality;
}

export function isVfxQualityLocked(): boolean {
  return locked;
}

/** Drops bloom cost. Ignored when `?vfx=high` or `?vfx=low` locks the setting. */
export function setVfxQuality(next: VfxQuality): void {
  if (locked || quality === next) return;
  quality = next;
  publish();
  listeners.forEach((listener) => listener());
}

export function subscribeVfxQuality(listener: () => void): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function particleBudget(): ParticleBudget {
  if (quality === "low") {
    return { smoke: 6, fire: 7, embers: 20, wisps: 12 };
  }
  return { smoke: 12, fire: 14, embers: 42, wisps: 24 };
}

export function bloomParams(level: VfxQuality = quality): BloomParams {
  if (level === "low") {
    return { intensity: 0.24, levels: 3, resolutionScale: 0.2, radius: 0.32 };
  }
  return { intensity: 0.32, levels: 4, resolutionScale: 0.3, radius: 0.42 };
}
