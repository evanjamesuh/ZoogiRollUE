/**
 * One playfield multiplier per arena.
 *
 * Layout tables stay in today's units. getMapLayout, the neon court,
 * ice patches, and hoodoo positions multiply by these when they are read.
 * mapDefaultConfigs re-exports this table and stores arenaScale on each
 * map config that has one.
 *
 * The target playable diameter is 17.5 Zoogi diameters. A Zoogi's draw
 * diameter is 1.72, so the floor is about 30.1 world units across.
 * Each factor is that target divided by the map's authored floor diameter.
 * Night Circuit is a 3:2 rectangle: 1.613 makes the short side 15 Zoogi
 * diameters and the long side about 22.5.
 */
export const ARENA_SCALE_BY_MAP: Record<string, number> = {
  grass: 1.115,
  ice: 0.86,
  lava: 0.836,
  space: 0.99,
  saturn: 0.993,
  neon: 1.613,
};

export function arenaScaleFor(mapId: string | null | undefined): number {
  if (!mapId) return ARENA_SCALE_BY_MAP.grass;
  return ARENA_SCALE_BY_MAP[mapId] ?? ARENA_SCALE_BY_MAP.grass;
}
