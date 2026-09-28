import { create } from "zustand";
import { persist } from "zustand/middleware";

export interface LevelReward {
  level: number;
  type: "title" | "skin" | "character" | "xp_boost" | "coins";
  id: string;
  name: string;
  description: string;
  rarity: "common" | "rare" | "epic" | "legendary";
}

export interface Challenge {
  id: string;
  name: string;
  description: string;
  type: "daily" | "weekly";
  target: number;
  progress: number;
  xpReward: number;
  completed: boolean;
  expiresAt: number;
}

export interface BattlePassTier {
  tier: number;
  xpRequired: number;
  freeReward: LevelReward | null;
  premiumReward: LevelReward | null;
}

const LEVEL_REWARDS: LevelReward[] = [
  { level: 2, type: "title", id: "title_rookie", name: "Rookie Roller", description: "Reached Level 2", rarity: "common" },
  { level: 3, type: "coins", id: "coins_100", name: "100 Coins", description: "Starting bonus", rarity: "common" },
  { level: 5, type: "title", id: "title_arena", name: "Arena Apprentice", description: "Reached Level 5", rarity: "common" },
  { level: 7, type: "skin", id: "skin_golden_trail", name: "Golden Trail", description: "Leave a shimmering path", rarity: "rare" },
  { level: 10, type: "title", id: "title_veteran", name: "Zoogi Veteran", description: "Reached Level 10", rarity: "rare" },
  { level: 12, type: "xp_boost", id: "boost_10", name: "10% XP Boost", description: "Earn XP faster", rarity: "rare" },
  { level: 15, type: "character", id: "char_shadow", name: "Shadow Wolf", description: "Wolfgang's dark variant", rarity: "epic" },
  { level: 20, type: "title", id: "title_champion", name: "Champion Roller", description: "Reached Level 20", rarity: "epic" },
  { level: 25, type: "skin", id: "skin_neon", name: "Neon Glow", description: "Electrifying effects", rarity: "epic" },
  { level: 30, type: "character", id: "char_blaze", name: "Blaze Phoenix", description: "Hotstreak's fire variant", rarity: "legendary" },
  { level: 40, type: "title", id: "title_legend", name: "Legendary Zoogi", description: "Reached Level 40", rarity: "legendary" },
  { level: 50, type: "character", id: "char_cosmic", name: "Cosmic Lars", description: "Lars's cosmic variant", rarity: "legendary" },
];

const BATTLE_PASS_TIERS: BattlePassTier[] = Array.from({ length: 50 }, (_, i) => ({
  tier: i + 1,
  xpRequired: (i + 1) * 1000,
  freeReward: i % 3 === 0 ? {
    level: i + 1,
    type: "coins" as const,
    id: `bp_coins_${i + 1}`,
    name: `${50 + i * 10} Coins`,
    description: "Battle Pass reward",
    rarity: i < 20 ? "common" as const : i < 40 ? "rare" as const : "epic" as const
  } : null,
  premiumReward: {
    level: i + 1,
    type: i % 5 === 0 ? "character" as const : i % 2 === 0 ? "skin" as const : "title" as const,
    id: `bp_premium_${i + 1}`,
    name: `Premium Tier ${i + 1}`,
    description: "Premium Battle Pass reward",
    rarity: i < 15 ? "common" as const : i < 30 ? "rare" as const : i < 45 ? "epic" as const : "legendary" as const
  }
}));

const generateDailyChallenges = (): Challenge[] => {
  const now = Date.now();
  const endOfDay = new Date();
  endOfDay.setHours(23, 59, 59, 999);
  
  return [
    {
      id: `daily_orbs_${now}`,
      name: "Orb Collector",
      description: "Collect 20 orbs",
      type: "daily",
      target: 20,
      progress: 0,
      xpReward: 100,
      completed: false,
      expiresAt: endOfDay.getTime()
    },
    {
      id: `daily_knockoffs_${now}`,
      name: "Ring Out!",
      description: "Knock off 3 enemies",
      type: "daily",
      target: 3,
      progress: 0,
      xpReward: 150,
      completed: false,
      expiresAt: endOfDay.getTime()
    },
    {
      id: `daily_games_${now}`,
      name: "Play Time",
      description: "Play 5 games",
      type: "daily",
      target: 5,
      progress: 0,
      xpReward: 75,
      completed: false,
      expiresAt: endOfDay.getTime()
    }
  ];
};

