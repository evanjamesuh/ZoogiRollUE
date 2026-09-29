import { motion, AnimatePresence } from "framer-motion";
import { useZoogiGame, MAP_OPTIONS, MapTheme } from "@/lib/stores/useZoogiGame";
import { useState, useEffect, useMemo, useRef, useCallback } from "react";
import { Star, Sparkles, MapPin, Settings, X, Image, Video, AlertCircle } from "lucide-react";
import type { CustomArena } from "./ArenaEditor";
import { getDeviceId } from "@/lib/deviceId";

const SETTINGS_STORAGE_KEY = "map_selection_dev_settings";
const IDB_NAME = "MapSelectionAssets";
const IDB_STORE = "assets";

interface DevSettings {
  buttonHeight: number;
  buttonRadius: number;
  buttonGap: number;
}

interface AssetData {
  backgrounds: Record<MapTheme, string | null>;
  previewVideos: Record<MapTheme, string | null>;
}

const DEFAULT_DEV_SETTINGS: DevSettings = {
  buttonHeight: 80,
  buttonRadius: 12,
  buttonGap: 12,
};

const DEFAULT_ASSETS: AssetData = {
  backgrounds: {
    grass: "/textures/meadow_background.png",
    ice: null,
    lava: null,
    space: null,
    saturn: null,
    neon: null,
  },
  previewVideos: {
    grass: "/videos/meadow_preview.mp4",
    ice: null,
    lava: null,
    space: null,
    saturn: null,
    neon: null,
  },
};

function openDB(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(IDB_NAME, 1);
    request.onerror = () => reject(request.error);
    request.onsuccess = () => resolve(request.result);
    request.onupgradeneeded = (event) => {
      const db = (event.target as IDBOpenDBRequest).result;
      if (!db.objectStoreNames.contains(IDB_STORE)) {
        db.createObjectStore(IDB_STORE, { keyPath: "id" });
      }
    };
  });
}

