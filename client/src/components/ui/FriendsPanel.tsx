import { useState, useEffect } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { X, Search, UserPlus, Check, X as XIcon, MessageCircle, Loader2 } from "lucide-react";
import { useAuth, User, Friendship } from "@/lib/stores/useAuth";
import { OnlineNotice } from "@/components/ui/OnlineNotice";

interface FriendsPanelProps {
  isOpen: boolean;
  onClose: () => void;
  onOpenChat: (friendId: number) => void;
}

export function FriendsPanel({ isOpen, onClose, onOpenChat }: FriendsPanelProps) {
  const [searchQuery, setSearchQuery] = useState("");
  const [searchResults, setSearchResults] = useState<User[]>([]);
  const [isSearching, setIsSearching] = useState(false);
  const [sentRequests, setSentRequests] = useState<Set<number>>(new Set());

  const { user, friends, pendingRequests, fetchFriends, searchUsers, sendFriendRequest, respondToFriendRequest } = useAuth();

  useEffect(() => {
    if (isOpen && user) {
      fetchFriends();
    }
  }, [isOpen, user, fetchFriends]);

  useEffect(() => {
    const search = async () => {
      if (searchQuery.length < 2) {
        setSearchResults([]);
        return;
      }
      setIsSearching(true);
      const results = await searchUsers(searchQuery);
      setSearchResults(results.filter((u) => u.id !== user?.id));
      setIsSearching(false);
    };
    const timeout = setTimeout(search, 300);
    return () => clearTimeout(timeout);
  }, [searchQuery, searchUsers, user?.id]);

  const handleSendRequest = async (friendId: number) => {
    await sendFriendRequest(friendId);
    setSentRequests((prev) => new Set(prev).add(friendId));
  };

  const isFriend = (userId: number) => {
    return friends.some(
      (f) => (f.userId === userId || f.friendId === userId) && f.status === "accepted"
    );
  };

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
            className="relative w-full max-w-md mx-4 max-h-[80vh] bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl shadow-2xl border border-gray-700 overflow-hidden flex flex-col"
            initial={{ scale: 0.9, opacity: 0, y: 20 }}
            animate={{ scale: 1, opacity: 1, y: 0 }}
            exit={{ scale: 0.9, opacity: 0, y: 20 }}
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-4 border-b border-gray-700 flex items-center justify-between">
              <h2 className="text-xl font-bold text-white">Friends</h2>
              <button
                onClick={onClose}
                className="text-gray-400 hover:text-white transition-colors"
              >
                <X size={24} />
              </button>
            </div>

            <OnlineNotice compact className="mx-4 mt-3" />

            <div className="p-4">
              <div className="relative">
                <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-400" size={18} />
                <input
                  type="text"
                  placeholder="Search players..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="w-full pl-10 pr-4 py-2 bg-gray-700/50 border border-gray-600 rounded-lg text-white placeholder-gray-400 focus:outline-none focus:ring-2 focus:ring-purple-500"
                />
                {isSearching && (
                  <Loader2 className="absolute right-3 top-1/2 -translate-y-1/2 text-gray-400 animate-spin" size={18} />
                )}
              </div>
            </div>

            <div className="flex-1 overflow-y-auto px-4 pb-4 space-y-4">
              {searchQuery.length >= 2 && (
                <div>
                  <h3 className="text-sm font-medium text-gray-400 mb-2">Search Results</h3>
                  {searchResults.length === 0 && !isSearching ? (
                    <p className="text-gray-500 text-sm">No players found</p>
                  ) : (
                    <div className="space-y-2">
                      {searchResults.map((result) => (
                        <div
                          key={result.id}
                          className="flex items-center justify-between p-3 bg-gray-700/50 rounded-lg"
                        >
                          <div className="flex items-center gap-3">
                            <div className="w-10 h-10 rounded-full bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center text-white font-bold">
                              {(result.displayName || result.username)[0].toUpperCase()}
                            </div>
                            <div>
                              <div className="text-white font-medium">
                                {result.displayName || result.username}
                              </div>
                              <div className="text-sm text-gray-400">@{result.username}</div>
                            </div>
                          </div>
                          {isFriend(result.id) ? (
                            <button
                              onClick={() => onOpenChat(result.id)}
                              className="p-2 bg-purple-600 hover:bg-purple-500 rounded-lg text-white"
                            >
                              <MessageCircle size={18} />
                            </button>
                          ) : sentRequests.has(result.id) ? (
                            <span className="text-sm text-gray-400">Request Sent</span>
                          ) : (
                            <button
                              onClick={() => handleSendRequest(result.id)}
                              className="p-2 bg-green-600 hover:bg-green-500 rounded-lg text-white"
                            >
                              <UserPlus size={18} />
                            </button>
                          )}
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}

              {pendingRequests.length > 0 && (
                <div>
                  <h3 className="text-sm font-medium text-gray-400 mb-2">Friend Requests</h3>
                  <div className="space-y-2">
                    {pendingRequests.map((req) => (
                      <div
                        key={req.id}
                        className="flex items-center justify-between p-3 bg-yellow-500/10 border border-yellow-500/30 rounded-lg"
                      >
                        <div className="flex items-center gap-3">
                          <div className="w-10 h-10 rounded-full bg-gradient-to-br from-yellow-500 to-orange-500 flex items-center justify-center text-white font-bold">
                            ?
                          </div>
                          <div className="text-white font-medium">New Request</div>
                        </div>
                        <div className="flex gap-2">
                          <button
                            onClick={() => respondToFriendRequest(req.id, true)}
                            className="p-2 bg-green-600 hover:bg-green-500 rounded-lg text-white"
                          >
                            <Check size={18} />
                          </button>
                          <button
                            onClick={() => respondToFriendRequest(req.id, false)}
                            className="p-2 bg-red-600 hover:bg-red-500 rounded-lg text-white"
                          >
                            <XIcon size={18} />
                          </button>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              <div>
                <h3 className="text-sm font-medium text-gray-400 mb-2">
                  Your Friends ({friends.length})
                </h3>
                {friends.length === 0 ? (
                  <p className="text-gray-500 text-sm">No friends yet. Search for players to add!</p>
                ) : (
                  <div className="space-y-2">
                    {friends.map((friendship) => {
                      const friendId = friendship.userId === user?.id ? friendship.friendId : friendship.userId;
                      return (
                        <div
                          key={friendship.id}
                          className="flex items-center justify-between p-3 bg-gray-700/50 rounded-lg"
                        >
                          <div className="flex items-center gap-3">
                            <div className="w-10 h-10 rounded-full bg-gradient-to-br from-green-500 to-teal-500 flex items-center justify-center text-white font-bold">
                              F
                            </div>
                            <div className="text-white font-medium">Friend #{friendId}</div>
                          </div>
                          <button
                            onClick={() => onOpenChat(friendId)}
                            className="p-2 bg-purple-600 hover:bg-purple-500 rounded-lg text-white"
                          >
                            <MessageCircle size={18} />
                          </button>
                        </div>
                      );
                    })}
                  </div>
                )}
              </div>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
