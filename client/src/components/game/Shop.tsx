import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { motion, AnimatePresence } from "framer-motion";
import { ArrowLeft, ShoppingBag, Crown, Sparkles, Lock, Check, X, Zap, Shield, Target, Move, Calendar, ChevronRight } from "lucide-react";
import { useState, useEffect, useMemo, useRef } from "react";
import { PREMIUM_CHARACTERS, CHARACTER_BUNDLE, type PremiumZoogi } from "@/lib/premiumCharacters";
import { Canvas, useFrame } from "@react-three/fiber";
import { OrbitControls } from "@react-three/drei";
import * as THREE from "three";
import { getSpotlightZoogi, getTimeRemaining, ZOOGI_LORE, getRarityColor, getRarityGradient } from "./ZoogiSpotlight";
import { VideoBackground } from "./VideoBackground";

function WeeklySpotlightBanner() {
  const spotlightData = useMemo(() => getSpotlightZoogi('weekly'), []);
  const timeRemaining = useMemo(() => getTimeRemaining('weekly'), []);
  const lore = ZOOGI_LORE[spotlightData.zoogi.id];
  const rarity = lore?.rarity || 'Common';
  
  return (
    <motion.div
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      className={`mb-4 p-4 rounded-2xl bg-gradient-to-r ${getRarityGradient(rarity)} border border-white/20 relative overflow-hidden`}
    >
      <div className="absolute inset-0 bg-[url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI1MCIgaGVpZ2h0PSI1MCI+PGNpcmNsZSBjeD0iMjUiIGN5PSIyNSIgcj0iMSIgZmlsbD0icmdiYSgyNTUsMjU1LDI1NSwwLjEpIi8+PC9zdmc+')] opacity-30" />
      <div className="relative flex items-center gap-4">
        <div className="flex items-center gap-2 text-white/70">
          <Calendar size={16} />
          <span className="text-sm font-medium">Weekly Spotlight</span>
        </div>
        <div className="flex items-center gap-3 flex-1">
          <div 
            className="w-10 h-10 rounded-full flex-shrink-0 shadow-lg"
            style={{ backgroundColor: spotlightData.zoogi.color }}
          />
          <div className="flex-1 min-w-0">
            <div className="flex items-center gap-2">
              <span className="text-white font-bold">{spotlightData.zoogi.name}</span>
              <span className={`text-xs ${getRarityColor(rarity)}`}>{rarity}</span>
              {spotlightData.isPremium && <Sparkles size={14} className="text-yellow-400" />}
            </div>
            <p className="text-white/50 text-xs">{timeRemaining} remaining</p>
          </div>
        </div>
        <ChevronRight className="text-white/40" size={20} />
      </div>
    </motion.div>
  );
}


function getDeviceId(): string {
  const storageKey = "zoogi_device_id";
  let deviceId = localStorage.getItem(storageKey);
  if (!deviceId) {
    deviceId = `device_${Date.now()}_${Math.random().toString(36).substring(2, 15)}`;
    localStorage.setItem(storageKey, deviceId);
  }
  return deviceId;
}

function RotatingZoogi({ color, accentColor }: { color: string; accentColor: string }) {
  const groupRef = useRef<THREE.Group>(null);
  
  useFrame((_, delta) => {
    if (groupRef.current) {
      groupRef.current.rotation.y += delta * 0.8;
    }
  });

  return (
    <group ref={groupRef}>
      <mesh position={[0, 0, 0]}>
        <sphereGeometry args={[1, 32, 32]} />
        <meshStandardMaterial color={color} />
      </mesh>
      <mesh position={[0.3, 0.4, 0.7]}>
        <sphereGeometry args={[0.25, 16, 16]} />
        <meshStandardMaterial color="white" />
      </mesh>
      <mesh position={[-0.3, 0.4, 0.7]}>
        <sphereGeometry args={[0.25, 16, 16]} />
        <meshStandardMaterial color="white" />
      </mesh>
      <mesh position={[0.3, 0.4, 0.85]}>
        <sphereGeometry args={[0.12, 16, 16]} />
        <meshStandardMaterial color="black" />
      </mesh>
      <mesh position={[-0.3, 0.4, 0.85]}>
        <sphereGeometry args={[0.12, 16, 16]} />
        <meshStandardMaterial color="black" />
      </mesh>
      <mesh position={[0, -0.1, 0.9]}>
        <sphereGeometry args={[0.15, 16, 16]} />
        <meshStandardMaterial color={accentColor} />
      </mesh>
    </group>
  );
}

