import { useZoogiGame, ZOOGI_ROSTER, Zoogi } from "@/lib/stores/useZoogiGame";
import { PREMIUM_CHARACTERS, PremiumZoogi } from "@/lib/premiumCharacters";
import { motion, AnimatePresence } from "framer-motion";
import { useState, useMemo } from "react";
import { ArrowLeft, Lock, Sparkles, Book, Star, Calendar } from "lucide-react";
import { getSpotlightZoogi, getTimeRemaining, ZOOGI_LORE as SPOTLIGHT_LORE, getRarityColor as getSpotlightRarityColor, getRarityGradient } from "./ZoogiSpotlight";

const PORTRAIT_IMAGES: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  pinpoint: "/portraits/pinpoint.png",
  bolt: "/portraits/bolt.png",
  wraps: "/portraits/wraps.png",
  lars: "/portraits/lars.png",
  nightshade: "/portraits/nightshade.png",
};

interface ZoogiLore {
  origin: string;
  habitat: string;
  funFact: string;
  rarity: "Common" | "Rare" | "Epic" | "Legendary";
}

const ZOOGI_LORE: Record<string, ZoogiLore> = {
  wolfgang: {
    origin: "Born from the howls of mountain winds, Wolfgang emerged during the Great Storm of the Northern Peaks. His pack was scattered, but he alone survived by learning to channel the wind itself.",
    habitat: "Prefers high altitude meadows and rocky outcrops where the wind is strongest. Often seen racing thunderclouds.",
    funFact: "Wolfgang's fur changes shade depending on the speed of nearby winds - the faster the gust, the darker he becomes.",
    rarity: "Common"
  },
  hotstreak: {
    origin: "Forged in the heart of Mount Caldera, Hotstreak was once a simple lava bubble until a lightning strike gave him consciousness. He rolled out of the volcano seeking adventure.",
    habitat: "Volcanic regions and hot springs. Absolutely cannot stand cold weather and will complain loudly about it.",
    funFact: "Hotstreak leaves tiny glass beads wherever he rolls - collectors prize these 'Streak Pearls' as good luck charms.",
    rarity: "Common"
  },
  lars: {
    origin: "Lars was the runt of his reef, constantly bumping into coral and getting laughed at. But each bump made him stronger until he became the greatest bouncer the ocean had ever seen.",
    habitat: "Coral reefs and shallow coastal waters, though he's been known to venture onto land when there's competition.",
    funFact: "Lars can hold his breath for exactly 47 minutes and 32 seconds. He times it perfectly every time.",
    rarity: "Common"
  },
  pinpoint: {
    origin: "Created in an ancient observatory by astronomers who needed to track stars with perfect precision. Pinpoint outlived his creators but never stopped calculating trajectories.",
    habitat: "Anywhere with a clear view of the stars. Often found on hilltops making complex calculations in the dirt.",
    funFact: "Pinpoint has calculated the exact trajectory needed to hit the moon. He's just waiting for a strong enough launch.",
    rarity: "Common"
  },
  bolt: {
    origin: "When lightning struck a thundercloud ten thousand times in the exact same spot, Bolt was born - a living spark of pure electricity with an insatiable need for speed.",
    habitat: "Storm clouds and power stations. Gets grumpy on sunny days.",
    funFact: "Bolt once powered an entire city for three seconds by accident. He still brags about it.",
    rarity: "Common"
  },
  blaze: {
    origin: "Hatched from a meteor that crashed into an active volcano, Blaze carries the fire of the cosmos within. Ancient texts speak of dragons like him protecting treasure hoards.",
    habitat: "Dragon roosts in volcanic mountain peaks. His nest is said to contain relics from fallen stars.",
    funFact: "Blaze's flames burn in seven different colors depending on his mood. Purple means he's hungry.",
    rarity: "Rare"
  },
  nightshade: {
    origin: "Summoned during an eclipse by accident when a wizard sneezed mid-incantation. Nightshade wasn't supposed to exist, but now refuses to un-exist out of spite.",
    habitat: "Shadows, dark alleys, and anywhere the light doesn't quite reach. Has a favorite spot under a specific bridge.",
    funFact: "Despite his fearsome appearance, Nightshade is terrified of butterflies. Nobody knows why.",
    rarity: "Rare"
  },
  phantom: {
    origin: "Once a treasure-hunting pirate who made a deal with sea spirits for immortality. The spirits had a sense of humor - he got immortality but lost his body.",
    habitat: "Haunted shipwrecks and foggy coastlines. Still looking for his original treasure map.",
    funFact: "Phantom can walk through walls but chooses to open doors normally because 'manners matter.'",
    rarity: "Epic"
  },
  inferno: {
    origin: "Inferno crawled up from the deepest volcanic vent ever discovered, where the pressure is so intense that diamonds form naturally. He brought several with him.",
    habitat: "Deep underground lava chambers. Surfaces only for worthy battles.",
    funFact: "Inferno's claws are harder than any known material. Scientists want to study them but he keeps scuttling away.",
    rarity: "Legendary"
  },
  wraps: {
    origin: "A pharaoh's most loyal guardian, Wraps volunteered to be mummified to protect the tomb forever. Five thousand years later, he decided the tomb was fine on its own.",
    habitat: "Desert ruins and ancient temples. Has strong opinions about proper pyramid maintenance.",
    funFact: "Wraps still receives birthday presents from grateful tomb visitors, which he carefully stores in a secret chamber.",
    rarity: "Rare"
  },
  bones: {
    origin: "Nobody remembers who Bones was when he was alive, including Bones himself. He doesn't let it bother him - he's having too much fun bouncing around.",
    habitat: "Anywhere spooky. Ironically loves birthday parties and shows up uninvited.",
    funFact: "Bones can rearrange his bones into different shapes but his favorite is 'skeleton riding tiny skeleton bicycle.'",
    rarity: "Rare"
  },
  thorn: {
    origin: "Thorn was a normal goblin until he fell into a magical bramble patch. When he emerged, the thorns had become part of him, and he was covered in sharp spines.",
    habitat: "Dense forests and overgrown gardens. His favorite snack is surprisingly roses.",
    funFact: "Despite being covered in spikes, Thorn gives the best hugs. Just very carefully.",
    rarity: "Rare"
  },
  bubbles: {
    origin: "Born in the deepest, coldest part of the Arctic Ocean, Bubbles spent years playing in underwater ice caves before discovering the surface world.",
    habitat: "Arctic waters and frozen lakes. Loves visiting ice cream shops for 'research.'",
    funFact: "Bubbles can blow bubbles that freeze instantly and shatter like glass. Uses them for decoration.",
    rarity: "Rare"
  },
  rotter: {
    origin: "Rotter was a gardener who loved his plants so much he became one with them after a magical fertilizer accident. He doesn't mind - the photosynthesis is nice.",
    habitat: "Swamps, bogs, and compost heaps. Surprisingly good at growing award-winning tomatoes.",
    funFact: "Rotter's toxic trail actually smells like fresh-cut grass. The poison is odorless.",
    rarity: "Epic"
  },
  frost: {
    origin: "When Wolfgang's pack was scattered in the Great Storm, one pup landed in the Arctic and was raised by ice spirits. Frost carries both wind and cold in her heart.",
    habitat: "Frozen tundras and glacial caves. Sometimes visits Wolfgang to reminisce about their puppyhood.",
    funFact: "Frost and Wolfgang have a rivalry going - they keep score of who has knocked off more opponents.",
    rarity: "Epic"
  },
  basher: {
    origin: "Basher was the champion of the Underground Orc Arena for fifty consecutive years. He retired undefeated, but got bored and is looking for new challenges.",
    habitat: "Mountain fortresses and arena battlegrounds. His trophy room takes up an entire castle.",
    funFact: "Despite his fearsome appearance, Basher is an accomplished chef. His specialty is surprisingly delicate soufflés.",
    rarity: "Legendary"
  }
};

