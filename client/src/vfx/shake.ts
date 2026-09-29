import * as THREE from "three";
import { useGameFeel } from "@/lib/stores/useGameFeel";

const world = new THREE.Vector3();

/**
 * Small shake that falls off with distance. Uses the existing screen-shake
 * channel GameCamera already applies, and never replaces a stronger shake.
 */
export function impulseShake(camera: THREE.Camera, origin: THREE.Vector3, radius: number): void {
  world.copy(origin);
  const dist = camera.position.distanceTo(world);
  const reach = Math.max(radius * 5.5, 14);
  const falloff = Math.max(0, 1 - dist / reach);
  const intensity = 0.28 * Math.min(1, radius / 8) * falloff;
  if (intensity < 0.015) return;
  const feel = useGameFeel.getState();
  const current = feel.screenShake;
  if (current) {
    const elapsed = Date.now() - current.startTime;
    const left = elapsed < current.duration ? current.intensity * (1 - elapsed / current.duration) : 0;
    if (left >= intensity) return;
  }
  feel.triggerScreenShake(intensity, 320);
}
