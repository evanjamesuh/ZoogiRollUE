import { useZoogiGame, ZOOGI_ROSTER, Zoogi, LocalPlayer } from "@/lib/stores/useZoogiGame";
import { motion, AnimatePresence } from "framer-motion";
import { useState, useEffect } from "react";
import { ArrowLeft, Users, Check, ChevronLeft, ChevronRight } from "lucide-react";

const PORTRAIT_IMAGES: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  pinpoint: "/portraits/pinpoint.png",
  bolt: "/portraits/bolt.png",
  wraps: "/portraits/wraps.png",
};

function ZoogiCard({ 
  zoogi, 
  isSelected, 
  isDisabled,
  onClick 
}: { 
  zoogi: Zoogi; 
  isSelected: boolean; 
  isDisabled: boolean;
  onClick: () => void;
}) {
  const hasPortrait = PORTRAIT_IMAGES[zoogi.id];
  
  return (
    <motion.button
      whileHover={{ scale: isDisabled ? 1 : 1.05 }}
      whileTap={{ scale: isDisabled ? 1 : 0.95 }}
      onClick={onClick}
      disabled={isDisabled}
      className={`relative p-3 rounded-xl transition-all ${
        isSelected 
          ? "bg-gradient-to-b from-white/30 to-white/10 ring-3 ring-yellow-400" 
          : isDisabled 
            ? "bg-white/5 opacity-50 cursor-not-allowed"
            : "bg-white/10 hover:bg-white/20"
      }`}
    >
      {hasPortrait ? (
        <div className="w-14 h-14 rounded-full mx-auto mb-1 shadow-lg overflow-hidden border-2 border-white/30">
          <img 
            src={PORTRAIT_IMAGES[zoogi.id]} 
            alt={zoogi.name}
            className="w-full h-full object-cover"
            style={zoogi.id === "wraps" ? { transform: "scale(1.4)", transformOrigin: "center 45%" } : undefined}
          />
        </div>
      ) : (
        <div
          className="w-14 h-14 rounded-full mx-auto mb-1 shadow-lg"
          style={{
            background: `radial-gradient(circle at 30% 30%, ${zoogi.secondaryColor}, ${zoogi.color})`,
          }}
        />
      )}
      <h3 className="text-white font-bold text-center text-sm">{zoogi.name}</h3>
      
      {isSelected && (
        <motion.div
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          className="absolute -top-1 -right-1 w-6 h-6 bg-yellow-400 rounded-full flex items-center justify-center"
        >
          <Check size={14} className="text-black" />
        </motion.div>
      )}
    </motion.button>
  );
}

function PlayerSetupCard({ 
  player, 
  playerIndex, 
  isActive, 
  usedZoogis,
  onSelectZoogi,
  onNameChange
}: { 
  player: LocalPlayer;
  playerIndex: number;
  isActive: boolean;
  usedZoogis: string[];
  onSelectZoogi: (zoogi: Zoogi) => void;
  onNameChange: (name: string) => void;
}) {
  const playerColors = ["#3B82F6", "#EF4444", "#22C55E", "#A855F7"];
  
  return (
    <motion.div
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      className={`bg-black/40 rounded-2xl p-4 backdrop-blur-sm transition-all ${
        isActive ? "ring-2 ring-yellow-400" : ""
      }`}
    >
      <div className="flex items-center gap-3 mb-3">
        <div 
          className="w-10 h-10 rounded-full flex items-center justify-center text-white font-bold"
          style={{ backgroundColor: playerColors[playerIndex] }}
        >
          {playerIndex + 1}
        </div>
        <input
          type="text"
          value={player.name}
          onChange={(e) => onNameChange(e.target.value)}
          className="flex-1 bg-white/10 text-white px-3 py-2 rounded-lg text-sm font-medium focus:outline-none focus:ring-2 focus:ring-yellow-400"
          placeholder={`Player ${playerIndex + 1}`}
        />
      </div>
      
      {isActive && (
        <motion.div
          initial={{ opacity: 0, height: 0 }}
          animate={{ opacity: 1, height: "auto" }}
          className="grid grid-cols-5 gap-2"
        >
          {ZOOGI_ROSTER.map((zoogi) => (
            <ZoogiCard
              key={zoogi.id}
              zoogi={zoogi}
              isSelected={player.zoogi?.id === zoogi.id}
              isDisabled={usedZoogis.includes(zoogi.id) && player.zoogi?.id !== zoogi.id}
              onClick={() => onSelectZoogi(zoogi)}
            />
          ))}
        </motion.div>
      )}
      
      {!isActive && player.zoogi && (
        <div className="flex items-center gap-3 p-2 bg-white/10 rounded-lg">
          {PORTRAIT_IMAGES[player.zoogi.id] ? (
            <div className="w-10 h-10 rounded-full shadow-lg overflow-hidden border-2 border-white/30">
              <img 
                src={PORTRAIT_IMAGES[player.zoogi.id]} 
                alt={player.zoogi.name}
                className="w-full h-full object-cover"
                style={player.zoogi.id === "wraps" ? { transform: "scale(1.4)", transformOrigin: "center 45%" } : undefined}
              />
            </div>
          ) : (
            <div
              className="w-10 h-10 rounded-full shadow-lg"
              style={{
                background: `radial-gradient(circle at 30% 30%, ${player.zoogi.secondaryColor}, ${player.zoogi.color})`,
              }}
            />
          )}
          <div>
            <p className="text-white font-semibold">{player.zoogi.name}</p>
            <p className="text-white/60 text-xs">{player.zoogi.type}</p>
          </div>
          <Check className="ml-auto text-green-400" size={20} />
        </div>
      )}
      
      {!isActive && !player.zoogi && (
        <div className="p-2 bg-white/10 rounded-lg text-white/50 text-center text-sm">
          Waiting to select...
        </div>
      )}
    </motion.div>
  );
}

