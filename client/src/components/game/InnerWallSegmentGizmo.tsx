import { useRef, useEffect } from "react";
import * as THREE from "three";
import { useThree } from "@react-three/fiber";
import { TransformControls } from "@react-three/drei";
import { useZoogiGame, WallSegmentConfig } from "@/lib/stores/useZoogiGame";

export function InnerWallSegmentGizmo() {
  const transformRef = useRef<any>(null);
  const targetRef = useRef<THREE.Object3D>(new THREE.Object3D());
  const { camera, gl } = useThree();
  
  const selectedInnerWallSegmentId = useZoogiGame((state) => state.selectedInnerWallSegmentId);
  const innerWallSegmentConfigs = useZoogiGame((state) => state.innerWallSegmentConfigs);
  const updateInnerWallSegmentConfig = useZoogiGame((state) => state.updateInnerWallSegmentConfig);
  const gameMode = useZoogiGame((state) => state.gameMode);
  
  const selectedConfig = selectedInnerWallSegmentId 
    ? innerWallSegmentConfigs.find(c => c.id === selectedInnerWallSegmentId) 
    : null;
  
  useEffect(() => {
    if (selectedConfig && targetRef.current) {
      const offset = selectedConfig.positionOffset;
      targetRef.current.position.set(offset.x, offset.y, offset.z);
    }
  }, [selectedConfig]);
  
  const handleChange = () => {
    if (!selectedInnerWallSegmentId || !targetRef.current) return;
    
    const pos = targetRef.current.position;
    updateInnerWallSegmentConfig(selectedInnerWallSegmentId, {
      positionOffset: {
        x: Math.round(pos.x * 10) / 10,
        y: Math.round(pos.y * 10) / 10,
        z: Math.round(pos.z * 10) / 10,
      }
    });
  };
  
  if (gameMode !== "map_editor" || !selectedInnerWallSegmentId || !selectedConfig) {
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
