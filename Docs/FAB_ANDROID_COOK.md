# Fab Android never-cook (2026-10-22)

## Where packs live
- Staging audit: `E:\UnrealProjects\ZoogiFabAssets` (EFFECTS_SCHEMATIC.md)
- Worktree with all 8 approved packs on disk: `C:\dev\ZoogiRollUnreal-fab` branch `fx/fab-packs`
- Combined ship branch: `C:\dev\ZoogiRollUnreal-combined` `combined-hud-tripo` (cook rules applied here)

## Evan-approved 8 packs
1. Impact Frames → `Content/Vefects/Easy_Impact_Frames`
2. Shockwaves → `Content/Vefects/Easy_Shockwaves_VFX`
3. Arrow Trail → `Content/ArrowTrail`
4. Spline VFX → `Content/_SplineVFX`
5. Nebula Skyboxes → `Content/Animated_Nebulas_Starter_Pack` (**Android NeverCook**)
6. VDB Nebula → `Content/Nebula_VDB` (**Android NeverCook**)
7. Fluid Orbs → `Content/Fluid_Orbs`
8. Mountain Tops → `Content/Iceland_Environment` (**Android NeverCook**; mobile uses `/Game/Env/Arenas/FrozenMountains`)

Roguelike Deckbuilder Toolkit left out of this Fab pass (already integrated as RLDB).

## Cook rules
- `Config/Android/AndroidGame.ini`: NeverCook nebula heavies + Iceland_Environment + Fab Demo/Map folders
- `Config/DefaultGame.ini`: NeverCook Fab Demo/Map on all platforms; PC may still cook nebula packs when present
- `.gitignore`: Fab licensed trees stay local (same spirit as Iceland_Environment)

## PC vs Android
- Win64/PC: approved packs available when present under Content/ (fab worktree or after migrate)
- Android: skips Animated_Nebulas_Starter_Pack, Nebula_VDB, Iceland_Environment, and Demo/Map showcases