const generateWeeklyChallenges = (): Challenge[] => {
  const now = Date.now();
  const endOfWeek = new Date();
  const daysUntilSunday = 7 - endOfWeek.getDay();
  endOfWeek.setDate(endOfWeek.getDate() + daysUntilSunday);
  endOfWeek.setHours(23, 59, 59, 999);
  
  return [
    {
      id: `weekly_wins_${now}`,
      name: "Victory Lap",
      description: "Win 10 games",
      type: "weekly",
      target: 10,
      progress: 0,
      xpReward: 500,
      completed: false,
      expiresAt: endOfWeek.getTime()
    },
    {
      id: `weekly_orbs_${now}`,
      name: "Orb Hoarder",
      description: "Collect 100 orbs",
      type: "weekly",
      target: 100,
      progress: 0,
      xpReward: 400,
      completed: false,
      expiresAt: endOfWeek.getTime()
    },
    {
      id: `weekly_knockoffs_${now}`,
      name: "Arena Master",
      description: "Knock off 20 enemies",
      type: "weekly",
      target: 20,
      progress: 0,
      xpReward: 600,
      completed: false,
      expiresAt: endOfWeek.getTime()
    }
  ];
};

interface XPGainEvent {
  id: string;
  amount: number;
  reason: string;
  timestamp: number;
}

interface ProgressionState {
  xp: number;
  level: number;
  totalXpEarned: number;
  
  unlockedRewards: string[];
  equippedTitle: string | null;
  equippedSkin: string | null;
  
  coins: number;
  xpBoostPercent: number;
  
  challenges: Challenge[];
  lastChallengeRefresh: number;
  
  totalGamesPlayed: number;
  totalWins: number;
  totalOrbsCollected: number;
  totalKnockouts: number;
  highestScore: number;
  loginStreak: number;
  lastLoginDate: string | null;
  favoriteZoogi: string | null;
  zoogiPlayCount: Record<string, number>;
  ownedZoogis: string[];
  
  battlePassTier: number;
  battlePassXp: number;
  hasPremiumPass: boolean;
  claimedBattlePassTiers: number[];
  
  recentXpGains: XPGainEvent[];
  
  addXp: (amount: number, reason: string) => void;
  getXpForLevel: (level: number) => number;
  getXpProgress: () => { current: number; required: number; percent: number };
  
  claimReward: (rewardId: string) => void;
  equipTitle: (titleId: string | null) => void;
  equipSkin: (skinId: string | null) => void;
  
  addCoins: (amount: number) => void;
  spendCoins: (amount: number) => boolean;
  
  updateChallengeProgress: (type: "orbs" | "knockoffs" | "games" | "wins", amount: number) => void;
  refreshChallenges: () => void;
  getActiveChallenges: () => Challenge[];
  
  claimBattlePassTier: (tier: number) => void;
  purchasePremiumPass: () => void;
  
  getLevelRewards: () => LevelReward[];
  getBattlePassTiers: () => BattlePassTier[];
  getAvailableRewards: () => LevelReward[];
  
  clearRecentXpGains: () => void;
  removeXpGain: (timestamp: number) => void;
  
  recordGamePlayed: (zoogiId: string, won: boolean, score: number) => void;
  recordOrbsCollected: (count: number) => void;
  recordKnockout: () => void;
  checkLoginStreak: () => void;
  unlockZoogi: (zoogiId: string) => void;
  getFavoriteZoogi: () => string | null;
}

