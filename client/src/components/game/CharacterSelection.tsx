import { useZoogiGame, ZOOGI_ROSTER, Zoogi, type CustomZoogiData } from "@/lib/stores/useZoogiGame";
import { motion, AnimatePresence } from "framer-motion";
import { useState, useEffect, useMemo, useCallback } from "react";
import { Wand2, ChevronLeft, ChevronRight, LogIn, Minus, Plus, Users } from "lucide-react";
import { getDeviceId } from "@/lib/deviceId";
import useEmblaCarousel from "embla-carousel-react";
import { AuthModal } from "@/components/ui/AuthModal";

const PORTRAIT_IMAGES: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  pinpoint: "/portraits/pinpoint.png",
  bolt: "/portraits/bolt.png",
  wraps: "/portraits/wraps.png",
  lars: "/portraits/lars.png",
};

function StatBar({ label, value, color }: { label: string; value: number; color: string }) {
  return (
    <div className="flex items-center gap-2">
      <span className="text-xs text-white/70 w-14">{label}</span>
      <div className="flex-1 h-2 bg-white/20 rounded-full overflow-hidden">
        <motion.div
          className="h-full rounded-full"
          style={{ backgroundColor: color }}
          initial={{ width: 0 }}
          animate={{ width: `${value}%` }}
          transition={{ duration: 0.4 }}
        />
      </div>
      <span className="text-xs text-white font-semibold w-6">{value}</span>
    </div>
  );
}

function ZoogiSlide({ zoogi, isSelected, onClick }: { zoogi: Zoogi; isSelected: boolean; onClick: () => void }) {
  const hasPortrait = PORTRAIT_IMAGES[zoogi.id];
  
  return (
    <motion.button
      whileTap={{ scale: 0.95 }}
      onClick={onClick}
      className={`relative flex flex-col items-center p-3 rounded-2xl transition-all min-w-[100px] ${
        isSelected 
          ? "bg-gradient-to-b from-yellow-500/30 to-orange-500/20 ring-3 ring-yellow-400" 
          : "bg-white/10 hover:bg-white/20"
      }`}
    >
      {hasPortrait ? (
        <div className="w-16 h-16 rounded-full mx-auto mb-1 shadow-lg overflow-hidden border-2 border-white/30">
          <img 
            src={PORTRAIT_IMAGES[zoogi.id]} 
            alt={zoogi.name}
            className="w-full h-full object-cover"
            style={zoogi.id === "wraps" ? { transform: "scale(1.4)", transformOrigin: "center 45%" } : undefined}
          />
        </div>
      ) : (
        <div
          className="w-16 h-16 rounded-full mx-auto mb-1 shadow-lg"
          style={{
            background: `radial-gradient(circle at 30% 30%, ${zoogi.secondaryColor}, ${zoogi.color})`,
          }}
        />
      )}
      <h3 className="text-white font-bold text-sm text-center">{zoogi.name}</h3>
      <p className="text-white/50 text-xs text-center">{zoogi.type}</p>
      
      {isSelected && (
        <motion.div
          className="absolute -top-1 -right-1 w-6 h-6 bg-yellow-400 rounded-full flex items-center justify-center"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
        >
          <span className="text-black font-bold text-sm">✓</span>
        </motion.div>
      )}
    </motion.button>
  );
}

function CustomZoogiSlide({ customZoogi, isSelected, onClick }: { customZoogi: CustomZoogiData; isSelected: boolean; onClick: () => void }) {
  return (
    <motion.button
      whileTap={{ scale: 0.95 }}
      onClick={onClick}
      className={`relative flex flex-col items-center p-3 rounded-2xl transition-all min-w-[100px] ${
        isSelected 
          ? "bg-gradient-to-b from-violet-500/30 to-fuchsia-500/20 ring-3 ring-violet-400" 
          : "bg-gradient-to-b from-violet-500/20 to-fuchsia-500/10 hover:from-violet-500/30"
      }`}
    >
      {customZoogi.thumbnailUrl ? (
        <div className="w-16 h-16 rounded-full mx-auto mb-1 shadow-lg overflow-hidden border-2 border-violet-400/50">
          <img 
            src={customZoogi.thumbnailUrl} 
            alt={customZoogi.name}
            className="w-full h-full object-cover"
          />
        </div>
      ) : (
        <div className="w-16 h-16 rounded-full mx-auto mb-1 shadow-lg bg-gradient-to-br from-violet-500 to-fuchsia-500 flex items-center justify-center">
          <Wand2 className="w-6 h-6 text-white" />
        </div>
      )}
      <h3 className="text-white font-bold text-sm text-center">{customZoogi.name}</h3>
      <p className="text-violet-300 text-xs text-center">Custom</p>
      
      {isSelected && (
        <motion.div
          className="absolute -top-1 -right-1 w-6 h-6 bg-violet-400 rounded-full flex items-center justify-center"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
        >
          <span className="text-white font-bold text-sm">✓</span>
        </motion.div>
      )}
    </motion.button>
  );
}

