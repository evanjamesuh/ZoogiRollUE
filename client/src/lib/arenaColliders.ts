import { getSnowmanPositions } from "./arenaConstants";
import { arenaScaleFor } from "./arenaScale";
import { leftNeonOpenEdge, neonCourtLayout, neonPlayHalfX, neonPlayHalfZ, neonRimGapAdjustments } from "./neonCourt";
import { ORB_DRAW_RADIUS, ZOOGI_DRAW_RADIUS } from "./restHeight";
import { MARBLE_WIDTH, placeScaledObstacles, type RimGapAdjustment } from "./obstaclePlacement";
import { BUMPER_RESTITUTION, ROCK_RESTITUTION } from "./simFeel";
import {
  ARABIAN_RIM,
  GRASS_RIM,
  ROUND_FLOOR_RADIUS,
  ROUND_KNOCKOFF_RADIUS,
  rimPosition,
  type RimMark,
} from "./roundRim";

/**
 * Solid shapes the marble simulation uses.
 *
 * Meadow's six rim solids are the mossy boulders and stumps drawn on the
 * grass disc. Each radius is that piece's horizontal footprint. The old
 * pinball radius (2) was paired with bumper.glb, which is 0.03 wide at the
 * scale it was drawn, so those hits were invisible.
 */
export const BUMPER_RADIUS = 0.955;
export const BUMPER_MODEL_URL = "/models/bumber1.glb";
/** Hit radius matches the drawn ball, so the mesh rests on the floor and touches when it collides. */
export const MARBLE_RADIUS = ZOOGI_DRAW_RADIUS;
export const ORB_RADIUS = ORB_DRAW_RADIUS;
export const SNOWMAN_RADIUS = 0.55;
export const REST_SPEED = 0.02;

export const GRASS_STAGE = {
  /**
   * The map editor still stores a stage transform for grass. Meadow no longer
   * loads a stage model; the disc is drawn in code.
   */
  modelScale: 48,
  modelOffsetY: -2.4,
  groundRadius: ROUND_FLOOR_RADIUS,
};

/**
 * winter_location.glb. Ground tops sit on y=0 at this offset.
 * Camp, towers, and walls are scenery outside the rink and are not solids.
 */
export const WINTER_STAGE = {
  modelScale: 0.9,
  modelOffsetY: -4.2,
  floorRadius: ROUND_FLOOR_RADIUS,
  knockoffRadius: ROUND_KNOCKOFF_RADIUS,
};

/**
 * cosmos_arena.glb floor (polygon56_Arena_Floor). It is an 18-gon, not a
 * circle: corners every 20°, with a corner on +X, at radius 15.743 around
 * a centre near (+0.250, -0.024). The flats sit at 15.743 * cos(10°), about
 * 15.504. cosmosPlayTransform recentres that polygon and keeps the corners
 * on the 15.6 knockout. The fall check uses the same 18-gon, so a marble
 * leaves at the lip on the flats as well as the corners.
 *
 * polygon30 (Arena Wall Barrier, the curb), polygon67 (Arena Wall), and
 * polygon72 (Crowd Stands, including its reflection) are pushed out until
 * their inner faces clear the farthest fall line by one marble radius.
 * The numbers below are those inner faces after the play transform and
 * before that push, at arena scale 1. The stands' vertices begin near
 * 15.04; the inner edges sit further in, about half a marble inside a flat.
 */
export const COSMOS_FLOOR_MESH = {
  centerX: 0.25045,
  centerY: -0.019,
  centerZ: -0.02365,
  radius: 15.743,
};

export const COSMOS_LIP_SIDES = 18;
/** Inner-face radius of the curb (Arena Wall Barrier) before it is pushed out. */
export const COSMOS_CURB_INNER = 14.544;
/** Inner-face radius of the arena wall before it is pushed out. */
export const COSMOS_WALL_INNER = 14.834;
/** Inner-face radius of the crowd stands before they are pushed out. */
export const COSMOS_STANDS_INNER = 14.885;

export const COSMOS_STAGE = {
  floorRadius: 15.6,
  knockoffRadius: 15.6,
};

/** Radius of a regular polygon at `angle`, with a vertex on +X. */
export function regularPolygonRadius(cornerRadius: number, sides: number, angle: number): number {
  const sector = (Math.PI * 2) / sides;
  const half = sector / 2;
  let wrapped = angle % sector;
  if (wrapped < 0) wrapped += sector;
  const fromVertex = Math.min(wrapped, sector - wrapped);
  return (cornerRadius * Math.cos(half)) / Math.cos(half - fromVertex);
}

