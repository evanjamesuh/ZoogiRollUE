import { useEffect, useRef, useMemo, useState, useCallback } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { WALL_OWNERSHIP_OUTER_RADIUS, WALL_OWNERSHIP_GAP_ANGLES } from "@/lib/arenaConstants";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

interface WallCell {
  id: string;
  segmentIndex: number;
  cellIndex: number;
  angle: number;
  isBroken: boolean;
  position: [number, number, number];
  rotation: { w: number; x: number; y: number; z: number };
  size: [number, number, number];
  ownerColor: string;
}

interface ScatteredBlock {
  id: string;
  position: [number, number, number];
  velocity: [number, number, number];
  rotation: [number, number, number];
  angularVelocity: [number, number, number];
  createdAt: number;
  color: string;
}

interface OuterRingWallProps {
  enabled?: boolean;
  wallHeight?: number;
  wallThickness?: number;
  wallRadius?: number;
  theme?: "grass" | "ice" | "lava" | "space" | "saturn";
  rebuildTime?: number;
}

const THEME_WALL_COLORS: Record<string, string> = {
  grass: "#00FFFF",
  ice: "#81D4FA",
  lava: "#FF5722",
  space: "#00FFFF",
  saturn: "#FFA726",
  tomb: "#E0B88A",
};

function createCurvedWallGeometry(
  innerRadius: number,
  outerRadius: number,
  height: number,
  startAngle: number,
  endAngle: number,
  segments: number = 32
): THREE.BufferGeometry {
  const geometry = new THREE.BufferGeometry();
  const vertices: number[] = [];
  const indices: number[] = [];
  const normals: number[] = [];
  const uvs: number[] = [];

  const angleRange = endAngle - startAngle;
  const segmentAngle = angleRange / segments;

  for (let i = 0; i <= segments; i++) {
    const angle = startAngle + i * segmentAngle;
    const cosA = Math.cos(angle);
    const sinA = Math.sin(angle);

    const innerX = cosA * innerRadius;
    const innerZ = sinA * innerRadius;
    const outerX = cosA * outerRadius;
    const outerZ = sinA * outerRadius;

    const u = i / segments;

    vertices.push(innerX, 0, innerZ);
    vertices.push(innerX, height, innerZ);
    vertices.push(outerX, 0, outerZ);
    vertices.push(outerX, height, outerZ);

    normals.push(-cosA, 0, -sinA);
    normals.push(-cosA, 0, -sinA);
    normals.push(cosA, 0, sinA);
    normals.push(cosA, 0, sinA);

    uvs.push(u, 0);
    uvs.push(u, 1);
    uvs.push(u, 0);
    uvs.push(u, 1);

    if (i < segments) {
      const base = i * 4;
      indices.push(base, base + 1, base + 4);
      indices.push(base + 1, base + 5, base + 4);
      indices.push(base + 2, base + 6, base + 3);
      indices.push(base + 3, base + 6, base + 7);
      indices.push(base, base + 2, base + 1);
      indices.push(base + 1, base + 2, base + 3);
      indices.push(base + 4, base + 5, base + 6);
      indices.push(base + 5, base + 7, base + 6);
    }
  }

  const startBase = 0;
  indices.push(startBase, startBase + 2, startBase + 1);
  indices.push(startBase + 1, startBase + 2, startBase + 3);

  const endBase = segments * 4;
  indices.push(endBase, endBase + 1, endBase + 2);
  indices.push(endBase + 1, endBase + 3, endBase + 2);

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));
  geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
  geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
  geometry.setIndex(indices);
  geometry.computeVertexNormals();

  return geometry;
}

