/**
 * Night Circuit, an original rectangular court.
 *
 * Closed neon rails bounce marbles. The four corners are open mouths, and the
 * glowing lip just outside the floor is the knockout line. Nothing here is
 * copied from another game's layout, marks, or meshes.
 */

import { BUMPER_RADIUS, MARBLE_RADIUS, type MapLayout, type ZonePlacement } from "./arenaColliders";
import { arenaScaleFor } from "./arenaScale";
import { MARBLE_WIDTH, RIM_GAP_FLUSH, RIM_GAP_LANE, rimGapInBand, rimGapWidths, snapRimGap, type RimGapAdjustment } from "./obstaclePlacement";
import { MAX_PLANAR_SPEED, RAIL_RESTITUTION } from "./simFeel";

/** Authored court, in today's units. Readers multiply by the neon arenaScale. */
export const NEON_HALF_X = 12;
export const NEON_HALF_Z = 8;
export const NEON_RAIL_DEPTH = 0.7;
/** How far each rail stops short of a corner, leaving a knockout mouth. */
export const NEON_CORNER_GAP = 3.15;

function courtScale(): number {
  return arenaScaleFor("neon");
}

/** Knockoff rectangle in world units. Rail thickness is not included. */
export function neonPlayHalfX(): number {
  return NEON_HALF_X * courtScale();
}

export function neonPlayHalfZ(): number {
  return NEON_HALF_Z * courtScale();
}

export interface NeonRail {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  /** Emissive hex used by the mesh. Not a collider. */
  color: string;
}

/** Corner-mouth width in world units, snapped out of the awkward rim-gap band. */
export function neonMouthWidth(): number {
  return snapRimGap(NEON_CORNER_GAP * courtScale(), MARBLE_WIDTH);
}

export function neonRails(): NeonRail[] {
  const scale = courtScale();
  const halfX = NEON_HALF_X * scale;
  const halfZ = NEON_HALF_Z * scale;
  const mouth = neonMouthWidth();
  const gapX = halfX - mouth;
  const gapZ = halfZ - mouth;
  const depth = NEON_RAIL_DEPTH;
  return [
    {
      id: "rail-north",
      minX: -gapX,
      maxX: gapX,
      minZ: halfZ - depth,
      maxZ: halfZ,
      color: "#b026ff",
    },
    {
      id: "rail-south",
      minX: -gapX,
      maxX: gapX,
      minZ: -halfZ,
      maxZ: -halfZ + depth,
      color: "#b026ff",
    },
    {
      id: "rail-east",
      minX: halfX - depth,
      maxX: halfX,
      minZ: -gapZ,
      maxZ: gapZ,
      color: "#ff2bd6",
    },
    {
      id: "rail-west",
      minX: -halfX,
      maxX: -halfX + depth,
      minZ: -gapZ,
      maxZ: gapZ,
      color: "#22e7ff",
    },
  ];
}

export interface NeonBumper {
  id: string;
  x: number;
  z: number;
}

/**
 * Posts and taller pylons. All of them use the shared bumper circle,
 * so a marble bounces at BUMPER_RADIUS. They stay off the corner mouths.
 * A slot against a pad that a marble almost fits is closed or opened
 * after the pads have taken their lane.
 */
const NEON_BUMPER_UNITS: NeonBumper[] = [
  { id: "neon-post-a", x: -5.1, z: 2.7 },
  { id: "neon-post-b", x: 4.2, z: 3.4 },
  { id: "neon-post-c", x: -3.4, z: -4.6 },
  { id: "neon-pylon-west", x: -6.6, z: 5.6 },
  { id: "neon-pylon-east", x: 5.8, z: -2.6 },
];

const NEON_SPAWN_UNITS = [
  { id: "spawn-0", x: -6.4, z: -1.1 },
  { id: "spawn-1", x: 6.6, z: 1.05 },
  { id: "spawn-2", x: -1.15, z: 4.15 },
  { id: "spawn-3", x: 1.2, z: -4.05 },
];

function neonSpawnPoints(): Array<{ id: string; x: number; z: number }> {
  const scale = courtScale();
  return NEON_SPAWN_UNITS.map((spawn) => ({ id: spawn.id, x: spawn.x * scale, z: spawn.z * scale }));
}

