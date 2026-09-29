import "@fontsource/share-tech-mono/400.css";

/**
 * Broadcast strip for Night Circuit. Digits use Share Tech Mono,
 * which is under the SIL Open Font License.
 * Width stays inside a phone screen so it does not cover the court.
 */
export function NeonScoreboard({
  playerScore,
  foeScore,
  round,
  maxRounds,
  timer,
}: {
  playerScore: number;
  foeScore: number;
  round: number;
  maxRounds: number;
  timer: string;
}) {
  const digit = {
    fontFamily: '"Share Tech Mono", monospace',
    fontWeight: 400,
    letterSpacing: "0.04em",
    textShadow: "0 0 10px currentColor",
  } as const;

  return (
    <div
      id="neon-scoreboard"
      className="pointer-events-none absolute left-1/2 z-30 w-[min(100%-0.75rem,520px)] -translate-x-1/2"
      style={{ top: "max(0.35rem, env(safe-area-inset-top))" }}
    >
      <div className="flex items-stretch" style={digit}>
        <div
          className="flex min-w-0 flex-1 items-center justify-between gap-1 px-2 py-1 sm:gap-3 sm:px-4 sm:py-2"
          style={{
            background: "linear-gradient(90deg, rgba(6,28,40,0.92), rgba(8,18,32,0.88))",
            border: "1px solid #22e7ff",
            boxShadow: "0 0 14px rgba(34,231,255,0.35)",
            clipPath: "polygon(8px 0, 100% 0, calc(100% - 8px) 100%, 0 100%)",
          }}
        >
          <span className="text-[9px] tracking-[0.14em] text-cyan-200/80 sm:text-[10px] sm:tracking-[0.2em]">YOU</span>
          <span className="text-2xl leading-none text-cyan-100 sm:text-4xl">{playerScore}</span>
        </div>
        <div
          className="flex min-w-0 flex-[1.2] flex-col items-center justify-center px-2 py-1 sm:px-5"
          style={{
            background: "rgba(6,8,16,0.92)",
            borderTop: "1px solid #b026ff",
            borderBottom: "1px solid #b026ff",
            boxShadow: "0 0 16px rgba(176,38,255,0.28)",
          }}
        >
          <span className="text-[9px] tracking-[0.16em] text-fuchsia-200/80 sm:text-[10px]">RND {round}/{maxRounds}</span>
          <span className="text-2xl leading-none text-white sm:text-4xl">{timer}</span>
        </div>
        <div
          className="flex min-w-0 flex-1 items-center justify-between gap-1 px-2 py-1 sm:gap-3 sm:px-4 sm:py-2"
          style={{
            background: "linear-gradient(90deg, rgba(32,8,24,0.88), rgba(48,8,28,0.92))",
            border: "1px solid #ff2bd6",
            boxShadow: "0 0 14px rgba(255,43,214,0.35)",
            clipPath: "polygon(0 0, calc(100% - 8px) 0, 100% 100%, 8px 100%)",
          }}
        >
          <span className="text-2xl leading-none text-pink-100 sm:text-4xl">{foeScore}</span>
          <span className="text-[9px] tracking-[0.14em] text-pink-200/80 sm:text-[10px] sm:tracking-[0.2em]">FOE</span>
        </div>
      </div>
    </div>
  );
}
