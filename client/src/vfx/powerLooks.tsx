import { useEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { InstancedSprites } from "./InstancedSprites";
import { clearSpritePool, createSpritePool } from "./pool";
import { mulberry32 } from "./random";
import { createRibbonGeometry, layHelix, layJagged, writeRibbon } from "./ribbon";
import {
  GLINT_FRAG,
  GLINT_VERT,
  GROUND_GLOW_FRAG,
  RIBBON_FRAG,
  RIBBON_VERT,
  RING_VERT,
  SHELL_FRAG,
  SHELL_VERT,
  STUN_RIM_FRAG,
} from "./shaders";
import { getVfxQuality } from "./quality";
import { ZOOGI_FX_SCALE } from "@/lib/restHeight";
import {
  seedBurstSparks,
  seedOzone,
  seedRisingEmbers,
  stepGlints,
  stepHaze,
  stepRising,
} from "./sim";
import { usePlayedClock } from "./bursts";

export type Vec3 = [number, number, number];

const ARC_POINTS = 8;

function ribbonMaterial(color: THREE.Color, cloth: boolean, hot = false): THREE.ShaderMaterial {
  const mat = new THREE.ShaderMaterial({
    uniforms: {
      uColor: { value: color },
      uFade: { value: 1 },
      uCloth: { value: cloth ? 1 : 0 },
      uEndFade: { value: 0 },
      uHot: { value: hot ? 1 : 0 },
    },
    vertexShader: RIBBON_VERT,
    fragmentShader: RIBBON_FRAG,
    transparent: true,
    depthWrite: false,
    depthTest: true,
    toneMapped: false,
    side: THREE.DoubleSide,
  });
  mat.alphaTest = 0;
  if (!cloth) {
    mat.blending = THREE.CustomBlending;
    mat.blendSrc = THREE.OneFactor;
    mat.blendDst = THREE.OneFactor;
    if (hot) mat.depthTest = false;
  }
  return mat;
}

export function JaggedArc({
  from,
  to,
  seed,
  width = 0.1,
  color = "#d7ecff",
  fade = 1,
  sag = 0.2,
  cloth = false,
  endFade = 0,
  hot = true,
  flicker = 0,
}: {
  from: Vec3;
  to: Vec3;
  seed: number;
  width?: number;
  color?: string;
  fade?: number;
  sag?: number;
  cloth?: boolean;
  endFade?: number;
  hot?: boolean;
  flicker?: number;
}) {
  const geo = useMemo(() => createRibbonGeometry(ARC_POINTS - 1), []);
  const material = useMemo(() => ribbonMaterial(new THREE.Color(color), cloth, hot && !cloth), [color, cloth, hot]);
  const points = useMemo(() => Array.from({ length: ARC_POINTS }, () => new THREE.Vector3()), []);
  const fromV = useRef(new THREE.Vector3());
  const toV = useRef(new THREE.Vector3());
  const jitterSeed = useRef(seed || 1);
  const flickerClock = useRef(0);
  const jitters = useRef<[number, number][]>([]);
  if (jitters.current.length === 0) {
    const rand = mulberry32(jitterSeed.current);
    jitters.current = Array.from({ length: ARC_POINTS - 2 }, () => [(rand() - 0.5) * 2, (rand() - 0.5) * 2]);
  }

  useEffect(() => () => {
    geo.dispose();
    material.dispose();
  }, [geo, material]);

  useFrame((state, dt) => {
    if (flicker > 0) {
      flickerClock.current += Math.min(dt, 1 / 30);
      if (flickerClock.current > flicker) {
        flickerClock.current = 0;
        jitterSeed.current = (jitterSeed.current + 17) % 9973;
        const rand = mulberry32(jitterSeed.current || 1);
        const next = jitters.current;
        for (let i = 0; i < next.length; i++) {
          next[i][0] = (rand() - 0.5) * 2;
          next[i][1] = (rand() - 0.5) * 2;
        }
      }
    }
    fromV.current.set(from[0], from[1], from[2]);
    toV.current.set(to[0], to[1], to[2]);
    layJagged(fromV.current, toV.current, jitters.current, points, sag);
    writeRibbon(geo, points, width, state.camera.position);
    material.uniforms.uFade.value = fade;
    material.uniforms.uEndFade.value = endFade;
  });

  return <mesh geometry={geo} material={material} frustumCulled={false} />;
}

export function StraightBeam({
  from,
  to,
  width = 0.035,
  color = "#f4fbff",
  endFade = 0,
}: {
  from: Vec3;
  to: Vec3;
  width?: number;
  color?: string;
  endFade?: number;
}) {
  const geo = useMemo(() => createRibbonGeometry(1), []);
  const material = useMemo(() => ribbonMaterial(new THREE.Color(color), false, false), [color]);
  const points = useMemo(() => [new THREE.Vector3(), new THREE.Vector3()], []);

  useEffect(() => () => {
    geo.dispose();
    material.dispose();
  }, [geo, material]);

  useFrame((state) => {
    points[0].set(from[0], from[1], from[2]);
    points[1].set(to[0], to[1], to[2]);
    writeRibbon(geo, points, width, state.camera.position);
    material.uniforms.uFade.value = 1;
    material.uniforms.uEndFade.value = endFade;
  });

  return <mesh geometry={geo} material={material} frustumCulled={false} />;
}

function BeamMotes({ from, to }: { from: Vec3; to: Vec3 }) {
  const pool = useMemo(() => {
    const sprites = createSpritePool(8);
    for (let i = 0; i < sprites.capacity; i++) {
      sprites.active[i] = 1;
      sprites.opacity[i] = 0.4;
      sprites.size[i] = 0.045;
      sprites.seed[i] = -(i + 1);
      sprites.r[i] = 0.75;
      sprites.g[i] = 0.86;
      sprites.b[i] = 1;
    }
    sprites.alive = sprites.capacity;
    return sprites;
  }, []);

  useEffect(() => () => clearSpritePool(pool), [pool]);

  useFrame(({ clock }) => {
    const time = clock.elapsedTime;
    for (let i = 0; i < pool.capacity; i++) {
      const u = ((i + 0.35) / pool.capacity + time * 0.04) % 1;
      const dx = to[0] - from[0];
      const dz = to[2] - from[2];
      const span = Math.hypot(dx, dz) || 1;
      const side = (i % 2 === 0 ? 1 : -1) * (0.1 + (i % 3) * 0.045);
      const wob = Math.sin(time * 1.8 + i * 1.7) * 0.04;
      pool.px[i] = from[0] + dx * u + (-dz / span) * side + wob;
      pool.py[i] = from[1] + (to[1] - from[1]) * u + Math.cos(time * 1.4 + i) * 0.05;
      pool.pz[i] = from[2] + dz * u + (dx / span) * side - wob * 0.4;
      pool.opacity[i] = 0.22 + 0.18 * Math.sin(time * 3.2 + i);
      pool.size[i] = 0.04 + (i % 3) * 0.015;
      pool.active[i] = 1;
    }
  });

  return <InstancedSprites pool={pool} mode="ember" />;
}

export function AimBeam({ from, to }: { from: Vec3; to: Vec3 }) {
  return (
    <group>
      <StraightBeam from={from} to={to} width={0.12 * ZOOGI_FX_SCALE} color="#b7d7ff" endFade={1} />
      <StraightBeam from={from} to={to} width={0.035 * ZOOGI_FX_SCALE} color="#f7fbff" endFade={1} />
      <Glint position={to} size={0.55} color="#eaf6ff" />
      <BeamMotes from={from} to={to} />
    </group>
  );
}

export function AimPath({ points }: { points: Vec3[] }) {
  const count = Math.max(2, points.length);
  const glowGeo = useMemo(() => createRibbonGeometry(count - 1), [count]);
  const coreGeo = useMemo(() => createRibbonGeometry(count - 1), [count]);
  const glow = useMemo(() => ribbonMaterial(new THREE.Color("#9ecbff"), false), []);
  const core = useMemo(() => ribbonMaterial(new THREE.Color("#f7fbff"), false), []);
  const buf = useMemo(() => Array.from({ length: count }, () => new THREE.Vector3()), [count]);

  useEffect(() => () => {
    glowGeo.dispose();
    coreGeo.dispose();
    glow.dispose();
    core.dispose();
  }, [glowGeo, coreGeo, glow, core]);

  useFrame((state) => {
    const n = Math.min(points.length, buf.length);
    for (let i = 0; i < n; i++) buf[i].set(points[i][0], points[i][1], points[i][2]);
    writeRibbon(glowGeo, buf, 0.11 * ZOOGI_FX_SCALE, state.camera.position);
    writeRibbon(coreGeo, buf, 0.032 * ZOOGI_FX_SCALE, state.camera.position);
    glow.uniforms.uFade.value = 0.9;
    core.uniforms.uFade.value = 1;
    glow.uniforms.uEndFade.value = 1;
    core.uniforms.uEndFade.value = 1;
  });

  const end = points[points.length - 1] ?? points[0];
  return (
    <group>
      <mesh geometry={glowGeo} material={glow} frustumCulled={false} />
      <mesh geometry={coreGeo} material={core} frustumCulled={false} />
      {end && <Glint position={end} size={0.46} color="#eaf6ff" />}
      {points[0] && end && <BeamMotes from={points[0]} to={end} />}
    </group>
  );
}

export function Glint({
  position,
  size = 0.45,
  color = "#e7f4ff",
}: {
  position: Vec3;
  size?: number;
  color?: string;
}) {
  const material = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uSize: { value: size * ZOOGI_FX_SCALE },
        uColor: { value: new THREE.Color(color) },
        uTime: { value: 0 },
      },
      vertexShader: GLINT_VERT,
      fragmentShader: GLINT_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneFactor,
    });
    mat.alphaTest = 0;
    return mat;
  }, [size, color]);

  useEffect(() => () => material.dispose(), [material]);

  useFrame((_, dt) => {
    material.uniforms.uTime.value += Math.min(dt, 1 / 30);
  });

  return (
    <mesh position={position} material={material} frustumCulled={false}>
      <planeGeometry args={[1, 1]} />
    </mesh>
  );
}

