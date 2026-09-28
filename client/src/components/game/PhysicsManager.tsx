import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { useCameraEffects } from "@/lib/stores/useCameraEffects";
import { useRef } from "react";

export function PhysicsManager() {
  const lastTurnEndRef = useRef(0);
  const lastIntensityCheckRef = useRef(0);
  const recentCollisionsRef = useRef(0);
  const collisionDecayRef = useRef(0);
  
  useFrame((state, delta) => {
    const store = useZoogiGame.getState();
    const cameraEffects = useCameraEffects.getState();
    
    if (store.phase !== "playing") return;
    
    // Skip physics during freeze frame
    if (cameraEffects.isFrozen) return;
    
    // Apply both slow motion and camera effect timeScale
    const adjustedDelta = delta * store.slowMotionFactor * cameraEffects.computedTimeScale;
    store.physicsTick(adjustedDelta);
    store.tickTimers(adjustedDelta);
    
    const { playerEntity, enemies, isPlayerTurn, turnIndex, endTurn, setMovementStopped, allMovementStopped, lastCollisionTime } = useZoogiGame.getState();
    
    if (!playerEntity) return;
    
    const currentTime = state.clock.elapsedTime;
    if (currentTime - lastIntensityCheckRef.current > 0.3) {
      lastIntensityCheckRef.current = currentTime;
      
      const playerSpeed = Math.sqrt(
        playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2
      );
      
      const now = Date.now();
      if (now - lastCollisionTime < 1500) {
        recentCollisionsRef.current = Math.min(recentCollisionsRef.current + 1, 5);
        collisionDecayRef.current = currentTime;
      } else if (currentTime - collisionDecayRef.current > 2) {
        recentCollisionsRef.current = Math.max(0, recentCollisionsRef.current - 1);
        collisionDecayRef.current = currentTime;
      }
      
      let nearbyEnemies = 0;
      const px = playerEntity.position[0];
      const pz = playerEntity.position[2];
      for (const enemy of enemies) {
        const dist = Math.sqrt(
          (enemy.position[0] - px) ** 2 + (enemy.position[2] - pz) ** 2
        );
        if (dist < 6) nearbyEnemies++;
      }
      
      useAudio.getState().updateIntensityFromGameState(
        playerSpeed,
        recentCollisionsRef.current,
        nearbyEnemies
      );
    }
    
    const { gameMode } = useZoogiGame.getState();
    const isFreeForAll = gameMode === "ringer_royale";
    
    if (!isFreeForAll) {
      if (!isPlayerTurn && turnIndex < enemies.length && enemies[turnIndex]?.isStunned && allMovementStopped) {
        console.log(`PhysicsManager: Enemy ${enemies[turnIndex].zoogi.name} is stunned, forcing turn skip`);
        endTurn();
        lastTurnEndRef.current = state.clock.elapsedTime;
        return;
      }
      
      const allEntities = [playerEntity, ...enemies];
      const allStopped = allEntities.every(e => {
        const speed = Math.sqrt(e.velocity[0] ** 2 + e.velocity[2] ** 2);
        return speed < 0.02;
      });
      
      if (allStopped && currentTime - lastTurnEndRef.current > 0.5) {
        const currentEntity = isPlayerTurn ? playerEntity : enemies[turnIndex];
        if (currentEntity) {
          const speed = Math.sqrt(currentEntity.velocity[0] ** 2 + currentEntity.velocity[2] ** 2);
          if (speed < 0.02) {
            setMovementStopped(true);
          }
        }
      }
    }
  });
  
  return null;
}
