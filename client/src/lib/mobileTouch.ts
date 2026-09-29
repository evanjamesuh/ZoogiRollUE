function isPanTarget(target: EventTarget | null): boolean {
  if (!(target instanceof Element)) return false;
  return Boolean(target.closest(".allow-pan-y, input, textarea, select"));
}

export function installMobileTouchGuards(): void {
  const preventPagePan = (event: TouchEvent) => {
    if (event.touches.length > 1) {
      event.preventDefault();
      return;
    }
    if (isPanTarget(event.target)) return;
    event.preventDefault();
  };

  let lastTouchEnd = 0;
  const preventDoubleTapZoom = (event: TouchEvent) => {
    const now = Date.now();
    if (now - lastTouchEnd < 300) event.preventDefault();
    lastTouchEnd = now;
  };

  const preventGesture = (event: Event) => {
    event.preventDefault();
  };

  document.addEventListener("touchmove", preventPagePan, { passive: false });
  document.addEventListener("touchend", preventDoubleTapZoom, { passive: false });
  document.addEventListener("gesturestart", preventGesture);
  document.addEventListener("gesturechange", preventGesture);
}
