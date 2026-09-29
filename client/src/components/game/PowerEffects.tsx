import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { SpectralWolf } from "@/vfx/SpectralWolf";
import { usePlayedClock } from "@/vfx/bursts";
import { emitImpact } from "@/vfx/impacts";
import { BoltDischarge, StunCrawlers, UnlockGlow } from "@/vfx/powerLooks";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export { ExplosionBlast } from "@/vfx/ExplosionBlast";

export type Vec3 = [number, number, number];

const STUN_BURST_DURATION = 0.9;
const UNLOCK_DURATION = 0.75;

function clamp01(v: number): number {
  return Math.max(0, Math.min(1, v));
}

function smoothstep(t: number): number {
  const x = clamp01(t);
  return x * x * (3 - 2 * x);
}

/** One rendered frame. A hitch must not skip the whole burst. */
const FRAME_STEP = 1 / 30;

/**
 * Played time starts on the first useFrame, using that frame's clock.
 * Wall-clock `Date.now()` runs ahead of the first paint, which used to
 * expire the burst before it was drawn.
 */
function usePlayedTime(frozenElapsed?: number) {
  const origin = useRef<number | null>(null);
  const played = useRef(0);
  return (clockElapsed: number) => {
    if (frozenElapsed !== undefined) return Math.max(0, frozenElapsed);
    if (origin.current === null) origin.current = clockElapsed;
    const uncapped = Math.max(0, clockElapsed - origin.current);
    const next = Math.min(uncapped, played.current + FRAME_STEP);
    played.current = next;
    return next;
  };
}

/** Keep the burst above the floor the marble is standing on. */
function aboveFloor(position: Vec3): Vec3 {
  return [position[0], Math.max(position[1], 0.5) + 0.35, position[2]];
}

const additive = {
  transparent: true,
  depthWrite: false,
  blending: THREE.AdditiveBlending,
  toneMapped: false,
} as const;

function stunnedWithin(origin: Vec3, radius: number): Vec3[] {
  const state = useZoogiGame.getState();
  const hits: Vec3[] = [];
  const consider = (entity: { position: [number, number, number]; isStunned: boolean } | null | undefined) => {
    if (!entity?.isStunned) return;
    const dx = entity.position[0] - origin[0];
    const dz = entity.position[2] - origin[2];
    if (dx * dx + dz * dz <= radius * radius) {
      hits.push([entity.position[0], entity.position[1], entity.position[2]]);
    }
  };
  consider(state.playerEntity);
  state.enemies.forEach(consider);
  return hits.slice(0, 4);
}

