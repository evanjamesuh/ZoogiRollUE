import { useState } from "react";
import { X, User, Sparkles, Check, Upload, Palette } from "lucide-react";
import { useProgression } from "@/lib/stores/useProgression";
import { useAuth } from "@/lib/stores/useAuth";
import { ZOOGI_ROSTER } from "@/lib/stores/useZoogiGame";

interface CustomizationPanelProps {
  onClose: () => void;
}

const PRESET_AVATARS = [
  { id: "avatar_default", url: "/avatars/default.png", name: "Default" },
  { id: "avatar_wolf", url: "/avatars/wolf.png", name: "Wolf" },
  { id: "avatar_flame", url: "/avatars/flame.png", name: "Flame" },
  { id: "avatar_ice", url: "/avatars/ice.png", name: "Ice" },
  { id: "avatar_electric", url: "/avatars/electric.png", name: "Electric" },
  { id: "avatar_cosmic", url: "/avatars/cosmic.png", name: "Cosmic" },
];

const ZOOGI_SKINS = [
  { id: "skin_default", name: "Default", description: "Original look", color: null, trail: null, glow: null, rarity: "common" as const },
  { id: "skin_golden_trail", name: "Golden Trail", description: "Leave a shimmering gold path", color: "#FFD700", trail: "gold", glow: null, rarity: "rare" as const },
  { id: "skin_neon", name: "Neon Glow", description: "Electrifying neon effects", color: "#00FFFF", trail: null, glow: "cyan", rarity: "epic" as const },
  { id: "skin_fire", name: "Fire Aura", description: "Blazing fire effects", color: "#FF4500", trail: "fire", glow: "orange", rarity: "epic" as const },
  { id: "skin_ice", name: "Ice Crystal", description: "Frozen ice effects", color: "#87CEEB", trail: "ice", glow: "blue", rarity: "rare" as const },
  { id: "skin_shadow", name: "Shadow Form", description: "Dark mysterious aura", color: "#1a1a2e", trail: "shadow", glow: "purple", rarity: "legendary" as const },
  { id: "skin_rainbow", name: "Rainbow Shift", description: "Ever-changing colors", color: "rainbow", trail: "rainbow", glow: "rainbow", rarity: "legendary" as const },
];

const AVATAR_COLORS = [
  "#FF4444", "#FF8800", "#FFCC00", "#44FF44", "#00CCFF", "#4444FF", "#AA44FF", "#FF44AA"
];

function getRarityColor(rarity: string): string {
  switch (rarity) {
    case "common": return "text-gray-400";
    case "rare": return "text-blue-400";
    case "epic": return "text-purple-400";
    case "legendary": return "text-yellow-400";
    default: return "text-gray-400";
  }
}

function getRarityBorder(rarity: string): string {
  switch (rarity) {
    case "common": return "border-gray-500";
    case "rare": return "border-blue-500";
    case "epic": return "border-purple-500";
    case "legendary": return "border-yellow-500 animate-pulse";
    default: return "border-gray-500";
  }
}

