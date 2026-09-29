import test from "node:test";
import assert from "node:assert/strict";
import { createSpritePool } from "./pool.ts";
import { mulberry32 } from "./random.ts";
import { seedBlastEmbers, seedBlastFire, seedBlastSmoke, seedBurstSparks, seedDustPuff, stepDust, stepEmbers, stepFire, stepGlints, stepSmoke } from "./sim.ts";

test("blast particles stay pooled, smoke rises, and embers cool", () => {
  const smoke = createSpritePool(28);
  const fire = createSpritePool(14);
  const embers = createSpritePool(42);
  const before = embers.g.length;
  seedBlastSmoke(smoke, 8, mulberry32(11), 0.5);
  seedBlastFire(fire, 8, mulberry32(17), 0.5);
  seedBlastEmbers(embers, 8, mulberry32(29), 0.5);
  assert.ok(smoke.alive >= 8);
  assert.ok(fire.alive >= 10);
  assert.ok(embers.alive >= 30);
  const first = embers.active.findIndex((flag) => flag === 1);
  const greenAtStart = embers.g[first];

  for (let frame = 0; frame < 48; frame++) {
    stepSmoke(smoke, frame / 60, 1 / 60);
    stepFire(fire, frame / 60, 1 / 60);
    stepEmbers(embers, frame / 60, 1 / 60);
  }

  assert.equal(embers.g.length, before);
  assert.equal(smoke.px.length, 28);
  assert.ok(embers.g[first] < greenAtStart);

  let outside = 0;
  let seen = 0;
  let top = 0;
  for (let i = 0; i < smoke.capacity; i++) {
    if (smoke.active[i] === 0) continue;
    seen += 1;
    top = Math.max(top, smoke.py[i] + smoke.size[i] * 0.5);
    const dist = Math.hypot(smoke.px[i], smoke.pz[i]);
    const half = smoke.size[i] * 0.5;
    if (dist + half > 8 * 1.35) outside += 1;
  }
  assert.ok(seen > 0);
  assert.ok(top > 3, `smoke top ${top} should clear the marbles`);
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

test("impact sparks and dust stay inside their pools", () => {
  const sparks = createSpritePool(18);
  const dust = createSpritePool(10);
  const rand = mulberry32(9);
  seedBurstSparks(sparks, rand, 14, [1, 0.8, 0.4], [1, 0.2, 0]);
  seedDustPuff(dust, rand, 8);
  assert.ok(sparks.alive >= 10);
  assert.ok(dust.alive >= 6);
  assert.equal(sparks.px.length, 18);
  const startX = sparks.px[0];
  for (let frame = 0; frame < 8; frame++) {
    stepGlints(sparks, frame / 60, 1 / 60);
    stepDust(dust, frame / 60, 1 / 60);
  }
  assert.ok(sparks.px[0] !== startX);
  assert.ok(dust.py[0] >= 0.04);
});
