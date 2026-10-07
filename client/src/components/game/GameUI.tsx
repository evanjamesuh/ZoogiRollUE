import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { Eye, EyeOff, User, X, Video, Check, ArrowUp, ArrowDown, ArrowLeft, ArrowRight, ChevronUp, ChevronDown, RotateCcw, RotateCw, Minus, Plus, Anchor, Download, ScanEye, Camera, Target, Package, MapPin, Palette } from "lucide-react";
import { MatchHud } from "./MatchHud";
import { moveSelectedElementByArrows, confirmPlacements, rotateSelectedElement, snapToGround, getIncrement, setIncrement } from "./DeveloperMoveControls";
import { getGlobalZoogiScale, setGlobalZoogiScale } from "./Zoogi";
import { getGlobalOrbScale, setGlobalOrbScale } from "./Orb";
import { PhysicsControlPanel } from "./PhysicsControlPanel";
import { MapEditorPanel } from "./MapEditorPanel";
import { BackgroundControlPanel, DEFAULT_BACKGROUND_SETTINGS } from "./BackgroundControlPanel";
import { ZoneEditorPanel } from "./ZoneEditorPanel";
import { CollisionTuningPanel } from "./CollisionTuningPanel";
import { AIControlsPanel } from "./AIControlsPanel";
import { exportAllOffsets } from "@/lib/treeOffsets";
import { useEffect, useState, useRef, useCallback, PointerEvent as ReactPointerEvent } from "react";

interface HoldButtonProps {
  onAction: () => void;
  className: string;
  title?: string;
  children: React.ReactNode;
}

function HoldButton({ onAction, className, title, children }: HoldButtonProps) {
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const buttonRef = useRef<HTMLButtonElement>(null);
  const activePointerRef = useRef<number | null>(null);
  
  const stopHold = useCallback(() => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    if (activePointerRef.current !== null && buttonRef.current) {
      try {
        buttonRef.current.releasePointerCapture(activePointerRef.current);
      } catch (e) {
        // Pointer capture may already be released
      }
      activePointerRef.current = null;
    }
  }, []);
  
  const startHold = useCallback((pointerId: number) => {
    if (intervalRef.current) return;
    activePointerRef.current = pointerId;
    if (buttonRef.current) {
      try {
        buttonRef.current.setPointerCapture(pointerId);
      } catch (e) {
        // Pointer capture may fail on some browsers
      }
    }
    onAction();
    intervalRef.current = setInterval(() => {
      onAction();
    }, 100);
  }, [onAction]);
  
  useEffect(() => {
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, []);
  
  return (
    <button
      ref={buttonRef}
      onPointerDown={(e: ReactPointerEvent) => {
        e.preventDefault();
        startHold(e.pointerId);
      }}
      onPointerUp={stopHold}
      onPointerCancel={stopHold}
      onLostPointerCapture={stopHold}
      className={className}
      title={title}
      style={{ touchAction: "none" }}
    >
      {children}
    </button>
  );
}

function LaunchPadTiltControl() {
  const launchPadTilt = useZoogiGame((state) => state.launchPadTilt);
  const setLaunchPadTilt = useZoogiGame((state) => state.setLaunchPadTilt);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Tilt:</span>
      <button
        onClick={() => setLaunchPadTilt(launchPadTilt - 2)}
        className="w-8 h-8 rounded-lg bg-cyan-600/90 text-white hover:bg-cyan-500 transition-all flex items-center justify-center"
        title="Less foreground (lower tilt)"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadTilt.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadTilt(launchPadTilt + 2)}
        className="w-8 h-8 rounded-lg bg-cyan-600/90 text-white hover:bg-cyan-500 transition-all flex items-center justify-center"
        title="More foreground (higher tilt)"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

function LaunchPadPitchControl() {
  const launchPadPitch = useZoogiGame((state) => state.launchPadPitch);
  const setLaunchPadPitch = useZoogiGame((state) => state.setLaunchPadPitch);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Pitch:</span>
      <button
        onClick={() => setLaunchPadPitch(launchPadPitch - 2)}
        className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
        title="Look down"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadPitch.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadPitch(launchPadPitch + 2)}
        className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
        title="Look up"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

