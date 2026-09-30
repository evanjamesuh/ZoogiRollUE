import {
  MARBLE_RADIUS,
  bumperSolids,
  cosmosLipRadius,
  getMapLayout,
} from "./arenaColliders";
import { meadowForestPieces } from "./meadowDressing";
import { neonObstacles, neonPlayHalfX, neonPlayHalfZ, neonRails, type NeonBox } from "./neonCourt";
import { ZOOGI_REST_Y } from "./restHeight";

/** How far a pad center sits outside the knockout line. Not a planar-physics retune. */
export const PAD_OUTSET = 1.35;
/** Cosmic stands sit just outside the lip, so those pads step out a little farther. */
export const SPACE_PAD_OUTSET = 2.6;
export const PAD_CLEARANCE = 0.2;
export const LAUNCH_PAD_COUNT = 4;

export interface LaunchPad {
  index: number;
  x: number;
  z: number;
}

const CARDINALS = [0, Math.PI / 2, Math.PI, -Math.PI / 2];

interface Circle {
  x: number;
  z: number;
  radius: number;
}

function blockers(map: string): Circle[] {
  const layout = getMapLayout(map);
  if (!layout) return [];
  const circles: Circle[] = [...layout.scenery, ...bumperSolids(layout.bumpers)];
  if (map === "grass") {
    for (const piece of meadowForestPieces()) {
      if (Math.hypot(piece.x, piece.z) > 40) continue;
      circles.push({
        x: piece.x,
        z: piece.z,
        radius: Math.max(piece.canopy, piece.trunk),
      });
    }
  }
  return circles;
}

function clearOfCircles(x: number, z: number, circles: Circle[]): boolean {
  const need = MARBLE_RADIUS + PAD_CLEARANCE;
  return circles.every((circle) => Math.hypot(x - circle.x, z - circle.z) >= circle.radius + need);
}

function clearOfBoxes(x: number, z: number, boxes: NeonBox[]): boolean {
  const need = MARBLE_RADIUS + PAD_CLEARANCE;
  return boxes.every((box) => {
    const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
    const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
    return Math.hypot(x - nearestX, z - nearestZ) >= need;
  });
}

function roundPads(map: string): LaunchPad[] {
  const layout = getMapLayout(map);
  if (!layout) return [];
  const circles = blockers(map);
  const pads: LaunchPad[] = [];
  const outset = map === "space" ? SPACE_PAD_OUTSET : PAD_OUTSET;
  for (let index = 0; index < LAUNCH_PAD_COUNT; index++) {
    const base = CARDINALS[index];
    let placed: LaunchPad | null = null;
    for (const extra of [0, 0.5, 1.0, 1.6, 2.4]) {
      for (const deg of [0, 12, -12, 22, -22, 32, -32]) {
        const angle = base + (deg * Math.PI) / 180;
        const lip = map === "space" ? cosmosLipRadius(angle) : layout.knockoffRadius;
        const radius = lip + outset + extra;
        const x = Math.cos(angle) * radius;
        const z = Math.sin(angle) * radius;
        if (!clearOfCircles(x, z, circles)) continue;
        placed = { index, x, z };
        break;
      }
      if (placed) break;
    }
    if (!placed) {
      const radius = layout.knockoffRadius + outset + 3;
      placed = { index, x: Math.cos(base) * radius, z: Math.sin(base) * radius };
    }
    pads.push(placed);
  }
  return pads;
}

function neonPads(): LaunchPad[] {
  const halfX = neonPlayHalfX();
  const halfZ = neonPlayHalfZ();
  const boxes = [...neonRails(), ...neonObstacles()];
  const signs = [
    [1, 1],
    [-1, 1],
    [-1, -1],
    [1, -1],
  ];
  return signs.map(([sx, sz], index) => {
    let x = sx * (halfX + PAD_OUTSET);
    let z = sz * (halfZ + PAD_OUTSET);
    for (let step = 0; step < 6 && !clearOfBoxes(x, z, boxes); step++) {
      x += sx * 0.45;
      z += sz * 0.45;
    }
    return { index, x, z };
  });
}

export function launchPadsFor(mapId: string | null | undefined): LaunchPad[] {
  if (!mapId || !getMapLayout(mapId)) return [];
  if (mapId === "neon") return neonPads();
  return roundPads(mapId);
}

export function launchPadByIndex(mapId: string | null | undefined, index: number): LaunchPad | null {
  const pads = launchPadsFor(mapId);
  if (pads.length === 0) return null;
  const wrapped = ((index % pads.length) + pads.length) % pads.length;
  return pads[wrapped];
}

/** World position for a pad, or null when the map has no pads (tests with no layout). */
export function launchPadPosition(
  mapId: string | null | undefined,
  index: number,
): [number, number, number] | null {
  const pad = launchPadByIndex(mapId, index);
  if (!pad) return null;
  return [pad.x, ZOOGI_REST_Y, pad.z];
}
