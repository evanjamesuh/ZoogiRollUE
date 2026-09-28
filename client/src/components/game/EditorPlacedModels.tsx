import { useRef, Suspense, useMemo, useState, useEffect, useCallback } from "react";
import { useFrame, ThreeEvent } from "@react-three/fiber";
import { useGLTF, TransformControls } from "@react-three/drei";
import * as THREE from "three";
import { useZoogiGame, EditorPlacedModel } from "@/lib/stores/useZoogiGame";

interface PlacedModelProps {
  model: EditorPlacedModel;
  isSelected: boolean;
  onClick: () => void;
}

function PlacedModel({ model, isSelected, onClick }: PlacedModelProps) {
  const groupRef = useRef<THREE.Group>(null);
  const [hovered, setHovered] = useState(false);
  
  const { scene } = useGLTF(model.modelUrl);
  
  const clonedScene = useMemo(() => {
    const clone = scene.clone(true);
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
    return clone;
  }, [scene]);
  
  const handleClick = (e: ThreeEvent<MouseEvent>) => {
    e.stopPropagation();
    onClick();
  };
  
  return (
    <group
      ref={groupRef}
      position={model.position}
      rotation={[model.rotation[0], model.rotation[1], model.rotation[2]]}
      scale={model.scale}
      onClick={handleClick}
      onPointerOver={(e) => {
        e.stopPropagation();
        setHovered(true);
        document.body.style.cursor = "pointer";
      }}
      onPointerOut={() => {
        setHovered(false);
        document.body.style.cursor = "default";
      }}
    >
      <primitive object={clonedScene} />
      {(isSelected || hovered) && (
        <mesh scale={[1.05, 1.05, 1.05]}>
          <boxGeometry args={[2, 2, 2]} />
          <meshBasicMaterial 
            color={isSelected ? "#a855f7" : "#ffffff"} 
            transparent 
            opacity={isSelected ? 0.3 : 0.15} 
            wireframe 
          />
        </mesh>
      )}
    </group>
  );
}

function SelectedModelWithGizmo({ model, onClick }: { model: EditorPlacedModel; onClick: () => void }) {
  const groupRef = useRef<THREE.Group>(null);
  const transformRef = useRef<any>(null);
  const isDragging = useRef(false);
  const [mounted, setMounted] = useState(false);
  const [hovered, setHovered] = useState(false);
  
  const transformMode = useZoogiGame((state) => state.transformMode);
  const updateEditorPlacedModel = useZoogiGame((state) => state.updateEditorPlacedModel);
  
  const { scene } = useGLTF(model.modelUrl);
  
  const clonedScene = useMemo(() => {
    const clone = scene.clone(true);
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
    return clone;
  }, [scene]);

  useEffect(() => {
    setMounted(true);
  }, []);

  const handleTransformChange = useCallback(() => {
    if (!groupRef.current) return;
    
    const pos = groupRef.current.position;
    const rot = groupRef.current.rotation;
    const scl = groupRef.current.scale;
    
    updateEditorPlacedModel(model.id, {
      position: [
        Math.round(pos.x * 10) / 10,
        Math.round(pos.y * 10) / 10,
        Math.round(pos.z * 10) / 10,
      ] as [number, number, number],
      rotation: [
        Math.round(rot.x * 100) / 100,
        Math.round(rot.y * 100) / 100,
        Math.round(rot.z * 100) / 100,
      ] as [number, number, number],
      scale: [
        Math.round(scl.x * 100) / 100,
        Math.round(scl.y * 100) / 100,
        Math.round(scl.z * 100) / 100,
      ] as [number, number, number],
    });
  }, [model.id, updateEditorPlacedModel]);

  const handleClick = (e: ThreeEvent<MouseEvent>) => {
    e.stopPropagation();
    onClick();
  };

  return (
    <>
      <group
        ref={groupRef}
        position={model.position}
        rotation={[model.rotation[0], model.rotation[1], model.rotation[2]]}
        scale={model.scale}
        onClick={handleClick}
        onPointerOver={(e) => {
          e.stopPropagation();
          setHovered(true);
          document.body.style.cursor = "pointer";
        }}
        onPointerOut={() => {
          setHovered(false);
          document.body.style.cursor = "default";
        }}
      >
        <primitive object={clonedScene} />
        <mesh scale={[1.05, 1.05, 1.05]}>
          <boxGeometry args={[2, 2, 2]} />
          <meshBasicMaterial 
            color="#a855f7"
            transparent 
            opacity={0.3} 
            wireframe 
          />
        </mesh>
      </group>
      {mounted && groupRef.current && (
        <TransformControls
          ref={transformRef}
          object={groupRef.current}
          mode={transformMode}
          size={0.8}
          onObjectChange={handleTransformChange}
          onMouseDown={() => { isDragging.current = true; }}
          onMouseUp={() => { isDragging.current = false; handleTransformChange(); }}
        />
      )}
    </>
  );
}

function PlacedModelWrapper({ model, isSelected, onClick }: PlacedModelProps) {
  return (
    <Suspense fallback={
      <mesh position={model.position} onClick={(e) => { e.stopPropagation(); onClick(); }}>
        <boxGeometry args={[1, 1, 1]} />
        <meshStandardMaterial color={isSelected ? "#a855f7" : "#8844aa"} wireframe />
      </mesh>
    }>
      {isSelected && useZoogiGame.getState().gameMode === "map_editor" ? (
        <SelectedModelWithGizmo model={model} onClick={onClick} />
      ) : (
        <PlacedModel model={model} isSelected={false} onClick={onClick} />
      )}
    </Suspense>
  );
}

export function EditorPlacedModels() {
  const { editorPlacedModels, selectedEditorModelId, selectEditorModel, gameMode } = useZoogiGame();
  
  if (editorPlacedModels.length === 0) return null;
  
  const handleSelectModel = (id: string) => {
    if (gameMode === "map_editor") {
      selectEditorModel(id);
    }
  };
  
  return (
    <group name="editor-placed-models">
      {editorPlacedModels.map((model) => (
        <PlacedModelWrapper 
          key={model.id} 
          model={model} 
          isSelected={selectedEditorModelId === model.id}
          onClick={() => handleSelectModel(model.id)}
        />
      ))}
    </group>
  );
}
