# Zoogi Roll (Unreal): Milestone "Awaken" code review

Written by Grok Bot for Evan on Sun Oct 4, 2026 (CT). This round is review only: no game code was changed.
Project: `C:\dev\ZoogiRollUnreal` (UE 5.6, C++ module `ZoogiRoll`, source in `Source/ZoogiRoll/{Public,Private}`, about 5,300 lines of .cpp).
This plan builds on the existing project. **Do not restart it.**

Naming: the spec's "Zoogy_Flare" means **Hotstreak** (Evan confirmed), so its example id is `Anime_Hotstreak_Ember_Critical_01`. All examples use the real roster: Wolfgang, Hotstreak, Lars, Pinpoint, Bolt, Wraps and Nightshade.

---

## 0. What landed in git (state at last PC contact)

These are the commits on top of `3c3941c`, oldest first:

| Commit | What it did |
|---|---|
| `a62e1a4` | Balance pass from sample 1 |
| `b8f47d7` | HUD readability (Slate-sized Roboto text, player name tags) |
| `9c10618` | KNOCKOUT! callout |
| `14fe3c1` | PROGRESS update |
| `08c78c6` | Results screen with portraits |
| `70c3f41` | Balance pass 3 (Hotstreak blast radius 360, Wraps bind yank 1.25x, wolf clones 1750) |
| `9d61cb9` | PROGRESS update (samples 1-2) |
| `9b671c1` | **Two-beat Battle camera for long locks** (target close-up, then whip back) + **bigger pulsing WINNER badge** |

So balance, the long-shot camera and the WINNER badge **are committed**.

Two other pieces are **not** in git and not on the PC:
- The Niagara **sparkle trail** on rolling Zoogis (`AZoogiBall::Tick`, `SparkClock`).
- The **orb fly-to-scoreboard** FX (`FZoogiFlyer`, `AZoogiHUD::DrawFlyers`, row flash).

Both exist only as box-side edits that were never built. They were left alone this round.

Other notes:
- The only uncommitted change on the PC was `Config/DefaultEngine.ini`, where the editor auto-added an AndroidFileServer section.
- The packaged exe (built Oct 3, 6:12 PM CT) has balance pass 3 but not the two-beat camera or the badge.
- The PC went offline at about 11:20 AM CT today, before I could re-run `git log`. Re-check with `git log --oneline -15` before starting.

---

## 1. Which classes control phases?

All turn flow lives in **`AZoogiGameMode`**. It is one class split across three files:

- **`Public/ZoogiGameMode.h`**
  - `enum class EZoogiPhase { Menu, Aiming, Rolling, GameOver }` is the coarse match state.
  - `enum class EZoogiStage { Draw, Standby, Main, Battle, End }` is the presentation stage inside a turn. Each stage drives a camera profile.
  - `namespace ZoogiStageTune` holds the stage timings: DrawHuman 1.4 s, StandbyMin/Max 0.9/2.6 s, BattleSwing 0.85 s, LongLockDist 1100, TargetBeat 0.55 s, LockImpactSpeed 650.
- **`Private/ZoogiGameMode.cpp`**
  - `StartMatch` and `BeginTurn` → `EndTurn` handle respawning knocked-out Zoogis, the orbs-left and round-limit check, and skipping stunned turns. `FinishGame` writes the FinalScores log line.
  - `Tick`, in the **Rolling** phase, runs `TickPowers`, `CPUPowerTick` and `CheckKnockoffs`. Once `AllAtRest()` has held for `SettleDelay` (1.5 s) it calls `EndTurn()`, with a 25 s safety net. **This settle-then-advance block is the de-facto Resolve step today**, but it has no state of its own.
  - `MenuAction(FName, int32)` is the single UI action dispatcher.
