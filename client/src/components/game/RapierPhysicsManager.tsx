import { useEffect, useState, useRef, useCallback } from "react";
import { useFrame } from "@react-three/fiber";
import { initRapier, isRapierReady, usePhysicsWorld } from "@/lib/physics";
import { usePhysicsArena } from "@/lib/physics/PhysicsArena";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useCameraEffects } from "@/lib/stores/useCameraEffects";
import { ARENA_RADIUS, isInGap, ZONE_SCORE_ORB } from "@/lib/arenaConstants";
import { triggerKnockoffCameraEffect, triggerCollisionCameraEffect } from "@/lib/stores/useCameraEffects";
import { triggerKnockoffFeel, triggerCollisionFeel, useGameFeel } from "@/lib/stores/useGameFeel";
import { useAudio } from "@/lib/stores/useAudio";
import { getPhysicsConfig, applyThemeOverrides } from "@/lib/physicsConfig";

function getPhysicsConstants() {
  const config = getPhysicsConfig();
  return {
    ZOOGI_RADIUS: config.zoogiRadius,
    SHOCKWAVE_RADIUS: config.shockwaveRadius,
    ARC_LANDING_VELOCITY_THRESHOLD: config.arcLandingVelocityThreshold,
    ORB_RADIUS: config.orbRadius,
    KNOCKOFF_Y_THRESHOLD: config.knockoffYThreshold,
    KNOCKOFF_RADIUS_THRESHOLD: config.arenaRadius + config.knockoffRadiusExtra,
    COLLISION_DISTANCE: config.zoogiRadius * 2 + config.collisionZoogiExtra,
    ORB_COLLISION_DISTANCE: config.zoogiRadius + config.orbRadius + config.collisionOrbExtra,
    MAX_VELOCITY: config.maxVelocity,
    MOVEMENT_STOPPED_THRESHOLD: config.movementStoppedThreshold,
    LAUNCH_IMPULSE_MULTIPLIER: config.launchImpulseMultiplier,
    ZOOGI_RESTITUTION: config.zoogiRestitution,
    ZOOGI_FRICTION: config.zoogiFriction,
    ZOOGI_LINEAR_DAMPING: config.zoogiLinearDamping,
    ZOOGI_ANGULAR_DAMPING: config.zoogiAngularDamping,
    ZOOGI_MASS: config.zoogiMass,
    ORB_RESTITUTION: config.orbRestitution,
    ORB_FRICTION: config.orbFriction,
    ORB_LINEAR_DAMPING: config.orbLinearDamping,
    ORB_ANGULAR_DAMPING: config.orbAngularDamping,
    SHOCKWAVE_COOLDOWN: config.shockwaveCooldown,
  };
}

interface EntityPhysicsMap {
  player: string | null;
  enemies: Map<string, string>;
  orbs: Map<string, string>;
}

export function RapierPhysicsManager() {
  const [initialized, setInitialized] = useState(false);
  const phase = useZoogiGame(state => state.phase);
  const selectedMap = useZoogiGame(state => state.selectedMap);
  const initialize = usePhysicsWorld(state => state.initialize);
  const isWorldInitialized = usePhysicsWorld(state => state.isInitialized);
  
  useEffect(() => {
    let mounted = true;
    
    const init = async () => {
      try {
        await initRapier();
        if (mounted) {
          await initialize();
          setInitialized(true);
          console.log("Rapier physics system ready");
        }
      } catch (error) {
        console.error("Failed to initialize Rapier:", error);
      }
    };
    
    if (phase === "playing" && !initialized) {
      init();
    }
    
    return () => {
      mounted = false;
    };
  }, [phase, initialized, initialize]);
  
  if (!initialized || !isWorldInitialized || phase !== "playing") {
    return null;
  }
  
  return <PhysicsRunner mapTheme={selectedMap || "grass"} />;
}

