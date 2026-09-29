import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import { useRef, useState, useMemo, useCallback, useEffect } from "react";
import * as THREE from "three";
import { useZoogiGame, MapTheme, MAP_OPTIONS } from "@/lib/stores/useZoogiGame";
import { ChevronLeft, Save, Trash2, Plus, RotateCcw, Check, X } from "lucide-react";
import { motion, AnimatePresence } from "framer-motion";
import { Arena } from "./Arena";
import { DeveloperMoveControls } from "./DeveloperMoveControls";

export interface PlacedDecoration {
  id: string;
  type: string;
  position: [number, number, number];
  rotation: number;
  scale: number;
}

export interface CustomArena {
  id: string;
  name: string;
  theme: MapTheme;
  decorations: PlacedDecoration[];
  createdAt: number;
}

const DECORATION_TYPES = [
  { id: "oak_tree", name: "Oak Tree", icon: "🌳", themes: ["grass"] },
  { id: "pine_tree", name: "Pine Tree", icon: "🌲", themes: ["grass"] },
  { id: "mushroom_red", name: "Red Mushroom", icon: "🍄", themes: ["grass"] },
  { id: "mushroom_white", name: "White Mushroom", icon: "🍄", themes: ["grass"] },
  { id: "flower_pink", name: "Pink Flowers", icon: "🌸", themes: ["grass"] },
  { id: "flower_yellow", name: "Yellow Flowers", icon: "🌼", themes: ["grass"] },
  { id: "snow_pine", name: "Snow Pine", icon: "🎄", themes: ["ice"] },
  { id: "snowman", name: "Snowman", icon: "⛄", themes: ["ice"] },
  { id: "ice_patch", name: "Ice Patch", icon: "❄️", themes: ["ice"] },
  { id: "hoodoo", name: "Desert Hoodoo", icon: "🪨", themes: ["lava"] },
  { id: "lava_rock", name: "Lava Rock", icon: "🌋", themes: ["lava"] },
  { id: "lava_glow", name: "Lava Glow", icon: "🔥", themes: ["lava"] },
  { id: "alien_crystal", name: "Alien Crystal", icon: "💎", themes: ["space"] },
  { id: "alien", name: "Alien", icon: "👽", themes: ["space"] },
  { id: "asteroid", name: "Asteroid", icon: "☄️", themes: ["space"] },
];

const ARENA_RADIUS = 8;

function getLocalStorageArenas(): CustomArena[] {
  try {
    const stored = localStorage.getItem("custom_arenas");
    return stored ? JSON.parse(stored) : [];
  } catch {
    return [];
  }
}

function saveLocalStorageArenas(arenas: CustomArena[]) {
  localStorage.setItem("custom_arenas", JSON.stringify(arenas));
}