/** Bumper posts in the floor texture's units. The texture group is scaled with the court. */
export function neonBumperMarks(): NeonBumper[] {
  const scale = courtScale();
  return neonBumpers().map((bumper) => ({ ...bumper, x: bumper.x / scale, z: bumper.z / scale }));
}

export function neonBumpers(): NeonBumper[] {
  return placedNeon().bumpers;
}

export interface NeonBox {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
}

/**
 * Raised pads near the ends and a broken center channel.
 * Axis-aligned, so they bounce with the same rail sweep. The corner
 * mouths and the south-center lane stay clear.
 */
function neonObstacleUnits(): NeonBox[] {
  return [
    { id: "pad-west", minX: -9.15, maxX: -7.45, minZ: -1.55, maxZ: 1.55 },
    { id: "pad-east", minX: 7.7, maxX: 9.2, minZ: -1.55, maxZ: 1.55 },
    { id: "pad-north", minX: -2.4, maxX: 0.4, minZ: 5.55, maxZ: 6.7 },
    { id: "pad-south", minX: 1.6, maxX: 3.8, minZ: -6.7, maxZ: -5.55 },
    { id: "channel-north", minX: -2.35, maxX: 2.35, minZ: 0.72, maxZ: 1.08 },
    { id: "channel-south", minX: -2.35, maxX: 2.35, minZ: -1.08, maxZ: -0.72 },
  ];
}

function scaledObstacleBoxes(): NeonBox[] {
  const scale = courtScale();
  return neonObstacleUnits().map((box) => {
    const cx = ((box.minX + box.maxX) / 2) * scale;
    const cz = ((box.minZ + box.maxZ) / 2) * scale;
    const hx = (box.maxX - box.minX) / 2;
    const hz = (box.maxZ - box.minZ) / 2;
    return { ...box, minX: cx - hx, maxX: cx + hx, minZ: cz - hz, maxZ: cz + hz };
  });
}

function railInnerHalf(): { x: number; z: number } {
  return {
    x: neonPlayHalfX() - NEON_RAIL_DEPTH,
    z: neonPlayHalfZ() - NEON_RAIL_DEPTH,
  };
}

/** Gap from a box to the rail's inner face. That face is the edge a marble hits. */
export function neonRailFaceGap(box: NeonBox): number {
  const inner = railInnerHalf();
  return Math.min(
    box.minX + inner.x,
    inner.x - box.maxX,
    box.minZ + inner.z,
    inner.z - box.maxZ,
  );
}

function nearestRailFace(box: NeonBox): { gap: number; dx: number; dz: number } {
  const inner = railInnerHalf();
  const faces = [
    { gap: box.minX + inner.x, dx: -1, dz: 0 },
    { gap: inner.x - box.maxX, dx: 1, dz: 0 },
    { gap: box.minZ + inner.z, dx: 0, dz: -1 },
    { gap: inner.z - box.maxZ, dx: 0, dz: 1 },
  ];
  faces.sort((a, b) => a.gap - b.gap);
  return faces[0];
}

/**
 * A lane of 1.8 from the rail can land on a spawn. Slide along the rail,
 * the smaller way, until the marble has room.
 */
function clearNeonSpawns(box: NeonBox): number {
  const need = MARBLE_WIDTH / 2 + 0.35 + 0.02;
  let dx = 0;
  const spawns = neonSpawnPoints();
  for (let pass = 0; pass < 4; pass++) {
    for (const spawn of spawns) {
      const x = spawn.x;
      const z = spawn.z;
      const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
      const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
      const gz = z - nearestZ;
      const gap = Math.hypot(x - nearestX, gz);
      if (gap >= need - 1e-4) continue;
      const zAbs = Math.abs(gz);
      const xNeed = zAbs >= need ? 0 : Math.sqrt(Math.max(0, need * need - zAbs * zAbs));
      const moveRight = x + xNeed - box.minX;
      const moveLeft = x - xNeed - box.maxX;
      const rightOk = moveRight > 1e-6;
      const leftOk = moveLeft < -1e-6;
      let shift = 0;
      if (rightOk && leftOk) shift = Math.abs(moveRight) <= Math.abs(moveLeft) ? moveRight : moveLeft;
      else if (rightOk) shift = moveRight;
      else if (leftOk) shift = moveLeft;
      else continue;
      box.minX += shift;
      box.maxX += shift;
      dx += shift;
    }
  }
  return dx;
}

