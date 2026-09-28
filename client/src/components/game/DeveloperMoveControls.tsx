import * as THREE from "three";
import { useRef, useState, useEffect } from "react";
import { useThree, useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { 
  getTreeOffset, 
  getTreeRotation, 
  setTreeOffset, 
  setTreeRotation, 
  confirmAllOffsets, 
  getAdjustedTreePositions,
  setCurrentMap,
  getMapElements,
  getElementOffset,
  setElementOffset,
  getElementRotation,
  setElementRotation
} from "@/lib/treeOffsets";
import { getTreePositions } from "@/lib/arenaConstants";
import type { MapTheme } from "@/lib/stores/useZoogiGame";

export { getTreeOffset, getTreeRotation, getAdjustedTreePositions };

interface MovableElement {
  type: string;
  index: number;
  position: [number, number, number];
  radius: number;
}

let currentIncrement = 0.1;

export function getIncrement(): number {
  return currentIncrement;
}

export function setIncrement(value: number) {
  currentIncrement = Math.max(0.01, Math.min(1.0, value));
  useZoogiGame.getState().incrementMoveCounter();
}

export function moveSelectedElementByArrows(deltaX: number, deltaY: number, deltaZ: number) {
  const state = useZoogiGame.getState();
  const selected = state.selectedMoveElement;
  if (!selected) return;
  
  const currentMap = state.selectedMap || "grass";
  const current = getElementOffset(currentMap, selected.type as any, selected.index);
  const newOffset: [number, number, number] = [current[0] + deltaX, current[1] + deltaY, current[2] + deltaZ];
  setElementOffset(currentMap, selected.type as any, selected.index, newOffset);
  state.incrementMoveCounter();
  console.log(`${selected.type} #${selected.index} moved to offset: (${newOffset[0].toFixed(2)}, ${newOffset[1].toFixed(2)}, ${newOffset[2].toFixed(2)})`);
}

export function rotateSelectedElement(deltaRotation: number) {
  const state = useZoogiGame.getState();
  const selected = state.selectedMoveElement;
  if (!selected) return;
  
  const currentMap = state.selectedMap || "grass";
  const current = getElementRotation(currentMap, selected.type as any, selected.index);
  const newRotation = current + deltaRotation;
  setElementRotation(currentMap, selected.type as any, selected.index, newRotation);
  state.incrementMoveCounter();
  console.log(`${selected.type} #${selected.index} rotated to: ${newRotation.toFixed(2)} radians`);
}

export function snapToGround() {
  const state = useZoogiGame.getState();
  const selected = state.selectedMoveElement;
  if (!selected) return;
  
  const currentMap = state.selectedMap || "grass";
  const current = getElementOffset(currentMap, selected.type as any, selected.index);
  setElementOffset(currentMap, selected.type as any, selected.index, [current[0], 0, current[2]]);
  state.incrementMoveCounter();
  console.log(`${selected.type} #${selected.index} snapped to ground (Y=0)`);
}

export function confirmPlacements() {
  const result = confirmAllOffsets();
  console.log("All placements confirmed:", JSON.stringify(result));
  const state = useZoogiGame.getState();
  state.incrementMoveCounter();
  state.setSelectedMoveElement(null);
}

export function DeveloperMoveControls() {
  const developerMoveMode = useZoogiGame((state) => state.developerMoveMode);
  const phase = useZoogiGame((state) => state.phase);
  const selectedMoveElement = useZoogiGame((state) => state.selectedMoveElement);
  const setSelectedMoveElement = useZoogiGame((state) => state.setSelectedMoveElement);
  const moveUpdateCounter = useZoogiGame((state) => state.moveUpdateCounter);
  const incrementMoveCounter = useZoogiGame((state) => state.incrementMoveCounter);
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  
  const { camera, gl, raycaster, scene } = useThree();
  const [isDragging, setIsDragging] = useState(false);
  const dragStartRef = useRef<{ x: number; z: number } | null>(null);
  const elementStartPosRef = useRef<[number, number, number] | null>(null);
  const planeRef = useRef<THREE.Plane>(new THREE.Plane(new THREE.Vector3(0, 1, 0), 0));
  
  // Only allow developer controls in arena editor phase - never during gameplay
  const isEditorPhase = phase === "arena_editor";
  const shouldBeActive = developerMoveMode && isEditorPhase;
  
  useEffect(() => {
    if (selectedMap) {
      setCurrentMap(selectedMap);
      // Clear selection and force re-render when map changes to prevent stale rings
      setSelectedMoveElement(null);
      setIsDragging(false);
      dragStartRef.current = null;
      elementStartPosRef.current = null;
      incrementMoveCounter();
    }
  }, [selectedMap, setSelectedMoveElement, incrementMoveCounter]);
  
  const getMovableElements = (): MovableElement[] => {
    const currentMap = selectedMap || "grass";
    const elements = getMapElements(currentMap);
    console.log(`[DEV MODE] getMovableElements called with map="${currentMap}", returning ${elements.length} elements:`, elements.map(e => e.type));
    return elements;
  };
  
  const findElementAtPosition = (worldX: number, worldZ: number): MovableElement | null => {
    const elements = getMovableElements();
    
    for (const element of elements) {
      const dx = worldX - element.position[0];
      const dz = worldZ - element.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < element.radius) {
        return element;
      }
    }
    
    return null;
  };
  
  const getWorldPosition = (clientX: number, clientY: number): { x: number; z: number } | null => {
    const rect = gl.domElement.getBoundingClientRect();
    const mouse = new THREE.Vector2(
      ((clientX - rect.left) / rect.width) * 2 - 1,
      -((clientY - rect.top) / rect.height) * 2 + 1
    );
    
    raycaster.setFromCamera(mouse, camera);
    const intersectPoint = new THREE.Vector3();
    const hit = raycaster.ray.intersectPlane(planeRef.current, intersectPoint);
    
    if (hit) {
      return { x: intersectPoint.x, z: intersectPoint.z };
    }
    return null;
  };
  
  useEffect(() => {
    if (!shouldBeActive) return;
    
    const handlePointerDown = (e: PointerEvent) => {
      const worldPos = getWorldPosition(e.clientX, e.clientY);
      if (!worldPos) return;
      
      const element = findElementAtPosition(worldPos.x, worldPos.z);
      
      if (element) {
        setSelectedMoveElement({ type: element.type, index: element.index });
        setIsDragging(true);
        useZoogiGame.getState().setDeveloperDragActive(true);
        dragStartRef.current = worldPos;
        elementStartPosRef.current = element.position;
        console.log(`Selected ${element.type} #${element.index} at (${element.position[0].toFixed(2)}, ${element.position[2].toFixed(2)})`);
      }
    };
    
    const handlePointerMove = (e: PointerEvent) => {
      if (!isDragging || !selectedMoveElement || !dragStartRef.current || !elementStartPosRef.current) return;
      
      const worldPos = getWorldPosition(e.clientX, e.clientY);
      if (!worldPos) return;
      
      const deltaX = worldPos.x - dragStartRef.current.x;
      const deltaZ = worldPos.z - dragStartRef.current.z;
      
      const currentMap = selectedMap || "grass";
      const startOffset = getElementOffset(currentMap, selectedMoveElement.type as any, selectedMoveElement.index);
      const currentY = startOffset[1];
      
      setElementOffset(currentMap, selectedMoveElement.type as any, selectedMoveElement.index, [
        startOffset[0] + deltaX,
        currentY,
        startOffset[2] + deltaZ
      ]);
      
      dragStartRef.current = worldPos;
      incrementMoveCounter();
    };
    
    const handlePointerUp = () => {
      if (isDragging && selectedMoveElement) {
        const currentMap = selectedMap || "grass";
        const offset = getElementOffset(currentMap, selectedMoveElement.type as any, selectedMoveElement.index);
        console.log(`${selectedMoveElement.type} #${selectedMoveElement.index} final offset: (${offset[0].toFixed(2)}, ${offset[1].toFixed(2)}, ${offset[2].toFixed(2)})`);
      }
      setIsDragging(false);
      useZoogiGame.getState().setDeveloperDragActive(false);
      dragStartRef.current = null;
      elementStartPosRef.current = null;
    };
    
    gl.domElement.addEventListener("pointerdown", handlePointerDown);
    gl.domElement.addEventListener("pointermove", handlePointerMove);
    gl.domElement.addEventListener("pointerup", handlePointerUp);
    
    return () => {
      gl.domElement.removeEventListener("pointerdown", handlePointerDown);
      gl.domElement.removeEventListener("pointermove", handlePointerMove);
      gl.domElement.removeEventListener("pointerup", handlePointerUp);
    };
  }, [shouldBeActive, isDragging, selectedMoveElement, camera, gl, raycaster, setSelectedMoveElement, incrementMoveCounter, selectedMap]);
  
  // Only render in editor phase with developer mode enabled
  if (!shouldBeActive) return null;
  
  const elements = getMovableElements();
  
  return (
    <group>
      {elements.map((element, i) => {
        const isSelected = selectedMoveElement?.type === element.type && selectedMoveElement?.index === element.index;
        
        return (
          <mesh
            key={`${element.type}-${element.index}-${moveUpdateCounter}`}
            position={[element.position[0], 0.5, element.position[2]]}
            rotation={[-Math.PI / 2, 0, 0]}
          >
            <ringGeometry args={[element.radius - 0.2, element.radius, 32]} />
            <meshBasicMaterial
              color={isSelected ? "#ff8800" : "#00ff00"}
              transparent
              opacity={isSelected ? 0.8 : 0.6}
              side={THREE.DoubleSide}
            />
          </mesh>
        );
      })}
      
      {selectedMoveElement && (
        <mesh position={[0, 5, 0]}>
          <boxGeometry args={[0.01, 0.01, 0.01]} />
          <meshBasicMaterial visible={false} />
        </mesh>
      )}
    </group>
  );
}
