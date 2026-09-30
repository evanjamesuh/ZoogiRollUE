import { useRef, useEffect, useState } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useGameFeel } from "@/lib/stores/useGameFeel";
import { useCameraEffects } from "@/lib/stores/useCameraEffects";
import {
  ARENA_FOV_DEG,
  ARENA_PITCH,
  actionBounds,
  cameraOffset,
  clampDistance,
  matchCameraMax,
  damp,
  decayTrauma,
  fitDistance,
  getTrauma,
  smoothShake,
} from "@/lib/cameraRig";
import * as THREE from "three";
import { arenaScaleFor } from "@/lib/mapDefaultConfigs";
import { getMapLayout } from "@/lib/arenaColliders";
import { neonPlayHalfX, neonPlayHalfZ } from "@/lib/neonCourt";
import { launchPadsFor } from "@/lib/launchPads";

export function DeveloperCamera() {
  const developerDragActive = useZoogiGame((state) => state.developerDragActive);
  const isAiming = useZoogiGame((state) => state.isAiming);
  const controlsRef = useRef<any>(null);
  
  // Lock camera when aiming or when developer drag is active
  const cameraLocked = developerDragActive || isAiming;
  
  useEffect(() => {
    const updateTarget = () => {
      if (controlsRef.current) {
        (window as any).__ZOOGI_CAMERA_TARGET__ = controlsRef.current.target;
      }
    };
    const interval = setInterval(updateTarget, 100);
    return () => {
      clearInterval(interval);
      delete (window as any).__ZOOGI_CAMERA_TARGET__;
    };
  }, []);
  
  return (
    <OrbitControls 
      ref={controlsRef}
      enablePan={!cameraLocked}
      enableZoom={!cameraLocked}
      enableRotate={!cameraLocked}
      minDistance={5}
      maxDistance={150}
      minPolarAngle={0}
      maxPolarAngle={Math.PI / 2}
    />
  );
}

