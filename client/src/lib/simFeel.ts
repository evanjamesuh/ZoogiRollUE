import { ARENA_SCALE_BY_MAP } from "./arenaScale";

/**
 * Match-feel numbers shared by the launch code and the fixed step.
 *
 * One physics step is 1/60 s. Speeds are world units per step, same as before,
 * so a call to physicsTick(1/60) is still exactly one step.
 *
 * These are higher than the old per-frame crawl: a full drag reaches the
 * cap in a shorter pull, rails and bumpers keep more speed, and the extra
 * rolling drag ends the slow tail so a turn can finish.
 *
 * Launch speed grows with the largest arenaScale so a full flick still
 * crosses every ring in about the same number of steps. Damping stays put.
 */

const LAUNCH_SCALE = Math.max(...Object.values(ARENA_SCALE_BY_MAP));

/** Hard cap on planar speed, in units per 1/60 s step. */
export const MAX_PLANAR_SPEED = 2.05 * LAUNCH_SCALE;

/** Drag distance (world units) times this is the launch speed, before the cap. */
export const LAUNCH_POWER_MULTIPLIER = 0.36 * LAUNCH_SCALE;

/** Top speed a player or AI flick is allowed to set. */
export const MAX_LAUNCH_SPEED = 2.05 * LAUNCH_SCALE;

/** Lock-on flicks use a set speed rather than the drag meter. */
export const LOCKON_LAUNCH_SPEED = 1.65 * LAUNCH_SCALE;

/** Seconds an AI waits after its turn starts before it flicks. */
export const AI_LAUNCH_DELAY = 0.4;

/** Per-step speed scale. 0.955 keeps a long slide without the old creep. */
export const LINEAR_DAMPING = 0.955;

/** Constant speed removed each step after the scale. Kills the low-speed tail. */
export const ROLLING_DRAG = 0.011;

/** Below this, a marble is treated as stopped. */
export const REST_SPEED = 0.02;

/**
 * Frozen Ring patches keep more speed than open ice, and still lose speed.
 * Damping stays under 1 and drag stays positive so a patch cannot motor a marble.
 */
export const ICE_LINEAR_DAMPING = 0.992;
export const ICE_ROLLING_DRAG = 0.002;

/** Marble-on-marble bounce. Orb hits stay softer so scoring contact is unchanged. */
export const MARBLE_RESTITUTION = 0.8;

/** Bumper posts. High, but still under 1 so a trapped marble can settle. */
export const BUMPER_RESTITUTION = 0.98;

/** Neon rails. Lively, and a little softer than the posts. */
export const RAIL_RESTITUTION = 0.94;

/** How long PhysicsManager waits after everything is stopped before it marks the turn idle. */
export const SETTLE_GRACE_SECONDS = 0.22;
