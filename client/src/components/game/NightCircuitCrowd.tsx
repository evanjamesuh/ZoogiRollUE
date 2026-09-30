import { useLayoutEffect, useMemo, useRef } from "react";
import * as THREE from "three";
import { NEON_HALF_X, NEON_HALF_Z } from "@/lib/neonCourt";
import { BOWL_DECKS, type BowlFace } from "./nightCircuitBowl";

type CrowdLight = { x: number; y: number; z: number; scale: number; phase: number; color: THREE.Color };

function crowdHash(n: number): number {
  const x = Math.sin(n * 127.1 + 311.7) * 43758.5453;
  return x - Math.floor(x);
}

/**
 * Scattered seat lights. Left is cyan, right is magenta, with a few warm
 * phones. Upper and farther seats are dimmer and thinner. The band behind
 * the top-center scoreboard stays dark.
 */
function crowdLights(): CrowdLight[] {
  const lights: CrowdLight[] = [];
  const cyan = new THREE.Color("#7ef6ff");
  const magenta = new THREE.Color("#ff6ad4");
  const phone = new THREE.Color("#ffd7a1");

  const push = (x: number, y: number, z: number, face: BowlFace, deckIndex: number) => {
    if (Math.abs(x) < NEON_HALF_X + 0.35 && Math.abs(z) < NEON_HALF_Z + 0.35) return;
    const scoreboard = NEON_HALF_X * 0.62;
    if (face === "north" && Math.abs(x) < scoreboard) return;
    if (face === "north" && y > 4.2 && Math.abs(x) < NEON_HALF_X * 0.9) return;

    const cell = crowdHash(Math.floor(x / 2.35) * 19.1 + Math.floor(y / 1.7) * 7.3 + Math.floor(z / 2.35) * 13.7);
    const onRim = face === "north" && deckIndex === 0 && Math.abs(x) < NEON_HALF_X * 2.2;
    if (!onRim && cell < 0.34) return;
    if (onRim && cell < 0.1) return;

    const salt = x * 3.1 + y * 5.7 + z * 8.3 + deckIndex * 2.2;
    const heightT = THREE.MathUtils.clamp((y - 0.2) / 10, 0, 1);
    const distT = THREE.MathUtils.clamp((Math.hypot(x, z) - 16) / 30, 0, 1);
    const density = onRim ? 0.86 : 0.58 - 0.28 * heightT - 0.18 * distT;
    if (crowdHash(salt + 21) > density) return;

    const jitter = crowdHash(salt + 2);
    const gain = onRim ? 0.46 : Math.max(0.08, 0.34 - 0.18 * heightT - 0.12 * distT);
    const tint = crowdHash(salt + 15) < 0.08 ? phone : x < 0 ? cyan : magenta;
    const color = tint.clone();
    color.multiplyScalar(gain);
    lights.push({
      x: x + (jitter - 0.5) * 0.42,
      y: y + (crowdHash(salt + 4) - 0.5) * 0.16,
      z: z + (crowdHash(salt + 6) - 0.5) * 0.28,
      scale: 0.72 + crowdHash(salt + 8) * 0.4,
      phase: crowdHash(salt + 11) * Math.PI * 2,
      color,
    });
  };

  const clear = 0.08;
  BOWL_DECKS.forEach((deck, deckIndex) => {
    const [cx, cy, cz] = deck.pos;
    const [sx, sy, sz] = deck.size;
    const near = deck.face === "north" && deckIndex === 0;
    const step = near ? 0.58 : deck.face === "north" ? 1.05 : 1.25;
    const top = cy + sy / 2 + clear;
    const x0 = cx - sx / 2 + 0.2;
    const x1 = cx + sx / 2 - 0.2;
    const z0 = cz - sz / 2 + 0.18;
    const z1 = cz + sz / 2 - 0.18;
    const y0 = cy - sy / 2 + 0.2;
    const y1 = cy + sy / 2 - 0.08;
    for (let z = Math.min(z0, z1); z <= Math.max(z0, z1) + 0.001; z += step) {
      for (let x = x0; x <= x1 + 0.001; x += step) {
        push(x, top, z, deck.face, deckIndex);
      }
    }
    if (deck.face === "north" || deck.face === "south") {
      const faceZ = deck.face === "north" ? cz + sz / 2 + clear : cz - sz / 2 - clear;
      for (let y = y0; y <= y1 + 0.001; y += step) {
        for (let x = x0; x <= x1 + 0.001; x += step) {
          push(x, y, faceZ, deck.face, deckIndex);
        }
      }
    } else {
      const faceX = deck.face === "west" ? cx + sx / 2 + clear : cx - sx / 2 - clear;
      for (let z = Math.min(z0, z1); z <= Math.max(z0, z1) + 0.001; z += step) {
        for (let y = y0; y <= y1 + 0.001; y += step) {
          push(faceX, y, z, deck.face, deckIndex);
        }
      }
    }
  });

  return lights;
}

/** Instanced seat lights for Night Circuit. One draw, soft pulse, no gameplay effect. */
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
      <sphereGeometry args={[0.056, 6, 5]} />
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
            float pulse = 0.5 + 0.5 * sin(uTime * 0.85 + vPhase);
            float twinkle = 0.88 + 0.12 * pulse;
            gl_FragColor = vec4(vColor * twinkle * 0.7, 1.0);
          }
        `}
      />
    </instancedMesh>
  );
}
