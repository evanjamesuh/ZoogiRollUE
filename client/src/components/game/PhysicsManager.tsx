import { useFrame } from "@react-three/fiber";
import { useRef } from "react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { useCameraEffects } from "@/lib/stores/useCameraEffects";
import { FIXED_DT, stepAccumulator } from "@/lib/fixedTimestep";
import { setInterpAlpha, setInterpFrame, type Vec3 } from "@/lib/renderInterp";
import { SETTLE_GRACE_SECONDS } from "@/lib/simFeel";

function captureBodies(): { id: string; pos: Vec3 }[] {
  const state = useZoogiGame.getState();
  const entries: { id: string; pos: Vec3 }[] = [];
  if (state.playerEntity) {
    const position = state.playerEntity.position;
    entries.push({ id: state.playerEntity.id, pos: [position[0], position[1], position[2]] });
  }
  for (const enemy of state.enemies) {
    entries.push({ id: enemy.id, pos: [enemy.position[0], enemy.position[1], enemy.position[2]] });
  }
  for (const orb of state.orbs) {
    if (!orb.isActive) continue;
    entries.push({ id: orb.id, pos: [orb.position[0], orb.position[1], orb.position[2]] });
  }
  return entries;
}

export function PhysicsManager() {
  const lastTurnEndRef = useRef(0);
  const lastIntensityCheckRef = useRef(0);
  const recentCollisionsRef = useRef(0);
  const collisionDecayRef = useRef(0);
  const accumulatorRef = useRef(0);

  // Priority 1 runs before the meshes read positions, so interpolation is fresh.
  useFrame((state, delta) => {
    const store = useZoogiGame.getState();
    const cameraEffects = useCameraEffects.getState();

    if (store.phase !== "playing") return;

    if (cameraEffects.isFrozen) return;

    const timeScale = store.slowMotionFactor * cameraEffects.computedTimeScale;
    const stepped = stepAccumulator(accumulatorRef.current, delta, timeScale);
    accumulatorRef.current = stepped.accumulator;

    let previous = captureBodies();
    for (let i = 0; i < stepped.steps; i++) {
      previous = captureBodies();
      store.physicsTick(FIXED_DT);
    }

    if (stepped.steps > 0) {
      const current = captureBodies();
      const currentById = new Map(current.map((entry) => [entry.id, entry.pos]));
      setInterpFrame(
        previous.map((entry) => ({
          id: entry.id,
          prev: entry.pos,
          curr: currentById.get(entry.id) ?? entry.pos,
        })),
        stepped.alpha,
      );
    } else {
      setInterpAlpha(stepped.alpha);
    }

    // The match clock follows real time. The sim above does not.
    store.tickTimers(Math.min(delta, 0.1) * timeScale);

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

      if (allStopped && currentTime - lastTurnEndRef.current > SETTLE_GRACE_SECONDS) {
        const currentEntity = isPlayerTurn ? playerEntity : enemies[turnIndex];
        if (currentEntity) {
          const speed = Math.sqrt(currentEntity.velocity[0] ** 2 + currentEntity.velocity[2] ** 2);
          if (speed < 0.02) {
            setMovementStopped(true);
          }
        }
      }
    }
  }, 1);

  return null;
}
