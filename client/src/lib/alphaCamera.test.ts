import test from "node:test";
import assert from "node:assert/strict";
import { ARENA_FOV_DEG } from "./cameraRig.ts";
import { FULL_PULL_DISTANCE } from "./simFeel.ts";
import { ZOOGI_REST_Y } from "./restHeight.ts";
import {
  AIM_DIST,
  AIM_PITCH_DEG,
  AIM_SCREEN_FRACTION,
  FALL_CAM_MIN_Y,
  FOLLOW_DIST,
  FOLLOW_PITCH_DEG,
  LAMBDA_RESET_DOLLY,
  LAMBDA_RESET_YAW,
  LAMBDA_RISE,
  LAMBDA_YAW_FOLLOW,
  YAW_FOLLOW_MAX_DEG_PER_SEC,
  YAW_FOLLOW_MIN_SPEED,
  type AlphaCamInput,
  type AlphaCamState,
  type AlphaTarget,
  biasedLookAt,
  dampYaw,
  distanceForPull,
  formatAlphaCamDebug,
  framingScale,
  headingTowardCenter,
  matchActorKey,
  offsetFromPose,
  rigLengths,
  screenFractionDown,
  stepAlphaCamera,
  viewNdc,
  wrapAngle,
} from "./alphaCamera.ts";

const FOV = ARENA_FOV_DEG;
const DT = 1 / 60;

function target(over: Partial<AlphaTarget> = {}): AlphaTarget {
  return { id: "player", x: 0, y: ZOOGI_REST_Y, z: 12, vx: 0, vz: 0, ...over };
}

function input(over: Partial<AlphaCamInput> = {}): AlphaCamInput {
  return {
    dt: DT,
    actorKey: "1:p",
    target: target(),
    isAiming: false,
    turnHasLaunched: false,
    fallen: false,
    fovDeg: FOV,
    arenaScale: 1,
    onCourt: false,
    ...over,
  };
}

function run(state: AlphaCamState | null, over: Partial<AlphaCamInput>, seconds: number, dt = DT): AlphaCamState {
  const steps = Math.max(1, Math.round(seconds / dt));
  let s = state;
  for (let i = 0; i < steps; i++) s = stepAlphaCamera(s, input({ ...over, dt }));
  return s!;
}

test("A2 A3 aim pose is 30 degrees and 7D at fov 60, about 10D at the match fov", () => {
  assert.equal(AIM_PITCH_DEG, 30);
  assert.equal(AIM_DIST, 7);
  assert.equal(FOLLOW_PITCH_DEG, 55);
  assert.equal(FOLLOW_DIST, 24);
  assert.ok(Math.abs(framingScale(60) - 1) < 1e-9);

  const at60 = rigLengths(60);
  const offset = offsetFromPose({ yaw: 0, pitch: at60.aimPitch, distance: at60.aimDist });
  assert.ok(Math.abs(offset.y - 3.5) < 1e-9, `up ${offset.y}`);
  assert.ok(Math.abs(-offset.z - 7 * Math.cos(Math.PI / 6)) < 1e-9, `back ${-offset.z}`);
  const pitch = Math.atan2(offset.y, Math.hypot(offset.x, offset.z)) * (180 / Math.PI);
  assert.ok(Math.abs(pitch - 30) < 1e-6);

  const match = rigLengths(FOV);
  assert.ok(Math.abs(match.aimDist - 10) < 0.05, `match aim dist ${match.aimDist}`);
  assert.ok(match.followDist > match.aimDist);
  assert.ok(Math.abs(match.followDist / match.aimDist - FOLLOW_DIST / AIM_DIST) < 1e-9);
});

test("arena scale multiplies aim and follow distances", () => {
  const unit = rigLengths(FOV, 1);
  const doubled = rigLengths(FOV, 2);
  assert.ok(Math.abs(doubled.aimDist - unit.aimDist * 2) < 1e-9);
  assert.ok(Math.abs(doubled.followDist - unit.followDist * 2) < 1e-9);
});

