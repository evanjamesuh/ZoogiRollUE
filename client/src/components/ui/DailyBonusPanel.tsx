import { useEffect, useState } from "react";
import { useCommunity } from "@/lib/stores/useCommunity";
import { X, Gift, Coins, Gem, Check, Flame } from "lucide-react";
import { OnlineNotice } from "@/components/ui/OnlineNotice";

interface DailyBonusPanelProps {
  onClose: () => void;
}

const dailyRewards = [
  { day: 1, coins: 50, gems: 0 },
  { day: 2, coins: 75, gems: 0 },
  { day: 3, coins: 100, gems: 1 },
  { day: 4, coins: 125, gems: 0 },
  { day: 5, coins: 150, gems: 2 },
  { day: 6, coins: 200, gems: 0 },
  { day: 7, coins: 300, gems: 5 },
];

export function DailyBonusPanel({ onClose }: DailyBonusPanelProps) {
  const { dailyBonus, fetchDailyBonus, claimDailyBonus } = useCommunity();
  const [claiming, setClaiming] = useState(false);
  const [claimedReward, setClaimedReward] = useState<{ coins: number; gems: number } | null>(null);

  useEffect(() => {
    fetchDailyBonus();
  }, [fetchDailyBonus]);

  const handleClaim = async () => {
    setClaiming(true);
    const result = await claimDailyBonus();
    setClaiming(false);
    if (result) {
      setClaimedReward(result.reward);
    }
  };

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-br from-yellow-900/95 to-amber-900/95 rounded-2xl w-full max-w-md overflow-hidden shadow-2xl border border-yellow-500/30">
        <div className="flex items-center justify-between p-4 border-b border-yellow-500/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-yellow-500 to-amber-600 flex items-center justify-center">
              <Gift className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Daily Bonus</h2>
              {dailyBonus && (
                <p className="text-yellow-300 text-sm flex items-center gap-1">
                  <Flame className="w-4 h-4" />
                  {dailyBonus.loginStreak} day streak
                </p>
              )}
            </div>
          </div>
          <button 
            onClick={onClose}
            className="p-2 hover:bg-white/10 rounded-lg transition-colors"
          >
            <X className="w-6 h-6 text-white" />
          </button>
        </div>

        <OnlineNotice compact className="mx-4 mt-3" />

        <div className="p-4">
          {claimedReward ? (
            <div className="text-center py-8">
              <div className="w-20 h-20 mx-auto bg-gradient-to-br from-green-500 to-emerald-600 rounded-full flex items-center justify-center mb-4">
                <Check className="w-10 h-10 text-white" />
              </div>
              <h3 className="text-2xl font-bold text-white mb-2">Reward Claimed!</h3>
              <div className="flex items-center justify-center gap-4 mt-4">
                <div className="flex items-center gap-2 bg-yellow-500/20 px-4 py-2 rounded-full">
                  <Coins className="w-5 h-5 text-yellow-400" />
                  <span className="text-white font-bold">+{claimedReward.coins}</span>
                </div>
                {claimedReward.gems > 0 && (
                  <div className="flex items-center gap-2 bg-purple-500/20 px-4 py-2 rounded-full">
                    <Gem className="w-5 h-5 text-purple-400" />
                    <span className="text-white font-bold">+{claimedReward.gems}</span>
                  </div>
                )}
              </div>
            </div>
          ) : (
            <>
              <div className="grid grid-cols-7 gap-2 mb-6">
                {dailyRewards.map((reward) => {
                  const isNext = dailyBonus && reward.day === dailyBonus.nextDay;
                  const isPast = dailyBonus && reward.day < dailyBonus.nextDay;
                  const canClaim = isNext && dailyBonus.canClaim;
                  
                  return (
                    <div
                      key={reward.day}
                      className={`relative rounded-xl p-2 text-center transition-all ${
                        canClaim
                          ? "bg-gradient-to-br from-yellow-500 to-amber-500 ring-2 ring-yellow-400 scale-110 z-10"
                          : isPast
                          ? "bg-white/5 opacity-50"
                          : "bg-white/10"
                      }`}
                    >
                      <div className={`text-xs mb-1 ${canClaim ? "text-white" : "text-white/60"}`}>
                        Day {reward.day}
                      </div>
                      <div className="flex flex-col items-center gap-1">
                        <Coins className={`w-4 h-4 ${canClaim ? "text-white" : "text-yellow-400"}`} />
                        <span className={`text-xs font-bold ${canClaim ? "text-white" : "text-white/80"}`}>
                          {reward.coins}
                        </span>
                      </div>
                      {reward.gems > 0 && (
                        <div className="flex items-center justify-center gap-1 mt-1">
                          <Gem className={`w-3 h-3 ${canClaim ? "text-white" : "text-purple-400"}`} />
                          <span className={`text-xs ${canClaim ? "text-white" : "text-purple-300"}`}>
                            +{reward.gems}
                          </span>
                        </div>
                      )}
                      {isPast && (
                        <div className="absolute inset-0 flex items-center justify-center">
                          <Check className="w-6 h-6 text-green-400" />
                        </div>
                      )}
                    </div>
                  );
                })}
              </div>

              <button
                onClick={handleClaim}
                disabled={!dailyBonus?.canClaim || claiming}
                className={`w-full py-4 rounded-xl font-bold text-lg transition-all ${
                  dailyBonus?.canClaim
                    ? "bg-gradient-to-r from-yellow-500 to-amber-500 text-white hover:from-yellow-600 hover:to-amber-600 active:scale-[0.98]"
                    : "bg-white/10 text-white/50 cursor-not-allowed"
                }`}
              >
                {claiming
                  ? "Claiming..."
                  : dailyBonus?.canClaim
                  ? "Claim Bonus"
                  : "Come Back Tomorrow"}
              </button>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
