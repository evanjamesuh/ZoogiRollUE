# Intentional differences: Unreal v3 (branch `cards`) vs the browser version

Baseline: the Phase 1 audit (PHASE1_OWNERSHIP.md §5 item 10). The browser build has the **Awaken artifact hand only** (draw 1 of Ember/Storm/Frost/Titan/Void; Infuse / Guard / Save) and **no card deck**. Per the spec these differences are documented, not ported back.

| Area | Browser version | Unreal v3 with cards on (`bCards=True` or `-ZoogiCards`) | Why |
|---|---|---|---|
| Pre-shot choice | Awaken: pick 1 drawn/saved artifact, then Infuse/Guard/Save | **Standby:** draw 5 cards, 5 Card Energy refreshed each turn, select any affordable set (max per commit from the ini), Confirm. Episode Awakening is a separate toggle (A key / button) | card book rules (cb.txt:1256) |
| Artifact names | Ember/Storm/Frost/Titan are artifacts | the same names are **cards** with card-book semantics (e.g. Frost = shared low-friction lane, Titan = mass x1.35 restored at turn end) | card book; the artifacts are kept, but only when cards are off |
| Void | artifact | not a card (not in the 15) | card book |
| Force labels | SOFT/MED/FULL | CONTROL/DRIVE/LAUNCH when cards are on (same impulses) | card book |
| Card effects | n/a | 15 cards applied once per contact via stable contact ids and an apply-once ledger | spec |
| Crystal Collapse | arena destruction off (`bDestruction=False`) | breakable pieces only for this card, as an exemption | Evan, Oct 13 7:58 PM |
| Mutual final KO (2v2) | last-hit team wins | **draw** when both teams' last Zoogis go out on the same action (cards on only) | card book :1261-1262 |
| Partner Follow-Up economy | team Combo gauge from 0, normal gain | **PROPOSED:** each team starts at 30 (one follow-up) and Combo gain x1.5 (cards on only) | so normal matches reach follow-ups |
| Episode Gauge | +gains to cap 100 | the same cap of 100, spent once for a 12 s / 4 × 3 s personalized episode built from ledger roles; **PROPOSED** gain x3.5 when cards are on | spec (Phase 5) + P6 tune |
| Episode recap unlock | — | PROPOSED rules U1-U4 (full/partial per player) | Phase 5 |
| CPU | artifact pick + Infuse/Guard | card planner through the same rules as a human; Standby Awakening from the classic CPU inputs | spec |
| Save / resume | none | SaveGame of the full match state; resume restores the identical hash (P6) | spec |
| Duplicate input | n/a | the same gameplay action in the same frame (e.g. touch + mouse) is applied once | spec |
| HP | none | none. Guard/Anchor "absorb 12" and the Momentum Steal x0.6 damage are display/log only; ring-outs decide the match | no HP model (Phase 1 decision) |
| Precision / Heavy Roll aim error | n/a | aim is deterministic, so the error envelope scales zero; Precision's visible part is the forecast | no aim-noise model |

With cards **off** (default `bCards=False`), the Unreal build keeps the pre-v3 rules. Its parity hash, roll ledger `e3f55c2b`, is unchanged through Phases 2-6.
