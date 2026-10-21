# Zoogi Roll v1.0 progression and economy (branch `progression`)
Binding: Evan's decisions (Oct 14, 2026). The full report is in the commit log and the agent report. See INTEGRATION.md, STORE_SETUP.md and pacing_results.txt.
- All balance values: `Content/Data/Progression/*.csv` (Economy, MasteryCurve, MatchRewards, CPUTiers, CPUCombos, Powers, Achievements, Titles, Cosmetics, Affinity, AffinityXP, GemPackages).
- Pacing proof: `python3 Scripts/pacing_sim.py Content/Data/Progression` reads the same CSVs the game loads (exit 1 if a target is missed).
- Tests: `Automation RunTests ZoogiRoll.Progression+ZoogiRoll.CPU+ZoogiRoll.Store` (and `ZoogiRoll.Cards.Elements`).
