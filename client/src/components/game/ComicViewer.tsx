import { useState, useRef, useEffect, Suspense, useMemo, Component, ReactNode } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { OrbitControls, useGLTF, Html, PerspectiveCamera } from "@react-three/drei";
import { ChevronLeft, ChevronUp, ChevronDown, Home, Loader2, BookOpen } from "lucide-react";
import * as THREE from "three";

interface ComicViewerProps {
  onBack: () => void;
}

interface ComicPanel {
  id: string;
  name: string;
  modelUrl: string;
  width: number;
  height: number;
}

const SAMPLE_PANELS: ComicPanel[] = [
  { id: "header", name: "Header", modelUrl: "/attached_assets/header.w100h50_1768883315034.glb", width: 100, height: 50 },
  { id: "panel1", name: "Panel 1", modelUrl: "/attached_assets/black.w100h25_1768883315033.glb", width: 100, height: 25 },
  { id: "footer", name: "Footer", modelUrl: "/attached_assets/footer.w100h50_1768883315034.glb", width: 100, height: 50 },
];

interface PanelErrorBoundaryProps {
  children: ReactNode;
  panelName: string;
}

interface PanelErrorBoundaryState {
  hasError: boolean;
}

class PanelErrorBoundary extends Component<PanelErrorBoundaryProps, PanelErrorBoundaryState> {
  constructor(props: PanelErrorBoundaryProps) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError(): PanelErrorBoundaryState {
    return { hasError: true };
  }

  componentDidCatch(error: Error) {
    console.error("Comic panel load error:", this.props.panelName, error);
  }

  render() {
    if (this.state.hasError) {
      return (
        <mesh>
          <planeGeometry args={[6, 3]} />
          <meshBasicMaterial color="#1a1a2e" />
        </mesh>
      );
    }
    return this.props.children;
  }
}

function ComicPanelModelInner({ panel, isActive }: { panel: ComicPanel; isActive: boolean }) {
  const groupRef = useRef<THREE.Group>(null);
  const { scene } = useGLTF(panel.modelUrl);
  
  const clonedScene = useMemo(() => scene.clone(), [scene]);
  
  useEffect(() => {
    console.log("Comic panel loaded:", panel.id);
    clonedScene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = false;
        child.receiveShadow = false;
      }
    });
  }, [clonedScene, panel.id]);

  useFrame(() => {
    if (groupRef.current) {
      const targetScale = isActive ? 1 : 0.85;
      const currentScale = groupRef.current.scale.x;
      const newScale = currentScale + (targetScale - currentScale) * 0.1;
      if (Math.abs(newScale - currentScale) > 0.001) {
        groupRef.current.scale.setScalar(newScale);
      }
    }
  });

  return (
    <group ref={groupRef}>
      <primitive object={clonedScene} scale={3} />
    </group>
  );
}

function PanelFallback() {
  return (
    <mesh>
      <planeGeometry args={[6, 3]} />
      <meshBasicMaterial color="#333" wireframe />
    </mesh>
  );
}

function ComicPanelModel({ panel, position, isActive }: { panel: ComicPanel; position: [number, number, number]; isActive: boolean }) {
  return (
    <group position={position}>
      <PanelErrorBoundary panelName={panel.name}>
        <Suspense fallback={<PanelFallback />}>
          <ComicPanelModelInner panel={panel} isActive={isActive} />
        </Suspense>
      </PanelErrorBoundary>
    </group>
  );
}

function ComicScene({ panels, currentPanelIndex }: { panels: ComicPanel[]; currentPanelIndex: number }) {
  const { camera } = useThree();
  const targetY = useRef(0);
  const panelSpacing = 8;
  
  useEffect(() => {
    targetY.current = -currentPanelIndex * panelSpacing;
  }, [currentPanelIndex, panelSpacing]);
  
  useFrame(() => {
    const diff = targetY.current - camera.position.y;
    if (Math.abs(diff) > 0.01) {
      camera.position.y += diff * 0.08;
    }
  });

  return (
    <>
      <ambientLight intensity={0.8} />
      <directionalLight position={[5, 10, 5]} intensity={0.6} />
      
      {panels.map((panel, index) => (
        <ComicPanelModel 
          key={panel.id}
          panel={panel} 
          position={[0, -index * panelSpacing, 0]} 
          isActive={index === currentPanelIndex}
        />
      ))}
    </>
  );
}

function LoadingOverlay() {
  return (
    <Html center>
      <div className="flex flex-col items-center gap-3 text-white">
        <Loader2 className="w-8 h-8 animate-spin" />
        <span className="text-sm">Loading panel...</span>
      </div>
    </Html>
  );
}