export function RicochetShell() {
  const material = useMemo(() => new THREE.ShaderMaterial({
    uniforms: { uTime: { value: 0 } },
    vertexShader: SHELL_VERT,
    fragmentShader: SHELL_FRAG,
    transparent: true,
    depthWrite: false,
    toneMapped: false,
    blending: THREE.AdditiveBlending,
    side: THREE.FrontSide,
  }), []);

  useEffect(() => () => material.dispose(), [material]);

  useFrame((_, dt) => {
    material.uniforms.uTime.value += Math.min(dt, 1 / 30);
  });

  return (
    <mesh material={material} renderOrder={2}>
      <sphereGeometry args={[0.68, 28, 20]} />
    </mesh>
  );
}

const HELIX = 64;

export function BindRibbons() {
  const started = useRef<number | null>(null);
  const coils = useMemo(() => [0, Math.PI], []);
  const geos = useMemo(() => coils.map(() => createRibbonGeometry(HELIX - 1)), [coils]);
  const materials = useMemo(
    () => coils.map(() => ribbonMaterial(new THREE.Color("#c4a574"), true)),
    [coils],
  );
  const bufs = useMemo(
    () => coils.map(() => Array.from({ length: HELIX }, () => new THREE.Vector3())),
    [coils],
  );
  const dust = useMemo(() => {
    const pool = createSpritePool(8);
    for (let i = 0; i < pool.capacity; i++) {
      pool.active[i] = 1;
      pool.opacity[i] = 0.45;
      pool.size[i] = 0.26 * ZOOGI_FX_SCALE;
      pool.rot[i] = i;
      pool.seed[i] = i + 1;
    }
    pool.alive = pool.capacity;
    return pool;
  }, []);

  useEffect(() => () => {
    geos.forEach((geo) => geo.dispose());
    materials.forEach((mat) => mat.dispose());
    clearSpritePool(dust);
  }, [geos, materials, dust]);

  useFrame((state) => {
    const clock = state.clock;
    if (started.current === null) started.current = clock.elapsedTime;
    const age = clock.elapsedTime - started.current;
    const tighten = Math.min(1, age / 0.4);
    const radius = 0.72 - tighten * 0.2;
    coils.forEach((phase, index) => {
      layHelix(2.35, radius, clock.elapsedTime, phase, bufs[index]);
      writeRibbon(geos[index], bufs[index], 0.25, state.camera.position);
      materials[index].uniforms.uFade.value = 0.92;
    });
    const spin = clock.elapsedTime * 0.9;
    for (let i = 0; i < dust.capacity; i++) {
      const ang = spin + (i / dust.capacity) * Math.PI * 2;
      const reach = 0.72 + (i % 3) * 0.12;
      dust.px[i] = Math.cos(ang) * reach;
      dust.py[i] = 0.06 + (i % 4) * 0.07;
      dust.pz[i] = Math.sin(ang) * reach;
      dust.opacity[i] = 0.38;
      dust.size[i] = (0.16 + (i % 3) * 0.05) * ZOOGI_FX_SCALE;
      dust.active[i] = 1;
    }
  });

  return (
    <group>
      {geos.map((geo, i) => (
        <mesh key={i} geometry={geo} material={materials[i]} frustumCulled={false} />
      ))}
      <InstancedSprites pool={dust} mode="dust" />
    </group>
  );
}

