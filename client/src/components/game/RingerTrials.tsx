import { useState, useRef, useEffect, useMemo, Suspense, useCallback } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { useGLTF, KeyboardControls, useKeyboardControls, Environment, OrbitControls } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import * as THREE from "three";
import { clone as skeletonClone } from "three/examples/jsm/utils/SkeletonUtils.js";
import { Sparkles, Camera, Move3D, Download } from "lucide-react";

enum Controls {
  forward = "forward",
  back = "back",
  left = "left",
  right = "right",
  jump = "jump",
  interact = "interact",
}

const MOVE_SPEED = 4;
const RUN_SPEED = 7;
const RUN_THRESHOLD = 0.85;
const JUMP_FORCE = 6;
const GRAVITY = -15;
const ACCEL = 20;
const DECEL = 12;
const FALL_RESPAWN_Y = -20;

const touchInputRef = { current: { x: 0, y: 0, jump: false, interact: false } };

function VirtualJoystick() {
  const joystickRef = useRef<HTMLDivElement>(null);
  const knobRef = useRef<HTMLDivElement>(null);
  const touchIdRef = useRef<number | null>(null);
  const centerRef = useRef({ x: 0, y: 0 });
  const RADIUS = 50;

  const handleStart = useCallback((e: React.TouchEvent) => {
    if (touchIdRef.current !== null) return;
    const touch = e.changedTouches[0];
    touchIdRef.current = touch.identifier;
    const rect = joystickRef.current?.getBoundingClientRect();
    if (rect) {
      centerRef.current = { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 };
    }
  }, []);

  const handleMove = useCallback((e: React.TouchEvent) => {
    e.preventDefault();
    for (let i = 0; i < e.changedTouches.length; i++) {
      const touch = e.changedTouches[i];
      if (touch.identifier === touchIdRef.current) {
        const dx = touch.clientX - centerRef.current.x;
        const dy = touch.clientY - centerRef.current.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const clampedDist = Math.min(dist, RADIUS);
        const angle = Math.atan2(dy, dx);
        const nx = Math.cos(angle) * clampedDist;
        const ny = Math.sin(angle) * clampedDist;

        if (knobRef.current) {
          knobRef.current.style.transform = `translate(${nx}px, ${ny}px)`;
        }

        const normX = clampedDist > 5 ? nx / RADIUS : 0;
        const normY = clampedDist > 5 ? ny / RADIUS : 0;
        touchInputRef.current.x = normX;
        touchInputRef.current.y = -normY;
      }
    }
  }, []);

  const handleEnd = useCallback((e: React.TouchEvent) => {
    for (let i = 0; i < e.changedTouches.length; i++) {
      if (e.changedTouches[i].identifier === touchIdRef.current) {
        touchIdRef.current = null;
        touchInputRef.current.x = 0;
        touchInputRef.current.y = 0;
        if (knobRef.current) {
          knobRef.current.style.transform = `translate(0px, 0px)`;
        }
      }
    }
  }, []);

  return (
    <div
      ref={joystickRef}
      className="absolute bottom-24 left-8 w-32 h-32 rounded-full bg-white/10 border-2 border-white/20 flex items-center justify-center z-40 select-none"
      style={{ touchAction: "none" }}
      onTouchStart={handleStart}
      onTouchMove={handleMove}
      onTouchEnd={handleEnd}
      onTouchCancel={handleEnd}
    >
      <div
        ref={knobRef}
        className="w-14 h-14 rounded-full bg-white/30 border-2 border-white/50 pointer-events-none"
        style={{ transition: "transform 0.05s ease-out" }}
      />
    </div>
  );
}

function TouchActionButtons() {
  const handleJumpStart = useCallback(() => {
    touchInputRef.current.jump = true;
  }, []);
  const handleJumpEnd = useCallback(() => {
    touchInputRef.current.jump = false;
  }, []);
  const handleInteractStart = useCallback(() => {
    touchInputRef.current.interact = true;
  }, []);
  const handleInteractEnd = useCallback(() => {
    touchInputRef.current.interact = false;
  }, []);

  return (
    <div className="absolute bottom-24 right-8 flex flex-col gap-3 z-40 select-none" style={{ touchAction: "none" }}>
      <button
        className="w-16 h-16 rounded-full bg-yellow-500/30 border-2 border-yellow-400/60 text-white font-bold text-xs flex items-center justify-center active:bg-yellow-500/50"
        onTouchStart={handleInteractStart}
        onTouchEnd={handleInteractEnd}
        onTouchCancel={handleInteractEnd}
      >
        E
      </button>
      <button
        className="w-16 h-16 rounded-full bg-blue-500/30 border-2 border-blue-400/60 text-white font-bold text-sm flex items-center justify-center active:bg-blue-500/50"
        onTouchStart={handleJumpStart}
        onTouchEnd={handleJumpEnd}
        onTouchCancel={handleJumpEnd}
      >
        Jump
      </button>
    </div>
  );
}

interface RingerConfig {
  name: string;
  selections: Record<string, string>;
  skinColor: string;
}

function CharacterPartModel({ path, bodySkeleton }: { path: string; bodySkeleton: THREE.Skeleton | null }) {
  const { scene } = useGLTF(path);
  const clonedScene = useMemo(() => {
    const clone = skeletonClone(scene);
    clone.traverse((child: THREE.Object3D) => {
      if (child instanceof THREE.SkinnedMesh && bodySkeleton) {
        child.frustumCulled = false;
        if (child.material) {
          child.material = (child.material as THREE.MeshStandardMaterial).clone();
          const mat = child.material as THREE.MeshStandardMaterial;
          mat.side = THREE.DoubleSide;
          mat.needsUpdate = true;
        }
        child.skeleton = bodySkeleton;
        child.bind(bodySkeleton, child.bindMatrix);
      } else if (child instanceof THREE.Mesh) {
        child.frustumCulled = false;
        if (child.material) {
          child.material = (child.material as THREE.MeshStandardMaterial).clone();
          const mat = child.material as THREE.MeshStandardMaterial;
          mat.side = THREE.DoubleSide;
          mat.needsUpdate = true;
        }
      }
    });
    return clone;
  }, [scene, bodySkeleton]);
  return <primitive object={clonedScene} />;
}

