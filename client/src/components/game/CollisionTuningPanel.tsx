import { useZoogiGame, DEFAULT_COLLISION_TUNING } from "@/lib/stores/useZoogiGame";

export function CollisionTuningPanel() {
  const { collisionTuning, setCollisionTuning, showCollisionTuningPanel, setShowCollisionTuningPanel } = useZoogiGame();

  const handleExport = () => {
    const dataStr = JSON.stringify(collisionTuning, null, 2);
    const blob = new Blob([dataStr], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "collision_tuning.json";
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleImport = () => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".json";
    input.onchange = (e) => {
      const file = (e.target as HTMLInputElement).files?.[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (ev) => {
          try {
            const data = JSON.parse(ev.target?.result as string);
            setCollisionTuning(data);
          } catch (err) {
            console.error("Failed to parse collision tuning file:", err);
          }
        };
        reader.readAsText(file);
      }
    };
    input.click();
  };

  const handleReset = () => {
    setCollisionTuning({ ...DEFAULT_COLLISION_TUNING });
  };

  if (!showCollisionTuningPanel) return null;

  return (
    <div 
      className="fixed top-20 right-4 bg-black/90 text-white p-4 rounded-lg shadow-xl z-50 w-72 max-h-[80vh] overflow-y-auto pointer-events-auto"
      onPointerDown={(e) => e.stopPropagation()}
      onTouchStart={(e) => e.stopPropagation()}
      style={{ touchAction: 'auto' }}
    >
      <div className="flex justify-between items-center mb-4">
        <h3 className="text-lg font-bold text-cyan-400">Collision Tuning</h3>
        <button
          onClick={() => setShowCollisionTuningPanel(false)}
          className="text-gray-400 hover:text-white text-xl"
        >
          ×
        </button>
      </div>

      <div className="space-y-4">
        <div>
          <label className="text-xs text-gray-400">Collision Buffer</label>
          <input
            type="range"
            min="0.1"
            max="2.0"
            step="0.1"
            value={collisionTuning.collisionBuffer}
            onChange={(e) => setCollisionTuning({ collisionBuffer: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.collisionBuffer.toFixed(1)}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Bounce Strength</label>
          <input
            type="range"
            min="0.1"
            max="1.5"
            step="0.05"
            value={collisionTuning.bounceStrength}
            onChange={(e) => setCollisionTuning({ bounceStrength: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.bounceStrength.toFixed(2)}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Break Threshold</label>
          <input
            type="range"
            min="0.3"
            max="5.0"
            step="0.1"
            value={collisionTuning.breakThreshold}
            onChange={(e) => setCollisionTuning({ breakThreshold: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.breakThreshold.toFixed(1)}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Scatter Force</label>
          <input
            type="range"
            min="0.5"
            max="5.0"
            step="0.1"
            value={collisionTuning.scatterForce}
            onChange={(e) => setCollisionTuning({ scatterForce: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.scatterForce.toFixed(1)}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Particle Count</label>
          <input
            type="range"
            min="5"
            max="100"
            step="5"
            value={collisionTuning.particleCount}
            onChange={(e) => setCollisionTuning({ particleCount: parseInt(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.particleCount}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Orb Break Multiplier</label>
          <input
            type="range"
            min="0.2"
            max="2.0"
            step="0.1"
            value={collisionTuning.orbBreakMultiplier}
            onChange={(e) => setCollisionTuning({ orbBreakMultiplier: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.orbBreakMultiplier.toFixed(1)}</div>
        </div>

        <div>
          <label className="text-xs text-gray-400">Min Bounce Velocity</label>
          <input
            type="range"
            min="0.1"
            max="1.0"
            step="0.05"
            value={collisionTuning.minBounceVelocity}
            onChange={(e) => setCollisionTuning({ minBounceVelocity: parseFloat(e.target.value) })}
            onPointerDown={(e) => e.stopPropagation()}
            onTouchStart={(e) => e.stopPropagation()}
            className="w-full"
            style={{ touchAction: 'auto' }}
          />
          <div className="text-xs text-cyan-300">{collisionTuning.minBounceVelocity.toFixed(2)}</div>
        </div>
      </div>

      <div className="flex gap-2 mt-4">
        <button
          onClick={handleExport}
          className="flex-1 bg-green-600 hover:bg-green-500 text-white py-2 px-3 rounded text-sm"
        >
          Export
        </button>
        <button
          onClick={handleImport}
          className="flex-1 bg-blue-600 hover:bg-blue-500 text-white py-2 px-3 rounded text-sm"
        >
          Import
        </button>
        <button
          onClick={handleReset}
          className="flex-1 bg-red-600 hover:bg-red-500 text-white py-2 px-3 rounded text-sm"
        >
          Reset
        </button>
      </div>
    </div>
  );
}
