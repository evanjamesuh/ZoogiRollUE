import { Component, ReactNode, Suspense, useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { OrbitControls, useGLTF } from "@react-three/drei";
import { BookOpen, ChevronLeft, ChevronRight, Loader2 } from "lucide-react";
import * as THREE from "three";
import {
  COMIC_VIEW_EXPOSURE,
  INTRO_COMIC_PAGE_COUNT,
  comicCommandForKey,
  comicCommandForSwipe,
  comicCommandForTap,
  comicPanelUrlsToKeep,
  introComicPanelUrl,
  introComicTitle,
  missingComicPanelMessage,
  prepareComicPanel,
  stepComicPage,
} from "@/lib/comicPanels";
import { retainComicPanel, syncComicPanelCache } from "@/lib/comicPanelCache";

interface ComicViewerProps {
  onBack: () => void;
}

type ManualCamera = THREE.PerspectiveCamera & { manual?: boolean };

function ComicPanelStage({ page, onReady }: { page: number; onReady: (page: number) => void }) {
  const url = introComicPanelUrl(page);
  const gltf = useGLTF(url);
  const prepared = useMemo(() => prepareComicPanel(gltf.scene, page), [gltf.scene, page]);
  const set = useThree((state) => state.set);
  const size = useThree((state) => state.size);
  const frames = useRef(0);
  const revealedPage = useRef(0);

  useLayoutEffect(() => {
    retainComicPanel(url, gltf.scene);
    const camera = prepared.camera as ManualCamera;
    camera.manual = true;
    camera.aspect = size.width / Math.max(1, size.height);
    camera.updateProjectionMatrix();
    set({ camera });
    frames.current = 0;
  }, [url, gltf.scene, prepared.camera, set, size.width, size.height]);

  // The first frame compiles shaders. Reveal the panel on the frame after that
  // so the loader stays up instead of flashing a blank canvas.
  useFrame(() => {
    if (revealedPage.current === page) return;
    frames.current += 1;
    if (frames.current >= 2) {
      revealedPage.current = page;
      onReady(page);
    }
  });

  return (
    <>
      <primitive object={gltf.scene} />
      <OrbitControls
        camera={prepared.camera}
        target={prepared.aim}
        makeDefault
        enablePan={false}
        enableDamping
        dampingFactor={0.08}
        rotateSpeed={0.55}
        zoomSpeed={0.6}
        minDistance={prepared.orbit.minDistance}
        maxDistance={prepared.orbit.maxDistance}
        minPolarAngle={prepared.orbit.minPolarAngle}
        maxPolarAngle={prepared.orbit.maxPolarAngle}
        minAzimuthAngle={prepared.orbit.minAzimuthAngle}
        maxAzimuthAngle={prepared.orbit.maxAzimuthAngle}
        touches={{
          ONE: THREE.TOUCH.PAN,
          TWO: THREE.TOUCH.DOLLY_ROTATE,
        }}
      />
    </>
  );
}

interface PanelErrorBoundaryProps {
  page: number;
  children: ReactNode;
}

interface PanelErrorBoundaryState {
  failed: boolean;
}

class PanelErrorBoundary extends Component<PanelErrorBoundaryProps, PanelErrorBoundaryState> {
  state: PanelErrorBoundaryState = { failed: false };

  static getDerivedStateFromError(): PanelErrorBoundaryState {
    return { failed: true };
  }

  componentDidCatch(error: Error) {
    console.error("Comic panel failed to load:", introComicPanelUrl(this.props.page), error);
  }

  render() {
    if (this.state.failed) return <MissingPanel page={this.props.page} />;
    return this.props.children;
  }
}

function MissingPanel({ page }: { page: number }) {
  return (
    <div className="flex flex-1 items-center justify-center px-6 py-10">
      <div className="max-w-md text-center">
        <p className="text-lg font-semibold text-white">
          Panel {page}, {introComicTitle(page)}, couldn’t be loaded
        </p>
        <p className="mt-3 text-sm leading-relaxed text-white/75">{missingComicPanelMessage(page)}</p>
      </div>
    </div>
  );
}

function LoadingOverlay() {
  return (
    <div className="absolute inset-0 z-10 flex items-center justify-center bg-[#07080f]">
      <div className="flex flex-col items-center gap-3 text-white">
        <Loader2 className="h-8 w-8 animate-spin text-cyan-300" />
        <span className="text-sm font-medium">Loading panel…</span>
      </div>
    </div>
  );
}

function ComicFrame({ children }: { children: ReactNode }) {
  const outerRef = useRef<HTMLDivElement>(null);
  const [box, setBox] = useState({ width: 0, height: 0 });

  useEffect(() => {
    const el = outerRef.current;
    if (!el) return;
    const fit = () => {
      const width = el.clientWidth;
      const height = el.clientHeight;
      const aspect = 16 / 9;
      let frameWidth = width;
      let frameHeight = frameWidth / aspect;
      if (frameHeight > height) {
        frameHeight = height;
        frameWidth = frameHeight * aspect;
      }
      setBox({
        width: Math.max(0, Math.floor(frameWidth)),
        height: Math.max(0, Math.floor(frameHeight)),
      });
    };
    fit();
    const observer = new ResizeObserver(fit);
    observer.observe(el);
    return () => observer.disconnect();
  }, []);

  return (
    <div ref={outerRef} className="flex min-h-0 w-full flex-1 items-center justify-center">
      <div className="relative bg-black" style={{ width: box.width, height: box.height }}>
        {box.width > 0 && children}
      </div>
    </div>
  );
}

export function ComicViewer({ onBack }: ComicViewerProps) {
  const [page, setPage] = useState(1);
  const [readyPage, setReadyPage] = useState(0);
  const frameRef = useRef<HTMLDivElement>(null);
  const onBackRef = useRef(onBack);
  onBackRef.current = onBack;
  const markReady = useCallback((shownPage: number) => setReadyPage(shownPage), []);

  const go = (delta: number) => setPage((current) => stepComicPage(current, delta));

  useEffect(() => {
    const keep = comicPanelUrlsToKeep(page);
    const next = keep[1];
    if (next) {
      Promise.resolve(useGLTF.preload(next)).catch(() => {
        // Reaching that page shows the missing-file message.
      });
    }
    syncComicPanelCache(keep);
  }, [page]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      const command = comicCommandForKey(event.key);
      if (!command) return;
      if (command !== "close" && target?.closest("button, input, textarea, a")) return;
      event.preventDefault();
      if (command === "close") onBackRef.current();
      if (command === "next") go(1);
      if (command === "prev") go(-1);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  useEffect(() => {
    const el = frameRef.current;
    if (!el) return;
    let startX = 0;
    let startY = 0;
    let startTime = 0;
    let pointerId = -1;

    const onDown = (event: PointerEvent) => {
      if (event.pointerType === "mouse" && event.button !== 0) return;
      pointerId = event.pointerId;
      startX = event.clientX;
      startY = event.clientY;
      startTime = performance.now();
    };

    const onUp = (event: PointerEvent) => {
      if (event.pointerId !== pointerId) return;
      pointerId = -1;
      const dx = event.clientX - startX;
      const dy = event.clientY - startY;
      const elapsed = performance.now() - startTime;
      if (event.pointerType !== "mouse" && elapsed < 500) {
        const swipe = comicCommandForSwipe(dx, dy);
        if (swipe === "next") {
          go(1);
          return;
        }
        if (swipe === "prev") {
          go(-1);
          return;
        }
      }
      if (Math.abs(dx) < 12 && Math.abs(dy) < 12 && elapsed < 400) {
        const rect = el.getBoundingClientRect();
        const tap = comicCommandForTap(event.clientX - rect.left, rect.width);
        if (tap === "next") go(1);
        if (tap === "prev") go(-1);
      }
    };

    el.addEventListener("pointerdown", onDown);
    el.addEventListener("pointerup", onUp);
    return () => {
      el.removeEventListener("pointerdown", onDown);
      el.removeEventListener("pointerup", onUp);
    };
  }, []);

  const loading = readyPage !== page;

  return (
    <div
      className="menu-safe fixed inset-0 z-50 flex flex-col select-none bg-[#07080f] text-white"
      style={{ touchAction: "none" }}
      data-testid="comic-viewer"
    >
      <div className="grid shrink-0 grid-cols-[1fr_auto_1fr] items-center gap-2 px-3 py-3">
        <div>
          <button
            type="button"
            onClick={onBack}
            data-testid="comic-back"
            className="inline-flex min-h-11 items-center gap-1 rounded-full bg-white/10 px-4 text-base font-semibold hover:bg-white/20"
          >
            <ChevronLeft size={18} />
            Back
          </button>
        </div>
        <div className="flex items-center gap-2 rounded-full bg-white/10 px-4 py-2">
          <BookOpen size={16} className="text-cyan-300" />
          <span className="text-sm font-bold">Story</span>
        </div>
        <div />
      </div>

      <PanelErrorBoundary key={page} page={page}>
        <div className="relative flex min-h-0 flex-1 flex-col">
          <div ref={frameRef} className="flex min-h-0 flex-1 flex-col">
            <ComicFrame>
              <Canvas
                className="h-full w-full"
                dpr={[1, 1.75]}
                frameloop="always"
                gl={{
                  antialias: true,
                  alpha: false,
                  powerPreference: "high-performance",
                  toneMapping: THREE.ACESFilmicToneMapping,
                  toneMappingExposure: COMIC_VIEW_EXPOSURE,
                }}
                onCreated={({ gl }) => {
                  gl.outputColorSpace = THREE.SRGBColorSpace;
                  gl.toneMapping = THREE.ACESFilmicToneMapping;
                  gl.toneMappingExposure = COMIC_VIEW_EXPOSURE;
                }}
              >
                <color attach="background" args={["#07080f"]} />
                <Suspense fallback={null}>
                  <ComicPanelStage page={page} onReady={markReady} />
                </Suspense>
              </Canvas>
            </ComicFrame>
          </div>
          {loading && <LoadingOverlay />}
        </div>
      </PanelErrorBoundary>

      <div className="flex shrink-0 flex-col items-center gap-2 px-3 pb-4 pt-2">
        <p className="text-xs text-white/45">Arrows, swipe, or tap the sides</p>
        <div className="flex items-center gap-2">
          <button
            type="button"
            onClick={() => go(-1)}
            disabled={page === 1}
            data-testid="comic-prev"
            aria-label="Previous panel"
            className="rounded-full bg-white/10 p-3 hover:bg-white/20 disabled:cursor-not-allowed disabled:opacity-40"
          >
            <ChevronLeft size={22} />
          </button>
          <div
            data-testid="comic-page"
            className="min-w-[9.5rem] rounded-full bg-white/10 px-4 py-2 text-center"
          >
            <div className="text-sm font-semibold">
              {page} / {INTRO_COMIC_PAGE_COUNT}
            </div>
            <div className="text-xs text-white/70">{introComicTitle(page)}</div>
          </div>
          <button
            type="button"
            onClick={() => go(1)}
            disabled={page === INTRO_COMIC_PAGE_COUNT}
            data-testid="comic-next"
            aria-label="Next panel"
            className="rounded-full bg-white/10 p-3 hover:bg-white/20 disabled:cursor-not-allowed disabled:opacity-40"
          >
            <ChevronRight size={22} />
          </button>
        </div>
      </div>
    </div>
  );
}
