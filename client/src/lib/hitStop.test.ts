import test from "node:test";
import assert from "node:assert/strict";
import {
  COLLISION_CAMERA_HOOK,
  KNOCKOFF_CAMERA_HOOK,
  triggerCollisionCameraEffect,
  triggerKnockoffCameraEffect,
  useCameraEffects,
} from "./stores/useCameraEffects.tsx";

test("a hit-stop ends on the next updates instead of sticking", () => {
  const fx = useCameraEffects.getState();
  fx.clearAllEffects();
  fx.triggerFreeze(0.04);
  assert.equal(useCameraEffects.getState().isFrozen, true);

  useCameraEffects.getState().update(0.016);
  assert.equal(useCameraEffects.getState().isFrozen, true);

  useCameraEffects.getState().update(0.05);
  const after = useCameraEffects.getState();
  assert.equal(after.isFrozen, false);
  assert.equal(after.freezeTimeLeft, 0);

  useCameraEffects.getState().update(0.05);
  assert.equal(useCameraEffects.getState().isFrozen, false);
  assert.equal(useCameraEffects.getState().freezeTimeLeft, 0);
});

test("knockouts and barrier hits shake the camera without freezing or slowing the sim", () => {
  assert.equal(KNOCKOFF_CAMERA_HOOK.effects.timeScale, undefined);
  assert.equal(KNOCKOFF_CAMERA_HOOK.effects.freezeFrame, undefined);
  assert.equal(COLLISION_CAMERA_HOOK.effects.timeScale, undefined);
  assert.equal(COLLISION_CAMERA_HOOK.effects.freezeFrame, undefined);
  assert.ok((KNOCKOFF_CAMERA_HOOK.effects.shake ?? 0) > 0);
  assert.ok((COLLISION_CAMERA_HOOK.effects.shake ?? 0) > 0);
  assert.ok((KNOCKOFF_CAMERA_HOOK.effects.zoom ?? 0) !== 0);

  const fx = useCameraEffects.getState();
  fx.clearAllEffects();
  triggerKnockoffCameraEffect();
  useCameraEffects.getState().update(0.016);
  assert.equal(useCameraEffects.getState().isFrozen, false);
  assert.equal(useCameraEffects.getState().computedTimeScale, 1);
  assert.ok(useCameraEffects.getState().computedShake > 0);

  fx.clearAllEffects();
  triggerCollisionCameraEffect(1);
  useCameraEffects.getState().update(0.016);
  assert.equal(useCameraEffects.getState().isFrozen, false);
  assert.equal(useCameraEffects.getState().computedTimeScale, 1);
  assert.ok(useCameraEffects.getState().computedShake > 0);
  fx.clearAllEffects();
});
