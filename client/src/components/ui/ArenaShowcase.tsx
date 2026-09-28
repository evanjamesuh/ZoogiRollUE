import { useEffect, useState } from "react";
import { useCommunity } from "@/lib/stores/useCommunity";
import { useAuth } from "@/lib/stores/useAuth";
import { X, Heart, Download, Map, Plus, Trash2, Share2 } from "lucide-react";

interface ArenaShowcaseProps {
  onClose: () => void;
  onUseArena?: (arenaData: any) => void;
}

export function ArenaShowcase({ onClose, onUseArena }: ArenaShowcaseProps) {
  const { user } = useAuth();
  const { 
    publicArenas, 
    userArenas,
    fetchPublicArenas, 
    fetchUserArenas,
    userArenaVotes, 
    fetchUserVotes,
    voteArena,
    downloadArena,
    deleteArena,
    createArena
  } = useCommunity();
  
  const [tab, setTab] = useState<"browse" | "my-arenas">("browse");
  const [selectedArena, setSelectedArena] = useState<number | null>(null);
  const [sortBy, setSortBy] = useState<"likes" | "downloads" | "newest">("likes");
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [newArenaName, setNewArenaName] = useState("");
  const [newArenaDesc, setNewArenaDesc] = useState("");

  useEffect(() => {
    fetchPublicArenas();
    if (user) {
      fetchUserVotes();
      fetchUserArenas();
    }
  }, [fetchPublicArenas, fetchUserArenas, fetchUserVotes, user]);

  const sortedArenas = [...publicArenas].sort((a, b) => {
    if (sortBy === "likes") return b.likes - a.likes;
    if (sortBy === "downloads") return b.downloads - a.downloads;
    return new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime();
  });

  const handleVote = async (id: number) => {
    if (!user) return;
    if (!userArenaVotes.includes(id)) {
      await voteArena(id);
    }
  };

  const handleDownload = async (id: number) => {
    const arena = await downloadArena(id);
    if (arena && onUseArena) {
      onUseArena(arena.arenaData);
      onClose();
    }
  };

  const handleDelete = async (id: number) => {
    if (confirm("Are you sure you want to delete this arena?")) {
      await deleteArena(id);
    }
  };

  const arenas = tab === "browse" ? sortedArenas : userArenas;
  const selected = selectedArena !== null ? arenas.find(a => a.id === selectedArena) : null;

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-br from-emerald-900/95 to-teal-900/95 rounded-2xl w-full max-w-4xl max-h-[90vh] overflow-hidden shadow-2xl border border-emerald-500/30">
        <div className="flex items-center justify-between p-4 border-b border-emerald-500/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-emerald-500 to-teal-600 flex items-center justify-center">
              <Map className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Arena Showcase</h2>
              <p className="text-emerald-300 text-sm">{publicArenas.length} arenas shared</p>
            </div>
          </div>
          <button 
            onClick={onClose}
            className="p-2 hover:bg-white/10 rounded-lg transition-colors"
          >
            <X className="w-6 h-6 text-white" />
          </button>
        </div>

        <div className="flex items-center gap-2 p-4 border-b border-emerald-500/20">
          <button
            onClick={() => setTab("browse")}
            className={`px-4 py-2 rounded-lg font-medium transition-colors ${
              tab === "browse" ? "bg-emerald-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            Browse
          </button>
          {user && (
            <button
              onClick={() => setTab("my-arenas")}
              className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                tab === "my-arenas" ? "bg-emerald-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
              }`}
            >
              My Arenas ({userArenas.length})
            </button>
          )}
          
          {tab === "browse" && (
            <div className="flex items-center gap-2 ml-auto">
              <span className="text-emerald-300 text-sm">Sort:</span>
              {(["likes", "downloads", "newest"] as const).map(option => (
                <button
                  key={option}
                  onClick={() => setSortBy(option)}
                  className={`px-3 py-1 rounded-full text-sm capitalize transition-colors ${
                    sortBy === option ? "bg-emerald-500 text-white" : "bg-white/10 text-white/70"
                  }`}
                >
                  {option}
                </button>
              ))}
            </div>
          )}
        </div>

        <div className="p-4 overflow-y-auto max-h-[60vh]">
          {arenas.length === 0 ? (
            <div className="text-center py-12">
              <Map className="w-16 h-16 mx-auto text-emerald-400 opacity-50 mb-4" />
              <p className="text-white/70 text-lg">
                {tab === "browse" ? "No arenas shared yet" : "You haven't created any arenas"}
              </p>
              <p className="text-emerald-400 text-sm mt-2">
                {tab === "browse" ? "Be the first to share!" : "Create one in the Arena Editor"}
              </p>
            </div>
          ) : (
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-4">
              {arenas.map((arena) => (
                <div
                  key={arena.id}
                  onClick={() => setSelectedArena(arena.id)}
                  className="bg-gradient-to-br from-white/10 to-white/5 rounded-xl overflow-hidden cursor-pointer hover:scale-105 transition-transform border border-white/10 hover:border-emerald-500/50"
                >
                  <div className="aspect-video bg-gradient-to-br from-emerald-800/50 to-teal-800/50 flex items-center justify-center">
                    {arena.thumbnailUrl ? (
                      <img 
                        src={arena.thumbnailUrl} 
                        alt={arena.name}
                        className="w-full h-full object-cover"
                      />
                    ) : (
                      <Map className="w-12 h-12 text-emerald-400" />
                    )}
                  </div>
                  <div className="p-3">
                    <h3 className="text-white font-semibold truncate">{arena.name}</h3>
                    {arena.description && (
                      <p className="text-white/60 text-sm truncate">{arena.description}</p>
                    )}
                    <div className="flex items-center gap-3 mt-2 text-sm">
                      <span className="flex items-center gap-1 text-pink-400">
                        <Heart className="w-4 h-4" /> {arena.likes}
                      </span>
                      <span className="flex items-center gap-1 text-emerald-400">
                        <Download className="w-4 h-4" /> {arena.downloads}
                      </span>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {selected && (
          <div className="fixed inset-0 bg-black/90 flex items-center justify-center z-60 p-8">
            <div className="bg-gradient-to-br from-emerald-900 to-teal-900 rounded-2xl max-w-lg w-full overflow-hidden">
              <div className="aspect-video bg-gradient-to-br from-emerald-800/50 to-teal-800/50 flex items-center justify-center">
                {selected.thumbnailUrl ? (
                  <img 
                    src={selected.thumbnailUrl} 
                    alt={selected.name}
                    className="w-full h-full object-contain"
                  />
                ) : (
                  <Map className="w-24 h-24 text-emerald-400" />
                )}
              </div>
              <div className="p-6">
                <h3 className="text-2xl font-bold text-white mb-2">{selected.name}</h3>
                {selected.description && (
                  <p className="text-white/70 mb-4">{selected.description}</p>
                )}
                <div className="flex items-center gap-4 mb-4">
                  <div className="flex items-center gap-1 text-pink-400">
                    <Heart className="w-5 h-5" />
                    <span>{selected.likes} likes</span>
                  </div>
                  <div className="flex items-center gap-1 text-emerald-400">
                    <Download className="w-5 h-5" />
                    <span>{selected.downloads} downloads</span>
                  </div>
                </div>
                <div className="flex gap-3">
                  {tab === "browse" && (
                    <>
                      <button
                        onClick={() => handleVote(selected.id)}
                        disabled={!user || userArenaVotes.includes(selected.id)}
                        className={`flex-1 py-3 rounded-xl font-semibold transition-colors flex items-center justify-center gap-2 ${
                          userArenaVotes.includes(selected.id)
                            ? "bg-pink-500 text-white"
                            : "bg-white/10 text-white hover:bg-white/20"
                        } ${!user ? "opacity-50 cursor-not-allowed" : ""}`}
                      >
                        <Heart className={`w-5 h-5 ${userArenaVotes.includes(selected.id) ? "fill-current" : ""}`} />
                        {userArenaVotes.includes(selected.id) ? "Liked" : "Like"}
                      </button>
                      <button
                        onClick={() => handleDownload(selected.id)}
                        className="flex-1 py-3 bg-emerald-500 text-white rounded-xl font-semibold hover:bg-emerald-600 transition-colors flex items-center justify-center gap-2"
                      >
                        <Download className="w-5 h-5" />
                        Use Arena
                      </button>
                    </>
                  )}
                  {tab === "my-arenas" && (
                    <button
                      onClick={() => handleDelete(selected.id)}
                      className="flex-1 py-3 bg-red-500/80 text-white rounded-xl font-semibold hover:bg-red-600 transition-colors flex items-center justify-center gap-2"
                    >
                      <Trash2 className="w-5 h-5" />
                      Delete
                    </button>
                  )}
                  <button
                    onClick={() => setSelectedArena(null)}
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
