# Zoogy Roll v3.0.0 Card-Driven Rolling Combat: Phase 1 (inspection and ownership)
2026-10-13, read-only. Code: PC `EVAN` worktree `C:\dev\ZoogiRollUnreal-episode` (branch episode-engine, HEAD 8c73614, 2026-10-09 17:47 CT, 5 uncommitted lines not read). Not modified, Unreal not launched, nothing built.
Paths are relative to `Source/ZoogiRoll/`. Rule sources: `C:\Users\skybi\Downloads\Zoogy_Roll_Full_Roster_and_Card_Book.pdf` (box copy `card-system/cardbook.pdf`, text `cb.txt`) and `Zoogy_Roll_Complete_Development_Package_Revised.pdf` (`devpkg.pdf`/`dp.txt`). Prior audit: `../episode-engine/AUDIT_2026-10-08.md`.
Labels: **VERIFIED-CODE** (file:line), **VERIFIED-DOC** (card book page/line in cb.txt), **PROPOSED**, **MISSING**.

## 0. Roguelike Deckbuilder Toolkit (UPDATE Oct 13 8:16 PM: found as a sample project)
**Update:** the toolkit is a Fab *sample project*, not an engine plugin: `C:\Users\skybi\OneDrive\Documents\Unreal Projects\RoguelikeDeckbuilderToolk\` (EngineAssociation 5.6, **Blueprint-only**: no Source, no Plugins; 684 content files / 230 MB, none online-only). Read-only copy: `C:\dev\toolkit-src`. Content `/Game/RLDB` (632 assets, all load in ZoogiRoll with 0 failures) + `/Game/ArrowTrail`. Its GameInstance (`BP_GI_RLDB`) is NOT adopted. See section 4 for the real class map. The original Phase 1 search notes follow.

Searched: project `Plugins\` (neither the main tree nor the worktree has a Plugins folder), `UE_5.6\Engine\Plugins\Marketplace` (only `Aura`), `C:\ProgramData\Epic\EpicGamesLauncher\VaultCache` (only `FabLibrary`, `FREEMounc1f4bef6b908V1`), `%LOCALAPPDATA%\EpicGamesLauncher\VaultCache` (doesn't exist), Downloads, and every `*.uplugin` on C:/D: to depth 5 whose name contains deck/rogue/card. Nothing found.
**What Evan has to do:** in the Epic launcher, go to Fab Library > Roguelike Deckbuilder Toolkit > Install to Engine (UE 5.6). If it's a code-and-content project instead, copy it into `ZoogiRollUnreal\Plugins\`. Then tell me the folder so I can read its real classes. Until then, this document **doesn't assume any toolkit API**. Section 3 maps by *role* only. The fallback is native C++ owners, which this game already uses for everything else.

## 1. Current phase flow (VERIFIED-CODE)
- Legacy stage FSM: `EZoogiStage` Draw/Awaken/Battle/Resolve/End and `EZoogiBattleStep` Aim/PowerUp/Rolling (`Public/ZoogiGameMode.h:84,95`). Turn order: classic rotation `ZoogiGameMode.cpp:411-435`; 2v2 team then teammate alternation `ZoogiCombo.cpp:244-272,464-496`.
- Battle flow overlay `UZoogiBattleStateMachine` (episode-engine T1): WAIT_FOR_COMMAND > ACTION_PREVIEW > MINIGAME | PHYSICS_RESOLVE > SHATTER_FAILURE > RESULT > EPISODE_EVENT > NEXT_TURN > MATCH_END (`ZoogiBattleTypes.cpp:11-23`). Transitions are validated in `IsValidTransition`/`Transition` (`ZoogiBattleStateMachine.cpp:162,182`); launch/objective commit `:515-530`; resolve `:548`; shatter `:664`; episode `:691`. Turn advance is still timer-driven in GameMode (audit row 16).
- Draw: the Awaken stage draws one artifact with GameRng (`ZoogiAwaken.cpp:123,127`). Hand cap `UZoogiArtifactComponent::HandCap` (trim at `ZoogiAwaken.cpp:50`), Infuse/Guard/Save (`ZoogiAwaken.cpp:148/172/191`).
- Controls: click target = lock, click again = confirm, click floor = free aim (`ZoogiPlayerController.cpp:755-764`, audit row 1). Aim nudges `<< < > >>` (`ZoogiAwakenWidget.cpp:555-558`) and force AUTO/SOFT/MED/FULL (`:572`), CONFIRM/CANCEL. No flick or drag strength. Launch: `FireShot` > `AZoogiBall::Launch`.
- Physics: per-ball mass override `ZoogiBall.cpp:81` and restore `:126` (already a cache-and-restore mass pattern, usable for Titan). Floor friction material `ZoogiArena.cpp:497`. 12 s physics timeout (audit row 16).
- Character abilities: `EZoogiPower` (WolfPack, Explosion, Ricochet, LockOn, StaticShock, BandageBind, ShadowStun, RuneSeal; `ZoogiFX.cpp:86-93`). Charges max 3 (`ZoogiPowers.cpp:26`, `ZoogiBall.h:83`); spend `ZoogiPowers.cpp:111,255`; star orb +1 (`ZoogiAwaken.cpp:542`). The "classic" powers path is behind `-ZoogiClassicPowers` (`ZoogiGameMode.h:309`). There are **no cooldowns** for abilities.
- Orbs: ORB OUT scoring and relay (`ZoogiCombo.cpp:740-765,820-830`), Awaken/star orbs, Resonance surge gives +1 draw (`ZoogiCombo.cpp:863-870`).
- Ring-out, recovery, victory: RingMarks (2 = KO, `ZoogiCombo.cpp:789`), Recovery Gate respawn (`ZoogiCombo.cpp:833-846`), `CheckKnockoffs` and `FinishGame`.
- Team Combo: team gauge `ComboEnergy[2]`, Partner Follow-Up cost `ZoogiComboTune::FollowUpCost` = 30 (`ZoogiCombo.cpp:443-444,899`), or paid from Episode Energy through `FollowUpCostSource` (`DefaultGame.ini [ZoogiRoll.EpisodeEnergy]`).
- Episode Energy: per-Zoogi `UZoogiEpisodeEnergyComponent`, Cap=100, StrongHitSpeed=900, ImpactCap=4 (`Config/DefaultGame.ini` [ZoogiRoll.EpisodeEnergy]; `ZoogiBattleStateMachine.cpp:216`).
- Awakening/reaction: `UZoogiReactionComponent` with ActivationRule=InfuseOrGuard, AwakenTurns=1, ReactionsPerAwakening=1 ([ZoogiRoll.Awakening]).
- Minigames: Halo Bank, Orb Line, Air Reaction and Rail Release, plus SkyBumper (`ZoogiMiniGame.*`, `ZoogiBattleHUD.cpp:68-70,214-220`, `ZoogiAerialPlay.cpp:157-203`). Failure goes to SHATTER_FAILURE and then ordinary resolution. MiniGameInputTimeout=6 and FollowUpOfferTimeout=12 ([ZoogiRoll.Battle]).
- Unlocks: [ZoogiRoll.Unlocks] FullEpisodePerf=12, FullEpisodeMinSegments=2, FullEpisodeMaxShatters=1; `ZoogiEpisodeLedger.*`.
- CPU: `PlanCPUAwaken` (`ZoogiAwaken.cpp:213-238`), power use `ZoogiGameMode.cpp:829`, prop routes `ZoogiAerialPlay.cpp:822-852`. All of these draw from GameRng.
- RNG: `GameRng` FRandomStream is used for gameplay. Cosmetic effects use their own streams (`ZoogiAerialFX.cpp:76`).
- Save/resume: **MISSING**. There's no USaveGame anywhere in Source.
- Tests: `-ZoogiBattleTest=<case>` (`ZoogiBattleTest.cpp`, about 50 cases, e.g. `:54-55,528,559`) and `ZoogiEpisodeTest`, `ZoogiShotTest`, `ZoogiFrozenPlayTest`, `-ZoogiUiTest` (43 checks). There are no UE Automation Spec tests.

## 2. The "15 established card types" and their rules
**Found in the card book PDF (VERIFIED-DOC), not in code.** The code has only 5 artifacts: `EZoogiArtifact` None/Ember/Storm/Frost/Titan/Void (`Public/ZoogiArtifacts.h:17-26`). Their data comes from a built-in table plus `DA_Artifact_<Key>`. Ember, Storm, Frost and Titan exist in code as Infuse/Guard artifacts, but with *different* semantics from the cards. Void isn't in the 15. The browser repo (GitHub search) and the /workspace notes have no deck rules beyond the Awaken artifact hand.

| Rule | Value | Status / source |
|---|---|---|
| Hand draw per turn | 5 | VERIFIED-DOC cb.txt:1256 ("Draw five") |
| Card Energy refresh per turn | 5 | VERIFIED-DOC cb.txt:1256 |
| Phase where cards are chosen | Awaken/Standby | VERIFIED-DOC cb.txt:1256 |
| Aim modes | CONTROL/DRIVE/LAUNCH | VERIFIED-DOC cb.txt:1256. Code has SOFT/MED/FULL (`ZoogiAwakenWidget.cpp:572`), so a mapping is needed |
| Lead rolls per turn | 1; one partner follow-up for 30 Combo; no third shot | VERIFIED-DOC :1257; 30 = VERIFIED-CODE `ZoogiCombo.cpp:443` |
| Card instances | one instance per committed lead action | VERIFIED-DOC (every card footer) |
| Win | 2 Ring Marks (1st = Recovery Gate, 2nd = KO); both opponents KO wins; mutual final KO = draw | VERIFIED-DOC :1261-1262; RingMarks VERIFIED-CODE `ZoogiCombo.cpp:789` (the draw rule isn't verified in code) |
| Episode | 100 = spend once on a 12 s branch, 4 x 3 s perspectives | VERIFIED-DOC (rules page); Cap 100 VERIFIED-CODE ini |
| Deck size / composition | 11 "evaluation decks" are mentioned, but their contents are in an archive (JSON/CSV) that **isn't on the PC** | MISSING. Proposed: 12 cards, max 2 copies |
| Discard/reshuffle, hand cap carry-over | none | MISSING. Proposed: unspent cards are discarded at turn end, reshuffle when the deck is empty |
| Ability cooldowns | none in doc or code (Charges 0-3 only) | Charges VERIFIED-CODE; cooldowns MISSING |
| Combo costs beyond follow-up | none | MISSING |
| Card tuning numbers | doc: "Added tuning values are proposed evaluation defaults" | VERIFIED-DOC, but the doc itself calls them proposed |

The 15 cards (cost / type / expiry come from the card book, cb.txt:516-1215; all are VERIFIED-DOC):
| # | Card | E | Type | Rule essence | Expiry | Existing hook |
|---|---|---|---|---|---|---|
| 1 | Storm | 1 | Infusion | temporary multiplier on the roll-control property; no auto-aim | TurnEnd | artifact Storm (other semantics) |
| 2 | Bank Strike | 2 | Technique | mark a bank; a real wall/bumper contact plus ally opening emits BankSetup | LeadActionEnd | bank preview `ZoogiTargeting.cpp:338-400`, BankSetup trigger `ZoogiCombo.cpp:587,596` |
| 3 | Guard | 1 | Defense | absorb 12 dmg, x0.75 enemy impulse once | NextQualifyingEnemyImpact | Awaken Guard (auto damping) |
| 4 | Orb Relay | 1 | Utility | neutral Orb relay candidate; partner + 30 Combo | LeadActionEnd | relay detection `ZoogiCombo.cpp:820-830` |
| 5 | Heavy Roll | 2 | Impact | +25% force, +20% aim error envelope | LeadActionEnd | Launch impulse |
| 6 | Ember | 2 | Infusion | +20% enemy push; clears Frost cells inside the scorch footprint only | TurnEnd | artifact Ember; no Frost cells exist |
| 7 | Titan | 2 | Infusion | mass x1.35, received push x0.8, cache and restore | TurnEnd | `ZoogiBall.cpp:81/126` |
| 8 | Frost | 2 | Terrain | shared low-friction lane; no stacking | Until cleared / match reset | **no cell grid**: MISSING |
| 9 | Precision | 1 | Control | -30% aim error; preview first contact plus 1 rebound | LeadActionEnd | 1-bounce preview exists |
| 10 | Momentum Steal | 2 | Technique | glancing hit: <=25% tangential transfer, damage 60% | FirstQualifiedContact | contact events `FZoogiContactEvent` |
| 11 | Crystal Collapse | 2 | Terrain | +1 integrity state, swap collision at a safe boundary | Persistent | ArenaPiece damage, but **destruction is disabled** (`bDestruction=False`), so this conflicts |
| 12 | Orb Magnet | 1 | Utility | 0.6 s pull, <=1.5 diameters, blocked by geometry | Commit prep then lead action | none |
| 13 | Double Impact | 2 | Impact | 2 bursts max, 2nd at 50% after 0.12 s, unique IDs | LeadActionEnd | none |
| 14 | Anchor Guard | 1 | Defense | arms after settle; absorb 12, -70% push; blocks friendly setup | NextQualifyingEnemyImpact | none |
| 15 | Recovery Roll | 1 | Control | +50% decel near the chosen safe zone; no heal; not for KO actors | LeadActionEnd | per-surface decel `GetRollingDecel` |
**Note:** "HP as damage feedback" (Guard absorbs 12) means an HP model, which this branch doesn't have. Proposed: a display-only HP value in the card component, because ring-outs decide the match.

## 3. State ownership map (one authoritative owner per item)
| State | Single owner | Others |
|---|---|---|
| Turn and phase (Standby/Select/Aim/Force/Confirm/Roll/Resolve/Next) | `UZoogiBattleStateMachine` (extended with STANDBY plus sub-steps). GameMode timers become requests | GameMode, HUD, CPU only *request* |
| Card Energy, deck, hand, discard, committed cards | NEW `UZoogiCardSubsystem` (world subsystem; per-player `FZoogiCardPlayerState`), validates before spending and applies once by CommitId | HUD/CPU submit `FZoogiCardCommand` |
| Active card effects (modifiers, expiry) | NEW `UZoogiCardEffectComponent` on each AZoogiBall; it only *modifies* existing parameters (mass, impulse, aim error, decel, contact push) and restores them | physics reads modified values |
| Terrain card state (Frost cells, scorch, crystal integrity) | NEW `AZoogiTerrainCellGrid` (Frost/Ember), `AZoogiArenaPiece` integrity (Crystal) | |
| Character ability Charges | `AZoogiBall::Charges` (unchanged) | |
| Team Combo | GameMode `ComboEnergy[]` / FollowUpCostSource (unchanged) | |
| Episode Gauge | `UZoogiEpisodeEnergyComponent` (unchanged) | cards never write it directly |
| Awakening | `UZoogiReactionComponent` (unchanged, optional) | |
| RNG | `GameMode::GameRng` (deck shuffle uses a derived seeded stream, recorded in the ledger) | |
| Event accounting | `UZoogiEpisodeLedger` plus card events (CommitId, EffectId, BurstId) | cinematics read only |
| Save/resume | NEW `UZoogiMatchSave` (USaveGame), snapshots at the NEXT_TURN boundary only | |

## 4. Toolkit-to-game integration map (real classes, RLDB sample, Blueprint)
| Toolkit (real asset) | What it is | Zoogy integration (cards branch) |
|---|---|---|
| `/Game/RLDB/Blueprints/Structs/FCard` (UserDefinedStruct: CardName, Description, Portrait, Tooltips, CardTags, CardEffects[FCardEffect], bTargeted, UseRules[FUseRule], PostUseEvent, EndTurnEvent, StartingStatuses, Repetitions, Frame, FrameTint, CardVisualWidget, DataRow, OwnerID) | card row struct | **Adopted.** `/Game/Cards/DT_ZoogiCards` = 15 rows on FCard (built by `Scripts/make_toolkit_cards.py` from Cards.csv). `FZoogiToolkitCardBackend` reads CardName/Description/cost by reflection |
| `BP_UseRule_StatCost` + `BP_Status_Mana` (`FUseRule` Rule/Status/Cost) | play cost paid in a stat | **Adopted as the cost source**: Mana = Card Energy. The cost is validated and spent by `FZoogiCardRules` (validate before spend, once) |
| `/Game/RLDB/Data/Cards/DT_Cards` (35 sample rows: Smite, Block, ...) | sample cards | Reference only. Not used in play |
| `BP_PileComponent` + `_Deck/_Draw/_Hand/_Discard/_Exhaust/_Destroy/_Void/_Shop`, `BPI_Pile`, `BP_Action_Reshuffle` | piles as actor components | **Not adopted for authority.** The shuffle isn't seedable (replay safety) and it's driven by `BP_TurnManagerRLDB`. Our piles live in `FZoogiCardRules`; the same pile semantics: draw, hand, discard, reshuffle on empty |
| `BP_TurnManagerRLDB`, `BP_ActionManagerRLDB`, `BP_CardPlayer`, `BP_Card`, `BP_CardGameSpawner`, `BP_ArenaMode`, `BP_GI_RLDB`, `BP_SaveGame_Run` | its own turn loop, game mode, game instance and save | **Rejected**: they would compete with `UZoogiBattleStateMachine` (the only turn owner) and the Zoogy GameMode |
| `BP_CardEffect_*` (Attack, AddStatus, Draw, Summon, ...), `BP_Action_*`, `BP_Targeting_*` | effects that directly deal damage or change stats | **Rejected for gameplay**: they fabricate outcomes. `DT_ZoogiCards.CardEffects` stays empty. Zoogy effects go through physics (Phase 3/4) |
| `WBP_CardVisual`, `BPI_CardWidget`, `T_CardFrame`, portraits | card visuals | Referenced by the rows (CardVisualWidget/Frame). Phase 3 UI can render the hand with WBP_CardVisual; Phase 2 uses the Canvas panel |
| `DT_*Tags` (GameplayTags tables, `Config/DefaultGameplayTags.ini` copied) | gameplay tags | Registered so the FCard rows resolve (`Event.Card.Discard`) |
| Adapter | `IZoogiCardBackend` (kept) | `FZoogiToolkitCardBackend` (default, `[ZoogiRoll.Cards] Backend=toolkit`), `FZoogiNativeCardBackend` (fallback / `-ZoogiCardBackend=native`) |

**ArrowTrail (free Fab pack, `VaultCache\FreeArrobd1437213b3bV2`):** 10 Niagara ribbon trails (`NS_ArrowTrail_Basic/_02/Fire/_02/Holy/Ice/Magic/_2/Nature/Water`), 2 master materials (`M_SimpleTrail`, `M_SimpleParticle`), 12 material instances, and textures. Its Demo and Map folders were not copied. It's a projectile-trail pack, so we use it for the Standby aim forecast: `NS_ArrowTrail_Basic` runs along the existing aim preview path (first contact plus one rebound), and `NS_ArrowTrail_Magic` marks the aim point or contact ghost. Both are visual only, shown only in AIM_FORCE (`UZoogiCardSubsystem::TickForecast`). It's copied to `Content/ArrowTrail/{FX,Material,Texture}`.

## 5. Gaps vs spec
1. The toolkit isn't installed (blocker for the plugin route; the native route is unblocked).
2. There's no card/deck/Card Energy system. The Awaken artifact hand (5 artifacts, draw 1) must be replaced or merged. Ember/Storm/Frost/Titan names collide. **Decision for Evan:** cards replace Awaken artifacts in v3 mode, with the artifacts kept behind a flag.
3. There's no STANDBY phase, and the Select > Aim > Force > Confirm sub-steps aren't owned by the FSM. Force is SOFT/MED/FULL, but the doc says CONTROL/DRIVE/LAUNCH.
4. No Frost cell grid / scorch footprint. Crystal Collapse conflicts with `bDestruction=False` (Evan's call on Oct 8, 11:15 PM).
5. No HP/damage model for Guard/Anchor absorb 12. No settle detection hook for Anchor. No Orb Magnet pull, Double Impact bursts or Momentum Steal transfer.
6. Save/resume is missing. Turn advance is still timer-based, and a human aim has no timeout.
7. Deck size and composition, discard rules and the 11 evaluation decks are missing (the archive JSON/CSV isn't on the PC). Ability cooldowns don't exist.
8. CPU has no card planner.
9. 4-player personalized episodes: partial (spotlight by recency only, audit row 20).
10. Browser version: Awaken artifacts only, no card deck. Differences need to be documented, not ported.

## 6. Acceptance matrix: 13 deliverables
| # | Deliverable | Now | Phase | Test (`-ZoogiCardTest=`) |
|---|---|---|---|---|
| 1 | Turn/phase FSM incl. Standby | Partial | P2 | `standby_flow`: logged order STANDBY>SELECT>AIM>FORCE>CONFIRM>ROLL>RESOLVE |
| 2 | Card/deck + 15 cards | Missing | P2-P4 | `deck_seed` (same seed = same hands), `card_<id>` x15 |
| 3 | Standby select/target/force/commit | Partial (aim/force/confirm exist) | P2 | `invalid_target_no_spend`, `cancel_refund` |
| 4 | Physics resolution | Works | - | `cards_off_identical` (cards Off = the 8c73614 finals) |
| 5 | Ally combos, bounded follow-ups | Works (30, one per team) | P4 | `no_third_shot`, `relay_needs_partner` |
| 6 | Physical Arena Orb objectives | Works | P4 | `orb_magnet_cap`, `no_score_on_select` |
| 7 | Defense/status/recovery/elimination | Partial (no HP) | P3 | `guard_once`, `anchor_settle`, `recovery_no_heal`, `mutual_ko_draw` |
| 8 | Guided minigames + failure | Works | P5 | existing halo/orbline/air + card combos |
| 9 | Episode Gauge + 4-player episodes | Partial | P5 | `episode_100_once`, `spotlight_all4` |
| 10 | Earned full-battle unlocks | Partial | P5 | existing unlock tests |
| 11 | CPU same rules | Partial | P4 | `cpu_card_validation` (CPU uses the same command path) |
| 12 | Save/resume, replay-safe accounting | Missing | P6 | `save_resume_equal`, `ledger_no_dup_ids` |
| 13 | Automated + runtime tests | Partial | all | runner script, packaged smoke |

## 7. Acceptance matrix: 15 cards (each must pass cards-off parity plus its card book VALIDATION line)
| Card | Pass condition (from card book VALIDATION) | Phase |
|---|---|---|
| Storm | same launch with vs without: control changes, target doesn't snap | P3 |
| Bank Strike | a missed wall gives no setup; a real useful bank gives exactly one BankSetup | P4 |
| Guard | 2 hits: the first is reduced once, the second is normal; a large impulse can still ring out | P3 |
| Orb Relay | legal Orb passes/scores only on boundary crossing; an Awaken Orb is rejected | P4 |
| Heavy Roll | higher momentum at the same angle; no extra turn | P3 |
| Ember | clears only overlapping Frost cells; push applied once; look restored | P4 |
| Titan | mass x1.35 measured; original mass after turn end and after recovery | P3 |
| Frost | both teams and the Orb slide; persists after handoff; no stacking | P4 |
| Precision | preview stops after 1 rebound; a missed line stays a miss | P3 |
| Momentum Steal | <=25% bound; head-on gets 0; no retrigger | P4 |
| Crystal Collapse | the opened lane can be traversed later; debris doesn't block; invalid target changes nothing (needs a destruction decision) | P4 |
| Orb Magnet | stops at an obstacle or the 1.5D cap; Awaken Orb refused; no score | P4 |
| Double Impact | <=2 unique burst IDs; Guard applies to the first only | P4 |
| Anchor Guard | no defense before settle; consumed once; friendly bumps can't farm setup | P3 |
| Recovery Roll | stop distance changes; HP and Ring Marks unchanged; a KO actor can't use it | P3 |

## 8. Phased plan (16 GB PC: one Unreal job at a time, build > test chain, no parallel editor)
- **P1 (done):** this document. Decisions needed: toolkit install; cards replace artifacts?; deck size; destruction for Crystal; CONTROL/DRIVE/LAUNCH vs SOFT/MED/FULL; HP model.
- **P2 (2 d):** `UZoogiCardDef` data (15 rows, CSV/DataTable), `UZoogiCardSubsystem` (seeded deck, draw 5, energy 5, validate > commit > apply once, ledger events), STANDBY phase in the FSM, a minimal UMG hand. Flag `[ZoogiRoll.Cards] bCards=False` keeps the current game byte-identical. One build plus 6 tests.
- **P3 (2 d):** effect component, plus the 7 parameter cards (Storm, Heavy, Titan, Precision, Guard, Anchor, Recovery Roll) and the HP display model.
- **P4 (3 d):** contact/terrain cards (Bank Strike, Orb Relay, Ember, Frost grid, Momentum Steal, Crystal, Orb Magnet, Double Impact), the CPU card planner, and follow-up integration.
- **P5 (1.5 d):** Episode/minigame/unlock integration and per-player spotlight.
- **P6 (1.5 d):** USaveGame at turn boundaries, replay hash test, packaged smoke, and the doc of differences from the browser version.
Each step: build (about 6 min), then one test batch, then a commit on a new branch `card-system` cut from episode-engine. Never more than one UnrealEditor-Cmd at a time; reuse the existing PC lock/queue scripts.


## 9. Phase 3 status (2026-10-13 21:30 CT): complete on branch `cards` (54327f2, cb56eff)

- **Owner:** UZoogiCardSubsystem stays the single owner. Effects live in ZoogiCardEffects.{h,cpp} (pure core) and ZoogiCardFxWorld.cpp (world glue).
- **Game hooks:** every hook is gated by CardsOn(): GameMode shot (before/after FireShot), AZoogiBall::OnSphereHit, and the combo ring-out mutual-KO rule. The cards-off parity hash e3f55c2b is unchanged.
- **Effects by card.** The tuning is the card book's param_a/param_b; numbers below marked PROPOSED are new:
  - **Storm / Heavy Roll:** scale the launch velocity, capped at FullLaunch x1.35 (PROPOSED).
  - **Precision:** aim in this build is deterministic, so there is no aim error to scale (x0.7 of zero; logged). Its visible part is the existing forecast.
  - **Titan:** mass x1.35, restored to the actual previous value at turn end. Received push x0.8.
  - **Guard x0.75 / Anchor Guard x0.3:** Anchor Guard arms at settle (RESOLVE). Each is consumed once on the first qualifying enemy impact (|impulse| >= 3000, PROPOSED). The push stays physical: only its magnitude is scaled.
  - **Ember:** enemy push x1.2 and clears only the Frost cells it overlaps.
  - **Frost:** a 6-cell shared lane; any Zoogi on it keeps only 35% of its rolling deceleration (friction x0.35).
  - **Bank Strike:** BankSetup only on a real wall/bumper contact (normal z < 0.5); then the first enemy push x1.15 (PROPOSED).
  - **Momentum Steal:** the first qualified contact consumes it; on a glancing hit it moves <= 25% of the shooter's tangential velocity to the target. The x0.6 damage is display-only.
  - **Double Impact:** a second burst at 50% after 0.12 s; max 2 per contact episode.
  - **Orb Relay:** shooter touches an orb, then the partner touches the same orb: +30 combo energy, once per orb life.
  - **Orb Magnet:** 0.6 s pull. The pull-attributable displacement is bounded at <= 1.5 diameters, a line trace to the shooter blocks it, and the added velocity is removed at the end.
  - **Recovery Roll:** deceleration x1.5 within 900 cm of the Recovery Gate (PROPOSED radius). It never touches Ring Marks.
  - **Crystal Collapse:** unchanged from Phase 2.
- **Not done in Phase 3 (Phase 4 candidates):**
  - No HP system: Guard/Anchor absorb 12 and Momentum Steal x0.6 are log/display only.
  - No hand UI art (WBP_CardVisual).
  - Precision/Heavy Roll aim-error envelopes have no effect until an aim-noise model exists.

## 11. Phase 5 status (2026-10-13 22:21 CT): complete on branch `cards` (6eee910, 88bc0b5)
- Episode engine extended, not replaced. The existing ledger stays the only source. UZoogiEpisodeDirector consumes it read-only, and the only things it writes are the Episode Energy spend and one EPISODE ledger event. Cards-off runs never activate it, so e3f55c2b holds.
- Perspectives are built from each player's actual role in the resolved action; misses and ring-outs are stated, not hidden. Playback is a copy; skip and watch produce identical sim and core hashes (runtime-tested).
- Recap unlock rules U1-U4 are PROPOSED and need sign-off. The thresholds live in ZoogiEpTune (RecapMinActions 6, RecapMinActive 2).
- Caveats: episodes auto-spend at the RESOLVE boundary (PROPOSED; no player prompt yet). Perspective lines are generated text, not the card book's 15 authored templates (only the PDF summary is on hand). Playback is a HUD overlay and does not pause the match. The gauge test uses a logged fixture because CPUs earn about 10 Episode per action.

## 12. Phase 6 status (2026-10-13 23:50 CT): complete on branch `cards` (0b74982 + docs/packaging commit)
- Save/resume: FZoogiSaveAccess is the only serializer, with friend access to the BattleStateMachine, the Episode subsystem/director and the card subsystem. Each core also has its own Serialize. Ownership is unchanged: a save only reads and writes each owner's fields.
- Validation, demo, coverage, docs and packaging: see Docs/Cards/COMPLETION_REPORT.md in the repo.
