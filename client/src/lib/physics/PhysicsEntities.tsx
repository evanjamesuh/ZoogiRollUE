import { useEffect, useRef, useCallback } from "react";
import { useFrame } from "@react-three/fiber";
import { usePhysicsWorld } from "./usePhysicsWorld";
import { useZoogiGame } from "../stores/useZoogiGame";
import { isRapierReady } from "./rapierInit";
import { ARENA_RADIUS } from "../arenaConstants";
import { triggerKnockoffCameraEffect, triggerCollisionCameraEffect } from "../stores/useCameraEffects";
import { triggerKnockoffFeel, triggerCollisionFeel } from "../stores/useGameFeel";

const ZOOGI_RADIUS = 0.5;
const ORB_RADIUS = 0.3;
const KNOCKOFF_Y_THRESHOLD = -2;
const KNOCKOFF_RADIUS_THRESHOLD = ARENA_RADIUS + 3;
const COLLISION_CHECK_INTERVAL = 5;
const MAX_VELOCITY = 15;

interface EntityPhysicsMap {
  player: string | null;
  enemies: Map<string, string>;
  orbs: Map<string, string>;
}

export function usePhysicsEntities() {
  const { world, isInitialized, createDynamicBall, removeBody, applyImpulse, setLinearVelocity, setAngularVelocity, getPosition, getLinearVelocity } = usePhysicsWorld();
  const phase = useZoogiGame(state => state.phase);
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const orbs = useZoogiGame(state => state.orbs);
  const updatePlayerPosition = useZoogiGame(state => state.updatePlayerPosition);
  const updateEnemy = useZoogiGame(state => state.updateEnemy);
  const updateOrb = useZoogiGame(state => state.updateOrb);
  const removeOrb = useZoogiGame(state => state.removeOrb);
  const scoreKnockOff = useZoogiGame(state => state.scoreKnockOff);
  
  const entityMap = useRef<EntityPhysicsMap>({
    player: null,
    enemies: new Map(),
    orbs: new Map()
  });
  const frameCount = useRef(0);
  const lastCollisions = useRef<Set<string>>(new Set());
  
  useEffect(() => {
    if (!isInitialized || !world || phase !== "playing") return;
    if (!isRapierReady()) return;
    
    if (playerEntity && !entityMap.current.player) {
      const physicsId = `player-${playerEntity.zoogi.id}`;
      createDynamicBall(
        physicsId,
        playerEntity.position,
        ZOOGI_RADIUS,
        "zoogi",
        {
          restitution: 0.9,
          friction: 0.3,
          linearDamping: 0.5,
          angularDamping: 0.6,
          mass: 1.0,
          userData: { entityId: "player", zoogiId: playerEntity.zoogi.id }
        }
      );
      entityMap.current.player = physicsId;
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
            restitution: 0.9,
            friction: 0.3,
            linearDamping: 0.5,
            angularDamping: 0.6,
            mass: 1.0,
            userData: { entityId: mapKey, zoogiId: enemy.zoogi.id }
          }
        );
        entityMap.current.enemies.set(mapKey, physicsId);
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
            restitution: 0.95,
            friction: 0.2,
            linearDamping: 0.3,
            angularDamping: 0.4,
            mass: 0.3,
            userData: { orbId: orb.id, points: orb.points }
          }
        );
        entityMap.current.orbs.set(orb.id, physicsId);
      }
    });
    
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
    }
  }, [phase, removeBody]);
  
  const syncPositionsToGame = useCallback(() => {
    if (!isInitialized) return;
    
    // Check if restriction phase is active (free roam mode disables knockoffs)
    const isRestricted = useZoogiGame.getState().restrictionPhaseActive;
    
    if (entityMap.current.player) {
      const pos = getPosition(entityMap.current.player);
      if (pos) {
        updatePlayerPosition([pos.x, pos.y, pos.z]);
        
        // Only check knockoff during restriction phase
        if (isRestricted) {
          const dist = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
          if (pos.y < KNOCKOFF_Y_THRESHOLD || dist > KNOCKOFF_RADIUS_THRESHOLD) {
            triggerKnockoffCameraEffect();
            triggerKnockoffFeel([pos.x, pos.y, pos.z]);
            scoreKnockOff("enemy");
          }
        }
      }
    }
    
    enemies.forEach((enemy) => {
      const physicsId = entityMap.current.enemies.get(enemy.zoogi.id);
      if (physicsId) {
        const pos = getPosition(physicsId);
        if (pos) {
          updateEnemy(enemy.id, { position: [pos.x, pos.y, pos.z] });
          
          // Only check knockoff during restriction phase
          if (isRestricted) {
            const dist = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
            if (pos.y < KNOCKOFF_Y_THRESHOLD || dist > KNOCKOFF_RADIUS_THRESHOLD) {
              triggerKnockoffCameraEffect();
              triggerKnockoffFeel([pos.x, pos.y, pos.z]);
              scoreKnockOff("enemy", 100);
            }
          }
        }
      }
    });
    
    orbs.forEach((orb) => {
      if (!orb.isActive) return;
      const physicsId = entityMap.current.orbs.get(orb.id);
      if (physicsId) {
        const pos = getPosition(physicsId);
        if (pos) {
          updateOrb(orb.id, { position: [pos.x, pos.y, pos.z] });
          
          // Only check knockoff during restriction phase
          if (isRestricted) {
            const dist = Math.sqrt(pos.x * pos.x + pos.z * pos.z);
            if (pos.y < KNOCKOFF_Y_THRESHOLD || dist > KNOCKOFF_RADIUS_THRESHOLD) {
              scoreKnockOff("orb", orb.points);
              removeOrb(orb.id);
              removeBody(physicsId);
              entityMap.current.orbs.delete(orb.id);
            }
          }
        }
      }
    });
  }, [isInitialized, getPosition, enemies, orbs, updatePlayerPosition, updateEnemy, updateOrb, scoreKnockOff, removeOrb, removeBody]);
  
  const checkCollisions = useCallback(() => {
    if (!world || !isInitialized) return;
    
    const playerPos = entityMap.current.player ? getPosition(entityMap.current.player) : null;
    if (!playerPos) return;
    
    orbs.forEach((orb) => {
      if (!orb.isActive) return;
      const physicsId = entityMap.current.orbs.get(orb.id);
      if (!physicsId) return;
      
      const orbPos = getPosition(physicsId);
      if (!orbPos) return;
      
      const dx = playerPos.x - orbPos.x;
      const dy = playerPos.y - orbPos.y;
      const dz = playerPos.z - orbPos.z;
      const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
      
      if (dist < ZOOGI_RADIUS + ORB_RADIUS + 0.2) {
        removeOrb(orb.id);
        removeBody(physicsId);
        entityMap.current.orbs.delete(orb.id);
      }
    });
    
    enemies.forEach((enemy) => {
      const enemyPhysicsId = entityMap.current.enemies.get(enemy.zoogi.id);
      if (!enemyPhysicsId) return;
      
      const enemyPos = getPosition(enemyPhysicsId);
      if (!enemyPos) return;
      
      const dx = playerPos.x - enemyPos.x;
      const dy = playerPos.y - enemyPos.y;
      const dz = playerPos.z - enemyPos.z;
      const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
      
      const collisionKey = `player-${enemy.zoogi.id}`;
      if (dist < ZOOGI_RADIUS * 2 + 0.1) {
        if (!lastCollisions.current.has(collisionKey)) {
          lastCollisions.current.add(collisionKey);
          triggerCollisionCameraEffect(1.0);
          triggerCollisionFeel(1.0);
        }
      } else {
        lastCollisions.current.delete(collisionKey);
      }
    });
  }, [world, isInitialized, getPosition, orbs, enemies, removeOrb, removeBody]);
  
  useFrame((_, delta) => {
    if (phase !== "playing" || !isInitialized) return;
    
    usePhysicsWorld.getState().step(delta);
    
    syncPositionsToGame();
    
    frameCount.current++;
    if (frameCount.current % COLLISION_CHECK_INTERVAL === 0) {
      checkCollisions();
    }
    
    if (entityMap.current.player) {
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
    }
    
    entityMap.current.enemies.forEach((physicsId) => {
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
    });
  });
  
  const launchEntity = useCallback((entityType: "player" | "enemy", entityId: string | null, impulse: { x: number; y: number; z: number }) => {
    if (entityType === "player" && entityMap.current.player) {
      applyImpulse(entityMap.current.player, impulse);
    } else if (entityType === "enemy" && entityId) {
      const physicsId = entityMap.current.enemies.get(entityId);
      if (physicsId) {
        applyImpulse(physicsId, impulse);
      }
    }
  }, [applyImpulse]);
  
  const freezeEntity = useCallback((entityType: "player" | "enemy", entityId: string | null) => {
    if (entityType === "player" && entityMap.current.player) {
      setLinearVelocity(entityMap.current.player, { x: 0, y: 0, z: 0 });
      setAngularVelocity(entityMap.current.player, { x: 0, y: 0, z: 0 });
    } else if (entityType === "enemy" && entityId) {
      const physicsId = entityMap.current.enemies.get(entityId);
      if (physicsId) {
        setLinearVelocity(physicsId, { x: 0, y: 0, z: 0 });
        setAngularVelocity(physicsId, { x: 0, y: 0, z: 0 });
      }
    }
  }, [setLinearVelocity, setAngularVelocity]);
  
  const applyRadialImpulse = useCallback((center: { x: number; y: number; z: number }, radius: number, force: number) => {
    if (entityMap.current.player) {
      const pos = getPosition(entityMap.current.player);
      if (pos) {
        const dx = pos.x - center.x;
        const dy = pos.y - center.y;
        const dz = pos.z - center.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        
        if (dist < radius && dist > 0.1) {
          const falloff = 1 - (dist / radius);
          const impulseStrength = force * falloff;
          applyImpulse(entityMap.current.player, {
            x: (dx / dist) * impulseStrength,
            y: 0.3 * impulseStrength,
            z: (dz / dist) * impulseStrength
          });
        }
      }
    }
    
    entityMap.current.enemies.forEach((physicsId) => {
      const pos = getPosition(physicsId);
      if (pos) {
        const dx = pos.x - center.x;
        const dy = pos.y - center.y;
        const dz = pos.z - center.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        
        if (dist < radius && dist > 0.1) {
          const falloff = 1 - (dist / radius);
          const impulseStrength = force * falloff;
          applyImpulse(physicsId, {
            x: (dx / dist) * impulseStrength,
            y: 0.3 * impulseStrength,
            z: (dz / dist) * impulseStrength
          });
        }
      }
    });
    
    entityMap.current.orbs.forEach((physicsId) => {
      const pos = getPosition(physicsId);
      if (pos) {
        const dx = pos.x - center.x;
        const dy = pos.y - center.y;
        const dz = pos.z - center.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        
        if (dist < radius && dist > 0.1) {
          const falloff = 1 - (dist / radius);
          const impulseStrength = force * falloff * 0.5;
          applyImpulse(physicsId, {
            x: (dx / dist) * impulseStrength,
            y: 0.2 * impulseStrength,
            z: (dz / dist) * impulseStrength
          });
        }
      }
    });
  }, [getPosition, applyImpulse]);
  
  const getEntityPhysicsId = useCallback((entityType: "player" | "enemy", entityId?: string): string | null => {
    if (entityType === "player") {
      return entityMap.current.player;
    } else if (entityId) {
      return entityMap.current.enemies.get(entityId) || null;
    }
    return null;
  }, []);
  
  return {
    launchEntity,
    freezeEntity,
    applyRadialImpulse,
    getEntityPhysicsId,
    entityMap: entityMap.current
  };
}
