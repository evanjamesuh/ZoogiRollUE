import { useVoiceChat } from "@/lib/stores/useVoiceChat";
import { motion, AnimatePresence } from "framer-motion";
import { Mic, MicOff, Phone, PhoneOff, Volume2, VolumeX, AlertCircle } from "lucide-react";
import { useEffect, useState } from "react";

interface VoiceChatUIProps {
  roomId: string;
  teamName?: string;
}

export function VoiceChatUI({ roomId, teamName = "Team" }: VoiceChatUIProps) {
  const { 
    isConnected, 
    isConnecting, 
    isMuted, 
    peers, 
    error,
    connect, 
    disconnect, 
    toggleMute,
    setError 
  } = useVoiceChat();
  
  const [showError, setShowError] = useState(false);

  useEffect(() => {
    if (error) {
      setShowError(true);
      const timer = setTimeout(() => {
        setShowError(false);
        setError(null);
      }, 4000);
      return () => clearTimeout(timer);
    }
  }, [error, setError]);

  const handleToggleConnection = async () => {
    if (isConnected) {
      disconnect();
    } else {
      await connect(roomId);
    }
  };

  const peerCount = peers.size;

  return (
    <div className="flex flex-col items-center gap-2">
      <AnimatePresence>
        {showError && error && (
          <motion.div
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -10 }}
            className="absolute -top-12 left-1/2 -translate-x-1/2 bg-red-500/90 text-white px-3 py-1.5 rounded-lg text-xs flex items-center gap-2 whitespace-nowrap"
          >
            <AlertCircle size={14} />
            {error}
          </motion.div>
        )}
      </AnimatePresence>

      <div className="flex items-center gap-2">
        <motion.button
          whileTap={{ scale: 0.95 }}
          onClick={handleToggleConnection}
          disabled={isConnecting}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            isConnected
              ? "bg-green-500/80 text-white hover:bg-green-600/80"
              : isConnecting
              ? "bg-yellow-500/80 text-white animate-pulse"
              : "bg-black/70 text-white/80 hover:bg-black/90"
          }`}
          title={isConnected ? "Leave voice chat" : "Join voice chat"}
        >
          {isConnected ? (
            <Phone size={20} />
          ) : isConnecting ? (
            <Phone size={20} className="animate-pulse" />
          ) : (
            <PhoneOff size={20} />
          )}
        </motion.button>

        {isConnected && (
          <motion.button
            initial={{ opacity: 0, scale: 0.8 }}
            animate={{ opacity: 1, scale: 1 }}
            whileTap={{ scale: 0.95 }}
            onClick={toggleMute}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              isMuted
                ? "bg-red-500/80 text-white hover:bg-red-600/80"
                : "bg-blue-500/80 text-white hover:bg-blue-600/80"
            }`}
            title={isMuted ? "Unmute" : "Mute"}
          >
            {isMuted ? <MicOff size={20} /> : <Mic size={20} />}
          </motion.button>
        )}
      </div>

      {isConnected && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          className="flex items-center gap-1.5 text-xs text-white/80"
        >
          <Volume2 size={14} className="text-green-400" />
          <span>{peerCount} teammate{peerCount !== 1 ? "s" : ""}</span>
        </motion.div>
      )}

      {isConnected && peerCount > 0 && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          className="flex flex-wrap gap-1 justify-center"
        >
          {Array.from(peers.values()).map((peer) => (
            <div
              key={peer.id}
              className={`w-6 h-6 rounded-full flex items-center justify-center text-xs ${
                peer.isMuted
                  ? "bg-gray-500/80 text-white/60"
                  : "bg-green-500/80 text-white"
              }`}
              title={peer.isMuted ? "Muted" : "Speaking"}
            >
              {peer.isMuted ? <VolumeX size={12} /> : <Volume2 size={12} />}
            </div>
          ))}
        </motion.div>
      )}
    </div>
  );
}

export function VoiceChatButton({ roomId }: { roomId: string }) {
  const { isConnected, isConnecting, isMuted, connect, disconnect, toggleMute, error } = useVoiceChat();
  const [expanded, setExpanded] = useState(false);

  const handleConnect = async () => {
    if (!isConnected && !isConnecting) {
      await connect(roomId);
    }
    setExpanded(true);
  };

  return (
    <div className="relative">
      {!isConnected && !expanded ? (
        <motion.button
          whileTap={{ scale: 0.95 }}
          onClick={handleConnect}
          disabled={isConnecting}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            isConnecting
              ? "bg-yellow-500/70 text-white animate-pulse"
              : "bg-emerald-500/70 text-white hover:bg-emerald-600/80"
          }`}
          title="Join team voice chat"
        >
          <Phone size={20} />
        </motion.button>
      ) : (
        <motion.div
          initial={{ width: 48 }}
          animate={{ width: "auto" }}
          className="flex items-center gap-2 bg-black/50 rounded-xl p-1 backdrop-blur-sm"
        >
          <button
            onClick={() => {
              disconnect();
              setExpanded(false);
            }}
            className="p-2 rounded-lg bg-red-500/80 text-white hover:bg-red-600/80 transition-colors"
            title="Leave voice chat"
          >
            <PhoneOff size={18} />
          </button>
          
          {isConnected && (
            <button
              onClick={toggleMute}
              className={`p-2 rounded-lg transition-colors ${
                isMuted
                  ? "bg-orange-500/80 text-white hover:bg-orange-600/80"
                  : "bg-blue-500/80 text-white hover:bg-blue-600/80"
              }`}
              title={isMuted ? "Unmute" : "Mute"}
            >
              {isMuted ? <MicOff size={18} /> : <Mic size={18} />}
            </button>
          )}
        </motion.div>
      )}

      {error && (
        <motion.div
          initial={{ opacity: 0, y: 5 }}
          animate={{ opacity: 1, y: 0 }}
          className="absolute -bottom-8 left-1/2 -translate-x-1/2 bg-red-500/90 text-white px-2 py-1 rounded text-xs whitespace-nowrap"
        >
          {error}
        </motion.div>
      )}
    </div>
  );
}