/** Scale and shift that put the floor's corners on the knockout and the centre on the origin. */
export function cosmosPlayTransform(): { x: number; y: number; z: number; scale: number } {
  const worldRadius = COSMOS_STAGE.knockoffRadius * arenaScaleFor("space");
  const scale = worldRadius / COSMOS_FLOOR_MESH.radius;
  return {
    x: -scale * COSMOS_FLOOR_MESH.centerX,
    y: -scale * COSMOS_FLOOR_MESH.centerY,
    z: -scale * COSMOS_FLOOR_MESH.centerZ,
    scale,
  };
}

/** World lip of the cosmic floor after cosmosPlayTransform. `radius` is a corner; flats are closer. */
export function cosmosDrawnLip(): {
  centerX: number;
  centerZ: number;
  radius: number;
  radiusAt: (angle: number) => number;
} {
  const placed = cosmosPlayTransform();
  const radiusAt = (angle: number) =>
    placed.scale * regularPolygonRadius(COSMOS_FLOOR_MESH.radius, COSMOS_LIP_SIDES, angle);
  return {
    centerX: placed.x + placed.scale * COSMOS_FLOOR_MESH.centerX,
    centerZ: placed.z + placed.scale * COSMOS_FLOOR_MESH.centerZ,
    radius: radiusAt(0),
    radiusAt,
  };
}

/** Fall line for Cosmic Platform: the same 18-gon the floor lip draws. */
export function cosmosLipRadius(angle: number): number {
  return cosmosDrawnLip().radiusAt(angle);
}

/** "curb" is Arena Wall Barrier. "wall" is Arena Wall, not the barrier. "stands" is Crowd Stands. */
export function cosmosRingRole(name: string): "curb" | "wall" | "stands" | null {
  const normalized = name.toLowerCase().replace(/[_]+/g, " ");
  if (normalized.includes("crowd stands")) return "stands";
  if (normalized.includes("arena wall barrier")) return "curb";
  if (normalized.includes("arena wall")) return "wall";
  return null;
}

/**
 * Radial scale that puts an inner face on the farthest fall line plus one
 * marble radius. 1 means the ring is already clear.
 */
export function cosmosRingPush(innerWorld: number): number {
  const limit = cosmosLipRadius(0) + MARBLE_RADIUS;
  if (!(innerWorld > 0) || innerWorld >= limit) return 1;
  return limit / innerWorld;
}

/**
 * arabian_nights_stage.glb is authored around (-656.5, 556.6, 11). That point
 * is the courtyard. The anchor puts it on the origin and the floor on y=0.
 * The round tiled plaza is drawn in code. Palace, domes, colonnade, and the
 * sunken pool stay outside the knockoff line and are not solids.
 */
export const ARABIAN_STAGE = {
  modelScale: 0.48,
  floorCenter: [-656.5, 556.6, 11] as [number, number, number],
  plazaCenter: [0, 0] as [number, number],
  floorRadius: ROUND_FLOOR_RADIUS,
  knockoffRadius: ROUND_KNOCKOFF_RADIUS,
};

/**
 * Pharaoh's Tomb. No stage model: the sandstone disk is drawn at this radius,
 * and the knockoff line is that same circle.
 */
export const TOMB_STAGE = {
  floorRadius: 15.5,
  knockoffRadius: 15.5,
};

export type TombPieceKind = "block" | "pillar" | "boulder" | "wall" | "brazier";

export interface TombPiece {
  id: string;
  kind: TombPieceKind;
  /** Radians, from +X toward +Z. */
  angle: number;
  distance: number;
  x: number;
  z: number;
  /**
   * Horizontal reach toward the origin. A marble-height solid uses this as its
   * collider radius. Backdrop pieces use it as the distance from the piece
   * centre to the innermost point, so the whole prop stays outside the ring.
   */
  radius: number;
  height: number;
  /** Wall length, or the same as radius for round props. */
  width: number;
}

function tombPiece(
  id: string,
  kind: TombPieceKind,
  angleDeg: number,
  distance: number,
  radius: number,
  height: number,
  width: number,
): TombPiece {
  const angle = (angleDeg * Math.PI) / 180;
  return {
    id,
    kind,
    angle,
    distance,
    radius,
    height,
    width,
    x: Math.cos(angle) * distance,
    z: Math.sin(angle) * distance,
  };
}

