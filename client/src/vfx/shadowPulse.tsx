import { useEffect, useLayoutEffect, useMemo, useRef, type MutableRefObject } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { InstancedSprites } from "./InstancedSprites";
import { clearSpritePool, createSpritePool, spawnSprite } from "./pool";
import { getVfxQuality } from "./quality";
import { hashSeed, mulberry32 } from "./random";
import { createRibbonGeometry, writeRibbon } from "./ribbon";
import {
  FAINT_GLINT_FRAG,
  GLINT_VERT,
  RIBBON_VERT,
  RING_VERT,
  SHADOW_GROUND_FRAG,
  SHADOW_POOL_FRAG,
  SHADOW_RIBBON_FRAG,
} from "./shaders";
import { seedShadowSuck, seedShadowWave, stepShadowSuck, stepShadowWave } from "./sim";
import { usePlayedClock } from "./bursts";

const SUCK_CAP = 18;
const WAVE_CAP = 48;
const WRAP_CAP = 10;
const SUCK_POINTS = 16;
const TENDRIL_POINTS = 22;

function shadowBudget() {
  const low = getVfxQuality() === "low";
  return {
    suck: low ? 8 : 16,
    wave: low ? 20 : 44,
    wrap: low ? 6 : 9,
    suckRibbons: low ? 2 : 4,
    tendrils: low ? 2 : 3,
    sizeScale: low ? 1.45 : 1,
  };
}

function ribbonMaterial(pop = false): THREE.ShaderMaterial {
  const mat = new THREE.ShaderMaterial({
    uniforms: {
      uColor: { value: new THREE.Color(pop ? "#1a0630" : "#2a1844") },
      uGlint: { value: new THREE.Color("#b69cff") },
      uFade: { value: 1 },
      uTime: { value: 0 },
      uPop: { value: pop ? 1 : 0 },
    },
    vertexShader: RIBBON_VERT,
    fragmentShader: SHADOW_RIBBON_FRAG,
    transparent: true,
    depthWrite: false,
    depthTest: true,
    toneMapped: false,
    side: THREE.DoubleSide,
  });
  mat.alphaTest = 0;
  return mat;
}

function smoothstep(t: number): number {
  const x = t < 0 ? 0 : t > 1 ? 1 : t;
  return x * x * (3 - 2 * x);
}

function SuckRibbon({
  index,
  count,
  elapsed,
}: {
  index: number;
  count: number;
  elapsed: MutableRefObject<number>;
}) {
  const geo = useMemo(() => createRibbonGeometry(SUCK_POINTS - 1), []);
  const material = useMemo(() => ribbonMaterial(), []);
  const points = useMemo(() => Array.from({ length: SUCK_POINTS }, () => new THREE.Vector3()), []);

  useEffect(() => () => {
    geo.dispose();
    material.dispose();
  }, [geo, material]);

  useFrame((state) => {
    const time = elapsed.current;
    const pull = smoothstep(time / 0.34);
    const base = (index / count) * Math.PI * 2;
    const outer = 2.45 * (1 - pull);
    for (let i = 0; i < SUCK_POINTS; i++) {
      const u = i / (SUCK_POINTS - 1);
      const rad = 0.12 + outer * u;
      const ang = base + u * 0.85 + Math.sin(time * 5 + index + u * 4) * 0.18 * (1 - pull);
      const y = 0.1 + Math.sin(u * Math.PI) * 0.42 * (1 - pull * 0.7);
      points[i].set(Math.cos(ang) * rad, y, Math.sin(ang) * rad);
    }
    writeRibbon(geo, points, 0.13 * (1 - pull * 0.45), state.camera.position);
    material.uniforms.uFade.value = (1 - smoothstep((time - 0.22) / 0.16)) * 0.9;
    material.uniforms.uTime.value = state.clock.elapsedTime;
  });

  return <mesh geometry={geo} material={material} frustumCulled={false} renderOrder={4} />;
}