function StatBar({ label, value, maxValue = 100 }: { label: string; value: number; maxValue?: number }) {
  const normalized = maxValue === 10 ? value * 10 : value;
  return (
    <div className="flex items-center gap-2">
      <span className="text-xs text-white/70 w-14">{label}</span>
      <div className="flex-1 h-2 bg-white/20 rounded-full overflow-hidden">
        <motion.div
          className="h-full bg-gradient-to-r from-cyan-400 to-blue-500"
          initial={{ width: 0 }}
          animate={{ width: `${normalized}%` }}
          transition={{ duration: 0.5, delay: 0.2 }}
        />
      </div>
      <span className="text-xs text-white/70 w-6">{value}</span>
    </div>
  );
}

function getRarityColor(rarity: string): string {
  switch (rarity) {
    case "Common": return "text-gray-400";
    case "Rare": return "text-blue-400";
    case "Epic": return "text-purple-400";
    case "Legendary": return "text-yellow-400";
    default: return "text-gray-400";
  }
}

function getRarityBgColor(rarity: string): string {
  switch (rarity) {
    case "Common": return "bg-gray-500/20";
    case "Rare": return "bg-blue-500/20";
    case "Epic": return "bg-purple-500/20";
    case "Legendary": return "bg-yellow-500/20";
    default: return "bg-gray-500/20";
  }
}

