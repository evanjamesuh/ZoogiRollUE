/**
 * Night Circuit, an original rectangular court.
 *
 * Closed neon rails bounce marbles. The four corners are open mouths, and the
 * glowing lip just outside the floor is the knockout line. Nothing here is
 * copied from another game's layout, marks, or meshes.
 */

import type { MapLayout, ZonePlacement } from "./arenaColliders";
import { scalePlay } from "./arenaScale";
import { MAX_PLANAR_SPEED, RAIL_RESTITUTION } from "./simFeel";

export const NEON_HALF_X = scalePlay(12);
export const NEON_HALF_Z = scalePlay(8);
export const NEON_RAIL_DEPTH = scalePlay(0.7);
/** How far each rail stops short of a corner, leaving a knockout mouth. */
export const NEON_CORNER_GAP = scalePlay(3.15);

export interface NeonRail {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  /** Emissive hex used by the mesh. Not a collider. */
  color: string;
}

const GAP_X = NEON_HALF_X - NEON_CORNER_GAP;
const GAP_Z = NEON_HALF_Z - NEON_CORNER_GAP;

export function neonRails(): NeonRail[] {
  return [
    {
      id: "rail-north",
      minX: -GAP_X,
      maxX: GAP_X,
      minZ: NEON_HALF_Z - NEON_RAIL_DEPTH,
      maxZ: NEON_HALF_Z,
      color: "#b026ff",
    },
    {
      id: "rail-south",
      minX: -GAP_X,
      maxX: GAP_X,
      minZ: -NEON_HALF_Z,
      maxZ: -NEON_HALF_Z + NEON_RAIL_DEPTH,
      color: "#b026ff",
    },
    {
      id: "rail-east",
      minX: NEON_HALF_X - NEON_RAIL_DEPTH,
      maxX: NEON_HALF_X,
      minZ: -GAP_Z,
      maxZ: GAP_Z,
      color: "#ff2bd6",
    },
    {
      id: "rail-west",
      minX: -NEON_HALF_X,
      maxX: -NEON_HALF_X + NEON_RAIL_DEPTH,
      minZ: -GAP_Z,
      maxZ: GAP_Z,
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
export function neonBumpers(): NeonBumper[] {
  return [
    { id: "neon-post-a", x: -5.1, z: 2.7 },
    { id: "neon-post-b", x: 4.2, z: 3.4 },
    { id: "neon-post-c", x: -3.4, z: -4.6 },
    { id: "neon-pylon-west", x: -6.6, z: 5.6 },
    { id: "neon-pylon-east", x: 5.8, z: -2.6 },
  ].map((bumper) => ({ ...bumper, x: scalePlay(bumper.x), z: scalePlay(bumper.z) }));
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
export function neonObstacles(): NeonBox[] {
  return [
    { id: "pad-west", minX: -9.15, maxX: -7.45, minZ: -1.55, maxZ: 1.55 },
    { id: "pad-east", minX: 7.7, maxX: 9.2, minZ: -1.55, maxZ: 1.55 },
    { id: "pad-north", minX: -2.4, maxX: 0.4, minZ: 5.55, maxZ: 6.7 },
    { id: "pad-south", minX: 1.6, maxX: 3.8, minZ: -6.7, maxZ: -5.55 },
    { id: "channel-north", minX: -2.35, maxX: 2.35, minZ: 0.72, maxZ: 1.08 },
    { id: "channel-south", minX: -2.35, maxX: 2.35, minZ: -1.08, maxZ: -0.72 },
  ].map((box) => ({
    ...box,
    minX: scalePlay(box.minX),
    maxX: scalePlay(box.maxX),
    minZ: scalePlay(box.minZ),
    maxZ: scalePlay(box.maxZ),
  }));
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
  return {
    id: "neon",
    floorRadius: NEON_HALF_Z,
    // Stored so other systems have a number. The live check is the rectangle.
    knockoffRadius: NEON_HALF_X + scalePlay(0.35),
    orbRingRadius: scalePlay(3.15),
    scenery: [],
    bumpers: neonBumpers(),
    zones: [
      zoneAt("spawn-0", scalePlay(-6.4), scalePlay(-1.1), true),
      zoneAt("spawn-1", scalePlay(6.6), scalePlay(1.05), true),
      zoneAt("spawn-2", scalePlay(-1.15), scalePlay(4.15), true),
      zoneAt("spawn-3", scalePlay(1.2), scalePlay(-4.05), true),
      zoneAt("score-0", scalePlay(7.6), scalePlay(4.55), false),
      zoneAt("score-1", scalePlay(-7.7), scalePlay(4.4), false),
      zoneAt("score-2", scalePlay(7.8), scalePlay(-4.35), false),
      zoneAt("score-3", scalePlay(-7.5), scalePlay(-4.5), false),
    ],
  };
}

/** True when a marble center has left the floor, including through a corner mouth. */
export function isOutsideNeonCourt(x: number, z: number): boolean {
  return Math.abs(x) > NEON_HALF_X || Math.abs(z) > NEON_HALF_Z;
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

      const nx = hit.nx;
      const nz = hit.nz;
      if (nx === 0 && nz === 0) continue;

      const dot = vx * nx + vz * nz;
      if (dot < 0) {
        vx = (vx - 2 * dot * nx) * restitution;
        vz = (vz - 2 * dot * nz) * restitution;
        [vx, vz] = capSpeed(vx, vz);
      }

      const skin = 0.03;
      if (nx < 0) x = rail.minX - radius - skin;
      else if (nx > 0) x = rail.maxX + radius + skin;
      if (nz < 0) z = rail.minZ - radius - skin;
      else if (nz > 0) z = rail.maxZ + radius + skin;

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
