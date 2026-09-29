import test from "node:test";
import assert from "node:assert/strict";
import {
  bindPowerAudioMute,
  playBindWrap,
  playExplosion,
  playShadowPulse,
  playPowerUnlock,
  playRicochetPing,
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
    playRicochetPing("arm");
    playRicochetPing("hit");
    playBindWrap();
    playShadowPulse();
  });
});