function localCrackles(originY: number): Array<[Vec3, Vec3]> {
  return [
    [[0, originY, 0], [1.15, originY + 0.8, 0.2]],
    [[0, originY, 0], [-0.4, originY + 1.1, 0.9]],
    [[0, originY, 0], [0.2, originY + 0.5, -1.2]],
  ];
}

export function BoltDischarge({
  position,
  targets,
  frozenElapsed,
}: {
  position: Vec3;
  targets: Vec3[];
  frozenElapsed?: number;
}) {
  const clock = usePlayedClock(frozenElapsed);
  const light = useRef<THREE.PointLight>(null);
  const quality = getVfxQuality() === "low" ? 0.5 : 1;
  const sparks = useMemo(() => createSpritePool(16), []);
  const haze = useMemo(() => createSpritePool(8), []);
  const seeded = useRef(false);
  const anchor = useMemo(() => [position[0], position[1], position[2]] as Vec3, [position]);

  useEffect(() => {
    clearSpritePool(sparks);
    clearSpritePool(haze);
    const rand = mulberry32((Math.floor(position[0] * 10) ^ Math.floor(position[2] * 10) ^ 17) >>> 0 || 1);
    seedBurstSparks(sparks, rand, Math.round(12 * quality), [0.72, 0.88, 1], [0, 1, 0]);
    seedOzone(haze, rand, Math.round(5 * quality));
    seeded.current = true;
  }, [sparks, haze, position, quality]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    if (typeof window !== "undefined") {
      (window as Window & { __vfxElapsed?: number }).__vfxElapsed = elapsed;
    }
    if (step > 0) {
      stepGlints(sparks, elapsed, step);
      stepHaze(haze, elapsed, step);
    }
    const flash = elapsed < 0.18 ? 1 - elapsed / 0.18 : Math.max(0, 1 - (elapsed - 0.18) / 0.55);
    if (light.current) {
      light.current.intensity = flash * 10.8;
      light.current.distance = 2.2;
    }
  });

  const pairs = targets.length > 0
    ? targets.slice(0, 4).map((target) => [anchor, [target[0], Math.max(target[1], 0.45), target[2]]] as [Vec3, Vec3])
    : localCrackles(anchor[1]);

  return (
    <group>
      <pointLight ref={light} position={anchor} color="#d7ecff" intensity={0} distance={2.2} decay={2} />
      <ArcSet pairs={pairs} />
      <group position={anchor}>
        <InstancedSprites pool={sparks} mode="ember" />
        <InstancedSprites pool={haze} mode="mist" />
      </group>
    </group>
  );
}

