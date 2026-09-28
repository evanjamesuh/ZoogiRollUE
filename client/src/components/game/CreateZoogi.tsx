import { useState, useRef, useCallback, useEffect, Suspense, Component, ReactNode, useMemo } from "react";
import { Canvas } from "@react-three/fiber";
import { OrbitControls, useGLTF, Environment, Center, PerspectiveCamera } from "@react-three/drei";
import { ChevronLeft, Upload, Camera, Loader2, CheckCircle, AlertCircle, Sparkles, Wand2, X, Download } from "lucide-react";
import * as THREE from "three";
import type { CustomZoogiData } from "@/lib/stores/useZoogiGame";
import { getDeviceId } from "@/lib/deviceId";
import { CircleCropTool } from "@/components/ui/CircleCropTool";
import { VideoBackground } from "./VideoBackground";

interface CreateZoogiProps {
  onBack: () => void;
  onZoogiCreated?: (zoogi: CustomZoogiData) => void;
}

export type CustomZoogi = CustomZoogiData;

type GenerationStatus = "idle" | "uploading" | "processing" | "succeeded" | "failed";

interface TaskStatus {
  id: string;
  status: "PENDING" | "IN_PROGRESS" | "SUCCEEDED" | "FAILED" | "EXPIRED";
  progress: number;
  model_urls?: {
    glb?: string;
  };
  thumbnail_url?: string;
  error?: {
    message: string;
  };
}

interface ErrorBoundaryProps {
  children: ReactNode;
  fallback: ReactNode;
}

interface ErrorBoundaryState {
  hasError: boolean;
}

class ModelErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  constructor(props: ErrorBoundaryProps) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError(): ErrorBoundaryState {
    return { hasError: true };
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo) {
    console.error("Model loading error:", error, errorInfo);
  }

  render() {
    if (this.state.hasError) {
      return this.props.fallback;
    }
    return this.props.children;
  }
}

function ModelPreviewInner({ modelUrl }: { modelUrl: string }) {
  console.log("ModelPreviewInner loading:", modelUrl);
  const { scene } = useGLTF(modelUrl);
  
  useEffect(() => {
    console.log("Model scene loaded successfully");
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
  }, [scene]);

  return (
    <Center>
      <primitive object={scene} scale={2.5} />
    </Center>
  );
}

function ModelLoadingFallback() {
  return (
    <mesh>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="#8B5CF6" wireframe />
    </mesh>
  );
}

function ModelErrorFallback() {
  return (
    <mesh>
      <boxGeometry args={[1.5, 1.5, 1.5]} />
      <meshStandardMaterial color="#EF4444" opacity={0.7} transparent />
    </mesh>
  );
}

function ModelPreview({ modelUrl }: { modelUrl: string }) {
  return (
    <ModelErrorBoundary fallback={<ModelErrorFallback />}>
      <Suspense fallback={<ModelLoadingFallback />}>
        <ModelPreviewInner modelUrl={modelUrl} />
      </Suspense>
    </ModelErrorBoundary>
  );
}

function PlaceholderModel() {
  return (
    <mesh>
      <sphereGeometry args={[1, 32, 32]} />
      <meshStandardMaterial color="#8B5CF6" opacity={0.5} transparent />
    </mesh>
  );
}

