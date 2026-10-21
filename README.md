# Zoogi Roll Arena (Unreal Engine 5.6) — code snapshot

A 2v2 marble battler: you and a CPU ally against two CPU Zoogis, on round arenas, with cards and an Awaken power.
This branch is a **code-only snapshot** of the Unreal project (C++ source, config, scripts, docs, UI art and the web UI).
The 3D content (`Content/*.uasset`, levels, marketplace VFX and the card toolkit) is not included: some of it is paid
marketplace content that can't be published. The game itself is built and played from the full project on Evan's PC.

## Where to look

| Area | Files |
|---|---|
| Match rules, turns (Draw → Awaken → Battle → Resolve → End) | `Source/ZoogiRoll/Private/ZoogiGameMode.cpp`, `ZoogiTargeting.cpp`, `ZoogiCombo.cpp` (2v2) |
| Cards (decks, hand, energy, effects) | `ZoogiCards.cpp`, `ZoogiCardSubsystem.cpp`, `ZoogiCardGlue.cpp`, `ZoogiCardFxWorld.cpp` |
| Awaken (charged by playing cards, 4 pips) | `ZoogiCardGlue.cpp` (`CardsAddAwakenCharge`, `CardsUseAwaken`), `ZoogiAwaken.cpp` |
| In-match HUD (portrait Board Battle layout) | `Source/ZoogiRoll/Private/ZoogiArenaHUDWidget.cpp` / `Public/ZoogiArenaHUDWidget.h` |
| Menus (home hub = reference image, Clash-style tabs) | `ZoogiMenuClash*.cpp`, `ZoogiMenuClashStyle.h` |
| Web UI screens in-game (Title, Deck, Arena, Story, Shop, Settings, Loading) | `Content/ZoogiUI/` (HTML/CSS/JS) + bridge `ZoogiWebUI.cpp` |
| Settings / feature switches | `Config/DefaultGame.ini` (`[ZoogiRoll.*]` sections) |
| Build / package / phone streaming | `Scripts/build.ps1`, `Scripts/package_app.ps1`, `Scripts/package_android.ps1`, `Scripts/pixelstream/` |
| History of decisions | `PROGRESS.md`, `Docs/` |

## Current rules (October 2026)

- **2v2:** you (P1) + CPU ally (P2) vs two CPUs (P3, P4). Starts on the Frozen Ring.
- **Decks:** 5 distinct cards, chosen in the Deck Builder. Your hand is the whole deck; a played card goes to the
  discard and comes back when the draw pile is empty.
- **Your turn:** aim right away. **Tap a card to play it** (no Activate button); keep tapping while you have energy
  (5 per turn). Then drag to aim and release to roll.
- **Awaken:** every card you play adds a pip; at 4 pips the AWAKEN button lights up and powers your Zoogi's element for that turn.
- **HUD (portrait):** top bar with the DRAW / AWAKEN / BATTLE / RESOLVE / END stepper, four player panels (score,
  Awaken pips, Episode energy), and a compact bottom bar (5 card slots, energy, AWAKEN). Reference art:
  `Content/Data/MenuArt/board_ref.png`, `tray_ref.png`, `home_ref.png`.

## Recent work (this branch)

- Stone / gold / sapphire reskin of every menu, Cinzel font.
- Home screen built from the approved reference image, every button live.
- zoogi-ui web screens running inside the game through Unreal's WebBrowser with a JS bridge.
- Full-screen portrait match HUD matching the Board Battle reference.
- Awaken charged by card play; cards optional before aiming; tap-to-play; 5-card decks.
- Windows standalone app and a Pixel Streaming setup for playing on a phone; Android APK setup in progress.

## Building it

Needs the full project (with `Content/`), Unreal Engine 5.6 and Visual Studio 2022. `Scripts/build.ps1` compiles the
editor target; `Scripts/package_app.ps1` makes the Windows game. Useful test flags: `-ZoogiAuto -ZoogiCombo -ZoogiArena=1
-ZoogiArenaHUDTest -ZoogiShots=<dir>` plays a full 2v2 match through the HUD and saves screenshots.
