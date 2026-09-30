import { useEffect, useState } from "react";
import { useCommunity } from "@/lib/stores/useCommunity";
import { useAuth } from "@/lib/stores/useAuth";
import { X, Trophy, Calendar, Users, Coins, Clock, ChevronRight } from "lucide-react";
import { OnlineNotice } from "@/components/ui/OnlineNotice";

interface TournamentsPanelProps {
  onClose: () => void;
}

export function TournamentsPanel({ onClose }: TournamentsPanelProps) {
  const { user } = useAuth();
  const { tournaments, fetchTournaments, joinTournament } = useCommunity();
  const [joining, setJoining] = useState<number | null>(null);

  useEffect(() => {
    fetchTournaments();
  }, [fetchTournaments]);

  const handleJoin = async (id: number) => {
    setJoining(id);
    await joinTournament(id);
    setJoining(null);
  };

  const formatTime = (dateStr: string) => {
    const date = new Date(dateStr);
    return date.toLocaleDateString(undefined, { 
      month: 'short', 
      day: 'numeric', 
      hour: '2-digit', 
      minute: '2-digit' 
    });
  };

  const getTimeRemaining = (endStr: string) => {
    const end = new Date(endStr);
    const now = new Date();
    const diff = end.getTime() - now.getTime();
    
    if (diff <= 0) return "Ended";
    
    const hours = Math.floor(diff / (1000 * 60 * 60));
    const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
    
    if (hours > 24) {
      const days = Math.floor(hours / 24);
      return `${days}d ${hours % 24}h`;
    }
    return `${hours}h ${minutes}m`;
  };

  const allTournaments = [...tournaments.active, ...tournaments.upcoming];

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-br from-rose-900/95 to-pink-900/95 rounded-2xl w-full max-w-2xl max-h-[90vh] overflow-hidden shadow-2xl border border-rose-500/30">
        <div className="flex items-center justify-between p-4 border-b border-rose-500/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-rose-500 to-pink-600 flex items-center justify-center">
              <Trophy className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-xl font-bold text-white">Tournaments</h2>
              <p className="text-rose-300 text-sm">
                {tournaments.active.length} active, {tournaments.upcoming.length} upcoming
              </p>
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

        <div className="p-4 overflow-y-auto max-h-[70vh]">
          {allTournaments.length === 0 ? (
            <div className="text-center py-12">
              <Trophy className="w-16 h-16 mx-auto text-rose-400 opacity-50 mb-4" />
              <p className="text-white/70 text-lg">No tournaments right now</p>
              <p className="text-rose-400 text-sm mt-2">Check back soon for new competitions!</p>
            </div>
          ) : (
            <div className="space-y-4">
              {tournaments.active.length > 0 && (
                <>
                  <h3 className="text-white font-semibold flex items-center gap-2">
                    <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse" />
                    Active Tournaments
                  </h3>
                  {tournaments.active.map((tournament) => (
                    <div
                      key={tournament.id}
                      className="bg-gradient-to-r from-green-900/30 to-emerald-900/30 rounded-xl p-4 border border-green-500/30"
                    >
                      <div className="flex items-start justify-between">
                        <div>
                          <h4 className="text-white font-bold text-lg">{tournament.name}</h4>
                          {tournament.description && (
                            <p className="text-white/60 text-sm mt-1">{tournament.description}</p>
                          )}
                        </div>
                        <div className="flex items-center gap-1 bg-green-500/20 px-2 py-1 rounded-full text-green-400 text-sm">
                          <Clock className="w-4 h-4" />
                          {getTimeRemaining(tournament.endTime)}
                        </div>
                      </div>
                      <div className="flex items-center gap-4 mt-3 text-sm">
                        {tournament.entryFee > 0 && (
                          <span className="flex items-center gap-1 text-yellow-400">
                            <Coins className="w-4 h-4" />
                            {tournament.entryFee} entry
                          </span>
                        )}
                        {tournament.maxParticipants && (
                          <span className="flex items-center gap-1 text-white/60">
                            <Users className="w-4 h-4" />
                            Max {tournament.maxParticipants}
                          </span>
                        )}
                      </div>
                      {user && (
                        <button
                          onClick={() => handleJoin(tournament.id)}
                          disabled={joining === tournament.id}
                          className="mt-3 w-full py-2 bg-green-500 text-white rounded-lg font-semibold hover:bg-green-600 transition-colors disabled:opacity-50"
                        >
                          {joining === tournament.id ? "Joining..." : "Join Tournament"}
                        </button>
                      )}
                    </div>
                  ))}
                </>
              )}

              {tournaments.upcoming.length > 0 && (
                <>
                  <h3 className="text-white font-semibold flex items-center gap-2 mt-6">
                    <Calendar className="w-4 h-4 text-rose-400" />
                    Upcoming Tournaments
                  </h3>
                  {tournaments.upcoming.map((tournament) => (
                    <div
                      key={tournament.id}
                      className="bg-gradient-to-r from-white/10 to-white/5 rounded-xl p-4 border border-white/10"
                    >
                      <div className="flex items-start justify-between">
                        <div>
                          <h4 className="text-white font-bold text-lg">{tournament.name}</h4>
                          {tournament.description && (
                            <p className="text-white/60 text-sm mt-1">{tournament.description}</p>
                          )}
                        </div>
                        <div className="text-rose-400 text-sm">
                          Starts {formatTime(tournament.startTime)}
                        </div>
                      </div>
                      <div className="flex items-center gap-4 mt-3 text-sm">
                        {tournament.entryFee > 0 && (
                          <span className="flex items-center gap-1 text-yellow-400">
                            <Coins className="w-4 h-4" />
                            {tournament.entryFee} entry
                          </span>
                        )}
                        {tournament.maxParticipants && (
                          <span className="flex items-center gap-1 text-white/60">
                            <Users className="w-4 h-4" />
                            Max {tournament.maxParticipants}
                          </span>
                        )}
                      </div>
                    </div>
                  ))}
                </>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