function PlayerCharacter({
  config,
  altarPosition,
  onAltarInteract,
  altarActive,
  externalRef,
  surfaceY,
  playerScale = 2,
  platformRef,
  knockbackRef,
}: {
  config: RingerConfig;
  altarPosition: [number, number, number];
  onAltarInteract: () => void;
  altarActive: boolean;
  externalRef?: React.RefObject<THREE.Group | null>;
  surfaceY: number;
  playerScale?: number;
  platformRef?: React.RefObject<THREE.Group | null>;
  knockbackRef?: React.RefObject<{ apply: (dirX: number, dirZ: number, force: number) => void } | null>;
}) {
  const internalRef = useRef<THREE.Group>(null);
  const groupRef = (externalRef || internalRef) as React.RefObject<THREE.Group>;
  const modelRef = useRef<THREE.Group>(null);
  const velocityRef = useRef(new THREE.Vector3());
  const knockbackTimer = useRef(0);

  useEffect(() => {
    if (knockbackRef && 'current' in knockbackRef) {
      (knockbackRef as React.MutableRefObject<{ apply: (dirX: number, dirZ: number, force: number) => void }>).current = {
        apply: (dirX: number, dirZ: number, force: number) => {
          velocityRef.current.x = dirX * force;
          velocityRef.current.z = dirZ * force;
          velocityRef.current.y = force * 0.3;
          knockbackTimer.current = 0.35;
        }
      };
    }
  }, [knockbackRef]);

  const isGroundedRef = useRef(true);
  const interactCooldownRef = useRef(0);
  const moveStateRef = useRef<"idle" | "walk" | "run" | "jump" | "fall" | "land">("idle");
  const raycasterRef = useRef(new THREE.Raycaster());
  const isFallingOffRef = useRef(false);
  const [, getState] = useKeyboardControls<Controls>();
  const { camera } = useThree();

  const bodyGltf = useGLTF("/models/character/body.glb");

  const idleGltf = useGLTF("/models/character/animations/idle.glb");
  const walkGltf = useGLTF("/models/character/animations/walking.glb");
  const runGltf = useGLTF("/models/character/animations/running.glb");
  const jumpGltf = useGLTF("/models/character/animations/jumping_up.glb");
  const fallGltf = useGLTF("/models/character/animations/falling_idle.glb");
  const landGltf = useGLTF("/models/character/animations/hard_landing.glb");

  const clonedBody = useMemo(() => {
    const clone = skeletonClone(bodyGltf.scene);
    clone.traverse((child: THREE.Object3D) => {
      if (child instanceof THREE.SkinnedMesh) {
        child.frustumCulled = false;
        child.visible = true;
        if (child.material) {
          child.material = (child.material as THREE.MeshStandardMaterial).clone();
          const mat = child.material as THREE.MeshStandardMaterial;
          mat.side = THREE.DoubleSide;
          if (mat.map) {
            mat.map.needsUpdate = true;
          }
          if (mat.normalMap) {
            mat.normalMap.needsUpdate = true;
          }
          mat.needsUpdate = true;
        }
        child.castShadow = true;
        child.receiveShadow = true;
      } else if (child instanceof THREE.Mesh) {
        child.frustumCulled = false;
        child.visible = true;
        if (child.material) {
          child.material = (child.material as THREE.MeshStandardMaterial).clone();
          const mat = child.material as THREE.MeshStandardMaterial;
          mat.side = THREE.DoubleSide;
          mat.needsUpdate = true;
        }
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
    return clone;
  }, [bodyGltf.scene]);

  const bodySkeleton = useMemo(() => {
    if (!clonedBody) return null;
    let skeleton: THREE.Skeleton | null = null;
    clonedBody.traverse((child: THREE.Object3D) => {
      if (child instanceof THREE.SkinnedMesh && !skeleton) {
        skeleton = child.skeleton;
      }
    });
    return skeleton;
  }, [clonedBody]);

  const mixerRef = useRef<THREE.AnimationMixer | null>(null);
  const actionsRef = useRef<Record<string, THREE.AnimationAction>>({});
  const currentActionRef = useRef<string>("idle");

  const retargetClip = useCallback((clip: THREE.AnimationClip, name: string) => {
    const newTracks: THREE.KeyframeTrack[] = [];
    for (const track of clip.tracks) {
      const newName = track.name.replace(/mixamorig:?/g, "");
      if (newName === "Hips.position") continue;
      const cloned = track.clone();
      cloned.name = newName;
      newTracks.push(cloned);
    }
    return new THREE.AnimationClip(name, clip.duration, newTracks);
  }, []);

  const fadeToAction = useCallback((name: string, duration = 0.2) => {
    const actions = actionsRef.current;
    const current = actions[currentActionRef.current];
    const next = actions[name];
    if (!next || name === currentActionRef.current) return;

    next.reset();
    next.play();
    if (current) {
      current.crossFadeTo(next, duration, true);
    }
    currentActionRef.current = name;
  }, []);

  useEffect(() => {
    if (!clonedBody) return;

    const mixer = new THREE.AnimationMixer(clonedBody);
    mixerRef.current = mixer;

    const clips: Record<string, THREE.AnimationClip> = {};
    if (idleGltf.animations.length > 0) clips.idle = retargetClip(idleGltf.animations[0], "idle");
    if (walkGltf.animations.length > 0) clips.walk = retargetClip(walkGltf.animations[0], "walk");
    if (runGltf.animations.length > 0) clips.run = retargetClip(runGltf.animations[0], "run");
    if (jumpGltf.animations.length > 0) clips.jump = retargetClip(jumpGltf.animations[0], "jump");
    if (fallGltf.animations.length > 0) clips.fall = retargetClip(fallGltf.animations[0], "fall");
    if (landGltf.animations.length > 0) clips.land = retargetClip(landGltf.animations[0], "land");

    const actions: Record<string, THREE.AnimationAction> = {};
    for (const [name, clip] of Object.entries(clips)) {
      const action = mixer.clipAction(clip);
      if (name === "jump" || name === "land") {
        action.setLoop(THREE.LoopOnce, 1);
        action.clampWhenFinished = true;
      }
      actions[name] = action;
    }
    actionsRef.current = actions;

    if (actions.idle) {
      actions.idle.play();
      currentActionRef.current = "idle";
    }

    const onFinished = (e: { action: THREE.AnimationAction }) => {
      if (e.action === actions.land || e.action === actions.jump) {
        if (moveStateRef.current === "land") {
          moveStateRef.current = "idle";
          fadeToAction("idle", 0.25);
        }
      }
    };
    mixer.addEventListener("finished", onFinished as any);

    return () => {
      mixer.removeEventListener("finished", onFinished as any);
      mixer.stopAllAction();
      mixer.uncacheRoot(clonedBody);
    };
  }, [clonedBody, idleGltf, walkGltf, runGltf, jumpGltf, fallGltf, landGltf, retargetClip, fadeToAction]);

  const activeParts = useMemo(() => {
    const parts: string[] = [];
    const CHARACTER_PARTS_MAP: Record<string, { id: string; path: string }[]> = {
      face: [
        { id: "face_usual", path: "/models/character/face_usual.glb" },
        { id: "face_happy", path: "/models/character/face_happy.glb" },
        { id: "face_angry", path: "/models/character/face_angry.glb" },
      ],
      hair: [
        { id: "hair_short", path: "/models/character/hair_short.glb" },
        { id: "hair_wavy", path: "/models/character/hair_wavy.glb" },
      ],
      hat: [
        { id: "hat_helmet", path: "/models/character/hat_helmet.glb" },
        { id: "hat_straw", path: "/models/character/hat_straw.glb" },
        { id: "hat_sports", path: "/models/character/hat_sports.glb" },
      ],
      top: [
        { id: "tshirt", path: "/models/character/tshirt.glb" },
        { id: "outerwear_jacket", path: "/models/character/outerwear_jacket.glb" },
        { id: "outerwear_hoodie", path: "/models/character/outerwear_hoodie.glb" },
      ],
      bottom: [
        { id: "pants_jeans", path: "/models/character/pants_jeans.glb" },
        { id: "pants_cargo", path: "/models/character/pants_cargo.glb" },
        { id: "shorts", path: "/models/character/shorts.glb" },
      ],
      shoes: [
        { id: "shoes_sneakers", path: "/models/character/shoes_sneakers.glb" },
        { id: "shoes_slippers", path: "/models/character/shoes_slippers_blue.glb" },
        { id: "shoes_sandals", path: "/models/character/shoes_sandals.glb" },
      ],
      glasses: [
        { id: "glasses_round", path: "/models/character/glasses_round.glb" },
        { id: "glasses_square", path: "/models/character/glasses_square.glb" },
      ],
      gloves: [
        { id: "gloves_dark", path: "/models/character/gloves_dark.glb" },
        { id: "gloves_light", path: "/models/character/gloves_light.glb" },
      ],
      mustache: [
        { id: "mustache_handlebar", path: "/models/character/mustache_handlebar.glb" },
        { id: "mustache_walrus", path: "/models/character/mustache_walrus.glb" },
      ],
      accessory: [
        { id: "acc_headphones", path: "/models/character/acc_headphones.glb" },
        { id: "acc_clown_nose", path: "/models/character/acc_clown_nose.glb" },
      ],
      costume: [
        { id: "costume_clown", path: "/models/character/costume_clown.glb" },
        { id: "costume_mouse", path: "/models/character/costume_mouse.glb" },
      ],
    };

    Object.entries(config.selections).forEach(([category, selectedId]) => {
      if (selectedId && selectedId !== "none") {
        const categoryParts = CHARACTER_PARTS_MAP[category];
        if (categoryParts) {
          const found = categoryParts.find((p) => p.id === selectedId);
          if (found) parts.push(found.path);
        }
      }
    });
    return parts;
  }, [config.selections]);

  useFrame((state, delta) => {
    if (!groupRef.current) return;

    if (knockbackTimer.current > 0) {
      knockbackTimer.current -= delta;
    }

    const inKnockback = knockbackTimer.current > 0;

    const controls = getState();
    const touch = touchInputRef.current;
    const moveDir = new THREE.Vector3();

    const cameraForward = new THREE.Vector3();
    camera.getWorldDirection(cameraForward);
    cameraForward.y = 0;
    cameraForward.normalize();

    const cameraRight = new THREE.Vector3();
    cameraRight.crossVectors(cameraForward, new THREE.Vector3(0, 1, 0)).normalize();

    const touchMag = Math.sqrt(touch.x * touch.x + touch.y * touch.y);

    if (!inKnockback) {
      if (controls.forward) moveDir.add(cameraForward);
      if (controls.back) moveDir.sub(cameraForward);
      if (controls.right) moveDir.add(cameraRight);
      if (controls.left) moveDir.sub(cameraRight);

      if (touchMag > 0.05) {
        moveDir.addScaledVector(cameraForward, touch.y);
        moveDir.addScaledVector(cameraRight, touch.x);
      }
    }

    const isMoving = !inKnockback && moveDir.length() > 0.05;
    const isRunning = touchMag > RUN_THRESHOLD;

    if (isMoving) {
      moveDir.normalize();
      const targetAngle = Math.atan2(moveDir.x, moveDir.z);
      const currentRotation = groupRef.current.rotation.y;
      const diff = targetAngle - currentRotation;
      const normalizedDiff = Math.atan2(Math.sin(diff), Math.cos(diff));
      groupRef.current.rotation.y += normalizedDiff * 10 * delta;
    }

    const targetSpeed = isRunning ? RUN_SPEED : MOVE_SPEED;

    if (inKnockback) {
      const IMPULSE_LOCKOUT = 0.15;
      const timeInKnockback = 0.35 - knockbackTimer.current;
      if (timeInKnockback > IMPULSE_LOCKOUT) {
        velocityRef.current.x *= (1 - 3.0 * delta);
        velocityRef.current.z *= (1 - 3.0 * delta);
      }
    } else if (isMoving) {
      const targetVx = moveDir.x * targetSpeed;
      const targetVz = moveDir.z * targetSpeed;
      velocityRef.current.x += (targetVx - velocityRef.current.x) * Math.min(1, ACCEL * delta);
      velocityRef.current.z += (targetVz - velocityRef.current.z) * Math.min(1, ACCEL * delta);
    } else {
      velocityRef.current.x += (0 - velocityRef.current.x) * Math.min(1, DECEL * delta);
      velocityRef.current.z += (0 - velocityRef.current.z) * Math.min(1, DECEL * delta);
      if (Math.abs(velocityRef.current.x) < 0.01) velocityRef.current.x = 0;
      if (Math.abs(velocityRef.current.z) < 0.01) velocityRef.current.z = 0;
    }

    if ((controls.jump || touch.jump) && isGroundedRef.current && !isFallingOffRef.current) {
      velocityRef.current.y = JUMP_FORCE;
      isGroundedRef.current = false;
    }

    velocityRef.current.y += GRAVITY * delta;

    const nextX = groupRef.current.position.x + velocityRef.current.x * delta;
    const nextY = groupRef.current.position.y + velocityRef.current.y * delta;
    const nextZ = groupRef.current.position.z + velocityRef.current.z * delta;

    let groundY = -Infinity;
    if (platformRef?.current) {
      raycasterRef.current.set(
        new THREE.Vector3(nextX, nextY + 5, nextZ),
        new THREE.Vector3(0, -1, 0)
      );
      const hits = raycasterRef.current.intersectObject(platformRef.current, true);
      if (hits.length > 0) {
        groundY = hits[0].point.y;
      }
    }

    groupRef.current.position.x = nextX;
    groupRef.current.position.z = nextZ;

    if (groundY > -Infinity) {
      isFallingOffRef.current = false;
      if (nextY <= groundY) {
        groupRef.current.position.y = groundY;
        velocityRef.current.y = 0;
        isGroundedRef.current = true;
      } else {
        groupRef.current.position.y = nextY;
      }
    } else {
      isFallingOffRef.current = true;
      isGroundedRef.current = false;
      groupRef.current.position.y = nextY;
    }

    if (groupRef.current.position.y < FALL_RESPAWN_Y) {
      groupRef.current.position.set(0, surfaceY + 2, 0);
      velocityRef.current.set(0, 0, 0);
      isGroundedRef.current = false;
      isFallingOffRef.current = false;
    }

    let newState: "idle" | "walk" | "run" | "jump" | "fall" | "land" = "idle";
    const wasGrounded = moveStateRef.current !== "jump" && moveStateRef.current !== "fall";
    const justLanded = !wasGrounded && isGroundedRef.current;

    if (!isGroundedRef.current) {
      if (velocityRef.current.y > 0) {
        newState = "jump";
      } else {
        newState = "fall";
      }
    } else if (justLanded && moveStateRef.current !== "land") {
      newState = "land";
    } else if (moveStateRef.current === "land") {
      newState = isMoving ? (isRunning ? "run" : "walk") : "land";
    } else if (isRunning && isMoving) {
      newState = "run";
    } else if (isMoving) {
      newState = "walk";
    }

    if (newState !== moveStateRef.current) {
      moveStateRef.current = newState;
      if (newState === "idle") fadeToAction("idle", 0.25);
      else if (newState === "walk") fadeToAction("walk", 0.2);
      else if (newState === "run") fadeToAction("run", 0.15);
      else if (newState === "jump") fadeToAction("jump", 0.1);
      else if (newState === "fall") fadeToAction("fall", 0.2);
      else if (newState === "land") fadeToAction("land", 0.15);
    }

    if (mixerRef.current) {
      mixerRef.current.update(delta);
    }

    if (interactCooldownRef.current > 0) {
      interactCooldownRef.current -= delta;
    }

    if ((controls.interact || touch.interact) && altarActive && interactCooldownRef.current <= 0) {
      const adx = groupRef.current.position.x - altarPosition[0];
      const adz = groupRef.current.position.z - altarPosition[2];
      const altarDist = Math.sqrt(adx * adx + adz * adz);
      if (altarDist < 2.5) {
        interactCooldownRef.current = 1.0;
        onAltarInteract();
      }
    }
  });

  return (
    <group ref={groupRef} position={[0, surfaceY, 0]}>
      <group ref={modelRef} scale={[playerScale, playerScale, playerScale]} position={[0, -0.15, 0]}>
        <primitive object={clonedBody} />
        {activeParts.map((path) => (
          <CharacterPartModel key={path} path={path} bodySkeleton={bodySkeleton} />
        ))}
      </group>
      <pointLight position={[0, 3, 0]} intensity={2} distance={10} color="#ffffff" />
    </group>
  );
}

function IsometricCamera({ target }: { target: React.RefObject<THREE.Group | null> }) {
  const { camera } = useThree();
  const offsetRef = useRef(new THREE.Vector3(8, 10, 8));
  const smoothPosRef = useRef(new THREE.Vector3());
  const smoothLookRef = useRef(new THREE.Vector3());
  const initializedRef = useRef(false);

  useFrame((_, delta) => {
    if (!target.current) return;
    const targetPos = target.current.position;
    const desired = new THREE.Vector3(
      targetPos.x + offsetRef.current.x,
      targetPos.y + offsetRef.current.y,
      targetPos.z + offsetRef.current.z
    );
    const desiredLook = new THREE.Vector3(targetPos.x, targetPos.y + 0.5, targetPos.z);

    if (!initializedRef.current) {
      smoothPosRef.current.copy(desired);
      smoothLookRef.current.copy(desiredLook);
      initializedRef.current = true;
    }

    const lerpFactor = 1 - Math.exp(-4 * delta);
    smoothPosRef.current.lerp(desired, lerpFactor);
    smoothLookRef.current.lerp(desiredLook, lerpFactor);

    camera.position.copy(smoothPosRef.current);
    camera.lookAt(smoothLookRef.current);
  });

  return null;
}

function DevCamera({ target, orbitEnabled }: { target: React.RefObject<THREE.Group | null>; orbitEnabled: boolean }) {
  const controlsRef = useRef<any>(null);

  useFrame(() => {
    if (!target.current || !controlsRef.current) return;
    const pos = target.current.position;
    controlsRef.current.target.lerp(new THREE.Vector3(pos.x, pos.y + 0.5, pos.z), 0.05);
    controlsRef.current.update();
  });

  return (
    <OrbitControls
      ref={controlsRef}
      enabled={orbitEnabled}
      enableDamping
      dampingFactor={0.1}
      minDistance={3}
      maxDistance={30}
      maxPolarAngle={Math.PI / 2}
    />
  );
}

function UnderworldPlatform({ round, onSurfaceHeight, platformRef, playerRef, onCoinCollect, onPlayerKnockback }: { round: number; onSurfaceHeight?: (y: number) => void; platformRef?: React.RefObject<THREE.Group | null>; playerRef?: React.RefObject<THREE.Group | null>; onCoinCollect?: () => void; onPlayerKnockback?: (dirX: number, dirZ: number, force: number) => void }) {
  const { scene } = useGLTF("/models/platforms/underworld.glb");
  const internalRef = useRef<THREE.Group>(null);
  const groupRef = platformRef || internalRef;

  const crossedBarsRefs = useRef<THREE.Object3D[]>([]);
  const hammerRefs = useRef<THREE.Object3D[]>([]);
  const coinRefs = useRef<THREE.Object3D[]>([]);
  const fireRefs = useRef<THREE.Object3D[]>([]);
  const coinOriginalY = useRef<Map<string, number>>(new Map());
  const collectedCoins = useRef<Set<string>>(new Set());
  const lastKnockbackTime = useRef(0);
  const obstacleWorldPos = useRef(new THREE.Vector3());

  const platformScene = useMemo(() => {
    const clone = scene.clone(true);
    const bars: THREE.Object3D[] = [];
    const hammers: THREE.Object3D[] = [];
    const coins: THREE.Object3D[] = [];
    const fires: THREE.Object3D[] = [];
    const origY = new Map<string, number>();

    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.receiveShadow = true;
        child.castShadow = true;
      }
      const n = child.name;
      if (n.startsWith("obstacle_6_")) {
        bars.push(child);
      } else if (n.startsWith("obstacle_1_002")) {
        child.rotation.y = Math.PI / 2;
        hammers.push(child);
      } else if (n.startsWith("coin_")) {
        coins.push(child);
        origY.set(child.uuid, child.position.y);
      } else if (n.startsWith("fire_")) {
        child.traverse((sub) => {
          if (sub instanceof THREE.Mesh && sub.material) {
            const mats = Array.isArray(sub.material) ? sub.material : [sub.material];
            const hasEmissive = mats.some((m: any) => 
              (m.name && m.name.toLowerCase().includes("emission")) ||
              (m.emissive && m.emissive.r + m.emissive.g + m.emissive.b > 0)
            );
            if (hasEmissive) {
              fires.push(sub);
            }
          }
        });
      }
    });

    crossedBarsRefs.current = bars;
    hammerRefs.current = hammers;
    coinRefs.current = coins;
    fireRefs.current = fires;
    coinOriginalY.current = origY;
    collectedCoins.current = new Set();

    return clone;
  }, [scene, round]);

  useFrame((state) => {
    const t = state.clock.elapsedTime;

    for (const bar of crossedBarsRefs.current) {
      bar.rotation.y = t * 1.5;
    }

    for (const hammer of hammerRefs.current) {
      hammer.rotation.x = Math.sin(t * 2.0) * 0.8;
    }

    const playerPos = playerRef?.current?.position;
    const coinWorldPos = new THREE.Vector3();
    for (const coin of coinRefs.current) {
      if (collectedCoins.current.has(coin.uuid)) continue;
      const baseY = coinOriginalY.current.get(coin.uuid) ?? coin.position.y;
      coin.position.y = baseY + Math.sin(t * 3 + coin.position.x * 2) * 0.15;
      coin.rotation.y = t * 2.5;

      if (playerPos) {
        coin.getWorldPosition(coinWorldPos);
        const dx = playerPos.x - coinWorldPos.x;
        const dy = playerPos.y - coinWorldPos.y;
        const dz = playerPos.z - coinWorldPos.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 2.0) {
          collectedCoins.current.add(coin.uuid);
          coin.visible = false;
          onCoinCollect?.();
        }
      }
    }

    for (const fire of fireRefs.current) {
      const seed = fire.position.x * 7.3 + fire.position.z * 3.7;
      const scaleY = 1.0 + Math.sin(t * 8 + seed) * 0.15 + Math.sin(t * 13 + seed * 0.5) * 0.1;
      const scaleX = 1.0 + Math.sin(t * 6 + seed * 1.2) * 0.08;
      fire.scale.set(scaleX, scaleY, scaleX);
    }

    const KNOCKBACK_COOLDOWN = 0.6;
    const canKnockback = (t - lastKnockbackTime.current) > KNOCKBACK_COOLDOWN;

    if (playerPos && onPlayerKnockback && canKnockback) {
      const HAMMER_HIT_RADIUS = 2.5;
      const BAR_HIT_RADIUS = 3.0;
      const KNOCKBACK_FORCE = 12;
      let wasHit = false;

      for (const hammer of hammerRefs.current) {
        hammer.getWorldPosition(obstacleWorldPos.current);
        const dx = playerPos.x - obstacleWorldPos.current.x;
        const dz = playerPos.z - obstacleWorldPos.current.z;
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist < HAMMER_HIT_RADIUS && dist > 0.01) {
          const nx = dx / dist;
          const nz = dz / dist;
          onPlayerKnockback(nx, nz, KNOCKBACK_FORCE);
          lastKnockbackTime.current = t;
          wasHit = true;
          break;
        }
      }

      if (!wasHit) {
        for (const bar of crossedBarsRefs.current) {
          bar.getWorldPosition(obstacleWorldPos.current);
          const dx = playerPos.x - obstacleWorldPos.current.x;
          const dz = playerPos.z - obstacleWorldPos.current.z;
          const dist = Math.sqrt(dx * dx + dz * dz);
          if (dist < BAR_HIT_RADIUS && dist > 0.01) {
            const nx = dx / dist;
            const nz = dz / dist;
            onPlayerKnockback(nx, nz, KNOCKBACK_FORCE * 0.8);
            lastKnockbackTime.current = t;
            break;
          }
        }
      }
    }
  });

  useEffect(() => {
    if (groupRef.current && onSurfaceHeight) {
      groupRef.current.updateWorldMatrix(true, true);
      const raycaster = new THREE.Raycaster();
      raycaster.set(new THREE.Vector3(0, 50, 0), new THREE.Vector3(0, -1, 0));
      const intersects = raycaster.intersectObject(groupRef.current, true);
      if (intersects.length > 0) {
        const surfaceY = intersects[0].point.y;
        console.log("Platform surface detected at Y:", surfaceY);
        onSurfaceHeight(surfaceY);
      } else {
        const box = new THREE.Box3().setFromObject(groupRef.current);
        console.log("Platform bounding box:", box.min.y, "to", box.max.y, "using max.y as surface");
        onSurfaceHeight(box.max.y);
      }
    }
  }, [platformScene, onSurfaceHeight]);

  return (
    <group ref={groupRef as React.RefObject<THREE.Group>} position={[0, 0, 0]} scale={[1, 1, 1]}>
      <primitive object={platformScene} />
    </group>
  );
}

