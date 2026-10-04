# Assignment for the coding AI (Cursor): Milestone "Awaken", C++ architecture

Copy and paste this whole file as the task.

## Context and rules

- **Project:** Zoogi Roll, a UE 5.6 C++ marble battler at `C:\dev\ZoogiRollUnreal` (module `ZoogiRoll`, source in `Source/ZoogiRoll/{Public,Private}`).
- **Do NOT start over.** Extend the existing classes. Do not add a second GameMode, phase manager, camera manager, HUD or FX spawner. Exactly **one** new manager is allowed: `UZoogiAnimeSubsystem` (the event bus).
- **Read first:** `Docs/MilestoneAwaken/REVIEW.md` (the architecture answers) and `Docs/MilestoneAwaken/ANIME-SEQUENCE-MATRIX.csv`.
- **Don't touch** `C:\dev\ZoogiRollUE` (the old web game), `C:\dev\pg` or port 5000.
- **Git:** commit locally in small steps. Do not push.
- **Keep working:**
  - The automation flags `-ZoogiAuto`, `-ZoogiShot*`, `-ZoogiHumanTest` and `-ZoogiMenuShot` (the last one needs `-ZoogiAuto`).
  - The balance log output (`FinalScores` line in `FinishGame`).
- **Build:** use `Scripts\build.ps1` (or UnrealBuildTool for `ZoogiRollEditor Win64 Development`). Package with `Scripts\package.ps1`.
- **Gameplay rules from Evan's spec:**
  - Zoogis stay **spherical with no limbs**.
  - **No flick or drag.** Clickable controls only.
  - **No Charge or Link actions yet.**

## Files to inspect first

| File | Why |
|---|---|
| `Public/ZoogiGameMode.h`, `Private/ZoogiGameMode.cpp` | `EZoogiPhase`, `EZoogiStage`, `ZoogiStageTune`; `BeginTurn`/`EndTurn`/`Tick` (Rolling → `AllAtRest` → `SettleDelay` → `EndTurn`); `CheckKnockoffs`; `MenuAction` dispatcher |
| `Private/ZoogiTargeting.cpp` | `SetStage`, `TickStages`, `LockOn`/`CommitLock`/`PickTarget`, `LockShotVelocity`, `FireLockedShot` |
| `Private/ZoogiPowers.cpp` | the 7 powers, `OnShooterContact`, `TickPowers`, `EndPowers`, `PlanCPUShot`, `CPUPowerTick` |
| `Public/ZoogiTypes.h`, `Private/ZoogiTypes.cpp` | `EZoogiPower`, `FZoogiDef`, `BuildRoster()`, `ZoogiUtil` material factories |
| `Public/ZoogiBall.h`, `Private/ZoogiBall.cpp` | sphere physics pawn, `Launch`, `SetPlanarVelocity`, `OnSphereHit`, `Charges`/`StunTurns`/`BoundShots` |
| `Private/ZoogiPlayerController.cpp` | `HandleInput`, `ZoogiCamTune` profiles, `UpdateCamera`, `OnImpact`, hit-stop `SlowMoUntil` |
| `Private/ZoogiHUD.cpp` | Canvas HUD, `Button()` → `FZoogiButton` → `HitTest`, `DrawStages`, `DrawAim`, `DrawPower` |
| `Private/ZoogiMenuWidget.cpp` | the pattern for C++-built UMG (`UZoogiButton` non-focusable, `UZoogiMenuClick`) |
| `Private/ZoogiFX.cpp`, `Private/ZoogiFXLibrary.cpp` | `AZoogiFX::Spawn`, `::Niagara`, `::PowerBurst`; Niagara tinting |
| `ZoogiRoll.Build.cs` | add `MediaAssets`, `LevelSequence`, `MovieScene` modules |

## Tasks (in order)

### 1. Phase state machine: merge and rename (in `AZoogiGameMode`)

Replace the old enums with these:

```cpp
UENUM() enum class EZoogiMatchState : uint8 { Menu, InMatch, GameOver };      // replaces EZoogiPhase
UENUM() enum class EZoogiTurnPhase  : uint8 { Draw, Awaken, Battle, Resolve, EndTurn }; // replaces EZoogiStage
UENUM() enum class EZoogiBattleStep : uint8 { SelectZoogi, Aim, Confirm, PowerUp, Launch, Rolling };
```

