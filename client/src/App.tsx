import { useEffect, useCallback, useState } from "react";
import { useZoogiGame, ZOOGI_ROSTER, type MapTheme } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";

const initAudioOnInteraction = (() => {
  let initialized = false;
  return (initFn: () => void) => {
    if (initialized) return;
    const handler = () => {
      initFn();
      initialized = true;
      document.removeEventListener('click', handler);
      document.removeEventListener('touchstart', handler);
      document.removeEventListener('touchend', handler);
      document.removeEventListener('keydown', handler);
    };
    document.addEventListener('click', handler, { passive: true });
    document.addEventListener('touchstart', handler, { passive: true });
    document.addEventListener('touchend', handler, { passive: true });
    document.addEventListener('keydown', handler);
  };
})();
import { MainMenu } from "@/components/game/MainMenu";
import { CharacterSelection } from "@/components/game/CharacterSelection";
import { MapSelection } from "@/components/game/MapSelection";
import { Game } from "@/components/game/Game";
import { GameOver } from "@/components/game/GameOver";
import { RoundEnd } from "@/components/game/RoundEnd";
import { Shop } from "@/components/game/Shop";
import { Zoogipedia } from "@/components/game/Zoogipedia";
import { LocalPlayerSetup } from "@/components/game/LocalPlayerSetup";
import { ArenaEditor } from "@/components/game/ArenaEditor";
import { VideoBackground } from "@/components/game/VideoBackground";
import { FeatureHub } from "@/components/game/FeatureHub";
import { MusicVisualizer } from "@/components/game/MusicVisualizer";
import { VantaDotsBackground } from "@/components/ui/VantaDotsBackground";
import { PowerPreview } from "@/components/game/PowerPreview";
import "@fontsource/inter";

function usePowerPreview(): boolean {
  const [enabled] = useState(() => {
    if (typeof window === "undefined") return false;
    return new URLSearchParams(window.location.search).get("powerPreview") === "1";
  });
  return enabled;
}

const PLAY_MAPS: MapTheme[] = ["grass", "ice", "lava", "space", "saturn", "tomb", "neon"];

/** Opens a match directly: /?play=grass&debug=colliders&view=top */
function usePlayShortcut() {
  const selectMap = useZoogiGame((state) => state.selectMap);
  const selectZoogi = useZoogiGame((state) => state.selectZoogi);
  const startGame = useZoogiGame((state) => state.startGame);
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const play = params.get("play");
    if (!play || !PLAY_MAPS.includes(play as MapTheme)) return;
    selectZoogi(ZOOGI_ROSTER[0]);
    selectMap(play as MapTheme);
    startGame();
    if (params.get("view") === "top") {
      window.setTimeout(() => {
        if (!useZoogiGame.getState().birdsEyeView) useZoogiGame.getState().toggleBirdsEyeView();
      }, 80);
    }
  }, [selectMap, selectZoogi, startGame]);
}

function App() {
  const powerPreview = usePowerPreview();
  usePlayShortcut();
  const phase = useZoogiGame((state) => state.phase);

  useEffect(() => {
    const debug = new URLSearchParams(window.location.search).get("debug");
    if (debug === null || debug === "colliders") return;
    const debugWindow = window as unknown as {
      __zoogi?: typeof useZoogiGame;
      __roster?: typeof ZOOGI_ROSTER;
      __audio?: typeof useAudio;
    };
    debugWindow.__zoogi = useZoogiGame;
    debugWindow.__roster = ZOOGI_ROSTER;
    debugWindow.__audio = useAudio;
  }, []);
  const { setHitSound, setSuccessSound, setMenuMusic, menuMusic, isMuted, stopMenuMusic, initAudioContext } = useAudio();

  useEffect(() => {
    initAudioOnInteraction(initAudioContext);

    const hitSound = new Audio("/sounds/hit.mp3");
    hitSound.volume = 0.5;
    hitSound.preload = "auto";
    hitSound.load();
    setHitSound(hitSound);

    const successSound = new Audio("/sounds/success.mp3");
    successSound.volume = 0.5;
    successSound.preload = "auto";
    successSound.load();
    setSuccessSound(successSound);

    const menuMusicAudio = new Audio("/sounds/main_menu_music.mp3");
    menuMusicAudio.volume = 0.3;
    menuMusicAudio.loop = true;
    menuMusicAudio.preload = "auto";
    menuMusicAudio.load();
    setMenuMusic(menuMusicAudio);
  }, [setHitSound, setSuccessSound, setMenuMusic, initAudioContext]);

  useEffect(() => {
    const isMenuPhase = ["menu", "character_selection", "map_selection", "local_setup", "arena_editor", "shop", "zoogipedia", "feature_hub"].includes(phase);
    const isVisualizerPhase = phase === "music_visualizer";
    
    if (menuMusic) {
      if (isMenuPhase && !isMuted) {
        menuMusic.loop = true;
        menuMusic.volume = 0.3;
        menuMusic.play().catch(err => console.log("Menu music play prevented:", err));
      } else if (!isMenuPhase && !isVisualizerPhase) {
        stopMenuMusic();
      }
    }
  }, [phase, menuMusic, isMuted, stopMenuMusic]);

  const showVideoBackground = ["local_setup", "arena_editor", "feature_hub"].includes(phase);
  const showVantaBackground = ["menu", "character_selection", "map_selection", "shop", "zoogipedia"].includes(phase);

  if (powerPreview) {
    return (
      <div style={{ width: "100%", height: "100%", position: "relative", overflow: "hidden" }}>
        <PowerPreview />
      </div>
    );
  }

  return (
    <div style={{ width: "100%", height: "100%", position: "relative", overflow: "hidden" }}>
      {showVantaBackground && (
        <VantaDotsBackground 
          color="#ff8820"
          color2="#ff8820"
          backgroundColor="#222222"
        />
      )}
      {showVideoBackground && <VideoBackground />}
      
      {phase === "menu" && <MainMenu />}
      
      {phase === "shop" && <Shop />}
      
      {phase === "zoogipedia" && <Zoogipedia />}
      
      {phase === "arena_editor" && <ArenaEditor />}
      
      {phase === "local_setup" && <LocalPlayerSetup />}
      
      {phase === "character_selection" && <CharacterSelection />}
      
      {phase === "feature_hub" && <FeatureHub />}
      
      {phase === "map_selection" && <MapSelection />}
      
      {phase === "playing" && <Game />}
      
      {phase === "round_end" && (
        <>
          <Game />
          <RoundEnd />
        </>
      )}
      
      {phase === "game_over" && (
        <>
          <Game />
          <GameOver />
        </>
      )}
      
      {phase === "music_visualizer" && <MusicVisualizer />}
    </div>
  );
}

export default App;
