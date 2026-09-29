import * as THREE from "three";
import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { visualPosition } from "@/lib/renderInterp";
import { Glint } from "@/vfx/powerLooks";

function FloatingStar({ position, color = "#FFD700" }: { position: [number, number, number]; color?: string }) {
  const starRef = useRef<THREE.Group>(null);
  const floatOffset = useRef(Math.random() * Math.PI * 2);
  
  const starShape = useMemo(() => {
    const shape = new THREE.Shape();
    const outerRadius = 0.3;
    const innerRadius = 0.12;
    const points = 5;
    
    for (let i = 0; i < points * 2; i++) {
      const radius = i % 2 === 0 ? outerRadius : innerRadius;
      const angle = (i * Math.PI) / points - Math.PI / 2;
      const x = Math.cos(angle) * radius;
      const y = Math.sin(angle) * radius;
      
      if (i === 0) {
        shape.moveTo(x, y);
      } else {
        shape.lineTo(x, y);
      }
    }
    shape.closePath();
    return shape;
  }, []);
  
  useFrame((state, delta) => {
    if (!starRef.current) return;
    
    floatOffset.current += delta * 2;
    const floatY = Math.sin(floatOffset.current) * 0.15;
    
    starRef.current.position.set(
      position[0],
      position[1] + 1.0 + floatY,
      position[2]
    );
    
    starRef.current.rotation.y += delta * 2;
    starRef.current.rotation.z = Math.sin(floatOffset.current * 0.5) * 0.2;
  });
  
  return (
    <group ref={starRef}>
      <mesh rotation={[0, 0, 0]}>
        <shapeGeometry args={[starShape]} />
        <meshBasicMaterial color={color} side={THREE.DoubleSide} />
      </mesh>
      <mesh rotation={[0, Math.PI, 0]}>
        <shapeGeometry args={[starShape]} />
        <meshBasicMaterial color={color} side={THREE.DoubleSide} />
      </mesh>
      <pointLight
        color={color}
        intensity={0.8}
        distance={3}
      />
    </group>
  );
}

function getStarColor(starOrbType: "wolfgang" | "hotstreak" | "bolt" | null | undefined): string {
  switch (starOrbType) {
    case "wolfgang": return "#FFD700";
    case "hotstreak": return "#FF4444";
    case "bolt": return "#4488FF";
    default: return "#FFD700";
  }
}

let globalOrbScale = 0.4;

export function getGlobalOrbScale(): number {
  return globalOrbScale;
}

export function setGlobalOrbScale(value: number) {
  globalOrbScale = Math.max(0.1, Math.min(1.0, value));
  useZoogiGame.getState().incrementMoveCounter();
}

interface OrbProps {
  orbId: string;
}

export function Orb({ orbId }: OrbProps) {
  const groupRef = useRef<THREE.Group>(null);
  const coinRef = useRef<THREE.Group>(null);
  const bobPhase = useRef(Math.random() * Math.PI * 2);
  
  const { orbs, lockOnEnabled, lockOnTargetId, setLockOnTarget } = useZoogiGame();
  const orb = orbs.find(o => o.id === orbId);
  
  const isLockedOn = lockOnTargetId === orbId;

  useFrame((state, delta) => {
    if (!groupRef.current || !orb || !orb.isActive) return;
    
    const vis = visualPosition(orb.id, orb.position);
    groupRef.current.position.set(vis[0], vis[1], vis[2]);
    
    if (coinRef.current) {
      bobPhase.current += delta * 2.0;
      coinRef.current.position.y = Math.sin(bobPhase.current) * 0.2;
      coinRef.current.rotation.y += delta * 3.0;
    }
  });
  
  const handleClick = (e: any) => {
    if (e.stopPropagation) e.stopPropagation();
    if (lockOnEnabled && orb) {
      setLockOnTarget(orbId, "orb");
      console.log("Locked onto orb:", orbId);
    }
  };

  if (!orb || !orb.isActive) return null;

  const scale = getGlobalOrbScale();

  return (
    <group>
      <group ref={groupRef} position={orb.position}>
        <group ref={coinRef}>

          <mesh castShadow scale={[scale, scale, scale]}>
            <cylinderGeometry args={[0.7, 0.7, 0.12, 32]} />
            <meshStandardMaterial
              color="#FFD700"
              emissive="#FFA500"
              emissiveIntensity={0.4}
              metalness={0.7}
              roughness={0.2}
            />
          </mesh>
          <mesh scale={[scale * 1.3, scale * 1.3, scale * 1.3]}>
            <sphereGeometry args={[0.8, 16, 16]} />
            <meshBasicMaterial
              color="#FFD700"
              transparent
              opacity={0.1}
            />
          </mesh>
          <mesh onClick={handleClick} onPointerDown={handleClick} scale={[scale * 1.2, scale * 1.2, scale * 1.2]}>
            <sphereGeometry args={[1, 16, 16]} />
            <meshBasicMaterial transparent opacity={0} />
          </mesh>
        </group>
        {isLockedOn && (
          <Glint position={[0, 0.15, 0]} size={0.62} color="#d5e8ff" />
        )}
      </group>
      
      <pointLight
        position={[orb.position[0], orb.position[1] + 0.5, orb.position[2]]}
        color="#FFD700"
        intensity={0.4}
        distance={3}
      />
      
      {orb.isStarOrb && (
        <FloatingStar position={orb.position} color={getStarColor(orb.starOrbType)} />
      )}
    </group>
  );
}

export function OrbManager() {
  const { orbs } = useZoogiGame();
  
  return (
    <>
      {orbs.filter(orb => orb.isActive).map(orb => (
        <Orb key={orb.id} orbId={orb.id} />
      ))}
    </>
  );
}
