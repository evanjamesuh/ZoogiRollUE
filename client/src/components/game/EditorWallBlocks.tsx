import { useRef } from "react";
import * as THREE from "three";
import { useZoogiGame, EditorWallBlock } from "@/lib/stores/useZoogiGame";

function WallBlock({ block, isSelected }: { block: EditorWallBlock; isSelected: boolean }) {
  const meshRef = useRef<THREE.Mesh>(null);
  const { dimensions, position, rotation } = block;
  
  return (
    <mesh
      ref={meshRef}
      position={position}
      rotation={[0, rotation, 0]}
    >
      <boxGeometry args={[dimensions.length, dimensions.height, dimensions.width]} />
      <meshStandardMaterial
        color={isSelected ? "#ff8800" : "#ff6600"}
        emissive={isSelected ? "#ff4400" : "#331100"}
        emissiveIntensity={isSelected ? 0.5 : 0.2}
        roughness={0.4}
        metalness={0.6}
      />
      {isSelected && (
        <lineSegments>
          <edgesGeometry args={[new THREE.BoxGeometry(dimensions.length, dimensions.height, dimensions.width)]} />
          <lineBasicMaterial color="#ffffff" linewidth={2} />
        </lineSegments>
      )}
    </mesh>
  );
}

export function EditorWallBlocks() {
  const { wallOwnershipConfig, selectedEditorWallBlockId } = useZoogiGame();
  
  if (!wallOwnershipConfig.enabled) return null;
  
  return (
    <group name="editor-wall-blocks">
      {wallOwnershipConfig.wallBlocks.map((block) => (
        <WallBlock
          key={block.id}
          block={block}
          isSelected={selectedEditorWallBlockId === block.id}
        />
      ))}
    </group>
  );
}
