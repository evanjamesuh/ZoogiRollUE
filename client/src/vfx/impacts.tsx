import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { InstancedSprites } from "./InstancedSprites";
import { clearSpritePool, createSpritePool } from "./pool";
import { mulberry32 } from "./random";
import { seedBurstSparks, seedDustPuff, stepDust, stepGlints } from "./sim";
import { usePlayedClock } from "./bursts";

export type ImpactKind = "spark" | "dust" | "electric";

export interface ImpactEvent {
  id: number;
  kind: ImpactKind;
  position: [number, number, number];
  dir: [number, number, number];
}

let impacts: ImpactEvent[] = [];
let nextId = 1;
const listeners = new Set<() => void>();

export function emitImpact(
  kind: ImpactKind,
  position: [number, number, number],
  dir: [number, number, number] = [0, 0.6, 0.2],
): void {
  impacts = [...impacts, { id: nextId++, kind, position: [...position], dir: [...dir] }];
  listeners.forEach((listener) => listener());
}

function dismissImpact(id: number): void {
  impacts = impacts.filter((impact) => impact.id !== id);
  listeners.forEach((listener) => listener());
}

function useImpacts(): ImpactEvent[] {
  const [items, setItems] = useState<ImpactEvent[]>(impacts);
  useEffect(() => {
    const pull = () => setItems(impacts.slice());
    listeners.add(pull);
    pull();
    return () => {
      listeners.delete(pull);
    };
  }, []);
  return items;
}

function ImpactBurst({ impact }: { impact: ImpactEvent }) {
  const clock = usePlayedClock();
  const light = useRef<THREE.PointLight>(null);
  const sparks = useMemo(() => createSpritePool(28), []);
  const dust = useMemo(() => createSpritePool(10), []);
  const electric = impact.kind === "electric";
  const dusty = impact.kind === "dust";

  useEffect(() => {
    const rand = mulberry32((impact.id * 1103515245) >>> 0 || 1);
    clearSpritePool(sparks);
    clearSpritePool(dust);
    if (dusty) {
      seedDustPuff(dust, rand, 8);
    } else if (electric) {
      seedBurstSparks(sparks, rand, 10, [0.75, 0.9, 1], [0, 1, 0]);
    } else {
      seedBurstSparks(sparks, rand, 22, [1, 0.86, 0.45], impact.dir, 2.4);
      seedDustPuff(dust, rand, 5);
    }
    return () => {
      clearSpritePool(sparks);
      clearSpritePool(dust);
    };
  }, [dust, dusty, electric, impact.dir, impact.id, sparks]);

  useFrame((state, dt) => {
    const { elapsed, dt: step } = clock(state.clock.elapsedTime, dt);
    if (step > 0) {
      if (!dusty) stepGlints(sparks, elapsed, step);
      if (dusty || impact.kind === "spark") stepDust(dust, elapsed, step);
    }
    const flash = elapsed < 0.08 ? 1 : Math.max(0, 1 - (elapsed - 0.08) / 0.18);
    if (light.current) {
      light.current.intensity = flash * (electric ? 10 : dusty ? 4 : 14);
      light.current.distance = electric ? 1.8 : dusty ? 1.4 : 1.6;
    }
    if (elapsed > 0.85) dismissImpact(impact.id);
  });

  const color = electric ? "#e7f3ff" : dusty ? "#e6c48a" : "#ffc56a";
  return (
    <group position={impact.position}>
      <pointLight ref={light} position={[0, 0.25, 0]} color={color} intensity={0} distance={1.6} decay={2} />
      {!dusty && <InstancedSprites pool={sparks} mode="ember" />}
      {(dusty || impact.kind === "spark") && <InstancedSprites pool={dust} mode="dust" />}
    </group>
  );
}

/** Persistent spark, dust, and stun-end puffs. Mount once per canvas. */
export function ImpactField() {
  const items = useImpacts();
  return (
    <>
      {items.map((impact) => (
        <ImpactBurst key={impact.id} impact={impact} />
      ))}
    </>
  );
}
