import { useRef, useMemo, useEffect } from "react";
import { useFrame, ThreeEvent } from "@react-three/fiber";
import * as THREE from "three";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { WALL_OWNERSHIP_CONTROL_ZONE_RADIUS, WALL_OWNERSHIP_GAP_ANGLES } from "@/lib/arenaConstants";

interface ControlPointZonesProps {
  enabled?: boolean;
  zoneRadius?: number;
  captureSpeed?: number;
  scorePerSecond?: number;
}

export function ControlPointZones({
  enabled = true,
  zoneRadius = WALL_OWNERSHIP_CONTROL_ZONE_RADIUS,
  captureSpeed = 0.5,
  scorePerSecond = 1,
}: ControlPointZonesProps) {
  const wallOwnershipMode = useZoogiGame(state => state.wallOwnershipMode);
  const controlZones = useZoogiGame(state => state.controlZones);
  const updateControlZone = useZoogiGame(state => state.updateControlZone);
  const addOwnershipScore = useZoogiGame(state => state.addOwnershipScore);
  const zoneEditorConfigs = useZoogiGame(state => state.zoneEditorConfigs);
  const initializeZoneConfigs = useZoogiGame(state => state.initializeZoneConfigs);
  const selectZone = useZoogiGame(state => state.selectZone);
  const selectedZoneId = useZoogiGame(state => state.selectedZoneId);
  const gameMode = useZoogiGame(state => state.gameMode);
  
  const lastScoreTimeRef = useRef<Map<string, number>>(new Map());
  
  useEffect(() => {
    if (zoneEditorConfigs.length === 0) {
      initializeZoneConfigs();
    }
  }, [zoneEditorConfigs.length, initializeZoneConfigs]);
  
  const zonePositions = useMemo(() => {
    if (zoneEditorConfigs.length > 0) {
      return zoneEditorConfigs
        .filter(config => config.visible)
        .map((config) => ({
          id: config.id,
          angle: config.angle,
          distance: config.distance,
          isSpawn: config.isSpawn,
          position: [
            Math.cos(config.angle) * config.distance,
            0.1,
            Math.sin(config.angle) * config.distance
          ] as [number, number, number]
        }));
    }
    return WALL_OWNERSHIP_GAP_ANGLES.map((angle, i) => ({
      id: `control-zone-${i}`,
      angle,
      distance: zoneRadius,
      isSpawn: true,
      position: [
        Math.cos(angle) * zoneRadius,
        0.1,
        Math.sin(angle) * zoneRadius
      ] as [number, number, number]
    }));
  }, [zoneRadius, zoneEditorConfigs]);

  useFrame((_, delta) => {
    if (!enabled) return;
    // Only process capture/scoring during restriction phase (when wallOwnershipMode is active)
    if (!wallOwnershipMode) return;

    const store = useZoogiGame.getState();
    const now = Date.now();
    
    const entityPositions: { pos: [number, number, number]; ownerId: string }[] = [];
    
    if (store.playerEntity) {
      entityPositions.push({
        pos: store.playerEntity.position,
        ownerId: `player-${store.currentLocalPlayerIndex}`
      });
    }
    
    store.enemies.forEach((enemy, idx) => {
      entityPositions.push({
        pos: enemy.position,
        ownerId: `player-${idx + 1}`
      });
    });

    zonePositions.forEach((zone) => {
      const storeZone = controlZones.find(z => z.id === zone.id);
      if (!storeZone) return;
      
      const CAPTURE_RADIUS = 4;
      const playersInZone: string[] = [];
      
      for (const entity of entityPositions) {
        const dx = entity.pos[0] - zone.position[0];
        const dz = entity.pos[2] - zone.position[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        
        if (dist < CAPTURE_RADIUS) {
          playersInZone.push(entity.ownerId);
        }
      }
      
      if (playersInZone.length === 0) {
        if (storeZone.captureProgress > 0) {
          const newProgress = Math.max(0, storeZone.captureProgress - delta * captureSpeed * 0.5);
          updateControlZone(zone.id, storeZone.ownerId, newProgress, false);
        }
        return;
      }
      
      const uniquePlayers = Array.from(new Set(playersInZone));
      const isContested = uniquePlayers.length > 1;
      
      if (isContested) {
        updateControlZone(zone.id, storeZone.ownerId, storeZone.captureProgress, true);
        return;
      }
      
      const capturingPlayer = uniquePlayers[0];
      
      if (storeZone.ownerId === capturingPlayer) {
        if (storeZone.captureProgress >= 1) {
          const lastScore = lastScoreTimeRef.current.get(zone.id) || 0;
          if (now - lastScore >= 1000) {
            lastScoreTimeRef.current.set(zone.id, now);
            addOwnershipScore(capturingPlayer, scorePerSecond);
          }
        }
        return;
      }
      
      if (storeZone.ownerId && storeZone.ownerId !== capturingPlayer) {
        const newProgress = Math.max(0, storeZone.captureProgress - delta * captureSpeed);
        if (newProgress <= 0) {
          updateControlZone(zone.id, null, 0, false);
        } else {
          updateControlZone(zone.id, storeZone.ownerId, newProgress, false);
        }
        return;
      }
      
      const newProgress = Math.min(1, storeZone.captureProgress + delta * captureSpeed);
      if (newProgress >= 1) {
        updateControlZone(zone.id, capturingPlayer, 1, false);
        addOwnershipScore(capturingPlayer, 5);
      } else {
        updateControlZone(zone.id, capturingPlayer, newProgress, false);
      }
    });
  });

  if (!enabled || gameMode !== "map_editor") return null;

  const handleZoneClick = (zoneId: string) => {
    if (gameMode === "map_editor") {
      selectZone(selectedZoneId === zoneId ? null : zoneId);
    }
  };

  return (
    <group name="control-point-zones">
      {zonePositions.map((zone) => {
        const storeZone = controlZones.find(z => z.id === zone.id);
        return (
          <ControlPointVisual 
            key={zone.id}
            zoneId={zone.id}
            position={zone.position}
            angle={zone.angle}
            ownerId={storeZone?.ownerId || null}
            ownerColor={storeZone?.ownerColor || "#ffffff"}
            captureProgress={storeZone?.captureProgress || 0}
            isContested={storeZone?.isContested || false}
            isSpawn={zone.isSpawn}
            isSelected={selectedZoneId === zone.id}
            isEditorMode={gameMode === "map_editor"}
            onClick={() => handleZoneClick(zone.id)}
          />
        );
      })}
    </group>
  );
}

interface ControlPointVisualProps {
  zoneId: string;
  position: [number, number, number];
  angle: number;
  ownerId: string | null;
  ownerColor: string;
  captureProgress: number;
  isContested: boolean;
  isSpawn?: boolean;
  isSelected?: boolean;
  isEditorMode?: boolean;
  onClick?: () => void;
}

function ControlPointVisual({ 
  position, 
  ownerColor, 
  captureProgress, 
  isContested,
  isSpawn = false,
  isSelected = false,
  isEditorMode = false,
  onClick
}: ControlPointVisualProps) {
  const ringRef = useRef<THREE.Mesh>(null);
  const pulseRef = useRef(0);
  
  useFrame((_, delta) => {
    if (!ringRef.current) return;
    pulseRef.current += delta * (isContested ? 8 : isSelected ? 5 : 3);
    
    const material = ringRef.current.material as THREE.MeshStandardMaterial;
    if (material) {
      if (isSelected) {
        const flash = Math.sin(pulseRef.current) * 0.3 + 0.7;
        material.emissiveIntensity = 0.5 + flash * 0.5;
        material.color.set("#00ffff");
      } else if (isContested) {
        const flash = Math.sin(pulseRef.current) * 0.5 + 0.5;
        material.emissiveIntensity = 0.3 + flash * 0.7;
        material.color.set("#ff4444");
      } else {
        material.emissiveIntensity = 0.2 + captureProgress * 0.6;
        material.color.set(ownerColor);
      }
    }
    
    const scale = isSelected ? 1.1 : 1 + Math.sin(pulseRef.current) * 0.05;
    ringRef.current.scale.setScalar(scale);
  });

  const handleClick = (e: ThreeEvent<MouseEvent>) => {
    if (isEditorMode && onClick) {
      e.stopPropagation();
      onClick();
    }
  };

  return (
    <group position={position} onClick={handleClick}>
      <mesh 
        ref={ringRef} 
        rotation={[-Math.PI / 2, 0, 0]}
      >
        <ringGeometry args={[3, 4, 32]} />
        <meshStandardMaterial 
          color={isSelected ? "#00ffff" : ownerColor}
          transparent
          opacity={isSelected ? 0.8 : 0.4 + captureProgress * 0.4}
          emissive={isSelected ? "#00ffff" : ownerColor}
          emissiveIntensity={isSelected ? 0.8 : 0.3}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
        <ringGeometry args={[3.2, 3.2 + captureProgress * 0.8, 32, 1, 0, captureProgress * Math.PI * 2]} />
        <meshStandardMaterial 
          color="#00ff88"
          emissive="#00ff88"
          emissiveIntensity={0.8}
          side={THREE.DoubleSide}
        />
      </mesh>
      
      {isContested && (
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.05, 0]}>
          <ringGeometry args={[2.5, 4.5, 6]} />
          <meshStandardMaterial 
            color="#ff4444"
            transparent
            opacity={0.6}
            emissive="#ff4444"
            emissiveIntensity={1}
            side={THREE.DoubleSide}
          />
        </mesh>
      )}
      
      {isSpawn && isEditorMode && (
        <group position={[0, 0.1, 0]}>
          <mesh rotation={[-Math.PI / 2, 0, 0]}>
            <circleGeometry args={[0.8, 6]} />
            <meshStandardMaterial 
              color="#00ff00"
              transparent
              opacity={0.7}
              emissive="#00ff00"
              emissiveIntensity={0.5}
              side={THREE.DoubleSide}
            />
          </mesh>
          <mesh position={[0, 0.5, 0]}>
            <coneGeometry args={[0.3, 0.6, 6]} />
            <meshStandardMaterial 
              color="#00ff00"
              emissive="#00ff00"
              emissiveIntensity={0.5}
            />
          </mesh>
        </group>
      )}
      
      {isSelected && (
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.08, 0]}>
          <ringGeometry args={[4.2, 4.5, 32]} />
          <meshStandardMaterial 
            color="#ffffff"
            transparent
            opacity={0.8}
            emissive="#ffffff"
            emissiveIntensity={1}
            side={THREE.DoubleSide}
          />
        </mesh>
      )}
    </group>
  );
}
