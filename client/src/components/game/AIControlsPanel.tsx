import type { ElementType } from "react";
import { useZoogiGame, DEFAULT_AI_CONTROLS } from "@/lib/stores/useZoogiGame";
import { X, Brain, Target, Shield, Clock, Crosshair, Zap } from "lucide-react";

export function AIControlsPanel() {
  const showPanel = useZoogiGame((state) => state.showAiControlsPanel);
  const aiControls = useZoogiGame((state) => state.aiControls);
  const setAiControls = useZoogiGame((state) => state.setAiControls);
  const setShowPanel = useZoogiGame((state) => state.setShowAiControlsPanel);

  if (!showPanel) return null;

  const SliderRow = ({ 
    label, 
    value, 
    min, 
    max, 
    step, 
    onChange, 
    icon: Icon,
    suffix = ""
  }: { 
    label: string; 
    value: number; 
    min: number; 
    max: number; 
    step: number; 
    onChange: (v: number) => void;
    icon: ElementType;
    suffix?: string;
  }) => (
    <div className="flex flex-col gap-1">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Icon size={14} className="text-orange-400" />
          <span className="text-white/80 text-xs">{label}</span>
        </div>
        <span className="text-orange-300 text-xs font-mono">{value.toFixed(2)}{suffix}</span>
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        onPointerDown={(e) => e.stopPropagation()}
        onTouchStart={(e) => e.stopPropagation()}
        className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer accent-orange-500"
        style={{ touchAction: 'auto' }}
      />
    </div>
  );

  return (
    <div 
      className="fixed top-20 right-4 w-72 bg-black/90 backdrop-blur-md rounded-xl p-4 z-50 border border-orange-500/30 pointer-events-auto"
      onPointerDown={(e) => e.stopPropagation()}
      onTouchStart={(e) => e.stopPropagation()}
      style={{ touchAction: 'auto' }}
    >
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <Brain size={18} className="text-orange-400" />
          <h3 className="text-white font-bold text-sm">AI Controls</h3>
        </div>
        <button
          onClick={() => setShowPanel(false)}
          className="text-white/60 hover:text-white transition-colors"
        >
          <X size={18} />
        </button>
      </div>

      <div className="space-y-4">
        <SliderRow
          label="Power Multiplier"
          value={aiControls.powerMultiplier}
          min={0.3}
          max={2.0}
          step={0.1}
          onChange={(v) => setAiControls({ powerMultiplier: v })}
          icon={Zap}
          suffix="x"
        />

        <div className="flex flex-col gap-1">
          <div className="flex items-center gap-2">
            <Target size={14} className="text-orange-400" />
            <span className="text-white/80 text-xs">Target Priority</span>
          </div>
          <select
            value={aiControls.targetPriority}
            onChange={(e) => setAiControls({ targetPriority: e.target.value as 'closest' | 'weakest' | 'random' })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full bg-gray-800 text-white text-xs rounded px-2 py-1.5 border border-gray-600"
            style={{ touchAction: 'auto' }}
          >
            <option value="closest">Closest Opponent</option>
            <option value="weakest">Weakest Opponent</option>
            <option value="random">Random Target</option>
          </select>
        </div>

        <SliderRow
          label="Edge Awareness"
          value={aiControls.edgeAwareness}
          min={0}
          max={1}
          step={0.1}
          onChange={(v) => setAiControls({ edgeAwareness: v })}
          icon={Target}
        />

        <SliderRow
          label="Self Preservation"
          value={aiControls.selfPreservation}
          min={0}
          max={1}
          step={0.1}
          onChange={(v) => setAiControls({ selfPreservation: v })}
          icon={Shield}
        />

        <SliderRow
          label="Reaction Delay"
          value={aiControls.reactionDelay}
          min={0.2}
          max={3.0}
          step={0.1}
          onChange={(v) => setAiControls({ reactionDelay: v })}
          icon={Clock}
          suffix="s"
        />

        <SliderRow
          label="Accuracy"
          value={aiControls.accuracy}
          min={0.1}
          max={1.0}
          step={0.1}
          onChange={(v) => setAiControls({ accuracy: v })}
          icon={Crosshair}
        />
      </div>

      <div className="mt-4 pt-3 border-t border-gray-700">
        <button
          onClick={() => setAiControls({ ...DEFAULT_AI_CONTROLS })}
          className="w-full px-3 py-1.5 bg-gray-700 hover:bg-gray-600 text-white text-xs rounded transition-colors"
        >
          Reset to Defaults
        </button>
      </div>

      <p className="text-white/40 text-[10px] mt-3 text-center">
        Adjust how AI opponents behave during gameplay
      </p>
    </div>
  );
}
