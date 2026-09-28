import { getSnowmanPositions } from "./arenaConstants";

/**
 * Solid shapes the marble simulation uses.
 *
 * Grass numbers were measured from floating_island_stage.glb (local space,
 * including node transforms) and scaled by GRASS_STAGE.modelScale. Rock and
 * bush radii are the horizontal footprint at marble height: the mean of the
 * mesh's X/Z size for rocks, and the narrow half of the canopy for bushes.
 * The old pinball radius (2) was paired with bumper.glb, which is 0.03 wide
 * at the scale it was drawn, so those hits were invisible.
 */
export const BUMPER_RADIUS = 0.955;
export const BUMPER_MODEL_URL = "/models/bumber1.glb";
export const MARBLE_RADIUS = 0.5;
export const ORB_RADIUS = 0.4;
export const SNOWMAN_RADIUS = 0.55;
export const REST_SPEED = 0.02;

export const GRASS_STAGE = {
  /** Central grass disk in the glb is 0.45 wide. Scale 60 makes it radius ~13.5. */
  modelScale: 60,
  modelOffsetY: 0,
  groundRadius: 0.225 * 60,
};

export type SolidKind = "bumper" | "rock" | "bush" | "snowman" | "hoodoo" | "prop";

export interface SolidCircle {
  id: string;
  x: number;
  z: number;
  radius: number;
  kind: SolidKind;
}

export interface ZonePlacement {
  id: string;
  angle: number;
  distance: number;
  visible: boolean;
  isSpawn: boolean;
}

export interface MapLayout {
  id: string;
  floorRadius: number;
  knockoffRadius: number;
  orbRingRadius: number;
  zones: ZonePlacement[];
  scenery: SolidCircle[];
  bumpers: { id: string; x: number; z: number }[];
}

interface LocalCircle {
  id: string;
  x: number;
  z: number;
  radius: number;
  kind: SolidKind;
}

// Footprints sampled from floating_island_stage.glb at scale 1.
const GRASS_LOCAL: LocalCircle[] = [
  { id: "S_7_rock_0", x: 0.12, z: 0.2, radius: 0.075, kind: "rock" },
  { id: "S_6_rock_0", x: -0.21, z: 0.08, radius: 0.0725, kind: "rock" },
  { id: "S_5_rock_0", x: -0.19, z: -0.14, radius: 0.065, kind: "rock" },
  { id: "S_8_rock_0", x: 0.23, z: 0, radius: 0.055, kind: "rock" },
  { id: "S_4_rock_0", x: 0.18, z: -0.14, radius: 0.0475, kind: "rock" },
  { id: "S_1_rock_0", x: -0.01, z: -0.23, radius: 0.05, kind: "rock" },
  { id: "Tree2_Leavs_0", x: -0.03, z: 0.2, radius: 0.0385, kind: "bush" },
  { id: "Tree1_Leavs_0", x: 0.11, z: -0.16, radius: 0.0275, kind: "bush" },
  { id: "Tree3_Leavs_0", x: -0.2, z: -0.07, radius: 0.0248, kind: "bush" },
  { id: "Tree8_Leavs_0", x: 0.16, z: 0.1, radius: 0.0193, kind: "bush" },
];

function grassScenery(): SolidCircle[] {
  const scale = GRASS_STAGE.modelScale;
  return GRASS_LOCAL.map((item) => ({
    id: item.id,
    x: item.x * scale,
    z: item.z * scale,
    radius: item.radius * scale,
    kind: item.kind,
  }));
}

function polar(id: string, angle: number, distance: number): { id: string; x: number; z: number } {
  return { id, x: Math.cos(angle) * distance, z: Math.sin(angle) * distance };
}

function zones(
  spawns: { angle: number; distance: number }[],
  scores: { angle: number; distance: number }[],
): ZonePlacement[] {
  return [
    ...spawns.map((spawn, i) => ({
      id: `spawn-${i}`,
      angle: spawn.angle,
      distance: spawn.distance,
      visible: false,
      isSpawn: true,
    })),
    ...scores.map((score, i) => ({
      id: `score-${i}`,
      angle: score.angle,
      distance: score.distance,
      visible: true,
      isSpawn: false,
    })),
  ];
}

export interface HoodooDecor {
  id: string;
  position: [number, number, number];
  scale: number;
  rotation: number;
  radius: number;
}

