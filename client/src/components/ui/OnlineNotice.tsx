import { WifiOff } from "lucide-react";
import { useOnlineMessage } from "@/lib/serverStatus";

export function OnlineNotice({ compact = false, className = "" }: { compact?: boolean; className?: string }) {
  const message = useOnlineMessage();
  if (!message) return null;

  return (
    <div
      role="status"
      className={`${
        compact
          ? "rounded-xl bg-black/50 border border-white/15 px-3 py-2 text-left"
          : "rounded-2xl bg-black/55 border border-white/15 px-4 py-3 text-left shadow-lg"
      } ${className}`}
    >
      <div className="flex items-start gap-2">
        <WifiOff className={`shrink-0 text-amber-300 ${compact ? "mt-0.5" : "mt-1"}`} size={compact ? 16 : 18} />
        <div>
          <p className={`font-semibold text-white ${compact ? "text-sm" : "text-base"}`}>
            Online features are off
          </p>
          <p className={`text-white/80 ${compact ? "text-xs mt-0.5" : "text-sm mt-1"}`}>{message}</p>
        </div>
      </div>
    </div>
  );
}
