# Assignment for the Unreal Engine AI: Milestone "Awaken", assets, VFX, UI, camera and anime playback

Copy and paste this whole file as the task.

## Context and rules

- **Project:** UE 5.6, `C:\dev\ZoogiRollUnreal`. Gameplay is C++ (module `ZoogiRoll`). Content is mostly created by Python scripts in `Scripts/`, run through `Scripts/run_python.ps1`.
- **Do not start over.** Extend the existing actors, materials and Niagara systems. Don't add a second GameMode, HUD, camera manager or FX spawner.
- **Read first:** `Docs/MilestoneAwaken/REVIEW.md`, `ASSIGNMENT-CURSOR.md` (the C++ API you hook into), `ASSIGNMENT-AURA.md` (the visual spec) and `ANIME-SEQUENCE-MATRIX.csv`.
- **Gameplay rules:** Zoogis stay **spheres with no limbs**. Transformations are temporary overlay states on the existing meshes. Use clickable controls only.
- **Naming:** wherever the spec says "Flare", use **Hotstreak**.
- **Git:** commit locally only. Don't push, and don't touch `C:\dev\ZoogiRollUE`, `C:\dev\pg` or port 5000.

## Step 0: inspect what already exists (report before changing anything)

| Area | Existing asset or code |
|---|---|
| Zoogi meshes | `/Game/Zoogis/<id>/SM_<id>` (wolfgang, hotstreak, lars, pinpoint, bolt, wraps, nightshade) with GLB-imported materials. **Never edit these materials.** |
| Materials | `/Game/Materials/M_ZoogiSurface`, `M_ZoogiGlow`, `M_ZoogiGround`, `M_ZoogiFX` (additive: Color, Strength, Ring). Created by `Scripts/setup_content.py` and `Scripts/make_fx.py`. |
| Niagara | `/Game/FX/NS_Burst_{Blue,Gold,Green,Purple,Red,White,Yellow}`, `NS_Explosion`, `NS_Sparks`, created by `Scripts/make_niagara.py` from engine templates. Recoloured with `UZoogiFXLibrary::TintNiagara` and `ScaleNiagaraFloats`. |
| FX spawner | `AZoogiFX::Spawn`, `::SpawnTrail`, `::Niagara(World, "NS_x", ...)`, `::PowerBurst` (`ZoogiFX.cpp`) |
| Ball | `AZoogiBall`: `Sphere` (physics root, r=50), `Visual` (mesh), `Ring` (`M_ZoogiGlow` dynamic material instance), `TurnLight`; `OnSphereHit` impact sparks |
| Camera | `AZoogiPlayerController::UpdateCamera`/`ApplyPostProcess`, `namespace ZoogiCamTune` (profiles PDraw…PTargetCU), `OnImpact` (shake, FOV punch, hit-stop via `SlowMoUntil`) |
| UI | Canvas `AZoogiHUD` (scoreboard, tracker, aim, cards, buttons via `Button()` → `MenuAction`); C++ UMG `UZoogiMenuWidget` (`UZoogiButton` non-focusable) |
| Audio | `/Game/Audio/S_<Name>` via `ZoogiUtil::PlaySfx` |
| Missing | Media Framework, Level Sequences, material parameter collections, overlay materials, DataTables or DataAssets |

## Tasks

### 1. Artifact materials (overlay approach)

- Create **`M_ZoogiArtifactOverlay`**: translucent or additive, two-sided off, fresnel rim, panner noise and a mask texture slot.
- **Parameters:**
  - Vectors: `CoreColor`, `RimColor`
  - Scalars: `CoreGlow`, `MaskAmount` (0→1 spread animation), `MaskTiling`, `Pulse`, `PulseSpeed`, `Distort` (Void), `Darken` (Titan/Void), `Frost` (adds white roughness look)
  - Texture: `Mask` (cracks, veins, frost, plates or noise)
- Create 5 instances, `MI_Artifact_<Ember|Storm|Frost|Titan|Void>`, plus `MI_Guard_<...>` for the protective bubble.
- **Apply:** `Visual->SetOverlayMaterial(MID)`, which works on `UStaticMeshComponent` in UE 5.6. Make a dynamic instance per Zoogi, owned by `UZoogiArtifactComponent`, and animate `MaskAmount` from 0 to 1 over about 0.6 s during the Infuse activation.
- **Titan plates:** add a slight Visual scale-up (×1.04) plus a darker overlay. If the Aura masks aren't ready, make an optional instanced "plate" shell mesh.
- **Guard bubble:** a sphere mesh (r about 62) with `M_ZoogiFX`-style fresnel, attached to `Sphere`, hidden by default.
- **Global material parameter collection** `MPC_ZoogiAnime`: `AnimeDim` (0-1, darkens the arena during activations), `SpeedLines`, `Time`. Use it only for global effects. Per-Zoogi state stays in dynamic instances, because a collection is global and can't differ per Zoogi.

