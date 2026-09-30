/**
 * Shared ring for Meadow, Frozen Ring, and Arabian Nights.
 * The painted floor ends at the knockoff line. floorRadius is the inner edge
 * of the rim band, so score zones sit on open floor and the lip is the out-line.
 */
export const ROUND_FLOOR_RADIUS = 15.2;
export const ROUND_KNOCKOFF_RADIUS = 15.5;

export interface RimMark {
  id: string;
  /** Degrees, from +X toward +Z. */
  angleDeg: number;
  distance: number;
  radius: number;
  /** Visual height. The collider is the horizontal footprint. */
  height: number;
}

/** Six rim obstacles, off the cardinal spawn lanes and off the diagonal score zones. */
const RIM_ANGLES = [32, 70, 152, 196, 250, 332];

function marks(prefix: string, distance: number, radius: number, height: number, distanceJitter: number[]): RimMark[] {
  return RIM_ANGLES.map((angleDeg, i) => ({
    id: `${prefix}-${i}`,
    angleDeg,
    distance: distance + (distanceJitter[i] ?? 0),
    radius,
    height,
  }));
}

export const GRASS_RIM: RimMark[] = [
  { id: "meadow-rock-0", angleDeg: 32, distance: 13.35, radius: 1.15, height: 0.85 },
  { id: "meadow-rock-1", angleDeg: 70, distance: 13.5, radius: 1.05, height: 0.72 },
  { id: "meadow-rock-2", angleDeg: 152, distance: 13.25, radius: 1.2, height: 0.9 },
  { id: "meadow-rock-3", angleDeg: 196, distance: 13.45, radius: 1.1, height: 0.78 },
  { id: "meadow-rock-4", angleDeg: 250, distance: 13.3, radius: 1.22, height: 0.88 },
  { id: "meadow-rock-5", angleDeg: 332, distance: 13.4, radius: 1.08, height: 0.7 },
];

/** Snowmen stand on the ice just inside the snowy lip. Footprint matches the base sphere. */
export const ICE_RIM: RimMark[] = marks("snowman", 14.15, 0.55, 1.7, [0, 0.08, -0.05, 0.04, -0.08, 0.02]);

/** Low planters on the tiled plaza. The collider is the pot's base. */
export const ARABIAN_RIM: RimMark[] = marks("planter", 13.55, 0.72, 0.95, [0, 0.06, -0.04, 0.05, -0.06, 0.03]);

export function rimPosition(mark: RimMark): { x: number; z: number } {
  const angle = (mark.angleDeg * Math.PI) / 180;
  return { x: Math.cos(angle) * mark.distance, z: Math.sin(angle) * mark.distance };
}
