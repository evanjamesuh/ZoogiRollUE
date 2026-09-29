import { ROUND_KNOCKOFF_RADIUS } from "./roundRim";

export interface Aabb {
  minX: number;
  minY: number;
  minZ: number;
  maxX: number;
  maxY: number;
  maxZ: number;
}

export function boxSize(box: Aabb): { x: number; y: number; z: number } {
  return {
    x: box.maxX - box.minX,
    y: box.maxY - box.minY,
    z: box.maxZ - box.minZ,
  };
}

/** Distance from the origin to the closest point of the box in the XZ plane. */
export function closestDistanceXZ(box: Aabb): number {
  const dx = box.minX > 0 ? box.minX : box.maxX < 0 ? -box.maxX : 0;
  const dz = box.minZ > 0 ? box.minZ : box.maxZ < 0 ? -box.maxZ : 0;
  return Math.hypot(dx, dz);
}

export function shiftBox(box: Aabb, dx: number, dz: number): Aabb {
  return {
    minX: box.minX + dx,
    maxX: box.maxX + dx,
    minY: box.minY,
    maxY: box.maxY,
    minZ: box.minZ + dz,
    maxZ: box.maxZ + dz,
  };
}

/**
 * Hide the playfield slab and sky domes. Props, even flat ones like a pool,
 * stay so they can be moved outside the ring.
 */
export function shouldHideRingPiece(name: string, box: Aabb): boolean {
  if (/sky|skydome|backdrop|FrontSide|BackSide/i.test(name)) return true;
  const size = boxSize(box);
  const xz = Math.max(size.x, size.z);
  const centerDist = Math.hypot((box.minX + box.maxX) / 2, (box.minZ + box.maxZ) / 2);
  const flat = size.y < 1.8;
  const surrounds = box.minX < -40 && box.maxX > 40 && box.minZ < -40 && box.maxZ > 40 && size.y > 30;
  if (surrounds && size.y > xz * 0.45) return true;
  if (flat && xz > 12 && centerDist < ROUND_KNOCKOFF_RADIUS) return true;
  if (closestDistanceXZ(box) < 0.5 && flat && xz > 8) return true;
  return false;
}

/**
 * World-space XZ translation that puts the box fully outside `limit`,
 * and pulls a far backdrop in so its centre stays near `maxCenter`.
 * A zero translation means the box already sits in the scenery band.
 */
export function translationToClear(box: Aabb, limit: number, maxCenter = 32): { dx: number; dz: number } {
  const cx = (box.minX + box.maxX) / 2;
  const cz = (box.minZ + box.maxZ) / 2;
  let dirX = cx;
  let dirZ = cz;
  let centerDist = Math.hypot(dirX, dirZ);
  if (centerDist < 1e-4) {
    dirX = 1;
    dirZ = 0;
    centerDist = 1;
  }
  dirX /= centerDist;
  dirZ /= centerDist;

  const closest = closestDistanceXZ(box);
  if (closest >= limit) {
    if (centerDist <= maxCenter) return { dx: 0, dz: 0 };
    let lo = 0;
    let hi = centerDist;
    for (let i = 0; i < 24; i++) {
      const mid = (lo + hi) / 2;
      const shifted = shiftBox(box, -dirX * mid, -dirZ * mid);
      if (closestDistanceXZ(shifted) >= limit) lo = mid;
      else hi = mid;
    }
    const pull = Math.min(lo, Math.max(0, centerDist - maxCenter));
    return { dx: -dirX * pull, dz: -dirZ * pull };
  }

  let hi = 4;
  while (closestDistanceXZ(shiftBox(box, dirX * hi, dirZ * hi)) < limit && hi < 400) {
    hi *= 2;
  }
  let lo = 0;
  for (let i = 0; i < 24; i++) {
    const mid = (lo + hi) / 2;
    if (closestDistanceXZ(shiftBox(box, dirX * mid, dirZ * mid)) >= limit) hi = mid;
    else lo = mid;
  }
  return { dx: dirX * hi, dz: dirZ * hi };
}
