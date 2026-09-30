import { useCallback, useState, type ReactNode } from "react";
import { BookOpen, ChevronLeft, Monitor, Play, Settings, Users } from "lucide-react";
import { useZoogiGame, ZOOGI_ROSTER } from "@/lib/stores/useZoogiGame";
import { ComicViewer } from "./ComicViewer";
import { SettingsScreen } from "./SettingsScreen";
import { useMenuKeys } from "@/hooks/useMenuKeys";
import { consumeMenuScreen, type MenuScreen } from "@/lib/menuReturn";

function MenuButton({
  onClick,
  className,
  children,
  testId,
}: {
  onClick: () => void;
  className: string;
  children: ReactNode;
  testId: string;
}) {
  return (
    <button
      type="button"
      data-testid={testId}
      onClick={onClick}
      className={`inline-flex min-h-14 w-full items-center justify-center gap-3 rounded-full px-6 text-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98] focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white ${className}`}
    >
      {children}
    </button>
  );
}

function BackButton({ onClick }: { onClick: () => void }) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="inline-flex min-h-11 items-center gap-1 self-start rounded-full bg-white/15 px-4 text-base font-semibold text-white hover:bg-white/25 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
    >
      <ChevronLeft size={20} />
      Back
    </button>
  );
}

function HomeMenu({
  onPlay,
  onStory,
  onSettings,
}: {
  onPlay: () => void;
  onStory: () => void;
  onSettings: () => void;
}) {
  return (
    <div className="menu-safe absolute inset-0 overflow-y-auto" data-testid="main-menu">
      <div className="mx-auto flex min-h-full w-full max-w-6xl flex-col justify-center gap-8 py-4 landscape:flex-row landscape:items-center landscape:justify-between landscape:gap-10 lg:gap-16">
        <div className="text-center landscape:max-w-md landscape:text-left lg:max-w-xl">
          <p className="text-sm font-bold tracking-[0.35em] text-orange-300">ZOOGI ROLL</p>
          <h1 className="mt-2 text-5xl font-black leading-none text-white lg:text-7xl">Arena</h1>
          <p className="mt-3 text-base text-white/75 lg:text-xl">
            Launch your Zoogi. Knock orbs and rivals off the court.
          </p>
          <div className="mt-5 flex flex-wrap justify-center gap-2 landscape:justify-start">
            {ZOOGI_ROSTER.map((zoogi) => (
              <span
                key={zoogi.id}
                title={zoogi.name}
                className="h-9 w-9 rounded-full shadow-md ring-2 ring-white/30 lg:h-11 lg:w-11"
                style={{
                  background: `radial-gradient(circle at 30% 30%, ${zoogi.secondaryColor}, ${zoogi.color})`,
                }}
              />
            ))}
          </div>
        </div>

        <div className="flex w-full flex-col gap-3 landscape:max-w-sm lg:max-w-md lg:gap-4">
          <MenuButton
            testId="menu-play"
            onClick={onPlay}
            className="bg-gradient-to-r from-green-500 to-emerald-600 text-white shadow-lg shadow-green-500/40"
          >
            <Play size={26} fill="white" />
            Play
          </MenuButton>
          <MenuButton
            testId="menu-story"
            onClick={onStory}
            className="bg-gradient-to-r from-emerald-600 to-teal-500 text-white shadow-lg shadow-emerald-500/30"
          >
            <BookOpen size={24} />
            Story
          </MenuButton>
          <MenuButton
            testId="menu-settings"
            onClick={onSettings}
            className="bg-white/15 text-white hover:bg-white/25"
          >
            <Settings size={24} />
            Settings
          </MenuButton>
        </div>
      </div>
    </div>
  );
}

function HowToPlay({
  onBack,
  onVsComputer,
  onLocal,
}: {
  onBack: () => void;
  onVsComputer: () => void;
  onLocal: () => void;
}) {
  return (
    <div className="menu-safe absolute inset-0 overflow-y-auto" data-testid="how-to-play">
      <div className="mx-auto flex min-h-full w-full max-w-6xl flex-col justify-center gap-6 py-2 landscape:flex-row landscape:items-center landscape:gap-10 lg:gap-16">
        <div className="landscape:max-w-sm lg:max-w-md">
          <BackButton onClick={onBack} />
          <h1 className="mt-4 text-4xl font-black leading-tight text-white lg:text-6xl">How do you want to play?</h1>
          <p className="mt-3 text-base text-white/70 lg:text-lg">Two ways to start a match. Both stay on this device.</p>
        </div>
        <div className="flex w-full flex-col gap-3 landscape:max-w-md lg:max-w-lg">
          <button
            type="button"
            data-testid="play-vs-computer"
            onClick={onVsComputer}
            className="flex min-h-[5.5rem] items-center gap-4 rounded-3xl bg-gradient-to-r from-green-500 to-emerald-600 px-5 text-left text-white shadow-lg shadow-green-500/30 hover:scale-[1.01] active:scale-[0.99] focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
          >
            <Monitor size={28} className="shrink-0" />
            <span>
              <span className="block text-xl font-bold">Vs Computer</span>
              <span className="block text-sm text-white/85">You pick a Zoogi and how many computer opponents.</span>
            </span>
          </button>
          <button
            type="button"
            data-testid="play-local"
            onClick={onLocal}
            className="flex min-h-[5.5rem] items-center gap-4 rounded-3xl bg-gradient-to-r from-blue-500 to-cyan-500 px-5 text-left text-white shadow-lg shadow-blue-500/30 hover:scale-[1.01] active:scale-[0.99] focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
          >
            <Users size={28} className="shrink-0" />
            <span>
              <span className="block text-xl font-bold">Local Multiplayer</span>
              <span className="block text-sm text-white/85">Two to four players share this phone or computer.</span>
            </span>
          </button>
        </div>
      </div>
    </div>
  );
}

export function MainMenu() {
  const [screen, setScreen] = useState<MenuScreen>(() => consumeMenuScreen());
  const setPhase = useZoogiGame((state) => state.setPhase);
  const setGameMode = useZoogiGame((state) => state.setGameMode);

  const goHome = useCallback(() => setScreen("home"), []);

  useMenuKeys({
    onBack: screen === "home" || screen === "story" ? undefined : goHome,
    onConfirm: screen === "home" ? () => setScreen("play") : undefined,
  });

  if (screen === "story") {
    return <ComicViewer onBack={goHome} />;
  }

  if (screen === "settings") {
    return <SettingsScreen onBack={goHome} />;
  }

  if (screen === "play") {
    return (
      <HowToPlay
        onBack={goHome}
        onVsComputer={() => {
          setGameMode("classic");
          setPhase("character_selection");
        }}
        onLocal={() => {
          setGameMode("local_multiplayer");
          setPhase("local_setup");
        }}
      />
    );
  }

  return (
    <HomeMenu
      onPlay={() => setScreen("play")}
      onStory={() => setScreen("story")}
      onSettings={() => setScreen("settings")}
    />
  );
}
