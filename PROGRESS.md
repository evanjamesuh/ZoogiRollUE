# Zoogi Roll â€” Unreal Engine 5.6 rebuild: progress log

Project: `C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject` (C++ module `ZoogiRoll`, Chaos physics).
The web game at `C:\dev\ZoogiRollUE` is read-only reference and is never modified.

## Frozen Ring polish round 3c: Tue Oct 6, 11:39 PM CT (merge ca9baa5, code 3bba5c7)
Packaged Development Win64 at **11:39 PM CT**, giving `Packaged\Windows\ZoogiRoll.exe` (exe written 11:39:37 PM CT, ~334 MB). Desktop **ZoogiRoll** / **ZoogiRoll - Shortcut** retargeted to the new exe. Local git only, never pushed. Editor DLL rebuilt 11:36 PM CT. **No Aura** in code paths (SM_AuraFrz_* unused; generated assets left on disk under Content/AuraModelGen and .Aura/).

### Look / stage
* Open curved wooden bleachers, **no roof**, 6 stepped rows, snow caps, low side/back walls, facade windows, lamp posts. Split into 3 breakable `stand` sections (HP 160) with stepped `ZStand` colliders. One snow pine beside each stand.
* Bigger elliptic islets (r 640 x stretch 1.4), walkable floor colliders, snow lip rails (`islet_rail`, HP 90) open at the bridge landing (+/-14 deg).
* Rollable wood bridges with deck + rail colliders (`ZBridge`/`ZRail`, Grow forming).
* Arena Orbs on each bleacher islet apron (`ExtraOrbSpots`; classic orb count 18 -> **22**).
* Layered jagged mountain ridges; brighter floor rune glow (2.5); sun light-shaft bloom (sun rays). Aurora panorama kept from r2.
* **Crater slab hook** (`AZoogiFloorScars::SpawnCraterSlabs(Loc, Normal, Power, Mat)`): placeholder tilted ice-slab ring on CRATER; ready for A&P CraterSlab data.

### Gameplay / damage (halved thresholds)
* HitSpeedMin 500->**250**, HitDamagePerSpeed 1/42->**1/21** (ArenaPiece). Rock CrackSpeed 900->**450**, ShatterSpeed 1600->**800**. Pillars remain unbreakable.
* Worked example icewall HP 120: old Damaged ~1357 cm/s, Cracked ~3272; new Damaged ~679, Cracked ~1636. A normal shot ~1980 cm/s now deals ~88 (cracks a wall / damages a stand).
* `-ZoogiFrozenPlayTest` harness (wall / stand / bridge / lip / ring-out). Show run: 52 shots.

### Aura note
* Generated Aura assets left unused (do not delete). Earlier ~10:39-10:44 PM package from another agent may have cooked r3b Aura code/assets; **this** package is Aura-free in code.
* Reverted Aura swap to daec51c-level procedural kit (not literal 32bdc94) then rebuilt as r3c; FloorScars SetScar rate-limit fix kept.

### Shots (agent box)
* Side-by-sides: `/workspace/zoogi-unreal/shots/arenas/frozen-buzzy/r3c_sbs_{hero,threequarter,topdown,hub}.jpg`
* Game only: `r3c_hero.jpg`, `r3c_threequarter.jpg`, `r3c_topdown.jpg`, `r3c_hub.jpg`, `r3c_stands_close_{W,E}.jpg`, `r3c_stands_mid_SE.jpg`, `r3c_stands_side_W.jpg`
* Prior review: `r3_stands_before_*.jpg`, `r3_*.jpg`