function FireTorch({ position }: { position: [number, number, number] }) {
  const lightRef = useRef<THREE.PointLight>(null);
  const flameRef = useRef<THREE.Mesh>(null);

  useFrame((state) => {
    const t = state.clock.elapsedTime;
    if (lightRef.current) {
      lightRef.current.intensity = 1.5 + Math.sin(t * 8 + position[0]) * 0.5 + Math.sin(t * 12 + position[2]) * 0.3;
    }
    if (flameRef.current) {
      flameRef.current.scale.y = 1 + Math.sin(t * 10 + position[0] * 2) * 0.3;
      flameRef.current.scale.x = 1 + Math.sin(t * 7 + position[2]) * 0.15;
      flameRef.current.position.y = position[1] + 0.3 + Math.sin(t * 6) * 0.05;
    }
  });

  return (
    <group>
      <mesh ref={flameRef} position={position}>
        <coneGeometry args={[0.15, 0.4, 8]} />
        <meshBasicMaterial color="#FF6B00" transparent opacity={0.9} />
      </mesh>
      <mesh position={[position[0], position[1] + 0.25, position[2]]}>
        <coneGeometry args={[0.08, 0.25, 6]} />
        <meshBasicMaterial color="#FFAA00" transparent opacity={0.8} />
      </mesh>
      <pointLight
        ref={lightRef}
        position={[position[0], position[1] + 0.5, position[2]]}
        color="#FF6B00"
        intensity={1.5}
        distance={6}
        decay={2}
      />
    </group>
  );
}

