import "@fontsource/share-tech-mono/400.css";

/**
 * Broadcast strip for Night Circuit. Digits use Share Tech Mono,
 * which is under the SIL Open Font License.
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
    <div id="neon-scoreboard" className="absolute top-3 left-1/2 -translate-x-1/2 z-20 pointer-events-none">
      <div className="flex items-stretch" style={digit}>
        <div
          className="flex items-center gap-3 pl-5 pr-6 py-2 min-w-[132px]"
          style={{
            background: "linear-gradient(90deg, rgba(6,28,40,0.92), rgba(8,18,32,0.88))",
            border: "1px solid #22e7ff",
            boxShadow: "0 0 14px rgba(34,231,255,0.35)",
            clipPath: "polygon(10px 0, 100% 0, calc(100% - 10px) 100%, 0 100%)",
          }}
        >
          <span className="text-[10px] tracking-[0.2em] text-cyan-200/80">YOU</span>
          <span className="text-4xl text-cyan-100 leading-none">{playerScore}</span>
        </div>
        <div
          className="flex flex-col items-center justify-center px-5 min-w-[168px] -mx-2"
          style={{
            background: "rgba(6,8,16,0.92)",
            borderTop: "1px solid #b026ff",
            borderBottom: "1px solid #b026ff",
            boxShadow: "0 0 16px rgba(176,38,255,0.28)",
          }}
        >
          <span className="text-[10px] tracking-[0.22em] text-fuchsia-200/80">RND {round}/{maxRounds}</span>
          <span className="text-4xl text-white leading-none">{timer}</span>
        </div>
        <div
          className="flex items-center justify-end gap-3 pl-6 pr-5 py-2 min-w-[132px]"
          style={{
            background: "linear-gradient(90deg, rgba(32,8,24,0.88), rgba(48,8,28,0.92))",
            border: "1px solid #ff2bd6",
            boxShadow: "0 0 14px rgba(255,43,214,0.35)",
            clipPath: "polygon(0 0, calc(100% - 10px) 0, 100% 100%, 10px 100%)",
          }}
        >
          <span className="text-4xl text-pink-100 leading-none">{foeScore}</span>
          <span className="text-[10px] tracking-[0.2em] text-pink-200/80">FOE</span>
        </div>
      </div>
    </div>
  );
}
