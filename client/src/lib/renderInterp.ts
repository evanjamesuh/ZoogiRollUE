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
  samples.clear();
  for (const entry of entries) {
    samples.set(entry.id, {
      prev: [entry.prev[0], entry.prev[1], entry.prev[2]],
      curr: [entry.curr[0], entry.curr[1], entry.curr[2]],
    });
  }
}

/** Keep the last samples and only move the blend forward. Used when this frame ran zero steps. */
export function setInterpAlpha(nextAlpha: number): void {
  alpha = Math.min(1, Math.max(0, nextAlpha));
}

export function visualPosition(id: string, fallback: Vec3): Vec3 {
  const sample = samples.get(id);
  if (!sample) return fallback;
  const t = alpha;
  return [
    sample.prev[0] + (sample.curr[0] - sample.prev[0]) * t,
    sample.prev[1] + (sample.curr[1] - sample.prev[1]) * t,
    sample.prev[2] + (sample.curr[2] - sample.prev[2]) * t,
  ];
}

export function resetInterp(): void {
  alpha = 1;
  samples.clear();
}
