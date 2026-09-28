import { useRef } from "react";
import * as THREE from "three";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame, EditorScoringZone } from "@/lib/stores/useZoogiGame";

function ScoringZone({ zone, isSelected }: { zone: EditorScoringZone; isSelected: boolean }) {
  const ringRef = useRef<THREE.Mesh>(null);
  const glowRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (glowRef.current) {
      const pulse = Math.sin(state.clock.elapsedTime * 2) * 0.3 + 0.7;
      (glowRef.current.material as THREE.MeshBasicMaterial).opacity = isSelected ? pulse * 0.5 : pulse * 0.3;
    }
  });
  
  return (
    <group position={zone.position} rotation={[0, zone.rotation, 0]}>
      <mesh ref={glowRef} rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[zone.radius, 32]} />
        <meshBasicMaterial
          color={isSelected ? "#00ffff" : "#00aaff"}
          transparent
          opacity={0.3}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh ref={ringRef} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
        <ringGeometry args={[zone.radius - 0.2, zone.radius, 32]} />
        <meshBasicMaterial
          color={isSelected ? "#ffffff" : "#00ffff"}
          transparent
          opacity={0.8}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      {isSelected && (
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.05, 0]}>
          <ringGeometry args={[zone.radius + 0.1, zone.radius + 0.3, 32]} />
          <meshBasicMaterial
            color="#ffffff"
            transparent
            opacity={0.6}
            side={THREE.DoubleSide}
          />
        </mesh>
      )}
    </group>
  );
}

export function EditorScoringZones() {
  const { wallOwnershipConfig, selectedEditorScoringZoneId } = useZoogiGame();
  
  if (!wallOwnershipConfig.enabled) return null;
  
  return (
    <group name="editor-scoring-zones">
      {wallOwnershipConfig.scoringZones.map((zone) => (
        <ScoringZone
          key={zone.id}
          zone={zone}
          isSelected={selectedEditorScoringZoneId === zone.id}
        />
      ))}
    </group>
  );
}
