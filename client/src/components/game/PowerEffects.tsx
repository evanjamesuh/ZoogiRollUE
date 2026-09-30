import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { SpectralWolf } from "@/vfx/SpectralWolf";
import { usePlayedClock } from "@/vfx/bursts";
import { emitImpact } from "@/vfx/impacts";
import { BoltDischarge, StunCrawlers, UnlockGlow } from "@/vfx/powerLooks";
import { ShadowPulseLook, ShadowWrap } from "@/vfx/shadowPulse";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { ZOOGI_FX_SCALE } from "@/lib/restHeight";

export { ShadowWrap };

export { ExplosionBlast } from "@/vfx/ExplosionBlast";

export type Vec3 = [number, number, number];

const STUN_BURST_DURATION = 0.9;
const UNLOCK_DURATION = 0.75;

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
 * Nightshade's shadow pulse. Wisps pull inward, then inky smoke rolls out
 * to the stun radius. Gameplay timing and radius stay as they were.
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

  if (done) return null;

  return (
    <group position={[position[0], 0, position[2]]}>
      <ShadowPulseLook
        radius={radius}
        startTime={startTime}
        duration={SHADOW_PULSE_DURATION}
        frozenElapsed={frozenElapsed}
        onFinished={() => setDone(true)}
      />
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
  look = "shock",
}: {
  position: Vec3;
  remaining: number;
  duration: number;
  /** Turn-based stuns have no countdown. Keep the crawlers until the stun is cleared. */
  pulse?: boolean;
  /** Shadow Stun keeps a dark wrap. Bolt's shock keeps the electric crawlers. */
  look?: "shock" | "shadow";
}) {
  const posRef = useRef(position);
  posRef.current = position;
  useEffect(() => () => {
    if (look === "shadow") return;
    const spot = posRef.current;
    emitImpact("electric", [spot[0], spot[1], spot[2]]);
  }, [look]);

  if (!pulse && remaining <= 0 && duration <= 0) return null;

  return (
    <group position={position} scale={ZOOGI_FX_SCALE}>
      {look === "shadow" ? <ShadowWrap /> : <StunCrawlers />}
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
