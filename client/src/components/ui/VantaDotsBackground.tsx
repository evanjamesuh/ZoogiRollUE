import { useEffect, useRef } from "react";

interface VantaDotsBackgroundProps {
  className?: string;
  color?: string;
  color2?: string;
  backgroundColor?: string;
}

function hexToRgb(hex: string): { r: number; g: number; b: number } {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : { r: 255, g: 119, b: 0 };
}

export function VantaDotsBackground({
  className = "",
  color = "#ff7700",
  color2 = "#ff4400",
  backgroundColor = "#222222",
}: VantaDotsBackgroundProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    
    let animationId: number;
    let dots: { x: number; y: number; baseY: number; phase: number; size: number }[] = [];
    
    const color1Rgb = hexToRgb(color);
    const color2Rgb = hexToRgb(color2);
    
    const resize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
      initDots();
    };
    
    const initDots = () => {
      dots = [];
      const spacing = 40;
      const cols = Math.ceil(canvas.width / spacing) + 1;
      const rows = Math.ceil(canvas.height / spacing) + 1;
      
      for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
          dots.push({
            x: col * spacing,
            y: row * spacing,
            baseY: row * spacing,
            phase: (col + row) * 0.3,
            size: 3 + Math.random() * 3
          });
        }
      }
    };
    
    const animate = (time: number) => {
      ctx.fillStyle = backgroundColor;
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      
      dots.forEach(dot => {
        const wave = Math.sin(time * 0.001 + dot.phase) * 15;
        const y = dot.baseY + wave;
        
        const perspective = 1 - (y / canvas.height) * 0.5;
        const size = dot.size * perspective;
        
        const gradient = ctx.createRadialGradient(dot.x, y, 0, dot.x, y, size * 2);
        gradient.addColorStop(0, `rgb(${color1Rgb.r}, ${color1Rgb.g}, ${color1Rgb.b})`);
        gradient.addColorStop(0.5, `rgb(${color2Rgb.r}, ${color2Rgb.g}, ${color2Rgb.b})`);
        gradient.addColorStop(1, 'transparent');
        
        ctx.beginPath();
        ctx.arc(dot.x, y, size, 0, Math.PI * 2);
        ctx.fillStyle = gradient;
        ctx.fill();
      });
      
      animationId = requestAnimationFrame(animate);
    };
    
    resize();
    window.addEventListener('resize', resize);
    animationId = requestAnimationFrame(animate);
    
    return () => {
      window.removeEventListener('resize', resize);
      cancelAnimationFrame(animationId);
    };
  }, [color, color2, backgroundColor]);
  
  return (
    <canvas 
      ref={canvasRef} 
      className={`absolute inset-0 ${className}`}
      style={{ background: backgroundColor, zIndex: 0 }}
    />
  );
}
