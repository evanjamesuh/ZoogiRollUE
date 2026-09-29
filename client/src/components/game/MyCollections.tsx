import { useState, useEffect, Suspense, Component, ReactNode, useMemo } from "react";
import { Canvas } from "@react-three/fiber";
import { OrbitControls, useGLTF, Environment, Center } from "@react-three/drei";
import { ChevronLeft, Package, MapPin, Loader2, Trash2, X, Star, Download } from "lucide-react";
import * as THREE from "three";
import { getDeviceId } from "@/lib/deviceId";
import { VideoBackground } from "./VideoBackground";

interface MyCollectionsProps {
  onBack: () => void;
  onSelectZoogi?: (zoogi: CustomZoogiItem) => void;
  onSelectArena?: (arena: CustomArenaItem) => void;
}

interface CustomZoogiItem {
  id: number;
  name: string;
  meshyTaskId: string;
  modelUrl: string;
  thumbnailUrl: string | null;
  deviceId: string;
  createdAt: Date;
  health?: number;
  speed?: number;
  power?: number;
  weight?: number;
  abilityName?: string | null;
  abilityDescription?: string | null;
  isDefault?: boolean;
}

interface CustomArenaItem {
  id: number;
  name: string;
  meshyTaskId: string;
  modelUrl: string;
  thumbnailUrl: string | null;
  deviceId: string;
  createdAt: Date;
  isDefault?: boolean;
}

const DEFAULT_ARENAS: CustomArenaItem[] = [
  {
    id: -1,
    name: "Bumber 1",
    meshyTaskId: "default_bumber1",
    modelUrl: "/models/bumber1.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    isDefault: true,
  },
  {
    id: -2,
    name: "Bumber 1 (Alt)",
    meshyTaskId: "default_bumber1_alt",
    modelUrl: "/models/bumber1.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    isDefault: true,
  },
];

const DEFAULT_ZOOGIS: CustomZoogiItem[] = [
  {
    id: -1,
    name: "Zoogi Marble",
    meshyTaskId: "default_zoogi_marble",
    modelUrl: "/models/zoogi_marble.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 100,
    speed: 7,
    power: 7,
    weight: 5,
    abilityName: "Rolling Thunder",
    abilityDescription: "A colorful marble with balanced stats",
    isDefault: true,
  },
  {
    id: -2,
    name: "Lars",
    meshyTaskId: "default_lars",
    modelUrl: "/models/lars.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 90,
    speed: 8,
    power: 6,
    weight: 6,
    abilityName: "Ice Slide",
    abilityDescription: "A swift Zoogi that glides across the arena with grace",
    isDefault: true,
  },
  {
    id: -3,
    name: "Hot Streak",
    meshyTaskId: "default_hotstreak",
    modelUrl: "/models/hotstreak.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 80,
    speed: 9,
    power: 8,
    weight: 4,
    abilityName: "Blazing Rush",
    abilityDescription: "A fiery Zoogi that leaves a trail of heat in its wake",
    isDefault: true,
  },
  {
    id: -4,
    name: "Inferno",
    meshyTaskId: "default_inferno",
    modelUrl: "/models/inferno.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 75,
    speed: 7,
    power: 10,
    weight: 5,
    abilityName: "Flame Burst",
    abilityDescription: "Unleashes explosive fire damage on impact",
    isDefault: true,
  },
  {
    id: -5,
    name: "Bolt",
    meshyTaskId: "default_bolt",
    modelUrl: "/models/bolt.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 70,
    speed: 10,
    power: 6,
    weight: 3,
    abilityName: "Lightning Strike",
    abilityDescription: "Blazing fast with electric stun ability",
    isDefault: true,
  },
  {
    id: -6,
    name: "Pinpoint",
    meshyTaskId: "default_pinpoint",
    modelUrl: "/models/pinpoint.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 85,
    speed: 6,
    power: 7,
    weight: 6,
    abilityName: "Precision Shot",
    abilityDescription: "Accurate and deadly with surgical precision",
    isDefault: true,
  },
  {
    id: -7,
    name: "Wolf Gang",
    meshyTaskId: "default_wolfgang",
    modelUrl: "/models/wolfgang.glb",
    thumbnailUrl: null,
    deviceId: "default",
    createdAt: new Date("2024-01-01"),
    health: 95,
    speed: 7,
    power: 8,
    weight: 7,
    abilityName: "Pack Howl",
    abilityDescription: "A fierce wolf with powerful pack instincts",
    isDefault: true,
  },
];

class ModelErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError() {
    return { hasError: true };
  }

  render() {
    if (this.state.hasError) {
      return this.props.fallback;
    }
    return this.props.children;
  }
}

