import { useRef, useEffect } from "react";
import * as THREE from "three";
import { useThree } from "@react-three/fiber";
import { TransformControls } from "@react-three/drei";
import { useZoogiGame, TransformElementType, TransformMode } from "@/lib/stores/useZoogiGame";

export function TransformGizmo() {
  const transformRef = useRef<any>(null);
  const targetRef = useRef<THREE.Object3D>(new THREE.Object3D());
  const { camera, gl } = useThree();
  
  const selectedTransformElement = useZoogiGame((state) => state.selectedTransformElement);
  const transformMode = useZoogiGame((state) => state.transformMode);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  const showTransformGizmo = useZoogiGame((state) => state.showTransformGizmo);
  const updateElementTransform = useZoogiGame((state) => state.updateElementTransform);
  const updateElementRotation = useZoogiGame((state) => state.updateElementRotation);
  const gameMode = useZoogiGame((state) => state.gameMode);
  
  const getOffset = (element: TransformElementType | null) => {
    switch (element) {
      case "arena_model": return elementTransforms.arenaModelOffset;
      case "outer_wall": return elementTransforms.outerWallOffset;
      case "middle_wall": return elementTransforms.middleWallOffset;
      case "inner_wall": return elementTransforms.innerWallOffset;
      case "zones": return elementTransforms.zonesOffset;
      case "knockoff_boundary": return elementTransforms.knockoffBoundaryOffset;
      default: return { x: 0, y: 0, z: 0 };
    }
  };
  
  const getRotation = (element: TransformElementType | null) => {
    switch (element) {
      case "arena_model": return elementTransforms.arenaModelRotation;
      case "outer_wall": return elementTransforms.outerWallRotation;
      case "middle_wall": return elementTransforms.middleWallRotation;
      case "inner_wall": return elementTransforms.innerWallRotation;
      case "zones": return elementTransforms.zonesRotation;
      case "knockoff_boundary": return elementTransforms.knockoffBoundaryRotation;
      default: return { x: 0, y: 0, z: 0 };
    }
  };
  
  useEffect(() => {
    if (selectedTransformElement && targetRef.current) {
      const offset = getOffset(selectedTransformElement);
      const rotation = getRotation(selectedTransformElement);
      targetRef.current.position.set(offset.x, offset.y, offset.z);
      targetRef.current.rotation.set(rotation.x, rotation.y, rotation.z);
    }
  }, [selectedTransformElement, elementTransforms]);
  
  const handleChange = () => {
    if (!selectedTransformElement || !targetRef.current) return;
    
    if (transformMode === "translate") {
      const pos = targetRef.current.position;
      updateElementTransform(selectedTransformElement, {
        x: Math.round(pos.x * 10) / 10,
        y: Math.round(pos.y * 10) / 10,
        z: Math.round(pos.z * 10) / 10,
      });
    } else if (transformMode === "rotate") {
      const rot = targetRef.current.rotation;
      updateElementRotation(selectedTransformElement, {
        x: Math.round(rot.x * 100) / 100,
        y: Math.round(rot.y * 100) / 100,
        z: Math.round(rot.z * 100) / 100,
      });
    }
  };
  
  if (gameMode !== "map_editor" || !showTransformGizmo || !selectedTransformElement) {
    return null;
  }
  
  const offset = getOffset(selectedTransformElement);
  
  return (
    <group>
      <primitive 
        object={targetRef.current} 
        position={[offset.x, offset.y, offset.z]}
      />
      <TransformControls
        ref={transformRef}
        object={targetRef.current}
        mode={transformMode}
        size={0.8}
        onObjectChange={handleChange}
      />
      <mesh position={[offset.x, offset.y + 0.5, offset.z]}>
        <boxGeometry args={[1, 1, 1]} />
        <meshStandardMaterial 
          color={getElementColor(selectedTransformElement)} 
          transparent 
          opacity={0.5}
          wireframe
        />
      </mesh>
    </group>
  );
}

function getElementColor(element: TransformElementType): string {
  switch (element) {
    case "arena_model": return "#3b82f6";
    case "outer_wall": return "#06b6d4";
    case "middle_wall": return "#a855f7";
    case "inner_wall": return "#ec4899";
    case "zones": return "#22c55e";
    case "knockoff_boundary": return "#f97316";
    default: return "#ffffff";
  }
}