- **`Private/ZoogiTargeting.cpp`**
  - `SetStage`, `TickStages`, `StageDuration`, `BattleDuration` and `IsLongLock` run the stages.
  - `LockOn`, `ClearLock`, `CycleLock`, `CommitLock` and `PickTarget` handle targeting.
  - `LockShotVelocity`, `UpdateLockAim` and `FireLockedShot` turn a lock into a launch.
- **`AZoogiPlayerController`** (`ZoogiPlayerController.h/.cpp`) is the listener. `OnStageChanged`, `OnTurnStarted`, `OnShotFired` and `OnImpact` drive the camera director and post-process.

Today's mapping is: Draw (hero shot) → Standby (overview, soft lock) → Main (over-the-shoulder lock, Tab or Space) → Battle (cinematic swing, then fire) → End (follow, aerial, settle) → `EndTurn`.

There are two parallel enums (`EZoogiPhase` and `EZoogiStage`) doing overlapping jobs. The new loop should **merge them** instead of adding a third state machine (see section 6).

## 2. How do Zoogi movement and collision work?

**`AZoogiBall`** (`ZoogiBall.h/.cpp`) is an `APawn`:
- **Physics body.** The root is a `USphereComponent` (radius 50) with simulate physics, object type `ECC_Pawn` and CCD on. Mass is `Def.MassRatio()` × 10 kg, where MassRatio is Defense/60 clamped to 0.55-1.8.
- **Visual.** A `UStaticMeshComponent` `Visual` uses `/Game/Zoogis/<id>/SM_<id>` with the GLB-imported materials. There is also a `Ring` glow (player colour, `M_ZoogiGlow` dynamic material instance) and a `TurnLight` point light.
- **Physics materials.** `BallPM` (restitution 0.75) and `OrbPM` (0.6) are created in `AZoogiGameMode::BeginPlay`. Physics substepping runs at 120 Hz (`DefaultEngine.ini`).
- **Launching.**
  - `Launch(FVector2D)` sets linear velocity plus a matching roll angular velocity.
  - `SetPlanarVelocity` lets powers redirect a Zoogi.
  - `SetPhasing` handles Bolt overlapping rivals instead of colliding, and turns on custom depth.
- **Rolling.** `ZoogiPhysics::ApplyRolling` (`ZoogiPhysics.cpp`) applies constant rolling deceleration (`AZoogiArena::GetRollingDecel` per theme) and roll-without-slip. `IsGrounded` traces against WorldStatic.
- **Collision callback.** `AZoogiBall::OnSphereHit`:
  - Records knock-off credit in `LastHitPlayer`.
  - Plays impact sparks (`NS_Sparks`) and a "Clack" sound when the impulse is above about 3500.
  - Calls `AZoogiGameMode::OnShooterContact` (in `ZoogiPowers.cpp`). That handles the first-contact juice (flash, ring, SMASH popup, `PC->OnImpact`: hit-stop, shake, FOV punch) and the armed-power triggers.
- **Other actors.**
  - **Orbs:** `AZoogiOrb`. `bStar` marks gold orbs, which give double points and +1 charge.
  - **Wolf clones:** `AZoogiClone`.
  - **Arenas:** `AZoogiArena` builds 7 arenas at runtime (`Build*`, `BuildRim`, Tomb bumpers, Neon rectangle court). Its query helpers are `DistanceToEdge`, `IsOffCourt` and `GetRollingDecel`.
- **Scoring is ring-out only.** `AZoogiGameMode::CheckKnockoffs` treats falling below Z −600 as out:
  - An orb scores 50, or 100 for a star orb.
  - Knocking off a rival Zoogi gives +100 to the last hitter and −75 to the victim.
  - **There is no HP or damage model.** "Damage" in Evan's spec has to mean knockback, status and score until Evan decides otherwise.

## 3. Where does power, artifact or card data live?

