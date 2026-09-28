import test from "node:test";
import assert from "node:assert/strict";
import {
  bindPowerAudioMute,
  playExplosion,
  playPowerUnlock,
  playStunEnd,
  playStunZap,
  playWolfDash,
} from "./powerSounds.ts";

test("power sounds stay silent and do not throw while muted", () => {
  bindPowerAudioMute(() => true);
  assert.doesNotThrow(() => {
    playPowerUnlock();
    playWolfDash();
    playExplosion();
    playStunZap();
    playStunEnd();
  });
});
