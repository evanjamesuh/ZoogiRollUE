import { useEffect, useRef, useMemo, useState, useCallback } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { usePhysicsWorld } from "@/lib/physics/usePhysicsWorld";
import { ARENA_RADIUS, WALL_OWNERSHIP_GAP_ANGLES } from "@/lib/arenaConstants";
import { isRapierReady } from "@/lib/physics/rapierInit";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { triggerCollisionCameraEffect } from "@/lib/stores/useCameraEffects";
import { triggerWallHitFeel, useGameFeel } from "@/lib/stores/useGameFeel";

interface InnerBarrierWallsProps {
  enabled?: boolean;
  wallHeight?: number;
  wallThickness?: number;
  barrierRadius?: number;
  middleWallRadius?: number;
  theme?: "grass" | "ice" | "lava" | "space" | "saturn";
}

// Inner rotating walls (4 solid arcs that spin but don't break)
interface RotatingWall {
  id: string;
  segmentIndex: number;
  centerAngle: number;
  startAngle: number;
  endAngle: number;
  angularVelocity: number; // Y-axis rotation speed
  currentRotation: number; // Current Y rotation offset
}

// Solid wall segment interface
interface WallSegment {
  id: string;
  segmentIndex: number;
  centerAngle: number;
  startAngle: number;
  endAngle: number;
  position: [number, number, number];
  rotation: { w: number; x: number; y: number; z: number };
}


function createCurvedWallGeometry(
  innerRadius: number,
  outerRadius: number,
  height: number,
  startAngle: number,
  endAngle: number,
  segments: number = 32,
  centerAtOrigin: boolean = false
): THREE.BufferGeometry {
  const geometry = new THREE.BufferGeometry();
  const vertices: number[] = [];
  const indices: number[] = [];
  const normals: number[] = [];
  const uvs: number[] = [];

  const angleRange = endAngle - startAngle;
  const segmentAngle = angleRange / segments;
  
  // Calculate the center of the arc (for centering geometry at origin)
  const centerAngle = (startAngle + endAngle) / 2;
  const midRadius = (innerRadius + outerRadius) / 2;
  const arcCenterX = centerAtOrigin ? Math.cos(centerAngle) * midRadius : 0;
  const arcCenterZ = centerAtOrigin ? Math.sin(centerAngle) * midRadius : 0;

  for (let i = 0; i <= segments; i++) {
    const angle = startAngle + i * segmentAngle;
    const cosA = Math.cos(angle);
    const sinA = Math.sin(angle);

    // Subtract arc center to position geometry centered at origin
    const innerX = cosA * innerRadius - arcCenterX;
    const innerZ = sinA * innerRadius - arcCenterZ;
    const outerX = cosA * outerRadius - arcCenterX;
    const outerZ = sinA * outerRadius - arcCenterZ;

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

  const topBaseIndex = vertices.length / 3;
  for (let i = 0; i <= segments; i++) {
    const angle = startAngle + i * segmentAngle;
    const cosA = Math.cos(angle);
    const sinA = Math.sin(angle);

    // Subtract arc center for top cap as well
    const innerX = cosA * innerRadius - arcCenterX;
    const innerZ = sinA * innerRadius - arcCenterZ;
    const outerX = cosA * outerRadius - arcCenterX;
    const outerZ = sinA * outerRadius - arcCenterZ;

    const u = i / segments;

    vertices.push(innerX, height, innerZ);
    vertices.push(outerX, height, outerZ);

    normals.push(0, 1, 0);
    normals.push(0, 1, 0);

    uvs.push(u, 0);
    uvs.push(u, 1);

    if (i < segments) {
      const base = topBaseIndex + i * 2;
      indices.push(base, base + 2, base + 1);
      indices.push(base + 1, base + 2, base + 3);
    }
  }

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));
  geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
  geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
  geometry.setIndex(indices);
  geometry.computeVertexNormals();

  return geometry;
}

// Solid cyan middle wall ring
function CyanMiddleWallRing({
  innerRadius,
  outerRadius,
  height,
  gapCount,
  gapWidth
}: {
  innerRadius: number;
  outerRadius: number;
  height: number;
  gapCount: number;
  gapWidth: number;
}) {
  const arcSegments = useMemo(() => {
    if (gapCount === 0) {
      return [{ startAngle: 0, endAngle: Math.PI * 2 }];
    }
    
    const gapAngle = gapWidth;
    const totalGapAngle = gapAngle * gapCount;
    const arcAngle = (Math.PI * 2 - totalGapAngle) / gapCount;
    
    const arcs: { startAngle: number; endAngle: number }[] = [];
    for (let i = 0; i < gapCount; i++) {
      const startAngle = i * (arcAngle + gapAngle);
      const endAngle = startAngle + arcAngle;
      arcs.push({ startAngle, endAngle });
    }
    return arcs;
  }, [gapCount, gapWidth]);
  
  return (
    <group name="cyan-middle-wall">
      {arcSegments.map((arc, idx) => {
        const geometry = createCurvedWallGeometry(
          innerRadius,
          outerRadius,
          height,
          arc.startAngle,
          arc.endAngle,
          Math.max(16, Math.ceil((arc.endAngle - arc.startAngle) / 0.08))
        );
        
        return (
          <mesh key={`cyan-arc-${idx}`} geometry={geometry} castShadow receiveShadow>
            <meshStandardMaterial
              color="#00FFFF"
              roughness={0.3}
              metalness={0.5}
              emissive={new THREE.Color("#00FFFF")}
              emissiveIntensity={0.3}
              side={THREE.DoubleSide}
            />
          </mesh>
        );
      })}
    </group>
  );
}

