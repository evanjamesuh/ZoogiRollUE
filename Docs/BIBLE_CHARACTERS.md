# Bible characters (Oct 6, 2026) — roster, powers, expressions, nubs

The 8 Buzzy "Zoogy Roll Master Asset Bible" characters replace the old 7 (Wolfgang, Hotstreak, Lars, Pinpoint, Bolt,
Wraps, Nightshade retire; their old meshes stay in `/Game/Zoogis/<id>` but nothing loads them, except Tavi's Friend
Squad falls back to `SM_wolfgang` if a caster mesh is missing). Collision radius, physics, mass formula and the turn /
phase logic are unchanged; the 2v2 Combo code is roster-agnostic and needed no change.

## Roster (index order = menu order = `-ZoogiPicks` indices)

| # | Id | Name | Type | Primary / secondary | Power (display) | Engine power | Inherited from | Spd/Pow/Def/Ctl | Affinity |
|---|----|------|------|---------------------|-----------------|--------------|----------------|-----------------|----------|
| 0 | blue | Blue | Lead Zoogy | #22C3E6 / #E2B866 | Resonance Rebound | Ricochet | Lars | 70/75/60/80 | Storm |
| 1 | tavi | Tavi | Best Friend | #F08A1C / #F5C76B | Friend Squad (3 amber echo clones of Tavi) | WolfPack | Wolfgang | 85/60/50/75 | Frost |
| 2 | vex | Vex | Rival | #D11A2A / #FF7A1A | Rival Blast | Explosion | Hotstreak | 55/90/62/45 | Ember |
| 3 | moss | Moss | Tactician | #2E6B45 / #6CC69A | Tactical Lock | LockOn | Pinpoint | 60/65/45/95 | Frost |
| 4 | orin | Master Orin | Arena Keeper | #8E8E8E / #D9A441 | Granite Bind | BandageBind | Wraps | 50/70/85/65 (heaviest) | Titan |
| 5 | sera | Sera | Archivist | #C7B6F2 / #D8B26A | Rune Seal (**new variant**) | RuneSeal | — | 58/62/66/84 | Void |
| 6 | luma | Luma | Announcer | #E81CC8 / #F2E23A | Hype Shock | StaticShock | Bolt | 80/55/40/70 | Storm |
| 7 | darkzoogy | Dark Zoogy | Ancient Mystery | #7A0E26 / #FF3048 | Shadow Stun | ShadowStun | Nightshade | 62/68/64/84 | Void |

Why: Vex (rival, aggressive) gets the blast; Master Orin (heavy, stoic keeper) gets the bind and the highest defense =
mass; Luma (hype announcer, energy) gets the phase-through shock; Dark Zoogy (most sinister) keeps the freeze-and-skip
Shadow Stun; Moss (tactician) the precise lock-on; Blue (earnest lead) the resonance rebound; Tavi (best friend) a squad
of friendly echoes. Stats/mass travel with the power, so balance is the balance those powers were tuned at.

**Rune Seal (Sera)** — instant power: Shadow Stun's pulse shape (radius 460 cm, rune ground rings + gold burst) applying
Bandage Bind's curse instead of the freeze: every rival inside gets `BoundShots = max(BoundShots, 2)` ("SEALED"), no
freeze, no skipped turn, no knock. Allies are never sealed in 2v2. CPU uses it like Shadow Stun. Log:
`P%d Sera rune seal bound %d`. Code: `ZoogiPowers.cpp` (`RuneSealRadius`), `EZoogiPower::RuneSeal`.

Artifact affinity is now derived from the power (`ZoogiArtifacts::Affinity`): Explosion→Ember, StaticShock/Ricochet→
Storm, LockOn/WolfPack→Frost, ShadowStun/RuneSeal→Void, BandageBind→Titan.

## Meshes / materials
* `/Game/Characters/Bible/<Asset>/SM_<Asset>` (shell + eyes/visor decal + trims + core, one slot per part) and
  `SM_<Asset>_Nubs`; instances `Materials/MI_<Asset>_<Slot>`; textures `Textures/T_<Asset>_*`.
  Assets: Blue, Tavi, Vex, Moss, MasterOrin, Sera, Luma, DarkZoogy (`FZoogiDef::MeshAsset`).
* Masters in `/Game/Characters/Bible/Materials`: `M_BibleZoogi`, `M_BibleZoogiGlass` (Blue / Luma shells),
  `M_BibleZoogiMasked`, `M_BibleZoogiFactor`, `M_BibleZoogiEyes`, `M_BibleZoogiEyesMasked` (expression atlas).
* Source: `SourceArt/Characters/Bible/<Asset>/<Asset>_UE.glb` + `T_*.png` + `bible_materials.json` (built on the box by
  `characters-bible/tools`: zchars/zgeo geometry, ztex textures, zexpr expression atlases, zblend export, zue staging).
