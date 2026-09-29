/**
 * Night Circuit, an original rectangular court.
 *
 * Closed neon rails bounce marbles. The four corners are open mouths, and the
 * glowing lip just outside the floor is the knockout line. Nothing here is
 * copied from another game's layout, marks, or meshes.
 */

import type { MapLayout, ZonePlacement } from "./arenaColliders";
import { arenaScaleFor } from "./arenaScale";
import { MARBLE_WIDTH, repairRectRimGaps, snapRimGap, type RimGapAdjustment } from "./obstaclePlacement";
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
 */
/** Bumper posts in today's units, for markings drawn inside the arena group. */
export function neonBumperMarks(): NeonBumper[] {
  return [
    { id: "neon-post-a", x: -5.1, z: 2.7 },
    { id: "neon-post-b", x: 4.2, z: 3.4 },
    { id: "neon-post-c", x: -3.4, z: -4.6 },
    { id: "neon-pylon-west", x: -6.6, z: 5.6 },
    { id: "neon-pylon-east", x: 5.8, z: -2.6 },
  ];
}

export function neonBumpers(): NeonBumper[] {
  const scale = courtScale();
  return neonBumperMarks().map((bumper) => ({ ...bumper, x: bumper.x * scale, z: bumper.z * scale }));
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

/** Pad centers move out with the court. The boxes themselves stay the same size. */
export function neonObstacles(): NeonBox[] {
  return repairRectRimGaps(scaledObstacleBoxes(), neonPlayHalfX(), neonPlayHalfZ(), MARBLE_WIDTH).boxes;
}

export function neonRimGapAdjustments(): RimGapAdjustment[] {
  const adjustments = repairRectRimGaps(scaledObstacleBoxes(), neonPlayHalfX(), neonPlayHalfZ(), MARBLE_WIDTH).adjustments;
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
      zoneAt("spawn-0", -6.4 * scale, -1.1 * scale, true),
      zoneAt("spawn-1", 6.6 * scale, 1.05 * scale, true),
      zoneAt("spawn-2", -1.15 * scale, 4.15 * scale, true),
      zoneAt("spawn-3", 1.2 * scale, -4.05 * scale, true),
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
