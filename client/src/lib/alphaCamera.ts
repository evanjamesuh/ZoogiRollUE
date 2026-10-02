/**
 * Alpha match camera: aim pose, follow the shot, hold if it falls out,
 * then a hard cut and an eased swoop into the next turn's aim pose.
 *
 * Distances are authored at a 60° vertical FOV (AIM_DIST, FOLLOW_DIST).
 * The match camera is 44°, so framingScale stretches them until a Zoogi
 * fills the frame the way it did in the alpha. arenaScale multiplies
 * those distances when a map is scaled. The slingshot pull stays in
 * world units.
 *
 * Easing is the shared damp(): x += (target - x) * (1 - exp(-λ dt)).
 * Yaw takes the short way around. Callers clamp dt to 0.05 s; this step
 * does too, so 30 Hz and 144 Hz land on the same pose.
 */

import { damp } from "./cameraRig";
import { FULL_PULL_DISTANCE } from "./simFeel";
import { ZOOGI_REST_Y } from "./restHeight";

export const AIM_PITCH_DEG = 30;
export const AIM_DIST = 7;
export const FOLLOW_PITCH_DEG = 55;
export const FOLLOW_DIST = 24;
export const REF_FOV_DEG = 60;

export const LAMBDA_RISE = 0.7;
export const LAMBDA_POS = 2.5;
export const LAMBDA_LOOK = 10;
export const LAMBDA_YAW_FOLLOW = 0.5;
export const YAW_FOLLOW_MAX_DEG_PER_SEC = 20;
export const YAW_FOLLOW_MIN_SPEED = 0.5;
export const LAMBDA_RESET_YAW = 4.6;
export const LAMBDA_RESET_DOLLY = 1.9;
export const LAMBDA_DRAG = 6;
export const FALL_CAM_MIN_Y = -8;

/** Marble sits this far down the aim frame (0 top, 1 bottom). */
export const AIM_SCREEN_FRACTION = 0.535;

/** Full-pull point is kept this far inside the vertical edge while dragging. */
export const PULL_VIEW_MARGIN = 0.12;

const DT_CAP = 0.05;
const YAW_FOLLOW_MAX_RAD = (YAW_FOLLOW_MAX_DEG_PER_SEC * Math.PI) / 180;

export type AlphaCamMode = "aim" | "follow" | "hold" | "cut";

export interface Vec3 {
  x: number;
  y: number;
  z: number;
}

export interface AlphaTarget {
  id: string;
  x: number;
  y: number;
  z: number;
  /** Planar velocity, world units per second. */
  vx: number;
  vz: number;
}

export interface AlphaCamInput {
  dt: number;
  actorKey: string;
  target: AlphaTarget;
  /** Slingshot drag is down. Heading stays put for the whole drag. */
  isAiming: boolean;
  turnHasLaunched: boolean;
  /** Knocked out, respawning, or below FALL_OUT_Y. Hold the last pose. */
  fallen: boolean;
  fovDeg: number;
  arenaScale: number;
  /** Inside the floor. A pad outside the court faces the arena center. */
  onCourt: boolean;
  /** Ability zoom. 1 leaves distances alone. */
  zoom?: number;
}

export interface AlphaCamState {
  mode: AlphaCamMode;
  actorKey: string;
  targetId: string;
  /** Aim heading in radians. 0 looks toward +Z; the camera sits on -Z. */
  yaw: number;
  /** Radians down from the horizon. */
  pitch: number;
  distance: number;
  /** Heading the reset swoop is easing toward. */
  aimYaw: number;
  anchorX: number;
  anchorY: number;
  anchorZ: number;
  camX: number;
  camY: number;
  camZ: number;
  lookX: number;
  lookY: number;
  lookZ: number;
  lastHeading: Record<string, number>;
}

export interface RigLengths {
  aimDist: number;
  followDist: number;
  aimPitch: number;
  followPitch: number;
}

export function wrapAngle(rad: number): number {
  const tau = Math.PI * 2;
  let a = (rad + Math.PI) % tau;
  if (a < 0) a += tau;
  return a - Math.PI;
}

/** Shortest-angle damp. Optional cap is radians per second. */
export function dampYaw(
  current: number,
  target: number,
  lambda: number,
  dt: number,
  maxRadPerSec?: number,
): number {
  if (!Number.isFinite(dt) || dt <= 0) return current;
  const delta = wrapAngle(target - current);
  let step = delta * (1 - Math.exp(-Math.max(0, lambda) * dt));
  if (maxRadPerSec !== undefined && Number.isFinite(maxRadPerSec)) {
    const cap = Math.max(0, maxRadPerSec) * dt;
    if (step > cap) step = cap;
    else if (step < -cap) step = -cap;
  }
  return wrapAngle(current + step);
}

