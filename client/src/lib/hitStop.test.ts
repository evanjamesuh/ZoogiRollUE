import test from "node:test";
import assert from "node:assert/strict";
import { useCameraEffects } from "./stores/useCameraEffects.tsx";

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