interface ZoogiEntry {
  id: string;
  name: string;
  type: string;
  color: string;
  secondaryColor?: string;
  accentColor?: string;
  ability: string;
  abilityDescription: string;
  stats: { speed: number; power: number; defense: number; control: number };
  isPremium: boolean;
  description?: string;
}

function convertToEntry(zoogi: Zoogi): ZoogiEntry {
  return {
    ...zoogi,
    isPremium: false,
    secondaryColor: zoogi.secondaryColor
  };
}

function convertPremiumToEntry(zoogi: PremiumZoogi): ZoogiEntry {
  return {
    id: zoogi.id,
    name: zoogi.name,
    type: zoogi.description.split(" ")[1] || "Monster",
    color: zoogi.color,
    accentColor: zoogi.accentColor,
    ability: zoogi.ability,
    abilityDescription: zoogi.abilityDescription,
    stats: zoogi.stats,
    isPremium: true,
    description: zoogi.description
  };
}

export function Zoogipedia() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const [selectedZoogi, setSelectedZoogi] = useState<ZoogiEntry | null>(null);
  const [filter, setFilter] = useState<"all" | "free" | "premium" | "featured">("all");
  
  const spotlightData = useMemo(() => getSpotlightZoogi('weekly'), []);
  const timeRemaining = useMemo(() => getTimeRemaining('weekly'), []);
  const spotlightLore = ZOOGI_LORE[spotlightData.zoogi.id];
  
  const allZoogis: ZoogiEntry[] = [
    ...ZOOGI_ROSTER.map(convertToEntry),
    ...PREMIUM_CHARACTERS.map(convertPremiumToEntry)
  ];
  
  const filteredZoogis = allZoogis.filter(z => {
    if (filter === "free") return !z.isPremium;
    if (filter === "premium") return z.isPremium;
    if (filter === "featured") return z.id === spotlightData.zoogi.id;
    return true;
  });

  return (
    <div className="fixed inset-0 flex flex-col bg-gradient-to-b from-amber-900 via-orange-900 to-black overflow-hidden">
      <div className="absolute inset-0 overflow-hidden pointer-events-none">
        {Array.from({ length: 15 }).map((_, i) => (
          <motion.div
            key={i}
            className="absolute text-amber-500/20"
            style={{
              left: `${(i * 17) % 100}%`,
              top: `${(i * 23) % 100}%`,
              fontSize: `${20 + (i % 3) * 10}px`,
            }}
            animate={{
              y: [0, -10, 0],
              rotate: [0, 5, -5, 0],
            }}
            transition={{
              duration: 4 + i % 2,
              repeat: Infinity,
              delay: i * 0.2,
            }}
          >
            <Book />
          </motion.div>
        ))}
      </div>

      <div className="relative z-10 p-4 flex items-center gap-4">
        <motion.button
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
          onClick={() => setPhase("menu")}
          className="p-2 bg-white/10 rounded-full hover:bg-white/20"
        >
          <ArrowLeft size={24} className="text-white" />
        </motion.button>
        
        <div className="flex items-center gap-2">
          <Book className="text-amber-400" size={28} />
          <h1 className="text-2xl md:text-3xl font-bold text-transparent bg-clip-text bg-gradient-to-r from-amber-400 to-orange-500">
            Zoogipedia
          </h1>
        </div>
      </div>

      <div className="relative z-10 px-4 mb-4">
        <div className="flex gap-2 flex-wrap">
          {(["all", "free", "premium", "featured"] as const).map((f) => (
            <button
              key={f}
              onClick={() => setFilter(f)}
              className={`px-4 py-2 rounded-full text-sm font-medium transition-all flex items-center gap-1 ${
                filter === f
                  ? f === "featured" ? "bg-gradient-to-r from-yellow-500 to-orange-500 text-black" : "bg-amber-500 text-black"
                  : "bg-white/10 text-white/70 hover:bg-white/20"
              }`}
            >
              {f === "featured" && <Star size={14} />}
              {f === "all" ? "All" : f === "free" ? "Free" : f === "premium" ? "Premium" : "Featured"}
            </button>
          ))}
        </div>
        
        {filter === "featured" && (
          <motion.div
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
            className={`mt-3 p-4 rounded-xl bg-gradient-to-r ${getRarityGradient(spotlightLore?.rarity || 'Common')} border border-white/20`}
          >
            <div className="flex items-center gap-2 mb-2">
              <Calendar size={16} className="text-white/70" />
              <span className="text-white/70 text-sm">Weekly Spotlight</span>
              <span className="text-white/50 text-xs ml-auto">{timeRemaining} remaining</span>
            </div>
            <div className="flex items-center gap-3">
              <div 
                className="w-12 h-12 rounded-full flex-shrink-0"
                style={{ backgroundColor: spotlightData.zoogi.color }}
              />
              <div>
                <h3 className="text-white font-bold">{spotlightData.zoogi.name}</h3>
                <p className="text-white/60 text-sm">{spotlightLore?.rarity || 'Common'} - Tap to learn more!</p>
              </div>
            </div>
          </motion.div>
        )}
      </div>

      <div className="flex-1 flex flex-col md:flex-row gap-4 p-4 overflow-hidden">
        <div className="md:w-1/2 overflow-y-auto pr-2 space-y-2">
          {filteredZoogis.map((zoogi, index) => {
            const lore = ZOOGI_LORE[zoogi.id];
            return (
              <motion.button
                key={zoogi.id}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.05 }}
                onClick={() => setSelectedZoogi(zoogi)}
                className={`w-full flex items-center gap-3 p-3 rounded-xl transition-all text-left ${
                  selectedZoogi?.id === zoogi.id
                    ? "bg-white/20 ring-2 ring-amber-400"
                    : "bg-black/30 hover:bg-black/40"
                }`}
              >
                {PORTRAIT_IMAGES[zoogi.id] ? (
                  <div className="w-12 h-12 rounded-full shadow-lg flex-shrink-0 overflow-hidden border-2 border-white/30">
                    <img 
                      src={PORTRAIT_IMAGES[zoogi.id]} 
                      alt={zoogi.name}
                      className="w-full h-full object-cover"
                    />
                  </div>
                ) : (
                  <div
                    className="w-12 h-12 rounded-full shadow-lg flex-shrink-0"
                    style={{
                      background: `radial-gradient(circle at 30% 30%, ${zoogi.secondaryColor || zoogi.accentColor || "#fff"}, ${zoogi.color})`,
                    }}
                  />
                )}
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2">
                    <span className="font-bold text-white">{zoogi.name}</span>
                    {zoogi.isPremium && (
                      <Sparkles size={14} className="text-amber-400" />
                    )}
                    {lore && (
                      <span className={`text-xs px-2 py-0.5 rounded-full ${getRarityBgColor(lore.rarity)} ${getRarityColor(lore.rarity)}`}>
                        {lore.rarity}
                      </span>
                    )}
                  </div>
                  <p className="text-white/60 text-sm truncate">{zoogi.type}</p>
                </div>
              </motion.button>
            );
          })}
        </div>

        <AnimatePresence mode="wait">
          {selectedZoogi ? (
            <motion.div
              key={selectedZoogi.id}
              initial={{ opacity: 0, x: 20 }}
              animate={{ opacity: 1, x: 0 }}
              exit={{ opacity: 0, x: -20 }}
              className="md:w-1/2 bg-black/40 rounded-2xl p-4 md:p-6 backdrop-blur-sm overflow-y-auto"
            >
              <div className="flex items-start gap-4 mb-4">
                {PORTRAIT_IMAGES[selectedZoogi.id] ? (
                  <div className="w-20 h-20 md:w-24 md:h-24 rounded-full shadow-lg flex-shrink-0 overflow-hidden border-2 border-white/30">
                    <img 
                      src={PORTRAIT_IMAGES[selectedZoogi.id]} 
                      alt={selectedZoogi.name}
                      className="w-full h-full object-cover"
                    />
                  </div>
                ) : (
                  <div
                    className="w-20 h-20 md:w-24 md:h-24 rounded-full shadow-lg flex-shrink-0"
                    style={{
                      background: `radial-gradient(circle at 30% 30%, ${selectedZoogi.secondaryColor || selectedZoogi.accentColor || "#fff"}, ${selectedZoogi.color})`,
                    }}
                  />
                )}
                <div>
                  <div className="flex items-center gap-2 mb-1">
                    <h2 className="text-2xl md:text-3xl font-bold text-white">{selectedZoogi.name}</h2>
                    {selectedZoogi.isPremium && (
                      <Sparkles size={20} className="text-amber-400" />
                    )}
                  </div>
                  <p className="text-white/60">{selectedZoogi.type} Type</p>
                  {ZOOGI_LORE[selectedZoogi.id] && (
                    <span className={`inline-block mt-1 text-sm px-3 py-1 rounded-full ${getRarityBgColor(ZOOGI_LORE[selectedZoogi.id].rarity)} ${getRarityColor(ZOOGI_LORE[selectedZoogi.id].rarity)}`}>
                      {ZOOGI_LORE[selectedZoogi.id].rarity}
                    </span>
                  )}
                </div>
              </div>

              <div className="mb-4 p-3 bg-amber-500/20 rounded-lg">
                <h3 className="text-amber-400 font-semibold text-sm mb-1 flex items-center gap-2">
                  <Sparkles size={14} />
                  {selectedZoogi.ability}
                </h3>
                <p className="text-white/80 text-sm">
                  {selectedZoogi.abilityDescription}
                </p>
              </div>

              <div className="mb-4">
                <h3 className="text-white/70 font-semibold text-sm mb-2">Stats</h3>
                <div className="space-y-2">
                  <StatBar label="Speed" value={selectedZoogi.stats.speed} maxValue={selectedZoogi.isPremium ? 10 : 100} />
                  <StatBar label="Power" value={selectedZoogi.stats.power} maxValue={selectedZoogi.isPremium ? 10 : 100} />
                  <StatBar label="Defense" value={selectedZoogi.stats.defense} maxValue={selectedZoogi.isPremium ? 10 : 100} />
                  <StatBar label="Control" value={selectedZoogi.stats.control} maxValue={selectedZoogi.isPremium ? 10 : 100} />
                </div>
              </div>

              {ZOOGI_LORE[selectedZoogi.id] && (
                <div className="space-y-4">
                  <div>
                    <h3 className="text-amber-400 font-semibold text-sm mb-2 flex items-center gap-2">
                      <Book size={14} />
                      Origin Story
                    </h3>
                    <p className="text-white/80 text-sm leading-relaxed">
                      {ZOOGI_LORE[selectedZoogi.id].origin}
                    </p>
                  </div>
                  
                  <div>
                    <h3 className="text-green-400 font-semibold text-sm mb-2">Habitat</h3>
                    <p className="text-white/80 text-sm leading-relaxed">
                      {ZOOGI_LORE[selectedZoogi.id].habitat}
                    </p>
                  </div>
                  
                  <div className="p-3 bg-purple-500/20 rounded-lg">
                    <h3 className="text-purple-400 font-semibold text-sm mb-1">Fun Fact</h3>
                    <p className="text-white/80 text-sm italic">
                      "{ZOOGI_LORE[selectedZoogi.id].funFact}"
                    </p>
                  </div>
                </div>
              )}

              {selectedZoogi.isPremium && (
                <motion.div 
                  initial={{ opacity: 0 }}
                  animate={{ opacity: 1 }}
                  className="mt-4 p-3 bg-gradient-to-r from-purple-500/30 to-pink-500/30 rounded-lg flex items-center gap-3"
                >
                  <Lock size={20} className="text-purple-400" />
                  <div>
                    <p className="text-white font-medium text-sm">Premium Character</p>
                    <p className="text-white/60 text-xs">Available in the Shop</p>
                  </div>
                </motion.div>
              )}
            </motion.div>
          ) : (
            <motion.div
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              className="md:w-1/2 bg-black/40 rounded-2xl p-6 backdrop-blur-sm flex flex-col items-center justify-center text-center"
            >
              <Book size={48} className="text-amber-400/50 mb-4" />
              <h3 className="text-white/70 text-lg font-medium mb-2">Select a Zoogi</h3>
              <p className="text-white/50 text-sm">
                Tap on any creature to learn about their origin story, habitat, and fun facts!
              </p>
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      <div className="relative z-10 p-4 text-center">
        <p className="text-white/50 text-sm">
          {allZoogis.length} Zoogis documented • {PREMIUM_CHARACTERS.length} Premium creatures
        </p>
      </div>
    </div>
  );
}
