/**
 * Fixed-heading match camera.
 *
 * Pitch is the angle down from the horizon (50–55°). Heading never changes:
 * the camera always sits on the +Z side and looks toward -Z. Follow, zoom,
 * and shake are all driven by real elapsed time, so 60 Hz and 144 Hz agree.
 */

export const ARENA_PITCH_DEG = 53;
export const ARENA_PITCH = (ARENA_PITCH_DEG * Math.PI) / 180;
export const ARENA_FOV_DEG = 44;

const SHAKE_CAP = 0.28;

let trauma = 0;

export function addTrauma(amount: number): void {
  if (!Number.isFinite(amount) || amount <= 0) return;
  trauma = Math.min(1, trauma + amount);
}

export function getTrauma(): number {
  return trauma;
}

export function decayTrauma(dt: number): void {
  if (dt <= 0 || trauma <= 0) return;
  trauma *= Math.exp(-1.8 * dt);
  if (trauma < 0.001) trauma = 0;
}

export function resetTrauma(): void {
  trauma = 0;
}

/** Frame-rate independent blend. One 1/30 step matches two 1/60 steps. */
export function damp(current: number, target: number, lambda: number, dt: number): number {
  if (!Number.isFinite(dt) || dt <= 0) return current;
  const t = 1 - Math.exp(-Math.max(0, lambda) * dt);
  return current + (target - current) * t;
}

export interface Bounds {
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
}

export function actionBounds(points: { x: number; z: number }[], padding = 0): Bounds {
  if (points.length === 0) {
    return { minX: -padding, maxX: padding, minZ: -padding, maxZ: padding };
  }
  let minX = Infinity;
  let maxX = -Infinity;
  let minZ = Infinity;
  let maxZ = -Infinity;
  for (const point of points) {
    if (point.x < minX) minX = point.x;
    if (point.x > maxX) maxX = point.x;
    if (point.z < minZ) minZ = point.z;
    if (point.z > maxZ) maxZ = point.z;
  }
  return {
    minX: minX - padding,
    maxX: maxX + padding,
    minZ: minZ - padding,
    maxZ: maxZ + padding,
  };
}

export interface Vec3 {
  x: number;
  y: number;
  z: number;
}

/** Offset from the look target. +Z is toward the camera. */
export function cameraOffset(distance: number, pitch = ARENA_PITCH): Vec3 {
  return {
    x: 0,
    y: Math.sin(pitch) * distance,
    z: Math.cos(pitch) * distance,
  };
}

function cross(a: Vec3, b: Vec3): Vec3 {
  return {
    x: a.y * b.z - a.z * b.y,
    y: a.z * b.x - a.x * b.z,
    z: a.x * b.y - a.y * b.x,
  };
}

function normalize(v: Vec3): Vec3 {
  const len = Math.hypot(v.x, v.y, v.z) || 1;
  return { x: v.x / len, y: v.y / len, z: v.z / len };
}

function dot(a: Vec3, b: Vec3): number {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * Smallest camera distance that keeps every padded bounds corner in frame.
 * `pitch` is radians down from the horizon. `fovDeg` is the vertical field of view.
 */
export function fitDistance(
  bounds: Bounds,
  pitch: number,
  fovDeg: number,
  aspect: number,
  padding = 0,
): number {
  const safeAspect = Number.isFinite(aspect) && aspect > 0.2 ? aspect : 1;
  const tanV = Math.tan(((fovDeg * Math.PI) / 180) / 2);
  const tanH = tanV * safeAspect;
  const minX = bounds.minX - padding;
  const maxX = bounds.maxX + padding;
  const minZ = bounds.minZ - padding;
  const maxZ = bounds.maxZ + padding;
  const lookX = (minX + maxX) / 2;
  const lookZ = (minZ + maxZ) / 2;
  const lookY = 0.2;
  const corners: Array<[number, number]> = [
    [minX, minZ],
    [minX, maxZ],
    [maxX, minZ],
    [maxX, maxZ],
  ];

  let distance = 18;
  for (let iter = 0; iter < 5; iter++) {
    const eye = cameraOffset(distance, pitch);
    const zaxis = normalize(eye);
    const xaxis = normalize(cross({ x: 0, y: 1, z: 0 }, zaxis));
    const yaxis = cross(zaxis, xaxis);
    let needed = 6;
    for (const [px, pz] of corners) {
      const q = { x: px - lookX, y: 0 - lookY, z: pz - lookZ };
      const alongRight = dot(q, xaxis);
      const alongUp = dot(q, yaxis);
      const alongForward = -dot(q, zaxis);
      const distH = Math.abs(alongRight) / tanH - alongForward;
      const distV = Math.abs(alongUp) / tanV - alongForward;
      needed = Math.max(needed, distH, distV);
    }
    distance = Math.max(6, needed);
  }
  return distance;
}

export function clampDistance(distance: number, min = 13.5, max = 34): number {
  return Math.min(max, Math.max(min, distance));
}

/**
 * How far the match camera may sit. Portrait phones need the extra room so
 * launch pads outside the lip stay in frame. Landscape stays closer.
 */
export function matchCameraMax(aspect: number, frameScale = 1): number {
  const portrait = aspect < 0.9;
  return (portrait ? 140 : 60) * frameScale;
}

/**
 * Smooth shake from trauma in 0..1. Trauma 0 is exactly zero.
 * The offset stays inside SHAKE_CAP on each axis. Noise is a sum of sines,
 * not a fresh random value, so it reads as one punch instead of static.
 */
export function smoothShake(amount: number, time: number): Vec3 {
  const traumaAmount = Math.min(1, Math.max(0, amount));
  if (traumaAmount <= 0) return { x: 0, y: 0, z: 0 };
  const mag = traumaAmount * traumaAmount;
  const wave = (seed: number, speed: number) => Math.sin(time * speed + seed);
  const x = (wave(1.3, 31) * 0.65 + wave(4.1, 19) * 0.35) * mag * 0.45;
  const y = (wave(2.2, 27) * 0.7 + wave(5.4, 15) * 0.3) * mag * 0.22;
  const z = (wave(3.7, 29) * 0.65 + wave(6.2, 21) * 0.35) * mag * 0.45;
  return {
    x: Math.min(SHAKE_CAP, Math.max(-SHAKE_CAP, x)),
    y: Math.min(SHAKE_CAP, Math.max(-SHAKE_CAP, y)),
    z: Math.min(SHAKE_CAP, Math.max(-SHAKE_CAP, z)),
  };
}

export const SHAKE_LIMIT = SHAKE_CAP;
