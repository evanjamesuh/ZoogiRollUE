import { useState, useEffect, useRef, useCallback } from "react";
import { useZoogiGame, ZOOGI_ROSTER } from "@/lib/stores/useZoogiGame";
import { 
  Play, Users, Wand2, ShoppingBag, Book, Trophy, 
  ImageIcon, Gift, Music, Sparkles, Palette,
  LogIn, UserCircle, ChevronRight,
  Map, Shield, Award, Video, Target, Clock, CheckCircle,
  Home, Swords, Coins, Star, Zap, Flame, Lock, User, Wrench
} from "lucide-react";
import { useAuth } from "@/lib/stores/useAuth";
import { useProgression } from "@/lib/stores/useProgression";
import useEmblaCarousel from "embla-carousel-react";
import { CinematicLogo } from "@/components/ui/CinematicLogo";

interface MainMenuTownProps {
  onNavigate: (action: string) => void;
}

function AnimatedDotsBackground() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    
    let animationId: number;
    let dots: { x: number; y: number; baseY: number; phase: number; size: number }[] = [];
    
    const resize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
      initDots();
    };
    
    const initDots = () => {
      dots = [];
      const spacing = 40;
      const cols = Math.ceil(canvas.width / spacing) + 1;
      const rows = Math.ceil(canvas.height / spacing) + 1;
      
      for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
          dots.push({
            x: col * spacing,
            y: row * spacing,
            baseY: row * spacing,
            phase: (col + row) * 0.3,
            size: 3 + Math.random() * 3
          });
        }
      }
    };
    
    const animate = (time: number) => {
      ctx.fillStyle = '#222222';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      
      dots.forEach(dot => {
        const wave = Math.sin(time * 0.001 + dot.phase) * 15;
        const y = dot.baseY + wave;
        
        const perspective = 1 - (y / canvas.height) * 0.5;
        const size = dot.size * perspective;
        
        const gradient = ctx.createRadialGradient(dot.x, y, 0, dot.x, y, size * 2);
        gradient.addColorStop(0, '#ff7700');
        gradient.addColorStop(0.5, '#ff4400');
        gradient.addColorStop(1, 'transparent');
        
        ctx.beginPath();
        ctx.arc(dot.x, y, size, 0, Math.PI * 2);
        ctx.fillStyle = gradient;
        ctx.fill();
      });
      
      animationId = requestAnimationFrame(animate);
    };
    
    resize();
    window.addEventListener('resize', resize);
    animationId = requestAnimationFrame(animate);
    
    return () => {
      window.removeEventListener('resize', resize);
      cancelAnimationFrame(animationId);
    };
  }, []);
  
  return (
    <canvas 
      ref={canvasRef} 
      className="absolute inset-0"
      style={{ background: '#222222' }}
    />
  );
}

const ALL_ZOOGIS = ZOOGI_ROSTER;