test("A1 A5 the aim camera sits behind the marble, facing the arena center", () => {
  const pose = stepAlphaCamera(null, input());
  assert.equal(pose.mode, "aim");
  const yaw = headingTowardCenter(0, 12);
  assert.ok(Math.abs(wrapAngle(pose.yaw - yaw)) < 1e-6);
  assert.ok(pose.camZ > 12, "camera is further out than the marble on +Z");
  assert.ok(pose.camY > ZOOGI_REST_Y);
  const aheadX = Math.sin(pose.yaw);
  const aheadZ = Math.cos(pose.yaw);
  assert.ok(aheadZ < -0.99 && Math.abs(aheadX) < 1e-6, "aim points toward the origin");
});

test("A4 the marble rests just below screen center in the aim pose", () => {
  const pose = stepAlphaCamera(null, input());
  const ndc = viewNdc(
    { x: pose.camX, y: pose.camY, z: pose.camZ },
    { x: pose.lookX, y: pose.lookY, z: pose.lookZ },
    { x: 0, y: ZOOGI_REST_Y, z: 12 },
    FOV,
  );
  const fraction = screenFractionDown(ndc.y);
  assert.ok(ndc.inFront);
  assert.ok(fraction > 0.52 && fraction < 0.55, `fraction ${fraction}`);
  assert.ok(Math.abs(fraction - AIM_SCREEN_FRACTION) < 0.01);
  const look = biasedLookAt(
    { x: pose.camX, y: pose.camY, z: pose.camZ },
    { x: 0, y: ZOOGI_REST_Y, z: 12 },
    FOV,
  );
  assert.ok(Math.abs(look.y - pose.lookY) < 1e-6);
});

test("A5 a marble on the court reuses its last aim heading; a pad faces center", () => {
  let pose = run(null, { onCourt: true, target: target({ x: 0, z: 8 }) }, DT);
  pose = run(pose, {
    onCourt: true,
    turnHasLaunched: true,
    target: target({ x: 0, z: 8, vx: 20, vz: 0 }),
  }, 0.1);
  assert.equal(pose.mode, "follow");
  assert.ok(Math.abs(wrapAngle(pose.lastHeading.player - Math.PI / 2)) < 1e-6);

  const cutAway = stepAlphaCamera(pose, input({
    actorKey: "1:e0",
    onCourt: false,
    target: target({ id: "enemy", x: 0, z: 26 }),
  }));
  assert.equal(cutAway.mode, "cut");
  assert.ok(Math.abs(wrapAngle(cutAway.aimYaw - Math.PI)) < 1e-6 || Math.abs(wrapAngle(cutAway.aimYaw - headingTowardCenter(0, 26))) < 1e-6);

  const back = stepAlphaCamera(cutAway, input({
    actorKey: "1:p",
    onCourt: true,
    target: target({ x: 2, z: 4 }),
  }));
  assert.equal(back.mode, "cut");
  assert.ok(Math.abs(wrapAngle(back.aimYaw - Math.PI / 2)) < 1e-6, `aim yaw ${back.aimYaw}`);
  assert.equal(back.targetId, "player");
});

