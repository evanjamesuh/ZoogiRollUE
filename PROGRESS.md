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
  - [x] Impact juice: the first contact of every shot gets a white flash + a ground ring in the shooter's colour,
        a short hit-stop (0.08x time for up to 0.09 s real time), camera shake and FOV punch scaled by shot speed,
        and a "SMASH!" popup on hard hits. The rolling shooter leaves a lightless ring trail in its player colour.
  - [x] Event feed under the scoreboard (newest 5, fade after ~6 s): knockouts with names and +100/-75, orb and
        gold-star knock-offs, rolled-off-the-edge, stunned/skips turn. Popups that spawn at the same spot
        (KNOCKOUT + FELL OFF) now stack instead of overprinting.
  - [x] Balance sample 1 (5:12-5:35 PM, packaged build, 20 four-CPU power-test matches, 2 rounds, `bal_*.log`):
        avg score / win share: Hotstreak 108 / 50%, Wolfgang 83 / 52%, Pinpoint 60 / 35%, Bolt 32 / 15%,
        Nightshade 27 / 18%, Lars 9 / 2%, Wraps 0 / 0% (9 matches). Causes found in the logs:
        Wraps' bind damped the victim's knock velocity by 45% (so its powered hits could never knock anyone off), and
        Lars' ricochet bounced toward the nearest target (usually centre orbs). Fixes: the bind keeps full hit force and
        the weak-shot curse lasts 2 shots; Lars ricochets toward the target easiest to knock off (close + near the edge
        on the bounce line) at the speed needed (1050-1900 cm/s); Hotstreak push 600+1400k -> 550+1300k; wolf clones
        1800 -> 1650 cm/s; the CPU's rival-hunt bias for rival powers 140 -> 100.
  - [x] HUD readability: all HUD text is drawn with a Slate font sized to the final pixel size (crisp at 1080p
        instead of upscaled bitmap glyphs), headings in Roboto Bold, ~10% larger. Player tags (P1..P4 pill in the
        player colour, with STUN / BOUND state) float over each Zoogi outside the Draw and Battle close-ups.
  - [x] KNOCKOUT! callout: big centre text in the attacker's colour with "ATTACKER > VICTIM", a shockwave ring at
        the edge and a camera kick.
  - [x] Balance sample 2 (5:38-6:10 PM, packaged, 20 four-CPU power-test matches, 3 rounds, 11-12 matches per Zoogi,
        `bal2_*.log`): avg score / win rate (25% = even): Hotstreak 144 / 50%, Pinpoint 114 / 32%, Lars 100 / 33%,
        Nightshade 100 / 27%, Bolt 71 / 23%, Wolfgang 64 / 5%, Wraps 10 / 4%. The Lars fix worked (9 -> 100 avg).
        Power use across the sample: 9 explosions (avg 2.9 pushed), 17 shadow stuns, 11 wolf packs,
        22 ricochets from 15 powered Lars shots, 4 binds from 16 powered Wraps shots, 6 Bolt stuns from 13; 39 knockouts vs 7 self-falls.
  - [x] Balance pass 3: Hotstreak explosion radius 390 -> 360; Wraps' bind also yanks the victim 25% harder (the CPU's
        knock-off maths accounts for it; menu text updated: "Curse enemies on contact: they take the hit 25% harder and
        their next 2 shots are weak"); wolf clones 1650 -> 1750 cm/s (sample-2 Wolfgang slump is partly noise).
        Sample 3 (`bal3_*.log`, same schedule) started 6:14 PM on the packaged build to check it.
  - [x] Results screen: dimmed backdrop, place numbers, Zoogi portraits, WINNER badge, colour-coded rows.
  - [x] Packaged exe rebuilt 6:12 PM (everything above, including balance pass 3). Smoke-tested: a full
        packaged 4-CPU Night Circuit match ran to the results screen with cooked portraits (`shots/pkg5`).

