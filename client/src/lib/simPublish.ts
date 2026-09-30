/**
 * A rolling marble only changes positions and velocities. Those writes stay on
 * the objects the meshes already hold, so a physics step does not have to
 * notify React. A knockout, a scored orb, or a new body still publishes.
 */

const MARBLE_FLAGS = [
  "isKnockedOut",
  "isRespawning",
  "isStunned",
  "hasShield",
  "score",
  "wolfgangAbilityUnlocked",
  "hotstreakAbilityUnlocked",
  "boltAbilityUnlocked",
  "larsAbilityUnlocked",
  "wrapsAbilityUnlocked",
  "nightshadeAbilityUnlocked",
] as const;

const ORB_FLAGS = ["isActive", "isStarOrb"] as const;

export interface MotionBaseline {
  phase: string;
  showExplosion: unknown;
  player: object | null;
  enemies: object[];
  orbs: object[];
  wolfClones: object[];
  fallingEntities: object[];
  pinballBumpers: object[];
}

export interface MotionNext {
  phase?: string;
  showExplosion?: unknown;
  player: object;
  enemies: object[];
  orbs: object[];
  wolfClones: object[];
  fallingEntities: object[];
  pinballBumpers: object[];
}

function rec(value: object): Record<string, unknown> {
  return value as Record<string, unknown>;
}

function sameId(a: object, b: object): boolean {
  return rec(a).id === rec(b).id;
}

function flagsDiffer(live: object[], next: object[], keys: readonly string[]): boolean {
  if (live.length !== next.length) return true;
  for (let i = 0; i < live.length; i++) {
    if (!sameId(live[i], next[i])) return true;
    const left = rec(live[i]);
    const right = rec(next[i]);
    for (const key of keys) {
      if (left[key] !== right[key]) return true;
    }
  }
  return false;
}

function bumperTimesDiffer(live: object[], next: object[]): boolean {
  if (live.length !== next.length) return true;
  for (let i = 0; i < live.length; i++) {
    const left = rec(live[i]);
    const right = rec(next[i]);
    if (left.id !== right.id) return true;
    if (left.lastHitTime !== right.lastHitTime) return true;
  }
  return false;
}

/** True when the step can be written onto the live objects without a store publish. */
export function motionOnly(live: MotionBaseline, next: MotionNext): boolean {
  if (next.phase !== undefined && next.phase !== live.phase) return false;
  if (Boolean(next.showExplosion) !== Boolean(live.showExplosion)) return false;
  if (!live.player || flagsDiffer([live.player], [next.player], MARBLE_FLAGS)) return false;
  if (flagsDiffer(live.enemies, next.enemies, MARBLE_FLAGS)) return false;
  if (flagsDiffer(live.orbs, next.orbs, ORB_FLAGS)) return false;
  if (flagsDiffer(live.wolfClones, next.wolfClones, ["isActive"])) return false;
  if (flagsDiffer(live.fallingEntities, next.fallingEntities, ["hasLanded"])) return false;
  if (bumperTimesDiffer(live.pinballBumpers, next.pinballBumpers)) return false;
  return true;
}

function isVec3(value: unknown): value is [number, number, number] {
  return Array.isArray(value)
    && value.length === 3
    && typeof value[0] === "number"
    && typeof value[1] === "number"
    && typeof value[2] === "number";
}

/** Copy fields onto the existing record. Position and velocity keep their array identity. */
export function overlayRecord(live: Record<string, unknown>, next: Record<string, unknown>): void {
  for (const key of Object.keys(next)) {
    const value = next[key];
    const prev = live[key];
    if (isVec3(value) && isVec3(prev)) {
      prev[0] = value[0];
      prev[1] = value[1];
      prev[2] = value[2];
    } else {
      live[key] = value;
    }
  }
}

export function overlayList(live: Record<string, unknown>[], next: Record<string, unknown>[]): void {
  for (let i = 0; i < next.length; i++) overlayRecord(live[i], next[i]);
}
