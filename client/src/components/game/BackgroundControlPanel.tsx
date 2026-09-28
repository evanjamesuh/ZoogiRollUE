import { useState, useRef } from "react";
import { Move, RotateCcw, FlipHorizontal, ImagePlus, X, Eye, EyeOff, Box, Layers, Target, Move3d, ChevronDown, ChevronUp, RotateCw } from "lucide-react";
import { DEFAULT_BACKGROUND_SETTINGS, DEFAULT_WALL_SETTINGS, DEFAULT_ZONE_SETTINGS, DEFAULT_ELEMENT_TRANSFORMS, useZoogiGame, ZoneEditorConfig, TransformElementType, TransformMode } from "@/lib/stores/useZoogiGame";

export interface BackgroundSettings {
  distance: number;
  yPos: number;
  width: number;
  height: number;
  rotation: number;
  mirrorBack: boolean;
  mirrorFront: boolean;
  mirrorLeft: boolean;
  mirrorRight: boolean;
  opacity: number;
  visible: boolean;
  customImage: string | null;
  groundScale: number;
  modelPositionX: number;
  modelPositionY: number;
  modelPositionZ: number;
  modelScale: number;
}

export interface WallSettings {
  outerWallEnabled: boolean;
  outerWallRadiusOffset: number;
  outerWallRows: number;
  outerWallBlocksPerRow: number;
  outerWallGapWidth: number;
  middleWallEnabled: boolean;
  middleWallRadiusOffset: number;
  middleWallRows: number;
  middleWallBlocksPerRow: number;
  middleWallGapWidth: number;
  innerWallEnabled: boolean;
  innerWallRadiusPercent: number;
  innerWallRows: number;
  innerWallBlocksPerRow: number;
  knockoffBoundaryRadius: number;
  knockoffBoundaryWidth: number;
  // New PBR/Video outer walls
  pbrInnerWallEnabled: boolean;
  pbrInnerWallRadiusOffset: number;
  pbrMiddleWallEnabled: boolean;
  pbrMiddleWallRadiusOffset: number;
  videoWallEnabled: boolean;
  videoWallRadiusOffset: number;
  videoWallHeight: number;
}

export interface ZoneSettings {
  globalRotationOffset: number;
  globalDistanceOffset: number;
  globalScale: number;
  zonesVisible: boolean;
}

export { DEFAULT_ZONE_SETTINGS };

interface BackgroundControlPanelProps {
  settings: BackgroundSettings;
  onSettingsChange: (settings: BackgroundSettings) => void;
  onImageUpload: (imageDataUrl: string) => void;
  wallSettings?: WallSettings;
  onWallSettingsChange?: (settings: WallSettings) => void;
  zoneSettings?: ZoneSettings;
  onZoneSettingsChange?: (settings: ZoneSettings) => void;
  defaultOpen?: boolean;
  isOpen?: boolean;
  onOpenChange?: (open: boolean) => void;
}

export { DEFAULT_BACKGROUND_SETTINGS };

