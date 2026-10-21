# Reproducible 2v2 cards match demo (Phase 6)

**Command** (any machine with this build, editor or packaged):
```
ZoogiRoll.exe -game -ZoogiAuto -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiRounds=8 -ZoogiCombo -ZoogiCards -ZoogiNoForm -UseFixedTimeStep -FPS=60 -ZoogiEpisodeSeed=3 -ZoogiMatchTest=demo -ZoogiDemoShots=<dir>
```
(editor: `Scripts\run_episode.ps1 -Name demo -Quit 1500 -Players 4 -Arena 1 -Rounds 8 -Extra "<flags above>"`, or `%TEMP%\cards_match.ps1`).

**Reproducibility.** Three runs of seed 3 gave identical results: demo3a (with screenshots), demo3b (plain), and k3 (with save → perturb → load at the turn boundary, after commit, at a follow-up offer and during episode playback). All three have ledger `b903b82a`, card state `a62d7bd9`, and full-state hash `e5b9e854`.

**Result:** `MATCH_TEST case=demo3a result=PASS mode=2v2 winner=0 (TEAM A) turns=12 actions=14 follow_offers=3 follow_ups=3 shatters=2 recoveries=9 awakenings=5 episodes=3 played=3 branches=RECOVERY+FAILURE+FAILURE`
Final banner: **TEAM A WINS!** (both TEAM B Zoogis knocked out).

## Turn by turn (from the run log; every Zoogi is CPU)
| Turn | Player | Cards committed (cost / energy) | What physically happened |
|---|---|---|---|
| 1 | P1 Blue (A) | Heavy Roll + Storm + Bank Strike (5 / 5→0) | Real bank contact gave **BANK SETUP**. Team A gauge 30 ≥ 30, so a **Partner Follow-Up was offered and P2 Tavi took it** (gauge 30→0). |
| 2 | P3 Vex (B) | Heavy Roll + Precision + Guard + Orb Relay (5 / 5→0) | Main shot, no trigger. |
| 3 | P2 Tavi (A) | Heavy Roll + Precision + Bank Strike (5 / 5→0) | BANK SETUP; gauge 0 < 30, so no offer. |
| 4 | P4 Moss (B) | Storm + Bank Strike + Titan (5 / 5→0) + Standby **Awakening** | Air Reaction minigame: **REDIRECT SUCCESS**. BANK SETUP → **follow-up by P3 Vex** (shot 06). |
| 5 | P1 Blue | Storm + Precision + Guard + Orb Relay (4 / 5→1) + Awakening | Hit orb O6 (ORB RELAY setup). |
| 6 | P3 Vex | Heavy Roll + Storm + Bank Strike (5) | BANK SETUP, no gauge. |
| 7 | P2 Tavi | Heavy Roll + Storm + Guard + Orb Relay (5) + Awakening | Air Reaction REDIRECT SUCCESS. **Team combo earned:** BANK SETUP + ORB RELAY + TERRAIN SETUP, gauge 45 → **follow-up by P1 Blue**. P2 spends Episode Gauge 100 → **Episode 1 (RECOVERY)**. |
| 8 | P4 Moss | Heavy Roll + Guard + Orb Relay (4 / 5→1) | Main shot while episode 1 plays (read-only overlay). |
| 9 | P1 Blue | Heavy Roll + Storm + Precision + Guard (5) + Awakening | Rolls out → P1 spends 100 → **Episode 2 (FAILURE)**: "P1 Blue commits... and misses". |
| 10 | P4 Moss | Storm + Precision + Guard (3 / 5→2) + Awakening | **Failed technique:** the Power Finisher combo **SHATTERS** (`Strike_FAILED: missing tag POWER_HIT`). P1's Air Reaction redirect is a **recovery** at the edge. P4 → **Episode 3 (FAILURE)**. |
| 11 | P2 Tavi | Heavy Roll + Storm + Bank Strike (5) | BANK SETUP; gauge 15 < 30. |
| 12 | P4 Moss | Heavy Roll + Bank Strike + Orb Relay (5) | Moss is rung out on its 2nd Ring Mark → **MATCH OVER, TEAM A WINS**. |

## Personalized episode perspectives (4 × 3 s each, from each player's real role)
- **Ep 1 RECOVERY (P2):** P2 "opens and connects - recovers at the edge"; P1, P3, P4 "watch it unfold, preparing a response".
- **Ep 2 FAILURE (P1):** P1 "commits... and misses - nothing connects and rolls out of the ring"; the others are preparing a response.
- **Ep 3 FAILURE (P4):** P4 "opens: strikes P1 Blue (the combo shatters) - recovers at the edge"; **P1 "takes the impact from P4 Moss and holds on"** (RECEIVING_IMPACT); P2 and P3 are preparing.

## Results / recap (PROPOSED U1-U4)
| Player | Team | Recap | Contributions (open / follow / receive / prep / miss / out) | Assembly perf |
|---|---|---|---|---|
| P1 Blue | A (winner) | FULL | 1 / 1 / 1 / 10 / 2 / 1 | 5.0 |
| P2 Tavi | A (winner) | FULL | 1 / 0 / 0 / 11 / 3 / 1 | 7.0 |
| P3 Vex | B | PARTIAL | 0 / 0 / 0 / 12 / 3 / 2 | 1.0 |
| P4 Moss | B | PARTIAL | 1 / 0 / 0 / 11 / 3 / 2 | 6.0 |

## Screenshots (engine viewport only, FScreenshotRequest; downscaled copies are in `demo/`)
`01_turn01_P1_standby_cards` (Standby hand: CARD ENERGY 0/0 after commit, effects listed) · `06_turn04_followup_offer` (PARTNER FOLLOW-UP!: BANK SETUP → VEX, cost 30) · `11/12_turn07_episode1_view1/2` (EPISODE 1 - RECOVERY with perspectives, SKIP button) · `16_turn09_P1_standby_cards` (Standby: "HEAVY ROLL + STORM + PRECISION + GUARD", Awakening) · `23/24_turn11_episode3_view1/2` (EPISODE 3 - FAILURE, perspectives 1-2) · `28_result` (MATCH OVER, TEAM A WINS!, ranking).
All 27 full-resolution frames: `Saved\Shots\demo3a\` on the PC. Frame 02 (turn-1 follow-up offer) was requested but not written, because the offer resolved within the same frame.
