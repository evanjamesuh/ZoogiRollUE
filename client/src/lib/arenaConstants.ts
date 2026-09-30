import { ZOOGI_REST_Y } from "./restHeight";
import { ICE_RIM } from "./roundRim";

export const ARENA_RADIUS = 18;
export const WALL_THICKNESS = 1.5;

// Knockout scoring constants
export const KNOCKOUT_RADIUS = 50; // Fixed knockout boundary radius - must be larger than outer walls
export const KNOCKOUT_SCORE_ORB = 50; // Points for knocking out an orb
export const KNOCKOUT_SCORE_PLAYER = 100; // Points for knocking out a player
export const KNOCKOUT_PENALTY = 75; // Penalty for falling off the island
export const BUMPER_SCORE = 50; // Points for touching a bumper
export const ZONE_SCORE_ORB = 15; // Points for player stopping inside a score zone (ring changes to player color)
export const KNOCKOUT_RESPAWN_BEAT = 1200; // Pause before a knocked-out marble returns to the floor
export const SCORE_ZONE_RADIUS = 4; // Radius of score zones for orb detection
export const RESTRICTION_PHASE_DURATION = 5 * 60 * 1000; // 5 minutes in ms
export const RESPAWN_DELAY = 3000; // 3 seconds before respawn
export const INVULNERABILITY_DURATION = 2000; // 2 seconds invulnerability after respawn

// Spawn points outside the ring - players start and respawn here
export const SPAWN_POINT_RADIUS = ARENA_RADIUS + 8; // Well outside the ring

// Ice theme: spawn inside the flat ice area (GLB island walls are at ~radius 15)
export const ICE_SPAWN_POINT_RADIUS = 10;

// Active map theme — set when a game starts so all spawn calls use the right radius
let _activeMapTheme = "grass";
export function setActiveMapTheme(theme: string) { _activeMapTheme = theme; }
export function getActiveMapTheme() { return _activeMapTheme; }

export const SPAWN_POINT_ANGLES = [
  0,           // Right (0°)
  Math.PI / 2, // Bottom (90°)
  Math.PI,     // Left (180°)
  Math.PI * 1.5, // Top (270°)
  Math.PI / 4,   // Bottom-right (45°)
  Math.PI * 3/4, // Bottom-left (135°)
  Math.PI * 5/4, // Top-left (225°)
  Math.PI * 7/4  // Top-right (315°)
];

// Get spawn point position by index
// If zoneEditorConfigs provided, use spawn zones from config; otherwise use fallback angles
export function getSpawnPointPosition(
  spawnIndex: number, 
  zoneEditorConfigs?: { angle: number; distance: number; isSpawn: boolean }[],
  mapTheme?: string
): [number, number, number] {
  // Get spawn zones from config if available
  if (zoneEditorConfigs && zoneEditorConfigs.length > 0) {
    const spawnZones = zoneEditorConfigs.filter(z => z.isSpawn);
    if (spawnZones.length > 0) {
      const zone = spawnZones[spawnIndex % spawnZones.length];
      return [
        Math.cos(zone.angle) * zone.distance,
        ZOOGI_REST_Y,
        Math.sin(zone.angle) * zone.distance
      ];
    }
  }
  // Use map-specific spawn radius — ice spawns inside the flat ice area to avoid GLB wall clipping
  const theme = mapTheme ?? _activeMapTheme;
  const spawnRadius = theme === "ice" ? ICE_SPAWN_POINT_RADIUS : SPAWN_POINT_RADIUS;
  const angle = SPAWN_POINT_ANGLES[spawnIndex % SPAWN_POINT_ANGLES.length];
  return [
    Math.cos(angle) * spawnRadius,
    ZOOGI_REST_Y,
    Math.sin(angle) * spawnRadius
  ];
}

// Check if position is inside the ring (for removing spawn immunity)
export function isInsideRing(x: number, z: number): boolean {
  const dist = Math.sqrt(x * x + z * z);
  return dist < ARENA_RADIUS;
}

// Check if a position is beyond the arena boundary
export function isBeyondArenaBoundary(x: number, z: number): boolean {
  const dist = Math.sqrt(x * x + z * z);
  return dist > KNOCKOUT_RADIUS;
}

// Get respawn position at a green pad
export function getGreenPadRespawnPosition(padIndex: number): [number, number, number] {
  const angle = WALL_OWNERSHIP_GAP_ANGLES[padIndex % WALL_OWNERSHIP_GAP_ANGLES.length];
  const radius = WALL_OWNERSHIP_GREEN_RESPAWN_RADIUS;
  return [
    Math.cos(angle) * radius,
    ZOOGI_REST_Y,
    Math.sin(angle) * radius
  ];
}

// Wall Ownership Mode constants
// These values are calibrated to match the visual arena decorations:
// - White circle markers on cosmos arena floor are at ~radius 10
// - Green respawn icons are positioned at ~radius 6
// - 6 gaps/zones at 60° intervals around the arena
// Adjust these values if arena model decorations change
export const WALL_OWNERSHIP_OUTER_RADIUS = ARENA_RADIUS + 16;
export const WALL_OWNERSHIP_CONTROL_ZONE_RADIUS = 27;
export const WALL_OWNERSHIP_GREEN_RESPAWN_RADIUS = 6;
export const WALL_OWNERSHIP_GAP_ANGLES = [0, Math.PI/3, 2*Math.PI/3, Math.PI, 4*Math.PI/3, 5*Math.PI/3];

