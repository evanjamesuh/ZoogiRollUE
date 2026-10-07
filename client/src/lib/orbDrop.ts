import { ORB_BOUNCE_REST, ORB_DROP_HEIGHT, ORB_DROP_STAGGER } from "./simFeel";
import { ORB_REST_Y } from "./restHeight";

export interface OrbVertical {
  isActive: boolean;
  isOutOfRing?: boolean;
  position: [number, number, number];
  velocity: [number, number, number];
}

/** World height of an orb released in the opening cascade. Index 0 is lowest. */
export function orbDropY(index: number): number {
  return ORB_REST_Y + ORB_DROP_HEIGHT + index * ORB_DROP_STAGGER;
}

/**
 * True when this orb is on the floor or already out of play.
 * A bounce still in the air keeps the shot locked.
 */
export function orbHasLanded(orb: OrbVertical): boolean {
  if (!orb.isActive || orb.isOutOfRing) return true;
  return orb.position[1] <= ORB_REST_Y + 0.02 && Math.abs(orb.velocity[1]) <= ORB_BOUNCE_REST;
}

export function orbsHaveLanded(orbs: OrbVertical[]): boolean {
  return orbs.every(orbHasLanded);
}
