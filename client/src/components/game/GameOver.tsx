import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { motion } from "framer-motion";
import ReactConfetti from "react-confetti";
import { useState, useEffect } from "react";
import { Trophy } from "lucide-react";

interface LeaderboardEntry {
  id: number;
  playerName: string;
  score: number;
  zoogiUsed: string;
  createdAt: string;
}

export function GameOver() {
  const { isVictory, score, playerEntity, enemies, returnToMenu, setPhase, restartWithSameZoogi } = useZoogiGame();
  const bestEnemyScore = Math.max(...enemies.map((enemy) => enemy.score || 0), 0);
  const tiedForTheWin = isVictory && score === bestEnemyScore;
  const [dimensions, setDimensions] = useState({ width: window.innerWidth, height: window.innerHeight });
  const [playerName, setPlayerName] = useState("");
  const [submitted, setSubmitted] = useState(false);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [showLeaderboard, setShowLeaderboard] = useState(false);

  useEffect(() => {
    const handleResize = () => {
      setDimensions({ width: window.innerWidth, height: window.innerHeight });
    };
    window.addEventListener("resize", handleResize);
    return () => window.removeEventListener("resize", handleResize);
  }, []);

  useEffect(() => {
    fetchLeaderboard();
  }, []);

  const fetchLeaderboard = async () => {
    try {
      const response = await fetch("/api/leaderboard?limit=10");
      if (response.ok) {
        const data = await response.json();
        setLeaderboard(data);
      }
    } catch (error) {
      console.error("Failed to fetch leaderboard:", error);
    }
  };

  const submitScore = async () => {
    if (!playerName.trim() || !playerEntity) return;
    try {
      const response = await fetch("/api/leaderboard", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          playerName: playerName.trim(),
          score,
          zoogiUsed: playerEntity.zoogi.name,
        }),
      });
      if (response.ok) {
        setSubmitted(true);
        fetchLeaderboard();
      }
    } catch (error) {
      console.error("Failed to submit score:", error);
    }
  };

  const handlePlayAgain = () => {
    restartWithSameZoogi();
  };

  const handleChangeZoogi = () => {
    returnToMenu();
    setPhase("character_selection");
  };

  return (
    <div className="fixed inset-0 flex items-center justify-center bg-black/80 z-50">
      {isVictory && (
        <ReactConfetti
          width={dimensions.width}
          height={dimensions.height}
          recycle={false}
          numberOfPieces={500}
        />
      )}

      <motion.div
        initial={{ scale: 0, rotate: -10 }}
        animate={{ scale: 1, rotate: 0 }}
        transition={{ type: "spring", duration: 0.5 }}
        className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-3xl p-8 text-center max-w-md mx-4 shadow-2xl"
      >
        <motion.div
          initial={{ y: -20, opacity: 0 }}
          animate={{ y: 0, opacity: 1 }}
          transition={{ delay: 0.2 }}
        >
          {isVictory ? (
            <>
              <h1 className="text-5xl font-black text-transparent bg-clip-text bg-gradient-to-r from-yellow-400 to-orange-500 mb-2">
                {tiedForTheWin ? "IT'S A TIE!" : "VICTORY!"}
              </h1>
              <p className="text-white/70">
                {tiedForTheWin ? "A tie goes to you. The match is yours!" : "All 3 rounds complete!"}
              </p>
            </>
          ) : (
            <>
              <h1 className="text-5xl font-black text-transparent bg-clip-text bg-gradient-to-r from-red-500 to-pink-500 mb-2">
                GAME OVER
              </h1>
              <p className="text-white/70">Better luck next time!</p>
            </>
          )}
        </motion.div>

        <motion.div
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          transition={{ delay: 0.4, type: "spring" }}
          className="my-8"
        >
          <p className="text-white/60 text-sm uppercase">Final Score</p>
          <p className="text-6xl font-bold text-yellow-400">{score}</p>
        </motion.div>

        {playerEntity && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 0.6 }}
            className="flex items-center justify-center gap-3 mb-6"
          >
            <div
              className="w-12 h-12 rounded-full shadow-lg"
              style={{
                background: `radial-gradient(circle at 30% 30%, ${playerEntity.zoogi.secondaryColor}, ${playerEntity.zoogi.color})`,
              }}
            />
            <div className="text-left">
              <p className="text-white font-semibold">{playerEntity.zoogi.name}</p>
              <p className="text-white/60 text-sm">{playerEntity.zoogi.type}</p>
            </div>
          </motion.div>
        )}

        {!submitted && isVictory && (
          <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.7 }}
            className="mb-6"
          >
            <p className="text-white/60 text-sm mb-2">Save your score to the leaderboard!</p>
            <div className="flex gap-2">
              <input
                type="text"
                value={playerName}
                onChange={(e) => setPlayerName(e.target.value)}
                placeholder="Enter your name"
                maxLength={20}
                className="flex-1 px-4 py-2 rounded-full bg-white/10 text-white border border-white/20 focus:outline-none focus:border-yellow-400"
              />
              <motion.button
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
                onClick={submitScore}
                disabled={!playerName.trim()}
                className="px-4 py-2 bg-yellow-500 text-black font-bold rounded-full disabled:opacity-50"
              >
                Save
              </motion.button>
            </div>
          </motion.div>
        )}

        {submitted && (
          <motion.p
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="text-green-400 mb-4"
          >
            Score saved to leaderboard!
          </motion.p>
        )}

        <motion.button
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 0.75 }}
          whileHover={{ scale: 1.02 }}
          onClick={() => setShowLeaderboard(!showLeaderboard)}
          className="flex items-center justify-center gap-2 w-full py-2 mb-4 text-yellow-400 hover:text-yellow-300"
        >
          <Trophy size={18} />
          <span>{showLeaderboard ? "Hide" : "View"} Leaderboard</span>
        </motion.button>

        {showLeaderboard && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: "auto" }}
            className="mb-6 bg-black/30 rounded-xl p-4 max-h-48 overflow-y-auto"
          >
            {leaderboard.length === 0 ? (
              <p className="text-white/50 text-sm">No scores yet. Be the first!</p>
            ) : (
              <div className="space-y-2">
                {leaderboard.map((entry, index) => (
                  <div
                    key={entry.id}
                    className="flex items-center justify-between text-sm"
                  >
                    <div className="flex items-center gap-2">
                      <span className={`font-bold ${index === 0 ? "text-yellow-400" : index === 1 ? "text-gray-300" : index === 2 ? "text-orange-400" : "text-white/60"}`}>
                        #{index + 1}
                      </span>
                      <span className="text-white">{entry.playerName}</span>
                      <span className="text-white/40 text-xs">({entry.zoogiUsed})</span>
                    </div>
                    <span className="text-yellow-400 font-bold">{entry.score}</span>
                  </div>
                ))}
              </div>
            )}
          </motion.div>
        )}

        <motion.div
          initial={{ y: 20, opacity: 0 }}
          animate={{ y: 0, opacity: 1 }}
          transition={{ delay: 0.8 }}
          className="flex flex-col gap-3"
        >
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={handlePlayAgain}
            className="w-full px-6 py-3 bg-gradient-to-r from-green-500 to-emerald-600 text-white font-bold rounded-full shadow-lg shadow-green-500/30"
          >
            Play Again
          </motion.button>
          
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={handleChangeZoogi}
            className="w-full px-6 py-3 bg-white/10 text-white font-semibold rounded-full hover:bg-white/20 transition-colors"
          >
            Change Zoogi
          </motion.button>
          
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={returnToMenu}
            className="w-full px-6 py-3 bg-white/10 text-white/70 font-semibold rounded-full hover:bg-white/20 transition-colors"
          >
            Main Menu
          </motion.button>
        </motion.div>
      </motion.div>
    </div>
  );
}