function StatBar({ label, value, icon: Icon, color }: { label: string; value: number; icon: any; color: string }) {
  return (
    <div className="flex items-center gap-3 mb-3">
      <div className="w-8 h-8 rounded-lg flex items-center justify-center" style={{ backgroundColor: `${color}30` }}>
        <Icon size={18} style={{ color }} />
      </div>
      <div className="flex-1">
        <div className="flex justify-between items-center mb-1">
          <span className="text-white/80 text-sm font-medium">{label}</span>
          <span className="text-white font-bold">{value}/10</span>
        </div>
        <div className="h-2 bg-white/10 rounded-full overflow-hidden">
          <motion.div 
            initial={{ width: 0 }}
            animate={{ width: `${value * 10}%` }}
            transition={{ duration: 0.5, delay: 0.2 }}
            className="h-full rounded-full"
            style={{ backgroundColor: color }}
          />
        </div>
      </div>
    </div>
  );
}

interface CharacterModalProps {
  character: PremiumZoogi;
  isOwned: boolean;
  onClose: () => void;
}

function CharacterModal({ character, isOwned, onClose }: CharacterModalProps) {
  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80"
      onClick={onClose}
    >
      <motion.div
        initial={{ scale: 0.9, opacity: 0 }}
        animate={{ scale: 1, opacity: 1 }}
        exit={{ scale: 0.9, opacity: 0 }}
        onClick={(e) => e.stopPropagation()}
        className="w-full max-w-md bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl overflow-hidden border border-white/20"
      >
        <div className="relative h-64 bg-gradient-to-b from-black/50 to-transparent">
          <Canvas camera={{ position: [0, 0.5, 4], fov: 45 }}>
            <ambientLight intensity={0.5} />
            <directionalLight position={[5, 5, 5]} intensity={1} />
            <directionalLight position={[-5, 3, -5]} intensity={0.5} />
            <RotatingZoogi color={character.color} accentColor={character.accentColor} />
            <OrbitControls enableZoom={false} enablePan={false} />
          </Canvas>
          
          <button
            onClick={onClose}
            className="absolute top-4 right-4 w-10 h-10 rounded-full bg-black/50 text-white flex items-center justify-center hover:bg-black/70 transition-colors"
          >
            <X size={24} />
          </button>
          
          {isOwned && (
            <div className="absolute top-4 left-4 px-3 py-1 bg-green-500 text-white text-sm font-bold rounded-full flex items-center gap-1">
              <Check size={16} /> Owned
            </div>
          )}
        </div>
        
        <div className="p-6">
          <div className="text-center mb-6">
            <h2 className="text-2xl font-bold text-white mb-1">{character.name}</h2>
            <p className="text-white/60 text-sm">{character.description}</p>
          </div>
          
          <div className="bg-white/5 rounded-xl p-4 mb-6">
            <div className="flex items-center gap-2 mb-3">
              <Sparkles size={18} className="text-yellow-400" />
              <span className="text-white font-semibold">{character.ability}</span>
            </div>
            <p className="text-white/70 text-sm">{character.abilityDescription}</p>
          </div>
          
          <div className="mb-6">
            <h3 className="text-white/80 font-semibold mb-4">Stats</h3>
            <StatBar label="Speed" value={character.stats.speed} icon={Move} color="#3B82F6" />
            <StatBar label="Power" value={character.stats.power} icon={Zap} color="#EF4444" />
            <StatBar label="Defense" value={character.stats.defense} icon={Shield} color="#22C55E" />
            <StatBar label="Control" value={character.stats.control} icon={Target} color="#A855F7" />
          </div>
          
          {!isOwned && (
            <div className="w-full py-4 rounded-xl font-bold text-lg text-center bg-gray-600 text-gray-300">
              Coming Soon
            </div>
          )}
        </div>
      </motion.div>
    </motion.div>
  );
}

