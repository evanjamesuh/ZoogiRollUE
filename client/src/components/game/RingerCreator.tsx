import { useState, useRef, useEffect, useMemo, useCallback, Suspense } from "react";
import { Canvas, useThree, useFrame } from "@react-three/fiber";
import { OrbitControls, useGLTF, Environment } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { ChevronLeft, ChevronRight, Check, RotateCcw, Play, ChevronUp, ChevronDown, Copy } from "lucide-react";
import * as THREE from "three";

interface CharacterPart {
  category: string;
  label: string;
  options: { id: string; name: string; path: string }[];
}

const CHARACTER_PARTS: CharacterPart[] = [
  {
    category: "face",
    label: "Face",
    options: [
      { id: "face_usual", name: "Usual", path: "/models/character/face_usual.glb" },
      { id: "face_happy", name: "Happy", path: "/models/character/face_happy.glb" },
      { id: "face_angry", name: "Angry", path: "/models/character/face_angry.glb" },
    ],
  },
  {
    category: "hair",
    label: "Hair",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "hair_short", name: "Short", path: "/models/character/hair_short.glb" },
      { id: "hair_wavy", name: "Wavy", path: "/models/character/hair_wavy.glb" },
    ],
  },
  {
    category: "hat",
    label: "Hat",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "hat_helmet", name: "Helmet", path: "/models/character/hat_helmet.glb" },
      { id: "hat_straw", name: "Straw", path: "/models/character/hat_straw.glb" },
      { id: "hat_sports", name: "Sports", path: "/models/character/hat_sports.glb" },
    ],
  },
  {
    category: "top",
    label: "Top",
    options: [
      { id: "tshirt", name: "T-Shirt", path: "/models/character/tshirt.glb" },
      { id: "outerwear_jacket", name: "Jacket", path: "/models/character/outerwear_jacket.glb" },
      { id: "outerwear_hoodie", name: "Hoodie", path: "/models/character/outerwear_hoodie.glb" },
    ],
  },
  {
    category: "bottom",
    label: "Bottom",
    options: [
      { id: "pants_jeans", name: "Jeans", path: "/models/character/pants_jeans.glb" },
      { id: "pants_cargo", name: "Cargo", path: "/models/character/pants_cargo.glb" },
      { id: "shorts", name: "Shorts", path: "/models/character/shorts.glb" },
    ],
  },
  {
    category: "shoes",
    label: "Shoes",
    options: [
      { id: "shoes_sneakers", name: "Sneakers", path: "/models/character/shoes_sneakers.glb" },
      { id: "shoes_slippers", name: "Slippers", path: "/models/character/shoes_slippers_blue.glb" },
      { id: "shoes_sandals", name: "Sandals", path: "/models/character/shoes_sandals.glb" },
    ],
  },
  {
    category: "glasses",
    label: "Glasses",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "glasses_round", name: "Round", path: "/models/character/glasses_round.glb" },
      { id: "glasses_square", name: "Square", path: "/models/character/glasses_square.glb" },
    ],
  },
  {
    category: "gloves",
    label: "Gloves",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "gloves_dark", name: "Dark", path: "/models/character/gloves_dark.glb" },
      { id: "gloves_light", name: "Light", path: "/models/character/gloves_light.glb" },
    ],
  },
  {
    category: "mustache",
    label: "Mustache",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "mustache_handlebar", name: "Handlebar", path: "/models/character/mustache_handlebar.glb" },
      { id: "mustache_walrus", name: "Walrus", path: "/models/character/mustache_walrus.glb" },
    ],
  },
  {
    category: "accessory",
    label: "Accessory",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "acc_headphones", name: "Headphones", path: "/models/character/acc_headphones.glb" },
      { id: "acc_clown_nose", name: "Clown Nose", path: "/models/character/acc_clown_nose.glb" },
    ],
  },
  {
    category: "costume",
    label: "Costume",
    options: [
      { id: "none", name: "None", path: "" },
      { id: "costume_clown", name: "Clown", path: "/models/character/costume_clown.glb" },
      { id: "costume_mouse", name: "Mouse", path: "/models/character/costume_mouse.glb" },
    ],
  },
];