function ModelPreviewInner({ modelUrl }: { modelUrl: string }) {
  const { scene } = useGLTF(modelUrl);
  
  useEffect(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
  }, [scene]);

  return (
    <Center>
      <primitive object={scene} scale={2} />
    </Center>
  );
}

function ModelLoadingFallback() {
  return (
    <mesh>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="#8B5CF6" wireframe />
    </mesh>
  );
}

function ModelErrorFallback() {
  return (
    <mesh>
      <boxGeometry args={[1.5, 1.5, 1.5]} />
      <meshStandardMaterial color="#EF4444" opacity={0.7} transparent />
    </mesh>
  );
}

function ModelPreview({ modelUrl }: { modelUrl: string }) {
  return (
    <ModelErrorBoundary fallback={<ModelErrorFallback />}>
      <Suspense fallback={<ModelLoadingFallback />}>
        <ModelPreviewInner modelUrl={modelUrl} />
      </Suspense>
    </ModelErrorBoundary>
  );
}

export function MyCollections({ onBack, onSelectZoogi, onSelectArena }: MyCollectionsProps) {
  const [activeTab, setActiveTab] = useState<"zoogis" | "arenas">("zoogis");
  const [zoogis, setZoogis] = useState<CustomZoogiItem[]>([]);
  const [arenas, setArenas] = useState<CustomArenaItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedItem, setSelectedItem] = useState<CustomZoogiItem | CustomArenaItem | null>(null);
  const [showDeleteConfirm, setShowDeleteConfirm] = useState<number | null>(null);
  const [deleting, setDeleting] = useState(false);

  useEffect(() => {
    loadCollections();
  }, []);

  const loadCollections = async () => {
    setLoading(true);
    try {
      const deviceId = getDeviceId();
      
      const [zoogiRes, arenaRes] = await Promise.all([
        fetch(`/api/custom-zoogis?deviceId=${deviceId}`),
        fetch(`/api/custom-arenas?deviceId=${deviceId}`)
      ]);
      
      if (zoogiRes.ok) {
        const zoogiData = await zoogiRes.json();
        const zoogiList = Array.isArray(zoogiData?.data) ? zoogiData.data : [];
        // Compute modelUrl from meshyTaskId if not present
        setZoogis(zoogiList.map((z: any) => ({
          ...z,
          modelUrl: z.modelUrl || `/api/meshy/download/${z.meshyTaskId}`,
          // Extract stats from the stats object if present
          health: z.stats?.health ?? z.health,
          speed: z.stats?.speed ?? z.speed,
          power: z.stats?.power ?? z.power,
          weight: z.stats?.weight ?? z.weight,
          abilityName: z.stats?.abilityName ?? z.abilityName,
          abilityDescription: z.stats?.abilityDescription ?? z.abilityDescription,
        })));
      }
      
      if (arenaRes.ok) {
        const arenaData = await arenaRes.json();
        const arenaList = Array.isArray(arenaData?.data) ? arenaData.data : [];
        // Compute modelUrl from meshyTaskId if not present
        setArenas(arenaList.map((a: any) => ({
          ...a,
          modelUrl: a.modelUrl || `/api/meshy/download/${a.meshyTaskId}`,
        })));
      }
    } catch (error) {
      console.error("Failed to load collections:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (id: number, type: "zoogi" | "arena") => {
    setDeleting(true);
    try {
      const deviceId = getDeviceId();
      const endpoint = type === "zoogi" 
        ? `/api/custom-zoogis/${id}?deviceId=${deviceId}` 
        : `/api/custom-arenas/${id}?deviceId=${deviceId}`;
      const res = await fetch(endpoint, { method: "DELETE" });
      
      if (res.ok) {
        if (type === "zoogi") {
          setZoogis(prev => prev.filter(z => z.id !== id));
        } else {
          setArenas(prev => prev.filter(a => a.id !== id));
        }
        setSelectedItem(null);
      }
    } catch (error) {
      console.error("Failed to delete:", error);
    } finally {
      setDeleting(false);
      setShowDeleteConfirm(null);
    }
  };

  const handleDownload = async (item: CustomZoogiItem | CustomArenaItem) => {
    try {
      const response = await fetch(item.modelUrl);
      if (!response.ok) throw new Error("Failed to fetch model");
      
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${item.name.replace(/\s+/g, "_")}.glb`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);
    } catch (error) {
      console.error("Failed to download model:", error);
    }
  };

  const allZoogis = useMemo(() => [...DEFAULT_ZOOGIS, ...zoogis], [zoogis]);
  const allArenas = useMemo(() => [...DEFAULT_ARENAS, ...arenas], [arenas]);
  const currentItems = activeTab === "zoogis" ? allZoogis : allArenas;

  return (
    <div className="fixed inset-0 z-50 overflow-hidden">
      <VideoBackground />
      
      <div className="relative z-10 h-full flex flex-col p-4 md:p-6">
        <div className="flex items-center justify-between mb-4">
          <button
            onClick={onBack}
            className="flex items-center gap-2 text-white/80 hover:text-white transition-colors"
          >
            <ChevronLeft size={24} />
            <span className="font-medium">Back</span>
          </button>
          
          <h1 className="text-2xl md:text-3xl font-bold text-transparent bg-clip-text bg-gradient-to-r from-amber-400 to-orange-500">
            My Collections
          </h1>
          
          <div className="w-24" />
        </div>
        
        <div className="flex justify-center gap-2 mb-6">
          <button
            onClick={() => setActiveTab("zoogis")}
            className={`flex items-center gap-2 px-6 py-3 rounded-xl font-semibold transition-all ${
              activeTab === "zoogis"
                ? "bg-gradient-to-r from-pink-500 to-orange-500 text-white shadow-lg shadow-pink-500/30"
                : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            <Star size={20} />
            <span>Zoogis ({allZoogis.length})</span>
          </button>
          <button
            onClick={() => setActiveTab("arenas")}
            className={`flex items-center gap-2 px-6 py-3 rounded-xl font-semibold transition-all ${
              activeTab === "arenas"
                ? "bg-gradient-to-r from-teal-500 to-blue-500 text-white shadow-lg shadow-teal-500/30"
                : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            <MapPin size={20} />
            <span>Arenas ({allArenas.length})</span>
          </button>
        </div>

        {loading ? (
          <div className="flex-1 flex items-center justify-center">
            <div className="flex flex-col items-center gap-4">
              <Loader2 size={48} className="text-purple-400 animate-spin" />
              <p className="text-white/70">Loading your collections...</p>
            </div>
          </div>
        ) : currentItems.length === 0 ? (
          <div className="flex-1 flex items-center justify-center">
            <div className="flex flex-col items-center gap-4 text-center p-8">
              <Package size={64} className="text-white/30" />
              <h2 className="text-xl font-semibold text-white/70">
                No {activeTab === "zoogis" ? "Zoogis" : "Arenas"} Yet
              </h2>
              <p className="text-white/50 max-w-md">
                Create your first custom {activeTab === "zoogis" ? "Zoogi" : "Arena"} using the Create button on the main menu!
              </p>
            </div>
          </div>
        ) : (
          <div className="flex-1 overflow-y-auto">
            <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
              {currentItems.map((item) => (
                <div
                  key={item.id}
                  onClick={() => setSelectedItem(item)}
                  className="relative group bg-white/10 backdrop-blur-sm rounded-xl overflow-hidden cursor-pointer hover:bg-white/20 transition-all hover:scale-105 border border-white/10"
                >
                  <div className="aspect-square bg-gradient-to-br from-purple-500/20 to-pink-500/20">
                    {item.thumbnailUrl ? (
                      <img 
                        src={item.thumbnailUrl} 
                        alt={item.name}
                        className="w-full h-full object-cover"
                      />
                    ) : item.modelUrl ? (
                      <Canvas camera={{ position: [3, 3, 3], fov: 50 }}>
                        <ambientLight intensity={0.6} />
                        <directionalLight position={[5, 5, 5]} intensity={0.8} />
                        <ModelErrorBoundary fallback={<ModelErrorFallback />}>
                          <Suspense fallback={<ModelLoadingFallback />}>
                            <ModelPreviewInner modelUrl={item.modelUrl} />
                          </Suspense>
                        </ModelErrorBoundary>
                        <OrbitControls enablePan={false} enableZoom={false} autoRotate autoRotateSpeed={2} />
                        <Environment preset="sunset" />
                      </Canvas>
                    ) : (
                      <div className="w-full h-full flex items-center justify-center">
                        {activeTab === "zoogis" ? (
                          <Star size={48} className="text-white/30" />
                        ) : (
                          <MapPin size={48} className="text-white/30" />
                        )}
                      </div>
                    )}
                  </div>
                  
                  <div className="p-3">
                    <h3 className="font-semibold text-white truncate">{item.name}</h3>
                    <p className="text-xs text-white/50">
                      {"isDefault" in item && item.isDefault ? "Free Asset" : new Date(item.createdAt).toLocaleDateString()}
                    </p>
                  </div>
                  
                  {"isDefault" in item && item.isDefault ? (
                    <div className="absolute top-2 right-2 px-2 py-1 bg-green-500/90 rounded-full text-xs font-bold text-white">
                      FREE
                    </div>
                  ) : (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setShowDeleteConfirm(item.id);
                      }}
                      className="absolute top-2 right-2 p-2 bg-red-500/80 rounded-full opacity-0 group-hover:opacity-100 transition-opacity hover:bg-red-600"
                    >
                      <Trash2 size={14} className="text-white" />
                    </button>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {selectedItem && (
        <div 
          className="fixed inset-0 bg-black/80 z-[60] flex items-center justify-center p-4"
          onClick={() => setSelectedItem(null)}
        >
          <div 
            className="bg-gradient-to-b from-slate-800 to-slate-900 rounded-2xl w-full max-w-2xl overflow-hidden"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center justify-between p-4 border-b border-white/10">
              <h2 className="text-xl font-bold text-white">{selectedItem.name}</h2>
              <button
                onClick={() => setSelectedItem(null)}
                className="p-2 hover:bg-white/10 rounded-full transition-colors"
              >
                <X size={20} className="text-white/70" />
              </button>
            </div>
            
            <div className="h-64 md:h-80">
              <Canvas shadows camera={{ position: [3, 3, 3], fov: 50 }}>
                <ambientLight intensity={0.5} />
                <directionalLight position={[5, 5, 5]} intensity={1} castShadow />
                <ModelPreview modelUrl={selectedItem.modelUrl} />
                <OrbitControls enablePan={false} enableZoom={true} />
                <Environment preset="sunset" />
              </Canvas>
            </div>
            
            <div className="p-4 border-t border-white/10">
              {"health" in selectedItem && (
                <div className="grid grid-cols-2 gap-2 mb-4 text-sm">
                  <div className="bg-white/10 rounded-lg p-2">
                    <span className="text-white/50">Health:</span>
                    <span className="text-white ml-2">{selectedItem.health}</span>
                  </div>
                  <div className="bg-white/10 rounded-lg p-2">
                    <span className="text-white/50">Speed:</span>
                    <span className="text-white ml-2">{selectedItem.speed}</span>
                  </div>
                  <div className="bg-white/10 rounded-lg p-2">
                    <span className="text-white/50">Power:</span>
                    <span className="text-white ml-2">{selectedItem.power}</span>
                  </div>
                  <div className="bg-white/10 rounded-lg p-2">
                    <span className="text-white/50">Weight:</span>
                    <span className="text-white ml-2">{selectedItem.weight}</span>
                  </div>
                </div>
              )}
              
              <div className="flex gap-3">
                <button
                  onClick={() => {
                    if (activeTab === "zoogis" && onSelectZoogi) {
                      onSelectZoogi(selectedItem as CustomZoogiItem);
                    } else if (activeTab === "arenas" && onSelectArena) {
                      onSelectArena(selectedItem as CustomArenaItem);
                    }
                    setSelectedItem(null);
                  }}
                  className="flex-1 py-3 bg-gradient-to-r from-green-500 to-emerald-600 rounded-xl font-semibold text-white hover:from-green-600 hover:to-emerald-700 transition-all"
                >
                  Select for Battle
                </button>
                <button
                  onClick={() => handleDownload(selectedItem)}
                  className="px-4 py-3 bg-blue-500/20 border border-blue-500/50 rounded-xl text-blue-400 hover:bg-blue-500/30 transition-all"
                  title="Download GLB Model"
                >
                  <Download size={20} />
                </button>
                {!("isDefault" in selectedItem && selectedItem.isDefault) && (
                  <button
                    onClick={() => setShowDeleteConfirm(selectedItem.id)}
                    className="px-4 py-3 bg-red-500/20 border border-red-500/50 rounded-xl text-red-400 hover:bg-red-500/30 transition-all"
                  >
                    <Trash2 size={20} />
                  </button>
                )}
              </div>
            </div>
          </div>
        </div>
      )}

      {showDeleteConfirm !== null && (
        <div 
          className="fixed inset-0 bg-black/80 z-[80] flex items-center justify-center p-4"
          onClick={() => setShowDeleteConfirm(null)}
        >
          <div 
            className="bg-slate-800 rounded-2xl p-6 max-w-sm w-full"
            onClick={(e) => e.stopPropagation()}
          >
            <h3 className="text-xl font-bold text-white mb-2">Delete {activeTab === "zoogis" ? "Zoogi" : "Arena"}?</h3>
            <p className="text-white/70 mb-6">This action cannot be undone. Your custom creation will be permanently removed.</p>
            
            <div className="flex gap-3">
              <button
                onClick={() => setShowDeleteConfirm(null)}
                className="flex-1 py-3 bg-white/10 rounded-xl font-semibold text-white hover:bg-white/20 transition-all"
              >
                Cancel
              </button>
              <button
                onClick={() => handleDelete(showDeleteConfirm, activeTab === "zoogis" ? "zoogi" : "arena")}
                disabled={deleting}
                className="flex-1 py-3 bg-red-500 rounded-xl font-semibold text-white hover:bg-red-600 transition-all disabled:opacity-50 flex items-center justify-center gap-2"
              >
                {deleting ? <Loader2 size={20} className="animate-spin" /> : null}
                Delete
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
