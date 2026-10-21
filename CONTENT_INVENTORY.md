# What is in the full project but not in this branch

This branch has all the code, config, scripts, docs and UI art. The rest of the Unreal project, **3,426 files, about
1.2 GB**, stays on Evan's PC. Most of it is binary Unreal assets (`.uasset` / `.umap`), which can only be opened in the Unreal
Editor. Some of it is paid marketplace content whose license forbids publishing it.

This file describes all of it; **`CONTENT_FILES.tsv`** lists every left-out file with its path, size and asset type.
Asset paths in the code (for example `/Game/VFX/KriptoFX/...`) map to `Content/VFX/KriptoFX/...` in that list.

## The game's own content (made for Zoogi Roll)

| What | Folder | Contents | How the code uses it |
|---|---|---|---|
| Main level | `Content/Maps/Meadow.umap` | the single level; every arena is built at runtime | `GameDefaultMap` in `Config/DefaultEngine.ini`; `AZoogiArena::Build` spawns the chosen arena |
| Zoogi characters (Bible set) | `Content/Characters/Bible` | 8 characters: Blue, Dark Zoogy, Luma, Master Orin, Moss, Sera, Tavi, Vex; each a mesh + optional "nubs" mesh, shell / core / eyes / trim materials, an expression eye atlas | `ZoogiTypes.cpp` roster, `ZoogiBall.cpp`, `ZoogiExpressionComponent.cpp` |
| Zoogi characters (GLB imports) | `Content/Zoogis/<name>` | bolt, hotstreak, lars, nightshade, pinpoint, wolfgang, wraps: one mesh + material + textures each | roster entries for those Zoogis |
| Orbs | `Content/Orbs` | the scoring orb mesh + textures | `ZoogiOrb.cpp` |
| Arena kits | `Content/Env/Incoming/<arena>` | modular pieces per arena: tomb (Pharaoh's Tomb), neon (Night Circuit), volcanic, training, arabian, cosmic (walls, floors, pillars, ramps, bridges, materials) | `ZoogiArenaIncoming.cpp`, `ZoogiIncoming.inl` |
| Frozen Ring + Meadow arena art | `Content/Env/Arenas` | Frozen Ring mountains, crater slabs, floor / displacement / glow textures, skies | `ZoogiArenaFrozen.cpp`, `ZoogiArenaMeadow.cpp`, `ZoogiFrozenRing2.cpp` |
| Obstacles | `Content/Env/Obstacles` | bumpers, bank walls, gates, rocks and their damage states | `ZoogiArenaObstacles.cpp`, `ZoogiObstacles.inl`, `ZoogiRock.cpp` |
| Spawn platforms | `Content/Env/SpawnPlatform` | the elevator pads Zoogis rise on, per-arena materials | `ZoogiSpawnElevator.cpp` |
| Props and scenery | `Content/Env/MeadowBatch1`, `MeadowBuildings`, `OakTree`, `PineTree`, `SnowPine`, `DesertHoodoo`, `AlienCrystal`, `Cosmic` | trees, buildings, rock spires, crystals, cosmic sky materials | arena builders, `ZoogiMeadowBatch1.inl` |
| Aim arrow trail | `Content/ArrowTrail` | 10 Niagara trails, materials, textures | `ZoogiAimArrow.cpp`, `ZoogiFrameTrail.cpp` |
| Awaken / card / Titan effects | `Content/VFX/AwakenFX`, `Content/VFX/TitanFX`, `Content/FX`, `Content/Materials/Artifacts` | element bursts (Ember, Storm, Frost, Titan, Void), guard shells, impact flashes | `ZoogiAwakenFX.cpp`, `ZoogiTitanMeteorFX.cpp`, `ZoogiFX.cpp`, `ZoogiArtifactComponent.cpp` |
| Anime cut-ins | `Content/Anime/Movies` | 10 short MP4 clips (Critical, Infuse, KO ...) + `Movies.csv`, `AnimeSequences.csv` | `ZoogiAnimeSubsystem.cpp` |
| Comic panels and portraits | `Content/UI/ComicSlices`, `Content/UI/Portraits` | comic panel art for episode moments, Zoogi portraits | `ZoogiComicSliceOverlay.cpp`, menus |
| Music | `Content/Audio/Music` | layered stems (Base, Battle, Shift, ShiftHit, Victory, Defeat) for 8 arenas: Arabian Nights, Cosmic Platform, Frozen Ring, Meadow Arena, Night Circuit, Pharaoh's Tomb, Training, Volcanic Pit | `ZoogiMusicSubsystem.cpp` |
| Voice and sounds | `Content/Audio/VO`, `Content/Audio` | announcer lines (Battle, Climax, Draw, KO ...), menu music, SFX | `ZoogiAnnouncerSubsystem.cpp`, `ZoogiRoll.cpp` |
| Card table | `Content/Cards/DT_ZoogiCards` | the 15 cards as toolkit data rows (name, cost, text) | `ZoogiCards.cpp` (`FZoogiToolkitCardBackend`); the same cards are in `Content/Data` CSVs in this branch |
| Source audio and portraits | `SourceArt/Audio` (20 WAV), `Art/Portraits` (15 PNG) | originals the imported assets were made from | import scripts in `Scripts/` |

## Marketplace content (licensed, cannot be published)

| Pack | Folder | What the game uses it for |
|---|---|---|
| **KriptoFX** (Niagara VFX pack) | `Content/VFX/KriptoFX` (341 files) | big hit, explosion and element effects; wrapped by `ZoogiKFXExtras.cpp` / `ZoogiKFXExtrasData.inl`; `Scripts/kfx` (left out) holds the porting scripts and textures derived from it |
| **Roguelike Deckbuilder Toolkit** (RLDB) | `Content/RLDB` (633 files: Blueprints, Data, Demo, Input, Maps, Widgets) | the card widget `WBP_CardVisual` used by the older card hand (`ZoogiCardHandUI.cpp`), the card data table format; its demo maps and blueprints are not used in matches |

Anyone working only from this branch can change rules, UI, HUD, menus, cards (via `Content/Data/*.csv`) and config; new
3D art or effects have to be added in the full project.

## Not in git at all (local only on Evan's PC)

`Content/AuraModelGen` (AI-generated Frozen arena meshes in progress), `Content/Movies` (menu background video),
`Content.zip` / `ZoogiRollSource.zip` (backups), and build outputs (`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`,
packaged builds).

## Left-out folders at a glance

| Folder | Files | Size | Main types |
|---|---|---|---|
| `Art/Portraits` | 15 | 1.2 MB | PNG file 15 |
| `Content/Anime` | 1 | 0.0 MB | CSV file 1 |
| `Content/Anime/Movies` | 11 | 31.7 MB | Video 10, CSV file 1 |
| `Content/ArrowTrail/FX` | 10 | 11.3 MB | Niagara system 10 |
| `Content/ArrowTrail/Material` | 15 | 0.3 MB | Material instance 12, Material 2, Material function 1 |
| `Content/ArrowTrail/Texture` | 18 | 48.7 MB | Texture 18 |
| `Content/Audio` | 32 | 27.2 MB | Sound 32 |
| `Content/Audio/Music` | 48 | 246.9 MB | Sound 48 |
| `Content/Audio/SFX` | 1 | 0.0 MB | MD file 1 |
| `Content/Audio/VO` | 77 | 18.6 MB | Sound 77 |
| `Content/Cards` | 1 | 0.0 MB | Data table 1 |
| `Content/Characters/Bible` | 149 | 49.6 MB | Texture 73, Material instance 54, Static mesh 16, Material 6 |
| `Content/Env/AlienCrystal` | 3 | 4.7 MB | Asset 2, Static mesh 1 |
| `Content/Env/Arenas` | 114 | 246.2 MB | Texture 95, Static mesh 12, Material 7 |
| `Content/Env/Cosmic` | 6 | 11.0 MB | Material 3, Texture 3 |
| `Content/Env/DesertHoodoo` | 3 | 4.4 MB | Asset 2, Static mesh 1 |
| `Content/Env/Incoming` | 1115 | 72.8 MB | Static mesh 377, Material 316, Asset 216, Material instance 202 |
| `Content/Env/MeadowBatch1` | 101 | 6.1 MB | Static mesh 67, Material instance 34 |
| `Content/Env/MeadowBuildings` | 9 | 1.0 MB | Static mesh 9 |
| `Content/Env/OakTree` | 3 | 4.8 MB | Asset 2, Static mesh 1 |
| `Content/Env/Obstacles` | 407 | 38.9 MB | Static mesh 152, Asset 125, Material 76, Material instance 51 |
| `Content/Env/PineTree` | 3 | 4.7 MB | Asset 2, Static mesh 1 |
| `Content/Env/SnowPine` | 3 | 4.3 MB | Asset 2, Static mesh 1 |
| `Content/Env/SpawnPlatform` | 122 | 12.9 MB | Material 64, Texture 35, Static mesh 23 |
| `Content/FX` | 11 | 7.3 MB | Niagara system 11 |
| `Content/Maps` | 1 | 0.0 MB | Level (map) 1 |
| `Content/Materials` | 6 | 0.1 MB | Material 6 |
| `Content/Materials/Artifacts` | 10 | 0.0 MB | Material instance 10 |
| `Content/Orbs` | 1 | 1.0 MB | Static mesh 1 |
| `Content/Orbs/orb` | 4 | 17.3 MB | Texture (GLB import) 3, Material (GLB import) 1 |
| `Content/RLDB/Blueprints` | 232 | 21.3 MB | Blueprint 171, Asset 61 |
| `Content/RLDB/Data` | 33 | 0.4 MB | Data table 32, Asset 1 |
| `Content/RLDB/Demo` | 310 | 126.4 MB | Texture 149, Asset 91, Animation 20, Material 18 |
| `Content/RLDB/Input` | 2 | 0.0 MB | Asset 2 |
| `Content/RLDB/Maps` | 7 | 3.2 MB | Level (map) 4, Asset 3 |
| `Content/RLDB/Widgets` | 49 | 9.3 MB | Widget blueprint 49 |
| `Content/UI/ComicSlices` | 12 | 16.0 MB | Texture 12 |
| `Content/UI/Portraits` | 15 | 1.6 MB | Texture 15 |
| `Content/VFX/AwakenFX` | 36 | 1.8 MB | Material instance 17, Texture 12, Material 5, Niagara system 2 |
| `Content/VFX/KriptoFX` | 341 | 119.2 MB | Material instance 122, Texture 121, Sound 39, Niagara system 33 |
| `Content/VFX/TitanFX` | 9 | 0.7 MB | Material instance 3, Material 3, Niagara system 2, Texture 1 |
| `Content/Zoogis/bolt` | 5 | 9.6 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Content/Zoogis/hotstreak` | 5 | 8.1 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Content/Zoogis/lars` | 5 | 9.0 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Content/Zoogis/nightshade` | 6 | 4.6 MB | Asset 5, Static mesh 1 |
| `Content/Zoogis/pinpoint` | 5 | 10.8 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Content/Zoogis/wolfgang` | 5 | 8.1 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Content/Zoogis/wraps` | 5 | 7.3 MB | Texture (GLB import) 3, Static mesh 1, Material (GLB import) 1 |
| `Scripts/kfx` | 24 | 0.9 MB | PNG file 12, PY file 7, PS1 file 2, TXT file 2 |
| `SourceArt/Audio` | 20 | 3.7 MB | WAV file 20 |