const SKIN_COLORS = [
  { id: "default", name: "Default", color: "#FFD5B8" },
  { id: "light", name: "Light", color: "#FFECD2" },
  { id: "tan", name: "Tan", color: "#D4A574" },
  { id: "brown", name: "Brown", color: "#8D5524" },
  { id: "dark", name: "Dark", color: "#5C3A1E" },
  { id: "blue", name: "Blue", color: "#4A90D9" },
  { id: "green", name: "Green", color: "#4ADE80" },
  { id: "red", name: "Red", color: "#EF4444" },
];

function CharacterPartModel({ path, skinColor }: { path: string; skinColor: string }) {
  const { scene } = useGLTF(path);
  const clonedScene = useMemo(() => {
    const clone = scene.clone(true);
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh && child.material) {
        const mat = child.material as THREE.MeshStandardMaterial;
        if (mat.map) {
        } else {
          child.material = mat.clone();
        }
      }
    });
    return clone;
  }, [scene, skinColor]);

  return <primitive object={clonedScene} />;
}

function CharacterBody({ skinColor }: { skinColor: string }) {
  const { scene } = useGLTF("/models/character/body.glb");
  const clonedScene = useMemo(() => {
    const clone = scene.clone(true);
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        const mat = (child.material as THREE.MeshStandardMaterial).clone();
        if (!mat.map) {
          mat.color.set(skinColor);
        }
        child.material = mat;
      }
    });
    return clone;
  }, [scene, skinColor]);

  return <primitive object={clonedScene} />;
}

function CharacterPreview({
  selections,
  skinColor,
}: {
  selections: Record<string, string>;
  skinColor: string;
}) {
  const groupRef = useRef<THREE.Group>(null);

  const activeParts = useMemo(() => {
    const parts: { category: string; path: string }[] = [];
    CHARACTER_PARTS.forEach((part) => {
      const selectedId = selections[part.category];
      if (selectedId && selectedId !== "none") {
        const option = part.options.find((o) => o.id === selectedId);
        if (option && option.path) {
          parts.push({ category: part.category, path: option.path });
        }
      }
    });
    return parts;
  }, [selections]);

  return (
    <group ref={groupRef} position={[0, -0.5, 0]}>
      <Suspense fallback={null}>
        <CharacterBody skinColor={skinColor} />
        {activeParts.map((part) => (
          <CharacterPartModel key={part.category + part.path} path={part.path} skinColor={skinColor} />
        ))}
      </Suspense>
    </group>
  );
}

function CameraController({ cameraYRef, orbitTargetYRef }: { cameraYRef: React.MutableRefObject<number>; orbitTargetYRef: React.MutableRefObject<number> }) {
  const { camera } = useThree();
  const controlsRef = useRef<any>(null);

  useFrame(() => {
    camera.position.y = cameraYRef.current;
    if (controlsRef.current) {
      controlsRef.current.target.y = orbitTargetYRef.current;
      controlsRef.current.update();
    }
  });

  return (
    <OrbitControls
      ref={controlsRef}
      enableZoom={true}
      enablePan={false}
      minDistance={2}
      maxDistance={6}
      target={[0, orbitTargetYRef.current, 0]}
      minPolarAngle={Math.PI / 6}
      maxPolarAngle={Math.PI / 2}
    />
  );
}