## Milestone "Awaken" (Oct 4, playable)
Plan and assignments: `Docs/MilestoneAwaken/` (REVIEW, MILESTONE, ASSIGNMENT-*, ANIME-SEQUENCE-MATRIX). Decisions approved by Evan Oct 4:
one Zoogi per player, no HP, deterministic crit, Guard = next opponent attack only, affinity map, Tactical camera default.
  - [x] Turn phase machine `Draw -> Awaken -> Battle (Aim / PowerUp / Rolling) -> Resolve -> End` (`SetStage` / `SetBattleStep`,
        `ZoogiTargeting.cpp`). Scores are recorded in a ledger during the roll and committed in Resolve.
  - [x] Artifacts: `ZoogiArtifacts.h` (DataTable row struct, built-in defaults, optional `/Game/Data/DT_Artifacts` override),
        `UZoogiArtifactComponent` (hand of 2, drawn, infused, guard, statuses, transformation visuals).
  - [x] Awaken: Infuse / Guard / Save (Canvas HUD cards + buttons, keys I / G / S, CPU logic in `PlanCPUAwaken`).
  - [x] Powered collisions (`ZoogiAwaken.cpp`): Ember burst + Burned, Storm chain + Shocked, Frost ice-slide + Frozen,
        Titan mass + shockwave, Void gravity well. Crit = contact speed >= 90% of the launch speed and cut <= 5 deg (x1.3; tightened Oct 4 from 75% / 8 deg).
        Affinity Infuse also fires the Zoogi's signature power. Guard: 40% less knockback + per-artifact attacker effect.
  - [x] Clickable angled aiming: click a target (lock) or the floor (free aim), cut nudges (A/D, arrows, HUD buttons),
        power steps, ghost-ball preview with target / shooter departure arrows and one bank bounce.
  - [x] Tactical camera default (`PTactical`), small emphasis, hit-stop only on powered hits; `-ZoogiCinematic` / CAM button
        for the Oct 3 per-phase cameras.
  - [x] `UZoogiAnimeSubsystem`: combat events -> sequence id via the matrix fallback chain (`Content/Anime/AnimeSequences.csv`),
        Full / Short / skip rules, world freeze + snapshot/restore for impact moments, placeholder overlay card, optional
        `Content/Anime/Movies/<id>.mp4` playback (test clip: `Anime_Any_Ember_Infuse_01`).
  - [x] Compiled on the PC Oct 4 12:34 PM (3 small fixes); `M_ZoogiArtifact` generated by `Scripts/make_artifact_material.py`.
  - [x] Verified Oct 4 12:36-12:45 PM: full loop runs to the results screen in 2-player (CPU power test, Volcanic), 2-player
        human-input sim (Meadow / Frozen) and 4-player CPU (Frozen) matches. Seen in logs: Infuse / Guard / Save, guard expiry,
        powered hits for all 5 artifacts, crits, affinity powers (Hotstreak + Ember explosion), KOs credited in Resolve,
        Combo / KO / Miss anime events, world freeze restore with 0.00 cm drift, mp4 test clip playback.
        Screenshots: `shots/aw1`, `shots/aw2s` (box).
  - [x] Playtest fixes: camera stays on the floor after the shooter falls, Awaken / Aim framing keeps the shooter above
        the HUD panels, CPU infuses once its hand has a card.
  - [x] Oct 4 1:00 PM: crits tightened (90% / 5 deg); CPUs play about half their straight shots with a 6-20 deg cut and pick cut
        plans more often; Hotstreak nerf (explosion radius 360 -> 310, push 550+1300k -> 470+1100k, Ember burst x0.55 when his
        affinity explosion also fires). NEXT (target) button in the aim bar, SKIP button on anime overlays.
  - [x] Polish: Awaken panel shows empty hand slots, larger texts and the affinity line; Resolve panel moved to the lower third
        with powered-hit / crit / KO / combo / guard notes; CPUs guard more (protecting a lead / playing safe); guard anime
        framed from outside the bubble.
  - [x] Oct 4 1:15-1:30 PM: Awaken panel key hints ([1]-[3], I / G / S), "GUARDED:" line listing rivals with a guard up,
        hand-full warning ("saving X discards Y") and a DISCARDED popup / feed line when SAVE pushes out the oldest card.
        `-ZoogiHumanTest` now cycles Save x3 / Guard (saved card) / Infuse and screenshots every human Awaken.
  - [x] Balance sample 4 (20 matches, artifact rules, 1:04 PM package): Hotstreak 46%, Wolfgang 41%, Wraps 29%, Nightshade 18%,
        Bolt 18%, Pinpoint 14%, Lars 8%. Hotstreak won by rarely being knocked off (lowest points lost) -> Defense 70 -> 62
        (mass 1.17 -> 1.03).
  - [x] Night Circuit CPU planning: rails count as "no edge" (only the corner mouths knock off) and pads / posts in the shot
        path or the target's exit path make a plan expensive (CPUs used to plan through them; whole matches ended 0-0).
  - [x] Balance sample 5 (28 matches, Defense 62 only): Pinpoint 49%, Hotstreak 46%, Lars 32%, Wolfgang 17%, Nightshade 13%,
        Wraps 12%, Bolt 7%. Points per turn showed Hotstreak's Ember-affinity turns at 91 (highest), Bolt's affinity turns at 25
        (half his plain infused turns) and Nightshade's at -1 -> Ember affinity burst x0.3, Bolt affinity stuns on impact,
        Nightshade's stun no longer freezes the knocked target (artifact rules).
  - [x] Balance sample 6 (28 matches, 1:51 PM package): Nightshade 49%, Lars 38%, Wolfgang 33%, Hotstreak 16%, Pinpoint 15%,
        Bolt 14%, Wraps 12% (3 all-zero matches excluded, none on Night Circuit any more). Affinity turns now: Hotstreak 56 pts
        (was 91), Bolt 54 (was 25); Nightshade affinity still ~0. With 12-14 games per Zoogi the noise is about +-25%, so the
        leader changes every sample (Hotstreak -> Pinpoint -> Nightshade); no further change made.
  New switches: `-ZoogiClassicPowers` (Oct 3 rules), `-ZoogiCinematic`, `-ZoogiAnime=Off|Short|Full` (auto runs without
  `-ZoogiShots` default to Off).

