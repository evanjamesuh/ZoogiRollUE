import { useEffect, useRef, useMemo, useState } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { useTexture, useVideoTexture } from "@react-three/drei";
import { ARENA_RADIUS } from "@/lib/arenaConstants";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

interface DestructibleRingWallProps {
  enabled?: boolean;
  wallHeight?: number;
  wallThickness?: number;
  wallRadius?: number;
  theme?: "grass" | "ice" | "lava" | "space" | "saturn";
}

const SEGMENTS_PER_RING = 24;

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

    uvs.push(u * 4, 0);
    uvs.push(u * 4, 1);
    uvs.push(u * 4, 0);
    uvs.push(u * 4, 1);

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

    const innerX = cosA * innerRadius;
    const innerZ = sinA * innerRadius;
    const outerX = cosA * outerRadius;
    const outerZ = sinA * outerRadius;

    const u = i / segments;

    vertices.push(innerX, height, innerZ);
    vertices.push(outerX, height, outerZ);

    normals.push(0, 1, 0);
    normals.push(0, 1, 0);

    uvs.push(u * 4, 0);
    uvs.push(u * 4, 1);

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

function SwivelWallRing({ 
  radius, 
  thickness, 
  height, 
  rotationSpeed, 
  rotationOffset,
  segments,
  gapCount,
  gapWidth,
  textures
}: { 
  radius: number; 
  thickness: number; 
  height: number; 
  rotationSpeed: number;
  rotationOffset: number;
  segments: number;
  gapCount: number;
  gapWidth: number;
  textures: { baseColor: THREE.Texture; normal: THREE.Texture; metallicRoughness: THREE.Texture };
}) {
  const groupRef = useRef<THREE.Group>(null);
  const [rotation, setRotation] = useState(rotationOffset);
  
  useFrame((_, delta) => {
    setRotation(prev => prev + rotationSpeed * delta);
  });
  
  const innerRadius = radius - thickness / 2;
  const outerRadius = radius + thickness / 2;
  
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
  
  // Memoize geometries based on radius and arc segments
  const geometries = useMemo(() => {
    return arcSegments.map((arc) => 
      createCurvedWallGeometry(
        innerRadius,
        outerRadius,
        height,
        arc.startAngle,
        arc.endAngle,
        Math.max(16, Math.ceil((arc.endAngle - arc.startAngle) / 0.08))
      )
    );
  }, [arcSegments, innerRadius, outerRadius, height]);
  
  return (
    <group ref={groupRef} rotation-y={rotation}>
      {geometries.map((geometry, idx) => (
        <mesh key={`arc-${idx}`} geometry={geometry} castShadow receiveShadow>
          <meshStandardMaterial
            map={textures.baseColor}
            normalMap={textures.normal}
            metalnessMap={textures.metallicRoughness}
            roughnessMap={textures.metallicRoughness}
            metalness={0.7}
            roughness={0.4}
            side={THREE.DoubleSide}
          />
        </mesh>
      ))}
    </group>
  );
}

