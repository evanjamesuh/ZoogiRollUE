import { useEffect, useState } from "react";
import { useCommunity } from "@/lib/stores/useCommunity";
import { useAuth } from "@/lib/stores/useAuth";
import { X, Users, Crown, Trophy, Plus, LogOut, ChevronRight } from "lucide-react";

interface ClansPanelProps {
  onClose: () => void;
}

export function ClansPanel({ onClose }: ClansPanelProps) {
  const { user } = useAuth();
  const { 
    clans, 
    myClan,
    fetchClans, 
    fetchMyClan,
    createClan,
    joinClan,
    leaveClan
  } = useCommunity();
  
  const [tab, setTab] = useState<"browse" | "my-clan" | "create">(myClan ? "my-clan" : "browse");
  const [newClanName, setNewClanName] = useState("");
  const [newClanTag, setNewClanTag] = useState("");
  const [newClanDesc, setNewClanDesc] = useState("");
  const [creating, setCreating] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    fetchClans();
    if (user) {
      fetchMyClan();
    }
  }, [fetchClans, fetchMyClan, user]);

  useEffect(() => {
    if (myClan) {
      setTab("my-clan");
    }
  }, [myClan]);

  const handleCreateClan = async () => {
    if (!newClanName || !newClanTag) {
      setError("Name and tag are required");
      return;
    }
    if (newClanTag.length > 5) {
      setError("Tag must be 5 characters or less");
      return;
    }
    
    setCreating(true);
    setError("");
    const result = await createClan({
      name: newClanName,
      tag: newClanTag,
      description: newClanDesc,
    });
    setCreating(false);
    
    if (result) {
      setNewClanName("");
      setNewClanTag("");
      setNewClanDesc("");
      setTab("my-clan");
    } else {
      setError("Failed to create clan. Name or tag may be taken.");
    }
  };

  const handleJoinClan = async (clanId: number) => {
    const success = await joinClan(clanId);
    if (success) {
      setTab("my-clan");
    }
  };

  const handleLeaveClan = async () => {
    if (confirm("Are you sure you want to leave this clan?")) {
      await leaveClan();
      setTab("browse");
    }
  };

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-br from-amber-900/95 to-orange-900/95 rounded-2xl w-full max-w-2xl max-h-[90vh] overflow-hidden shadow-2xl border border-amber-500/30">
        <div className="flex items-center justify-between p-4 border-b border-amber-500/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-amber-500 to-orange-600 flex items-center justify-center">
              <Users className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Clans</h2>
              <p className="text-amber-300 text-sm">{clans.length} clans active</p>
            </div>
          </div>
          <button 
            onClick={onClose}
            className="p-2 hover:bg-white/10 rounded-lg transition-colors"
          >
            <X className="w-6 h-6 text-white" />
          </button>
        </div>

        <div className="flex items-center gap-2 p-4 border-b border-amber-500/20">
          <button
            onClick={() => setTab("browse")}
            className={`px-4 py-2 rounded-lg font-medium transition-colors ${
              tab === "browse" ? "bg-amber-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
            }`}
          >
            Browse
          </button>
          {user && myClan && (
            <button
              onClick={() => setTab("my-clan")}
              className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                tab === "my-clan" ? "bg-amber-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
              }`}
            >
              My Clan
            </button>
          )}
          {user && !myClan && (
            <button
              onClick={() => setTab("create")}
              className={`px-4 py-2 rounded-lg font-medium transition-colors flex items-center gap-2 ${
                tab === "create" ? "bg-amber-500 text-white" : "bg-white/10 text-white/70 hover:bg-white/20"
              }`}
            >
              <Plus className="w-4 h-4" />
              Create
            </button>
          )}
        </div>

        <div className="p-4 overflow-y-auto max-h-[60vh]">
          {tab === "browse" && (
            <>
              {clans.length === 0 ? (
                <div className="text-center py-12">
                  <Users className="w-16 h-16 mx-auto text-amber-400 opacity-50 mb-4" />
                  <p className="text-white/70 text-lg">No clans yet</p>
                  <p className="text-amber-400 text-sm mt-2">Be the first to create one!</p>
                </div>
              ) : (
                <div className="space-y-3">
                  {clans.map((clan) => (
                    <div
                      key={clan.id}
                      className="bg-gradient-to-r from-white/10 to-white/5 rounded-xl p-4 flex items-center gap-4"
                    >
                      <div className="w-14 h-14 rounded-xl bg-gradient-to-br from-amber-600 to-orange-700 flex items-center justify-center text-white font-bold text-lg">
                        [{clan.tag}]
                      </div>
                      <div className="flex-1">
                        <h3 className="text-white font-bold text-lg">{clan.name}</h3>
                        {clan.description && (
                          <p className="text-white/60 text-sm">{clan.description}</p>
                        )}
                        <div className="flex items-center gap-4 mt-1 text-sm">
                          <span className="flex items-center gap-1 text-amber-400">
                            <Users className="w-4 h-4" /> {clan.memberCount}
                          </span>
                          <span className="flex items-center gap-1 text-yellow-400">
                            <Trophy className="w-4 h-4" /> {clan.totalScore}
                          </span>
                        </div>
                      </div>
                      {user && !myClan && (
                        <button
                          onClick={() => handleJoinClan(clan.id)}
                          className="px-4 py-2 bg-amber-500 text-white rounded-lg font-semibold hover:bg-amber-600 transition-colors"
                        >
                          Join
                        </button>
                      )}
                    </div>
                  ))}
                </div>
              )}
            </>
          )}

          {tab === "my-clan" && myClan && (
            <div>
              <div className="bg-gradient-to-br from-amber-800/50 to-orange-800/50 rounded-xl p-6 mb-4">
                <div className="flex items-center gap-4 mb-4">
                  <div className="w-20 h-20 rounded-xl bg-gradient-to-br from-amber-500 to-orange-600 flex items-center justify-center text-white font-bold text-2xl">
                    [{myClan.clan.tag}]
                  </div>
                  <div>
                    <h3 className="text-2xl font-bold text-white">{myClan.clan.name}</h3>
                    {myClan.clan.description && (
                      <p className="text-white/70">{myClan.clan.description}</p>
                    )}
                    <div className="flex items-center gap-1 text-amber-300 mt-1">
                      {myClan.membership.role === "leader" ? (
                        <>
                          <Crown className="w-4 h-4" />
                          <span className="text-sm font-medium">Leader</span>
                        </>
                      ) : (
                        <span className="text-sm">Member</span>
                      )}
                    </div>
                  </div>
                </div>
                <div className="grid grid-cols-3 gap-3 mb-4">
                  <div className="bg-white/10 rounded-lg p-3 text-center">
                    <div className="text-white/60 text-xs">Members</div>
                    <div className="text-white font-bold text-xl">{myClan.clan.memberCount}</div>
                  </div>
                  <div className="bg-white/10 rounded-lg p-3 text-center">
                    <div className="text-white/60 text-xs">Total Wins</div>
                    <div className="text-white font-bold text-xl">{myClan.clan.totalWins}</div>
                  </div>
                  <div className="bg-white/10 rounded-lg p-3 text-center">
                    <div className="text-white/60 text-xs">Total Score</div>
                    <div className="text-white font-bold text-xl">{myClan.clan.totalScore}</div>
                  </div>
                </div>
                {myClan.membership.role !== "leader" && (
                  <button
                    onClick={handleLeaveClan}
                    className="w-full py-3 bg-red-500/30 text-red-300 rounded-xl font-semibold hover:bg-red-500/50 transition-colors flex items-center justify-center gap-2"
                  >
                    <LogOut className="w-5 h-5" />
                    Leave Clan
                  </button>
                )}
              </div>
              
              <h4 className="text-white font-semibold mb-3">Members ({myClan.members?.length || 0})</h4>
              <div className="space-y-2">
                {myClan.members?.map((member) => (
                  <div key={member.id} className="bg-white/10 rounded-lg p-3 flex items-center gap-3">
                    <div className="w-10 h-10 rounded-full bg-gradient-to-br from-amber-500 to-orange-600 flex items-center justify-center">
                      <Users className="w-5 h-5 text-white" />
                    </div>
                    <div className="flex-1">
                      <span className="text-white font-medium">User #{member.userId}</span>
                    </div>
                    {member.role === "leader" && (
                      <Crown className="w-5 h-5 text-yellow-400" />
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}

          {tab === "create" && (
            <div className="max-w-md mx-auto">
              <h3 className="text-xl font-bold text-white mb-4 text-center">Create a Clan</h3>
              
              {error && (
                <div className="bg-red-500/20 border border-red-500/50 rounded-lg p-3 mb-4 text-red-300 text-sm">
                  {error}
                </div>
              )}
              
              <div className="space-y-4">
                <div>
                  <label className="block text-white/70 text-sm mb-1">Clan Name</label>
                  <input
                    type="text"
                    value={newClanName}
                    onChange={(e) => setNewClanName(e.target.value)}
                    placeholder="Enter clan name"
                    className="w-full px-4 py-3 bg-white/10 border border-white/20 rounded-xl text-white placeholder-white/40 focus:outline-none focus:border-amber-500"
                  />
                </div>
                <div>
                  <label className="block text-white/70 text-sm mb-1">Clan Tag (max 5 chars)</label>
                  <input
                    type="text"
                    value={newClanTag}
                    onChange={(e) => setNewClanTag(e.target.value.toUpperCase().slice(0, 5))}
                    placeholder="ABC"
                    maxLength={5}
                    className="w-full px-4 py-3 bg-white/10 border border-white/20 rounded-xl text-white placeholder-white/40 focus:outline-none focus:border-amber-500 uppercase"
                  />
                </div>
                <div>
                  <label className="block text-white/70 text-sm mb-1">Description (optional)</label>
                  <textarea
                    value={newClanDesc}
                    onChange={(e) => setNewClanDesc(e.target.value)}
                    placeholder="Tell others about your clan"
                    rows={3}
                    className="w-full px-4 py-3 bg-white/10 border border-white/20 rounded-xl text-white placeholder-white/40 focus:outline-none focus:border-amber-500 resize-none"
                  />
                </div>
                <button
                  onClick={handleCreateClan}
                  disabled={creating || !newClanName || !newClanTag}
                  className="w-full py-4 bg-gradient-to-r from-amber-500 to-orange-500 text-white rounded-xl font-bold text-lg hover:from-amber-600 hover:to-orange-600 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
                >
                  {creating ? "Creating..." : "Create Clan"}
                </button>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