function GlowingRune({ position, color = "#00D4FF", size = 0.3 }: { position: [number, number, number]; color?: string; size?: number }) {
  const meshRef = useRef<THREE.Mesh>(null);
  const glowRef = useRef<THREE.Mesh>(null);

  useFrame((state) => {
    const t = state.clock.elapsedTime;
    const pulse = 0.5 + Math.sin(t * 2 + position[0] * 3 + position[2] * 2) * 0.5;
    if (meshRef.current) {
      (meshRef.current.material as THREE.MeshBasicMaterial).opacity = 0.4 + pulse * 0.6;
    }
    if (glowRef.current) {
      glowRef.current.scale.setScalar(1 + pulse * 0.3);
      (glowRef.current.material as THREE.MeshBasicMaterial).opacity = 0.05 + pulse * 0.1;
    }
  });

  return (
    <group position={position}>
      <mesh ref={meshRef}>
        <circleGeometry args={[size, 6]} />
        <meshBasicMaterial color={color} transparent opacity={0.8} side={THREE.DoubleSide} />
      </mesh>
      <mesh ref={glowRef}>
        <circleGeometry args={[size * 2.5, 16]} />
        <meshBasicMaterial color={color} transparent opacity={0.1} side={THREE.DoubleSide} />
      </mesh>
      <pointLight color={color} intensity={0.3} distance={3} decay={2} />
    </group>
  );
}

