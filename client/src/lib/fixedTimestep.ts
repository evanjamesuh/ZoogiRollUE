/**
 * Fixed-step accumulator.
 *
 * The marble sim moves by a set amount per call and does not look at the
 * frame delta. Calling it once per rendered frame made 144 Hz monitors play
 * faster than 60 Hz, and a long frame moved the marbles only one step.
 *
 * Feed every frame's delta in. `steps` is how many 1/60 s sim calls to run.
 * `alpha` is how far the renderer is between the previous step and the
 * current one (0 = just stepped, 1 = the next step is due).
 */

export const FIXED_DT = 1 / 60;

/** A hitch may catch up this many steps, then leftover time is dropped. */
export const MAX_SUBSTEPS = 4;

export interface StepResult {
  accumulator: number;
  steps: number;
  alpha: number;
}

export function stepAccumulator(
  accumulator: number,
  frameDelta: number,
  timeScale = 1,
): StepResult {
  const scale = Number.isFinite(timeScale) ? Math.max(0, timeScale) : 1;
  const frame = Number.isFinite(frameDelta) ? Math.min(Math.max(frameDelta, 0), 0.1) : 0;
  let acc = accumulator + frame * scale;
  if (!Number.isFinite(acc) || acc < 0) acc = 0;

  let steps = 0;
  while (acc >= FIXED_DT && steps < MAX_SUBSTEPS) {
    acc -= FIXED_DT;
    steps += 1;
  }

  if (steps >= MAX_SUBSTEPS && acc > FIXED_DT) {
    acc = FIXED_DT * 0.999;
  }

  const alpha = FIXED_DT > 0 ? Math.min(1, Math.max(0, acc / FIXED_DT)) : 0;
  return { accumulator: acc, steps, alpha };
}