/**
 * Pads are measured against the rail's inner face, not the knockout line
 * outside the rail. A slot in the awkward band opens into a 1.8 lane.
 * Flushing onto the rail would leave a marble overlapping both solids.
 */
function placeNeonObstacles(): { boxes: NeonBox[]; adjustments: RimGapAdjustment[] } {
  const boxes = scaledObstacleBoxes();
  const adjustments: RimGapAdjustment[] = [];
  const lane = RIM_GAP_LANE * MARBLE_WIDTH;
  for (const box of boxes) {
    const nearest = nearestRailFace(box);
    if (!rimGapInBand(nearest.gap, MARBLE_WIDTH)) continue;
    const before = nearest.gap;
    const shift = nearest.gap - lane;
    box.minX += nearest.dx * shift;
    box.maxX += nearest.dx * shift;
    box.minZ += nearest.dz * shift;
    box.maxZ += nearest.dz * shift;
    const after = nearestRailFace(box).gap;
    adjustments.push({
      meshKey: box.id,
      ids: [box.id],
      kind: "edge",
      direction: shift > 0 ? "outward" : "inward",
      dx: nearest.dx * shift,
      dz: nearest.dz * shift,
      distance: Math.abs(shift),
      clearances: [{ id: `${box.id}-rail`, before: rimGapWidths(before), after: rimGapWidths(after) }],
    });
    const slide = clearNeonSpawns(box);
    if (Math.abs(slide) > 1e-6) {
      adjustments.push({
        meshKey: box.id,
        ids: [box.id],
        kind: "edge",
        direction: slide > 0 ? "outward" : "inward",
        dx: slide,
        dz: 0,
        distance: Math.abs(slide),
        clearances: [{ id: `${box.id}-spawn`, before: rimGapWidths(after), after: rimGapWidths(neonRailFaceGap(box)) }],
      });
    }
  }
  return { boxes, adjustments };
}

/** Gap between two boxes. Overlap comes back negative. */
export function neonAabbGap(a: NeonBox, b: NeonBox): number {
  const dx = Math.max(a.minX - b.maxX, b.minX - a.maxX, 0);
  const dz = Math.max(a.minZ - b.maxZ, b.minZ - a.maxZ, 0);
  if (dx === 0 && dz === 0) {
    const overlapX = Math.min(a.maxX, b.maxX) - Math.max(a.minX, b.minX);
    const overlapZ = Math.min(a.maxZ, b.maxZ) - Math.max(a.minZ, b.minZ);
    return -Math.min(overlapX, overlapZ);
  }
  return Math.hypot(dx, dz);
}

/** Gap from a bumper's circle to the nearest point on a box. */
export function neonBumperBoxGap(box: NeonBox, bumper: NeonBumper, radius = BUMPER_RADIUS): number {
  const nearestX = Math.min(box.maxX, Math.max(box.minX, bumper.x));
  const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, bumper.z));
  return Math.hypot(bumper.x - nearestX, bumper.z - nearestZ) - radius;
}

function bumperBeside(box: NeonBox, x: number, z: number, radius: number, targetGap: number): { x: number; z: number } {
  const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
  const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
  let dx = x - nearestX;
  let dz = z - nearestZ;
  let dist = Math.hypot(dx, dz);
  if (dist < 1e-6) {
    const cx = (box.minX + box.maxX) / 2;
    const cz = (box.minZ + box.maxZ) / 2;
    dx = x - cx;
    dz = z - cz;
    dist = Math.hypot(dx, dz);
    if (dist < 1e-6) {
      dx = 1;
      dz = 0;
      dist = 1;
    }
  }
  const targetDist = targetGap + radius;
  return {
    x: nearestX + (dx / dist) * targetDist,
    z: nearestZ + (dz / dist) * targetDist,
  };
}