function FloatingEmbers() {
  const count = 30;
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const dummy = useMemo(() => new THREE.Object3D(), []);

  const particleData = useMemo(() => {
    return Array.from({ length: count }, (_, i) => ({
      x: (Math.random() - 0.5) * 16,
      z: (Math.random() - 0.5) * 16,
      baseY: Math.random() * 4 + 2,
      speed: 0.3 + Math.random() * 0.5,
      phase: Math.random() * Math.PI * 2,
      drift: (Math.random() - 0.5) * 0.5,
    }));
  }, []);

  useFrame((state) => {
    if (!meshRef.current) return;
    const t = state.clock.elapsedTime;
    particleData.forEach((p, i) => {
      dummy.position.set(
        p.x + Math.sin(t * p.drift + p.phase) * 0.5,
        p.baseY + Math.sin(t * p.speed + p.phase) * 1.5,
        p.z + Math.cos(t * p.drift * 0.7 + p.phase) * 0.5
      );
      const s = 0.02 + Math.sin(t * 3 + p.phase) * 0.01;
      dummy.scale.setScalar(s);
      dummy.updateMatrix();
      meshRef.current!.setMatrixAt(i, dummy.matrix);
    });
    meshRef.current.instanceMatrix.needsUpdate = true;
  });

  return (
    <instancedMesh ref={meshRef} args={[undefined, undefined, count]}>
      <sphereGeometry args={[1, 6, 6]} />
      <meshBasicMaterial color="#FF6B00" transparent opacity={0.8} />
    </instancedMesh>
  );
}

