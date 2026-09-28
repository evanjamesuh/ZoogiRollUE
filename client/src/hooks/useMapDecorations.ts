import { useEffect, useMemo } from "react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { getDeviceId } from "@/lib/deviceId";

export function useMapDecorations() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const phase = useZoogiGame((state) => state.phase);
  const loadEditorPlacedModels = useZoogiGame((state) => state.loadEditorPlacedModels);
  
  const deviceId = useMemo(() => getDeviceId(), []);
  
  useEffect(() => {
    if (phase !== "playing" || !selectedMap) return;
    
    const storageKey = `map_decorations_${selectedMap}_${deviceId}`;
    const saved = localStorage.getItem(storageKey);
    
    if (saved) {
      try {
        const data = JSON.parse(saved);
        if (data.placedModels && Array.isArray(data.placedModels) && data.placedModels.length > 0) {
          loadEditorPlacedModels(data.placedModels);
          console.log(`Loaded ${data.placedModels.length} decorations for map: ${selectedMap}`);
        }
      } catch (err) {
        console.error("Failed to load map decorations:", err);
      }
    }
  }, [phase, selectedMap, deviceId, loadEditorPlacedModels]);
}
