import { useEffect, useRef } from "react";

export function VideoBackground() {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    const video = videoRef.current;
    if (!video) return;

    // Ensure video always plays
    const ensurePlaying = () => {
      if (video.paused) {
        video.play().catch(() => {});
      }
    };

    // Handle visibility changes - resume when tab becomes visible
    const handleVisibilityChange = () => {
      if (!document.hidden) {
        ensurePlaying();
      }
    };

    // Handle any pause event - immediately resume
    const handlePause = () => {
      setTimeout(ensurePlaying, 50);
    };

    // Handle errors - try to restart
    const handleError = () => {
      video.load();
      ensurePlaying();
    };

    video.addEventListener("pause", handlePause);
    video.addEventListener("error", handleError);
    document.addEventListener("visibilitychange", handleVisibilityChange);

    // Initial play
    ensurePlaying();

    return () => {
      video.removeEventListener("pause", handlePause);
      video.removeEventListener("error", handleError);
      document.removeEventListener("visibilitychange", handleVisibilityChange);
    };
  }, []);

  return (
    <div className="fixed inset-0 overflow-hidden" style={{ zIndex: -1 }}>
      <video
        ref={videoRef}
        autoPlay
        loop
        muted
        playsInline
        className="absolute inset-0 w-full h-full object-cover"
      >
        <source src="/main-bg-video.mp4" type="video/mp4" />
      </video>
      <div className="absolute inset-0 bg-gradient-to-b from-black/40 via-black/20 to-black/60" />
    </div>
  );
}
