# Zoogi Roll — Unreal Engine 5.6 rebuild: progress log

Project: `C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject` (C++ module `ZoogiRoll`, Chaos physics).
The web game at `C:\dev\ZoogiRollUE` is read-only reference and is never modified.

## How to open and play
1. Double-click `C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject` (UE 5.6). It is already built
   (`Binaries\Win64\UnrealEditor-ZoogiRoll.dll`). If UE asks to rebuild, say Yes, or run
   `powershell -File C:\dev\ZoogiRollUnreal\Scripts\build.ps1`.
2. The editor opens `/Game/Maps/Meadow` (an empty level: the game mode builds the arena, sky
   and lights when play starts). Press **Play** (Alt+P). A "Selected Viewport" or
   "New Editor Window (PIE)" both work.
3. Or run it standalone without the editor UI:
   `"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject -game -windowed -ResX=1600 -ResY=900`

### Controls
* Menu: pick 2-4 players, each player's Zoogi (< >), HUMAN/CPU, rounds and arena, then START MATCH.
* Your turn: **press the left mouse button anywhere, pull back (toward the camera), release** — the
  Zoogi shoots the opposite way. The dotted line is the predicted roll (red past the edge), the bar is
  power; past the white tick (80%) the shot ramps into a full slingshot.
* **A / D**, arrow keys, or **right-mouse drag** turn the aim heading before you pull.
  Right-click during a pull cancels it.
* **Esc**: cancel pull / back to menu. **Ctrl+R**: restart the match.

### Rules (ported from useZoogiGame.tsx / simFeel.ts / arenaConstants.ts)
* Hotseat turns, 2-4 local players (any mix of human and CPU). A turn passes when every marble and
  orb has been at rest for 1.5 s (SETTLE_DELAY_SECONDS).
* Orb knocked off the edge: +50 to the Zoogi that last touched it (chains count: shooter -> Zoogi -> orb).
* Zoogi knocked off: -75 for it, +100 to the shooter who knocked it. It returns to its start pad.
* The match ends when all orbs are gone or after N rounds; highest score wins.
* Physics: 1 web unit = 100 cm. Zoogi radius 50 cm, orb 40 cm (0.512 mass), per-Zoogi mass =
  defense/60 clamped 0.55-1.8 (x10 kg). Constant rolling deceleration 4.5 u/s^2 (ice 1.15),
  launch curve `launchSpeedPerSecForPull` (quadratic to 80%, smoothstep to 36 u/s at full pull),
  restitution ~0.75 Zoogi-Zoogi / ~0.68 Zoogi-orb, CCD on, physics sub-stepping at 120 Hz.
* Camera follows alpha-camera-spec.md: aim pose 30 deg / 7 D behind the Zoogi, eases back and up
  while pulling, rises to a 55 deg / 24 D soft follow after release with heading drift after
  bounces, holds when the marble falls, and hard-cuts + swoops into the next player's aim pose.

## Milestones
- [x] **1. Project + build** — C++ project created, `.gitignore`, local git repo. Builds with
  `Build.bat ZoogiRollEditor Win64 Development` (0 errors). Headless editor (`UnrealEditor-Cmd
  -run=pythonscript`) opens it with 0 errors.
- [x] **2. Import** — Interchange glTF import via Python (`Scripts/setup_content.py`): Wolfgang,
  Hotstreak, Pinpoint, Bolt, Wraps, Lars, Nightshade, orb, 5 stylized scenery props.
  The Zoogi GLBs require `EXT_texture_webp`, which UE's glTF importer rejects, so
  `Scripts/glb_unwebp.py` rewrites copies (webp -> png) into `SourceArt/Zoogis` first (originals untouched).
  Materials: `M_ZoogiSurface`, `M_ZoogiGlow` (emissive + fresnel rim: the light-blue orbs),
  `M_ZoogiGround` (world-noise two-tone ground). Models are fitted at runtime (bounds fit to the
  1 m marble; Nightshade pivot-fit like zoogiModels.ts).
  The web Meadow arena is drawn in code (no GLB), so the UE Meadow is also built in code:
  floating grass island (radius 15.5 u), six rim boulders at the GRASS_RIM spots with cylinder
  colliders, flowers, start pads, and floating forest islets using the imported trees.
- [x] **3. Playable Meadow match** — everything listed above (pawns, slingshot, trajectory +
  power meter, Chaos bounce/roll, orbs, knock-off scoring, settle-based turns, 2-4 hotseat,
  alpha camera, HUD with scoreboard/turn banner/popups, results screen) plus CPU opponents.
- [x] **4. Verification** — `Scripts/run_demo.ps1` runs the game with `-ZoogiAuto` (CPU players)
  and saves screenshots on gameplay events. `-ZoogiHumanTest` drives P1 with a scripted virtual
  mouse through the same drag code (verified: 679 cm ground pull -> 90% shot). Screenshots are
  mirrored to the box at `/workspace/zoogi-unreal/shots/`.
- [ ] **5. More** — arenas (Frozen Ring / Volcanic Pit / Cosmic Platform are selectable, built in
  code, first pass), Zoogi powers, polish.

## Command-line switches (for testing)
`-ZoogiAuto` (all CPU, start at once), `-ZoogiPlayers=N`, `-ZoogiArena=0..3`, `-ZoogiRounds=N`,
`-ZoogiShots=<dir>` (event screenshots), `-ZoogiQuit=<sec>`, `-ZoogiMenuShot`, `-ZoogiHumanTest`.

## Scripts
* `Scripts/build.ps1` — compile the editor target (log: `Saved/Logs/build.log`).
* `Scripts/run_python.ps1 -Script Scripts\setup_content.py` — headless import/material setup.
* `Scripts/run_demo.ps1` — scripted demo run + screenshots.
* `Scripts/glb_unwebp.py` — strip EXT_texture_webp from a GLB (run with Python + Pillow).

## Known issues / notes
* Real-mouse play has not been exercised by a person yet (the scripted virtual mouse uses the
  same code path). Please try a match and report how the pull feels (`ZoogiConst::DragScale`).
* The PC had ~300 MB free RAM during builds (another UE editor with 5.5 GB was open), so UBT
  falls back to 1 compile action; builds still take ~1 min.
* CPU players are simple (pick one of the 3 best orb/rival shots, aim error from Control stat).
* No sound yet. No Zoogi powers yet (abilities are shown in the menu only).
* Island edge collision is the engine cylinder's convex hull (a many-sided polygon close to r=15.5).
