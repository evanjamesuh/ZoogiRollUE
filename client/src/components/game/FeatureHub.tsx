import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { motion } from "framer-motion";
import { 
  Play, 
  Users, 
  Wand2, 
  MapPin, 
  Trophy, 
  Gift, 
  ShoppingBag,
  Map,
  Sparkles,
  BookOpen,
  Star,
  Image,
  Presentation,
  Shield,
  Swords,
  Video,
  Calendar,
  Radio,
  Home,
  Music
} from "lucide-react";
import { useState, type ReactNode } from "react";
import { CreateZoogi } from "./CreateZoogi";
import { MyCollections } from "./MyCollections";

interface FeatureButtonProps {
  icon: ReactNode;
  label: string;
  onClick: () => void;
  gradient: string;
  size?: "large" | "medium" | "small";
}

function FeatureButton({ icon, label, onClick, gradient, size = "small" }: FeatureButtonProps) {
  const sizeClasses = {
    large: "py-4 px-6 text-lg",
    medium: "py-3 px-4 text-base",
    small: "py-2 px-3 text-sm"
  };

  return (
    <motion.button
      whileHover={{ scale: 1.05 }}
      whileTap={{ scale: 0.95 }}
      onClick={onClick}
      className={`flex items-center justify-center gap-2 rounded-full font-bold text-white shadow-lg ${gradient} ${sizeClasses[size]}`}
    >
      {icon}
      <span>{label}</span>
    </motion.button>
  );
}