function ArcSet({ pairs }: { pairs: Array<[Vec3, Vec3]> }) {
  const bolts = pairs.flatMap((pair, i) => {
    const from = pair[0];
    const to = pair[1];
    const pointAt = (t: number, lift: number): Vec3 => [
      from[0] + (to[0] - from[0]) * t,
      from[1] + (to[1] - from[1]) * t + lift,
      from[2] + (to[2] - from[2]) * t,
    ];
    const forks: Array<{ key: string; from: Vec3; to: Vec3; salt: number }> = [
      { key: `a${i}`, from: pointAt(0.34, 0.12), to: [pointAt(0.34, 0.12)[0] + 0.55 * ZOOGI_FX_SCALE, pointAt(0.34, 0.12)[1] + 0.35 * ZOOGI_FX_SCALE, pointAt(0.34, 0.12)[2] - 0.4 * ZOOGI_FX_SCALE], salt: 11 },
      { key: `b${i}`, from: pointAt(0.58, 0.08), to: [pointAt(0.58, 0.08)[0] - 0.48 * ZOOGI_FX_SCALE, pointAt(0.58, 0.08)[1] + 0.42 * ZOOGI_FX_SCALE, pointAt(0.58, 0.08)[2] + 0.36 * ZOOGI_FX_SCALE], salt: 19 },
      { key: `c${i}`, from: pointAt(0.46, 0.16), to: [pointAt(0.46, 0.16)[0] + 0.22 * ZOOGI_FX_SCALE, pointAt(0.46, 0.16)[1] + 0.55 * ZOOGI_FX_SCALE, pointAt(0.46, 0.16)[2] + 0.5 * ZOOGI_FX_SCALE], salt: 23 },
    ];
    return [
      { key: `glow${i}`, from, to, width: 0.12 * ZOOGI_FX_SCALE, color: "#9ecfff", sag: 0.1, salt: 3, hot: false },
      { key: `core${i}`, from, to, width: 0.032 * ZOOGI_FX_SCALE, color: "#ffffff", sag: 0.1, salt: 3, hot: true },
      ...forks.map((fork) => ({
        key: fork.key,
        from: fork.from,
        to: fork.to,
        width: 0.045 * ZOOGI_FX_SCALE,
        color: "#c5e6ff",
        sag: 0.05,
        salt: fork.salt,
        hot: false,
      })),
    ];
  });
  return (
    <group>
      {bolts.map((bolt) => (
        <JaggedArc
          key={bolt.key}
          from={bolt.from}
          to={bolt.to}
          seed={17 + bolt.salt}
          flicker={0.09}
          width={bolt.width}
          color={bolt.color}
          sag={bolt.sag}
          hot={bolt.hot}
        />
      ))}
    </group>
  );
}

