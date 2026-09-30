import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { clearSpritePool, createSpritePool } from "./pool";
import { mulberry32 } from "./random";
import { BURST_CAP, seedColdBurst, stepPuff } from "./sim";
import { InstancedSprites } from "./InstancedSprites";

export interface SmokeBurst {
  id: number;
  x: number;
  y: number;
  z: number;
}

let bursts: SmokeBurst[] = [];
let nextId = 1;
const listeners = new Set<() => void>();

export function emitSmokeBurst(position: [number, number, number]): void {
  bursts = [...bursts, { id: nextId++, x: position[0], y: position[1], z: position[2] }];
  listeners.forEach((listener) => listener());
}

export function dismissSmokeBurst(id: number): void {
  bursts = bursts.filter((burst) => burst.id !== id);
  listeners.forEach((listener) => listener());
}

function useSmokeBursts(): SmokeBurst[] {
  const [items, setItems] = useState<SmokeBurst[]>(bursts);
  useEffect(() => {
    const pull = () => setItems(bursts.slice());
    listeners.add(pull);
    pull();
    return () => {
      listeners.delete(pull);
    };
  }, []);
  return items;
}

function ColdPuff({ burst }: { burst: SmokeBurst }) {
  const pool = useMemo(() => createSpritePool(BURST_CAP), []);
  const elapsed = useRef(0);
  const closed = useRef(false);

  useLayoutEffect(() => {
    clearSpritePool(pool);
    seedColdBurst(pool, mulberry32((burst.id * 1103515245) >>> 0 || 1));
  }, [pool, burst.id]);

  useFrame((_, dt) => {
    if (closed.current) return;
    const step = Math.min(dt, 1 / 30);
    elapsed.current += step;
    stepPuff(pool, elapsed.current, step);
    if (elapsed.current > 1.15) {
      closed.current = true;
      dismissSmokeBurst(burst.id);
    }
  });

  return (
    <group position={[burst.x, burst.y, burst.z]}>
      <InstancedSprites pool={pool} mode="mist" />
    </group>
  );
}

/** Renders wolf spawn / vanish puffs. Mount once per canvas. */
export function SmokeBurstField() {
  const items = useSmokeBursts();
  return (
    <>
      {items.map((burst) => (
        <ColdPuff key={burst.id} burst={burst} />
      ))}
    </>
  );
}

export function usePlayedClock(frozenElapsed?: number) {
  const origin = useRef<number | null>(null);
  const played = useRef(0);
  return (clockElapsed: number, dt: number) => {
    if (frozenElapsed !== undefined) return { elapsed: Math.max(0, frozenElapsed), dt: 0 };
    if (origin.current === null) origin.current = clockElapsed;
    const hold = typeof window !== "undefined"
      ? (window as Window & { __vfxHold?: number }).__vfxHold
      : undefined;
    if (typeof hold === "number" && played.current >= hold) {
      return { elapsed: played.current, dt: 0 };
    }
    const uncapped = Math.max(0, clockElapsed - origin.current);
    const step = Math.min(Math.max(dt, 0), 1 / 30);
    let next = Math.min(uncapped, played.current + step);
    if (typeof hold === "number" && next > hold) next = hold;
    const used = next - played.current;
    played.current = next;
    return { elapsed: next, dt: used };
  };
}