/** Bolt's discharge. Arcs run to stunned marbles; timing stays 0.9s. */
export function StunBurst({
  position,
  startTime: _startTime,
  radius = 8,
  frozenElapsed,
  targets,
}: {
  position: Vec3;
  startTime: number;
  radius?: number;
  frozenElapsed?: number;
  targets?: Vec3[];
}) {
  const [done, setDone] = useState(false);
  const ended = useRef(false);
  const clock = usePlayedClock(frozenElapsed);
  const player = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const live = useMemo(() => {
    if (targets) return targets;
    return stunnedWithin(position, radius);
  }, [targets, position, radius, player, enemies]);

  useFrame((state, dt) => {
    const { elapsed } = clock(state.clock.elapsedTime, dt);
    if (frozenElapsed === undefined && !ended.current && elapsed >= STUN_BURST_DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <BoltDischarge
      position={[position[0], Math.max(position[1], 0.5), position[2]]}
      targets={live}
      frozenElapsed={frozenElapsed}
    />
  );
}

const SHADOW_PULSE_DURATION = 2.6;

/**
 * Nightshade's close-range shadow pulse. A dark disc and lavender rings
 * expand to the stun radius, then fade. No point light, so it does not hitch.
 */
export function ShadowPulse({
  position,
  startTime,
  radius = 4.5,
  frozenElapsed,
}: {
  position: Vec3;
  startTime: number;
  radius?: number;
  frozenElapsed?: number;
}) {
  const [done, setDone] = useState(false);
  const ended = useRef(false);
  const ring = useRef<THREE.Mesh>(null);
  const ringMat = useRef<THREE.MeshBasicMaterial>(null);
  const ring2 = useRef<THREE.Mesh>(null);
  const ring2Mat = useRef<THREE.MeshBasicMaterial>(null);
  const washMat = useRef<THREE.MeshBasicMaterial>(null);
  const core = useRef<THREE.Mesh>(null);
  const coreMat = useRef<THREE.MeshBasicMaterial>(null);
  const playedTime = usePlayedTime(frozenElapsed);
  void startTime;

  useFrame((state) => {
    const elapsed = playedTime(state.clock.elapsedTime);
    const expand = Math.max(0.55, smoothstep(elapsed / 0.22));
    const fade = 1 - smoothstep((elapsed - 1.15) / 1.35);
    const ringR = Math.max(0.35, radius * expand);
    if (ring.current) ring.current.scale.setScalar(ringR);
    if (ring2.current) ring2.current.scale.setScalar(ringR * 0.62);
    if (ringMat.current) ringMat.current.opacity = 0.95 * Math.max(fade, expand < 1 ? 0.85 : 0);
    if (ring2Mat.current) ring2Mat.current.opacity = 0.75 * fade;
    if (washMat.current) washMat.current.opacity = 0.42 * smoothstep(elapsed / 0.12) * Math.max(fade, 0.15);
    const coreScale = (0.45 + expand * 1.1) * Math.max(fade, 0.04);
    if (core.current) core.current.scale.setScalar(coreScale);
    if (coreMat.current) coreMat.current.opacity = 0.8 * fade;
    if (frozenElapsed === undefined && !ended.current && elapsed >= SHADOW_PULSE_DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <group position={aboveFloor(position)} renderOrder={3}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.16, 0]}>
        <circleGeometry args={[radius, 64]} />
        <meshBasicMaterial
          ref={washMat}
          color="#170a33"
          transparent
          opacity={0.4}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.24, 0]}>
        <ringGeometry args={[0.92, 1, 72]} />
        <meshBasicMaterial
          ref={ringMat}
          color="#b69cff"
          transparent
          opacity={0.95}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={ring2} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.3, 0]}>
        <ringGeometry args={[0.86, 1, 64]} />
        <meshBasicMaterial
          ref={ring2Mat}
          color="#9F7AEA"
          opacity={0.8}
          {...additive}
          side={THREE.DoubleSide}
        />
      </mesh>
      <mesh ref={core} position={[0, 0.9, 0]}>
        <sphereGeometry args={[0.7, 20, 20]} />
        <meshBasicMaterial ref={coreMat} color="#6B46C1" opacity={0.75} {...additive} />
      </mesh>
    </group>
  );
}

/**
 * Sits on a stunned marble until the stun ends, then leaves a small spark puff.
 * `pulse` is the turn-stun path: the crawlers stay until the parent unmounts.
 */
export function StunnedIndicator({
  position,
  remaining,
  duration,
  pulse = false,
}: {
  position: Vec3;
  remaining: number;
  duration: number;
  /** Turn-based stuns have no countdown. Keep the crawlers until the stun is cleared. */
  pulse?: boolean;
}) {
  const posRef = useRef(position);
  posRef.current = position;
  useEffect(() => () => {
    const spot = posRef.current;
    emitImpact("electric", [spot[0], spot[1], spot[2]]);
  }, []);

  if (!pulse && remaining <= 0 && duration <= 0) return null;

  return (
    <group position={position}>
      <StunCrawlers />
    </group>
  );
}

export function WolfCloneLook({
  position,
  velocity = [0, 0, 0],
}: {
  position: Vec3;
  velocity?: Vec3;
  startTime?: number;
  radius?: number;
}) {
  return <SpectralWolf position={position} velocity={velocity} />;
}

/** Warm pulse, rising embers, and a soft ground ring when a star coin unlocks a power. */
export function PowerUnlockFlash({
  position,
  startTime: _startTime,
  radius: _radius = 2.4,
  color: _color = "#ffd700",
  frozenElapsed,
}: {
  position: Vec3;
  startTime: number;
  radius?: number;
  color?: string;
  frozenElapsed?: number;
}) {
  const [done, setDone] = useState(false);
  const ended = useRef(false);
  const clock = usePlayedClock(frozenElapsed);

  useFrame((state, dt) => {
    const { elapsed } = clock(state.clock.elapsedTime, dt);
    if (frozenElapsed === undefined && !ended.current && elapsed >= UNLOCK_DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <UnlockGlow
      position={[position[0], Math.max(position[1], 0.5), position[2]]}
      frozenElapsed={frozenElapsed}
    />
  );
}
