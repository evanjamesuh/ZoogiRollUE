import { useState } from "react";
import { ChevronLeft, Volume2, VolumeX } from "lucide-react";
import { useAudio } from "@/lib/stores/useAudio";
import {
  DEFAULT_ARENA_BUTTON_SETTINGS,
  loadArenaButtonSettings,
  saveArenaButtonSettings,
  type ArenaButtonSettings,
} from "@/lib/arenaButtonSettings";

interface SettingsScreenProps {
  onBack: () => void;
}

function SliderRow({
  label,
  hint,
  value,
  min,
  max,
  onChange,
}: {
  label: string;
  hint: string;
  value: number;
  min: number;
  max: number;
  onChange: (value: number) => void;
}) {
  return (
    <label className="block rounded-2xl bg-white/10 p-4">
      <span className="flex items-baseline justify-between gap-3">
        <span className="text-base font-semibold text-white">{label}</span>
        <span className="text-sm tabular-nums text-white/70">{value}px</span>
      </span>
      <span className="mt-1 block text-sm text-white/60">{hint}</span>
      <input
        type="range"
        min={min}
        max={max}
        value={value}
        aria-label={label}
        onChange={(event) => onChange(parseInt(event.target.value, 10))}
        className="mt-2 h-11 w-full accent-orange-400"
      />
    </label>
  );
}

export function SettingsScreen({ onBack }: SettingsScreenProps) {
  const isMuted = useAudio((state) => state.isMuted);
  const toggleMute = useAudio((state) => state.toggleMute);
  const [buttons, setButtons] = useState<ArenaButtonSettings>(loadArenaButtonSettings);

  const updateButtons = (patch: Partial<ArenaButtonSettings>) => {
    const next = { ...buttons, ...patch };
    setButtons(next);
    saveArenaButtonSettings(next);
  };

  return (
    <div className="menu-safe absolute inset-0 overflow-y-auto" data-testid="settings-screen">
      <div className="mx-auto flex min-h-full w-full max-w-3xl flex-col gap-5 py-2 lg:justify-center lg:py-10">
        <div className="flex items-center gap-3">
          <button
            type="button"
            onClick={onBack}
            className="inline-flex min-h-11 items-center gap-1 rounded-full bg-white/15 px-4 text-base font-semibold text-white hover:bg-white/25 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
          >
            <ChevronLeft size={20} />
            Back
          </button>
          <h1 className="text-3xl font-black text-white lg:text-5xl">Settings</h1>
        </div>

        <button
          type="button"
          onClick={() => toggleMute()}
          className="flex min-h-[4.5rem] items-center gap-4 rounded-2xl bg-white/10 px-4 text-left hover:bg-white/15 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
        >
          <span className="flex h-12 w-12 shrink-0 items-center justify-center rounded-full bg-orange-500 text-white">
            {isMuted ? <VolumeX size={24} /> : <Volume2 size={24} />}
          </span>
          <span className="min-w-0 flex-1">
            <span className="block text-lg font-bold text-white">Sound</span>
            <span className="block text-sm text-white/70">Menu music and match sounds</span>
          </span>
          <span className={`rounded-full px-4 py-2 text-sm font-bold ${isMuted ? "bg-white/15 text-white/80" : "bg-green-500 text-white"}`}>
            {isMuted ? "Off" : "On"}
          </span>
        </button>

        <div className="rounded-3xl bg-black/35 p-4 backdrop-blur-sm lg:p-6">
          <h2 className="text-lg font-bold text-white">Arena buttons</h2>
          <p className="mt-1 text-sm text-white/65">
            Changes how the arena choices look on the Choose Arena screen.
          </p>
          <div className="mt-4 flex flex-col gap-3">
            <SliderRow
              label="Button size"
              hint="Taller buttons are easier to tap."
              value={buttons.buttonHeight}
              min={50}
              max={120}
              onChange={(buttonHeight) => updateButtons({ buttonHeight })}
            />
            <SliderRow
              label="Button roundness"
              hint="How round the arena buttons are."
              value={buttons.buttonRadius}
              min={0}
              max={30}
              onChange={(buttonRadius) => updateButtons({ buttonRadius })}
            />
            <SliderRow
              label="Space between buttons"
              hint="The gap between arena choices."
              value={buttons.buttonGap}
              min={4}
              max={24}
              onChange={(buttonGap) => updateButtons({ buttonGap })}
            />
          </div>
          <button
            type="button"
            onClick={() => {
              setButtons(DEFAULT_ARENA_BUTTON_SETTINGS);
              saveArenaButtonSettings(DEFAULT_ARENA_BUTTON_SETTINGS);
            }}
            className="mt-4 min-h-11 rounded-full bg-white/10 px-5 text-sm font-semibold text-white hover:bg-white/20 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-white"
          >
            Reset arena buttons
          </button>
        </div>
      </div>
    </div>
  );
}
