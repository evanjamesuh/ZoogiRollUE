import { motion, AnimatePresence } from "framer-motion";
import { useState, useMemo, useEffect, useRef } from "react";
import { Sparkles, Star, Clock, Calendar, ChevronRight, X } from "lucide-react";
import { ZOOGI_ROSTER, Zoogi } from "@/lib/stores/useZoogiGame";
import { PREMIUM_CHARACTERS, PremiumZoogi } from "@/lib/premiumCharacters";

interface ZoogiLore {
  origin: string;
  habitat: string;
  funFact: string;
  rarity: "Common" | "Rare" | "Epic" | "Legendary";
  quote?: string;
}

const ZOOGI_LORE: Record<string, ZoogiLore> = {
  wolfgang: {
    origin: "Born from the howls of mountain winds, Wolfgang emerged during the Great Storm of the Northern Peaks. His pack was scattered, but he alone survived by learning to channel the wind itself.",
    habitat: "Prefers high altitude meadows and rocky outcrops where the wind is strongest. Often seen racing thunderclouds.",
    funFact: "Wolfgang's fur changes shade depending on the speed of nearby winds - the faster the gust, the darker he becomes.",
    rarity: "Common",
    quote: "The wind carries more than whispers - it carries victory."
  },
  hotstreak: {
    origin: "Forged in the heart of Mount Caldera, Hotstreak was once a simple lava bubble until a lightning strike gave him consciousness. He rolled out of the volcano seeking adventure.",
    habitat: "Volcanic regions and hot springs. Absolutely cannot stand cold weather and will complain loudly about it.",
    funFact: "Hotstreak leaves tiny glass beads wherever he rolls - collectors prize these 'Streak Pearls' as good luck charms.",
    rarity: "Common",
    quote: "Too hot to handle, too fast to catch!"
  },
  lars: {
    origin: "Lars was the runt of his reef, constantly bumping into coral and getting laughed at. But each bump made him stronger until he became the greatest bouncer the ocean had ever seen.",
    habitat: "Coral reefs and shallow coastal waters, though he's been known to venture onto land when there's competition.",
    funFact: "Lars can hold his breath for exactly 47 minutes and 32 seconds. He times it perfectly every time.",
    rarity: "Common",
    quote: "Every bounce makes me stronger. Try me."
  },
  pinpoint: {
    origin: "Created in an ancient observatory by astronomers who needed to track stars with perfect precision. Pinpoint outlived his creators but never stopped calculating trajectories.",
    habitat: "Anywhere with a clear view of the stars. Often found on hilltops making complex calculations in the dirt.",
    funFact: "Pinpoint has calculated the exact trajectory needed to hit the moon. He's just waiting for a strong enough launch.",
    rarity: "Common",
    quote: "I never miss. It's mathematically impossible."
  },
  bolt: {
    origin: "When lightning struck a thundercloud ten thousand times in the exact same spot, Bolt was born - a living spark of pure electricity with an insatiable need for speed.",
    habitat: "Storm clouds and power stations. Gets grumpy on sunny days.",
    funFact: "Bolt once powered an entire city for three seconds by accident. He still brags about it.",
    rarity: "Common",
    quote: "Blink and you'll miss me. Actually, don't blink - you'll still miss me."
  },
  blaze: {
    origin: "Hatched from a meteor that crashed into an active volcano, Blaze carries the fire of the cosmos within. Ancient texts speak of dragons like him protecting treasure hoards.",
    habitat: "Dragon roosts in volcanic mountain peaks. His nest is said to contain relics from fallen stars.",
    funFact: "Blaze's flames burn in seven different colors depending on his mood. Purple means he's hungry.",
    rarity: "Rare",
    quote: "From the stars I came, and stars I shall make of my enemies."
  },
  nightshade: {
    origin: "Summoned during an eclipse by accident when a wizard sneezed mid-incantation. Nightshade wasn't supposed to exist, but now refuses to un-exist out of spite.",
    habitat: "Shadows, dark alleys, and anywhere the light doesn't quite reach. Has a favorite spot under a specific bridge.",
    funFact: "Despite his fearsome appearance, Nightshade is terrified of butterflies. Nobody knows why.",
    rarity: "Rare",
    quote: "I shouldn't exist. Neither should your lead in this match."
  },
  phantom: {
    origin: "Once a treasure-hunting pirate who made a deal with sea spirits for immortality. The spirits had a sense of humor - he got immortality but lost his body.",
    habitat: "Haunted shipwrecks and foggy coastlines. Still looking for his original treasure map.",
    funFact: "Phantom can walk through walls but chooses to open doors normally because 'manners matter.'",
    rarity: "Epic",
    quote: "You can't hit what you can't touch."
  },
  inferno: {
    origin: "Inferno crawled up from the deepest volcanic vent ever discovered, where the pressure is so intense that diamonds form naturally. He brought several with him.",
    habitat: "Deep underground lava chambers. Surfaces only for worthy battles.",
    funFact: "Inferno's claws are harder than any known material. Scientists want to study them but he keeps scuttling away.",
    rarity: "Legendary",
    quote: "The deeper the fire, the hotter it burns."
  },
  wraps: {
    origin: "A pharaoh's most loyal guardian, Wraps volunteered to be mummified to protect the tomb forever. Five thousand years later, he decided the tomb was fine on its own.",
    habitat: "Desert ruins and ancient temples. Has strong opinions about proper pyramid maintenance.",
    funFact: "Wraps still receives birthday presents from grateful tomb visitors, which he carefully stores in a secret chamber.",
    rarity: "Rare",
    quote: "Time is on my side. Literally - I've got five thousand years of experience."
  },
  bones: {
    origin: "Nobody remembers who Bones was when he was alive, including Bones himself. He doesn't let it bother him - he's having too much fun bouncing around.",
    habitat: "Anywhere spooky. Ironically loves birthday parties and shows up uninvited.",
    funFact: "Bones can rearrange his bones into different shapes but his favorite is 'skeleton riding tiny skeleton bicycle.'",
    rarity: "Rare",
    quote: "No brain, no pain, all gain!"
  },
  thorn: {
    origin: "Thorn was a normal goblin until he fell into a magical bramble patch. When he emerged, the thorns had become part of him, and he was covered in sharp spines.",
    habitat: "Dense forests and overgrown gardens. His favorite snack is surprisingly roses.",
    funFact: "Despite being covered in spikes, Thorn gives the best hugs. Just very carefully.",
    rarity: "Rare",
    quote: "Go ahead, get close. I dare you."
  },
  bubbles: {
    origin: "Born in the deepest, coldest part of the Arctic Ocean, Bubbles spent years playing in underwater ice caves before discovering the surface world.",
    habitat: "Arctic waters and frozen lakes. Loves visiting ice cream shops for 'research.'",
    funFact: "Bubbles can blow bubbles that freeze instantly and shatter like glass. Uses them for decoration.",
    rarity: "Rare",
    quote: "Cold never bothered me anyway. Neither will you."
  },
  rotter: {
    origin: "Rotter was a gardener who loved his plants so much he became one with them after a magical fertilizer accident. He doesn't mind - the photosynthesis is nice.",
    habitat: "Swamps, bogs, and compost heaps. Surprisingly good at growing award-winning tomatoes.",
    funFact: "Rotter's toxic trail actually smells like fresh-cut grass. The poison is odorless.",
    rarity: "Epic",
    quote: "Growth comes from decay. You'll learn that soon."
  },
  frost: {
    origin: "When Wolfgang's pack was scattered in the Great Storm, one pup landed in the Arctic and was raised by ice spirits. Frost carries both wind and cold in her heart.",
    habitat: "Frozen tundras and glacial caves. Sometimes visits Wolfgang to reminisce about their puppyhood.",
    funFact: "Frost and Wolfgang have a rivalry going - they keep score of who has knocked off more opponents.",
    rarity: "Epic",
    quote: "Ice runs through my veins. What do you have?"
  },
  basher: {
    origin: "Basher was the champion of the Underground Orc Arena for fifty consecutive years. He retired undefeated, but got bored and is looking for new challenges.",
    habitat: "Mountain fortresses and arena battlegrounds. His trophy room takes up an entire castle.",
    funFact: "Despite his fearsome appearance, Basher is an accomplished chef. His specialty is surprisingly delicate soufflés.",
    rarity: "Legendary",
    quote: "Fifty years undefeated. You won't be the one to change that."
  }
};

