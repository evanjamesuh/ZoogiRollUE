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
  const sparks = useMemo(() => createSpritePool(18), []);
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
      seedBurstSparks(sparks, rand, 14, [1, 0.78, 0.38], impact.dir);
      seedDustPuff(dust, rand, 6);
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
    const flash = elapsed < 0.08 ? 1 : Math.max(0, 1 - (elapsed - 0.08) / 0.16);
    if (light.current) light.current.intensity = flash * (electric ? 14 : dusty ? 6 : 22);
    if (elapsed > 0.85) dismissImpact(impact.id);
  });

  const color = electric ? "#d7ecff" : dusty ? "#e6c48a" : "#ffe2a8";
  return (
    <group position={impact.position}>
      <pointLight ref={light} position={[0, 0.35, 0]} color={color} intensity={0} distance={4.5} decay={2} />
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