function DecorationMesh({ type, selected }: { type: string; selected?: boolean }) {
  switch (type) {
    case "oak_tree":
      return (
        <group>
          <mesh position={[0, 0.4, 0]}>
            <cylinderGeometry args={[0.1, 0.15, 0.6, 6]} />
            <meshStandardMaterial color="#5D4037" />
          </mesh>
          <mesh position={[0, 1.1, 0]}>
            <sphereGeometry args={[0.6, 8, 8]} />
            <meshStandardMaterial color={selected ? "#4CAF50" : "#2E7D32"} />
          </mesh>
        </group>
      );
    case "pine_tree":
      return (
        <group>
          <mesh position={[0, 0.3, 0]}>
            <cylinderGeometry args={[0.08, 0.12, 0.5, 6]} />
            <meshStandardMaterial color="#4E342E" />
          </mesh>
          <mesh position={[0, 0.7, 0]}>
            <coneGeometry args={[0.4, 0.6, 6]} />
            <meshStandardMaterial color={selected ? "#4CAF50" : "#1B5E20"} />
          </mesh>
          <mesh position={[0, 1.1, 0]}>
            <coneGeometry args={[0.3, 0.5, 6]} />
            <meshStandardMaterial color={selected ? "#66BB6A" : "#2E7D32"} />
          </mesh>
        </group>
      );
    case "mushroom_red":
    case "mushroom_white":
      return (
        <group>
          <mesh position={[0, 0.15, 0]}>
            <cylinderGeometry args={[0.08, 0.1, 0.25, 8]} />
            <meshStandardMaterial color="#F5DEB3" />
          </mesh>
          <mesh position={[0, 0.35, 0]}>
            <sphereGeometry args={[0.2, 16, 8, 0, Math.PI * 2, 0, Math.PI / 2]} />
            <meshStandardMaterial color={type === "mushroom_red" ? (selected ? "#FF6B6B" : "#EF4444") : "#FFFFFF"} />
          </mesh>
        </group>
      );
    case "flower_pink":
    case "flower_yellow":
      return (
        <group>
          <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
            <circleGeometry args={[0.4, 8]} />
            <meshStandardMaterial color="#7CB342" />
          </mesh>
          <mesh position={[0, 0.12, 0]}>
            <sphereGeometry args={[0.08, 6, 6]} />
            <meshStandardMaterial 
              color={type === "flower_pink" ? (selected ? "#FF69B4" : "#EC407A") : (selected ? "#FFEB3B" : "#FDD835")} 
              emissive={type === "flower_pink" ? "#FF69B4" : "#FFEB3B"}
              emissiveIntensity={selected ? 0.5 : 0.2}
            />
          </mesh>
        </group>
      );
    case "snow_pine":
      return (
        <group>
          <mesh position={[0, 0.25, 0]}>
            <cylinderGeometry args={[0.08, 0.1, 0.4, 6]} />
            <meshStandardMaterial color="#5D4037" />
          </mesh>
          <mesh position={[0, 0.6, 0]}>
            <coneGeometry args={[0.35, 0.5, 6]} />
            <meshStandardMaterial color={selected ? "#A5D6A7" : "#E8F5E9"} />
          </mesh>
          <mesh position={[0, 0.95, 0]}>
            <coneGeometry args={[0.25, 0.4, 6]} />
            <meshStandardMaterial color="#FFFFFF" />
          </mesh>
        </group>
      );
    case "snowman":
      return (
        <group>
          <mesh position={[0, 0.25, 0]}>
            <sphereGeometry args={[0.25, 16, 16]} />
            <meshStandardMaterial color={selected ? "#E3F2FD" : "white"} />
          </mesh>
          <mesh position={[0, 0.55, 0]}>
            <sphereGeometry args={[0.18, 16, 16]} />
            <meshStandardMaterial color="white" />
          </mesh>
          <mesh position={[0, 0.8, 0]}>
            <sphereGeometry args={[0.12, 16, 16]} />
            <meshStandardMaterial color="white" />
          </mesh>
        </group>
      );
    case "ice_patch":
      return (
        <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <circleGeometry args={[0.8, 16]} />
          <meshStandardMaterial color={selected ? "#90CAF9" : "#E3F2FD"} transparent opacity={0.6} />
        </mesh>
      );
    case "hoodoo":
      return (
        <group>
          <mesh position={[0, 0.4, 0]}>
            <cylinderGeometry args={[0.2, 0.3, 0.7, 6]} />
            <meshStandardMaterial color={selected ? "#FFAB91" : "#D4A574"} />
          </mesh>
          <mesh position={[0, 0.9, 0]}>
            <cylinderGeometry args={[0.25, 0.2, 0.4, 6]} />
            <meshStandardMaterial color="#C4956A" />
          </mesh>
          <mesh position={[0, 1.2, 0]}>
            <sphereGeometry args={[0.28, 6, 6]} />
            <meshStandardMaterial color="#B4855A" />
          </mesh>
        </group>
      );
    case "lava_rock":
      return (
        <mesh position={[0, 0.25, 0]}>
          <dodecahedronGeometry args={[0.3, 0]} />
          <meshStandardMaterial color={selected ? "#616161" : "#4A4A4A"} roughness={0.9} />
        </mesh>
      );
    case "lava_glow":
      return (
        <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <circleGeometry args={[0.6, 16]} />
          <meshStandardMaterial 
            color="#FF4500" 
            emissive="#FF4500" 
            emissiveIntensity={selected ? 0.8 : 0.5}
            transparent 
            opacity={0.5} 
          />
        </mesh>
      );
    case "alien_crystal":
      return (
        <group>
          <mesh position={[0, 0.4, 0]}>
            <cylinderGeometry args={[0.1, 0.25, 0.7, 5]} />
            <meshStandardMaterial 
              color={selected ? "#BA68C8" : "#9C27B0"} 
              emissive="#9C27B0" 
              emissiveIntensity={selected ? 0.6 : 0.4}
              transparent
              opacity={0.9}
            />
          </mesh>
          <mesh position={[0, 0.85, 0]}>
            <octahedronGeometry args={[0.2, 0]} />
            <meshStandardMaterial 
              color="#E040FB" 
              emissive="#E040FB" 
              emissiveIntensity={0.7}
              transparent
              opacity={0.85}
            />
          </mesh>
        </group>
      );
    case "alien":
      return (
        <group>
          <mesh position={[0, 0.3, 0]}>
            <capsuleGeometry args={[0.15, 0.25, 8, 16]} />
            <meshStandardMaterial color={selected ? "#9CCC65" : "#7CFC00"} emissive="#7CFC00" emissiveIntensity={0.3} />
          </mesh>
          <mesh position={[0, 0.65, 0]}>
            <sphereGeometry args={[0.18, 16, 16]} />
            <meshStandardMaterial color="#7CFC00" emissive="#7CFC00" emissiveIntensity={0.3} />
          </mesh>
        </group>
      );
    case "asteroid":
      return (
        <mesh position={[0, 0.3, 0]}>
          <icosahedronGeometry args={[0.35, 0]} />
          <meshStandardMaterial color={selected ? "#9E9E9E" : "#757575"} roughness={0.8} />
        </mesh>
      );
    default:
      return (
        <mesh position={[0, 0.25, 0]}>
          <boxGeometry args={[0.4, 0.4, 0.4]} />
          <meshStandardMaterial color={selected ? "#90CAF9" : "#2196F3"} />
        </mesh>
      );
  }
}

