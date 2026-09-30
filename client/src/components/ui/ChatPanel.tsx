import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { X, Send, Hash, MessageCircle } from "lucide-react";
import { useChat } from "@/lib/stores/useChat";
import { useAuth } from "@/lib/stores/useAuth";
import { OnlineNotice } from "@/components/ui/OnlineNotice";
import { useOnlineMessage } from "@/lib/serverStatus";

interface ChatPanelProps {
  isOpen: boolean;
  onClose: () => void;
  directMessageFriendId?: number | null;
}

export function ChatPanel({ isOpen, onClose, directMessageFriendId }: ChatPanelProps) {
  const [message, setMessage] = useState("");
  const [activeTab, setActiveTab] = useState<"global" | "direct">(
    directMessageFriendId ? "direct" : "global"
  );
  const messagesEndRef = useRef<HTMLDivElement>(null);

  const { user } = useAuth();
  const onlineMessage = useOnlineMessage();
  const { messages, fetchMessages, fetchDirectMessages, sendMessage } = useChat();

  const currentChannel = activeTab === "direct" && directMessageFriendId 
    ? `direct_${directMessageFriendId}` 
    : "global";

  const currentMessages = messages[currentChannel] || [];

  useEffect(() => {
    if (isOpen && user) {
      if (activeTab === "global") {
        fetchMessages("global");
      } else if (directMessageFriendId) {
        fetchDirectMessages(directMessageFriendId);
      }
    }
  }, [isOpen, user, activeTab, directMessageFriendId, fetchMessages, fetchDirectMessages]);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [currentMessages]);

  useEffect(() => {
    if (directMessageFriendId) {
      setActiveTab("direct");
    }
  }, [directMessageFriendId]);

  const handleSend = async () => {
    if (!message.trim()) return;
    
    await sendMessage(
      message,
      activeTab === "direct" ? "direct" : "global",
      activeTab === "direct" ? directMessageFriendId ?? undefined : undefined
    );
    setMessage("");
  };

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="fixed bottom-4 right-4 w-80 h-96 bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl shadow-2xl border border-gray-700 z-50 flex flex-col overflow-hidden"
          initial={{ scale: 0.9, opacity: 0, y: 20 }}
          animate={{ scale: 1, opacity: 1, y: 0 }}
          exit={{ scale: 0.9, opacity: 0, y: 20 }}
        >
          <div className="p-3 border-b border-gray-700 flex items-center justify-between">
            <div className="flex gap-2">
              <button
                onClick={() => setActiveTab("global")}
                className={`flex items-center gap-1 px-3 py-1 rounded-full text-sm transition-colors ${
                  activeTab === "global"
                    ? "bg-purple-600 text-white"
                    : "bg-gray-700 text-gray-300 hover:bg-gray-600"
                }`}
              >
                <Hash size={14} />
                Global
              </button>
              {directMessageFriendId && (
                <button
                  onClick={() => setActiveTab("direct")}
                  className={`flex items-center gap-1 px-3 py-1 rounded-full text-sm transition-colors ${
                    activeTab === "direct"
                      ? "bg-purple-600 text-white"
                      : "bg-gray-700 text-gray-300 hover:bg-gray-600"
                  }`}
                >
                  <MessageCircle size={14} />
                  DM
                </button>
              )}
            </div>
            <button
              onClick={onClose}
              className="text-gray-400 hover:text-white transition-colors"
            >
              <X size={20} />
            </button>
          </div>

          <div className="flex-1 overflow-y-auto p-3 space-y-2">
            {onlineMessage ? (
              <OnlineNotice compact />
            ) : currentMessages.length === 0 ? (
              <div className="flex items-center justify-center h-full text-gray-500 text-sm">
                No messages yet. Say hello!
              </div>
            ) : (
              currentMessages.map((msg) => (
                <div
                  key={msg.id}
                  className={`flex flex-col ${
                    msg.senderId === user?.id ? "items-end" : "items-start"
                  }`}
                >
                  <div
                    className={`max-w-[80%] px-3 py-2 rounded-lg ${
                      msg.senderId === user?.id
                        ? "bg-purple-600 text-white"
                        : "bg-gray-700 text-gray-100"
                    }`}
                  >
                    {msg.senderId !== user?.id && (
                      <div className="text-xs text-gray-400 mb-1">User #{msg.senderId}</div>
                    )}
                    <div className="text-sm">{msg.message}</div>
                  </div>
                  <div className="text-xs text-gray-500 mt-1">
                    {new Date(msg.createdAt).toLocaleTimeString([], {
                      hour: "2-digit",
                      minute: "2-digit",
                    })}
                  </div>
                </div>
              ))
            )}
            <div ref={messagesEndRef} />
          </div>

          <div className="p-3 border-t border-gray-700">
            <div className="flex gap-2">
              <input
                type="text"
                value={message}
                onChange={(e) => setMessage(e.target.value)}
                onKeyPress={handleKeyPress}
                placeholder="Type a message..."
                className="flex-1 px-3 py-2 bg-gray-700 border border-gray-600 rounded-lg text-white placeholder-gray-400 focus:outline-none focus:ring-2 focus:ring-purple-500 text-sm"
                maxLength={500}
              />
              <button
                onClick={handleSend}
                disabled={!message.trim()}
                className="p-2 bg-purple-600 hover:bg-purple-500 disabled:bg-gray-600 disabled:cursor-not-allowed rounded-lg text-white transition-colors"
              >
                <Send size={18} />
              </button>
            </div>
          </div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
