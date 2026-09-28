import { useState, useEffect, useMemo } from "react";
import { Trophy, Crown, Medal, Calendar, Star, Flame, Clock, Users, ChevronRight, Sparkles } from "lucide-react";
import { useAuth } from "@/lib/stores/useAuth";
import { getSpotlightZoogi, getTimeRemaining as getSpotlightTimeRemaining, ZOOGI_LORE, getRarityColor, getRarityGradient } from "@/components/game/ZoogiSpotlight";

interface LeaderboardEntry {
  id: number;
  playerName: string;
  score: number;
  zoogiUsed: string;
  createdAt?: string;
  userId?: number;
  rank?: number;
}

interface SeasonalEntry {
  id: number;
  userId: number;
  username: string;
  totalScore: number;
  gamesPlayed: number;
  wins: number;
  rank: number;
}

interface SeasonInfo {
  id: number;
  name: string;
  startDate: string;
  endDate: string;
  isActive: boolean;
}

type LeaderboardTab = "all-time" | "weekly" | "seasonal";

interface EnhancedLeaderboardProps {
  onClose: () => void;
}

export function EnhancedLeaderboard({ onClose }: EnhancedLeaderboardProps) {
  const { user } = useAuth();
  const [activeTab, setActiveTab] = useState<LeaderboardTab>("all-time");
  const [allTimeEntries, setAllTimeEntries] = useState<LeaderboardEntry[]>([]);
  const [weeklyEntries, setWeeklyEntries] = useState<LeaderboardEntry[]>([]);
  const [seasonalEntries, setSeasonalEntries] = useState<SeasonalEntry[]>([]);
  const [currentSeason, setCurrentSeason] = useState<SeasonInfo | null>(null);
  const [loading, setLoading] = useState(true);
  const [userRank, setUserRank] = useState<number | null>(null);

  useEffect(() => {
    fetchLeaderboards();
  }, []);

  const fetchLeaderboards = async () => {
    setLoading(true);
    try {
      const [allTimeRes, weeklyRes, seasonRes] = await Promise.all([
        fetch("/api/leaderboard"),
        fetch("/api/leaderboard/weekly"),
        fetch("/api/seasons/current")
      ]);

      if (allTimeRes.ok) {
        const data = await allTimeRes.json();
        setAllTimeEntries(data || []);
      }

      if (weeklyRes.ok) {
        const data = await weeklyRes.json();
        setWeeklyEntries(data.data || []);
      }

      if (seasonRes.ok) {
        const data = await seasonRes.json();
        if (data.season) {
          setCurrentSeason(data.season);
          const seasonalRes = await fetch(`/api/leaderboard/seasonal/${data.season.id}`);
          if (seasonalRes.ok) {
            const seasonalData = await seasonalRes.json();
            setSeasonalEntries(seasonalData.data || []);
            if (user) {
              const userEntry = seasonalData.data?.find((e: SeasonalEntry) => e.userId === user.id);
              if (userEntry) setUserRank(userEntry.rank);
            }
          }
        }
      }
    } catch (error) {
      console.error("Error fetching leaderboards:", error);
    } finally {
      setLoading(false);
    }
  };

  const getRankIcon = (rank: number) => {
    if (rank === 1) return <Crown className="w-5 h-5 text-yellow-400" />;
    if (rank === 2) return <Medal className="w-5 h-5 text-gray-300" />;
    if (rank === 3) return <Medal className="w-5 h-5 text-amber-600" />;
    return <span className="w-5 h-5 flex items-center justify-center text-white/50 text-sm font-bold">{rank}</span>;
  };

  const getRankBg = (rank: number) => {
    if (rank === 1) return "bg-gradient-to-r from-yellow-500/30 to-amber-500/20 border-yellow-500/40";
    if (rank === 2) return "bg-gradient-to-r from-gray-400/20 to-gray-500/10 border-gray-400/30";
    if (rank === 3) return "bg-gradient-to-r from-amber-600/20 to-orange-600/10 border-amber-600/30";
    return "bg-white/5 border-white/10";
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleDateString("en-US", {
      month: "short",
      day: "numeric"
    });
  };

  const getTimeRemaining = () => {
    if (!currentSeason) return null;
    const endDate = new Date(currentSeason.endDate);
    const now = new Date();
    const diff = endDate.getTime() - now.getTime();
    const days = Math.floor(diff / (1000 * 60 * 60 * 24));
    return days > 0 ? `${days} days left` : "Ending soon";
  };

  const tabs = [
    { id: "all-time" as LeaderboardTab, label: "All-Time", icon: Star },
    { id: "weekly" as LeaderboardTab, label: "Weekly", icon: Calendar },
    { id: "seasonal" as LeaderboardTab, label: "Season", icon: Flame }
  ];

  return (
    <div
      className="absolute inset-0 flex items-center justify-center bg-black/80 z-50"
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl max-w-lg w-full mx-4 max-h-[85vh] overflow-hidden shadow-2xl border border-white/10">
        <div className="bg-gradient-to-r from-yellow-500 to-amber-500 p-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <Trophy className="w-8 h-8 text-white" />
              <div>
                <h2 className="text-xl font-bold text-white">Leaderboards</h2>
                <p className="text-white/70 text-xs">See who's on top</p>
              </div>
            </div>
            <button
              onClick={onClose}
              className="text-white/80 hover:text-white text-2xl font-bold"
            >
              ×
            </button>
          </div>
        </div>

        <div className="flex border-b border-white/10">
          {tabs.map((tab) => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`flex-1 flex items-center justify-center gap-2 py-3 text-sm font-semibold transition-all ${
                activeTab === tab.id
                  ? "text-yellow-400 border-b-2 border-yellow-400 bg-white/5"
                  : "text-white/60 hover:text-white/80"
              }`}
            >
              <tab.icon className="w-4 h-4" />
              {tab.label}
            </button>
          ))}
        </div>

        {currentSeason && activeTab === "seasonal" && (
          <div className="p-3 bg-gradient-to-r from-purple-900/40 to-pink-900/40 border-b border-white/10">
            <div className="flex items-center justify-between">
              <div>
                <h3 className="text-white font-bold">{currentSeason.name}</h3>
                <p className="text-white/60 text-xs">
                  {formatDate(currentSeason.startDate)} - {formatDate(currentSeason.endDate)}
                </p>
              </div>
              <div className="flex items-center gap-1 text-amber-400 text-sm">
                <Clock className="w-4 h-4" />
                <span>{getTimeRemaining()}</span>
              </div>
            </div>
            {userRank && (
              <div className="mt-2 flex items-center gap-2 text-cyan-400 text-sm">
                <Users className="w-4 h-4" />
                <span>Your Rank: #{userRank}</span>
              </div>
            )}
          </div>
        )}

        <div className="p-4 overflow-y-auto max-h-[50vh]">
          {loading ? (
            <div className="flex items-center justify-center py-12">
              <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-yellow-400" />
            </div>
          ) : (
            <div className="space-y-2">
              {activeTab === "all-time" && allTimeEntries.map((entry, index) => (
                <div
                  key={entry.id}
                  className={`flex items-center gap-3 p-3 rounded-xl border ${getRankBg(index + 1)}`}
                >
                  <div className="w-8 flex justify-center">
                    {getRankIcon(index + 1)}
                  </div>
                  <div className="flex-1 min-w-0">
                    <p className="text-white font-semibold truncate">{entry.playerName}</p>
                    <p className="text-white/50 text-xs">{entry.zoogiUsed}</p>
                  </div>
                  <div className="text-right">
                    <p className="text-yellow-400 font-bold">{entry.score.toLocaleString()}</p>
                    <p className="text-white/40 text-xs">points</p>
                  </div>
                </div>
              ))}

              {activeTab === "weekly" && weeklyEntries.map((entry, index) => (
                <div
                  key={entry.id}
                  className={`flex items-center gap-3 p-3 rounded-xl border ${getRankBg(index + 1)}`}
                >
                  <div className="w-8 flex justify-center">
                    {getRankIcon(index + 1)}
                  </div>
                  <div className="flex-1 min-w-0">
                    <p className="text-white font-semibold truncate">{entry.playerName}</p>
                    <p className="text-white/50 text-xs">{entry.zoogiUsed}</p>
                  </div>
                  <div className="text-right">
                    <p className="text-cyan-400 font-bold">{entry.score.toLocaleString()}</p>
                    <p className="text-white/40 text-xs">this week</p>
                  </div>
                </div>
              ))}

              {activeTab === "seasonal" && seasonalEntries.map((entry) => (
                <div
                  key={entry.id}
                  className={`flex items-center gap-3 p-3 rounded-xl border ${getRankBg(entry.rank)} ${
                    user?.id === entry.userId ? "ring-2 ring-cyan-400/50" : ""
                  }`}
                >
                  <div className="w-8 flex justify-center">
                    {getRankIcon(entry.rank)}
                  </div>
                  <div className="flex-1 min-w-0">
                    <p className="text-white font-semibold truncate">
                      {entry.username}
                      {user?.id === entry.userId && (
                        <span className="ml-2 text-xs text-cyan-400">(You)</span>
                      )}
                    </p>
                    <p className="text-white/50 text-xs">
                      {entry.wins}W / {entry.gamesPlayed} games
                    </p>
                  </div>
                  <div className="text-right">
                    <p className="text-purple-400 font-bold">{entry.totalScore.toLocaleString()}</p>
                    <p className="text-white/40 text-xs">season pts</p>
                  </div>
                </div>
              ))}

              {((activeTab === "all-time" && allTimeEntries.length === 0) ||
                (activeTab === "weekly" && weeklyEntries.length === 0) ||
                (activeTab === "seasonal" && seasonalEntries.length === 0)) && (
                <div className="text-center py-12 text-white/50">
                  <Trophy className="w-12 h-12 mx-auto mb-3 opacity-50" />
                  <p>No scores yet. Be the first!</p>
                </div>
              )}
            </div>
          )}
        </div>

        <WeeklySpotlightSection />

        <div className="p-4 border-t border-white/10 bg-black/20">
          <div className="flex items-center justify-between text-white/50 text-xs">
            <span>Updated every few minutes</span>
            <button
              onClick={fetchLeaderboards}
              className="text-cyan-400 hover:text-cyan-300 flex items-center gap-1"
            >
              Refresh
              <ChevronRight className="w-3 h-3" />
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

