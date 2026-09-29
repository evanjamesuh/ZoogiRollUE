import { useLayoutEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { SpectralWolf } from "@/vfx/SpectralWolf";

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

function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function hashSeed(position: Vec3, startTime: number, salt: number): number {
  const x = Math.imul(Math.floor(position[0] * 100) | 0, 374761393);
  const y = Math.imul(Math.floor(position[1] * 100) | 0, 668265263);
  const z = Math.imul(Math.floor(position[2] * 100) | 0, 1274126177);
  const time = Math.imul(startTime | 0, 1442695041);
  return (x ^ y ^ z ^ time ^ salt) >>> 0 || 1;
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

interface BoltSeg {
  position: Vec3;
  quaternion: [number, number, number, number];
  length: number;
}

interface BoltSpec {
  color: string;
  flicker: number;
  segments: BoltSeg[];
}

function buildBolts(position: Vec3, startTime: number, radius: number): BoltSpec[] {
  const rand = mulberry32(hashSeed(position, startTime, 29));
  const count = 8;
  const xAxis = new THREE.Vector3(1, 0, 0);
  return Array.from({ length: count }, (_, index) => {
    const angle = (index / count) * Math.PI * 2 + (rand() - 0.5) * 0.4;
    const reach = radius * (0.82 + rand() * 0.18);
    const points = [new THREE.Vector3(0, 0.7, 0)];
    const steps = 5;
    for (let s = 1; s <= steps; s++) {
      const u = s / steps;
      const jitter = (1 - u * 0.4) * (0.45 + rand() * 1.2);
      points.push(new THREE.Vector3(
        Math.cos(angle) * reach * u + (rand() - 0.5) * jitter,
        0.45 + Math.sin(u * Math.PI) * (0.5 + rand() * 1.8),
        Math.sin(angle) * reach * u + (rand() - 0.5) * jitter
      ));
    }
    const segments: BoltSeg[] = [];
    for (let s = 0; s < points.length - 1; s++) {
      const a = points[s];
      const b = points[s + 1];
      const dir = b.clone().sub(a);
      const length = Math.max(dir.length(), 0.05);
      dir.multiplyScalar(1 / length);
      const q = new THREE.Quaternion().setFromUnitVectors(xAxis, dir);
      const mid = a.clone().add(b).multiplyScalar(0.5);
      segments.push({
        position: [mid.x, mid.y, mid.z],
        quaternion: [q.x, q.y, q.z, q.w],
        length,
      });
    }
    return {
      color: index % 2 === 0 ? "#fde047" : "#38bdf8",
      flicker: rand() * Math.PI * 2,
      segments,
    };
  });
}

/** Bolt's electric ring. Lightning geometry is seeded once so it does not jump. */
export function StunBurst({
  position,
  startTime,
  radius = 8,
  frozenElapsed,
}: {
  position: Vec3;
  startTime: number;
  radius?: number;
  frozenElapsed?: number;
}) {
  const [done, setDone] = useState(false);
  const ended = useRef(false);
  const px = position[0];
  const py = position[1];
  const pz = position[2];
  const bolts = useMemo(
    () => buildBolts([px, py, pz], startTime, radius),
    [px, py, pz, startTime, radius]
  );
  const boltGroups = useRef<(THREE.Group | null)[]>([]);
  const ring = useRef<THREE.Mesh>(null);
  const ringMat = useRef<THREE.MeshBasicMaterial>(null);
  const ring2 = useRef<THREE.Mesh>(null);
  const ring2Mat = useRef<THREE.MeshBasicMaterial>(null);
  const washMat = useRef<THREE.MeshBasicMaterial>(null);
  const core = useRef<THREE.Mesh>(null);
  const coreMat = useRef<THREE.MeshBasicMaterial>(null);
  const playedTime = usePlayedTime(frozenElapsed);

  useFrame((state) => {
    const elapsed = playedTime(state.clock.elapsedTime);
    const expand = Math.max(0.62, smoothstep(elapsed / 0.2));
    const fade = 1 - smoothstep((elapsed - 0.34) / 0.56);
    const ringR = Math.max(0.4, radius * expand);
    if (ring.current) ring.current.scale.setScalar(ringR);
    if (ring2.current) ring2.current.scale.setScalar(ringR * 0.72);
    if (ringMat.current) ringMat.current.opacity = 0.95 * Math.max(fade, expand < 1 ? 0.9 : 0);
    if (ring2Mat.current) ring2Mat.current.opacity = 0.8 * fade;
    if (washMat.current) washMat.current.opacity = 0.38 * smoothstep(elapsed / 0.1) * Math.max(fade, 0.2);
    const coreScale = (0.6 + expand * 1.8) * Math.max(fade, 0.05);
    if (core.current) core.current.scale.setScalar(coreScale);
    if (coreMat.current) coreMat.current.opacity = 0.85 * fade;

    boltGroups.current.forEach((group, i) => {
      if (!group) return;
      const bolt = bolts[i];
      const wave = 0.5 + 0.5 * Math.sin(elapsed * (28 + i * 3) + bolt.flicker);
      const gate = Math.sin(elapsed * 46 + bolt.flicker) > -0.35 ? 1 : 0.28;
      const opacity = fade * (0.35 + 0.65 * wave) * gate;
      group.visible = fade > 0.03;
      group.children.forEach((child) => {
        const mesh = child as THREE.Mesh;
        const mat = mesh.material;
        if (!Array.isArray(mat)) mat.opacity = opacity;
      });
    });

    if (frozenElapsed === undefined && !ended.current && elapsed >= STUN_BURST_DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <group position={aboveFloor(position)} renderOrder={3}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.2, 0]}>
        <circleGeometry args={[radius, 64]} />
        <meshBasicMaterial
          ref={washMat}
          color="#082038"
          transparent
          opacity={0.35}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.28, 0]}>
        <ringGeometry args={[0.9, 1, 72]} />
        <meshBasicMaterial
          ref={ringMat}
          color="#38bdf8"
          transparent
          opacity={0.95}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={ring2} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.34, 0]}>
        <ringGeometry args={[0.92, 1, 64]} />
        <meshBasicMaterial
          ref={ring2Mat}
          color="#fde047"
          opacity={0.85}
          {...additive}
          side={THREE.DoubleSide}
        />
      </mesh>
      <mesh ref={core} position={[0, 1.5, 0]}>
        <sphereGeometry args={[1, 20, 20]} />
        <meshBasicMaterial ref={coreMat} color="#e0f2fe" opacity={0.8} {...additive} />
      </mesh>
      {bolts.map((bolt, i) => (
        <group
          key={i}
          ref={(el) => {
            boltGroups.current[i] = el;
          }}
        >
          {bolt.segments.map((seg, s) => (
            <mesh
              key={s}
              position={seg.position}
              quaternion={seg.quaternion}
              scale={[seg.length, 1, 1]}
            >
              <boxGeometry args={[1, 0.42, 0.42]} />
              <meshBasicMaterial color={bolt.color} transparent opacity={0.95} toneMapped={false} />
            </mesh>
          ))}
        </group>
      ))}
    </group>
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
 * Sits on a stunned marble. `remaining` / `duration` drive the shrinking ring.
 * Between store updates the ring keeps shrinking from the last remaining value.
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
  /** Turn-based stuns have no countdown. Pulse the ring until the stun is cleared. */
  pulse?: boolean;
}) {
  const group = useRef<THREE.Group>(null);
  const orbit = useRef<THREE.Group>(null);
  const ring = useRef<THREE.Mesh>(null);
  const ringMat = useRef<THREE.MeshBasicMaterial>(null);
  const baseRemaining = useRef(remaining);
  const baseDuration = useRef(duration);
  const stamp = useRef(Date.now());

  if (remaining !== baseRemaining.current || duration !== baseDuration.current) {
    baseRemaining.current = remaining;
    baseDuration.current = duration;
    stamp.current = Date.now();
  }

  const apply = (shown: number) => {
    const dur = Math.max(baseDuration.current, 0.001);
    const ratio = clamp01(shown / dur);
    const scale = 0.7 + 1.5 * ratio;
    if (ring.current) {
      ring.current.scale.setScalar(scale);
      ring.current.visible = shown > 0.02;
    }
    if (ringMat.current) ringMat.current.opacity = 0.45 + 0.5 * ratio;
    if (group.current) group.current.visible = shown > 0.02;
  };

  useLayoutEffect(() => {
    if (pulse) return;
    apply(remaining);
    // The layout pass only needs the latest props; apply closes over the refs.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [remaining, duration, pulse]);

  useFrame((_, delta) => {
    if (pulse) {
      const wobble = 0.5 + 0.5 * Math.sin(Date.now() / 170);
      const scale = 1.35 + wobble * 0.75;
      if (ring.current) {
        ring.current.scale.setScalar(scale);
        ring.current.visible = true;
      }
      if (ringMat.current) ringMat.current.opacity = 0.55 + wobble * 0.4;
      if (group.current) group.current.visible = true;
      if (orbit.current) orbit.current.rotation.y += delta * 2.4;
      return;
    }
    const shown = Math.max(0, baseRemaining.current - (Date.now() - stamp.current) / 1000);
    apply(shown);
    if (orbit.current) orbit.current.rotation.y += delta * 2.4;
  });

  if (!pulse && remaining <= 0 && duration <= 0) return null;

  const sparks = [0, 1, 2, 3, 4, 5];

  return (
    <group ref={group} position={position}>
      <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.28, 0]}>
        <ringGeometry args={[0.78, 1, 40]} />
        <meshBasicMaterial
          ref={ringMat}
          color="#fde047"
          transparent
          opacity={0.9}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <group ref={orbit} position={[0, 1.15, 0]}>
        {sparks.map((i) => {
          const angle = (i / sparks.length) * Math.PI * 2;
          const color = i % 2 === 0 ? "#fde047" : "#38bdf8";
          return (
            <mesh key={i} position={[Math.cos(angle) * 1.15, Math.sin(i) * 0.08, Math.sin(angle) * 1.15]}>
              {i % 2 === 0 ? (
                <octahedronGeometry args={[0.28, 0]} />
              ) : (
                <sphereGeometry args={[0.16, 10, 10]} />
              )}
              <meshBasicMaterial color={color} toneMapped={false} />
            </mesh>
          );
        })}
      </group>
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

/** Quick burst when a star coin unlocks a power. */
export function PowerUnlockFlash({
  position,
  startTime,
  radius = 2.4,
  color = "#ffd700",
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
  const root = useRef<THREE.Group>(null);
  const ring = useRef<THREE.Mesh>(null);
  const ringMat = useRef<THREE.MeshBasicMaterial>(null);
  const discMat = useRef<THREE.MeshBasicMaterial>(null);
  const core = useRef<THREE.Mesh>(null);
  const coreMat = useRef<THREE.MeshBasicMaterial>(null);
  const rayRefs = useRef<(THREE.Mesh | null)[]>([]);
  const starRefs = useRef<(THREE.Mesh | null)[]>([]);
  const rays = 8;
  const px = position[0];
  const py = position[1];
  const pz = position[2];
  const stars = useMemo(() => {
    const rand = mulberry32(hashSeed([px, py, pz], startTime, 53));
    return Array.from({ length: 6 }, () => ({
      angle: rand() * Math.PI * 2,
      lift: 0.4 + rand() * 1.1,
      dist: 0.4 + rand() * 0.6,
    }));
  }, [px, py, pz, startTime]);
  const playedTime = usePlayedTime(frozenElapsed);

  useFrame((state) => {
    const elapsed = playedTime(state.clock.elapsedTime);
    const life = elapsed <= 0.62 ? 1 : 1 - smoothstep((elapsed - 0.62) / (UNLOCK_DURATION - 0.62));
    const pop = Math.max(0.85, Math.sin(clamp01(elapsed / 0.5) * Math.PI));
    if (root.current) root.current.rotation.y = elapsed * 2.5;
    if (ring.current) ring.current.scale.setScalar(Math.max(radius * 0.72, radius * smoothstep(elapsed / 0.16)));
    if (ringMat.current) ringMat.current.opacity = life;
    if (discMat.current) discMat.current.opacity = 0.92 * life;
    const coreScale = 0.45 + pop * 1.7;
    if (core.current) core.current.scale.setScalar(coreScale);
    if (coreMat.current) coreMat.current.opacity = 0.95 * life;
    for (let i = 0; i < rays; i++) {
      const mesh = rayRefs.current[i];
      if (!mesh) continue;
      const len = 0.35 + pop * radius;
      mesh.scale.set(len, 0.28, 0.28);
      mesh.position.set(Math.cos((i / rays) * Math.PI * 2) * (len * 0.5), 0.15, Math.sin((i / rays) * Math.PI * 2) * (len * 0.5));
      const mat = mesh.material;
      if (!Array.isArray(mat)) mat.opacity = 0.9 * life;
    }
    stars.forEach((star, i) => {
      const mesh = starRefs.current[i];
      if (!mesh) return;
      const dist = star.dist * radius * smoothstep(elapsed / 0.3);
      mesh.position.set(Math.cos(star.angle) * dist, 0.4 + star.lift * pop, Math.sin(star.angle) * dist);
      mesh.rotation.y = elapsed * 3 + star.angle;
      const mat = mesh.material;
      if (!Array.isArray(mat)) mat.opacity = 0.95 * life;
    });
    if (frozenElapsed === undefined && !ended.current && elapsed >= UNLOCK_DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <group ref={root} position={aboveFloor(position)} renderOrder={8}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.45, 0]} renderOrder={8}>
        <circleGeometry args={[radius * 0.92, 48]} />
        <meshBasicMaterial ref={discMat} color={color} transparent opacity={0.92} depthWrite={false} depthTest={false} side={THREE.DoubleSide} toneMapped={false} />
      </mesh>
      <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.5, 0]} renderOrder={9}>
        <ringGeometry args={[0.78, 1, 40]} />
        <meshBasicMaterial
          ref={ringMat}
          color="#fff6c2"
          opacity={1}
          transparent
          depthWrite={false}
          depthTest={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={core} position={[0, 1.15, 0]} renderOrder={10}>
        <sphereGeometry args={[1.15, 18, 18]} />
        <meshBasicMaterial ref={coreMat} color={color} opacity={0.95} depthTest={false} {...additive} />
      </mesh>
      <mesh position={[0, 1.15, 0]} renderOrder={11}>
        <sphereGeometry args={[0.48, 12, 12]} />
        <meshBasicMaterial color="#ffffff" opacity={1} depthTest={false} {...additive} />
      </mesh>
      {Array.from({ length: rays }).map((_, i) => (
        <mesh
          key={i}
          ref={(el) => {
            rayRefs.current[i] = el;
          }}
          rotation={[0, -((i / rays) * Math.PI * 2), 0]}
        >
          <boxGeometry args={[1, 1, 1]} />
          <meshBasicMaterial color={color} transparent opacity={0.95} depthTest={false} toneMapped={false} />
        </mesh>
      ))}
      {stars.map((_, i) => (
        <mesh
          key={`star-${i}`}
          ref={(el) => {
            starRefs.current[i] = el;
          }}
        >
          <octahedronGeometry args={[0.28, 0]} />
          <meshBasicMaterial color="#fff6c2" transparent opacity={1} depthTest={false} toneMapped={false} />
        </mesh>
      ))}
    </group>
  );
}