## Bible characters + phase expressions: Tue Oct 6, packaged 9:48 PM CT (commit bbd99b6)
Packaged Development Win64 `Packaged\Windows\ZoogiRoll.exe` (exe written 9:48 PM CT). Packaged smoke test (picks 4-7) passed: Sera rune seal fired and the match reached game over. Local git only, never pushed. One leftover: in the 9:48 exe Master Orin's shell has the darker 0.6 granite tint. The lighter 0.9 tint (`MI_MasterOrin_Shell`) is imported and committed and will be in the next package.
The 8 Buzzy "Master Asset Bible" characters replace the old 7 (Wolfgang, Hotstreak, Lars, Pinpoint, Bolt, Wraps, Nightshade retired). Roster order is also the menu order and the `-ZoogiPicks` indices: 0 Blue, 1 Tavi, 2 Vex, 3 Moss, 4 Master Orin, 5 Sera, 6 Luma, 7 Dark Zoogy. Collision radius, physics, the mass formula and the turn/phase logic are unchanged. The 2v2 Combo needed no change because it is roster-agnostic. Full details are in `Docs/BIBLE_CHARACTERS.md`.
* **Powers follow personality, and stats/mass come with the power.** Vex: Rival Blast (Explosion, from Hotstreak). Master Orin: Granite Bind (Bandage Bind, from Wraps; heaviest). Luma: Hype Shock (Static Shock, from Bolt). Dark Zoogy: Shadow Stun (from Nightshade). Moss: Tactical Lock (Lock-On, from Pinpoint). Blue: Resonance Rebound (Ricochet, from Lars). Tavi: Friend Squad (Wolf Pack, amber echo clones of Tavi). **Sera: Rune Seal**, a new small variant: Shadow Stun's 460 cm pulse applies Bandage Bind's curse (rivals' next 2 shots are weak) with no freeze. Artifact affinity is derived from the power.
* **Models:** `/Game/Characters/Bible/<Asset>/SM_<Asset>` + `SM_<Asset>_Nubs` + `MI_<Asset>_<Slot>`. The masters are in `/Game/Characters/Bible/Materials`. To re-import: `.\Scripts\run_python.ps1 Scripts\import_bible_characters.py import_bible` (source `SourceArt/Characters/Bible`; set env `ZOOGI_BIBLE_FORCE=1` to rebuild the masters). Portraits: `Art/Portraits/T_Portrait_<id>.png` â†’ `Scripts\import_portraits.py`.
* **Flipper nubs** are visual only and a separate mesh. To hide them everywhere, set `ShowFlipperNubs=False` under `[ZoogiRoll.Characters]` in `Config/DefaultGame.ini` (`-ZoogiNoNubs` hides them for one run).
* **Expressions** (`UZoogiExpressionComponent`, one per Zoogi) use a 12-cell eye atlas per character: neutral (blinks), focused (Aim), determined (PowerUp / Infuse / Partner Follow-Up), excited (Rolling / landed hit), dizzy (hard hit / stunned), worried (edge / incoming / bumped), ko (falling out), happy (KO credit / orb / win), sad (lose / bound), guard (Awaken Guard / braced) and flourish (Draw, Ally Bump, idle personality beat).
  * Each character also gets nub motion, core glow and small hops/wobbles. Dark Zoogy's visor optic pulses.
  * Settled Zoogis turn their face to the camera (`FaceCamera`, `FaceMaxTiltDeg` in the same ini section).
  * Log lines look like `Expr P1 Blue -> focused (aim)`. `-ZoogiExprDemo` photographs every expression.
* **In-game face legibility:** at the 50Â° camera the eye atlases are a game-only build, with thicker strokes, a light halo behind the eyes and low window glow. Colour maps are forced to sRGB/`TC_Default` because UE's normal-map auto-detection had turned Blue's atlas into BC5 (green/yellow eyes). Nanite is off on these meshes because it caused holes in the decals. Orin's granite gets a warm `Tint`, and Dark Zoogy's optic slits are bigger and brighter.
* **Verified (editor runs):** every character's 12 expressions were captured via `-ZoogiExprDemo`. Expressions change by phase in matches and in 2v2 Combo. With `-ZoogiClassicPowers -ZoogiPowerTest` these powers fired: Blue ricochet, Tavi wolf pack, Vex explosion, Moss Tactical Lock, Sera rune seal, Dark Zoogy shadow stun. Orin Granite Bind and Luma Hype Shock armed and fired but didn't land a hit in that run. Note that the default artifact mode never uses character powers.
* Shots on the agent box: `/workspace/zoogi-unreal/characters-bible/{compare_all.jpg,expr_contact.jpg,expr_contact_ingame.jpg}` and `/workspace/zoogi-unreal/shots/bible/`.

## Frozen Ring polish round 2: Tue Oct 6, 9:45 PM CT (merge 39f4ef4, code 32bdc94)
Packaged Development Win64 at **9:41â€“9:43 PM CT**, giving `Packaged\Windows\ZoogiRoll.exe` (exe written 9:42 PM CT, 334 MB). The editor DLL was rebuilt at 9:31 PM CT. Local git only, never pushed. A packaged smoke run (`-ZoogiCombo -ZoogiArena=1`) confirmed that forming completes in 2.6 s (colliders on) and the Recovery Gate returns players. Classic Frozen ran to game over with a ring-out crash on a bridge.
* Look: the aurora and rainbow sky panorama is rebuilt from the Buzzy hero image with feathered seams. The snow rim is raised and lumpy. A bright cyan boundary ring and the hub rune circles and spokes are added. Cliffs and the underside are paler, and the mountains sit further out and lower, fading into a pale haze (fog starts at 90 m so the court stays crisp).
* Forming: the 4 hub pillars now break through the floor at 1.3/1.4/1.5/1.6 s with a +14 cm overshoot and are settled by 2.6 s, following the timing in `reskin-incoming/02-frozen-ring/animations/forming/forming_spec.md`.
* Destruction: floor craters are now art-scale (charcoal crater plus a cyan crack web, no bloom blow-out). The black 2v2 hex-section slab when damaged or cracked is fixed. A ring-out with no piece in its flight cone now damages the nearest rim piece.
* Shots (`-ZoogiDamageShots`): new Buzzy-framed overview, HUD hidden, exposure settles per view.
* Note: the working tree also carries another agent's uncommitted Bible-characters work (ZoogiTypes/Powers/Ball/Expression, portraits, DefaultGame.ini). This package includes it, because it compiles from the shared tree.
* Side-by-sides: `/workspace/zoogi-unreal/shots/arenas/frozen-buzzy/r2_*.jpg` on the agent box.

## Frozen Ring rebuilt from the Buzzy art (round 1): Tue Oct 6, 8:45 PM CT (merge 5bd6ea3, code 4386d51)
Packaged Development Win64 at **8:40â€“8:43 PM CT**, giving `Packaged\Windows\ZoogiRoll.exe` (exe written 8:41 PM CT, 334 MB). The editor DLL was rebuilt at 8:38 PM CT. Branch `frozen-buzzy` was merged into `main` locally and never pushed. A packaged smoke run (`-ZoogiCombo -ZoogiArena=1`) confirmed that Frozen 2v2 forms the stage (84 pieces in 2.6 s, colliders switch on once each piece settles) and that the Recovery Gate returns players. A Meadow regression smoke run is unchanged.

**What's new on FROZEN RING** (menu ARENA â†’ Frozen Ring, Classic or 2v2 COMBO)
* Rebuilt to match Evan's Buzzy Frozen Ring art. Textures are derived from the Buzzy images (`Scripts/arenas/gen_frozen.py` â†’ `T_Frozen*`, `M_FrozenPiece`, `M_FrozenDecal`). The scene has a glowing icy court with cyan Resonance runes, a snowy rim with ice blocks, curved bank walls and arches, four snow-roof pavilion stands on floating islets with bridges, frozen cliffs with icicles, mountains, an aurora sky, and the hub of 4 ice-stone pillars with ember/frost/storm/titan orbs on a rune floor.
* **Stage forming intro** (`UZoogiStageForming`, reusable): the arena assembles in about 2.6 s at match start. SKIP with click, Space, Enter or Esc. The 4 hub pillars rise out of the floor. No piece has collision until it has settled.
* **Destruction everywhere except the pillars** (`AZoogiArenaPiece`): walls, arches, stands, bridges, rim blocks, ramp, platform, bumpers and rocks all go intact â†’ damaged â†’ cracked â†’ broken. Damage comes from impacts, the power hook (Ember/Frost/Storm/Titan, extensible) and ring-out crashes. The floor gets scars (cracks â†’ cyan craters). The 4 pillars and their orbs are unbreakable. Fairness: broken pieces never block a path or trap a Zoogi, and the floor collider stays intact.
* 2v2 COMBO on Frozen: bank wall, ramp and hex platform, 3 ice-crystal bumpers, a damageable hex-ice section, the Recovery Gate, and Arena Orb sockets.
* Debug flags: `-ZoogiFormShots`, `-ZoogiDamageShots`, `-ZoogiNoForm`, `-ZoogiFormSlow=N`, `-ZoogiShowcaseWait=N`.
* Side-by-sides (Buzzy on the left, game on the right): `/workspace/zoogi-unreal/shots/arenas/frozen-buzzy/` on the agent box.

## Latest 2v2 build â€” Tue Oct 6, 7:02 PM CT (commit cc39942)
Packaged Development Win64 at **7:00â€“7:01 PM CT** â†’ `Packaged\Windows\ZoogiRoll.exe` (bootstrap) + `ZoogiRoll\Binaries\Win64\ZoogiRoll.exe` (349 MB). Editor DLL `Binaries\Win64\UnrealEditor-ZoogiRoll.dll` is current (6:59 PM CT). Desktop **ZoogiRoll** / **ZoogiRoll - Shortcut** already point at the packaged exe; **Open Zoogi Roll in Unreal** opens the `.uproject`. Local git only â€” never pushed. Packaged smoke (`-ZoogiCombo`) confirmed Mode: 2v2 COMBO + Meadow props.

**What's in this build (PDF Part 4 complete)**
* 2v2 Competitive Combo on Meadow: team turns DRAWâ†’AWAKENâ†’BATTLEâ†’RESOLVEâ†’END, CONTROL/DRIVE/LAUNCH (no flick), Ally Bump, Combo Gauge + Partner Follow-Up (cost 30), Guard Reaction / Guard Break, Ring Marks + Recovery Gate, neutral Arena Orbs (ORB OUT â†’ Resonance / Arena Control â†’ respawn; Orb Relay + RELAY FINISH), Resonance Shift (bumpers swell at 50 control), damageable floor NORMALâ†’DAMAGEDâ†’CRACKED.
* Menu **MODE: CLASSIC | 2v2 COMBO**. Roster-agnostic (`ZoogiUtil::Roster()` / `Def.Name` only â€” no hardcoded character names in 2v2 logic/UI).
* Verified demos: DoD (bankâ†’Ally Bumpâ†’Follow-Upâ†’ring-outâ†’Recovery Gateâ†’Guard Reactionâ†’KO), Orb Relay + Resonance Shift, reactive terrain, CPU smoke. Shots: `/workspace/zoogi-unreal/shots/combo2v2/`.

**Try it now**
1. Double-click Desktop **ZoogiRoll** (packaged), or **Open Zoogi Roll in Unreal** â†’ Play.
2. Menu â†’ **2v2 COMBO** â†’ START MATCH (or `-ZoogiCombo`).
3. Keys: Awaken I/G/S Â· Battle Q switch Â· 1 CONTROL / 2 DRIVE / 3 LAUNCH Â· A/D cut Â· SPACE confirm Â· Follow-Up F / X Â· C camera.

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

## 2v2 Competitive Combo mode (Oct 6, Part 4 of Evan's development package)
Classic is unchanged and still the default. **Menu -> MODE: 2v2 COMBO -> START MATCH** (or `-ZoogiCombo` on the command line).
TEAM A = P1 + P2, TEAM B = P3 + P4 (any four Zoogis from the roster; team slots read `ZoogiUtil::Roster()` - no
character names in the 2v2 code, so the new roster drops in). A team with one human is played entirely by that human.
Code: `ZoogiCombo.cpp` (rules, ring-outs, resolve, guard reactions, demo director), `ZoogiComboArena.cpp/.h`
(prototype props), hooks in GameMode / Targeting / Awaken / Powers / Ball / HUD / AwakenWidget / MenuWidget / PlayerController.

**How to play (team turn = DRAW -> AWAKEN -> BATTLE -> RESOLVE -> END TURN)**
1. AWAKEN as before (pick card, I / G / S = Infuse / Guard / Save). **Q** (or the Q SWITCH button) swaps to your
   teammate; the drawn card / infusion moves with the selection. Stunned Zoogis sit the team turn out.
2. BATTLE (no flick): click an enemy / orb / teammate to lock, or click the floor to aim at a point; A/D = 2 deg cut.
   Pick the force with **1 CONTROL / 2 DRIVE / 3 LAUNCH** (or the aim-bar buttons, Up/Down cycles), **SPACE** confirms.
   CONTROL = soft, stops on the point (~4-10 m/s); DRIVE = standard (~14.5 m/s); LAUNCH = max (~27 m/s, 1.15x mass,
   breaks Guards). SPD/PWR/CTL nudge the speed (+-few %).
3. Preview is limited: initial path + first predicted collision + first major rebound (wall bank -> next contact).
4. Hitting your teammate = **ALLY BUMP** (redirect only: no damage, no artifact effect, no ring-out credit).
5. RESOLVE: if the shot earned a trigger (ALLY BUMP, BANK SETUP, AIRBORNE SETUP, ORB RELAY, GUARD BREAK, POWER SETUP,
   TERRAIN SETUP) and the team gauge has 30, **PARTNER FOLLOW-UP!** appears: **F** accept (partner shoots straight away,
   1.08x speed) or **X** skip. One per team turn. CPU partners always accept.
6. Ring Marks (two gold squares by each name): 1st ring-out -> Recovery Gate (south) returns the Zoogi at Resolve;
   2nd -> KNOCKED OUT for the round. Both teammates KO = MATCH OVER. At the round limit: most Ring Marks, then score.

**Combo Gauge** (per team, 0-100, top-right panel): bank setup +15, ALLY BUMP +25, ORB OUT +20, ORB RELAY +30,
GUARD BREAK +20, airborne interception +20, terrain setup +15, power setup +10, enemy ring-out +15, Guard Reaction
+15 (defender). Follow-up costs 30. Tuning: `ZoogiComboTune` in `ZoogiGameMode.h`.
**Guard**: a prepared Guard hit by CONTROL/DRIVE = **Guard Reaction** (knockback -40%, one edge save at the rim,
breaks an incoming follow-up, guard consumed); LAUNCH or a critical = **GUARD BREAK** (no damping, attacker trigger).
Guards last until the end of the opponent's team turn.
**Arena Orbs** are neutral and never destroyed: ORB OUT = +50 (star +100, relay finish +25) + 1 Resonance + 25 Arena
Control, respawn at home on Resolve. 3 Resonance = +1 artifact draw for the team. First team to 50 Arena Control
triggers the **Resonance Shift** (bumpers swell 70 -> 105 cm and kick 800 cm/s); every 100 control = +30 gauge.

**Prototype arena = Meadow** (`AZoogiComboArena`, spawned only in 2v2): curved ricochet wall (195-245 deg, r 11.5 m),
launch ramp (x -7.6..-5.4 m, y 2.9..5.1 m), raised platform (7.5, 9 m), 3 bumpers, Recovery Gate (south rim),
damageable floor section (x -1.5..2.5 m, y 2..6 m) NORMAL -> DAMAGED (tilts + raised edge) -> CRACKED (raised lip,
acts as a bank face / launch lip) on heavy landings / impacts.

**Verification runs** (`Scripts\run_demo.ps1 -Name comboN -Quit 220 -Rounds 6 -Extra "-ZoogiComboDemo=N"`; the demo
director plays all four Zoogis through the same calls as the HUD; shots in `Saved\Shots\comboN`, copies on the box
in `/workspace/zoogi-unreal/shots/combo2v2/`):
* Demo 1 = First Definition of Done: P1 DRIVE -> curved wall bank -> ALLY BUMP into P2 (BANK SETUP + ALLY BUMP, gauge 40)
  -> PARTNER FOLLOW-UP -> P2 hits P3 -> RING OUT (mark 1) -> Recovery Gate -> later DRIVE into P3's Guard = Guard
  Reaction -> LAUNCH -> second ring-out KO -> Resolve -> turns advance -> MATCH OVER.
* Demo 2 = Orb Relay: P1 CONTROL sends an orb to P2 (ORB RELAY +30) -> follow-up -> P2 knocks it out; more ORB OUTs ->
  Resonance Shift at 50 control, Resonance surge at 3.
* Demo 3 = Reactive Terrain: DRIVE over the ramp -> airborne -> landing DAMAGES the section; second -> CRACKED; TEAM B
  then banks off the cracked lip (changed geometry).
* CPU smoke (`-ZoogiCombo`, all CPU): CPUs pick forces, take follow-ups and finish the match.

### Controls (lock-on targeting, since 4:20 PM Oct 3 â€” the drag-back slingshot was removed)
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
- [x] **1. Project + build** â€” C++ project created, `.gitignore`, local git repo. Builds with
  `Build.bat ZoogiRollEditor Win64 Development` (0 errors). Headless editor (`UnrealEditor-Cmd
  -run=pythonscript`) opens it with 0 errors.
- [x] **2. Import** â€” Interchange glTF import via Python (`Scripts/setup_content.py`): Wolfgang,
  Hotstreak, Pinpoint, Bolt, Wraps, Lars, Nightshade, orb, 5 stylized scenery props.
  The Zoogi GLBs require `EXT_texture_webp`, which UE's glTF importer rejects, so
  `Scripts/glb_unwebp.py` rewrites copies (webp -> png) into `SourceArt/Zoogis` first (originals untouched).
  Materials: `M_ZoogiSurface`, `M_ZoogiGlow` (emissive + fresnel rim: the light-blue orbs),
  `M_ZoogiGround` (world-noise two-tone ground). Models are fitted at runtime (bounds fit to the
  1 m marble; Nightshade pivot-fit like zoogiModels.ts).
  The web Meadow arena is drawn in code (no GLB), so the UE Meadow is also built in code:
  floating grass island (radius 15.5 u), six rim boulders at the GRASS_RIM spots with cylinder
  colliders, flowers, start pads, and floating forest islets using the imported trees.
- [x] **3. Playable Meadow match** â€” everything listed above (pawns, shooting, trajectory +
  power meter, Chaos bounce/roll, orbs, knock-off scoring, settle-based turns, 2-4 hotseat,
  alpha camera, HUD with scoreboard/turn banner/popups, results screen) plus CPU opponents.
- [x] **4. Verification** â€” `Scripts/run_demo.ps1` runs the game with `-ZoogiAuto` (CPU players)
  and saves screenshots on gameplay events. `-ZoogiHumanTest` drives P1 with a scripted virtual
  mouse and keys through the real lock-on input path (soft lock, Tab, switch, Space). Screenshots are
  mirrored to the box at `/workspace/zoogi-unreal/shots/`.
- [x] **4b. Packaged build** â€” `Scripts/package.ps1` (BuildCookRun, Development, ~2 min) ->
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
    synthwave skyline) â€” all 7 web arenas now selectable. Night Circuit uses arena-shape queries
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

