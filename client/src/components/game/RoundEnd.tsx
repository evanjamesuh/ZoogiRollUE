import { motion, AnimatePresence } from "framer-motion";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useState, useEffect, useMemo } from "react";
import Confetti from "react-confetti";
import { Trophy, Star, Zap, Target, Award } from "lucide-react";

function useWindowSize() {
  const [size, setSize] = useState({ width: 0, height: 0 });
  
  useEffect(() => {
    function updateSize() {
      setSize({ width: window.innerWidth, height: window.innerHeight });
    }
    updateSize();
    window.addEventListener("resize", updateSize);
    return () => window.removeEventListener("resize", updateSize);
  }, []);
  
  return size;
}

const celebrationEmojis = ["🎯", "🏆", "⭐", "🎮", "🔥", "💫", "✨"];

const precomputedParticles = [
  { x: 15, duration: 2.3 },
  { x: 32, duration: 3.1 },
  { x: 48, duration: 2.7 },
  { x: 65, duration: 3.5 },
  { x: 78, duration: 2.9 },
  { x: 25, duration: 3.2 },
  { x: 55, duration: 2.5 },
  { x: 88, duration: 3.8 },
];

function FloatingParticle({ delay, emoji, particleIndex }: { delay: number; emoji: string; particleIndex: number }) {
  const particle = precomputedParticles[particleIndex % precomputedParticles.length];
  
  return (
    <motion.div
      initial={{ opacity: 0, y: 100, x: `${particle.x}vw` }}
      animate={{ 
        opacity: [0, 1, 1, 0], 
        y: [-20, -200],
        rotate: [0, 360]
      }}
      transition={{ 
        delay, 
        duration: particle.duration,
        ease: "easeOut"
      }}
      className="absolute text-3xl pointer-events-none"
      style={{ left: 0 }}
    >
      {emoji}
    </motion.div>
  );
}

function StatBar({ label, value, color, delay }: { label: string; value: number; color: string; delay: number }) {
  return (
    <motion.div
      initial={{ opacity: 0, x: -30 }}
      animate={{ opacity: 1, x: 0 }}
      transition={{ delay, duration: 0.4 }}
      className="flex items-center justify-between gap-4 bg-white/10 rounded-lg px-4 py-2"
    >
      <span className="text-white/70 text-sm">{label}</span>
      <motion.span
        initial={{ scale: 0 }}
        animate={{ scale: 1 }}
        transition={{ delay: delay + 0.2, type: "spring", stiffness: 200 }}
        className={`font-bold text-lg ${color}`}
      >
        {value}
      </motion.span>
    </motion.div>
  );
}

