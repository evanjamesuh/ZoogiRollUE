/**
 * One playfield multiplier per arena.
 *
 * Layout tables stay in today's units. getMapLayout, the neon court,
 * ice patches, and hoodoo positions multiply by these when they are read.
 * mapDefaultConfigs re-exports this table and stores arenaScale on each
 * map config that has one.
 */
export const ARENA_SCALE_BY_MAP: Record<string, number> = {
  grass: 1.5,
  ice: 1.5,
  lava: 1.5,
  space: 1.5,
  saturn: 1.5,
  neon: 1.5,
};

export function arenaScaleFor(mapId: string | null | undefined): number {
  if (!mapId) return ARENA_SCALE_BY_MAP.grass;
  return ARENA_SCALE_BY_MAP[mapId] ?? ARENA_SCALE_BY_MAP.grass;
}