export function ComicViewer({ onBack }: ComicViewerProps) {
  const [currentPanelIndex, setCurrentPanelIndex] = useState(0);
  const [isLoading, setIsLoading] = useState(true);
  const panels = SAMPLE_PANELS;
  
  useEffect(() => {
    const timer = setTimeout(() => setIsLoading(false), 1000);
    return () => clearTimeout(timer);
  }, []);

  const goToNextPanel = () => {
    if (currentPanelIndex < panels.length - 1) {
      setCurrentPanelIndex(prev => prev + 1);
    }
  };

  const goToPrevPanel = () => {
    if (currentPanelIndex > 0) {
      setCurrentPanelIndex(prev => prev - 1);
    }
  };

  const goToFirstPanel = () => {
    setCurrentPanelIndex(0);
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      switch (e.key) {
        case "ArrowDown":
        case "ArrowRight":
        case " ":
          e.preventDefault();
          goToNextPanel();
          break;
        case "ArrowUp":
        case "ArrowLeft":
          e.preventDefault();
          goToPrevPanel();
          break;
        case "Home":
          e.preventDefault();
          goToFirstPanel();
          break;
        case "Escape":
          onBack();
          break;
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [currentPanelIndex, panels.length]);

  useEffect(() => {
    let touchStartY = 0;
    let touchStartX = 0;
    
    const handleTouchStart = (e: TouchEvent) => {
      touchStartY = e.touches[0].clientY;
      touchStartX = e.touches[0].clientX;
    };
    
    const handleTouchEnd = (e: TouchEvent) => {
      const touchEndY = e.changedTouches[0].clientY;
      const touchEndX = e.changedTouches[0].clientX;
      const diffY = touchStartY - touchEndY;
      const diffX = touchStartX - touchEndX;
      
      if (Math.abs(diffY) > Math.abs(diffX) && Math.abs(diffY) > 50) {
        if (diffY > 0) {
          goToNextPanel();
        } else {
          goToPrevPanel();
        }
      } else if (Math.abs(diffX) > 50) {
        if (diffX > 0) {
          goToNextPanel();
        } else {
          goToPrevPanel();
        }
      }
    };
    
    window.addEventListener("touchstart", handleTouchStart);
    window.addEventListener("touchend", handleTouchEnd);
    return () => {
      window.removeEventListener("touchstart", handleTouchStart);
      window.removeEventListener("touchend", handleTouchEnd);
    };
  }, [currentPanelIndex, panels.length]);

  return (
    <div className="fixed inset-0 z-50 flex flex-col bg-gradient-to-b from-gray-900 via-black to-gray-900">
      <div className="absolute top-4 left-4 z-10">
        <button
          onClick={onBack}
          className="flex items-center gap-2 px-4 py-2 bg-black/60 backdrop-blur-sm text-white rounded-full hover:bg-black/80 transition-colors"
        >
          <ChevronLeft size={20} />
          <span className="font-medium">Back</span>
        </button>
      </div>

      <div className="absolute top-4 left-1/2 -translate-x-1/2 z-10">
        <div className="flex items-center gap-2 px-4 py-2 bg-black/60 backdrop-blur-sm rounded-full">
          <BookOpen size={18} className="text-cyan-400" />
          <span className="text-white font-bold">3D Comic</span>
        </div>
      </div>

      <div className="flex-1 relative">
        {isLoading ? (
          <div className="absolute inset-0 flex items-center justify-center bg-black">
            <div className="flex flex-col items-center gap-4">
              <div className="w-24 h-24 border-4 border-cyan-500 border-t-transparent rounded-full animate-spin" />
              <span className="text-white text-lg font-medium animate-pulse">Loading 3D Comic...</span>
            </div>
          </div>
        ) : (
          <Canvas 
            shadows={false}
            gl={{ 
              antialias: false,
              powerPreference: "low-power",
              failIfMajorPerformanceCaveat: false
            }}
          >
            <color attach="background" args={["#0a0a0f"]} />
            <PerspectiveCamera makeDefault position={[0, 0, 12]} fov={50} />
            <Suspense fallback={<LoadingOverlay />}>
              <ComicScene panels={panels} currentPanelIndex={currentPanelIndex} />
            </Suspense>
            <OrbitControls 
              enableZoom={true} 
              enablePan={false}
              minDistance={5}
              maxDistance={20}
              enableRotate={true}
              maxPolarAngle={Math.PI * 0.75}
              minPolarAngle={Math.PI * 0.25}
            />
          </Canvas>
        )}
      </div>

      <div className="absolute bottom-4 left-1/2 -translate-x-1/2 z-10 flex items-center gap-2">
        <button
          onClick={goToFirstPanel}
          disabled={currentPanelIndex === 0}
          className="p-3 bg-black/60 backdrop-blur-sm text-white rounded-full hover:bg-black/80 transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
          title="Go to start"
        >
          <Home size={20} />
        </button>
        
        <button
          onClick={goToPrevPanel}
          disabled={currentPanelIndex === 0}
          className="p-3 bg-black/60 backdrop-blur-sm text-white rounded-full hover:bg-black/80 transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
          title="Previous panel"
        >
          <ChevronUp size={24} />
        </button>
        
        <div className="px-4 py-2 bg-black/60 backdrop-blur-sm rounded-full min-w-[80px] text-center">
          <span className="text-white font-medium">
            {currentPanelIndex + 1} / {panels.length}
          </span>
        </div>
        
        <button
          onClick={goToNextPanel}
          disabled={currentPanelIndex === panels.length - 1}
          className="p-3 bg-black/60 backdrop-blur-sm text-white rounded-full hover:bg-black/80 transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
          title="Next panel"
        >
          <ChevronDown size={24} />
        </button>
      </div>

      <div className="absolute bottom-20 left-1/2 -translate-x-1/2 z-10">
        <span className="text-white/50 text-xs">
          Swipe or use arrow keys to navigate
        </span>
      </div>
    </div>
  );
}