function OuterVideoWall({
  radius,
  thickness,
  height
}: {
  radius: number;
  thickness: number;
  height: number;
}) {
  const innerRadius = radius - thickness / 2;
  const outerRadius = radius + thickness / 2;
  
  const [currentVideo, setCurrentVideo] = useState(0);
  const [flickerIntensity, setFlickerIntensity] = useState(1);
  const [glitchActive, setGlitchActive] = useState(false);
  const [transitionProgress, setTransitionProgress] = useState(0);
  const [isTransitioning, setIsTransitioning] = useState(false);
  const transitionStartTime = useRef(0);
  const lastSwitchTime = useRef(0);
  const flickerTimeRef = useRef(0);
  
  const videoTexture1 = useVideoTexture("/videos/outer_ring_1.mp4", { loop: true, muted: true, start: true });
  const videoTexture2 = useVideoTexture("/videos/outer_ring_2.mp4", { loop: true, muted: true, start: true });
  const videoTexture3 = useVideoTexture("/videos/outer_ring_3.mp4", { loop: true, muted: true, start: true });
  const videoTexture4 = useVideoTexture("/videos/outer_ring_4.mp4", { loop: true, muted: true, start: true });
  
  const videoTextures = useMemo(() => [videoTexture1, videoTexture2, videoTexture3, videoTexture4], [videoTexture1, videoTexture2, videoTexture3, videoTexture4]);
  
  useEffect(() => {
    videoTextures.forEach(tex => {
      tex.wrapS = THREE.RepeatWrapping;
      tex.wrapT = THREE.RepeatWrapping;
      tex.repeat.set(6, 1);
    });
  }, [videoTextures]);
  
  const nextSwitchInterval = useRef(5000);
  const glitchEndTime = useRef(0);
  const glitchOpacity = useRef(1);
  
  useFrame((_, delta) => {
    const now = Date.now();
    
    flickerTimeRef.current += delta;
    const baseFlicker = 0.85 + Math.sin(flickerTimeRef.current * 15) * 0.08;
    const noiseFlicker = 0.9 + Math.sin(flickerTimeRef.current * 47.3) * Math.sin(flickerTimeRef.current * 23.7) * 0.15;
    setFlickerIntensity(baseFlicker * noiseFlicker);
    
    const glitchNoise = Math.sin(flickerTimeRef.current * 127.3) * Math.sin(flickerTimeRef.current * 73.1);
    if (glitchNoise > 0.95 && !glitchActive) {
      setGlitchActive(true);
      glitchEndTime.current = now + 50 + Math.abs(Math.sin(flickerTimeRef.current * 31.7)) * 100;
      glitchOpacity.current = 0.7 + Math.abs(Math.sin(flickerTimeRef.current * 19.3)) * 0.3;
    }
    
    if (glitchActive && now > glitchEndTime.current) {
      setGlitchActive(false);
    }
    
    if (!isTransitioning && now - lastSwitchTime.current > nextSwitchInterval.current) {
      setIsTransitioning(true);
      transitionStartTime.current = now;
      lastSwitchTime.current = now;
      nextSwitchInterval.current = 5000 + Math.abs(Math.sin(flickerTimeRef.current * 11.3)) * 3000;
    }
    
    if (isTransitioning) {
      const elapsed = now - transitionStartTime.current;
      const duration = 800;
      const progress = Math.min(elapsed / duration, 1);
      setTransitionProgress(progress);
      
      if (progress >= 1) {
        setIsTransitioning(false);
        setCurrentVideo(prev => (prev + 1) % 4);
        setTransitionProgress(0);
      }
    }
  });
  
  const geometry = useMemo(() => 
    createCurvedWallGeometry(innerRadius, outerRadius, height, 0, Math.PI * 2, 64),
    [innerRadius, outerRadius, height]
  );
  
  const currentTexture = videoTextures[currentVideo];
  const nextTexture = videoTextures[(currentVideo + 1) % 4];
  
  const emissiveColor = useMemo(() => {
    const colors = [
      new THREE.Color(0.1, 0.3, 0.8),
      new THREE.Color(0.8, 0.2, 0.3),
      new THREE.Color(0.2, 0.8, 0.3),
      new THREE.Color(0.8, 0.6, 0.1)
    ];
    return colors[currentVideo];
  }, [currentVideo]);
  
  return (
    <group>
      <mesh geometry={geometry} castShadow receiveShadow>
        <meshStandardMaterial
          map={isTransitioning && transitionProgress > 0.5 ? nextTexture : currentTexture}
          emissive={emissiveColor}
          emissiveIntensity={flickerIntensity * 0.4 * (glitchActive ? 2.5 : 1)}
          metalness={0.3}
          roughness={0.5}
          side={THREE.DoubleSide}
          opacity={glitchActive ? glitchOpacity.current : 1}
          transparent={glitchActive}
        />
      </mesh>
      
      {glitchActive && (
        <mesh geometry={geometry} position-y={0.02}>
          <meshBasicMaterial
            color={new THREE.Color(0, 1, 1)}
            opacity={0.3}
            transparent
            side={THREE.DoubleSide}
          />
        </mesh>
      )}
      
      {isTransitioning && (
        <>
          {Array.from({ length: 8 }).map((_, i) => (
            <mesh 
              key={`scan-${i}`}
              geometry={geometry} 
              position-y={(transitionProgress * height * 2 - height) + (i * 0.3)}
              scale-y={0.02}
            >
              <meshBasicMaterial
                color={new THREE.Color(1, 1, 1)}
                opacity={0.6 * (1 - transitionProgress)}
                transparent
                side={THREE.DoubleSide}
              />
            </mesh>
          ))}
        </>
      )}
      
      <pointLight
        position={[0, height / 2, 0]}
        color={emissiveColor}
        intensity={flickerIntensity * 2}
        distance={15}
      />
    </group>
  );
}