function bumperSitsClear(x: number, z: number, boxes: NeonBox[]): boolean {
  if (Math.abs(x) + BUMPER_RADIUS >= neonPlayHalfX() - 1) return false;
  if (Math.abs(z) + BUMPER_RADIUS >= neonPlayHalfZ() - 1) return false;
  const spawnNeed = BUMPER_RADIUS + MARBLE_RADIUS + 0.3;
  for (const spawn of neonSpawnPoints()) {
    if (Math.hypot(x - spawn.x, z - spawn.z) <= spawnNeed) return false;
  }
  for (const box of boxes) {
    if (neonBumperBoxGap(box, { id: "", x, z }) < 0) return false;
  }
  return true;
}

/**
 * A pad that leaves a marble almost, but not quite, able to pass a post
 * is closed to 0.3 or opened to 1.8. Pads stay where the rail lane put
 * them; the post moves. When one post is boxed in by two pads, the side
 * that clears every slot wins.
 */
function placeNeonBumpers(boxes: NeonBox[]): { bumpers: NeonBumper[]; adjustments: RimGapAdjustment[] } {
  const scale = courtScale();
  const bumpers = NEON_BUMPER_UNITS.map((bumper) => ({ ...bumper, x: bumper.x * scale, z: bumper.z * scale }));
  const adjustments: RimGapAdjustment[] = [];
  const bandCount = (x: number, z: number) =>
    boxes.reduce((count, box) => count + (rimGapInBand(neonBumperBoxGap(box, { id: "", x, z })) ? 1 : 0), 0);

  for (let pass = 0; pass < 8; pass++) {
    let moved = false;
    for (const bumper of bumpers) {
      const stuck = bandCount(bumper.x, bumper.z);
      if (stuck === 0) continue;
      let best: { x: number; z: number; boxId: string; before: number; after: number; shift: number } | null = null;
      for (const box of boxes) {
        const before = neonBumperBoxGap(box, bumper);
        if (!rimGapInBand(before)) continue;
        const preferred = snapRimGap(before);
        const alternate = Math.abs(preferred - RIM_GAP_FLUSH * MARBLE_WIDTH) < 1e-6 ? RIM_GAP_LANE * MARBLE_WIDTH : RIM_GAP_FLUSH * MARBLE_WIDTH;
        for (const target of [preferred, alternate]) {
          const next = bumperBeside(box, bumper.x, bumper.z, BUMPER_RADIUS, target);
          if (!bumperSitsClear(next.x, next.z, boxes)) continue;
          const remaining = bandCount(next.x, next.z);
          if (remaining >= stuck) continue;
          const shift = Math.hypot(next.x - bumper.x, next.z - bumper.z);
          if (!best || remaining < bandCount(best.x, best.z) || (remaining === bandCount(best.x, best.z) && shift < best.shift)) {
            best = { x: next.x, z: next.z, boxId: box.id, before, after: target, shift };
          }
        }
      }
      if (!best) continue;
      adjustments.push({
        meshKey: bumper.id,
        ids: [bumper.id],
        kind: "mouth",
        direction: best.after > best.before ? "opened" : "closed",
        dx: best.x - bumper.x,
        dz: best.z - bumper.z,
        distance: best.shift,
        clearances: [{ id: `${bumper.id}-${best.boxId}`, before: rimGapWidths(best.before), after: rimGapWidths(best.after) }],
      });
      bumper.x = best.x;
      bumper.z = best.z;
      moved = true;
    }
    if (!moved) break;
  }
  return { bumpers, adjustments };
}

function placedNeon(): { boxes: NeonBox[]; bumpers: NeonBumper[]; adjustments: RimGapAdjustment[] } {
  const obstacles = placeNeonObstacles();
  const bumpers = placeNeonBumpers(obstacles.boxes);
  return {
    boxes: obstacles.boxes,
    bumpers: bumpers.bumpers,
    adjustments: [...obstacles.adjustments, ...bumpers.adjustments],
  };
}

/** Pad centers move out with the court. The boxes themselves stay the same size. */
export function neonObstacles(): NeonBox[] {
  return placedNeon().boxes;
}

export function neonRimGapAdjustments(): RimGapAdjustment[] {
  const adjustments = placedNeon().adjustments;
  const authoredMouth = NEON_CORNER_GAP * courtScale();
  const mouth = neonMouthWidth();
  if (Math.abs(mouth - authoredMouth) > 1e-6) {
    adjustments.push({
      meshKey: "corner-mouth",
      ids: ["corner-mouth"],
      kind: "mouth",
      direction: mouth > authoredMouth ? "opened" : "closed",
      dx: 0,
      dz: 0,
      distance: Math.abs(mouth - authoredMouth),
      clearances: [{ id: "corner-mouth", before: authoredMouth / MARBLE_WIDTH, after: mouth / MARBLE_WIDTH }],
    });
  }
  return adjustments;
}