function MysticalParticles() {
  const count = 20;
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const dummy = useMemo(() => new THREE.Object3D(), []);

  const particleData = useMemo(() => {
    return Array.from({ length: count }, () => ({
      x: (Math.random() - 0.5) * 20,
      z: (Math.random() - 0.5) * 20,
      baseY: Math.random() * 6 + 1,
      speed: 0.2 + Math.random() * 0.3,
      phase: Math.random() * Math.PI * 2,
    }));
  }, []);

  useFrame((state) => {
    if (!meshRef.current) return;
    const t = state.clock.elapsedTime;
    particleData.forEach((p, i) => {
      dummy.position.set(
        p.x + Math.sin(t * 0.3 + p.phase) * 1,
        p.baseY + Math.sin(t * p.speed + p.phase) * 2,
        p.z + Math.cos(t * 0.2 + p.phase) * 1
      );
      const s = 0.015 + Math.sin(t * 2 + p.phase) * 0.008;
      dummy.scale.setScalar(s);
      dummy.updateMatrix();
      meshRef.current!.setMatrixAt(i, dummy.matrix);
    });
    meshRef.current.instanceMatrix.needsUpdate = true;
  });

  return (
    <instancedMesh ref={meshRef} args={[undefined, undefined, count]}>
      <sphereGeometry args={[1, 6, 6]} />
      <meshBasicMaterial color="#00D4FF" transparent opacity={0.6} />
    </instancedMesh>
  );
}

function PlatformEnvironmentEffects({ surfaceY }: { surfaceY: number }) {
  const torchPositions: [number, number, number][] = useMemo(() => [
    [-4, surfaceY + 0.5, -3],
    [4, surfaceY + 0.5, -3],
    [-5, surfaceY + 0.5, 2],
    [5, surfaceY + 0.5, 2],
    [-3, surfaceY + 0.5, 5],
    [3, surfaceY + 0.5, 5],
    [0, surfaceY + 0.5, -6],
    [0, surfaceY + 0.5, 7],
  ], [surfaceY]);


  return (
    <>
      {torchPositions.map((pos, i) => (
        <FireTorch key={`torch_${i}`} position={pos} />
      ))}
      <MysticalParticles />
    </>
  );
}

function SceneBackground({ color }: { color: string }) {
  const { scene } = useThree();
  useEffect(() => {
    scene.background = new THREE.Color(color);
  }, [scene, color]);
  return null;
}

function GroundPlane() {
  return null;
}

function AltarMarker({ position, isActive, onInteract }: { 
  position: [number, number, number]; 
  isActive: boolean;
  onInteract: () => void;
}) {
  const meshRef = useRef<THREE.Mesh>(null);
  const glowRef = useRef<THREE.Mesh>(null);

  useFrame((state) => {
    if (meshRef.current) {
      meshRef.current.rotation.y = state.clock.elapsedTime;
      if (glowRef.current) {
        const pulse = 1 + Math.sin(state.clock.elapsedTime * 2) * 0.15;
        glowRef.current.scale.setScalar(pulse);
      }
    }
  });

  return (
    <group position={position}>
      <mesh ref={meshRef} position={[0, 1.5, 0]}>
        <octahedronGeometry args={[0.6, 0]} />
        <meshStandardMaterial
          color={isActive ? "#FACC15" : "#6B7280"}
          emissive={isActive ? "#FACC15" : "#374151"}
          emissiveIntensity={isActive ? 1 : 0.2}
          metalness={0.7}
          roughness={0.1}
        />
      </mesh>
      <mesh ref={glowRef} position={[0, 1.5, 0]}>
        <sphereGeometry args={[1, 16, 16]} />
        <meshBasicMaterial 
          color={isActive ? "#FACC15" : "#6B7280"} 
          transparent 
          opacity={isActive ? 0.15 : 0.05} 
        />
      </mesh>
      {isActive && (
        <pointLight position={[0, 2, 0]} color="#FACC15" intensity={1} distance={5} />
      )}
      <mesh position={[0, 0.05, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.8, 1.2, 32]} />
        <meshStandardMaterial 
          color={isActive ? "#FACC15" : "#4B5563"} 
          emissive={isActive ? "#FACC15" : "#1F2937"}
          emissiveIntensity={0.5}
        />
      </mesh>
    </group>
  );
}

function GameScene({
  config,
  round,
  altarActive,
  altarPosition,
  onAltarInteract,
  devMode,
  playerRef,
  isDragging,
  surfaceY,
  surfaceReady,
  onSurfaceHeight,
  playerScale,
  onCoinCollect,
}: {
  config: RingerConfig;
  round: number;
  altarActive: boolean;
  altarPosition: [number, number, number];
  onAltarInteract: () => void;
  devMode: boolean;
  playerRef: React.RefObject<THREE.Group | null>;
  isDragging: boolean;
  surfaceY: number;
  surfaceReady: boolean;
  onSurfaceHeight: (y: number) => void;
  playerScale: number;
  onCoinCollect: () => void;
}) {
  const roundThemes = useMemo(() => [
    { ambient: 0.7, dirColor: "#ffeedd", dirIntensity: 1.2, pointColor: "#8B5CF6", hemiSky: "#4db8c7", hemiGround: "#1a3a4a", hemiIntensity: 0.8, fogColor: "#1a5c6b", fogNear: 20, fogFar: 80, bgColor: "#1a5c6b" },
    { ambient: 0.6, dirColor: "#ffaa66", dirIntensity: 1.0, pointColor: "#EF4444", hemiSky: "#3d8a8f", hemiGround: "#2a1a0a", hemiIntensity: 0.7, fogColor: "#1a4a5a", fogNear: 20, fogFar: 75, bgColor: "#1a4a5a" },
    { ambient: 0.5, dirColor: "#88ffbb", dirIntensity: 1.0, pointColor: "#10B981", hemiSky: "#2a7a6a", hemiGround: "#0a2a1e", hemiIntensity: 0.7, fogColor: "#0f4a40", fogNear: 20, fogFar: 75, bgColor: "#0f4a40" },
  ], []);

  const theme = roundThemes[round] || roundThemes[0];
  const platformRef = useRef<THREE.Group>(null);
  const playerKnockbackRef = useRef<{ apply: (dirX: number, dirZ: number, force: number) => void } | null>(null);

  const handlePlayerKnockback = useCallback((dirX: number, dirZ: number, force: number) => {
    playerKnockbackRef.current?.apply(dirX, dirZ, force);
  }, []);

  return (
    <>

      <SceneBackground color={theme.bgColor} />
      <ambientLight intensity={theme.ambient} color="#c8e8f0" />
      <directionalLight
        position={[10, 15, 10]}
        intensity={theme.dirIntensity}
        color={theme.dirColor}
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
        shadow-camera-far={50}
        shadow-camera-left={-20}
        shadow-camera-right={20}
        shadow-camera-top={20}
        shadow-camera-bottom={-20}
      />
      <directionalLight
        position={[-8, 10, -5]}
        intensity={0.4}
        color="#6ec8d8"
      />
      <pointLight position={[0, 8, 0]} intensity={0.6} color={theme.pointColor} />
      <pointLight position={[15, 5, 15]} intensity={0.3} color="#4db8c7" distance={40} />
      <pointLight position={[-15, 5, -15]} intensity={0.3} color="#4db8c7" distance={40} />
      <hemisphereLight args={[theme.hemiSky, theme.hemiGround, theme.hemiIntensity]} />
      <fog attach="fog" args={[theme.fogColor, theme.fogNear, theme.fogFar]} />

      <Suspense fallback={null}>
        <UnderworldPlatform round={round} onSurfaceHeight={onSurfaceHeight} platformRef={platformRef} playerRef={playerRef} onCoinCollect={onCoinCollect} onPlayerKnockback={handlePlayerKnockback} />
      </Suspense>

      <GroundPlane />

      {surfaceReady && (
        <Suspense fallback={
          <mesh position={[0, surfaceY + 1, 0]}>
            <sphereGeometry args={[0.5, 16, 16]} />
            <meshStandardMaterial color="#ffff00" emissive="#ffff00" emissiveIntensity={1} />
          </mesh>
        }>
          <PlayerCharacter
            config={config}
            altarPosition={altarPosition}
            onAltarInteract={onAltarInteract}
            altarActive={altarActive}
            externalRef={playerRef}
            surfaceY={surfaceY}
            playerScale={playerScale}
            platformRef={platformRef}
            knockbackRef={playerKnockbackRef}
          />
        </Suspense>
      )}

      <AltarMarker 
        position={altarPosition} 
        isActive={altarActive}
        onInteract={onAltarInteract}
      />

      <PlatformEnvironmentEffects surfaceY={surfaceY} />

      {devMode ? (
        <DevCamera target={playerRef} orbitEnabled={!isDragging} />
      ) : (
        <IsometricCamera target={playerRef} />
      )}

      <Environment preset="night" />
    </>
  );
}

