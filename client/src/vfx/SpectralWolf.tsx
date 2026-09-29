import { useEffect, useMemo, useRef, type RefObject } from "react";
import { createPortal, useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import { InstancedSprites } from "./InstancedSprites";
import { emitSmokeBurst } from "./bursts";
import { createSpritePool } from "./pool";
import { nextRand } from "./random";
import { COLD_EMBER_CAP, WISP_CAP, emitColdEmber, emitWisp, stepColdEmbers, stepWisps } from "./sim";
import { WOLF_FRAG, WOLF_VERT } from "./shaders";
import { useVfxTextures } from "./textures";

export type Vec3 = [number, number, number];

let sharedGeometry: THREE.BufferGeometry | null = null;
let geometryUsers = 0;

function place(geometry: THREE.BufferGeometry, x: number, y: number, z: number, rx = 0, ry = 0, rz = 0): THREE.BufferGeometry {
  geometry.applyMatrix4(new THREE.Matrix4().compose(
    new THREE.Vector3(x, y, z),
    new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz)),
    new THREE.Vector3(1, 1, 1),
  ));
  return geometry;
}

function buildWolfGeometry(): THREE.BufferGeometry {
  const parts: THREE.BufferGeometry[] = [
    place(new THREE.BoxGeometry(0.42, 0.32, 0.86), 0, 0.02, 0.02),
    place(new THREE.BoxGeometry(0.36, 0.26, 0.32), 0, 0.08, 0.36),
    place(new THREE.BoxGeometry(0.38, 0.28, 0.34), 0, 0.04, -0.32),
    place(new THREE.BoxGeometry(0.22, 0.22, 0.28), 0, 0.22, 0.48, -0.45, 0, 0),
    place(new THREE.BoxGeometry(0.32, 0.26, 0.36), 0, 0.34, 0.66),
    place(new THREE.BoxGeometry(0.16, 0.12, 0.28), 0, 0.26, 0.92),
    place(new THREE.ConeGeometry(0.07, 0.24, 4), -0.1, 0.56, 0.6, 0.15, 0, -0.25),
    place(new THREE.ConeGeometry(0.07, 0.24, 4), 0.1, 0.56, 0.6, 0.15, 0, 0.25),
    place(new THREE.BoxGeometry(0.08, 0.36, 0.1), -0.14, -0.22, 0.26),
    place(new THREE.BoxGeometry(0.08, 0.36, 0.1), 0.14, -0.22, 0.26),
    place(new THREE.BoxGeometry(0.1, 0.34, 0.12), -0.13, -0.2, -0.28),
    place(new THREE.BoxGeometry(0.1, 0.34, 0.12), 0.13, -0.2, -0.28),
    place(new THREE.BoxGeometry(0.08, 0.08, 0.48), 0, 0.16, -0.62, 0.8, 0, 0),
  ];
  const merged = mergeGeometries(parts, false);
  for (const part of parts) part.dispose();
  if (!merged) {
    return new THREE.BoxGeometry(0.4, 0.3, 0.8);
  }
  merged.computeBoundingSphere();
  return merged;
}

function retainWolfGeometry(): THREE.BufferGeometry {
  geometryUsers += 1;
  if (!sharedGeometry) sharedGeometry = buildWolfGeometry();
  return sharedGeometry;
}

function releaseWolfGeometry(): void {
  geometryUsers = Math.max(0, geometryUsers - 1);
  if (geometryUsers === 0 && sharedGeometry) {
    sharedGeometry.dispose();
    sharedGeometry = null;
  }
}

const worldPos = new THREE.Vector3();
const forward = new THREE.Vector3();

