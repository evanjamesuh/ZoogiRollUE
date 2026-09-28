import { useState, useEffect, useRef, useMemo, useCallback } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { OrbitControls, Stars, Grid, Text } from "@react-three/drei";
import * as THREE from "three";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { Play, Pause, SkipBack, SkipForward, Home, Camera, Volume2, VolumeX, Music, List } from "lucide-react";

interface MusicTrack {
  id: string;
  title: string;
  artist: string;
  file: string;
}

const MUSIC_TRACKS: MusicTrack[] = [
  {
    id: "muted-trumpet",
    title: "Muted Trumpet",
    artist: "Evan James",
    file: "/sounds/evan_james_muted_trumpet.mp3",
  },
  {
    id: "sunday-funday",
    title: "Sunday Funday",
    artist: "Evan James",
    file: "/sounds/evan_james_sunday_funday.mp3",
  },
];

interface Note {
  id: string;
  track: number;
  startTime: number;
  duration: number;
  pitch: number;
  velocity: number;
}

interface Track {
  id: number;
  name: string;
  color: string;
  notes: Note[];
}

interface Song {
  id: string;
  title: string;
  artist: string;
  duration: number;
  tracks: Track[];
}

const TRACK_COLORS = [
  "#FF00FF",
  "#00FFFF",
  "#00FF00",
  "#FFFF00",
  "#FF6600",
  "#FF0066",
  "#6600FF",
  "#00FF66",
];

const CAMERA_PRESETS = [
  { name: "Perspective", position: [40, 25, 60], target: [0, 5, 0] },
  { name: "Top Down", position: [0, 80, 0], target: [0, 0, 0] },
  { name: "Side View", position: [80, 10, 0], target: [0, 5, 0] },
  { name: "Follow", position: [0, 15, 30], target: [0, 5, 0] },
];

function generateSampleSong(): Song {
  const tracks: Track[] = [];
  const duration = 225;
  
  for (let t = 0; t < 8; t++) {
    const notes: Note[] = [];
    const noteCount = 80 + Math.floor(Math.random() * 120);
    
    for (let n = 0; n < noteCount; n++) {
      const startTime = Math.random() * duration;
      const noteDuration = 0.1 + Math.random() * 2.0;
      const basePitch = 20 + t * 10;
      const pitch = basePitch + Math.floor(Math.random() * 24);
      
      notes.push({
        id: `t${t}-n${n}`,
        track: t,
        startTime,
        duration: noteDuration,
        pitch,
        velocity: 0.5 + Math.random() * 0.5,
      });
    }
    
    notes.sort((a, b) => a.startTime - b.startTime);
    
    const trackNames = ["Lead Synth", "Bass", "Drums", "Pad", "Arp", "FX", "Strings", "Vocals"];
    tracks.push({
      id: t,
      name: trackNames[t],
      color: TRACK_COLORS[t],
      notes,
    });
  }
  
  return {
    id: "main-track",
    title: "Zoogi Arena Theme",
    artist: "Zoogi Sound Team",
    duration,
    tracks,
  };
}

interface NoteBlockProps {
  note: Note;
  trackColor: string;
  currentTime: number;
  timeScale: number;
}

function NoteBlock({ note, trackColor, currentTime, timeScale }: NoteBlockProps) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  const z = (note.startTime - currentTime) * timeScale;
  const y = (note.pitch - 50) * 0.3;
  const x = (note.track - 3.5) * 6;
  
  const width = Math.max(0.3, note.duration * timeScale * 0.8);
  const height = 0.4;
  const depth = 0.4;
  
  const isActive = currentTime >= note.startTime && currentTime <= note.startTime + note.duration;
  const opacity = isActive ? 1.0 : 0.7;
  const emissiveIntensity = isActive ? 0.8 : 0.3;
  
  if (z < -20 || z > 200) return null;
  
  return (
    <mesh ref={meshRef} position={[x, y, -z]}>
      <boxGeometry args={[width, height, depth]} />
      <meshStandardMaterial
        color={trackColor}
        emissive={trackColor}
        emissiveIntensity={emissiveIntensity}
        transparent
        opacity={opacity}
      />
    </mesh>
  );
}

interface VisualizerSceneProps {
  song: Song;
  currentTime: number;
  isPlaying: boolean;
  cameraPreset: number;
}

