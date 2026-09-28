import { useState, useRef, useEffect, useCallback } from "react";
import { ZoomIn, ZoomOut, Move, Check, X, RotateCcw } from "lucide-react";

interface CircleCropToolProps {
  imageSrc: string;
  onCropComplete: (croppedImageDataUrl: string) => void;
  onCancel: () => void;
  minCircleSize?: number;
  maxCircleSize?: number;
  defaultCircleSize?: number;
}

export function CircleCropTool({
  imageSrc,
  onCropComplete,
  onCancel,
  minCircleSize = 100,
  maxCircleSize = 400,
  defaultCircleSize = 200,
}: CircleCropToolProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const imageRef = useRef<HTMLImageElement | null>(null);
  
  const [circleSize, setCircleSize] = useState(defaultCircleSize);
  const [imageOffset, setImageOffset] = useState({ x: 0, y: 0 });
  const [imageScale, setImageScale] = useState(1);
  const [isDragging, setIsDragging] = useState(false);
  const [isResizing, setIsResizing] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });
  const [imageLoaded, setImageLoaded] = useState(false);
  const [naturalSize, setNaturalSize] = useState({ width: 0, height: 0 });

  useEffect(() => {
    const img = new Image();
    img.crossOrigin = "anonymous";
    img.onload = () => {
      imageRef.current = img;
      setNaturalSize({ width: img.naturalWidth, height: img.naturalHeight });
      
      const containerWidth = containerRef.current?.clientWidth || 300;
      const containerHeight = containerRef.current?.clientHeight || 300;
      const scaleToFit = Math.min(
        containerWidth / img.naturalWidth,
        containerHeight / img.naturalHeight
      ) * 0.8;
      setImageScale(scaleToFit);
      setImageLoaded(true);
    };
    img.src = imageSrc;
  }, [imageSrc]);

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    const rect = containerRef.current?.getBoundingClientRect();
    if (!rect) return;

    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const centerX = rect.width / 2;
    const centerY = rect.height / 2;
    const radius = circleSize / 2;
    
    const distFromEdge = Math.abs(Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2)) - radius);
    
    if (distFromEdge < 20) {
      setIsResizing(true);
      setDragStart({ x: e.clientX, y: e.clientY });
    } else {
      setIsDragging(true);
      setDragStart({ x: e.clientX - imageOffset.x, y: e.clientY - imageOffset.y });
    }
  }, [circleSize, imageOffset]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (isResizing) {
      const rect = containerRef.current?.getBoundingClientRect();
      if (!rect) return;
      
      const centerX = rect.width / 2;
      const centerY = rect.height / 2;
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      
      const newRadius = Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2));
      const newSize = Math.max(minCircleSize, Math.min(maxCircleSize, newRadius * 2));
      setCircleSize(newSize);
    } else if (isDragging) {
      setImageOffset({
        x: e.clientX - dragStart.x,
        y: e.clientY - dragStart.y,
      });
    }
  }, [isDragging, isResizing, dragStart, minCircleSize, maxCircleSize]);

  const handleMouseUp = useCallback(() => {
    setIsDragging(false);
    setIsResizing(false);
  }, []);

  const handleTouchStart = useCallback((e: React.TouchEvent) => {
    if (e.touches.length === 1) {
      const touch = e.touches[0];
      const rect = containerRef.current?.getBoundingClientRect();
      if (!rect) return;

      const x = touch.clientX - rect.left;
      const y = touch.clientY - rect.top;
      const centerX = rect.width / 2;
      const centerY = rect.height / 2;
      const radius = circleSize / 2;
      
      const distFromEdge = Math.abs(Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2)) - radius);
      
      if (distFromEdge < 30) {
        setIsResizing(true);
        setDragStart({ x: touch.clientX, y: touch.clientY });
      } else {
        setIsDragging(true);
        setDragStart({ x: touch.clientX - imageOffset.x, y: touch.clientY - imageOffset.y });
      }
    }
  }, [circleSize, imageOffset]);

  const handleTouchMove = useCallback((e: React.TouchEvent) => {
    if (e.touches.length === 1) {
      const touch = e.touches[0];
      
      if (isResizing) {
        const rect = containerRef.current?.getBoundingClientRect();
        if (!rect) return;
        
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const x = touch.clientX - rect.left;
        const y = touch.clientY - rect.top;
        
        const newRadius = Math.sqrt(Math.pow(x - centerX, 2) + Math.pow(y - centerY, 2));
        const newSize = Math.max(minCircleSize, Math.min(maxCircleSize, newRadius * 2));
        setCircleSize(newSize);
      } else if (isDragging) {
        setImageOffset({
          x: touch.clientX - dragStart.x,
          y: touch.clientY - dragStart.y,
        });
      }
    }
  }, [isDragging, isResizing, dragStart, minCircleSize, maxCircleSize]);

  const handleTouchEnd = useCallback(() => {
    setIsDragging(false);
    setIsResizing(false);
  }, []);

  const handleZoom = (delta: number) => {
    setImageScale(prev => Math.max(0.1, Math.min(3, prev + delta)));
  };

  const handleReset = () => {
    setImageOffset({ x: 0, y: 0 });
    setCircleSize(defaultCircleSize);
    if (imageRef.current && containerRef.current) {
      const containerWidth = containerRef.current.clientWidth;
      const containerHeight = containerRef.current.clientHeight;
      const scaleToFit = Math.min(
        containerWidth / imageRef.current.naturalWidth,
        containerHeight / imageRef.current.naturalHeight
      ) * 0.8;
      setImageScale(scaleToFit);
    }
  };

  const handleCrop = () => {
    if (!imageRef.current || !containerRef.current) return;

    const canvas = document.createElement("canvas");
    const outputSize = 512;
    canvas.width = outputSize;
    canvas.height = outputSize;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    ctx.beginPath();
    ctx.arc(outputSize / 2, outputSize / 2, outputSize / 2, 0, Math.PI * 2);
    ctx.closePath();
    ctx.clip();

    const containerWidth = containerRef.current.clientWidth;
    const containerHeight = containerRef.current.clientHeight;
    const centerX = containerWidth / 2;
    const centerY = containerHeight / 2;

    const scaledWidth = naturalSize.width * imageScale;
    const scaledHeight = naturalSize.height * imageScale;
    const imageX = centerX - scaledWidth / 2 + imageOffset.x;
    const imageY = centerY - scaledHeight / 2 + imageOffset.y;

    const cropRadius = circleSize / 2;
    const cropLeft = centerX - cropRadius;
    const cropTop = centerY - cropRadius;

    const srcX = (cropLeft - imageX) / imageScale;
    const srcY = (cropTop - imageY) / imageScale;
    const srcSize = circleSize / imageScale;

    ctx.drawImage(
      imageRef.current,
      srcX, srcY, srcSize, srcSize,
      0, 0, outputSize, outputSize
    );

    const dataUrl = canvas.toDataURL("image/png");
    onCropComplete(dataUrl);
  };

  return (
    <div className="fixed inset-0 bg-black/90 z-50 flex flex-col">
      <div className="flex items-center justify-between p-4 border-b border-white/10">
        <button
          onClick={onCancel}
          className="flex items-center gap-2 text-white/70 hover:text-white transition-colors"
        >
          <X size={24} />
          <span>Cancel</span>
        </button>
        
        <h2 className="text-lg font-semibold text-white">Crop to Circle</h2>
        
        <button
          onClick={handleCrop}
          disabled={!imageLoaded}
          className="flex items-center gap-2 px-4 py-2 bg-green-500 hover:bg-green-600 rounded-lg text-white font-semibold transition-colors disabled:opacity-50"
        >
          <Check size={20} />
          <span>Apply</span>
        </button>
      </div>

      <div className="flex-1 flex items-center justify-center p-4 overflow-hidden">
        <div
          ref={containerRef}
          className="relative w-full max-w-md aspect-square bg-black/50 rounded-2xl overflow-hidden cursor-move select-none"
          onMouseDown={handleMouseDown}
          onMouseMove={handleMouseMove}
          onMouseUp={handleMouseUp}
          onMouseLeave={handleMouseUp}
          onTouchStart={handleTouchStart}
          onTouchMove={handleTouchMove}
          onTouchEnd={handleTouchEnd}
        >
          {imageLoaded && imageRef.current && (
            <img
              src={imageSrc}
              alt="Crop preview"
              className="absolute pointer-events-none"
              style={{
                width: naturalSize.width * imageScale,
                height: naturalSize.height * imageScale,
                left: `calc(50% - ${(naturalSize.width * imageScale) / 2}px + ${imageOffset.x}px)`,
                top: `calc(50% - ${(naturalSize.height * imageScale) / 2}px + ${imageOffset.y}px)`,
              }}
              draggable={false}
            />
          )}
          
          <div 
            className="absolute pointer-events-none"
            style={{
              left: `calc(50% - ${circleSize / 2}px)`,
              top: `calc(50% - ${circleSize / 2}px)`,
              width: circleSize,
              height: circleSize,
            }}
          >
            <div 
              className="absolute inset-0 rounded-full border-4 border-white shadow-[0_0_0_9999px_rgba(0,0,0,0.7)]"
              style={{
                boxShadow: `0 0 0 9999px rgba(0,0,0,0.7), inset 0 0 0 2px rgba(255,255,255,0.3)`,
              }}
            />
            
            <div className="absolute -top-3 left-1/2 -translate-x-1/2 w-6 h-6 bg-white rounded-full border-2 border-gray-400 cursor-ns-resize pointer-events-auto" />
            <div className="absolute -bottom-3 left-1/2 -translate-x-1/2 w-6 h-6 bg-white rounded-full border-2 border-gray-400 cursor-ns-resize pointer-events-auto" />
            <div className="absolute top-1/2 -left-3 -translate-y-1/2 w-6 h-6 bg-white rounded-full border-2 border-gray-400 cursor-ew-resize pointer-events-auto" />
            <div className="absolute top-1/2 -right-3 -translate-y-1/2 w-6 h-6 bg-white rounded-full border-2 border-gray-400 cursor-ew-resize pointer-events-auto" />
          </div>
        </div>
      </div>

      <div className="p-4 border-t border-white/10">
        <div className="flex items-center justify-center gap-4 mb-4">
          <button
            onClick={() => handleZoom(-0.1)}
            className="p-3 bg-white/10 hover:bg-white/20 rounded-full transition-colors"
          >
            <ZoomOut size={24} className="text-white" />
          </button>
          
          <div className="flex items-center gap-2 px-4 py-2 bg-white/10 rounded-full">
            <Move size={18} className="text-white/70" />
            <span className="text-white/70 text-sm">Drag to position</span>
          </div>
          
          <button
            onClick={() => handleZoom(0.1)}
            className="p-3 bg-white/10 hover:bg-white/20 rounded-full transition-colors"
          >
            <ZoomIn size={24} className="text-white" />
          </button>
          
          <button
            onClick={handleReset}
            className="p-3 bg-white/10 hover:bg-white/20 rounded-full transition-colors"
          >
            <RotateCcw size={24} className="text-white" />
          </button>
        </div>
        
        <div className="flex flex-col items-center gap-2">
          <label className="text-white/70 text-sm">Circle Size: {Math.round(circleSize)}px</label>
          <input
            type="range"
            min={minCircleSize}
            max={maxCircleSize}
            value={circleSize}
            onChange={(e) => setCircleSize(Number(e.target.value))}
            className="w-64 accent-purple-500"
          />
        </div>
        
        <p className="text-center text-white/50 text-xs mt-4">
          Drag the image to position it. Drag the circle edge or use the slider to resize.
        </p>
      </div>
    </div>
  );
}
