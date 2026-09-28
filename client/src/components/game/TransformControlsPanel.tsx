import { useState } from "react";
import { Move, RotateCcw, Maximize, X, ChevronDown, ChevronUp, Box, Layers, Target, Circle, Square, Eye, EyeOff } from "lucide-react";
import { useZoogiGame, TransformElementType, TransformMode } from "@/lib/stores/useZoogiGame";

const ELEMENT_OPTIONS: { id: TransformElementType; label: string; icon: React.ElementType; color: string }[] = [
  { id: "arena_model", label: "Arena Model", icon: Box, color: "bg-blue-500" },
  { id: "outer_wall", label: "Cyan Block Wall", icon: Layers, color: "bg-cyan-500" },
  { id: "middle_wall", label: "Bumper Wall", icon: Layers, color: "bg-purple-500" },
  { id: "inner_wall", label: "Inner Wall", icon: Layers, color: "bg-pink-500" },
  { id: "zones", label: "Score Zones", icon: Target, color: "bg-green-500" },
  { id: "knockoff_boundary", label: "Knockoff Boundary", icon: Circle, color: "bg-orange-500" },
];

const MODE_OPTIONS: { id: TransformMode; label: string; icon: React.ElementType; key: string }[] = [
  { id: "translate", label: "Move", icon: Move, key: "W" },
  { id: "rotate", label: "Rotate", icon: RotateCcw, key: "E" },
  { id: "scale", label: "Scale", icon: Maximize, key: "R" },
];