function PlacedDecorationComponent({ 
  decoration, 
  selected, 
  onClick 
}: { 
  decoration: PlacedDecoration; 
  selected: boolean;
  onClick: () => void;
}) {
  const groupRef = useRef<THREE.Group>(null);
  
  return (
    <group 
      ref={groupRef}
      position={decoration.position}
      rotation={[0, decoration.rotation, 0]}
      scale={decoration.scale}
      onClick={(e) => {
        e.stopPropagation();
        onClick();
      }}
    >
      <DecorationMesh type={decoration.type} selected={selected} />
      {selected && (
        <mesh position={[0, 0, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[0.5, 0.6, 16]} />
          <meshBasicMaterial color="#FFEB3B" transparent opacity={0.8} />
        </mesh>
      )}
    </group>
  );
}

function EditorArena({ 
  theme, 
  decorations, 
  selectedId,
  onSelectDecoration,
  onPlaceDecoration,
  placingType
}: { 
  theme: MapTheme;
  decorations: PlacedDecoration[];
  selectedId: string | null;
  onSelectDecoration: (id: string | null) => void;
  onPlaceDecoration: (position: [number, number, number]) => void;
  placingType: string | null;
}) {
  const groupRef = useRef<THREE.Group>(null);
  const { camera, raycaster, pointer } = useThree();
  
  const arenaColor = useMemo(() => {
    switch (theme) {
      case "grass": return "#4CAF50";
      case "ice": return "#81D4FA";
      case "lava": return "#FF5722";
      case "space": return "#7C4DFF";
      case "tomb": return "#E0B88A";
      default: return "#4CAF50";
    }
  }, [theme]);
  
  useFrame(() => {
    if (groupRef.current) {
      groupRef.current.rotation.y += 0.001;
    }
  });
  
  const handleArenaClick = useCallback((e: THREE.Event) => {
    if (placingType) {
      const point = (e as any).point;
      if (point) {
        const dist = Math.sqrt(point.x * point.x + point.z * point.z);
        if (dist < ARENA_RADIUS * 0.9) {
          onPlaceDecoration([point.x, 0, point.z]);
        }
      }
    } else {
      onSelectDecoration(null);
    }
  }, [placingType, onPlaceDecoration, onSelectDecoration]);
  
  return (
    <group ref={groupRef}>
      <mesh 
        position={[0, -0.15, 0]} 
        onClick={handleArenaClick}
        onPointerDown={(e) => e.stopPropagation()}
      >
        <cylinderGeometry args={[ARENA_RADIUS, ARENA_RADIUS, 0.3, 64]} />
        <meshStandardMaterial color="#1a1a2e" />
      </mesh>
      
      <mesh position={[0, 0.01, 0]} onClick={handleArenaClick}>
        <cylinderGeometry args={[ARENA_RADIUS - 0.2, ARENA_RADIUS - 0.2, 0.02, 64]} />
        <meshStandardMaterial color={arenaColor} />
      </mesh>
      
      <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[ARENA_RADIUS - 1, ARENA_RADIUS - 0.2, 64]} />
        <meshStandardMaterial color="#ff4444" opacity={0.3} transparent side={THREE.DoubleSide} />
      </mesh>
      
      {decorations.map((dec) => (
        <PlacedDecorationComponent
          key={dec.id}
          decoration={dec}
          selected={selectedId === dec.id}
          onClick={() => onSelectDecoration(dec.id)}
        />
      ))}
      
      {placingType && (
        <mesh position={[0, 0.5, 0]}>
          <sphereGeometry args={[0.2, 8, 8]} />
          <meshBasicMaterial color="#FFEB3B" transparent opacity={0.5} />
        </mesh>
      )}
    </group>
  );
}

