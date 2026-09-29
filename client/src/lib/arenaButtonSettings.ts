export const ARENA_BUTTON_SETTINGS_KEY = "map_selection_dev_settings";

export interface ArenaButtonSettings {
  buttonHeight: number;
  buttonRadius: number;
  buttonGap: number;
}

export const DEFAULT_ARENA_BUTTON_SETTINGS: ArenaButtonSettings = {
  buttonHeight: 80,
  buttonRadius: 12,
  buttonGap: 12,
};

export function loadArenaButtonSettings(): ArenaButtonSettings {
  try {
    const stored = localStorage.getItem(ARENA_BUTTON_SETTINGS_KEY);
    if (stored) {
      const parsed = JSON.parse(stored) as Partial<ArenaButtonSettings>;
      return { ...DEFAULT_ARENA_BUTTON_SETTINGS, ...parsed };
    }
  } catch {
    // Keep the defaults when storage is unavailable.
  }
  return DEFAULT_ARENA_BUTTON_SETTINGS;
}

export function saveArenaButtonSettings(settings: ArenaButtonSettings) {
  try {
    localStorage.setItem(ARENA_BUTTON_SETTINGS_KEY, JSON.stringify(settings));
  } catch {
    // The arena screen still uses the in-memory values for this visit.
  }
}
