import { useZoogiGame, ZOOGI_ROSTER } from "@/lib/stores/useZoogiGame";
import { Trophy, ShoppingBag, Book, Users, Wifi, Play, Lock, ChevronLeft, Star, Target, Gift, Calendar, ChevronRight, Sparkles, User, Zap, Award, Flame, Map, Music, Pause, Wand2, LogIn, MessageCircle, UserCircle, ImageIcon, Swords, Coins, Home, Palette } from "lucide-react";
import { useState, useEffect, useCallback } from "react";
import { useProgression, type Challenge, type BattlePassTier } from "@/lib/stores/useProgression";
import { useAudio } from "@/lib/stores/useAudio";
import { useAuth } from "@/lib/stores/useAuth";
import { ZoogiSpotlight } from "./ZoogiSpotlight";
import { PREMIUM_CHARACTERS } from "@/lib/premiumCharacters";
import { CreateZoogi } from "./CreateZoogi";
import { ComicViewer } from "./ComicViewer";
import { MyCollections } from "./MyCollections";
import { AuthModal } from "@/components/ui/AuthModal";
import { UserProfile } from "@/components/ui/UserProfile";
import { FriendsPanel } from "@/components/ui/FriendsPanel";
import { ChatPanel } from "@/components/ui/ChatPanel";
import { ZoogiGallery } from "@/components/ui/ZoogiGallery";
import { ArenaShowcase } from "@/components/ui/ArenaShowcase";
import { ClansPanel } from "@/components/ui/ClansPanel";
import { TournamentsPanel } from "@/components/ui/TournamentsPanel";
import { DailyBonusPanel } from "@/components/ui/DailyBonusPanel";
import { EnhancedLeaderboard } from "@/components/ui/EnhancedLeaderboard";
import { ReplaysPanel } from "@/components/ui/ReplaysPanel";
import { CustomizationPanel } from "@/components/ui/CustomizationPanel";
import { MainMenuTown } from "./MainMenuTown";
import useEmblaCarousel from "embla-carousel-react";

interface LeaderboardEntry {
  id: number;
  playerName: string;
  score: number;
  zoogiUsed: string;
}

const ALL_ZOOGIS = [
  ...ZOOGI_ROSTER.map(z => ({ id: z.id, name: z.name, color: z.color })),
  ...PREMIUM_CHARACTERS.map(z => ({ id: z.id, name: z.name, color: z.color }))
];

function MiniRadioPlayer() {
  const { menuMusic } = useAudio();
  const [isPlaying, setIsPlaying] = useState(false);
  
  const togglePlay = () => {
    if (!menuMusic) return;
    
    if (isPlaying) {
      menuMusic.pause();
      setIsPlaying(false);
    } else {
      menuMusic.currentTime = 0;
      menuMusic.play().catch(err => console.log("Radio play prevented:", err));
      setIsPlaying(true);
    }
  };
  
  useEffect(() => {
    if (menuMusic) {
      const handleEnded = () => setIsPlaying(false);
      const handlePlay = () => setIsPlaying(true);
      const handlePause = () => setIsPlaying(false);
      
      menuMusic.addEventListener('ended', handleEnded);
      menuMusic.addEventListener('play', handlePlay);
      menuMusic.addEventListener('pause', handlePause);
      
      setIsPlaying(!menuMusic.paused);
      
      return () => {
        menuMusic.removeEventListener('ended', handleEnded);
        menuMusic.removeEventListener('play', handlePlay);
        menuMusic.removeEventListener('pause', handlePause);
      };
    }
  }, [menuMusic]);
  
  return (
    <button
      onClick={togglePlay}
      className="w-full flex items-center gap-2 p-2 rounded-lg bg-white/10 hover:bg-white/20 transition-all"
    >
      <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center flex-shrink-0">
        {isPlaying ? (
          <Pause size={14} className="text-white" />
        ) : (
          <Play size={14} className="text-white ml-0.5" />
        )}
      </div>
      <div className="flex-1 text-left">
        <div className="text-white text-xs font-medium">Track 1</div>
        <div className="text-white/50 text-[10px]">6d 3h left</div>
      </div>
    </button>
  );
}