/**
 * Inward suck, then a low inky wave that stops on the gameplay radius.
 * Crack-glow and the soft front live on the ground disc. The point light
 * stays dim and short so the grass keeps its color.
 */
export function ShadowPulseLook({
  radius,
  startTime,
  duration,
  frozenElapsed,
  onFinished,
}: {
  radius: number;
  startTime: number;
  duration: number;
  frozenElapsed?: number;
  onFinished?: () => void;
}) {
  const budget = useMemo(() => shadowBudget(), []);
  const suck = useMemo(() => createSpritePool(SUCK_CAP), []);
  const wave = useMemo(() => createSpritePool(WAVE_CAP), []);
  const elapsedRef = useRef(0);
  const finished = useRef(false);
  const finish = useRef(onFinished);
  finish.current = onFinished;
  const light = useRef<THREE.PointLight>(null);
  const clock = usePlayedClock(frozenElapsed);
  const warm = useRef(0);
  const crest = useRef(0);

  const ground = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uTime: { value: 0 },
        uFront: { value: 0.08 },
        uVeil: { value: 0 },
        uEdge: { value: 0 },
      },
      vertexShader: RING_VERT,
      fragmentShader: SHADOW_GROUND_FRAG,
      transparent: true,
      depthWrite: false,
      depthTest: true,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneFactor,
    });
    mat.alphaTest = 0;
    mat.polygonOffset = true;
    mat.polygonOffsetFactor = -2;
    mat.polygonOffsetUnits = -2;
    return mat;
  }, []);

  const pool = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uTime: { value: 0 },
        uFront: { value: 0.08 },
        uDensity: { value: 0 },
      },
      vertexShader: RING_VERT,
      fragmentShader: SHADOW_POOL_FRAG,
      transparent: true,
      depthWrite: false,
      depthTest: true,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.CustomBlending,
      blendSrc: THREE.DstColorFactor,
      blendDst: THREE.ZeroFactor,
    });
    mat.alphaTest = 0;
    mat.polygonOffset = true;
    mat.polygonOffsetFactor = -1;
    mat.polygonOffsetUnits = -1;
    return mat;
  }, []);

  useLayoutEffect(() => {
    clearSpritePool(suck);
    clearSpritePool(wave);
    const rand = mulberry32(hashSeed(radius, startTime, 0, 1, 77));
    seedShadowSuck(suck, rand, budget.suck);
    seedShadowWave(wave, radius, rand, budget.wave, budget.sizeScale);
  }, [suck, wave, radius, startTime, budget]);

  useEffect(() => () => {
    ground.dispose();
    pool.dispose();
    clearSpritePool(suck);
    clearSpritePool(wave);
  }, [ground, pool, suck, wave]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    elapsedRef.current = elapsed;
    if (typeof window !== "undefined") {
      (window as Window & { __vfxElapsed?: number }).__vfxElapsed = elapsed;
    }
    if (step > 0) {
      stepShadowSuck(suck, elapsed, step);
      stepShadowWave(wave, radius, elapsed, step);
    }

    let front = 0.35;
    for (let i = 0; i < wave.capacity; i++) {
      if (wave.active[i] === 0) continue;
      const edge = Math.hypot(wave.px[i], wave.pz[i]) + wave.size[i] * 0.42;
      if (edge > front) front = edge;
    }
    const waveIn = smoothstep((elapsed - 0.18) / 0.14);
    const edgeFade = waveIn * (1 - smoothstep((elapsed - 1.05) / 0.7));
    const veil = waveIn * (1 - smoothstep((elapsed - 1.28) / 1.15));
    const body = waveIn * (1 - smoothstep((elapsed - 1.25) / 1.2));
    ground.uniforms.uTime.value = state.clock.elapsedTime;
    const frontFrac = Math.min(1, front / radius);
    ground.uniforms.uFront.value = frontFrac;
    ground.uniforms.uEdge.value = edgeFade * 0.2;
    ground.uniforms.uVeil.value = veil * 0.95;
    pool.uniforms.uTime.value = state.clock.elapsedTime;
    pool.uniforms.uFront.value = frontFrac;
    pool.uniforms.uDensity.value = body;
    if (typeof window !== "undefined") {
      (window as Window & { __vfxFront?: number }).__vfxFront = frontFrac;
    }

    let intensity = 0;
    if (elapsed < 0.26) intensity = (elapsed / 0.26) * 1.6;
    else if (elapsed < 0.48) intensity = 1.6;
    else if (elapsed < 1.15) intensity = 1.6 * (1 - (elapsed - 0.48) / 0.67);
    if (light.current) {
      light.current.intensity = intensity;
      light.current.distance = 2.45;
    }

    if (frozenElapsed === undefined && !finished.current && elapsed >= duration) {
      finished.current = true;
      finish.current?.();
    }
  });

  return (
    <group>
      <pointLight ref={light} position={[0, 0.65, 0]} color="#6B46C1" intensity={0} distance={2.45} decay={2} />
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.04, 0]} frustumCulled={false} renderOrder={1}>
        <circleGeometry args={[radius, 72]} />
        <primitive object={pool} attach="material" />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.09, 0]} frustumCulled={false} renderOrder={3}>
        <circleGeometry args={[radius, 72]} />
        <primitive object={ground} attach="material" />
      </mesh>
      {Array.from({ length: budget.suckRibbons }, (_, index) => (
        <SuckRibbon key={index} index={index} count={budget.suckRibbons} elapsed={elapsedRef} />
      ))}
      <InstancedSprites pool={suck} mode="smoke" warm={warm} ink />
      <InstancedSprites pool={wave} mode="smoke" warm={warm} crest={crest} flat ink />
    </group>
  );
}

