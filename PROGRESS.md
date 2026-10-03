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
4. Or play the packaged build (no editor needed): `C:\dev\ZoogiRollUnreal\Packaged\Windows\ZoogiRoll.exe`
   (rebuild it with `powershell -File C:\dev\ZoogiRollUnreal\Scripts\package.ps1`, ~2-4 min).

### Controls (lock-on targeting, since 4:20 PM Oct 3 — the drag-back slingshot was removed)
* Menu (UMG, `ZoogiMenuWidget.cpp`): pick 2-4 players, each player's Zoogi (< >) on its card (power,
  stat bars, mass), HUMAN/CPU, rounds and one of 7 arenas, then START MATCH. (`-ZoogiCanvasMenu` = old canvas menu.)
* Every turn runs through five stages, shown in the phase tracker at the top with a banner on each change:
  **DRAW** (hero shot of the active Zoogi) -> **STANDBY** (calm overview; click a target = soft lock) ->
  **MAIN** (tactical over-the-shoulder; lock / switch targets) -> **BATTLE** (cinematic lock-on, ~0.5 s hold,
  then the shot fires) -> **END** (follow cam, then a near top-down aerial while everything settles).
* **Left-click an enemy Zoogi or an orb** to lock on (hover shows a soft ring + name). Locked target gets a
  glowing ground ring, rotating reticle brackets, a highlight shell and a bobbing marker; the aim line,
  predicted roll and shot power show, and the target panel (top right) gives name, distance, shot % and hit speed.
* **Tab** cycles targets (nearest first). **Click another target** to switch. **RMB or Esc** clears the lock
  (Esc with no lock = back to menu).
* **SPACE** (or clicking the locked target again) fires: the shot goes straight at the target, power scales with
  distance (closer = softer, farther = harder, capped at the old max power).
* **E / Q** (or the POWER button, bottom right): use your Zoogi's power (SPACE now fires).
* **Ctrl+R**: restart the match. A/D turning was dropped (no free camera while targeting).
* Camera profiles per stage are one table, `ZoogiCamTune` at the top of `ZoogiPlayerController.cpp`
  (pitch, boom, FOV, Dutch tilt, lag, yaw spring, vignette, DOF, grade) plus Battle extras (arc swing with
  dip + overshoot, FOV punch-in, shake, 0.6x slow-mo dip while everything is at rest, punch-out on fire).
  Stage timings: `ZoogiStageTune` in `ZoogiGameMode.h`. A sphere probe on the camera channel keeps the camera
  out of walls and props (decorative props block only the camera channel).
* Per-phase cameras (4:05 PM spec, each with its own look). Every profile change eases in on the clock over its
  `BlendIn` column (0.4-0.6 s); after that the profile's own lags handle tracking:
  1. DRAW hero shot: pitch 7, close boom, push-in plus a 40 deg orbit sweep, FOV 102, warm grade, light DOF.
  2. STANDBY calm overview: pitch 66, far boom, slow 5 deg/s drift around the arena centre, neutral soft grade, no shake.
     A click here is a soft lock (ring and highlight only, the camera stays wide).
  3. MAIN tactical over-the-shoulder: pitch 37, medium boom, aimed down the shooter-to-target line, or toward the
     arena centre with no lock. Responsive, with a medium swing on lock and a whip-pan on switch. Clean grade.
  4. BATTLE dramatic lock-on: pitch 24, arc swing, FOV punch, 4 deg Dutch, shake, DOF, vignette, contrasty grade,
     0.5 s hold (CPU 0.3 s), fire, then the follow cam. Long-range locks pull the look point back so the shooter stays in frame.
  5. END aerial resolution: follow the shot 1.4 s, then pull up to pitch 83 over the whole arena with a slow settle,
     a cooler, desaturated grade and score popups. On Night Circuit it squares up to the rectangle and comes in closer.
  Battle also slides in cinematic letterbox bars; the bottom power meter hides (the target panel keeps shot % / speed).

### Zoogi powers
Every Zoogi starts with 1 power charge (max 3). The three **gold star orbs** are worth 100 and give
the scorer +1 charge. One power per turn. Charges show as `***` in the scoreboard and power panel.
| Zoogi | Power | How it works |
|---|---|---|
| Wolfgang | Wolf Pack | While rolling: 3 ghost wolves spawn and chase the nearest orbs/rivals for ~3 s (credit goes to you). |
| Hotstreak | Instant Explosion | While aiming or rolling: blast (r 4.5 u) pushes every orb and rival away from you. |
| Lars | Ricochet | Arm while aiming: after each of your next 2 hits you bounce toward the nearest target. |
| Pinpoint | Laser Trajectory | Arm while aiming: your shot snaps onto the orb/rival nearest your aim (within 35 deg); laser + LOCKED reticle. |
| Bolt | Static Shock | Arm while aiming: this shot phases through rivals and stuns them (they skip their next turn). |
| Wraps | Bandage Bind | Arm while aiming: rivals you touch are slowed and their next shot is 35% weaker. |
| Nightshade | Shadow Stun | While aiming or rolling: shadow pulse (r 5.2 u) freezes nearby rivals and they skip their next turn. |
CPU players use powers too (about half the time they have a charge).