**There are no artifacts and no cards yet.** What exists:
- **`ZoogiTypes.h`**
  - `EZoogiPower` has 7 signature powers, one per Zoogi:
    - WolfPack (Wolfgang)
    - Explosion (Hotstreak)
    - Ricochet (Lars)
    - LockOn (Pinpoint)
    - StaticShock (Bolt)
    - BandageBind (Wraps)
    - ShadowStun (Nightshade)
  - `FZoogiDef` holds Id, Name, Type, colours, ability text, Speed, Power, Defense, Control and PowerKind.
- **`ZoogiTypes.cpp`** `BuildRoster()` **hard-codes the roster in C++**. There are no DataTables or DataAssets anywhere in the project.
- **`ZoogiPowers.cpp`** implements the powers as part of `AZoogiGameMode`:
  - `TryPower`, `CanUsePower` and `FireInstantPower`
  - `OnArmedShot` and `OnShooterContact`
  - `TickPowers` and `EndPowers`
  - `GetLockTarget` and `GetEffectivePull`
  - CPU side: `PlanCPUShot` and `CPUPowerTick`
- **Per-Zoogi runtime state on `AZoogiBall`:**
  - `Charges` starts at 1, caps at 3, and gains +1 per **star orb**.
  - `StunTurns`, `BoundShots` and `bPhasing`.
- `ZoogiUtil::IsArmedPower` splits **armed** powers (Ricochet, LockOn, StaticShock, BandageBind), which modify the next shot, from **instant** ones (WolfPack, Explosion, ShadowStun), which fire while rolling.

## 4. How are UI actions triggered (HUD Canvas vs UMG)?

There are two layers, and both funnel into **`AZoogiGameMode::MenuAction(FName Action, int32 Index)`**.

**In-match UI is a Canvas HUD.** `AZoogiHUD` (`ZoogiHUD.h/.cpp`) contains:
- scoreboard, feed, stage tracker and banners (`DrawStages`)
- aim line, dotted path and power meter (`DrawAim`)
- power card (`DrawPower`)
- popups, the KO callout, the results screen and player tags

**Canvas buttons are immediate-mode.** `AZoogiHUD::Button(...)` draws a button and registers an `FZoogiButton` rectangle. Then `AZoogiPlayerController::HandleInput` hit-tests clicks against those rectangles and calls `MenuAction`. Current in-match actions are `"Power"`, `"Menu"` and `"Rematch"`.

**The main menu is UMG built in C++.**
- `UZoogiMenuWidget` (`ZoogiMenuWidget.h/.cpp`) builds the menu.
- `UZoogiButton` is a non-focusable button, so it doesn't steal Space or Tab.
- `UZoogiMenuClick` forwards clicks to `MenuAction`.
- Portraits are `/Game/UI/Portraits/T_Portrait_<id>`.

**Keyboard and mouse are read in `AZoogiPlayerController::HandleInput`:**
- LMB runs `PickTarget`, which only hits Zoogis and orbs.
- Tab cycles targets and Space commits.
- RMB or Esc clears the lock.
- E or Q uses the power. Ctrl+R restarts.
- `-ZoogiHumanTest` runs a scripted human-input test.

## 5. Existing VFX, material and Niagara hooks

- **`AZoogiFX`** (`ZoogiFX.h/.cpp`):
  - `Spawn(...)` makes an additive expanding sphere or ring with a point-light flash, using `M_ZoogiFX` (params Color, Strength, Ring).
  - `SpawnTrail(...)` drops lightweight trail rings.
  - `Niagara(World, "NS_x", Loc, Rot, Scale)` spawns a cached Niagara system.
  - `PowerBurst(...)` gives a per-power colour burst.
- **Niagara assets in `/Game/FX`:**
  - `NS_Burst_{Blue,Gold,Green,Purple,Red,White,Yellow}`, `NS_Explosion` and `NS_Sparks`.
  - They were made by `Scripts/make_niagara.py`, which duplicates engine templates, and recoloured or scaled through `UZoogiFXLibrary` (`TintNiagara`, `ScaleNiagaraFloats`).
  - Unused engine templates are also available: RadialBurst, DirectionalBurst, LocationBasedRibbon, DynamicBeam, HangingParticulates, BlowingParticles, UpwardMeshBurst and others. These cover trails, arcs and mist.