function ShadowTendril({ phase }: { phase: number }) {
  const geo = useMemo(() => createRibbonGeometry(TENDRIL_POINTS - 1), []);
  const material = useMemo(() => ribbonMaterial(true), []);
  const points = useMemo(() => Array.from({ length: TENDRIL_POINTS }, () => new THREE.Vector3()), []);

  useEffect(() => () => {
    geo.dispose();
    material.dispose();
  }, [geo, material]);

  useFrame((state) => {
    const time = state.clock.elapsedTime;
    for (let i = 0; i < TENDRIL_POINTS; i++) {
      const u = i / (TENDRIL_POINTS - 1);
      const ang = phase + time * 0.4 + u * 10.2;
      const y = -0.5 + u * 1.72 + Math.sin(time * 0.8 + phase + u * 4.0) * 0.04;
      const rad = 0.62 + Math.sin(u * 7.0 + phase) * 0.06 + Math.sin(time * 0.7 + u * 5.0) * 0.025;
      points[i].set(Math.cos(ang) * rad, y, Math.sin(ang) * rad);
    }
    writeRibbon(geo, points, 0.04, state.camera.position);
    material.uniforms.uFade.value = 0.95;
    material.uniforms.uTime.value = time;
  });

  return <mesh geometry={geo} material={material} frustumCulled={false} renderOrder={5} />;
}

function GripPool() {
  const material = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uTime: { value: 0 },
        uFront: { value: 1 },
        uDensity: { value: 0.96 },
      },
      vertexShader: RING_VERT,
      fragmentShader: SHADOW_POOL_FRAG,
      transparent: true,
      depthWrite: false,
      depthTest: true,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.CustomBlending,
      blendSrc: THREE.DstColorFactor,
      blendDst: THREE.ZeroFactor,
    });
    mat.alphaTest = 0;
    mat.polygonOffset = true;
    mat.polygonOffsetFactor = -1;
    mat.polygonOffsetUnits = -1;
    return mat;
  }, []);

  useEffect(() => () => material.dispose(), [material]);

  useFrame(({ clock }) => {
    material.uniforms.uTime.value = clock.elapsedTime;
  });

  return (
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.46, 0]} frustumCulled={false} renderOrder={1}>
      <circleGeometry args={[1.2, 40]} />
      <primitive object={material} attach="material" />
    </mesh>
  );
}

