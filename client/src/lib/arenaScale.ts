/**
 * Every arena's playable diameter grows by the same factor.
 * Visual floors, colliders, spawns, and launch speed all read this
 * so the ring stays centred and the edges stay lined up.
 */
export const ARENA_SCALE = 1.5;

export function scalePlay(value: number): number {
  return value * ARENA_SCALE;
}
