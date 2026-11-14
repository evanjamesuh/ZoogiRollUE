# Progression core -> Gameplay & UI (menu branch) integration note

Branch `progression` (Oct 14, 2026). Written for the Gameplay & UI agent that owns `menu` (`UZoogiPowerUnlockSubsystem`).
Nothing on `menu` was edited. This note lists what `menu` should consume and where the two overlap.

## What exists on `progression`

| Piece | Where |
|---|---|
| Plain-C++ core (wallet, mastery, focus, hooks, match rewards, daily cap/reset, unlocks, affinity, achievements, titles, cosmetics, purchases) | `Source/ZoogiRoll/Public/ZoogiProgression.h`, `Private/ZoogiProgression.cpp` |
| Owner with real clock + file | `UZoogiProgressionSubsystem` (`ZoogiProgressionSubsystem.h/.cpp`), profile `Saved/SaveGames/ZoogiProgression.json` (+ `.bak`, atomic write; `-ZoogiProfile=<file>` for test profiles) |
| Match listener | `UZoogiProgressionMatchBridge` (world subsystem): reads `AZoogiGameMode::OnMatchEvent` only. Off for `-ZoogiAuto`, `-ZoogiBattleTest`, `-ZoogiEpisodeP5Test`, automation, or `-ZoogiProgression=Off` |
| CPU tiers | `ZoogiCPUBrain.h/.cpp`, `[ZoogiRoll.CPU] Tier`, `-ZoogiCPUTier=` |
| Store | `ZoogiStore.h/.cpp` (`IZoogiStoreBackend`, `FZoogiPurchaseService`) |
| Data (all balance values) | `Content/Data/Progression/*.csv` |
| Results badge (title + border + gems today) | `ZoogiProgressionHUD.cpp` (`AZoogiHUD::DrawProgressionBadge`, Canvas, GameOver only) |

## Overlap with `menu` (conflicts to resolve when merging `menu`)

1. **Two saves.** `menu` saves `UZoogiUnlockSave` in slot `ZoogiUnlocks` (`Gems = 340`, `Stats["void_mastery"] = 3`, `UnlockedPowers`, `EquippedByZoogi`).
   `progression` saves gems, mastery and unlocks in `ZoogiProgression.json`. **Recommendation:** keep `ZoogiUnlocks` only for
   `EquippedByZoogi` (per-Zoogi equipped power) and make `UZoogiPowerUnlockSubsystem` delegate:
   - `GetGems()` -> `UZoogiProgressionSubsystem::Get(this)->Core().Get().Wallet.Total()`
   - `GetStat("void_mastery")` -> `Core().MasteryLevel(EZoogiElement::Void)` (same for fire/shock/frost)
   - `IsUnlocked(id)` -> `Core().IsPowerUnlocked(id)` (keep the `-ZoogiUnlockAll` cheat in menu)
   - `CanUnlock` / `Unlock` -> `Core().CanUnlockPower` / `UZoogiProgressionSubsystem::UnlockPower` (mastery first, else gems via the ledger)
   - one-time migration: if a `ZoogiUnlocks` save has `UnlockedPowers`, record them with `Unlocks.Add({id, "migrated", now})`; do **not** migrate the placeholder 340 gems (Evan's launch default is 0 gems, Level 1 - `Economy.csv start_gems`).
2. **Power costs moved to data.** `menu`'s `BuildPowerDefs()` has 150/200/250/300 gems and Void 2/4/5, Fire 4. The retuned values
   (Evan Q5 pacing) are in `Content/Data/Progression/Powers.csv`: Precision Void 2 / 300, Ember Fire 3 / 400, Crystal Collapse Void 4 / 500,
   Titan Meteor Void 5 / 550. Same power ids as `menu` (`void_1_precision`, `fire_2_ember`, `void_2_crystal_collapse`, `void_2_titan_meteor`,
   `shock_1_storm`, `frost_2_frost`). Name, description and card art can stay in `BuildPowerDefs`; requirements/costs should come from `Core().Config().FindPower(id)`.
3. **Deck filter.** `ZoogiCards::SetPowerUnlockQuery` is still registered by `menu` only. `progression` does not register it (parity).
   Powers.csv `card_id` gives the card for each power if `FZoogiCardDef::RequiredPower` gets filled later.
4. **Focus Element UI.** `SetFocusElement(E, EZoogiPlayContext::Standby, ...)` only succeeds in Standby (Evan Q8). The Standby UI
   should call `UZoogiProgressionSubsystem::SetFocusElement`. Menu changes are refused unless `Economy.csv focus_change_context=Menu`.
5. **Shop / wallet UI.** Show `Wallet.Earned()` / `Wallet.Purchased()` / `EarnedToday()` / `RemainingToday()` / `NextResetLocal()`;
   gem packages from `Config().GemPackages`; purchases only through `FZoogiPurchaseService` (never grant gems directly).
6. **Titles / cosmetics.** `EquipTitle`, `EquipCosmetic(id)`, `EquippedCosmetic(slot)`; slots in `Cosmetics.csv`.

## Hooks still to wire from gameplay code (API ready, not called yet)

The bridge only has what `OnMatchEvent` carries: match start/over, KO, ring-out (no attribution -> element Neutral = Focus),
Episode gauge full, return-to-menu (= forfeit). These need one-line calls from the owning systems:
`Core().HookTrickShot(Tally(), "Trick.BankStrike", element)`, `HookPowerUsed(Tally(), element)`, `HookOrbCollected(Tally())`,
`HookAwakening(Tally())`, `HookKO/HookRingOut` with the causing card's element, and `Tally().Turns / IdleTurns / SignatureHits`.
Get the tally with `UZoogiProgressionSubsystem::Get(this)->Tally()` while `HasOpenMatch()`.
