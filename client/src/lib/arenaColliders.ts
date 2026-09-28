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

/**
 * winter_location.glb at the transform WinterLocationScene draws.
 * Ground tops sit on y=0. The open rink is inside the camp props (~radius 12);
 * snow past the knockoff is backdrop, same as the grass map's outer islands.
 */
export const WINTER_STAGE = {
  modelScale: 0.9,
  modelOffsetY: -4.2,
  floorRadius: 17.5,
  knockoffRadius: 18.4,
};

/**
 * cosmos_arena.glb. The arena floor rim is ~15.75 local and the wall stands on
 * that rim. Scale 1.08 brings the rim to ~17 so a full flick still leaves the
 * platform. The wall is the visual edge; a closed collider ring would make
 * knockouts impossible, so the knockoff line sits on the rim instead.
 * Floor mesh is at local y=-0.02.
 */
export const COSMOS_STAGE = {
  modelScale: 1.08,
  modelOffsetY: 0.02 * 1.08,
  floorRadius: 16.6,
  knockoffRadius: 17.5,
};

/**
 * arabian_nights_stage.glb is authored around (-656.5, 556.6, 11). The anchor
 * puts one courtyard point at the origin and the floor on y=0. With the stage
 * loaded, the open plaza is centered near (-9.9, -4.8), not on that point.
 * An east wall about 6 tall stands 13.5 from the origin (due east), inside the
 * old 17.2 line. The out-line is the circle around the plaza that meets that wall.
 */
const ARABIAN_PLAZA_CENTER: [number, number] = [-9.9, -4.8];
const ARABIAN_EAST_WALL = { x: 13.5, z: 0 };
const ARABIAN_KNOCKOFF = Math.hypot(
  ARABIAN_EAST_WALL.x - ARABIAN_PLAZA_CENTER[0],
  ARABIAN_EAST_WALL.z - ARABIAN_PLAZA_CENTER[1],
);

export const ARABIAN_STAGE = {
  modelScale: 0.48,
  floorCenter: [-656.5, 556.6, 11] as [number, number, number],
  plazaAnchor: [-526.5, 556.6, 11] as [number, number, number],
  plazaCenter: ARABIAN_PLAZA_CENTER,
  floorRadius: Number((ARABIAN_KNOCKOFF - 0.35).toFixed(2)),
  knockoffRadius: Number(ARABIAN_KNOCKOFF.toFixed(2)),
};

export function arabianPlayTransform(): { x: number; y: number; z: number; scale: number } {
  const scale = ARABIAN_STAGE.modelScale;
  const [x, y, z] = ARABIAN_STAGE.plazaAnchor;
  return { x: -scale * x, y: -scale * y, z: -scale * z, scale };
}

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

/**
 * stylized_desert_hoodoo.glb at scale 1: XZ size 0.579 x 0.525, ymin -0.367.
 * Radius is the mean horizontal half-extent (the same measure as the grass rocks).
 * The mesh is centered on the origin, so the group is lifted until the base sits on y=0.
 */
const HOODOO_FOOTPRINT = (0.579 + 0.525) / 4;
const HOODOO_BASE_LIFT = 0.367;

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
      position: [x, HOODOO_BASE_LIFT * scale, z],
      scale,
      rotation: (i * 1.8) % (Math.PI * 2),
      radius: HOODOO_FOOTPRINT * scale,
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

/**
 * Camp props in winter_location.glb that rise through marble height.
 * Crates, the barrel, and rails use the mean of the mesh's X/Z half-extents;
 * those circles sit on the props. The towers and wall-16 do not: each tower
 * box is about 1 unit wider than the stone, and wall-16's face is about 2.5
 * where the box average is 3.7. Skating penguins are omitted. Stacked crates
 * whose bottoms are above the marble are omitted.
 * Off until the winter mesh mounts, so a missing model leaves no camp walls.
 */
const ICE_CAMP: SolidCircle[] = [
  { id: "box-12", x: 12.28, z: -3.39, radius: 0.61, kind: "prop" },
  { id: "box-13", x: 12.8, z: -2.1, radius: 1.05, kind: "prop" },
  { id: "box-14", x: -12.37, z: -3.96, radius: 0.61, kind: "prop" },
  { id: "rail-12", x: 11.67, z: -6.49, radius: 1.19, kind: "prop" },
  { id: "box-15", x: -12.94, z: -5.22, radius: 1.05, kind: "prop" },
  { id: "box-16", x: 14.12, z: -1.02, radius: 1.05, kind: "prop" },
  { id: "barrel-15", x: 15.46, z: -0.3, radius: 0.67, kind: "prop" },
  { id: "box-17", x: -14.32, z: -6.24, radius: 1.05, kind: "prop" },
  { id: "wall-16", x: 10.94, z: 11.5, radius: 2.5, kind: "prop" },
  { id: "rail-16", x: 14.12, z: -8.7, radius: 1.19, kind: "prop" },
  { id: "box-18", x: -15.48, z: -6.89, radius: 0.6, kind: "prop" },
  { id: "tower-17a", x: 14.67, z: 8.25, radius: 1.87, kind: "prop" },
  { id: "tower-17b", x: 7.25, z: 14.99, radius: 1.87, kind: "prop" },
  { id: "tower-19", x: -4.83, z: 19.01, radius: 1.38, kind: "prop" },
];

let winterCampActive = false;

/** True only after winter_location.glb has mounted. A failed load leaves this false. */
export function setWinterCampActive(active: boolean) {
  winterCampActive = active;
}

function iceScenery(): SolidCircle[] {
  return getSnowmanPositions().map((snowman, i) => ({
    id: `snowman-${i}`,
    x: snowman.position[0],
    z: snowman.position[2],
    radius: SNOWMAN_RADIUS,
    kind: "snowman" as const,
  }));
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
    WINTER_STAGE.floorRadius,
    WINTER_STAGE.knockoffRadius,
    iceScenery(),
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
  space: buildLayout("space", COSMOS_STAGE.floorRadius, COSMOS_STAGE.knockoffRadius, [], 8.5, DIAGONAL),
  saturn: buildLayout("saturn", ARABIAN_STAGE.floorRadius, ARABIAN_STAGE.knockoffRadius, [], 8.5, DIAGONAL),
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
  const camp = input.map === "ice" && winterCampActive ? ICE_CAMP : [];
  const scenery = [...(layout?.scenery ?? []), ...camp];
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