function PhysicsRunner({ mapTheme }: { mapTheme: string }) {
  applyThemeOverrides(mapTheme);
  usePhysicsArena({ mapTheme, hasGaps: true, skipWalls: false, skipFloor: false });
  
  const { 
    world, 
    isInitialized, 
    createDynamicBall, 
    removeBody, 
    applyImpulse, 
    setLinearVelocity, 
    setAngularVelocity, 
    getPosition, 
    getLinearVelocity,
    resetBody,
    getBody
  } = usePhysicsWorld();
  
  const phase = useZoogiGame(state => state.phase);
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const orbs = useZoogiGame(state => state.orbs);
  const slowMotionFactor = useZoogiGame(state => state.slowMotionFactor);
  const { isFrozen, computedTimeScale } = useCameraEffects();
  const { playSound } = useAudio();
  
  const entityMap = useRef<EntityPhysicsMap>({
    player: null,
    enemies: new Map(),
    orbs: new Map()
  });
  const frameCount = useRef(0);
  const lastCollisionChecks = useRef<Map<string, number>>(new Map());
  const entitiesCreated = useRef(false);
  const lastPlayerVelocityRef = useRef<[number, number, number]>([0, 0, 0]);
  const lastLaunchImpulseTime = useRef(0);
  const lastShockwaveTime = useRef(0);
  const lastEnemyVelocitiesRef = useRef<Map<string, [number, number, number]>>(new Map());
  
  const triggerPhoneVibration = useCallback((duration: number = 100) => {
    if (typeof navigator !== 'undefined' && 'vibrate' in navigator) {
      navigator.vibrate(duration);
    }
  }, []);
  
  const applyShockwave = useCallback((
    impactPos: { x: number; y: number; z: number },
    impactVelocityY: number,
    excludePhysicsId: string
  ) => {
    const { SHOCKWAVE_RADIUS, SHOCKWAVE_COOLDOWN } = getPhysicsConstants();
    const now = Date.now();
    if (now - lastShockwaveTime.current < SHOCKWAVE_COOLDOWN) return;
    lastShockwaveTime.current = now;
    
    const intensity = Math.min(1.5, Math.abs(impactVelocityY) / 8);
    
    triggerCollisionCameraEffect(intensity);
    triggerCollisionFeel(intensity);
    triggerPhoneVibration(Math.floor(intensity * 150));
    
    entityMap.current.orbs.forEach((physicsId, orbId) => {
      if (physicsId === excludePhysicsId) return;
      const orbPos = getPosition(physicsId);
      if (!orbPos) return;
      
      const dx = orbPos.x - impactPos.x;
      const dz = orbPos.z - impactPos.z;
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < SHOCKWAVE_RADIUS && dist > 0.1) {
        const force = (1 - dist / SHOCKWAVE_RADIUS) * intensity * 8;
        const pushForce = {
          x: (dx / dist) * force,
          y: 0.5 + intensity * 0.5,
          z: (dz / dist) * force
        };
        applyImpulse(physicsId, pushForce);
      }
    });
    
    entityMap.current.enemies.forEach((physicsId, zoogiId) => {
      if (physicsId === excludePhysicsId) return;
      const enemyPos = getPosition(physicsId);
      if (!enemyPos) return;
      
      const dx = enemyPos.x - impactPos.x;
      const dz = enemyPos.z - impactPos.z;
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < SHOCKWAVE_RADIUS && dist > 0.1) {
        const force = (1 - dist / SHOCKWAVE_RADIUS) * intensity * 6;
        const pushForce = {
          x: (dx / dist) * force,
          y: 0.3 + intensity * 0.3,
          z: (dz / dist) * force
        };
        applyImpulse(physicsId, pushForce);
        
        const store = useZoogiGame.getState();
        const enemy = store.enemies.find(e => e.zoogi.id === zoogiId);
        if (enemy) {
          store.updateEnemy(enemy.id, { lastHitByPlayer: true });
        }
      }
    });
    
    playSound("collision_enemy");
  }, [getPosition, applyImpulse, triggerPhoneVibration, playSound]);
  
  useEffect(() => {
    if (!isInitialized || !world || phase !== "playing" || entitiesCreated.current) return;
    if (!isRapierReady()) return;
    
    const { 
      ZOOGI_RADIUS, ZOOGI_RESTITUTION, ZOOGI_FRICTION, 
      ZOOGI_LINEAR_DAMPING, ZOOGI_ANGULAR_DAMPING, ZOOGI_MASS,
      ORB_RADIUS, ORB_RESTITUTION, ORB_FRICTION,
      ORB_LINEAR_DAMPING, ORB_ANGULAR_DAMPING
    } = getPhysicsConstants();
    
    if (playerEntity && !entityMap.current.player) {
      const physicsId = `player-${playerEntity.zoogi.id}`;
      createDynamicBall(
        physicsId,
        playerEntity.position,
        ZOOGI_RADIUS,
        "zoogi",
        {
          restitution: ZOOGI_RESTITUTION,
          friction: ZOOGI_FRICTION,
          linearDamping: ZOOGI_LINEAR_DAMPING,
          angularDamping: ZOOGI_ANGULAR_DAMPING,
          mass: ZOOGI_MASS,
          userData: { entityId: "player", zoogiId: playerEntity.zoogi.id }
        }
      );
      entityMap.current.player = physicsId;
      console.log("Created player physics body:", physicsId, "at", playerEntity.position);
    }
    
    enemies.forEach((enemy) => {
      const mapKey = enemy.zoogi.id;
      if (!entityMap.current.enemies.has(mapKey)) {
        const physicsId = `enemy-${enemy.zoogi.id}`;
        createDynamicBall(
          physicsId,
          enemy.position,
          ZOOGI_RADIUS,
          "zoogi",
          {
            restitution: ZOOGI_RESTITUTION,
            friction: ZOOGI_FRICTION,
            linearDamping: ZOOGI_LINEAR_DAMPING,
            angularDamping: ZOOGI_ANGULAR_DAMPING,
            mass: ZOOGI_MASS,
            userData: { entityId: enemy.id, zoogiId: enemy.zoogi.id }
          }
        );
        entityMap.current.enemies.set(mapKey, physicsId);
        console.log("Created enemy physics body:", physicsId, "at", enemy.position);
      }
    });
    
    orbs.forEach((orb) => {
      if (!entityMap.current.orbs.has(orb.id) && orb.isActive) {
        const physicsId = `orb-${orb.id}`;
        createDynamicBall(
          physicsId,
          orb.position,
          ORB_RADIUS,
          "orb",
          {
            restitution: ORB_RESTITUTION,
            friction: ORB_FRICTION,
            linearDamping: ORB_LINEAR_DAMPING,
            angularDamping: ORB_ANGULAR_DAMPING,
            mass: 0.5,
            userData: { orbId: orb.id, points: orb.points }
          }
        );
        entityMap.current.orbs.set(orb.id, physicsId);
      }
    });
    
    entitiesCreated.current = true;
    console.log("All physics entities created");
    
  }, [isInitialized, world, phase, playerEntity, enemies, orbs, createDynamicBall]);
  
  useEffect(() => {
    if (phase !== "playing") {
      if (entityMap.current.player) {
        removeBody(entityMap.current.player);
        entityMap.current.player = null;
      }
      entityMap.current.enemies.forEach((physicsId) => {
        removeBody(physicsId);
      });
      entityMap.current.enemies.clear();
      entityMap.current.orbs.forEach((physicsId) => {
        removeBody(physicsId);
      });
      entityMap.current.orbs.clear();
      entitiesCreated.current = false;
      console.log("Cleaned up all physics bodies");
    }
  }, [phase, removeBody]);
  
  useEffect(() => {
    if (!playerEntity || !entityMap.current.player) return;
    
    const { LAUNCH_IMPULSE_MULTIPLIER } = getPhysicsConstants();
    const lastVel = lastPlayerVelocityRef.current;
    const currentVel = playerEntity.velocity;
    
    const lastSpeed = Math.sqrt(lastVel[0] ** 2 + lastVel[2] ** 2);
    const currentSpeed = Math.sqrt(currentVel[0] ** 2 + currentVel[2] ** 2);
    
    const now = Date.now();
    if (currentSpeed > lastSpeed + 0.3 && currentSpeed > 0.5 && (now - lastLaunchImpulseTime.current > 2000)) {
      const impulse = {
        x: currentVel[0] * LAUNCH_IMPULSE_MULTIPLIER,
        y: 0.5,
        z: currentVel[2] * LAUNCH_IMPULSE_MULTIPLIER
      };
      applyImpulse(entityMap.current.player, impulse);
      lastLaunchImpulseTime.current = now;
      console.log("Applied launch impulse:", impulse);
    }
    
    lastPlayerVelocityRef.current = [...currentVel] as [number, number, number];
  }, [playerEntity?.velocity, applyImpulse]);
  
  useFrame((state, delta) => {
    if (phase !== "playing" || !isInitialized) return;
    
    if (isFrozen) return;
    
    const { MAX_VELOCITY, KNOCKOFF_Y_THRESHOLD, KNOCKOFF_RADIUS_THRESHOLD } = getPhysicsConstants();
    
    const adjustedDelta = delta * slowMotionFactor * computedTimeScale;
    usePhysicsWorld.getState().step(adjustedDelta);
    
    frameCount.current++;
    
    if (entityMap.current.player && playerEntity) {
      const vel = getLinearVelocity(entityMap.current.player);
      if (vel) {
        const speed = Math.sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (speed > MAX_VELOCITY) {
          const scale = MAX_VELOCITY / speed;
          setLinearVelocity(entityMap.current.player, {
            x: vel.x * scale,
            y: vel.y * scale,
            z: vel.z * scale
          });
        }
      }
      
      const pos = getPosition(entityMap.current.player);
      if (pos) {
        const distance = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
        const store = useZoogiGame.getState();
        const isRestricted = store.restrictionPhaseActive;
        // Only check knockoff during restriction phase (free roam mode has no knockoffs)
        if (isRestricted && (pos.y < KNOCKOFF_Y_THRESHOLD || distance > KNOCKOFF_RADIUS_THRESHOLD)) {
          console.log("Player knocked off arena!", { pos, distance });
          store.respawnPlayer();
          
          const spawnPos = store.playerEntity?.position;
          if (spawnPos) {
            resetBody(entityMap.current.player, { x: spawnPos[0], y: spawnPos[1], z: spawnPos[2] });
          }
          
          triggerKnockoffCameraEffect();
          triggerKnockoffFeel([pos.x, pos.y, pos.z]);
          playSound("fall_player");
        }
        
        // Real-time zone detection - check if player rolls through a zone ring
        const zoneConfigs = store.zoneEditorConfigs;
        const ZONE_CAPTURE_RADIUS = 4;
        
        for (const zoneConfig of zoneConfigs) {
          if (!zoneConfig.visible || zoneConfig.isSpawn) continue; // Skip spawn zones
          
          const zoneX = Math.cos(zoneConfig.angle) * zoneConfig.distance;
          const zoneZ = Math.sin(zoneConfig.angle) * zoneConfig.distance;
          const dx = pos.x - zoneX;
          const dz = pos.z - zoneZ;
          const distToZone = Math.sqrt(dx * dx + dz * dz);
          
          if (distToZone < ZONE_CAPTURE_RADIUS && !store.isZoneScoredThisTurn(zoneConfig.id)) {
            // Player entered this zone - award points and change ring color
            store.markZoneScored(zoneConfig.id);
            const playerColor = store.playerEntity?.zoogi?.color || "#00ff00";
            store.addScore(ZONE_SCORE_ORB);
            store.updateZoneOwnership(zoneConfig.id, playerColor);
            store.triggerKnockoffBoundaryFlash(playerColor);
            console.log(`Zone ${zoneConfig.id} scored! +${ZONE_SCORE_ORB} points, ring changed to ${playerColor}`);
            playSound("orb_collect");
          }
        }
      }
    }
    
    entityMap.current.enemies.forEach((physicsId, zoogiId) => {
      const vel = getLinearVelocity(physicsId);
      if (vel) {
        const speed = Math.sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (speed > MAX_VELOCITY) {
          const scale = MAX_VELOCITY / speed;
          setLinearVelocity(physicsId, {
            x: vel.x * scale,
            y: vel.y * scale,
            z: vel.z * scale
          });
        }
      }
      
      const pos = getPosition(physicsId);
      if (pos) {
        const distance = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
        const store = useZoogiGame.getState();
        const isRestricted = store.restrictionPhaseActive;
        // Only check knockoff during restriction phase (free roam mode has no knockoffs)
        if (isRestricted && (pos.y < KNOCKOFF_Y_THRESHOLD || distance > KNOCKOFF_RADIUS_THRESHOLD)) {
          const enemy = store.enemies.find(e => e.zoogi.id === zoogiId);
          if (enemy && enemy.lastHitByPlayer) {
            console.log("Enemy knocked off by player!", zoogiId);
            store.scoreKnockOff("enemy");
            triggerKnockoffCameraEffect();
            triggerKnockoffFeel([pos.x, pos.y, pos.z]);
            playSound("fall_enemy");
          }
          
          if (enemy) {
            store.respawnEnemy(enemy.id);
            const respawnedEnemy = store.enemies.find(e => e.id === enemy.id);
            if (respawnedEnemy) {
              resetBody(physicsId, { 
                x: respawnedEnemy.position[0], 
                y: respawnedEnemy.position[1], 
                z: respawnedEnemy.position[2] 
              });
            }
          }
        }
      }
    });
    
    entityMap.current.orbs.forEach((physicsId, orbId) => {
      const pos = getPosition(physicsId);
      if (pos) {
        const distance = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
        let shouldRemove = false;
        
        // Always check Y threshold for falling off
        if (pos.y < KNOCKOFF_Y_THRESHOLD) {
          shouldRemove = true;
        }
        
        // Check radial knockoff with gap handling
        if (!shouldRemove && distance > KNOCKOFF_RADIUS_THRESHOLD) {
          // If in gap zone, only knock off if much further beyond the threshold
          const inGap = isInGap(pos.x, pos.z, 0.4); // 0.4 = orb radius
          const maxGapDistance = KNOCKOFF_RADIUS_THRESHOLD + 3; // Allow 3 units through gap
          
          if (!inGap || distance > maxGapDistance) {
            shouldRemove = true;
          }
        }
        
        if (shouldRemove) {
          const store = useZoogiGame.getState();
          store.removeOrb(orbId);
          removeBody(physicsId);
          entityMap.current.orbs.delete(orbId);
          playSound("fall_orb");
        }
      }
    });
    
    checkCollisions();
  });
  
  const checkOrbToOrbCollisions = useCallback(() => {
    const { ORB_RADIUS } = getPhysicsConstants();
    const orbCollisionDist = ORB_RADIUS * 2 + 0.1;
    const orbIds = Array.from(entityMap.current.orbs.keys());
    
    for (let i = 0; i < orbIds.length; i++) {
      for (let j = i + 1; j < orbIds.length; j++) {
        const orbId1 = orbIds[i];
        const orbId2 = orbIds[j];
        const physicsId1 = entityMap.current.orbs.get(orbId1);
        const physicsId2 = entityMap.current.orbs.get(orbId2);
        
        if (!physicsId1 || !physicsId2) continue;
        
        const pos1 = getPosition(physicsId1);
        const pos2 = getPosition(physicsId2);
        
        if (!pos1 || !pos2) continue;
        
        const dx = pos2.x - pos1.x;
        const dy = pos2.y - pos1.y;
        const dz = pos2.z - pos1.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        
        if (dist < orbCollisionDist && dist > 0.01) {
          const vel1 = getLinearVelocity(physicsId1);
          const vel2 = getLinearVelocity(physicsId2);
          
          if (!vel1 || !vel2) continue;
          
          const speed1 = Math.sqrt(vel1.x ** 2 + vel1.z ** 2);
          const speed2 = Math.sqrt(vel2.x ** 2 + vel2.z ** 2);
          
          if (speed1 < 0.1 && speed2 < 0.1) continue;
          
          const normalX = dx / dist;
          const normalZ = dz / dist;
          
          const scatterForce = Math.max(speed1, speed2) * 1.5 + 0.5;
          
          const perpX = -normalZ;
          const perpZ = normalX;
          
          const relVelX = vel1.x - vel2.x;
          const relVelZ = vel1.z - vel2.z;
          const perpDot = relVelX * perpX + relVelZ * perpZ;
          const scatterSide = perpDot >= 0 ? 1 : -1;
          
          const push1 = {
            x: (-normalX + perpX * scatterSide * 0.3) * scatterForce,
            y: 0.35,
            z: (-normalZ + perpZ * scatterSide * 0.3) * scatterForce
          };
          
          const push2 = {
            x: (normalX - perpX * scatterSide * 0.3) * scatterForce,
            y: 0.35,
            z: (normalZ - perpZ * scatterSide * 0.3) * scatterForce
          };
          
          applyImpulse(physicsId1, push1);
          applyImpulse(physicsId2, push2);
        }
      }
    }
  }, [getPosition, getLinearVelocity, applyImpulse]);
  
  const checkCollisions = useCallback(() => {
    if (!entityMap.current.player) return;
    
    const { COLLISION_DISTANCE, ORB_COLLISION_DISTANCE, ARC_LANDING_VELOCITY_THRESHOLD } = getPhysicsConstants();
    
    checkOrbToOrbCollisions();
    
    const store = useZoogiGame.getState();
    const playerPos = getPosition(entityMap.current.player);
    if (!playerPos) return;
    
    const now = Date.now();
    
    entityMap.current.enemies.forEach((physicsId, zoogiId) => {
      const enemyPos = getPosition(physicsId);
      if (!enemyPos) return;
      
      const dx = playerPos.x - enemyPos.x;
      const dz = playerPos.z - enemyPos.z;
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < COLLISION_DISTANCE) {
        const collisionKey = `player-${zoogiId}`;
        const lastCollision = lastCollisionChecks.current.get(collisionKey) || 0;
        
        if (now - lastCollision > 100) {
          lastCollisionChecks.current.set(collisionKey, now);
          
          const playerVel = getLinearVelocity(entityMap.current.player!);
          const enemyVel = getLinearVelocity(physicsId);
          
          if (playerVel && enemyVel) {
            const playerSpeed = Math.sqrt(playerVel.x ** 2 + playerVel.z ** 2);
            const enemySpeed = Math.sqrt(enemyVel.x ** 2 + enemyVel.z ** 2);
            
            const enemy = store.enemies.find(e => e.zoogi.id === zoogiId);
            if (enemy && playerSpeed > enemySpeed * 0.5) {
              store.updateEnemy(enemy.id, { lastHitByPlayer: true });
            }
            
            const isArcLanding = playerVel.y < ARC_LANDING_VELOCITY_THRESHOLD;
            
            if (isArcLanding) {
              applyShockwave(playerPos, playerVel.y, entityMap.current.player!);
              useZoogiGame.setState({ lastCollisionTime: Date.now() });
            } else {
              const impactStrength = Math.min(1, (playerSpeed + enemySpeed) / 3);
              if (impactStrength > 0.2) {
                triggerCollisionCameraEffect(impactStrength);
                triggerCollisionFeel(impactStrength);
                const collisionPos: [number, number, number] = [
                  (playerPos.x + enemyPos.x) / 2,
                  (playerPos.y + enemyPos.y) / 2 + 0.5,
                  (playerPos.z + enemyPos.z) / 2
                ];
                const playerColor = store.playerEntity?.zoogi?.color || "#ff7700";
                const enemyObj = store.enemies.find(e => e.zoogi.id === zoogiId);
                const enemyColor = enemyObj?.zoogi?.color || "#ffffff";
                useGameFeel.getState().triggerCollisionBurst(collisionPos, impactStrength, playerColor, enemyColor);
                playSound("collision_enemy");
                useZoogiGame.setState({ lastCollisionTime: Date.now() });
              }
            }
          }
        }
      }
    });
    
    entityMap.current.orbs.forEach((physicsId, orbId) => {
      const orbPos = getPosition(physicsId);
      if (!orbPos) return;
      
      const dx = playerPos.x - orbPos.x;
      const dz = playerPos.z - orbPos.z;
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < ORB_COLLISION_DISTANCE) {
        const orb = store.orbs.find(o => o.id === orbId && o.isActive);
        if (orb) {
          store.updateOrb(orbId, { lastHitBy: "player" });
          
          const playerVel = getLinearVelocity(entityMap.current.player!);
          if (playerVel) {
            const isArcLanding = playerVel.y < ARC_LANDING_VELOCITY_THRESHOLD;
            
            if (isArcLanding) {
              applyShockwave(playerPos, playerVel.y, entityMap.current.player!);
              
              const pushForce = {
                x: (orbPos.x - playerPos.x) * 5,
                y: 1.0 + Math.abs(playerVel.y) * 0.2,
                z: (orbPos.z - playerPos.z) * 5
              };
              applyImpulse(physicsId, pushForce);
            } else {
              const pushForce = {
                x: (orbPos.x - playerPos.x) * 3,
                y: 0.5,
                z: (orbPos.z - playerPos.z) * 3
              };
              applyImpulse(physicsId, pushForce);
              playSound("collision_enemy");
            }
          }
        }
      }
    });
  }, [getPosition, getLinearVelocity, applyImpulse, applyShockwave, playSound, checkOrbToOrbCollisions]);
  
  return null;
}
