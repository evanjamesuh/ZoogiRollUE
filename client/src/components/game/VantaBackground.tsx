import { useEffect, useRef, useState } from "react";
import * as THREE from "three";

export function VantaBackground() {
  const vantaRef = useRef<HTMLDivElement>(null);
  const [vantaEffect, setVantaEffect] = useState<any>(null);

  useEffect(() => {
    if (!vantaRef.current) return;
    
    const loadVanta = async () => {
      try {
        const VANTA = await import("vanta/dist/vanta.dots.min");
        
        if (vantaEffect) {
          vantaEffect.destroy();
        }
        
        const effect = VANTA.default({
          el: vantaRef.current,
          THREE: THREE,
          mouseControls: true,
          touchControls: true,
          gyroControls: false,
          minHeight: 200.0,
          minWidth: 200.0,
          scale: 1.0,
          scaleMobile: 1.0,
          color: 0xff7700,
          color2: 0xff4400,
          backgroundColor: 0x222222,
          size: 8.9,
          spacing: 36.0,
          showLines: false
        });
        
        setVantaEffect(effect);
      } catch (error) {
        console.error("Failed to load Vanta.js:", error);
      }
    };
    
    loadVanta();
    
    return () => {
      if (vantaEffect) {
        vantaEffect.destroy();
      }
    };
  }, []);

  return (
    <div 
      ref={vantaRef} 
      className="fixed inset-0"
      style={{ zIndex: -1 }}
    />
  );
}