test("A7 a drag dollies out along the same heading and a cancel returns", () => {
  const aim = stepAlphaCamera(null, input());
  const yaw = aim.yaw;
  const pitch = aim.pitch;
  const dragging = run(aim, { isAiming: true, target: target({ vx: 30, vz: -10 }) }, 1);
  assert.equal(dragging.mode, "aim");
  assert.ok(Math.abs(wrapAngle(dragging.yaw - yaw)) < 1e-6, "drag does not orbit");
  assert.ok(Math.abs(dragging.pitch - pitch) < 1e-6, "drag keeps pitch");
  assert.ok(dragging.distance > aim.distance + 10, `dist ${dragging.distance}`);

  const needed = distanceForPull({ pullDistance: FULL_PULL_DISTANCE, pitch, fovDeg: FOV });
  assert.ok(Math.abs(dragging.distance - Math.max(aim.distance, needed)) < 0.15);

  const pull = {
    x: 0 - Math.sin(yaw) * FULL_PULL_DISTANCE,
    y: 0,
    z: 12 - Math.cos(yaw) * FULL_PULL_DISTANCE,
  };
  const cam = { x: dragging.camX, y: dragging.camY, z: dragging.camZ };
  const look = { x: dragging.lookX, y: dragging.lookY, z: dragging.lookZ };
  const ndc = viewNdc(cam, look, pull, FOV);
  assert.ok(ndc.inFront, "full pull is in front of the camera");
  assert.ok(ndc.y > -1 && ndc.y < -0.7, `pull ndc ${ndc.y}`);

  const aimNdc = viewNdc(
    { x: aim.camX, y: aim.camY, z: aim.camZ },
    { x: aim.lookX, y: aim.lookY, z: aim.lookZ },
    pull,
    FOV,
  );
  assert.ok(!aimNdc.inFront || aimNdc.y < -1, "aim pose cannot see a full pull");

  const cancelled = run(dragging, { isAiming: false }, 1);
  assert.ok(Math.abs(cancelled.distance - aim.distance) < 0.2, `returned dist ${cancelled.distance}`);
  assert.ok(Math.abs(wrapAngle(cancelled.yaw - yaw)) < 1e-4);
});