function zoneAt(id: string, x: number, z: number, isSpawn: boolean): ZonePlacement {
  return {
    id,
    angle: Math.atan2(z, x),
    distance: Math.hypot(x, z),
    visible: !isSpawn,
    isSpawn,
  };
}

export function neonCourtLayout(): MapLayout {
  const scale = courtScale();
  return {
    id: "neon",
    floorRadius: NEON_HALF_Z * scale,
    // Stored so other systems have a number. The live check is the rectangle.
    knockoffRadius: (NEON_HALF_X + 0.35) * scale,
    orbRingRadius: 3.15 * scale,
    scenery: [],
    bumpers: neonBumpers(),
    zones: [
      ...neonSpawnPoints().map((spawn) => zoneAt(spawn.id, spawn.x, spawn.z, true)),
      zoneAt("score-0", 7.6 * scale, 4.55 * scale, false),
      zoneAt("score-1", -7.7 * scale, 4.4 * scale, false),
      zoneAt("score-2", 7.8 * scale, -4.35 * scale, false),
      zoneAt("score-3", -7.5 * scale, -4.5 * scale, false),
    ],
  };
}

/** True when a marble center has left the floor rectangle, including through a corner mouth. */
export function isOutsideNeonCourt(x: number, z: number): boolean {
  return Math.abs(x) > neonPlayHalfX() || Math.abs(z) > neonPlayHalfZ();
}

function overlapsBox(x: number, z: number, radius: number, box: NeonBox): boolean {
  const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
  const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
  return Math.hypot(x - nearestX, z - nearestZ) < radius;
}

/**
 * Knockout is the corner mouth. A center that has crossed the rectangle but
 * is still touching a rail or pad has not reached the open edge.
 */
export function leftNeonOpenEdge(x: number, z: number, radius: number): boolean {
  if (!isOutsideNeonCourt(x, z)) return false;
  for (const box of [...neonRails(), ...neonObstacles()]) {
    if (overlapsBox(x, z, radius, box)) return false;
  }
  return true;
}

/** Inward normal for a rail that sits on the court boundary. Interior pads return null. */
function boundaryInward(rail: NeonBox): { x: number; z: number } | null {
  const halfX = neonPlayHalfX();
  const halfZ = neonPlayHalfZ();
  const eps = 0.05;
  if (Math.abs(rail.maxZ - halfZ) < eps && rail.minZ > 0) return { x: 0, z: -1 };
  if (Math.abs(rail.minZ + halfZ) < eps && rail.maxZ < 0) return { x: 0, z: 1 };
  if (Math.abs(rail.maxX - halfX) < eps && rail.minX > 0) return { x: -1, z: 0 };
  if (Math.abs(rail.minX + halfX) < eps && rail.maxX < 0) return { x: 1, z: 0 };
  return null;
}

export interface RailHit {
  id: string;
  normal: [number, number, number];
  point: [number, number, number];
}