### Rules (ported from useZoogiGame.tsx / simFeel.ts / arenaConstants.ts)
* Hotseat turns, 2-4 local players (any mix of human and CPU). A turn passes when every marble and
  orb has been at rest for 1.5 s (SETTLE_DELAY_SECONDS).
* Orb knocked off the edge: +50 to the Zoogi that last touched it (chains count: shooter -> Zoogi -> orb).
* Zoogi knocked off: -75 for it, +100 to the shooter who knocked it. It returns to its start pad.
* The match ends when all orbs are gone or after N rounds; highest score wins.
* Physics: 1 web unit = 100 cm. Zoogi radius 50 cm, orb 40 cm (0.512 mass), per-Zoogi mass =
  defense/60 clamped 0.55-1.8 (x10 kg). Constant rolling deceleration 4.5 u/s^2 (ice 1.15),
  shot speed scales with distance to the locked target (capped at the web game's 36 u/s max),
  restitution ~0.75 Zoogi-Zoogi / ~0.68 Zoogi-orb, CCD on, physics sub-stepping at 120 Hz.
* Camera: per-phase duel cameras (see Controls above); the alpha-camera-spec drag/aim pose was replaced.

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
- [x] **3. Playable Meadow match** — everything listed above (pawns, shooting, trajectory +
  power meter, Chaos bounce/roll, orbs, knock-off scoring, settle-based turns, 2-4 hotseat,
  alpha camera, HUD with scoreboard/turn banner/popups, results screen) plus CPU opponents.
- [x] **4. Verification** — `Scripts/run_demo.ps1` runs the game with `-ZoogiAuto` (CPU players)
  and saves screenshots on gameplay events. `-ZoogiHumanTest` drives P1 with a scripted virtual
  mouse and keys through the real lock-on input path (soft lock, Tab, switch, Space). Screenshots are
  mirrored to the box at `/workspace/zoogi-unreal/shots/`.
- [x] **4b. Packaged build** — `Scripts/package.ps1` (BuildCookRun, Development, ~2 min) ->
  `Packaged\Windows\ZoogiRoll.exe` (~0.9 GB). Verified with a 4-CPU auto match (screenshots `shots/pkg1`).
- [~] **5. More** (in progress)
  - [x] All 7 Zoogi powers + gold star orbs/charges + CPU power use + HUD power panel (`ZoogiPowers.cpp`).
  - [x] Additive FX material `M_ZoogiFX` (`Scripts/make_fx.py`) for shockwaves, rings, sparks, clones.
  - [x] Impact sparks on hard hits, knock-off bursts.
  - [x] Frozen Ring, Volcanic Pit, Cosmic Platform verified in-game (selectable in the menu).
  - [x] Audio: Evan's menu/match music from the web game + original synthesized SFX
    (launch, clack, score, fall, explosion, zap, arm, turn, win) in `/Game/Audio` (`Scripts/import_audio.py`).
  - [x] Arabian Nights (desert island, palace islets with domes/minarets, lanterns, crescent moon),
    Pharaoh's Tomb (sandstone court, obelisks, torches, sarcophagus islets) and Night Circuit
    (rectangular 24x16 m neon court with rails, corner mouths, solid pads/posts, centre channels,
    synthwave skyline) — all 7 web arenas now selectable. Night Circuit uses arena-shape queries
    (`IsOffCourt`, `DistanceToEdge`, `IsSpotClear`) for orbs, CPU aim and the trajectory line.
  - [x] Proper UMG main menu built in C++ (`UZoogiMenuWidget`, no Blueprint asset): title, players/rounds,
    one card per player with Zoogi, power description and stat bars, arena grid, START / QUIT.
  - [x] Fixed: in-match MENU / REMATCH / POWER buttons were swallowed by a misplaced `else` in `MenuAction`.
  - [x] Niagara VFX: engine templates duplicated into `/Game/FX`, rescaled and recoloured by
    `Scripts/make_niagara.py` via `UZoogiFXLibrary` (sparks on hits, coloured bursts for knock-offs and powers,
    explosion for Hotstreak), layered on the `AZoogiFX` glows.
  - [x] RPG lock-on targeting + per-stage duel cameras (replaces drag-to-shoot), CPU uses the same flow.
  - [x] Per-phase camera profiles with time-based 0.4-0.6 s blends between phases; Main aims at the arena centre
        with no lock; Battle keeps the shooter in frame on long locks; darker Arabian sand.
  - [x] Battle letterbox bars slide in (the power meter and controls line hide under them); Night Circuit End aerial
        squares up to the rectangular court and sits closer so it fills the frame.
  - [x] Polish queue pass 1 (4:35-5:00 PM):
        - Lock-on/camera blends: Main and Battle blend in with an ease-out (respond immediately), the mood shots
          (Draw / Standby / End) ease in and out; lock-in brackets swing in with a half turn, shell and light flash.
        - CPU: rejects shots that can't knock the target off from there, avoids shots where it would follow
          the target off the edge, never picks a clearly worse shot just for variety, spends charges more when
          it has 2+, keeps rival-only powers for rival shots, and caps Bolt's phasing shot short of the edge.
        - Audio: per-asset loudness trims from measured source loudness (Score +7.6 dB, HitClick +5 dB, Fall -4.4 dB,
          Arm/Zap -2 dB) in `ZoogiUtil::PlaySfx`; still not listened to.
        - Balance: Hotstreak explosion radius 450 -> 390 and push 650+1500k -> 600+1400k; Bolt stun push 450 -> 680; Lars ricochet speed 900-1400 -> 1050-1700; Pinpoint's power reworked
          for lock-on (the locked shot hits 30% harder, since everyone gets a perfect lock line now).
        - Fixed: Meadow grass/flower and star-field ISMs fell back to the default material (the shared materials
          lacked the instanced-mesh usage flag); `Scripts/fix_material_usage.py` sets it.
  - [x] Battle framing guard (5:00 PM): the camera tilts so the shooter's bottom stays within 62% of the half-height
        below centre (clear of the letterbox / bottom HUD) and the target's top within 62% above (below the top HUD);
        long locks rise +10 deg and pull back 30%, and the FOV widens (up to 92) when both can't fit. Letterbox 7.5%.
  - [x] CPU power timing: armed powers arm once the target is locked in Main (visible before Battle), Wolf Pack
        releases while fast with 2+ targets near, Hotstreak blows at the crowd peak / with a rival near the rim.
  - [x] Automation quits reliably (auto match over -> quit, plus a forced exit 5 s after ZoogiQuit if needed).
  - [x] Menu portraits: each player card shows an in-game hero shot of its Zoogi (captured with `-ZoogiNoHUD`
        on the Draw camera, cropped, in `Art/Portraits`, imported by `Scripts/import_portraits.py` to
        `/Game/UI/Portraits`, cooked via DirectoriesToAlwaysCook). Menu subtitle no longer says "pull back".
  - [ ] More polish.

## Command-line switches (for testing)
`-ZoogiAuto` (all CPU, start at once), `-ZoogiPlayers=N`, `-ZoogiArena=0..6` (0 Meadow, 1 Frozen, 2 Volcanic, 3 Cosmic, 4 Arabian, 5 Tomb, 6 Night Circuit), `-ZoogiRounds=N`,
`-ZoogiShots=<dir>` (event screenshots), `-ZoogiQuit=<sec>`, `-ZoogiMenuShot`, `-ZoogiHumanTest`,
`-ZoogiCanvasMenu`, `-ZoogiPowerTest` (3 charges each, CPUs always use powers), `-ZoogiPicks=4,5,6,0` (roster index per slot:
0 Wolfgang, 1 Hotstreak, 2 Lars, 3 Pinpoint, 4 Bolt, 5 Wraps, 6 Nightshade).

## Scripts
* `Scripts/build.ps1` — compile the editor target (log: `Saved/Logs/build.log`).
* `Scripts/run_python.ps1 -Script Scripts\setup_content.py` — headless import/material setup.
* `Scripts/run_demo.ps1` — scripted demo run + screenshots.
* `Scripts/glb_unwebp.py` — strip EXT_texture_webp from a GLB (run with Python + Pillow).
* `Scripts/make_fx.py`, `Scripts/import_audio.py` — FX material / audio import (headless).
* `Scripts/package.ps1` — packaged Win64 build into `Packaged/`.

## Known issues / notes
* Real-mouse play has not been exercised by a person yet (the scripted virtual mouse uses the
  same code path). Please try a match and report how targeting and the phase cameras feel (`ZoogiCamTune`).
* The PC had ~300 MB free RAM during builds (another UE editor with 5.5 GB was open), so UBT
  falls back to 1 compile action; builds still take ~1 min.
* CPU players are simple (pick one of the 3 best orb/rival shots, aim error from Control stat).
* Sound has not been listened to on the PC (all automated runs use -NoSound so nothing played while
  Evan was working). Music volume is 0.35; tune in `AZoogiGameMode::PlayMusic`.
* Power balance: second pass from 7 four-CPU power-test matches (`FinalScores:` log line); still a small sample.
  Radii/speeds at the top of `ZoogiPowers.cpp`.
* Island edge collision is the engine cylinder's convex hull (a many-sided polygon close to r=15.5).
