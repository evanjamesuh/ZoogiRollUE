# Battle Royale (free-for-all ring-out brawl)

Branch `feat/battle-royale` (from tip `f55a82d`). Local only, never pushed.

## What it is
- A new game mode on the PLAY screen: **MODE > BATTLE ROYALE**. It uses the old PRACTICE slot, which said "coming soon". The `play_ref_l/p.png` art is repainted to match.
- Command line: `-ZoogiRoyale` turns the mode on. Add `-ZoogiAuto` for an all-CPU spectator match that quits on its own.
- Works on every existing arena (whichever arena is picked on the PLAY screen), with 2 to 4 Zoogis.
- **There are no turns.** Every Zoogi rolls at the same time.
- Knock rivals off the arena. A ring-out costs one life (3 by default). When a Zoogi runs out of lives it is eliminated. The last Zoogi still in wins.
- A **knockback surge** ramps from x1 to x1.9 between 45 s and 150 s, so every match ends.

## Controls (Oct 26: Marble Clash style)
| | Phone / tablet | PC |
|---|---|---|
| Move | Left touch circle, lower left. A finger anywhere on the left half moves the circle under it. | WASD / arrows (or gamepad left stick) |
| Aim | Drag anywhere on the right side. The crosshair stays at the centre of the screen. | Q / E, or right-mouse drag (or gamepad right stick) |
| Jump | Up-arrow button (right side) | Space (gamepad A) |
| Transform | Swap button (lower right): ball <-> robot, any time, 1 s cooldown | F / Tab (gamepad X) |
| Fire | **No button.** In robot form the arm cannons fire on their own while the crosshair is on a rival (9 deg aim-assist cone, 23 m range, clear line of sight). | same |
| Menu | MENU button, top right | Esc |

Movement is relative to the camera: pushing up on the circle rolls the Zoogi away from the camera.

## The transforming model (ZoogiMorph)
- Every Zoogi in Battle Royale is the rigged Zoogi Blue model (`/Game/ZoogiBlue/Mesh/SK_ZoogiBlue`, 39 clips). Zoogi Blue uses it in every other mode too, where it always stays a ball.
- **Ball form:** the perfect-sphere shell rolls with the physics sphere. Faster (10 m/s), and rams knock rivals back as before.
- **Robot form:** ToBot (0.8 s) opens it into the robot. It hovers on foot thrusters (8-way strafe clips relative to where it faces), is 28% slower, 1.6x heavier and takes 25% less knockback from rams. The body and cannons face the crosshair; a lower, closer camera helps aiming.
- **Plasma bolts:** a shot every 0.26 s, alternating right / left cannon. Each bolt knocks the target back 4.3 m/s (x surge, less against robots), stuns its drive for 0.28 s and counts for ring-out credit. Walls and the respawn shield stop bolts.
- **CPUs:** robots at range (they circle the target and shoot), balls up close (they ram). They hop over incoming bolts.
- The Awaken charge from the first version is not used in this mode any more (the code is still there).

## Rules detail
- **Knockback:**
  - When two Zoogis hit each other, the one moving into the other adds `closing speed x 0.42` of extra knockback to the rival. This only counts above 220 cm/s and is capped at 900 cm/s.
  - Then the surge and Awaken multipliers apply.
  - It is applied on the next tick, outside the physics hit callback.
- **Credit:** a ring-out within 5 s of a rival's hit counts as that rival's KO. Otherwise it reads "rolled off the edge".
- **Respawn:**
  - After 1.6 s the Zoogi drops back in at the safe spot farthest from rivals.
  - It then has 2 s of shield: it is phasing, blinks, and neither takes nor gives knockback.
- **CPU brain:**
  - **Edge sense:** it predicts its own position and checks `AZoogiArena::IsOffCourt` plus a floor trace. If the predicted spot is off the arena, it brakes and heads for the middle.
  - **Targeting:** it picks targets that are near and near the edge, and avoids a transformed rival.
  - **Attacking:** it circles to the centre side of the target and charges outward, leading the target's movement.
- **Orbs** stay on the arena as obstacles. Knocking one off feeds your Awaken charge.

## Code
| File | What |
|---|---|
| `Public/ZoogiRoyale.h` | Tuning (`FZoogiRoyaleTune`, `[ZoogiRoll.Royale]`), match state (`FZoogiRoyale`), touch layout (`ZoogiRoyaleUI::Layout`) |
| `Private/ZoogiRoyale.cpp` | `AZoogiGameMode::Royale*`: start, tick, ring-outs, respawn, Awaken, knockback, CPU brain, frame dump |
| `Private/ZoogiRoyaleInput.cpp` | `AZoogiPlayerController::Royale*`: multi-touch move circle and look drag, keys, chase camera |
| `Private/ZoogiRoyaleHUD.cpp` | `AZoogiHUD::DrawRoyale`: roster with lives, Zoogis left / clock / surge, kill feed, name tags, move circle, AWAKEN button, countdown, results (REMATCH / MENU) |

The hooks into existing code are small:
- `AZoogiGameMode`:
  - Mode switch on `MenuAction Mode 2`, plus the `RoyaleAwaken` action.
  - `StartMatch` branches to the royale start.
  - `Tick` runs `RoyaleTick` in place of the turn flow.
  - Launch flow is off in this mode.
- `AZoogiBall`:
  - `DriveInput` / `DriveAccel` / `DriveMaxSpeed` drive, applied before rolling resistance.
  - In a royale match, `OnSphereHit` hands contacts to `RoyaleOnBallHit`.
- `AZoogiPlayerController::PlayerTick`: the royale input and camera replace `HandleInput` / `UpdateCamera`. The occlusion fade is kept.
- `AZoogiHUD::DrawHUD` draws the royale HUD. The arena UMG HUD is off in this mode (`ZoogiArenaHUD::IsActive`).
- `ZoogiPhoneControls`: `DrawStickAt` / `DrawAwakenAt` / `DrawRingAt` reuse the approved stick and Awaken-sun look.
- `UZoogiMenuRoot`: the PLAY screen MODE slot 2.

Every number lives in `Config/DefaultGame.ini [ZoogiRoll.Royale]`.

## Proof captures
To dump 15 fps frames, run the game with `-ZoogiRoyale -ZoogiAuto -ZoogiShots=<dir> -ZoogiRoyaleFrames`. Named shots are also saved (`royale_countdown`, `royale_fight`, `royale_awaken_P#`, `royale_ringout_*`, `royale_win`).