// Gap configuration matching DestructibleRingWall.tsx
// 4 gaps at cardinal directions: right (0°), bottom (90°), left (180°), top (270°)
const GAP_ANGLES = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
const GAP_WIDTH = Math.PI / 10; // ~18 degrees on each side of cardinal direction
export const GAP_ARC = GAP_WIDTH * 2; // Total gap arc

export function isInGap(x: number, z: number, entityRadius: number = 0): boolean {
  const angle = Math.atan2(z, x);
  // Normalize angle to [0, 2π)
  const normalizedAngle = angle < 0 ? angle + Math.PI * 2 : angle;
  
  const dist = Math.sqrt(x * x + z * z);
  const angularMargin = dist > 0 ? (entityRadius * 0.3) / dist : 0;
  
  for (const gapAngle of GAP_ANGLES) {
    let diff = Math.abs(normalizedAngle - gapAngle);
    // Handle wrap-around (e.g., angle near 2π vs gapAngle at 0)
    if (diff > Math.PI) diff = Math.PI * 2 - diff;
    
    if (diff < GAP_WIDTH - angularMargin) {
      return true;
    }
  }
  return false;
}

interface SkaterPositionData {
  position: [number, number, number];
  velocity: [number, number, number];
  radius: number;
}

let sharedSkaterPositions: SkaterPositionData[] = [];

export function setSkaterPositions(positions: SkaterPositionData[]): void {
  sharedSkaterPositions = positions;
}

export function getSkaterPositions(): SkaterPositionData[] {
  return sharedSkaterPositions;
}

interface TrainCarPositionData {
  position: [number, number, number];
  velocity: [number, number, number];
  radius: number;
}

let sharedTrainCarPositions: TrainCarPositionData[] = [];

export function setTrainCarPositions(positions: TrainCarPositionData[]): void {
  sharedTrainCarPositions = positions;
}

export function getTrainCarPositions(): TrainCarPositionData[] {
  return sharedTrainCarPositions;
}

export function getTreePositions(): { position: [number, number, number]; radius: number }[] {
  return [
    { position: [-2.52, 0.06, 10.74], radius: 0.6 },
    { position: [-8.14, 0.37, 10.14], radius: 0.6 },
    { position: [-12.46, 0.5, 0.28], radius: 0.6 },
    { position: [1.37, 0.36, -8.9], radius: 0.6 },
    { position: [12.11, 0.3, 4.45], radius: 0.6 },
    { position: [-6.2, 0.36, -8.92], radius: 0.6 },
    { position: [7.1, 0.3, -1.28], radius: 0.6 },
    { position: [4.46, 0.6, 10.55], radius: 0.6 }
  ];
}

export function getSnowmanPositions(): { position: [number, number, number]; radius: number }[] {
  // Six snowmen on the rink rim, off the cardinal lanes. The base sphere is this radius.
  return ICE_RIM.map((mark) => {
    const angle = (mark.angleDeg * Math.PI) / 180;
    return {
      position: [Math.cos(angle) * mark.distance, 0, Math.sin(angle) * mark.distance],
      radius: mark.radius,
    };
  });
}

export function getAlienPositions(): { position: [number, number, number]; radius: number }[] {
  return [
    { position: [8, 0, -8], radius: 1.5 },
    { position: [8, 0, 8], radius: 1.5 },
    { position: [-8, 0, -8], radius: 1.5 },
    { position: [-8, 0, 8], radius: 1.5 },
    { position: [0, 0, -10], radius: 1.5 },
    { position: [0, 0, 10], radius: 1.5 }
  ];
}

export function getCrystalPositions(): { position: [number, number, number]; radius: number }[] {
  return [
    { position: [8.78, 0.5, 4.79], radius: 1.2 },
    { position: [-7.67, 0.5, 14.04], radius: 1.2 },
    { position: [-9.6, 0.5, 2.82], radius: 1.2 },
    { position: [-11.41, 0.5, -6.23], radius: 1.2 },
    { position: [-4.5, 0.5, -15.35], radius: 1.2 },
    { position: [4.79, 0.5, -8.78], radius: 1.2 },
    { position: [12.47, 0.5, -3.66], radius: 1.2 }
  ];
}

export function getRockPositions(): { position: [number, number, number]; radius: number }[] {
  return [
    { position: [7.37, 0, 3.12], radius: 1.0 },
    { position: [6.2, 0, 10.28], radius: 1.0 },
    { position: [-1.37, 0, 15.94], radius: 1.0 },
    { position: [-5.24, 0, 6.05], radius: 1.0 },
    { position: [-11.69, 0, 2.72], radius: 1.0 },
    { position: [-14.74, 0, -6.23], radius: 1.0 },
    { position: [-4.13, 0, -6.85], radius: 1.0 },
    { position: [1.03, 0, -11.96], radius: 1.0 },
    { position: [10.48, 0, -12.09], radius: 1.0 },
    { position: [7.79, 0, -1.81], radius: 1.0 }
  ];
}

export function getMushroomPositions(): { position: [number, number, number]; radius: number }[] {
  return [
    { position: [8, 0, -8], radius: 2.5 },
    { position: [8, 0, 8], radius: 2.5 }
  ];
}

export function getSnowPinePositions(): { position: [number, number, number]; radius: number }[] {
  const positions: { position: [number, number, number]; radius: number }[] = [];
  const treeCount = 10;
  
  for (let i = 0; i < treeCount; i++) {
    const angle = (i / treeCount) * Math.PI * 2 + 0.8;
    const radius = 9 + (i % 3) * 4;
    const x = Math.cos(angle) * radius;
    const z = Math.sin(angle) * radius;
    
    if (z > 8 && Math.abs(x) < 5) continue;
    
    positions.push({
      position: [x, 0, z],
      radius: 2.0
    });
  }
  
  return positions;
}