export function RoundEnd() {
  const { currentRound, maxRounds, score, playerEntity, enemies, startNextRound } = useZoogiGame();
  const [showConfetti, setShowConfetti] = useState(true);
  const [countdown, setCountdown] = useState(5);
  const [autoStarting, setAutoStarting] = useState(false);
  const windowSize = useWindowSize();
  
  useEffect(() => {
    const confettiTimer = setTimeout(() => setShowConfetti(false), 4000);
    return () => clearTimeout(confettiTimer);
  }, []);
  
  useEffect(() => {
    if (countdown > 0 && autoStarting) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else if (countdown === 0 && autoStarting) {
      startNextRound();
    }
  }, [countdown, autoStarting, startNextRound]);
  
  const handleStartNext = () => {
    setAutoStarting(true);
  };
  
  const knockouts = (playerEntity as any)?.knockouts || 0;
  const orbsCollected = (playerEntity as any)?.orbsCollected || 0;
  
  const isWinning = enemies.every(e => score > (e.score || 0));

  return (
    <div className="absolute inset-0 bg-black/85 backdrop-blur-sm flex items-center justify-center z-50 overflow-hidden">
      {showConfetti && (
        <Confetti
          width={windowSize.width || 800}
          height={windowSize.height || 600}
          numberOfPieces={200}
          recycle={false}
          gravity={0.2}
          colors={['#FFD700', '#FF6B6B', '#4ECDC4', '#45B7D1', '#96E6A1', '#DDA0DD']}
        />
      )}
      
      {[...Array(8)].map((_, i) => (
        <FloatingParticle 
          key={i} 
          delay={i * 0.3} 
          emoji={celebrationEmojis[i % celebrationEmojis.length]}
          particleIndex={i}
        />
      ))}
      
      <motion.div
        initial={{ scale: 0, rotate: -15, opacity: 0 }}
        animate={{ scale: 1, rotate: 0, opacity: 1 }}
        transition={{ type: "spring", stiffness: 200, damping: 15 }}
        className="relative"
      >
        <motion.div
          animate={{ 
            boxShadow: [
              "0 0 20px rgba(255, 215, 0, 0.3)",
              "0 0 60px rgba(255, 215, 0, 0.6)",
              "0 0 20px rgba(255, 215, 0, 0.3)"
            ]
          }}
          transition={{ duration: 2, repeat: Infinity }}
          className="bg-gradient-to-br from-indigo-900 via-purple-900 to-indigo-900 p-8 rounded-3xl text-center border-4 border-yellow-400/80 max-w-md mx-4"
        >
          <motion.div
            initial={{ y: -30, opacity: 0 }}
            animate={{ y: 0, opacity: 1 }}
            transition={{ delay: 0.2 }}
            className="flex items-center justify-center gap-2 mb-2"
          >
            <Star className="w-6 h-6 text-yellow-400" />
            <span className="text-yellow-400/80 text-sm font-medium uppercase tracking-wider">
              Round Complete
            </span>
            <Star className="w-6 h-6 text-yellow-400" />
          </motion.div>
          
          <motion.h1
            initial={{ scale: 0.5, opacity: 0 }}
            animate={{ scale: 1, opacity: 1 }}
            transition={{ delay: 0.3, type: "spring", stiffness: 150 }}
            className="text-5xl font-black text-transparent bg-clip-text bg-gradient-to-r from-yellow-300 via-yellow-400 to-orange-400 mb-4"
          >
            Round {currentRound}
          </motion.h1>
          
          <motion.div
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            transition={{ delay: 0.4, type: "spring" }}
            className="relative mb-6"
          >
            <motion.div
              animate={{ rotate: [0, 10, -10, 0] }}
              transition={{ delay: 0.6, duration: 0.5 }}
              className="text-7xl"
            >
              {isWinning ? "🏆" : "🎯"}
            </motion.div>
            {isWinning && (
              <motion.div
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 0.8 }}
                className="absolute -bottom-2 left-1/2 -translate-x-1/2 bg-green-500 text-white text-xs font-bold px-3 py-1 rounded-full"
              >
                LEADING!
              </motion.div>
            )}
          </motion.div>
          
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.5 }}
            className="bg-black/30 rounded-2xl p-4 mb-6"
          >
            <div className="flex items-center justify-center gap-2 mb-3">
              <Trophy className="w-5 h-5 text-yellow-400" />
              <span className="text-white/90 font-semibold">Your Score</span>
            </div>
            <motion.p
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ delay: 0.6, type: "spring", stiffness: 200 }}
              className="text-5xl font-black text-yellow-400 mb-4"
            >
              {score}
            </motion.p>
            
            <div className="space-y-2">
              <StatBar label="Knockouts" value={knockouts} color="text-red-400" delay={0.7} />
              <StatBar label="Orbs Collected" value={orbsCollected} color="text-cyan-400" delay={0.8} />
            </div>
          </motion.div>
          
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 0.9 }}
            className="flex items-center justify-center gap-2 text-white/60 mb-6"
          >
            <Target className="w-4 h-4" />
            <span className="text-sm">
              Round {currentRound} of {maxRounds}
            </span>
          </motion.div>
          
          <AnimatePresence mode="wait">
            {!autoStarting ? (
              <motion.button
                key="start-button"
                initial={{ y: 30, opacity: 0 }}
                animate={{ y: 0, opacity: 1 }}
                exit={{ scale: 0.8, opacity: 0 }}
                transition={{ delay: 1 }}
                onClick={handleStartNext}
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
                className="relative px-10 py-4 bg-gradient-to-r from-green-400 via-emerald-500 to-green-400 text-black font-bold text-xl rounded-full shadow-lg shadow-green-500/30 overflow-hidden group"
              >
                <motion.div
                  className="absolute inset-0 bg-gradient-to-r from-transparent via-white/30 to-transparent"
                  animate={{ x: ["-100%", "100%"] }}
                  transition={{ duration: 2, repeat: Infinity, repeatDelay: 1 }}
                />
                <span className="relative flex items-center gap-2">
                  <Zap className="w-5 h-5" />
                  Start Round {currentRound + 1}
                </span>
              </motion.button>
            ) : (
              <motion.div
                key="countdown"
                initial={{ scale: 0.8, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                className="text-center"
              >
                <p className="text-white/70 text-sm mb-2">Starting in...</p>
                <motion.div
                  key={countdown}
                  initial={{ scale: 1.5, opacity: 0 }}
                  animate={{ scale: 1, opacity: 1 }}
                  className="text-6xl font-black text-green-400"
                >
                  {countdown}
                </motion.div>
              </motion.div>
            )}
          </AnimatePresence>
        </motion.div>
        
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 1.2 }}
          className="absolute -top-4 -left-4"
        >
          <Award className="w-12 h-12 text-yellow-400 drop-shadow-lg" />
        </motion.div>
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 1.3 }}
          className="absolute -top-4 -right-4"
        >
          <Award className="w-12 h-12 text-yellow-400 drop-shadow-lg transform -scale-x-100" />
        </motion.div>
      </motion.div>
    </div>
  );
}
