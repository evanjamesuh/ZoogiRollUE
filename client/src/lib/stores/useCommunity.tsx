import { create } from "zustand";

interface PublicZoogi {
  id: number;
  name: string;
  meshyTaskId: string;
  thumbnailUrl: string | null;
  stats: any;
  likes: number;
  userId: number | null;
  createdAt: string;
}

interface PublicArena {
  id: number;
  userId: number;
  name: string;
  description: string | null;
  arenaData: any;
  thumbnailUrl: string | null;
  likes: number;
  downloads: number;
  createdAt: string;
}

interface Clan {
  id: number;
  name: string;
  tag: string;
  description: string | null;
  leaderId: number;
  iconUrl: string | null;
  totalWins: number;
  totalScore: number;
  memberCount: number;
}

interface ClanMember {
  id: number;
  clanId: number;
  userId: number;
  role: string;
  joinedAt: string;
}

interface Tournament {
  id: number;
  name: string;
  description: string | null;
  startTime: string;
  endTime: string;
  entryFee: number;
  prizePool: any;
  maxParticipants: number | null;
  status: string;
}

interface Challenge {
  id: number;
  name: string;
  description: string;
  type: string;
  target: number;
  reward: any;
  progress: number;
  completed: boolean;
  rewardClaimed: boolean;
}

interface DailyBonusState {
  canClaim: boolean;
  nextDay: number;
  loginStreak: number;
}

interface CommunityState {
  publicZoogis: PublicZoogi[];
  publicArenas: PublicArena[];
  userArenas: PublicArena[];
  userZoogiVotes: number[];
  userArenaVotes: number[];
  clans: Clan[];
  myClan: { clan: Clan; membership: ClanMember; members: ClanMember[] } | null;
  tournaments: { active: Tournament[]; upcoming: Tournament[] };
  challenges: Challenge[];
  dailyBonus: DailyBonusState | null;
  loading: boolean;
  
  fetchPublicZoogis: () => Promise<void>;
  fetchPublicArenas: () => Promise<void>;
  fetchUserArenas: () => Promise<void>;
  fetchUserVotes: () => Promise<void>;
  voteZoogi: (id: number) => Promise<boolean>;
  unvoteZoogi: (id: number) => Promise<boolean>;
  shareZoogi: (id: number) => Promise<boolean>;
  voteArena: (id: number) => Promise<boolean>;
  downloadArena: (id: number) => Promise<any>;
  createArena: (data: { name: string; description?: string; arenaData: any; isPublic?: boolean }) => Promise<any>;
  deleteArena: (id: number) => Promise<boolean>;
  
  fetchClans: () => Promise<void>;
  fetchMyClan: () => Promise<void>;
  createClan: (data: { name: string; tag: string; description?: string }) => Promise<any>;
  joinClan: (id: number) => Promise<boolean>;
  leaveClan: () => Promise<boolean>;
  
  fetchTournaments: () => Promise<void>;
  joinTournament: (id: number) => Promise<boolean>;
  
  fetchChallenges: () => Promise<void>;
  claimChallengeReward: (id: number) => Promise<boolean>;
  
  fetchDailyBonus: () => Promise<void>;
  claimDailyBonus: () => Promise<any>;
}