export function MainMenuTown({ onNavigate }: MainMenuTownProps) {
  const [showSplash, setShowSplash] = useState(true);
  const [showSessionInfo, setShowSessionInfo] = useState(false);
  const [selectedSlide, setSelectedSlide] = useState(0);
  const { user, sessionInfo } = useAuth();
  const { setPhase } = useZoogiGame();
  const { level, xp, getXpProgress, totalGamesPlayed, totalWins, totalOrbsCollected, totalKnockouts, highestScore, loginStreak, ownedZoogis, favoriteZoogi } = useProgression();
  const xpProgress = getXpProgress();
  const [emblaRef, emblaApi] = useEmblaCarousel({ loop: false });

  const onSelect = useCallback(() => {
    if (!emblaApi) return;
    setSelectedSlide(emblaApi.selectedScrollSnap());
  }, [emblaApi]);

  useEffect(() => {
    if (!emblaApi) return;
    emblaApi.on('select', onSelect);
    return () => { emblaApi.off('select', onSelect); };
  }, [emblaApi, onSelect]);

  const handleTapToEnter = () => {
    setShowSplash(false);
  };

  const formatSessionExpiry = (daysRemaining: number | undefined): string => {
    if (!daysRemaining) return "Active";
    if (daysRemaining <= 0) return "Expired";
    if (daysRemaining === 1) return "1 day left";
    return `${daysRemaining} days left`;
  };

  if (showSplash) {
    return (
      <div 
        className="absolute inset-0 cursor-pointer"
        onClick={handleTapToEnter}
        onTouchEnd={handleTapToEnter}
      >
        <AnimatedDotsBackground />
        <CinematicLogo />
      </div>
    );
  }

  return (
    <div className="absolute inset-0 bg-gradient-to-b from-gray-900 via-purple-900/50 to-gray-900 overflow-auto">
      <div className="absolute inset-0 pointer-events-none">
        <AnimatedDotsBackground />
      </div>
      <div className="absolute inset-0 bg-gradient-to-b from-black/40 via-transparent to-black/60 pointer-events-none" />
      
      <div className="relative z-10 min-h-full flex flex-col pb-8">
        <div className="flex items-center justify-between p-4">
          <div className="flex items-center gap-2">
            <button
              onClick={() => setShowSplash(true)}
              className="w-10 h-10 rounded-full bg-green-500/80 backdrop-blur-sm flex items-center justify-center text-white hover:bg-green-500 transition-all"
            >
              <Home size={20} />
            </button>
            <button
              onClick={() => onNavigate("radio")}
              className="w-10 h-10 rounded-full bg-white/20 backdrop-blur-sm flex items-center justify-center text-white hover:bg-white/30 transition-all"
            >
              <Music size={20} />
            </button>
          </div>
          
          <div className="flex items-center gap-2">
            {user ? (
              <>
                <button
                  onClick={() => setShowSessionInfo(!showSessionInfo)}
                  className="p-2 bg-green-500/30 backdrop-blur-sm rounded-full hover:bg-green-500/40 transition-all"
                >
                  <CheckCircle size={16} className="text-green-400" />
                </button>
                <button
                  onClick={() => onNavigate("profile")}
                  className="p-2 bg-white/20 backdrop-blur-sm rounded-full hover:bg-white/30 transition-all flex items-center gap-2"
                >
                  <UserCircle size={24} className="text-white" />
                </button>
              </>
            ) : (
              <button
                onClick={() => onNavigate("login")}
                className="px-4 py-2 bg-white/20 backdrop-blur-sm rounded-full hover:bg-white/30 transition-all flex items-center gap-2"
              >
                <ChevronRight size={18} className="text-white" />
                <span className="text-white text-sm font-medium">Sign In</span>
              </button>
            )}
          </div>
        </div>

        {user && showSessionInfo && (
          <div className="absolute top-16 right-4 z-20 bg-black/70 backdrop-blur-md rounded-xl p-3 text-white text-xs min-w-[160px]">
            <div className="flex items-center gap-2 mb-2">
              <Clock size={14} className="text-cyan-400" />
              <span className="font-semibold">Session Status</span>
            </div>
            <div className="space-y-1 text-white/80">
              <div className="flex justify-between">
                <span>Status:</span>
                <span className="text-green-400 font-medium">Active</span>
              </div>
              <div className="flex justify-between">
                <span>Expires:</span>
                <span className="text-cyan-300">{formatSessionExpiry(sessionInfo?.daysRemaining)}</span>
              </div>
            </div>
          </div>
        )}

        <div className="flex-1 flex flex-col items-center px-4 pt-8">
          <div className="w-full max-w-sm overflow-hidden cursor-grab active:cursor-grabbing" ref={emblaRef} style={{ touchAction: 'pan-y' }}>
            <div className="flex">
              <div className="flex-shrink-0 w-full min-w-0 px-1">
                <div className="flex flex-col gap-3">
                  <button
                    onClick={() => {
                      onNavigate("enter_battle_stadium");
                    }}
                    className="flex items-center justify-center gap-3 px-8 py-4 bg-gradient-to-r from-green-500 to-emerald-600 text-white text-xl font-bold rounded-full shadow-lg shadow-green-500/40 hover:shadow-green-500/60 transition-all active:scale-95"
                  >
                    <Play size={24} fill="white" />
                    Enter Battle Stadium
                  </button>

                  <button
                    onClick={() => onNavigate("multiplayer")}
                    className="flex items-center justify-center gap-3 px-8 py-4 bg-gradient-to-r from-purple-500 to-violet-600 text-white text-xl font-bold rounded-full shadow-lg shadow-purple-500/40 hover:shadow-purple-500/60 transition-all active:scale-95"
                  >
                    <Users size={24} />
                    Multiplayer
                  </button>

                  <button
                    onClick={() => onNavigate("marble_arena")}
                    className="flex items-center justify-center gap-3 px-8 py-4 bg-gradient-to-r from-amber-500 to-orange-600 text-white text-xl font-bold rounded-full shadow-lg shadow-amber-500/40 hover:shadow-amber-500/60 transition-all active:scale-95"
                  >
                    <Target size={24} />
                    Marble Arena
                  </button>
                </div>

                  <div className="w-full grid grid-cols-2 gap-3 mt-4">
            <button
              onClick={() => onNavigate("enter_battle_stadium")}
              className="bg-gradient-to-br from-orange-500 via-red-500 to-purple-600 rounded-2xl p-4 flex flex-col items-center gap-2 hover:scale-[1.02] transition-all active:scale-95 shadow-lg relative overflow-hidden"
            >
              <div className="absolute inset-0 bg-[radial-gradient(circle_at_50%_0%,rgba(255,255,255,0.15),transparent_60%)]" />
              <div className="w-12 h-12 bg-white/20 rounded-xl flex items-center justify-center relative">
                <Flame size={24} className="text-white" />
              </div>
              <div className="flex items-center gap-1">
                <Sparkles size={12} className="text-yellow-300" />
                <span className="text-yellow-300 text-xs font-bold">NEW</span>
              </div>
              <span className="text-white font-bold text-sm">The Ringer Trials</span>
            </button>

            <button
              onClick={() => onNavigate("create_arena")}
              className="bg-gradient-to-br from-emerald-500 to-teal-600 rounded-2xl p-4 flex flex-col items-center gap-2 hover:scale-[1.02] transition-all active:scale-95 shadow-lg"
            >
              <div className="w-12 h-12 bg-white/20 rounded-xl flex items-center justify-center">
                <Book size={24} className="text-white" />
              </div>
              <div className="flex items-center gap-1">
                <Sparkles size={12} className="text-cyan-300" />
                <span className="text-cyan-300 text-xs font-bold">3D</span>
              </div>
              <span className="text-white font-bold">3D Comic</span>
            </button>
          </div>

          <div className="w-full max-w-sm grid grid-cols-3 gap-2 mt-6">
            <button
              onClick={() => onNavigate("challenges")}
              className="flex items-center gap-2 px-3 py-3 bg-gray-800/80 backdrop-blur rounded-xl hover:bg-gray-700/80 transition-all"
            >
              <Target size={18} className="text-green-400" />
              <span className="text-white text-sm font-medium">Challenges</span>
            </button>
            <button
              onClick={() => onNavigate("battle_pass")}
              className="flex items-center gap-2 px-3 py-3 bg-gradient-to-r from-amber-600 to-orange-600 rounded-xl hover:opacity-90 transition-all"
            >
              <Swords size={18} className="text-white" />
              <span className="text-white text-sm font-medium">Battle Pass</span>
            </button>
            <button
              onClick={() => onNavigate("customization")}
              className="flex items-center gap-2 px-3 py-3 bg-gradient-to-r from-fuchsia-500 to-pink-500 rounded-xl hover:opacity-90 transition-all"
            >
              <Palette size={18} className="text-white" />
              <span className="text-white text-sm font-medium">Customize</span>
            </button>
          </div>

          <div className="w-full max-w-sm grid grid-cols-4 gap-2 mt-4">
            <button
              onClick={() => onNavigate("shop")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-blue-600/80 rounded-xl hover:bg-blue-500/80 transition-all"
            >
              <ShoppingBag size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Shop</span>
            </button>
            <button
              onClick={() => onNavigate("collections")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-orange-600/80 rounded-xl hover:bg-orange-500/80 transition-all"
            >
              <ImageIcon size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Arenas</span>
            </button>
            <button
              onClick={() => onNavigate("zoogipedia")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-cyan-600/80 rounded-xl hover:bg-cyan-500/80 transition-all"
            >
              <Book size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Wiki</span>
            </button>
            <button
              onClick={() => onNavigate("collections")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-pink-600/80 rounded-xl hover:bg-pink-500/80 transition-all"
            >
              <ImageIcon size={18} className="text-white" />
              <span className="text-white text-xs font-medium">My Stuff</span>
            </button>
          </div>

          <div className="w-full max-w-sm grid grid-cols-4 gap-2 mt-2">
            <button
              onClick={() => onNavigate("leaderboard")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-yellow-600/80 rounded-xl hover:bg-yellow-500/80 transition-all"
            >
              <Trophy size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Scores</span>
            </button>
            <button
              onClick={() => onNavigate("community")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-green-600/80 rounded-xl hover:bg-green-500/80 transition-all"
            >
              <ImageIcon size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Gallery</span>
            </button>
            <button
              onClick={() => onNavigate("arena_showcase")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-violet-600/80 rounded-xl hover:bg-violet-500/80 transition-all"
            >
              <Map size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Showcase</span>
            </button>
            <button
              onClick={() => onNavigate("clans")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-purple-600/80 rounded-xl hover:bg-purple-500/80 transition-all"
            >
              <Shield size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Clans</span>
            </button>
          </div>

          <div className="w-full max-w-sm grid grid-cols-4 gap-2 mt-2">
            <button
              onClick={() => onNavigate("tournaments")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-red-600/80 rounded-xl hover:bg-red-500/80 transition-all"
            >
              <Award size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Tourneys</span>
            </button>
            <button
              onClick={() => onNavigate("replays")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-indigo-600/80 rounded-xl hover:bg-indigo-500/80 transition-all"
            >
              <Video size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Replays</span>
            </button>
            <button
              onClick={() => onNavigate("daily")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-amber-600/80 rounded-xl hover:bg-amber-500/80 transition-all"
            >
              <Gift size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Daily</span>
            </button>
            <button
              onClick={() => onNavigate("radio")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-fuchsia-600/80 rounded-xl hover:bg-fuchsia-500/80 transition-all"
            >
              <Music size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Radio</span>
            </button>
            <button
              onClick={() => onNavigate("customization")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-purple-600/80 rounded-xl hover:bg-purple-500/80 transition-all"
            >
              <Palette size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Customize</span>
            </button>
            <button
              onClick={() => onNavigate("create_zoogi")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-rose-600/80 rounded-xl hover:bg-rose-500/80 transition-all"
            >
              <Wand2 size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Create Zoogi</span>
            </button>
            <button
              onClick={() => onNavigate("map_editor")}
              className="flex flex-col items-center gap-1 px-3 py-3 bg-teal-600/80 rounded-xl hover:bg-teal-500/80 transition-all"
            >
              <Wrench size={18} className="text-white" />
              <span className="text-white text-xs font-medium">Map Edit</span>
            </button>
                  </div>
              </div>
              
              <div className="flex-shrink-0 w-full min-w-0 px-1">
                <div className="bg-black/50 backdrop-blur-sm rounded-2xl p-4">
                  <div className="flex items-center gap-2 mb-3">
                    <User className="w-5 h-5 text-cyan-400" />
                    <span className="text-white font-bold text-lg">My Profile</span>
                  </div>
                  
                  <div className="bg-black/40 rounded-xl p-3 mb-3">
                    <div className="flex items-center justify-between mb-2">
                      <div className="flex items-center gap-2">
                        <Star className="w-5 h-5 text-yellow-400" />
                        <span className="text-white font-bold">Level {level}</span>
                      </div>
                      <span className="text-white/60 text-sm">{xp}/{xpProgress.required} XP</span>
                    </div>
                    <div className="h-2 bg-black/50 rounded-full overflow-hidden">
                      <div 
                        className="h-full bg-gradient-to-r from-purple-500 to-blue-500 transition-all"
                        style={{ width: `${xpProgress.percent}%` }}
                      />
                    </div>
                  </div>
                  
                  <div className="grid grid-cols-2 gap-2 mb-3">
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="flex items-center justify-center gap-1 text-green-400 mb-1">
                        <Play size={12} />
                        <span className="text-xs">Games</span>
                      </div>
                      <span className="text-white font-bold">{totalGamesPlayed}</span>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="flex items-center justify-center gap-1 text-yellow-400 mb-1">
                        <Trophy size={12} />
                        <span className="text-xs">Wins</span>
                      </div>
                      <span className="text-white font-bold">{totalWins}</span>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="flex items-center justify-center gap-1 text-cyan-400 mb-1">
                        <Zap size={12} />
                        <span className="text-xs">Orbs</span>
                      </div>
                      <span className="text-white font-bold">{totalOrbsCollected}</span>
                    </div>
                    <div className="bg-white/10 rounded-lg p-2 text-center">
                      <div className="flex items-center justify-center gap-1 text-red-400 mb-1">
                        <Flame size={12} />
                        <span className="text-xs">KOs</span>
                      </div>
                      <span className="text-white font-bold">{totalKnockouts}</span>
                    </div>
                  </div>
                  
                  <div className="flex items-center justify-between text-xs text-white/60 mb-3">
                    <div className="flex items-center gap-1">
                      <Award size={12} className="text-orange-400" />
                      <span>High Score: <span className="text-white font-semibold">{highestScore}</span></span>
                    </div>
                    <div className="flex items-center gap-1">
                      <Flame size={12} className="text-red-400" />
                      <span>Streak: <span className="text-white font-semibold">{loginStreak} days</span></span>
                    </div>
                  </div>
                  
                  <div className="mb-3">
                    <div className="flex items-center justify-between mb-2">
                      <span className="text-white/70 text-xs uppercase tracking-wider">Collection</span>
                      <span className="text-cyan-400 text-xs font-semibold">{(ownedZoogis || []).length}/{ALL_ZOOGIS.length}</span>
                    </div>
                    <div className="grid grid-cols-8 gap-1">
                      {ALL_ZOOGIS.map((zoogi) => {
                        const isOwned = (ownedZoogis || []).includes(zoogi.id);
                        const isFavorite = favoriteZoogi === zoogi.id;
                        return (
                          <div
                            key={zoogi.id}
                            className={`relative w-8 h-8 rounded-full flex items-center justify-center ${
                              isOwned ? '' : 'opacity-30 grayscale'
                            }`}
                            style={{ backgroundColor: isOwned ? (zoogi.color || '#6B7280') : '#4B5563' }}
                            title={zoogi.name}
                          >
                            {!isOwned && (
                              <Lock size={10} className="text-white/60" />
                            )}
                            {isFavorite && isOwned && (
                              <Star size={10} className="text-yellow-400 absolute -top-1 -right-1" />
                            )}
                          </div>
                        );
                      })}
                    </div>
                  </div>
                  
                  {favoriteZoogi && (
                    <div className="text-center text-xs text-white/50 mb-3">
                      Favorite: <span className="text-white">{ALL_ZOOGIS.find(z => z.id === favoriteZoogi)?.name || 'None'}</span>
                    </div>
                  )}
                  
                  <button
                    onClick={() => onNavigate("customization")}
                    className="w-full flex items-center justify-center gap-2 px-4 py-3 bg-gradient-to-r from-fuchsia-500 to-pink-500 text-white font-bold rounded-xl hover:shadow-lg hover:shadow-fuchsia-500/30 transition-all active:scale-[0.98]"
                  >
                    <Palette size={18} />
                    Customize
                  </button>
                </div>
              </div>
            </div>
          </div>
          
          <div className="flex justify-center gap-2 mt-3">
            {[0, 1].map((idx) => (
              <button
                key={idx}
                onClick={() => emblaApi?.scrollTo(idx)}
                className={`w-2 h-2 rounded-full transition-all ${
                  selectedSlide === idx ? 'bg-white w-4' : 'bg-white/40'
                }`}
              />
            ))}
          </div>

          <div className="mt-4 text-center">
            <p className="text-white/40 text-xs font-medium tracking-widest uppercase">ZOOGI ROLL</p>
            <p className="text-white/60 text-sm mt-1">Swipe for profile • Drag to launch!</p>
          </div>
        </div>
      </div>
    </div>
  );
}
