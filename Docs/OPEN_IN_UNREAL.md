# Opening Zoogi Roll in your own Unreal Editor (UE 5.6)

**To open it:** double-click **"Open Zoogi Roll in Unreal"** on your Desktop.
(That shortcut runs `C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe "C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject"`.)
Double-clicking `C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject` also works: the project is set to engine 5.6.
You can also use the Epic Games Launcher (Unreal Engine > Library > My Projects > ZoogiRoll).

- The first load takes about 30-40 seconds. You should **not** get a "missing modules, rebuild?" prompt, because the
  game code (the ZoogiRollEditor Development target) is already compiled. If you ever do see it after a code change,
  click **Yes**, or run `C:\dev\ZoogiRollUnreal\Scripts\build.ps1` with the editor closed.
- The Aura plugin loads with the project. It doesn't block anything: if Aura's own window or sign-in pops up, you can
  ignore it or close it.

**Which map:** the editor opens **`Content/Maps/Meadow`** by default. That's the only map you need. The level itself is
almost empty on purpose: the game builds the arena, the sky, the Zoogis and the orbs in code when you press Play
(`AZoogiArena` / `AZoogiGameMode`). Pick which arena you want (Meadow, Frozen Ring, Volcanic Pit, Cosmic Platform,
Arabian Nights, Pharaoh's Tomb or Night Circuit) in the game's own menu.

**To play:**
1. Press the green **Play** button on the toolbar (or Alt+P). It plays in the editor viewport.
2. The Zoogi Roll menu appears. Choose players, Zoogis, the arena and the mode (Classic or 2v2 Combo), then START MATCH.
3. Click in the viewport to give it the mouse. **Shift+F1** frees the mouse. In the editor, **Esc** stops Play (that is the editor default; in the packaged game Esc goes back to the game menu).
   You can also press the red **Stop** button on the toolbar.
   For a bigger window, use the dropdown next to Play and pick **New Editor Window (PIE)** or **Standalone Game**.

**In a match:** clicking a Zoogi or orb locks on, and clicking the floor or a wall aims at that point. Pick
CONTROL / DRIVE / LAUNCH, then press CONFIRM (or Space). In Awaken, I / G / S = Infuse / Guard / Save.

**The packaged game (no editor needed):** use the "ZoogiRoll - Shortcut" on your Desktop
(`C:\dev\ZoogiRollUnreal\Packaged\Windows\ZoogiRoll.exe`).