function VisualizerScene({ song, currentTime, isPlaying, cameraPreset }: VisualizerSceneProps) {
  const { camera } = useThree();
  const controlsRef = useRef<any>(null);
  const timeScale = 8;
  
  useEffect(() => {
    const preset = CAMERA_PRESETS[cameraPreset];
    camera.position.set(preset.position[0], preset.position[1], preset.position[2]);
    if (controlsRef.current) {
      controlsRef.current.target.set(preset.target[0], preset.target[1], preset.target[2]);
      controlsRef.current.update();
    }
  }, [cameraPreset, camera]);
  
  useFrame(() => {
    if (cameraPreset === 3 && isPlaying) {
      const targetZ = -currentTime * timeScale + 30;
      camera.position.z = THREE.MathUtils.lerp(camera.position.z, targetZ, 0.02);
      if (controlsRef.current) {
        controlsRef.current.target.z = THREE.MathUtils.lerp(controlsRef.current.target.z, targetZ - 30, 0.02);
        controlsRef.current.update();
      }
    }
  });
  
  const visibleNotes = useMemo(() => {
    const notes: { note: Note; color: string }[] = [];
    const windowStart = currentTime - 3;
    const windowEnd = currentTime + 30;
    
    song.tracks.forEach((track) => {
      track.notes.forEach((note) => {
        if (note.startTime >= windowStart && note.startTime <= windowEnd) {
          notes.push({ note, color: track.color });
        }
      });
    });
    
    return notes;
  }, [song, Math.floor(currentTime * 2)]);
  
  return (
    <>
      <color attach="background" args={["#0a0a1a"]} />
      <fog attach="fog" args={["#0a0a1a", 50, 200]} />
      
      <ambientLight intensity={0.3} />
      <pointLight position={[0, 30, 0]} intensity={1} color="#ffffff" />
      <pointLight position={[50, 20, 50]} intensity={0.5} color="#ff00ff" />
      <pointLight position={[-50, 20, -50]} intensity={0.5} color="#00ffff" />
      
      <Stars radius={200} depth={100} count={2000} factor={4} saturation={0} />
      
      <Grid
        position={[0, -10, 0]}
        args={[400, 400]}
        cellSize={5}
        cellThickness={0.5}
        cellColor="#2a2a4a"
        sectionSize={20}
        sectionThickness={1}
        sectionColor="#3a3a5a"
        fadeDistance={200}
        fadeStrength={1}
        infiniteGrid
      />
      
      <mesh position={[0, -10.1, -currentTime * timeScale]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[60, 2]} />
        <meshBasicMaterial color="#ffffff" opacity={0.3} transparent />
      </mesh>
      
      {song.tracks.map((track, idx) => (
        <mesh key={track.id} position={[(idx - 3.5) * 6, -10, -currentTime * timeScale]}>
          <boxGeometry args={[0.2, 30, 0.2]} />
          <meshBasicMaterial color={track.color} opacity={0.6} transparent />
        </mesh>
      ))}
      
      {visibleNotes.map(({ note, color }) => (
        <NoteBlock
          key={note.id}
          note={note}
          trackColor={color}
          currentTime={currentTime}
          timeScale={timeScale}
        />
      ))}
      
      <OrbitControls
        ref={controlsRef}
        enablePan
        enableZoom
        enableRotate
        maxPolarAngle={Math.PI * 0.9}
        minDistance={10}
        maxDistance={150}
      />
    </>
  );
}