function segmentHitsAabb(
  x0: number,
  z0: number,
  x1: number,
  z1: number,
  minX: number,
  maxX: number,
  minZ: number,
  maxZ: number,
): { t: number; nx: number; nz: number } | null {
  const dx = x1 - x0;
  const dz = z1 - z0;
  let tmin = 0;
  let tmax = 1;
  let nx = 0;
  let nz = 0;
  let hitNormalX = 0;
  let hitNormalZ = 0;

  const slabs: Array<[number, number, number, number, "x" | "z"]> = [
    [x0, dx, minX, maxX, "x"],
    [z0, dz, minZ, maxZ, "z"],
  ];

  for (const [p, d, min, max, axis] of slabs) {
    if (Math.abs(d) < 1e-8) {
      if (p < min || p > max) return null;
      continue;
    }
    let t1 = (min - p) / d;
    let t2 = (max - p) / d;
    // t1 becomes the entry time. enterIsMin means that entry is the min face,
    // whose outward normal points toward -axis.
    let enterIsMin = true;
    if (t1 > t2) {
      const swap = t1;
      t1 = t2;
      t2 = swap;
      enterIsMin = false;
    }
    if (t1 > tmin) {
      tmin = t1;
      const outward = enterIsMin ? -1 : 1;
      if (axis === "x") {
        hitNormalX = outward;
        hitNormalZ = 0;
      } else {
        hitNormalX = 0;
        hitNormalZ = outward;
      }
    }
    tmax = Math.min(tmax, t2);
    if (tmin > tmax) return null;
  }

  if (tmax < 0 || tmin > 1) return null;
  nx = hitNormalX;
  nz = hitNormalZ;
  if (nx === 0 && nz === 0) {
    // Started inside. Push out along the shallowest overlap.
    const left = x0 - minX;
    const right = maxX - x0;
    const down = z0 - minZ;
    const up = maxZ - z0;
    const pen = Math.min(left, right, down, up);
    if (pen === left) nx = -1;
    else if (pen === right) nx = 1;
    else if (pen === down) nz = -1;
    else nz = 1;
  }
  return { t: Math.min(1, Math.max(0, tmin)), nx, nz };
}

function capSpeed(vx: number, vz: number): [number, number] {
  const speed = Math.hypot(vx, vz);
  if (speed <= MAX_PLANAR_SPEED || speed < 1e-8) return [vx, vz];
  const scale = MAX_PLANAR_SPEED / speed;
  return [vx * scale, vz * scale];
}

/**
 * Bounce a marble off the rails. A center that is already outside and stays
 * outside is left alone so a knockout can finish.
 */
export function resolveNeonRails(
  prev: [number, number, number],
  pos: [number, number, number],
  vel: [number, number, number],
  radius: number,
  restitution = RAIL_RESTITUTION,
): { pos: [number, number, number]; vel: [number, number, number]; hits: RailHit[] } {
  if (isOutsideNeonCourt(prev[0], prev[2]) && isOutsideNeonCourt(pos[0], pos[2])) {
    return { pos, vel, hits: [] };
  }

  let x = pos[0];
  let z = pos[2];
  let vx = vel[0];
  let vz = vel[2];
  const hits: RailHit[] = [];
  const boxes = [...neonRails(), ...neonObstacles()];

  for (let pass = 0; pass < 3; pass++) {
    let hitThisPass = false;
    for (const rail of boxes) {
      const hit = segmentHitsAabb(
        prev[0],
        prev[2],
        x,
        z,
        rail.minX - radius,
        rail.maxX + radius,
        rail.minZ - radius,
        rail.maxZ + radius,
      );
      if (!hit) continue;

      let nx = hit.nx;
      let nz = hit.nz;
      if (nx === 0 && nz === 0) continue;

      const skin = 0.03;
      const place = (px: number, pz: number): { x: number; z: number } => {
        let nextX = x;
        let nextZ = z;
        if (px < 0) nextX = rail.minX - radius - skin;
        else if (px > 0) nextX = rail.maxX + radius + skin;
        if (pz < 0) nextZ = rail.minZ - radius - skin;
        else if (pz > 0) nextZ = rail.maxZ + radius + skin;
        return { x: nextX, z: nextZ };
      };

      // A marble that started on the court must not be shoved out through a rail.
      const inward = boundaryInward(rail);
      const startedOnCourt = !isOutsideNeonCourt(prev[0], prev[2]);
      if (inward && startedOnCourt) {
        const placed = place(nx, nz);
        const facesOut = nx * inward.x + nz * inward.z < 0;
        if (facesOut || isOutsideNeonCourt(placed.x, placed.z)) {
          nx = inward.x;
          nz = inward.z;
        }
      }

      const dot = vx * nx + vz * nz;
      if (dot < 0) {
        vx = (vx - 2 * dot * nx) * restitution;
        vz = (vz - 2 * dot * nz) * restitution;
        [vx, vz] = capSpeed(vx, vz);
      }

      const parked = place(nx, nz);
      x = parked.x;
      z = parked.z;

      hits.push({
        id: rail.id,
        normal: [nx, 0, nz],
        point: [x, pos[1], z],
      });
      hitThisPass = true;
    }
    if (!hitThisPass) break;
  }

  return { pos: [x, pos[1], z], vel: [vx, vel[1], vz], hits };
}