export function Shop() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const [ownedCharacters, setOwnedCharacters] = useState<string[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedCharacter, setSelectedCharacter] = useState<PremiumZoogi | null>(null);
  
  const deviceId = useMemo(() => getDeviceId(), []);

  useEffect(() => {
    fetch(`/api/purchases?deviceId=${encodeURIComponent(deviceId)}`)
      .then(res => res.json())
      .then(purchasesData => {
        const owned = (purchasesData.data || []).map((p: any) => p.characterId);
        setOwnedCharacters(owned);
      })
      .catch(console.error)
      .finally(() => setLoading(false));
  }, [deviceId]);

  const isOwned = (characterId: string): boolean => {
    return ownedCharacters.includes(characterId) || ownedCharacters.includes("monster-pack");
  };

  return (
    <div className="fixed inset-0 flex flex-col overflow-hidden">
      <VideoBackground />
      <div className="relative z-10 flex items-center justify-between p-4 border-b border-white/10">
        <motion.button
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
          onClick={() => setPhase("menu")}
          className="flex items-center gap-2 px-4 py-2 bg-white/10 text-white rounded-full hover:bg-white/20 transition-colors"
        >
          <ArrowLeft size={20} />
          Back
        </motion.button>
        
        <h1 className="text-2xl font-bold text-white flex items-center gap-2">
          <ShoppingBag size={24} className="text-yellow-400" />
          Character Shop
        </h1>
        
        <div className="w-24" />
      </div>

      <div className="flex-1 overflow-y-auto p-4">
        {loading ? (
          <div className="flex items-center justify-center h-full">
            <motion.div
              animate={{ rotate: 360 }}
              transition={{ duration: 1, repeat: Infinity, ease: "linear" }}
              className="w-12 h-12 border-4 border-white/20 border-t-yellow-400 rounded-full"
            />
          </div>
        ) : (
          <>
            <WeeklySpotlightBanner />
            
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              className="mb-8 p-6 bg-gradient-to-r from-yellow-500/20 via-orange-500/20 to-red-500/20 rounded-2xl border border-yellow-500/30"
            >
              <div className="flex items-center gap-3 mb-4">
                <Crown size={32} className="text-yellow-400" />
                <div>
                  <h2 className="text-xl font-bold text-white">{CHARACTER_BUNDLE.name}</h2>
                  <p className="text-white/70 text-sm">{CHARACTER_BUNDLE.description}</p>
                </div>
              </div>
              
              {(() => {
                const bundleOwned = isOwned("monster-pack");
                
                return (
                  <div
                    className={`w-full py-3 rounded-xl font-bold text-lg text-center transition-all ${
                      bundleOwned
                        ? "bg-green-600 text-white"
                        : "bg-gray-600 text-gray-300"
                    }`}
                  >
                    {bundleOwned ? (
                      <span className="flex items-center justify-center gap-2">
                        <Check size={20} /> Owned
                      </span>
                    ) : (
                      "Coming Soon"
                    )}
                  </div>
                );
              })()}
            </motion.div>

            <h3 className="text-lg font-semibold text-white/80 mb-4 flex items-center gap-2">
              <Sparkles size={20} className="text-purple-400" />
              Individual Characters
            </h3>

            <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
              {PREMIUM_CHARACTERS.map((character, index) => {
                const owned = isOwned(character.id);
                
                return (
                  <motion.div
                    key={character.id}
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ delay: index * 0.05 }}
                    onClick={() => setSelectedCharacter(character)}
                    className={`relative p-4 rounded-xl border transition-all cursor-pointer ${
                      owned
                        ? "bg-green-900/20 border-green-500/30"
                        : "bg-white/5 border-white/10 hover:bg-white/10 hover:border-white/20"
                    }`}
                  >
                    <div 
                      className="w-16 h-16 mx-auto rounded-full mb-3 flex items-center justify-center"
                      style={{ backgroundColor: character.color }}
                    >
                      <div 
                        className="w-8 h-8 rounded-full"
                        style={{ backgroundColor: character.accentColor }}
                      />
                    </div>
                    
                    <h4 className="text-white font-bold text-center mb-1">{character.name}</h4>
                    <p className="text-white/50 text-xs text-center mb-2">{character.ability}</p>
                    
                    <div className="flex justify-center gap-1 mb-3">
                      {Object.entries(character.stats).map(([stat, value]) => (
                        <div 
                          key={stat} 
                          className="w-1.5 rounded-full bg-white/20"
                          style={{ height: `${value * 3}px` }}
                          title={`${stat}: ${value}`}
                        />
                      ))}
                    </div>
                    
                    <div
                      className={`w-full py-2 rounded-lg font-semibold text-sm text-center ${
                        owned
                          ? "bg-green-600/50 text-white"
                          : "bg-gray-600 text-gray-300"
                      }`}
                    >
                      {owned ? (
                        <span className="flex items-center justify-center gap-1">
                          <Check size={14} /> Owned
                        </span>
                      ) : (
                        <span className="flex items-center justify-center gap-1">
                          <Lock size={14} /> Soon
                        </span>
                      )}
                    </div>
                  </motion.div>
                );
              })}
            </div>
          </>
        )}
      </div>
      
      <AnimatePresence>
        {selectedCharacter && (
          <CharacterModal
            character={selectedCharacter}
            isOwned={isOwned(selectedCharacter.id)}
            onClose={() => setSelectedCharacter(null)}
          />
        )}
      </AnimatePresence>
    </div>
  );
}
