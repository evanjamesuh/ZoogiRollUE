import { useEffect, useState } from "react";

export interface ViewportLayout {
  phone: boolean;
  portrait: boolean;
}

export function readViewportLayout(): ViewportLayout {
  if (typeof window === "undefined") return { phone: false, portrait: true };
  const width = window.innerWidth;
  const height = window.innerHeight;
  const coarse = window.matchMedia("(pointer: coarse)").matches;
  return {
    phone: coarse || Math.min(width, height) <= 500,
    portrait: height >= width,
  };
}

export function useViewportLayout(): ViewportLayout {
  const [layout, setLayout] = useState(readViewportLayout);
  useEffect(() => {
    const update = () => setLayout(readViewportLayout());
    window.addEventListener("resize", update);
    window.addEventListener("orientationchange", update);
    return () => {
      window.removeEventListener("resize", update);
      window.removeEventListener("orientationchange", update);
    };
  }, []);
  return layout;
}

export function prefersCompactHud(): boolean {
  return readViewportLayout().phone;
}

export function isMobileGraphics(): boolean {
  return prefersCompactHud();
}

export function canvasPixelRatio(): [number, number] {
  // A 2x buffer plus bloom is a 4k-class fill rate on a 1440p monitor.
  return isMobileGraphics() ? [1, 1.25] : [1, 1.5];
}