async function saveAssetsToDB(assets: AssetData): Promise<void> {
  const db = await openDB();
  const tx = db.transaction(IDB_STORE, "readwrite");
  const store = tx.objectStore(IDB_STORE);
  store.put({ id: "mapAssets", ...assets });
  return new Promise((resolve, reject) => {
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}

async function loadAssetsFromDB(): Promise<{ assets: AssetData; available: boolean }> {
  try {
    const db = await openDB();
    const tx = db.transaction(IDB_STORE, "readonly");
    const store = tx.objectStore(IDB_STORE);
    const request = store.get("mapAssets");
    return new Promise((resolve) => {
      request.onsuccess = () => {
        if (request.result) {
          const { id, ...assets } = request.result;
          resolve({ assets: { ...DEFAULT_ASSETS, ...assets }, available: true });
        } else {
          resolve({ assets: DEFAULT_ASSETS, available: true });
        }
      };
      request.onerror = () => resolve({ assets: DEFAULT_ASSETS, available: true });
    });
  } catch {
    return { assets: DEFAULT_ASSETS, available: false };
  }
}

function loadDevSettings(): DevSettings {
  try {
    const stored = localStorage.getItem(SETTINGS_STORAGE_KEY);
    if (stored) {
      const parsed = JSON.parse(stored);
      return { ...DEFAULT_DEV_SETTINGS, ...parsed };
    }
  } catch {}
  return DEFAULT_DEV_SETTINGS;
}

function saveDevSettings(settings: DevSettings) {
  try {
    localStorage.setItem(SETTINGS_STORAGE_KEY, JSON.stringify(settings));
  } catch {}
}

interface MeshyArena {
  id: number;
  name: string;
  description?: string;
  meshyTaskId: string;
  thumbnailUrl?: string;
  createdAt: string;
}

function getLocalStorageArenas(): CustomArena[] {
  try {
    const stored = localStorage.getItem("custom_arenas");
    return stored ? JSON.parse(stored) : [];
  } catch {
    return [];
  }
}

export function MapSelection() {
  const { selectMap, setPhase, selectedMap, gameMode } = useZoogiGame();
  const [customArenas, setCustomArenas] = useState<CustomArena[]>([]);
  const [meshyArenas, setMeshyArenas] = useState<MeshyArena[]>([]);
  const [selectedCustomArena, setSelectedCustomArena] = useState<string | null>(null);
  const [selectedMeshyArena, setSelectedMeshyArena] = useState<number | null>(null);
  
  const [showDevControls, setShowDevControls] = useState(false);
  const [devSettings, setDevSettings] = useState<DevSettings>(loadDevSettings);
  const [assets, setAssets] = useState<AssetData>(DEFAULT_ASSETS);
  const [assetsLoaded, setAssetsLoaded] = useState(false);
  const [storageAvailable, setStorageAvailable] = useState(true);
  const [editingMap, setEditingMap] = useState<MapTheme | null>(null);
  const [uploadError, setUploadError] = useState<string | null>(null);
  
  const imageInputRef = useRef<HTMLInputElement>(null);
  const videoInputRef = useRef<HTMLInputElement>(null);
  
  const deviceId = useMemo(() => getDeviceId(), []);

  useEffect(() => {
    if (!window.indexedDB) {
      setStorageAvailable(false);
      setAssetsLoaded(true);
      return;
    }
    
    loadAssetsFromDB()
      .then(({ assets: loadedAssets, available }) => {
        setAssets(loadedAssets);
        setStorageAvailable(available);
        setAssetsLoaded(true);
      });
  }, []);
  
  useEffect(() => {
    setCustomArenas(getLocalStorageArenas());
    
    fetch(`/api/custom-arenas?deviceId=${deviceId}`)
      .then(res => res.json())
      .then(data => {
        if (data.data) {
          setMeshyArenas(data.data);
        }
      })
      .catch(err => console.error("Failed to fetch custom arenas:", err));
  }, [deviceId]);

  useEffect(() => {
    saveDevSettings(devSettings);
  }, [devSettings]);

  const saveAssets = useCallback(async (newAssets: AssetData): Promise<boolean> => {
    try {
      await saveAssetsToDB(newAssets);
      setAssets(newAssets);
      setUploadError(null);
      return true;
    } catch (err) {
      console.error("Failed to save assets:", err);
      setUploadError("Failed to save asset. Storage may be unavailable or full.");
      return false;
    }
  }, []);

  const handleSelectMap = (mapId: MapTheme, customId?: string) => {
    if (customId) {
      const customArena = customArenas.find(a => a.id === customId);
      if (customArena) {
        selectMap(mapId, customId, customArena.decorations);
      } else {
        selectMap(mapId);
      }
    } else {
      selectMap(mapId);
    }
    setSelectedCustomArena(customId || null);
    setSelectedMeshyArena(null);
  };

  const handleSelectMeshyArena = (arena: MeshyArena) => {
    const modelUrl = `/api/meshy/download/${arena.meshyTaskId}`;
    selectMap("grass", undefined, undefined, arena.id, modelUrl);
    setSelectedMeshyArena(arena.id);
    setSelectedCustomArena(null);
  };

  const handleStartGame = () => {
    if (gameMode === "map_editor") {
      useZoogiGame.getState().startMapEditor();
    } else if (gameMode === "local_multiplayer") {
      useZoogiGame.getState().startLocalGame();
    } else {
      useZoogiGame.getState().startGame();
    }
  };
  
  const handleBack = () => {
    if (gameMode === "map_editor") {
      setPhase("menu");
    } else if (gameMode === "local_multiplayer") {
      setPhase("local_setup");
    } else {
      setPhase("character_selection");
    }
  };

  const handleImageUpload = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file || !editingMap) return;
    
    if (file.size > 10 * 1024 * 1024) {
      setUploadError("Image too large. Max 10MB.");
      e.target.value = "";
      return;
    }
    
    const reader = new FileReader();
    reader.onload = async (event) => {
      const dataUrl = event.target?.result as string;
      const newAssets = {
        ...assets,
        backgrounds: { ...assets.backgrounds, [editingMap]: dataUrl }
      };
      await saveAssets(newAssets);
      setEditingMap(null);
    };
    reader.onerror = () => {
      setUploadError("Failed to read image file.");
    };
    reader.readAsDataURL(file);
    e.target.value = "";
  };

  const handleVideoUpload = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file || !editingMap) return;
    
    if (file.size > 50 * 1024 * 1024) {
      setUploadError("Video too large. Max 50MB.");
      e.target.value = "";
      return;
    }
    
    const reader = new FileReader();
    reader.onload = async (event) => {
      const dataUrl = event.target?.result as string;
      const newAssets = {
        ...assets,
        previewVideos: { ...assets.previewVideos, [editingMap]: dataUrl }
      };
      await saveAssets(newAssets);
      setEditingMap(null);
    };
    reader.onerror = () => {
      setUploadError("Failed to read video file.");
    };
    reader.readAsDataURL(file);
    e.target.value = "";
  };

  const openImageUpload = (mapId: MapTheme) => {
    setEditingMap(mapId);
    setUploadError(null);
    imageInputRef.current?.click();
  };

  const openVideoUpload = (mapId: MapTheme) => {
    setEditingMap(mapId);
    setUploadError(null);
    videoInputRef.current?.click();
  };

  const clearBackground = async (mapId: MapTheme) => {
    const newAssets = {
      ...assets,
      backgrounds: { ...assets.backgrounds, [mapId]: null }
    };
    await saveAssets(newAssets);
  };

  const clearVideo = async (mapId: MapTheme) => {
    const newAssets = {
      ...assets,
      previewVideos: { ...assets.previewVideos, [mapId]: null }
    };
    await saveAssets(newAssets);
  };

  const resetToDefaults = async () => {
    setDevSettings(DEFAULT_DEV_SETTINGS);
    await saveAssets(DEFAULT_ASSETS);
  };

  const selectedMapInfo = MAP_OPTIONS.find(m => m.id === selectedMap);
  const selectedCustomInfo = selectedCustomArena ? customArenas.find(a => a.id === selectedCustomArena) : null;

  if (!assetsLoaded) {
    return (
      <div className="absolute inset-0 flex items-center justify-center">
        <div className="text-white">Loading...</div>
      </div>
    );
  }

  return (
    <div className="allow-pan-y absolute inset-0 flex flex-col items-center justify-start sm:justify-center p-4 sm:p-6 overflow-y-auto">
      <input
        ref={imageInputRef}
        type="file"
        accept="image/*"
        className="hidden"
        onChange={handleImageUpload}
      />
      <input
        ref={videoInputRef}
        type="file"
        accept="video/*"
        className="hidden"
        onChange={handleVideoUpload}
      />

      <button
        onClick={() => setShowDevControls(!showDevControls)}
        className="absolute top-4 right-4 z-50 p-2 bg-black/50 rounded-full text-white/70 hover:text-white transition-colors"
      >
        <Settings size={20} />
      </button>

      <AnimatePresence>
        {showDevControls && (
          <motion.div
            initial={{ opacity: 0, x: 300 }}
            animate={{ opacity: 1, x: 0 }}
            exit={{ opacity: 0, x: 300 }}
            className="absolute top-14 right-4 z-50 bg-black/90 backdrop-blur-sm rounded-xl p-4 w-72 max-h-[80vh] overflow-y-auto"
          >
            <h3 className="text-white font-bold mb-4 flex items-center gap-2">
              <Settings size={16} />
              Developer Controls
            </h3>

            {!storageAvailable && (
              <div className="mb-4 p-2 bg-yellow-600/30 border border-yellow-500 rounded-lg flex items-center gap-2 text-yellow-300 text-xs">
                <AlertCircle size={14} />
                Storage unavailable. Uploads won't persist after reload.
              </div>
            )}

            {uploadError && (
              <div className="mb-4 p-2 bg-red-600/30 border border-red-500 rounded-lg flex items-center gap-2 text-red-300 text-xs">
                <AlertCircle size={14} />
                {uploadError}
              </div>
            )}

            <div className="space-y-4">
              <div>
                <label className="text-white/70 text-xs uppercase tracking-wider">Button Height</label>
                <input
                  type="range"
                  min="50"
                  max="120"
                  value={devSettings.buttonHeight}
                  onChange={(e) => setDevSettings(prev => ({ ...prev, buttonHeight: parseInt(e.target.value) }))}
                  className="w-full mt-1"
                />
                <span className="text-white/50 text-xs">{devSettings.buttonHeight}px</span>
              </div>

              <div>
                <label className="text-white/70 text-xs uppercase tracking-wider">Border Radius</label>
                <input
                  type="range"
                  min="0"
                  max="30"
                  value={devSettings.buttonRadius}
                  onChange={(e) => setDevSettings(prev => ({ ...prev, buttonRadius: parseInt(e.target.value) }))}
                  className="w-full mt-1"
                />
                <span className="text-white/50 text-xs">{devSettings.buttonRadius}px</span>
              </div>

              <div>
                <label className="text-white/70 text-xs uppercase tracking-wider">Button Gap</label>
                <input
                  type="range"
                  min="4"
                  max="24"
                  value={devSettings.buttonGap}
                  onChange={(e) => setDevSettings(prev => ({ ...prev, buttonGap: parseInt(e.target.value) }))}
                  className="w-full mt-1"
                />
                <span className="text-white/50 text-xs">{devSettings.buttonGap}px</span>
              </div>

              <div className="border-t border-white/20 pt-4">
                <label className="text-white/70 text-xs uppercase tracking-wider mb-2 block">Map Assets</label>
                {MAP_OPTIONS.map((map) => (
                  <div key={map.id} className="mb-3 p-2 bg-white/5 rounded-lg">
                    <p className="text-white text-sm font-semibold mb-2">{map.name}</p>
                    
                    <div className="flex gap-2 mb-2">
                      <button
                        onClick={() => openImageUpload(map.id)}
                        className="flex-1 flex items-center justify-center gap-1 px-2 py-1.5 bg-blue-600 hover:bg-blue-500 rounded text-white text-xs transition-colors"
                      >
                        <Image size={12} />
                        {assets.backgrounds[map.id] ? "Change BG" : "Add BG"}
                      </button>
                      {assets.backgrounds[map.id] && (
                        <button
                          onClick={() => clearBackground(map.id)}
                          className="p-1.5 bg-red-600 hover:bg-red-500 rounded text-white transition-colors"
                        >
                          <X size={12} />
                        </button>
                      )}
                    </div>

                    <div className="flex gap-2">
                      <button
                        onClick={() => openVideoUpload(map.id)}
                        className="flex-1 flex items-center justify-center gap-1 px-2 py-1.5 bg-purple-600 hover:bg-purple-500 rounded text-white text-xs transition-colors"
                      >
                        <Video size={12} />
                        {assets.previewVideos[map.id] ? "Change Video" : "Add Video"}
                      </button>
                      {assets.previewVideos[map.id] && (
                        <button
                          onClick={() => clearVideo(map.id)}
                          className="p-1.5 bg-red-600 hover:bg-red-500 rounded text-white transition-colors"
                        >
                          <X size={12} />
                        </button>
                      )}
                    </div>

                    {(assets.backgrounds[map.id] || assets.previewVideos[map.id]) && (
                      <div className="mt-2 flex gap-1">
                        {assets.backgrounds[map.id] && (
                          <span className="text-green-400 text-xs flex items-center gap-1">
                            <Image size={10} /> BG set
                          </span>
                        )}
                        {assets.previewVideos[map.id] && (
                          <span className="text-purple-400 text-xs flex items-center gap-1">
                            <Video size={10} /> Video set
                          </span>
                        )}
                      </div>
                    )}
                  </div>
                ))}
              </div>

              <button
                onClick={resetToDefaults}
                className="w-full py-2 bg-red-600/50 hover:bg-red-600 rounded-lg text-white text-sm transition-colors"
              >
                Reset to Defaults
              </button>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      <motion.h1
        initial={{ y: -50, opacity: 0 }}
        animate={{ y: 0, opacity: 1 }}
        className="text-2xl sm:text-3xl font-bold text-white mb-1 mt-2 relative z-10"
      >
        Select Arena
      </motion.h1>
      
      <motion.p
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        transition={{ delay: 0.2 }}
        className="text-gray-300 text-sm mb-4 relative z-10"
      >
        Choose your battlefield
      </motion.p>

      <motion.div 
        initial={{ scale: 0.8, opacity: 0 }}
        animate={{ scale: 1, opacity: 1 }}
        transition={{ delay: 0.3 }}
        className="w-full max-w-xs h-24 landscape:h-16 sm:h-40 rounded-2xl overflow-hidden mb-3 bg-black/40 relative z-10"
        style={{
          boxShadow: selectedMapInfo ? `0 0 40px ${selectedMapInfo.color}40` : 'none'
        }}
      >
        <AnimatePresence mode="wait">
          {selectedMap && (
            <motion.div
              key={selectedMap}
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
              transition={{ duration: 0.3 }}
              className="w-full h-full"
            >
              {assets.previewVideos[selectedMap] ? (
                <video
                  src={assets.previewVideos[selectedMap]!}
                  autoPlay
                  loop
                  muted
                  playsInline
                  className="w-full h-full object-cover"
                />
              ) : assets.backgrounds[selectedMap] ? (
                <div 
                  className="w-full h-full bg-cover bg-center"
                  style={{ backgroundImage: `url(${assets.backgrounds[selectedMap]})` }}
                />
              ) : (
                <div 
                  className="w-full h-full flex items-center justify-center"
                  style={{ backgroundColor: selectedMapInfo?.color || '#333' }}
                >
                  <span className="text-white/50 text-sm">No preview available</span>
                </div>
              )}
            </motion.div>
          )}
        </AnimatePresence>
        {!selectedMap && (
          <div className="absolute inset-0 flex items-center justify-center text-white/50 text-sm">
            Tap an arena to preview
          </div>
        )}
        {(selectedMapInfo || selectedCustomInfo) && (
          <div className="absolute bottom-2 left-0 right-0 text-center">
            <span className="text-white font-bold text-lg drop-shadow-lg">
              {selectedCustomInfo ? (
                <span className="flex items-center justify-center gap-1">
                  <Star size={16} className="text-yellow-400" />
                  {selectedCustomInfo.name}
                </span>
              ) : selectedMapInfo?.name}
            </span>
          </div>
        )}
      </motion.div>

      <div 
        className="flex flex-col max-w-xs w-full relative z-10 pr-1"
        style={{ gap: `${devSettings.buttonGap}px` }}
      >
        {MAP_OPTIONS.map((map, index) => {
          const backgroundImage = assets.backgrounds[map.id];
          return (
            <motion.button
              key={map.id}
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              transition={{ delay: 0.1 * index }}
              onClick={() => handleSelectMap(map.id)}
              className={`relative transition-all duration-300 overflow-hidden ${
                selectedMap === map.id && !selectedCustomArena
                  ? "ring-3 ring-yellow-400 scale-[1.02]"
                  : "hover:scale-[1.01]"
              }`}
              style={{
                height: `${Math.min(devSettings.buttonHeight, Math.min(window.innerWidth, window.innerHeight) < 500 ? 56 : devSettings.buttonHeight)}px`,
                borderRadius: `${devSettings.buttonRadius}px`,
                backgroundColor: backgroundImage ? undefined : map.color,
                boxShadow: selectedMap === map.id && !selectedCustomArena
                  ? `0 0 20px ${map.color}` 
                  : `0 4px 10px rgba(0,0,0,0.3)`
              }}
            >
              {backgroundImage && (
                <div 
                  className="absolute inset-0 bg-cover bg-center"
                  style={{ 
                    backgroundImage: `url(${backgroundImage})`,
                    borderRadius: `${devSettings.buttonRadius}px`
                  }}
                />
              )}
              
              <div 
                className="absolute inset-0 bg-gradient-to-r from-black/60 via-black/30 to-transparent"
                style={{ borderRadius: `${devSettings.buttonRadius}px` }}
              />
              
              <div className="relative z-10 h-full flex flex-col justify-center p-4 text-white text-left">
                <h3 className="text-base font-bold drop-shadow-lg">{map.name}</h3>
                <p className="text-xs opacity-90 drop-shadow">{map.description}</p>
              </div>
              
              {!backgroundImage && (
                <div 
                  className="absolute inset-0 opacity-20 pointer-events-none"
                  style={{
                    background: `radial-gradient(circle at 30% 30%, white, transparent)`,
                    borderRadius: `${devSettings.buttonRadius}px`
                  }}
                />
              )}
              
              {selectedMap === map.id && !selectedCustomArena && (
                <motion.div
                  initial={{ scale: 0 }}
                  animate={{ scale: 1 }}
                  className="absolute top-2 right-2 w-6 h-6 bg-yellow-400 rounded-full flex items-center justify-center z-20"
                >
                  <span className="text-black font-bold text-xs">✓</span>
                </motion.div>
              )}
            </motion.button>
          );
        })}
      </div>
      
      {customArenas.length > 0 && (
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.5 }}
          className="mt-4 w-full max-w-xs relative z-10"
        >
          <p className="text-white/60 text-xs font-semibold uppercase tracking-wider mb-2 flex items-center gap-1">
            <Star size={12} className="text-yellow-400" />
            My Custom Arenas
          </p>
          <div className="flex gap-2 overflow-x-auto pb-2">
            {customArenas.map((arena) => {
              const themeColor = MAP_OPTIONS.find(m => m.id === arena.theme)?.color || "#4CAF50";
              const isSelected = selectedCustomArena === arena.id;
              return (
                <motion.button
                  key={arena.id}
                  onClick={() => handleSelectMap(arena.theme, arena.id)}
                  className={`flex-shrink-0 px-4 py-2 rounded-lg transition-all ${
                    isSelected ? "ring-2 ring-yellow-400" : ""
                  }`}
                  style={{ 
                    backgroundColor: themeColor,
                    opacity: isSelected ? 1 : 0.7
                  }}
                >
                  <div className="flex items-center gap-2 text-white">
                    <Star size={14} className="text-yellow-300" />
                    <span className="text-sm font-semibold whitespace-nowrap">{arena.name}</span>
                  </div>
                </motion.button>
              );
            })}
          </div>
        </motion.div>
      )}

      <div className="safe-bottom-pad sticky bottom-0 z-20 mt-3 flex w-full max-w-xs flex-col items-center bg-gradient-to-t from-black/85 via-black/70 to-transparent pt-3">
      <motion.button
        initial={{ y: 50, opacity: 0 }}
        animate={{ y: 0, opacity: 1 }}
        transition={{ delay: 0.5 }}
        onClick={handleStartGame}
        disabled={!selectedMap}
        className={`min-h-12 w-full px-8 py-4 rounded-full text-xl font-bold transition-all duration-300 relative z-10 ${
          selectedMap
            ? gameMode === "map_editor"
              ? "bg-gradient-to-r from-violet-500 to-purple-600 text-white hover:scale-105 cursor-pointer"
              : "bg-gradient-to-r from-yellow-400 to-orange-500 text-black hover:scale-105 cursor-pointer"
            : "bg-gray-600 text-gray-400 cursor-not-allowed"
        }`}
      >
        {gameMode === "map_editor" ? "Open Editor" : "Start Battle!"}
      </motion.button>

      <motion.button
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        transition={{ delay: 0.6 }}
        onClick={handleBack}
        className="mt-3 min-h-11 px-4 text-gray-300 hover:text-white transition-colors relative z-10"
      >
        ← Back
      </motion.button>
      </div>
    </div>
  );
}