export function ArenaEditor() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const setMapForEditing = useZoogiGame((state) => state.setMapForEditing);
  const setDeveloperMoveMode = useZoogiGame((state) => state.setDeveloperMoveMode);
  const [customArenas, setCustomArenas] = useState<CustomArena[]>([]);
  const [editingArena, setEditingArena] = useState<CustomArena | null>(null);
  const [editingBaseArena, setEditingBaseArena] = useState<MapTheme | null>(null);
  const [selectedTheme, setSelectedTheme] = useState<MapTheme>("grass");
  const [selectedDecorationId, setSelectedDecorationId] = useState<string | null>(null);
  const [placingType, setPlacingType] = useState<string | null>(null);
  const [arenaName, setArenaName] = useState("My Arena");
  const [showSaveConfirm, setShowSaveConfirm] = useState(false);
  
  useEffect(() => {
    setCustomArenas(getLocalStorageArenas());
  }, []);
  
  const availableDecorations = useMemo(() => {
    return DECORATION_TYPES.filter(d => d.themes.includes(selectedTheme));
  }, [selectedTheme]);
  
  const handleCreateNew = () => {
    const newArena: CustomArena = {
      id: `arena_${Date.now()}`,
      name: "New Arena",
      theme: selectedTheme,
      decorations: [],
      createdAt: Date.now()
    };
    setEditingArena(newArena);
    setArenaName(newArena.name);
  };
  
  const handleEditArena = (arena: CustomArena) => {
    setEditingArena(arena);
    setSelectedTheme(arena.theme);
    setArenaName(arena.name);
  };
  
  const handleEditBaseArena = (theme: MapTheme) => {
    setEditingBaseArena(theme);
    setSelectedTheme(theme);
    setMapForEditing(theme);
    setDeveloperMoveMode(true);
  };
  
  const handleExitBaseArenaEditor = () => {
    setEditingBaseArena(null);
    setDeveloperMoveMode(false);
  };
  
  const handleDeleteArena = (id: string) => {
    const updated = customArenas.filter(a => a.id !== id);
    setCustomArenas(updated);
    saveLocalStorageArenas(updated);
  };
  
  const handlePlaceDecoration = useCallback((position: [number, number, number]) => {
    if (!editingArena || !placingType) return;
    
    const newDecoration: PlacedDecoration = {
      id: `dec_${Date.now()}`,
      type: placingType,
      position,
      rotation: Math.random() * Math.PI * 2,
      scale: 1
    };
    
    setEditingArena({
      ...editingArena,
      decorations: [...editingArena.decorations, newDecoration]
    });
    setPlacingType(null);
  }, [editingArena, placingType]);
  
  const handleDeleteSelected = useCallback(() => {
    if (!editingArena || !selectedDecorationId) return;
    
    setEditingArena({
      ...editingArena,
      decorations: editingArena.decorations.filter(d => d.id !== selectedDecorationId)
    });
    setSelectedDecorationId(null);
  }, [editingArena, selectedDecorationId]);
  
  const handleSaveArena = useCallback(() => {
    if (!editingArena) return;
    
    const updatedArena = {
      ...editingArena,
      name: arenaName,
      theme: selectedTheme
    };
    
    const existingIndex = customArenas.findIndex(a => a.id === updatedArena.id);
    let updated: CustomArena[];
    
    if (existingIndex >= 0) {
      updated = [...customArenas];
      updated[existingIndex] = updatedArena;
    } else {
      updated = [...customArenas, updatedArena];
    }
    
    setCustomArenas(updated);
    saveLocalStorageArenas(updated);
    setShowSaveConfirm(true);
    setTimeout(() => setShowSaveConfirm(false), 2000);
  }, [editingArena, arenaName, selectedTheme, customArenas]);
  
  const handleBack = () => {
    if (editingBaseArena) {
      handleExitBaseArenaEditor();
    } else if (editingArena) {
      setEditingArena(null);
      setSelectedDecorationId(null);
      setPlacingType(null);
    } else {
      setPhase("menu");
    }
  };
  
  // Base arena editing mode - shows the actual game arena with developer controls enabled
  if (editingBaseArena) {
    const mapOption = MAP_OPTIONS.find(m => m.id === editingBaseArena);
    return (
      <div className="fixed inset-0 bg-gradient-to-b from-gray-900 to-black flex flex-col">
        <div className="flex items-center justify-between p-4 bg-black/50 z-10">
          <button
            onClick={handleBack}
            className="flex items-center gap-2 text-white/70 hover:text-white"
          >
            <ChevronLeft size={20} />
            Back
          </button>
          
          <h2 className="text-white font-bold text-lg">
            Editing: {mapOption?.name || editingBaseArena}
          </h2>
          
          <div className="w-20" />
        </div>
        
        <div className="flex-1 relative">
          <Canvas
            camera={{ position: [0, 25, 25], fov: 50 }}
            gl={{ antialias: true }}
          >
            <ambientLight intensity={0.6} />
            <directionalLight position={[10, 15, 10]} intensity={1} />
            <pointLight position={[-10, 10, -10]} intensity={0.4} />
            <Arena theme={editingBaseArena} />
            <DeveloperMoveControls />
            <OrbitControls 
              enablePan={true} 
              enableZoom={true} 
              enableRotate={true}
              maxPolarAngle={Math.PI / 2.2}
              minDistance={10}
              maxDistance={50}
            />
          </Canvas>
          
          <div className="absolute bottom-4 left-1/2 -translate-x-1/2 bg-black/70 text-white px-4 py-2 rounded-lg text-sm text-center">
            <p>Drag green circles to move elements</p>
            <p className="text-white/60 text-xs">Use mouse/touch to rotate and zoom camera</p>
          </div>
        </div>
      </div>
    );
  }
  
  if (editingArena) {
    return (
      <div className="fixed inset-0 bg-gradient-to-b from-gray-900 to-black flex flex-col">
        <div className="flex items-center justify-between p-4 bg-black/50">
          <button
            onClick={handleBack}
            className="flex items-center gap-2 text-white/70 hover:text-white"
          >
            <ChevronLeft size={20} />
            Back
          </button>
          
          <input
            type="text"
            value={arenaName}
            onChange={(e) => setArenaName(e.target.value)}
            className="bg-white/10 text-white text-center font-bold px-4 py-2 rounded-lg border border-white/20"
            placeholder="Arena Name"
          />
          
          <button
            onClick={handleSaveArena}
            className="flex items-center gap-2 px-4 py-2 bg-green-500 text-white font-semibold rounded-lg hover:bg-green-600"
          >
            <Save size={18} />
            Save
          </button>
        </div>
        
        <div className="flex-1 relative">
          <Canvas
            camera={{ position: [0, 12, 12], fov: 50 }}
            gl={{ antialias: true }}
          >
            <ambientLight intensity={0.6} />
            <directionalLight position={[10, 15, 10]} intensity={1} />
            <pointLight position={[-10, 10, -10]} intensity={0.4} />
            <EditorArena
              theme={selectedTheme}
              decorations={editingArena.decorations}
              selectedId={selectedDecorationId}
              onSelectDecoration={setSelectedDecorationId}
              onPlaceDecoration={handlePlaceDecoration}
              placingType={placingType}
            />
          </Canvas>
          
          {placingType && (
            <div className="absolute top-4 left-1/2 -translate-x-1/2 bg-yellow-500 text-black px-4 py-2 rounded-full font-semibold text-sm">
              Tap on arena to place {DECORATION_TYPES.find(d => d.id === placingType)?.name}
            </div>
          )}
          
          {selectedDecorationId && (
            <motion.button
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              onClick={handleDeleteSelected}
              className="absolute top-4 right-4 flex items-center gap-2 px-4 py-2 bg-red-500 text-white font-semibold rounded-lg"
            >
              <Trash2 size={18} />
              Delete
            </motion.button>
          )}
          
          <AnimatePresence>
            {showSaveConfirm && (
              <motion.div
                initial={{ opacity: 0, y: -20 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0 }}
                className="absolute top-4 left-1/2 -translate-x-1/2 bg-green-500 text-white px-6 py-3 rounded-full font-bold flex items-center gap-2"
              >
                <Check size={20} />
                Arena Saved!
              </motion.div>
            )}
          </AnimatePresence>
        </div>
        
        <div className="p-4 bg-black/70">
          <div className="flex gap-2 mb-3 overflow-x-auto pb-2">
            {MAP_OPTIONS.map((map) => (
              <button
                key={map.id}
                onClick={() => {
                  setSelectedTheme(map.id);
                  setEditingArena({ ...editingArena, theme: map.id, decorations: [] });
                }}
                className={`px-4 py-2 rounded-lg font-semibold text-sm whitespace-nowrap transition-all ${
                  selectedTheme === map.id
                    ? "ring-2 ring-yellow-400"
                    : "opacity-60"
                }`}
                style={{ backgroundColor: map.color }}
              >
                {map.name}
              </button>
            ))}
          </div>
          
          <div className="flex gap-2 overflow-x-auto pb-2">
            {availableDecorations.map((dec) => (
              <button
                key={dec.id}
                onClick={() => setPlacingType(placingType === dec.id ? null : dec.id)}
                className={`flex flex-col items-center gap-1 px-3 py-2 rounded-lg transition-all min-w-[70px] ${
                  placingType === dec.id
                    ? "bg-yellow-500 text-black"
                    : "bg-white/10 text-white hover:bg-white/20"
                }`}
              >
                <span className="text-2xl">{dec.icon}</span>
                <span className="text-xs font-medium">{dec.name}</span>
              </button>
            ))}
          </div>
        </div>
      </div>
    );
  }
  
  return (
    <div className="fixed inset-0 bg-gradient-to-b from-gray-900 to-black flex flex-col p-6">
      <div className="flex items-center justify-between mb-6">
        <button
          onClick={() => setPhase("menu")}
          className="flex items-center gap-2 text-white/70 hover:text-white"
        >
          <ChevronLeft size={20} />
          Back
        </button>
        <h1 className="text-2xl font-bold text-white">My Arenas</h1>
        <div className="w-20" />
      </div>
      
      <button
        onClick={handleCreateNew}
        className="flex items-center justify-center gap-2 w-full py-4 bg-gradient-to-r from-cyan-500 to-teal-500 text-white font-bold rounded-xl mb-6 hover:shadow-lg hover:shadow-cyan-500/30 transition-all"
      >
        <Plus size={24} />
        Create New Arena
      </button>
      
      <p className="text-white/60 text-sm mb-2">Edit base arena elements:</p>
      <div className="flex gap-2 mb-4">
        {MAP_OPTIONS.map((map) => (
          <button
            key={map.id}
            onClick={() => handleEditBaseArena(map.id)}
            className={`flex-1 px-3 py-2 rounded-lg font-semibold text-sm transition-all ${
              selectedTheme === map.id
                ? "ring-2 ring-yellow-400"
                : "opacity-60"
            }`}
            style={{ backgroundColor: map.color }}
          >
            {map.name.split(" ")[0]}
          </button>
        ))}
      </div>
      
      <div className="flex-1 overflow-y-auto">
        {customArenas.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-full text-white/50">
            <p className="text-lg mb-2">No custom arenas yet</p>
            <p className="text-sm">Create your first arena above!</p>
          </div>
        ) : (
          <div className="grid gap-4">
            {customArenas.map((arena) => (
              <motion.div
                key={arena.id}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                className="bg-white/10 rounded-xl p-4"
              >
                <div className="flex items-center justify-between mb-2">
                  <h3 className="text-white font-bold text-lg">{arena.name}</h3>
                  <div 
                    className="px-2 py-1 rounded text-xs font-semibold"
                    style={{ backgroundColor: MAP_OPTIONS.find(m => m.id === arena.theme)?.color }}
                  >
                    {MAP_OPTIONS.find(m => m.id === arena.theme)?.name}
                  </div>
                </div>
                
                <p className="text-white/60 text-sm mb-3">
                  {arena.decorations.length} decorations
                </p>
                
                <div className="flex gap-2">
                  <button
                    onClick={() => handleEditArena(arena)}
                    className="flex-1 flex items-center justify-center gap-2 py-2 bg-blue-500 text-white font-semibold rounded-lg"
                  >
                    Edit
                  </button>
                  <button
                    onClick={() => handleDeleteArena(arena.id)}
                    className="px-4 py-2 bg-red-500/20 text-red-400 rounded-lg hover:bg-red-500/30"
                  >
                    <Trash2 size={18} />
                  </button>
                </div>
              </motion.div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
