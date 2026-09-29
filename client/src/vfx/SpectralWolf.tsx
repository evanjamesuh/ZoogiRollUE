import { useEffect, useMemo, useRef, type RefObject } from "react";
import { createPortal, useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import { InstancedSprites } from "./InstancedSprites";
import { emitSmokeBurst } from "./bursts";
import { createSpritePool } from "./pool";
import { nextRand } from "./random";
import { COLD_EMBER_CAP, WISP_CAP, emitColdEmber, emitMist, stepColdEmbers, stepMist } from "./sim";
import { WOLF_FRAG, WOLF_VERT } from "./shaders";
import { useVfxTextures } from "./textures";

export type Vec3 = [number, number, number];

let sharedGeometry: THREE.BufferGeometry | null = null;
let geometryUsers = 0;
let wolfDrop = 0;

const compose = new THREE.Matrix4();
const composePos = new THREE.Vector3();
const composeQuat = new THREE.Quaternion();
const composeScale = new THREE.Vector3();
const composeEuler = new THREE.Euler();

function place(
  geometry: THREE.BufferGeometry,
  x: number,
  y: number,
  z: number,
  sx: number,
  sy: number,
  sz: number,
  rx = 0,
  ry = 0,
  rz = 0,
): THREE.BufferGeometry {
  composeEuler.set(rx, ry, rz);
  composeQuat.setFromEuler(composeEuler);
  composePos.set(x, y, z);
  composeScale.set(sx, sy, sz);
  compose.compose(composePos, composeQuat, composeScale);
  geometry.applyMatrix4(compose);
  return geometry;
}

function ball(
  x: number,
  y: number,
  z: number,
  sx: number,
  sy: number,
  sz: number,
  rx = 0,
  ry = 0,
  rz = 0,
): THREE.BufferGeometry {
  return place(new THREE.SphereGeometry(1, 12, 9), x, y, z, sx, sy, sz, rx, ry, rz);
}

function buildWolfGeometry(): THREE.BufferGeometry {
  const parts: THREE.BufferGeometry[] = [
    ball(0, 0.58, 0.05, 0.32, 0.24, 0.4),
    ball(0, 0.42, -0.28, 0.26, 0.2, 0.34),
    ball(0, 0.46, -0.58, 0.3, 0.22, 0.28),
    ball(0, 0.36, 0.38, 0.16, 0.14, 0.2, -0.35),
    ball(0, 0.22, 0.62, 0.18, 0.15, 0.2),
    ball(0, 0.14, 0.86, 0.09, 0.08, 0.2),
    ball(0, 0.12, 1.02, 0.05, 0.045, 0.06),
    place(new THREE.ConeGeometry(1, 1, 8), -0.09, 0.46, 0.55, 0.06, 0.26, 0.05, 0.15, 0, -0.4),
    place(new THREE.ConeGeometry(1, 1, 8), 0.09, 0.46, 0.55, 0.06, 0.26, 0.05, 0.15, 0, 0.4),
    ball(0, 0.5, -0.82, 0.1, 0.1, 0.16),
    ball(0, 0.66, -1.0, 0.14, 0.13, 0.14, 0.45),
    ball(0, 0.82, -1.12, 0.12, 0.12, 0.12, 0.35),
    ball(0, 0.92, -1.22, 0.08, 0.08, 0.09),
    ball(-0.18, 0.34, 0.28, 0.08, 0.2, 0.09, -0.95),
    ball(-0.18, 0.08, 0.52, 0.06, 0.18, 0.055, -0.3),
    ball(-0.18, -0.1, 0.64, 0.07, 0.04, 0.09),
    ball(0.18, 0.32, 0.02, 0.08, 0.16, 0.08, 0.85),
    ball(0.18, 0.08, -0.08, 0.055, 0.15, 0.05, 0.15),
    ball(0.18, -0.08, -0.06, 0.065, 0.04, 0.08),
    ball(-0.17, 0.28, -0.62, 0.085, 0.2, 0.09, 0.8),
    ball(-0.17, 0.02, -0.82, 0.06, 0.18, 0.055, 0.2),
    ball(-0.17, -0.14, -0.92, 0.07, 0.04, 0.09),
    ball(0.17, 0.32, -0.32, 0.085, 0.18, 0.09, -0.65),
    ball(0.17, 0.06, -0.12, 0.06, 0.17, 0.055, -0.15),
    ball(0.17, -0.12, -0.02, 0.07, 0.04, 0.09),
  ];
  const merged = mergeGeometries(parts, false);
  for (const part of parts) part.dispose();
  if (!merged) return new THREE.SphereGeometry(0.4, 10, 8);
  merged.computeBoundingBox();
  const minY = merged.boundingBox?.min.y ?? 0;
  wolfDrop = -0.5 - minY;
  merged.translate(0, wolfDrop, 0);
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
const prevPos = new THREE.Vector3();
const sample = new THREE.Vector3();
const forward = new THREE.Vector3();
const local = new THREE.Vector3();
const emitAt = new THREE.Vector3();

const MIST_ANCHORS: Array<[number, number, number]> = [
  [0, 0.55, 0.15],
  [0, 0.35, -0.45],
  [0, 0.25, 0.7],
  [0, 0.7, -1.05],
  [0.2, 0.4, -0.1],
  [-0.2, 0.4, -0.1],
];

function WolfTrail({ anchor }: { anchor: RefObject<THREE.Group | null> }) {
  const scene = useThree((state) => state.scene);
  const wisps = useMemo(() => createSpritePool(WISP_CAP), []);
  const embers = useMemo(() => createSpritePool(COLD_EMBER_CAP), []);
  const acc = useRef({ ember: 0, n: 1, elapsed: 0, anchor: 0, ready: false });

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
      if (!state.ready) {
        prevPos.copy(worldPos);
        state.ready = true;
      }
      const jump = prevPos.distanceTo(worldPos);
      const stamps = Math.min(5, Math.max(1, Math.ceil(jump / 0.42)));
      for (let s = 1; s <= stamps; s++) {
        sample.lerpVectors(prevPos, worldPos, s / stamps);
        const slot = MIST_ANCHORS[state.anchor % MIST_ANCHORS.length];
        state.anchor += 1;
        local.set(slot[0], slot[1] + wolfDrop, slot[2]);
        local.applyQuaternion(group.quaternion).multiplyScalar(group.scale.x);
        emitAt.copy(sample).add(local);
        emitMist(wisps, emitAt.x, emitAt.y, emitAt.z, backX, backZ, nextRand(state));
      }
      prevPos.copy(worldPos);
      state.ember += step;
      if (state.ember >= 0.09) {
        state.ember = 0;
        emitColdEmber(
          embers,
          worldPos.x + backX * 0.55,
          worldPos.y + 0.2,
          worldPos.z + backZ * 0.55,
          backX,
          backZ,
          nextRand(state),
        );
      }
    }
    stepMist(wisps, state.elapsed, step);
    stepColdEmbers(embers, state.elapsed, step);
  });

  return createPortal(
    <>
      <InstancedSprites pool={wisps} mode="mist" />
      <InstancedSprites pool={embers} mode="additive" />
    </>,
    scene,
  );
}

/**
 * Spectral wolf. Gameplay still drives position and velocity; this is only
 * the mist silhouette, the cold trail, and the spawn / vanish puff.
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
    const pulse = 0.78 + 0.22 * Math.sin(clock.elapsedTime * 3.4 + position[0]);
    material.uniforms.uTime.value += Math.min(dt, 1 / 30);
    material.uniforms.uFade.value = pulse;
  });

  return (
    <>
      <group ref={group} position={position} scale={1.22}>
        <mesh geometry={geometry} material={material} renderOrder={4} />
      </group>
      <WolfTrail anchor={group} />
    </>
  );
}