- Create the partial `Private/ZoogiTurn.cpp`. Move `SetStage`/`TickStages`/`StageDuration` there as `SetTurnPhase`/`TickTurn`/`SetBattleStep`.
- Mapping from the old stages:
  - Standby → **Awaken**
  - Main + Battle → **Battle** (Aim/Confirm/PowerUp)
  - Rolling + End → **Battle.Rolling**, then **Resolve**
- Rename `ZoogiStageTune` to `ZoogiTurnTune` and keep the existing timings where they still apply.
- Add `PowerUpTime` (0.35 s normal, longer when infused).
- Update `AZoogiPlayerController::OnStageChanged` → `OnTurnPhaseChanged(EZoogiTurnPhase, EZoogiBattleStep)`.
- Update the HUD tracker in `DrawStages` to show **DRAW · AWAKEN · BATTLE · RESOLVE · END**.
- Only `SetTurnPhase` is allowed to change phase. Log every transition as `LogZoogi: Phase X -> Y` so the automation logs show it.

### 2. Artifact data

Create `Public/ZoogiArtifactData.h`:

```cpp
UENUM(BlueprintType) enum class EZoogiArtifact : uint8 { None, Ember, Storm, Frost, Titan, Void };
UENUM(BlueprintType) enum class EZoogiElement  : uint8 { Neutral, Fire, Lightning, Ice, Earth, Void };

UCLASS(BlueprintType) class UZoogiArtifactData : public UPrimaryDataAsset {
  GENERATED_BODY()
public:
  UPROPERTY(EditAnywhere) EZoogiArtifact Id; UPROPERTY(EditAnywhere) EZoogiElement Element;
  UPROPERTY(EditAnywhere) FText DisplayName; UPROPERTY(EditAnywhere) FText InfuseText, GuardText;
  UPROPERTY(EditAnywhere) TSoftObjectPtr<UTexture2D> Icon;
  UPROPERTY(EditAnywhere) FLinearColor CoreColor, RimColor;
  // Infuse (powered collision) tuning
  UPROPERTY(EditAnywhere) float LaunchSpeedMul = 1.f, MassMul = 1.f, ImpactRadius = 0.f, ImpactPush = 0.f;
  UPROPERTY(EditAnywhere) float StatusTurns = 0.f;      // e.g. Frozen 1, Shocked 1
  // Guard tuning
  UPROPERTY(EditAnywhere) float GuardKnockbackMul = 0.6f, GuardMassMul = 1.f, GuardReflect = 0.f;
  // Visuals
  UPROPERTY(EditAnywhere) TSoftObjectPtr<UMaterialInterface> OverlayMaterial;   // shell transformation
  UPROPERTY(EditAnywhere) TSoftObjectPtr<class UNiagaraSystem> AuraFX, TrailFX, ImpactFX, GuardFX;
  UPROPERTY(EditAnywhere) FName SfxInfuse, SfxImpact, SfxGuard;
  UPROPERTY(EditAnywhere) FName AnimeKey;               // "Ember" etc, used in sequence ids
  UPROPERTY(EditAnywhere) FName AffinityZoogi;          // e.g. "hotstreak"
};
```

- Create 5 assets in `/Game/Artifacts/`: `DA_Artifact_Ember`, `DA_Artifact_Storm`, `DA_Artifact_Frost`, `DA_Artifact_Titan`, `DA_Artifact_Void`. A Python script in `Scripts/make_artifacts.py`, following the `make_niagara.py` style, is fine.
- Starting Infuse tuning, all values tunable:

| Artifact | Infuse effect |
|---|---|
| **Ember** | on first contact: radial flame burst, radius 260, push 900; target Burned (next shot −15%) |
| **Storm** | on contact: lightning chains to up to 2 more Zoogis or orbs within 650 (push 600 each); first target Shocked (next shot power capped at 70%) |
| **Frost** | target Frozen for 1 turn: its rolling decel drops to ice level after the hit, so it slides far; ice shards push small things in a 180 cone |
| **Titan** | shooter `MassMul` 1.7, launch speed ×0.9; on contact a ground shockwave, radius 320, push 700 |
| **Void** | at impact, a gravity pull for 0.6 s toward the impact point (radius 450, accel 2500), then release |

