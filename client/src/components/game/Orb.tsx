import * as THREE from "three";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { visualPosition } from "@/lib/renderInterp";
import { ORB_BODY_COLOR, ORB_DRAW_RADIUS, ORB_GLOW_COLOR } from "@/lib/restHeight";
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
  const bullseyeRef = useRef<THREE.Group>(null);
  const bullseyeRotationRef = useRef(0);
  const visRef = useRef<[number, number, number]>([0, 0, 0]);

  const appearance = useZoogiGame((state) => {
    const orb = state.orbs.find((entry) => entry.id === orbId);
    if (!orb?.isActive) return "";
    return orb.isStarOrb ? (orb.starOrbType ?? "star") : "orb";
  });
  const lockOnEnabled = useZoogiGame((state) => state.lockOnEnabled);
  const isLockedOn = useZoogiGame((state) => state.lockOnTargetId === orbId);
  const setLockOnTarget = useZoogiGame((state) => state.setLockOnTarget);

  useFrame((_, delta) => {
    const group = groupRef.current;
    if (!group) return;
    const orb = useZoogiGame.getState().orbs.find((entry) => entry.id === orbId);
    if (!orb || !orb.isActive) {
      group.visible = false;
      return;
    }
    group.visible = true;
    const vis = visualPosition(orb.id, orb.position, visRef.current);
    group.position.set(vis[0], vis[1], vis[2]);
    if (bullseyeRef.current) {
      bullseyeRotationRef.current += delta * 1.5;
      bullseyeRef.current.rotation.z = bullseyeRotationRef.current;
      bullseyeRef.current.position.set(vis[0], 0.04, vis[2]);
    }
  });

  const handleClick = (e: { stopPropagation?: () => void }) => {
    if (e.stopPropagation) e.stopPropagation();
    if (lockOnEnabled && appearance) {
      setLockOnTarget(orbId, "orb");
      console.log("Locked onto orb:", orbId);
    }
  };

  if (!appearance) return null;

  const radius = ORB_DRAW_RADIUS * getGlobalOrbScale();
  const starColor = getStarColor(appearance === "orb" ? null : appearance as "wolfgang" | "hotstreak" | "bolt");

  return (
    <group>
      <group ref={groupRef}>
        <mesh onClick={handleClick} onPointerDown={handleClick}>
          <sphereGeometry args={[radius, 40, 32]} />
          <meshPhysicalMaterial
            color={ORB_BODY_COLOR}
            emissive={ORB_GLOW_COLOR}
            emissiveIntensity={0.95}
            roughness={0.04}
            metalness={0.18}
            clearcoat={1}
            clearcoatRoughness={0.03}
          />
        </mesh>
        <mesh position={[radius * 0.22, radius * 0.48, radius * 0.28]} scale={[0.28, 0.16, 0.22]}>
          <sphereGeometry args={[radius, 12, 10]} />
          <meshBasicMaterial color="#f4fbff" />
        </mesh>
        <mesh>
          <sphereGeometry args={[radius * 1.1, 20, 14]} />
          <meshBasicMaterial color="#7ec8ff" transparent opacity={0.16} depthWrite={false} />
        </mesh>
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -radius + 0.025, 0]}>
          <circleGeometry args={[radius * 0.62, 24]} />
          <meshBasicMaterial color="#021433" transparent opacity={0.4} depthWrite={false} />
        </mesh>
        {appearance !== "orb" && (
          <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -radius + 0.04, 0]}>
            <ringGeometry args={[radius * 1.25, radius * 1.55, 40]} />
            <meshBasicMaterial color={starColor} transparent opacity={0.85} side={THREE.DoubleSide} />
          </mesh>
        )}
        {isLockedOn && (
          <Glint position={[0, radius * 0.85, 0]} size={0.62} color="#d5e8ff" />
        )}
      </group>

      {isLockedOn && (
        <group ref={bullseyeRef} rotation={[-Math.PI / 2, 0, 0]}>
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
  const activeIds = useZoogiGame((state) => state.orbs.filter((orb) => orb.isActive).map((orb) => orb.id).join("|"));
  const ids = activeIds ? activeIds.split("|") : [];

  return (
    <>
      {ids.map((id) => (
        <Orb key={id} orbId={id} />
      ))}
    </>
  );
}