- **Materials in `/Game/Materials`:**
  - `M_ZoogiSurface` (Color, Roughness, Metallic, EmissiveColor, EmissiveStrength)
  - `M_ZoogiGlow` (Color, Strength; fresnel glow)
  - `M_ZoogiGround` (ColorA/B, NoiseScale, Roughness)
  - `M_ZoogiFX` (additive unlit)
  - Factories: `ZoogiUtil::SurfaceMID/GlowMID/GroundMID/FxMID` in `ZoogiTypes.cpp`. Creation scripts: `Scripts/setup_content.py` and `Scripts/make_fx.py`.
- **Zoogi models** keep their GLB materials. Only the `Ring` dynamic material instance and `TurnLight` are driven at runtime: player colour, pulse when active, stun and bind tints.
- **`AZoogiTargetMarker`** draws the lock ring, ticks, highlight shell and arrow using `M_ZoogiGlow` dynamic material instances.
- **Camera and post-process:** `AZoogiPlayerController::UpdateCamera` and `ApplyPostProcess`.
  - `namespace ZoogiCamTune` has profiles PDraw, PStandby, PMain, PBattle, PEnd, PFollow, PMenu and PTargetCU. Fields are pitch, distance, FOV, dutch, lags, yaw spring, vignette, DOF, saturation, contrast, gain and blend-in.
  - It also holds the Battle framing guard, the two-beat long-lock camera, impact shake, hit-stop (`SlowMoUntil`) and FOV punches.
- **Audio:** `ZoogiUtil::PlaySfx("Name")` plays `/Game/Audio/S_<Name>`.
- **Not present yet:** Media Framework, Level Sequences or Sequencer, material parameter collections, overlay materials, DataTables or DataAssets.
  - `Build.cs` already links UMG, Niagara and NiagaraCore.
  - `MediaAssets` and `LevelSequence`/`MovieScene` must be added for anime playback.

---

## 6. What to modify next (ordered; reuse → extend → add; no duplicate managers)

The rule: **`AZoogiGameMode` stays the only turn authority. `AZoogiPlayerController` stays the only camera director. `AZoogiHUD` and UMG stay the only UI. `AZoogiFX` stays the only VFX spawner.** Exactly **one** new manager is allowed: the anime/event bus subsystem.

1. **Merge the turn state machine (extend `AZoogiGameMode`).**
   - Replace `EZoogiStage` with `EZoogiTurnPhase { Draw, Awaken, Battle, Resolve, EndTurn }`.
   - Add a Battle sub-step enum `EZoogiBattleStep { SelectZoogi, Aim, Confirm, PowerUp, Launch, Rolling }`.
   - Reduce `EZoogiPhase` to `{ Menu, InMatch, GameOver }`, and remove `Aiming`/`Rolling` once everything compiles.
   - Move `SetStage`/`TickStages` into a new partial file `Private/ZoogiTurn.cpp` (still `AZoogiGameMode`).
   - Old → new mapping:
     - Standby → **Awaken**
     - Main + Battle → **Battle**
     - Rolling + End → **Battle.Rolling**, then **Resolve**
   - `ZoogiStageTune` becomes `ZoogiTurnTune`.
2. **Add a real Resolve state (extend `Tick` and `CheckKnockoffs`).**
   - Move the settle detection out of `Tick`'s Rolling branch into `TickResolve`. The settle rule is `AllAtRest` for 0.6-1.0 s, or a timeout of about 8 s after launch.
   - During the roll, **record** facts (contacts, knock-offs, artifact triggers) into an `FZoogiTurnLedger` instead of only applying score immediately.
   - Resolve then applies, in order: artifact after-effects, then statuses (stun, bind, frozen and so on), then Guard consumption, then score and KO, then combos and environment, then final positions and respawn.
   - It emits combat events, waits for any anime, and **only then** calls `EndTurn`.
   - Live knock-off detection can stay where it is, but scoring is committed in Resolve.
