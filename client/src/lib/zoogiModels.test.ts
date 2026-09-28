import test from "node:test";
import assert from "node:assert/strict";
import * as THREE from "three";
import { resolveZoogiModel, rollMarble, ZOOGI_MODELS } from "./zoogiModels.ts";

function fresh(): THREE.Object3D {
  return new THREE.Object3D();
}

function topAfter(moves: Array<[number, number]>, radius = 0.5): THREE.Vector3 {
  const object = fresh();
  for (const [x, z] of moves) rollMarble(object, x, z, radius);
  return new THREE.Vector3(0, 1, 0).applyQuaternion(object.quaternion);
}

test("rolling forward tumbles over the facing direction", () => {
  const top = topAfter([
    [0, 0],
    [0, Math.PI / 2],
  ]);
  assert.ok(top.y < -0.99, `top should end underneath, got ${top.toArray()}`);
  assert.ok(Math.abs(top.x) < 1e-4 && Math.abs(top.z) < 1e-4);
});

test("rolling sideways tumbles around the other axis", () => {
  const top = topAfter([
    [0, 0],
    [Math.PI / 4, 0],
  ]);
  assert.ok(top.x > 0.99, `top should swing toward +X, got ${top.toArray()}`);
  assert.ok(Math.abs(top.z) < 1e-4);
});

test("a diagonal roll is not stuck on a single axis", () => {
  const top = topAfter([
    [0, 0],
    [0.2, 0.2],
  ]);
  assert.ok(top.x > 0.2, `expected travel in X, got ${top.toArray()}`);
  assert.ok(top.z > 0.2, `expected travel in Z, got ${top.toArray()}`);
});

test("a respawn jump stands the model back up instead of spinning", () => {
  const object = fresh();
  rollMarble(object, 0, 0, 0.5);
  rollMarble(object, 0, 0.4, 0.5);
  const tilted = object.quaternion.clone();
  rollMarble(object, 20, 0, 0.5);
  assert.ok(object.quaternion.angleTo(new THREE.Quaternion()) < 1e-6);
  assert.ok(tilted.angleTo(new THREE.Quaternion()) > 0.2);
});

test("roster models point at their glb, including lars", () => {
  assert.equal(resolveZoogiModel("wolfgang")?.url, "/models/wolfgang.glb");
  assert.equal(resolveZoogiModel("lars")?.url, "/models/lars.glb");
  assert.equal(ZOOGI_MODELS.lars.scale, 1);
  assert.equal(resolveZoogiModel("brand-new")?.url, "/models/brand-new.glb");
  assert.equal(resolveZoogiModel("custom_4", "https://example.com/a.glb")?.url, "https://example.com/a.glb");
  assert.equal(resolveZoogiModel("custom_4"), null);
});
