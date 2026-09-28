import { create } from "zustand";

export interface ChatMessage {
  id: number;
  senderId: number;
  receiverId: number | null;
  channel: string;
  message: string;
  createdAt: string;
}

interface ChatState {
  messages: Record<string, ChatMessage[]>;
  activeChannel: string;
  isLoading: boolean;
  
  setActiveChannel: (channel: string) => void;
  fetchMessages: (channel: string) => Promise<void>;
  fetchDirectMessages: (friendId: number) => Promise<void>;
  sendMessage: (message: string, channel?: string, receiverId?: number) => Promise<void>;
}

export const useChat = create<ChatState>()((set, get) => ({
  messages: {},
  activeChannel: "global",
  isLoading: false,

  setActiveChannel: (channel: string) => {
    set({ activeChannel: channel });
  },

  fetchMessages: async (channel: string) => {
    set({ isLoading: true });
    try {
      const res = await fetch(`/api/chat/${channel}`);
      const data = await res.json();
      if (res.ok) {
        set((state) => ({
          messages: { ...state.messages, [channel]: data.messages },
          isLoading: false,
        }));
      }
    } catch (err) {
      console.error("Fetch messages error:", err);
      set({ isLoading: false });
    }
  },

  fetchDirectMessages: async (friendId: number) => {
    set({ isLoading: true });
    try {
      const res = await fetch(`/api/chat/direct/${friendId}`);
      const data = await res.json();
      if (res.ok) {
        const key = `direct_${friendId}`;
        set((state) => ({
          messages: { ...state.messages, [key]: data.messages },
          isLoading: false,
        }));
      }
    } catch (err) {
      console.error("Fetch DMs error:", err);
      set({ isLoading: false });
    }
  },

  sendMessage: async (message: string, channel?: string, receiverId?: number) => {
    try {
      const res = await fetch("/api/chat", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message, channel: channel || "global", receiverId }),
      });
      const data = await res.json();
      if (res.ok) {
        const key = receiverId ? `direct_${receiverId}` : (channel || "global");
        set((state) => ({
          messages: {
            ...state.messages,
            [key]: [...(state.messages[key] || []), data.message],
          },
        }));
      }
    } catch (err) {
      console.error("Send message error:", err);
    }
  },
}));