export function LocalPlayerSetup() {
  const { 
    localPlayers, 
    localPlayerCount, 
    setLocalPlayerCount,
    setLocalPlayerZoogi,
    setLocalPlayerName,
    setPhase,
    startLocalGame
  } = useZoogiGame();
  
  const [activePlayerIndex, setActivePlayerIndex] = useState(0);
  
  useEffect(() => {
    if (localPlayers.length === 0) {
      setLocalPlayerCount(2);
    }
  }, [localPlayers.length, setLocalPlayerCount]);
  
  const usedZoogis = localPlayers
    .filter((p, i) => i !== activePlayerIndex && p.zoogi)
    .map(p => p.zoogi!.id);
  
  const allPlayersReady = localPlayers.every(p => p.zoogi !== null);
  
  const handleSelectZoogi = (zoogi: Zoogi) => {
    setLocalPlayerZoogi(activePlayerIndex, zoogi);
    if (activePlayerIndex < localPlayers.length - 1) {
      setTimeout(() => setActivePlayerIndex(activePlayerIndex + 1), 300);
    }
  };
  
  const handlePlayerCountChange = (delta: number) => {
    const newCount = Math.min(4, Math.max(2, localPlayerCount + delta));
    if (newCount !== localPlayerCount) {
      setLocalPlayerCount(newCount);
      setActivePlayerIndex(0);
    }
  };
  
  const handleStartGame = () => {
    if (allPlayersReady) {
      useZoogiGame.getState().setGameMode("local_multiplayer");
      setPhase("map_selection");
    }
  };

  return (
    <div className="fixed inset-0 flex flex-col overflow-hidden">
      <div className="relative z-10 p-4 flex items-center gap-4">
        <motion.button
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
          onClick={() => setPhase("menu")}
          className="p-2 bg-white/10 rounded-full hover:bg-white/20"
        >
          <ArrowLeft size={24} className="text-white" />
        </motion.button>
        
        <div className="flex items-center gap-2">
          <Users className="text-blue-400" size={28} />
          <h1 className="text-2xl md:text-3xl font-bold text-transparent bg-clip-text bg-gradient-to-r from-blue-400 to-cyan-400">
            Local Multiplayer
          </h1>
        </div>
      </div>

      <div className="relative z-10 px-4 mb-4">
        <div className="flex items-center justify-center gap-4 bg-black/30 rounded-xl p-3">
          <span className="text-white/70">Players:</span>
          <button
            onClick={() => handlePlayerCountChange(-1)}
            disabled={localPlayerCount <= 2}
            className="p-2 bg-white/10 rounded-full hover:bg-white/20 disabled:opacity-30 disabled:cursor-not-allowed"
          >
            <ChevronLeft size={20} className="text-white" />
          </button>
          <span className="text-white font-bold text-2xl w-8 text-center">{localPlayerCount}</span>
          <button
            onClick={() => handlePlayerCountChange(1)}
            disabled={localPlayerCount >= 4}
            className="p-2 bg-white/10 rounded-full hover:bg-white/20 disabled:opacity-30 disabled:cursor-not-allowed"
          >
            <ChevronRight size={20} className="text-white" />
          </button>
        </div>
      </div>

      <div className="flex-1 overflow-y-auto px-4 pb-4">
        <div className="space-y-4">
          {localPlayers.map((player, index) => (
            <div key={player.id} onClick={() => setActivePlayerIndex(index)}>
              <PlayerSetupCard
                player={player}
                playerIndex={index}
                isActive={activePlayerIndex === index}
                usedZoogis={usedZoogis}
                onSelectZoogi={handleSelectZoogi}
                onNameChange={(name) => setLocalPlayerName(index, name)}
              />
            </div>
          ))}
        </div>
      </div>

      <div className="relative z-10 p-4 flex gap-4">
        <motion.button
          whileHover={{ scale: 1.02 }}
          whileTap={{ scale: 0.98 }}
          onClick={() => setPhase("menu")}
          className="flex-1 py-3 bg-white/10 text-white font-semibold rounded-full hover:bg-white/20"
        >
          Back
        </motion.button>
        
        <motion.button
          whileHover={{ scale: allPlayersReady ? 1.02 : 1 }}
          whileTap={{ scale: allPlayersReady ? 0.98 : 1 }}
          onClick={handleStartGame}
          disabled={!allPlayersReady}
          className={`flex-1 py-3 font-bold rounded-full transition-all ${
            allPlayersReady
              ? "bg-gradient-to-r from-green-500 to-emerald-600 text-white shadow-lg shadow-green-500/50"
              : "bg-white/20 text-white/50 cursor-not-allowed"
          }`}
        >
          {allPlayersReady ? "Choose Arena →" : `Select All Zoogis (${localPlayers.filter(p => p.zoogi).length}/${localPlayerCount})`}
        </motion.button>
      </div>
    </div>
  );
}