export const useCommunity = create<CommunityState>((set, get) => ({
  publicZoogis: [],
  publicArenas: [],
  userArenas: [],
  userZoogiVotes: [],
  userArenaVotes: [],
  clans: [],
  myClan: null,
  tournaments: { active: [], upcoming: [] },
  challenges: [],
  dailyBonus: null,
  loading: false,

  fetchPublicZoogis: async () => {
    try {
      const res = await fetch("/api/gallery/zoogis");
      const data = await res.json();
      set({ publicZoogis: data.data || [] });
    } catch (error) {
      console.error("Failed to fetch public zoogis:", error);
    }
  },

  fetchPublicArenas: async () => {
    try {
      const res = await fetch("/api/arenas/public");
      const data = await res.json();
      set({ publicArenas: data.data || [] });
    } catch (error) {
      console.error("Failed to fetch public arenas:", error);
    }
  },

  fetchUserArenas: async () => {
    try {
      const res = await fetch("/api/arenas/mine");
      const data = await res.json();
      set({ userArenas: data.data || [] });
    } catch (error) {
      console.error("Failed to fetch user arenas:", error);
    }
  },

  fetchUserVotes: async () => {
    try {
      const [zoogiRes, arenaRes] = await Promise.all([
        fetch("/api/gallery/votes"),
        fetch("/api/arenas/votes"),
      ]);
      const zoogiData = await zoogiRes.json();
      const arenaData = await arenaRes.json();
      set({ 
        userZoogiVotes: zoogiData.votes || [],
        userArenaVotes: arenaData.votes || [],
      });
    } catch (error) {
      console.error("Failed to fetch votes:", error);
    }
  },

  voteZoogi: async (id: number) => {
    try {
      const res = await fetch(`/api/gallery/zoogis/${id}/vote`, { method: "POST" });
      if (res.ok) {
        set(state => ({ userZoogiVotes: [...state.userZoogiVotes, id] }));
        get().fetchPublicZoogis();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to vote zoogi:", error);
      return false;
    }
  },

  unvoteZoogi: async (id: number) => {
    try {
      const res = await fetch(`/api/gallery/zoogis/${id}/vote`, { method: "DELETE" });
      if (res.ok) {
        set(state => ({ userZoogiVotes: state.userZoogiVotes.filter(v => v !== id) }));
        get().fetchPublicZoogis();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to unvote zoogi:", error);
      return false;
    }
  },

  shareZoogi: async (id: number) => {
    try {
      const res = await fetch(`/api/gallery/zoogis/${id}/share`, { method: "POST" });
      if (res.ok) {
        get().fetchPublicZoogis();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to share zoogi:", error);
      return false;
    }
  },

  voteArena: async (id: number) => {
    try {
      const res = await fetch(`/api/arenas/${id}/vote`, { method: "POST" });
      if (res.ok) {
        set(state => ({ userArenaVotes: [...state.userArenaVotes, id] }));
        get().fetchPublicArenas();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to vote arena:", error);
      return false;
    }
  },

  downloadArena: async (id: number) => {
    try {
      const res = await fetch(`/api/arenas/${id}/download`, { method: "POST" });
      const data = await res.json();
      return data.arena;
    } catch (error) {
      console.error("Failed to download arena:", error);
      return null;
    }
  },

  createArena: async (data) => {
    try {
      const res = await fetch("/api/arenas", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data),
      });
      const result = await res.json();
      if (res.ok) {
        get().fetchUserArenas();
        return result.arena;
      }
      return null;
    } catch (error) {
      console.error("Failed to create arena:", error);
      return null;
    }
  },

  deleteArena: async (id: number) => {
    try {
      const res = await fetch(`/api/arenas/${id}`, { method: "DELETE" });
      if (res.ok) {
        get().fetchUserArenas();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to delete arena:", error);
      return false;
    }
  },

  fetchClans: async () => {
    try {
      const res = await fetch("/api/clans");
      const data = await res.json();
      set({ clans: data.data || [] });
    } catch (error) {
      console.error("Failed to fetch clans:", error);
    }
  },

  fetchMyClan: async () => {
    try {
      const res = await fetch("/api/clans/mine");
      const data = await res.json();
      if (data.clan) {
        set({ myClan: data });
      } else {
        set({ myClan: null });
      }
    } catch (error) {
      console.error("Failed to fetch my clan:", error);
    }
  },

  createClan: async (data) => {
    try {
      const res = await fetch("/api/clans", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data),
      });
      const result = await res.json();
      if (res.ok) {
        get().fetchMyClan();
        get().fetchClans();
        return result.clan;
      }
      return null;
    } catch (error) {
      console.error("Failed to create clan:", error);
      return null;
    }
  },

  joinClan: async (id: number) => {
    try {
      const res = await fetch(`/api/clans/${id}/join`, { method: "POST" });
      if (res.ok) {
        get().fetchMyClan();
        get().fetchClans();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to join clan:", error);
      return false;
    }
  },

  leaveClan: async () => {
    try {
      const res = await fetch("/api/clans/leave", { method: "POST" });
      if (res.ok) {
        set({ myClan: null });
        get().fetchClans();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to leave clan:", error);
      return false;
    }
  },

  fetchTournaments: async () => {
    try {
      const res = await fetch("/api/tournaments");
      const data = await res.json();
      set({ tournaments: { active: data.active || [], upcoming: data.upcoming || [] } });
    } catch (error) {
      console.error("Failed to fetch tournaments:", error);
    }
  },

  joinTournament: async (id: number) => {
    try {
      const res = await fetch(`/api/tournaments/${id}/join`, { method: "POST" });
      if (res.ok) {
        get().fetchTournaments();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to join tournament:", error);
      return false;
    }
  },

  fetchChallenges: async () => {
    try {
      const res = await fetch("/api/challenges");
      const data = await res.json();
      set({ challenges: data.data || [] });
    } catch (error) {
      console.error("Failed to fetch challenges:", error);
    }
  },

  claimChallengeReward: async (id: number) => {
    try {
      const res = await fetch(`/api/challenges/${id}/claim`, { method: "POST" });
      if (res.ok) {
        get().fetchChallenges();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to claim challenge:", error);
      return false;
    }
  },

  fetchDailyBonus: async () => {
    try {
      const res = await fetch("/api/daily-bonus");
      const data = await res.json();
      set({ dailyBonus: data });
    } catch (error) {
      console.error("Failed to fetch daily bonus:", error);
    }
  },

  claimDailyBonus: async () => {
    try {
      const res = await fetch("/api/daily-bonus/claim", { method: "POST" });
      const data = await res.json();
      if (res.ok) {
        get().fetchDailyBonus();
        return data;
      }
      return null;
    } catch (error) {
      console.error("Failed to claim daily bonus:", error);
      return null;
    }
  },
}));