export function CustomizationPanel({ onClose }: CustomizationPanelProps) {
  const [activeTab, setActiveTab] = useState<"avatar" | "skins">("avatar");
  const [selectedAvatarColor, setSelectedAvatarColor] = useState("#4444FF");
  const { user, updateProfile } = useAuth();
  const { equippedSkin, equipSkin, unlockedRewards, level } = useProgression();

  const handleSelectAvatar = async (avatarUrl: string) => {
    await updateProfile({ avatarUrl });
  };

  const handleSelectSkin = (skinId: string) => {
    equipSkin(skinId === "skin_default" ? null : skinId);
  };

  const isSkinUnlocked = (skin: typeof ZOOGI_SKINS[0]) => {
    if (skin.id === "skin_default") return true;
    if (unlockedRewards.includes(skin.id)) return true;
    if (skin.id === "skin_golden_trail" && level >= 7) return true;
    if (skin.id === "skin_neon" && level >= 25) return true;
    return false;
  };

  const getUnlockRequirement = (skin: typeof ZOOGI_SKINS[0]): string | null => {
    if (isSkinUnlocked(skin)) return null;
    if (skin.id === "skin_golden_trail") return "Reach Level 7";
    if (skin.id === "skin_neon") return "Reach Level 25";
    if (skin.id === "skin_fire") return "Battle Pass Tier 15";
    if (skin.id === "skin_ice") return "Battle Pass Tier 10";
    if (skin.id === "skin_shadow") return "Battle Pass Tier 40";
    if (skin.id === "skin_rainbow") return "Battle Pass Tier 50";
    return "Unlock through progression";
  };

  return (
    <div className="fixed inset-0 bg-black/80 flex items-center justify-center z-50 p-4">
      <div className="bg-gradient-to-b from-gray-900 to-gray-800 rounded-2xl max-w-2xl w-full max-h-[90vh] overflow-hidden border border-white/10">
        <div className="flex items-center justify-between p-4 border-b border-white/10">
          <h2 className="text-xl font-bold text-white flex items-center gap-2">
            <Palette className="w-6 h-6 text-purple-400" />
            Customization
          </h2>
          <button
            onClick={onClose}
            className="p-2 rounded-lg hover:bg-white/10 transition-colors"
          >
            <X className="w-5 h-5 text-white" />
          </button>
        </div>

        <div className="flex border-b border-white/10">
          <button
            onClick={() => setActiveTab("avatar")}
            className={`flex-1 p-3 flex items-center justify-center gap-2 transition-colors ${
              activeTab === "avatar"
                ? "bg-purple-600/30 text-purple-300 border-b-2 border-purple-500"
                : "text-gray-400 hover:text-white hover:bg-white/5"
            }`}
          >
            <User className="w-4 h-4" />
            Player Avatar
          </button>
          <button
            onClick={() => setActiveTab("skins")}
            className={`flex-1 p-3 flex items-center justify-center gap-2 transition-colors ${
              activeTab === "skins"
                ? "bg-purple-600/30 text-purple-300 border-b-2 border-purple-500"
                : "text-gray-400 hover:text-white hover:bg-white/5"
            }`}
          >
            <Sparkles className="w-4 h-4" />
            Zoogi Skins
          </button>
        </div>

        <div className="p-4 overflow-y-auto max-h-[60vh]">
          {activeTab === "avatar" && (
            <div className="space-y-6">
              <div>
                <h3 className="text-sm font-medium text-gray-400 mb-3">Current Avatar</h3>
                <div className="flex items-center gap-4">
                  <div 
                    className="w-20 h-20 rounded-full flex items-center justify-center text-2xl font-bold text-white border-2 border-purple-500"
                    style={{ backgroundColor: selectedAvatarColor }}
                  >
                    {user?.displayName?.[0]?.toUpperCase() || user?.username?.[0]?.toUpperCase() || "?"}
                  </div>
                  <div>
                    <p className="text-white font-medium">{user?.displayName || user?.username || "Guest"}</p>
                    <p className="text-gray-400 text-sm">Level {level}</p>
                  </div>
                </div>
              </div>

              <div>
                <h3 className="text-sm font-medium text-gray-400 mb-3">Avatar Color</h3>
                <div className="grid grid-cols-8 gap-2">
                  {AVATAR_COLORS.map((color) => (
                    <button
                      key={color}
                      onClick={() => setSelectedAvatarColor(color)}
                      className={`w-10 h-10 rounded-full border-2 transition-all ${
                        selectedAvatarColor === color 
                          ? "border-white scale-110" 
                          : "border-transparent hover:border-white/50"
                      }`}
                      style={{ backgroundColor: color }}
                    >
                      {selectedAvatarColor === color && (
                        <Check className="w-5 h-5 text-white mx-auto" />
                      )}
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <h3 className="text-sm font-medium text-gray-400 mb-3">Preset Avatars</h3>
                <div className="grid grid-cols-3 sm:grid-cols-6 gap-3">
                  {PRESET_AVATARS.map((avatar) => (
                    <button
                      key={avatar.id}
                      onClick={() => handleSelectAvatar(avatar.url)}
                      className={`p-2 rounded-lg border transition-all ${
                        user?.avatarUrl === avatar.url
                          ? "border-purple-500 bg-purple-500/20"
                          : "border-white/10 hover:border-purple-500/50 hover:bg-white/5"
                      }`}
                    >
                      <div className="w-12 h-12 rounded-full bg-gradient-to-br from-gray-600 to-gray-700 mx-auto flex items-center justify-center">
                        <User className="w-6 h-6 text-gray-400" />
                      </div>
                      <p className="text-xs text-center text-gray-300 mt-1">{avatar.name}</p>
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <h3 className="text-sm font-medium text-gray-400 mb-3">Upload Custom Avatar</h3>
                <label className="flex items-center justify-center gap-2 p-4 border-2 border-dashed border-white/20 rounded-lg hover:border-purple-500/50 hover:bg-white/5 cursor-pointer transition-all">
                  <Upload className="w-5 h-5 text-gray-400" />
                  <span className="text-gray-400">Click to upload image</span>
                  <input
                    type="file"
                    accept="image/*"
                    className="hidden"
                    onChange={(e) => {
                      const file = e.target.files?.[0];
                      if (file) {
                        const reader = new FileReader();
                        reader.onloadend = () => {
                          handleSelectAvatar(reader.result as string);
                        };
                        reader.readAsDataURL(file);
                      }
                    }}
                  />
                </label>
              </div>
            </div>
          )}

          {activeTab === "skins" && (
            <div className="space-y-4">
              <p className="text-gray-400 text-sm">
                Customize your Zoogi's appearance with unique skins. Skins apply visual effects like trails, glows, and color changes.
              </p>
              
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                {ZOOGI_SKINS.map((skin) => {
                  const unlocked = isSkinUnlocked(skin);
                  const isEquipped = equippedSkin === skin.id || (skin.id === "skin_default" && !equippedSkin);
                  const unlockReq = getUnlockRequirement(skin);

                  return (
                    <button
                      key={skin.id}
                      onClick={() => unlocked && handleSelectSkin(skin.id)}
                      disabled={!unlocked}
                      className={`p-4 rounded-xl border-2 transition-all text-left ${
                        isEquipped
                          ? "border-purple-500 bg-purple-500/20"
                          : unlocked
                          ? `${getRarityBorder(skin.rarity)} hover:bg-white/5`
                          : "border-gray-700 opacity-60"
                      }`}
                    >
                      <div className="flex items-start gap-3">
                        <div 
                          className={`w-12 h-12 rounded-full flex items-center justify-center ${
                            skin.glow ? "shadow-lg" : ""
                          }`}
                          style={{ 
                            backgroundColor: skin.color === "rainbow" 
                              ? undefined 
                              : skin.color || "#6B7280",
                            background: skin.color === "rainbow" 
                              ? "linear-gradient(45deg, #ff0000, #ff7f00, #ffff00, #00ff00, #0000ff, #8b00ff)"
                              : undefined,
                            boxShadow: skin.glow 
                              ? `0 0 20px ${skin.glow === "rainbow" ? "#ff00ff" : skin.color}` 
                              : undefined
                          }}
                        >
                          <Sparkles className="w-6 h-6 text-white" />
                        </div>
                        <div className="flex-1">
                          <div className="flex items-center gap-2">
                            <span className="font-medium text-white">{skin.name}</span>
                            <span className={`text-xs uppercase ${getRarityColor(skin.rarity)}`}>
                              {skin.rarity}
                            </span>
                          </div>
                          <p className="text-sm text-gray-400">{skin.description}</p>
                          {!unlocked && unlockReq && (
                            <p className="text-xs text-yellow-500 mt-1">🔒 {unlockReq}</p>
                          )}
                          {isEquipped && (
                            <p className="text-xs text-purple-400 mt-1 flex items-center gap-1">
                              <Check className="w-3 h-3" /> Equipped
                            </p>
                          )}
                        </div>
                      </div>
                    </button>
                  );
                })}
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