// Rainbow animated material for inner walls
function RainbowAnimatedMaterial({ 
  wallIndex, 
  isSelected, 
  isOccluded 
}: { 
  wallIndex: number; 
  isSelected: boolean; 
  isOccluded: boolean;
}) {
  const materialRef = useRef<THREE.MeshStandardMaterial>(null);
  const timeRef = useRef(0);
  
  // Create a stable rainbow color based on wall index with time animation
  useFrame((state, delta) => {
    if (!materialRef.current) return;
    timeRef.current += delta;
    
    // Animate hue based on time + wall index offset for variety
    const hueOffset = wallIndex * 0.25; // Different starting hue per wall
    const animatedHue = (timeRef.current * 0.1 + hueOffset) % 1;
    
    // Create vibrant rainbow color
    const color = new THREE.Color();
    color.setHSL(animatedHue, 0.9, 0.55);
    
    materialRef.current.color = color;
    materialRef.current.emissive = color;
    materialRef.current.emissiveIntensity = 0.4 + Math.sin(timeRef.current * 2) * 0.1;
  });
  
  if (isSelected) {
    return (
      <meshStandardMaterial 
        color="#FFD700"
        roughness={0.3}
        metalness={0.5}
        emissive={new THREE.Color("#FFD700")}
        emissiveIntensity={0.8}
        side={THREE.DoubleSide}
        transparent={isOccluded}
        opacity={isOccluded ? 0.25 : 1}
      />
    );
  }
  
  return (
    <meshStandardMaterial
      ref={materialRef}
      color="#ff0000"
      roughness={0.3}
      metalness={0.5}
      emissive={new THREE.Color("#ff0000")}
      emissiveIntensity={0.4}
      side={THREE.DoubleSide}
      transparent={isOccluded}
      opacity={isOccluded ? 0.25 : 1}
    />
  );
}