export function CreateZoogi({ onBack, onZoogiCreated }: CreateZoogiProps) {
  const [imagePreview, setImagePreview] = useState<string | null>(null);
  const [imageBase64, setImageBase64] = useState<string | null>(null);
  const [showCropTool, setShowCropTool] = useState(false);
  const [rawImageSrc, setRawImageSrc] = useState<string | null>(null);
  const [status, setStatus] = useState<GenerationStatus>("idle");
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [taskId, setTaskId] = useState<string | null>(null);
  const [modelUrl, setModelUrl] = useState<string | null>(null);
  const [thumbnailUrl, setThumbnailUrl] = useState<string | null>(null);
  const [zoogiName, setZoogiName] = useState("");
  const [customStats, setCustomStats] = useState({
    speed: 5,
    power: 5,
    control: 5,
    ability: 5,
  });
  
  const fileInputRef = useRef<HTMLInputElement>(null);
  const pollIntervalRef = useRef<NodeJS.Timeout | null>(null);

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    if (!file.type.startsWith("image/")) {
      setError("Please select an image file");
      return;
    }

    if (file.size > 10 * 1024 * 1024) {
      setError("Image must be less than 10MB");
      return;
    }

    const reader = new FileReader();
    reader.onload = (e) => {
      const result = e.target?.result as string;
      setRawImageSrc(result);
      setShowCropTool(true);
      setError(null);
    };
    reader.readAsDataURL(file);
  };

  const handleCropComplete = (croppedImageDataUrl: string) => {
    setImagePreview(croppedImageDataUrl);
    const base64Data = croppedImageDataUrl.split(",")[1];
    setImageBase64(base64Data);
    setShowCropTool(false);
    setRawImageSrc(null);
  };

  const handleCropCancel = () => {
    setShowCropTool(false);
    setRawImageSrc(null);
  };

  const pollErrorCountRef = useRef(0);
  const MAX_POLL_ERRORS = 5;

  const pollTaskStatus = useCallback(async (id: string) => {
    try {
      const response = await fetch(`/api/meshy/task/${id}`);
      if (!response.ok) {
        throw new Error("Failed to fetch task status");
      }
      
      const task: TaskStatus = await response.json();
      setProgress(task.progress);
      pollErrorCountRef.current = 0;

      if (task.status === "SUCCEEDED") {
        if (pollIntervalRef.current) {
          clearInterval(pollIntervalRef.current);
          pollIntervalRef.current = null;
        }
        setStatus("succeeded");
        if (task.model_urls?.glb) {
          const proxyUrl = `/api/meshy/download/${id}`;
          console.log("Setting model URL (proxy):", proxyUrl);
          setModelUrl(proxyUrl);
        } else {
          console.warn("Task succeeded but no GLB URL found:", task);
        }
        if (task.thumbnail_url) {
          setThumbnailUrl(task.thumbnail_url);
        }
      } else if (task.status === "FAILED" || task.status === "EXPIRED") {
        if (pollIntervalRef.current) {
          clearInterval(pollIntervalRef.current);
          pollIntervalRef.current = null;
        }
        setStatus("failed");
        const errorMessage = task.error?.message || "3D generation failed";
        if (errorMessage.toLowerCase().includes("server is busy") || errorMessage.toLowerCase().includes("server busy")) {
          setError("The 3D generation service is currently busy. Please wait a moment and try again.");
        } else {
          setError(errorMessage);
        }
      }
    } catch (err: any) {
      console.error("Error polling task:", err);
      pollErrorCountRef.current++;
      
      if (pollErrorCountRef.current >= MAX_POLL_ERRORS) {
        if (pollIntervalRef.current) {
          clearInterval(pollIntervalRef.current);
          pollIntervalRef.current = null;
        }
        setStatus("failed");
        setError("Connection lost. Please try again.");
      }
    }
  }, []);

  const startGeneration = async () => {
    if (!imagePreview) {
      setError("Please upload an image first");
      return;
    }

    setStatus("uploading");
    setError(null);
    setProgress(0);

    try {
      const response = await fetch("/api/meshy/image-to-3d", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          imageDataUrl: imagePreview,
          surfaceMode: "organic",
        }),
      });

      if (!response.ok) {
        const errorData = await response.json();
        throw new Error(errorData.error || "Failed to start generation");
      }

      const data = await response.json();
      setTaskId(data.taskId);
      setStatus("processing");

      pollIntervalRef.current = setInterval(() => {
        pollTaskStatus(data.taskId);
      }, 3000);
    } catch (err: any) {
      setStatus("failed");
      setError(err.message || "Failed to start 3D generation");
    }
  };

  const [isSaving, setIsSaving] = useState(false);
  const deviceId = useMemo(() => getDeviceId(), []);

  const saveCustomZoogi = async () => {
    if (!taskId || !zoogiName.trim() || status !== "succeeded" || !modelUrl) {
      setError("Please enter a name for your Zoogi");
      return;
    }

    setIsSaving(true);
    setError(null);

    try {
      const response = await fetch("/api/custom-zoogis", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          deviceId,
          name: zoogiName.trim(),
          meshyTaskId: taskId,
          thumbnailUrl: thumbnailUrl || null,
          stats: customStats,
        }),
      });

      if (!response.ok) {
        const errorData = await response.json();
        throw new Error(errorData.error || "Failed to save Zoogi");
      }

      const savedZoogi = await response.json();
      
      const customZoogi: CustomZoogi = {
        id: `db_${savedZoogi.id}`,
        name: savedZoogi.name,
        modelUrl: `/api/meshy/download/${savedZoogi.meshyTaskId}`,
        thumbnailUrl: savedZoogi.thumbnailUrl || undefined,
        stats: savedZoogi.stats as typeof customStats,
        createdAt: new Date(savedZoogi.createdAt).getTime(),
      };

      onZoogiCreated?.(customZoogi);
      onBack();
    } catch (err: any) {
      setError(err.message || "Failed to save Zoogi");
    } finally {
      setIsSaving(false);
    }
  };

  const [isDownloading, setIsDownloading] = useState(false);

  const downloadModel = async () => {
    if (!taskId || !modelUrl) return;
    
    setIsDownloading(true);
    try {
      const response = await fetch(`/api/meshy/download/${taskId}`);
      if (!response.ok) {
        throw new Error("Failed to download model");
      }
      
      const blob = await response.blob();
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${zoogiName.trim() || "custom-zoogi"}.glb`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err: any) {
      setError(err.message || "Failed to download model");
    } finally {
      setIsDownloading(false);
    }
  };

  const reset = () => {
    if (pollIntervalRef.current) {
      clearInterval(pollIntervalRef.current);
      pollIntervalRef.current = null;
    }
    pollErrorCountRef.current = 0;
    setImagePreview(null);
    setImageBase64(null);
    setStatus("idle");
    setProgress(0);
    setError(null);
    setTaskId(null);
    setModelUrl(null);
    setThumbnailUrl(null);
    setZoogiName("");
    setCustomStats({ speed: 5, power: 5, control: 5, ability: 5 });
  };

  useEffect(() => {
    return () => {
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current);
      }
    };
  }, []);

  const renderStatusIcon = () => {
    switch (status) {
      case "uploading":
      case "processing":
        return <Loader2 className="w-6 h-6 animate-spin text-purple-400" />;
      case "succeeded":
        return <CheckCircle className="w-6 h-6 text-green-400" />;
      case "failed":
        return <AlertCircle className="w-6 h-6 text-red-400" />;
      default:
        return <Sparkles className="w-6 h-6 text-purple-400" />;
    }
  };

  const getStatusText = () => {
    switch (status) {
      case "uploading":
        return "Uploading image...";
      case "processing":
        return `Generating 3D model... ${progress}%`;
      case "succeeded":
        return "Your Zoogi is ready!";
      case "failed":
        return error || "Generation failed";
      default:
        return "Upload an image to create your Zoogi";
    }
  };

  return (
    <div className="fixed inset-0 z-50 overflow-hidden">
      <VideoBackground />
      
      <div className="relative z-10 w-full h-full flex flex-col">
        <div className="flex items-center justify-between p-4 border-b border-white/10">
          <button
            onClick={onBack}
            className="flex items-center gap-2 text-white/80 hover:text-white transition-colors"
          >
            <ChevronLeft className="w-6 h-6" />
            <span className="font-semibold">Back</span>
          </button>
          <h1 className="text-xl font-bold text-white flex items-center gap-2">
            <Wand2 className="w-5 h-5 text-purple-400" />
            Create Your Zoogi
          </h1>
          <div className="w-20" />
        </div>

        <div className="flex-1 flex flex-col lg:flex-row gap-4 p-4 overflow-auto">
          <div className="flex-1 flex flex-col gap-4">
            <div className="bg-black/40 backdrop-blur-sm rounded-2xl p-4 border border-white/10">
              <div className="flex items-center gap-2 mb-3">
                {renderStatusIcon()}
                <span className="text-white font-medium">{getStatusText()}</span>
              </div>

              {status === "processing" && (
                <div className="w-full h-2 bg-white/10 rounded-full overflow-hidden">
                  <div
                    className="h-full bg-gradient-to-r from-purple-500 to-pink-500 transition-all duration-500"
                    style={{ width: `${progress}%` }}
                  />
                </div>
              )}

              {error && status === "failed" && (
                <p className="text-red-400 text-sm mt-2">{error}</p>
              )}
            </div>

            <div className="bg-black/40 backdrop-blur-sm rounded-2xl p-4 border border-white/10 flex-1">
              <h3 className="text-white font-semibold mb-3">Reference Image</h3>
              
              {imagePreview ? (
                <div className="relative">
                  <img
                    src={imagePreview}
                    alt="Reference"
                    className="w-full max-h-64 object-contain rounded-xl"
                  />
                  {status === "idle" && (
                    <button
                      onClick={reset}
                      className="absolute top-2 right-2 p-2 bg-black/50 rounded-full hover:bg-black/70 transition-colors"
                    >
                      <X className="w-4 h-4 text-white" />
                    </button>
                  )}
                </div>
              ) : (
                <div
                  onClick={() => fileInputRef.current?.click()}
                  className="border-2 border-dashed border-white/20 rounded-xl p-8 flex flex-col items-center justify-center cursor-pointer hover:border-purple-500/50 transition-colors"
                >
                  <Upload className="w-12 h-12 text-white/40 mb-3" />
                  <p className="text-white/60 text-center">
                    Tap to upload an image of your Zoogi design
                  </p>
                  <p className="text-white/40 text-sm mt-2">PNG, JPG up to 10MB</p>
                </div>
              )}

              <input
                ref={fileInputRef}
                type="file"
                accept="image/*"
                onChange={handleFileSelect}
                className="hidden"
              />
            </div>

            {imagePreview && status === "idle" && (
              <button
                onClick={startGeneration}
                className="w-full py-4 bg-gradient-to-r from-purple-600 to-pink-600 rounded-xl text-white font-bold text-lg hover:from-purple-500 hover:to-pink-500 transition-all active:scale-[0.98] flex items-center justify-center gap-2"
              >
                <Wand2 className="w-5 h-5" />
                Generate 3D Model
              </button>
            )}
          </div>

          <div className="flex-1 flex flex-col gap-4">
            <div className="bg-black/40 backdrop-blur-sm rounded-2xl border border-white/10 flex-1 min-h-[300px]">
              <Canvas>
                <PerspectiveCamera makeDefault position={[0, 1, 4]} />
                <ambientLight intensity={0.5} />
                <directionalLight position={[5, 5, 5]} intensity={1} />
                <Environment preset="sunset" />
                {modelUrl ? (
                  <ModelPreview modelUrl={modelUrl} />
                ) : (
                  <PlaceholderModel />
                )}
                <OrbitControls
                  enablePan={false}
                  minDistance={2}
                  maxDistance={8}
                  autoRotate={status === "succeeded"}
                  autoRotateSpeed={2}
                />
              </Canvas>
            </div>

            {status === "succeeded" && (
              <div className="bg-black/40 backdrop-blur-sm rounded-2xl p-4 border border-white/10">
                <h3 className="text-white font-semibold mb-3">Customize Your Zoogi</h3>
                
                <div className="mb-4">
                  <label className="text-white/70 text-sm mb-1 block">Name</label>
                  <input
                    type="text"
                    value={zoogiName}
                    onChange={(e) => setZoogiName(e.target.value)}
                    placeholder="Enter a name..."
                    className="w-full px-4 py-2 bg-white/10 border border-white/20 rounded-lg text-white placeholder:text-white/40 focus:outline-none focus:border-purple-500"
                    maxLength={20}
                  />
                </div>

                <div className="grid grid-cols-2 gap-3 mb-4">
                  {(["speed", "power", "control", "ability"] as const).map((stat) => (
                    <div key={stat}>
                      <label className="text-white/70 text-sm capitalize block mb-1">
                        {stat}: {customStats[stat]}
                      </label>
                      <input
                        type="range"
                        min={1}
                        max={10}
                        value={customStats[stat]}
                        onChange={(e) =>
                          setCustomStats((prev) => ({
                            ...prev,
                            [stat]: parseInt(e.target.value),
                          }))
                        }
                        className="w-full h-2 bg-white/10 rounded-lg appearance-none cursor-pointer accent-purple-500"
                      />
                    </div>
                  ))}
                </div>

                <div className="flex gap-2">
                  <button
                    onClick={saveCustomZoogi}
                    disabled={isSaving}
                    className="flex-1 py-3 bg-gradient-to-r from-green-600 to-emerald-600 rounded-xl text-white font-bold hover:from-green-500 hover:to-emerald-500 transition-all active:scale-[0.98] flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                  >
                    {isSaving ? (
                      <>
                        <Loader2 className="w-5 h-5 animate-spin" />
                        Saving...
                      </>
                    ) : (
                      <>
                        <CheckCircle className="w-5 h-5" />
                        Save & Play
                      </>
                    )}
                  </button>
                  <button
                    onClick={downloadModel}
                    disabled={isDownloading}
                    className="py-3 px-4 bg-gradient-to-r from-blue-600 to-cyan-600 rounded-xl text-white font-bold hover:from-blue-500 hover:to-cyan-500 transition-all active:scale-[0.98] flex items-center justify-center gap-2 disabled:opacity-50 disabled:cursor-not-allowed"
                    title="Export GLB model"
                  >
                    {isDownloading ? (
                      <Loader2 className="w-5 h-5 animate-spin" />
                    ) : (
                      <Download className="w-5 h-5" />
                    )}
                  </button>
                </div>
              </div>
            )}

            {status === "failed" && (
              <button
                onClick={reset}
                className="w-full py-3 bg-white/10 border border-white/20 rounded-xl text-white font-medium hover:bg-white/20 transition-all"
              >
                Try Again
              </button>
            )}
          </div>
        </div>
      </div>

      {showCropTool && rawImageSrc && (
        <CircleCropTool
          imageSrc={rawImageSrc}
          onCropComplete={handleCropComplete}
          onCancel={handleCropCancel}
          minCircleSize={100}
          maxCircleSize={350}
          defaultCircleSize={200}
        />
      )}
    </div>
  );
}