/**
 * Rounded sandstone bumpers just inside the rim. Angles sit off the cardinal
 * spawn lanes and off the score-zone disks, with wide gaps so a marble still
 * has an open roll to the edge.
 */
export function getTombBlocks(): TombPiece[] {
  return [
    tombPiece("sandstone-0", "block", 55, 13.4, 1.2, 1.05, 1.2),
    tombPiece("sandstone-1", "block", 150, 13.3, 1.15, 0.98, 1.15),
    tombPiece("sandstone-2", "block", 238, 13.45, 1.2, 1.08, 1.2),
    tombPiece("sandstone-3", "block", 328, 13.2, 1.1, 0.95, 1.1),
  ];
}

/** Walls, pillars, boulders and braziers. All of these sit fully outside the knockoff line and are not solids. */
export function getTombBackdrop(): TombPiece[] {
  const walls = [
    [200, 5.6],
    [252, 3.4],
    [308, 6.2],
    [18, 4.6],
    [68, 5.2],
    [122, 3.8],
    [162, 5.8],
  ].map(([deg, height], i) => tombPiece(`wall-${i}`, "wall", deg, 23.2, 0.7, height, 6.4));
  const pillars = [
    [186, 6.6],
    [236, 4.1],
    [286, 7.0],
    [346, 5.2],
    [42, 6.1],
    [96, 3.5],
    [146, 5.7],
  ].map(([deg, height], i) => tombPiece(`pillar-${i}`, "pillar", deg, 18.95, 0.7, height, 0.7));
  const boulders = [
    tombPiece("boulder-0", "boulder", 214, 20.5, 1.5, 1.65, 1.5),
    tombPiece("boulder-1", "boulder", 268, 20.1, 1.3, 1.4, 1.3),
    tombPiece("boulder-2", "boulder", 332, 20.8, 1.55, 1.75, 1.55),
    tombPiece("boulder-3", "boulder", 12, 20.3, 1.2, 1.25, 1.2),
    tombPiece("boulder-4", "boulder", 84, 21.1, 1.65, 1.85, 1.65),
    tombPiece("boulder-5", "boulder", 138, 20.2, 1.35, 1.45, 1.35),
  ];
  const braziers = [
    tombPiece("brazier-0", "brazier", 90, 17.5, 0.42, 1.15, 0.42),
    tombPiece("brazier-1", "brazier", 270, 17.5, 0.42, 1.15, 0.42),
  ];
  return [...walls, ...pillars, ...boulders, ...braziers];
}

