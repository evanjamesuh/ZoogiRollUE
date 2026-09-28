import { useState, useEffect, useMemo, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { useZoogiGame, EditorPlacedModel, EditorWallBlock, EditorScoringZone, WallSegmentConfig, DEFAULT_WALL_SETTINGS } from "@/lib/stores/useZoogiGame";
import { Package, Plus, Save, Trash2, Focus, Box, Shield, Target, RotateCw, Eye, EyeOff, Move, Sliders } from "lucide-react";
import { toast } from "sonner";
import { getDeviceId } from "@/lib/deviceId";

function ModelIcon({ name, type }: { name: string; type: "default" | "custom" }) {
  const bgColor = type === "default" 
    ? "from-purple-600 to-violet-700" 
    : "from-orange-500 to-amber-600";
  
  return (
    <div className={`w-12 h-12 rounded-lg bg-gradient-to-br ${bgColor} flex items-center justify-center`}>
      <Box size={24} className="text-white" />
    </div>
  );
}

interface AvailableModel {
  id: string;
  name: string;
  modelUrl: string;
  type: "default" | "custom";
  thumbnailUrl?: string;
}

const DEFAULT_MODELS: AvailableModel[] = [
  {
    id: "bumber1",
    name: "Bumber 1",
    modelUrl: "/models/bumber1.glb",
    type: "default"
  },
  {
    id: "bumber1_alt",
    name: "Bumber 1 (Alt)",
    modelUrl: "/models/bumber1.glb",
    type: "default"
  }
];

interface MapEditorPanelProps {
  isOpen: boolean;
  onClose: () => void;
}

export function MapEditorPanel({ isOpen, onClose }: MapEditorPanelProps) {
  const [customArenas, setCustomArenas] = useState<AvailableModel[]>([]);
  const [activeTab, setActiveTab] = useState<"models" | "placed" | "walls">("models");
  const [isSaving, setIsSaving] = useState(false);
  const [isAutoSaving, setIsAutoSaving] = useState(false);
  const deviceId = useMemo(() => getDeviceId(), []);
  const lastSavedModelsRef = useRef<string>("");
  const autoSaveIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  
  const { 
    selectedMap, 
    editorPlacedModels, 
    addEditorPlacedModel, 
    removeEditorPlacedModel,
    selectEditorModel,
    selectedEditorModelId,
    wallOwnershipConfig,
    toggleWallOwnershipEditor,
    addEditorWallBlock,
    updateEditorWallBlock,
    removeEditorWallBlock,
    selectEditorWallBlock,
    selectedEditorWallBlockId,
    addEditorScoringZone,
    updateEditorScoringZone,
    removeEditorScoringZone,
    selectEditorScoringZone,
    selectedEditorScoringZoneId,
    wallSettings,
    setWallSettings,
    wallSegmentConfigs,
    selectedWallSegmentId,
    selectWallSegment,
    toggleWallSegmentVisibility,
    innerWallSegmentConfigs,
    toggleInnerWallSegmentVisibility,
    selectedInnerWallSegmentId,
    selectInnerWallSegment,
    updateInnerWallSegmentConfig,
    updateWallSegmentConfig
  } = useZoogiGame();
  
  useEffect(() => {
    fetch(`/api/custom-arenas?deviceId=${deviceId}`)
      .then(res => res.json())
      .then(data => {
        if (data.data) {
          const customModels: AvailableModel[] = data.data.map((arena: any) => ({
            id: `custom_${arena.id}`,
            name: arena.name,
            modelUrl: `/api/meshy/download/${arena.meshyTaskId}`,
            type: "custom" as const,
            thumbnailUrl: arena.thumbnailUrl
          }));
          setCustomArenas(customModels);
        }
      })
      .catch(err => console.error("Failed to fetch custom arenas:", err));
  }, [deviceId]);

  const allModels = [...DEFAULT_MODELS, ...customArenas];

  const handlePlaceModel = (model: AvailableModel) => {
    const newId = addEditorPlacedModel({
      modelUrl: model.modelUrl,
      name: model.name,
      position: [0, 0, 0],
      rotation: [0, 0, 0],
      scale: [1, 1, 1]
    });
    selectEditorModel(newId);
  };

  const handleSaveAndClose = async () => {
    if (!selectedMap) {
      onClose();
      return;
    }
    
    setIsSaving(true);
    
    try {
      const state = useZoogiGame.getState();
      const storageKey = `map_decorations_${selectedMap}_${deviceId}`;
      const exportData = {
        mapId: selectedMap,
        deviceId,
        placedModels: editorPlacedModels,
        wallOwnershipConfig: wallOwnershipConfig,
        zoneSettings: state.zoneSettings,
        zoneEditorConfigs: state.zoneEditorConfigs,
        wallSettings: state.wallSettings,
        wallSegmentConfigs: state.wallSegmentConfigs,
        innerWallSegmentConfigs: state.innerWallSegmentConfigs,
        timestamp: Date.now()
      };
      
      localStorage.setItem(storageKey, JSON.stringify(exportData));
      console.log("Map decorations saved to localStorage:", storageKey);
      
      // Update lastSavedModelsRef for autosave change detection
      lastSavedModelsRef.current = JSON.stringify({ 
        models: editorPlacedModels, 
        config: wallOwnershipConfig,
        zoneSettings: state.zoneSettings,
        zoneEditorConfigs: state.zoneEditorConfigs,
        wallSettings: state.wallSettings,
        wallSegmentConfigs: state.wallSegmentConfigs,
        innerWallSegmentConfigs: state.innerWallSegmentConfigs
      });
      
      // Also save to database
      await useZoogiGame.getState().saveMapDecorations();
      
    } catch (err) {
      console.error("Error saving map decorations:", err);
    } finally {
      setIsSaving(false);
      onClose();
    }
  };
  
  useEffect(() => {
    if (!selectedMap || !deviceId) return;
    
    const storageKey = `map_decorations_${selectedMap}_${deviceId}`;
    const saved = localStorage.getItem(storageKey);
    
    if (saved) {
      try {
        const data = JSON.parse(saved);
        if (data.placedModels && Array.isArray(data.placedModels)) {
          const state = useZoogiGame.getState();
          state.loadEditorPlacedModels(data.placedModels);
          console.log("Loaded saved map decorations:", data.placedModels.length);
          
          const loadedConfig = data.wallOwnershipConfig || { enabled: false, wallBlocks: [], scoringZones: [] };
          if (data.wallOwnershipConfig) {
            state.loadWallOwnershipConfig(data.wallOwnershipConfig);
            console.log("Loaded wall ownership config:", data.wallOwnershipConfig);
          }
          
          if (data.zoneSettings) {
            state.setZoneSettings(data.zoneSettings);
            console.log("Loaded zone settings:", data.zoneSettings);
          }
          
          if (data.zoneEditorConfigs && Array.isArray(data.zoneEditorConfigs)) {
            data.zoneEditorConfigs.forEach((config: any) => {
              state.updateZoneConfig(config.id, {
                angle: config.angle,
                distance: config.distance,
                visible: config.visible,
                isSpawn: config.isSpawn
              });
            });
            console.log("Loaded zone editor configs:", data.zoneEditorConfigs.length);
          }
          
          if (data.wallSettings) {
            // Merge with defaults to ensure new properties (pbrMiddleWallRadiusOffset, etc.) have values
            const mergedWallSettings = { ...DEFAULT_WALL_SETTINGS, ...data.wallSettings };
            state.setWallSettings(mergedWallSettings);
            console.log("Loaded wall settings:", mergedWallSettings);
          }
          
          if (data.wallSegmentConfigs && Array.isArray(data.wallSegmentConfigs)) {
            state.setWallSegmentConfigs(data.wallSegmentConfigs);
            console.log("Loaded wall segment configs:", data.wallSegmentConfigs.length);
          }
          
          if (data.innerWallSegmentConfigs && Array.isArray(data.innerWallSegmentConfigs)) {
            state.setInnerWallSegmentConfigs(data.innerWallSegmentConfigs);
            console.log("Loaded inner wall segment configs:", data.innerWallSegmentConfigs.length);
          }
          
          lastSavedModelsRef.current = JSON.stringify({ 
            models: data.placedModels, 
            config: loadedConfig,
            zoneSettings: data.zoneSettings || {},
            zoneEditorConfigs: data.zoneEditorConfigs || [],
            wallSettings: data.wallSettings || {},
            wallSegmentConfigs: data.wallSegmentConfigs || [],
            innerWallSegmentConfigs: data.innerWallSegmentConfigs || []
          });
        }
      } catch (err) {
        console.error("Failed to parse saved map decorations:", err);
      }
    } else {
      lastSavedModelsRef.current = JSON.stringify({ models: [], config: { enabled: false, wallBlocks: [], scoringZones: [] }, zoneSettings: {}, zoneEditorConfigs: [] });
    }
  }, [selectedMap, deviceId]);

  useEffect(() => {
    if (!isOpen) {
      if (autoSaveIntervalRef.current) {
        clearInterval(autoSaveIntervalRef.current);
        autoSaveIntervalRef.current = null;
      }
      return;
    }
    
    const performAutoSave = async () => {
      const state = useZoogiGame.getState();
      const currentModels = state.editorPlacedModels;
      const currentConfig = state.wallOwnershipConfig;
      const currentZoneSettings = state.zoneSettings;
      const currentZoneConfigs = state.zoneEditorConfigs;
      const currentWallSettings = state.wallSettings;
      const currentWallSegmentConfigs = state.wallSegmentConfigs;
      const currentInnerWallSegmentConfigs = state.innerWallSegmentConfigs;
      const currentModelsJson = JSON.stringify({ 
        models: currentModels, 
        config: currentConfig,
        zoneSettings: currentZoneSettings,
        zoneEditorConfigs: currentZoneConfigs,
        wallSettings: currentWallSettings,
        wallSegmentConfigs: currentWallSegmentConfigs,
        innerWallSegmentConfigs: currentInnerWallSegmentConfigs
      });
      
      if (currentModelsJson === lastSavedModelsRef.current) return;
      if (!selectedMap || !deviceId) return;
      
      setIsAutoSaving(true);
      
      try {
        const storageKey = `map_decorations_${selectedMap}_${deviceId}`;
        const exportData = {
          mapId: selectedMap,
          deviceId,
          placedModels: currentModels,
          wallOwnershipConfig: currentConfig,
          zoneSettings: currentZoneSettings,
          zoneEditorConfigs: currentZoneConfigs,
          wallSettings: currentWallSettings,
          wallSegmentConfigs: currentWallSegmentConfigs,
          innerWallSegmentConfigs: currentInnerWallSegmentConfigs,
          timestamp: Date.now()
        };
        
        localStorage.setItem(storageKey, JSON.stringify(exportData));
        await state.saveMapDecorations();
        
        lastSavedModelsRef.current = currentModelsJson;
        toast.success("Auto-saved", { duration: 1500 });
      } catch (err) {
        console.error("Auto-save failed:", err);
      } finally {
        setIsAutoSaving(false);
      }
    };
    
    autoSaveIntervalRef.current = setInterval(performAutoSave, 30000);
    
    return () => {
      if (autoSaveIntervalRef.current) {
        clearInterval(autoSaveIntervalRef.current);
        autoSaveIntervalRef.current = null;
      }
    };
  }, [isOpen, selectedMap, deviceId]);

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          initial={{ x: 300, opacity: 0 }}
          animate={{ x: 0, opacity: 1 }}
          exit={{ x: 300, opacity: 0 }}
          className="absolute right-0 top-0 bottom-0 w-72 bg-black/90 backdrop-blur-sm z-50 flex flex-col pointer-events-auto"
          onClick={(e) => e.stopPropagation()}
        >
          <div className="flex items-center justify-center p-4 border-b border-white/20">
            <h2 className="text-white font-bold text-lg flex items-center gap-2">
              <Package size={20} className="text-purple-400" />
              My Arenas
            </h2>
          </div>

          <div className="flex border-b border-white/20">
            <button
              onClick={() => setActiveTab("models")}
              className={`flex-1 py-2 text-xs font-medium transition-colors ${
                activeTab === "models"
                  ? "text-purple-400 border-b-2 border-purple-400"
                  : "text-white/60 hover:text-white"
              }`}
            >
              Models
            </button>
            <button
              onClick={() => setActiveTab("placed")}
              className={`flex-1 py-2 text-xs font-medium transition-colors ${
                activeTab === "placed"
                  ? "text-purple-400 border-b-2 border-purple-400"
                  : "text-white/60 hover:text-white"
              }`}
            >
              Placed ({editorPlacedModels.length})
            </button>
            <button
              onClick={() => setActiveTab("walls")}
              className={`flex-1 py-2 text-xs font-medium transition-colors ${
                activeTab === "walls"
                  ? "text-orange-400 border-b-2 border-orange-400"
                  : "text-white/60 hover:text-white"
              }`}
            >
              Walls
            </button>
          </div>

          <div className="flex-1 overflow-y-auto p-4">
            {activeTab === "models" && (
              <div className="space-y-3">
                <p className="text-white/60 text-xs mb-2">
                  Tap a model to add it to the arena
                </p>
                {allModels.map((model) => (
                  <motion.button
                    key={model.id}
                    onClick={() => handlePlaceModel(model)}
                    whileHover={{ scale: 1.02 }}
                    whileTap={{ scale: 0.98 }}
                    className="w-full p-3 rounded-lg bg-white/10 hover:bg-white/20 transition-colors text-left"
                  >
                    <div className="flex items-center gap-3">
                      <ModelIcon name={model.name} type={model.type} />
                      <div className="flex-1">
                        <p className="text-white font-medium text-sm">{model.name}</p>
                        <p className="text-white/50 text-xs capitalize">{model.type}</p>
                      </div>
                      <Plus size={16} className="text-green-400" />
                    </div>
                  </motion.button>
                ))}
                
                {allModels.length === 0 && (
                  <p className="text-white/40 text-sm text-center py-8">
                    No models available
                  </p>
                )}
              </div>
            )}
            
            {activeTab === "placed" && (
              <div className="space-y-3">
                <p className="text-white/60 text-xs mb-2">
                  Models placed in this arena
                </p>
                {editorPlacedModels.map((model) => (
                  <div
                    key={model.id}
                    className={`p-3 rounded-lg flex items-center justify-between cursor-pointer transition-all ${
                      selectedEditorModelId === model.id
                        ? "bg-purple-600/40 ring-2 ring-purple-400"
                        : "bg-white/10 hover:bg-white/20"
                    }`}
                    onClick={() => selectEditorModel(model.id)}
                  >
                    <div className="flex items-center gap-3">
                      <ModelIcon name={model.name} type="default" />
                      <div>
                        <p className="text-white font-medium text-sm">{model.name}</p>
                        <p className="text-white/50 text-xs">
                          Pos: {(model.position || [0,0,0]).map(v => v.toFixed(1)).join(", ")}
                        </p>
                      </div>
                    </div>
                    <div className="flex gap-1">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          selectEditorModel(model.id);
                        }}
                        className="p-2 text-purple-400 hover:text-purple-300 hover:bg-purple-500/20 rounded-lg transition-colors"
                        title="Select and edit"
                      >
                        <Focus size={16} />
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          removeEditorPlacedModel(model.id);
                        }}
                        className="p-2 text-red-400 hover:text-red-300 hover:bg-red-500/20 rounded-lg transition-colors"
                        title="Delete"
                      >
                        <Trash2 size={16} />
                      </button>
                    </div>
                  </div>
                ))}
                
                {editorPlacedModels.length === 0 && (
                  <p className="text-white/40 text-sm text-center py-8">
                    No models placed yet
                  </p>
                )}
              </div>
            )}
            
            {activeTab === "walls" && (
              <div className="space-y-4">
                {/* Outer Wall Gap Size */}
                <div className="bg-purple-900/30 rounded-lg p-3 border border-purple-500/30">
                  <div className="flex items-center gap-2 mb-3">
                    <Sliders size={14} className="text-purple-400" />
                    <span className="text-white text-sm font-medium">Outer Wall Gap Size</span>
                  </div>
                  <div className="flex items-center gap-3">
                    <input
                      type="range"
                      min="0.05"
                      max="2.5"
                      step="0.05"
                      value={wallSettings.outerWallGapWidth}
                      onChange={(e) => setWallSettings({ outerWallGapWidth: parseFloat(e.target.value) })}
                      className="flex-1 accent-purple-400"
                    />
                    <span className="text-purple-300 text-xs w-12">
                      {(wallSettings.outerWallGapWidth * 180 / Math.PI).toFixed(0)}°
                    </span>
                  </div>
                </div>

                {/* Middle Wall Gap Size */}
                <div className="bg-cyan-900/30 rounded-lg p-3 border border-cyan-500/30">
                  <div className="flex items-center gap-2 mb-3">
                    <Sliders size={14} className="text-cyan-400" />
                    <span className="text-white text-sm font-medium">Middle Wall Gap Size</span>
                  </div>
                  <div className="flex items-center gap-3">
                    <input
                      type="range"
                      min="0.05"
                      max="2.5"
                      step="0.05"
                      value={wallSettings.middleWallGapWidth}
                      onChange={(e) => setWallSettings({ middleWallGapWidth: parseFloat(e.target.value) })}
                      className="flex-1 accent-cyan-400"
                    />
                    <span className="text-cyan-300 text-xs w-12">
                      {(wallSettings.middleWallGapWidth * 180 / Math.PI).toFixed(0)}°
                    </span>
                  </div>
                </div>

                {/* Wall Segments */}
                <div className="bg-cyan-900/30 rounded-lg p-3 border border-cyan-500/30">
                  <div className="flex items-center gap-2 mb-3">
                    <Move size={14} className="text-cyan-400" />
                    <span className="text-white text-sm font-medium">Wall Segments ({wallSegmentConfigs.length})</span>
                  </div>
                  <p className="text-white/50 text-xs mb-3">
                    Click on a wall segment in the 3D view to select it. Use the gizmo to move it.
                  </p>
                  <div className="space-y-2 max-h-48 overflow-y-auto">
                    {wallSegmentConfigs.map((config, index) => (
                      <div
                        key={config.id}
                        className={`p-2 rounded-lg cursor-pointer transition-all flex items-center justify-between ${
                          selectedWallSegmentId === config.id
                            ? "bg-yellow-500/40 ring-1 ring-yellow-400"
                            : "bg-white/10 hover:bg-white/20"
                        }`}
                        onClick={() => selectWallSegment(config.id)}
                      >
                        <span className="text-white text-xs">Segment {index + 1}</span>
                        <div className="flex items-center gap-2">
                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              toggleWallSegmentVisibility(config.id);
                            }}
                            className={`p-1 rounded transition-colors ${
                              config.visible 
                                ? "text-cyan-400 hover:bg-cyan-500/20" 
                                : "text-red-400 hover:bg-red-500/20"
                            }`}
                            title={config.visible ? "Hide segment" : "Show segment"}
                          >
                            {config.visible ? <Eye size={14} /> : <EyeOff size={14} />}
                          </button>
                          {selectedWallSegmentId === config.id && config.positionOffset && (
                            <span className="text-white/50 text-xs">
                              [{config.positionOffset.x.toFixed(1)}, {config.positionOffset.y.toFixed(1)}, {config.positionOffset.z.toFixed(1)}]
                            </span>
                          )}
                        </div>
                      </div>
                    ))}
                  </div>
                  {selectedWallSegmentId && (
                    <div className="mt-3 pt-3 border-t border-white/10 space-y-2">
                      <button
                        onClick={() => toggleWallSegmentVisibility(selectedWallSegmentId)}
                        className={`w-full py-2 text-xs rounded-lg transition-colors flex items-center justify-center gap-2 ${
                          wallSegmentConfigs.find(c => c.id === selectedWallSegmentId)?.visible
                            ? "bg-red-500/20 hover:bg-red-500/40 text-red-300"
                            : "bg-green-500/20 hover:bg-green-500/40 text-green-300"
                        }`}
                      >
                        {wallSegmentConfigs.find(c => c.id === selectedWallSegmentId)?.visible ? (
                          <><EyeOff size={14} /> Hide Selected</>
                        ) : (
                          <><Eye size={14} /> Show Selected</>
                        )}
                      </button>
                      <button
                        onClick={() => {
                          updateWallSegmentConfig(selectedWallSegmentId, { positionOffset: { x: 0, y: 0, z: 0 } });
                        }}
                        className="w-full py-2 text-xs bg-cyan-500/20 hover:bg-cyan-500/40 text-cyan-300 rounded-lg transition-colors"
                      >
                        Reset Selected Position
                      </button>
                    </div>
                  )}
                </div>

                {/* Inner Wall Segments */}
                <div className="bg-purple-900/30 rounded-lg p-3 border border-purple-500/30">
                  <div className="flex items-center gap-2 mb-3">
                    <Move size={14} className="text-purple-400" />
                    <span className="text-white text-sm font-medium">Inner Wall Segments ({innerWallSegmentConfigs.length})</span>
                  </div>
                  <p className="text-white/50 text-xs mb-3">
                    Click on an inner wall in the 3D view to select it. Use the gizmo to move it.
                  </p>
                  <div className="space-y-2 max-h-48 overflow-y-auto">
                    {innerWallSegmentConfigs.map((config, index) => (
                      <div
                        key={config.id}
                        className={`p-2 rounded-lg cursor-pointer transition-all flex items-center justify-between ${
                          selectedInnerWallSegmentId === config.id
                            ? "bg-yellow-500/40 ring-1 ring-yellow-400"
                            : "bg-white/10 hover:bg-white/20"
                        }`}
                        onClick={() => selectInnerWallSegment(config.id)}
                      >
                        <span className="text-white text-xs">Inner {index + 1}</span>
                        <div className="flex items-center gap-2">
                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              toggleInnerWallSegmentVisibility(config.id);
                            }}
                            className={`p-1 rounded transition-colors ${
                              config.visible 
                                ? "text-purple-400 hover:bg-purple-500/20" 
                                : "text-red-400 hover:bg-red-500/20"
                            }`}
                            title={config.visible ? "Hide segment" : "Show segment"}
                          >
                            {config.visible ? <Eye size={14} /> : <EyeOff size={14} />}
                          </button>
                          {selectedInnerWallSegmentId === config.id && config.positionOffset && (
                            <span className="text-white/50 text-xs">
                              [{config.positionOffset.x.toFixed(1)}, {config.positionOffset.y.toFixed(1)}, {config.positionOffset.z.toFixed(1)}]
                            </span>
                          )}
                        </div>
                      </div>
                    ))}
                  </div>
                  {selectedInnerWallSegmentId && (
                    <div className="mt-3 pt-3 border-t border-white/10 space-y-2">
                      <button
                        onClick={() => toggleInnerWallSegmentVisibility(selectedInnerWallSegmentId)}
                        className={`w-full py-2 text-xs rounded-lg transition-colors flex items-center justify-center gap-2 ${
                          innerWallSegmentConfigs.find(c => c.id === selectedInnerWallSegmentId)?.visible
                            ? "bg-red-500/20 hover:bg-red-500/40 text-red-300"
                            : "bg-green-500/20 hover:bg-green-500/40 text-green-300"
                        }`}
                      >
                        {innerWallSegmentConfigs.find(c => c.id === selectedInnerWallSegmentId)?.visible ? (
                          <><EyeOff size={14} /> Hide Selected</>
                        ) : (
                          <><Eye size={14} /> Show Selected</>
                        )}
                      </button>
                      <button
                        onClick={() => {
                          updateInnerWallSegmentConfig(selectedInnerWallSegmentId, { positionOffset: { x: 0, y: 0, z: 0 } });
                        }}
                        className="w-full py-2 text-xs bg-purple-500/20 hover:bg-purple-500/40 text-purple-300 rounded-lg transition-colors"
                      >
                        Reset Selected Position
                      </button>
                    </div>
                  )}
                </div>


                <div className="border-t border-white/10 pt-4">
                  <div className="flex items-center justify-between">
                    <span className="text-white text-sm font-medium">Ringer Royale Mode</span>
                    <button
                      onClick={toggleWallOwnershipEditor}
                      className={`px-3 py-1 rounded-full text-xs font-medium transition-colors ${
                        wallOwnershipConfig.enabled
                          ? "bg-orange-500 text-white"
                          : "bg-white/20 text-white/60"
                      }`}
                    >
                      {wallOwnershipConfig.enabled ? "ON" : "OFF"}
                    </button>
                  </div>
                </div>
                
                {wallOwnershipConfig.enabled && (
                  <>
                    <div className="border-t border-white/10 pt-4">
                      <div className="flex items-center justify-between mb-2">
                        <span className="text-white/80 text-sm flex items-center gap-2">
                          <Shield size={14} className="text-orange-400" />
                          Wall Blocks ({wallOwnershipConfig.wallBlocks.length})
                        </span>
                        <button
                          onClick={() => addEditorWallBlock({
                            position: [0, 1, -15],
                            rotation: 0,
                            dimensions: { length: 2, width: 1, height: 2 }
                          })}
                          className="p-1 bg-orange-500/20 hover:bg-orange-500/40 rounded text-orange-400"
                        >
                          <Plus size={14} />
                        </button>
                      </div>
                      
                      {wallOwnershipConfig.wallBlocks.map((block) => (
                        <div
                          key={block.id}
                          className={`p-2 rounded-lg mb-2 cursor-pointer transition-all ${
                            selectedEditorWallBlockId === block.id
                              ? "bg-orange-600/40 ring-1 ring-orange-400"
                              : "bg-white/10 hover:bg-white/20"
                          }`}
                          onClick={() => selectEditorWallBlock(block.id)}
                        >
                          <div className="flex items-center justify-between mb-2">
                            <span className="text-white text-xs font-medium">Block</span>
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                removeEditorWallBlock(block.id);
                              }}
                              className="p-1 text-red-400 hover:bg-red-500/20 rounded"
                            >
                              <Trash2 size={12} />
                            </button>
                          </div>
                          
                          {selectedEditorWallBlockId === block.id && (
                            <div className="space-y-2 text-xs">
                              <div>
                                <label className="text-white/60">Dimensions (L×W×H)</label>
                                <div className="flex gap-1 mt-1">
                                  <input
                                    type="number"
                                    value={block.dimensions.length}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      dimensions: { ...block.dimensions, length: parseFloat(e.target.value) || 1 }
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="0.5"
                                  />
                                  <input
                                    type="number"
                                    value={block.dimensions.width}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      dimensions: { ...block.dimensions, width: parseFloat(e.target.value) || 1 }
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="0.5"
                                  />
                                  <input
                                    type="number"
                                    value={block.dimensions.height}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      dimensions: { ...block.dimensions, height: parseFloat(e.target.value) || 1 }
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="0.5"
                                  />
                                </div>
                              </div>
                              <div>
                                <label className="text-white/60">Position (X, Y, Z)</label>
                                <div className="flex gap-1 mt-1">
                                  <input
                                    type="number"
                                    value={block.position[0]}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      position: [parseFloat(e.target.value) || 0, block.position[1], block.position[2]]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="1"
                                  />
                                  <input
                                    type="number"
                                    value={block.position[1]}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      position: [block.position[0], parseFloat(e.target.value) || 0, block.position[2]]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="0.5"
                                  />
                                  <input
                                    type="number"
                                    value={block.position[2]}
                                    onChange={(e) => updateEditorWallBlock(block.id, {
                                      position: [block.position[0], block.position[1], parseFloat(e.target.value) || 0]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="1"
                                  />
                                </div>
                              </div>
                              <div>
                                <label className="text-white/60 flex items-center gap-1">
                                  <RotateCw size={10} /> Rotation (°)
                                </label>
                                <input
                                  type="range"
                                  min="0"
                                  max="360"
                                  value={(block.rotation * 180 / Math.PI)}
                                  onChange={(e) => updateEditorWallBlock(block.id, {
                                    rotation: parseFloat(e.target.value) * Math.PI / 180
                                  })}
                                  className="w-full mt-1"
                                />
                              </div>
                            </div>
                          )}
                        </div>
                      ))}
                    </div>
                    
                    <div className="border-t border-white/10 pt-4">
                      <div className="flex items-center justify-between mb-2">
                        <span className="text-white/80 text-sm flex items-center gap-2">
                          <Target size={14} className="text-cyan-400" />
                          Scoring Zones ({wallOwnershipConfig.scoringZones.length})
                        </span>
                        <button
                          onClick={() => addEditorScoringZone({
                            position: [0, 0.1, 0],
                            rotation: 0,
                            radius: 3
                          })}
                          className="p-1 bg-cyan-500/20 hover:bg-cyan-500/40 rounded text-cyan-400"
                        >
                          <Plus size={14} />
                        </button>
                      </div>
                      
                      {wallOwnershipConfig.scoringZones.map((zone) => (
                        <div
                          key={zone.id}
                          className={`p-2 rounded-lg mb-2 cursor-pointer transition-all ${
                            selectedEditorScoringZoneId === zone.id
                              ? "bg-cyan-600/40 ring-1 ring-cyan-400"
                              : "bg-white/10 hover:bg-white/20"
                          }`}
                          onClick={() => selectEditorScoringZone(zone.id)}
                        >
                          <div className="flex items-center justify-between mb-2">
                            <span className="text-white text-xs font-medium">Zone</span>
                            <button
                              onClick={(e) => {
                                e.stopPropagation();
                                removeEditorScoringZone(zone.id);
                              }}
                              className="p-1 text-red-400 hover:bg-red-500/20 rounded"
                            >
                              <Trash2 size={12} />
                            </button>
                          </div>
                          
                          {selectedEditorScoringZoneId === zone.id && (
                            <div className="space-y-2 text-xs">
                              <div>
                                <label className="text-white/60">Radius</label>
                                <input
                                  type="range"
                                  min="1"
                                  max="10"
                                  step="0.5"
                                  value={zone.radius}
                                  onChange={(e) => updateEditorScoringZone(zone.id, {
                                    radius: parseFloat(e.target.value)
                                  })}
                                  className="w-full mt-1"
                                />
                                <span className="text-white/40">{zone.radius}</span>
                              </div>
                              <div>
                                <label className="text-white/60">Position (X, Y, Z)</label>
                                <div className="flex gap-1 mt-1">
                                  <input
                                    type="number"
                                    value={zone.position[0]}
                                    onChange={(e) => updateEditorScoringZone(zone.id, {
                                      position: [parseFloat(e.target.value) || 0, zone.position[1], zone.position[2]]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="1"
                                  />
                                  <input
                                    type="number"
                                    value={zone.position[1]}
                                    onChange={(e) => updateEditorScoringZone(zone.id, {
                                      position: [zone.position[0], parseFloat(e.target.value) || 0, zone.position[2]]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="0.1"
                                  />
                                  <input
                                    type="number"
                                    value={zone.position[2]}
                                    onChange={(e) => updateEditorScoringZone(zone.id, {
                                      position: [zone.position[0], zone.position[1], parseFloat(e.target.value) || 0]
                                    })}
                                    className="w-12 px-1 py-0.5 bg-black/50 rounded text-white text-center"
                                    step="1"
                                  />
                                </div>
                              </div>
                              <div>
                                <label className="text-white/60 flex items-center gap-1">
                                  <RotateCw size={10} /> Rotation (°)
                                </label>
                                <input
                                  type="range"
                                  min="0"
                                  max="360"
                                  value={(zone.rotation * 180 / Math.PI)}
                                  onChange={(e) => updateEditorScoringZone(zone.id, {
                                    rotation: parseFloat(e.target.value) * Math.PI / 180
                                  })}
                                  className="w-full mt-1"
                                />
                              </div>
                            </div>
                          )}
                        </div>
                      ))}
                    </div>
                  </>
                )}
              </div>
            )}
          </div>

          <div className="p-4 border-t border-white/20 space-y-2">
            <p className="text-white/60 text-xs mb-2">
              Current map: <span className="text-purple-400">{selectedMap}</span>
            </p>
            <button
              onClick={handleSaveAndClose}
              disabled={isSaving}
              className={`w-full py-2 rounded-lg font-medium text-sm flex items-center justify-center gap-2 transition-colors ${
                !isSaving
                  ? "bg-green-500 hover:bg-green-600 text-white"
                  : "bg-gray-600 text-gray-400 cursor-not-allowed"
              }`}
            >
              <Save size={16} />
              {isSaving ? "Saving..." : "Save & Close"}
            </button>
          </div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