3. **Add artifact data (DataAssets).**
   - Create `UZoogiArtifactData : UPrimaryDataAsset` with Id, element, display name, icon, colours, Infuse params, Guard params, VFX refs (aura, trail, impact Niagara), overlay material, sounds and anime keys.
   - Make 5 assets in `/Game/Artifacts/`: `DA_Artifact_Ember`, `DA_Artifact_Storm`, `DA_Artifact_Frost`, `DA_Artifact_Titan`, `DA_Artifact_Void`.
   - Use DataAssets, not a DataTable, because each artifact references several assets (Niagara, materials, icons) and needs per-artifact tuning in the editor.
   - **Do** use a DataTable for the anime lookup (step 6), which is naturally row-shaped.
   - Optionally move the roster into `DA_Zoogi_<id>` later. That isn't required for this milestone.
4. **Add per-Zoogi artifact state (extend `AZoogiBall` with one component).**
   - Create `UZoogiArtifactComponent` on `AZoogiBall`. It holds `Hand` (saved artifacts, cap 2), `Infused`, `Guard` (artifact plus turns left), and owns the **visual transformation**:
     - an overlay material on `Visual` via `SetOverlayMaterial`
     - an optional shell sphere
     - attached aura and trail Niagara components
     - core light, driven through the existing `TurnLight`
     - squash and stretch, spin and wobble
   - This is a component on the existing pawn, not a manager.
5. **Add the Awaken UI and actions (extend the `MenuAction` dispatcher).**
   - New actions: `"AwakenSelect" idx`, `"Infuse" artifactIdx`, `"Guard" artifactIdx`, `"Save"`, `"AwakenDone"`.
   - Build the UI as a UMG `UZoogiAwakenWidget`, in C++ like `UZoogiMenuWidget` (non-focusable `UZoogiButton`), optionally reparented as `WBP_Awaken` for design polish.
   - The Canvas HUD keeps the scoreboard and banners. The Awaken panel is UMG because it needs card art, hover and tooltips.
   - CPUs decide in `PlanCPUAwaken` (new, next to `PlanCPUShot`).
6. **Add the combat event bus (the one new manager).**
   - Define the `FZoogiCombatEvent` USTRUCT.
   - Create `UZoogiAnimeSubsystem : UWorldSubsystem` with `Emit(Event)`, a lookup in `DT_AnimeSequences` (row struct `FZoogiAnimeRow`), a fallback chain, skip and shorten bookkeeping, preload, and playback (real-time, Level Sequence or Media).
   - It exposes `OnSequenceFinished`. The GameMode waits on it and never inspects playback details.
7. **Rework the camera (extend `ZoogiCamTune` and `UpdateCamera`).**
   - Add `PTactical` as the default for every turn phase, with small emphasis offsets. The dramatic profiles become "shots" that only the anime subsystem or Micro moments request (see section 8).
8. **Add angled aiming (extend `ZoogiTargeting.cpp`).**
   - Free floor-click aim, cut-angle nudges, ghost-ball preview and a one-bounce bank preview (see section 9).
9. **Implement artifact gameplay effects (extend `ZoogiPowers.cpp`, or add `ZoogiArtifacts.cpp` as another `AZoogiGameMode` partial).**
   - Infuse hooks into the existing `OnShooterContact`, `TickPowers` and `EndPowers` paths.
   - Guard hooks into `OnSphereHit` for the guarded victim.
10. **Make powers coexist** (section 7), then **re-run the balance streams** (`-ZoogiAuto` bal logs), since artifacts change the economy.

## 7. How the 7 per-Zoogi powers coexist with artifacts (recommendation)

**Recommended: artifacts become the turn resource, and signature powers become Affinity bonuses.**

