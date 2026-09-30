import { useZoogiGame, ZOOGI_ROSTER, Zoogi, LocalPlayer } from "@/lib/stores/useZoogiGame";
import { motion } from "framer-motion";
import { useState, useEffect, useCallback } from "react";
import { Check, ChevronLeft, ChevronRight } from "lucide-react";
import { useMenuKeys } from "@/hooks/useMenuKeys";
import { returnToMenuScreen } from "@/lib/menuReturn";

const PORTRAIT_IMAGES: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  pinpoint: "/portraits/pinpoint.png",
  bolt: "/portraits/bolt.png",
  wraps: "/portraits/wraps.png",
  lars: "/portraits/lars.png",
  nightshade: "/portraits/nightshade.png",
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
      <h3 className="w-full truncate text-center text-xs font-bold text-white sm:text-sm">{zoogi.name}</h3>
      
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
          className="min-h-11 flex-1 rounded-lg bg-white/10 px-3 py-2 text-sm font-medium text-white focus:outline-none focus:ring-2 focus:ring-yellow-400"
          placeholder={`Player ${playerIndex + 1}`}
        />
      </div>
      
      {isActive && (
        <motion.div
          initial={{ opacity: 0, height: 0 }}
          animate={{ opacity: 1, height: "auto" }}
          className="grid grid-cols-3 gap-2 sm:grid-cols-4 lg:grid-cols-7"
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
  
  const goBack = useCallback(() => {
    returnToMenuScreen("play");
    setPhase("menu");
  }, [setPhase]);

  const handleStartGame = useCallback(() => {
    if (!allPlayersReady) return;
    useZoogiGame.getState().setGameMode("local_multiplayer");
    setPhase("map_selection");
  }, [allPlayersReady, setPhase]);

  useMenuKeys({ onBack: goBack, onConfirm: handleStartGame });

  return (
    <div className="menu-safe fixed inset-0 flex flex-col overflow-hidden" data-testid="local-setup">
      <div className="relative z-10 flex items-center gap-3 pb-3">
        <button
          type="button"
          onClick={goBack}
          className="inline-flex min-h-11 items-center gap-1 rounded-full bg-white/10 px-4 font-semibold text-white hover:bg-white/20"
        >
          <ChevronLeft size={20} />
          Back
        </button>
        
        <div className="min-w-0">
          <h1 className="bg-gradient-to-r from-blue-400 to-cyan-400 bg-clip-text text-2xl font-bold text-transparent md:text-3xl">
            Choose Your Zoogis
          </h1>
          <p className="text-sm text-white/60">Local multiplayer on this device</p>
        </div>
      </div>

      <div className="relative z-10 mb-4">
        <div className="mx-auto flex max-w-md items-center justify-center gap-4 rounded-xl bg-black/30 p-3">
          <span className="text-white/70">Players:</span>
          <button
            onClick={() => handlePlayerCountChange(-1)}
            disabled={localPlayerCount <= 2}
            aria-label="Fewer players"
            className="flex h-11 w-11 items-center justify-center rounded-full bg-white/10 hover:bg-white/20 disabled:cursor-not-allowed disabled:opacity-30"
          >
            <ChevronLeft size={20} className="text-white" />
          </button>
          <span className="w-8 text-center text-2xl font-bold text-white">{localPlayerCount}</span>
          <button
            onClick={() => handlePlayerCountChange(1)}
            disabled={localPlayerCount >= 4}
            aria-label="More players"
            className="flex h-11 w-11 items-center justify-center rounded-full bg-white/10 hover:bg-white/20 disabled:cursor-not-allowed disabled:opacity-30"
          >
            <ChevronRight size={20} className="text-white" />
          </button>
        </div>
      </div>

      <div className="flex-1 overflow-y-auto pb-4">
        <div className="mx-auto grid w-full max-w-6xl gap-4 lg:grid-cols-2">
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

      <div className="relative z-10 flex gap-3 pt-2">
        <button
          type="button"
          onClick={goBack}
          className="min-h-11 flex-1 rounded-full bg-white/10 font-semibold text-white hover:bg-white/20"
        >
          Back
        </button>
        
        <motion.button
          whileHover={{ scale: allPlayersReady ? 1.02 : 1 }}
          whileTap={{ scale: allPlayersReady ? 0.98 : 1 }}
          onClick={handleStartGame}
          disabled={!allPlayersReady}
          className={`min-h-11 flex-1 rounded-full font-bold transition-all ${
            allPlayersReady
              ? "bg-gradient-to-r from-green-500 to-emerald-600 text-white shadow-lg shadow-green-500/50"
              : "cursor-not-allowed bg-white/20 text-white/50"
          }`}
        >
          {allPlayersReady ? "Choose Arena →" : `Select All Zoogis (${localPlayers.filter(p => p.zoogi).length}/${localPlayerCount})`}
        </motion.button>
      </div>
    </div>
  );
}