/** 60° distances times this match a narrower match FOV. */
export function framingScale(fovDeg: number): number {
  const fov = Number.isFinite(fovDeg) && fovDeg > 2 ? fovDeg : REF_FOV_DEG;
  const ref = Math.tan(((REF_FOV_DEG * Math.PI) / 180) / 2);
  const actual = Math.tan(((fov * Math.PI) / 180) / 2);
  return ref / actual;
}

export function rigLengths(fovDeg: number, arenaScale = 1, zoom = 1): RigLengths {
  const scale = Number.isFinite(arenaScale) && arenaScale > 0 ? arenaScale : 1;
  const zoomScale = Number.isFinite(zoom) && zoom > 0 ? zoom : 1;
  const frame = framingScale(fovDeg) * scale * zoomScale;
  return {
    aimDist: AIM_DIST * frame,
    followDist: FOLLOW_DIST * frame,
    aimPitch: (AIM_PITCH_DEG * Math.PI) / 180,
    followPitch: (FOLLOW_PITCH_DEG * Math.PI) / 180,
  };
}

/** Direction from the marble toward the arena origin, as an aim yaw. */
export function headingTowardCenter(x: number, z: number): number {
  if (Math.hypot(x, z) < 1e-3) return 0;
  return Math.atan2(-x, -z);
}

export function headingFromVelocity(vx: number, vz: number): number {
  return Math.atan2(vx, vz);
}

/**
 * Active-actor key. The cut fires when this changes, including the round,
 * not when the court merely goes still.
 */
export function matchActorKey(state: {
  gameMode: string;
  currentLocalPlayerIndex: number;
  isPlayerTurn: boolean;
  turnIndex: number;
  currentRound: number;
}): string {
  const who = state.gameMode === "local_multiplayer"
    ? String(state.currentLocalPlayerIndex)
    : (state.isPlayerTurn ? "p" : `e${state.turnIndex}`);
  return `${state.currentRound}:${who}`;
}

export interface Pose {
  yaw: number;
  pitch: number;
  distance: number;
}

/** Camera offset from the look anchor. Yaw 0 aims at +Z, so the camera sits on -Z. */
export function offsetFromPose(pose: Pose): Vec3 {
  const horiz = Math.cos(pose.pitch) * pose.distance;
  return {
    x: -Math.sin(pose.yaw) * horiz,
    y: Math.sin(pose.pitch) * pose.distance,
    z: -Math.cos(pose.yaw) * horiz,
  };
}

/**
 * Distance at which a ground point `pullDistance` behind the marble sits
 * inside the vertical FOV. Pitch stays put; back and up both grow with distance.
 */
export function distanceForPull(args: {
  pullDistance: number;
  pitch: number;
  fovDeg: number;
  marbleY?: number;
  margin?: number;
}): number {
  const pitch = args.pitch;
  const s = Math.max(0, args.pullDistance);
  const m = args.marbleY ?? ZOOGI_REST_Y;
  const margin = args.margin ?? PULL_VIEW_MARGIN;
  const half = ((args.fovDeg * Math.PI) / 180) / 2;
  const T = Math.tan(half) * (1 - Math.min(0.45, Math.max(0, margin)));
  if (!(T > 1e-4)) return s * 4;
  const sin = Math.sin(pitch);
  const cos = Math.cos(pitch);
  const d = (s * (sin + T * cos) + m * cos) / T - m * sin;
  return Math.max(0, d);
}

/** OpenGL-style NDC. +Y is the top of the frame. `inFront` is false behind the camera. */
export function viewNdc(cam: Vec3, look: Vec3, point: Vec3, fovDeg: number): { x: number; y: number; inFront: boolean } {
  let zx = cam.x - look.x;
  let zy = cam.y - look.y;
  let zz = cam.z - look.z;
  const zl = Math.hypot(zx, zy, zz) || 1;
  zx /= zl;
  zy /= zl;
  zz /= zl;

  let xx = zz;
  let xy = 0;
  let xz = -zx;
  const xl = Math.hypot(xx, xy, xz);
  if (xl < 1e-8) {
    xx = 1;
    xy = 0;
    xz = 0;
  } else {
    xx /= xl;
    xy /= xl;
    xz /= xl;
  }

  const yx = zy * xz - zz * xy;
  const yy = zz * xx - zx * xz;
  const yz = zx * xy - zy * xx;

  const px = point.x - cam.x;
  const py = point.y - cam.y;
  const pz = point.z - cam.z;
  const vx = px * xx + py * xy + pz * xz;
  const vy = px * yx + py * yy + pz * yz;
  const vz = px * zx + py * zy + pz * zz;
  const depth = -vz;
  const tanV = Math.tan(((fovDeg * Math.PI) / 180) / 2);
  if (!(depth > 1e-6) || !(tanV > 0)) return { x: 0, y: 0, inFront: false };
  return { x: (vx / depth) / tanV, y: (vy / depth) / tanV, inFront: true };
}