export const useProgression = create<ProgressionState>()(
  persist(
    (set, get) => ({
      xp: 0,
      level: 1,
      totalXpEarned: 0,
      
      unlockedRewards: [],
      equippedTitle: null,
      equippedSkin: null,
      
      coins: 0,
      xpBoostPercent: 0,
      
      challenges: [...generateDailyChallenges(), ...generateWeeklyChallenges()],
      lastChallengeRefresh: Date.now(),
      
      totalGamesPlayed: 0,
      totalWins: 0,
      totalOrbsCollected: 0,
      totalKnockouts: 0,
      highestScore: 0,
      loginStreak: 0,
      lastLoginDate: null,
      favoriteZoogi: null,
      zoogiPlayCount: {},
      ownedZoogis: ["wolfgang", "hotstreak", "lars", "pinpoint", "bolt"],
      
      battlePassTier: 0,
      battlePassXp: 0,
      hasPremiumPass: false,
      claimedBattlePassTiers: [],
      
      recentXpGains: [],
      
      addXp: (amount, reason) => {
        const state = get();
        const boostedAmount = Math.floor(amount * (1 + state.xpBoostPercent / 100));
        
        let newXp = state.xp + boostedAmount;
        let newLevel = state.level;
        let newUnlocked = [...state.unlockedRewards];
        let newCoins = state.coins;
        let newXpBoost = state.xpBoostPercent;
        
        while (newXp >= get().getXpForLevel(newLevel)) {
          newXp -= get().getXpForLevel(newLevel);
          newLevel++;
          
          const levelReward = LEVEL_REWARDS.find(r => r.level === newLevel);
          if (levelReward && !newUnlocked.includes(levelReward.id)) {
            newUnlocked.push(levelReward.id);
            
            if (levelReward.type === "coins") {
              const coinMatch = levelReward.name.match(/(\d+)/);
              if (coinMatch) newCoins += parseInt(coinMatch[1]);
            }
            if (levelReward.type === "xp_boost") {
              const boostMatch = levelReward.name.match(/(\d+)/);
              if (boostMatch) newXpBoost += parseInt(boostMatch[1]);
            }
          }
        }
        
        let newBattlePassXp = state.battlePassXp + boostedAmount;
        let newBattlePassTier = state.battlePassTier;
        
        while (newBattlePassTier < 50) {
          const tierData = BATTLE_PASS_TIERS[newBattlePassTier];
          if (tierData && newBattlePassXp >= tierData.xpRequired) {
            newBattlePassXp -= tierData.xpRequired;
            newBattlePassTier++;
          } else {
            break;
          }
        }
        
        if (newBattlePassTier >= 50) {
          newBattlePassTier = 50;
        }
        
        const xpEvent: XPGainEvent = {
          id: `xp-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`,
          amount: boostedAmount,
          reason,
          timestamp: Date.now()
        };
        
        set({
          xp: newXp,
          level: newLevel,
          totalXpEarned: state.totalXpEarned + boostedAmount,
          unlockedRewards: newUnlocked,
          coins: newCoins,
          xpBoostPercent: newXpBoost,
          battlePassXp: newBattlePassXp,
          battlePassTier: newBattlePassTier,
          recentXpGains: [...state.recentXpGains.slice(-4), xpEvent]
        });
      },
      
      getXpForLevel: (level) => {
        return Math.floor(100 * Math.pow(1.15, level - 1));
      },
      
      getXpProgress: () => {
        const state = get();
        const required = state.getXpForLevel(state.level);
        return {
          current: state.xp,
          required,
          percent: Math.min(100, (state.xp / required) * 100)
        };
      },
      
      claimReward: (rewardId) => {
        const state = get();
        if (!state.unlockedRewards.includes(rewardId)) {
          set({ unlockedRewards: [...state.unlockedRewards, rewardId] });
        }
      },
      
      equipTitle: (titleId) => set({ equippedTitle: titleId }),
      equipSkin: (skinId) => set({ equippedSkin: skinId }),
      
      addCoins: (amount) => set(state => ({ coins: state.coins + amount })),
      
      spendCoins: (amount) => {
        const state = get();
        if (state.coins >= amount) {
          set({ coins: state.coins - amount });
          return true;
        }
        return false;
      },
      
      updateChallengeProgress: (type, amount) => {
        const state = get();
        const now = Date.now();
        
        const validChallenges = state.challenges.filter(c => c.expiresAt > now);
        
        const typeMap: Record<string, string[]> = {
          orbs: ["daily_orbs", "weekly_orbs"],
          knockoffs: ["daily_knockoffs", "weekly_knockoffs"],
          games: ["daily_games"],
          wins: ["weekly_wins"]
        };
        
        const matchingPrefixes = typeMap[type] || [];
        let xpToAdd = 0;
        
        const updatedChallenges = validChallenges.map(challenge => {
          if (challenge.completed) return challenge;
          
          const matches = matchingPrefixes.some(prefix => challenge.id.startsWith(prefix));
          if (!matches) return challenge;
          
          const newProgress = Math.min(challenge.target, challenge.progress + amount);
          const nowCompleted = newProgress >= challenge.target;
          
          if (nowCompleted && !challenge.completed) {
            xpToAdd += challenge.xpReward;
          }
          
          return {
            ...challenge,
            progress: newProgress,
            completed: nowCompleted
          };
        });
        
        set({ challenges: updatedChallenges });
        
        if (xpToAdd > 0) {
          get().addXp(xpToAdd, "Challenge Complete");
        }
      },
      
      refreshChallenges: () => {
        const state = get();
        const now = Date.now();
        
        const validChallenges = state.challenges.filter(c => c.expiresAt > now);
        const hasDailies = validChallenges.some(c => c.type === "daily");
        const hasWeeklies = validChallenges.some(c => c.type === "weekly");
        
        let newChallenges = [...validChallenges];
        
        if (!hasDailies) {
          newChallenges = [...newChallenges, ...generateDailyChallenges()];
        }
        if (!hasWeeklies) {
          newChallenges = [...newChallenges, ...generateWeeklyChallenges()];
        }
        
        set({ challenges: newChallenges, lastChallengeRefresh: now });
      },
      
      getActiveChallenges: () => {
        const state = get();
        const now = Date.now();
        
        const validChallenges = state.challenges.filter(c => c.expiresAt > now);
        return validChallenges;
      },
      
      claimBattlePassTier: (tier) => {
        const state = get();
        if (state.claimedBattlePassTiers.includes(tier)) return;
        if (tier > state.battlePassTier) return;
        
        const tierData = BATTLE_PASS_TIERS[tier - 1];
        if (!tierData) return;
        
        let newCoins = state.coins;
        let newUnlocked = [...state.unlockedRewards];
        
        if (tierData.freeReward) {
          newUnlocked.push(tierData.freeReward.id);
          if (tierData.freeReward.type === "coins") {
            const match = tierData.freeReward.name.match(/(\d+)/);
            if (match) newCoins += parseInt(match[1]);
          }
        }
        
        if (state.hasPremiumPass && tierData.premiumReward) {
          newUnlocked.push(tierData.premiumReward.id);
        }
        
        set({
          claimedBattlePassTiers: [...state.claimedBattlePassTiers, tier],
          coins: newCoins,
          unlockedRewards: newUnlocked
        });
      },
      
      purchasePremiumPass: () => set({ hasPremiumPass: true }),
      
      getLevelRewards: () => LEVEL_REWARDS,
      getBattlePassTiers: () => BATTLE_PASS_TIERS,
      
      getAvailableRewards: () => {
        const state = get();
        return LEVEL_REWARDS.filter(r => r.level <= state.level);
      },
      
      clearRecentXpGains: () => set({ recentXpGains: [] }),
      
      removeXpGain: (timestamp: number) => set((state) => ({
        recentXpGains: state.recentXpGains.filter(g => g.timestamp !== timestamp)
      })),
      
      recordGamePlayed: (zoogiId: string, won: boolean, score: number) => {
        const state = get();
        const newPlayCount = { ...state.zoogiPlayCount };
        newPlayCount[zoogiId] = (newPlayCount[zoogiId] || 0) + 1;
        
        let favorite = state.favoriteZoogi;
        let maxPlays = 0;
        for (const [id, count] of Object.entries(newPlayCount)) {
          if (count > maxPlays) {
            maxPlays = count;
            favorite = id;
          }
        }
        
        set({
          totalGamesPlayed: state.totalGamesPlayed + 1,
          totalWins: state.totalWins + (won ? 1 : 0),
          highestScore: Math.max(state.highestScore, score),
          zoogiPlayCount: newPlayCount,
          favoriteZoogi: favorite
        });
      },
      
      recordOrbsCollected: (count: number) => set((state) => ({
        totalOrbsCollected: state.totalOrbsCollected + count
      })),
      
      recordKnockout: () => set((state) => ({
        totalKnockouts: state.totalKnockouts + 1
      })),
      
      checkLoginStreak: () => {
        const state = get();
        const today = new Date().toDateString();
        const yesterday = new Date(Date.now() - 86400000).toDateString();
        
        if (state.lastLoginDate === today) return;
        
        if (state.lastLoginDate === yesterday) {
          set({
            loginStreak: state.loginStreak + 1,
            lastLoginDate: today
          });
        } else {
          set({
            loginStreak: 1,
            lastLoginDate: today
          });
        }
      },
      
      unlockZoogi: (zoogiId: string) => set((state) => ({
        ownedZoogis: state.ownedZoogis.includes(zoogiId) 
          ? state.ownedZoogis 
          : [...state.ownedZoogis, zoogiId]
      })),
      
      getFavoriteZoogi: () => get().favoriteZoogi
    }),
    {
      name: "zoogi-progression",
      partialize: (state) => ({
        xp: state.xp,
        level: state.level,
        totalXpEarned: state.totalXpEarned,
        unlockedRewards: state.unlockedRewards,
        equippedTitle: state.equippedTitle,
        equippedSkin: state.equippedSkin,
        coins: state.coins,
        xpBoostPercent: state.xpBoostPercent,
        challenges: state.challenges,
        lastChallengeRefresh: state.lastChallengeRefresh,
        totalGamesPlayed: state.totalGamesPlayed,
        totalWins: state.totalWins,
        totalOrbsCollected: state.totalOrbsCollected,
        totalKnockouts: state.totalKnockouts,
        highestScore: state.highestScore,
        loginStreak: state.loginStreak,
        lastLoginDate: state.lastLoginDate,
        favoriteZoogi: state.favoriteZoogi,
        zoogiPlayCount: state.zoogiPlayCount,
        ownedZoogis: state.ownedZoogis,
        battlePassTier: state.battlePassTier,
        battlePassXp: state.battlePassXp,
        hasPremiumPass: state.hasPremiumPass,
        claimedBattlePassTiers: state.claimedBattlePassTiers
      }),
      merge: (persistedState: any, currentState) => ({
        ...currentState,
        ...persistedState,
        xp: persistedState?.xp ?? 0,
        level: persistedState?.level ?? 1,
        totalXpEarned: persistedState?.totalXpEarned ?? 0,
        unlockedRewards: persistedState?.unlockedRewards ?? [],
        equippedTitle: persistedState?.equippedTitle ?? null,
        equippedSkin: persistedState?.equippedSkin ?? null,
        coins: persistedState?.coins ?? 0,
        xpBoostPercent: persistedState?.xpBoostPercent ?? 0,
        challenges: persistedState?.challenges ?? [...generateDailyChallenges(), ...generateWeeklyChallenges()],
        lastChallengeRefresh: persistedState?.lastChallengeRefresh ?? Date.now(),
        totalGamesPlayed: persistedState?.totalGamesPlayed ?? 0,
        totalWins: persistedState?.totalWins ?? 0,
        totalOrbsCollected: persistedState?.totalOrbsCollected ?? 0,
        totalKnockouts: persistedState?.totalKnockouts ?? 0,
        highestScore: persistedState?.highestScore ?? 0,
        loginStreak: persistedState?.loginStreak ?? 0,
        lastLoginDate: persistedState?.lastLoginDate ?? null,
        favoriteZoogi: persistedState?.favoriteZoogi ?? null,
        zoogiPlayCount: persistedState?.zoogiPlayCount ?? {},
        ownedZoogis: persistedState?.ownedZoogis ?? ["wolfgang", "hotstreak", "lars", "pinpoint", "bolt"],
        battlePassTier: persistedState?.battlePassTier ?? 0,
        battlePassXp: persistedState?.battlePassXp ?? 0,
        hasPremiumPass: persistedState?.hasPremiumPass ?? false,
        claimedBattlePassTiers: persistedState?.claimedBattlePassTiers ?? []
      })
    }
  )
);

export const XP_VALUES = {
  ORB_COLLECT: 10,
  ENEMY_KNOCKOFF: 50,
  PLAYER_KNOCKOFF: 75,
  GAME_WIN: 100,
  GAME_PLAYED: 25,
  CHALLENGE_BONUS: 0
};
