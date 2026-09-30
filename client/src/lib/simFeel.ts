/**
 * Match-feel numbers shared by the launch code and the fixed step.
 *
 * One physics step is 1/60 s. Planar speeds are world units per step.
 * Rolling friction is a constant deceleration, so a shot slows in a
 * straight line. A full slingshot still crosses every court in under
 * a second and flies off if nothing stops it. A modest shot comes to
 * rest in about three seconds.
 */

import { ORB_DRAW_RADIUS, ZOOGI_DRAW_RADIUS } from "./restHeight";

export const SIM_HZ = 60;

/** Zoogis are the heavy body. Orbs use the volume ratio of the two radii. */
export const ZOOGI_MASS = 1;
export const ORB_MASS = (ORB_DRAW_RADIUS / ZOOGI_DRAW_RADIUS) ** 3;

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
export const FULL_LAUNCH_SPEED_PER_SEC = 46;

/** Hard cap on planar speed, in units per step. */
export const MAX_PLANAR_SPEED = FULL_LAUNCH_SPEED_PER_SEC / SIM_HZ;

/** Top speed a player or AI flick is allowed to set. */
export const MAX_LAUNCH_SPEED = MAX_PLANAR_SPEED;

/**
 * The aim drag is measured in world units and capped at this pull.
 * Speed is pull distance times the multiplier, so a longer pull is
 * a stronger shot across the whole drag, and a full pull is full power.
 */
export const FULL_PULL_DISTANCE = 15;
export const LAUNCH_POWER_MULTIPLIER = MAX_LAUNCH_SPEED / FULL_PULL_DISTANCE;

/** Lock-on flicks use a set speed rather than the drag meter. */
export const LOCKON_LAUNCH_SPEED = MAX_LAUNCH_SPEED * 0.55;

/**
 * A shot that stays on the court. Rest time is this speed divided by
 * ROLLING_DECEL: 13.5 / 4.5 = 3 seconds. Stopping distance is 20.25.
 */
export const NORMAL_LAUNCH_SPEED_PER_SEC = 13.5;
export const NORMAL_LAUNCH_SPEED = NORMAL_LAUNCH_SPEED_PER_SEC / SIM_HZ;

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
