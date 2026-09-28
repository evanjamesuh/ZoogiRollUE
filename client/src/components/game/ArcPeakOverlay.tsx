import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";

interface ArcPeakOverlayProps {
  isActive: boolean;
  onComplete: () => void;
  videos?: string[];
}

const BUBBLE_POSITIONS = [
  { x: "10%", y: "30%", rotation: -8 },
  { x: "35%", y: "25%", rotation: 3 },
  { x: "60%", y: "35%", rotation: -5 },
];

const PLACEHOLDER_EMOJIS = ["😱", "💥", "🎯"];

export function ArcPeakOverlay({ isActive, onComplete, videos }: ArcPeakOverlayProps) {
  const [currentIndex, setCurrentIndex] = useState(-1);
  const [showBubbles, setShowBubbles] = useState<boolean[]>([false, false, false]);
  const timerRef = useRef<NodeJS.Timeout | null>(null);

  useEffect(() => {
    if (isActive) {
      setCurrentIndex(0);
      setShowBubbles([false, false, false]);
      
      const sequence = async () => {
        await new Promise(r => setTimeout(r, 50));
        setShowBubbles([true, false, false]);
        
        await new Promise(r => setTimeout(r, 300));
        setShowBubbles([true, true, false]);
        
        await new Promise(r => setTimeout(r, 300));
        setShowBubbles([true, true, true]);
        
        await new Promise(r => setTimeout(r, 400));
        onComplete();
        setShowBubbles([false, false, false]);
        setCurrentIndex(-1);
      };
      
      sequence();
    }
    
    return () => {
      if (timerRef.current) {
        clearTimeout(timerRef.current);
      }
    };
  }, [isActive, onComplete]);

  if (!isActive && currentIndex === -1) return null;

  return (
    <div 
      className="fixed inset-0 pointer-events-none z-50"
      style={{ perspective: "1000px" }}
    >
      <AnimatePresence>
        {showBubbles.map((show, index) => (
          show && (
            <motion.div
              key={index}
              initial={{ scale: 0, opacity: 0, rotateY: -90 }}
              animate={{ scale: 1, opacity: 1, rotateY: 0 }}
              exit={{ scale: 0, opacity: 0, rotateY: 90 }}
              transition={{
                type: "spring",
                stiffness: 400,
                damping: 20,
                duration: 0.4
              }}
              className="absolute"
              style={{
                left: BUBBLE_POSITIONS[index].x,
                top: BUBBLE_POSITIONS[index].y,
                transform: `rotate(${BUBBLE_POSITIONS[index].rotation}deg)`,
              }}
            >
              <SpeechBubble 
                emoji={PLACEHOLDER_EMOJIS[index]}
                videoSrc={videos?.[index]}
              />
            </motion.div>
          )
        ))}
      </AnimatePresence>
    </div>
  );
}

interface SpeechBubbleProps {
  emoji: string;
  videoSrc?: string;
}

function SpeechBubble({ emoji, videoSrc }: SpeechBubbleProps) {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    if (videoRef.current && videoSrc) {
      videoRef.current.play().catch(() => {});
    }
  }, [videoSrc]);

  return (
    <div className="relative">
      <div 
        className="relative bg-white rounded-3xl p-2 shadow-2xl border-4 border-black"
        style={{
          width: "120px",
          height: "100px",
          clipPath: "polygon(0% 0%, 100% 0%, 100% 75%, 70% 75%, 50% 100%, 50% 75%, 0% 75%)",
        }}
      >
        <div 
          className="w-full h-full rounded-2xl overflow-hidden flex items-center justify-center bg-gradient-to-br from-pink-100 to-purple-100"
          style={{ height: "70px" }}
        >
          {videoSrc ? (
            <video 
              ref={videoRef}
              src={videoSrc}
              muted
              playsInline
              className="w-full h-full object-cover"
            />
          ) : (
            <span className="text-5xl animate-bounce">{emoji}</span>
          )}
        </div>
      </div>
      
      <div 
        className="absolute -bottom-2 left-1/2 transform -translate-x-1/2"
        style={{
          width: 0,
          height: 0,
          borderLeft: "15px solid transparent",
          borderRight: "15px solid transparent",
          borderTop: "20px solid white",
          filter: "drop-shadow(0 2px 2px rgba(0,0,0,0.3))"
        }}
      />
      
      <motion.div
        className="absolute -top-2 -right-2 w-6 h-6 bg-yellow-400 rounded-full flex items-center justify-center text-xs font-bold shadow-lg"
        animate={{ 
          scale: [1, 1.2, 1],
          rotate: [0, 10, -10, 0]
        }}
        transition={{ 
          duration: 0.5,
          repeat: Infinity,
          repeatDelay: 0.5
        }}
      >
        ⚡
      </motion.div>
    </div>
  );
}

export default ArcPeakOverlay;