export function OuterRingWall({
  enabled = true,
  wallHeight = 3.0,
  wallThickness = 0.8,
  wallRadius = WALL_OWNERSHIP_OUTER_RADIUS,
  theme = "grass",
  rebuildTime = 5000,
}: OuterRingWallProps) {
  const wallOwnershipMode = useZoogiGame(state => state.wallOwnershipMode);
  const gameMode = useZoogiGame(state => state.gameMode);
  const wallSettings = useZoogiGame(state => state.wallSettings);
  const shouldSpawn = wallOwnershipMode || gameMode === "map_editor";
  
  const wallColor = THEME_WALL_COLORS[theme] || THEME_WALL_COLORS.grass;
  const isGlowing = theme === "space";
  const [wallCells, setWallCells] = useState<WallCell[]>([]);
  const [scatteredBlocks, setScatteredBlocks] = useState<ScatteredBlock[]>([]);
  const cellHitCooldown = useRef<Map<string, number>>(new Map());
  const rebuildTimers = useRef<Map<string, number>>(new Map());
  const wallGroupRef = useRef<THREE.Group>(null);

  const gapWidth = wallSettings.outerWallGapWidth ?? Math.PI / 4;

  const wallSegments = useMemo(() => {
    if (!enabled || !shouldSpawn) return [];
    
    const segments: { id: string; startAngle: number; endAngle: number }[] = [];
    const sortedGaps = [...WALL_OWNERSHIP_GAP_ANGLES].sort((a, b) => a - b);
    
    for (let i = 0; i < sortedGaps.length; i++) {
      const gapEnd = sortedGaps[i] + gapWidth / 2;
      const nextGapStart = (sortedGaps[(i + 1) % sortedGaps.length] - gapWidth / 2 + Math.PI * 2) % (Math.PI * 2);
      
      let endAngle = nextGapStart;
      if (i === sortedGaps.length - 1) {
        endAngle = sortedGaps[0] - gapWidth / 2 + Math.PI * 2;
      }
      
      if (endAngle < gapEnd) {
        endAngle += Math.PI * 2;
      }
      
      segments.push({
        id: `outer-curved-wall-${i}`,
        startAngle: gapEnd,
        endAngle: endAngle,
      });
    }
    
    return segments;
  }, [enabled, shouldSpawn, gapWidth]);

  const innerRadius = wallRadius - wallThickness / 2;
  const outerRadius = wallRadius + wallThickness / 2;
  const midRadius = (innerRadius + outerRadius) / 2;

  useEffect(() => {
    if (!enabled || !shouldSpawn) {
      rebuildTimers.current.clear();
      setWallCells([]);
      setScatteredBlocks([]);
      cellHitCooldown.current.clear();
      return;
    }

    const cells: WallCell[] = [];
    const cellsPerSegment = 10;

    wallSegments.forEach((segment, segIdx) => {
      const angleRange = segment.endAngle - segment.startAngle;
      const cellAngle = angleRange / cellsPerSegment;

      for (let i = 0; i < cellsPerSegment; i++) {
        const cellStartAngle = segment.startAngle + cellAngle * i;
        const cellEndAngle = segment.startAngle + cellAngle * (i + 1);
        const angle = (cellStartAngle + cellEndAngle) / 2;

        const x = Math.cos(angle) * midRadius;
        const z = Math.sin(angle) * midRadius;

        const exactCellAngle = cellEndAngle - cellStartAngle;
        const physicsBoxLength = 2 * midRadius * Math.sin(exactCellAngle / 2);
        const visualBoxLength = physicsBoxLength * 1.05;

        const quat = new THREE.Quaternion();
        quat.setFromAxisAngle(new THREE.Vector3(0, 1, 0), -angle + Math.PI / 2);

        cells.push({
          id: `outer-wall-cell-${segIdx}-${i}`,
          segmentIndex: segIdx,
          cellIndex: i,
          angle,
          isBroken: false,
          position: [x, wallHeight / 2, z],
          rotation: { w: quat.w, x: quat.x, y: quat.y, z: quat.z },
          size: [visualBoxLength, wallHeight, wallThickness],
          ownerColor: wallColor,
        });
      }
    });

    setWallCells(cells);
  }, [enabled, shouldSpawn, wallSegments, midRadius, wallHeight, wallThickness, wallColor]);

  const breakCell = useCallback((cell: WallCell, impactVelocity: [number, number, number], impactForce: number) => {
    rebuildTimers.current.set(cell.id, Date.now());
    
    const newBlocks: ScatteredBlock[] = [];
    const blockWidth = cell.size[0] / 2;
    const blockHeight = cell.size[1] / 2;
    
    const scatterMultiplier = Math.min(2.0, impactForce / 2.0);
    
    for (let row = 0; row < 2; row++) {
      for (let col = 0; col < 2; col++) {
        const offsetX = (col - 0.5) * blockWidth * 0.8;
        const offsetY = (row - 0.5) * blockHeight * 0.8;
        
        const localOffset = new THREE.Vector3(offsetX, offsetY, 0);
        const quat = new THREE.Quaternion(cell.rotation.x, cell.rotation.y, cell.rotation.z, cell.rotation.w);
        localOffset.applyQuaternion(quat);
        
        const blockPos: [number, number, number] = [
          cell.position[0] + localOffset.x,
          cell.position[1] + localOffset.y,
          cell.position[2] + localOffset.z
        ];
        
        const scatterForce = 1.5 * scatterMultiplier;
        const randomAngle = Math.random() * Math.PI * 2;
        const blockVel: [number, number, number] = [
          impactVelocity[0] * 0.4 + Math.cos(randomAngle) * scatterForce * Math.random(),
          Math.random() * 2 * scatterMultiplier + 0.5,
          impactVelocity[2] * 0.4 + Math.sin(randomAngle) * scatterForce * Math.random()
        ];
        
        newBlocks.push({
          id: `scattered-outer-${cell.id}-${row}-${col}`,
          position: blockPos,
          velocity: blockVel,
          rotation: [Math.random() * Math.PI, Math.random() * Math.PI, Math.random() * Math.PI],
          angularVelocity: [(Math.random() - 0.5) * 4 * scatterMultiplier, (Math.random() - 0.5) * 4 * scatterMultiplier, (Math.random() - 0.5) * 4 * scatterMultiplier],
          createdAt: Date.now(),
          color: cell.ownerColor,
        });
      }
    }
    
    setWallCells(prev => prev.map(c => c.id === cell.id ? { ...c, isBroken: true } : c));
    setScatteredBlocks(prev => [...prev, ...newBlocks]);
  }, []);

  const rebuildCell = useCallback((cellId: string) => {
    setWallCells(prev => prev.map(c => c.id === cellId ? { ...c, isBroken: false } : c));
    rebuildTimers.current.delete(cellId);
  }, []);

  useFrame(() => {
    if (!enabled || !shouldSpawn) return;
    
    const store = useZoogiGame.getState();
    const now = Date.now();
    const COLLISION_COOLDOWN = 200;
    const COLLISION_RADIUS = 2.5;
    
    rebuildTimers.current.forEach((startTime, cellId) => {
      if (now - startTime >= rebuildTime) {
        rebuildCell(cellId);
      }
    });
    
    const zoogiPositions: { pos: [number, number, number]; vel: [number, number, number]; color: string }[] = [];
    
    if (store.playerEntity && !store.playerEntity.isKnockedOut) {
      zoogiPositions.push({
        pos: store.playerEntity.position,
        vel: store.playerEntity.velocity,
        color: store.playerEntity.zoogi.color
      });
    }
    
    store.enemies.forEach(enemy => {
      if (!enemy.isKnockedOut) {
        zoogiPositions.push({
          pos: enemy.position,
          vel: enemy.velocity,
          color: enemy.zoogi.color
        });
      }
    });

    wallCells.forEach(cell => {
      if (cell.isBroken) return;
      
      for (const zoogi of zoogiPositions) {
        const dx = cell.position[0] - zoogi.pos[0];
        const dz = cell.position[2] - zoogi.pos[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        
        if (dist < COLLISION_RADIUS) {
          const lastHit = cellHitCooldown.current.get(cell.id) || 0;
          if (now - lastHit < COLLISION_COOLDOWN) continue;
          
          const speed = Math.sqrt(zoogi.vel[0] ** 2 + zoogi.vel[2] ** 2);
          if (speed < 0.5) continue;
          
          cellHitCooldown.current.set(cell.id, now);
          
          setWallCells(prev => prev.map(c => 
            c.id === cell.id ? { ...c, ownerColor: zoogi.color } : c
          ));
          
          if (speed > 3.0) {
            breakCell(cell, zoogi.vel, speed);
          }
        }
      }
    });

    const ENTITY_RADIUS = 0.5;
    const wallBoundary = midRadius - ENTITY_RADIUS;
    
    const activeCellAngles: { start: number; end: number }[] = [];
    wallCells.forEach(cell => {
      if (!cell.isBroken) {
        const cellHalfAngle = Math.atan2(cell.size[0] / 2, midRadius);
        activeCellAngles.push({
          start: cell.angle - cellHalfAngle,
          end: cell.angle + cellHalfAngle
        });
      }
    });

    const isAngleBlocked = (entityAngle: number): boolean => {
      let normalizedAngle = entityAngle;
      while (normalizedAngle < 0) normalizedAngle += Math.PI * 2;
      while (normalizedAngle >= Math.PI * 2) normalizedAngle -= Math.PI * 2;
      
      for (const cell of activeCellAngles) {
        let start = cell.start;
        let end = cell.end;
        while (start < 0) start += Math.PI * 2;
        while (end < 0) end += Math.PI * 2;
        
        if (start > end) {
          if (normalizedAngle >= start || normalizedAngle <= end) return true;
        } else {
          if (normalizedAngle >= start && normalizedAngle <= end) return true;
        }
      }
      return false;
    };
    
    if (store.playerEntity && !store.playerEntity.isKnockedOut) {
      const px = store.playerEntity.position[0];
      const pz = store.playerEntity.position[2];
      const playerRadius = Math.sqrt(px * px + pz * pz);
      const playerAngle = Math.atan2(pz, px);
      
      if (playerRadius >= wallBoundary && isAngleBlocked(playerAngle)) {
        const normalX = px / playerRadius;
        const normalZ = pz / playerRadius;
        const radialVel = store.playerEntity.velocity[0] * normalX + store.playerEntity.velocity[2] * normalZ;
        
        if (radialVel > 0) {
          const bounceFactor = 0.7;
          const bounceVelX = store.playerEntity.velocity[0] - 2 * radialVel * normalX * bounceFactor;
          const bounceVelZ = store.playerEntity.velocity[2] - 2 * radialVel * normalZ * bounceFactor;
          
          const clampedRadius = wallBoundary - 0.3;
          const clampedX = normalX * clampedRadius;
          const clampedZ = normalZ * clampedRadius;
          
          store.updatePlayerVelocity([bounceVelX, store.playerEntity.velocity[1], bounceVelZ]);
          store.updatePlayerPosition([clampedX, store.playerEntity.position[1], clampedZ]);
        }
      }
    }
    
    store.enemies.forEach(enemy => {
      if (enemy.isKnockedOut) return;
      const ex = enemy.position[0];
      const ez = enemy.position[2];
      const enemyRadius = Math.sqrt(ex * ex + ez * ez);
      const enemyAngle = Math.atan2(ez, ex);
      
      if (enemyRadius >= wallBoundary && isAngleBlocked(enemyAngle)) {
        const normalX = ex / enemyRadius;
        const normalZ = ez / enemyRadius;
        const radialVel = enemy.velocity[0] * normalX + enemy.velocity[2] * normalZ;
        
        if (radialVel > 0) {
          const bounceFactor = 0.7;
          const bounceVelX = enemy.velocity[0] - 2 * radialVel * normalX * bounceFactor;
          const bounceVelZ = enemy.velocity[2] - 2 * radialVel * normalZ * bounceFactor;
          
          const clampedRadius = wallBoundary - 0.3;
          const clampedX = normalX * clampedRadius;
          const clampedZ = normalZ * clampedRadius;
          
          store.updateEnemy(enemy.id, {
            velocity: [bounceVelX, enemy.velocity[1], bounceVelZ],
            position: [clampedX, enemy.position[1], clampedZ]
          });
        }
      }
    });

    setScatteredBlocks(prev => {
      const expireTime = 4000;
      const step = 0.05;
      return prev.filter(block => {
        if (now - block.createdAt > expireTime) return false;
        block.velocity = [
          block.velocity[0] * 0.98,
          block.velocity[1] - 0.2,
          block.velocity[2] * 0.98,
        ];
        block.position = [
          block.position[0] + block.velocity[0] * step,
          Math.max(0, block.position[1] + block.velocity[1] * step),
          block.position[2] + block.velocity[2] * step,
        ];
        block.rotation = [
          block.rotation[0] + block.angularVelocity[0] * step,
          block.rotation[1] + block.angularVelocity[1] * step,
          block.rotation[2] + block.angularVelocity[2] * step,
        ];
        return true;
      });
    });
  });

  if (!enabled || !shouldSpawn) return null;

  return (
    <group ref={wallGroupRef} name="outer-ring-wall">
      {wallSegments.map((segment) => {
        const segmentCells = wallCells.filter(c => c.segmentIndex === parseInt(segment.id.split('-').pop() || '0'));
        const allBroken = segmentCells.length > 0 && segmentCells.every(c => c.isBroken);
        
        if (allBroken) return null;
        
        const geometry = createCurvedWallGeometry(
          innerRadius,
          outerRadius,
          wallHeight,
          segment.startAngle,
          segment.endAngle,
          32
        );
        
        const segmentColor = segmentCells.length > 0 ? segmentCells[0].ownerColor : wallColor;
        
        return (
          <mesh key={segment.id} geometry={geometry} castShadow receiveShadow>
            <meshStandardMaterial
              color={segmentColor}
              roughness={isGlowing ? 0.3 : 0.7}
              metalness={isGlowing ? 0.5 : 0.1}
              emissive={isGlowing ? new THREE.Color(segmentColor) : undefined}
              emissiveIntensity={isGlowing ? 0.3 : 0}
              side={THREE.DoubleSide}
            />
          </mesh>
        );
      })}
      
      {scatteredBlocks.map((block) => (
        <mesh
          key={block.id}
          position={block.position}
          rotation={block.rotation}
        >
          <boxGeometry args={[0.5, 0.5, 0.3]} />
          <meshStandardMaterial
            color={block.color}
            roughness={0.6}
            metalness={0.2}
          />
        </mesh>
      ))}
    </group>
  );
}