function FaintGlint() {
  const ref = useRef<THREE.Mesh>(null);
  const material = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uSize: { value: 0.5 },
        uColor: { value: new THREE.Color("#b69cff") },
        uTime: { value: 0 },
      },
      vertexShader: GLINT_VERT,
      fragmentShader: FAINT_GLINT_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneFactor,
    });
    mat.alphaTest = 0;
    return mat;
  }, []);

  useEffect(() => () => material.dispose(), [material]);

  useFrame(({ clock }) => {
    const time = clock.elapsedTime;
    material.uniforms.uTime.value = time;
    ref.current?.position.set(
      Math.cos(time * 0.7) * 0.2,
      0.32 + Math.sin(time * 1.4) * 0.04,
      Math.sin(time * 0.7) * 0.2,
    );
  });

  return (
    <mesh ref={ref} material={material} frustumCulled={false} renderOrder={6}>
      <planeGeometry args={[1, 1]} />
    </mesh>
  );
}

/** Dark smoke and a crawling tendril. Stays mounted for as long as the stun does. */
export function ShadowWrap() {
  const budget = useMemo(() => shadowBudget(), []);
  const smoke = useMemo(() => createSpritePool(WRAP_CAP), []);
  const warm = useRef(0);
  const crestWrap = useRef(0);

  useLayoutEffect(() => {
    clearSpritePool(smoke);
    const rand = mulberry32(hashSeed(budget.wrap, 3, 1, 9, 4));
    const inks: Array<[number, number, number]> = [
      [0.05, 0.012, 0.1],
      [0.08, 0.02, 0.16],
      [0.12, 0.03, 0.22],
    ];
    for (let i = 0; i < budget.wrap; i++) {
      const ink = inks[i % inks.length];
      spawnSprite(smoke, {
        x: 0,
        y: 0,
        z: 0,
        vx: 0,
        vy: 0,
        vz: 0,
        life: 60,
        size: 0.36 + (i % 3) * 0.1,
        grow: 0,
        spin: (rand() - 0.5) * 0.6,
        r: ink[0],
        g: ink[1],
        b: ink[2],
        seed: rand() * 8 + i,
      });
      smoke.opacity[i] = 0.8;
      smoke.active[i] = 1;
    }
  }, [smoke, budget.wrap]);

  useEffect(() => () => clearSpritePool(smoke), [smoke]);

  useFrame(({ clock }, dt) => {
    const time = clock.elapsedTime;
    const count = budget.wrap;
    const step = Math.min(dt, 0.05);
    for (let i = 0; i < count; i++) {
      const tendril = i % budget.tendrils;
      const phase = tendril * ((Math.PI * 2) / budget.tendrils) + 0.35;
      const u = ((Math.floor(i / budget.tendrils) / 3) + time * 0.14) % 1;
      const ang = phase + time * 0.4 + u * 10.2 + 0.4;
      const y = -0.28 + u * 1.45 + Math.sin(time * 1.1 + i) * 0.04;
      const rad = 0.72 + u * 0.22;
      smoke.px[i] = Math.cos(ang) * rad;
      smoke.py[i] = y;
      smoke.pz[i] = Math.sin(ang) * rad;
      smoke.opacity[i] = 0.72 + (i % 3) * 0.06;
      smoke.size[i] = smoke.size0[i];
      smoke.rot[i] += smoke.spin[i] * step;
      smoke.active[i] = 1;
    }
  });

  return (
    <group>
      <GripPool />
      {Array.from({ length: budget.tendrils }, (_, index) => (
        <ShadowTendril key={index} phase={index * ((Math.PI * 2) / budget.tendrils) + 0.35} />
      ))}
      <InstancedSprites pool={smoke} mode="smoke" warm={warm} crest={crestWrap} ink />
      <FaintGlint />
    </group>
  );
}