test("A7 a drag during the reset swoop does not yaw", () => {
  let pose = run(null, { turnHasLaunched: true, target: target({ vx: 0, vz: -20 }) }, 6);
  assert.equal(pose.mode, "follow");
  const highDist = pose.distance;
  pose = stepAlphaCamera(pose, input({
    actorKey: "1:e0",
    target: target({ id: "enemy", x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }));
  const cutYaw = pose.yaw;
  const dragged = run(pose, {
    actorKey: "1:e0",
    isAiming: true,
    target: target({ id: "enemy", x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }, 0.5);
  assert.ok(Math.abs(wrapAngle(dragged.yaw - cutYaw)) < 1e-6, "swoop yaw pauses while dragging");
  assert.ok(dragged.distance >= highDist - 0.2);
});

test("B12 the follow rise is about 60% at 1.3s and about 93% at 3.7s", () => {
  const aim = stepAlphaCamera(null, input());
  const lengths = rigLengths(FOV);
  const at13 = run(aim, { turnHasLaunched: true, target: target({ vx: 0, vz: -15 }) }, 1.3);
  const rise = (at13.distance - lengths.aimDist) / (lengths.followDist - lengths.aimDist);
  const closed = 1 - Math.exp(-LAMBDA_RISE * 1.3);
  assert.ok(Math.abs(rise - closed) < 0.02, `rise ${rise} closed ${closed}`);
  assert.ok(rise > 0.55 && rise < 0.65, `60% band ${rise}`);

  const at37 = run(aim, { turnHasLaunched: true, target: target({ vx: 0, vz: -15 }) }, 3.7);
  const later = (at37.distance - lengths.aimDist) / (lengths.followDist - lengths.aimDist);
  assert.ok(later > 0.9 && later < 0.96, `93% band ${later}`);
  assert.equal(at37.mode, "follow");
});

test("B13 XZ follow lags the marble while the look-at stays on it", () => {
  const aim = stepAlphaCamera(null, input({ target: target({ x: 0, z: 0 }), onCourt: true }));
  const moved = run(aim, { target: target({ x: 8, z: 0 }), onCourt: true }, 0.4);
  const caught = 8 * (1 - Math.exp(-2.5 * 0.4));
  assert.ok(Math.abs(moved.anchorX - caught) < 0.05, `anchor ${moved.anchorX} expected ${caught}`);
  assert.ok(moved.camX < 8 - 1, "camera trails the marble");
  assert.ok(Math.abs(moved.lookX - 8) < 0.5, `look ${moved.lookX}`);
});

test("B14 a straight shot keeps its heading; a bounce yaws slowly and never snaps", () => {
  const aim = stepAlphaCamera(null, input());
  const straight = run(aim, { turnHasLaunched: true, target: target({ vx: 0, vz: -18 }) }, 2);
  assert.ok(Math.abs(wrapAngle(straight.yaw - aim.yaw)) < 1e-3, `yaw drifted ${straight.yaw}`);

  const bounced = run(straight, { turnHasLaunched: true, target: target({ vx: 18, vz: 0 }) }, 1);
  const turned = Math.abs(wrapAngle(bounced.yaw - straight.yaw));
  const cap = (YAW_FOLLOW_MAX_DEG_PER_SEC * Math.PI) / 180;
  assert.ok(Math.abs(turned - cap) < 0.02, `turned ${(turned * 180) / Math.PI}°`);
  assert.ok(turned < Math.PI / 2, "did not snap the long way");
  assert.equal(LAMBDA_YAW_FOLLOW, 0.5);
  assert.ok(YAW_FOLLOW_MIN_SPEED === 0.5);

  const slow = run(bounced, { turnHasLaunched: true, target: target({ vx: 0.2, vz: 0 }) }, 1);
  assert.ok(Math.abs(wrapAngle(slow.yaw - bounced.yaw)) < 1e-4, "below 0.5 u/s the heading holds");
});

test("B14 yaw takes the short way around and respects the rate cap at 30 and 60 Hz", () => {
  const start = 170 * Math.PI / 180;
  const goal = -170 * Math.PI / 180;
  const once = dampYaw(start, goal, 0.5, 1 / 60, (20 * Math.PI) / 180);
  assert.ok(wrapAngle(once - start) > 0, "short way is +20°, not the long way back");
  assert.ok(Math.abs(wrapAngle(once - start)) < (21 * Math.PI) / 180);

  let a = 0;
  let b = 0;
  const targetYaw = Math.PI / 2;
  for (let i = 0; i < 60; i++) a = dampYaw(a, targetYaw, 0.5, 1 / 60, (20 * Math.PI) / 180);
  for (let i = 0; i < 30; i++) b = dampYaw(b, targetYaw, 0.5, 1 / 30, (20 * Math.PI) / 180);
  assert.ok(Math.abs(a - b) < 1e-6, `60Hz ${a} 30Hz ${b}`);
  assert.ok(Math.abs(a - (20 * Math.PI) / 180) < 1e-6);
});

test("B15 the camera follows a fall down to the floor clamp, then holds when fallen", () => {
  const aim = stepAlphaCamera(null, input());
  const dropped = run(aim, { target: target({ y: -30 }) }, 3);
  assert.ok(Math.abs(dropped.camY - FALL_CAM_MIN_Y) < 1e-4, `camY ${dropped.camY}`);
  assert.ok(dropped.lookY < -20, `look still tracks the marble ${dropped.lookY}`);

  const falling = run(aim, { turnHasLaunched: true, target: target({ y: -1, vx: 0, vz: -12 }) }, 0.5);
  const held = run(falling, {
    turnHasLaunched: true,
    fallen: true,
    target: target({ x: 40, y: -30, z: -20, vx: 0, vz: -12 }),
  }, 1);
  assert.equal(held.mode, "hold");
  assert.equal(held.camX, falling.camX);
  assert.equal(held.camY, falling.camY);
  assert.equal(held.camZ, falling.camZ);
  assert.equal(held.lookX, falling.lookX);
  assert.equal(held.yaw, falling.yaw);
});

test("C17 C18 a turn change hard-cuts, then swoops yaw faster than the dolly", () => {
  let pose = run(null, { turnHasLaunched: true, target: target({ vx: 0, vz: -16 }) }, 8);
  const overviewDist = pose.distance;
  const overviewPitch = pose.pitch;
  const overviewYaw = pose.yaw;
  const cut = stepAlphaCamera(pose, input({
    actorKey: "2:p",
    onCourt: false,
    target: target({ x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }));
  assert.equal(cut.mode, "cut");
  assert.equal(cut.distance, overviewDist);
  assert.equal(cut.pitch, overviewPitch);
  assert.equal(cut.yaw, overviewYaw);
  const offset = offsetFromPose({ yaw: overviewYaw, pitch: overviewPitch, distance: overviewDist });
  assert.ok(Math.abs(cut.camX - (10 + offset.x)) < 1e-6);
  assert.ok(Math.abs(cut.camZ - (0 + offset.z)) < 1e-6);
  assert.ok(Math.abs(cut.lookX - 10) < 1e-6, "first frame looks at the new marble");

  const yawTarget = headingTowardCenter(10, 0);
  const half = run(cut, {
    actorKey: "2:p",
    onCourt: false,
    target: target({ x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }, 0.5);
  const yawFrac = 1 - Math.abs(wrapAngle(yawTarget - half.yaw)) / Math.abs(wrapAngle(yawTarget - overviewYaw));
  const yawClosed = 1 - Math.exp(-LAMBDA_RESET_YAW * 0.5);
  assert.ok(Math.abs(yawFrac - yawClosed) < 0.03, `yaw frac ${yawFrac} closed ${yawClosed}`);
  assert.ok(yawFrac > 0.85 && yawFrac < 0.95);

  const dolly = run(cut, {
    actorKey: "2:p",
    onCourt: false,
    target: target({ x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }, 1.2);
  const lengths = rigLengths(FOV);
  const distFrac = (overviewDist - dolly.distance) / (overviewDist - lengths.aimDist);
  const distClosed = 1 - Math.exp(-LAMBDA_RESET_DOLLY * 1.2);
  assert.ok(Math.abs(distFrac - distClosed) < 0.03, `dist frac ${distFrac}`);
  assert.ok(distFrac > 0.85 && distFrac < 0.95);

  const settled = run(cut, {
    actorKey: "2:p",
    onCourt: false,
    target: target({ x: 10, y: ZOOGI_REST_Y, z: 0 }),
  }, 2.4);
  assert.equal(settled.mode, "aim");
  assert.ok(Math.abs(settled.distance - lengths.aimDist) < 0.1);
});

test("C17 the same marble in a new round still cuts, and rest alone does not", () => {
  const aim = stepAlphaCamera(null, input());
  const resting = run(aim, { turnHasLaunched: false, target: target({ x: 3, z: 9 }) }, 1);
  assert.equal(resting.mode, "aim");
  assert.equal(resting.actorKey, "1:p");

  const nextRound = stepAlphaCamera(resting, input({ actorKey: "2:p", target: target({ x: 3, z: 9 }) }));
  assert.equal(nextRound.mode, "cut");
  assert.equal(matchActorKey({
    gameMode: "classic",
    currentLocalPlayerIndex: 0,
    isPlayerTurn: true,
    turnIndex: 0,
    currentRound: 2,
  }), "2:p");
  assert.equal(matchActorKey({
    gameMode: "local_multiplayer",
    currentLocalPlayerIndex: 1,
    isPlayerTurn: false,
    turnIndex: 3,
    currentRound: 4,
  }), "4:1");
  assert.equal(matchActorKey({
    gameMode: "classic",
    currentLocalPlayerIndex: 0,
    isPlayerTurn: false,
    turnIndex: 2,
    currentRound: 1,
  }), "1:e2");
});

test("one slow frame matches two fast frames while the rise is uncapped", () => {
  const aim = stepAlphaCamera(null, input());
  const shot = input({ turnHasLaunched: true, target: target({ vx: 0, vz: -12 }) });
  const slow = stepAlphaCamera(aim, { ...shot, dt: 1 / 30 });
  const mid = stepAlphaCamera(aim, { ...shot, dt: 1 / 60 });
  const fast = stepAlphaCamera(mid, { ...shot, dt: 1 / 60 });
  assert.ok(Math.abs(slow.distance - fast.distance) < 1e-6, `slow ${slow.distance} fast ${fast.distance}`);
  assert.ok(Math.abs(slow.pitch - fast.pitch) < 1e-6);
  assert.ok(Math.abs(slow.camZ - fast.camZ) < 1e-5);
});

test("debug readout names the state, target, pitch, distance, and yaw", () => {
  const pose = stepAlphaCamera(null, input());
  const text = formatAlphaCamDebug(pose);
  assert.match(text, /^AIM  target=player  pitch=30\.0°  dist=\d+\.\d  yaw=/);
});