* Import / re-import (editor closed): `.\Scripts\run_python.ps1 Scripts\import_bible_characters.py import_bible`
  (`--force` rebuilds the masters; names or env `ZOOGI_BIBLE_ONLY=Blue,Tavi` limit the import). Report: `Saved\import_bible_report.json`.
* Portraits: `Art/Portraits/T_Portrait_<id>.png` (256², meadow look) → `Scripts\import_portraits.py`.
* In-game legibility (50° gameplay camera): the eye atlases are a game-only build (`zexpr.py --ingame`). Strokes are
  1.5× thicker, there is a soft light halo behind the eye shapes, and the goggle-window glow is baked low. Skin decals
  (Tavi, Orin) get hard alpha with no soft socket shadows. The importer forces sRGB + `TC_Default` on every colour map,
  because UE's normal-map auto-detection otherwise flags bluish textures (Blue's atlas) as BC5 and drops the blue
  channel. Eye atlases are `never_stream`. Nanite is off on these meshes, since its quantisation punched holes in the
  decal. Orin's shell has `Tint` (0.9, 0.87, 0.8) so the granite stays gray under the arena sky.
* Close in a game run, the active player's turn light (60 cd in the player colour) tints that Zoogi's shell. `-ZoogiExprDemo`
  switches it off while it photographs the faces.
* Re-import needs no UE process running, `-game` sessions included: a running game locks the character packages and
  the saves fail (`LogSavePackage: Error` in `import_bible.log`).

## Flipper nubs (visual only)
`AZoogiBall::Nubs` is a separate no-collision mesh attached to the model. **Hide them project-wide:** set
`ShowFlipperNubs=False` under `[ZoogiRoll.Characters]` in `Config/DefaultGame.ini` (rebuild/package not needed for the
editor; the packaged build reads the cooked ini, so re-package). Per run: `-ZoogiNoNubs` (or `-ZoogiNubs` to force on).

## Expressions (UZoogiExpressionComponent)
One component per Zoogi (`ZoogiExpressionComponent.h/.cpp`) polls the turn state each frame and drives the eye atlas
cell (`ExprIndex`), blinks, eye/core glow, nub motion, small body beats (existing squash / wobble hooks) and the face
turn. Visual only — physics and turn logic untouched.

| Cell | Expression | Triggered by |
|------|------------|--------------|
| 0 | neutral | idle / Awaken undecided / Save / Resolve / End Turn; blinks every 2–5 s (persona) |
| 1 | blink | 0.12 s blinks over neutral/focused/worried/excited/determined (never on Dark Zoogy's visor) |
| 2 | focused | Battle **Aim** (active Zoogi) |
| 3 | determined | Battle **PowerUp**, Awaken **Infuse**, **Partner Follow-Up** (the partner who acts); core flares |
| 4 | excited | **Rolling** shooter (>180 cm/s), landing a hit, partner follow-up (teammate); nubs flap |
| 5 | dizzy | hard impact taken (>9000 impulse), stunned (Static/Hype Shock, Shadow Stun) — woozy wobble |
| 6 | worried | near the edge (<2.2 radii), a rolling rival bearing down, being knocked around, light bump — nub tremble |
| 7 | ko | falling off the rim / knocked out |
| 8 | happy | knock-out credit, orb scored, points this turn, **match won** (happy hops) |
| 9 | sad | **match lost**, bound/sealed (curse), active while bound |
| 10 | guard | Awaken **Guard**, and braced while an attacker rolls; guard hit — nubs tucked in |
| 11 | flourish | **Draw** hero beat, **Ally Bump** (team), idle personality beat every 5–14 s |

Personalities (flourish cell + motion tuning): Vex smug side-glance, Master Orin stoic meditation lids (slow, minimal
motion, rare blinks), Luma hyper star eyes (fast blinks, big flaps, frequent beats), Sera serene closed smile + sparkle,
Tavi cheerful wink (brows + mouth on the skin face), Moss calm knowing half-lid, Blue earnest sparkle, Dark Zoogy
menacing visor slits: red emissive optic shapes per expression with a slow pulse that quickens / heats when focused or
charging (`GlowTint`, `EmissiveBoost`).

**Face turn:** while a Zoogi is settled the model slerps (sphere-local `FaceAlign`) so its face looks toward the camera,
tipped up at most `FaceMaxTiltDeg` (32°) for the 45–60° gameplay camera; while rolling the offset is frozen so the face
rolls naturally. Config `[ZoogiRoll.Characters] FaceCamera=True`, `FaceMaxTiltDeg=32`; `-ZoogiNoFaceCam` per run.

Verification: log lines `Expr P1 Blue -> focused (aim)`; `-ZoogiExprDemo` (with `-ZoogiShots`) freezes the first Draw
beat and photographs every expression of every Zoogi at the gameplay angle (`exprdemo_<id>_<NN>_<expr>.png`).
