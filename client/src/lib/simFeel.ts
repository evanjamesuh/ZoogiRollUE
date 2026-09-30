/**
 * Match-feel numbers shared by the launch code and the fixed step.
 *
 * One physics step is 1/60 s. Planar speeds are world units per step.
 * Rolling friction is a constant deceleration. Pull strength is not
 * linear: everyday drags stay in a narrow speed band and rest in a
 * few seconds, and only the end of the slingshot kicks up to a full
 * shot that crosses the court and flies off.
 */

import { ORB_DRAW_RADIUS, ZOOGI_DRAW_RADIUS } from "./restHeight";

export const SIM_HZ = 60;

/**
 * Reference mass for a Zoogi. Orbs use the volume ratio of the two radii,
 * so every Zoogi stays heavier than an orb. A Zoogi's own mass comes from
 * defense: see zoogiMassFromDefense.
 */
export const ZOOGI_MASS = 1;
export const ORB_MASS = (ORB_DRAW_RADIUS / ZOOGI_DRAW_RADIUS) ** 3;

/**
 * Heavier Zoogis (higher defense) hit harder and get pushed less.
 * Mass is defense / 60, so 60 defense is the reference body. Clamped
 * so a junk stat cannot undercut an orb or become a wall.
 */
export function zoogiMassFromDefense(defense: number): number {
  const stat = Number.isFinite(defense) ? defense : 60;
  return Math.min(1.8, Math.max(0.55, stat / 60));
}

/** Constant deceleration, world units per second squared. */
export const ROLLING_DECEL = 4.5;
export const ICE_ROLLING_DECEL = 1.15;

/** Units of speed removed per step. Ice loses less, and still loses some. */
export const ROLLING_DRAG = ROLLING_DECEL / (SIM_HZ * SIM_HZ);
export const ICE_ROLLING_DRAG = ICE_ROLLING_DECEL / (SIM_HZ * SIM_HZ);

/**
 * Kept at 1. The roll uses linear drag only, so an exponential scale
 * would curve the shot.
 */
export const LINEAR_DAMPING = 1;
export const ICE_LINEAR_DAMPING = 1;

/** Below this, a marble is treated as stopped. Units per step. */
export const REST_SPEED = 0.02;

/**
 * Full slingshot, world units per second.
 * At this speed a court of about 30 Zoogi widths takes under a second
 * to cross, and the stopping distance is several courts, so an open
 * shot flies off.
 */
export const FULL_LAUNCH_SPEED_PER_SEC = 36;

/** Hard cap on planar speed, in units per step. */
export const MAX_PLANAR_SPEED = FULL_LAUNCH_SPEED_PER_SEC / SIM_HZ;

/** Top speed a player or AI flick is allowed to set. */
export const MAX_LAUNCH_SPEED = MAX_PLANAR_SPEED;

/**
 * The aim drag is measured in world units and capped at this pull.
 * Speed comes from launchSpeedForPull, not from a constant times the
 * drag. The everyday band is gentle. The last part of the pull is the
 * kick that reaches full power.
 */
export const FULL_PULL_DISTANCE = 15;

/** Kept so older call sites can scale a raw drag. New shots use launchSpeedForPull. */
export const LAUNCH_POWER_MULTIPLIER = MAX_LAUNCH_SPEED / FULL_PULL_DISTANCE;

/**
 * Everyday pulls (through 95%) ease from a soft flick up to a shot
 * that still rests in a few seconds. Past that, the slingshot kicks
 * up to a full pull.
 */
const EVERYDAY_PULL = 0.95;
const EVERYDAY_TOP_SPEED = 16.2;
const MID_PULL = 0.5;
const MID_SPEED = 12.5;

/** World units per second for a pull fraction in 0..1. */
export function launchSpeedPerSecForPull(pullFraction: number): number {
  const p = Math.min(1, Math.max(0, pullFraction));
  if (p <= EVERYDAY_PULL) {
    const u = p / EVERYDAY_PULL;
    const uMid = MID_PULL / EVERYDAY_PULL;
    const curve = (EVERYDAY_TOP_SPEED * uMid - MID_SPEED) / (uMid - uMid * uMid);
    const slope = EVERYDAY_TOP_SPEED - curve;
    return slope * u + curve * u * u;
  }
  const u = (p - EVERYDAY_PULL) / (1 - EVERYDAY_PULL);
  return EVERYDAY_TOP_SPEED + (FULL_LAUNCH_SPEED_PER_SEC - EVERYDAY_TOP_SPEED) * Math.pow(u, 1.35);
}

