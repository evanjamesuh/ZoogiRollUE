/**
 * Render-only blend between the last two sim steps.
 * The store keeps the true positions. Meshes read this so a 144 Hz screen
 * paints in-between frames instead of holding still until the next 1/60 step.
 */

export type Vec3 = [number, number, number];

interface Sample {
  prev: Vec3;
  curr: Vec3;
}

let alpha = 1;
const samples = new Map<string, Sample>();

export function setInterpFrame(entries: { id: string; prev: Vec3; curr: Vec3 }[], nextAlpha: number): void {
  alpha = Math.min(1, Math.max(0, nextAlpha));
  const seen = new Set<string>();
  for (const entry of entries) {
    seen.add(entry.id);
    let sample = samples.get(entry.id);
    if (!sample) {
      sample = { prev: [0, 0, 0], curr: [0, 0, 0] };
      samples.set(entry.id, sample);
    }
    sample.prev[0] = entry.prev[0];
    sample.prev[1] = entry.prev[1];
    sample.prev[2] = entry.prev[2];
    sample.curr[0] = entry.curr[0];
    sample.curr[1] = entry.curr[1];
    sample.curr[2] = entry.curr[2];
  }
  for (const id of samples.keys()) {
    if (!seen.has(id)) samples.delete(id);
  }
}

/** Keep the last samples and only move the blend forward. Used when this frame ran zero steps. */
export function setInterpAlpha(nextAlpha: number): void {
  alpha = Math.min(1, Math.max(0, nextAlpha));
}

/** A step this large is a respawn, not a roll. Draw the store position. */
const TELEPORT_DISTANCE = 2.5;

export function visualPosition(id: string, fallback: Vec3, out?: Vec3): Vec3 {
  const sample = samples.get(id);
  const write = (x: number, y: number, z: number): Vec3 => {
    if (!out) return [x, y, z];
    out[0] = x;
    out[1] = y;
    out[2] = z;
    return out;
  };
  if (!sample) return write(fallback[0], fallback[1], fallback[2]);
  const stepJump = Math.hypot(sample.curr[0] - sample.prev[0], sample.curr[2] - sample.prev[2]);
  const storeJump = Math.hypot(fallback[0] - sample.curr[0], fallback[2] - sample.curr[2]);
  if (stepJump > TELEPORT_DISTANCE || storeJump > TELEPORT_DISTANCE) {
    return write(fallback[0], fallback[1], fallback[2]);
  }
  const t = alpha;
  return write(
    sample.prev[0] + (sample.curr[0] - sample.prev[0]) * t,
    sample.prev[1] + (sample.curr[1] - sample.prev[1]) * t,
    sample.prev[2] + (sample.curr[2] - sample.prev[2]) * t,
  );
}

export function resetInterp(): void {
  alpha = 1;
  samples.clear();
}