## Destructible rocks + anime SFX (Oct 4, 2:00-2:45 PM)
- **`AZoogiRock`** (`ZoogiRock.h/.cpp`, reusable by any arena via `InitRock(radius, height, yaw, colours, physmat, seed)`):
  C++ fallback, not Chaos Geometry Collection (no pre-fractured asset pipeline headless; Chaos fracture also would make the
  outcome depend on debris physics). Solid invisible cylinder collider + squashed-sphere visual + side stone.
  - Contact speed = the ball's approach velocity (sampled before the physics step) along the contact normal.
  - >= 900 cm/s (full launch 3600): **crack** (darker, shrinks 5%, side stone breaks into 3 chips, 4 crack lines, debris FX,
    `RockCrack`). A cracked rock hit again >= 1600 cm/s, or any **strong** hit (active shooter Awakened/Infused, or a
    crit-grade hit: >= 90% of its launch speed, <= 5 deg cut, >= 1600 cm/s) **shatters**: collider off, 8 chunks on scripted
    ballistic arcs (no physics bodies, floor bounce, shrink-fade, gone by 3 s), 4 flat rubble pebbles without collision,
    dust ring + `NS_Dust` + `NS_Debris`, `RockShatter`, light camera kick (`OnImpact(0.35)`), popup + feed line.
  - Deterministic: debris is cosmetic and seeded; nothing it does touches the marbles, so the result is settled by the
    marble physics before any anime sequence plays. 11 small mesh parts per rock, ticks only while chunks fly.
  - Meadow rim boulders are rocks now (+2 new ones at 111 / 291 deg, rim only, clear of the shot lanes). `Arena->ResetRocks()`
    at match start restores them. Note: a shattered rim rock no longer stops a marble at that edge.
  - `-ZoogiRockTest`: CPUs always SAVE and shoot the nearest standing rock (60% power if intact, 97% if cracked).
