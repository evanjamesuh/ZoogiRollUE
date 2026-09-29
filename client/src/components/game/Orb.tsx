import * as THREE from "three";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { visualPosition } from "@/lib/renderInterp";
import { ORB_BODY_COLOR, ORB_DRAW_RADIUS, ORB_GLOW_COLOR, ORB_REST_Y } from "@/lib/restHeight";
import { Glint } from "@/vfx/powerLooks";

function getStarColor(starOrbType: "wolfgang" | "hotstreak" | "bolt" | null | undefined): string {
  switch (starOrbType) {
    case "wolfgang": return "#FFD700";
    case "hotstreak": return "#FF4444";
    case "bolt": return "#4488FF";
    default: return "#FFD700";
  }
}

let globalOrbScale = 1;

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
  const bullseyeRotationRef = useRef(0);

  const { orbs, lockOnEnabled, lockOnTargetId, setLockOnTarget } = useZoogiGame();
  const orb = orbs.find(o => o.id === orbId);

  const isLockedOn = lockOnTargetId === orbId;

  useFrame((_, delta) => {
    if (!groupRef.current || !orb || !orb.isActive) return;

    const vis = visualPosition(orb.id, orb.position);
    groupRef.current.position.set(vis[0], ORB_REST_Y, vis[2]);
    bullseyeRotationRef.current += delta * 1.5;
  });

  const handleClick = (e: { stopPropagation?: () => void }) => {
    if (e.stopPropagation) e.stopPropagation();
    if (lockOnEnabled && orb) {
      setLockOnTarget(orbId, "orb");
      console.log("Locked onto orb:", orbId);
    }
  };

  if (!orb || !orb.isActive) return null;

  const radius = ORB_DRAW_RADIUS * getGlobalOrbScale();
  const starColor = getStarColor(orb.starOrbType);

  return (
    <group>
      <group ref={groupRef} position={[orb.position[0], ORB_REST_Y, orb.position[2]]}>
        <mesh
          castShadow
          receiveShadow
          onClick={handleClick}
          onPointerDown={handleClick}
        >
          <sphereGeometry args={[radius, 32, 24]} />
          <meshPhysicalMaterial
            color={ORB_BODY_COLOR}
            emissive={ORB_GLOW_COLOR}
            emissiveIntensity={0.85}
            roughness={0.06}
            metalness={0.2}
            clearcoat={1}
            clearcoatRoughness={0.04}
          />
        </mesh>
        <mesh>
          <sphereGeometry args={[radius * 1.08, 24, 16]} />
          <meshBasicMaterial color="#7ec8ff" transparent opacity={0.18} depthWrite={false} />
        </mesh>
        {orb.isStarOrb && (
          <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -radius + 0.03, 0]}>
            <ringGeometry args={[radius * 1.2, radius * 1.7, 40]} />
            <meshBasicMaterial color={starColor} transparent opacity={0.9} side={THREE.DoubleSide} />
          </mesh>
        )}
        <pointLight color="#6eb6ff" intensity={0.7} distance={4.5} position={[0, radius * 0.4, 0]} />
        {isLockedOn && (
          <Glint position={[0, radius * 0.85, 0]} size={0.62} color="#d5e8ff" />
        )}
      </group>

      {isLockedOn && (
        <group position={[orb.position[0], 0.04, orb.position[2]]} rotation={[-Math.PI / 2, 0, bullseyeRotationRef.current]}>
          <mesh>
            <ringGeometry args={[radius * 1.3, radius * 1.6, 32]} />
            <meshBasicMaterial color="#8B5CF6" transparent opacity={0.8} />
          </mesh>
          <mesh>
            <ringGeometry args={[radius * 0.65, radius * 0.9, 32]} />
            <meshBasicMaterial color="#8B5CF6" transparent opacity={0.6} />
          </mesh>
          <mesh>
            <circleGeometry args={[radius * 0.24, 16]} />
            <meshBasicMaterial color="#8B5CF6" transparent opacity={0.9} />
          </mesh>
        </group>
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
