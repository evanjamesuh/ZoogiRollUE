import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { InstancedSprites } from "./InstancedSprites";
import { clearSpritePool, createSpritePool } from "./pool";
import { hashSeed, mulberry32 } from "./random";
import { impulseShake } from "./shake";
import { EMBER_CAP, SMOKE_CAP, seedBlastEmbers, seedBlastSmoke, stepEmbers, stepSmoke } from "./sim";
import { RING_FRAG, RING_VERT } from "./shaders";
import { useVfxTextures } from "./textures";
import { usePlayedClock } from "./bursts";

export type Vec3 = [number, number, number];

const DURATION = 2.45;

/**
 * Hotstreak blast and grenades. Gameplay radius is unchanged: the shockwave
 * stops on `radius`, and the smoke / embers are seeded to stay inside it.
 * The core is a point light with falloff, not a flat orange sphere.
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
  const heat = useRef<THREE.Mesh>(null);
  const heatMat = useRef<THREE.MeshBasicMaterial>(null);
  const scorchMat = useRef<THREE.MeshBasicMaterial>(null);
  const smoke = useMemo(() => createSpritePool(SMOKE_CAP), []);
  const embers = useMemo(() => createSpritePool(EMBER_CAP), []);
  const clock = usePlayedClock(frozenElapsed);
  const blast = useRef(new THREE.Vector3());

  const px = position[0];
  const py = position[1];
  const pz = position[2];
  const lift = Math.max(py, 0.35);

  useEffect(() => {
    clearSpritePool(smoke);
    clearSpritePool(embers);
    seedBlastSmoke(smoke, radius, mulberry32(hashSeed(px, py, pz, startTime, 11)), lift);
    seedBlastEmbers(embers, radius, mulberry32(hashSeed(px, py, pz, startTime, 29)), lift);
  }, [smoke, embers, radius, px, py, pz, startTime, lift]);

  const ringMaterial = useMemo(() => {
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uWave: { value: 0 },
        uOpacity: { value: 0 },
        uTime: { value: 0 },
      },
      vertexShader: RING_VERT,
      fragmentShader: RING_FRAG,
      transparent: true,
      depthWrite: false,
      toneMapped: false,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending,
    });
    return mat;
  }, []);

  useEffect(() => () => ringMaterial.dispose(), [ringMaterial]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    if (step > 0) {
      stepSmoke(smoke, elapsed, step);
      stepEmbers(embers, elapsed, step);
    }

    const flashAmt = elapsed < 0.18 ? Math.pow(1 - elapsed / 0.18, 1.35) : 0;
    const after = elapsed < 0.9 ? Math.pow(1 - elapsed / 0.9, 1.7) : 0;
    const scale = Math.min(1.15, radius / 6);
    if (light.current) {
      light.current.intensity = (flashAmt * 150 + after * 18) * scale;
      light.current.distance = radius * (1.2 + flashAmt);
      light.current.decay = 2;
      light.current.color.setRGB(1, 0.46 + flashAmt * 0.42, 0.14 + flashAmt * 0.52);
    }
    if (flash.current && flashMat.current) {
      flash.current.quaternion.copy(state.camera.quaternion);
      const size = Math.max(0.001, radius * (0.18 + flashAmt * 0.42));
      flash.current.scale.setScalar(size);
      flash.current.visible = flashAmt > 0.02;
      flashMat.current.opacity = flashAmt;
      flashMat.current.color.setRGB(3.4 * Math.max(flashAmt, 0.001), 1.7 * Math.max(flashAmt, 0.001), 0.7 * Math.max(flashAmt, 0.001));
    }
    if (heat.current && heatMat.current) {
      heat.current.quaternion.copy(state.camera.quaternion);
      const grow = radius * (0.22 + Math.min(elapsed / 0.5, 1) * 0.55);
      heat.current.scale.setScalar(Math.max(0.001, grow));
      const heatLife = elapsed < 0.55 ? 1 - elapsed / 0.55 : 0;
      heatMat.current.opacity = 0.28 * heatLife;
      heat.current.visible = heatLife > 0.02;
    }
    if (scorchMat.current) {
      const fade = 1 - Math.min(1, Math.max(0, (elapsed - 0.35) / 2.0));
      const arrive = Math.min(1, elapsed / 0.12);
      scorchMat.current.opacity = 0.72 * arrive * Math.max(fade, 0);
    }
    {
      const wave = Math.min(1, elapsed / 0.42);
      const fade = elapsed < 0.42 ? 1 : Math.max(0, 1 - (elapsed - 0.42) / 0.38);
      ringMaterial.uniforms.uWave.value = 0.08 + wave * 0.92;
      ringMaterial.uniforms.uOpacity.value = 0.55 * fade * Math.min(1, elapsed / 0.05);
      ringMaterial.uniforms.uTime.value = elapsed;
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
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.035, 0]}>
        <circleGeometry args={[radius * 0.62, 40]} />
        <meshBasicMaterial
          ref={scorchMat}
          map={textures.scorch}
          color="#ffffff"
          transparent
          opacity={0.7}
          depthWrite={false}
          toneMapped={false}
        />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.05, 0]}>
        <circleGeometry args={[radius, 48]} />
        <primitive object={ringMaterial} attach="material" />
      </mesh>
      <pointLight
        ref={light}
        position={[0, lift + 0.7, 0]}
        color="#ffb27a"
        intensity={0}
        distance={radius * 2}
        decay={2}
      />
      <mesh ref={heat} position={[0, lift + 0.15, 0]}>
        <planeGeometry args={[1, 1]} />
        <meshBasicMaterial
          ref={heatMat}
          map={textures.smoke}
          color="#ff7a3c"
          transparent
          opacity={0}
          depthWrite={false}
          blending={THREE.AdditiveBlending}
          toneMapped={false}
        />
      </mesh>
      <mesh ref={flash} position={[0, lift + 0.2, 0]}>
        <planeGeometry args={[1, 1]} />
        <meshBasicMaterial
          ref={flashMat}
          map={textures.ember}
          color="#fff4e4"
          transparent
          opacity={0}
          depthWrite={false}
          blending={THREE.AdditiveBlending}
          toneMapped={false}
        />
      </mesh>
      <InstancedSprites pool={smoke} mode="smoke" />
      <InstancedSprites pool={embers} mode="additive" />
    </group>
  );
}
