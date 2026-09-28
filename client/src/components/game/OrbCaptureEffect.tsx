import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export function OrbCaptureEffects() {
  const orbCaptureEffects = useZoogiGame((state) => state.orbCaptureEffects);
  const cleanupOrbCaptureEffects = useZoogiGame((state) => state.cleanupOrbCaptureEffects);
  const lastCleanupRef = useRef(0);
  
  useFrame(() => {
    const now = Date.now();
    if (now - lastCleanupRef.current > 200) {
      lastCleanupRef.current = now;
      cleanupOrbCaptureEffects();
    }
  });

  return (
    <>
      {orbCaptureEffects.map((effect) => (
        <OrbCaptureVisual key={effect.id} effect={effect} />
      ))}
    </>
  );
}

interface OrbCaptureVisualProps {
  effect: {
    id: string;
    position: [number, number, number];
    color: string;
    createdAt: number;
    duration: number;
  };
}

function OrbCaptureVisual({ effect }: OrbCaptureVisualProps) {
  const groupRef = useRef<THREE.Group>(null);
  const beamRef = useRef<THREE.Mesh>(null);
  const ring1Ref = useRef<THREE.Mesh>(null);
  const ring2Ref = useRef<THREE.Mesh>(null);
  const ring3Ref = useRef<THREE.Mesh>(null);
  const coreRef = useRef<THREE.Mesh>(null);
  const particlesRef = useRef<THREE.Points>(null);
  
  const particlePositions = useMemo(() => {
    const count = 24;
    const positions = new Float32Array(count * 3);
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2;
      const radius = 0.8 + Math.random() * 0.4;
      positions[i * 3] = Math.cos(angle) * radius;
      positions[i * 3 + 1] = Math.random() * 0.5;
      positions[i * 3 + 2] = Math.sin(angle) * radius;
    }
    return positions;
  }, []);

  useFrame(() => {
    if (!groupRef.current) return;
    
    const elapsed = Date.now() - effect.createdAt;
    const progress = Math.min(elapsed / effect.duration, 1);
    
    const easeOutCubic = 1 - Math.pow(1 - progress, 3);
    const easeInOut = progress < 0.5 
      ? 4 * progress * progress * progress 
      : 1 - Math.pow(-2 * progress + 2, 3) / 2;
    
    if (beamRef.current) {
      const beamProgress = progress < 0.3 ? progress / 0.3 : progress > 0.7 ? 1 - (progress - 0.7) / 0.3 : 1;
      beamRef.current.scale.set(0.3, beamProgress * 8, 0.3);
      beamRef.current.position.y = beamProgress * 4;
      (beamRef.current.material as THREE.MeshBasicMaterial).opacity = beamProgress * 0.7;
    }
    
    if (ring1Ref.current) {
      const ringScale = 0.5 + easeOutCubic * 2;
      ring1Ref.current.scale.set(ringScale, ringScale, ringScale);
      ring1Ref.current.rotation.z = elapsed * 0.003;
      (ring1Ref.current.material as THREE.MeshBasicMaterial).opacity = (1 - progress) * 0.8;
    }
    
    if (ring2Ref.current) {
      const ringScale = 0.3 + easeOutCubic * 2.5;
      ring2Ref.current.scale.set(ringScale, ringScale, ringScale);
      ring2Ref.current.rotation.z = -elapsed * 0.004;
      ring2Ref.current.position.y = easeOutCubic * 0.5;
      (ring2Ref.current.material as THREE.MeshBasicMaterial).opacity = (1 - progress) * 0.6;
    }
    
    if (ring3Ref.current) {
      const ringScale = 0.4 + easeOutCubic * 3;
      ring3Ref.current.scale.set(ringScale, ringScale, ringScale);
      ring3Ref.current.rotation.z = elapsed * 0.005;
      ring3Ref.current.position.y = easeOutCubic * 1;
      (ring3Ref.current.material as THREE.MeshBasicMaterial).opacity = (1 - progress) * 0.4;
    }
    
    if (coreRef.current) {
      const corePhase = progress < 0.5 
        ? 0.5 + progress 
        : 1 - (progress - 0.5) * 2;
      coreRef.current.scale.setScalar(corePhase);
      (coreRef.current.material as THREE.MeshBasicMaterial).opacity = corePhase * 0.9;
    }
    
    if (particlesRef.current) {
      particlesRef.current.rotation.y = elapsed * 0.002;
      particlesRef.current.position.y = easeInOut * 2;
      (particlesRef.current.material as THREE.PointsMaterial).opacity = (1 - progress) * 0.8;
    }
  });

  return (
    <group ref={groupRef} position={effect.position}>
      <mesh ref={beamRef} position={[0, 0, 0]}>
        <cylinderGeometry args={[0.15, 0.25, 1, 8]} />
        <meshBasicMaterial 
          color={effect.color} 
          transparent 
          opacity={0.7}
        />
      </mesh>
      
      <mesh ref={ring1Ref} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.1, 0]}>
        <ringGeometry args={[0.6, 0.8, 32]} />
        <meshBasicMaterial 
          color={effect.color} 
          transparent 
          opacity={0.8}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh ref={ring2Ref} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.2, 0]}>
        <ringGeometry args={[0.3, 0.5, 6]} />
        <meshBasicMaterial 
          color="#FFFFFF" 
          transparent 
          opacity={0.6}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh ref={ring3Ref} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.3, 0]}>
        <ringGeometry args={[0.2, 0.35, 8]} />
        <meshBasicMaterial 
          color={effect.color} 
          transparent 
          opacity={0.4}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh ref={coreRef} position={[0, 0.3, 0]}>
        <sphereGeometry args={[0.25, 12, 12]} />
        <meshBasicMaterial 
          color="#FFFFFF" 
          transparent 
          opacity={0.9}
        />
      </mesh>
      
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={24}
            array={particlePositions}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.1}
          color={effect.color}
          transparent
          opacity={0.8}
          sizeAttenuation
        />
      </points>
      
      <pointLight
        color={effect.color}
        intensity={2}
        distance={5}
        decay={2}
      />
    </group>
  );
}