## Milestone "Awaken": Definition of Done audit (Oct 4, 3:00 PM, checked against the code in this repo)
Checklist: `Docs/MilestoneAwaken/ASSIGNMENT-UNREAL.md`, "Definition of done". A box is ticked only when there is evidence.
Screenshot folders are on the box (`/workspace/zoogi-unreal/shots/...`).

| # | Item | Status | Evidence / what is missing |
|---|------|--------|----------------------------|
| 1 | Step 0 inventory `Docs/MilestoneAwaken/UNREAL-INVENTORY.md` | **Done (Oct 4, 4:40 PM)** | Written from the real project contents (commit dbd61dd). |
| 2 | 5 overlay MIs + 5 guard looks on all 7 Zoogis, GLB materials untouched; shots of each artifact on 2+ Zoogis | **Mostly done (Oct 4, ~5 PM)** | One procedural `M_ZoogiArtifact` (`Scripts/make_artifact_material.py`, commit ffcee15) with per-Zoogi dynamic instances via `SetOverlayMaterial` (`ZoogiArtifactComponent.cpp`), GLB materials untouched; guard = `GuardShell` sphere + mesh orbiters/plates. No `M_ZoogiArtifactOverlay` / `MI_Artifact_*` / `MI_Guard_*` assets, no MaskAmount/MPC. Shots `shots/aw1`, `aw2s` show some artifacts, not a 5x2 set. **Update (Oct 4, ~5 PM): mostly done.** `Scripts/make_artifact_instances.py` makes `/Game/Materials/Artifacts/MI_Artifact_<Key>` (from `M_ZoogiArtifact`) and `MI_Guard_<Key>` (from `M_ZoogiFX`) for all 5 artifacts; the component builds its per-Zoogi dynamic instances from them (falls back to the base material if one is missing). Titan infuse also scales the model x1.04. Still missing: MaskAmount / MPC / mask textures, and a full 5x2 shot set. |
| 3 | 20 Niagara systems (aura / trail / impact / guard x5) by script, readable on 7 arenas | **Missing** | `Content/FX` has NS_Burst x7, NS_Explosion, NS_Sparks, NS_Dust, NS_Debris only; no `make_artifact_fx.py`, no per-artifact UNiagaraComponents. |
| 4 | Squash, pulse, spin, shell-expansion hooks; no limbs | **Done (Oct 4, ~5 PM)** | `AZoogiBall::PulseCore`, `Squash` (launch stretch, impact squash), `SpinBoost`, `Wobble`, `ShellExpand` (power-up pulse), visual only, physics sphere untouched; see "Awaken checklist pass" below. Subtle at the tactical camera distance; `-ZoogiHookDemo` shows them close up. No limbs (OK). |
| 5 | Awaken UMG with mouse (Infuse / Guard / Save), no click-through to world picking | **Done (Oct 4, 3:20 PM)** | Was Canvas-drawn (panel backgrounds could click through). Now `UZoogiAwakenWidget` (UMG), see the section below; `-ZoogiUiTest` log + `shots/umg/`. Not yet tried by a person with a real mouse. |
| 6 | Tactical camera default + small emphasis + Cinematic toggle | **Done** | `PTactical` profile (`ZoogiPlayerController.cpp`), `bCinematicCam`, CAM button / C key / `-ZoogiCinematic` (commit 1794c83); visible in `shots/aw1`, `aw5`. |
| 7 | Physics for 5 Infuse + 5 Guard, driven by the DataAssets | **Done (Oct 4, ~5 PM)** | All 10 effects implemented (`ZoogiAwaken.cpp`, `ZoogiArtifacts.cpp`, commit 8eb941b) and seen in logs (powered hits for all 5, guards). Data is a C++ default table with an optional `/Game/Data/DT_Artifacts` DataTable that does not exist; no DataAssets. **Update (Oct 4, ~5 PM): done.** Values now come from `/Game/Data/Artifacts/DA_Artifact_<Key>` DataAssets (`UZoogiArtifactData`, made by `Scripts/make_artifact_data.py`); DataTable and C++ table are fallbacks. A power test with DataAssets vs. `-ZoogiBuiltInArtifacts` gave identical rows (5/5, struct compare) and the same behaviour. |
| 8 | Anime: real-time Micro `Anime_Any_Ember_Infuse_01`, Level Sequence Standard `Anime_Hotstreak_Ember_Critical_01`, Media `Anime_Any_Any_KO_01`; preload, transition, exact restore (logged) | **Partial** | Real-time Micro cards with freeze/restore and logged drift ("Anime unfreeze: restored ... max drift 0.00 cm", commit 181492f). Media: `Anime_Any_Any_KO_01.mp4` (cut from Evan's splash video) plays as a video cut-in on KO, plus Infuse and Critical clips (see "Anime video cut-ins" below, `shots/anime/`). Still missing: there is no Level Sequence. |
| 9 | Packaged Win64 build plays all three | **Partial** | Packaged build of HEAD d00bb4d (Oct 4, 4:15 PM, `PACKAGE_EXIT=0`), smoke-tested: the real-time cards, the mp4 cut-ins (Infuse, KO, Critical, staged non-UFS from `Content/Anime`), the menu video, `-ZoogiUiTest` 43/0, rock crack/shatter, pizza (21 toppings) and the Cosmic backdrop all work. There is no Level Sequence to play. Note: `package.ps1` does not clear old staged movies, so a deleted clip stays in `Packaged/` until it is removed by hand. Repackaged Oct 4, 5:15 PM (HEAD 085421b): DataAssets, MIs and the Ember/Titan infuse clips work in the packaged game, see "Awaken checklist pass". |
| 10 | Local commits only | **Done** | Every commit local on `C:\dev\ZoogiRollUnreal`; nothing pushed. |

## Awaken checklist pass (Oct 4, 4:30-5:30 PM)
- **Anime card caption:** the debug clip-id line under the anime cut-in title is gone; `-ZoogiAnimeDebug` brings it back.
- **Zoogi hooks (visual only; the physics sphere is untouched):** `AZoogiBall::PulseCore`, `Squash(Amount, Dir)` (+ stretches
  along Dir, - compresses, roughly volume-preserving, <= 0.15 s), `SpinBoost` (extra model spin that decays), `Wobble`, `ShellExpand`
  (shell 1 -> 1.06 and back). Launch = stretch 0.08 x power + 360 deg x power spin boost; impact above 3000 impulse = squash
  0.04-0.12 + wobble; every artifact power-up pulse = shell expansion. `ResetVisualHooks()` runs on respawn/placement.
  `-ZoogiHookGain=<x>` scales them, `-ZoogiNoHooks` turns them off, `-ZoogiHookShots` logs each one with a screenshot.
  `-ZoogiHookDemo` puts a close-up camera on P1 during its first turn and takes `hookdemo_*` shots (rest, squash, stretch,
  shell expansion, spin before/after, wobble; the squash shot usually lands after its 0.075 s peak). At the tactical camera distance they are subtle on purpose; tune with the gain.
- **Artifact DataAssets:** `UZoogiArtifactData` (one `FZoogiArtifactRow` per asset). Lookup order: `/Game/Data/Artifacts/DA_Artifact_<Key>`,
  then `/Game/Data/DT_Artifacts`, then the C++ table (`-ZoogiBuiltInArtifacts` forces C++). The log has one `ArtifactRow` line per
  artifact and an `Artifacts:` summary that says where each row came from and whether it matches the C++ defaults.
  `Scripts/make_artifact_data.py` writes the 5 assets from the C++ defaults; `/Game/Data` is always cooked (`DefaultGame.ini`).
  Checked: `-ZoogiPowerTest` 4-player matches with DataAssets vs. `-ZoogiBuiltInArtifacts` gave identical rows and the same
  powered hits / infuses, 0 errors. No guard block came up in those runs (CPUs always infuse), so Guard values are only
  checked by the struct compare.
- **Artifact material instances:** `Scripts/make_artifact_instances.py` makes `/Game/Materials/Artifacts/MI_Artifact_<Key>`
  (parent `M_ZoogiArtifact`) and `MI_Guard_<Key>` (parent `M_ZoogiFX`). `UZoogiArtifactComponent` builds its dynamic instances from
  them (log: `Artifact material ...`) and falls back to the base material if one is missing. Titan infuse also scales the
  model x1.04 (`AZoogiBall::SetModelScale`).
- **Anime clips:** `Anime_Any_Ember_Infuse_01` (fire clip) and `Anime_Any_Titan_Infuse_01` (stone clip) are the old Infuse
  variants 02/03, now tied to their artifacts (the artifact-specific id is looked up first). Frost/Storm/Void use `Anime_Any_Any_Infuse_01`.
- **Packaged build (Oct 4, 5:15 PM, HEAD 085421b, `PACKAGE_EXIT=0`):** no stale staged movies (the 10 files in
  `Packaged/.../Content/Anime/Movies` match `Content/Anime/Movies`). Smoke runs of `Packaged/Windows/ZoogiRoll.exe`
  (`-ZoogiAuto -ZoogiPowerTest -ZoogiAnime=Full -ZoogiHumanTest`, Meadow and Cosmic): `Artifacts: ... 5/5 from data, 5 identical`
  (the DataAssets are cooked), `MI_Artifact_*` instances load, and the cut-ins play `Anime_Any_Ember_Infuse_01.mp4` for Ember,
  `Anime_Any_Titan_Infuse_01.mp4` for Titan, `Anime_Any_Any_Infuse_01.mp4` for Frost/Storm/Void, plus `KO_02` / `Critical_02` / `Critical_03` variants. Exit code 0.

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

## Sound brief, part 2 (Oct 4, 2:40-3:00 PM)
- **Climax, once per match:** the match-winning KO (the shooter's KO on the last turn leaves them the sole leader), or a KO
  on the next-to-last turn that takes the sole lead. Music drops out completely, `ClimaxRumble` plays (2.2 s + 0.3 s
  silence, Resolve waits for it), then a white impact frame (drawn even with Anime=Off) + `CritBoom` on the same frame, the
  KO/Combo anime card, a camera kick and the music swells back over 1.6 s. Every other hit only ducks the music ~4.4 dB.
  `-ZoogiClimaxTest` gives the first KO of any round the climax.
- **Victory crowd:** `CrowdWave1` with the win sting, `CrowdWave2` (denser) 3 s later; a draw gets only wave 1.
- **Roll loop:** one looping audio component per Zoogi (`AZoogiBall::TickRollSound`), volume/pitch follow speed, heavier
  Zoogis deeper; fades out at rest or when knocked off.
- **UI:** `UiConfirm` (Start / Rematch / Infuse / Guard / Save / aim confirm), `UiCancel` (cancel / menu / quit),
  `UiClick` (everything else). Replaces HitClick on menu actions.
- Pizza crust colour fix applied (crust puffs use SurfaceMID: GroundMID on instanced meshes fell back to grey).

## Rock realism pass (Oct 4, ~3:05 PM)
Visual-only rework of `AZoogiRock`; the hit rules are unchanged (crack >= 900 cm/s; shatter >= 1600 on a cracked rock, or any
Awakened / crit-grade hit), and so are the collider, events, SFX, popup, feed line and `ResetRocks()`.
- Boulder and side stone are procedural meshes (`ProceduralMeshComponent` plugin): a lumpy ellipsoid with a few flat
  weathered faces, seeded per rock, same footprint as before.
- **Crack:** the boulder really splits along a jagged fracture through the impact point. One half tilts ~3 deg and slides
  ~1.5 cm, so dark fracture faces show through the gap. Branching zigzag cracks (dark gap + lighter chipped edges, tapering)
  hug the surface, and a flake pops off and leaves a dented fresh-stone scar. Flake + 4 pebbles + stone grit + a slow,
  grey-brown lit dust puff. The old glowing sphere and spark FX are gone.
- **Shatter:** angular shards in three size classes (darker fresh-break faces) fly on scripted arcs. They bounce, tumble
  and slide to rest against the court plane, then big/far ones sink into the turf; small near ones stay as rubble. A heavy
  dust cloud billows, hangs and settles. Rubble = sheared stump with a rough fresh top, 9 half-buried stones and a powder stain.
- No debris has collision or a physics body (scripted motion only; one line trace per piece at first touchdown, to tell
  turf from the island edge). Everything is hidden/rebuilt by `ResetRock()`. Per rock: ~16 mesh components + 2 instanced
  grit sets + 14 dust spheres; ticks only while something moves.
- While the rock FX play, the shooter's bright speed sparks are held for 0.6 s (`SparkClock`) so the stone debris reads.
- **Pass 2 (Oct 4, ~5 PM):** the crack now radiates from the impact. One main fracture is hinged at the far side, so the gap
  is widest at the hit and closes away from it, with dark faces inside. It has 3-6 forked branches at irregular angles,
  hairlines near the hit and a crushed light powder spot, all with multi-scale noise. The dust is much denser (8+3 puffs on
  crack; 10 burst puffs + a 10-puff ground ring on shatter), lingers ~1.8-2.6 s and stays lit. The stump is a Voronoi-faceted
  jagged base (varied heights/tilts, 2 teeth, dark fresh tops, leaning fragments). Shards are convex hulls with
  cleave faces (3 main size classes + 12 small chips). Speed sparks near the rock are cut and held for 1.2 s.
  Under `-ZoogiRockTest` only, a test camera frames the rock for crack_after / shatter_after / rubble.
- Dust material: `Scripts/make_dust_material.py` -> `/Game/Materials/M_ZoogiDust` (lit translucent, soft edge, depth fade).
  Without it the rock falls back to `NS_Dust`.

## Awaken UMG controls (Oct 4, 3:00-3:50 PM)
- **`UZoogiAwakenWidget`** (`ZoogiAwakenWidget.h/.cpp`) is a C++ UMG widget (CanvasPanel root, no Blueprint asset). It replaces the
  Canvas-drawn Awaken panel and aim bar and keeps the same layout, fonts and colours.
  - Awaken panel: the cards are buttons (click to pick) with procedural gem textures, then INFUSE / GUARD / SAVE, the
    INFUSE / GUARD texts for the picked card, the rival-guards line and the key hints.
  - Aim bar: << < > >>, AUTO / SOFT / MED / FULL, NEXT, CONFIRM and CANCEL, with a live info line.
  - It is rebuilt only when its content changes.
  - Both panels are `UBorder`s that swallow mouse presses, so a click on a panel background never reaches world picking.
    `ZoogiPlayerController` also skips the world pick, hover and RMB clear-lock while the cursor is over a panel or a
    control took the press.
- `-ZoogiCanvasAwaken` brings back the old Canvas panel as a fallback.
- **`-ZoogiUiTest`** runs a scripted match with P1 as the human player:
  - Odd turns click through real Slate mouse events (move, press and release at the control's centre, after Slate's hit
    test finds that control). Even turns use keys.
  - Each turn picks a card, clicks the panel background (checks that nothing leaks into the game), clicks open viewport
    once as a control, then Infuse / Guard / Save in rotation. Then: aim-bar background click, lock, nudge >>, power FULL,
    CONFIRM.
  - Every step is logged as `UITEST PASS/FAIL`, ending with `UITEST SUMMARY`. Shots: `umg_awaken_tN`,
    `umg_awaken_hover_tN`, `umg_aim_tN`.
  - A mouse pick passes only if the card button's OnClicked actually fired, because the drawn card is pre-selected.
- **Results** (Oct 4, 3:43 PM, run `ui5`): **43 passed, 0 failed.**
  - Earlier runs had 2 failures each: a synthetic click took the press but never fired, and once fired later on another
    button. The likely cause is the test window losing OS focus to another app. The synthetic click now re-activates
    Slate first if the app is inactive, and logs `UITEST note` when it does or when mouse capture is left over.
  - Not yet tried by a person with a real mouse.

## Anime video cut-ins + menu video (Oct 4, 3:30-3:50 PM)
- `Content/Anime/Movies/` holds short clips cut from Evan's own Zoogi Roll videos, cropped below their title text and
  scaled to 1280x720. `Movies.csv` lists them, and the game reads it at start-up:
  - `Anime_Any_Any_Infuse_01.mp4` (2.0 s, map-select video)
  - `Anime_Any_Any_Critical_01.mp4` (2.0 s, main-bg video)
  - `Anime_Any_Any_KO_01.mp4` (2.5 s, splash video: wolf vs fire dragon)
  - `Menu_Background.mp4` (main-bg video, looped)
- `UZoogiAnimeSubsystem` picks a clip for each Full-length anime by trying, in order: the exact id,
  `Anime_Any_<Artifact>_<Event>`, `Anime_<Zoogi>_Any_<Event>`, then `Anime_Any_Any_<Event>`.
  - The clip plays inside the enlarged cut-in card (74% of the screen width), with a white flash on impact events.
  - It stays on screen for the clip's own length, clamped to 1.5-3 s.
  - Short plays and CPU-only matches keep the real-time card.
  - `-ZoogiNoAnimeMovies` turns cut-ins off.
  - The old test clip `Anime_Any_Ember_Infuse_01.mp4` is removed.
- The main menu plays `Menu_Background.mp4` behind the menu, cover-cropped and slightly darkened, and it pauses while
  the menu is hidden. `-ZoogiMenuArena` brings back the old live-arena backdrop.
- Seen in run `anime2` (4 players with a human slot): Infuse cut-ins for all five artifacts, a KO cut-in and a Critical
  cut-in (Pinpoint + Frost, 99%, 0.5 deg). Shots `anime_cutinN_<Event>` (first 2 per event) are in `shots/anime/`. The
  full map is in `Docs/MilestoneAwaken/ANIME-SEQUENCE-MATRIX.md`, "Video cut-ins in the build now".

## Pizza court: no crust (Oct 4, ~3:10 PM)
- The crust ring and the sauce dots are gone (commit "Pizza: remove crust and sauce dots"). The court is the bare pizza
  with 21 toppings.
- `-ZoogiPopShots` takes `popN_<kind>_a/b` screenshots 0.1 s and 0.25 s after each of the first 8 topping pops.

## Command-line switches (for testing)
`-ZoogiAuto` (all CPU, start at once), `-ZoogiPlayers=N`, `-ZoogiArena=0..6` (0 Meadow, 1 Frozen, 2 Volcanic, 3 Cosmic, 4 Arabian, 5 Tomb, 6 Night Circuit), `-ZoogiRounds=N`,
`-ZoogiShots=<dir>` (event screenshots), `-ZoogiQuit=<sec>`, `-ZoogiMenuShot`, `-ZoogiHumanTest`,
`-ZoogiCanvasMenu`, `-ZoogiCanvasAwaken`, `-ZoogiUiTest`, `-ZoogiPopShots`, `-ZoogiNoAnimeMovies`, `-ZoogiMenuArena`, `-ZoogiRockTest`, `-ZoogiClimaxTest`, `-ZoogiSfxVol=0..100`, `-ZoogiPowerTest` (3 charges each, CPUs always use powers), `-ZoogiAnimeDebug` (show the clip-id caption on anime cards), `-ZoogiHookGain=<x>` (scale the squash/spin/wobble/shell hooks, default 1), `-ZoogiNoHooks`, `-ZoogiHookShots` (log hooks + screenshots), `-ZoogiHookDemo` (close-up camera on P1 during its first turn, shots of each hook), `-ZoogiBuiltInArtifacts` (ignore the DataAssets/DataTable, use the C++ table), `-ZoogiPicks=4,5,6,0` (roster index per slot:
0 Wolfgang, 1 Hotstreak, 2 Lars, 3 Pinpoint, 4 Bolt, 5 Wraps, 6 Nightshade).

## Scripts
* `Scripts/build.ps1` â€” compile the editor target (log: `Saved/Logs/build.log`).
* `Scripts/run_python.ps1 -Script Scripts\setup_content.py` â€” headless import/material setup.
* `Scripts/run_demo.ps1` â€” scripted demo run + screenshots.
* `Scripts/glb_unwebp.py` â€” strip EXT_texture_webp from a GLB (run with Python + Pillow).
* `Scripts/make_fx.py`, `Scripts/import_audio.py` â€” FX material / audio import (headless).
* `Scripts/package.ps1` â€” packaged Win64 build into `Packaged/`.
* `Scripts/synth_sfx.py <outdir>` â€” regenerates the 13 SFX WAVs (plain Python 3 + numpy/scipy, ffmpeg for loudness);
  `Scripts/import_sfx.py` imports them; `Scripts/make_rock_fx.py` makes `NS_Dust` / `NS_Debris`.
* `Scripts/run_python.ps1 -Script Scripts\make_dust_material.py` â€” rock dust material `M_ZoogiDust` (headless, re-runnable).
* `Scripts/make_artifact_data.py`, `Scripts/make_artifact_instances.py` â€” artifact DataAssets / material instances (headless, re-runnable).

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

## Cosmic: animated "space journey" backdrop (Oct 4, 4:05 PM)
  - [x] Aura could NOT be used: Aura is installed/enabled, its MCP servers start (61 inspector + 97 editor tools listed via
        a stdio client), but every call returns "No valid session token found" and the editor log says
        `Aura Session Token not found - launching client for login` / `401 User not authenticated`. Evan has to sign in
        once in the Aura window (Aura.exe / app.tryaura.dev) with his paid/trial account; no Aura credits were used.
  - [x] Built without Aura (procedural, `Scripts/cosmic/*.py` + `Scripts/make_cosmic_backdrop.py`, assets in `/Game/Env/Cosmic`,
        cooked via the existing `/Game/Env` DirectoriesToAlwaysCook):
        * `T_CosmicNebula` 4096x2048 equirect panorama (domain-warped noise nebula in purple/blue/cyan/magenta/orange,
          ~9000 baked stars; alpha = "near wisps" mask). BC7, no mips, 8 MB.
        * `M_CosmicSky` on a 900 m sphere: sampled from the view direction (reads as infinitely far), far layer drifts
          ~0.22 deg/s, near-wisp layer ~0.43 deg/s (parallax), dimmed straight under the board (NadirDim 0.7).
        * `M_CosmicStreak`: 600 additive star-streak quads in one ISM; all motion is World Position Offset on the GPU
          (PerInstanceRandom phase/speed/tint, no tick/CPU), fade out within 28-63 m of the arena centre so nothing moves
          right behind the play area. Not Niagara (Niagara authoring needed Aura's vfx_agent); same look, cheaper.
        * Two planets (`T_CosmicPlanetA` ringed gas giant, `T_CosmicPlanetB` ice moon, `M_CosmicPlanet` translucent quads)
          on a pivot turning 0.5 deg/s (URotatingMovementComponent), below the horizon so they drift through the Tactical view.
        * Replaces the old 1500-sphere star shell (kept as fallback if the assets are missing; `-ZoogiNoSpaceBackdrop` for A/B).
  - [x] Perf A/B (1600x900 windowed, uncapped, 1200 frames each, `run_cosmic_perf_on/off.log` + Saved/Profiling/CSV):
        GPU 5.48 ms with vs 5.37 ms without (+0.1 ms); BasePass is lower (0.62 vs 0.74, the sphere stars are gone),
        translucency +0.02 ms. No new lights, shadows, volumetrics or Lumen contributors (sky/streaks/planets have
        distance-field / dynamic-indirect lighting off).
  - [x] Verified in `-game -ZoogiArena=3` runs (c1-c5): board and Zoogis read clearly against the darker backdrop; streaks
        visibly move between frames 1 s apart. Box: `shots/cosmic/cosmic_backdrop_wide.jpg`, `_t1.jpg`, `_t2.jpg`,
        `_motion_crop_t1_t2.jpg`, `_aim2.jpg`, `_gameover_lowcam.jpg`.
  - Tuning knobs: material params (M_CosmicSky Bright / NearStr / NadirDim / RateFar / RateNear; M_CosmicStreak Bright /
    Speed / ClearR / FadeW / ColA / ColB) and `ZoogiCosmic::` constants + planet placement in `AZoogiArena::AddSpaceBackdrop`.
  - Note: the packaged build needs a re-cook to pick this up (not packaged by this task).

## Aura pass: Cosmic nebula/planets + custom anime cut-ins (Oct 4, 4:16-4:45 PM)
- Aura MCP is working again (`get_headless_status` -> `editor_connected`). Aura `generate_images` made 10 stills (all in
  `Saved/generated_images`, copied to `SourceArt/Cosmic/aura/` and `SourceArt/Anime/`; SourceArt is gitignored):
  `cosmic_nebula_a/_b` (landscape), `cosmic_planet_ringed/_icemoon` (square, transparent), and `anime_critical_a/_b`,
  `anime_infuse_a/_b`, `anime_ko_a/_b` (landscape; glowing limbless marble Zoogis, no faces/animals/text).
- Cosmic (commit `4a191c3`): `Scripts/cosmic/build_aura_nebula.py` builds a 4096x2048 equirect from the two nebula stills
  (side by side, 512 px smoothstep cross-fades at both seams, mirror-extended vertically, faded to the poles, lower half dimmed,
  highlight roll-off, pole-corrected stars, alpha = the old procedural near-wisps mask), mean level about the same as the
  procedural sky. Planet A = Aura ringed gas giant (1024), planet B = Aura ice moon (512). Reimported with the usual
  `make_cosmic_backdrop.py`; no code change. The procedural PNGs are kept as `SourceArt/Cosmic/T_Cosmic*_proc.png`
  (copy them back over `T_Cosmic*.png` and rerun the script to revert). Streaks unchanged (no Niagara swap).
- Anime (commit `73b84fe`): `Anime_Any_Any_Critical_01` (2.0 s), `_Infuse_01` (2.0 s), `_KO_01` (2.5 s) in
  `Content/Anime/Movies` are now unique custom clips made only from the Aura stills (no Replit footage), rendered by
  `Scripts/anime/make_cutins.py <png dir> <out dir>` (PIL + ffmpeg, 1280x720, 24 fps, H.264 High):
  Critical = slow push + star glint -> white flash -> golden starburst with punch-out zoom, shake, shock rings;
  Infuse = rolling push into an energy spiral with converging rings -> blue flash -> tilt up a charged aura pillar with rising lines;
  KO = black/white impact frames -> marble blasted off the arena edge with horizontal speed lines and shake -> flash -> push into the
  twilight sky as it streaks away, star twinkle. Movies.csv keeps the same ids/lengths (source columns updated).
- Menu_Background.mp4 unchanged: no animal faces (checked at 1-2 fps over all 20.5 s); the floating spheres carry abstract
  swirl patterns, and the big white/teal one (~1 s, 5.5 s, 12 s) loosely reads like a mask but is not an animal.
- Tested: `-game -ZoogiArena=3 -ZoogiAnime=Full -ZoogiHumanTest -ZoogiPowerTest -ZoogiPlayers=4` (run_cosmic_aura4.log):
  Infuse cut-ins x3 and the KO cut-in played from the new mp4s; Critical did not come up in the test matches (same file format).
  Shots: box `shots/cosmic/aura_*.jpg`, `shots/anime/nodog_*.png`, contact sheets `shots/anime/contact_{Critical,Infuse,KO}.png`.

## Cracks build, packaged build, open-in-editor (Oct 6, 3:42-4:00 PM)
- Rock/crack work: every rock patch is in. `git apply --check -R` passes for `rock-real-2.patch` (commit d1ddf48) and
  `crust-fix.patch`. `rock-real.patch` (7e9c225) is superseded by pass 2. `rock2_progress.patch` = the "Pass 2" note
  already in this file. No `rock-real-3.patch` exists on the PC or on the box, so there was nothing new to apply.
  `build.ps1`: up to date, BUILD_EXIT=0.
- Verified: `run_demo.ps1 -Name rockreal3 -Quit 150 -Players 2 -Arena 0 -Rounds 3 -Extra "-ZoogiRockTest"`: 4 rocks
  CRACKED, ZoogiRock_5 SHATTERED (8 chunks) on its second hard hit. Shots in `Saved/Shots/rockreal3` (box
  `shots/cracks/rockreal3_*.png` + `contact_rockreal3.jpg`).
- Packaged: `Scripts/package.ps1` PACKAGE_EXIT=0. `Packaged/Windows/ZoogiRoll.exe` is from 3:52 PM Oct 6 (the cook was
  incremental: the Cosmic Aura nebula/planet assets from Oct 4 4:35 PM were already cooked and are in the pak).
  Smoke test: packaged `-ZoogiAuto -ZoogiArena=3` loads the Cosmic backdrop (M_CosmicSky / streaks / planets) and plays.
  Desktop "ZoogiRoll - Shortcut" (OneDrive Desktop) -> `Packaged\Windows\ZoogiRoll.exe`.
- Editor: Desktop shortcut **"Open Zoogi Roll in Unreal"** -> `UE_5.6\...\UnrealEditor.exe "C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject"`.
  `.uproject` EngineAssociation 5.6 (Version Selector association OK). Test-opened: editor up in 33 s on `/Game/Maps/Meadow`,
  no missing-module prompt, no `Error:` lines, then closed cleanly (`LogExit: Exiting.`). The Aura plugin (1.1.0) loads.
  Its "project requires update (SupportedTargetPlatforms)" nag is silenced by adding `"SupportedTargetPlatforms": ["Win64","Mac"]`
  to the Aura entry in the .uproject. Note for Evan: `Docs/OPEN_IN_UNREAL.md`.

* Meadow Arena package (Oct 7, 12:10 AM CT; combined main with ui-polish battle UI). Hand-built kit, no Aura.
  Resonance pillars x4 (unbreakable, rise at match start), stadium-seat wall (48 breakable wood segments) with 4 orb-exit gaps
  240 cm wide centred at 0/90/180/270 deg (half-gap 8.9 deg at r 1568; colliders open, no forming/debris/crater slabs in the gaps),
  2 curved ricochet walls, launch ramp, raised platform, 8 breakable rim rocks, terrace, 6 stands (25/115/145/205/230/335),
  Recovery Gate shrines, pagodas, waterfalls, 4 extra Arena Orb spots, floor scars + A&P CraterSlab kit (SM_MeadowCraterSlab_A-D,
  Cracked/Shattered/Titan tiers, gap culling; FBX imported at 0.01 scale). 48 pieces / 48 forming entries.
  -ZoogiArenaPlayTest (new generic harness, ZoogiArenaPlayTest.h): orb through each gap -> ORB OUT (4/4 PASS), seat-wall control
  shot stays in (PASS), normal Zoogi shot cracks the seat wall (PASS) and a ricochet wall (PASS).
  ui-polish merged clean (no conflicts); post-merge unity-build ambiguity (ZoogiMeadow vs ZoogiFrozen names) fixed by scoping
  the using-namespace. Shots: box shots/arenas/meadow-buzzy/ (r2_sbs_*.jpg, r2_arena_meadow_*.jpg, r2_arenatest_*.jpg).
  Still to do on Meadow: sky panorama not imported on main (T_MeadowSky), floor glow/carving weaker than Buzzy, pillars small, scars
  still use the Frozen decal set.
## Meadow r3 (Oct 7, 12:34 AM CT)
- Buzzy Meadow sky (T_MeadowSky), floor glow 2.6, pillars H340 x1.3 (unbreakable, rise at start), Meadow brown/green scar set + dark crater (no cyan), terrace cut with drop channels at 0/90/180/270 (240 cm), 6-tier stands r1950, Meadow lighting/saturation.
- Gap test 7/7 PASS. Packaged 12:34 AM CT, PACKAGE_EXIT=0, Desktop shortcut OK.

## Meadow r4 (Oct 7, 1:02 AM CT)
- Bold green floor seal (rebaked T_MeadowGlow: thick core + halo; GlowTint 1, glow 9, saturated green), layered mountain ridges (green/teal/blue, snow caps) with ridge waterfalls, fog cut back, hand-built pagoda tower + gatehouse hall on far hills (out of reach, solid scenery).
- Gap test 7/7 PASS. Packaged 1:02 AM CT, PACKAGE_EXIT=0, Desktop shortcut OK.

## Meadow r5-r7 (Oct 7, 4:31 AM CT)
- r5: ramps mapped to the Buzzy art (spiral ramp + raised disc, 2 tall quarter-pipes by the N gap, 2 curved lips at the front stand ends; breakable, smooth tangent colliders), benches, pillars at art yaws, 4 stand arcs, shrines, tower cluster, wide gatehouse, golem; removed non-art pieces; ZoogiLaunch playtest.
- r6: taller spiral ring wall + west hub wall, wider pipes, valley floor, waterfall mesas, softer hills.
- r7: floor seal redrawn to the art layout.
- Tests 12/12 PASS (4 gaps ORB OUT, control, seat-wall + bench crack, 2 pipes, 2 lips, spiral). Packaged r5 4:13, r6 4:23, r7 4:31 AM CT; PACKAGE_EXIT=0, Desktop shortcut OK.
- r8: seal glow deeper saturated green. Tests 12/12 PASS. Packaged 4:40 AM CT, PACKAGE_EXIT=0, Desktop shortcut OK.

## Meadow r9-r17 (Grok Bot, 2026-10-07)
- r9/r10: full-size spiral (r300-600), per-map orb ring (OrbRingRadiusOverride 690 cm, +7 deg; Meadow only), benches snapped, warm tan floor, saturation 1.08.
- r11-r13: sandstone cliffs/seats, pale distant ridges, terrace-level plain to r14000 with ravines at the 4 gaps, rounder golem.
- r14: plain raised to stand level (no moat/underside), shrines and golem re-seated at -40, seal glow 7.0.
- r15-r17: warm olive grass (#93A644/#A6B656), grassy gap banks, dense flowers and field rocks to r4200, pinkish stand wood (#CDA28B/#D9B29A/#A0705A), red gatehouse roof, art-matched hero/3-4 cameras.
- Comparison shots: add -ZoogiNoHUD with -ZoogiArenaShots for viewport-only captures (no UI source changes).
- Tests: 12/12 ARENATEST pass every round. Packaged r14 (06:42) and r17 (07:02).
- Open: toon/outline post-process, layered valleys + rocky mountains + more waterfalls, bigger pagoda/golem detail.

## Battle Episode Engine phase B (Grok Bot, 2026-10-08)
Spec: SPEC_battle_engine_2026-10-08 / AUDIT_2026-10-08 T1-T5. Engine only (debug overlay + BATTLE log lines). New flow OFF by default: [ZoogiRoll.Battle] Enabled=False; -ZoogiBattle=On|Off; any -ZoogiBattleTest turns it on.

| task | what | tests (Frozen Ring, -ZoogiArena=1, fixed step 60, seed 7) | commit |
|---|---|---|---|
| T1 | UZoogiBattleStateMachine (WAIT_FOR_COMMAND > ACTION_PREVIEW > [MINIGAME] > PHYSICS_RESOLVE > RESULT > EPISODE_EVENT > NEXT_TURN, validated transitions, BATTLE state lines); FAIL > SHATTER_FAILURE > NEXT_TURN (cancels follow-up offer, skips cards/scenes/holds, ends turn); MISSED... card bypass when flow on (bMissConsolationCard); human Partner Follow-Up offer timeout (FollowUpOfferTimeout=12 s) | roll, miss_no_consolation, timeout, shatter_basic (+Episode Off: same first_final) PASS; roll flow-off PASS with identical finals; miss_no_consolation flow-off keeps the old MISSED card (expected) | 51140f5 |
| T2 | Episode Energy component (gains from gameplay truth, cap 100, actor only, CSV/ini table), combos paid from energy, Awakening = Infuse or Guard only (never energy), reaction entitlement | energy_gain, combo_spend, energy_full_no_awaken, awaken_gate PASS; 2v2 CPU match flow on/off: actions 1-4 identical (action 5 differs only because the skipped MISSED card no longer consumes the shared random stream before P1's next draw) | 51140f5 |
| T3 | AZoogiAerialProp debug shapes (Sky Bumper: real impulse, rebound x2.0 about contact normal, cap 3200; Resonance Halo: pass trigger vs 15 rim spheres; Launch Rail entry box); Rail Release mini-game (EARLY/CENTER/LATE windows 0.25/0.20/0.25 s -> pitch 35 / 6 / yaw 30 + pitch -4, game time, no time dilation; F9 / CPU / test input; timeout = SHATTER_FAILURE); zone debug tags court/aerial(z>150)/recovery(r>0.92 KO); Halo Bank counts halo pass/rim; Sky Bumper counts bumper hits; OrbLine commits the contact orb. Props only spawn in tests or with [ZoogiRoll.Aerial] bAerialProps=True (default False) | halo_pass, halo_rim, sky_bumper (in 799 -> out 1598, x2.00), rail_release, rail_timeout, halo_success, halo_fail, orbline_success, orbline_fail, air_entitled, air_not_entitled, air_fail PASS; Episode Off pairs (halo_pass, sky_bumper, rail_release, orbline_success, air_not_entitled) identical finals | 708d608 |
| T4 | Ledger (zones + PROP: tags, CRC hash), fragments (defeat/ko), match-end assembler, unlocks (winner full / loser partial), slice_eye_<zoogi>_<emotion> ids | ko, unlocks, replay_order x2 (same ledger_hash b4f62faa), segment_retained PASS | 51140f5, a09a26b |
| T5 | Full -ZoogiBattleTest suite; midair_collision reworked to an EARLY rail arc into a parked target | four_player_2v2 (+Off), one_v_one, queued_effects, midair_collision (+Off: identical finals, air contact z=90) PASS. 2v2 flow ON (Balanced) finals == pure old play (-ZoogiBattle=Off -ZoogiEpisode=Off) finals in t5b. Caveat: earlier multi-turn CPU runs differed at a later card draw because artifact draws / CPU planning share global FMath::Rand with cosmetic FX; this happens with the old play too (not battle-engine physics) | a09a26b, a6f0e04 |
| default on | [ZoogiRoll.Battle] Enabled=True (all T1-T5 cases pass; scripted physics finals identical to -ZoogiEpisode=Off and -ZoogiBattle=Off runs; 2v2 flow ON == pure old play in t5b). Enabled=False or -ZoogiBattle=Off restores the old flow; bAerialProps stays False | regression on1: roll, roll@flowoff (identical finals), shatter_basic (+@off identical), timeout, four_player_2v2, sky_bumper, rail_release PASS | see git log |
| merge+package | main ff 556fb1a..b1576f1 (Evan's 22 uncommitted lines untouched, diff hash unchanged); package 09:11:44-09:15:36 CT PACKAGE_EXIT=0; packaged smoke EXIT_CODE=0 flow=ON, 10 actions, 68 BATTLE state lines, AirReaction shatter -> next player, no crash | packaged smoke | b1576f1 |
| smoke script | Scripts/smoke_packaged.ps1: screenshots only from the game's own viewport (-ZoogiEpisodeShots); no OS-level screen/window capture (a PrintWindow-on-HWND version hung against the D3D12 window and was dropped); waitclear + lock inside; log summary (flow, GAME rng seed, BATTLE/state/action counts, crash lines) | trial of the PrintWindow version hung (killed, own lock released); final version used for the packaged smoke | b06a772, e9e68d9 |
| game RNG | GameRng (FRandomStream on AZoogiGameMode, host only) for DoDraw artifact draws, CPU Awaken guard choice, CPU shot plan pick/cut/aim error; seed -ZoogiMatchSeed, else -ZoogiEpisodeSeed, else fresh per match; logs "GAME rng seed=" | four_player_2v2 x3 identical (ledger_hash d7ae6f19), one_v_one x3 identical (288b814a); 2v2 Episode Off == Balanced; 2v2 and 1v1 with flow on + -ZoogiBattleAwaken=None == -ZoogiBattle=Off finals (with Awakening reactions on, the CPU's Air Reaction BRACE is a deliberate new-flow rule and changes later turns); 2v2 -ZoogiBattle=Off == -ZoogiBattle=Off -ZoogiEpisode=Off. Full suite2: 40/40 PASS (28 cases + 8 Episode Off pairs with identical finals + replay_order x2 + roll@flowoff + 2 noreact) | see git log |
| merge+package 2 | main ff b1576f1..bf6dd32 (Evan's 22 uncommitted lines untouched, diff hash unchanged); package 10:17:02-10:21:11 CT PACKAGE_EXIT=0; packaged smoke (Scripts/smoke_packaged.ps1, viewport-only shots) EXIT_CODE=0, flow=ON, GAME rng seed fresh, 8 actions, 67 state lines, 0 crash lines, 13 viewport shots | packaged smoke | bf6dd32 |