function getRarityColor(rarity: string): string {
  switch (rarity) {
    case "Common": return "text-gray-300";
    case "Rare": return "text-blue-400";
    case "Epic": return "text-purple-400";
    case "Legendary": return "text-yellow-400";
    default: return "text-gray-300";
  }
}

function getRarityGradient(rarity: string): string {
  switch (rarity) {
    case "Common": return "from-gray-600 to-gray-800";
    case "Rare": return "from-blue-600 to-blue-900";
    case "Epic": return "from-purple-600 to-purple-900";
    case "Legendary": return "from-yellow-600 via-orange-600 to-red-700";
    default: return "from-gray-600 to-gray-800";
  }
}

function getRarityGlow(rarity: string): string {
  switch (rarity) {
    case "Common": return "shadow-gray-500/30";
    case "Rare": return "shadow-blue-500/50";
    case "Epic": return "shadow-purple-500/50";
    case "Legendary": return "shadow-yellow-500/50";
    default: return "shadow-gray-500/30";
  }
}

function getSpotlightZoogi(type: 'daily' | 'weekly'): { zoogi: Zoogi | PremiumZoogi; isPremium: boolean } {
  const allZoogis = [
    ...ZOOGI_ROSTER.map(z => ({ zoogi: z, isPremium: false })),
    ...PREMIUM_CHARACTERS.map(z => ({ zoogi: z as any, isPremium: true }))
  ];
  
  const now = new Date();
  const dayOfYear = Math.floor((now.getTime() - new Date(now.getFullYear(), 0, 0).getTime()) / (1000 * 60 * 60 * 24));
  const weekOfYear = Math.floor(dayOfYear / 7);
  
  const seed = type === 'daily' ? dayOfYear : weekOfYear;
  const index = seed % allZoogis.length;
  
  return allZoogis[index];
}

