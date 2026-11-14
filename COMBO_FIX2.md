# COMBO_FIX2 — aerial launch behind-cam + combo clear-aim harden

**Branch:** `cam/pad-clamp2-bypass`  
**Prior tip (failed playtest):** `b50c74b` (combo-clear-aim)  
**This tip:** see `git rev-parse HEAD` after commit below  
**Scope:** camera-only (`ZoogiPlayerController.cpp`). No lock, no push, no package.

## Evan product bar (Oct 23)

> "cam stay behind character when launching from aerial platform."

1. **On aerial pad (aim):** behind-Zoogi looking into the arena — already PASS on merge16 (pad-clear-aim / Clamp2 bypass).
2. **When launching** (fire / leave pad / airborne): do **not** snap to soft tactical top-down — keep behind-character follow through the launch until normal post-shot rules apply.

Priority: aerial-platform behind-cam for Evan; combo gate still needed for ZOOGI package green.

## Why b50c74b failed (combo DRAG TO AIM still soft Tac)

Cook is OK (parent/ZOOGI confirmed merge `40348b9` / block intact in `d26415d`). Logic/gate miss.

`b50c74b` added:

```cpp
const bool bComboClearAim = G->bComboMode && G->IsAimStep() && ... && ZoogiAimView::Enabled() && ViewAspect >= 1.f;
```

Asymmetry vs pad:

| Gate | `bPadClearAim` / `bPadAim` | `bComboClearAim` (b50c74b) |
|------|----------------------------|----------------------------|
| Lock bypass | yes (via `bAimViewWanted`) | yes |
| `ZoogiAimView::Enabled()` | **no** | **yes** |
| On SharedPlatform | `bOnLaunchPad` | n/a (combo floor spawns) |

Exact PASS/FAIL matrix from merge16 playtest: **1v1 pad AIM PASS / combo 2v2 Meadow+Volcanic FAIL**. That matches AimView Enabled soft-off killing combo while pad clear-aim still arms. No AimView override found in Config / pkg-merge16 ini (default CVar is 1), so either runtime override or another Wanted defeat — harden mirrors pad regardless: drop `Enabled()` for combo clear-aim.

Also made the Battle Aim predicate explicit (`Phase==Aiming && Stage==Battle && BattleStep==Aim`) — same as `IsAimStep()`, banner-aligned with DRAG TO AIM / YOUR TURN.

## Aerial launch snap (Evan) — root cause

`zoogi.Cam.PadView` defaults to **0** (SolvePose soft-off — intentional).

`UpdatePadView` early-outed on `!ZoogiLaunchPadCam::Enabled()` and **forced `PadPhase = 0`**.

Sequence on fire from aerial platform:

1. Aim on pad: `Ball->bOnLaunchPad` true → `bPadClearAim` → AimView behind Zoogi. PASS.
2. Fire / leave pad: `bOnLaunchPad` cleared.
3. `UpdatePadView` with PadView=0 zeros `PadPhase` → `bPadView` false.
4. AimView drops the same frame → soft Tac mid-air. **FAIL Evan.**

Intended flight path already existed (`PadPhase` 1→2→land→`PadBlend`, `AimViewYaw = PadLaunchYaw` while `PadPhase==2`) but never ran under default PadView=0.

## Fix (this tip)

1. **`UpdatePadView`:** track pad → flight → land even when PadView=0. Early-out only on `!G || !Shooter || bCinematicCam`. SolvePose stays soft-off (`bPadPoseValid = false`; `SolvePose` still gated on `Enabled()`).
2. **`bComboClearAim`:** drop `ZoogiAimView::Enabled()` (mirror `bPadClearAim`); explicit Battle Aim predicate.

## Confidence

- Aerial launch behind-cam: **high** (causal chain locked; PadPhase flight path already written).
- Combo DRAG TO AIM: **medium-high** (Enabled asymmetry matches PASS/FAIL; no live AimView=0 proof in package ini — if still FAIL after cook, next dig is post-Wanted defeat / Lock path).

## Files

- `Source/ZoogiRoll/Private/ZoogiPlayerController.cpp`
- `COMBO_FIX2.md` (this file)

## Verify after cook

1. 1v1 aerial pad AIM: still behind Zoogi (no SolvePose 2/3 framing).
2. Launch from aerial platform: cam stays behind character through flight; no soft-Tac snap mid-air; blend to normal after land.
3. Combo 2v2 Meadow+Volcanic DRAG TO AIM / YOUR TURN: behind-Zoogi AimView (not soft Tac), even with TargetLock / HumanTest.
