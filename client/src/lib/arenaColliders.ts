import { getSnowmanPositions } from "./arenaConstants";
import { neonCourtLayout } from "./neonCourt";
import { BUMPER_RESTITUTION } from "./simFeel";

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
 * cosmos_arena.glb. Scale 1.08. The playable floor ends at the inner face of
 * the lip wall (about 15.6). That wall is drawn only, so the knockoff line is
 * the inner face: a marble is out where the floor visibly ends, not after it
 * has rolled through the wall. Floor mesh is at local y=-0.02.
 */
export const COSMOS_STAGE = {
  modelScale: 1.08,
  modelOffsetY: 0.02 * 1.08,
  floorRadius: 15.2,
  knockoffRadius: 15.6,
};

/**
 * arabian_nights_stage.glb is authored around (-656.5, 556.6, 11). The anchor
 * puts one courtyard point at the origin and the floor on y=0. The clean plaza
 * (about radius 15.5) sat near (-4, -16). Shifting the stage +4 x and +16 z
 * lands that circle on the origin, so the standard spawns, bumpers, and score
 * zones stay centred on flat plaza inside the knockoff line.
 */
const ARABIAN_PLAZA_SHIFT: [number, number] = [4, 16];

export const ARABIAN_STAGE = {
  modelScale: 0.48,
  floorCenter: [-656.5, 556.6, 11] as [number, number, number],
  plazaAnchor: [-526.5, 556.6, 11] as [number, number, number],
  plazaShift: ARABIAN_PLAZA_SHIFT,
  plazaCenter: [0, 0] as [number, number],
  floorRadius: 15.15,
  knockoffRadius: 15.5,
};

export function arabianPlayTransform(): { x: number; y: number; z: number; scale: number } {
  const scale = ARABIAN_STAGE.modelScale;
  const [x, y, z] = ARABIAN_STAGE.plazaAnchor;
  const [shiftX, shiftZ] = ARABIAN_STAGE.plazaShift;
  return { x: -scale * x + shiftX, y: -scale * y, z: -scale * z + shiftZ, scale };
}