export function MusicVisualizer() {
  const setPhase = useZoogiGame((state) => state.setPhase);
  const { menuMusic } = useAudio();
  
  const [song] = useState<Song>(() => generateSampleSong());
  const [isPlaying, setIsPlaying] = useState(false);
  const [currentTime, setCurrentTime] = useState(0);
  const [audioDuration, setAudioDuration] = useState(180);
  const [cameraPreset, setCameraPreset] = useState(0);
  const [selectedTrackIndex, setSelectedTrackIndex] = useState(0);
  const [showTrackSelector, setShowTrackSelector] = useState(false);
  const [isMuted, setIsMuted] = useState(false);
  const animationRef = useRef<number>();
  const lastTimeRef = useRef<number>(0);
  const audioRef = useRef<HTMLAudioElement | null>(null);
  
  const selectedTrack = MUSIC_TRACKS[selectedTrackIndex];
  
  // Initialize audio element for selected track
  useEffect(() => {
    // Stop menu music when visualizer loads
    if (menuMusic) {
      menuMusic.pause();
    }
    
    // Stop and cleanup previous audio before creating new one
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.src = "";
      audioRef.current = null;
    }
    
    // Create audio for selected track
    const audio = new Audio(selectedTrack.file);
    audio.loop = true;
    audio.volume = 0.5;
    audio.preload = "auto";
    
    // Get actual audio duration when metadata loads
    audio.addEventListener("loadedmetadata", () => {
      if (audio.duration && !isNaN(audio.duration)) {
        setAudioDuration(audio.duration);
      }
    });
    
    audioRef.current = audio;
    
    setIsPlaying(false);
    setCurrentTime(0);
    
    return () => {
      if (audioRef.current) {
        audioRef.current.pause();
        audioRef.current.src = "";
        audioRef.current = null;
      }
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [selectedTrack.file, menuMusic]);
  
  // Handle play/pause and animation
  useEffect(() => {
    const audio = audioRef.current;
    
    if (isPlaying) {
      if (audio && audio.paused) {
        audio.play().catch((err) => {
          console.log("Audio play prevented:", err);
        });
      }
      
      const animate = (timestamp: number) => {
        if (lastTimeRef.current === 0) {
          lastTimeRef.current = timestamp;
        }
        
        const delta = (timestamp - lastTimeRef.current) / 1000;
        lastTimeRef.current = timestamp;
        
        setCurrentTime((prev) => {
          let next = prev + delta;
          if (next >= audioDuration) {
            next = 0;
            if (audio) {
              audio.currentTime = 0;
            }
          }
          return next;
        });
        
        animationRef.current = requestAnimationFrame(animate);
      };
      
      animationRef.current = requestAnimationFrame(animate);
    } else {
      if (audio && !audio.paused) {
        audio.pause();
      }
      lastTimeRef.current = 0;
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    }
    
    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [isPlaying, audioDuration]);
  
  const handlePlayPause = useCallback(() => {
    const newPlaying = !isPlaying;
    setIsPlaying(newPlaying);
    
    if (newPlaying && audioRef.current) {
      audioRef.current.muted = false;
      audioRef.current.volume = 0.5;
      audioRef.current.play().catch((err) => {
        console.log("Play attempt failed:", err);
      });
    }
  }, [isPlaying]);
  
  const handleTrackChange = useCallback((index: number) => {
    setSelectedTrackIndex(index);
    setShowTrackSelector(false);
    setIsPlaying(false);
    setCurrentTime(0);
  }, []);
  
  const toggleMute = useCallback(() => {
    setIsMuted((prev) => {
      const newMuted = !prev;
      if (audioRef.current) {
        audioRef.current.muted = newMuted;
      }
      return newMuted;
    });
  }, []);
  
  const handleSeek = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const newTime = parseFloat(e.target.value);
    setCurrentTime(newTime);
    if (audioRef.current) {
      audioRef.current.currentTime = newTime;
    }
  }, []);
  
  const handleBack = useCallback(() => {
    const newTime = Math.max(0, currentTime - 10);
    setCurrentTime(newTime);
    if (audioRef.current) {
      audioRef.current.currentTime = newTime;
    }
  }, [currentTime]);
  
  const handleForward = useCallback(() => {
    const newTime = Math.min(audioDuration, currentTime + 10);
    setCurrentTime(newTime);
    if (audioRef.current) {
      audioRef.current.currentTime = newTime;
    }
  }, [currentTime, audioDuration]);
  
  const handleRestart = useCallback(() => {
    setCurrentTime(0);
    if (audioRef.current) {
      audioRef.current.currentTime = 0;
    }
  }, []);
  
  const formatTime = (time: number) => {
    if (!isFinite(time) || isNaN(time)) return "0:00";
    const minutes = Math.floor(time / 60);
    const seconds = Math.floor(time % 60);
    return `${minutes}:${seconds.toString().padStart(2, "0")}`;
  };
  
  const handleClose = useCallback(() => {
    setIsPlaying(false);
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.currentTime = 0;
    }
    setPhase("menu");
  }, [setPhase]);
  
  return (
    <div className="fixed inset-0 z-50 bg-[#0a0a1a]">
      <Canvas
        camera={{ position: [40, 25, 60], fov: 60 }}
        gl={{ antialias: true }}
      >
        <VisualizerScene
          song={song}
          currentTime={currentTime}
          isPlaying={isPlaying}
          cameraPreset={cameraPreset}
        />
      </Canvas>
      
      <div className="absolute top-4 left-4 flex gap-2">
        <button
          onClick={handleClose}
          className="p-3 rounded-full bg-black/50 hover:bg-black/70 text-white transition-all"
        >
          <Home size={24} />
        </button>
      </div>
      
      <div className="absolute top-4 right-4 flex gap-2">
        {CAMERA_PRESETS.map((preset, idx) => (
          <button
            key={preset.name}
            onClick={() => setCameraPreset(idx)}
            className={`px-4 py-2 rounded-lg font-medium transition-all ${
              cameraPreset === idx
                ? "bg-purple-500 text-white"
                : "bg-black/50 text-white/70 hover:bg-black/70 hover:text-white"
            }`}
          >
            {preset.name}
          </button>
        ))}
      </div>
      
      <div className="absolute bottom-0 left-0 right-0 p-6 bg-gradient-to-t from-black/80 to-transparent">
        <div className="max-w-4xl mx-auto">
          <div className="flex items-center gap-4 mb-4">
            <div className="w-16 h-16 rounded-lg bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center">
              <Music size={28} className="text-white" />
            </div>
            <div className="flex-1">
              <h2 className="text-white text-xl font-bold">{selectedTrack.title}</h2>
              <p className="text-white/60">{selectedTrack.artist}</p>
            </div>
            <div className="relative">
              <button
                onClick={() => setShowTrackSelector(!showTrackSelector)}
                className="p-2 rounded-full bg-white/10 hover:bg-white/20 text-white mr-2"
                title="Select Track"
              >
                <List size={20} />
              </button>
              {showTrackSelector && (
                <div className="absolute bottom-full right-0 mb-2 bg-black/90 rounded-lg p-2 min-w-[200px] backdrop-blur-sm border border-white/20">
                  {MUSIC_TRACKS.map((track, idx) => (
                    <button
                      key={track.id}
                      onClick={() => handleTrackChange(idx)}
                      className={`w-full text-left px-3 py-2 rounded-lg transition-all ${
                        selectedTrackIndex === idx
                          ? "bg-purple-500 text-white"
                          : "text-white/70 hover:bg-white/10 hover:text-white"
                      }`}
                    >
                      <div className="font-medium">{track.title}</div>
                      <div className="text-xs opacity-70">{track.artist}</div>
                    </button>
                  ))}
                </div>
              )}
            </div>
            <button
              onClick={toggleMute}
              className="p-2 rounded-full bg-white/10 hover:bg-white/20 text-white"
            >
              {isMuted ? <VolumeX size={20} /> : <Volume2 size={20} />}
            </button>
          </div>
          
          <div className="flex items-center gap-4">
            <span className="text-white/60 text-sm w-12 text-right">
              {formatTime(currentTime)}
            </span>
            <input
              type="range"
              min={0}
              max={isFinite(audioDuration) && audioDuration > 0 ? audioDuration : 180}
              step={0.1}
              value={isFinite(currentTime) ? currentTime : 0}
              onChange={handleSeek}
              className="flex-1 h-1 bg-white/20 rounded-full appearance-none cursor-pointer [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-white"
            />
            <span className="text-white/60 text-sm w-12">
              {isFinite(audioDuration) ? formatTime(audioDuration) : "--:--"}
            </span>
          </div>
          
          <div className="flex items-center justify-center gap-4 mt-4">
            <button
              onClick={handleBack}
              className="p-3 rounded-full bg-white/10 hover:bg-white/20 text-white transition-all"
            >
              <SkipBack size={24} />
            </button>
            <button
              onClick={handlePlayPause}
              className="p-4 rounded-full bg-gradient-to-r from-purple-500 to-pink-500 hover:from-purple-400 hover:to-pink-400 text-white transition-all"
            >
              {isPlaying ? <Pause size={32} /> : <Play size={32} className="ml-1" />}
            </button>
            <button
              onClick={handleForward}
              className="p-3 rounded-full bg-white/10 hover:bg-white/20 text-white transition-all"
            >
              <SkipForward size={24} />
            </button>
          </div>
          
          <div className="flex justify-center gap-2 mt-4">
            {song.tracks.map((track) => (
              <div
                key={track.id}
                className="flex items-center gap-1 px-2 py-1 rounded bg-black/30"
              >
                <div
                  className="w-3 h-3 rounded-full"
                  style={{ backgroundColor: track.color }}
                />
                <span className="text-white/70 text-xs">{track.name}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
