import { useEffect, useCallback, useState } from "react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { useGLTF } from "@react-three/drei";
import * as THREE from "three";

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
import { RingerCreator } from "@/components/game/RingerCreator";
import { RingerTrials } from "@/components/game/RingerTrials";
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

function RingerTrialsLoading() {
  const setPhase = useZoogiGame((state) => state.setPhase);

  useEffect(() => {
    useGLTF.clear("/models/character/body.glb");
    useGLTF.clear("/models/character/body.glb?t=trials");
    useGLTF.clear("/models/platforms/underworld.glb");
    useGLTF.clear("/models/platforms/underworld.glb?t=trials");
    THREE.Cache.clear();

    const timer = setTimeout(() => {
      setPhase("ringer_trials");
    }, 1000);
    return () => clearTimeout(timer);
  }, [setPhase]);

  return (
    <div className="fixed inset-0 bg-black flex items-center justify-center">
      <div className="text-center">
        <div className="text-white text-2xl font-bold mb-4">Entering the Underworld...</div>
        <div className="w-16 h-16 border-4 border-purple-500 border-t-transparent rounded-full animate-spin mx-auto" />
      </div>
    </div>
  );
}

function App() {
  const powerPreview = usePowerPreview();
  const phase = useZoogiGame((state) => state.phase);
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
  const showVantaBackground = ["menu", "character_selection", "map_selection", "shop", "zoogipedia", "ringer_creator"].includes(phase);

  if (powerPreview) {
    return (
      <div style={{ width: "100vw", height: "100vh", position: "relative", overflow: "hidden" }}>
        <PowerPreview />
      </div>
    );
  }

  return (
    <div style={{ width: "100vw", height: "100vh", position: "relative", overflow: "hidden" }}>
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
      
      {phase === "ringer_creator" && <RingerCreator />}
      
      {phase === "ringer_trials_loading" && <RingerTrialsLoading />}
      
      {phase === "ringer_trials" && <RingerTrials />}
    </div>
  );
}

export default App;
