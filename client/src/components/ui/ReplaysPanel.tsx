import { useState, useEffect } from "react";
import { Film, Play, Trash2, Globe, Lock, Clock, Share2, User, ChevronRight, Download } from "lucide-react";
import { useAuth } from "@/lib/stores/useAuth";

interface Replay {
  id: number;
  userId: number;
  title: string;
  replayData: any;
  duration: number;
  isPublic: boolean;
  createdAt: string;
  username?: string;
}

interface ReplaysPanelProps {
  onClose: () => void;
  onPlayReplay?: (replayData: any) => void;
}

type Tab = "public" | "mine";

export function ReplaysPanel({ onClose, onPlayReplay }: ReplaysPanelProps) {
  const { user } = useAuth();
  const [activeTab, setActiveTab] = useState<Tab>("public");
  const [publicReplays, setPublicReplays] = useState<Replay[]>([]);
  const [myReplays, setMyReplays] = useState<Replay[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedReplay, setSelectedReplay] = useState<Replay | null>(null);

  useEffect(() => {
    fetchReplays();
  }, [user]);

  const fetchReplays = async () => {
    setLoading(true);
    try {
      const publicRes = await fetch("/api/replays/public?limit=50");
      if (publicRes.ok) {
        const data = await publicRes.json();
        setPublicReplays(data.data || []);
      }

      if (user) {
        const mineRes = await fetch("/api/replays/mine");
        if (mineRes.ok) {
          const data = await mineRes.json();
          setMyReplays(data.data || []);
        }
      }
    } catch (error) {
      console.error("Error fetching replays:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (replayId: number) => {
    if (!confirm("Delete this replay?")) return;
    
    try {
      const res = await fetch(`/api/replays/${replayId}`, {
        method: "DELETE"
      });
      
      if (res.ok) {
        setMyReplays(prev => prev.filter(r => r.id !== replayId));
        setPublicReplays(prev => prev.filter(r => r.id !== replayId));
      }
    } catch (error) {
      console.error("Error deleting replay:", error);
    }
  };

  const handleShare = async (replay: Replay) => {
    const shareUrl = `${window.location.origin}?replay=${replay.id}`;
    
    if (navigator.share) {
      try {
        await navigator.share({
          title: `Zoogi Roll Arena Replay: ${replay.title}`,
          text: `Watch this epic Zoogi Roll Arena match!`,
          url: shareUrl
        });
      } catch (error) {
        copyToClipboard(shareUrl);
      }
    } else {
      copyToClipboard(shareUrl);
    }
  };

  const copyToClipboard = (text: string) => {
    navigator.clipboard.writeText(text);
    alert("Link copied to clipboard!");
  };

  const formatDuration = (seconds: number) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, "0")}`;
  };

  const formatDate = (dateString: string) => {
    return new Date(dateString).toLocaleDateString("en-US", {
      month: "short",
      day: "numeric",
      hour: "2-digit",
      minute: "2-digit"
    });
  };

  const handlePlayReplay = (replay: Replay) => {
    if (onPlayReplay && replay.replayData) {
      onPlayReplay(replay.replayData);
      onClose();
    } else {
      setSelectedReplay(replay);
    }
  };

  const currentReplays = activeTab === "public" ? publicReplays : myReplays;

  return (
    <div
      className="absolute inset-0 flex items-center justify-center bg-black/80 z-50"
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl max-w-lg w-full mx-4 max-h-[85vh] overflow-hidden shadow-2xl border border-white/10">
        <div className="bg-gradient-to-r from-indigo-500 to-purple-500 p-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <Film className="w-8 h-8 text-white" />
              <div>
                <h2 className="text-xl font-bold text-white">Replays</h2>
                <p className="text-white/70 text-xs">Watch & share epic moments</p>
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
          <button
            onClick={() => setActiveTab("public")}
            className={`flex-1 flex items-center justify-center gap-2 py-3 text-sm font-semibold transition-all ${
              activeTab === "public"
                ? "text-indigo-400 border-b-2 border-indigo-400 bg-white/5"
                : "text-white/60 hover:text-white/80"
            }`}
          >
            <Globe className="w-4 h-4" />
            Community ({publicReplays.length})
          </button>
          {user && (
            <button
              onClick={() => setActiveTab("mine")}
              className={`flex-1 flex items-center justify-center gap-2 py-3 text-sm font-semibold transition-all ${
                activeTab === "mine"
                  ? "text-purple-400 border-b-2 border-purple-400 bg-white/5"
                  : "text-white/60 hover:text-white/80"
              }`}
            >
              <User className="w-4 h-4" />
              My Replays ({myReplays.length})
            </button>
          )}
        </div>

        <div className="p-4 overflow-y-auto max-h-[55vh]">
          {loading ? (
            <div className="flex items-center justify-center py-12">
              <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-indigo-400" />
            </div>
          ) : currentReplays.length === 0 ? (
            <div className="text-center py-12 text-white/50">
              <Film className="w-12 h-12 mx-auto mb-3 opacity-50" />
              <p>{activeTab === "public" ? "No community replays yet" : "You haven't saved any replays"}</p>
              <p className="text-xs mt-2">Complete a match and save it to share!</p>
            </div>
          ) : (
            <div className="space-y-3">
              {currentReplays.map((replay) => (
                <div
                  key={replay.id}
                  className="bg-white/5 rounded-xl border border-white/10 overflow-hidden hover:border-indigo-500/50 transition-all"
                >
                  <div className="p-3">
                    <div className="flex items-start justify-between mb-2">
                      <div className="flex-1 min-w-0">
                        <h3 className="text-white font-semibold truncate">{replay.title}</h3>
                        <div className="flex items-center gap-3 text-white/50 text-xs mt-1">
                          {replay.username && (
                            <span className="flex items-center gap-1">
                              <User className="w-3 h-3" />
                              {replay.username}
                            </span>
                          )}
                          <span className="flex items-center gap-1">
                            <Clock className="w-3 h-3" />
                            {formatDuration(replay.duration)}
                          </span>
                          <span>
                            {replay.isPublic ? (
                              <span className="flex items-center gap-1 text-green-400">
                                <Globe className="w-3 h-3" />
                                Public
                              </span>
                            ) : (
                              <span className="flex items-center gap-1">
                                <Lock className="w-3 h-3" />
                                Private
                              </span>
                            )}
                          </span>
                        </div>
                      </div>
                    </div>

                    <div className="flex items-center gap-2 mt-3">
                      <button
                        onClick={() => handlePlayReplay(replay)}
                        className="flex-1 flex items-center justify-center gap-2 py-2 bg-gradient-to-r from-indigo-500 to-purple-500 rounded-lg text-white text-sm font-semibold hover:opacity-90 transition-all"
                      >
                        <Play className="w-4 h-4" />
                        Watch
                      </button>
                      
                      {replay.isPublic && (
                        <button
                          onClick={() => handleShare(replay)}
                          className="p-2 bg-white/10 rounded-lg text-white/70 hover:text-white hover:bg-white/20 transition-all"
                        >
                          <Share2 className="w-4 h-4" />
                        </button>
                      )}
                      
                      {user && replay.userId === user.id && (
                        <button
                          onClick={() => handleDelete(replay.id)}
                          className="p-2 bg-red-500/20 rounded-lg text-red-400 hover:bg-red-500/30 transition-all"
                        >
                          <Trash2 className="w-4 h-4" />
                        </button>
                      )}
                    </div>
                  </div>
                  
                  <div className="px-3 py-2 bg-black/20 border-t border-white/5 text-white/40 text-xs">
                    {formatDate(replay.createdAt)}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {!user && (
          <div className="p-4 border-t border-white/10 bg-indigo-900/20">
            <p className="text-center text-white/60 text-sm">
              Log in to save and share your own replays!
            </p>
          </div>
        )}

        {selectedReplay && (
          <div
            className="absolute inset-0 flex items-center justify-center bg-black/90 z-60"
            onClick={() => setSelectedReplay(null)}
          >
            <div 
              className="bg-gray-800 rounded-xl p-6 max-w-sm w-full mx-4"
              onClick={(e) => e.stopPropagation()}
            >
              <h3 className="text-white font-bold text-lg mb-2">{selectedReplay.title}</h3>
              <p className="text-white/60 text-sm mb-4">
                Replay playback coming soon! For now, you can share the link with friends.
              </p>
              <div className="flex gap-2">
                <button
                  onClick={() => handleShare(selectedReplay)}
                  className="flex-1 py-2 bg-indigo-500 rounded-lg text-white font-semibold"
                >
                  Share Link
                </button>
                <button
                  onClick={() => setSelectedReplay(null)}
                  className="px-4 py-2 bg-white/10 rounded-lg text-white"
                >
                  Close
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