/** Same placement the lava map draws. Radius matches the wide cap of the fallback hoodoo (0.6 * scale), slightly inset. */
export function getHoodooDecor(): HoodooDecor[] {
  const decor: HoodooDecor[] = [];
  const count = 6;
  for (let i = 0; i < count; i++) {
    const angle = (i / count) * Math.PI * 2 + 0.3;
    const distance = 10 + (i % 2) * 4;
    const x = Math.cos(angle) * distance;
    const z = Math.sin(angle) * distance;
    if (z > 8 && Math.abs(x) < 5) continue;
    const scale = 2.0 + (i % 3) * 0.4;
    decor.push({
      id: `hoodoo-${i}`,
      position: [x, 0, z],
      scale,
      rotation: (i * 1.8) % (Math.PI * 2),
      radius: 0.55 * scale,
    });
  }
  return decor;
}

export interface IcePatch {
  id: string;
  x: number;
  z: number;
  radius: number;
  rotation: number;
}

/** Drawn ice disks and the speed boost use this one list. */
export function getIcePatches(): IcePatch[] {
  const patches: IcePatch[] = [];
  const patchCount = 4;
  for (let i = 0; i < patchCount; i++) {
    const angle = (i / patchCount) * Math.PI * 2 + 0.3;
    const distance = 8 + (i % 2) * 4;
    patches.push({
      id: `ice-${i}`,
      x: Math.cos(angle) * distance,
      z: Math.sin(angle) * distance,
      radius: 3.0 + (i % 2) * 0.5,
      rotation: angle,
    });
  }
  return patches;
}

const CARDINAL = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
const DIAGONAL = [Math.PI / 4, (3 * Math.PI) / 4, (5 * Math.PI) / 4, (7 * Math.PI) / 4];

function flatBumpers(distance: number, angles: number[]): { id: string; x: number; z: number }[] {
  return angles.map((angle, i) => polar(`bumper-${i}`, angle, distance));
}

function buildLayout(
  id: string,
  floorRadius: number,
  knockoffRadius: number,
  scenery: SolidCircle[],
  bumperDistance: number,
  bumperAngles: number[],
  scoreAngles: number[] = [0.35, 1.9, 3.5, 5.1],
): MapLayout {
  const bumpers = flatBumpers(bumperDistance, bumperAngles);
  return {
    id,
    floorRadius,
    knockoffRadius,
    orbRingRadius: 4.2,
    scenery,
    bumpers,
    zones: zones(
      CARDINAL.map((angle) => ({ angle, distance: 7.6 })),
      scoreAngles.map((angle) => ({ angle, distance: 11.2 })),
    ),
  };
}

const LAYOUTS: Record<string, MapLayout> = {
  grass: {
    id: "grass",
    floorRadius: GRASS_STAGE.groundRadius,
    // Just past the outermost rock so the white ring is the drop after the rim, not empty void.
    knockoffRadius: 19.2,
    orbRingRadius: 4.2,
    scenery: grassScenery(),
    bumpers: flatBumpers(6.4, DIAGONAL),
    zones: zones(
      CARDINAL.map((angle) => ({ angle, distance: 8.1 })),
      // Angles that stay clear of the rim rocks and bushes all the way to the drop.
      // Far enough from the cardinal spawns that a radius-4 zone does not cover the spawn,
      // and still on a ray that misses the rim rocks.
      [20, 120, 240, 290].map((deg) => ({ angle: (deg * Math.PI) / 180, distance: 11 })),
    ),
  },
  ice: buildLayout(
    "ice",
    17.5,
    18.4,
    getSnowmanPositions().map((snowman, i) => ({
      id: `snowman-${i}`,
      x: snowman.position[0],
      z: snowman.position[2],
      radius: SNOWMAN_RADIUS,
      kind: "snowman" as const,
    })),
    6.6,
    DIAGONAL,
  ),
  lava: buildLayout(
    "lava",
    18,
    18.6,
    getHoodooDecor().map((hoodoo) => ({
      id: hoodoo.id,
      x: hoodoo.position[0],
      z: hoodoo.position[2],
      radius: hoodoo.radius,
      kind: "hoodoo" as const,
    })),
    6.4,
    DIAGONAL,
    [55, 115, 210, 300].map((deg) => (deg * Math.PI) / 180),
  ),
  space: buildLayout("space", 18, 18.6, [], 8.5, DIAGONAL),
  saturn: buildLayout("saturn", 18, 18.6, [], 8.5, DIAGONAL),
};

export function getMapLayout(mapId: string | null | undefined): MapLayout | null {
  if (!mapId) return null;
  return LAYOUTS[mapId] ?? null;
}

export function bumperSolids(bumpers: { id: string; x: number; z: number }[]): SolidCircle[] {
  return bumpers.map((bumper) => ({
    id: bumper.id,
    x: bumper.x,
    z: bumper.z,
    radius: BUMPER_RADIUS,
    kind: "bumper" as const,
  }));
}

export interface EditorProp {
  id: string;
  modelUrl: string;
  position: [number, number, number];
  scale: [number, number, number];
}