- **SFX**: 13 original sounds made by `Scripts/synth_sfx.py` (numpy/scipy, seeded; list + recipes in
  `Content/Audio/SFX/CREDITS.md`). Nothing sampled; the Naruto/Boruto refs were only used for timing/style (movie team's
  measurements + `TEAM-NOTES-fight-and-victory.md`). About -16 LUFS max momentary, true peak <= -1 dBTP. Imported as
  `/Game/Audio/S_<Name>` by `Scripts/import_sfx.py` (a harmless "BINKA decoder" ensure in the commandlet makes PY_EXIT=1;
  all 13 assets import and save).
  - Wiring: launch = Inhale wind-up (ends with a 0.11 s held-pose beat) + Dash whoosh; Zoogi-vs-Zoogi hard hit = HitCrack
    (orbs keep Clack); crit/KO/combo = CritBoom on the impact frame + ~4.4 dB music duck; Infuse = AwakenCharge + artifact
    stinger; Guard / guard block = GuardClang; powered hit = artifact stinger (Ember / Storm / Frost / Titan / Void); rocks =
    RockCrack / RockShatter. Played from `UZoogiAnimeSubsystem::PlayEventSfx` (also with Anime=Off) as UI sounds so they
    play through anime freezes.
  - SFX volume: new SFX -/+ row in the menu (steps of 25%), `-ZoogiSfxVol=0..100`.
- Pizza court (teammate's patch, applied as its own commit): Arabian Nights is now a pizza with 21 pepperoni/cheese
  toppings that pop when a Zoogi rolls over them (no collision; reset each match). Tomato planters unchanged.

## Command-line switches (for testing)
`-ZoogiAuto` (all CPU, start at once), `-ZoogiPlayers=N`, `-ZoogiArena=0..6` (0 Meadow, 1 Frozen, 2 Volcanic, 3 Cosmic, 4 Arabian, 5 Tomb, 6 Night Circuit), `-ZoogiRounds=N`,
`-ZoogiShots=<dir>` (event screenshots), `-ZoogiQuit=<sec>`, `-ZoogiMenuShot`, `-ZoogiHumanTest`,
`-ZoogiCanvasMenu`, `-ZoogiRockTest`, `-ZoogiSfxVol=0..100`, `-ZoogiPowerTest` (3 charges each, CPUs always use powers), `-ZoogiPicks=4,5,6,0` (roster index per slot:
0 Wolfgang, 1 Hotstreak, 2 Lars, 3 Pinpoint, 4 Bolt, 5 Wraps, 6 Nightshade).

## Scripts
* `Scripts/build.ps1` — compile the editor target (log: `Saved/Logs/build.log`).
* `Scripts/run_python.ps1 -Script Scripts\setup_content.py` — headless import/material setup.
* `Scripts/run_demo.ps1` — scripted demo run + screenshots.
* `Scripts/glb_unwebp.py` — strip EXT_texture_webp from a GLB (run with Python + Pillow).
* `Scripts/make_fx.py`, `Scripts/import_audio.py` — FX material / audio import (headless).
* `Scripts/package.ps1` — packaged Win64 build into `Packaged/`.
* `Scripts/synth_sfx.py <outdir>` — regenerates the 13 SFX WAVs (plain Python 3 + numpy/scipy, ffmpeg for loudness);
  `Scripts/import_sfx.py` imports them; `Scripts/make_rock_fx.py` makes `NS_Dust` / `NS_Debris`.

## Known issues / notes
* Real-mouse play has not been exercised by a person yet (the scripted virtual mouse uses the
  same code path). Please try a match and report how targeting and the phase cameras feel (`ZoogiCamTune`).
* The PC had ~300 MB free RAM during builds (another UE editor with 5.5 GB was open), so UBT
  falls back to 1 compile action; builds still take ~1 min.
* CPU players are simple (pick one of the 3 best orb/rival shots, aim error from Control stat).
* Sound has not been listened to on the PC (all automated runs use -NoSound so nothing played while
  Evan was working), including the new anime SFX; levels were set by measurement only. Music volume is 0.35; tune in `AZoogiGameMode::PlayMusic`.
* Power balance: tuned from two 20-match all-CPU samples (`FinalScores:` log line); CPU-only data, and 2-3 shots per
  Zoogi per match make single matches noisy. Wraps is still the weakest; Hotstreak the strongest.
  Radii/speeds at the top of `ZoogiPowers.cpp`.
* Island edge collision is the engine cylinder's convex hull (a many-sided polygon close to r=15.5).