export function CharacterSelection() {
  const { selectedZoogi, selectZoogi, selectCustomZoogi, selectedCustomZoogi, setPhase, gameMode, startPracticeGame, aiPlayerCount, incrementAiPlayerCount, decrementAiPlayerCount } = useZoogiGame();
  const [customZoogis, setCustomZoogis] = useState<CustomZoogiData[]>([]);
  const [isLoadingCustom, setIsLoadingCustom] = useState(true);
  const [showAuthModal, setShowAuthModal] = useState(false);
  const deviceId = useMemo(() => getDeviceId(), []);
  
  const [emblaRef, emblaApi] = useEmblaCarousel({ 
    loop: false, 
    align: "center",
    containScroll: "trimSnaps"
  });
  
  const [canScrollPrev, setCanScrollPrev] = useState(false);
  const [canScrollNext, setCanScrollNext] = useState(false);
  
  const scrollPrev = useCallback(() => emblaApi?.scrollPrev(), [emblaApi]);
  const scrollNext = useCallback(() => emblaApi?.scrollNext(), [emblaApi]);
  
  const onSelect = useCallback(() => {
    if (!emblaApi) return;
    setCanScrollPrev(emblaApi.canScrollPrev());
    setCanScrollNext(emblaApi.canScrollNext());
  }, [emblaApi]);
  
  useEffect(() => {
    if (!emblaApi) return;
    onSelect();
    emblaApi.on("select", onSelect);
    emblaApi.on("reInit", onSelect);
    return () => {
      emblaApi.off("select", onSelect);
      emblaApi.off("reInit", onSelect);
    };
  }, [emblaApi, onSelect]);
  
  useEffect(() => {
    const loadCustomZoogis = async () => {
      try {
        const response = await fetch(`/api/custom-zoogis?deviceId=${encodeURIComponent(deviceId)}`);
        if (response.ok) {
          const data = await response.json();
          const zoogis: CustomZoogiData[] = data.data.map((z: any) => ({
            id: `db_${z.id}`,
            name: z.name,
            modelUrl: `/api/meshy/download/${z.meshyTaskId}`,
            thumbnailUrl: z.thumbnailUrl || undefined,
            stats: z.stats,
            createdAt: new Date(z.createdAt).getTime(),
          }));
          setCustomZoogis(zoogis);
        }
      } catch (e) {
        console.error("Failed to load custom Zoogis:", e);
      } finally {
        setIsLoadingCustom(false);
      }
    };
    loadCustomZoogis();
  }, [deviceId]);
  
  const displayZoogi = selectedZoogi || ZOOGI_ROSTER[0];
  const hasSelection = selectedZoogi || selectedCustomZoogi;

  return (
    <div className="fixed inset-0 flex flex-col overflow-hidden">
      <div className="absolute top-4 right-4 z-20">
        <motion.button
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
          onClick={() => setShowAuthModal(true)}
          className="flex items-center gap-2 px-4 py-2 rounded-full bg-white/20 backdrop-blur-sm text-white font-semibold"
        >
          <LogIn size={18} />
          <span>Sign In</span>
        </motion.button>
      </div>
      
      {showAuthModal && (
        <AuthModal isOpen={showAuthModal} onClose={() => setShowAuthModal(false)} />
      )}
      
      <div className="flex-1 flex flex-col p-4 relative z-10 overflow-y-auto">
        <motion.h1
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
          className="text-2xl md:text-3xl font-bold text-white text-center mb-4 mt-40"
        >
          Choose Your Zoogi
        </motion.h1>
        
        <div className="relative mb-4">
          <div className="overflow-hidden" ref={emblaRef}>
            <div className="flex gap-2 px-4">
              {ZOOGI_ROSTER.map((zoogi) => (
                <div key={zoogi.id} className="flex-shrink-0">
                  <ZoogiSlide
                    zoogi={zoogi}
                    isSelected={selectedZoogi?.id === zoogi.id}
                    onClick={() => selectZoogi(zoogi)}
                  />
                </div>
              ))}
            </div>
          </div>
          
          {canScrollPrev && (
            <button
              onClick={scrollPrev}
              className="absolute left-0 top-1/2 -translate-y-1/2 w-8 h-8 bg-black/60 rounded-full flex items-center justify-center text-white z-10"
            >
              <ChevronLeft size={20} />
            </button>
          )}
          {canScrollNext && (
            <button
              onClick={scrollNext}
              className="absolute right-0 top-1/2 -translate-y-1/2 w-8 h-8 bg-black/60 rounded-full flex items-center justify-center text-white z-10"
            >
              <ChevronRight size={20} />
            </button>
          )}
        </div>
        
        {customZoogis.length > 0 && (
          <div className="mb-4">
            <h2 className="text-sm text-violet-300 font-semibold mb-2 flex items-center gap-2 px-4">
              <Wand2 className="w-4 h-4" />
              Your Custom Zoogis
            </h2>
            <div className="flex gap-2 px-4 overflow-x-auto pb-2">
              {customZoogis.map((customZoogi) => (
                <div key={customZoogi.id} className="flex-shrink-0">
                  <CustomZoogiSlide
                    customZoogi={customZoogi}
                    isSelected={selectedCustomZoogi?.id === customZoogi.id}
                    onClick={() => selectCustomZoogi(customZoogi)}
                  />
                </div>
              ))}
            </div>
          </div>
        )}
        
        <AnimatePresence mode="wait">
          {selectedZoogi && (
            <motion.div
              key={displayZoogi.id}
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -10 }}
              className="bg-black/40 backdrop-blur-sm rounded-2xl p-4 mx-auto w-full max-w-md"
            >
              <div className="flex items-center gap-3 mb-3">
                {PORTRAIT_IMAGES[displayZoogi.id] ? (
                  <div className="w-12 h-12 rounded-full shadow-lg overflow-hidden border-2 border-white/30">
                    <img 
                      src={PORTRAIT_IMAGES[displayZoogi.id]} 
                      alt={displayZoogi.name}
                      className="w-full h-full object-cover"
                    />
                  </div>
                ) : (
                  <div
                    className="w-12 h-12 rounded-full shadow-lg"
                    style={{
                      background: `radial-gradient(circle at 30% 30%, ${displayZoogi.secondaryColor}, ${displayZoogi.color})`,
                    }}
                  />
                )}
                <div className="flex-1">
                  <h2 className="text-xl font-bold text-white">{displayZoogi.name}</h2>
                  <p className="text-white/60 text-sm">{displayZoogi.type} Type</p>
                </div>
              </div>

              <div className="mb-3 p-2 bg-white/10 rounded-lg">
                <h3 className="text-yellow-400 font-semibold text-sm">
                  {displayZoogi.ability}
                </h3>
                <p className="text-white/70 text-xs">
                  {displayZoogi.abilityDescription}
                </p>
              </div>

              <div className="space-y-1">
                <StatBar label="Speed" value={displayZoogi.stats.speed} color="#3B82F6" />
                <StatBar label="Power" value={displayZoogi.stats.power} color="#EF4444" />
                <StatBar label="Defense" value={displayZoogi.stats.defense} color="#22C55E" />
                <StatBar label="Control" value={displayZoogi.stats.control} color="#A855F7" />
              </div>
            </motion.div>
          )}
          {selectedCustomZoogi && !selectedZoogi && (
            <motion.div
              key={selectedCustomZoogi.id}
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -10 }}
              className="bg-gradient-to-b from-violet-900/40 to-fuchsia-900/40 backdrop-blur-sm rounded-2xl p-4 mx-auto w-full max-w-md border border-violet-500/30"
            >
              <div className="flex items-center gap-3 mb-3">
                {selectedCustomZoogi.thumbnailUrl ? (
                  <div className="w-12 h-12 rounded-full shadow-lg overflow-hidden border-2 border-violet-400/50">
                    <img 
                      src={selectedCustomZoogi.thumbnailUrl} 
                      alt={selectedCustomZoogi.name}
                      className="w-full h-full object-cover"
                    />
                  </div>
                ) : (
                  <div className="w-12 h-12 rounded-full shadow-lg bg-gradient-to-br from-violet-500 to-fuchsia-500 flex items-center justify-center">
                    <Wand2 className="w-5 h-5 text-white" />
                  </div>
                )}
                <div className="flex-1">
                  <h2 className="text-xl font-bold text-white">{selectedCustomZoogi.name}</h2>
                  <p className="text-violet-300 text-sm">Custom Zoogi</p>
                </div>
              </div>

              <div className="space-y-1">
                <StatBar label="Speed" value={(selectedCustomZoogi.stats?.speed || 5) * 10} color="#3B82F6" />
                <StatBar label="Power" value={(selectedCustomZoogi.stats?.power || 5) * 10} color="#EF4444" />
                <StatBar label="Control" value={(selectedCustomZoogi.stats?.control || 5) * 10} color="#A855F7" />
                <StatBar label="Ability" value={(selectedCustomZoogi.stats?.ability || 5) * 10} color="#22C55E" />
              </div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      {gameMode === "classic" && (
        <div className="px-4 pb-2 relative z-10">
          <div className="flex items-center justify-center gap-3 p-3 bg-white/10 backdrop-blur-sm rounded-xl max-w-xs mx-auto">
            <Users className="w-5 h-5 text-white/70" />
            <span className="text-white/80 text-sm font-medium">AI Opponents:</span>
            <div className="flex items-center gap-2">
              <motion.button
                whileTap={{ scale: 0.9 }}
                onClick={decrementAiPlayerCount}
                className="w-8 h-8 flex items-center justify-center bg-white/20 hover:bg-white/30 rounded-full transition-colors"
                disabled={aiPlayerCount === 0}
              >
                <Minus className="w-4 h-4 text-white" />
              </motion.button>
              <span className="text-white font-bold text-lg w-6 text-center">{aiPlayerCount}</span>
              <motion.button
                whileTap={{ scale: 0.9 }}
                onClick={incrementAiPlayerCount}
                className="w-8 h-8 flex items-center justify-center bg-white/20 hover:bg-white/30 rounded-full transition-colors"
                disabled={aiPlayerCount === 3}
              >
                <Plus className="w-4 h-4 text-white" />
              </motion.button>
            </div>
          </div>
        </div>
      )}

      <div className="p-4 flex justify-between gap-4 relative z-10 bg-gradient-to-t from-black/80 to-transparent pt-6">
        <motion.button
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
          onClick={() => setPhase("menu")}
          className="px-6 py-3 bg-white/10 text-white font-semibold rounded-full hover:bg-white/20 transition-colors"
        >
          Back
        </motion.button>
        
        <motion.button
          whileHover={{ scale: hasSelection ? 1.05 : 1 }}
          whileTap={{ scale: hasSelection ? 0.95 : 1 }}
          onClick={() => {
            if (!hasSelection) return;
            if (gameMode === "practice") {
              startPracticeGame();
            } else {
              setPhase("map_selection");
            }
          }}
          className={`flex-1 max-w-xs px-6 py-3 font-bold rounded-full transition-all ${
            hasSelection 
              ? "bg-gradient-to-r from-green-500 to-emerald-600 text-white shadow-lg shadow-green-500/50" 
              : "bg-white/20 text-white/50 cursor-not-allowed"
          }`}
          disabled={!hasSelection}
        >
          {hasSelection ? (gameMode === "practice" ? "Start Practice →" : "Choose Arena →") : "Select a Zoogi"}
        </motion.button>
      </div>
    </div>
  );
}
