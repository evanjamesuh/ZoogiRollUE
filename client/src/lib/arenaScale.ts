/**
 * One playfield multiplier per arena.
 *
 * Layout tables stay in authored units. getMapLayout, the neon court,
 * ice patches, and hoodoo positions multiply by these when they are read.
 * mapDefaultConfigs re-exports this table and stores arenaScale on each
 * map config that has one. Change a map by editing its number here.
 *
 * Every factor is 1. That is the final size: Meadow's knockoff of 15.5
 * is about 31 Zoogi widths across, which matches the alpha. Cosmic
 * Platform's stage model is shifted and scaled onto the 15.6 lip
 * separately from this factor.
 */
export const ARENA_SCALE_BY_MAP: Record<string, number> = {
  grass: 1,
  ice: 1,
  lava: 1,
  space: 1,
  saturn: 1,
  tomb: 1,
  neon: 1,
};

export function arenaScaleFor(mapId: string | null | undefined): number {
  if (!mapId) return ARENA_SCALE_BY_MAP.grass;
  return ARENA_SCALE_BY_MAP[mapId] ?? ARENA_SCALE_BY_MAP.grass;
}
