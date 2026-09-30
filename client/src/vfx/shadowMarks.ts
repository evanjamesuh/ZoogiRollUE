import { useEffect, useState } from "react";

const bound = new Set<string>();
const listeners = new Set<() => void>();

function publish(): void {
  listeners.forEach((listener) => listener());
}

/** Visual-only. Marks marbles that Shadow Stun froze so the wrap can linger. */
export function bindShadowStun(ids: readonly string[]): void {
  let changed = false;
  for (const id of ids) {
    if (bound.has(id)) continue;
    bound.add(id);
    changed = true;
  }
  if (changed) publish();
}

export function releaseShadowStun(id: string): void {
  if (bound.delete(id)) publish();
}

export function clearShadowStuns(): void {
  if (bound.size === 0) return;
  bound.clear();
  publish();
}

export function isShadowBound(id: string): boolean {
  return bound.has(id);
}

export function useShadowBound(id: string): boolean {
  const [on, setOn] = useState(() => bound.has(id));
  useEffect(() => {
    const pull = () => setOn(bound.has(id));
    listeners.add(pull);
    pull();
    return () => {
      listeners.delete(pull);
    };
  }, [id]);
  return on;
}
