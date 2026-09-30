import { useEffect, useState } from "react";
import { useCommunity } from "@/lib/stores/useCommunity";
import { useAuth } from "@/lib/stores/useAuth";
import { X, Heart, Download, ChevronLeft, ChevronRight, Eye, Sparkles } from "lucide-react";
import { OnlineNotice } from "@/components/ui/OnlineNotice";

interface ZoogiGalleryProps {
  onClose: () => void;
}

export function ZoogiGallery({ onClose }: ZoogiGalleryProps) {
  const { user } = useAuth();
  const { 
    publicZoogis, 
    fetchPublicZoogis, 
    userZoogiVotes, 
    fetchUserVotes,
    voteZoogi,
    unvoteZoogi
  } = useCommunity();
  
  const [selectedZoogi, setSelectedZoogi] = useState<number | null>(null);
  const [sortBy, setSortBy] = useState<"likes" | "newest">("likes");

  useEffect(() => {
    fetchPublicZoogis();
    if (user) {
      fetchUserVotes();
    }
  }, [fetchPublicZoogis, fetchUserVotes, user]);

  const sortedZoogis = [...publicZoogis].sort((a, b) => {
    if (sortBy === "likes") return b.likes - a.likes;
    return new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime();
  });

  const handleVote = async (id: number) => {
    if (!user) return;
    if (userZoogiVotes.includes(id)) {
      await unvoteZoogi(id);
    } else {
      await voteZoogi(id);
    }
  };

  const selected = selectedZoogi !== null ? sortedZoogis.find(z => z.id === selectedZoogi) : null;

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-br from-purple-900/95 to-indigo-900/95 rounded-2xl w-full max-w-4xl max-h-[90vh] overflow-hidden shadow-2xl border border-purple-500/30">
        <div className="flex items-center justify-between p-4 border-b border-purple-500/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-pink-500 to-purple-600 flex items-center justify-center">
              <Sparkles className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Community Gallery</h2>
              <p className="text-purple-300 text-sm">{publicZoogis.length} custom Zoogis shared</p>
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

        <div className="flex items-center gap-2 p-4 border-b border-purple-500/20">
          <span className="text-purple-300 text-sm">Sort by:</span>
          <button
            onClick={() => setSortBy("likes")}
            className={`px-3 py-1 rounded-full text-sm transition-colors ${
              sortBy === "likes" ? "bg-purple-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            Most Liked
          </button>
          <button
            onClick={() => setSortBy("newest")}
            className={`px-3 py-1 rounded-full text-sm transition-colors ${
              sortBy === "newest" ? "bg-purple-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            Newest
          </button>
        </div>

        <div className="p-4 overflow-y-auto max-h-[60vh]">
          {sortedZoogis.length === 0 ? (
            <div className="text-center py-12">
              <Sparkles className="w-16 h-16 mx-auto text-purple-400 opacity-50 mb-4" />
              <p className="text-white/70 text-lg">No Zoogis shared yet</p>
              <p className="text-purple-400 text-sm mt-2">Be the first to share your creation!</p>
            </div>
          ) : (
            <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
              {sortedZoogis.map((zoogi) => (
                <div
                  key={zoogi.id}
                  onClick={() => setSelectedZoogi(zoogi.id)}
                  className="bg-gradient-to-br from-white/10 to-white/5 rounded-xl overflow-hidden cursor-pointer hover:scale-105 transition-transform border border-white/10 hover:border-purple-500/50"
                >
                  <div className="aspect-square bg-gradient-to-br from-purple-800/50 to-indigo-800/50 flex items-center justify-center">
                    {zoogi.thumbnailUrl ? (
                      <img 
                        src={zoogi.thumbnailUrl} 
                        alt={zoogi.name}
                        className="w-full h-full object-cover"
                      />
                    ) : (
                      <Sparkles className="w-12 h-12 text-purple-400" />
                    )}
                  </div>
                  <div className="p-3">
                    <h3 className="text-white font-semibold truncate">{zoogi.name}</h3>
                    <div className="flex items-center justify-between mt-2">
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          handleVote(zoogi.id);
                        }}
                        disabled={!user}
                        className={`flex items-center gap-1 px-2 py-1 rounded-full text-sm transition-colors ${
                          userZoogiVotes.includes(zoogi.id)
                            ? "bg-pink-500 text-white"
                            : "bg-white/10 text-white/70 hover:bg-white/20"
                        } ${!user ? "opacity-50 cursor-not-allowed" : ""}`}
                      >
                        <Heart className={`w-4 h-4 ${userZoogiVotes.includes(zoogi.id) ? "fill-current" : ""}`} />
                        <span>{zoogi.likes}</span>
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {selected && (
          <div className="fixed inset-0 bg-black/90 flex items-center justify-center z-60 p-8">
            <div className="bg-gradient-to-br from-purple-900 to-indigo-900 rounded-2xl max-w-lg w-full overflow-hidden">
              <div className="aspect-square bg-gradient-to-br from-purple-800/50 to-indigo-800/50 flex items-center justify-center">
                {selected.thumbnailUrl ? (
                  <img 
                    src={selected.thumbnailUrl} 
                    alt={selected.name}
                    className="w-full h-full object-contain"
                  />
                ) : (
                  <Sparkles className="w-24 h-24 text-purple-400" />
                )}
              </div>
              <div className="p-6">
                <h3 className="text-2xl font-bold text-white mb-2">{selected.name}</h3>
                <div className="flex items-center gap-4 mb-4">
                  <div className="flex items-center gap-1 text-pink-400">
                    <Heart className="w-5 h-5 fill-current" />
                    <span>{selected.likes} likes</span>
                  </div>
                </div>
                {selected.stats && (
                  <div className="grid grid-cols-2 gap-2 mb-4">
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="text-white/60 text-xs">Power</div>
                      <div className="text-white font-bold">{(selected.stats as any).power || 5}</div>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="text-white/60 text-xs">Speed</div>
                      <div className="text-white font-bold">{(selected.stats as any).speed || 5}</div>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="text-white/60 text-xs">Weight</div>
                      <div className="text-white font-bold">{(selected.stats as any).weight || 5}</div>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="text-white/60 text-xs">Bounce</div>
                      <div className="text-white font-bold">{(selected.stats as any).bounce || 5}</div>
                    </div>
                  </div>
                )}
                <div className="flex gap-3">
                  <button
                    onClick={() => handleVote(selected.id)}
                    disabled={!user}
                    className={`flex-1 py-3 rounded-xl font-semibold transition-colors flex items-center justify-center gap-2 ${
                      userZoogiVotes.includes(selected.id)
                        ? "bg-pink-500 text-white"
                        : "bg-white/10 text-white hover:bg-white/20"
                    } ${!user ? "opacity-50 cursor-not-allowed" : ""}`}
                  >
                    <Heart className={`w-5 h-5 ${userZoogiVotes.includes(selected.id) ? "fill-current" : ""}`} />
                    {userZoogiVotes.includes(selected.id) ? "Liked" : "Like"}
                  </button>
                  <button
                    onClick={() => setSelectedZoogi(null)}
                    className="px-6 py-3 bg-white/10 text-white rounded-xl font-semibold hover:bg-white/20 transition-colors"
                  >
                    Close
                  </button>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