export function RingerCreator() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const [activeCategory, setActiveCategory] = useState(0);
  const [selections, setSelections] = useState<Record<string, string>>(() => {
    const defaults: Record<string, string> = {};
    CHARACTER_PARTS.forEach((part) => {
      defaults[part.category] = part.options[0].id;
    });
    return defaults;
  });
  const [skinColorIndex, setSkinColorIndex] = useState(0);
  const [ringerName, setRingerName] = useState("");

  const currentPart = CHARACTER_PARTS[activeCategory];
  const currentSelection = selections[currentPart.category];
  const currentOptionIndex = currentPart.options.findIndex((o) => o.id === currentSelection);

  const handleNextOption = () => {
    const nextIndex = (currentOptionIndex + 1) % currentPart.options.length;
    setSelections((prev) => ({
      ...prev,
      [currentPart.category]: currentPart.options[nextIndex].id,
    }));
  };

  const handlePrevOption = () => {
    const prevIndex = (currentOptionIndex - 1 + currentPart.options.length) % currentPart.options.length;
    setSelections((prev) => ({
      ...prev,
      [currentPart.category]: currentPart.options[prevIndex].id,
    }));
  };

  const cameraYRef = useRef(1.5);
  const orbitTargetYRef = useRef(0.7);
  const [cameraY, setCameraY] = useState(1.5);
  const [targetY, setTargetY] = useState(0.7);
  const [exported, setExported] = useState(false);

  const adjustCamera = useCallback((dir: "up" | "down") => {
    const step = 0.1;
    const delta = dir === "up" ? step : -step;
    cameraYRef.current = parseFloat((cameraYRef.current + delta).toFixed(2));
    orbitTargetYRef.current = parseFloat((orbitTargetYRef.current + delta).toFixed(2));
    setCameraY(cameraYRef.current);
    setTargetY(orbitTargetYRef.current);
  }, []);

  const exportCameraPosition = useCallback(() => {
    const data = `Camera Y: ${cameraYRef.current}, Target Y: ${orbitTargetYRef.current}`;
    navigator.clipboard.writeText(data).then(() => {
      setExported(true);
      setTimeout(() => setExported(false), 2000);
    });
  }, []);

  const handleReset = () => {
    const defaults: Record<string, string> = {};
    CHARACTER_PARTS.forEach((part) => {
      defaults[part.category] = part.options[0].id;
    });
    setSelections(defaults);
    setSkinColorIndex(0);
    setRingerName("");
  };

  const handleConfirm = () => {
    const savedRinger = {
      name: ringerName || "Ringer",
      selections,
      skinColor: SKIN_COLORS[skinColorIndex].color,
      createdAt: Date.now(),
    };
    localStorage.setItem("ringerCharacter", JSON.stringify(savedRinger));
    setPhase("ringer_trials_loading");
  };

  return (
    <div className="fixed inset-0 bg-gradient-to-b from-gray-900 via-purple-900/30 to-gray-900 flex flex-col overflow-hidden">
      <div className="flex items-center justify-between px-4 py-2 shrink-0">
        <button
          onClick={() => setPhase("menu")}
          className="flex items-center gap-1 px-3 py-1.5 bg-white/10 rounded-full text-white text-sm hover:bg-white/20 transition-all"
        >
          <ChevronLeft size={16} />
          Back
        </button>
        <h1 className="text-white text-lg font-bold">Create Your Ringer</h1>
        <button
          onClick={handleReset}
          className="flex items-center gap-1 px-3 py-1.5 bg-white/10 rounded-full text-white hover:bg-white/20 transition-all"
        >
          <RotateCcw size={14} />
        </button>
      </div>

      <div className="relative shrink-0" style={{ height: "38vh", minHeight: "200px" }}>
        <Canvas
          camera={{ position: [0, 1.5, 3.5], fov: 40 }}
          style={{ position: "absolute", inset: 0 }}
        >
          <ambientLight intensity={0.8} />
          <directionalLight position={[3, 5, 3]} intensity={1} />
          <directionalLight position={[-3, 3, -3]} intensity={0.3} />
          <pointLight position={[0, 3, 0]} intensity={0.5} color="#a855f7" />
          <CharacterPreview selections={selections} skinColor={SKIN_COLORS[skinColorIndex].color} />
          <Environment preset="city" />
          <CameraController cameraYRef={cameraYRef} orbitTargetYRef={orbitTargetYRef} />
        </Canvas>

        <div className="absolute right-2 top-1/2 -translate-y-1/2 flex flex-col gap-1 z-10">
          <button
            onClick={() => adjustCamera("up")}
            className="w-9 h-9 flex items-center justify-center bg-black/60 text-white rounded-full hover:bg-black/80 active:scale-90 transition-all"
          >
            <ChevronUp size={20} />
          </button>
          <button
            onClick={() => adjustCamera("down")}
            className="w-9 h-9 flex items-center justify-center bg-black/60 text-white rounded-full hover:bg-black/80 active:scale-90 transition-all"
          >
            <ChevronDown size={20} />
          </button>
        </div>

        <button
          onClick={exportCameraPosition}
          className="absolute left-2 bottom-2 flex items-center gap-1 px-2 py-1 bg-black/60 text-white/80 text-xs rounded-full hover:bg-black/80 active:scale-95 transition-all z-10"
        >
          <Copy size={12} />
          {exported ? "Copied!" : `Y:${cameraY} T:${targetY}`}
        </button>
      </div>

      <div className="flex-1 bg-black/60 backdrop-blur-lg border-t border-white/10 overflow-y-auto">
        <div className="max-w-md mx-auto px-4 py-3">
          <input
            type="text"
            value={ringerName}
            onChange={(e) => setRingerName(e.target.value)}
            placeholder="Enter Ringer name..."
            maxLength={20}
            className="w-full px-4 py-2 bg-white/10 border border-white/20 rounded-xl text-white placeholder-white/40 text-center text-base font-bold focus:outline-none focus:border-purple-500 mb-2"
          />

          <div className="flex items-center justify-center gap-1 mb-2">
            {SKIN_COLORS.map((sc, idx) => (
              <button
                key={sc.id}
                onClick={() => setSkinColorIndex(idx)}
                className={`w-7 h-7 rounded-full border-2 transition-all ${
                  idx === skinColorIndex ? "border-white scale-110" : "border-transparent"
                }`}
                style={{ backgroundColor: sc.color }}
                title={sc.name}
              />
            ))}
          </div>

          <div className="flex gap-1 overflow-x-auto pb-1 mb-2 scrollbar-hide">
            {CHARACTER_PARTS.map((part, idx) => (
              <button
                key={part.category}
                onClick={() => setActiveCategory(idx)}
                className={`px-3 py-1 rounded-full text-xs font-semibold whitespace-nowrap transition-all ${
                  idx === activeCategory
                    ? "bg-purple-500 text-white"
                    : "bg-white/10 text-white/60 hover:bg-white/20"
                }`}
              >
                {part.label}
              </button>
            ))}
          </div>

          <div className="flex items-center justify-between bg-white/5 rounded-xl p-2 mb-3">
            <button
              onClick={handlePrevOption}
              className="w-9 h-9 rounded-full bg-white/10 flex items-center justify-center text-white hover:bg-white/20 transition-all"
            >
              <ChevronLeft size={18} />
            </button>
            <div className="text-center">
              <div className="text-white/50 text-[10px] uppercase tracking-wider">{currentPart.label}</div>
              <div className="text-white font-bold text-base">
                {currentPart.options[currentOptionIndex]?.name || "None"}
              </div>
            </div>
            <button
              onClick={handleNextOption}
              className="w-9 h-9 rounded-full bg-white/10 flex items-center justify-center text-white hover:bg-white/20 transition-all"
            >
              <ChevronRight size={18} />
            </button>
          </div>

          <button
            onClick={handleConfirm}
            className="w-full flex items-center justify-center gap-2 px-6 py-3 bg-gradient-to-r from-green-500 to-emerald-600 text-white text-lg font-bold rounded-full shadow-lg shadow-green-500/30 hover:shadow-green-500/50 transition-all active:scale-95 mb-4"
          >
            <Play size={20} fill="white" />
            Enter the Trials
          </button>
        </div>
      </div>
    </div>
  );
}

useGLTF.preload("/models/character/body.glb");