export function TransformControlsPanel() {
  const [isOpen, setIsOpen] = useState(true);
  
  const selectedTransformElement = useZoogiGame((state) => state.selectedTransformElement);
  const transformMode = useZoogiGame((state) => state.transformMode);
  const showTransformGizmo = useZoogiGame((state) => state.showTransformGizmo);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  const setSelectedTransformElement = useZoogiGame((state) => state.setSelectedTransformElement);
  const setTransformMode = useZoogiGame((state) => state.setTransformMode);
  const setShowTransformGizmo = useZoogiGame((state) => state.setShowTransformGizmo);
  const updateElementTransform = useZoogiGame((state) => state.updateElementTransform);
  const gameMode = useZoogiGame((state) => state.gameMode);
  
  if (gameMode !== "map_editor") return null;
  
  const getCurrentOffset = () => {
    switch (selectedTransformElement) {
      case "arena_model": return elementTransforms.arenaModelOffset;
      case "outer_wall": return elementTransforms.outerWallOffset;
      case "middle_wall": return elementTransforms.middleWallOffset;
      case "inner_wall": return elementTransforms.innerWallOffset;
      case "zones": return elementTransforms.zonesOffset;
      case "knockoff_boundary": return elementTransforms.knockoffBoundaryOffset;
      default: return { x: 0, y: 0, z: 0 };
    }
  };
  
  const handleOffsetChange = (axis: "x" | "y" | "z", value: number) => {
    if (!selectedTransformElement) return;
    const current = getCurrentOffset();
    updateElementTransform(selectedTransformElement, { ...current, [axis]: value });
  };

  const Slider = ({ label, value, onChange, min, max, step = 0.1 }: { label: string; value: number; onChange: (v: number) => void; min: number; max: number; step?: number }) => (
    <div className="space-y-1">
      <div className="flex justify-between text-[10px]">
        <span className="text-white/60">{label}</span>
        <span className="text-purple-300">{value.toFixed(1)}</span>
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onInput={(e) => onChange(Number((e.target as HTMLInputElement).value))}
        onChange={(e) => onChange(Number(e.target.value))}
        className="w-full h-1 bg-white/20 rounded-full appearance-none cursor-pointer [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-purple-500 [&::-webkit-slider-thumb]:rounded-full"
      />
    </div>
  );

  return (
    <div className="absolute top-20 left-4 w-56 bg-black/80 backdrop-blur-md rounded-lg border border-white/10 text-white text-sm overflow-hidden z-50">
      <div className="flex items-center justify-between p-2 border-b border-white/10 bg-gradient-to-r from-purple-900/50 to-transparent">
        <div className="flex items-center gap-2">
          <Move size={14} className="text-purple-400" />
          <span className="text-xs font-semibold">Transform Controls</span>
        </div>
        <div className="flex items-center gap-1">
          <button 
            onClick={() => setShowTransformGizmo(!showTransformGizmo)} 
            className={`p-1 rounded transition-colors ${showTransformGizmo ? 'bg-purple-600 text-white' : 'hover:bg-white/10 text-white/50'}`}
            title={showTransformGizmo ? "Hide 3D Gizmo" : "Show 3D Gizmo"}
          >
            {showTransformGizmo ? <Eye size={12} /> : <EyeOff size={12} />}
          </button>
          <button onClick={() => setIsOpen(!isOpen)} className="p-1 hover:bg-white/10 rounded">
            {isOpen ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
          </button>
        </div>
      </div>
      
      {isOpen && (
        <div className="p-2 space-y-3">
          <div className="space-y-1">
            <span className="text-[10px] text-white/60 uppercase tracking-wider">Select Element</span>
            <div className="grid grid-cols-2 gap-1">
              {ELEMENT_OPTIONS.map((option) => {
                const Icon = option.icon;
                const isSelected = selectedTransformElement === option.id;
                return (
                  <button
                    key={option.id}
                    onClick={() => setSelectedTransformElement(isSelected ? null : option.id)}
                    className={`flex items-center gap-1 p-1.5 rounded text-[10px] transition-all ${
                      isSelected 
                        ? `${option.color} text-white` 
                        : "bg-white/10 hover:bg-white/20 text-white/70"
                    }`}
                  >
                    <Icon size={10} />
                    <span className="truncate">{option.label}</span>
                  </button>
                );
              })}
            </div>
          </div>
          
          {selectedTransformElement && (
            <>
              <div className="space-y-1">
                <span className="text-[10px] text-white/60 uppercase tracking-wider">Mode</span>
                <div className="flex gap-1">
                  {MODE_OPTIONS.map((mode) => {
                    const Icon = mode.icon;
                    const isSelected = transformMode === mode.id;
                    return (
                      <button
                        key={mode.id}
                        onClick={() => setTransformMode(mode.id)}
                        className={`flex-1 flex flex-col items-center gap-0.5 p-1.5 rounded text-[9px] transition-all ${
                          isSelected 
                            ? "bg-purple-600 text-white" 
                            : "bg-white/10 hover:bg-white/20 text-white/70"
                        }`}
                        title={`${mode.label} (${mode.key})`}
                      >
                        <Icon size={12} />
                        <span>{mode.label}</span>
                        <span className="text-[8px] opacity-50">{mode.key}</span>
                      </button>
                    );
                  })}
                </div>
              </div>
              
              <div className="space-y-2 border-t border-white/10 pt-2">
                <span className="text-[10px] text-white/60 uppercase tracking-wider">Position Offset</span>
                <Slider 
                  label="X" 
                  value={getCurrentOffset().x} 
                  onChange={(v) => handleOffsetChange("x", v)} 
                  min={-50} 
                  max={50} 
                  step={0.5}
                />
                <Slider 
                  label="Y" 
                  value={getCurrentOffset().y} 
                  onChange={(v) => handleOffsetChange("y", v)} 
                  min={-20} 
                  max={20} 
                  step={0.1}
                />
                <Slider 
                  label="Z" 
                  value={getCurrentOffset().z} 
                  onChange={(v) => handleOffsetChange("z", v)} 
                  min={-50} 
                  max={50} 
                  step={0.5}
                />
              </div>
              
              <button
                onClick={() => {
                  if (selectedTransformElement) {
                    updateElementTransform(selectedTransformElement, { x: 0, y: 0, z: 0 });
                  }
                }}
                className="w-full py-1.5 text-[10px] bg-white/10 hover:bg-white/20 rounded transition-colors"
              >
                Reset to Origin
              </button>
            </>
          )}
          
          <div className="text-[9px] text-white/40 text-center pt-1 border-t border-white/10">
            Click element to select, use sliders to move
          </div>
        </div>
      )}
    </div>
  );
}
