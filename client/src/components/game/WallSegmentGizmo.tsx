import { useRef, useEffect } from "react";
import * as THREE from "three";
import { useThree } from "@react-three/fiber";
import { TransformControls } from "@react-three/drei";
import { useZoogiGame, WallSegmentConfig } from "@/lib/stores/useZoogiGame";

export function WallSegmentGizmo() {
  const transformRef = useRef<any>(null);
  const targetRef = useRef<THREE.Object3D>(new THREE.Object3D());
  const { camera, gl } = useThree();
  
  const selectedWallSegmentId = useZoogiGame((state) => state.selectedWallSegmentId);
  const wallSegmentConfigs = useZoogiGame((state) => state.wallSegmentConfigs);
  const updateWallSegmentConfig = useZoogiGame((state) => state.updateWallSegmentConfig);
  const gameMode = useZoogiGame((state) => state.gameMode);
  
  const selectedConfig = selectedWallSegmentId 
    ? wallSegmentConfigs.find(c => c.id === selectedWallSegmentId) 
    : null;
  
  useEffect(() => {
    if (selectedConfig && targetRef.current) {
      const offset = selectedConfig.positionOffset;
      targetRef.current.position.set(offset.x, offset.y, offset.z);
    }
  }, [selectedConfig]);
  
  const handleChange = () => {
    if (!selectedWallSegmentId || !targetRef.current) return;
    
    const pos = targetRef.current.position;
    updateWallSegmentConfig(selectedWallSegmentId, {
      positionOffset: {
        x: Math.round(pos.x * 10) / 10,
        y: Math.round(pos.y * 10) / 10,
        z: Math.round(pos.z * 10) / 10,
      }
    });
  };
  
  if (gameMode !== "map_editor" || !selectedWallSegmentId || !selectedConfig) {
    return null;
  }
  
  const offset = selectedConfig.positionOffset;
  
  return (
    <group>
      <primitive 
        object={targetRef.current} 
        position={[offset.x, offset.y, offset.z]}
      />
      <TransformControls
        ref={transformRef}
        object={targetRef.current}
        mode="translate"
        size={0.6}
        onObjectChange={handleChange}
      />
    </group>
  );
}
