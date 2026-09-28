import { useState } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { X, Trophy, Target, Flame, Star, Coins, Gem, Edit2, Check, LogOut, Lock } from "lucide-react";
import { useAuth, User } from "@/lib/stores/useAuth";
import { ZOOGI_ROSTER } from "@/lib/stores/useZoogiGame";

const PORTRAIT_IMAGES: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  wraps: "/portraits/wraps.png",
};

interface UserProfileProps {
  isOpen: boolean;
  onClose: () => void;
  user: User;
}

export function UserProfile({ isOpen, onClose, user }: UserProfileProps) {
  const [isEditing, setIsEditing] = useState(false);
  const [displayName, setDisplayName] = useState(user.displayName || "");
  const { updateProfile, logout } = useAuth();

  const handleSave = async () => {
    await updateProfile({ displayName: displayName || null });
    setIsEditing(false);
  };

  const handleLogout = async () => {
    await logout();
    onClose();
  };

  const winRate = user.gamesPlayed > 0 
    ? Math.round((user.totalWins / user.gamesPlayed) * 100) 
    : 0;

  const xpToNextLevel = user.level * 100;
  const xpProgress = (user.xp / xpToNextLevel) * 100;

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
        >
          <motion.div
            className="relative w-full max-w-lg mx-4 max-h-[85vh] bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl shadow-2xl border border-gray-700 overflow-hidden flex flex-col"
            initial={{ scale: 0.9, opacity: 0, y: 20 }}
            animate={{ scale: 1, opacity: 1, y: 0 }}
            exit={{ scale: 0.9, opacity: 0, y: 20 }}
            onClick={(e) => e.stopPropagation()}
          >
            <div className="absolute top-0 left-0 right-0 h-24 bg-gradient-to-r from-purple-600 to-pink-600 z-0" />
            
            <button
              onClick={onClose}
              className="absolute top-4 right-4 text-white/80 hover:text-white transition-colors z-10"
            >
              <X size={24} />
            </button>

            <div className="relative pt-12 px-6 pb-6 overflow-y-auto">
              <div className="flex items-end gap-4 mb-6">
                <div className="w-20 h-20 rounded-full bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center text-3xl font-bold text-white border-4 border-gray-800">
                  {(user.displayName || user.username)[0].toUpperCase()}
                </div>
                <div className="flex-1 pb-1">
                  {isEditing ? (
                    <div className="flex items-center gap-2">
                      <input
                        type="text"
                        value={displayName}
                        onChange={(e) => setDisplayName(e.target.value)}
                        className="px-2 py-1 bg-gray-700 border border-gray-600 rounded text-white text-lg w-full"
                        maxLength={30}
                      />
                      <button
                        onClick={handleSave}
                        className="p-1 text-green-400 hover:text-green-300"
                      >
                        <Check size={20} />
                      </button>
                    </div>
                  ) : (
                    <div className="flex items-center gap-2">
                      <h2 className="text-xl font-bold text-white">
                        {user.displayName || user.username}
                      </h2>
                      <button
                        onClick={() => setIsEditing(true)}
                        className="text-gray-400 hover:text-white"
                      >
                        <Edit2 size={16} />
                      </button>
                    </div>
                  )}
                  <p className="text-gray-400">@{user.username}</p>
                </div>
              </div>

              <div className="flex items-center gap-2 mb-4">
                <div className="flex-1 h-2 bg-gray-700 rounded-full overflow-hidden">
                  <motion.div
                    className="h-full bg-gradient-to-r from-yellow-400 to-orange-500"
                    initial={{ width: 0 }}
                    animate={{ width: `${xpProgress}%` }}
                    transition={{ duration: 0.5 }}
                  />
                </div>
                <span className="text-sm text-gray-400">
                  Level {user.level} ({user.xp}/{xpToNextLevel} XP)
                </span>
              </div>

              <div className="grid grid-cols-2 gap-3 mb-4">
                <div className="flex items-center gap-3 p-3 bg-gray-700/50 rounded-lg">
                  <Coins className="text-yellow-400" size={24} />
                  <div>
                    <div className="text-lg font-bold text-white">{user.coins}</div>
                    <div className="text-xs text-gray-400">Coins</div>
                  </div>
                </div>
                <div className="flex items-center gap-3 p-3 bg-gray-700/50 rounded-lg">
                  <Gem className="text-purple-400" size={24} />
                  <div>
                    <div className="text-lg font-bold text-white">{user.gems}</div>
                    <div className="text-xs text-gray-400">Gems</div>
                  </div>
                </div>
              </div>

              <div className="grid grid-cols-2 gap-3 mb-4">
                <div className="flex items-center gap-3 p-3 bg-gray-700/50 rounded-lg">
                  <Trophy className="text-yellow-500" size={24} />
                  <div>
                    <div className="text-lg font-bold text-white">{user.totalWins}</div>
                    <div className="text-xs text-gray-400">Wins</div>
                  </div>
                </div>
                <div className="flex items-center gap-3 p-3 bg-gray-700/50 rounded-lg">
                  <Target className="text-red-400" size={24} />
                  <div>
                    <div className="text-lg font-bold text-white">{user.totalKnockoffs}</div>
                    <div className="text-xs text-gray-400">Knockoffs</div>
                  </div>
                </div>
              </div>

              <div className="grid grid-cols-3 gap-3 mb-4">
                <div className="p-3 bg-gray-700/50 rounded-lg text-center">
                  <div className="text-lg font-bold text-white">{user.gamesPlayed}</div>
                  <div className="text-xs text-gray-400">Games</div>
                </div>
                <div className="p-3 bg-gray-700/50 rounded-lg text-center">
                  <div className="text-lg font-bold text-white">{winRate}%</div>
                  <div className="text-xs text-gray-400">Win Rate</div>
                </div>
                <div className="p-3 bg-gray-700/50 rounded-lg text-center">
                  <div className="text-lg font-bold text-white">{user.highScore}</div>
                  <div className="text-xs text-gray-400">High Score</div>
                </div>
              </div>

              {/* Zoogi Collection */}
              <div className="mb-4">
                <div className="flex items-center justify-between mb-2">
                  <h3 className="text-sm font-semibold text-gray-300">Zoogi Collection</h3>
                  <span className="text-xs text-gray-500">{ZOOGI_ROSTER.length}/{ZOOGI_ROSTER.length * 2}</span>
                </div>
                <div className="flex gap-3 overflow-x-auto pb-2">
                  {[...ZOOGI_ROSTER, ...ZOOGI_ROSTER].map((zoogi, index) => {
                    const hasPortrait = PORTRAIT_IMAGES[zoogi.id];
                    const isCollected = index < ZOOGI_ROSTER.length;
                    
                    return (
                      <div
                        key={`${zoogi.id}-${index}`}
                        className={`relative flex-shrink-0 w-12 h-12 rounded-full border-2 overflow-hidden ${
                          isCollected 
                            ? "border-yellow-400 animate-pulse" 
                            : "border-gray-600 opacity-50"
                        }`}
                        style={isCollected ? {
                          boxShadow: "0 0 12px 4px rgba(250, 204, 21, 0.5), 0 0 20px 8px rgba(250, 204, 21, 0.3)"
                        } : undefined}
                      >
                        {hasPortrait ? (
                          <img 
                            src={PORTRAIT_IMAGES[zoogi.id]} 
                            alt={zoogi.name}
                            className={`w-full h-full object-cover ${!isCollected && "grayscale"}`}
                            style={zoogi.id === "wraps" ? { transform: "scale(1.4)", transformOrigin: "center 45%" } : undefined}
                          />
                        ) : (
                          <div
                            className={`w-full h-full ${!isCollected && "grayscale"}`}
                            style={{
                              background: `radial-gradient(circle at 30% 30%, ${zoogi.secondaryColor}, ${zoogi.color})`,
                            }}
                          />
                        )}
                        {!isCollected && (
                          <div className="absolute inset-0 flex items-center justify-center bg-black/50">
                            <Lock size={14} className="text-gray-400" />
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              </div>

              {user.loginStreak > 0 && (
                <div className="flex items-center gap-2 p-3 bg-gradient-to-r from-orange-500/20 to-red-500/20 border border-orange-500/30 rounded-lg mb-4">
                  <Flame className="text-orange-400" size={24} />
                  <div>
                    <div className="text-white font-semibold">{user.loginStreak} Day Streak!</div>
                    <div className="text-xs text-orange-300">Keep playing daily for rewards</div>
                  </div>
                </div>
              )}

              <button
                onClick={handleLogout}
                className="w-full py-3 bg-gray-700 hover:bg-gray-600 text-gray-300 hover:text-white font-medium rounded-lg transition-colors flex items-center justify-center gap-2"
              >
                <LogOut size={18} />
                Sign Out
              </button>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