export function screenFractionDown(ndcY: number): number {
  return (1 - ndcY) / 2;
}

/**
 * Look slightly above the marble so it sits just below screen center.
 * The camera position stays on the aim pitch; only the look point rises.
 */
export function biasedLookAt(cam: Vec3, target: Vec3, fovDeg: number, fractionDown = AIM_SCREEN_FRACTION): Vec3 {
  const dx = target.x - cam.x;
  const dy = target.y - cam.y;
  const dz = target.z - cam.z;
  const dist = Math.hypot(dx, dy, dz);
  if (dist < 1e-6) return { x: target.x, y: target.y, z: target.z };
  const fx = dx / dist;
  const fy = dy / dist;
  const fz = dz / dist;
  let ux = -fx * fy;
  let uy = 1 - fy * fy;
  let uz = -fz * fy;
  const ul = Math.hypot(ux, uy, uz);
  if (ul < 1e-6) return { x: target.x, y: target.y, z: target.z };
  ux /= ul;
  uy /= ul;
  uz /= ul;
  const ndc = 1 - 2 * fractionDown;
  const halfTan = Math.tan(((fovDeg * Math.PI) / 180) / 2);
  const angle = Math.atan(-ndc * halfTan);
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  return {
    x: cam.x + (fx * c + ux * s) * dist,
    y: cam.y + (fy * c + uy * s) * dist,
    z: cam.z + (fz * c + uz * s) * dist,
  };
}

function aimYawFor(input: AlphaCamInput, lastHeading: Record<string, number>): number {
  const stored = lastHeading[input.target.id];
  if (input.onCourt && stored !== undefined && Number.isFinite(stored)) return stored;
  return headingTowardCenter(input.target.x, input.target.z);
}

function clampDt(dt: number): number {
  if (!Number.isFinite(dt) || dt <= 0) return 0;
  return Math.min(dt, DT_CAP);
}

function place(
  anchor: Vec3,
  pose: Pose,
  marble: Vec3,
  fovDeg: number,
  centerMarble: boolean,
): { cam: Vec3; look: Vec3 } {
  const offset = offsetFromPose(pose);
  const cam = {
    x: anchor.x + offset.x,
    y: Math.max(FALL_CAM_MIN_Y, anchor.y + offset.y),
    z: anchor.z + offset.z,
  };
  const look = centerMarble ? { x: marble.x, y: marble.y, z: marble.z } : biasedLookAt(cam, marble, fovDeg);
  return { cam, look };
}

function snapState(
  input: AlphaCamInput,
  mode: AlphaCamMode,
  pose: Pose,
  aimYaw: number,
  lastHeading: Record<string, number>,
  centerMarble: boolean,
): AlphaCamState {
  const marble = { x: input.target.x, y: input.target.y, z: input.target.z };
  const placed = place(marble, pose, marble, input.fovDeg, centerMarble);
  return {
    mode,
    actorKey: input.actorKey,
    targetId: input.target.id,
    yaw: pose.yaw,
    pitch: pose.pitch,
    distance: pose.distance,
    aimYaw,
    anchorX: marble.x,
    anchorY: marble.y,
    anchorZ: marble.z,
    camX: placed.cam.x,
    camY: placed.cam.y,
    camZ: placed.cam.z,
    lookX: placed.look.x,
    lookY: placed.look.y,
    lookZ: placed.look.z,
    lastHeading,
  };
}

