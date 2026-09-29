import { useLayoutEffect, useMemo, useRef } from "react";
import * as THREE from "three";

/**
 * Seat positions for the Night Circuit crowd. These match the stand meshes
 * and far-bowl tiers in NeonCourtArena. Lights sit just off those surfaces.
 */
const STANDS: Array<{ pos: [number, number, number]; size: [number, number, number] }> = [
  { pos: [0, 0.46, 9.55], size: [15.4, 0.92, 2.15] },
  { pos: [0, 1.02, 10.9], size: [16.2, 0.55, 1.15] },
  { pos: [13.2, 0.4, 0], size: [1.65, 0.8, 8.4] },
  { pos: [-13.2, 0.4, 0], size: [1.65, 0.8, 8.4] },
  { pos: [0, 1.55, -15.4], size: [24, 1.35, 2.6] },
  { pos: [0, 1.55, 15.4], size: [24, 1.35, 2.6] },
  { pos: [16.6, 1.15, 0], size: [2.2, 1.15, 16] },
  { pos: [-16.6, 1.15, 0], size: [2.2, 1.15, 16] },
];

const FAR_TIERS = [
  { z: -9.22, y: 0.58, h: 0.82, depth: 0.7, width: 16.6 },
  { z: -10.02, y: 1.16, h: 0.96, depth: 0.78, width: 17.8 },
  { z: -10.86, y: 1.74, h: 1.08, depth: 0.84, width: 18.8 },
];

type CrowdLight = { x: number; y: number; z: number; scale: number; phase: number; color: THREE.Color };

function crowdHash(n: number): number {
  const x = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return x - Math.floor(x);
}

/**
 * Points of light in the seats. They stay outside the floor and do not
 * light the stand surfaces. Left is cyan, right is magenta, with a few
 * warm phone lights mixed in.
 */
function crowdLights(): CrowdLight[] {
  const lights: CrowdLight[] = [];
  const cyan = new THREE.Color("#7ef6ff");
  const magenta = new THREE.Color("#ff6ad4");
  const phone = new THREE.Color("#ffd7a1");
  const push = (x: number, y: number, z: number) => {
    const salt = lights.length * 1.17 + x * 3.1 + y * 5.7 + z * 8.3;
    const jitter = crowdHash(salt + 2);
    lights.push({
      x: x + (jitter - 0.5) * 0.05,
      y: y + (crowdHash(salt + 4) - 0.5) * 0.03,
      z: z + (crowdHash(salt + 6) - 0.5) * 0.04,
      scale: 0.8 + crowdHash(salt + 8) * 0.28,
      phase: crowdHash(salt + 11) * Math.PI * 2,
      color: crowdHash(salt + 15) < 0.08 ? phone : x < 0 ? cyan : magenta,
    });
  };
  const gridXZ = (y: number, x0: number, x1: number, z0: number, z1: number, step: number) => {
    for (let z = Math.min(z0, z1); z <= Math.max(z0, z1) + 0.001; z += step) {
      for (let x = x0; x <= x1 + 0.001; x += step) {
        if (Math.abs(x) < 0.18) continue;
        push(x, y, z);
      }
    }
  };
  const gridXY = (z: number, x0: number, x1: number, y0: number, y1: number, step: number) => {
    for (let y = y0; y <= y1 + 0.001; y += step) {
      for (let x = x0; x <= x1 + 0.001; x += step) {
        if (Math.abs(x) < 0.18) continue;
        push(x, y, z);
      }
    }
  };
  const gridYZ = (x: number, y0: number, y1: number, z0: number, z1: number, step: number) => {
    for (let z = Math.min(z0, z1); z <= Math.max(z0, z1) + 0.001; z += step) {
      for (let y = y0; y <= y1 + 0.001; y += step) {
        push(x, y, z);
      }
    }
  };

  const farStep = 0.26;
  const standStep = 0.32;
  const clear = 0.08;
  for (const tier of FAR_TIERS) {
    const half = tier.width / 2 - 0.35;
    const top = tier.y + tier.h / 2 + clear;
    const front = tier.z + tier.depth / 2;
    const back = tier.z - tier.depth / 2;
    gridXZ(top, -half, half, back + 0.1, front - 0.06, farStep);
    gridXY(front + clear, -half, half, tier.y - tier.h / 2 + 0.12, tier.y + tier.h / 2 - 0.04, farStep);
  }

  for (const stand of STANDS) {
    const [cx, cy, cz] = stand.pos;
    const [sx, sy, sz] = stand.size;
    const top = cy + sy / 2 + clear;
    gridXZ(top, cx - sx / 2 + 0.12, cx + sx / 2 - 0.12, cz - sz / 2 + 0.1, cz + sz / 2 - 0.1, standStep);
    if (Math.abs(cx) >= Math.abs(cz)) {
      const faceX = cx - Math.sign(cx) * (sx / 2 + clear);
      if (Math.abs(faceX) < 12.25) continue;
      gridYZ(faceX, cy - sy / 2 + 0.1, cy + sy / 2 - 0.04, cz - sz / 2 + 0.12, cz + sz / 2 - 0.12, standStep);
    } else {
      const faceZ = cz - Math.sign(cz) * (sz / 2 + clear);
      if (Math.abs(faceZ) < 8.25) continue;
      gridXY(faceZ, cx - sx / 2 + 0.12, cx + sx / 2 - 0.12, cy - sy / 2 + 0.1, cy + sy / 2 - 0.04, standStep);
    }
  }

  return lights;
}