function WeeklySpotlightSection() {
  const spotlightData = useMemo(() => getSpotlightZoogi('weekly'), []);
  const timeRemaining = useMemo(() => getSpotlightTimeRemaining('weekly'), []);
  const lore = ZOOGI_LORE[spotlightData.zoogi.id];
  const rarity = lore?.rarity || 'Common';
  
  return (
    <div className={`mx-4 mb-4 p-3 rounded-xl bg-gradient-to-r ${getRarityGradient(rarity)} border border-white/20`}>
      <div className="flex items-center gap-2 mb-2">
        <Sparkles className="w-4 h-4 text-yellow-400" />
        <span className="text-white/80 text-xs font-semibold">Weekly Spotlight</span>
        <span className="text-white/50 text-xs ml-auto">{timeRemaining}</span>
      </div>
      <div className="flex items-center gap-3">
        <div 
          className="w-10 h-10 rounded-full flex-shrink-0 shadow-lg"
          style={{ backgroundColor: spotlightData.zoogi.color }}
        />
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <span className="text-white font-bold text-sm">{spotlightData.zoogi.name}</span>
            <span className={`text-xs ${getRarityColor(rarity)}`}>{rarity}</span>
          </div>
          <p className="text-white/60 text-xs truncate">{spotlightData.zoogi.ability}</p>
        </div>
      </div>
    </div>
  );
}