function LaunchPadHeightControl() {
  const launchPadHeight = useZoogiGame((state) => state.launchPadHeight);
  const setLaunchPadHeight = useZoogiGame((state) => state.setLaunchPadHeight);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Height:</span>
      <button
        onClick={() => setLaunchPadHeight(launchPadHeight - 5)}
        className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
        title="Move camera down"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadHeight.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadHeight(launchPadHeight + 5)}
        className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
        title="Move camera up"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

export function GameUI() {
  const playerReady = useZoogiGame((state) => state.playerEntity != null);
  const gameMode = useZoogiGame((state) => state.gameMode);
  const devMoveRefresh = useZoogiGame((state) => (state.developerMoveMode ? Math.floor(state.gameTimer * 4) : 0));
  const firstPersonView = useZoogiGame((state) => state.firstPersonView);
  const toggleFirstPersonView = useZoogiGame((state) => state.toggleFirstPersonView);
  const overShoulderView = useZoogiGame((state) => state.overShoulderView);
  const toggleOverShoulderView = useZoogiGame((state) => state.toggleOverShoulderView);
  const birdsEyeView = useZoogiGame((state) => state.birdsEyeView);
  const toggleBirdsEyeView = useZoogiGame((state) => state.toggleBirdsEyeView);
  const launchPadView = useZoogiGame((state) => state.launchPadView);
  const toggleLaunchPadView = useZoogiGame((state) => state.toggleLaunchPadView);

  const [showExportMenu, setShowExportMenu] = useState(false);
  const [showMapEditorPanel, setShowMapEditorPanel] = useState(false);
  const [showBackgroundPanel, setShowBackgroundPanel] = useState(false);
  const isMapEditor = gameMode === "map_editor";

  if (!playerReady && !isMapEditor) return null;

  // Map Editor mode - get reactive state with hooks
  const selectedEditorModelId = useZoogiGame(state => state.selectedEditorModelId);
  const editorSnapEnabled = useZoogiGame(state => state.editorSnapEnabled);
  const editorPlacedModels = useZoogiGame(state => state.editorPlacedModels);
  const developerCamera = useZoogiGame(state => state.developerCamera);
  const backgroundSettings = useZoogiGame(state => state.backgroundSettings);
  const setBackgroundSettings = useZoogiGame(state => state.setBackgroundSettings);
  const wallSettings = useZoogiGame(state => state.wallSettings);
  const setWallSettings = useZoogiGame(state => state.setWallSettings);
  const zoneSettings = useZoogiGame(state => state.zoneSettings);
  const setZoneSettings = useZoogiGame(state => state.setZoneSettings);
  
  // Map Editor mode - show simplified UI
  if (isMapEditor) {
    const { 
      toggleEditorSnap, 
      moveSelectedEditorModel, 
      rotateSelectedEditorModel,
      scaleSelectedEditorModel,
      selectEditorModel,
      removeEditorPlacedModel,
      toggleDeveloperCamera
    } = useZoogiGame.getState();
    
    const selectedModel = editorPlacedModels.find(m => m.id === selectedEditorModelId);
    
    return (
      <>
      <div className="fixed inset-0 pointer-events-none z-10">
        {/* Map Editor Header */}
        <div className="absolute top-4 left-4 right-4 flex justify-between items-start pointer-events-auto">
          <div className="bg-black/70 backdrop-blur-md rounded-xl p-3">
            <div className="flex items-center gap-2">
              <MapPin className="w-5 h-5 text-purple-400" />
              <h2 className="text-white font-bold">Map Editor</h2>
            </div>
            <p className="text-white/60 text-xs mt-1">Place and arrange 3D models</p>
          </div>
          
          <button
            onClick={() => useZoogiGame.getState().returnToMenu()}
            className="bg-red-600/80 hover:bg-red-500 text-white px-4 py-2 rounded-lg font-medium transition-colors"
          >
            Exit Editor
          </button>
        </div>
        
        {/* All Buttons - Left Side */}
        <div className={`absolute top-1/2 -translate-y-1/2 flex flex-col gap-2 pointer-events-auto transition-all ${showBackgroundPanel ? 'left-72' : 'left-4'}`}>
          {/* Models, Save, Export */}
          <button
            onClick={() => setShowMapEditorPanel(!showMapEditorPanel)}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${showMapEditorPanel ? 'bg-purple-500' : 'bg-purple-600 hover:bg-purple-500'} text-white`}
            title="Models"
          >
            <Package size={20} />
          </button>
          <button
            onClick={async () => {
              try {
                const success = await useZoogiGame.getState().saveMapDecorations();
                if (success) {
                  alert("Map saved successfully!");
                } else {
                  alert("Failed to save map");
                }
              } catch (err) {
                console.error("Save error:", err);
                alert("Failed to save map - please try again");
              }
            }}
            className="p-3 rounded-xl backdrop-blur-sm bg-green-600 hover:bg-green-500 text-white transition-all"
            title="Save"
          >
            <Check size={20} />
          </button>
          <button
            onClick={() => useZoogiGame.getState().exportBackgroundSettings()}
            className="p-3 rounded-xl backdrop-blur-sm bg-blue-600 hover:bg-blue-500 text-white transition-all"
            title="Export"
          >
            <Download size={20} />
          </button>
          <button
            onClick={() => {
              console.log("[GameUI] Pink button clicked, current showBackgroundPanel:", showBackgroundPanel);
              setShowBackgroundPanel(!showBackgroundPanel);
            }}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${showBackgroundPanel ? 'bg-pink-500' : 'bg-pink-600 hover:bg-pink-500'} text-white`}
            title="Background Controls"
          >
            <Palette size={20} />
          </button>
          <button
            onClick={() => useZoogiGame.getState().setShowCollisionTuningPanel(!useZoogiGame.getState().showCollisionTuningPanel)}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${useZoogiGame.getState().showCollisionTuningPanel ? 'bg-cyan-500' : 'bg-cyan-600 hover:bg-cyan-500'} text-white`}
            title="Collision Tuning"
          >
            <Target size={20} />
          </button>
          
          <div className="h-2" /> {/* Spacer */}
          
          {/* Camera View Buttons */}
          <button
            onClick={toggleFirstPersonView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              firstPersonView 
                ? "bg-green-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="First person view"
          >
            <User size={20} />
          </button>
          
          <button
            onClick={toggleOverShoulderView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              overShoulderView 
                ? "bg-orange-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Over the shoulder view"
          >
            <ScanEye size={20} />
          </button>
          
          <button
            onClick={toggleBirdsEyeView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              birdsEyeView 
                ? "bg-blue-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Bird's eye view"
          >
            {birdsEyeView ? <EyeOff size={20} /> : <Eye size={20} />}
          </button>
          
          <button
            onClick={toggleLaunchPadView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              launchPadView 
                ? "bg-cyan-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Launch Pad view"
          >
            <Target size={20} />
          </button>
          
          <button
            onClick={toggleDeveloperCamera}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              developerCamera
                ? "bg-yellow-500/90 text-black ring-2 ring-white"
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Developer Camera (free roam)"
          >
            <Video size={20} />
          </button>
        </div>
        
        {/* Background Control Panel - Only show when not in map editor mode (map editor has its own) */}
        {showBackgroundPanel && !isMapEditor && (
          <div className="pointer-events-auto">
            <BackgroundControlPanel
              settings={backgroundSettings}
              onSettingsChange={setBackgroundSettings}
              onImageUpload={(imageDataUrl) => {
                setBackgroundSettings({
                  ...backgroundSettings,
                  customImage: imageDataUrl
                });
              }}
              wallSettings={wallSettings}
              onWallSettingsChange={setWallSettings}
              zoneSettings={zoneSettings}
              onZoneSettingsChange={setZoneSettings}
            />
          </div>
        )}
        
        {/* Transform Controls - Bottom Right */}
        {selectedModel && (
          <div className="absolute bottom-4 right-4 bg-black/80 backdrop-blur-md rounded-xl p-4 pointer-events-auto">
            <div className="text-white text-sm font-medium mb-2">{selectedModel.name}</div>
            <div className="text-white/60 text-xs mb-3">
              Pos: {selectedModel.position.map(v => v.toFixed(1)).join(", ")}
            </div>
            
            {/* Movement Controls */}
            <div className="grid grid-cols-3 gap-1 mb-2">
              <div />
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0, -0.5)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowUp size={16} />
              </HoldButton>
              <div />
              <HoldButton
                onAction={() => moveSelectedEditorModel(-0.5, 0, 0)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowLeft size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0, 0.5)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowDown size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0.5, 0, 0)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowRight size={16} />
              </HoldButton>
            </div>
            
            {/* Height & Rotation Controls */}
            <div className="flex gap-1 mb-2">
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0.5, 0)}
                className="flex-1 h-10 bg-blue-600 hover:bg-blue-500 rounded-lg flex items-center justify-center text-white"
                title="Raise"
              >
                <ChevronUp size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, -0.5, 0)}
                className="flex-1 h-10 bg-blue-600 hover:bg-blue-500 rounded-lg flex items-center justify-center text-white"
                title="Lower"
              >
                <ChevronDown size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => rotateSelectedEditorModel("y", -15)}
                className="flex-1 h-10 bg-orange-600 hover:bg-orange-500 rounded-lg flex items-center justify-center text-white"
                title="Rotate Left"
              >
                <RotateCcw size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => rotateSelectedEditorModel("y", 15)}
                className="flex-1 h-10 bg-orange-600 hover:bg-orange-500 rounded-lg flex items-center justify-center text-white"
                title="Rotate Right"
              >
                <RotateCw size={16} />
              </HoldButton>
            </div>
            
            {/* Scale Controls */}
            <div className="flex gap-1 mb-2">
              <HoldButton
                onAction={() => scaleSelectedEditorModel(-0.1)}
                className="flex-1 h-10 bg-purple-600 hover:bg-purple-500 rounded-lg flex items-center justify-center text-white"
                title="Scale Down"
              >
                <Minus size={16} />
              </HoldButton>
              <div className="flex-1 h-10 bg-slate-700 rounded-lg flex items-center justify-center text-white text-xs">
                {selectedModel?.scale[0].toFixed(1)}x
              </div>
              <HoldButton
                onAction={() => scaleSelectedEditorModel(0.1)}
                className="flex-1 h-10 bg-purple-600 hover:bg-purple-500 rounded-lg flex items-center justify-center text-white"
                title="Scale Up"
              >
                <Plus size={16} />
              </HoldButton>
            </div>
            
            {/* Action Buttons */}
            <div className="flex gap-1">
              <button
                onClick={toggleEditorSnap}
                className={`flex-1 py-2 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium ${
                  editorSnapEnabled ? "bg-cyan-600" : "bg-slate-600"
                }`}
              >
                <Anchor size={12} />
                Snap
              </button>
              <button
                onClick={() => selectEditorModel(null)}
                className="flex-1 py-2 bg-green-600 hover:bg-green-500 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium"
              >
                <Check size={12} />
                Confirm
              </button>
              <button
                onClick={() => {
                  removeEditorPlacedModel(selectedEditorModelId!);
                  selectEditorModel(null);
                }}
                className="flex-1 py-2 bg-red-600 hover:bg-red-500 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium"
              >
                <X size={12} />
                Delete
              </button>
            </div>
          </div>
        )}
        
        {/* Camera Controls hint */}
        <div className="absolute bottom-4 left-4 bg-black/70 backdrop-blur-md rounded-xl p-3 pointer-events-auto">
          <p className="text-white/80 text-sm font-medium">Camera Controls</p>
          <p className="text-white/50 text-xs">Drag to rotate • Pinch to zoom</p>
        </div>
      </div>
      
      {/* Map Editor Panel - fixed position with pointer-events enabled */}
      <div className="fixed inset-0 pointer-events-none z-50">
        <div className="pointer-events-auto">
          <MapEditorPanel isOpen={showMapEditorPanel} onClose={() => setShowMapEditorPanel(false)} />
        </div>
      </div>
      
      {/* Background Control Panel for Map Editor */}
      <BackgroundControlPanel
        settings={backgroundSettings}
        onSettingsChange={setBackgroundSettings}
        onImageUpload={(imageDataUrl) => {
          setBackgroundSettings({
            ...backgroundSettings,
            customImage: imageDataUrl
          });
        }}
        wallSettings={wallSettings}
        onWallSettingsChange={setWallSettings}
        zoneSettings={zoneSettings}
        onZoneSettingsChange={setZoneSettings}
        isOpen={showBackgroundPanel}
        onOpenChange={setShowBackgroundPanel}
      />
      </>
    );
  }

  return (
    <div className="fixed inset-0 pointer-events-none z-10">
      <MatchHud />
      {devMoveRefresh >= 0 && useZoogiGame.getState().developerMoveMode && (

        <div className="absolute top-4 right-4 flex items-center gap-2 pointer-events-auto">
          <button
            onClick={() => useZoogiGame.getState().toggleIoControlsVisible()}
            className={`w-10 h-10 rounded-lg flex items-center justify-center transition-all ${
              useZoogiGame.getState().ioControlsVisible
                ? "bg-blue-600 hover:bg-blue-500"
                : "bg-gray-700/70 hover:bg-gray-600"
            }`}
            title={useZoogiGame.getState().ioControlsVisible ? "Hide Element Controls" : "Show Element Controls"}
          >
            <Anchor size={18} className="text-white" />
          </button>
          <button
            onClick={() => useZoogiGame.getState().toggleDevToolsVisible()}
            className={`w-10 h-10 rounded-lg flex items-center justify-center transition-all ${
              useZoogiGame.getState().devToolsVisible
                ? "bg-green-600 hover:bg-green-500"
                : "bg-gray-700/70 hover:bg-gray-600"
            }`}
            title={useZoogiGame.getState().devToolsVisible ? "Hide Physics Panel" : "Show Physics Panel"}
          >
            {useZoogiGame.getState().devToolsVisible ? <Eye size={18} className="text-white" /> : <EyeOff size={18} className="text-white" />}
          </button>
          {useZoogiGame.getState().devToolsVisible && <PhysicsControlPanel />}
        </div>
      )}
      
      {useZoogiGame.getState().developerMoveMode && useZoogiGame.getState().ioControlsVisible && (
        <div className="absolute bottom-4 left-1/2 transform -translate-x-1/2 flex flex-col items-center gap-2 pointer-events-auto">
          <div className="bg-black/80 rounded-xl p-3 backdrop-blur-sm">
            <div className="text-white/70 text-xs text-center mb-2">
              {useZoogiGame.getState().selectedMoveElement 
                ? `Selected: Tree #${useZoogiGame.getState().selectedMoveElement?.index}` 
                : "Click an element to select"}
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <button
                onClick={() => setIncrement(getIncrement() - 0.05)}
                className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
                title="Decrease step size"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-12 text-center font-mono">{getIncrement().toFixed(2)}</span>
              <button
                onClick={() => setIncrement(getIncrement() + 0.05)}
                className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
                title="Increase step size"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <span className="text-white/60 text-xs">Char:</span>
              <button
                onClick={() => setGlobalZoogiScale(getGlobalZoogiScale() - 0.01)}
                className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
                title="Decrease character scale"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-10 text-center font-mono">{getGlobalZoogiScale().toFixed(2)}</span>
              <button
                onClick={() => setGlobalZoogiScale(getGlobalZoogiScale() + 0.01)}
                className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
                title="Increase character scale"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <span className="text-white/60 text-xs">Orb:</span>
              <button
                onClick={() => setGlobalOrbScale(getGlobalOrbScale() - 0.01)}
                className="w-8 h-8 rounded-lg bg-yellow-600/90 text-white hover:bg-yellow-500 transition-all flex items-center justify-center"
                title="Decrease orb scale"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-10 text-center font-mono">{getGlobalOrbScale().toFixed(2)}</span>
              <button
                onClick={() => setGlobalOrbScale(getGlobalOrbScale() + 0.01)}
                className="w-8 h-8 rounded-lg bg-yellow-600/90 text-white hover:bg-yellow-500 transition-all flex items-center justify-center"
                title="Increase orb scale"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <LaunchPadTiltControl />
            <LaunchPadPitchControl />
            <LaunchPadHeightControl />
            
            <div className="flex gap-3 items-center">
              <div className="flex flex-col items-center gap-1">
                <button
                  onClick={() => moveSelectedElementByArrows(0, 0, -getIncrement())}
                  className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                  title="Move Forward"
                >
                  <ArrowUp size={18} />
                </button>
                <div className="flex gap-1">
                  <button
                    onClick={() => moveSelectedElementByArrows(-getIncrement(), 0, 0)}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Left"
                  >
                    <ArrowLeft size={18} />
                  </button>
                  <button
                    onClick={() => moveSelectedElementByArrows(0, 0, getIncrement())}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Backward"
                  >
                    <ArrowDown size={18} />
                  </button>
                  <button
                    onClick={() => moveSelectedElementByArrows(getIncrement(), 0, 0)}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Right"
                  >
                    <ArrowRight size={18} />
                  </button>
                </div>
              </div>
              <div className="flex flex-col gap-1">
                <button
                  onClick={() => moveSelectedElementByArrows(0, getIncrement(), 0)}
                  className="w-10 h-10 rounded-lg bg-blue-600/90 text-white hover:bg-blue-500 transition-all flex items-center justify-center"
                  title="Raise"
                >
                  <ChevronUp size={18} />
                </button>
                <button
                  onClick={() => moveSelectedElementByArrows(0, -getIncrement(), 0)}
                  className="w-10 h-10 rounded-lg bg-blue-600/90 text-white hover:bg-blue-500 transition-all flex items-center justify-center"
                  title="Lower"
                >
                  <ChevronDown size={18} />
                </button>
              </div>
              <div className="flex flex-col gap-1">
                <button
                  onClick={() => rotateSelectedElement(-0.1)}
                  className="w-10 h-10 rounded-lg bg-orange-600/90 text-white hover:bg-orange-500 transition-all flex items-center justify-center"
                  title="Rotate Left"
                >
                  <RotateCcw size={18} />
                </button>
                <button
                  onClick={() => rotateSelectedElement(0.1)}
                  className="w-10 h-10 rounded-lg bg-orange-600/90 text-white hover:bg-orange-500 transition-all flex items-center justify-center"
                  title="Rotate Right"
                >
                  <RotateCw size={18} />
                </button>
              </div>
            </div>
            
            <div className="flex gap-2 mt-3">
              <button
                onClick={snapToGround}
                className="flex-1 px-3 py-2 rounded-lg bg-cyan-600 text-white font-bold hover:bg-cyan-500 transition-all flex items-center justify-center gap-2"
                title="Snap to ground (Y=0)"
              >
                <Anchor size={16} />
                Snap
              </button>
              <button
                onClick={confirmPlacements}
                className="flex-1 px-3 py-2 rounded-lg bg-green-600 text-white font-bold hover:bg-green-500 transition-all flex items-center justify-center gap-2"
                title="Confirm all placements"
              >
                <Check size={16} />
                Confirm
              </button>
              <button
                onClick={() => setShowExportMenu(!showExportMenu)}
                className={`flex-1 px-3 py-2 rounded-lg ${showExportMenu ? 'bg-purple-400' : 'bg-purple-600'} text-white font-bold hover:bg-purple-500 transition-all flex items-center justify-center gap-2`}
                title="Export options"
              >
                <Download size={16} />
                Export
              </button>
            </div>
            
            {showExportMenu && (
              <div className="flex flex-col gap-2 mt-2 p-2 bg-gray-900/90 rounded-lg border border-purple-500/50">
                <button
                  onClick={() => {
                    const data = exportAllOffsets();
                    navigator.clipboard.writeText(data).then(() => {
                      alert("All element positions copied to clipboard!\n\nCheck console for full output.");
                    }).catch(() => {
                      prompt("Copy these positions:", data);
                    });
                    console.log("EXPORTED ELEMENT POSITIONS:\n" + data);
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-purple-600 text-white font-bold hover:bg-purple-500 transition-all flex items-center justify-center gap-2"
                  title="Export all element positions"
                >
                  <Download size={16} />
                  Export Elements
                </button>
                <button
                  onClick={() => {
                    const camera = (window as any).__ZOOGI_CAMERA__;
                    const player = useZoogiGame.getState().playerEntity;
                    const orbitAngle = (window as any).__ZOOGI_ORBIT_ANGLE__ || 0;
                    if (camera && player) {
                      const pos = camera.position;
                      const target = (window as any).__ZOOGI_CAMERA_TARGET__ || { x: 0, y: 0, z: 0 };
                      const playerPos = player.position;
                      const worldDx = pos.x - playerPos[0];
                      const worldDz = pos.z - playerPos[2];
                      const localForward = worldDx * Math.sin(orbitAngle) + worldDz * Math.cos(orbitAngle);
                      const localRight = worldDx * Math.cos(orbitAngle) - worldDz * Math.sin(orbitAngle);
                      const targetDx = target.x - playerPos[0];
                      const targetDz = target.z - playerPos[2];
                      const targetLocalForward = targetDx * Math.sin(orbitAngle) + targetDz * Math.cos(orbitAngle);
                      const targetLocalRight = targetDx * Math.cos(orbitAngle) - targetDz * Math.sin(orbitAngle);
                      const offsetPos: [number, number, number] = [
                        parseFloat(localForward.toFixed(2)),
                        parseFloat(pos.y.toFixed(2)),
                        parseFloat(localRight.toFixed(2))
                      ];
                      const offsetLookAt: [number, number, number] = [
                        parseFloat(targetLocalForward.toFixed(2)),
                        parseFloat(target.y.toFixed(2)),
                        parseFloat(targetLocalRight.toFixed(2))
                      ];
                      const cameraData = {
                        position: offsetPos,
                        lookAt: offsetLookAt
                      };
                      const dataStr = JSON.stringify(cameraData, null, 2);
                      navigator.clipboard.writeText(dataStr).then(() => {
                        alert(`Camera Position Saved (rotation-aware)!\n\nForward/Height/Right: [${offsetPos.join(', ')}]\nLookAt: [${offsetLookAt.join(', ')}]\n\nCamera will follow player facing direction!`);
                      }).catch(() => {
                        prompt("Copy camera position:", dataStr);
                      });
                      console.log("CAMERA POSITION (rotation-aware, forward/height/right):", dataStr);
                      
                      useZoogiGame.getState().setSavedCameraPosition({
                        position: offsetPos,
                        lookAt: offsetLookAt
                      });
                    } else if (!camera) {
                      alert("Enable Developer Camera first, then position it where you want!");
                    } else {
                      alert("No player entity found!");
                    }
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-cyan-600 text-white font-bold hover:bg-cyan-500 transition-all flex items-center justify-center gap-2"
                  title="Save current camera position for over-shoulder view"
                >
                  <Camera size={16} />
                  Save Camera Position
                </button>
                <button
                  onClick={() => {
                    const state = useZoogiGame.getState();
                    const tilt = state.launchPadTilt;
                    const pitch = state.launchPadPitch;
                    const height = state.launchPadHeight;
                    
                    const codeSnippet = `// Launch Pad Camera Settings - paste in client/src/lib/stores/useZoogiGame.tsx (initial state)
launchPadTilt: ${tilt},
launchPadPitch: ${pitch},
launchPadHeight: ${height},`;
                    
                    navigator.clipboard.writeText(codeSnippet).then(() => {
                      alert(`Launch Pad Camera Code Copied!\n\nPaste these values in useZoogiGame.tsx initial state:\n\nlaunchPadTilt: ${tilt}\nlaunchPadPitch: ${pitch}\nlaunchPadHeight: ${height}`);
                    }).catch(() => {
                      prompt("Copy Launch Pad camera code:", codeSnippet);
                    });
                    console.log("=== LAUNCH PAD CAMERA CODE ===");
                    console.log("Paste in: client/src/lib/stores/useZoogiGame.tsx (initial state section)");
                    console.log(codeSnippet);
                    console.log("==============================");
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-green-600 text-white font-bold hover:bg-green-500 transition-all flex items-center justify-center gap-2"
                  title="Save Launch Pad camera tilt, pitch, and height as code"
                >
                  <Camera size={16} />
                  Save Launch Pad Camera
                </button>
              </div>
            )}
          </div>
        </div>
      )}
      
      {/* Panels available during gameplay */}
      <CollisionTuningPanel />
      <AIControlsPanel />
    </div>
  );
}