export function DestructibleRingWall({
  enabled = true,
  wallHeight = 3.0,
  wallThickness = 0.6,
  wallRadius = ARENA_RADIUS + 0.5,
  theme = "grass",
}: DestructibleRingWallProps) {
  const gameMode = useZoogiGame(state => state.gameMode);
  const wallSettings = useZoogiGame(state => state.wallSettings);
  const elementTransforms = useZoogiGame(state => state.elementTransforms);
  
  const baseColorTex = useTexture("/textures/spaceship_hull_baseColor.png");
  const normalTex = useTexture("/textures/spaceship_hull_normal.png");
  const metallicRoughnessTex = useTexture("/textures/spaceship_hull_metallicRoughness.png");
  
  useEffect(() => {
    [baseColorTex, normalTex, metallicRoughnessTex].forEach(tex => {
      tex.wrapS = THREE.RepeatWrapping;
      tex.wrapT = THREE.RepeatWrapping;
      tex.repeat.set(8, 2);
    });
  }, [baseColorTex, normalTex, metallicRoughnessTex]);
  
  const textures = useMemo(() => ({
    baseColor: baseColorTex,
    normal: normalTex,
    metallicRoughness: metallicRoughnessTex
  }), [baseColorTex, normalTex, metallicRoughnessTex]);
  
  const pbrInnerEnabled = wallSettings?.pbrInnerWallEnabled ?? true;
  const pbrMiddleEnabled = wallSettings?.pbrMiddleWallEnabled ?? true;
  const videoEnabled = wallSettings?.videoWallEnabled ?? true;
  
  const pbrInnerOffset = wallSettings?.pbrInnerWallRadiusOffset ?? 18;
  const pbrMiddleOffset = wallSettings?.pbrMiddleWallRadiusOffset ?? 20;
  const videoOffset = wallSettings?.videoWallRadiusOffset ?? 22;
  const videoHeight = wallSettings?.videoWallHeight ?? 3;
  
  const innerWallRadius = ARENA_RADIUS + pbrInnerOffset;
  const middleWallRadius = ARENA_RADIUS + pbrMiddleOffset;
  const outerWallRadius = ARENA_RADIUS + videoOffset;
  
  const gapWidth = wallSettings?.middleWallGapWidth || 0.4;
  
  // Don't render anything if component is disabled AND all sub-walls are disabled
  const hasAnyWallEnabled = pbrInnerEnabled || pbrMiddleEnabled || videoEnabled;
  if (!enabled && !hasAnyWallEnabled) return null;
  
  return (
    <group name="solid-ring-walls">
      {pbrInnerEnabled && (
        <SwivelWallRing
          key={`pbr-inner-${innerWallRadius}`}
          radius={innerWallRadius}
          thickness={wallThickness}
          height={wallHeight}
          rotationSpeed={0.15}
          rotationOffset={0}
          segments={SEGMENTS_PER_RING}
          gapCount={4}
          gapWidth={gapWidth}
          textures={textures}
        />
      )}
      
      {pbrMiddleEnabled && (
        <SwivelWallRing
          key={`pbr-middle-${middleWallRadius}`}
          radius={middleWallRadius}
          thickness={wallThickness}
          height={wallHeight}
          rotationSpeed={-0.1}
          rotationOffset={Math.PI / 8}
          segments={SEGMENTS_PER_RING}
          gapCount={4}
          gapWidth={gapWidth}
          textures={textures}
        />
      )}
      
      {videoEnabled && (
        <group 
          rotation={[
            elementTransforms.outerWallRotation?.x ?? 0,
            elementTransforms.outerWallRotation?.y ?? 0,
            elementTransforms.outerWallRotation?.z ?? 0
          ]}
          position={[
            elementTransforms.outerWallOffset?.x ?? 0,
            elementTransforms.outerWallOffset?.y ?? 0,
            elementTransforms.outerWallOffset?.z ?? 0
          ]}
        >
          <OuterVideoWall
            key={`video-${outerWallRadius}`}
            radius={outerWallRadius}
            thickness={wallThickness}
            height={videoHeight}
          />
        </group>
      )}
    </group>
  );
}