export function StunCrawlers() {
  const tick = useRef(0);
  const seed = useRef(2);
  const ends = useRef<[Vec3, Vec3][]>([
    [[0.2, 0.15, 0.35], [-0.35, 0.7, -0.1]],
    [[-0.3, 0.2, -0.25], [0.25, 0.75, 0.3]],
    [[0.05, 0.18, 0.1], [0.15, 0.62, -0.35]],
  ]);
  const rim = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: { uTime: { value: 0 } },
      vertexShader: SHELL_VERT,
      fragmentShader: STUN_RIM_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneFactor,
      side: THREE.FrontSide,
    });
    mat.alphaTest = 0;
    return mat;
  }, []);

  useEffect(() => () => rim.dispose(), [rim]);

  useFrame((_, dt) => {
    const step = Math.min(dt, 1 / 30);
    tick.current += step;
    rim.uniforms.uTime.value += step;
    if (tick.current > 0.14) {
      tick.current = 0;
      seed.current += 1;
      const rand = mulberry32(seed.current * 19 + 5);
      for (const pair of ends.current) {
        const a = rand() * Math.PI * 2;
        const b = a + 1.2 + rand();
        const ya = 0.05 + rand() * 0.35;
        const yb = 0.35 + rand() * 0.55;
        pair[0][0] = Math.cos(a) * 0.48;
        pair[0][1] = ya;
        pair[0][2] = Math.sin(a) * 0.48;
        pair[1][0] = Math.cos(b) * 0.5;
        pair[1][1] = yb;
        pair[1][2] = Math.sin(b) * 0.5;
      }
    }
  });

  return (
    <group>
      <mesh material={rim} frustumCulled={false}>
        <sphereGeometry args={[0.56, 24, 18]} />
      </mesh>
      {ends.current.map((pair, i) => (
        <JaggedArc
          key={i}
          from={pair[0]}
          to={pair[1]}
          seed={11 + i}
          flicker={0.14}
          width={0.08}
          color="#f4fbff"
          sag={0.06}
          fade={0.95}
        />
      ))}
    </group>
  );
}

export function UnlockGlow({
  position,
  frozenElapsed,
}: {
  position: Vec3;
  frozenElapsed?: number;
}) {
  const clock = usePlayedClock(frozenElapsed);
  const light = useRef<THREE.PointLight>(null);
  const ring = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uColor: { value: new THREE.Color("#ffc56a") },
        uOpacity: { value: 0 },
        uWave: { value: 0.2 },
      },
      vertexShader: RING_VERT,
      fragmentShader: GROUND_GLOW_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
    });
    mat.alphaTest = 0;
    return mat;
  }, []);
  const embers = useMemo(() => createSpritePool(18), []);

  useEffect(() => {
    const rand = mulberry32(91);
    clearSpritePool(embers);
    seedRisingEmbers(embers, rand, getVfxQuality() === "low" ? 8 : 16);
    return () => ring.dispose();
  }, [embers, ring]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    if (typeof window !== "undefined") {
      (window as Window & { __vfxElapsed?: number }).__vfxElapsed = elapsed;
    }
    if (step > 0) stepRising(embers, elapsed, step);
    const flash = elapsed < 0.16 ? elapsed / 0.16 : Math.max(0, 1 - (elapsed - 0.16) / 0.7);
    if (light.current) {
      light.current.intensity = flash * 16;
      light.current.distance = 2.2;
    }
    ring.uniforms.uWave.value = 0.28 + Math.min(1, elapsed / 0.4) * 0.48;
    ring.uniforms.uOpacity.value = 0.95 * flash;
  });

  return (
    <group position={position}>
      <pointLight ref={light} position={[0, 0.7, 0]} color="#ff9a3a" intensity={0} distance={2.2} decay={2} />
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.42, 0]} scale={ZOOGI_FX_SCALE}>
        <circleGeometry args={[0.75, 40]} />
        <primitive object={ring} attach="material" />
      </mesh>
      <InstancedSprites pool={embers} mode="ember" />
    </group>
  );
}