export function GameCamera() {
  const { camera, gl, size } = useThree();
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const birdsEyeView = useZoogiGame((state) => state.birdsEyeView);
  const firstPersonView = useZoogiGame((state) => state.firstPersonView);
  const overShoulderView = useZoogiGame((state) => state.overShoulderView);
  const launchPadView = useZoogiGame((state) => state.launchPadView);
  const launchPadTilt = useZoogiGame((state) => state.launchPadTilt);
  const launchPadPitch = useZoogiGame((state) => state.launchPadPitch);
  const launchPadHeight = useZoogiGame((state) => state.launchPadHeight);
  const savedCameraPosition = useZoogiGame((state) => state.savedCameraPosition);
  const developerCamera = useZoogiGame((state) => state.developerCamera);
  const gameMode = useZoogiGame((state) => state.gameMode);
  const currentLocalPlayerIndex = useZoogiGame((state) => state.currentLocalPlayerIndex);
  const isPlayerTurn = useZoogiGame((state) => state.isPlayerTurn);
  const turnIndex = useZoogiGame((state) => state.turnIndex);
  const cinematicArcMode = useZoogiGame((state) => state.cinematicArcMode);
  const arcPeakEffectActive = useZoogiGame((state) => state.arcPeakEffectActive);
  const arcPeakTargetPosition = useZoogiGame((state) => state.arcPeakTargetPosition);
  const isAiming = useZoogiGame((state) => state.isAiming);
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const cameraPositionRef = useRef(new THREE.Vector3(0, 18, 22));
  const lookAtRef = useRef(new THREE.Vector3(0, 0, 0));
  const cinematicAngleRef = useRef(0);
  
  const arenaSnapRef = useRef(false);
  const arenaViewRef = useRef(true);
  
  const [orbitAngle, setOrbitAngle] = useState(0);
  const wideShot = new URLSearchParams(window.location.search).get("view") === "wide";
  const BIRDS_EYE_INITIAL_ZOOM = 70;
  const BIRDS_EYE_MIN_ZOOM = 35;
  const [birdsEyeZoom, setBirdsEyeZoom] = useState(BIRDS_EYE_INITIAL_ZOOM);
  const isDraggingRef = useRef(false);
  const lastXRef = useRef(0);
  const dragStartedOnCanvasRef = useRef(false);
  const pinchStartDistanceRef = useRef(0);
  const pinchStartZoomRef = useRef(70);

  useEffect(() => {
    (window as any).__ZOOGI_CAMERA__ = camera;
    return () => {
      delete (window as any).__ZOOGI_CAMERA__;
    };
  }, [camera]);
  
  useEffect(() => {
    (window as any).__ZOOGI_ORBIT_ANGLE__ = orbitAngle;
    return () => {
      delete (window as any).__ZOOGI_ORBIT_ANGLE__;
    };
  }, [orbitAngle]);

  arenaViewRef.current = !birdsEyeView && !firstPersonView && !overShoulderView && !launchPadView && !developerCamera;

  useEffect(() => {
    (window as any).__ZOOGI_SET_ORBIT__ = (angle: number) => setOrbitAngle(angle);
    return () => {
      delete (window as any).__ZOOGI_SET_ORBIT__;
    };
  }, []);

  useEffect(() => {
    const canvas = gl.domElement;
    
    const getTouchDistance = (touches: TouchList) => {
      if (touches.length < 2) return 0;
      const dx = touches[0].clientX - touches[1].clientX;
      const dy = touches[0].clientY - touches[1].clientY;
      return Math.sqrt(dx * dx + dy * dy);
    };
    
    const handlePointerDown = (e: PointerEvent) => {
      const target = e.target as HTMLElement;
      if (target.closest('[data-zoogi]')) return;
      
      isDraggingRef.current = true;
      dragStartedOnCanvasRef.current = true;
      lastXRef.current = e.clientX;
    };
    
    const handlePointerMove = (e: PointerEvent) => {
      if (!isDraggingRef.current || !dragStartedOnCanvasRef.current) return;
      
      if (arenaViewRef.current) return;
      const deltaX = e.clientX - lastXRef.current;
      lastXRef.current = e.clientX;
      
      setOrbitAngle(prev => prev + deltaX * 0.01);
    };
    
    const handlePointerUp = () => {
      isDraggingRef.current = false;
      dragStartedOnCanvasRef.current = false;
    };
    
    const handleTouchStart = (e: TouchEvent) => {
      const target = e.target as HTMLElement;
      if (target.closest('[data-zoogi]')) return;
      
      if (e.touches.length === 2 && birdsEyeView) {
        pinchStartDistanceRef.current = getTouchDistance(e.touches);
        pinchStartZoomRef.current = birdsEyeZoom;
        isDraggingRef.current = false;
      } else if (e.touches.length === 1) {
        isDraggingRef.current = true;
        dragStartedOnCanvasRef.current = true;
        lastXRef.current = e.touches[0].clientX;
      }
    };
    
    const handleTouchMove = (e: TouchEvent) => {
      if (e.touches.length === 2 && birdsEyeView) {
        const currentDistance = getTouchDistance(e.touches);
        const scale = pinchStartDistanceRef.current / currentDistance;
        const newZoom = Math.max(BIRDS_EYE_MIN_ZOOM, Math.min(BIRDS_EYE_INITIAL_ZOOM, pinchStartZoomRef.current * scale));
        setBirdsEyeZoom(newZoom);
        return;
      }
      
      if (!isDraggingRef.current || !dragStartedOnCanvasRef.current) return;
      
      if (e.touches.length === 1) {
        if (arenaViewRef.current) return;
        const deltaX = e.touches[0].clientX - lastXRef.current;
        lastXRef.current = e.touches[0].clientX;
        
        setOrbitAngle(prev => prev + deltaX * 0.01);
      }
    };
    
    const handleTouchEnd = () => {
      isDraggingRef.current = false;
      dragStartedOnCanvasRef.current = false;
    };
    
    const handleWheel = (e: WheelEvent) => {
      if (birdsEyeView) {
        e.preventDefault();
        const delta = e.deltaY > 0 ? 5 : -5;
        setBirdsEyeZoom(prev => Math.max(BIRDS_EYE_MIN_ZOOM, Math.min(BIRDS_EYE_INITIAL_ZOOM, prev + delta)));
      }
    };
    
    canvas.addEventListener('pointerdown', handlePointerDown);
    canvas.addEventListener('pointermove', handlePointerMove);
    canvas.addEventListener('pointerup', handlePointerUp);
    canvas.addEventListener('pointerleave', handlePointerUp);
    canvas.addEventListener('touchstart', handleTouchStart, { passive: true });
    canvas.addEventListener('touchmove', handleTouchMove, { passive: true });
    canvas.addEventListener('touchend', handleTouchEnd);
    canvas.addEventListener('wheel', handleWheel, { passive: false });
    
    return () => {
      canvas.removeEventListener('pointerdown', handlePointerDown);
      canvas.removeEventListener('pointermove', handlePointerMove);
      canvas.removeEventListener('pointerup', handlePointerUp);
      canvas.removeEventListener('pointerleave', handlePointerUp);
      canvas.removeEventListener('touchstart', handleTouchStart);
      canvas.removeEventListener('touchmove', handleTouchMove);
      canvas.removeEventListener('touchend', handleTouchEnd);
      canvas.removeEventListener('wheel', handleWheel);
    };
  }, [gl, birdsEyeView, birdsEyeZoom]);

  useFrame((state, delta) => {
    if (wideShot) {
      camera.position.set(14, 7.2, 30);
      camera.lookAt(0, 1.2, -1);
      return;
    }
    if (developerCamera) return;
    
    // Handle map editor mode with no player - use static camera based on view mode
    if (!playerEntity) {
      if (birdsEyeView) {
        // Top-down view
        camera.position.set(0, birdsEyeZoom, 0.1);
        camera.lookAt(0, 0, 0);
      } else if (firstPersonView) {
        // Ground-level view from edge of arena
        const x = Math.sin(orbitAngle) * 15;
        const z = Math.cos(orbitAngle) * 15;
        camera.position.set(x, 2, z);
        camera.lookAt(0, 1, 0);
      } else if (overShoulderView) {
        // Elevated side view
        const x = Math.sin(orbitAngle) * 18;
        const z = Math.cos(orbitAngle) * 18;
        camera.position.set(x, 8, z);
        camera.lookAt(0, 0, 0);
      } else if (launchPadView) {
        // Fixed launch pad perspective
        camera.position.set(0, launchPadHeight, launchPadTilt);
        camera.lookAt(0, 0, 0);
      } else {
        // Default orbit view for map editor
        const orbitRadius = 25;
        const orbitHeight = 15;
        const x = Math.sin(orbitAngle) * orbitRadius;
        const z = Math.cos(orbitAngle) * orbitRadius;
        camera.position.set(x, orbitHeight, z);
        camera.lookAt(0, 0, 0);
      }
      return;
    }

    let targetEntity = playerEntity;
    if (gameMode === "local_multiplayer") {
      if (currentLocalPlayerIndex > 0) {
        const enemyEntity = enemies[currentLocalPlayerIndex - 1];
        if (enemyEntity) {
          targetEntity = enemyEntity;
        }
      }
    } else {
      if (!isPlayerTurn && enemies.length > 0 && turnIndex < enemies.length) {
        targetEntity = enemies[turnIndex];
      }
    }

    const playerPos = targetEntity.position;
    const playerVelocity = targetEntity.velocity;
    decayTrauma(Math.min(delta, 0.05));
    
    const gameFeel = useGameFeel.getState();
    gameFeel.update();
    
    const cameraEffects = useCameraEffects.getState();
    cameraEffects.update(delta);
    
    // Skip camera updates during freeze frame (but allow arc peak effect to update)
    if (cameraEffects.isFrozen && !arcPeakEffectActive) return;
    
    let gameFeelShake = 0;
    if (gameFeel.screenShake) {
      const elapsed = Date.now() - gameFeel.screenShake.startTime;
      const progress = elapsed / gameFeel.screenShake.duration;
      if (progress < 1) {
        gameFeelShake = gameFeel.screenShake.intensity * (1 - progress);
      }
    }
    
    const abilityShake = cameraEffects.computedShake;
    // Small bumps stay still. Only a real hit, or an ability that asks for it, shakes.
    const punch = Math.max(getTrauma(), gameFeelShake > 0.35 ? gameFeelShake : 0, abilityShake > 0.35 ? abilityShake : 0);
    
    const abilityZoom = cameraEffects.computedZoom;
    const zoomNudge = 1 + Math.max(-0.12, Math.min(0.18, abilityZoom * 0.01));
    
    if (!arenaViewRef.current) {
      const persp = camera as THREE.PerspectiveCamera;
      if (persp.isPerspectiveCamera && Math.abs(persp.fov - 50) > 0.1) {
        persp.fov = 50;
        persp.updateProjectionMatrix();
      }
    }

    let idealCameraPos: THREE.Vector3;
    let idealLookAt: THREE.Vector3;
    
    // Arc peak effect: floor-level view from next to target looking up at incoming Zoogi
    // Skip camera switch for birds eye view - only show overlay without changing camera
    if (arcPeakEffectActive && arcPeakTargetPosition && !birdsEyeView) {
      const targetPos = arcPeakTargetPosition;
      // Position camera at floor level (just above target's ground), slightly offset from target
      const dirToPlayer = new THREE.Vector3(
        playerPos[0] - targetPos[0],
        0,
        playerPos[2] - targetPos[2]
      ).normalize();
      // Offset camera slightly to the side for a dramatic angle
      const sideOffset = new THREE.Vector3(-dirToPlayer.z, 0, dirToPlayer.x).multiplyScalar(1.5);
      // Use target's Y position as ground reference, add small offset to stay above surface
      const floorLevel = targetPos[1] + 0.2;
      idealCameraPos = new THREE.Vector3(
        targetPos[0] + sideOffset.x,
        floorLevel,
        targetPos[2] + sideOffset.z
      );
      // Look up at the incoming Zoogi
      idealLookAt = new THREE.Vector3(
        playerPos[0],
        playerPos[1] + 1.5, // Look up at the Zoogi
        playerPos[2]
      );
    } else if (launchPadView) {
      // Fixed launch pad view - camera stays in place, doesn't follow orbit
      idealCameraPos = new THREE.Vector3(0, launchPadHeight, launchPadTilt);
      
      // Pitch controls camera rotation up/down around its right axis
      const pitchRad = (launchPadPitch * Math.PI) / 180;
      
      // Calculate base forward direction (from camera to arena center)
      const baseForward = new THREE.Vector3(0, -launchPadHeight, -launchPadTilt).normalize();
      
      // Calculate right axis (perpendicular to forward and world up)
      const worldUp = new THREE.Vector3(0, 1, 0);
      let rightAxis = new THREE.Vector3().crossVectors(baseForward, worldUp);
      // Fallback to X axis if forward is aligned with up (tilt near 0)
      if (rightAxis.lengthSq() < 0.001) {
        rightAxis.set(1, 0, 0);
      } else {
        rightAxis.normalize();
      }
      
      // Rotate the forward vector around the right axis by pitch angle
      const rotatedForward = baseForward.clone().applyAxisAngle(rightAxis, pitchRad);
      
      // Compute lookAt point
      const distToCenter = Math.sqrt(launchPadHeight * launchPadHeight + launchPadTilt * launchPadTilt);
      idealLookAt = idealCameraPos.clone().add(rotatedForward.multiplyScalar(distToCenter));
    } else if (birdsEyeView) {
      const distance = 0.1;
      idealCameraPos = new THREE.Vector3(
        Math.sin(orbitAngle) * distance,
        birdsEyeZoom,
        Math.cos(orbitAngle) * distance
      );
      idealLookAt = new THREE.Vector3(0, 0, 0);
    } else if (firstPersonView) {
      const lookDistance = 10;
      idealCameraPos = new THREE.Vector3(
        playerPos[0],
        playerPos[1] + 1.5,
        playerPos[2]
      );
      idealLookAt = new THREE.Vector3(
        playerPos[0] + Math.sin(orbitAngle) * lookDistance,
        1,
        playerPos[2] + Math.cos(orbitAngle) * lookDistance
      );
    } else if (overShoulderView) {
      if (cinematicArcMode && playerVelocity) {
        const speed = Math.sqrt(playerVelocity[0] ** 2 + playerVelocity[2] ** 2);
        let lookAheadX = 0;
        let lookAheadZ = 0;
        if (speed > 0.1) {
          const movementAngle = Math.atan2(playerVelocity[0], playerVelocity[2]);
          const sideAngle = movementAngle - Math.PI / 2;
          cinematicAngleRef.current = sideAngle;
          const lookAheadDistance = Math.min(speed * 4, 6);
          lookAheadX = (playerVelocity[0] / speed) * lookAheadDistance;
          lookAheadZ = (playerVelocity[2] / speed) * lookAheadDistance;
        }
        const sideDistance = 8;
        const heightOffset = 3;
        idealCameraPos = new THREE.Vector3(
          playerPos[0] + Math.sin(cinematicAngleRef.current) * sideDistance,
          playerPos[1] + heightOffset,
          playerPos[2] + Math.cos(cinematicAngleRef.current) * sideDistance
        );
        idealLookAt = new THREE.Vector3(
          playerPos[0] + lookAheadX,
          playerPos[1] + 1,
          playerPos[2] + lookAheadZ
        );
      } else if (savedCameraPosition) {
        const saved = savedCameraPosition;
        const localForward = saved.position[0];
        const offsetY = saved.position[1];
        const localRight = saved.position[2];
        const worldX = localForward * Math.sin(orbitAngle) + localRight * Math.cos(orbitAngle);
        const worldZ = localForward * Math.cos(orbitAngle) - localRight * Math.sin(orbitAngle);
        idealCameraPos = new THREE.Vector3(
          playerPos[0] + worldX,
          offsetY,
          playerPos[2] + worldZ
        );
        idealLookAt = new THREE.Vector3(
          playerPos[0],
          playerPos[1] + 0.5,
          playerPos[2]
        );
      } else {
        const localForward = -3;
        const heightOffset = 2.5;
        const localRight = -3;
        const worldX = localForward * Math.sin(orbitAngle) + localRight * Math.cos(orbitAngle);
        const worldZ = localForward * Math.cos(orbitAngle) - localRight * Math.sin(orbitAngle);
        idealCameraPos = new THREE.Vector3(
          playerPos[0] + worldX,
          heightOffset,
          playerPos[2] + worldZ
        );
        idealLookAt = new THREE.Vector3(
          playerPos[0],
          playerPos[1] + 0.5,
          playerPos[2]
        );
      }
    } else {
      const persp = camera as THREE.PerspectiveCamera;
      if (persp.isPerspectiveCamera && Math.abs(persp.fov - ARENA_FOV_DEG) > 0.1) {
        persp.fov = ARENA_FOV_DEG;
        persp.updateProjectionMatrix();
      }
      const onCourt = (entity: { isKnockedOut?: boolean; isRespawning?: boolean }) =>
        !entity.isKnockedOut && !entity.isRespawning;
      const points = [
        ...(onCourt(playerEntity) ? [{ x: playerEntity.position[0], z: playerEntity.position[2] }] : []),
        ...enemies.filter(onCourt).map((enemy) => ({ x: enemy.position[0], z: enemy.position[2] })),
      ];
      const padCount = (playerEntity ? 1 : 0) + enemies.length;
      for (const pad of launchPadsFor(selectedMap).slice(0, padCount)) {
        points.push({ x: pad.x, z: pad.z });
      }
      const aspect = size.width / Math.max(1, size.height);
      const neonCourt = selectedMap === "neon";
      const frameScale = arenaScaleFor(selectedMap);
      if (neonCourt) {
        const halfX = neonPlayHalfX();
        const halfZ = neonPlayHalfZ();
        points.push(
          { x: halfX, z: halfZ },
          { x: halfX, z: -halfZ },
          { x: -halfX, z: halfZ },
          { x: -halfX, z: -halfZ },
        );
      } else {
        const ring = getMapLayout(selectedMap)?.floorRadius ?? 18 * frameScale;
        points.push(
          { x: ring, z: ring },
          { x: ring, z: -ring },
          { x: -ring, z: ring },
          { x: -ring, z: -ring },
        );
      }
      // Night Circuit sits a little farther back and aims slightly toward the
      // far bowl so the stands and skyline clear the top of the frame.
      const pad = ((aspect < 0.9 ? 1.3 : 2.6) + (neonCourt ? 1.6 : 0)) * frameScale;
      const bounds = actionBounds(points, 0);
      const lookX = ((bounds.minX + bounds.maxX) / 2) * 0.7;
      const lookZ = ((bounds.minZ + bounds.maxZ) / 2) * 0.7 + (neonCourt ? -3.5 * frameScale : 0);
      const distance = clampDistance(
        fitDistance(bounds, ARENA_PITCH, ARENA_FOV_DEG, aspect, pad) * zoomNudge,
        (neonCourt ? 19 : 13.5) * frameScale,
        matchCameraMax(aspect, frameScale),
      );
      const offset = cameraOffset(distance, ARENA_PITCH);
      idealCameraPos = new THREE.Vector3(lookX + offset.x, offset.y, lookZ + offset.z);
      idealLookAt = new THREE.Vector3(lookX, 0.35, lookZ);
    }

    // Skip all camera effects in birds eye view - pure observation mode
    if (!birdsEyeView) {
      // Apply camera effects offset (for aim/ability effects)
      const effectOffset = cameraEffects.computedOffset;
      if (effectOffset.x !== 0 || effectOffset.y !== 0 || effectOffset.z !== 0) {
        idealCameraPos.x += effectOffset.x;
        idealCameraPos.y += effectOffset.y;
        idealCameraPos.z += effectOffset.z;
        idealLookAt.y += effectOffset.y * 0.5; // Lift look target slightly for thumb-safe framing
      }

      // Apply target focus effect - override look-at to pan toward target
      const targetPos = cameraEffects.computedTargetPosition;
      if (targetPos) {
        // Blend between player and target for cinematic pan
        idealLookAt = new THREE.Vector3(
          targetPos.x,
          targetPos.y + 0.5,
          targetPos.z
        );
      }
    }

    if (birdsEyeView) {
      // Fixed camera - no interpolation, no shake for birds eye
      cameraPositionRef.current.copy(idealCameraPos);
      lookAtRef.current.copy(idealLookAt);
      camera.position.copy(idealCameraPos);
      camera.lookAt(idealLookAt);
    } else {
      const dt = Math.min(Math.max(delta, 0), 0.05);
      const arenaView = arenaViewRef.current;
      if (arenaView && !arenaSnapRef.current) {
        cameraPositionRef.current.copy(idealCameraPos);
        lookAtRef.current.copy(idealLookAt);
        arenaSnapRef.current = true;
      } else {
        const lambda = arcPeakEffectActive ? 8 : arenaView ? 3.4 : 5;
        cameraPositionRef.current.set(
          damp(cameraPositionRef.current.x, idealCameraPos.x, lambda, dt),
          damp(cameraPositionRef.current.y, idealCameraPos.y, lambda, dt),
          damp(cameraPositionRef.current.z, idealCameraPos.z, lambda, dt),
        );
        lookAtRef.current.set(
          damp(lookAtRef.current.x, idealLookAt.x, lambda, dt),
          damp(lookAtRef.current.y, idealLookAt.y, lambda, dt),
          damp(lookAtRef.current.z, idealLookAt.z, lambda, dt),
        );
      }

      const shake = smoothShake(Math.min(1, punch), state.clock.elapsedTime);
      camera.position.copy(cameraPositionRef.current).add(new THREE.Vector3(shake.x, shake.y, shake.z));
      camera.lookAt(lookAtRef.current);

      const abilityTilt = arenaView ? 0 : cameraEffects.computedTilt;
      if (abilityTilt !== 0) {
        camera.rotateZ(abilityTilt * Math.PI / 180 * 0.3);
      }
    }
  });

  if (developerCamera) {
    return <DeveloperCamera />;
  }
  
  return null;
}
