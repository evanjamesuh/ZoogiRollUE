import { useZoogiGame } from "./stores/useZoogiGame";

export interface PhysicsConfig {
  gravity: number;
  fixedTimestep: number;
  maxSubsteps: number;
  
  zoogiRadius: number;
  zoogiMass: number;
  zoogiRestitution: number;
  zoogiFriction: number;
  zoogiLinearDamping: number;
  zoogiAngularDamping: number;
  
  orbRadius: number;
  orbRestitution: number;
  orbFriction: number;
  orbLinearDamping: number;
  orbAngularDamping: number;
  
  arenaRadius: number;
  wallThickness: number;
  
  collisionZoogiExtra: number;
  collisionOrbExtra: number;
  knockoffYThreshold: number;
  knockoffRadiusExtra: number;
  movementStoppedThreshold: number;
  maxVelocity: number;
  
  launchImpulseMultiplier: number;
  arcLandingVelocityThreshold: number;
  shockwaveRadius: number;
  shockwaveCooldown: number;
}

const defaultConfig: PhysicsConfig = {
  gravity: -9.81,
  fixedTimestep: 1 / 120,
  maxSubsteps: 8,
  
  zoogiRadius: 0.5,
  zoogiMass: 1.0,
  zoogiRestitution: 0.85,
  zoogiFriction: 0.4,
  zoogiLinearDamping: 0.8,
  zoogiAngularDamping: 0.9,
  
  orbRadius: 0.3,
  orbRestitution: 0.9,
  orbFriction: 0.3,
  orbLinearDamping: 0.5,
  orbAngularDamping: 0.6,
  
  arenaRadius: 18,
  wallThickness: 1.5,
  
  collisionZoogiExtra: 0.2,
  collisionOrbExtra: 0.1,
  knockoffYThreshold: -2,
  knockoffRadiusExtra: 2,
  movementStoppedThreshold: 0.05,
  maxVelocity: 15,
  
  launchImpulseMultiplier: 6,
  arcLandingVelocityThreshold: -3,
  shockwaveRadius: 4.0,
  shockwaveCooldown: 500,
};

const iceOverrides: Partial<PhysicsConfig> = {
  maxSubsteps: 3,
  zoogiRestitution: 0.97,
  zoogiFriction: 0.03,
  zoogiLinearDamping: 0.05,
  zoogiAngularDamping: 0.1,
  orbRestitution: 0.95,
  orbFriction: 0.05,
  orbLinearDamping: 0.08,
  orbAngularDamping: 0.1,
  maxVelocity: 25,
  launchImpulseMultiplier: 12,
};

let currentConfig: PhysicsConfig = { ...defaultConfig };

export function applyThemeOverrides(theme: string): void {
  currentConfig = { ...defaultConfig };
  if (theme === "ice") {
    Object.assign(currentConfig, iceOverrides);
  }
}

export function getPhysicsConfig(): PhysicsConfig {
  return currentConfig;
}

export function setPhysicsValue<K extends keyof PhysicsConfig>(key: K, value: PhysicsConfig[K]): void {
  currentConfig[key] = value;
  useZoogiGame.getState().incrementMoveCounter();
}

export function resetPhysicsConfig(): void {
  currentConfig = { ...defaultConfig };
  useZoogiGame.getState().incrementMoveCounter();
}

export function exportPhysicsConfigAsCode(): string {
  const c = currentConfig;
  return `// Physics Configuration - Paste these values into their respective files

// Dev-panel snapshot. The live match steps in useZoogiGame.physicsTick.
const FIXED_TIMESTEP = ${c.fixedTimestep.toFixed(6)};
const MAX_SUBSTEPS = ${c.maxSubsteps};
const gravity = { x: 0, y: ${c.gravity}, z: 0 };

// === store simulation reference ===
const ZOOGI_RADIUS = ${c.zoogiRadius};
const SHOCKWAVE_RADIUS = ${c.shockwaveRadius};
const ARC_LANDING_VELOCITY_THRESHOLD = ${c.arcLandingVelocityThreshold};
const ORB_RADIUS = ${c.orbRadius};
const KNOCKOFF_Y_THRESHOLD = ${c.knockoffYThreshold};
const KNOCKOFF_RADIUS_THRESHOLD = ARENA_RADIUS + ${c.knockoffRadiusExtra};
const COLLISION_DISTANCE = ZOOGI_RADIUS * 2 + ${c.collisionZoogiExtra};
const ORB_COLLISION_DISTANCE = ZOOGI_RADIUS + ORB_RADIUS + ${c.collisionOrbExtra};
const MAX_VELOCITY = ${c.maxVelocity};
const MOVEMENT_STOPPED_THRESHOLD = ${c.movementStoppedThreshold};
const LAUNCH_IMPULSE_MULTIPLIER = ${c.launchImpulseMultiplier};

// Zoogi physics body options:
{
  restitution: ${c.zoogiRestitution},
  friction: ${c.zoogiFriction},
  linearDamping: ${c.zoogiLinearDamping},
  angularDamping: ${c.zoogiAngularDamping},
  mass: ${c.zoogiMass},
}

// Orb physics body options:
{
  restitution: ${c.orbRestitution},
  friction: ${c.orbFriction},
  linearDamping: ${c.orbLinearDamping},
  angularDamping: ${c.orbAngularDamping},
}

// === arenaConstants.ts ===
export const ARENA_RADIUS = ${c.arenaRadius};
export const WALL_THICKNESS = ${c.wallThickness};

// Shockwave cooldown: ${c.shockwaveCooldown}ms
`;
}

export function getPhysicsConfigJSON(): string {
  return JSON.stringify(currentConfig, null, 2);
}