### 2. Niagara (per artifact: aura, trail, impact, guard)

- Build from the engine templates already used by `make_niagara.py`, so they stay script-reproducible. Add `Scripts/make_artifact_fx.py`.

| System | Template base | Notes |
|---|---|---|
| `NS_Aura_Ember` | HangingParticulates + BlowingParticles | rising embers, heat-haze sprite (refraction) |
| `NS_Aura_Storm` | DynamicBeam / StaticBeam | 2-3 orbiting arcs (beam endpoints on an orbit) |
| `NS_Aura_Frost` | HangingParticulates | low cold mist, glitter |
| `NS_Aura_Titan` | UpwardMeshBurst (small, looping) | floating pebbles |
| `NS_Aura_Void` | SingleLoopingParticle (ring meshes) | 2 tilted orbiting rings, inward motes |
| `NS_Trail_<Artifact>` | LocationBasedRibbon | fire / lightning (jagged) / ice / dust / dark smear ribbons; Frost also spawns floor decals that fade over 2 s |
| `NS_Impact_<Artifact>` | RadialBurst, DirectionalBurst, SimpleExplosion | flame ring, lightning burst + chain beams to the targets the C++ passes in, ice shard fan, shockwave ring + debris, implosion → release |
| `NS_Guard_<Artifact>` | OmnidirectionalBurst | shield break flash |

- Expose user parameters `User.Color`, `User.Scale`, `User.Intensity` and (for Storm chains) `User.TargetPositions` (array).
- **Normal hits keep `NS_Sparks`.** Powered hits use `NS_Impact_*`.
- **Spawning:**
  - Aura and trail are **attached** `UNiagaraComponent`s owned by `UZoogiArtifactComponent`. The trail is active only while the ball is moving faster than about 150 cm/s.
  - Impacts go through `AZoogiFX::Niagara`.

### 3. Ball animation hooks (no limbs)

Add these in `UZoogiArtifactComponent` / `AZoogiBall`, as C++ or Blueprint-callable:

- **`PulseCore(Strength)`:** `TurnLight` intensity and colour + overlay `CoreGlow` spike.
- **`Squash(Amount, Dir)`:** insert a `SquashRoot` (absolute rotation along velocity) between `Sphere` and `Visual`. `Visual` copies the body rotation, so it still rolls. Scale it to (1+a, 1−a/2, 1−a/2) for ≤ 0.15 s on launch and impact.
- **`Wobble`, `SpinBoost`:** small rotation noise on Visual, and extra angular velocity at launch. These are visual only, not physics.
- **Shell expansion:** Visual scale 1.0 → 1.06 → 1.0 during PowerUp.

### 4. Awaken UMG

- Create `UZoogiAwakenWidget`, built in C++ like `UZoogiMenuWidget`, and optionally reparented as `WBP_Awaken` for layout polish.
  - Drawn card, saved cards, and **INFUSE / GUARD / SAVE** buttons.
  - The buttons are non-focusable `UZoogiButton`s that call `AZoogiGameMode::MenuAction("Infuse"|"Guard"|"Save"|"AwakenSelect"|"AwakenDone", idx)`.
- Battle aim controls (◀ ▶ nudges, SOFT/MED/FULL, CONFIRM, CANCEL) can be Canvas buttons in `AZoogiHUD::DrawAim`, matching the current style, or UMG. Pick one and be consistent.
- Draw the aim preview (ghost contact, arrows, bank marker) on the HUD from `FZoogiAimPreview`.
- **Input:** make sure UMG clicks don't also reach `HandleInput`'s world pick. Use the existing non-focusable button pattern, and add a guard flag (for example `bUIClickThisFrame`, set by the widget) that `HandleInput` checks before picking. No such guard exists today; the main menu avoids the problem because world picking is off in the Menu phase.

### 5. Camera emphasis

- Implement `PTactical` and the small emphasis rules from REVIEW.md section 8:

| Rule | Value |
|---|---|
| Zoom on active | −10% boom, 30% look bias |
| Launch tracking | follow the shooter-target midpoint, then the ball |
| Impact shake | 40% of the current amplitude |
| FOV punch | 3° |
| Optional hit-stop | 0.06-0.1 s, powered hits only |
| Auto-return | about 0.5 s |

- Keep the dramatic profiles as named shots for the anime subsystem: `Shot_HeroIntro` (PDraw), `Shot_LockOn` (PBattle), `Shot_TargetCU` (PTargetCU), `Shot_Aerial` (PEnd).
- Add the `CameraStyle = Tactical | Cinematic` setting.

### 6. Physics hooks (artifact effects; values in the DataAssets)