function WolfTrail({ anchor }: { anchor: RefObject<THREE.Group | null> }) {
  const scene = useThree((state) => state.scene);
  const wisps = useMemo(() => createSpritePool(WISP_CAP), []);
  const embers = useMemo(() => createSpritePool(COLD_EMBER_CAP), []);
  const acc = useRef({ smoke: 0, ember: 0, n: 1, elapsed: 0 });

  useFrame((_, dt) => {
    const step = Math.min(dt, 1 / 30);
    const state = acc.current;
    state.elapsed += step;
    const group = anchor.current;
    if (group) {
      group.getWorldPosition(worldPos);
      group.getWorldDirection(forward);
      const backX = -forward.x;
      const backZ = -forward.z;
      state.smoke += step;
      state.ember += step;
      const smokeEvery = 0.05;
      const emberEvery = 0.08;
      if (state.smoke >= smokeEvery) {
        state.smoke = 0;
        const roll = nextRand(state);
        emitWisp(
          wisps,
          worldPos.x + backX * 0.72,
          worldPos.y + 0.05,
          worldPos.z + backZ * 0.72,
          backX,
          backZ,
          roll,
        );
      }
      if (state.ember >= emberEvery) {
        state.ember = 0;
        const roll = nextRand(state);
        emitColdEmber(
          embers,
          worldPos.x + backX * 0.4,
          worldPos.y + 0.12,
          worldPos.z + backZ * 0.4,
          backX,
          backZ,
          roll,
        );
      }
    }
    stepWisps(wisps, state.elapsed, step);
    stepColdEmbers(embers, state.elapsed, step);
  });

  return createPortal(
    <>
      <InstancedSprites pool={wisps} mode="smoke" />
      <InstancedSprites pool={embers} mode="additive" />
    </>,
    scene,
  );
}

/**
 * Spectral wolf. Gameplay still drives position and velocity; this is only
 * the look, the smoke trail, and the spawn / vanish puff.
 */
export function SpectralWolf({
  position,
  velocity = [0, 0, 0],
}: {
  position: Vec3;
  velocity?: Vec3;
}) {
  const group = useRef<THREE.Group>(null);
  const velRef = useRef(velocity);
  velRef.current = velocity;
  const textures = useVfxTextures();
  const geometry = useMemo(() => retainWolfGeometry(), []);
  const lastWorld = useRef(new THREE.Vector3(position[0], position[1], position[2]));
  const sawWorld = useRef(false);

  const material = useMemo(() => new THREE.ShaderMaterial({
    uniforms: {
      uTime: { value: 0 },
      uFade: { value: 1 },
      uNoise: { value: textures.smoke },
    },
    vertexShader: WOLF_VERT,
    fragmentShader: WOLF_FRAG,
    transparent: true,
    depthWrite: false,
    toneMapped: false,
    side: THREE.DoubleSide,
  }), [textures.smoke]);

  useEffect(() => {
    return () => {
      material.dispose();
    };
  }, [material]);

  useEffect(() => {
    return () => {
      releaseWolfGeometry();
    };
  }, []);

  useEffect(() => {
    const root = group.current;
    if (root) {
      root.getWorldPosition(lastWorld.current);
      sawWorld.current = true;
      emitSmokeBurst([lastWorld.current.x, lastWorld.current.y, lastWorld.current.z]);
    }
    return () => {
      if (!sawWorld.current) return;
      const p = lastWorld.current;
      emitSmokeBurst([p.x, p.y, p.z]);
    };
  }, []);

  useFrame(({ clock }, dt) => {
    const root = group.current;
    if (!root) return;
    const [vx, , vz] = velRef.current;
    if (Math.hypot(vx, vz) > 0.02) root.rotation.y = Math.atan2(vx, vz);
    root.getWorldPosition(lastWorld.current);
    sawWorld.current = true;
    const pulse = 0.82 + 0.18 * Math.sin(clock.elapsedTime * 4.2 + position[0]);
    material.uniforms.uTime.value += Math.min(dt, 1 / 30);
    material.uniforms.uFade.value = pulse;
  });

  return (
    <>
      <group ref={group} position={position}>
        <mesh geometry={geometry} material={material} scale={[1.35, 1.45, 1.9]} />
        <pointLight position={[0, 0.35, 0.15]} color="#c5dcff" intensity={2.2} distance={4.2} decay={2} />
      </group>
      <WolfTrail anchor={group} />
    </>
  );
}