- **Charges and E/Q are retired in Artifact mode.**
  - The resource is now: draw 1 artifact per turn, then Awaken uses it (Infuse or Guard) or saves it (hand cap 2).
  - **Star orbs** grant +1 bonus artifact draw on your next Draw, replacing +1 charge. That keeps gold orbs meaningful.
- **Each Zoogi gets one affinity artifact.** Infusing it upgrades the artifact effect with that Zoogi's signature power, and plays the per-Zoogi anime when one exists. Mapping (**approved by Evan, Oct 4**):

| Zoogi | Signature power | Affinity artifact | Affinity upgrade (power reused) |
|---|---|---|---|
| Hotstreak | Instant Explosion | Ember Stone | Flame burst uses the Explosion code (radius 360) |
| Bolt | Static Shock | Storm Eye | Phases through the first rival and stuns (StaticShock) |
| Lars | Ricochet | Storm Eye (chain) *or* Titan Core | Lightning chain = Ricochet re-target after the hit |
| Pinpoint | Laser Trajectory | Frost Crystal | Ice-lane shot: +30% (PinpointPowerBoost), straight line |
| Nightshade | Shadow Stun | Void Shard | Gravity pull + ShadowStun on the pulled rivals |
| Wraps | Bandage Bind | Titan Core | Heavy hit + BandageBind on contact |
| Wolfgang | Wolf Pack | Frost Crystal *or* Void Shard | Clones spawn at impact (WolfPack) |

- **Why this approach:**
  - It reuses all of the tested power code in `ZoogiPowers.cpp`.
  - It keeps each Zoogi's identity.
  - It avoids two resource systems competing for the same click.
  - It gives the anime matrix a natural "signature" tier.
- **Alternatives considered:**
  - (a) Keep both systems: too many decisions per turn, and it breaks Awaken's single choice.
  - (b) Drop the powers: this wastes working code and character identity.
- **Mode flag.** Keep a `bArtifactMode` setting (default ON), so the current Charges plus E/Q build still runs for comparison and for the existing balance harness.

## 8. Camera reconciliation (stated clearly)

Evan's new spec asks for a mostly fixed **elevated tactical camera with small emphasis**. Yesterday's work built **big per-stage dramatic cameras**: the Draw hero orbit, Standby aerial, Main over-the-shoulder, Battle dutch lock-on swing, the TargetCU two-beat, and the End top-down.

**Recommendation:**

1. **The Tactical profile becomes the default for all turn phases.**
   - Add `PTactical` to `ZoogiCamTune`: pitch about 55°, distance about 2300 scaled by arena size, FOV about 80, no dutch, neutral grade, light vignette, no DOF.
   - Emphasis is small offsets on top of it, not profile swaps:

| Moment | Emphasis on top of PTactical |
|---|---|
| Awaken / aim | ~10% shorter boom and look-at biased 30% toward the active Zoogi |
| Launch | look-at tracks the shooter-target midpoint, then the ball |
| Impact | existing `OnImpact` shake at about 40% amplitude, FOV punch 3° instead of 8° |
| Optional hit-stop | existing `SlowMoUntil`, 0.06-0.1 s, on powered hits only |
| Afterwards | auto-return over about 0.5 s |

2. **Keep the dramatic profiles, but move them to the anime layer.**
   - PBattle, PTargetCU (two-beat), PDraw hero and PEnd aerial are not deleted.
   - They become named camera shots (`Shot_HeroIntro`, `Shot_LockOn`, `Shot_TargetCU`, `Shot_Aerial`) that `UZoogiAnimeSubsystem` can request for **Micro and Standard real-time sequences**, e.g. Infuse activation uses the hero shot and a crit uses TargetCU.
   - That reuses yesterday's work exactly where the spec wants drama.
3. **Add a Settings toggle.** Camera style: **Tactical** (default) or **Cinematic** (the current per-stage behaviour). Evan can compare them side by side, and nothing built yesterday is lost.

