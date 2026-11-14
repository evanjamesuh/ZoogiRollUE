# Phone Controls — power orbs overlay (ui/phone-orbs-overlay)

Visual reference: Evan revised mockup `approved-omni-sticks.png` / `final-mockup.png` **for the ORBS (and Awaken)**. Omni sticks are optional / off by default.

## Evan direction (2026-10-23 CT via Mobile/ZOOGI) — CONFIRMED

- **YES** on phone-controls, but **NOT** a full omni-stick rewrite.
- Tablet already has aim/power controls — **ADD power orbs** onto that existing tablet UI.
- **Power orbs = ability bubbles + Awaken** (mockup), **NOT** a separate energy/charge set.
- Gameplay & UI: **power orbs overlay**, not dual sticks replacing tablet controls.
- Left/right omni sticks **NOT** the default on tablet (`bDrawOmniSticks=false`).

## What “power orbs” are

The **4 shared ability bubbles** (shield, target/rings, crystalline Z/bolt, 8-point star) + **Awaken bubble** (orange/gold sun, fiery ring) from ab9f805 / merged f12c9e5.

Right stick is power *pull* (not orbs) — sticks **hidden by default**. No second energy/charge orb set.

## Ownership

- **Gameplay & UI (this tip):** look, layout, hit targets, ability/Awaken art, draw flags, Arena AWAKEN button hide, callback stubs.
- **Mobile & Online Play:** touch → game wiring + tablet screenshot. Arena tray stays.

## Defaults (`[ZoogiRoll.PhoneControls]`)

| Flag | Default | Meaning |
|---|---|---|
| `bEnabled` | True | Master enable |
| `bDrawOmniSticks` | **False** | **CRITICAL** — dual sticks OFF; tablet keeps existing aim/power; Arena tray stays |
| `bDrawAbilityOrbs` | **True** | Draw the 4 ability bubbles |
| `bDrawAwakenOrb` | **True** | Draw Awaken sun |
| `bHideArenaAwakenWithOrbs` | **True** | When Awaken orb drawn: hide Arena `UZoogiArenaHUDWidget` large gold AWAKEN button (one path). **Charge pips stay.** |
| `bUseOrbScreenAnchor` | False | When True, pivot orb arc on `OrbAnchorXPct`/`YPct` instead of `RightStickCX`/`CY` |
| `OrbAnchorXPct` / `OrbAnchorYPct` | 0.89 / 0.81 | Optional screen-% thumb zone |

Existing size/angle knobs still place the orb arc around the (possibly invisible) right-stick anchor.

## Cmdline

| Flag | Effect |
|---|---|
| `-ZoogiNoPhoneControls` | Force off |
| `-ZoogiPhoneControls` | Force on |
| `-ZoogiPhoneControlsSticks` | Force draw omni sticks (PC / mockup review only) |

## Hit-test when sticks are hidden (Mobile contract)

- Claims **only** `PhoneAbility` (index 0–3) and `PhoneAwaken`.
- Does **NOT** claim `PhoneAim` / `PhonePower` / `PhoneCancel` — tablet aim/power widgets keep those touches.

## Mobile wiring contract (delegates)

| Delegate / Notify | Mobile wires to | While sticks off |
|---|---|---|
| `OnAbilityIndex` / `NotifyAbilityIndex` | Shared-ability / card | **Wire this** — orb taps |
| `OnAwakenPressed` / `NotifyAwakenPressed` | `PressAwaken` / `MenuAction("AwakenUse")` | **Wire this** — Awaken orb (Arena large button hidden) |
| `OnAimAxis` / `NotifyAimAxis` | Left stick AIM | **Unwired** — tablet aim owns it |
| `OnPowerPull01` / `NotifyPowerPull01` | Right stick pull | **Unwired** — tablet power owns it |
| `OnFireRelease` / `NotifyFireRelease` | Release + flick | **Unwired** while sticks off |
| `OnCancelAbort` / `NotifyCancelAbort` | Cancel ring abort | **Unwired** while sticks off |
| `OnStandbyLookAxis` | Stub look-around | Stub |
| `OnStandbyPeek` | Stub peek/emotes | Stub |

## Awaken one-path

When `bDrawAwakenOrb` + `bHideArenaAwakenWithOrbs` (defaults): Arena HUD skips building the large gold AWAKEN hit target / chrome; charge pips still refresh. Phone Awaken orb → `OnAwakenPressed` → Mobile → `PressAwaken` / `AwakenUse`.

Physics / camera untouched. Tip-ready for ZOOGI merge into `combined-hud-tripo` @ f12c9e5.
