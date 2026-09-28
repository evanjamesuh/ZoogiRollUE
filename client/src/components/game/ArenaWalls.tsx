import * as THREE from "three";
import { useMemo } from "react";
import { ARENA_RADIUS, WALL_THICKNESS, isInGap, GAP_ARC } from "@/lib/arenaConstants";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export { ARENA_RADIUS, WALL_THICKNESS, isInGap, GAP_ARC };

interface ArenaWallsProps {
  theme: "grass" | "ice" | "lava" | "space" | "saturn";
}

export const WALL_HEIGHT = 2.5;
const SEGMENT_COUNT = 48;
const GAP_COUNT = 5;
const GAP_SIZE = 3;

export function ArenaWalls({ theme }: ArenaWallsProps) {
  const wallSegments = useZoogiGame((state) => state.wallSegments);
  
  const wallConfig = useMemo(() => {
    switch (theme) {
      case "space":
        return { 
          colors: ["#5C5C5C", "#4A4A4A", "#3D3D3D", "#6B6B6B"],
          roughness: 0.9,
          metalness: 0,
          emissive: "#000000",
          emissiveIntensity: 0
        };
      case "grass":
        return { 
          colors: ["#8B4513", "#6B3E0A", "#A0522D", "#654321"],
          roughness: 0.85,
          metalness: 0,
          emissive: "#000000",
          emissiveIntensity: 0
        };
      case "lava":
        return { 
          colors: ["#2D0A0A", "#1A0505", "#3D0A0A", "#4A0F0F"],
          roughness: 0.7,
          metalness: 0.1,
          emissive: "#FF4500",
          emissiveIntensity: 0.3
        };
      case "ice":
        return { 
          colors: ["#B0E0E6", "#87CEEB", "#ADD8E6", "#E0FFFF"],
          roughness: 0.1,
          metalness: 0.4,
          emissive: "#87CEEB",
          emissiveIntensity: 0.1
        };
      case "saturn":
        return { 
          colors: ["#3E2723", "#4E342E", "#5D4037", "#6D4C41"],
          roughness: 0.6,
          metalness: 0.3,
          emissive: "#FFA726",
          emissiveIntensity: 0.2
        };
      default:
        return { 
          colors: ["#5C5C5C", "#4A4A4A", "#3D3D3D", "#6B6B6B"],
          roughness: 0.9,
          metalness: 0,
          emissive: "#000000",
          emissiveIntensity: 0
        };
    }
  }, [theme]);

  const segments = useMemo(() => {
    const result: { index: number; angle: number; x: number; z: number; colorIndex: number; heightVar: number }[] = [];
    const segmentsPerSection = SEGMENT_COUNT / GAP_COUNT;
    
    for (let i = 0; i < SEGMENT_COUNT; i++) {
      const posInSection = i % segmentsPerSection;
      if (posInSection < GAP_SIZE) continue;
      
      const angle = (i / SEGMENT_COUNT) * Math.PI * 2;
      const wallRadius = ARENA_RADIUS + WALL_THICKNESS / 2;
      result.push({
        index: i,
        angle,
        x: Math.cos(angle) * wallRadius,
        z: Math.sin(angle) * wallRadius,
        colorIndex: i % 4,
        heightVar: 0.8 + Math.sin(i * 0.7) * 0.4
      });
    }
    return result;
  }, []);

  const segmentArc = (Math.PI * 2) / SEGMENT_COUNT;
  const segmentWidth = 2 * (ARENA_RADIUS + WALL_THICKNESS / 2) * Math.sin(segmentArc / 2) * 1.1;

  const getSegmentHealth = (index: number) => {
    const seg = wallSegments.find(s => s.index === index);
    return seg ? seg.health / seg.maxHealth : 1;
  };
  
  const isSegmentDestroyed = (index: number) => {
    const seg = wallSegments.find(s => s.index === index);
    return seg ? seg.isDestroyed : false;
  };

  return (
    <group>
      {segments.map((seg, i) => {
        if (isSegmentDestroyed(seg.index)) return null;
        
        const healthRatio = getSegmentHealth(seg.index);
        const height = WALL_HEIGHT * seg.heightVar * Math.max(0.3, healthRatio);
        const damageColor = healthRatio < 1 
          ? new THREE.Color(wallConfig.colors[seg.colorIndex]).lerp(new THREE.Color("#8B0000"), 1 - healthRatio)
          : new THREE.Color(wallConfig.colors[seg.colorIndex]);
        
        return (
          <mesh
            key={i}
            position={[seg.x, height / 2, seg.z]}
            rotation={[0, -seg.angle + Math.PI / 2, 0]}
            castShadow
            receiveShadow
          >
            <boxGeometry args={[segmentWidth, height, WALL_THICKNESS]} />
            <meshStandardMaterial
              color={damageColor}
              roughness={wallConfig.roughness}
              metalness={wallConfig.metalness}
              emissive={healthRatio < 0.5 ? "#FF4500" : wallConfig.emissive}
              emissiveIntensity={healthRatio < 0.5 ? 0.3 : wallConfig.emissiveIntensity}
              transparent={theme === "ice"}
              opacity={theme === "ice" ? 0.75 : 1}
            />
          </mesh>
        );
      })}
      {theme === "lava" && segments.filter((_, i) => i % 6 === 0).map((seg, i) => (
        <pointLight
          key={`glow-${i}`}
          position={[seg.x * 0.95, WALL_HEIGHT + 0.5, seg.z * 0.95]}
          color="#FF4500"
          intensity={0.4}
          distance={6}
        />
      ))}
    </group>
  );
}

export function getCollisionRadius(): number {
  return ARENA_RADIUS;
}
