# Changelog: v3 cards (branch `cards`)

## v3.0.0 (2026-10-13), Phases 2-6
### Preserved
- Cards off (`bCards=False`, the default) is bit-identical to pre-v3: the roll parity ledger hash stays `e3f55c2b` in every phase.
- The classic Awaken artifact flow, 2v2 Combo rules (Follow-Up cost 30, Ring Marks, Recovery Gate), the existing Episode ledger (`ev_<seq>_<kind>` ids) and its presentation are all preserved. The Phase 5 director only reads the ledger.
- Input schemes (mouse, keyboard, touch), HUD and UMG Awaken panel (Canvas fallback still available).

### Fixed
- Commit applies once per player turn, and invalid selections spend nothing (P2).
- Titan mass is restored to the actual previous value (no drift), and an effect applies at most once per contact episode (P3).
- Seed-11 effects rotation: 5 cards armed per turn (P4 test harness).
- The CPU uses Awakening in cards mode under the human rules (P5).
- **P6:** a duplicate gameplay input in one frame (e.g. touch + synthesized mouse, double Confirm) is applied once. The validator's save parsing accepts comma lists. The full-state hash excludes wall-clock fields (match id / timestamps) so replays compare.

### New
- **P2** card data (Cards.csv / Decks.csv / toolkit DT_ZoogiCards), the rules core, Standby, CPU planner, Crystal Collapse exemption.
- **P3** all 15 card effects in real physics (FZoogiCardEffectCore + world glue).
- **P4** UMG card hand on RLDB WBP_CardVisual, team combos and minigames with cards.
- **P5** the Battle Episode Engine extension: roles, gauge spend, branches, skip-safe playback, recap unlocks (PROPOSED).
- **P6:**
  - `UZoogiMatchSave` SaveGame + `FZoogiSaveAccess` (capture / restore / full hash) and `-ZoogiSaveTest=boundary,commit,followup,episode`;
  - the `-ZoogiMatchTest` complete-match validator (stall, result, follow-ups, shatters, recoveries, awakenings, episodes, saves, hashes) and `-ZoogiDemoShots`;
  - `CARD_COV` coverage, `ZoogiRoll.P6.*` automation tests, and the `keys` / `dup_input` runtime tests;
  - PROPOSED earning tune (Combo x1.5, start gauge 30, Episode x3.5; overrides `-ZoogiComboGainMul=`, `-ZoogiComboStart=`, `-ZoogiEpisodeGainMul=`);
  - packaging: DT_ZoogiCards / RLDB widgets cooked, `Content/Data` CSVs staged, `Scripts\package.ps1 -Root`.
