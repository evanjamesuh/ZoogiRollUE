# Milestone "Awaken" (M1): first playable target

## The playable target

One match (Evan + 1 CPU, any arena) plays this loop every turn, with clickable controls only:

1. **Draw:** an artifact card is drawn: Ember, Storm, Frost, Titan or Void.
2. **Awaken:** the Zoogi is selected (one per player, so this is automatic but shown). The player clicks **Infuse**, **Guard** or **Save**.
3. **Visible:** Infuse shows the transformation on the shell (overlay, aura, core pulse, and later the trail). Guard shows the bubble. Save shows the card in the hand.
4. **Battle:** the player picks a target (lock-on) or clicks the floor (free aim), nudges the cut angle with ◀ ▶, sees the preview (ghost contact, arrows, bank line) and clicks **CONFIRM**.
5. **Powered roll:** PowerUp (shell expansion and pulse), then launch with squash, spin and the artifact trail.
6. **Collision:** the artifact effect fires (Ember flame burst, Storm chain, Frost freeze, Titan shockwave, Void pull) with its Niagara impact and physics reaction.
7. **Resolve:** wait for settle (or the 8 s timeout), then commit score, statuses, Guard consumption and KOs, and respawn.
8. **The turn passes** to the next player.

**Anime in M1:** real-time Micro placeholders only (Infuse, Guard and PoweredHit for each artifact), plus one Media pipeline test clip. The Standard Level Sequences (`Anime_Hotstreak_Ember_Critical_01` first) are M2.

## Ordered task breakdown

| # | Task | Owner | Depends on | Estimate |
|---|---|---|---|---|
| 0 | Sync and commit the box-only sparkle trail + orb flyer code (or drop it); re-read the bal3 results; tag `pre-awaken` | Grok / Evan | none | 0.5 d |
| 1 | Merge the phase enums → `EZoogiTurnPhase` + `EZoogiBattleStep` in `AZoogiGameMode` (`ZoogiTurn.cpp`); HUD tracker; keep the automation flags working | Cursor | 0 | 1.5 d |
| 2 | Resolve state + `FZoogiTurnLedger`; score committed in Resolve; settle and timeout | Cursor | 1 | 1 d |
| 3 | `UZoogiArtifactData` + 5 DataAssets (script) + `UZoogiArtifactComponent` (state only) + Draw bag + `bArtifactMode` flag | Cursor | 1 | 1 d |
| 4 | Awaken actions in `MenuAction` + CPU `PlanCPUAwaken` + `UZoogiAwakenWidget` (functional buttons, plain style) | Cursor | 3 | 1.5 d |
| 5 | Infuse and Guard gameplay effects (5 + 5), affinity hooks into `ZoogiPowers.cpp` | Cursor | 3 | 2 d |
| 6 | Angled aiming: floor click, power steps, ◀ ▶ nudges, ghost contact + arrows, one-bounce bank preview, CPU cut angles | Cursor | 1 | 2 d |
| 7 | `FZoogiCombatEvent` + `UZoogiAnimeSubsystem` + `DT_AnimeSequences` (CSV import) + snapshot / restore + real-time Micro back-end | Cursor | 2 | 2 d |
| 8 | `PTactical` camera default + small emphasis + `PlayShot` + the Tactical/Cinematic setting | Cursor / Unreal AI | 1 | 1 d |
| 9 | Visual specs: Awaken UI mockups, 5 icons, 5 transformation boards, masks and flipbooks (parallel with 1-8) | Aura | none | 3-4 d (parallel) |
| 10 | `M_ZoogiArtifactOverlay` + 5 + 5 material instances, guard bubble, squash / pulse / spin hooks | Unreal AI | 3, 9 (placeholders OK) | 1.5 d |
| 11 | 20 Niagara systems (aura, trail, impact, guard) via script | Unreal AI | 9 (placeholders OK) | 2.5 d |
| 12 | Awaken UMG styling to the Aura mockups; aim-control visuals | Unreal AI | 4, 9 | 1 d |
| 13 | Media pipeline test (`Anime_Any_Any_KO_01` test MP4: preload, transition, return-to-state) | Unreal AI | 7 | 0.5 d |
| 14 | Integration pass, screenshots (`shoot.ps1`), human test, balance stream (20 matches, artifact mode) and tuning | Grok / Evan | all | 1.5 d |

- **Total:** about 19-20 working days of tasks. Running Aura in parallel and splitting Cursor and the Unreal AI after task 3 brings it to about **2-2.5 calendar weeks**.
- **Critical path:** 1 → 2 → 7 → 13 → 14, and 1 → 3 → 5 → 14.
- **First playable (ugly, functional) checkpoint:** after tasks 1-6, about 9 days in. The loop plays with tints and placeholder bursts.

## Decisions (approved by Evan, Oct 4 2026)

Evan approved all six recommendations below on Oct 4: one Zoogi per player, no HP, the deterministic crit rule, Guard for the next opponent attack only, the affinity mapping (Hotstreak→Ember, Bolt→Storm, Lars→Storm, Pinpoint→Frost, Nightshade→Void, Wraps→Titan, Wolfgang→Frost) and the Tactical default camera.

1. **Multiple Zoogis per player?** Recommendation: keep 1 per player for M1. Selection is automatic and Guard protects your own Zoogi. The data supports teams later.
2. **Powers vs artifacts.** Recommendation: artifacts replace Charges and E/Q, and signature powers become affinity upgrades (mapping in REVIEW.md section 7). Confirm the mapping, especially Lars and Wolfgang.
3. **HP or damage?** The game is ring-out only today. Recommendation: no HP in M1. "Damage" means knockback, statuses and score.
4. **Crit rule.** Proposal: a powered hit at ≥ 75% speed and cut < 8°, deterministic.
5. **Guard duration.** Proposal: the next opponent's attack only (with 3-4 players: the next opponent turn).
6. **Camera.** Proposal: Tactical by default, with the dramatic profiles reused as anime shots and a Cinematic toggle.

## Out of scope for M1

- Charge and Link actions.
- Per-Zoogi Standard/Special anime, apart from the Hotstreak + Ember pair in M2.
- Ultimates.
- Native mobile builds. Phone testing via Pixel Streaming is optional (see the report).