export function arabianPlayTransform(): { x: number; y: number; z: number; scale: number } {
  const scale = ARABIAN_STAGE.modelScale;
  const [x, y, z] = ARABIAN_STAGE.floorCenter;
  return { x: -scale * x, y: -scale * y, z: -scale * z, scale };
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

function rimScenery(marks: RimMark[], kind: SolidKind): SolidCircle[] {
  return marks.map((mark) => {
    const { x, z } = rimPosition(mark);
    return { id: mark.id, x, z, radius: mark.radius, kind };
  });
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

function hoodooUnits(): HoodooDecor[] {
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

/** Hoodoo stones keep their size. Only the ring they stand on grows. */
export function getHoodooDecor(): HoodooDecor[] {
  const placed = new Map((getMapLayout("lava")?.scenery ?? []).map((solid) => [solid.id, solid]));
  return hoodooUnits().map((hoodoo) => {
    const solid = placed.get(hoodoo.id);
    return {
      ...hoodoo,
      position: [solid?.x ?? hoodoo.position[0], hoodoo.position[1], solid?.z ?? hoodoo.position[2]] as [number, number, number],
      radius: solid?.radius ?? hoodoo.radius,
    };
  });
}

export interface IcePatch {
  id: string;
  x: number;
  z: number;
  radius: number;
  rotation: number;
}

function icePatchUnits(): IcePatch[] {
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

/** Drawn ice disks and the slippery patch surface use this one list. */
export function getIcePatches(): IcePatch[] {
  const scale = arenaScaleFor("ice");
  return icePatchUnits().map((patch) => ({
    ...patch,
    x: patch.x * scale,
    z: patch.z * scale,
    radius: patch.radius * scale,
  }));
}

const CARDINAL = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
const DIAGONAL = [Math.PI / 4, (3 * Math.PI) / 4, (5 * Math.PI) / 4, (7 * Math.PI) / 4];

function flatBumpers(distance: number, angles: number[]): { id: string; x: number; z: number }[] {
  return angles.map((angle, i) => polar(`bumper-${i}`, angle, distance));
}

/**
 * Camp, towers, and walls from winter_location.glb sit outside the rink as
 * scenery. They are not solids. The flag still tells the collider overlay
 * that the winter mesh has mounted.
 */
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

/** Collider overlay subscribes when the winter mesh mounts. Camp props are not solids. */
export function subscribeWinterCamp(listener: () => void): () => void {
  winterCampListeners.add(listener);
  return () => winterCampListeners.delete(listener);
}

function iceScenery(): SolidCircle[] {
  return getSnowmanPositions().map((snowman, i) => ({
    id: `snowman-${i}`,
    x: snowman.position[0],
    z: snowman.position[2],
    radius: snowman.radius,
    kind: "snowman" as const,
  }));
}

const ROUND_SPAWN_DISTANCE = 8;
const ROUND_SCORE_DISTANCE = 9.4;
const ROUND_SCORE_ANGLES = [45, 135, 225, 315].map((deg) => (deg * Math.PI) / 180);
const ROUND_BUMPER_DISTANCE = 6.3;
const ROUND_BUMPER_ANGLES = [22, 112, 202, 292].map((deg) => (deg * Math.PI) / 180);

function roundLayout(id: string, scenery: SolidCircle[]): MapLayout {
  return {
    id,
    floorRadius: ROUND_FLOOR_RADIUS,
    knockoffRadius: ROUND_KNOCKOFF_RADIUS,
    orbRingRadius: 4.2,
    scenery,
    bumpers: flatBumpers(ROUND_BUMPER_DISTANCE, ROUND_BUMPER_ANGLES),
    zones: zones(
      CARDINAL.map((angle) => ({ angle, distance: ROUND_SPAWN_DISTANCE })),
      ROUND_SCORE_ANGLES.map((angle) => ({ angle, distance: ROUND_SCORE_DISTANCE })),
    ),
  };
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
  grass: roundLayout("grass", rimScenery(GRASS_RIM, "rock")),
  ice: roundLayout("ice", iceScenery()),
  lava: buildLayout(
    "lava",
    18,
    18.6,
    hoodooUnits().map((hoodoo) => ({
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
  saturn: roundLayout("saturn", rimScenery(ARABIAN_RIM, "prop")),
  tomb: buildLayout(
    "tomb",
    TOMB_STAGE.floorRadius,
    TOMB_STAGE.knockoffRadius,
    getTombBlocks().map((block) => ({
      id: block.id,
      x: block.x,
      z: block.z,
      radius: block.radius,
      kind: "rock" as const,
    })),
    6.6,
    DIAGONAL,
  ),
};

/**
 * Spread a layout written in today's units. Obstacle and bumper radii stay
 * put so the larger floor has more open space. Positions, the floor, the
 * knockoff line, spawns, and score zones all grow. Circles that belong to
 * one mesh keep their offsets, and a rim slot a marble almost fits through
 * is nudged shut or open.
 */
export function scaleLayout(layout: MapLayout, scale: number): MapLayout {
  const knockoffRadius = layout.knockoffRadius * scale;
  const scenery = placeScaledObstacles(layout.scenery, scale, knockoffRadius, MARBLE_RADIUS * 2).circles;
  return {
    ...layout,
    floorRadius: layout.floorRadius * scale,
    knockoffRadius,
    orbRingRadius: layout.orbRingRadius * scale,
    zones: layout.zones.map((zone) => ({ ...zone, distance: zone.distance * scale })),
    scenery,
    bumpers: layout.bumpers.map((bumper) => ({ ...bumper, x: bumper.x * scale, z: bumper.z * scale })),
  };
}

export type { RimGapAdjustment };

/** Every read-time rim nudge, including camp props that appear with the winter mesh. */
export function listRimGapAdjustments(): Array<RimGapAdjustment & { mapId: string }> {
  const rows: Array<RimGapAdjustment & { mapId: string }> = [];
  const marble = MARBLE_RADIUS * 2;
  if (Math.abs(marble - MARBLE_WIDTH) > 1e-9) {
    throw new Error(`marble width ${marble} drifted from obstacle placement ${MARBLE_WIDTH}`);
  }
  for (const mapId of Object.keys(LAYOUTS)) {
    const layout = LAYOUTS[mapId];
    const scale = arenaScaleFor(mapId);
    const placed = placeScaledObstacles(layout.scenery, scale, layout.knockoffRadius * scale, marble);
    for (const adjustment of placed.adjustments) rows.push({ mapId, ...adjustment });
  }
  for (const adjustment of neonRimGapAdjustments()) rows.push({ mapId: "neon", ...adjustment });
  return rows;
}

/** Collider meshes inside this map's dress group, and the world nudge applied to each. */
/**
 * Code-drawn obstacles (rocks, snowmen, planters, hoodoos, tomb blocks, neon
 * posts) sit outside any dress group and already use the scaled layout.
 * Nothing embedded in a stage model is a collider, so there is no counter-scale.
 */
export function embeddedObstaclePose(_mapId: string): { meshKeys: string[]; nudges: { meshKey: string; dx: number; dz: number }[] } {
  return { meshKeys: [], nudges: [] };
}

/** Floor disk and knockout ring the arena draws. Both match the colliders. */
export function arenaVisualEdge(mapId: string):
  | { shape: "circle"; floorRadius: number; knockoffRadius: number }
  | { shape: "rect"; halfX: number; halfZ: number } {
  if (mapId === "neon") return { shape: "rect", halfX: neonPlayHalfX(), halfZ: neonPlayHalfZ() };
  const layout = getMapLayout(mapId);
  return {
    shape: "circle",
    floorRadius: layout?.floorRadius ?? 0,
    knockoffRadius: layout?.knockoffRadius ?? 0,
  };
}

export function getMapLayout(mapId: string | null | undefined): MapLayout | null {
  if (!mapId) return null;
  if (mapId === "neon") return neonCourtLayout();
  const layout = LAYOUTS[mapId];
  if (!layout) return null;
  return scaleLayout(layout, arenaScaleFor(mapId));
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
  // Camp, towers, and walls are scenery outside the rink and are not solids.
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
  restitution = ROCK_RESTITUTION,
  keepInsideRadius?: number,
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
      let nx = dx / dist;
      let nz = dz / dist;
      if (keepInsideRadius !== undefined) {
        const solidDist = Math.hypot(solid.x, solid.z);
        const facesOut = nx * solid.x + nz * solid.z > 0;
        const onRim = solidDist + solid.radius >= keepInsideRadius - entityRadius - 0.05;
        if (facesOut && onRim && solidDist > 1e-6) {
          nx = -solid.x / solidDist;
          nz = -solid.z / solidDist;
        }
      }
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

/**
 * True only when the body is past the knockout line on an angle the rim
 * solids do not cover. Overlapping a meadow rock, or sitting on its outer
 * face, is not the open edge.
 */
export function centerPastOpenEdge(
  mapId: string | null | undefined,
  x: number,
  z: number,
  knockoffRadius: number,
  bodyRadius: number,
  solids: SolidCircle[],
): boolean {
  if (mapId === "neon") return leftNeonOpenEdge(x, z, bodyRadius);
  const angle = Math.atan2(z, x);
  const lipCorner = mapId === "space" ? cosmosLipRadius(0) : 0;
  const fallRadius = mapId === "space" && lipCorner > 0
    ? cosmosLipRadius(angle) * (knockoffRadius / lipCorner)
    : knockoffRadius;
  // A hair of float must not drop a marble that is sitting on the lip.
  if (Math.hypot(x, z) <= fallRadius + 1e-6) return false;
  const walls = solids.filter((solid) => solid.kind !== "bumper");
  if (pointInsideSolid(x, z, walls, bodyRadius)) return false;
  const rimX = Math.cos(angle) * fallRadius;
  const rimZ = Math.sin(angle) * fallRadius;
  for (const solid of walls) {
    const rimClearance = Math.hypot(rimX - solid.x, rimZ - solid.z) - solid.radius;
    if (rimClearance >= bodyRadius) continue;
    const bodyGap = Math.hypot(x - solid.x, z - solid.z) - solid.radius;
    // Still against the wall. Far past it, the body has left the court.
    if (bodyGap < bodyRadius) return false;
  }
  return true;
}

export function pointInsideSolid(x: number, z: number, solids: SolidCircle[], padding = 0): SolidCircle | null {
  for (const solid of solids) {
    if (Math.hypot(x - solid.x, z - solid.z) < solid.radius + padding) return solid;
  }
  return null;
}