function TrialsHUD({
  round,
  ringerName,
  onPause,
  devMode,
  onToggleDevMode,
  gizmoMode,
  onCycleGizmo,
  onSavePositions,
  selectedObject,
  playerScale,
  onScaleChange,
  coinScore,
}: {
  round: number;
  ringerName: string;
  onPause: () => void;
  devMode: boolean;
  onToggleDevMode: () => void;
  gizmoMode: "translate" | "rotate" | "scale";
  onCycleGizmo: () => void;
  onSavePositions: () => void;
  selectedObject: string | null;
  playerScale: number;
  onScaleChange: (scale: number) => void;
  coinScore: number;
}) {
  const gizmoLabels = { translate: "Move", rotate: "Rotate", scale: "Scale" };

  return (
    <div className="absolute inset-0 pointer-events-none z-10">
      <div className="absolute top-4 left-4 pointer-events-auto">
        <div className="bg-black/60 backdrop-blur-md rounded-xl p-3 border border-white/10">
          <div className="flex items-center gap-2">
            <div className="text-white font-bold text-sm">{ringerName}</div>
            <div className="text-purple-400 text-xs font-semibold px-2 py-0.5 bg-purple-500/20 rounded-full">
              Round {round + 1}/3
            </div>
          </div>
          {coinScore > 0 && (
            <div className="flex items-center gap-1.5 mt-1.5">
              <div className="w-4 h-4 rounded-full bg-gradient-to-br from-yellow-300 to-yellow-600 border border-yellow-400/50 shadow-sm shadow-yellow-500/30" />
              <span className="text-yellow-400 font-bold text-sm">{coinScore}</span>
            </div>
          )}
        </div>
      </div>

      <div className="absolute top-4 right-4 pointer-events-auto flex gap-2 items-start">
        <div className="bg-black/60 backdrop-blur-md rounded-xl p-2 border border-white/10 flex items-center gap-2">
          <span className="text-white/60 text-xs">Size</span>
          <button
            onClick={() => onScaleChange(Math.max(0.5, playerScale - 0.5))}
            className="w-7 h-7 rounded-lg bg-white/10 text-white text-sm font-bold hover:bg-white/20 transition-all flex items-center justify-center"
          >
            -
          </button>
          <span className="text-white text-xs font-mono w-8 text-center">{playerScale.toFixed(1)}</span>
          <button
            onClick={() => onScaleChange(Math.min(10, playerScale + 0.5))}
            className="w-7 h-7 rounded-lg bg-white/10 text-white text-sm font-bold hover:bg-white/20 transition-all flex items-center justify-center"
          >
            +
          </button>
        </div>
        <button
          onClick={onPause}
          className="px-4 py-2 bg-black/60 backdrop-blur-md rounded-xl border border-white/10 text-white text-sm font-medium hover:bg-white/10 transition-all"
        >
          Menu
        </button>
      </div>


      <div className="absolute bottom-4 left-1/2 transform -translate-x-1/2 pointer-events-auto">
        <div className="bg-black/40 backdrop-blur-sm rounded-lg px-4 py-2 border border-white/5">
          <div className="flex gap-3 items-center text-white/50 text-xs">
            <span className="hidden md:inline">WASD - Move</span>
            <span className="hidden md:inline">Space - Jump</span>
            <span className="hidden md:inline">E - Interact</span>
            <div className="w-px h-4 bg-white/20 hidden md:block" />
            <button
              onClick={onToggleDevMode}
              className={`flex items-center gap-1 px-2 py-1 rounded-lg transition-all ${
                devMode ? "bg-cyan-500/30 text-cyan-400 border border-cyan-500/50" : "hover:bg-white/10 text-white/50"
              }`}
              title="Dev Cam"
            >
              <Camera size={14} />
              <span className="text-xs font-medium">Dev</span>
            </button>
            {devMode && (
              <>
                <button
                  onClick={onCycleGizmo}
                  className="flex items-center gap-1 px-2 py-1 rounded-lg bg-purple-500/30 text-purple-400 border border-purple-500/50 transition-all"
                  title={`Gizmo: ${gizmoLabels[gizmoMode]}`}
                >
                  <Move3D size={14} />
                  <span className="text-xs font-medium">{gizmoLabels[gizmoMode]}</span>
                </button>
                <button
                  onClick={onSavePositions}
                  className="flex items-center gap-1 px-2 py-1 rounded-lg bg-green-500/30 text-green-400 border border-green-500/50 hover:bg-green-500/40 transition-all"
                  title="Save & Download Positions"
                >
                  <Download size={14} />
                  <span className="text-xs font-medium">Save</span>
                </button>
              </>
            )}
          </div>
        </div>
      </div>

      {devMode && selectedObject && (
        <div className="absolute bottom-16 left-1/2 transform -translate-x-1/2 pointer-events-none">
          <div className="bg-cyan-500/20 backdrop-blur-md rounded-lg px-3 py-1.5 border border-cyan-500/30">
            <span className="text-cyan-400 text-xs font-medium">Selected: {selectedObject}</span>
          </div>
        </div>
      )}

    </div>
  );
}