/** Planar speed, units per step, for a pull fraction in 0..1. */
export function launchSpeedForPull(pullFraction: number): number {
  return launchSpeedPerSecForPull(pullFraction) / SIM_HZ;
}

/**
 * Turn a slingshot drag into a planar velocity. `boost` is a power-up
 * scale and still cannot exceed a full pull.
 */
export function launchPlanarVelocity(dx: number, dz: number, boost = 1): [number, number] | null {
  const raw = Math.hypot(dx, dz);
  if (!(raw > 1)) return null;
  const pull = Math.min(raw, FULL_PULL_DISTANCE);
  let speed = launchSpeedForPull(pull / FULL_PULL_DISTANCE) * (boost > 0 ? boost : 1);
  if (speed > MAX_LAUNCH_SPEED) speed = MAX_LAUNCH_SPEED;
  return [(dx / raw) * speed, (dz / raw) * speed];
}

/** Lock-on is a firm everyday flick, about a 70% pull, not a fly-off. */
export const LOCKON_LAUNCH_SPEED = launchSpeedForPull(0.7);

/** A 50% pull. Everyday shots in this band rest on the court. */
export const NORMAL_LAUNCH_SPEED = launchSpeedForPull(0.5);
export const NORMAL_LAUNCH_SPEED_PER_SEC = NORMAL_LAUNCH_SPEED * SIM_HZ;

/** Seconds an AI waits after its turn starts before it flicks. */
export const AI_LAUNCH_DELAY = 0.4;

/**
 * Slightly soft billiards. Head-on, equal mass, target at rest:
 * the target leaves with (1+e)/2 = 0.875 of the incoming speed,
 * and the shooter keeps (1-e)/2 = 0.125.
 */
export const MARBLE_RESTITUTION = 0.75;
export const MOMENTUM_TRANSFER = (1 + MARBLE_RESTITUTION) / 2;
export const SHOOTER_KEEP = (1 - MARBLE_RESTITUTION) / 2;

/** Bumper posts. A near-elastic kick, still under 1 so a trap can settle. */
export const BUMPER_RESTITUTION = 0.98;

/** Rocks and other scenery. An ordinary ricochet, much softer than a bumper. */
export const ROCK_RESTITUTION = 0.55;

/** Neon rails and raised pads. Walls, so they ricochet like rocks. */
export const RAIL_RESTITUTION = 0.7;

/**
 * After every marble and orb is at rest or has fallen out, wait this
 * long before the turn passes.
 */
export const SETTLE_DELAY_SECONDS = 1.2;
export const SETTLE_DELAY_STEPS = Math.round(SETTLE_DELAY_SECONDS * SIM_HZ);

/**
 * A body that never quite rests (a jittering contact) must not hold the
 * turn forever. The clock starts when the shot happens.
 */
export const SETTLE_TIMEOUT_SECONDS = 48;
export const SETTLE_TIMEOUT_STEPS = Math.round(SETTLE_TIMEOUT_SECONDS * SIM_HZ);

/**
 * Orbs are released above the floor and fall with FALL_GRAVITY.
 * Later orbs start higher so the ring lands as a short cascade.
 * Nothing waits in the air.
 */
export const ORB_DROP_HEIGHT = 7.2;
export const ORB_DROP_STAGGER = 0.42;

/** Orb-on-orb bounce. The floor uses the same number. */
export const ORB_ORB_RESTITUTION = 0.55;

/** Upward speed, units per step, below which a floor hit settles. */
export const ORB_BOUNCE_REST = 0.015;

/** PhysicsManager uses the same pause before it marks the court idle. */
export const SETTLE_GRACE_SECONDS = SETTLE_DELAY_SECONDS;

/** Fall off an open edge, world units per second squared. */
export const FALL_GRAVITY = 9.81;
export const FALL_GRAVITY_STEP = FALL_GRAVITY / (SIM_HZ * SIM_HZ);

/** Below this height the body has fallen out and no longer blocks the turn. */
export const FALL_OUT_Y = -6;