export function InnerBarrierWalls({
  enabled = true,
  wallHeight = 3.0,
  wallThickness = 0.8,
  barrierRadius = ARENA_RADIUS * 0.70,
  middleWallRadius = ARENA_RADIUS + 2,
  theme = "grass",
}: InnerBarrierWallsProps) {
  const wallOwnershipMode = useZoogiGame(state => state.wallOwnershipMode);
  const gameMode = useZoogiGame(state => state.gameMode);
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const orbs = useZoogiGame(state => state.orbs);
  const updateOrb = useZoogiGame(state => state.updateOrb);
  const innerWallSegmentConfigs = useZoogiGame(state => state.innerWallSegmentConfigs);
  const setInnerWallSegmentConfigs = useZoogiGame(state => state.setInnerWallSegmentConfigs);
  const selectedWallSegmentId = useZoogiGame(state => state.selectedWallSegmentId);
  const selectWallSegment = useZoogiGame(state => state.selectWallSegment);
  const selectedInnerWallSegmentId = useZoogiGame(state => state.selectedInnerWallSegmentId);
  const selectInnerWallSegment = useZoogiGame(state => state.selectInnerWallSegment);
  const updateInnerWallSegmentConfig = useZoogiGame(state => state.updateInnerWallSegmentConfig);
  // Inner walls now appear in all game modes
  const shouldSpawn = true;
  const isMapEditor = gameMode === "map_editor";
  const { world, isInitialized, createKinematicBox, createDynamicBox, removeBody, getBody, getPosition, getRotation, applyImpulse, setAngularVelocity } = usePhysicsWorld();
  const [hoveredSegmentId, setHoveredSegmentId] = useState<string | null>(null);
  
  const physicsCreated = useRef(false);
  const [wallSegmentState, setWallSegmentState] = useState<WallSegment[]>([]);
  const segmentPhysicsIds = useRef<Map<string, string>>(new Map());
  const lastCollisionCheck = useRef<number>(0);
  const segmentHitCooldown = useRef<Map<string, number>>(new Map());
  const ZOOGI_MASS = 2.0;
  const segmentGroupRefs = useRef<Map<string, THREE.Group>>(new Map());
  
  // Inner rotating walls - spin but don't break (walls are now solid)
  const [rotatingWalls, setRotatingWalls] = useState<RotatingWall[]>([]);
  const rotatingWallRefs = useRef<Map<string, THREE.Group>>(new Map());
  const rotationStateRef = useRef<Map<string, { currentRotation: number; angularVelocity: number }>>(new Map());
  const [occludedMeshes, setOccludedMeshes] = useState<Set<string>>(new Set());
  const { camera } = useThree();
  const raycasterRef = useRef(new THREE.Raycaster());
  const wallGroupRef = useRef<THREE.Group>(null);
  
  const wallSegments = useMemo(() => {
    if (!enabled) return [];
    
    const segments: { id: string; startAngle: number; endAngle: number }[] = [];
    const gapWidth = Math.PI / 12;
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
        id: `inner-curved-wall-${i}`,
        startAngle: gapEnd,
        endAngle: endAngle,
      });
    }
    
    return segments;
  }, [enabled]);

  useEffect(() => {
    if (wallSegments.length > 0) {
      const needsSync = innerWallSegmentConfigs.length !== wallSegments.length ||
        innerWallSegmentConfigs.some((cfg, idx) => {
          const seg = wallSegments[idx];
          return seg && (Math.abs(cfg.startAngle - seg.startAngle) > 0.01 || Math.abs(cfg.endAngle - seg.endAngle) > 0.01);
        });
      
      if (needsSync) {
        const configs = wallSegments.map((seg, idx) => {
          // Preserve existing visibility/offset from loaded configs
          const existingConfig = innerWallSegmentConfigs.find(c => c.id === seg.id);
          return {
            id: seg.id,
            segmentIndex: idx,
            visible: existingConfig?.visible ?? true,
            positionOffset: existingConfig?.positionOffset ?? { x: 0, y: 0, z: 0 },
            startAngle: seg.startAngle,
            endAngle: seg.endAngle
          };
        });
        setInnerWallSegmentConfigs(configs);
        console.log("Synced inner wall segment configs (preserved visibility)");
      }
    }
  }, [wallSegments, innerWallSegmentConfigs, setInnerWallSegmentConfigs]);

  const getInnerSegmentConfig = (segmentId: string) => {
    return innerWallSegmentConfigs.find(c => c.id === segmentId);
  };

  const innerRadius = barrierRadius - wallThickness / 2;
  const outerRadius = barrierRadius + wallThickness / 2;
  const midRadius = (innerRadius + outerRadius) / 2;

  useEffect(() => {
    // Clean up physics bodies when disabled
    if (!enabled) {
      segmentPhysicsIds.current.forEach((physicsId) => {
        removeBody(physicsId);
      });
      segmentPhysicsIds.current.clear();
      physicsCreated.current = false;
      setWallSegmentState([]);
      setRotatingWalls([]);
      rotationStateRef.current.clear();
      rotatingWallRefs.current.clear();
      segmentHitCooldown.current.clear();
      console.log("InnerBarrierWalls disabled - cleaned up all physics bodies and rotation state");
      return;
    }
    
    if (!shouldSpawn || !isInitialized || !world) return;
    if (!isRapierReady()) return;
    
    // Always clear existing physics bodies first
    segmentPhysicsIds.current.forEach((physicsId) => {
      removeBody(physicsId);
    });
    segmentPhysicsIds.current.clear();
    
    // Wait for configs to sync
    if (wallSegments.length > 0 && innerWallSegmentConfigs.length !== wallSegments.length) {
      console.log("Waiting for inner wall configs to sync...", innerWallSegmentConfigs.length, "vs", wallSegments.length);
      return;
    }

    physicsCreated.current = true;
    
    // Create ONE physics body per segment (4 solid segments total)
    const segments: WallSegment[] = [];
    
    wallSegments.forEach((segment, segIdx) => {
      const segmentConfig = getInnerSegmentConfig(segment.id);
      const isVisible = segmentConfig ? segmentConfig.visible : true;
      const offset = segmentConfig?.positionOffset || { x: 0, y: 0, z: 0 };
      
      const centerAngle = (segment.startAngle + segment.endAngle) / 2;
      const x = Math.cos(centerAngle) * midRadius + offset.x;
      const z = Math.sin(centerAngle) * midRadius + offset.z;
      
      // Calculate segment arc length for physics box
      const angleRange = segment.endAngle - segment.startAngle;
      const arcLength = angleRange * midRadius;
      
      const quat = new THREE.Quaternion();
      quat.setFromAxisAngle(new THREE.Vector3(0, 1, 0), -centerAngle + Math.PI / 2);
      
      segments.push({
        id: segment.id,
        segmentIndex: segIdx,
        centerAngle,
        startAngle: segment.startAngle,
        endAngle: segment.endAngle,
        position: [x, wallHeight / 2 + offset.y, z],
        rotation: { w: quat.w, x: quat.x, y: quat.y, z: quat.z },
      });
      
      // Create ONE dynamic physics body per segment
      if (isVisible) {
        const physicsId = `inner-wall-segment-physics-${segIdx}`;
        const body = createDynamicBox(
          physicsId,
          [x, wallHeight / 2 + offset.y, z],
          [arcLength, wallHeight, wallThickness * 1.5],
          { w: quat.w, x: quat.x, y: quat.y, z: quat.z },
          { 
            restitution: 0.6, 
            friction: 0.5,
            mass: 25.0, // Heavier for full segment
            linearDamping: 0.8,
            angularDamping: 0.5
          }
        );
        
        if (body) {
          segmentPhysicsIds.current.set(segment.id, physicsId);
        }
      }
    });
    
    setWallSegmentState(segments);
    
    // Also initialize the rotating walls array (for inner layer spin behavior)
    const rotWalls: RotatingWall[] = wallSegments.map((segment, idx) => ({
      id: segment.id,
      segmentIndex: idx,
      centerAngle: (segment.startAngle + segment.endAngle) / 2,
      startAngle: segment.startAngle,
      endAngle: segment.endAngle,
      angularVelocity: 0,
      currentRotation: 0
    }));
    setRotatingWalls(rotWalls);
    
    // Initialize the ref-based rotation state for performance
    rotWalls.forEach(wall => {
      rotationStateRef.current.set(wall.id, {
        currentRotation: 0,
        angularVelocity: 0
      });
    });
    
    console.log(`Created ${segments.length} inner ROTATING wall segments at radius ${midRadius.toFixed(2)}`);

    return () => {
      segmentPhysicsIds.current.forEach((physicsId) => {
        removeBody(physicsId);
      });
      segmentPhysicsIds.current.clear();
      physicsCreated.current = false;
      setWallSegmentState([]);
      setRotatingWalls([]);
      rotationStateRef.current.clear();
      rotatingWallRefs.current.clear();
      segmentHitCooldown.current.clear();
    };
  }, [enabled, shouldSpawn, isInitialized, world, wallSegments, midRadius, wallHeight, wallThickness, createDynamicBox, removeBody, innerWallSegmentConfigs]);


  // Function to spin a rotating wall when hit off-center
  const spinRotatingWall = useCallback((wallId: string, torque: number) => {
    // Update the ref directly (for performance - no re-renders)
    const stateEntry = rotationStateRef.current.get(wallId);
    if (stateEntry) {
      rotationStateRef.current.set(wallId, {
        ...stateEntry,
        angularVelocity: stateEntry.angularVelocity + torque
      });
    }
  }, []);

  const lastPlayerRadius = useRef<number>(0);
  const lastEnemyRadii = useRef<Map<string, number>>(new Map());
  const lastOrbRadii = useRef<Map<string, number>>(new Map());
  const orbBounceCooldown = useRef<Map<string, number>>(new Map());
  
  // Helper to check if entity is within a pivoting wall segment
  // Transforms entity position into wall-local space for accurate collision detection
  const isEntityInPivotingWallSegment = useCallback((
    entityX: number,
    entityZ: number,
    segment: { startAngle: number; endAngle: number },
    rotationOffset: number,
    wallMidRadius: number, // Use the same midRadius as wall placement for perfect alignment
    checkThickness: boolean = false, // Also check radial distance to wall
    wallThicknessVal: number = 1.0
  ): boolean => {
    // Calculate the arc center (pivot point) for this segment - must match rendering
    const arcCenterAngle = (segment.startAngle + segment.endAngle) / 2;
    const arcCenterX = Math.cos(arcCenterAngle) * wallMidRadius;
    const arcCenterZ = Math.sin(arcCenterAngle) * wallMidRadius;
    
    // Transform entity position relative to pivot point
    const relX = entityX - arcCenterX;
    const relZ = entityZ - arcCenterZ;
    
    // Apply inverse rotation to get entity in wall-local space
    const cosR = Math.cos(-rotationOffset);
    const sinR = Math.sin(-rotationOffset);
    const localX = relX * cosR - relZ * sinR;
    const localZ = relX * sinR + relZ * cosR;
    
    // Transform back to arena coordinates (in wall's original unrotated frame)
    const unrotatedX = localX + arcCenterX;
    const unrotatedZ = localZ + arcCenterZ;
    
    // Check radial distance in unrotated frame
    const unrotatedRadius = Math.sqrt(unrotatedX * unrotatedX + unrotatedZ * unrotatedZ);
    
    // If checking thickness, ensure entity is within wall ring in the transformed space
    if (checkThickness) {
      const ZOOGI_RADIUS = 0.5;
      const innerEdge = wallMidRadius - wallThicknessVal / 2 - ZOOGI_RADIUS;
      const outerEdge = wallMidRadius + wallThicknessVal / 2 + ZOOGI_RADIUS;
      if (unrotatedRadius < innerEdge || unrotatedRadius > outerEdge) {
        return false;
      }
    }
    
    // Now check if this unrotated position is within the segment's original angle bounds
    const entityAngle = Math.atan2(unrotatedZ, unrotatedX);
    const normalizedAngle = entityAngle < 0 ? entityAngle + Math.PI * 2 : entityAngle;
    
    let startAngle = segment.startAngle;
    let endAngle = segment.endAngle;
    
    // Normalize to [0, 2π]
    while (startAngle < 0) startAngle += Math.PI * 2;
    while (endAngle < 0) endAngle += Math.PI * 2;
    startAngle = startAngle % (Math.PI * 2);
    endAngle = endAngle % (Math.PI * 2);
    
    // Handle wrap-around
    if (endAngle < startAngle) endAngle += Math.PI * 2;
    let checkAngle = normalizedAngle;
    if (checkAngle < startAngle) checkAngle += Math.PI * 2;
    
    return checkAngle >= startAngle && checkAngle <= endAngle;
  }, []);
  
  const handleWallCollision = useCallback((
    entityPos: [number, number, number],
    entityVel: [number, number, number],
    prevRadius: number,
    isPlayer: boolean,
    enemyId?: string
  ) => {
    const ZOOGI_RADIUS = 0.5;
    const px = entityPos[0];
    const pz = entityPos[2];
    const currentRadius = Math.sqrt(px * px + pz * pz);
    
    // Check if entity is in a VISIBLE segment (accounting for pivot rotation AND wall thickness)
    // This properly detects collision with rotated wall geometry
    let isInVisibleSegment = false;
    let collidingRotationOffset = 0;
    for (const seg of wallSegments) {
      const cfg = innerWallSegmentConfigs.find(c => c.id === seg.id);
      const isVisible = cfg?.visible ?? true;
      if (isVisible) {
        // Get rotation offset from ref
        const stateEntry = rotationStateRef.current.get(seg.id);
        const rotationOffset = stateEntry?.currentRotation || 0;
        
        // Use proper pivot-aware collision detection with thickness check
        if (isEntityInPivotingWallSegment(px, pz, seg, rotationOffset, midRadius, true, wallThickness)) {
          isInVisibleSegment = true;
          collidingRotationOffset = rotationOffset;
          break;
        }
      }
    }
    
    // Only block if in a visible segment - gaps and hidden segments allow passage
    if (!isInVisibleSegment) return currentRadius;
    
    const angle = Math.atan2(pz, px);
    const normalizedAngle = angle < 0 ? angle + Math.PI * 2 : angle;
    
    // Walls are now solid - no broken segments to check
    
    const normalX = px / currentRadius;
    const normalZ = pz / currentRadius;
    const radialVel = entityVel[0] * normalX + entityVel[2] * normalZ;
    
    // Determine if entity is approaching from inside (moving outward) or outside (moving inward)
    const isApproachingFromInside = radialVel > 0;
    const isApproachingFromOutside = radialVel < 0;
    
    const speed = Math.sqrt(entityVel[0] ** 2 + entityVel[2] ** 2);
    const impactForce = Math.max(Math.abs(radialVel), speed * 0.5) * ZOOGI_MASS;
    
    const feedbackIntensity = Math.min(1.0, impactForce / 4.0);
    if (feedbackIntensity > 0.1) {
      triggerCollisionCameraEffect(feedbackIntensity);
      triggerWallHitFeel(feedbackIntensity);
    }
    
    // Find the segment the entity hit (accounting for pivot rotation)
    let segmentToHit: WallSegment | null = null;
    let wallRotationState: { currentRotation: number; angularVelocity: number } | null = null;
    for (const segment of wallSegmentState) {
      // Get the rotating wall's current rotation offset from ref (for performance)
      const stateEntry = rotationStateRef.current.get(segment.id);
      const rotationOffset = stateEntry?.currentRotation || 0;
      
      // Use proper pivot-aware collision detection with thickness
      if (isEntityInPivotingWallSegment(px, pz, segment, rotationOffset, midRadius, true, wallThickness)) {
        segmentToHit = segment;
        wallRotationState = stateEntry || null;
        break;
      }
    }
    
    const store = useZoogiGame.getState();
    // Clamp to appropriate side based on approach direction (using midRadius since walls rotate around their centers)
    const wallInnerEdge = midRadius - wallThickness / 2 - ZOOGI_RADIUS;
    const wallOuterEdge = midRadius + wallThickness / 2 + ZOOGI_RADIUS;
    const clampedRadius = isApproachingFromOutside 
      ? wallOuterEdge + 0.3  // Push outward if coming from outside
      : wallInnerEdge - 0.3; // Push inward if coming from inside
    
    // Inner walls SPIN when hit - calculate impact offset from center
    if (segmentToHit) {
      // Use the rotated center angle for proper impact offset calculation
      const rotationOffset = wallRotationState?.currentRotation || 0;
      const rotatedCenterAngle = ((segmentToHit.startAngle + segmentToHit.endAngle) / 2) + rotationOffset;
      const arcHalfSpan = (segmentToHit.endAngle - segmentToHit.startAngle) / 2;
      
      // Calculate impact offset in the rotated frame
      let impactOffset = normalizedAngle - rotatedCenterAngle;
      // Normalize to [-π, π]
      while (impactOffset > Math.PI) impactOffset -= Math.PI * 2;
      while (impactOffset < -Math.PI) impactOffset += Math.PI * 2;
      
      const normalizedOffset = Math.abs(impactOffset) / arcHalfSpan;
      
      // Inner walls spin easily - lower threshold and higher torque multiplier
      const torqueDirection = impactOffset > 0 ? 1 : -1;
      // Increased spin: 1.5x force multiplier and higher base spin (0.5 instead of 0.3)
      const torqueAmount = impactForce * 1.5 * (0.5 + normalizedOffset * 0.8); // Spins much more easily
      spinRotatingWall(segmentToHit.id, torqueDirection * torqueAmount);
      console.log(`Inner wall SPINNING: offset=${normalizedOffset.toFixed(2)}, torque=${(torqueDirection * torqueAmount).toFixed(2)}`);
    }
    
    // Bounce the entity (walls are solid - never break)
    {
      const bounceFactor = 0.8; // Solid bounce strength
      const bounceVelX = entityVel[0] - 2 * radialVel * normalX * bounceFactor;
      const bounceVelZ = entityVel[2] - 2 * radialVel * normalZ * bounceFactor;
      
      const clampedX = normalX * clampedRadius;
      const clampedZ = normalZ * clampedRadius;
      
      if (isPlayer) {
        store.updatePlayerVelocity([bounceVelX, entityVel[1], bounceVelZ]);
        store.updatePlayerPosition([clampedX, entityPos[1], clampedZ]);
      } else if (enemyId) {
        store.updateEnemy(enemyId, {
          velocity: [bounceVelX, entityVel[1], bounceVelZ],
          position: [clampedX, entityPos[1], clampedZ]
        });
      }
      
      console.log(`Wall bounce: force ${impactForce.toFixed(2)} (solid wall)`);
    }
    
    return clampedRadius;
  }, [barrierRadius, midRadius, wallSegmentState, triggerCollisionCameraEffect, triggerWallHitFeel, wallSegments, innerWallSegmentConfigs, isEntityInPivotingWallSegment]);
  
  useFrame((state, delta) => {
    // Update ROTATING WALLS - apply angular velocity and damping (using ref to avoid re-renders)
    rotatingWalls.forEach(wall => {
      const stateEntry = rotationStateRef.current.get(wall.id);
      if (!stateEntry) return;
      
      const newRotation = stateEntry.currentRotation + stateEntry.angularVelocity * delta;
      const dampedVelocity = stateEntry.angularVelocity * 0.95;
      
      // Update ref (no re-render)
      rotationStateRef.current.set(wall.id, {
        currentRotation: newRotation,
        angularVelocity: Math.abs(dampedVelocity) < 0.01 ? 0 : dampedVelocity
      });
      
      // Update the visual rotation of the wall
      const group = rotatingWallRefs.current.get(wall.id);
      if (group) {
        group.rotation.y = newRotation;
      }
    });
    
    // Sync wall segment visual positions with physics bodies
    wallSegmentState.forEach(segment => {
      const physicsId = segmentPhysicsIds.current.get(segment.id);
      if (!physicsId) return;
      
      const pos = getPosition(physicsId);
      const rot = getRotation(physicsId);
      if (!pos || !rot) return;
      
      // Directly update group transform (no React re-render)
      const group = segmentGroupRefs.current.get(segment.id);
      if (group) {
        group.position.set(pos.x, pos.y, pos.z);
        group.quaternion.set(rot.x, rot.y, rot.z, rot.w);
      }
    });
    
    // Collision detection enabled for Rapier physics walls
    if (!enabled || !shouldSpawn || wallSegmentState.length === 0) return;
    
    const now = Date.now();
    
    if (playerEntity && !playerEntity.isKnockedOut) {
      const prevRadius = lastPlayerRadius.current;
      const newRadius = handleWallCollision(
        playerEntity.position,
        playerEntity.velocity,
        prevRadius,
        true
      );
      lastPlayerRadius.current = newRadius;
    }
    
    enemies.forEach(enemy => {
      if (!enemy.isKnockedOut && enemy.id) {
        const prevRadius = lastEnemyRadii.current.get(enemy.id) || 0;
        const newRadius = handleWallCollision(
          enemy.position,
          enemy.velocity,
          prevRadius,
          false,
          enemy.id
        );
        lastEnemyRadii.current.set(enemy.id, newRadius);
      }
    });
    
    const ORB_RADIUS = 0.4;
    
    orbs.forEach(orb => {
      if (!orb.isActive) return;
      
      const lastCooldown = orbBounceCooldown.current.get(orb.id) || 0;
      if (now - lastCooldown < 50) return; // Reduced cooldown for better detection
      
      const ox = orb.position[0];
      const oz = orb.position[2];
      const currentOrbRadius = Math.sqrt(ox * ox + oz * oz);
      const orbSpeed = Math.sqrt(orb.velocity[0] ** 2 + orb.velocity[2] ** 2);
      
      // Use pivot-aware collision detection for orbs - same as players
      // Check if orb is in ANY visible segment's rotated collision zone
      let isOrbInVisibleSegment = false;
      let collidingSegment: { startAngle: number; endAngle: number } | null = null;
      let collidingRotationOffset = 0;
      
      for (const seg of wallSegments) {
        const cfg = innerWallSegmentConfigs.find(c => c.id === seg.id);
        const isVisible = cfg?.visible ?? true;
        if (isVisible) {
          // Get rotation offset from ref
          const stateEntry = rotationStateRef.current.get(seg.id);
          const rotationOffset = stateEntry?.currentRotation || 0;
          
          // Use proper pivot-aware collision detection with thickness (adjusted for orb radius)
          if (isEntityInPivotingWallSegment(ox, oz, seg, rotationOffset, midRadius, true, wallThickness + ORB_RADIUS * 2)) {
            isOrbInVisibleSegment = true;
            collidingSegment = seg;
            collidingRotationOffset = rotationOffset;
            break;
          }
        }
      }
      
      // If orb is not in any visible wall segment, let it pass freely
      if (!isOrbInVisibleSegment) {
        lastOrbRadii.current.set(orb.id, currentOrbRadius);
        return;
      }
      
      // Walls are solid - no broken segments to check
      
      const angle = Math.atan2(oz, ox);
      const safeRadius = Math.max(currentOrbRadius, 0.1);
      const normalX = ox / safeRadius;
      const normalZ = oz / safeRadius;
      const radialVel = orb.velocity[0] * normalX + orb.velocity[2] * normalZ;
      const normalizedOrbAngle = angle < 0 ? angle + Math.PI * 2 : angle;
      
      // Orb is in a visible segment - handle collision (walls are solid, never break)
      {
        // Calculate impact force from orb
        const ORB_MASS = 0.5;
        const orbImpactForce = Math.max(Math.abs(radialVel), orbSpeed * 0.5) * ORB_MASS;
        
        // Wall vibration effect on orb impact
        const feedbackIntensity = Math.min(1.0, orbImpactForce / 2.0);
        if (feedbackIntensity > 0.05) {
          triggerCollisionCameraEffect(feedbackIntensity * 0.5);
          triggerWallHitFeel(feedbackIntensity * 0.3);
        }
        
        // Walls are solid - just bounce the orb
        const bounceFactor = 0.8; // Solid bounce strength
        const minBounceVelocity = 1.5;
        
        // For slow orbs, ensure minimum bounce velocity away from wall
        const effectiveRadialVel = Math.max(Math.abs(radialVel), minBounceVelocity);
        const bounceVelX = orb.velocity[0] - 2 * effectiveRadialVel * normalX * bounceFactor;
        const bounceVelZ = orb.velocity[2] - 2 * effectiveRadialVel * normalZ * bounceFactor;
        
        // Clamp orb to inner edge of wall ring (midRadius - wallThickness/2 - margin)
        const clampedOrbRadius = midRadius - wallThickness / 2 - ORB_RADIUS - 0.3;
        const clampedOrbX = normalX * clampedOrbRadius;
        const clampedOrbZ = normalZ * clampedOrbRadius;
        
        updateOrb(orb.id, {
          velocity: [bounceVelX, orb.velocity[1], bounceVelZ],
          position: [clampedOrbX, orb.position[1], clampedOrbZ]
        });
        
        console.log(`Orb bounced off inner wall at radius ${currentOrbRadius.toFixed(2)}, clamped to ${clampedOrbRadius.toFixed(2)}`);
        orbBounceCooldown.current.set(orb.id, now);
        lastOrbRadii.current.set(orb.id, clampedOrbRadius);
      }
    });
    
    // Walls are now solid - no scattered blocks to update
    
    // Camera occlusion detection using raycasting
    if (playerEntity && camera && wallGroupRef.current) {
      const camPos = camera.position;
      const playerPos = new THREE.Vector3(...playerEntity.position);
      const direction = playerPos.clone().sub(camPos).normalize();
      const distance = camPos.distanceTo(playerPos);
      
      raycasterRef.current.set(camPos, direction);
      raycasterRef.current.far = distance;
      
      const meshChildren = wallGroupRef.current.children.filter(
        (child): child is THREE.Mesh => child instanceof THREE.Mesh && child.name.startsWith('wall-mesh-')
      );
      
      const intersects = raycasterRef.current.intersectObjects(meshChildren, false);
      
      const newOccluded = new Set<string>();
      intersects.forEach(hit => {
        if (hit.object.name.startsWith('wall-mesh-')) {
          newOccluded.add(hit.object.name);
        }
      });
      
      setOccludedMeshes(prev => {
        if (prev.size !== newOccluded.size || Array.from(prev).some(s => !newOccluded.has(s))) {
          return newOccluded;
        }
        return prev;
      });
    }
  });

  // intactGeometries now just creates geometry for each intact segment
  const intactGeometries = useMemo(() => {
    if (!enabled || wallSegments.length === 0 || wallSegmentState.length === 0) return [];
    
    const result: { id: string; geometry: THREE.BufferGeometry; segmentIndex: number }[] = [];
    
    wallSegmentState.forEach((segment, segIdx) => {
      const segmentConfig = getInnerSegmentConfig(segment.id);
      if (segmentConfig && !segmentConfig.visible) return;
      
      // Create geometry for the entire segment
      result.push({
        id: `intact-${segment.id}`,
        geometry: createCurvedWallGeometry(innerRadius, outerRadius, wallHeight, segment.startAngle, segment.endAngle, 16),
        segmentIndex: segment.segmentIndex
      });
    });
    
    return result;
  }, [enabled, wallSegments, wallSegmentState, innerRadius, outerRadius, wallHeight, getInnerSegmentConfig]);


  if (!enabled || !shouldSpawn || wallSegments.length === 0) return null;

  const handleSegmentClick = (segmentId: string, e: any) => {
    if (gameMode !== "map_editor") return;
    e.stopPropagation();
    selectInnerWallSegment(segmentId);
  };

  return (
    <group name="inner-barrier-walls" ref={wallGroupRef}>
      {/* INNER ROTATING WALLS - 4 solid curved arcs that spin but don't break */}
      {rotatingWalls.map((wall, idx) => {
        const segmentConfig = getInnerSegmentConfig(wall.id);
        const isVisible = segmentConfig?.visible ?? true;
        const isSelected = !!(segmentConfig && selectedInnerWallSegmentId === segmentConfig.id);
        const meshName = `rotating-wall-${wall.id}`;
        const isOccluded = occludedMeshes.has(meshName);
        
        if (!isVisible) return null;
        
        // Create curved geometry centered at its own origin for proper pivot rotation
        const geometry = createCurvedWallGeometry(
          innerRadius, outerRadius, wallHeight,
          wall.startAngle, wall.endAngle, 16, true // centerAtOrigin=true
        );
        
        // Calculate the arc center position to place the group
        const arcCenterAngle = (wall.startAngle + wall.endAngle) / 2;
        const arcMidRadius = (innerRadius + outerRadius) / 2;
        const arcCenterX = Math.cos(arcCenterAngle) * arcMidRadius;
        const arcCenterZ = Math.sin(arcCenterAngle) * arcMidRadius;
        
        return (
          <group
            key={wall.id}
            position={[arcCenterX, 0, arcCenterZ]}
            ref={(group) => {
              if (group) {
                rotatingWallRefs.current.set(wall.id, group);
                const stateEntry = rotationStateRef.current.get(wall.id);
                if (stateEntry) {
                  group.rotation.y = stateEntry.currentRotation;
                }
              } else {
                rotatingWallRefs.current.delete(wall.id);
              }
            }}
          >
            <mesh
              name={meshName}
              geometry={geometry}
              castShadow
              receiveShadow
              onClick={(e) => handleSegmentClick(wall.id, e)}
              onPointerOver={() => gameMode === "map_editor" && (document.body.style.cursor = "pointer")}
              onPointerOut={() => gameMode === "map_editor" && (document.body.style.cursor = "default")}
            >
              <RainbowAnimatedMaterial 
                wallIndex={idx}
                isSelected={isSelected}
                isOccluded={isOccluded}
              />
            </mesh>
          </group>
        );
      })}
      
      {/* SOLID CYAN MIDDLE WALL - positioned using middleWallRadius from Middle Wall settings */}
      <CyanMiddleWallRing 
        innerRadius={middleWallRadius}
        outerRadius={middleWallRadius + wallThickness}
        height={wallHeight}
        gapCount={4}
        gapWidth={0.4}
      />
    </group>
  );
}