export function RingerTrials() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const [round, setRound] = useState(0);
  const [surfaceY, setSurfaceY] = useState(1.5);
  const [surfaceReady, setSurfaceReady] = useState(false);
  const [coinScore, setCoinScore] = useState(0);

  const [altarActive, setAltarActive] = useState(true);
  const [showRoundTransition, setShowRoundTransition] = useState(false);
  const [showGrandFinale, setShowGrandFinale] = useState(false);

  const [devMode, setDevMode] = useState(false);
  const [selectedObject, setSelectedObject] = useState<string | null>(null);
  const [gizmoMode, setGizmoMode] = useState<"translate" | "rotate" | "scale">("translate");
  const [isDragging, setIsDragging] = useState(false);
  const [playerScale, setPlayerScale] = useState(2);
  const playerRef = useRef<THREE.Group>(null);
  const sceneObjectsRef = useRef<Map<string, THREE.Object3D>>(new Map());

  const handleSelectObject = useCallback((name: string, obj: THREE.Object3D) => {
    sceneObjectsRef.current.set(name, obj);
    setSelectedObject(name);
  }, []);

  const handleCycleGizmo = useCallback(() => {
    setGizmoMode((prev) => {
      if (prev === "translate") return "rotate";
      if (prev === "rotate") return "scale";
      return "translate";
    });
  }, []);

  const handleSavePositions = useCallback(() => {
    const data: Record<string, { position: [number, number, number]; rotation: [number, number, number]; scale: [number, number, number] }> = {};
    sceneObjectsRef.current.forEach((obj, name) => {
      data[name] = {
        position: [
          parseFloat(obj.position.x.toFixed(3)),
          parseFloat(obj.position.y.toFixed(3)),
          parseFloat(obj.position.z.toFixed(3)),
        ],
        rotation: [
          parseFloat(obj.rotation.x.toFixed(3)),
          parseFloat(obj.rotation.y.toFixed(3)),
          parseFloat(obj.rotation.z.toFixed(3)),
        ],
        scale: [
          parseFloat(obj.scale.x.toFixed(3)),
          parseFloat(obj.scale.y.toFixed(3)),
          parseFloat(obj.scale.z.toFixed(3)),
        ],
      };
    });

    if (playerRef.current) {
      const p = playerRef.current;
      data["player"] = {
        position: [
          parseFloat(p.position.x.toFixed(3)),
          parseFloat(p.position.y.toFixed(3)),
          parseFloat(p.position.z.toFixed(3)),
        ],
        rotation: [
          parseFloat(p.rotation.x.toFixed(3)),
          parseFloat(p.rotation.y.toFixed(3)),
          parseFloat(p.rotation.z.toFixed(3)),
        ],
        scale: [
          parseFloat(p.scale.x.toFixed(3)),
          parseFloat(p.scale.y.toFixed(3)),
          parseFloat(p.scale.z.toFixed(3)),
        ],
      };
    }

    const json = JSON.stringify({ round, timestamp: new Date().toISOString(), objects: data }, null, 2);
    const blob = new Blob([json], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `ringer-scene-round${round + 1}-${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }, [round]);

  const [ringerConfig, setRingerConfig] = useState<RingerConfig>(() => {
    try {
      const saved = localStorage.getItem("ringerCharacter");
      if (saved) return JSON.parse(saved);
    } catch {}
    return {
      name: "Ringer",
      selections: { face: "face_usual" },
      skinColor: "#FFD5B8",
    };
  });

  const handleCoinCollect = useCallback(() => {
    setCoinScore((prev) => prev + 1);
  }, []);

  const handleSurfaceHeight = useCallback((y: number) => {
    setSurfaceY(y);
    setSurfaceReady(true);
  }, []);

  const altarPosition = useMemo<[number, number, number]>(() => [0, surfaceY, -4], [surfaceY]);

  const handleAltarInteract = useCallback(() => {
    if (!altarActive) return;

    setAltarActive(false);

    if (round < 2) {
      setShowRoundTransition(true);
      setTimeout(() => {
        const nextRound = round + 1;
        setRound(nextRound);
        if (playerRef.current) {
          playerRef.current.position.set(0, surfaceY, 0);
        }
        setShowRoundTransition(false);
        setTimeout(() => setAltarActive(true), 1000);
      }, 2000);
    } else {
      setShowGrandFinale(true);
    }
  }, [altarActive, round, surfaceY]);

  const keyMap = useMemo(
    () => [
      { name: Controls.forward, keys: ["ArrowUp", "KeyW"] },
      { name: Controls.back, keys: ["ArrowDown", "KeyS"] },
      { name: Controls.left, keys: ["ArrowLeft", "KeyA"] },
      { name: Controls.right, keys: ["ArrowRight", "KeyD"] },
      { name: Controls.jump, keys: ["Space"] },
      { name: Controls.interact, keys: ["KeyE"] },
    ],
    []
  );

  return (
    <div className="fixed inset-0" style={{ backgroundColor: "#1a5c6b" }}>
      <KeyboardControls map={keyMap}>
        <Canvas
          key="ringer-trials-canvas"
          shadows
          camera={{ position: [8, 10, 8], fov: 50 }}
          style={{ position: "absolute", inset: 0 }}
          onCreated={({ gl }) => {
            gl.outputColorSpace = THREE.SRGBColorSpace;
            const canvas = gl.domElement;
            canvas.addEventListener("webglcontextlost", (e) => {
              e.preventDefault();
            });
          }}
        >
          <GameScene
            config={ringerConfig}
            round={round}
            altarActive={altarActive}
            altarPosition={altarPosition}
            onAltarInteract={handleAltarInteract}
            devMode={devMode}
            playerRef={playerRef}
            isDragging={isDragging}
            surfaceY={surfaceY}
            surfaceReady={surfaceReady}
            onSurfaceHeight={handleSurfaceHeight}
            playerScale={playerScale}
            onCoinCollect={handleCoinCollect}
          />
        </Canvas>

        <VirtualJoystick />
        <TouchActionButtons />

        <TrialsHUD
          round={round}
          ringerName={ringerConfig.name}
          onPause={() => setPhase("menu")}
          devMode={devMode}
          onToggleDevMode={() => {
            setDevMode((prev) => !prev);
            if (devMode) setSelectedObject(null);
          }}
          gizmoMode={gizmoMode}
          onCycleGizmo={handleCycleGizmo}
          onSavePositions={handleSavePositions}
          selectedObject={selectedObject}
          playerScale={playerScale}
          onScaleChange={setPlayerScale}
          coinScore={coinScore}
        />

        {showRoundTransition && (
          <div className="absolute inset-0 bg-black/80 flex items-center justify-center z-50 animate-fade-in">
            <div className="text-center">
              <h2 className="text-4xl font-bold text-white mb-2">Round {round + 2}</h2>
              <p className="text-white/60 text-lg">Entering next platform...</p>
            </div>
          </div>
        )}

        {showGrandFinale && (
          <div className="absolute inset-0 bg-gradient-to-b from-black via-purple-900/60 to-black flex items-center justify-center z-50">
            <div className="text-center max-w-lg mx-4">
              <div className="mb-6">
                <Sparkles size={48} className="text-yellow-400 mx-auto mb-3 animate-pulse" />
                <h1 className="text-5xl font-bold text-transparent bg-clip-text bg-gradient-to-r from-yellow-400 via-orange-500 to-red-500 mb-2">
                  Grand Altar
                </h1>
                <p className="text-white/70 text-lg">
                  {ringerConfig.name} channels the elemental powers!
                </p>
              </div>

              <div className="flex gap-3 justify-center">
                <button
                  onClick={() => {
                    setShowGrandFinale(false);
                    setRound(0);
                    setAltarActive(true);
                  }}
                  className="px-6 py-3 bg-gradient-to-r from-purple-500 to-violet-600 text-white font-bold rounded-full hover:shadow-lg hover:shadow-purple-500/30 transition-all active:scale-95"
                >
                  Play Again
                </button>
                <button
                  onClick={() => setPhase("menu")}
                  className="px-6 py-3 bg-white/10 text-white font-bold rounded-full hover:bg-white/20 transition-all active:scale-95"
                >
                  Return to Menu
                </button>
              </div>
            </div>
          </div>
        )}
      </KeyboardControls>
    </div>
  );
}
