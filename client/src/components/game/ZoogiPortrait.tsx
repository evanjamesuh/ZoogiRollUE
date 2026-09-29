import { useId, useState } from "react";

const PORTRAIT_SRC: Record<string, string> = {
  wolfgang: "/portraits/wolfgang.png",
  hotstreak: "/portraits/hotstreak.png",
  pinpoint: "/portraits/pinpoint.png",
  bolt: "/portraits/bolt.png",
  wraps: "/portraits/wraps.png",
  lars: "/portraits/lars.png",
  nightshade: "/portraits/nightshade.png",
};

export interface PortraitZoogi {
  id: string;
  name: string;
  color: string;
  secondaryColor: string;
}

/**
 * Drawn stand-in for the optional portrait PNGs. Those files are not in the
 * repo, and a missing one used to come back as the HTML page, so the browser
 * painted the alt text inside the circle.
 */
function PortraitArt({ zoogi }: { zoogi: PortraitZoogi }) {
  const gradientId = useId();
  const id = zoogi.id;
  return (
    <svg viewBox="0 0 64 64" className="h-full w-full" aria-hidden="true">
      <defs>
        <radialGradient id={gradientId} cx="38%" cy="32%" r="70%">
          <stop offset="0%" stopColor={zoogi.secondaryColor} />
          <stop offset="55%" stopColor={zoogi.color} />
          <stop offset="100%" stopColor="#140818" />
        </radialGradient>
      </defs>
      <circle cx="32" cy="32" r="30" fill={`url(#${gradientId})`} />
      <ellipse cx="24" cy="22" rx="8" ry="5" fill="#ffffff" opacity="0.28" />
      {id === "wolfgang" && (
        <>
          <polygon points="14,28 8,8 24,18" fill={zoogi.secondaryColor} />
          <polygon points="50,28 56,8 40,18" fill={zoogi.secondaryColor} />
        </>
      )}
      {id === "hotstreak" && (
        <path d="M32 6c4 6 2 8 6 12 2-6 8-6 8 2 0 6-6 8-8 4 2 8-2 14-8 14s-12-8-8-16c2 4 2 4 4-2-4 0-8-4-6-10 2 4 4 2 4-4z" fill="#FDE68A" />
      )}
      {id === "lars" && <rect x="18" y="14" width="28" height="6" rx="3" fill="#E0F2FE" />}
      {id === "bolt" && <polygon points="34,8 22,34 32,34 28,56 46,28 34,28" fill="#FEF9C3" />}
      {id === "pinpoint" && <circle cx="32" cy="18" r="5" fill="none" stroke="#F5F3FF" strokeWidth="2" />}
      {id === "wraps" && (
        <>
          <path d="M16 24h32M18 32h28M20 40h24" stroke="#E7E5E4" strokeWidth="3" />
        </>
      )}
      {id === "nightshade" && <path d="M16 20c8 10 24 10 32 0-4 16-28 16-32 0z" fill="#1a1028" opacity="0.85" />}
      <circle cx="24" cy="34" r="3.2" fill="#0b0614" />
      <circle cx="40" cy="34" r="3.2" fill="#0b0614" />
      <circle cx="25" cy="33" r="1" fill="#ffffff" />
      <circle cx="41" cy="33" r="1" fill="#ffffff" />
      <path d="M26 44c3 3 9 3 12 0" fill="none" stroke="#0b0614" strokeWidth="1.6" strokeLinecap="round" />
    </svg>
  );
}

export function ZoogiPortrait({ zoogi, className = "" }: { zoogi: PortraitZoogi; className?: string }) {
  const src = PORTRAIT_SRC[zoogi.id];
  const [photo, setPhoto] = useState<"pending" | "shown" | "fallback">(src ? "pending" : "fallback");

  return (
    <div
      className={`relative overflow-hidden rounded-full bg-black/40 ${className}`}
      data-portrait={photo === "shown" ? "photo" : photo === "fallback" ? "art" : "pending"}
      role="img"
      aria-label={zoogi.name}
    >
      <PortraitArt zoogi={zoogi} />
      {src && photo !== "fallback" && (
        <img
          src={src}
          alt=""
          className={`absolute inset-0 h-full w-full object-cover ${photo === "shown" ? "opacity-100" : "opacity-0"}`}
          style={zoogi.id === "wraps" ? { transform: "scale(1.4)", transformOrigin: "center 45%" } : undefined}
          onLoad={() => setPhoto("shown")}
          onError={() => setPhoto("fallback")}
        />
      )}
    </div>
  );
}
