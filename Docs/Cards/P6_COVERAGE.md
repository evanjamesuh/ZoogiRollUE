# Card coverage report (Phase 6, event-supported)

Every number comes from `CARD_COV` lines written by `UZoogiCardSubsystem::DumpCoverage` at match end:
- **selected** = Select/Toggle accepted by the rules (human or CPU);
- **committed** = `commit_card` ledger events;
- **condition** = the card's world condition was actually met (e.g. a real wall/bumper contact for Bank Strike, a qualifying enemy impact for Guard, the shooter touching an orb for Orb Relay, being inside the recovery radius for Recovery Roll), counted once per (card instance, event);
- **triggered** = the effect really applied (FxUses: apply-once ledger hits).

## A. Natural CPU complete matches (no fixtures)
Runs: 2v2 seeds 3/7/11/13 (k*), FFA seed 7 (hf7, hf7save, ffa7). Totals across 7 matches:

| Card | Selected | Committed | Condition met | Triggered |
|---|---|---|---|---|
| Storm | 79 | 79 | 79 | 79 |
| Bank Strike | 35 | 35 | 27 | 27 |
| Guard | 68 | 68 | 14 | 14 |
| Orb Relay | 39 | 39 | 16 (shooter touched an orb) | 4 (partner relayed the same orb) |
| Heavy Roll | 75 | 75 | 75 | 75 |
| Titan | 34 | 34 | 34 | 34 |
| Precision | 51 | 51 | 51 | 51 |
| Crystal Collapse | 8 | 8 | 7 | 7 |
| Recovery Roll | 16 | 16 | 7 | 7 |
| Ember, Frost, Momentum Steal, Orb Magnet, Double Impact, Anchor Guard | 0 | 0 | 0 | 0 |

The CPU planner never picked the last six cards in these seeds (it favours the cheap launch/defence cards from the default deck). This is a CPU-strategy limitation, not a rules gap.

## B. Focused scenarios for the cards not hit naturally
`-ZoogiCardTest=effects` (5 cards armed per turn, real rolls) PASS on two seeds; FxUses (triggered):
- seed 7 (204 checks / 0 fails): anchor_guard 2, bank_strike 6, double_impact 4, **ember 21, frost 4, momentum_steal 3, orb_magnet 4**, guard 6, heavy_roll 10, orb_relay 2, precision 8, recovery_roll 3, storm 8, titan 9
- seed 11 (183 / 0): adds crystal_collapse 1; anchor_guard 2, double_impact 4, ember 16, frost 4, momentum_steal 4, orb_magnet 4 ...
- `crystal_collapse` PASS 26/0 and `mutual_ko` PASS 20/0, plus 17 `ZoogiRoll.Cards.Fx.*` automation tests (one per effect, including Anchor Guard settle arming, the Double Impact 2-burst cap, the Frost no-stack rule and the Ember overlap clear).

**All 15 cards are covered.** Nine are covered end to end (selected → committed → condition → triggered) in natural CPU matches. The other six have their condition met and their effect triggered in the focused real-physics effects scenario, where the test arms them each turn, and in their dedicated automation tests.