- **Guard** (lasts through the **next opponent's attack only**) gives knockback ×0.6 on the guarded Zoogi, plus a flavour per artifact:

| Artifact | Guard flavour |
|---|---|
| Ember | attacker bounces back with a flame pop |
| Storm | attacker gets Shocked |
| Frost | attacker velocity ×0.6 at contact |
| Titan | guarded mass ×1.8 |
| Void | attacker deflected 25° |

### 3. Per-Zoogi artifact state

Add `UZoogiArtifactComponent` (`Public/ZoogiArtifactComponent.h`) to `AZoogiBall`:
- `TArray<UZoogiArtifactData*> Hand` (cap 2), `UZoogiArtifactData* Infused`, `UZoogiArtifactData* Guard`, `int32 GuardTurns`, plus statuses (`Burned`, `Shocked`, `Frozen`; reuse `StunTurns` and `BoundShots`).
- **Visual API**, implemented by the Unreal AI or by you as stubs that apply the DataAsset refs:
  - `SetInfusedVisual(Data, Stage)`, `SetGuardVisual(Data)`, `ClearVisuals()`
  - `PulseCore(Strength)`, `Squash(Amount, Dir)`
- In `ZoogiBall.cpp`, the overlay goes on through `Visual->SetOverlayMaterial(MID)`. **Never edit the imported GLB materials.**
- Add `OwnerPlayer`/`TeamIndex` to `AZoogiBall`, so N Zoogis per player is possible later. This milestone stays at 1 per player.

### 4. Draw and Awaken actions

- **Draw.** Each player has a shuffled artifact bag (2 of each type, reshuffled when empty). Draw 1 per turn, +1 if a star orb was scored last turn. **Star orbs no longer add `Charges` in artifact mode.**
- **Awaken.** No repositioning and no flick. New `MenuAction` names:
  - `AwakenSelect <zoogiIdx>`
  - `Infuse <handIdx>` (the drawn card is index 0)
  - `Guard <handIdx>`
  - `Save`
  - `AwakenDone`
- Rules:
  - At most **one** Infuse or Guard per turn.
  - Unused drawn artifacts go to the Hand. If the Hand is over its cap, the oldest is discarded (with a feed line).
- Add `bool bArtifactMode = true` on the GameMode, with a menu toggle and the `-ZoogiClassicPowers` flag to turn it off. When it's off, the old Charges plus E/Q power flow runs unchanged.
- **Affinity.** If `Data->AffinityZoogi == Ball->Def.Id`, the Infuse also triggers that Zoogi's signature power through the existing `ZoogiPowers.cpp` functions. The mapping is in REVIEW.md section 7 and needs Evan's confirmation, so make it data, not code.
- **CPU.** Add `PlanCPUAwaken()` next to `PlanCPUShot`:
  - Infuse if a knock-off line exists.
  - Guard if the CPU is near an edge and threatened.
  - Otherwise Save.

### 5. Battle sub-steps and angled aiming

Sub-steps run **SelectZoogi → Aim → Confirm → PowerUp → Launch → Rolling**.

Aim, all extending `ZoogiTargeting.cpp`:
- **Lock-on** works as today (click a Zoogi or orb).
- **Free aim.** In `HandleInput`, a floor click (trace WorldStatic when `PickTarget` misses) sets a free direction. Power comes from click distance using the existing stopping-distance maths, plus `AimPower <0..2>` steps (Soft/Med/Full).
- **Cut nudges.** `AimNudge -1/+1` actions rotate the locked direction 2° per step, clamped to ±30°. `LockShotVelocity` must accept an angle offset.
- **Preview data for the HUD.** Expose `FZoogiAimPreview { Path points, GhostContact, TargetDir, ShooterDir, BankPoint, bOffCourt }`.
  - Compute one reflection off rim, bumper or wall collision with a sphere sweep (radius 50).
  - Compute the contact normal for cut shots.
- **Controls.** Confirm is Space or `AimConfirm`. Cancel is RMB, Esc or `AimCancel`. Delete the dead `RotateAim()` or wire it to the nudges.
- **CPU.** `PlanCPUShot` samples cut angles −20…+20 and bank candidates.

### 6. Resolve state (authoritative results)

- Add `FZoogiTurnLedger`, reset at Launch. It records contacts (who, whom, speed, normal, time), knock-offs, artifact triggers, guard triggers and status applications.
- Change `CheckKnockoffs` so it **records** a knock-off and makes the body inert (as now), but the **score is committed in Resolve**. Keep the live KO popup.
- Enter Resolve when `AllAtRest()` has held for 0.8 s, or 8 s after launch (the safety timeout).
- Resolve does these steps **in order**:
  1. Artifact after-effects
  2. Statuses
  3. Guard consumption
  4. Score and KOs
  5. Combos (2+ knock-offs in one shot = combo)
  6. Environment
  7. Respawn and final positions
- Then it emits events (step 7), waits on `UZoogiAnimeSubsystem::IsBusy()` and moves to **EndTurn** → `BeginTurn`.
- Crit: **powered hit with impact speed ≥ 75% of full and cut < 8°**. Put it in `ZoogiTurnTune` so Evan can change it.

### 7. Event struct and AnimeSequence event bus (the only new manager)

```cpp
UENUM() enum class EZoogiCombatEventType : uint8 { Infuse, Guard, Save, PoweredHit, Hit, Miss, Critical, GuardBlock, Combo, StatusApplied, KO, Ultimate };
USTRUCT(BlueprintType) struct FZoogiCombatEvent {
  GENERATED_BODY()
  UPROPERTY() EZoogiCombatEventType Type; UPROPERTY() FName ActiveZoogi; UPROPERTY() FName TargetZoogi;
  UPROPERTY() TWeakObjectPtr<AZoogiBall> Active; UPROPERTY() TWeakObjectPtr<AActor> Target;
  UPROPERTY() EZoogiArtifact Artifact; UPROPERTY() EZoogiElement Element; UPROPERTY() EZoogiPower Ability;
  UPROPERTY() bool bHit=false, bCrit=false, bGuardActivated=false, bKO=false, bUltimate=false, bAffinity=false;
  UPROPERTY() int32 Combo=0; UPROPERTY() FName Status; UPROPERTY() float ImpactSpeed=0; UPROPERTY() FVector ImpactPoint;
  UPROPERTY() int32 Turn=0, Round=0, Player=0;
};
USTRUCT() struct FZoogiAnimeRow : public FTableRowBase {   // DT_AnimeSequences, row name = sequence id
  GENERATED_BODY()
  UPROPERTY(EditAnywhere) EZoogiCombatEventType EventType; UPROPERTY(EditAnywhere) FName Zoogi;  // "Any" = generic
  UPROPERTY(EditAnywhere) EZoogiArtifact Artifact; UPROPERTY(EditAnywhere) bool bRequireCrit=false, bRequireAffinity=false;
  UPROPERTY(EditAnywhere) uint8 Tier;            // 0 Micro,1 Standard,2 Special,3 Ultimate
  UPROPERTY(EditAnywhere) float Length, ShortLength; UPROPERTY(EditAnywhere) uint8 Format; // 0 RealTime,1 Sequence,2 Media,3 Hybrid
  UPROPERTY(EditAnywhere) TSoftObjectPtr<class ULevelSequence> Sequence; UPROPERTY(EditAnywhere) TSoftObjectPtr<class UMediaSource> Media;
  UPROPERTY(EditAnywhere) FName RealTimeShot;    // e.g. Shot_TargetCU (camera profile + FX preset)
  UPROPERTY(EditAnywhere) int32 FullPlaysPerMatch=1; UPROPERTY(EditAnywhere) int32 Priority=0; UPROPERTY(EditAnywhere) bool bPlaceholder=true;
};
```

- **`UZoogiAnimeSubsystem : UWorldSubsystem`** (`ZoogiAnimeSubsystem.h/.cpp`) provides:
  - `void Emit(const FZoogiCombatEvent&)`, which queues the event.
  - `FName Resolve(const FZoogiCombatEvent&)`, which runs the lookup with the fallback chain below.
  - `bool IsBusy()`, `Skip()` and the `OnSequenceFinished` delegate.
  - Preloading: on Infuse, async-load the rows for that artifact (`FStreamableManager`).
- **Fallback chain:**
  1. `Anime_<Zoogi>_<Artifact>_<Event>_NN`, where Event is one of Infuse, Guard, PoweredHit, Critical, KO, Miss or Status, e.g. `Anime_Hotstreak_Ember_Critical_01` (the spec's "Flare" = Hotstreak)
  2. `Anime_Any_<Artifact>_<Event>_NN`
  3. `Anime_Any_Any_<Event>_NN`
  4. none, which falls back to a Micro real-time default
- **Skip and shorten.** A row plays in full `FullPlaysPerMatch` times, then at `ShortLength`. Clicking skips after 0.3 s. CPU-vs-CPU turns force Short. There is a settings option for Anime: Full / Short / Off.
- **Authoritative order.** The game result is already computed before `Emit` (Resolve, or the frozen impact moment).
  - The subsystem takes an `FZoogiWorldSnapshot` (every `AZoogiBall`, `AZoogiOrb` and `AZoogiClone`: transform, linear and angular velocity, awake state; plus the camera pose), pauses gameplay, plays, restores the snapshot **exactly**, then resumes.
  - Anime must never change gameplay state.
- **Impact moments.** On the first powered contact (`OnShooterContact` with `Infused != null`):
  1. Compute the artifact effect immediately (velocities set).
  2. Take the snapshot.
  3. Emit `PoweredHit` (Micro, real-time by default).
  4. Restore and continue the roll.
- **Playback.** Implement the playback back-ends as an interface so the Unreal AI can fill them:
  - `RealTime`: ask the PlayerController for a camera shot + `AZoogiFX` preset for N seconds.
  - `Sequence`: `ALevelSequenceActor`/`ULevelSequencePlayer`.
  - `Media`: `UMediaPlayer` + a fullscreen widget.

### 8. Camera hooks (`AZoogiPlayerController`)

- Add the `PTactical` profile and make it the default for all turn phases. It gets small emphasis: a slight zoom on the active Zoogi, launch tracking, impact shake at 40%, an optional 0.06-0.1 s hit-stop and auto-return. Values are in REVIEW.md section 8.
- Keep PDraw, PBattle, PTargetCU and PEnd as named shots: `PlayShot(FName Shot, float Duration)`, used only by the anime subsystem.
- Add the setting `CameraStyle = Tactical | Cinematic`. Cinematic is the current behaviour.

## Definition of done

- [ ] The build compiles cleanly (no new warnings in ZoogiRoll), and the editor and packaged game both run.
- [ ] One match (2 players, 1 human + 1 CPU) plays the full loop **Draw → Awaken → Battle → Resolve → EndTurn**, and the log shows each `Phase X -> Y` transition.
- [ ] Awaken offers Infuse, Guard and Save via clickable buttons. Each works, and the hand cap and discard work.
- [ ] Infused shots apply the artifact's DataAsset tuning. Guard reduces knockback during the next opponent attack. Both are visible: at minimum an overlay tint and a Niagara aura, as stubs from the DataAsset.
- [ ] Angled aiming works: a floor click gives a free direction, ◀ ▶ nudges give ±30° cuts, and the preview shows the ghost contact, departure arrows and a one-bounce bank line. The CPU uses cut angles.
- [ ] Resolve commits the score after settle or timeout. The turn never advances before Resolve finishes.
- [ ] `UZoogiAnimeSubsystem` receives events, resolves ids from `DT_AnimeSequences` (imported from the matrix CSV) with fallbacks, and logs `Anime: <event> -> <id> (tier, format)`.
- [ ] A placeholder real-time Micro plays, followed by an exact restore.
  - **Test:** log the snapshot vs post-restore positions; the difference must be < 0.1 cm.
- [ ] `bArtifactMode=false` (or `-ZoogiClassicPowers`) reproduces the old Charges + E/Q behaviour.
- [ ] `-ZoogiAuto` balance runs still finish and print `FinalScores`. `-ZoogiHumanTest` is updated for Awaken and passes.
- [ ] Commits are small and local, and PROGRESS.md is updated.
