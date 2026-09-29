/**
 * One playfield multiplier per arena.
 *
 * Layout tables stay in today's units. getMapLayout, the neon court,
 * ice patches, and hoodoo positions multiply by these when they are read.
 * mapDefaultConfigs re-exports this table and stores arenaScale on each
 * map config that has one.
 *
 * Meadow, Frozen Ring, Arabian Nights, and Pharaoh's Tomb are already about
 * 18 Zoogi diameters across (floor radius 15.2–15.5), so their factor is 1
 * and the new round art stays on the collider. Cosmic Platform's factor is
 * also 1; its stage model is shifted and scaled onto the 15.6 lip separately.
 * Volcanic Pit is still authored at radius 18; 0.836 brings that floor to
 * about 17.5 Zoogi diameters. Night Circuit is a 3:2 rectangle: 1.613
 * makes the short side 15 Zoogi diameters and the long side about 22.5.
 */
export const ARENA_SCALE_BY_MAP: Record<string, number> = {
  grass: 1,
  ice: 1,
  lava: 0.836,
  space: 1,
  saturn: 1,
  tomb: 1,
  neon: 1.613,
};

export function arenaScaleFor(mapId: string | null | undefined): number {
  if (!mapId) return ARENA_SCALE_BY_MAP.grass;
  return ARENA_SCALE_BY_MAP[mapId] ?? ARENA_SCALE_BY_MAP.grass;
}