export function FeatureHub() {
  const { setPhase, setGameMode } = useZoogiGame();
  const [musicEnabled, setMusicEnabled] = useState(true);
  const [showCreateZoogi, setShowCreateZoogi] = useState(false);
  const [showCollections, setShowCollections] = useState(false);

  const handlePlayNow = () => {
    setGameMode("classic");
    setPhase("character_selection");
  };

  const handleMultiplayer = () => {
    setGameMode("local_multiplayer");
    setPhase("local_setup");
  };

  const handleBack = () => {
    setPhase("character_selection");
  };

  const handleShop = () => {
    setPhase("shop");
  };

  const handleWiki = () => {
    setPhase("zoogipedia");
  };

  const comingSoon = () => {
    alert("Coming Soon!");
  };

  if (showCreateZoogi) {
    return <CreateZoogi onBack={() => setShowCreateZoogi(false)} />;
  }

  if (showCollections) {
    return <MyCollections onBack={() => setShowCollections(false)} />;
  }

  return (
    <div className="fixed inset-0 flex flex-col overflow-hidden">
      <div className="absolute inset-0 bg-gradient-to-b from-orange-900/80 via-red-900/60 to-purple-900/80" />
      
      <div className="relative z-10 flex-1 flex flex-col p-4 overflow-y-auto">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-2">
            <motion.button
              whileHover={{ scale: 1.1 }}
              whileTap={{ scale: 0.9 }}
              onClick={handleBack}
              className="w-10 h-10 rounded-full bg-white/20 flex items-center justify-center text-white"
            >
              <Home size={20} />
            </motion.button>
            <motion.button
              whileHover={{ scale: 1.1 }}
              whileTap={{ scale: 0.9 }}
              onClick={() => setMusicEnabled(!musicEnabled)}
              className="w-10 h-10 rounded-full bg-white/20 flex items-center justify-center text-white"
            >
              <Music size={20} />
            </motion.button>
          </div>
          
          <motion.button
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={comingSoon}
            className="flex items-center gap-2 px-4 py-2 rounded-full bg-white/20 text-white font-semibold"
          >
            <span>→</span>
            <span>Sign In</span>
          </motion.button>
        </div>

        <div className="flex flex-col gap-3 mb-4">
          <motion.button
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
            onClick={handlePlayNow}
            className="w-full py-4 px-6 rounded-full bg-gradient-to-r from-green-400 to-green-600 text-white text-xl font-bold shadow-lg flex items-center justify-center gap-3"
          >
            <Play size={24} fill="white" />
            <span>Play Now</span>
          </motion.button>

          <motion.button
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
            onClick={handleMultiplayer}
            className="w-full py-4 px-6 rounded-full bg-gradient-to-r from-purple-500 to-violet-600 text-white text-xl font-bold shadow-lg flex items-center justify-center gap-3"
          >
            <Users size={24} />
            <span>Multiplayer</span>
          </motion.button>

        </div>

        <div className="grid grid-cols-2 gap-3 mb-4">
          <motion.button
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
            onClick={() => setShowCreateZoogi(true)}
            className="flex flex-col items-center justify-center gap-2 p-4 rounded-2xl bg-gradient-to-br from-pink-400 to-pink-600 text-white font-bold shadow-lg"
          >
            <Wand2 size={28} />
            <div className="flex items-center gap-1">
              <Sparkles size={12} />
              <span className="text-xs opacity-80">AI</span>
            </div>
            <span>Create Zoogi</span>
          </motion.button>

          <motion.button
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
            onClick={() => {
              setGameMode("map_editor");
              setPhase("map_selection");
            }}
            className="flex flex-col items-center justify-center gap-2 p-4 rounded-2xl bg-gradient-to-br from-teal-400 to-teal-600 text-white font-bold shadow-lg"
          >
            <MapPin size={28} />
            <span>Map Editor</span>
          </motion.button>
        </div>

        <div className="flex justify-center gap-2 mb-4">
          <div className="w-2 h-2 rounded-full bg-white/60" />
          <div className="w-2 h-2 rounded-full bg-white/30" />
        </div>

        <div className="grid grid-cols-2 gap-3 mb-4">
          <FeatureButton
            icon={<Trophy size={16} />}
            label="Challenges"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-slate-600 to-slate-700"
          />
          <FeatureButton
            icon={<Gift size={16} />}
            label="Battle Pass"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-orange-500 to-orange-600"
          />
        </div>

        <div className="grid grid-cols-4 gap-2 mb-4">
          <FeatureButton
            icon={<ShoppingBag size={14} />}
            label="Shop"
            onClick={handleShop}
            gradient="bg-gradient-to-r from-blue-500 to-blue-600"
          />
          <FeatureButton
            icon={<Map size={14} />}
            label="Arenas"
            onClick={() => setShowCollections(true)}
            gradient="bg-gradient-to-r from-orange-400 to-orange-500"
          />
          <FeatureButton
            icon={<Sparkles size={14} />}
            label="Create"
            onClick={() => setShowCreateZoogi(true)}
            gradient="bg-gradient-to-r from-blue-400 to-blue-500"
          />
          <FeatureButton
            icon={<BookOpen size={14} />}
            label="Wiki"
            onClick={handleWiki}
            gradient="bg-gradient-to-r from-yellow-400 to-yellow-500"
          />
        </div>

        <div className="grid grid-cols-4 gap-2 mb-4">
          <FeatureButton
            icon={<Star size={14} />}
            label="Scores"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-orange-400 to-orange-500"
          />
          <FeatureButton
            icon={<Image size={14} />}
            label="Gallery"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-green-400 to-green-500"
          />
          <FeatureButton
            icon={<Presentation size={14} />}
            label="Showcase"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-blue-400 to-blue-500"
          />
          <FeatureButton
            icon={<Shield size={14} />}
            label="Clans"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-orange-400 to-orange-500"
          />
        </div>

        <div className="grid grid-cols-4 gap-2 mb-6">
          <FeatureButton
            icon={<Swords size={14} />}
            label="Tourneys"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-purple-400 to-purple-500"
          />
          <FeatureButton
            icon={<Video size={14} />}
            label="Replays"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-green-400 to-green-500"
          />
          <FeatureButton
            icon={<Calendar size={14} />}
            label="Daily"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-purple-400 to-purple-500"
          />
          <FeatureButton
            icon={<Radio size={14} />}
            label="Radio"
            onClick={comingSoon}
            gradient="bg-gradient-to-r from-blue-400 to-blue-500"
          />
        </div>

        <div className="text-center text-white/40 text-xs">
          ZOOGI ROLL
        </div>
        <div className="text-center text-white/60 text-xs mt-1">
          Drag to launch your Zoogi into battle!
        </div>
      </div>
    </div>
  );
}
