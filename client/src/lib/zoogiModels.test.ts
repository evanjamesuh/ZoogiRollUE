import test from "node:test";
import assert from "node:assert/strict";
import * as THREE from "three";
import { fittedBoxSize, fittedUniformScale, marbleUniformScale, resolveZoogiModel, rollMarble, ROSTER_REFERENCE_RADIUS, ZOOGI_MODELS, zoogiModelPreloadUrls } from "./zoogiModels.ts";
import { ZOOGI_DRAW_RADIUS } from "./restHeight.ts";

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
  assert.ok(zoogiModelPreloadUrls().includes("/models/lars.glb"), "Lars should preload with the other models");
  assert.equal(ZOOGI_MODELS.lars.scale, 1);
  assert.equal(resolveZoogiModel("nightshade")?.url, "/models/nightshade.glb");
  assert.ok(zoogiModelPreloadUrls().includes("/models/nightshade.glb"), "Nightshade should preload with the other models");
  assert.equal(ZOOGI_MODELS.nightshade.fit, "pivot");
  assert.equal(ZOOGI_MODELS.nightshade.referenceRadius, 0.958);
  assert.equal(ZOOGI_MODELS.nightshade.offset[1], 0);
  assert.ok(
    Math.abs(marbleUniformScale(0.5, ZOOGI_MODELS.nightshade) - 0.5 / 0.958) < 1e-9,
    "Nightshade should use the roster scale, not a bounding-box fit",
  );
  assert.equal(resolveZoogiModel("brand-new")?.url, "/models/brand-new.glb");
  assert.equal(resolveZoogiModel("custom_4", "https://example.com/a.glb")?.url, "https://example.com/a.glb");
  assert.equal(resolveZoogiModel("custom_4"), null);
});

test("roster models draw at the fitted bounding box, including Nightshade's pivot fit", () => {
  const marble = ZOOGI_DRAW_RADIUS;
  for (const [id, settings] of Object.entries(ZOOGI_MODELS)) {
    if (settings.fit === "pivot") {
      const raw = { x: ROSTER_REFERENCE_RADIUS * 2, y: 2.4, z: ROSTER_REFERENCE_RADIUS * 2 };
      const scale = fittedUniformScale(settings, marble, raw);
      assert.ok(Math.abs(scale - marble / 0.958) < 1e-9, `${id} scale ${scale}`);
      const box = fittedBoxSize(settings, marble, raw);
      const mesh = new THREE.Mesh(new THREE.BoxGeometry(raw.x, raw.y, raw.z));
      mesh.scale.setScalar(scale);
      const measured = new THREE.Box3().setFromObject(mesh).getSize(new THREE.Vector3());
      mesh.geometry.dispose();
      assert.ok(Math.abs(measured.x - box.x) < 1e-6, `${id} box x`);
      assert.ok(Math.abs(measured.x - marble * 2) < 1e-6, `${id} ball width ${measured.x}`);
      assert.ok(measured.y > marble * 2, `${id} horns stay taller than the ball`);
      continue;
    }
    const raw = { x: 1.2, y: 1.84, z: 1.5 };
    const box = fittedBoxSize(settings, marble, raw);
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(raw.x, raw.y, raw.z));
    mesh.scale.setScalar(fittedUniformScale(settings, marble, raw));
    const measured = new THREE.Box3().setFromObject(mesh).getSize(new THREE.Vector3());
    mesh.geometry.dispose();
    const longest = Math.max(measured.x, measured.y, measured.z);
    assert.ok(Math.abs(longest - marble * 2) < 1e-6, `${id} longest side ${longest}`);
    assert.ok(Math.abs(measured.y - box.y) < 1e-6, `${id} box`);
  }
});