/** Every map's out-line is centred on the origin. Callers must apply this on each map so a previous map cannot leave a leftover shift. */
export function knockoffOffsetForMap(_mapId: string | null | undefined): { x: number; y: number; z: number } {
  return { x: 0, y: 0, z: 0 };
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

/**
 * Rim rocks and trees on floating_island_stage.glb at GRASS_STAGE.modelScale.
 * Each big rock is split along its long axis so the circle follows the stone
 * instead of a single disk that was mostly empty air. Tree1's leaves sit above
 * marble height; the collider is the trunk.
 */
function grassScenery(): SolidCircle[] {
  const scale = GRASS_STAGE.modelScale;
  const tree = (id: string, x: number, z: number, radius: number): SolidCircle => ({
    id,
    x: x * scale,
    z: z * scale,
    radius: radius * scale,
    kind: "bush",
  });
  return [
    { id: "S_7_rock_a", x: 4.7, z: 13.2, radius: 2.9, kind: "rock" },
    { id: "S_7_rock_b", x: 9.6, z: 11.2, radius: 2.5, kind: "rock" },
    { id: "S_6_rock_a", x: -13.7, z: 2.7, radius: 2.8, kind: "rock" },
    { id: "S_6_rock_b", x: -12.1, z: 7.8, radius: 2.5, kind: "rock" },
    { id: "S_5_rock_a", x: -10.5, z: -9.9, radius: 2.5, kind: "rock" },
    { id: "S_5_rock_b", x: -12.8, z: -6.4, radius: 2.3, kind: "rock" },
    { id: "S_8_rock_a", x: 14.0, z: -3.0, radius: 1.75, kind: "rock" },
    { id: "S_8_rock_b", x: 14.0, z: 0.1, radius: 1.75, kind: "rock" },
    { id: "S_8_rock_c", x: 14.0, z: 2.9, radius: 1.45, kind: "rock" },
    { id: "S_4_rock_a", x: 9.6, z: -9.9, radius: 2.0, kind: "rock" },
    { id: "S_4_rock_b", x: 11.5, z: -7.4, radius: 2.0, kind: "rock" },
    { id: "S_1_rock_a", x: -3.2, z: -13.9, radius: 1.6, kind: "rock" },
    { id: "S_1_rock_b", x: -0.7, z: -13.9, radius: 1.6, kind: "rock" },
    { id: "S_1_rock_c", x: 1.8, z: -13.9, radius: 1.6, kind: "rock" },
    { id: "Tree1_Leavs_0", x: 6.6, z: -9.6, radius: 0.35, kind: "bush" },
    tree("Tree2_Leavs_0", -0.03, 0.2, 0.0385),
    tree("Tree3_Leavs_0", -0.2, -0.07, 0.0248),
    tree("Tree8_Leavs_0", 0.16, 0.1, 0.0193),
  ];
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
 * stylized_desert_hoodoo.glb at scale 1: full XZ size 0.579 x 0.525, ymin -0.367.
 * At marble height the stone is about 0.51 across (half-extent 0.25). The old
 * mean of the bounding box (0.276) left an empty rim around the rock.
 * The mesh is centered on the origin, so the group is lifted until the base sits on y=0.
 */
const HOODOO_FOOTPRINT = 0.25;
const HOODOO_BASE_LIFT = 0.367;

export function getHoodooDecor(): HoodooDecor[] {
  const decor: HoodooDecor[] = [];
  const count = 6;
  for (let i = 0; i < count; i++) {
    const angle = (i / count) * Math.PI * 2 + 0.3;
    const distance = 10 + (i % 2) * 4;
    const x = Math.cos(angle) * distance;
    const z = Math.sin(angle) * distance;
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

/** Drawn ice disks and the slippery patch surface use this one list. */
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
 * Crates, the barrel, and rails use the mean of the mesh's X/Z half-extents.
 * The three rink towers are about 4.0 across at marble height, so the circle
 * is 2.05. wall-16 is a diagonal bar about 2.2 wide; three circles follow it
 * instead of one disk around the bar's bounding box. Skating penguins are
 * omitted. Stacked crates whose bottoms are above the marble are omitted.
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
  { id: "wall-16a", x: 10.94, z: 11.5, radius: 1.15, kind: "prop" },
  { id: "wall-16b", x: 12.64, z: 9.95, radius: 1.15, kind: "prop" },
  { id: "wall-16c", x: 9.24, z: 13.05, radius: 1.15, kind: "prop" },
  { id: "rail-16", x: 14.12, z: -8.7, radius: 1.19, kind: "prop" },
  { id: "box-18", x: -15.48, z: -6.89, radius: 0.6, kind: "prop" },
  { id: "tower-17a", x: 14.67, z: 8.25, radius: 2.05, kind: "prop" },
  { id: "tower-17b", x: 7.25, z: 14.99, radius: 2.05, kind: "prop" },
  { id: "tower-19", x: -4.83, z: 19.01, radius: 2.05, kind: "prop" },
];

let winterCampActive = false;
let winterCampVersion = 0;
const winterCampListeners = new Set<() => void>();

/** True only after winter_location.glb has mounted. A failed load leaves this false. */
export function setWinterCampActive(active: boolean) {
  if (winterCampActive === active) return;
  winterCampActive = active;
  winterCampVersion += 1;
  winterCampListeners.forEach((listener) => listener());
}

export function getWinterCampVersion(): number {
  return winterCampVersion;
}

/** Collider overlay subscribes so camp solids appear when the winter mesh mounts. */
export function subscribeWinterCamp(listener: () => void): () => void {
  winterCampListeners.add(listener);
  return () => winterCampListeners.delete(listener);
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
    // Grass edge is about 13.2–15 (typically 13.5). 15.5 is just past the outer
    // lobes, so a marble still on grass is in, and leaving the island is out
    // without the old flight across empty air out to 19.2.
    knockoffRadius: 15.5,
    orbRingRadius: 4.2,
    scenery: grassScenery(),
    bumpers: flatBumpers(6.4, DIAGONAL),
    zones: zones(
      // South spawn (105°) sits in front of the big tree canopy, which covers
      // about x -5.7..2.5 and z 7.7..15.9. The four outward-lane counts match.
      [
        { angle: (350 * Math.PI) / 180, distance: 8 },
        { angle: (105 * Math.PI) / 180, distance: 7 },
        { angle: (190 * Math.PI) / 180, distance: 8 },
        { angle: (270 * Math.PI) / 180, distance: 8.2 },
      ],
      // Radius-4 zones at 9.4 end near 13.4, on the grass (edge ~13.5).
      [45, 150, 230, 315].map((deg) => ({ angle: (deg * Math.PI) / 180, distance: 9.4 })),
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
  if (mapId === "neon") return neonCourtLayout();
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
      const bounce = solid.kind === "bumper" ? Math.max(restitution, BUMPER_RESTITUTION) : restitution;
      if (dot < 0) {
        vx = (vx - 2 * dot * nx) * bounce;
        vz = (vz - 2 * dot * nz) * bounce;
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
