import { getHoodooDecor, getMapLayout, type HoodooDecor } from "./arenaColliders.ts";

/**
 * Places each lava hoodoo on the scaled scenery circle the match colliders use.
 * Scale, yaw, and the base lift still come from the decor entry for that id.
 */
export function hoodoosOnLavaColliders(): HoodooDecor[] {
  const visuals = new Map(getHoodooDecor().map((hoodoo) => [hoodoo.id, hoodoo]));
  const solids = (getMapLayout("lava")?.scenery ?? []).filter((solid) => solid.kind === "hoodoo");
  return solids.map((solid) => {
    const visual = visuals.get(solid.id);
    const scale = visual && Number.isFinite(visual.scale) && visual.scale !== 0 ? visual.scale : 1;
    return {
      id: solid.id,
      position: [solid.x, visual?.position[1] ?? 0, solid.z],
      scale,
      rotation: visual?.rotation ?? 0,
      radius: solid.radius,
    };
  });
}
