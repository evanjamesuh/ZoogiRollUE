import { useFrame } from "@react-three/fiber";
import { useRef } from "react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { FIXED_DT, stepAccumulator } from "@/lib/fixedTimestep";
import { setInterpAlpha, setInterpFrame, type Vec3 } from "@/lib/renderInterp";
import { SETTLE_GRACE_SECONDS } from "@/lib/simFeel";

interface BodySample {
  id: string;
  pos: Vec3;
}

const previousBodies: BodySample[] = [];
const currentBodies: BodySample[] = [];

function fillBodies(buf: BodySample[]): number {
  const state = useZoogiGame.getState();
  let n = 0;
  const push = (id: string, position: Vec3) => {
    let row = buf[n];
    if (!row) {
      row = { id, pos: [position[0], position[1], position[2]] };
      buf[n] = row;
    } else {
      row.id = id;
      row.pos[0] = position[0];
      row.pos[1] = position[1];
      row.pos[2] = position[2];
    }
    n += 1;
  };
  if (state.playerEntity) push(state.playerEntity.id, state.playerEntity.position);
  for (const enemy of state.enemies) push(enemy.id, enemy.position);
  for (const orb of state.orbs) {
    if (!orb.isActive) continue;
    push(orb.id, orb.position);
  }
  buf.length = n;
  return n;
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

    if (store.phase !== "playing") return;

    // Hit-pause and slow-motion stay visual. The sim always steps in real time.
    const timeScale = 1;
    const stepped = stepAccumulator(accumulatorRef.current, delta, timeScale);
    accumulatorRef.current = stepped.accumulator;

    if (stepped.steps > 0) {
      fillBodies(previousBodies);
      for (let i = 0; i < stepped.steps; i++) {
        if (i > 0) fillBodies(previousBodies);
        store.physicsTick(FIXED_DT);
      }
      fillBodies(currentBodies);
      const frame: { id: string; prev: Vec3; curr: Vec3 }[] = [];
      for (let i = 0; i < previousBodies.length; i++) {
        const prev = previousBodies[i];
        const curr = currentBodies.find((entry) => entry.id === prev.id) ?? prev;
        frame.push({ id: prev.id, prev: prev.pos, curr: curr.pos });
      }
      setInterpFrame(frame, stepped.alpha);
    } else {
      setInterpAlpha(stepped.alpha);
    }

    // The match clock follows real time. The sim above does not.
    store.tickTimers(Math.min(delta, 0.1));

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
