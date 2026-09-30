import assert from "node:assert/strict";
import test from "node:test";
import { getHoodooDecor, getMapLayout } from "./arenaColliders.ts";
import { hoodoosOnLavaColliders } from "./lavaHoodooPlacement.ts";

test("lava hoodoo decor sits on the collider centers", () => {
  const layout = getMapLayout("lava");
  assert.ok(layout);
  const solids = layout.scenery.filter((solid) => solid.kind === "hoodoo");
  const decor = getHoodooDecor();
  assert.equal(decor.length, solids.length);
  assert.ok(solids.length > 0);
  for (const hoodoo of decor) {
    const solid = solids.find((candidate) => candidate.id === hoodoo.id);
    assert.ok(solid, hoodoo.id);
    assert.equal(hoodoo.position[0], solid.x);
    assert.equal(hoodoo.position[2], solid.z);
    assert.equal(hoodoo.radius, solid.radius);
  }

  const placed = hoodoosOnLavaColliders();
  assert.equal(placed.length, solids.length);
  for (const hoodoo of placed) {
    const solid = solids.find((candidate) => candidate.id === hoodoo.id);
    assert.ok(solid, hoodoo.id);
    assert.equal(hoodoo.position[0], solid.x);
    assert.equal(hoodoo.position[2], solid.z);
    assert.equal(hoodoo.radius, solid.radius);
  }
});
