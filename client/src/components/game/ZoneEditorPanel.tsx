import { useState, useEffect } from "react";
import { Target, ChevronDown, ChevronUp, Eye, EyeOff, MapPin, X, RotateCcw } from "lucide-react";
import { useZoogiGame, ZoneEditorConfig } from "@/lib/stores/useZoogiGame";

export function ZoneEditorPanel() {
  const [isOpen, setIsOpen] = useState(false);
  const [activeSection, setActiveSection] = useState<string | null>(null);
  
  const zoneEditorConfigs = useZoogiGame(state => state.zoneEditorConfigs);
  const selectedZoneId = useZoogiGame(state => state.selectedZoneId);
  const selectZone = useZoogiGame(state => state.selectZone);
  const updateZoneConfig = useZoogiGame(state => state.updateZoneConfig);
  const initializeZoneConfigs = useZoogiGame(state => state.initializeZoneConfigs);
  const gameMode = useZoogiGame(state => state.gameMode);

  useEffect(() => {
    if (gameMode === "map_editor" && zoneEditorConfigs.length === 0) {
      initializeZoneConfigs();
    }
  }, [gameMode, zoneEditorConfigs.length, initializeZoneConfigs]);

  if (gameMode !== "map_editor") return null;

  const selectedZone = zoneEditorConfigs.find(z => z.id === selectedZoneId);

  const Slider = ({ label, value, onChange, min, max, step = 1 }: { label: string; value: number; onChange: (v: number) => void; min: number; max: number; step?: number }) => (
    <div className="space-y-1">
      <div className="flex justify-between text-[10px]">
        <span className="text-white/60">{label}</span>
        <span className="text-cyan-300">{typeof value === 'number' ? value.toFixed(1) : value}</span>
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
        className="w-full h-1 bg-white/20 rounded-full appearance-none cursor-pointer [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-cyan-500 [&::-webkit-slider-thumb]:rounded-full"
      />
    </div>
  );

  const Toggle = ({ label, checked, onChange, icon: Icon }: { label: string; checked: boolean; onChange: (v: boolean) => void; icon?: React.ElementType }) => (
    <div className="flex items-center justify-between">
      <div className="flex items-center gap-1">
        {Icon && <Icon size={12} className="text-cyan-400" />}
        <span className="text-[10px] text-white/60">{label}</span>
      </div>
      <button
        onClick={() => onChange(!checked)}
        className={`w-8 h-4 rounded-full transition-colors ${checked ? 'bg-cyan-500' : 'bg-white/20'}`}
      >
        <div className={`w-3 h-3 rounded-full bg-white transition-transform ${checked ? 'translate-x-4' : 'translate-x-0.5'}`} />
      </button>
    </div>
  );

  const Section = ({ id, title, children }: { id: string; title: string; children: React.ReactNode }) => (
    <div className="border-b border-white/10 last:border-b-0">
      <button
        onClick={() => setActiveSection(activeSection === id ? null : id)}
        className="w-full flex items-center justify-between p-2 hover:bg-white/5 transition-colors"
      >
        <span className="text-xs font-medium">{title}</span>
        {activeSection === id ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
      </button>
      {activeSection === id && (
        <div className="p-2 pt-0 space-y-2">
          {children}
        </div>
      )}
    </div>
  );

  if (!isOpen) {
    return (
      <button
        onClick={() => setIsOpen(true)}
        className="fixed right-4 top-1/2 -translate-y-1/2 z-50 p-3 bg-gradient-to-br from-cyan-600 to-blue-700 rounded-full text-white shadow-lg hover:scale-110 transition-transform pointer-events-auto"
        title="Zone Editor"
      >
        <Target size={20} />
      </button>
    );
  }

  return (
    <div className="fixed right-4 top-1/2 -translate-y-1/2 z-50 w-64 bg-black/90 backdrop-blur-xl rounded-xl border border-white/20 text-white shadow-2xl overflow-hidden pointer-events-auto">
      <div className="flex items-center justify-between p-3 border-b border-white/10 bg-gradient-to-r from-cyan-600/20 to-blue-600/20">
        <div className="flex items-center gap-2">
          <Target size={16} className="text-cyan-400" />
          <span className="text-sm font-bold">Zone Editor</span>
        </div>
        <button onClick={() => setIsOpen(false)} className="p-1 hover:bg-white/10 rounded">
          <X size={14} />
        </button>
      </div>

      <div className="max-h-[60vh] overflow-y-auto">
        <Section id="zones" title="Score Rings">
          <div className="space-y-1">
            {zoneEditorConfigs.map((zone, i) => (
              <button
                key={zone.id}
                onClick={() => selectZone(selectedZoneId === zone.id ? null : zone.id)}
                className={`w-full flex items-center justify-between p-2 rounded-lg text-xs transition-colors ${
                  selectedZoneId === zone.id 
                    ? 'bg-cyan-500/30 border border-cyan-400/50' 
                    : 'bg-white/5 hover:bg-white/10'
                }`}
              >
                <span>Zone {i + 1}</span>
                <div className="flex items-center gap-2">
                  {zone.isSpawn && <MapPin size={12} className="text-green-400" />}
                  {zone.visible ? (
                    <Eye size={12} className="text-cyan-400" />
                  ) : (
                    <EyeOff size={12} className="text-white/40" />
                  )}
                </div>
              </button>
            ))}
          </div>
        </Section>

        {selectedZone && (
          <Section id="config" title={`Zone ${zoneEditorConfigs.findIndex(z => z.id === selectedZoneId) + 1} Settings`}>
            <Slider
              label="Rotation (°)"
              value={(selectedZone.angle * 180 / Math.PI) % 360}
              onChange={(v) => updateZoneConfig(selectedZone.id, { angle: (v * Math.PI / 180) })}
              min={0}
              max={360}
              step={15}
            />
            <Slider
              label="Distance"
              value={selectedZone.distance}
              onChange={(v) => updateZoneConfig(selectedZone.id, { distance: v })}
              min={5}
              max={25}
              step={0.5}
            />
            <Toggle
              label="Visible"
              checked={selectedZone.visible}
              onChange={(v) => updateZoneConfig(selectedZone.id, { visible: v })}
              icon={Eye}
            />
            <Toggle
              label="Spawn Point"
              checked={selectedZone.isSpawn}
              onChange={(v) => updateZoneConfig(selectedZone.id, { isSpawn: v })}
              icon={MapPin}
            />
          </Section>
        )}

        <div className="p-2">
          <button
            onClick={() => {
              const defaultConfigs = zoneEditorConfigs.map((zone, i) => ({
                ...zone,
                angle: (i / 6) * Math.PI * 2,
                distance: 10,
                visible: true,
                isSpawn: true,
              }));
              defaultConfigs.forEach(config => {
                updateZoneConfig(config.id, { 
                  angle: config.angle, 
                  distance: config.distance, 
                  visible: config.visible, 
                  isSpawn: config.isSpawn 
                });
              });
            }}
            className="w-full flex items-center justify-center gap-2 p-2 bg-white/10 hover:bg-white/20 rounded-lg text-xs transition-colors"
          >
            <RotateCcw size={12} />
            Reset All Zones
          </button>
        </div>
      </div>
    </div>
  );
}
