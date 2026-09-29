import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { InstancedSprites } from "./InstancedSprites";
import { clearSpritePool, createSpritePool } from "./pool";
import { hashSeed, mulberry32 } from "./random";
import { impulseShake } from "./shake";
import {
  EMBER_CAP,
  FIRE_CAP,
  SMOKE_CAP,
  seedBlastEmbers,
  seedBlastFire,
  seedBlastSmoke,
  stepEmbers,
  stepFire,
  stepSmoke,
} from "./sim";
import { RING_FRAG, RING_VERT } from "./shaders";
import { useVfxTextures } from "./textures";
import { usePlayedClock } from "./bursts";

export type Vec3 = [number, number, number];

const DURATION = 4;

/**
 * Hotstreak blast and grenades. Gameplay radius is unchanged: smoke, fire,
 * and embers are seeded so the visible cloud stays on `radius`.
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
  const shook = useRef(false);
  const textures = useVfxTextures();
  const light = useRef<THREE.PointLight>(null);
  const flash = useRef<THREE.Mesh>(null);
  const flashMat = useRef<THREE.MeshBasicMaterial>(null);
  const scorchMat = useRef<THREE.MeshBasicMaterial>(null);
  const smoke = useMemo(() => createSpritePool(SMOKE_CAP), []);
  const fire = useMemo(() => createSpritePool(FIRE_CAP), []);
  const embers = useMemo(() => createSpritePool(EMBER_CAP), []);
  const clock = usePlayedClock(frozenElapsed);
  const blast = useRef(new THREE.Vector3());
  const warm = useRef(1);

  const px = position[0];
  const py = position[1];
  const pz = position[2];
  const lift = Math.max(py, 0.35);

  useEffect(() => {
    clearSpritePool(smoke);
    clearSpritePool(fire);
    clearSpritePool(embers);
    seedBlastSmoke(smoke, radius, mulberry32(hashSeed(px, py, pz, startTime, 11)), lift);
    seedBlastFire(fire, radius, mulberry32(hashSeed(px, py, pz, startTime, 17)), lift);
    seedBlastEmbers(embers, radius, mulberry32(hashSeed(px, py, pz, startTime, 29)), lift);
  }, [smoke, fire, embers, radius, px, py, pz, startTime, lift]);

  const ringMaterial = useMemo(() => {
    return new THREE.ShaderMaterial({
      uniforms: {
        uWave: { value: 0 },
        uOpacity: { value: 0 },
      },
      vertexShader: RING_VERT,
      fragmentShader: RING_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.NormalBlending,
    });
  }, []);

  useEffect(() => () => ringMaterial.dispose(), [ringMaterial]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    if (typeof window !== "undefined") {
      (window as Window & { __vfxElapsed?: number }).__vfxElapsed = elapsed;
    }
    warm.current = elapsed < 0.5 ? 1 - elapsed / 0.5 : 0;
    if (step > 0) {
      stepSmoke(smoke, elapsed, step);
      stepFire(fire, elapsed, step);
      stepEmbers(embers, elapsed, step);
    }

    const flashAmt = elapsed < 0.16 ? Math.pow(1 - elapsed / 0.16, 1.2) : 0;
    const after = elapsed < 0.7 ? Math.pow(1 - elapsed / 0.7, 1.4) : 0;
    const scale = Math.min(1.15, radius / 6);
    if (light.current) {
      light.current.intensity = (flashAmt * 80 + after * 22) * scale;
      light.current.distance = radius * (1.35 + flashAmt * 0.4);
      light.current.decay = 2;
      light.current.color.setRGB(1, 0.24, 0.03);
    }
    if (flash.current && flashMat.current) {
      flash.current.quaternion.copy(state.camera.quaternion);
      const breadth = Math.min(3.6, 1.6 + radius * 0.25);
      flash.current.scale.setScalar(Math.max(0.001, breadth));
      flash.current.visible = flashAmt > 0.02;
      flashMat.current.opacity = flashAmt;
      flashMat.current.color.setRGB(3.4, 1.7, 0.62);
    }
    if (scorchMat.current) {
      const arrive = Math.min(1, elapsed / 0.12);
      const fade = elapsed < 1.2 ? 1 : Math.max(0, 1 - (elapsed - 1.2) / 2.4);
      scorchMat.current.opacity = 0.6 * arrive * fade;
    }
    {
      const wave = Math.min(1, elapsed / 0.32);
      const fade = elapsed < 0.22 ? Math.min(1, elapsed / 0.04) : Math.max(0, 1 - (elapsed - 0.22) / 0.16);
      ringMaterial.uniforms.uWave.value = 0.06 + wave * 0.9;
      ringMaterial.uniforms.uOpacity.value = 0.22 * fade;
    }

    if (!shook.current && frozenElapsed === undefined) {
      shook.current = true;
      blast.current.set(px, lift, pz);
      impulseShake(state.camera, blast.current, radius);
    }

    if (frozenElapsed === undefined && !ended.current && elapsed >= DURATION) {
      ended.current = true;
      setDone(true);
    }
  });

  if (done) return null;

  return (
    <group position={[px, 0, pz]} renderOrder={3}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.04, 0]}>
        <circleGeometry args={[radius * 0.5, 40]} />
        <meshBasicMaterial
          ref={scorchMat}
          map={textures.scorch}
          color="#ffffff"
          transparent
          opacity={0.9}
          depthWrite={false}
          toneMapped={false}
          polygonOffset
          polygonOffsetFactor={-2}
        />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.06, 0]}>
        <circleGeometry args={[radius, 64]} />
        <primitive object={ringMaterial} attach="material" />
      </mesh>
      <pointLight
        ref={light}
        position={[0, lift + 0.8, 0]}
        color="#ff5a12"
        intensity={0}
        distance={radius * 2}
        decay={2}
      />
      <mesh ref={flash} position={[0, lift + 0.35, 0]}>
        <planeGeometry args={[1, 1]} />
        <meshBasicMaterial
          ref={flashMat}
          map={textures.ember}
          color="#ffd2a0"
          transparent
          opacity={0}
          depthWrite={false}
          blending={THREE.AdditiveBlending}
          toneMapped={false}
        />
      </mesh>
      <InstancedSprites pool={smoke} mode="smoke" warm={warm} />
      <InstancedSprites pool={fire} mode="fire" />
      <InstancedSprites pool={embers} mode="ember" />
    </group>
  );
}