- **Ember / Titan:** radial impulse at the contact point (`AddImpulse` per body in radius, or reuse the Explosion push code in `ZoogiPowers.cpp`).
- **Storm:** pick up to 2 nearest bodies, apply a push, and pass their positions to `NS_Impact_Storm`.
- **Frost:** set the target's `RollingDecel` to the ice value for 1 turn (Frozen status), and spawn the shard push.
- **Void:** for 0.6 s, apply a force toward the impact point to bodies in radius (in `TickPowers`), then release.
- **Titan Infuse:** `Sphere->SetMassOverrideInKg` ×1.7 during the shot, restored in `EndPowers`/Resolve.
- **Guard:** in the guarded ball's `OnSphereHit`, scale the incoming velocity change (knockback ×0.6) and apply the flavour effect.

### 7. Anime playback (inside `UZoogiAnimeSubsystem` back-ends)

- **Real-time:** request a camera shot + an FX preset + `MPC_ZoogiAnime.AnimeDim` + a speed-line overlay widget (a material on a UMG Image) for N seconds. Gameplay is frozen by the subsystem's snapshot and pause.
- **Level Sequence:**
  - Play `ALevelSequenceActor` with `ULevelSequencePlayer`, bound to the **live** Zoogi actors via binding overrides, so the current overlay, aura and trail state carries over (continuity).
  - The sequence animates only a cine camera, lights and FX actors. It must **not** keyframe gameplay actor transforms. Use spawnable proxy copies if the Zoogi has to move.
  - On finish, restore the camera and the world snapshot.
- **Media (pre-rendered):**
  - `UMediaPlayer` + `UMediaTexture` on a fullscreen UMG Image (`WBP_AnimeOverlay`). `UFileMediaSource` points to `Content/Movies/<id>.mp4`.
  - **Preload:** call `OpenSource` with `PlayOnOpen=false` when the event becomes possible (on Infuse, preload that artifact's PoweredHit/Critical clips), then `Play` on emit.
  - Add `Content/Movies` to Project Settings → Packaging → "Additional Non-Asset Directories to Copy".
  - H.264 MP4, 1920×1080, 30 or 60 fps. Electra or WMF on Windows; Electra on mobile.
- **Transitions:** a 0.15-0.25 s white flash or radial wipe in and out (a UMG animation). The out-transition reveals the **restored** frame.
- **Return-to-state test:** log the positions and velocities of all bodies before and after playback. The difference must be < 0.1 cm and < 0.1 cm/s.

### Format decision table (per tier)

| Tier | Length | Default format | Why | Use pre-rendered video when… | Continuity handling |
|---|---|---|---|---|---|
| **Micro** | 0.5-1.5 s | **Real-time** (camera shot + Niagara + material param + overlay widget) | zero load, always matches state, cheap to vary per Zoogi | never | automatic (live actors) |
| **Standard** | 2-4 s | **Level Sequence** (Sequencer, stylised post, live-actor bindings) | matches the Zoogi's look and artifact state exactly; reusable across 7 Zoogis via bindings | a hand-drawn effect is the star (use **Hybrid**: Sequence + an alpha video overlay of flames, speed lines) | bindings to live actors; restore snapshot |
| **Special / Combo** | 4-7 s | **Hybrid** (Sequence + Media overlay with alpha, or sprite flipbooks) | needs anime smears and impact frames that are hard in real time, but must still show the actual Zoogis | fully stylised 2D cut-in with no gameplay actors visible | in/out frames are real-time; video only in the middle |
| **Ultimate** | 7-12 s | **Pre-rendered video** (Media Framework), optionally Hybrid | maximum anime quality and cost control; plays rarely | always, but author **per Zoogi** (7) and use a generic per-artifact tail | start and end on real-time frames: freeze → flash → video → flash → restored arena; the video tints its last frames to match |

## Definition of done

- [ ] Step 0 inventory written to `Docs/MilestoneAwaken/UNREAL-INVENTORY.md`.
- [ ] 5 overlay material instances + 5 guard looks, applied at runtime to all 7 Zoogis with no change to the GLB materials. Screenshots of each artifact on at least 2 Zoogis.
- [ ] 20 Niagara systems (aura, trail, impact, guard × 5), created by script, readable on all 7 arenas.
- [ ] Squash, pulse, spin and shell-expansion hooks working. No limbs.
- [ ] Awaken UMG works with the mouse (Infuse, Guard, Save), with no click-through to world picking.
- [ ] The tactical camera is the default with small emphasis, plus a working Cinematic toggle.
- [ ] Physics effects for all 5 Infuse and 5 Guard effects, driven by the DataAssets.
- [ ] Anime: one real-time Micro (`Anime_Any_Ember_Infuse_01`), one Level Sequence Standard (`Anime_Hotstreak_Ember_Critical_01`) and one Media placeholder (any `Anime_Any_Any_KO_01` test MP4). Each preloads, transitions and returns to the exact state (logged).
- [ ] Packaged Win64 build plays all three.
- [ ] Local commits only.
