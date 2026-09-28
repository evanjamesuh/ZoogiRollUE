import * as THREE from "three";
import { useTexture } from "@react-three/drei";
import { useMemo } from "react";
import { BackgroundSettings } from "./BackgroundControlPanel";

interface MeadowsBackgroundProps {
  settings?: BackgroundSettings;
  customImageTexture?: THREE.Texture | null;
}

export function MeadowsBackground({ settings, customImageTexture }: MeadowsBackgroundProps) {
  const defaultTexture = useTexture("/textures/meadows_background.png");
  const baseTexture = customImageTexture || defaultTexture;
  
  const distance = settings?.distance ?? 200;
  const yPos = settings?.yPos ?? 70;
  const width = settings?.width ?? 500;
  const height = settings?.height ?? 250;
  const rotation = settings?.rotation ?? 0;
  const opacity = settings?.opacity ?? 0.9;
  const visible = settings?.visible ?? true;
  const mirrorBack = settings?.mirrorBack ?? false;
  const mirrorFront = settings?.mirrorFront ?? true;
  const mirrorLeft = settings?.mirrorLeft ?? false;
  const mirrorRight = settings?.mirrorRight ?? true;
  
  const textures = useMemo(() => {
    const back = baseTexture.clone();
    back.wrapS = back.wrapT = THREE.RepeatWrapping;
    if (mirrorBack) {
      back.repeat.set(-1, 1);
      back.offset.set(1, 0);
    } else {
      back.repeat.set(1, 1);
      back.offset.set(0, 0);
    }
    
    const front = baseTexture.clone();
    front.wrapS = front.wrapT = THREE.RepeatWrapping;
    if (mirrorFront) {
      front.repeat.set(-1, 1);
      front.offset.set(1, 0);
    } else {
      front.repeat.set(1, 1);
      front.offset.set(0, 0);
    }
    
    const left = baseTexture.clone();
    left.wrapS = left.wrapT = THREE.RepeatWrapping;
    if (mirrorLeft) {
      left.repeat.set(-1, 1);
      left.offset.set(1, 0);
    } else {
      left.repeat.set(1, 1);
      left.offset.set(0, 0);
    }
    
    const right = baseTexture.clone();
    right.wrapS = right.wrapT = THREE.RepeatWrapping;
    if (mirrorRight) {
      right.repeat.set(-1, 1);
      right.offset.set(1, 0);
    } else {
      right.repeat.set(1, 1);
      right.offset.set(0, 0);
    }
    
    return { back, front, left, right };
  }, [baseTexture, mirrorBack, mirrorFront, mirrorLeft, mirrorRight]);
  
  if (!visible) return null;
  
  const rotationRad = (rotation * Math.PI) / 180;
  
  return (
    <group rotation={[0, rotationRad, 0]}>
      {/* Back */}
      <mesh position={[0, yPos, -distance]} renderOrder={-1}>
        <planeGeometry args={[width, height]} />
        <meshBasicMaterial 
          map={textures.back} 
          side={THREE.FrontSide}
          transparent
          opacity={opacity}
          depthWrite={false}
        />
      </mesh>
      
      {/* Front */}
      <mesh position={[0, yPos, distance]} rotation={[0, Math.PI, 0]} renderOrder={-1}>
        <planeGeometry args={[width, height]} />
        <meshBasicMaterial 
          map={textures.front} 
          side={THREE.FrontSide}
          transparent
          opacity={opacity}
          depthWrite={false}
        />
      </mesh>
      
      {/* Left */}
      <mesh position={[-distance, yPos, 0]} rotation={[0, Math.PI / 2, 0]} renderOrder={-1}>
        <planeGeometry args={[width, height]} />
        <meshBasicMaterial 
          map={textures.left} 
          side={THREE.FrontSide}
          transparent
          opacity={opacity}
          depthWrite={false}
        />
      </mesh>
      
      {/* Right */}
      <mesh position={[distance, yPos, 0]} rotation={[0, -Math.PI / 2, 0]} renderOrder={-1}>
        <planeGeometry args={[width, height]} />
        <meshBasicMaterial 
          map={textures.right} 
          side={THREE.FrontSide}
          transparent
          opacity={opacity}
          depthWrite={false}
        />
      </mesh>
    </group>
  );
}
