import { useState, useEffect, useRef, useCallback } from "react";

interface Particle {
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  maxLife: number;
  size: number;
  r: number;
  g: number;
  b: number;
}

interface CinematicLogoProps {
  onAnimationComplete?: () => void;
}

export function CinematicLogo({ onAnimationComplete }: CinematicLogoProps) {
  const [stage, setStage] = useState(0);
  const [showTapPrompt, setShowTapPrompt] = useState(false);
  const [animationComplete, setAnimationComplete] = useState(false);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const particlesRef = useRef<Particle[]>([]);
  const animationFrameRef = useRef<number>(0);
  const particlesActiveRef = useRef(false);
  const dimensionsRef = useRef({ width: 0, height: 0, dpr: 1 });

  const createParticleBurst = useCallback((centerX: number, centerY: number) => {
    const particles: Particle[] = [];
    const colors = [
      { r: 255, g: 119, b: 0 },
      { r: 255, g: 68, b: 0 },
      { r: 255, g: 170, b: 0 },
      { r: 255, g: 85, b: 0 },
      { r: 255, g: 255, b: 255 },
    ];
    
    for (let i = 0; i < 40; i++) {
      const angle = (Math.PI * 2 * i) / 40 + Math.random() * 0.5;
      const speed = 3 + Math.random() * 6;
      const c = colors[Math.floor(Math.random() * colors.length)];
      particles.push({
        x: centerX,
        y: centerY,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 1,
        maxLife: 0.8 + Math.random() * 0.4,
        size: 2 + Math.random() * 3,
        r: c.r,
        g: c.g,
        b: c.b,
      });
    }
    
    for (let i = 0; i < 12; i++) {
      const angle = Math.random() * Math.PI * 2;
      const speed = 1 + Math.random() * 3;
      particles.push({
        x: centerX,
        y: centerY,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 1,
        maxLife: 1.2 + Math.random() * 0.5,
        size: 4 + Math.random() * 4,
        r: 255,
        g: 255,
        b: 255,
      });
    }
    
    particlesRef.current = particles;
  }, []);

  const animateParticles = useCallback(() => {
    if (!particlesActiveRef.current) return;
    
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    
    const { width, height, dpr } = dimensionsRef.current;
    
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, width, height);
    
    const particles = particlesRef.current;
    let hasActiveParticles = false;
    
    for (let i = particles.length - 1; i >= 0; i--) {
      const p = particles[i];
      p.x += p.vx;
      p.y += p.vy;
      p.vx *= 0.96;
      p.vy *= 0.96;
      p.life -= 0.03;
      
      if (p.life <= 0) {
        particles.splice(i, 1);
        continue;
      }
      
      hasActiveParticles = true;
      const alpha = Math.min(1, p.life / p.maxLife);
      
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size * 1.5, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${p.r}, ${p.g}, ${p.b}, ${alpha * 0.6})`;
      ctx.fill();
      
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size * 0.6, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(255, 255, 255, ${alpha})`;
      ctx.fill();
    }
    
    if (hasActiveParticles && particlesActiveRef.current) {
      animationFrameRef.current = requestAnimationFrame(animateParticles);
    }
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    
    const resizeCanvas = () => {
      if (canvas) {
        const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
        const width = window.innerWidth;
        const height = window.innerHeight;
        dimensionsRef.current = { width, height, dpr };
        canvas.width = width * dpr;
        canvas.height = height * dpr;
        canvas.style.width = `${width}px`;
        canvas.style.height = `${height}px`;
        const ctx = canvas.getContext("2d");
        if (ctx) {
          ctx.setTransform(1, 0, 0, 1, 0, 0);
          ctx.scale(dpr, dpr);
        }
      }
    };
    
    resizeCanvas();

    const timers: NodeJS.Timeout[] = [];
    let particleTimeout: NodeJS.Timeout;
    
    timers.push(setTimeout(() => setStage(1), 100));
    timers.push(setTimeout(() => setStage(2), 500));
    timers.push(setTimeout(() => {
      setStage(3);
      if (canvas) {
        const { width, height } = dimensionsRef.current;
        particlesActiveRef.current = true;
        createParticleBurst(width / 2, height / 2);
        animateParticles();
        particleTimeout = setTimeout(() => {
          particlesActiveRef.current = false;
          cancelAnimationFrame(animationFrameRef.current);
          particlesRef.current = [];
        }, 600);
      }
    }, 900));
    timers.push(setTimeout(() => {
      setStage(4);
      setAnimationComplete(true);
      onAnimationComplete?.();
    }, 1400));
    timers.push(setTimeout(() => setShowTapPrompt(true), 2000));
    
    window.addEventListener("resize", resizeCanvas);
    
    return () => {
      timers.forEach(clearTimeout);
      clearTimeout(particleTimeout);
      particlesActiveRef.current = false;
      cancelAnimationFrame(animationFrameRef.current);
      window.removeEventListener("resize", resizeCanvas);
    };
  }, [createParticleBurst, animateParticles, onAnimationComplete]);

  return (
    <div className="absolute inset-0 flex items-center justify-center overflow-hidden">
      <style>{`
        @keyframes slideFromLeft {
          0% {
            transform: translateX(-150%) scale(1.1);
            opacity: 0;
            filter: blur(8px);
          }
          60% {
            transform: translateX(5%) scale(1);
            opacity: 1;
            filter: blur(0);
          }
          100% {
            transform: translateX(0) scale(1);
            opacity: 1;
            filter: blur(0);
          }
        }
        
        @keyframes slideFromRight {
          0% {
            transform: translateX(150%) scale(1.1);
            opacity: 0;
            filter: blur(8px);
          }
          60% {
            transform: translateX(-5%) scale(1);
            opacity: 1;
            filter: blur(0);
          }
          100% {
            transform: translateX(0) scale(1);
            opacity: 1;
            filter: blur(0);
          }
        }
        
        @keyframes snapCenter {
          0% {
            transform: scale(0) rotate(-10deg);
            opacity: 0;
          }
          50% {
            transform: scale(1.3) rotate(5deg);
            opacity: 1;
          }
          70% {
            transform: scale(0.95) rotate(-2deg);
          }
          100% {
            transform: scale(1) rotate(0deg);
            opacity: 1;
          }
        }
        
        @keyframes screenShake {
          0%, 100% { transform: translate(0, 0); }
          10% { transform: translate(-4px, 2px); }
          20% { transform: translate(4px, -2px); }
          30% { transform: translate(-3px, 3px); }
          40% { transform: translate(3px, -1px); }
          50% { transform: translate(-2px, 1px); }
          60% { transform: translate(2px, -2px); }
          70% { transform: translate(-1px, 1px); }
          80% { transform: translate(1px, 0px); }
        }
        
        @keyframes radialFlare {
          0% {
            transform: scale(0);
            opacity: 1;
          }
          50% {
            opacity: 0.8;
          }
          100% {
            transform: scale(3);
            opacity: 0;
          }
        }
        
        @keyframes lensFlare {
          0% {
            opacity: 0;
            transform: scale(0.5) rotate(0deg);
          }
          30% {
            opacity: 1;
            transform: scale(1.2) rotate(45deg);
          }
          100% {
            opacity: 0;
            transform: scale(2) rotate(90deg);
          }
        }
        
        @keyframes glowPulse {
          0%, 100% {
            filter: drop-shadow(0 0 20px rgba(255, 119, 0, 0.6)) drop-shadow(0 0 40px rgba(255, 68, 0, 0.4));
          }
          50% {
            filter: drop-shadow(0 0 30px rgba(255, 119, 0, 0.8)) drop-shadow(0 0 60px rgba(255, 68, 0, 0.6));
          }
        }
        
        @keyframes fadeInUp {
          0% {
            opacity: 0;
            transform: translateY(20px);
          }
          100% {
            opacity: 1;
            transform: translateY(0);
          }
        }
        
        .word-zoogi {
          animation: slideFromLeft 0.5s cubic-bezier(0.16, 1, 0.3, 1) forwards;
          text-shadow: 
            0 0 10px rgba(255, 119, 0, 0.8),
            0 0 20px rgba(255, 119, 0, 0.6),
            0 0 40px rgba(255, 119, 0, 0.4),
            4px 4px 0 rgba(0, 0, 0, 0.5);
        }
        
        .word-arena {
          animation: slideFromRight 0.5s cubic-bezier(0.16, 1, 0.3, 1) forwards;
          text-shadow: 
            0 0 10px rgba(255, 68, 0, 0.8),
            0 0 20px rgba(255, 68, 0, 0.6),
            0 0 40px rgba(255, 68, 0, 0.4),
            4px 4px 0 rgba(0, 0, 0, 0.5);
        }
        
        .word-roll {
          animation: snapCenter 0.4s cubic-bezier(0.34, 1.56, 0.64, 1) forwards;
          text-shadow: 
            0 0 15px rgba(255, 255, 255, 0.9),
            0 0 30px rgba(255, 170, 0, 0.8),
            0 0 50px rgba(255, 119, 0, 0.6),
            4px 4px 0 rgba(0, 0, 0, 0.5);
        }
        
        .screen-shake {
          animation: screenShake 0.4s ease-out;
        }
        
        .radial-flare {
          animation: radialFlare 0.8s ease-out forwards;
        }
        
        .lens-flare {
          animation: lensFlare 1s ease-out forwards;
        }
        
        .settled-glow {
          animation: glowPulse 2s ease-in-out infinite;
        }
        
        .tap-prompt {
          animation: fadeInUp 0.6s ease-out forwards;
        }
        
        .glossy-text {
          background: linear-gradient(180deg, 
            #ffffff 0%, 
            #ffcc88 20%, 
            #ff7700 40%, 
            #ff4400 60%, 
            #cc3300 80%, 
            #992200 100%
          );
          -webkit-background-clip: text;
          background-clip: text;
          -webkit-text-fill-color: transparent;
          position: relative;
        }
        
        .glossy-text::before {
          content: attr(data-text);
          position: absolute;
          left: 0;
          top: 0;
          background: linear-gradient(180deg, 
            rgba(255,255,255,0.4) 0%, 
            rgba(255,255,255,0) 50%
          );
          -webkit-background-clip: text;
          background-clip: text;
          -webkit-text-fill-color: transparent;
        }
      `}</style>

      <canvas
        ref={canvasRef}
        className="absolute inset-0 pointer-events-none z-20"
      />

      {stage >= 3 && (
        <>
          <div className="absolute inset-0 flex items-center justify-center pointer-events-none z-10">
            <div className="w-32 h-32 rounded-full bg-gradient-radial from-white via-orange-400 to-transparent radial-flare" 
              style={{
                background: "radial-gradient(circle, rgba(255,255,255,0.9) 0%, rgba(255,170,0,0.6) 30%, rgba(255,119,0,0.3) 60%, transparent 100%)"
              }}
            />
          </div>
          
          <div className="absolute inset-0 flex items-center justify-center pointer-events-none z-10">
            <div 
              className="w-64 h-1 lens-flare"
              style={{
                background: "linear-gradient(90deg, transparent, rgba(255,255,255,0.8), transparent)",
                boxShadow: "0 0 20px rgba(255,170,0,0.5)"
              }}
            />
          </div>
        </>
      )}

      <div className={`flex flex-col items-center gap-0 z-30 ${stage >= 3 ? "screen-shake" : ""}`}>
        {stage >= 1 && (
          <div 
            className={`word-zoogi glossy-text font-black text-5xl sm:text-6xl md:text-7xl tracking-tight ${animationComplete ? "settled-glow" : ""}`}
            data-text="ZOOGI"
            style={{ fontFamily: "'Arial Black', sans-serif" }}
          >
            ZOOGI
          </div>
        )}
        
        {stage >= 3 && (
          <div 
            className={`word-roll glossy-text font-black text-4xl sm:text-5xl md:text-6xl tracking-widest ${animationComplete ? "settled-glow" : ""}`}
            data-text="ROLL"
            style={{ 
              fontFamily: "'Arial Black', sans-serif",
              marginTop: "-0.2em",
              marginBottom: "-0.2em"
            }}
          >
            ROLL
          </div>
        )}
        
        {stage >= 2 && (
          <div 
            className={`word-arena glossy-text font-black text-5xl sm:text-6xl md:text-7xl tracking-tight ${animationComplete ? "settled-glow" : ""}`}
            data-text="ARENA"
            style={{ fontFamily: "'Arial Black', sans-serif" }}
          >
            ARENA
          </div>
        )}
      </div>

      {showTapPrompt && (
        <div className="absolute bottom-24 left-0 right-0 flex justify-center z-30">
          <p className="tap-prompt text-white/80 text-lg font-medium tracking-wide animate-pulse">
            Tap anywhere to start
          </p>
        </div>
      )}
    </div>
  );
}