## 9. Angled aiming plan (clickable only)

**How it works right now:**
- **Humans cannot aim at an angle.** A locked shot always goes **straight at the target's centre**: `LockShotVelocity` uses the unit vector to the target, with power auto-scaled by distance, so every hit is head-on.
- **Clicking the floor does nothing**, because `PickTarget` only hits Zoogis and orbs.
- `RotateAim()` exists but nothing calls it. The drag slingshot was removed on Oct 3, 4:20 PM.
- **Pinpoint's** old 35° cone snap (`GetLockTarget`/`LockOnCone`) only applies to the legacy pull path. Under lock-on the shot already points at the target, so Pinpoint's power now just hits 30% harder.
- The only off-centre shots are CPU aim error: ±(100−Control)/12°, i.e. about ±0.4° to ±4.6° across the current roster.

**Plan (all built on the existing `AimPull`/`SetAim`/`ReleaseAim` + `LockShotVelocity` pipeline):**

1. **Free aim (floor click).**
   - In Battle.Aim, a left-click on the floor (trace `ECC_Visibility`/WorldStatic, where `PickTarget` misses) sets a **free direction** toward the click point.
   - Power comes from click distance (the same stopping-distance maths the HUD dotted path uses), with clickable **power steps** (Soft / Medium / Full).
   - Clicking a Zoogi or orb still locks as now.
