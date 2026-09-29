export function prefersCompactHud(): boolean {
  if (typeof window === "undefined") return false;
  const shortSide = Math.min(window.innerWidth, window.innerHeight);
  const coarse = window.matchMedia("(pointer: coarse)").matches;
  return coarse || shortSide <= 500;
}

export function isMobileGraphics(): boolean {
  return prefersCompactHud();
}

export function canvasPixelRatio(): [number, number] {
  return isMobileGraphics() ? [1, 1.5] : [1, 2];
}