function getTimeRemaining(type: 'daily' | 'weekly'): string {
  const now = new Date();
  
  if (type === 'daily') {
    const tomorrow = new Date(now);
    tomorrow.setDate(tomorrow.getDate() + 1);
    tomorrow.setHours(0, 0, 0, 0);
    const diff = tomorrow.getTime() - now.getTime();
    const hours = Math.floor(diff / (1000 * 60 * 60));
    const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
    return `${hours}h ${minutes}m`;
  } else {
    const dayOfWeek = now.getDay();
    const daysUntilSunday = 7 - dayOfWeek;
    const nextSunday = new Date(now);
    nextSunday.setDate(nextSunday.getDate() + daysUntilSunday);
    nextSunday.setHours(0, 0, 0, 0);
    const diff = nextSunday.getTime() - now.getTime();
    const days = Math.floor(diff / (1000 * 60 * 60 * 24));
    const hours = Math.floor((diff % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
    return `${days}d ${hours}h`;
  }
}

interface SpotlightCardProps {
  type: 'daily' | 'weekly';
  onClick: () => void;
}

function SpotlightCard({ type, onClick }: SpotlightCardProps) {
  const [timeRemaining, setTimeRemaining] = useState(() => getTimeRemaining(type));
  const [dateSeed, setDateSeed] = useState(() => {
    const now = new Date();
    const dayOfYear = Math.floor((now.getTime() - new Date(now.getFullYear(), 0, 0).getTime()) / (1000 * 60 * 60 * 24));
    return type === 'daily' ? dayOfYear : Math.floor(dayOfYear / 7);
  });
  
  useEffect(() => {
    const interval = setInterval(() => {
      setTimeRemaining(getTimeRemaining(type));
      const now = new Date();
      const dayOfYear = Math.floor((now.getTime() - new Date(now.getFullYear(), 0, 0).getTime()) / (1000 * 60 * 60 * 24));
      const newSeed = type === 'daily' ? dayOfYear : Math.floor(dayOfYear / 7);
      if (newSeed !== dateSeed) {
        setDateSeed(newSeed);
      }
    }, 60000);
    return () => clearInterval(interval);
  }, [type, dateSeed]);
  
  const { zoogi, isPremium } = useMemo(() => getSpotlightZoogi(type), [type, dateSeed]);
  const lore = ZOOGI_LORE[zoogi.id];
  const rarity = lore?.rarity || "Common";
  
  return (
    <motion.button
      onClick={onClick}
      className={`relative flex items-center gap-3 p-3 rounded-xl bg-gradient-to-r ${getRarityGradient(rarity)} shadow-lg ${getRarityGlow(rarity)} overflow-hidden w-full text-left`}
      whileHover={{ scale: 1.02 }}
      whileTap={{ scale: 0.98 }}
    >
      <div className="absolute inset-0 bg-[url('data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI1MCIgaGVpZ2h0PSI1MCI+PGNpcmNsZSBjeD0iMjUiIGN5PSIyNSIgcj0iMSIgZmlsbD0icmdiYSgyNTUsMjU1LDI1NSwwLjEpIi8+PC9zdmc+')] opacity-30" />
      
      <div 
        className="relative w-12 h-12 rounded-full flex items-center justify-center shadow-inner"
        style={{ backgroundColor: zoogi.color }}
      >
        <div 
          className="absolute inset-1 rounded-full opacity-60"
          style={{ 
            background: `radial-gradient(circle at 30% 30%, ${(zoogi as any).secondaryColor || (zoogi as any).accentColor || zoogi.color}, transparent)` 
          }}
        />
        {isPremium && (
          <Sparkles className="absolute -top-1 -right-1 w-4 h-4 text-yellow-400" />
        )}
      </div>
      
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2 mb-0.5">
          {type === 'daily' ? (
            <Clock className="w-3 h-3 text-white/60" />
          ) : (
            <Calendar className="w-3 h-3 text-white/60" />
          )}
          <span className="text-white/60 text-xs uppercase tracking-wider">
            {type === 'daily' ? 'Daily' : 'Weekly'} Spotlight
          </span>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-white font-bold truncate">{zoogi.name}</span>
          <span className={`text-xs ${getRarityColor(rarity)}`}>{rarity}</span>
        </div>
        <p className="text-white/50 text-xs truncate">
          {timeRemaining} remaining
        </p>
      </div>
      
      <ChevronRight className="w-5 h-5 text-white/40" />
    </motion.button>
  );
}

interface SpotlightModalProps {
  type: 'daily' | 'weekly';
  onClose: () => void;
}

function SpotlightModal({ type, onClose }: SpotlightModalProps) {
  const { zoogi, isPremium } = useMemo(() => getSpotlightZoogi(type), [type]);
  const lore = ZOOGI_LORE[zoogi.id];
  const rarity = lore?.rarity || "Common";
  const scrollContainerRef = useRef<HTMLDivElement>(null);
  
  useEffect(() => {
    if (scrollContainerRef.current) {
      scrollContainerRef.current.scrollTop = 0;
    }
  }, []);
  
  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      className="fixed inset-0 flex items-center justify-center bg-black/80 z-50 p-4"
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <motion.div
        initial={{ scale: 0.9, y: 20 }}
        animate={{ scale: 1, y: 0 }}
        exit={{ scale: 0.9, y: 20 }}
        className={`relative bg-gradient-to-b ${getRarityGradient(rarity)} rounded-2xl max-w-md w-full max-h-[40vh] shadow-2xl ${getRarityGlow(rarity)} flex flex-col mt-[12rem] mb-[12rem]`}
      >
        <div className="absolute inset-0 bg-black/20 rounded-2xl pointer-events-none" />
        
        <div className="sticky top-0 z-10 flex items-center justify-between p-4 bg-gradient-to-b from-black/40 to-transparent rounded-t-2xl">
          <div className="flex items-center gap-2">
            {type === 'daily' ? (
              <Clock className="w-5 h-5 text-white/80" />
            ) : (
              <Calendar className="w-5 h-5 text-white/80" />
            )}
            <span className="text-white/80 text-sm uppercase tracking-wider font-semibold">
              {type === 'daily' ? 'Daily' : 'Weekly'} Spotlight
            </span>
          </div>
          <button
            onClick={onClose}
            className="w-8 h-8 flex items-center justify-center rounded-full bg-black/40 text-white/80 hover:text-white hover:bg-black/60 transition-colors"
          >
            <X size={18} />
          </button>
        </div>
        
        <div 
          ref={scrollContainerRef}
          className="flex-1 overflow-y-auto overscroll-contain px-6 pb-6"
          style={{ WebkitOverflowScrolling: 'touch' }}
        >
          <div className="text-center text-white/50 text-xs mb-4">
            {getTimeRemaining(type)} remaining
          </div>
          
          <div className="flex flex-col items-center mb-6">
            <motion.div
              className="relative w-28 h-28 rounded-full flex items-center justify-center shadow-2xl mb-4"
              style={{ backgroundColor: zoogi.color }}
              animate={{ 
                boxShadow: [
                  `0 0 20px ${zoogi.color}80`,
                  `0 0 40px ${zoogi.color}60`,
                  `0 0 20px ${zoogi.color}80`
                ]
              }}
              transition={{ duration: 2, repeat: Infinity }}
            >
              <div 
                className="absolute inset-2 rounded-full opacity-60"
                style={{ 
                  background: `radial-gradient(circle at 30% 30%, ${(zoogi as any).secondaryColor || (zoogi as any).accentColor || zoogi.color}, transparent)` 
                }}
              />
              {isPremium && (
                <motion.div
                  className="absolute -top-2 -right-2"
                  animate={{ rotate: [0, 10, -10, 0] }}
                  transition={{ duration: 2, repeat: Infinity }}
                >
                  <Sparkles className="w-8 h-8 text-yellow-400 drop-shadow-lg" />
                </motion.div>
              )}
            </motion.div>
            
            <h2 className="text-3xl font-bold text-white mb-1">{zoogi.name}</h2>
            <div className="flex items-center gap-2 mb-2">
              <span className={`px-3 py-1 rounded-full text-sm font-medium ${getRarityColor(rarity)} bg-black/30`}>
                {rarity}
              </span>
              {'type' in zoogi && (
                <span className="text-white/60 text-sm">{zoogi.type}</span>
              )}
            </div>
            
            {lore?.quote && (
              <p className="text-white/70 italic text-center text-sm">"{lore.quote}"</p>
            )}
          </div>
          
          <div className="space-y-4">
            <div className="bg-black/30 rounded-xl p-4">
              <div className="flex items-center gap-2 mb-2">
                <Star className="w-4 h-4 text-yellow-400" />
                <h3 className="text-white font-semibold">Ability: {zoogi.ability}</h3>
              </div>
              <p className="text-white/70 text-sm">{zoogi.abilityDescription}</p>
            </div>
            
            {lore && (
              <>
                <div className="bg-black/30 rounded-xl p-4">
                  <h3 className="text-white font-semibold mb-2">Origin Story</h3>
                  <p className="text-white/70 text-sm leading-relaxed">{lore.origin}</p>
                </div>
                
                <div className="bg-black/30 rounded-xl p-4">
                  <h3 className="text-white font-semibold mb-2">Habitat</h3>
                  <p className="text-white/70 text-sm leading-relaxed">{lore.habitat}</p>
                </div>
                
                <div className="bg-black/30 rounded-xl p-4">
                  <h3 className="text-white font-semibold mb-2">Fun Fact</h3>
                  <p className="text-white/70 text-sm leading-relaxed">{lore.funFact}</p>
                </div>
              </>
            )}
            
            <div className="bg-black/30 rounded-xl p-4">
              <h3 className="text-white font-semibold mb-3">Stats</h3>
              <div className="space-y-2">
                {Object.entries(zoogi.stats).map(([key, value]) => (
                  <div key={key} className="flex items-center gap-2">
                    <span className="text-white/60 text-xs w-16 capitalize">{key}</span>
                    <div className="flex-1 h-2 bg-black/40 rounded-full overflow-hidden">
                      <motion.div
                        className="h-full bg-gradient-to-r from-cyan-400 to-blue-500"
                        initial={{ width: 0 }}
                        animate={{ width: `${value > 10 ? value : value * 10}%` }}
                        transition={{ duration: 0.5, delay: 0.2 }}
                      />
                    </div>
                    <span className="text-white/60 text-xs w-6">{value}</span>
                  </div>
                ))}
              </div>
            </div>
            
            <motion.button
              onClick={onClose}
              whileTap={{ scale: 0.95 }}
              className="w-full py-3 mt-2 bg-white/20 hover:bg-white/30 text-white font-semibold rounded-xl transition-colors"
            >
              Close
            </motion.button>
          </div>
        </div>
      </motion.div>
    </motion.div>
  );
}

export { getSpotlightZoogi, getTimeRemaining, SpotlightCard, SpotlightModal, ZOOGI_LORE, getRarityColor, getRarityGradient };

export function ZoogiSpotlight() {
  const [selectedSpotlight, setSelectedSpotlight] = useState<'weekly' | null>(null);
  
  return (
    <>
      <div className="w-full">
        <SpotlightCard type="weekly" onClick={() => setSelectedSpotlight('weekly')} />
      </div>
      
      <AnimatePresence>
        {selectedSpotlight && (
          <SpotlightModal 
            type={selectedSpotlight} 
            onClose={() => setSelectedSpotlight(null)} 
          />
        )}
      </AnimatePresence>
    </>
  );
}
