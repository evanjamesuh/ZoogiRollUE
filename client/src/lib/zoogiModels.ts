import * as THREE from "three";

/**
 * Per-character model settings for matches.
 *
 * Each roster Zoogi is drawn from its GLB instead of a plain colored ball.
 * The loader fits the model to the marble (same size as the old ball) and
 * rests it on the floor. These values are extra tweaks on top of that:
 *
 * - scale: 1 matches the marble. 1.1 is 10% bigger, 0.9 is 10% smaller.
 * - offset: a nudge in world units, [x, y, z]. [0, 0, 0] sits on the floor
 *   like the ball. Raise Y if a character should float.
 * - rotation: an extra turn in radians, [x, y, z], before the marble tumbles.
 *   The models already face +Z (forward), so [0, 0, 0] is right.
 *   To turn a face, change Y. Math.PI is a half turn, Math.PI / 2 is a quarter turn.
 *
 * Lars is listed here even when he is not on the roster yet. As soon as the
 * roster includes him, matches load /models/lars.glb with these settings.
 */
export interface ZoogiModelSettings {
  url: string;
  scale: number;
  offset: [number, number, number];
  rotation: [number, number, number];
}

function model(url: string): ZoogiModelSettings {
  return {
    url,
    scale: 1,
    offset: [0, 0, 0],
    rotation: [0, 0, 0],
  };
}

export const ZOOGI_MODELS: Record<string, ZoogiModelSettings> = {
  wolfgang: model("/models/wolfgang.glb"),
  hotstreak: model("/models/hotstreak.glb"),
  pinpoint: model("/models/pinpoint.glb"),
  bolt: model("/models/bolt.glb"),
  wraps: model("/models/wraps.glb"),
  lars: model("/models/lars.glb"),
};

/**
 * Files to fetch as soon as the game loads, so a match does not wait on them.
 * Lars is included with the others, so /models/lars.glb is already downloading
 * before a match starts instead of showing the blue ball while it loads.
 */
export function zoogiModelPreloadUrls(): string[] {
  return ["wolfgang", "hotstreak", "pinpoint", "bolt", "wraps", "lars"].map((id) => ZOOGI_MODELS[id].url);
}

/**
 * Which file to draw for this Zoogi.
 * A custom (Meshy) model wins. Otherwise the roster entry, or /models/<id>.glb
 * for a future roster id. Custom ids with no file stay on the colored ball.
 */
export function resolveZoogiModel(zoogiId: string, customModelUrl?: string): ZoogiModelSettings | null {
  if (customModelUrl) return model(customModelUrl);
  const known = ZOOGI_MODELS[zoogiId];
  if (known) return known;
  if (!zoogiId || zoogiId.startsWith("custom_")) return null;
  return model(`/models/${zoogiId}.glb`);
}

const rollAxis = new THREE.Vector3();
const rollQuat = new THREE.Quaternion();

/** A move this large in one frame is a respawn, not a roll. */
const TELEPORT_DISTANCE = 2.5;

/**
 * Tumble the marble the way a real ball rolls, in whatever direction it traveled.
 *
 * The turn is (distance along the floor this frame) / radius, around the
 * horizontal axis perpendicular to that travel, so the top of the ball moves
 * with the roll. A respawn jump resets the model to facing forward.
 */
export function rollMarble(object: THREE.Object3D, x: number, z: number, radius: number) {
  const previous = object.userData.rollPrevious as { x: number; z: number } | undefined;
  object.userData.rollPrevious = { x, z };
  if (!previous || radius <= 0) return;

  const dx = x - previous.x;
  const dz = z - previous.z;
  const distance = Math.hypot(dx, dz);
  if (distance < 1e-5) return;
  if (distance > TELEPORT_DISTANCE) {
    object.quaternion.identity();
    return;
  }

  // up × direction of travel. Positive rotation around that axis rolls the ball forward.
  rollAxis.set(dz, 0, -dx).multiplyScalar(1 / distance);
  rollQuat.setFromAxisAngle(rollAxis, distance / radius);
  object.quaternion.premultiply(rollQuat);
}