/** Instanced seat lights for Night Circuit. One draw, shader pulse, no gameplay effect. */
export function NightCircuitCrowd() {
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const uniforms = useMemo(() => ({ uTime: { value: 0 } }), []);
  const spots = useMemo(() => crowdLights(), []);

  useLayoutEffect(() => {
    const mesh = meshRef.current;
    if (!mesh) return;
    const dummy = new THREE.Object3D();
    const phases = new Float32Array(spots.length);
    spots.forEach((spot, i) => {
      dummy.position.set(spot.x, spot.y, spot.z);
      dummy.scale.setScalar(spot.scale);
      dummy.rotation.set(0, 0, 0);
      dummy.updateMatrix();
      mesh.setMatrixAt(i, dummy.matrix);
      mesh.setColorAt(i, spot.color);
      phases[i] = spot.phase;
    });
    mesh.instanceMatrix.needsUpdate = true;
    if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
    mesh.geometry.setAttribute("phase", new THREE.InstancedBufferAttribute(phases, 1));
    mesh.raycast = () => {};
    mesh.count = spots.length;
  }, [spots]);

  return (
    <instancedMesh
      ref={meshRef}
      args={[undefined, undefined, spots.length]}
      frustumCulled={false}
      renderOrder={2}
      onBeforeRender={(_renderer, _scene, _camera, _geometry, material) => {
        const mat = material as THREE.ShaderMaterial;
        if (mat?.uniforms?.uTime) mat.uniforms.uTime.value = performance.now() * 0.001;
      }}
    >
      <sphereGeometry args={[0.075, 6, 5]} />
      <shaderMaterial
        toneMapped={false}
        transparent
        depthWrite={false}
        blending={THREE.AdditiveBlending}
        uniforms={uniforms}
        vertexShader={`
          attribute float phase;
          varying vec3 vColor;
          varying float vPhase;
          void main() {
            vColor = instanceColor;
            vPhase = phase;
            vec4 mvPosition = modelViewMatrix * instanceMatrix * vec4(position, 1.0);
            gl_Position = projectionMatrix * mvPosition;
          }
        `}
        fragmentShader={`
          uniform float uTime;
          varying vec3 vColor;
          varying float vPhase;
          void main() {
            float pulse = 0.5 + 0.5 * sin(uTime * 1.35 + vPhase);
            float twinkle = 0.62 + 0.38 * pulse;
            gl_FragColor = vec4(vColor * twinkle * 1.85, 1.0);
          }
        `}
      />
    </instancedMesh>
  );
}
