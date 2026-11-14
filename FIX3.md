# FIX3 — PadPhase Query occupancy + Meadow combo clear-aim (Evan Oct 23)

**Branch:** `cam/pad-clamp2-bypass`  
**Prior tip (pkg-merge18 OVERALL FAIL):** `6f277b3`  
**This tip:** see `git rev-parse HEAD` after commit  
**Scope:** camera-only (`ZoogiLaunchPadCam.cpp/.h`, `ZoogiPlayerController.cpp`). No lock, no push, no package.

## Product bar (do not over-fix)

- **Oct 19:** after launch, main follow is always **top-down**. Behind-Zoogi is for **aiming only**.
- **Oct 23:** "stay behind when launching from aerial platform" = the **pad→arena PadPhase flight arc** only (leave SharedPlatform through airborne). **Not** all of ROLL ACTIVE once on the court floor.
- merge18 `1v1_volcanic_L_roll_active_TacFAIL.png`: Zoogi already near arena center under ROLL ACTIVE + soft Tac — **correct post-land top-down**, not proof of mid-flight Tac-snap. Mid-flight frames were not captured (049 still on-pad DRAG TO AIM behind; 050+ already center ROLL ACTIVE Tac).
- Still fix: combo Meadow soft-Tac during **DRAG TO AIM** (aiming-only rule). Volcanic combo already PASS behind AimView.

## Root cause

### Aerial PadPhase never armed under PadView=0

`6f277b3` removed `UpdatePadView`'s `Enabled()` early-out (so PadPhase 1→2→land could run), but left:

```cpp
// ZoogiLaunchPadCam::Query
if (!Shooter || !Enabled()) return false;
```

`UpdatePadView` arms PadPhase only via `Query(...)`. With default `zoogi.Cam.PadView 0`, Query always failed → PadPhase stayed 0 → on fire `bOnLaunchPad` clears and AimView drops the same frame → soft Tac if still airborne. SolvePose correctly stays Enabled-gated (pad-as-gameplay).

**Fix:** Query = occupancy only (`if (!Shooter) return false`). Behind-cam via `bPadView = PadPhase > 0` covers pad→flight only; after land PadPhase clears and main follow is top-down. Do **not** force AimView for all ROLL ACTIVE.

### Combo Meadow soft-Tac during DRAG TO AIM

`6f277b3` dropped `ZoogiAimView::Enabled()` from `bComboClearAim` (Volcanic combo PASS in merge18). Meadow still FAIL under the same Lock-bypass because of remaining flaky gates:

```cpp
&& Shooter && !bShooterLeaving && ZoogiFramed::ViewAspect(this) >= 1.f;
```

Meadow `GComboSpawns` sit near hub pillars; `IsLeavingPlay` / aspect flakes defeat clear-aim while Volcanic passes. CAMWATCH far-from-centre restore also skipped pad but not AimView, so a brief AimView could be yanked back to Tac.

**Fix:**
1. `bComboClearAim`: Battle Aim banner gates only (`bComboMode && Aiming && Battle && Aim && !anime && !cine && Shooter`) — drop leaving + ViewAspect.
2. CAMWATCH Aiming far-restore: also skip while `bAimViewActive`.
3. AimView `ProbeFrom`: raise to `max(60, Portrait::AimProbeHeight()*0.5)` so Meadow pillars don't collapse the chase.

## Confidence

| Case | Confidence | Why |
|------|------------|-----|
| Aerial pad→flight behind (PadPhase) | **high** | Causal: Query Enabled gate blocked PadPhase under PadView=0; flight path already written; TacFAIL shot is post-land OK per Oct 19/23 |
| Post-land ROLL ACTIVE top-down | **high** (must stay) | Intentional — fix does not force AimView for all ROLL ACTIVE |
| Combo Meadow DRAG TO AIM behind | **medium-high** | Leaving/ViewAspect + CAMWATCH match Meadow-vs-Volcanic PASS/FAIL; pillar probe is belt-and-suspenders |
| 1v1 SharedPlatform pad AIM | **high** (regression) | Unchanged pad-clear-aim / Clamp2 bypass |

## Files

- `Source/ZoogiRoll/Private/ZoogiLaunchPadCam.cpp` — Query occupancy without Enabled
- `Source/ZoogiRoll/Public/ZoogiLaunchPadCam.h` — comment: PadPhase flight vs SolvePose
- `Source/ZoogiRoll/Private/ZoogiPlayerController.cpp` — combo clear-aim + CAMWATCH + AimView probe
- `FIX3.md` (this file)

## Verify after cook

1. 1v1 aerial pad AIM: still behind Zoogi (no SolvePose 2/3).
2. Launch from aerial platform: behind through **PadPhase flight only**; after land, normal top-down follow (ROLL ACTIVE Tac OK).
3. Combo 2v2 Meadow **and** Volcanic DRAG TO AIM / YOUR TURN: behind-Zoogi AimView (not soft Tac).
