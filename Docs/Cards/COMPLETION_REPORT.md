# v3 cards: completion report (Phases 2-6), 2026-10-13 CT

Branch `cards` (worktree `C:\dev\ZoogiRollUnreal-cards`), not pushed. Commits: Phase 2 b90a85b · Phase 3 54327f2, cb56eff · Phase 4 1609a86, 0a656d7 · Phase 5 6eee910, 88bc0b5 · Phase 6 0b74982 plus the docs/packaging commit.

## Operating instructions
**Turn cards on**
- per run: `-ZoogiCards`, or `-ini:Game:[ZoogiRoll.Cards]:bCards=True`;
- project-wide: `Config/DefaultGame.ini [ZoogiRoll.Cards] bCards=True` (default False = classic rules, parity hash e3f55c2b).
- Other keys in that section: HandSize, EnergyPerTurn, DeckSize, MaxCopies, MaxCardsPerCommit, Backend (toolkit | native).

**Play (Standby)**
- Click/tap a card, or press 1-5, to toggle it.
- Backspace clears the selection. A or the AWAKEN button requests Episode Awakening.
- Enter, Space or CONFIRM commits. The panel shows the cost, the remaining Card Energy and the effects before you confirm.
- 2v2 Partner Follow-Up: F/Enter accepts, X/Backspace skips. Episode playback can be skipped with the SKIP button.

**Packaged build**
- Build: `Scripts\package.ps1 -Root C:\dev\ZoogiRollUnreal-cards` (run it under the unreal.lock: `%TEMP%\cards_pkg.ps1`). The output is `Packaged\Windows\ZoogiRoll.exe`.
- DT_ZoogiCards and `/Game/RLDB/Widgets` are cooked, and `Content/Data` (Cards.csv, Decks.csv) is staged.
- Smoke command: `ZoogiRoll.exe -ZoogiCards -ZoogiCombo -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiAuto` (or the -ini form).

**Validation flags**
- `-ZoogiMatchTest=<tag>`: complete-match validator; prints `MATCH_TEST` and `CARD_COV`, then quits.
- `-ZoogiSaveTest=boundary,commit,followup,episode`: SaveGame slot `ZoogiP6_<point>`, perturb, load, then check that the hash and bytes are restored.
- `-ZoogiDemoShots=<dir>`: engine-viewport screenshots.
- `-ZoogiCardTest=hand_ui|keys|dup_input|effects|standby_flow|crystal_collapse|mutual_ko`.
- Automation: `ZoogiRoll.Cards`, `ZoogiRoll.Episode.P5`, `ZoogiRoll.P6`.
- Tuning overrides (PROPOSED values): `-ZoogiComboGainMul=1.5 -ZoogiComboStart=30 -ZoogiEpisodeGainMul=3.5`.

## Phase 6 results
| Requirement | Evidence |
|---|---|
| Save/resume at the turn boundary, after commit, at a follow-up offer and during episode playback; the load restores an identical hash | Every SAVE_TEST PASS across all 18 editor match runs (2v2 and FFA, 0 save failures): the hash and the re-captured bytes are identical after the perturbation. **Uninterrupted vs save-and-resume runs end in identical final hashes** (seed 11: ecf045e0 with and without saves; seed 3: e5b9e854 for k3 with all 4 save points vs the demo runs). |
| Replay hash | The same seed twice gives identical ledger, cards and full-state hashes (g11a = g11b; demo3a = demo3b). Watch vs skip episode: identical. |
| Duplicate input / repeated contact | Automation ZoogiRoll.P6.DuplicateInput and RepeatedContact (100 callbacks of one contact → applied once, ≤2 bursts). Runtime `dup_input` PASS 19/0: the same toggle twice in a frame, touch + mouse on one card in a frame, and a double Confirm → one commit, energy spent once (3 duplicates ignored). |
| Save/resume units | ZoogiRoll.P6.CardRulesSaveResume (it continues identically for 11 more turns, shuffles included), EffectCoreSaveResume, EpisodeCoreSaveResume. |
| CPU complete-match progression | Every CPU match run (18 editor + 4 packaged) reached GameOver with a declared winner or draw; max turn gap 32 s, no stalls. |
| Touch / mouse / keyboard | Runtime `hand_ui` PASS 24/0 (real Slate mouse click, key 2, touch tap, touch Confirm), `keys` PASS 20/0 (keyboard-only: 1, Backspace, A, 1, Enter), `dup_input` (touch + mouse). |
| 2v2 demo | `P6_DEMO_2V2.md`: seed 3, TEAM A wins after 12 turns: 3 follow-ups and an earned team combo (BANK + ORB RELAY + TERRAIN), a failed technique (Power Finisher SHATTERED), recoveries (Air Reaction REDIRECT at the edge), 3 personalized episodes. |
| Coverage | `P6_COVERAGE.md`: all 15 cards covered, 9 naturally and 6 in the focused effects scenario. |
| Earning tune (PROPOSED) | Before (Combo x1.5, Episode x2.5, no start gauge; 8-round 2v2): seed 7 → 0 follow-ups; seed 11 → 1 follow-up, 0 episodes. After (start gauge 30, Combo x1.5, Episode x3.5): 2v2 seeds 3/7/11/13 → follow-ups 3/1/1/1, episodes earned 3/2/1/0; FFA seed 7 → 6 episodes. No test top-ups. |
| Packaged build | PACKAGE_EXIT=0. The packaged smoke (seed 3, 2v2) logs `CARDS match: backend=rldb_toolkit data=csv defs=15 ok=1`, so the DT and CSVs load from the pak. The -ini and the -ZoogiCards runs gave identical results (winner TEAM B, 2 follow-ups, 2 episodes, final 67529bc7), and a repeat run was identical. Cards off packaged: PASS (no card state). Screenshots: `packaged/`. |
| Regression | Automation 34/34 (23 cards + 6 P5 + 5 P6); the cards-off roll ledger is still **e3f55c2b**; effects s7/s11, standby_flow, crystal_collapse, mutual_ko, hand_ui, keys and dup_input all PASS. |

## Limitations / caveats (honest)
- **Save scope.** The SaveGame restores match state into the *running* world: bodies, gauges, ledger, cards, effects and episode state. It is not a cold-boot "load from the main menu" flow; there is no UI for it yet. Chaos internals (contact caches, islands) are not serialized. A restore therefore leaves bodies that are already in the saved state untouched and re-sleeps sleeping bodies. Once, in an FFA restore mid-episode while bodies were moving (hf7save), the gameplay ledger content stayed identical but the per-event bit-exact `final_hash` diverged at sub-quantization level.
- **Editor vs packaged.** The same seed produces a different (but internally deterministic) match in editor vs packaged builds (physics/frame timing), so demo hashes are per build type.
- The CPU planner never chooses Ember, Frost, Momentum Steal, Orb Magnet, Double Impact or Anchor Guard on its own in the sampled seeds.
- The PROPOSED values need sign-off: the tuning above, the Phase 5 recap U1-U4 and auto-spend at RESOLVE, and the Phase 3 thresholds.
- There is no HP model: absorb/damage numbers are display/log only. Aim-error cards have no effect (deterministic aim).
- The CPU hand panel shows plain card tiles in screenshots. The toolkit WBP_CardVisual shows on the human Standby (hand_ui test).
- One requested demo frame (turn-1 follow-up offer, editor run) was not written.
- **Repo hygiene.** `Config/DefaultEngine.ini` and `DefaultInput.ini` carry uncommitted edits that predate this work and are left untouched. The worktree holds about 160 MB of binary assets (RLDB) without LFS. `Packaged/` is git-ignored. Nothing is pushed.
