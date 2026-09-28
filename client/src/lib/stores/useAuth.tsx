import { create } from "zustand";
import { persist } from "zustand/middleware";

export interface User {
  id: number;
  username: string;
  displayName: string | null;
  avatarUrl: string | null;
  favoriteZoogi: string | null;
  totalWins: number;
  totalLosses: number;
  totalKnockoffs: number;
  highScore: number;
  gamesPlayed: number;
  coins: number;
  gems: number;
  level: number;
  xp: number;
  loginStreak: number;
  createdAt: string;
}

export interface Friendship {
  id: number;
  userId: number;
  friendId: number;
  status: string;
  createdAt: string;
}

export interface SessionInfo {
  expiresAt: string | null;
  maxAge: number | null;
  daysRemaining: number;
}

interface AuthState {
  user: User | null;
  sessionInfo: SessionInfo | null;
  isLoading: boolean;
  error: string | null;
  friends: Friendship[];
  pendingRequests: Friendship[];
  
  login: (username: string, password: string) => Promise<boolean>;
  register: (username: string, password: string, displayName?: string) => Promise<boolean>;
  logout: () => Promise<void>;
  fetchMe: () => Promise<void>;
  updateProfile: (updates: Partial<Pick<User, "displayName" | "avatarUrl" | "favoriteZoogi">>) => Promise<void>;
  fetchFriends: () => Promise<void>;
  sendFriendRequest: (friendId: number) => Promise<void>;
  respondToFriendRequest: (friendshipId: number, accept: boolean) => Promise<void>;
  searchUsers: (query: string) => Promise<User[]>;
  clearError: () => void;
}

export const useAuth = create<AuthState>()(
  persist(
    (set, get) => ({
      user: null,
      sessionInfo: null,
      isLoading: false,
      error: null,
      friends: [],
      pendingRequests: [],

      login: async (username: string, password: string) => {
        set({ isLoading: true, error: null });
        try {
          const res = await fetch("/api/auth/login", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "include",
            body: JSON.stringify({ username, password }),
          });
          const data = await res.json();
          if (!res.ok) {
            set({ error: data.error, isLoading: false });
            return false;
          }
          set({ user: data.user, isLoading: false });
          return true;
        } catch (err) {
          set({ error: "Connection error", isLoading: false });
          return false;
        }
      },

      register: async (username: string, password: string, displayName?: string) => {
        set({ isLoading: true, error: null });
        try {
          const res = await fetch("/api/auth/register", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "include",
            body: JSON.stringify({ username, password, displayName }),
          });
          const data = await res.json();
          if (!res.ok) {
            set({ error: data.error, isLoading: false });
            return false;
          }
          set({ user: data.user, isLoading: false });
          return true;
        } catch (err) {
          set({ error: "Connection error", isLoading: false });
          return false;
        }
      },

      logout: async () => {
        try {
          await fetch("/api/auth/logout", { method: "POST", credentials: "include" });
        } catch (err) {
          console.error("Logout error:", err);
        }
        set({ user: null, sessionInfo: null, friends: [], pendingRequests: [] });
      },

      fetchMe: async () => {
        set({ isLoading: true });
        try {
          const res = await fetch("/api/auth/me", { credentials: "include" });
          const data = await res.json();
          set({ user: data.user, sessionInfo: data.sessionInfo, isLoading: false });
        } catch (err) {
          set({ isLoading: false });
        }
      },

      updateProfile: async (updates) => {
        const { user } = get();
        if (!user) return;
        
        try {
          const res = await fetch("/api/users/profile", {
            method: "PATCH",
            headers: { "Content-Type": "application/json" },
            credentials: "include",
            body: JSON.stringify(updates),
          });
          const data = await res.json();
          if (res.ok) {
            set({ user: data.user });
          }
        } catch (err) {
          console.error("Update profile error:", err);
        }
      },

      fetchFriends: async () => {
        try {
          const res = await fetch("/api/friends", { credentials: "include" });
          const data = await res.json();
          if (res.ok) {
            const { user } = get();
            const friends = data.friendships.filter((f: Friendship) => f.status === "accepted");
            const pendingRequests = data.friendships.filter(
              (f: Friendship) => f.status === "pending" && f.friendId === user?.id
            );
            set({ friends, pendingRequests });
          }
        } catch (err) {
          console.error("Fetch friends error:", err);
        }
      },

      sendFriendRequest: async (friendId: number) => {
        try {
          await fetch("/api/friends/request", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "include",
            body: JSON.stringify({ friendId }),
          });
        } catch (err) {
          console.error("Send friend request error:", err);
        }
      },

      respondToFriendRequest: async (friendshipId: number, accept: boolean) => {
        try {
          await fetch(`/api/friends/${friendshipId}`, {
            method: "PATCH",
            headers: { "Content-Type": "application/json" },
            credentials: "include",
            body: JSON.stringify({ status: accept ? "accepted" : "rejected" }),
          });
          get().fetchFriends();
        } catch (err) {
          console.error("Respond to friend request error:", err);
        }
      },

      searchUsers: async (query: string) => {
        try {
          const res = await fetch(`/api/users/search?q=${encodeURIComponent(query)}`, { credentials: "include" });
          const data = await res.json();
          return data.users || [];
        } catch (err) {
          console.error("Search users error:", err);
          return [];
        }
      },

      clearError: () => set({ error: null }),
    }),
    {
      name: "zoogi-auth",
      partialize: (state) => ({ user: state.user, sessionInfo: state.sessionInfo }),
    }
  )
);
