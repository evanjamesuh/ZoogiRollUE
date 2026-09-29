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

export type Vec3 = [number, number, number];

let sharedGeometry: THREE.BufferGeometry | null = null;
let geometryUsers = 0;
let wolfDrop = 0;
let wolfFit = 1;

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

function tag(geometry: THREE.BufferGeometry, limb: number, pivot: THREE.Vector3): THREE.BufferGeometry {
  const count = geometry.getAttribute("position").count;
  const limbs = new Float32Array(count);
  const pivots = new Float32Array(count * 3);
  limbs.fill(limb);
  for (let i = 0; i < count; i++) {
    pivots[i * 3] = pivot.x;
    pivots[i * 3 + 1] = pivot.y;
    pivots[i * 3 + 2] = pivot.z;
  }
  geometry.setAttribute("aLimb", new THREE.BufferAttribute(limbs, 1));
  geometry.setAttribute("aPivot", new THREE.BufferAttribute(pivots, 3));
  return geometry;
}

function capsule(radius: number, length: number, x: number, y: number, z: number, rx = 0, ry = 0, rz = 0): THREE.BufferGeometry {
  return place(new THREE.CapsuleGeometry(radius, length, 3, 8), x, y, z, 1, 1, 1, rx, ry, rz);
}

function buildWolfGeometry(): THREE.BufferGeometry {
  const bodyPivot = new THREE.Vector3(0, 0.35, 0);
  const rump = new THREE.Vector3(0, 0.4, -0.62);
  const parts: THREE.BufferGeometry[] = [
    tag(capsule(0.28, 0.72, 0, 0.5, -0.08, Math.PI / 2, 0, 0), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.36, 14, 12), 0, 0.58, 0.28, 1.18, 1.02, 1.22), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.24, 12, 10), 0, 0.66, 0.62, 1.12, 0.96, 1.28), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.22, 14, 12), 0, 0.72, 1.02, 0.74, 0.66, 1.95), 0, bodyPivot),
    tag(place(new THREE.ConeGeometry(0.045, 0.38, 5), -0.12, 1.05, 0.78, 1, 1, 1, 0.15, 0, -0.55), 0, bodyPivot),
    tag(place(new THREE.ConeGeometry(0.045, 0.38, 5), 0.12, 1.05, 0.78, 1, 1, 1, 0.15, 0, 0.55), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.2, 12, 10), -0.16, 0.38, 0.22, 1.15, 0.85, 1.25), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.2, 12, 10), 0.16, 0.38, 0.02, 1.15, 0.85, 1.25), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.22, 12, 10), -0.14, 0.36, -0.32, 1.2, 0.9, 1.3), 0, bodyPivot),
    tag(place(new THREE.SphereGeometry(0.22, 12, 10), 0.14, 0.36, -0.16, 1.2, 0.9, 1.3), 0, bodyPivot),
    tag(capsule(0.14, 0.2, -0.18, 0.26, 0.24, -0.28, 0, 0), 1, new THREE.Vector3(-0.18, 0.4, 0.2)),
    tag(capsule(0.14, 0.18, 0.18, 0.26, 0.06, 0.25, 0, 0), 2, new THREE.Vector3(0.18, 0.4, 0.06)),
    tag(capsule(0.145, 0.22, -0.15, 0.24, -0.22, 0.32, 0, 0), 3, new THREE.Vector3(-0.15, 0.38, -0.16)),
    tag(capsule(0.145, 0.2, 0.15, 0.24, -0.04, -0.22, 0, 0), 4, new THREE.Vector3(0.15, 0.38, -0.06)),
    tag(capsule(0.07, 0.14, 0, 0.46, -0.72, 0.7, 0, 0), 5, rump),
    tag(capsule(0.04, 0.1, 0, 0.52, -1.02, 0.95, 0, 0), 5, rump),
  ];
  const merged = mergeGeometries(parts, false);
  for (const part of parts) part.dispose();
  if (!merged) return new THREE.SphereGeometry(0.4, 10, 8);
  merged.computeBoundingBox();
  const box = merged.boundingBox;
  const height = box ? box.max.y - box.min.y : 1;
  const fit = 1.7 / Math.max(height, 0.001);
  wolfFit = fit;
  merged.scale(fit, fit, fit);
  const pivots = merged.getAttribute("aPivot");
  for (let i = 0; i < pivots.count; i++) {
    pivots.setXYZ(i, pivots.getX(i) * fit, pivots.getY(i) * fit, pivots.getZ(i) * fit);
  }
  merged.computeBoundingBox();
  const minY = merged.boundingBox?.min.y ?? 0;
  wolfDrop = -0.5 - minY;
  merged.translate(0, wolfDrop, 0);
  for (let i = 0; i < pivots.count; i++) pivots.setY(i, pivots.getY(i) + wolfDrop);
  merged.computeVertexNormals();
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
  [0, 0.55, 0.25],
  [0, 0.42, -0.35],
  [0, 0.36, 0.75],
  [0, 0.7, -0.95],
  [0.22, 0.48, 0.05],
  [-0.22, 0.48, 0.05],
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
      const stamps = jump < 0.2 ? 0 : Math.min(2, Math.ceil(jump / 0.9));
      for (let s = 1; s <= stamps; s++) {
        sample.lerpVectors(prevPos, worldPos, s / stamps);
        const slot = MIST_ANCHORS[state.anchor % MIST_ANCHORS.length];
        state.anchor += 1;
        local.set(slot[0] * wolfFit, slot[1] * wolfFit + wolfDrop, slot[2] * wolfFit);
        local.applyQuaternion(group.quaternion).multiplyScalar(group.scale.x);
        emitAt.copy(sample).add(local);
        emitMist(wisps, emitAt.x, emitAt.y, emitAt.z, backX, backZ, nextRand(state));
      }
      prevPos.copy(worldPos);
      state.ember += step;
      if (state.ember >= 0.22) {
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
      <InstancedSprites pool={embers} mode="ember" />
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
  const geometry = useMemo(() => retainWolfGeometry(), []);
  const lastWorld = useRef(new THREE.Vector3(position[0], position[1], position[2]));
  const sawWorld = useRef(false);

  const material = useMemo(() => new THREE.ShaderMaterial({
    uniforms: {
      uTime: { value: 0 },
      uFade: { value: 1 },
    },
    vertexShader: WOLF_VERT,
    fragmentShader: WOLF_FRAG,
    transparent: true,
    depthWrite: false,
    depthTest: true,
    toneMapped: false,
    blending: THREE.NormalBlending,
    side: THREE.FrontSide,
    wireframe: false,
    alphaTest: 0,
  }), []);

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
    const pulse = 0.94 + 0.06 * Math.sin(clock.elapsedTime * 3.4 + position[0]);
    material.uniforms.uTime.value += Math.min(dt, 1 / 30);
    material.uniforms.uFade.value = pulse;
  });

  return (
    <>
      <group ref={group} position={position}>
        <mesh geometry={geometry} material={material} renderOrder={4} />
      </group>
      <WolfTrail anchor={group} />
    </>
  );
}