export function stepAlphaCamera(prev: AlphaCamState | null, input: AlphaCamInput): AlphaCamState {
  const dt = clampDt(input.dt);
  const lengths = rigLengths(input.fovDeg, input.arenaScale, input.zoom ?? 1);
  const marble = { x: input.target.x, y: input.target.y, z: input.target.z };

  if (!prev) {
    const yaw = aimYawFor(input, {});
    return snapState(
      input,
      "aim",
      { yaw, pitch: lengths.aimPitch, distance: lengths.aimDist },
      yaw,
      {},
      false,
    );
  }

  if (prev.actorKey !== input.actorKey) {
    const aimYaw = aimYawFor(input, prev.lastHeading);
    const pose = { yaw: prev.yaw, pitch: prev.pitch, distance: prev.distance };
    return snapState(input, "cut", pose, aimYaw, { ...prev.lastHeading }, true);
  }

  let mode = prev.mode;
  let lastHeading = prev.lastHeading;
  if (input.turnHasLaunched && (mode === "aim" || mode === "cut")) {
    const speed = Math.hypot(input.target.vx, input.target.vz);
    const releaseYaw = speed > YAW_FOLLOW_MIN_SPEED
      ? headingFromVelocity(input.target.vx, input.target.vz)
      : prev.yaw;
    lastHeading = { ...prev.lastHeading, [input.target.id]: releaseYaw };
    mode = "follow";
  }

  if (mode === "hold" || (input.fallen && mode !== "cut")) {
    return {
      ...prev,
      mode: "hold",
      actorKey: input.actorKey,
      targetId: input.target.id,
      lastHeading,
    };
  }

  const dragging = input.isAiming && (mode === "aim" || mode === "cut");
  let yaw = prev.yaw;
  let pitch = prev.pitch;
  let distance = prev.distance;

  if (dragging) {
    const needed = distanceForPull({
      pullDistance: FULL_PULL_DISTANCE,
      pitch,
      fovDeg: input.fovDeg,
      margin: PULL_VIEW_MARGIN,
    });
    const pullDist = Math.max(lengths.aimDist, needed);
    // Already high enough (mid-swoop): don't dolly in under the finger.
    const targetDist = distance > pullDist ? distance : pullDist;
    distance = damp(distance, targetDist, LAMBDA_DRAG, dt);
  } else if (mode === "follow") {
    pitch = damp(pitch, lengths.followPitch, LAMBDA_RISE, dt);
    distance = damp(distance, lengths.followDist, LAMBDA_RISE, dt);
    const speed = Math.hypot(input.target.vx, input.target.vz);
    if (speed > YAW_FOLLOW_MIN_SPEED) {
      yaw = dampYaw(yaw, headingFromVelocity(input.target.vx, input.target.vz), LAMBDA_YAW_FOLLOW, dt, YAW_FOLLOW_MAX_RAD);
    }
  } else if (mode === "cut") {
    yaw = dampYaw(yaw, prev.aimYaw, LAMBDA_RESET_YAW, dt);
    pitch = damp(pitch, lengths.aimPitch, LAMBDA_RESET_DOLLY, dt);
    distance = damp(distance, lengths.aimDist, LAMBDA_RESET_DOLLY, dt);
  } else {
    yaw = dampYaw(yaw, prev.aimYaw, LAMBDA_DRAG, dt);
    pitch = damp(pitch, lengths.aimPitch, LAMBDA_DRAG, dt);
    distance = damp(distance, lengths.aimDist, LAMBDA_DRAG, dt);
  }

  const anchorX = damp(prev.anchorX, marble.x, LAMBDA_POS, dt);
  const anchorY = damp(prev.anchorY, marble.y, LAMBDA_POS, dt);
  const anchorZ = damp(prev.anchorZ, marble.z, LAMBDA_POS, dt);
  const pose = { yaw, pitch, distance };
  const placed = place(
    { x: anchorX, y: anchorY, z: anchorZ },
    pose,
    marble,
    input.fovDeg,
    mode === "follow",
  );

  const lookX = damp(prev.lookX, placed.look.x, LAMBDA_LOOK, dt);
  const lookY = damp(prev.lookY, placed.look.y, LAMBDA_LOOK, dt);
  const lookZ = damp(prev.lookZ, placed.look.z, LAMBDA_LOOK, dt);

  let nextMode = mode;
  if (mode === "cut" && !dragging) {
    const yawErr = Math.abs(wrapAngle(prev.aimYaw - yaw));
    const distErr = Math.abs(distance - lengths.aimDist);
    const pitchErr = Math.abs(pitch - lengths.aimPitch);
    const distSpan = Math.max(1, Math.abs(lengths.followDist - lengths.aimDist));
    // λ 1.9 is about 98% done at 2.2 s. Call that the aim pose.
    if (yawErr < 0.02 && distErr < Math.max(0.15, distSpan * 0.02) && pitchErr < 0.02) nextMode = "aim";
  }

  return {
    mode: nextMode,
    actorKey: input.actorKey,
    targetId: input.target.id,
    yaw,
    pitch,
    distance,
    aimYaw: prev.aimYaw,
    anchorX,
    anchorY,
    anchorZ,
    camX: placed.cam.x,
    camY: placed.cam.y,
    camZ: placed.cam.z,
    lookX,
    lookY,
    lookZ,
    lastHeading,
  };
}

export function formatAlphaCamDebug(state: AlphaCamState): string {
  const pitch = (state.pitch * 180) / Math.PI;
  const yaw = (wrapAngle(state.yaw) * 180) / Math.PI;
  return `${state.mode.toUpperCase()}  target=${state.targetId}  pitch=${pitch.toFixed(1)}°  dist=${state.distance.toFixed(1)}  yaw=${yaw.toFixed(1)}°`;
}