export function BackgroundControlPanel({ settings, onSettingsChange, onImageUpload, wallSettings, onWallSettingsChange, zoneSettings, onZoneSettingsChange, defaultOpen = false, isOpen: controlledIsOpen, onOpenChange }: BackgroundControlPanelProps) {
  const [internalIsOpen, setInternalIsOpen] = useState(defaultOpen);
  
  // Use controlled state if provided, otherwise use internal state
  const isOpen = controlledIsOpen !== undefined ? controlledIsOpen : internalIsOpen;
  const setIsOpen = (open: boolean) => {
    if (onOpenChange) {
      onOpenChange(open);
    } else {
      setInternalIsOpen(open);
    }
  };
  const [activeSection, setActiveSection] = useState<string | null>("position");
  const fileInputRef = useRef<HTMLInputElement>(null);
  
  const zoneEditorConfigs = useZoogiGame(state => state.zoneEditorConfigs);
  const updateZoneConfig = useZoogiGame(state => state.updateZoneConfig);
  const initializeZoneConfigs = useZoogiGame(state => state.initializeZoneConfigs);
  
  const selectedTransformElement = useZoogiGame(state => state.selectedTransformElement);
  const setSelectedTransformElement = useZoogiGame(state => state.setSelectedTransformElement);
  const showTransformGizmo = useZoogiGame(state => state.showTransformGizmo);
  const setShowTransformGizmo = useZoogiGame(state => state.setShowTransformGizmo);
  const setElementTransforms = useZoogiGame(state => state.setElementTransforms);
  const transformMode = useZoogiGame(state => state.transformMode);
  const setTransformMode = useZoogiGame(state => state.setTransformMode);
  
  const toggleGizmo = (element: TransformElementType) => {
    console.log("[BackgroundControlPanel] toggleGizmo called:", { element, currentSelected: selectedTransformElement, showTransformGizmo });
    if (selectedTransformElement === element && showTransformGizmo) {
      console.log("[BackgroundControlPanel] Hiding gizmo");
      setSelectedTransformElement(null);
      setShowTransformGizmo(false);
    } else {
      console.log("[BackgroundControlPanel] Showing gizmo for:", element);
      setSelectedTransformElement(element);
      setShowTransformGizmo(true);
    }
  };

  const updateSetting = <K extends keyof BackgroundSettings>(key: K, value: BackgroundSettings[K]) => {
    onSettingsChange({ ...settings, [key]: value });
  };

  const updateWallSetting = <K extends keyof WallSettings>(key: K, value: WallSettings[K]) => {
    if (wallSettings && onWallSettingsChange) {
      console.log("[BackgroundControlPanel] Updating wall setting:", key, "=", value);
      onWallSettingsChange({ ...wallSettings, [key]: value });
    }
  };
  
  const updateZoneSetting = <K extends keyof ZoneSettings>(key: K, value: ZoneSettings[K]) => {
    if (zoneSettings && onZoneSettingsChange) {
      onZoneSettingsChange({ ...zoneSettings, [key]: value });
    }
  };
  
  const applyGlobalZoneRotation = (rotationOffset: number) => {
    if (!zoneSettings || !onZoneSettingsChange) return;
    
    const rotationDelta = (rotationOffset - zoneSettings.globalRotationOffset) * (Math.PI / 180);
    
    zoneEditorConfigs.forEach(zone => {
      updateZoneConfig(zone.id, { angle: zone.angle + rotationDelta });
    });
    
    onZoneSettingsChange({ ...zoneSettings, globalRotationOffset: rotationOffset });
  };
  
  const applyGlobalZoneDistance = (distanceOffset: number) => {
    if (!zoneSettings || !onZoneSettingsChange) return;
    
    const distanceDelta = distanceOffset - zoneSettings.globalDistanceOffset;
    
    zoneEditorConfigs.forEach(zone => {
      updateZoneConfig(zone.id, { distance: Math.max(5, zone.distance + distanceDelta) });
    });
    
    onZoneSettingsChange({ ...zoneSettings, globalDistanceOffset: distanceOffset });
  };
  
  const toggleAllZonesVisible = (visible: boolean) => {
    zoneEditorConfigs.forEach(zone => {
      updateZoneConfig(zone.id, { visible });
    });
    if (zoneSettings && onZoneSettingsChange) {
      onZoneSettingsChange({ ...zoneSettings, zonesVisible: visible });
    }
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (event) => {
        const dataUrl = event.target?.result as string;
        onImageUpload(dataUrl);
      };
      reader.readAsDataURL(file);
    }
  };

  const Section = ({ id, title, icon: Icon, children, gizmoElement }: { id: string; title: string; icon: React.ElementType; children: React.ReactNode; gizmoElement?: TransformElementType }) => {
    const isGizmoActive = gizmoElement && selectedTransformElement === gizmoElement && showTransformGizmo;
    const isOpen = activeSection === id;
    
    return (
      <div className="border-b border-white/10 last:border-b-0">
        <div className="flex items-center">
          <div className="flex-1 flex items-center px-3 py-3">
            <div className="flex items-center gap-2 flex-1">
              <Icon size={16} className="text-purple-400" />
              <span className="text-sm font-medium">{title}</span>
            </div>
          </div>
          {gizmoElement && (
            <button
              type="button"
              onPointerDown={(e) => {
                e.preventDefault();
                e.stopPropagation();
                console.log("[GIZMO BUTTON] POINTER DOWN:", gizmoElement);
                toggleGizmo(gizmoElement);
              }}
              className={`p-3 rounded transition-colors cursor-pointer ${isGizmoActive ? 'bg-cyan-500 text-white' : 'hover:bg-white/10 text-white/50'}`}
              title={isGizmoActive ? "Hide Transform Gizmo" : "Show Transform Gizmo"}
            >
              <Move3d size={16} />
            </button>
          )}
          <button
            type="button"
            onClick={(e) => {
              e.preventDefault();
              e.stopPropagation();
              setActiveSection(isOpen ? null : id);
            }}
            className="p-2 mr-1 rounded hover:bg-white/10 transition-colors cursor-pointer"
            title={isOpen ? "Collapse section" : "Expand section"}
          >
            {isOpen ? <ChevronUp size={16} className="text-white/50" /> : <ChevronDown size={16} className="text-white/50" />}
          </button>
        </div>
        {isOpen && (
          <div className="p-2 pt-0 space-y-2">
            {children}
          </div>
        )}
      </div>
    );
  };

  const Slider = ({ label, value, onChange, min, max, step = 1 }: { label: string; value: number; onChange: (v: number) => void; min: number; max: number; step?: number }) => (
    <div className="space-y-1">
      <div className="flex justify-between text-[10px]">
        <span className="text-white/60">{label}</span>
        <span className="text-purple-300">{typeof value === 'number' ? (step < 1 ? value.toFixed(1) : Math.round(value)) : value}</span>
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

  const Toggle = ({ label, checked, onChange }: { label: string; checked: boolean; onChange: (v: boolean) => void }) => (
    <div className="flex items-center justify-between">
      <span className="text-[10px] text-white/60">{label}</span>
      <button
        onClick={() => onChange(!checked)}
        className={`w-8 h-4 rounded-full transition-colors ${checked ? 'bg-purple-500' : 'bg-white/20'}`}
      >
        <div className={`w-3 h-3 rounded-full bg-white transition-transform ${checked ? 'translate-x-4' : 'translate-x-0.5'}`} />
      </button>
    </div>
  );

  if (!isOpen) {
    // In controlled mode, don't render anything when closed (parent controls visibility)
    if (controlledIsOpen !== undefined) {
      return null;
    }
    // In uncontrolled mode, show a button to open
    return (
      <button
        onClick={() => setIsOpen(true)}
        className="fixed left-4 top-1/2 -translate-y-1/2 z-50 p-3 bg-gradient-to-br from-purple-600 to-indigo-700 rounded-full text-white shadow-lg hover:scale-110 transition-transform pointer-events-auto"
        title="Background Controls"
      >
        <ImagePlus size={20} />
      </button>
    );
  }

  return (
    <div className="fixed left-4 top-1/2 -translate-y-1/2 z-50 w-64 bg-black/90 backdrop-blur-xl rounded-xl border border-white/20 text-white shadow-2xl overflow-hidden pointer-events-auto">
      <div className="flex items-center justify-between p-3 border-b border-white/10 bg-gradient-to-r from-purple-600/20 to-indigo-600/20">
        <div className="flex items-center gap-2">
          <ImagePlus size={16} className="text-purple-400" />
          <span className="text-sm font-bold">Background Controls</span>
        </div>
        <button onClick={() => setIsOpen(false)} className="p-1 hover:bg-white/10 rounded">
          <X size={14} />
        </button>
      </div>

      <div className="max-h-[60vh] overflow-y-auto">
        <div className="p-2 border-b border-white/10">
          <button
            onClick={() => updateSetting('visible', !settings.visible)}
            className={`w-full flex items-center justify-center gap-2 p-2 rounded-lg transition-colors ${settings.visible ? 'bg-purple-500/30 text-purple-300' : 'bg-white/10 text-white/60'}`}
          >
            {settings.visible ? <Eye size={16} /> : <EyeOff size={16} />}
            <span className="text-xs font-medium">{settings.visible ? 'Visible' : 'Hidden'}</span>
          </button>
        </div>

        {selectedTransformElement && showTransformGizmo && (
          <div className="p-2 border-b border-white/10 bg-cyan-900/30">
            <div className="text-xs text-cyan-300 mb-2 text-center font-medium">
              Gizmo Mode: {selectedTransformElement.replace('_', ' ')}
            </div>
            <div className="flex gap-2">
              <button
                onClick={() => setTransformMode("translate")}
                className={`flex-1 flex items-center justify-center gap-1 p-2 rounded-lg transition-colors ${transformMode === 'translate' ? 'bg-cyan-500 text-white' : 'bg-white/10 text-white/60 hover:bg-white/20'}`}
              >
                <Move3d size={14} />
                <span className="text-xs">Move</span>
              </button>
              <button
                onClick={() => setTransformMode("rotate")}
                className={`flex-1 flex items-center justify-center gap-1 p-2 rounded-lg transition-colors ${transformMode === 'rotate' ? 'bg-cyan-500 text-white' : 'bg-white/10 text-white/60 hover:bg-white/20'}`}
              >
                <RotateCw size={14} />
                <span className="text-xs">Rotate</span>
              </button>
            </div>
          </div>
        )}

        <Section id="position" title="Position" icon={Move}>
          <Slider label="Distance" value={settings.distance} onChange={(v) => updateSetting('distance', v)} min={50} max={500} step={10} />
          <Slider label="Height (Y)" value={settings.yPos} onChange={(v) => updateSetting('yPos', v)} min={0} max={200} step={5} />
          <Slider label="Width" value={settings.width} onChange={(v) => updateSetting('width', v)} min={100} max={1000} step={25} />
          <Slider label="Height" value={settings.height} onChange={(v) => updateSetting('height', v)} min={50} max={500} step={25} />
          <Slider label="Ground Size" value={settings.groundScale ?? 3} onChange={(v) => updateSetting('groundScale', v)} min={1} max={10} step={0.5} />
        </Section>

        <Section id="rotation" title="Rotation" icon={RotateCcw}>
          <Slider label="Rotation (°)" value={settings.rotation} onChange={(v) => updateSetting('rotation', v)} min={0} max={360} step={15} />
          <Slider label="Opacity" value={settings.opacity} onChange={(v) => updateSetting('opacity', v)} min={0.1} max={1} step={0.1} />
        </Section>

        <Section id="mirror" title="Mirror" icon={FlipHorizontal}>
          <Toggle label="Mirror Back" checked={settings.mirrorBack} onChange={(v) => updateSetting('mirrorBack', v)} />
          <Toggle label="Mirror Front" checked={settings.mirrorFront} onChange={(v) => updateSetting('mirrorFront', v)} />
          <Toggle label="Mirror Left" checked={settings.mirrorLeft} onChange={(v) => updateSetting('mirrorLeft', v)} />
          <Toggle label="Mirror Right" checked={settings.mirrorRight} onChange={(v) => updateSetting('mirrorRight', v)} />
        </Section>

        <Section id="image" title="Custom Image" icon={ImagePlus}>
          <input
            ref={fileInputRef}
            type="file"
            accept="image/*"
            onChange={handleFileChange}
            className="hidden"
          />
          <button
            onClick={() => fileInputRef.current?.click()}
            className="w-full p-2 bg-purple-500/20 hover:bg-purple-500/30 border border-purple-500/50 rounded-lg text-xs text-purple-300 transition-colors"
          >
            Upload New Image
          </button>
          {settings.customImage && (
            <button
              onClick={() => updateSetting('customImage', null)}
              className="w-full p-2 bg-red-500/20 hover:bg-red-500/30 border border-red-500/50 rounded-lg text-xs text-red-300 transition-colors"
            >
              Reset to Default
            </button>
          )}
        </Section>

        <Section id="model" title="3D Model" icon={Box} gizmoElement="arena_model">
          <Slider label="Position X" value={settings.modelPositionX ?? 0} onChange={(v) => updateSetting('modelPositionX', v)} min={-10} max={10} step={0.5} />
          <Slider label="Position Y" value={settings.modelPositionY ?? -0.5} onChange={(v) => updateSetting('modelPositionY', v)} min={-5} max={5} step={0.1} />
          <Slider label="Position Z" value={settings.modelPositionZ ?? 0} onChange={(v) => updateSetting('modelPositionZ', v)} min={-10} max={10} step={0.5} />
          <Slider label="Scale" value={settings.modelScale ?? 3} onChange={(v) => updateSetting('modelScale', v)} min={0.5} max={200} step={1} />
        </Section>

        {wallSettings && onWallSettingsChange && (
          <>
            <Section id="cyanBlockWall" title="Cyan Block Wall" icon={Layers} gizmoElement="outer_wall">
              <Toggle label="Enabled" checked={wallSettings.outerWallEnabled} onChange={(v) => updateWallSetting('outerWallEnabled', v)} />
              <Slider label="Distance Offset" value={wallSettings.outerWallRadiusOffset} onChange={(v) => updateWallSetting('outerWallRadiusOffset', v)} min={0} max={25} step={1} />
              <Slider label="Rows" value={wallSettings.outerWallRows} onChange={(v) => updateWallSetting('outerWallRows', v)} min={1} max={10} step={1} />
              <Slider label="Blocks/Row" value={wallSettings.outerWallBlocksPerRow} onChange={(v) => updateWallSetting('outerWallBlocksPerRow', v)} min={8} max={120} step={1} />
            </Section>

            <Section id="bumperWall" title="Bumper Wall" icon={Layers} gizmoElement="middle_wall">
              <Toggle label="Enabled" checked={wallSettings.middleWallEnabled} onChange={(v) => updateWallSetting('middleWallEnabled', v)} />
              <Slider label="Distance Offset" value={wallSettings.middleWallRadiusOffset} onChange={(v) => updateWallSetting('middleWallRadiusOffset', v)} min={-10} max={15} step={1} />
              <Slider label="Rows" value={wallSettings.middleWallRows} onChange={(v) => updateWallSetting('middleWallRows', v)} min={1} max={10} step={1} />
              <Slider label="Blocks/Row" value={wallSettings.middleWallBlocksPerRow} onChange={(v) => updateWallSetting('middleWallBlocksPerRow', v)} min={8} max={120} step={1} />
            </Section>

            <Section id="innerWall" title="Inner Barrier Wall" icon={Layers} gizmoElement="inner_wall">
              <Toggle label="Enabled" checked={wallSettings.innerWallEnabled} onChange={(v) => updateWallSetting('innerWallEnabled', v)} />
              <Slider label="Radius (%)" value={wallSettings.innerWallRadiusPercent} onChange={(v) => updateWallSetting('innerWallRadiusPercent', v)} min={10} max={100} step={1} />
              <Slider label="Rows" value={wallSettings.innerWallRows} onChange={(v) => updateWallSetting('innerWallRows', v)} min={1} max={10} step={1} />
            </Section>


            <Section id="knockoffBoundary" title="Knockoff Boundary" icon={Target} gizmoElement="knockoff_boundary">
              <Slider label="Radius" value={wallSettings.knockoffBoundaryRadius ?? 12} onChange={(v) => updateWallSetting('knockoffBoundaryRadius', v)} min={5} max={100} step={1} />
              <Slider label="Line Width" value={wallSettings.knockoffBoundaryWidth ?? 0.3} onChange={(v) => updateWallSetting('knockoffBoundaryWidth', v)} min={0.1} max={2} step={0.1} />
            </Section>
          </>
        )}

        {zoneSettings && onZoneSettingsChange && (
          <Section id="scoreZones" title="Score Zones" icon={Target} gizmoElement="zones">
            <div className="space-y-3">
              <button
                onClick={() => toggleAllZonesVisible(!zoneSettings.zonesVisible)}
                className={`w-full flex items-center justify-center gap-2 p-2 rounded-lg transition-colors ${zoneSettings.zonesVisible ? 'bg-cyan-500/30 text-cyan-300' : 'bg-white/10 text-white/60'}`}
              >
                {zoneSettings.zonesVisible ? <Eye size={14} /> : <EyeOff size={14} />}
                <span className="text-xs font-medium">{zoneSettings.zonesVisible ? 'All Visible' : 'All Hidden'}</span>
              </button>
              
              <Slider 
                label="Rotate All (°)" 
                value={zoneSettings.globalRotationOffset} 
                onChange={(v) => applyGlobalZoneRotation(v)} 
                min={0} 
                max={360} 
                step={1} 
              />
              
              <Slider 
                label="Distance Offset" 
                value={zoneSettings.globalDistanceOffset} 
                onChange={(v) => applyGlobalZoneDistance(v)} 
                min={-25} 
                max={25} 
                step={1} 
              />
              
              <div className="border-t border-white/10 pt-2 mt-2">
                <span className="text-[10px] text-white/60 block mb-2">Individual Zones</span>
                <div className="space-y-1 max-h-32 overflow-y-auto">
                  {zoneEditorConfigs.map((zone, index) => (
                    <div key={zone.id} className="flex items-center justify-between p-1.5 bg-white/5 rounded">
                      <span className="text-[10px] text-white/80">Zone {index + 1}</span>
                      <div className="flex items-center gap-2">
                        <button
                          onClick={() => updateZoneConfig(zone.id, { isSpawn: !zone.isSpawn })}
                          className={`px-1.5 py-0.5 text-[9px] rounded transition-colors ${zone.isSpawn ? 'bg-green-500/30 text-green-300' : 'bg-white/10 text-white/40'}`}
                          title="Spawn Point"
                        >
                          SP
                        </button>
                        <button
                          onClick={() => updateZoneConfig(zone.id, { visible: !zone.visible })}
                          className={`p-1 rounded transition-colors ${zone.visible ? 'text-cyan-400' : 'text-white/30'}`}
                        >
                          {zone.visible ? <Eye size={12} /> : <EyeOff size={12} />}
                        </button>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
              
              <button
                onClick={() => {
                  initializeZoneConfigs();
                  onZoneSettingsChange(DEFAULT_ZONE_SETTINGS);
                }}
                className="w-full p-1.5 bg-white/10 hover:bg-white/20 rounded text-[10px] text-white/70 transition-colors"
              >
                Reset Zones
              </button>
            </div>
          </Section>
        )}
      </div>

      <div className="p-2 border-t border-white/10">
        <button
          onClick={() => {
            console.log("[BackgroundControlPanel] Reset All to Default clicked");
            
            // Reset background settings
            onSettingsChange(DEFAULT_BACKGROUND_SETTINGS);
            
            // Reset wall settings
            if (onWallSettingsChange) {
              onWallSettingsChange(DEFAULT_WALL_SETTINGS);
            }
            
            // Reset zone settings
            if (onZoneSettingsChange) {
              onZoneSettingsChange(DEFAULT_ZONE_SETTINGS);
              initializeZoneConfigs();
            }
            
            // Reset transform gizmo state
            setSelectedTransformElement(null);
            setShowTransformGizmo(false);
            
            // Reset all element transforms to default positions
            setElementTransforms(DEFAULT_ELEMENT_TRANSFORMS);
            
            console.log("[BackgroundControlPanel] All settings reset to defaults");
          }}
          className="w-full p-2 bg-white/10 hover:bg-white/20 rounded-lg text-xs transition-colors"
        >
          Reset All to Default
        </button>
      </div>
    </div>
  );
}
