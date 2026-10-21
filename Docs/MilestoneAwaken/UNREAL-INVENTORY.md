# Unreal inventory: Milestone "Awaken" (Step 0)

What exists in `C:\dev\ZoogiRollUnreal` (UE 5.6, module `ZoogiRoll`). Taken from the project itself (Content folder listing, `Source/`, `Scripts/`, `Config/`), at commit `73b84fe` on Oct 4 2026, 4:30 PM CT.

The Step 0 table in `ASSIGNMENT-UNREAL.md` was written before the milestone work started. This file lists what is actually there now, including what the Awaken work has added so far. "Missing" at the end lists what still doesn't exist.

## Step 0 table

| Area | Existing asset or code |
|---|---|
| Zoogi meshes | `/Game/Zoogis/<id>/SM_<id>` for 7 Zoogis: bolt, hotstreak, lars, nightshade, pinpoint, wolfgang, wraps. GLB imports, each with its own material (`<id>_material_0`, Nightshade's is `Nightshade`) and 3–4 textures. **These materials are never edited.** Loaded by path in `AZoogiBall::InitZoogi`. A coloured `/Engine/BasicShapes/Sphere` is used if a mesh is missing. |
| Materials | `/Game/Materials/`: `M_ZoogiSurface`, `M_ZoogiGlow`, `M_ZoogiGround` (`Scripts/setup_content.py`); `M_ZoogiFX` (additive: Color, Strength, Ring; `make_fx.py`); `M_ZoogiDust` (`make_dust_material.py`); **`M_ZoogiArtifact`** (additive overlay: Color, Strength, Pattern 0–5, Mask 0–1; fresnel rim plus moving bands; `make_artifact_material.py`). Cosmic arena: `/Game/Env/Cosmic/M_CosmicSky`, `M_CosmicPlanet`, `M_CosmicStreak` + `T_CosmicNebula`, `T_CosmicPlanetA/B` (`make_cosmic_backdrop.py`, owned by the other worker). |
| Overlay | `UZoogiArtifactComponent` makes dynamic instances of `M_ZoogiArtifact` at runtime: `Visual->SetOverlayMaterial(OverlayMID)` on the character, plus a translucent `ArtifactShell` sphere. Also orbiters (Storm arcs, Void rings, Ember motes, Frost glitter), shell plates (Ember cracks, Storm veins, Frost shards, Titan stone plates) and an `ArtifactGuard` bubble (`M_ZoogiFX`). Mask animates 0→1 over 0.6 s. No material *instance assets* (MI_*) yet. |
| Niagara | `/Game/FX/`: `NS_Burst_{Blue,Gold,Green,Purple,Red,White,Yellow}`, `NS_Explosion`, `NS_Sparks`, `NS_Debris`, `NS_Dust`. Made from engine templates by `make_niagara.py` / `make_rock_fx.py`, and recoloured with `UZoogiFXLibrary::TintNiagara` / `ScaleNiagaraFloats` (also `DescribeNiagara`, `RecompileNiagara`; `inspect_niagara.py`). None of the 20 per-artifact systems (`NS_Aura_*`, `NS_Trail_*`, `NS_Impact_*`, `NS_Guard_*`) exist yet. Artifacts reuse the bursts: Ember `NS_Explosion`/`NS_Burst_Red`, Storm `NS_Burst_White`/`Blue`, Frost `NS_Burst_Blue`/`White`, Titan `NS_Burst_Gold`, Void `NS_Burst_Purple`. |
| FX spawner | `AZoogiFX` (`ZoogiFX.cpp`): `Spawn` (sphere/ring flash, optional follow actor), `SpawnTrail` (lightless ground puffs), `Niagara(World, "NS_x", Loc, Rot, Scale)` (`-ZoogiNoNiagara` turns it off), `PowerBurst(EZoogiPower)`. Per-artifact trails are `SpawnTrail` calls in `UZoogiArtifactComponent::TickVisuals`. |
| Ball | `AZoogiBall` (Pawn, ticks PrePhysics): `Sphere` (Chaos physics root, r = 50, CCD, Pawn channel), `Visual` (mesh attached to Sphere, no collision, `FitVisual` scale/yaw), `Ring` (flat `M_ZoogiGlow` disc in the player colour), `TurnLight`, `Artifact` (`UZoogiArtifactComponent`). `OnSphereHit` handles knockout credit, guarded hits (`AZoogiGameMode::OnGuardedHit`), rock hits, `NS_Sparks` and crack/clack sounds. Also a roll-loop sound, the player-colour trail and sparkle trail, `SetMassScale` (Titan / guard mass) and `SetPhasing` (Bolt). Existing visual hook: `UZoogiArtifactComponent::Pulse` / `ImpactPulse` / `GuardFlash` (glow, light, the shell squash-stretches along velocity). No squash, spin or shell-expansion hooks on the character model yet. |
| Physics | `ZoogiPhysics` (rolling deceleration, grounded test), `ZoogiConst` (radius, masses, launch speeds), physical material set in `InitZoogi`. Artifact effects are in `ZoogiAwaken.cpp`: Ember/Titan radial push, Storm 2-target chain, Frost Frozen + shard push, Void 0.6 s pull, Titan ×1.7 mass, Guard knockback ×0.6 plus a flavour effect per artifact. |
| Artifact data | `FZoogiArtifactRow : FTableRowBase` in `ZoogiArtifacts.h` holds every value: colours, launch/mass multipliers, impact radius/push, chain count, pull time/accel, status turns, guard knockback/mass/reflect/attacker multipliers, Niagara names, anime key and affinity. The built-in table is `BuildDefaults()` in `ZoogiArtifacts.cpp`. It optionally reads `/Game/Data/DT_Artifacts`, which doesn't exist. **No DataTable or DataAsset assets.** |
| Game flow | `AZoogiGameMode` (single GameMode): phases, stages, battle steps (Draw → PowerUp/Awaken → Aim → Rolling → Resolve), artifact hand/infuse/guard/status, scoring, knockouts, rocks, 7 arenas (`EZoogiArena`: Meadow, Frozen, Volcanic, Cosmic, Arabian, Tomb, Neon, built by `AZoogiArena` in the single map `/Game/Maps/Meadow`). Plus `AZoogiOrb`, `AZoogiRock` (destructible), `AZoogiClone`, `AZoogiTopping` (pizza), `AZoogiTargetMarker`, `ZoogiTargeting` (aim preview), `ZoogiPowers` (classic powers via `-ZoogiClassicPowers`). |
| Camera | `AZoogiPlayerController::UpdateCamera` / `ApplyPostProcess`; `namespace ZoogiCamTune` profiles `PMenu`, `PDraw`, `PStandby`, `PMain`, `PBattle`, `PFollow`, `PTargetCU`, `PEnd`, `PAnime` and **`PTactical`** (default; `-ZoogiCinematic` switches to the dramatic profiles). `OnImpact` (shake, FOV punch, hit-stop via `SlowMoUntil`), `OnPoweredHit`, `OnAnimeStarted` / `OnAnimeFinished`. |
| UI | Canvas `AZoogiHUD` (scoreboard, tracker, aim, cards, popups, feed, anime cards, `Button()` → `MenuAction`). C++ UMG: `UZoogiMenuWidget` (main menu, non-focusable `UZoogiButton`, menu background video) and **`UZoogiAwakenWidget`** (drawn and saved cards, INFUSE / GUARD / SAVE, aim nudges SOFT/MED/FULL, CONFIRM / CANCEL, with a click-through guard). `-ZoogiCanvasAwaken` / `-ZoogiCanvasMenu` fall back to Canvas. `/Game/UI/Portraits/T_Portrait_<id>` ×7 (`import_portraits.py`). Self-test: `-ZoogiUiTest` (43 checks). |
| Anime | `UZoogiAnimeSubsystem` (tickable world subsystem): `Emit(FZoogiCombatEvent)`, Full/Short/Off modes (`-ZoogiAnime=`), world freeze by snapshot plus pause, restore on finish, skip, per-event sounds. Sequences come from `Content/Anime/AnimeSequences.csv` (matrix). Real-time cards are drawn by the HUD. **Media back-end:** `UMediaPlayer` + `UMediaTexture` + `UFileMediaSource` reading `Content/Anime/Movies/<name>.mp4` (`Movies.csv`: `Anime_Any_Any_Critical_01`, `_Infuse_01`, `_KO_01`, plus `Menu_Background`), staged NonUFS. `-ZoogiNoAnimeMovies` turns clips off. No Level Sequence back-end. |
| Audio | `/Game/Audio/S_<Name>`, 31 sounds through `ZoogiUtil::PlaySfx` (`import_audio.py`, `import_sfx.py`, `synth_sfx.py`; credits in `Audio/SFX/CREDITS.md`). Includes artifact stings `S_Sting{Ember,Storm,Frost,Titan,Void}`, `S_AwakenCharge`, `S_GuardClang`, `S_CritBoom`, `S_HitCrack`, rock crack/shatter, crowd waves, UI click/confirm/cancel, roll loop, menu and match music. |
| Environment | `/Game/Env/{AlienCrystal,DesertHoodoo,OakTree,PineTree,SnowPine}/SM_*` (Tripo GLB imports with their own materials), `/Game/Orbs/SM_Orb`. Arena dressing, bumpers and rocks are procedural in `AZoogiArena` / `AZoogiRock`. |
| Build / tools | `Scripts/`: `build.ps1`, `package.ps1` (Win64 Development → `Packaged/`), `run_demo.ps1`, `run_python.ps1` (headless `UnrealEditor-Cmd -run=pythonscript`), `sync.ps1`, the content scripts above, `glb_unwebp.py`, `fix_material_usage.py`, `anime/make_cutins.py`, `cosmic/`. Modules (`ZoogiRoll.Build.cs`): Core, CoreUObject, Engine, InputCore, PhysicsCore, Slate, SlateCore, UMG, Niagara, NiagaraCore, MediaAssets, ProceduralMeshComponent. `DefaultGame.ini` always-cooks Zoogis, Orbs, Env, Materials, Audio, FX, UI, `/Engine/BasicShapes` and VolumetricClouds, and stages `Anime` as NonUFS. |
| Missing | Per-artifact material instances (`MI_Artifact_*`, `MI_Guard_*`); the 20 per-artifact Niagara systems; `MPC_ZoogiAnime` (no material parameter collections); Level Sequences (and the LevelSequence / MovieScene modules); DataTables or DataAssets for the artifact table; squash / spin / shell-expansion hooks on the model; `CameraStyle` as a saved setting (only the command-line switch). |

## Source files

`Source/ZoogiRoll`: 23 `.cpp` and 20 `.h`, about 11.5k lines of C++.

| File | Role |
|---|---|
| `ZoogiGameMode` | match flow, menu actions, scoring, screenshots (`-ZoogiShots=`), automation switches |
| `ZoogiAwaken.cpp` | Awaken: draw, infuse, guard, save, powered hits, guard blocks, status effects (part of `AZoogiGameMode`) |
| `ZoogiArtifacts` | artifact enum, row struct, built-in table, affinity map |
| `ZoogiArtifactComponent` | per-Zoogi hand / infuse / guard / status and the transformation visuals |
| `ZoogiBall`, `ZoogiOrb`, `ZoogiRock`, `ZoogiClone`, `ZoogiTopping` | gameplay actors |
| `ZoogiArena` | the 7 procedural arenas |
| `ZoogiPhysics`, `ZoogiTargeting`, `ZoogiPowers` | rolling physics, aim preview, classic powers |
| `ZoogiPlayerController` | input, camera, post process, human and UI self-tests |
| `ZoogiHUD`, `ZoogiMenuWidget`, `ZoogiAwakenWidget`, `ZoogiTargetMarker` | UI |
| `ZoogiAnimeSubsystem` | anime events, freeze / restore, cut-in clips |
| `ZoogiFX`, `ZoogiFXLibrary` | FX spawner, Niagara recolour tools |
| `ZoogiTypes`, `ZoogiRoll` | roster, constants, utilities, log category |

## Command-line switches (automation)

`-ZoogiAuto`, `-ZoogiShots=<dir>`, `-ZoogiQuit=<s>`, `-ZoogiPlayers=`, `-ZoogiRounds=`, `-ZoogiArena=`, `-ZoogiPicks=`, `-ZoogiPowerTest`, `-ZoogiUiTest`, `-ZoogiHumanTest`, `-ZoogiRockTest`, `-ZoogiClimaxTest`, `-ZoogiMenuShot`, `-ZoogiMenuArena`, `-ZoogiPopShots`, `-ZoogiCinematic`, `-ZoogiClassicPowers`, `-ZoogiArtifacts`, `-ZoogiAnime=Full|Short|Off`, `-ZoogiNoAnimeMovies`, `-ZoogiNoNiagara`, `-ZoogiNoHUD`, `-ZoogiNoSpaceBackdrop`, `-ZoogiCanvasAwaken`, `-ZoogiCanvasMenu`, `-ZoogiSfxVol=`.

## Content folder (126 files)

| Folder | Contents |
|---|---|
| `Anime/` | `AnimeSequences.csv`; `Movies/` with 4 mp4 + `Movies.csv` (non-asset, staged NonUFS) |
| `Audio/` | 31 `S_*` SoundWaves, `SFX/CREDITS.md` |
| `Env/` | 5 Tripo props (mesh + material + texture each), `Cosmic/` (3 materials, 3 textures) |
| `FX/` | 11 Niagara systems |
| `Maps/` | `Meadow.umap` (the only map; arenas are built at runtime) |
| `Materials/` | 6 materials (no instances) |
| `Orbs/` | `SM_Orb` + material + 3 textures |
| `UI/Portraits/` | 7 portrait textures |
| `Zoogis/` | 7 × (`SM_<id>` + 1 material + 3–4 textures) |
| (none) | no `Data/`, no Level Sequences, no MPC, no Blueprints |