export function editorPropSolids(models: EditorProp[]): SolidCircle[] {
  return models.map((model) => {
    const scale = Math.max(Math.abs(model.scale[0]), Math.abs(model.scale[2]), 0.01);
    const bumper = model.modelUrl.includes("bumber1") || model.modelUrl.includes("bumper.glb");
    return {
      id: `placed-${model.id}`,
      x: model.position[0],
      z: model.position[2],
      radius: (bumper ? BUMPER_RADIUS : 0.6) * scale,
      kind: bumper ? "bumper" : "prop",
    };
  });
}

export function collectMatchSolids(input: {
  map: string | null | undefined;
  bumpers: { id: string; position: [number, number, number] }[];
  landedRocks: { id: string; position: [number, number, number]; radius: number }[];
  editorModels: EditorProp[];
}): SolidCircle[] {
  const layout = getMapLayout(input.map);
  const scenery = layout?.scenery ?? [];
  const bumpers: SolidCircle[] = input.bumpers.map((bumper) => ({
    id: bumper.id,
    x: bumper.position[0],
    z: bumper.position[2],
    radius: BUMPER_RADIUS,
    kind: "bumper",
  }));
  const rocks: SolidCircle[] = input.landedRocks.map((rock) => ({
    id: rock.id,
    x: rock.position[0],
    z: rock.position[2],
    radius: rock.radius,
    kind: "rock",
  }));
  return [...scenery, ...bumpers, ...rocks, ...editorPropSolids(input.editorModels)];
}

export function resolveSolidCollision(
  prev: [number, number, number],
  pos: [number, number, number],
  vel: [number, number, number],
  entityRadius: number,
  solids: SolidCircle[],
  restitution = 0.72,
): { pos: [number, number, number]; vel: [number, number, number]; hits: string[] } {
  let x = pos[0];
  let z = pos[2];
  let vx = vel[0];
  let vz = vel[2];
  const hits: string[] = [];
  const y = pos[1];

  for (let pass = 0; pass < 3; pass++) {
    let hitThisPass = false;
    for (const solid of solids) {
      const minDist = solid.radius + entityRadius;
      let hitX = x;
      let hitZ = z;
      let found = false;
      const steps = 6;
      for (let s = 1; s <= steps; s++) {
        const t = s / steps;
        const sx = prev[0] + (x - prev[0]) * t;
        const sz = prev[2] + (z - prev[2]) * t;
        if (Math.hypot(sx - solid.x, sz - solid.z) < minDist) {
          hitX = sx;
          hitZ = sz;
          found = true;
          break;
        }
      }
      if (!found) continue;

      let dx = hitX - solid.x;
      let dz = hitZ - solid.z;
      let dist = Math.hypot(dx, dz);
      if (dist < 1e-6) {
        dx = 1;
        dz = 0;
        dist = 1;
      }
      const nx = dx / dist;
      const nz = dz / dist;
      const dot = vx * nx + vz * nz;
      if (dot < 0) {
        vx = (vx - 2 * dot * nx) * restitution;
        vz = (vz - 2 * dot * nz) * restitution;
        const speed = Math.hypot(vx, vz);
        if (speed < REST_SPEED) {
          vx = 0;
          vz = 0;
        }
      }
      const sep = minDist + 0.04;
      x = solid.x + nx * sep;
      z = solid.z + nz * sep;
      hits.push(solid.id);
      hitThisPass = true;
    }
    if (!hitThisPass) break;
  }

  return { pos: [x, y, z], vel: [vx, vel[1], vz], hits };
}

/** True when a marble of the given radius can roll from the origin to the drop without touching a solid. */
export function laneIsClear(layout: MapLayout, angle: number, entityRadius = MARBLE_RADIUS): boolean {
  const solids = [...layout.scenery, ...bumperSolids(layout.bumpers)];
  const step = 0.35;
  for (let distance = 0; distance <= layout.knockoffRadius; distance += step) {
    const x = Math.cos(angle) * distance;
    const z = Math.sin(angle) * distance;
    for (const solid of solids) {
      if (Math.hypot(x - solid.x, z - solid.z) < solid.radius + entityRadius) return false;
    }
  }
  return true;
}

export function countClearLanes(layout: MapLayout, samples = 24): number {
  let clear = 0;
  for (let i = 0; i < samples; i++) {
    if (laneIsClear(layout, (i / samples) * Math.PI * 2)) clear++;
  }
  return clear;
}

export function pointInsideSolid(x: number, z: number, solids: SolidCircle[], padding = 0): SolidCircle | null {
  for (const solid of solids) {
    if (Math.hypot(x - solid.x, z - solid.z) < solid.radius + padding) return solid;
  }
  return null;
}