2. **Cut-angle nudges on a lock.**
   - Clickable **◀ ▶** buttons, plus mouse-wheel as a bonus, rotate the shot direction in 2° steps up to ±30° off centre.
   - The hit then lands off-centre and **cuts** the target sideways.
   - Show the **ghost-ball contact point** (where the shooter's centre is at contact), the **target departure arrow** along the contact normal, and the **shooter deflection arrow** (90° rule, scaled by the mass ratio).
3. **Bank-shot preview.**
   - The dotted path reflects **once** off rims (`BuildRim` segments), Tomb bumpers and Arabian walls, using a sweep trace with radius 50.
   - Damping comes from the physics-material restitution: 0.75 for balls, plus the rim value.
   - On open-edge arenas the path shows red past the edge, as now.
4. **Confirm** stays Space or a clickable **CONFIRM** button. RMB, Esc or a **CANCEL** button clears.
5. **CPU.** `PlanCPUShot` evaluates a few cut angles (−20…+20°) and bank candidates, so CPUs use the same system.
6. **Pinpoint.** Its affinity shows a longer, exact preview (two bounces, laser line) instead of a cone snap.

## 10. Risks

- **Turn design: "select a Zoogi."** The game is **one Zoogi per player**, a hotseat with 2-4 players. Selecting a Zoogi only matters with multiple Zoogis per side.
  - Recommendation for this milestone: keep one Zoogi per player. Selection is automatic but still shown, Infuse targets your Zoogi and Guard protects your Zoogi.
  - Add `OwnerPlayer`/`TeamIndex` fields so teams of N are possible later. **Evan to decide.**
- **No HP or damage.** Resolve "damage" means knockback, statuses and score until Evan decides whether to add HP. Adding HP is a big balance change.
- **"Crit" is undefined.** Proposal: a powered hit with impact speed ≥ 75% of full and a head-on contact (cut < 8°). It's deterministic and skill-based, with no random rolls. **Approved by Evan (Oct 4).** Implementation note: "75%" is measured against the shot's own launch speed (`ZoogiStageTune::CritImpactFrac`), so a precise short shot can crit too; switching it to 75% of full power is a one-line change.
- **Returning to the exact same state after anime.**
  - Chaos physics isn't deterministic across a pause and resume.
  - Mitigation: anime plays only at **frozen** moments. Snapshot every body's transform, linear and angular velocity and sleep state in `FZoogiWorldSnapshot`, pause, play, restore, then resume. Never let the anime move gameplay actors.
- **Two state machines today** (`EZoogiPhase` + `EZoogiStage`). The merge must be done in one go, or bugs like "Rolling while Stage=Draw" will appear. The balance and screenshot automation flags (`-ZoogiAuto`, `-ZoogiShot*`, `-ZoogiHumanTest`) must keep working.
- **Balance reset.** Artifacts plus affinity invalidate balance samples 1-3, so re-run the streams.
  - The latest sample (sample 2): Hotstreak 50% wins vs Wraps 4%.
  - Sample 3 (`bal3`) results were never read.
- **Media and packaging.**
  - `.mp4` files must live in `Content/Movies` and be staged (Project Settings → Packaging → additional non-asset directories).
  - On Windows playback uses WMF or Electra. Codec choice matters for Android and iOS (H.264 baseline or main).
- **Mobile.** Lumen, VSM and DX12 settings, runtime-built arenas and mouse-only input (hover, RMB, Tab, Space) all need a touch and scalability pass before native phone builds.
- **Production cost of anime.** 5 artifacts × 7 Zoogis × 3-4 event types explodes quickly. Mitigation: a generic per-artifact fallback first (see the matrix) and per-Zoogi variants only for affinity pairs.
- **Squash and stretch on a rolling, physics-rotated mesh.** Non-uniform scale on a rotating child can shear. Use an absolute-rotation `SquashRoot` scene component (scale along velocity) with `Visual` copying the body rotation, and keep it brief (≤ 0.15 s).
- **Box-only uncommitted code** (sparkle trail, orb flyers). Decide whether to sync, build and commit it before the Awaken work, so it doesn't conflict with the `AZoogiBall`/`AZoogiHUD` edits.

---

## Appendix: testing on a phone

- **Pixel Streaming 2 (the fastest route, no mobile port).**
  - UE 5.6 ships the **PixelStreaming2** plugin under `Engine/Plugins/Media/PixelStreaming2`. The legacy `PixelStreaming` plugin is deprecated. Presence on Evan's install still needs a read-only check, because the PC was offline when this was written.
  - **Setup:**
    1. Enable the plugin in `ZoogiRoll.uproject`.
    2. Get the signalling and web server (Wilbur) from the `EpicGamesExt/PixelStreamingInfrastructure` repo, branch **UE5.6**. It needs **Node.js**.
    3. Run the packaged game with `-PixelStreamingURL=ws://127.0.0.1:8888 -RenderOffscreen` (or windowed). **Don't use port 5000.**
    4. The phone opens `http://<PC-LAN-IP>` on the same Wi-Fi.
  - **AMD RX 6600:** Pixel Streaming uses AMD's **AMF** H.264 hardware encoder. RDNA2 supports it, but Epic calls AMD support **experimental**, and quality and stability vary with the driver.
    - Fallback: the software **VP8/VP9** encoder, via `-PixelStreamingEncoderCodec=VP8` or `VP9` (more CPU, a little more latency).
    - Start at 1280×720 and 30-60 fps.
  - **Input:** the frontend maps touch to mouse, so a tap acts as a left click. The new clickable-only design (Awaken buttons, CONFIRM, ◀ ▶) suits this.
    - RMB, Tab, Space and the keyboard shortcuts have no touch equivalent, so every action needs an on-screen button.
    - Hover-only UI won't work.
- **Native Android:**
  - Install Android platform support for UE 5.6, plus Android Studio with the matching SDK/NDK/JDK through Turnkey (`SetupAndroid.bat`).
  - Package `Android_ASTC`.
  - Mobile pass: turn off Lumen and VSM, use the mobile forward or deferred renderer, set scalability, and check that the runtime-built arenas and translucency fit the budget.
  - Touch UI pass. Also check that `Config/DefaultEngine.ini` (which already gained an AndroidFileServer section) is OK.
- **Native iOS:** needs a **Mac with Xcode** (or remote build from Windows to a Mac), an **Apple Developer account** with signing certificates and a provisioning profile, and the same mobile renderer and touch pass.
