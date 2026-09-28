export interface SkinEffect {
  id: string;
  name: string;
  trailColor: string | null;
  trailType: "solid" | "gradient" | "particles" | "rainbow" | null;
  glowColor: string | null;
  glowIntensity: number;
  colorOverride: string | null;
  particleEffect: "none" | "sparkle" | "fire" | "ice" | "shadow" | "rainbow";
}

export const SKIN_EFFECTS: Record<string, SkinEffect> = {
  default: {
    id: "default",
    name: "Default",
    trailColor: null,
    trailType: null,
    glowColor: null,
    glowIntensity: 0,
    colorOverride: null,
    particleEffect: "none"
  },
  skin_golden_trail: {
    id: "skin_golden_trail",
    name: "Golden Trail",
    trailColor: "#FFD700",
    trailType: "gradient",
    glowColor: "#FFD700",
    glowIntensity: 0.3,
    colorOverride: null,
    particleEffect: "sparkle"
  },
  skin_neon: {
    id: "skin_neon",
    name: "Neon Glow",
    trailColor: "#00FFFF",
    trailType: "solid",
    glowColor: "#00FFFF",
    glowIntensity: 0.8,
    colorOverride: null,
    particleEffect: "sparkle"
  },
  skin_fire: {
    id: "skin_fire",
    name: "Fire Aura",
    trailColor: "#FF4500",
    trailType: "gradient",
    glowColor: "#FF6600",
    glowIntensity: 0.6,
    colorOverride: null,
    particleEffect: "fire"
  },
  skin_ice: {
    id: "skin_ice",
    name: "Ice Crystal",
    trailColor: "#87CEEB",
    trailType: "particles",
    glowColor: "#ADD8E6",
    glowIntensity: 0.4,
    colorOverride: null,
    particleEffect: "ice"
  },
  skin_shadow: {
    id: "skin_shadow",
    name: "Shadow Form",
    trailColor: "#1a1a2e",
    trailType: "gradient",
    glowColor: "#4B0082",
    glowIntensity: 0.5,
    colorOverride: "#1a1a2e",
    particleEffect: "shadow"
  },
  skin_rainbow: {
    id: "skin_rainbow",
    name: "Rainbow Shift",
    trailColor: null,
    trailType: "rainbow",
    glowColor: null,
    glowIntensity: 0.6,
    colorOverride: null,
    particleEffect: "rainbow"
  }
};

export function getSkinEffect(skinId: string | null): SkinEffect {
  if (!skinId) return SKIN_EFFECTS.default;
  return SKIN_EFFECTS[skinId] || SKIN_EFFECTS.default;
}

export function getRainbowColor(time: number): string {
  const hue = (time * 100) % 360;
  return `hsl(${hue}, 100%, 50%)`;
}
