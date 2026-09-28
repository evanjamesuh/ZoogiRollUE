import { useEffect, useState } from "react";
import { initRapier, isRapierReady } from "./rapierInit";
import { usePhysicsWorld } from "./usePhysicsWorld";

interface PhysicsProviderProps {
  children: React.ReactNode;
}

export function PhysicsProvider({ children }: PhysicsProviderProps) {
  const [ready, setReady] = useState(false);
  const initialize = usePhysicsWorld(state => state.initialize);
  const cleanup = usePhysicsWorld(state => state.cleanup);
  const isInitialized = usePhysicsWorld(state => state.isInitialized);
  
  useEffect(() => {
    let mounted = true;
    
    const init = async () => {
      try {
        await initRapier();
        if (mounted) {
          await initialize();
          setReady(true);
        }
      } catch (error) {
        console.error("Failed to initialize Rapier physics:", error);
      }
    };
    
    init();
    
    return () => {
      mounted = false;
      cleanup();
    };
  }, [initialize, cleanup]);
  
  if (!ready || !isInitialized) {
    return null;
  }
  
  return <>{children}</>;
}
