import test from "node:test";
import assert from "node:assert/strict";
import { createSpritePool } from "./pool.ts";
import { mulberry32 } from "./random.ts";
import { seedBlastEmbers, seedBlastSmoke, stepEmbers, stepSmoke } from "./sim.ts";

test("blast particles stay pooled and embers cool", () => {
  const smoke = createSpritePool(48);
  const embers = createSpritePool(64);
  const before = embers.g.length;
  seedBlastSmoke(smoke, 8, mulberry32(11), 0.5);
  seedBlastEmbers(embers, 8, mulberry32(29), 0.5);
  assert.ok(smoke.alive > 20);
  assert.ok(embers.alive > 30);
  const first = embers.active.findIndex((flag) => flag === 1);
  const greenAtStart = embers.g[first];

  for (let frame = 0; frame < 40; frame++) {
    stepSmoke(smoke, frame / 60, 1 / 60);
    stepEmbers(embers, frame / 60, 1 / 60);
  }

  assert.equal(embers.g.length, before);
  assert.equal(smoke.px.length, 48);
  assert.ok(embers.g[first] < greenAtStart);

  let outside = 0;
  let seen = 0;
  for (let i = 0; i < smoke.capacity; i++) {
    if (smoke.active[i] === 0) continue;
    seen += 1;
    const dist = Math.hypot(smoke.px[i], smoke.pz[i]);
    const half = smoke.size[i] * 0.5;
    if (dist + half > 8 * 1.15) outside += 1;
  }
  assert.ok(seen > 0);
  assert.equal(outside, 0, "smoke should stay inside the push radius");
});

test("grenade-sized blasts use the same sim at a smaller radius", () => {
  const embers = createSpritePool(64);
  seedBlastEmbers(embers, 4, mulberry32(3), 0.5);
  for (let frame = 0; frame < 50; frame++) stepEmbers(embers, frame / 60, 1 / 60);
  for (let i = 0; i < embers.capacity; i++) {
    if (embers.active[i] === 0) continue;
    const dist = Math.hypot(embers.px[i], embers.pz[i]);
    assert.ok(dist < 4 * 1.2, `ember traveled ${dist}`);
  }
});
