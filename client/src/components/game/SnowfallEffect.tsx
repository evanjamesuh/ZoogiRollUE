import * as THREE from "three";
import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import { ARENA_RADIUS } from "@/lib/arenaConstants";

const PARTICLE_COUNT = 200;
const SPREAD = ARENA_RADIUS * 3;
const HEIGHT_MAX = 20;
const HEIGHT_MIN = -1;
const FALL_SPEED_MIN = 1.5;
const FALL_SPEED_MAX = 3.5;

export function SnowfallEffect() {
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const dummy = useMemo(() => new THREE.Object3D(), []);

  const particleData = useMemo(() => {
    return Array.from({ length: PARTICLE_COUNT }, () => ({
      x: (Math.random() - 0.5) * SPREAD,
      y: Math.random() * HEIGHT_MAX,
      z: (Math.random() - 0.5) * SPREAD,
      fallSpeed: FALL_SPEED_MIN + Math.random() * (FALL_SPEED_MAX - FALL_SPEED_MIN),
      driftSpeedX: (Math.random() - 0.5) * 0.4,
      driftSpeedZ: (Math.random() - 0.5) * 0.4,
      phase: Math.random() * Math.PI * 2,
      size: 0.03 + Math.random() * 0.04,
    }));
  }, []);

  useFrame((state, delta) => {
    if (!meshRef.current) return;
    const t = state.clock.elapsedTime;

    for (let i = 0; i < PARTICLE_COUNT; i++) {
      const p = particleData[i];

      p.y -= p.fallSpeed * delta;

      if (p.y < HEIGHT_MIN) {
        p.y = HEIGHT_MAX;
        p.x = (Math.random() - 0.5) * SPREAD;
        p.z = (Math.random() - 0.5) * SPREAD;
      }

      const lateralX = Math.sin(t * p.driftSpeedX + p.phase) * 0.8;
      const lateralZ = Math.cos(t * p.driftSpeedZ * 0.7 + p.phase) * 0.8;

      dummy.position.set(p.x + lateralX, p.y, p.z + lateralZ);
      dummy.scale.setScalar(p.size);
      dummy.updateMatrix();
      meshRef.current!.setMatrixAt(i, dummy.matrix);
    }

    meshRef.current.instanceMatrix.needsUpdate = true;
  });

  return (
    <instancedMesh ref={meshRef} args={[undefined, undefined, PARTICLE_COUNT]}>
      <planeGeometry args={[1, 1]} />
      <meshBasicMaterial
        color="#E8F0FF"
        transparent
        opacity={0.7}
        side={THREE.DoubleSide}
        depthWrite={false}
      />
    </instancedMesh>
  );
}