function rosterZoogiForCollection(item: { modelUrl: string; meshyTaskId: string }) {
  const fileId = item.modelUrl.match(/\/([^/?#]+)\.glb(?:$|\?)/i)?.[1]?.toLowerCase();
  const taskId = item.meshyTaskId.startsWith("default_")
    ? item.meshyTaskId.slice("default_".length).toLowerCase()
    : null;
  for (const id of [fileId, taskId]) {
    if (!id) continue;
    const match = ZOOGI_ROSTER.find((zoogi) => zoogi.id === id);
    if (match) return match;
  }
  return null;
}

export function MainMenu() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const setGameMode = useZoogiGame((state) => state.setGameMode);
  const selectZoogi = useZoogiGame((state) => state.selectZoogi);
  const selectCustomZoogi = useZoogiGame((state) => state.selectCustomZoogi);
  const { setMenuMusic, playMenuMusic, stopMenuMusic, isMuted } = useAudio();
  const { user, fetchMe } = useAuth();
  const [showLeaderboard, setShowLeaderboard] = useState(false);
  const [showMultiplayer, setShowMultiplayer] = useState(false);
  const [showChallenges, setShowChallenges] = useState(false);
  const [showBattlePass, setShowBattlePass] = useState(false);
  const [showCreateZoogi, setShowCreateZoogi] = useState(false);
  const [showComicViewer, setShowComicViewer] = useState(false);
  const [showAuthModal, setShowAuthModal] = useState(false);
  const [showUserProfile, setShowUserProfile] = useState(false);
  const [showFriendsPanel, setShowFriendsPanel] = useState(false);
  const [showChatPanel, setShowChatPanel] = useState(false);
  const [chatFriendId, setChatFriendId] = useState<number | null>(null);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [selectedSlide, setSelectedSlide] = useState(0);
  const [showGallery, setShowGallery] = useState(false);
  const [showArenaShowcase, setShowArenaShowcase] = useState(false);
  const [showClans, setShowClans] = useState(false);
  const [showTournaments, setShowTournaments] = useState(false);
  const [showDailyBonus, setShowDailyBonus] = useState(false);
  const [showReplays, setShowReplays] = useState(false);
  const [showMiniRadio, setShowMiniRadio] = useState(false);
  const [showTownView, setShowTownView] = useState(true);
  const [showCollections, setShowCollections] = useState(false);
  const [showCustomization, setShowCustomization] = useState(false);
  
  const [emblaRef, emblaApi] = useEmblaCarousel({ loop: false });

  const handleTownNavigate = useCallback((action: string) => {
    console.log("Town navigation:", action);
    switch (action) {
      case "play":
        setGameMode("classic");
        setPhase("character_selection");
        break;
      case "enter_battle_stadium":
        setPhase("ringer_creator");
        break;
      case "multiplayer":
        setShowMultiplayer(true);
        setShowTownView(false);
        break;
      case "practice":
        setGameMode("practice");
        setPhase("character_selection");
        break;
      case "marble_arena":
        setGameMode("classic");
        setPhase("character_selection");
        break;
      case "create_zoogi":
        setShowCreateZoogi(true);
        break;
      case "create_arena":
        setShowComicViewer(true);
        break;
      case "shop":
        setPhase("shop");
        break;
      case "zoogipedia":
        setPhase("zoogipedia");
        break;
      case "leaderboard":
        setShowLeaderboard(true);
        break;
      case "community":
        setShowGallery(true);
        break;
      case "challenges":
        setShowChallenges(true);
        break;
      case "battle_pass":
        setShowBattlePass(true);
        break;
      case "arena_showcase":
        setShowArenaShowcase(true);
        break;
      case "radio":
        setPhase("music_visualizer");
        break;
      case "daily":
        setShowDailyBonus(true);
        break;
      case "login":
        setShowAuthModal(true);
        break;
      case "profile":
        setShowUserProfile(true);
        break;
      case "clans":
        setShowClans(true);
        break;
      case "tournaments":
        setShowTournaments(true);
        break;
      case "replays":
        setShowReplays(true);
        break;
      case "customization":
        setShowCustomization(true);
        break;
      case "collections":
        setShowCollections(true);
        break;
      case "map_editor":
        setGameMode("map_editor");
        setPhase("map_selection");
        break;
      default:
        console.log("Unknown action:", action);
    }
  }, [setGameMode, setPhase]);

  useEffect(() => {
    fetchMe();
  }, [fetchMe]);

  const handleOpenDirectChat = (friendId: number) => {
    setChatFriendId(friendId);
    setShowFriendsPanel(false);
    setShowChatPanel(true);
  };
  
  useEffect(() => {
    const menuAudio = new Audio("/sounds/main_menu_music.mp3");
    menuAudio.loop = true;
    menuAudio.volume = 0.3;
    setMenuMusic(menuAudio);
    
    if (!isMuted) {
      menuAudio.play().catch(err => console.log("Menu music autoplay prevented:", err));
    }
    
    return () => {
      stopMenuMusic();
      setMenuMusic(null as any);
    };
  }, [setMenuMusic, stopMenuMusic, isMuted]);
  
  const onSelect = useCallback(() => {
    if (!emblaApi) return;
    const newSlide = emblaApi.selectedScrollSnap();
    setSelectedSlide(newSlide);
    if (newSlide === 0) {
      setShowMultiplayer(false);
    }
  }, [emblaApi]);
  
  useEffect(() => {
    if (!emblaApi) return;
    emblaApi.on('select', onSelect);
    return () => { emblaApi.off('select', onSelect); };
  }, [emblaApi, onSelect]);
  
  const { 
    level, 
    xp, 
    getXpProgress, 
    getActiveChallenges,
    refreshChallenges,
    battlePassTier, 
    battlePassXp, 
    hasPremiumPass, 
    getBattlePassTiers,
    claimBattlePassTier,
    claimedBattlePassTiers,
    totalGamesPlayed,
    totalWins,
    totalOrbsCollected,
    totalKnockouts,
    highestScore,
    loginStreak,
    favoriteZoogi,
    ownedZoogis,
    checkLoginStreak
  } = useProgression();
  
  const xpProgress = getXpProgress();
  const battlePassTiers = getBattlePassTiers();
  
  useEffect(() => {
    refreshChallenges();
    checkLoginStreak();
  }, []);
  
  const challenges = getActiveChallenges();

  useEffect(() => {
    if (showLeaderboard) {
      fetch("/api/leaderboard?limit=10")
        .then((res) => res.json())
        .then(setLeaderboard)
        .catch(console.error);
    }
  }, [showLeaderboard]);

  if (showTownView) {
    return (
      <>
        <MainMenuTown onNavigate={handleTownNavigate} />
        
        {showLeaderboard && (
          <EnhancedLeaderboard onClose={() => setShowLeaderboard(false)} />
        )}
        
        {showCreateZoogi && (
          <CreateZoogi 
            onBack={() => setShowCreateZoogi(false)}
            onZoogiCreated={(zoogi) => {
              console.log("Custom Zoogi created:", zoogi);
              setShowCreateZoogi(false);
            }}
          />
        )}

        {showComicViewer && (
          <ComicViewer 
            onBack={() => setShowComicViewer(false)}
          />
        )}
        
        {showGallery && (
          <ZoogiGallery onClose={() => setShowGallery(false)} />
        )}
        
        {showDailyBonus && (
          <DailyBonusPanel onClose={() => setShowDailyBonus(false)} />
        )}
        
        {showAuthModal && (
          <AuthModal isOpen={showAuthModal} onClose={() => setShowAuthModal(false)} />
        )}
        
        {showUserProfile && user && (
          <UserProfile isOpen={showUserProfile} onClose={() => setShowUserProfile(false)} user={user} />
        )}
        
        {showFriendsPanel && (
          <FriendsPanel 
            isOpen={showFriendsPanel}
            onClose={() => setShowFriendsPanel(false)}
            onOpenChat={handleOpenDirectChat}
          />
        )}
        
        {showChatPanel && (
          <ChatPanel 
            isOpen={showChatPanel}
            onClose={() => {
              setShowChatPanel(false);
              setChatFriendId(null);
            }}
            directMessageFriendId={chatFriendId}
          />
        )}
        
        {showClans && (
          <ClansPanel onClose={() => setShowClans(false)} />
        )}
        
        {showTournaments && (
          <TournamentsPanel onClose={() => setShowTournaments(false)} />
        )}
        
        {showReplays && (
          <ReplaysPanel onClose={() => setShowReplays(false)} />
        )}
        
        {showCollections && (
          <MyCollections
            onBack={() => setShowCollections(false)}
            onSelectZoogi={(zoogi) => {
              const rosterZoogi = rosterZoogiForCollection(zoogi);
              if (rosterZoogi) {
                selectZoogi(rosterZoogi);
              } else {
                selectCustomZoogi({
                  id: `collection_${zoogi.id}`,
                  name: zoogi.name,
                  modelUrl: zoogi.modelUrl,
                  thumbnailUrl: zoogi.thumbnailUrl || undefined,
                  stats: {
                    speed: zoogi.speed ?? 5,
                    power: zoogi.power ?? 5,
                    control: 5,
                    ability: 5,
                  },
                  createdAt: new Date(zoogi.createdAt).getTime(),
                });
              }
              setShowCollections(false);
              setGameMode("classic");
              setPhase("character_selection");
            }}
          />
        )}
        
        {showCustomization && (
          <CustomizationPanel onClose={() => setShowCustomization(false)} />
        )}
        
        {showArenaShowcase && (
          <ArenaShowcase onClose={() => setShowArenaShowcase(false)} />
        )}
        
        {showMiniRadio && (
          <div className="fixed top-16 left-4 z-50 bg-gray-900/95 backdrop-blur-sm rounded-xl p-3 shadow-xl border border-white/10 w-48">
            <MiniRadioPlayer />
          </div>
        )}
        
        {showChallenges && (
          <div
            className="fixed inset-0 flex items-center justify-center bg-black/70 z-50"
            onClick={(e) => e.target === e.currentTarget && setShowChallenges(false)}
          >
            <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl p-6 max-w-md w-full mx-4 max-h-[80vh] overflow-y-auto">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-2xl font-bold text-green-400 flex items-center gap-2">
                  <Target size={24} /> Challenges
                </h2>
                <button
                  onClick={() => setShowChallenges(false)}
                  className="text-white/60 hover:text-white text-2xl"
                >
                  ×
                </button>
              </div>

              <div className="mb-4">
                <h3 className="text-white/70 text-sm font-semibold flex items-center gap-2 mb-3">
                  <Calendar size={14} /> Daily Challenges
                </h3>
                <div className="space-y-2">
                  {challenges.filter(c => c.type === "daily").map(challenge => (
                    <div 
                      key={challenge.id}
                      className={`p-3 rounded-lg ${challenge.completed ? 'bg-green-500/20' : 'bg-white/5'}`}
                    >
                      <div className="flex items-center justify-between mb-1">
                        <span className="text-white font-medium">{challenge.name}</span>
                        <span className="text-yellow-400 text-sm font-bold">+{challenge.xpReward} XP</span>
                      </div>
                      <p className="text-white/60 text-xs mb-2">{challenge.description}</p>
                      <div className="flex items-center gap-2">
                        <div className="flex-1 h-2 bg-black/50 rounded-full overflow-hidden">
                          <div 
                            className={`h-full ${challenge.completed ? 'bg-green-500' : 'bg-blue-500'}`}
                            style={{ width: `${(challenge.progress / challenge.target) * 100}%` }}
                          />
                        </div>
                        <span className="text-white/70 text-xs">{challenge.progress}/{challenge.target}</span>
                        {challenge.completed && <span className="text-green-400 text-xs">Complete!</span>}
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              <div>
                <h3 className="text-white/70 text-sm font-semibold flex items-center gap-2 mb-3">
                  <Star size={14} /> Weekly Challenges
                </h3>
                <div className="space-y-2">
                  {challenges.filter(c => c.type === "weekly").map(challenge => (
                    <div 
                      key={challenge.id}
                      className={`p-3 rounded-lg ${challenge.completed ? 'bg-green-500/20' : 'bg-white/5'}`}
                    >
                      <div className="flex items-center justify-between mb-1">
                        <span className="text-white font-medium">{challenge.name}</span>
                        <span className="text-yellow-400 text-sm font-bold">+{challenge.xpReward} XP</span>
                      </div>
                      <p className="text-white/60 text-xs mb-2">{challenge.description}</p>
                      <div className="flex items-center gap-2">
                        <div className="flex-1 h-2 bg-black/50 rounded-full overflow-hidden">
                          <div 
                            className={`h-full ${challenge.completed ? 'bg-green-500' : 'bg-purple-500'}`}
                            style={{ width: `${(challenge.progress / challenge.target) * 100}%` }}
                          />
                        </div>
                        <span className="text-white/70 text-xs">{challenge.progress}/{challenge.target}</span>
                        {challenge.completed && <span className="text-green-400 text-xs">Complete!</span>}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}
        
        {showBattlePass && (
          <div
            className="fixed inset-0 flex items-center justify-center bg-black/70 z-50"
            onClick={(e) => e.target === e.currentTarget && setShowBattlePass(false)}
          >
            <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl p-6 max-w-lg w-full mx-4 max-h-[80vh] overflow-y-auto">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-2xl font-bold text-amber-400 flex items-center gap-2">
                  <Gift size={24} /> Battle Pass
                </h2>
                <button
                  onClick={() => setShowBattlePass(false)}
                  className="text-white/60 hover:text-white text-2xl"
                >
                  ×
                </button>
              </div>

              <div className="bg-black/40 rounded-lg p-4 mb-4">
                <div className="flex items-center justify-between mb-2">
                  <span className="text-white font-bold">Season 1</span>
                  <span className="text-amber-400 font-bold">Tier {battlePassTier}/50</span>
                </div>
                <div className="h-3 bg-black/50 rounded-full overflow-hidden mb-2">
                  <div 
                    className="h-full bg-gradient-to-r from-amber-500 to-orange-500"
                    style={{ width: `${(battlePassTier / 50) * 100}%` }}
                  />
                </div>
                {!hasPremiumPass && (
                  <button className="w-full mt-2 py-2 bg-gradient-to-r from-amber-500 to-orange-500 rounded-lg text-white font-bold text-sm">
                    Upgrade to Premium
                  </button>
                )}
              </div>

              <div className="space-y-2">
                {battlePassTiers.slice(0, 20).map((tier) => {
                  const isUnlocked = battlePassTier >= tier.tier;
                  const isClaimed = claimedBattlePassTiers.includes(tier.tier);
                  const canClaim = isUnlocked && !isClaimed;
                  
                  return (
                    <div 
                      key={tier.tier}
                      className={`flex items-center gap-3 p-3 rounded-lg ${isUnlocked ? 'bg-white/10' : 'bg-black/30'}`}
                    >
                      <div className={`w-10 h-10 rounded-full flex items-center justify-center font-bold ${isUnlocked ? 'bg-amber-500 text-white' : 'bg-gray-700 text-gray-400'}`}>
                        {tier.tier}
                      </div>
                      <div className="flex-1">
                        <div className="flex items-center gap-2">
                          <span className={`font-medium ${isUnlocked ? 'text-white' : 'text-gray-500'}`}>
                            {tier.freeReward?.name || 'XP Reward'}
                          </span>
                          {tier.premiumReward && (
                            <span className="text-amber-400 text-xs">
                              Premium: {tier.premiumReward.name}
                            </span>
                          )}
                        </div>
                      </div>
                      {canClaim && (
                        <button
                          onClick={() => claimBattlePassTier(tier.tier)}
                          className="px-3 py-1 bg-green-500 rounded text-white text-sm font-bold"
                        >
                          Claim
                        </button>
                      )}
                      {isClaimed && (
                        <span className="text-green-400 text-sm">Claimed</span>
                      )}
                    </div>
                  );
                })}
              </div>
            </div>
          </div>
        )}
      </>
    );
  }

  return (
    <div className="fixed inset-0 flex flex-col items-center overflow-y-auto overflow-x-hidden">
      
      <div className="absolute top-4 left-4 z-20 flex items-center gap-2">
        <button
          onClick={() => {
            setShowTownView(true);
            setShowMultiplayer(false);
          }}
          className="w-10 h-10 rounded-full bg-gradient-to-br from-green-500 to-emerald-600 backdrop-blur-sm flex items-center justify-center text-white hover:scale-105 transition-transform shadow-lg"
          title="Back to Town"
        >
          <Home size={18} />
        </button>
        
        <button
          onClick={() => setShowMiniRadio(!showMiniRadio)}
          className="w-10 h-10 rounded-full bg-gradient-to-br from-purple-600 to-pink-600 backdrop-blur-sm flex items-center justify-center text-white hover:scale-105 transition-transform shadow-lg"
        >
          <Music size={18} />
        </button>
        
        {showMiniRadio && (
          <div className="absolute top-12 left-0 bg-gray-900/95 backdrop-blur-sm rounded-xl p-3 shadow-xl border border-white/10 w-48">
            <div className="flex items-center gap-2 mb-2">
              <Music className="w-4 h-4 text-purple-400" />
              <span className="text-white text-sm font-semibold">Weekly Radio</span>
            </div>
            <MiniRadioPlayer />
          </div>
        )}
      </div>

      <div className="absolute top-4 right-4 z-20 flex items-center gap-2">
        {user ? (
          <>
            <button
              onClick={() => setShowChatPanel(!showChatPanel)}
              className="w-10 h-10 rounded-full bg-purple-600/80 backdrop-blur-sm flex items-center justify-center text-white hover:bg-purple-500 transition-colors"
            >
              <MessageCircle size={20} />
            </button>
            <button
              onClick={() => setShowFriendsPanel(true)}
              className="w-10 h-10 rounded-full bg-blue-600/80 backdrop-blur-sm flex items-center justify-center text-white hover:bg-blue-500 transition-colors"
            >
              <Users size={20} />
            </button>
            <button
              onClick={() => setShowUserProfile(true)}
              className="w-10 h-10 rounded-full bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center text-white font-bold text-lg hover:scale-105 transition-transform"
            >
              {(user.displayName || user.username)[0].toUpperCase()}
            </button>
          </>
        ) : (
          <button
            onClick={() => setShowAuthModal(true)}
            className="flex items-center gap-2 px-4 py-2 bg-white/10 backdrop-blur-sm rounded-full text-white hover:bg-white/20 transition-colors"
          >
            <LogIn size={18} />
            <span className="text-sm font-medium">Sign In</span>
          </button>
        )}
      </div>

      <div className="flex flex-col gap-3 relative z-10 w-full max-w-sm px-4 pt-24 pb-8">
        <div className="overflow-hidden" ref={emblaRef}>
          <div className="flex">
            <div className="flex-shrink-0 w-full min-w-0 px-1">
              {!showMultiplayer ? (
                <div className="flex flex-col gap-4">
                  <button
                    onClick={() => {
                      setGameMode("classic");
                      setPhase("character_selection");
                    }}
                    className="flex items-center justify-center gap-2 px-8 py-4 min-h-[56px] bg-gradient-to-r from-green-500 to-emerald-600 text-white text-xl font-bold rounded-full shadow-lg shadow-green-500/50 hover:shadow-green-500/70 transition-shadow active:scale-95"
                  >
                    <Play size={24} />
                    Play
                  </button>

                  <button
                    onClick={() => setShowMultiplayer(true)}
                    className="flex items-center justify-center gap-2 px-8 py-4 min-h-[56px] bg-gradient-to-r from-blue-500 to-purple-600 text-white text-xl font-bold rounded-full shadow-lg shadow-blue-500/50 hover:shadow-blue-500/70 transition-shadow active:scale-95"
                  >
                    <Users size={24} />
                    Multiplayer
                  </button>
                  
                  <div className="mt-3">
                    <button
                      onClick={() => setShowComicViewer(true)}
                      className="w-full bg-gradient-to-r from-green-600 via-emerald-500 to-teal-500 rounded-xl p-3 relative overflow-hidden group hover:shadow-lg hover:shadow-emerald-500/30 transition-all active:scale-[0.98]"
                    >
                      <div className="absolute inset-0 bg-gradient-to-r from-white/0 via-white/20 to-white/0 translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-700" />
                      <div className="flex flex-col items-center gap-2 text-center">
                        <div className="w-10 h-10 rounded-lg bg-white/20 backdrop-blur-sm flex items-center justify-center shadow-lg">
                          <Book className="w-5 h-5 text-white" />
                        </div>
                        <div>
                          <div className="flex items-center justify-center gap-1 mb-0.5">
                            <Sparkles className="w-3 h-3 text-cyan-300" />
                            <span className="text-cyan-300 text-[10px] font-bold uppercase">3D</span>
                          </div>
                          <div className="text-white font-bold text-sm">3D Comic</div>
                        </div>
                      </div>
                    </button>
                  </div>
                </div>
              ) : (
                <div className="flex flex-col gap-4">
                  <button
                    onClick={() => setShowMultiplayer(false)}
                    className="flex items-center justify-center gap-2 px-4 py-2 text-white/70 hover:text-white text-sm font-medium transition-colors self-start"
                  >
                    <ChevronLeft size={18} />
                    Back
                  </button>

                  <div className="flex flex-col gap-3">
                    <p className="text-white/50 text-xs font-semibold uppercase tracking-wider text-center flex items-center justify-center gap-2">
                      <Wifi size={12} />
                      Online
                    </p>
                    <button
                      onClick={() => {
                        setGameMode("ringer_royale");
                        setPhase("character_selection");
                      }}
                      className="flex items-center justify-center gap-2 px-6 py-3 min-h-[48px] bg-gradient-to-r from-purple-600 to-pink-600 text-white text-lg font-semibold rounded-full hover:shadow-lg hover:shadow-purple-500/30 transition-all active:scale-95"
                    >
                      Ringer Royale
                    </button>
                    <button
                      disabled
                      className="flex items-center justify-center gap-2 px-6 py-3 min-h-[48px] bg-gray-700/50 text-gray-400 text-lg font-semibold rounded-full cursor-not-allowed"
                    >
                      <Lock size={16} />
                      Co-op 2v2
                      <span className="text-xs bg-yellow-500/20 text-yellow-400 px-2 py-0.5 rounded-full">Soon</span>
                    </button>
                  </div>

                  <div className="flex flex-col gap-3">
                    <p className="text-white/50 text-xs font-semibold uppercase tracking-wider text-center">
                      Local
                    </p>
                    <button
                      onClick={() => setPhase("local_setup")}
                      className="flex items-center justify-center gap-2 px-6 py-3 min-h-[48px] bg-gradient-to-r from-blue-500 to-cyan-500 text-white text-lg font-semibold rounded-full hover:shadow-lg hover:shadow-blue-500/30 transition-all active:scale-95"
                    >
                      <Users size={20} />
                      Local Multiplayer
                    </button>
                    <button
                      onClick={() => {
                        setGameMode("classic");
                        setPhase("character_selection");
                      }}
                      className="flex items-center justify-center gap-2 px-6 py-3 min-h-[48px] bg-gradient-to-r from-emerald-500 to-teal-500 text-white text-lg font-semibold rounded-full hover:shadow-lg hover:shadow-emerald-500/30 transition-all active:scale-95"
                    >
                      <Swords size={20} />
                      Single Player
                    </button>
                  </div>
                </div>
              )}
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
                
                <div className="mb-2">
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
                  onClick={() => setShowCustomization(true)}
                  className="w-full flex items-center justify-center gap-2 px-4 py-3 bg-gradient-to-r from-fuchsia-500 to-pink-500 text-white font-bold rounded-xl hover:shadow-lg hover:shadow-fuchsia-500/30 transition-all active:scale-[0.98]"
                >
                  <Palette size={18} />
                  Customize
                </button>
              </div>
            </div>
          </div>
        </div>
        
        <div className="flex justify-center gap-2 mt-1">
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

        <div className="flex gap-2 mt-2">
          <button
            onClick={() => setShowChallenges(true)}
            className="flex-1 flex items-center justify-center gap-2 px-3 py-2.5 min-h-[40px] bg-gradient-to-r from-green-600 to-emerald-600 text-white text-xs font-semibold rounded-full hover:shadow-lg transition-all active:scale-95"
          >
            <Target size={14} />
            Challenges
          </button>
          <button
            onClick={() => setShowBattlePass(true)}
            className="flex-1 flex items-center justify-center gap-2 px-3 py-2.5 min-h-[40px] bg-gradient-to-r from-amber-500 to-orange-500 text-white text-xs font-semibold rounded-full hover:shadow-lg transition-all active:scale-95"
          >
            <Gift size={14} />
            Battle Pass
          </button>
        </div>

        <div className="grid grid-cols-4 gap-2 mt-2">
          <button
            onClick={() => setPhase("shop")}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-purple-500 to-pink-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <ShoppingBag size={14} />
            Shop
          </button>
          
          <button
            onClick={() => setPhase("arena_editor")}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-cyan-500 to-teal-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Map size={14} />
            Arenas
          </button>

          <button
            onClick={() => setShowCreateZoogi(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-violet-500 to-fuchsia-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Wand2 size={14} />
            Create
          </button>
          
          <button
            onClick={() => setPhase("zoogipedia")}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-amber-500 to-orange-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Book size={14} />
            Wiki
          </button>
          
          <button
            onClick={() => setShowLeaderboard(!showLeaderboard)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-yellow-500 to-amber-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Trophy size={14} />
            Scores
          </button>

          <button
            onClick={() => setShowGallery(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-pink-500 to-rose-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <ImageIcon size={14} />
            Gallery
          </button>
          
          <button
            onClick={() => setShowArenaShowcase(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-emerald-500 to-teal-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Map size={14} />
            Showcase
          </button>
          
          <button
            onClick={() => setShowClans(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-blue-500 to-indigo-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Users size={14} />
            Clans
          </button>
          
          <button
            onClick={() => setShowTournaments(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-rose-500 to-red-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Swords size={14} />
            Tourneys
          </button>
          
          <button
            onClick={() => setShowReplays(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-indigo-500 to-purple-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Play size={14} />
            Replays
          </button>
          
          <button
            onClick={() => setShowDailyBonus(true)}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-orange-500 to-red-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Gift size={14} />
            Daily
          </button>
          
          <button
            onClick={() => setPhase("music_visualizer")}
            className="flex items-center justify-center gap-1 px-2 py-2.5 min-h-[40px] bg-gradient-to-r from-fuchsia-500 to-pink-500 text-white text-[11px] font-semibold rounded-xl hover:shadow-lg transition-all active:scale-95"
          >
            <Music size={14} />
            Radio
          </button>
        </div>
      </div>

      {showLeaderboard && (
        <EnhancedLeaderboard onClose={() => setShowLeaderboard(false)} />
      )}

      <p className="mt-16 text-white/50 text-sm z-10">
        Drag to launch your Zoogi into battle!
      </p>

      {showChallenges && (
        <div
          className="absolute inset-0 flex items-center justify-center bg-black/70 z-50"
          onClick={(e) => e.target === e.currentTarget && setShowChallenges(false)}
        >
          <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl p-6 max-w-md w-full mx-4 max-h-[80vh] overflow-y-auto">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-2xl font-bold text-green-400 flex items-center gap-2">
                <Target size={24} /> Challenges
              </h2>
              <button
                onClick={() => setShowChallenges(false)}
                className="text-white/60 hover:text-white text-2xl"
              >
                ×
              </button>
            </div>

            <div className="mb-4">
              <h3 className="text-white/70 text-sm font-semibold flex items-center gap-2 mb-3">
                <Calendar size={14} /> Daily Challenges
              </h3>
              <div className="space-y-2">
                {challenges.filter(c => c.type === "daily").map(challenge => (
                  <div 
                    key={challenge.id}
                    className={`p-3 rounded-lg ${challenge.completed ? 'bg-green-500/20' : 'bg-white/5'}`}
                  >
                    <div className="flex items-center justify-between mb-1">
                      <span className="text-white font-medium">{challenge.name}</span>
                      <span className="text-yellow-400 text-sm font-bold">+{challenge.xpReward} XP</span>
                    </div>
                    <p className="text-white/60 text-xs mb-2">{challenge.description}</p>
                    <div className="flex items-center gap-2">
                      <div className="flex-1 h-2 bg-black/50 rounded-full overflow-hidden">
                        <div 
                          className={`h-full ${challenge.completed ? 'bg-green-500' : 'bg-blue-500'}`}
                          style={{ width: `${(challenge.progress / challenge.target) * 100}%` }}
                        />
                      </div>
                      <span className="text-white/70 text-xs">{challenge.progress}/{challenge.target}</span>
                      {challenge.completed && <span className="text-green-400 text-xs">Complete!</span>}
                    </div>
                  </div>
                ))}
              </div>
            </div>

            <div>
              <h3 className="text-white/70 text-sm font-semibold flex items-center gap-2 mb-3">
                <Calendar size={14} /> Weekly Challenges
              </h3>
              <div className="space-y-2">
                {challenges.filter(c => c.type === "weekly").map(challenge => (
                  <div 
                    key={challenge.id}
                    className={`p-3 rounded-lg ${challenge.completed ? 'bg-green-500/20' : 'bg-white/5'}`}
                  >
                    <div className="flex items-center justify-between mb-1">
                      <span className="text-white font-medium">{challenge.name}</span>
                      <span className="text-yellow-400 text-sm font-bold">+{challenge.xpReward} XP</span>
                    </div>
                    <p className="text-white/60 text-xs mb-2">{challenge.description}</p>
                    <div className="flex items-center gap-2">
                      <div className="flex-1 h-2 bg-black/50 rounded-full overflow-hidden">
                        <div 
                          className={`h-full ${challenge.completed ? 'bg-green-500' : 'bg-purple-500'}`}
                          style={{ width: `${(challenge.progress / challenge.target) * 100}%` }}
                        />
                      </div>
                      <span className="text-white/70 text-xs">{challenge.progress}/{challenge.target}</span>
                      {challenge.completed && <span className="text-green-400 text-xs">Complete!</span>}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      )}

      {showCreateZoogi && (
        <CreateZoogi 
          onBack={() => setShowCreateZoogi(false)}
          onZoogiCreated={(zoogi) => {
            console.log("Custom Zoogi created:", zoogi);
            setShowCreateZoogi(false);
          }}
        />
      )}

      {showComicViewer && (
        <ComicViewer 
          onBack={() => setShowComicViewer(false)}
        />
      )}

      {showBattlePass && (
        <div
          className="absolute inset-0 flex items-center justify-center bg-black/70 z-50"
          onClick={(e) => e.target === e.currentTarget && setShowBattlePass(false)}
        >
          <div className="bg-gradient-to-b from-gray-800 to-gray-900 rounded-2xl p-6 max-w-lg w-full mx-4 max-h-[80vh] overflow-y-auto">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-2xl font-bold text-amber-400 flex items-center gap-2">
                <Gift size={24} /> Battle Pass
              </h2>
              <button
                onClick={() => setShowBattlePass(false)}
                className="text-white/60 hover:text-white text-2xl"
              >
                ×
              </button>
            </div>

            <div className="bg-black/40 rounded-lg p-4 mb-4">
              <div className="flex items-center justify-between mb-2">
                <span className="text-white font-bold">Season 1</span>
                <span className="text-amber-400 font-bold">Tier {battlePassTier}/50</span>
              </div>
              <div className="h-3 bg-black/50 rounded-full overflow-hidden mb-2">
                <div 
                  className="h-full bg-gradient-to-r from-amber-500 to-orange-500"
                  style={{ width: `${(battlePassTier / 50) * 100}%` }}
                />
              </div>
              {!hasPremiumPass && (
                <button className="w-full mt-2 py-2 bg-gradient-to-r from-amber-500 to-orange-500 rounded-lg text-white font-bold text-sm">
                  Upgrade to Premium
                </button>
              )}
            </div>

            <div className="space-y-2">
              {battlePassTiers.slice(0, 20).map((tier) => {
                const isUnlocked = battlePassTier >= tier.tier;
                const isClaimed = claimedBattlePassTiers.includes(tier.tier);
                const canClaim = isUnlocked && !isClaimed;
                
                return (
                  <div 
                    key={tier.tier}
                    className={`flex items-center gap-3 p-3 rounded-lg ${isUnlocked ? 'bg-white/10' : 'bg-black/30'}`}
                  >
                    <div className={`w-10 h-10 rounded-full flex items-center justify-center font-bold ${isUnlocked ? 'bg-amber-500 text-white' : 'bg-gray-700 text-gray-400'}`}>
                      {tier.tier}
                    </div>
                    
                    <div className="flex-1 flex gap-2">
                      {tier.freeReward && (
                        <div className={`flex-1 p-2 rounded ${isClaimed ? 'bg-green-500/20' : 'bg-white/5'}`}>
                          <p className="text-white text-xs font-medium truncate">{tier.freeReward.name}</p>
                          <p className="text-white/50 text-[10px]">Free</p>
                        </div>
                      )}
                      {tier.premiumReward && (
                        <div className={`flex-1 p-2 rounded ${!hasPremiumPass ? 'bg-gray-700/50 opacity-50' : isClaimed ? 'bg-amber-500/20' : 'bg-amber-500/10'}`}>
                          <p className="text-amber-300 text-xs font-medium truncate">{tier.premiumReward.name}</p>
                          <p className="text-amber-400/50 text-[10px]">Premium</p>
                        </div>
                      )}
                    </div>
                    
                    {canClaim && (
                      <button 
                        onClick={() => claimBattlePassTier(tier.tier)}
                        className="px-3 py-1 bg-green-500 rounded text-white text-xs font-bold"
                      >
                        Claim
                      </button>
                    )}
                    {isClaimed && (
                      <span className="text-green-400 text-xs">Claimed</span>
                    )}
                  </div>
                );
              })}
            </div>
          </div>
        </div>
      )}

      <AuthModal isOpen={showAuthModal} onClose={() => setShowAuthModal(false)} />
      
      {user && (
        <>
          <UserProfile 
            isOpen={showUserProfile} 
            onClose={() => setShowUserProfile(false)} 
            user={user} 
          />
          <FriendsPanel 
            isOpen={showFriendsPanel} 
            onClose={() => setShowFriendsPanel(false)} 
            onOpenChat={handleOpenDirectChat}
          />
          <ChatPanel 
            isOpen={showChatPanel} 
            onClose={() => setShowChatPanel(false)} 
            directMessageFriendId={chatFriendId}
          />
          {showDailyBonus && (
            <DailyBonusPanel onClose={() => setShowDailyBonus(false)} />
          )}
        </>
      )}

      {showGallery && (
        <ZoogiGallery onClose={() => setShowGallery(false)} />
      )}

      {showArenaShowcase && (
        <ArenaShowcase onClose={() => setShowArenaShowcase(false)} />
      )}

      {showClans && (
        <ClansPanel onClose={() => setShowClans(false)} />
      )}

      {showTournaments && (
        <TournamentsPanel onClose={() => setShowTournaments(false)} />
      )}

      {showReplays && (
        <ReplaysPanel onClose={() => setShowReplays(false)} />
      )}
      
      {showCustomization && (
        <CustomizationPanel onClose={() => setShowCustomization(false)} />
      )}
    </div>
  );
}
