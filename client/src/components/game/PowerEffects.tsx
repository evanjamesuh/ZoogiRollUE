import { useLayoutEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";

export type Vec3 = [number, number, number];

const EXPLOSION_DURATION = 0.95;
const STUN_BURST_DURATION = 0.9;
const UNLOCK_DURATION = 0.55;

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

interface SparkSpec {
  angle: number;
  out: number;
  up: number;
  size: number;
  color: string;
  spin: number;
  shape: "octa" | "box";
}

function useSparkField(position: Vec3, startTime: number, count: number, salt: number): SparkSpec[] {
  const px = position[0];
  const py = position[1];
  const pz = position[2];
  return useMemo(() => {
    const rand = mulberry32(hashSeed([px, py, pz], startTime, salt));
    return Array.from({ length: count }, (): SparkSpec => ({
      angle: rand() * Math.PI * 2,
      out: 0.35 + rand() * 0.65,
      up: 0.4 + rand() * 1.4,
      size: 0.22 + rand() * 0.28,
      color: rand() > 0.45 ? "#ffd24a" : rand() > 0.5 ? "#ff6a00" : "#fff4d0",
      spin: rand() * Math.PI * 2,
      shape: rand() > 0.7 ? "box" : "octa",
    }));
  }, [px, py, pz, startTime, count, salt]);
}

/**
 * Hotstreak blast. The ground disc and the expanding ring both stop at `radius`
 * (8 in a match) so the push area reads from the wide camera.
 */
export function ExplosionBlast({
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
  const discMat = useRef<THREE.MeshBasicMaterial>(null);
  const ring = useRef<THREE.Mesh>(null);
  const ringMat = useRef<THREE.MeshBasicMaterial>(null);
  const edge = useRef<THREE.Mesh>(null);
  const edgeMat = useRef<THREE.MeshBasicMaterial>(null);
  const fire = useRef<THREE.Mesh>(null);
  const fireMat = useRef<THREE.MeshBasicMaterial>(null);
  const core = useRef<THREE.Mesh>(null);
  const coreMat = useRef<THREE.MeshBasicMaterial>(null);
  const sparkRefs = useRef<(THREE.Mesh | null)[]>([]);
  const sparks = useSparkField(position, startTime, 18, 11);
  const playedTime = usePlayedTime(frozenElapsed);

  useFrame((state) => {
    const elapsed = playedTime(state.clock.elapsedTime);
    const expand = Math.max(0.62, smoothstep(elapsed / 0.22));
    const fade = 1 - smoothstep((elapsed - 0.38) / 0.57);
    const ringR = Math.max(0.4, radius * expand);

    if (discMat.current) {
      const discIn = Math.max(0.85, smoothstep(elapsed / 0.12));
      discMat.current.opacity = 0.42 * discIn * Math.max(fade, 0.15 * (1 - smoothstep(elapsed / 0.95)));
    }
    if (ring.current) ring.current.scale.setScalar(ringR);
    if (ringMat.current) ringMat.current.opacity = 0.95 * (expand > 0.98 ? fade : 0.95);
    if (edge.current) edge.current.scale.setScalar(ringR);
    if (edgeMat.current) edgeMat.current.opacity = 0.9 * fade;

    const fireGrow = Math.max(0.72, smoothstep(elapsed / 0.1));
    const fireShrink = 1 - smoothstep((elapsed - 0.42) / 0.5);
    const fireScale = Math.max(0.001, fireGrow * fireShrink * 4.6);
    if (fire.current) fire.current.scale.setScalar(fireScale);
    if (fireMat.current) fireMat.current.opacity = 0.9 * fireShrink;
    if (core.current) core.current.scale.setScalar(Math.max(0.001, fireScale * 0.45));
    if (coreMat.current) coreMat.current.opacity = 0.95 * fireShrink;

    const travel = smoothstep(Math.min(elapsed / 0.72, 1));
    const sparkFade = 1 - smoothstep((elapsed - 0.25) / 0.7);
    sparks.forEach((spark, i) => {
      const mesh = sparkRefs.current[i];
      if (!mesh) return;
      const dist = travel * radius * spark.out;
      const y = Math.sin(travel * Math.PI) * spark.up * 2.4;
      mesh.position.set(Math.cos(spark.angle) * dist, 0.3 + y, Math.sin(spark.angle) * dist);
      mesh.scale.setScalar(spark.size * (0.45 + sparkFade));
      mesh.rotation.y = spark.spin + elapsed * 4;
      const mat = mesh.material;
      if (!Array.isArray(mat)) mat.opacity = 0.95 * sparkFade;
      mesh.visible = sparkFade > 0.02;
    });

    if (frozenElapsed === undefined && !ended.current && elapsed >= EXPLOSION_DURATION) {
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
          ref={discMat}
          color="#ff5a1f"
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
          color="#ffb000"
          transparent
          opacity={0.95}
          depthWrite={false}
          side={THREE.DoubleSide}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={edge} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.34, 0]}>
        <ringGeometry args={[0.96, 1, 72]} />
        <meshBasicMaterial
          ref={edgeMat}
          color="#fff4c4"
          opacity={0.9}
          {...additive}
          side={THREE.DoubleSide}
        />
      </mesh>
      <mesh ref={fire} position={[0, 1.7, 0]}>
        <sphereGeometry args={[1, 24, 24]} />
        <meshBasicMaterial ref={fireMat} color="#ff6a00" opacity={0.9} {...additive} />
      </mesh>
      <mesh ref={core} position={[0, 1.7, 0]}>
        <sphereGeometry args={[1, 20, 20]} />
        <meshBasicMaterial ref={coreMat} color="#fff6d8" opacity={0.95} {...additive} />
      </mesh>
      {sparks.map((spark, i) => (
        <mesh
          key={i}
          ref={(el) => {
            sparkRefs.current[i] = el;
          }}
        >
          {spark.shape === "box" ? (
            <boxGeometry args={[1, 0.35, 0.35]} />
          ) : (
            <octahedronGeometry args={[1, 0]} />
          )}
          <meshBasicMaterial color={spark.color} transparent opacity={0.95} toneMapped={false} />
        </mesh>
      ))}
    </group>
  );
}

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

function useWolfBadgeTexture(): THREE.CanvasTexture | null {
  return useMemo(() => {
    if (typeof document === "undefined") return null;
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const g = canvas.getContext("2d");
    if (!g) return null;
    g.clearRect(0, 0, 128, 128);
    g.fillStyle = "rgba(186, 220, 255, 0.95)";
    g.beginPath();
    g.arc(64, 70, 46, 0, Math.PI * 2);
    g.fill();
    g.fillStyle = "#16324f";
    g.beginPath();
    g.moveTo(30, 58);
    g.lineTo(48, 12);
    g.lineTo(62, 58);
    g.fill();
    g.beginPath();
    g.moveTo(98, 58);
    g.lineTo(80, 12);
    g.lineTo(66, 58);
    g.fill();
    g.fillStyle = "#f7fbff";
    g.beginPath();
    g.arc(50, 72, 6, 0, Math.PI * 2);
    g.arc(78, 72, 6, 0, Math.PI * 2);
    g.fill();
    g.fillStyle = "#ffffff";
    g.beginPath();
    g.moveTo(48, 88);
    g.lineTo(56, 112);
    g.lineTo(64, 88);
    g.fill();
    g.beginPath();
    g.moveTo(64, 88);
    g.lineTo(72, 112);
    g.lineTo(80, 88);
    g.fill();
    const texture = new THREE.CanvasTexture(canvas);
    texture.needsUpdate = true;
    return texture;
  }, []);
}

/** Translucent silver-blue clone with a glow trail, ears, and a fang badge. */
export function WolfCloneLook({
  position,
  velocity = [0, 0, 0],
}: {
  position: Vec3;
  velocity?: Vec3;
  startTime?: number;
  radius?: number;
}) {
  const group = useRef<THREE.Group>(null);
  const shellMat = useRef<THREE.MeshBasicMaterial>(null);
  const velRef = useRef(velocity);
  velRef.current = velocity;
  const badge = useWolfBadgeTexture();

  useFrame(({ clock }) => {
    const root = group.current;
    if (!root) return;
    const [vx, , vz] = velRef.current;
    const speed = Math.hypot(vx, vz);
    if (speed > 0.02) root.rotation.y = Math.atan2(vx, vz);
    const pulse = 0.5 + 0.5 * Math.sin(clock.elapsedTime * 5.5);
    if (shellMat.current) shellMat.current.opacity = 0.22 + pulse * 0.16;
  });

  return (
    <group ref={group} position={position}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.42, 0]}>
        <circleGeometry args={[2.15, 28]} />
        <meshBasicMaterial color="#7ec8ff" opacity={0.7} {...additive} side={THREE.DoubleSide} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, -1.55]}>
        <coneGeometry args={[0.55, 3.6, 10]} />
        <meshBasicMaterial color="#b9e4ff" opacity={0.7} {...additive} />
      </mesh>
      {[0, 1, 2, 3, 4, 5].map((i) => (
        <mesh key={i} position={[0, 0.05, -0.7 - i * 0.42]} scale={0.46 - i * 0.05}>
          <sphereGeometry args={[1, 10, 10]} />
          <meshBasicMaterial
            color="#d7efff"
            opacity={0.55 - i * 0.07}
            {...additive}
          />
        </mesh>
      ))}
      <mesh>
        <sphereGeometry args={[0.5, 24, 24]} />
        <meshStandardMaterial
          color="#d5e4f2"
          emissive="#6eb6ff"
          emissiveIntensity={0.85}
          metalness={0.82}
          roughness={0.16}
          transparent
          opacity={0.84}
        />
      </mesh>
      <mesh scale={1.22}>
        <sphereGeometry args={[0.5, 18, 18]} />
        <meshBasicMaterial ref={shellMat} color="#9fd4ff" opacity={0.28} {...additive} />
      </mesh>
      <mesh position={[-0.2, 0.42, -0.02]} rotation={[0.15, 0, -0.4]}>
        <coneGeometry args={[0.16, 0.42, 4]} />
        <meshStandardMaterial color="#9eb4c9" emissive="#8ec5ff" emissiveIntensity={0.45} metalness={0.7} roughness={0.28} />
      </mesh>
      <mesh position={[0.2, 0.42, -0.02]} rotation={[0.15, 0, 0.4]}>
        <coneGeometry args={[0.16, 0.42, 4]} />
        <meshStandardMaterial color="#9eb4c9" emissive="#8ec5ff" emissiveIntensity={0.45} metalness={0.7} roughness={0.28} />
      </mesh>
      <mesh position={[-0.08, -0.02, 0.46]} rotation={[0.4, 0, 0.15]}>
        <coneGeometry args={[0.05, 0.2, 4]} />
        <meshBasicMaterial color="#ffffff" toneMapped={false} />
      </mesh>
      <mesh position={[0.08, -0.02, 0.46]} rotation={[0.4, 0, -0.15]}>
        <coneGeometry args={[0.05, 0.2, 4]} />
        <meshBasicMaterial color="#ffffff" toneMapped={false} />
      </mesh>
      {badge && (
        <sprite position={[0, 1.15, 0]} scale={[1.45, 1.45, 1]}>
          <spriteMaterial map={badge} transparent depthWrite={false} toneMapped={false} />
        </sprite>
      )}
    </group>
  );
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
    const life = 1 - smoothstep(elapsed / UNLOCK_DURATION);
    const pop = Math.max(0.72, Math.sin(clamp01(elapsed / 0.42) * Math.PI));
    if (root.current) root.current.rotation.y = elapsed * 2.5;
    if (ring.current) ring.current.scale.setScalar(Math.max(radius * 0.55, radius * smoothstep(elapsed / 0.2)));
    if (ringMat.current) ringMat.current.opacity = 0.9 * life;
    if (discMat.current) discMat.current.opacity = 0.5 * life;
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
    <group ref={root} position={aboveFloor(position)} renderOrder={3}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.2, 0]}>
        <circleGeometry args={[radius * 0.92, 48]} />
        <meshBasicMaterial ref={discMat} color={color} transparent opacity={0.42} depthWrite={false} side={THREE.DoubleSide} toneMapped={false} />
      </mesh>
      <mesh ref={ring} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.28, 0]}>
        <ringGeometry args={[0.82, 1, 40]} />
        <meshBasicMaterial
          ref={ringMat}
          color={color}
          opacity={0.9}
          {...additive}
          side={THREE.DoubleSide}
        />
      </mesh>
      <mesh ref={core} position={[0, 1.6, 0]}>
        <sphereGeometry args={[0.85, 18, 18]} />
        <meshBasicMaterial ref={coreMat} color={color} opacity={0.95} {...additive} />
      </mesh>
      <mesh position={[0, 1.6, 0]}>
        <sphereGeometry args={[0.34, 12, 12]} />
        <meshBasicMaterial color="#ffffff" opacity={0.9} {...additive} />
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
          <meshBasicMaterial color={color} transparent opacity={0.9} toneMapped={false} />
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
          <meshBasicMaterial color={color} transparent opacity={0.95} toneMapped={false} />
        </mesh>
      ))}
    </group>
  );
}
