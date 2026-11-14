# HANDOFF - Claude session, Oct 26 2026

Branch `feat/battle-royale` in `C:\dev\ZoogiRollUnreal-combined` (worktree of `C:\dev\ZoogiRollUnreal`). Local only, never pushed.

## Done today
- **Battle Royale with the transforming Zoogi Blue model** (commit `c71e18b`)
  - SK_ZoogiBlue (39 clips) imported to `/Game/ZoogiBlue`, cooked always.
  - `UZoogiMorphComponent` (ZoogiMorph.h/.cpp): Blue is the new model in every mode (ball only); every Zoogi in Battle Royale can transform.
  - Marble Clash controls: left stick, drag-to-aim centre crosshair, JUMP (up arrow), TRANSFORM (swap). Robot cannons auto-fire when the crosshair is on a rival (no fire button).
  - Proofs: `C:\dev\proofs\royale_robot_cpu*`, `royale_robot_human`, `blue_model_2v2_ball.jpg`.
- **Windows Shipping package** `Packaged\pkg-royale2` (desktop shortcut NOT changed - still pkg-merge42).
- **E: backup** `E:\ZoogiBackup\2026-10-26\`: pkg-royale2, git bundle `f55a82d..feat/battle-royale`, ZoogiBlue model, proof videos.
- **MODE SELECTION screen** (Marble Clash style, landscape) - replaces every menu while we test.
  - `Source/ZoogiRoll/Private/ZoogiModeSelect.cpp`; hook at the top of `UZoogiMenuRoot::Rebuild`; `-ZoogiOldMenu` restores the old menus.
  - Cards: BATTLE ROYALE (Meadow, 4), 2V2 BATTLE (HomeBattle arena, 4), CLASSIC (Frozen Ring, 4 FFA), TRAINING (1v1 Training Arena), STORY (episode 1 anime). A tap starts the mode straight away.
  - Art: `Content/Data/MenuArt/ModeSelect/*` from `Scripts/make_mode_select.py`; font `Content/Data/Fonts/LilitaOne-Regular.ttf` (SIL OFL).
  - Tested on PC: royale + classic + 2v2 cards start their matches; MENU in a match returns here. Proof `C:\dev\proofs\mode_select_pc.jpg`.
  - `Config/DefaultEngine.ini`: Android `Orientation=SensorLandscape` (was Sensor).

## Android - DONE (mode-select build on the tablet)
- Turnkey reports "Android Studio is not installed correctly" (Platform_InvalidHostPrerequisites). Unreal's "install SDK" prompt auto-downloaded
  `android-studio-2024.1.2.13-windows.exe` to `%TEMP%\Turnkey\DownloadCache` and launched it; Evan declined the UAC prompt. NOTHING was installed.
  The Turnkey warning does not block BuildCookRun - only the Platforms > Android quick-launch/"Install SDK" path tries to fix it. Avoid that menu.
- Toolchain (all already present, nothing installed):
  - SDK  `C:\Users\skybi\AppData\Local\Android\Sdk` (build-tools, platforms, platform-tools, cmdline-tools, ndk)
  - NDK  `...\Sdk\ndk\27.2.12479018` (r27c; also 25.1.8937393)
  - JDK  `C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot`
  - Engine Android support `C:\Program Files\Epic Games\UE_5.6\Engine\Build\Android`
  - Same paths as `Scripts/package_android.ps1` / DefaultEngine.ini `[/Script/AndroidPlatformEditor.AndroidSDKSettings]`; re-entered in
    Project Settings > Platforms > Android SDK (unchanged values).
  - Env vars ANDROID_HOME / NDKROOT / JAVA_HOME: could not be read from this session (no Windows shell); package_android.ps1 sets them itself.
- Build (13:00-13:06 CT): Legacy Project Launcher, custom profile "New Profile 0": build Shipping arm64, cook by the book Android_ASTC,
  package & store locally. Result: Completed 5 / Failed 0. NDK r27c clang found, UBT compiled the module in 4:35.
  - APK `C:\dev\ZoogiRollUnreal-combined\Binaries\Android\ZoogiRoll-Android-Shipping-arm64.apk` (1.07 GB, data inside the APK)
    + Install/Uninstall .bat (same generated install flow as earlier packages). Copies of the .bat files + README in `C:\dev\pkg-modeselect-android`
    (the 1 GB APK was not copied: the drive was too slow under load; use the Binaries\Android one).
- Install (13:10-13:19 CT): same profile switched to no build / deploy "Copy to device" on SM-X133 (adb serial R8YL5170YXL, Android API 36):
  `adb uninstall com.zoogiroll.arena` -> Success, `adb install ...arm64.apk` -> Success (79 s), pm grant READ/WRITE_EXTERNAL_STORAGE, UECommandLine
  cleanup. Deploy step Completed; the auto-launch step was canceled (it hung), so open Zoogi Roll on the tablet by hand.
  Proofs: `C:\dev\proofs\android_modeselect_adb_install.jpg`, `android_modeselect_launcher_done.jpg`.
- Not yet verified on the tablet itself (no screenshot from the device); the game should open straight to MODE SELECTION in landscape.

## Commits (local, not pushed)
- `c71e18b` Battle Royale transforming model + Marble Clash controls
- `ef2c602` Mode Selection screen + Android SensorLandscape. Made with write-tree/commit-tree/update-ref because `git commit` timed out
  (>160 s index refresh on this mount); post-commit hook (git-lfs) did not run. ZoogiMenuRoot.cpp/.h show large diffs that are
  line-ending only (`git diff --ignore-cr-at-eol` = 3 / 5 real lines).

## Next
- Evan to check the tablet build; then iPhone via MacinCloud remote build (needs SSH-enabled Mac with Xcode + Apple Developer certificates).
- Desktop shortcut "Zoogi Roll.lnk" still points at pkg-merge42 (not changed).

## Locks
- `C:\dev\unreal.lock` held by this session during editor builds / packaging; released 13:25 CT. Editor closed.

## iPhone (started 13:40 CT)
- Plan: Unreal remote build from this PC to a MacinCloud Mac over SSH (Mac needs Xcode + Remote Login + admin to hold the signing cert;
  MacinCloud: only the Dedicated plan has admin), then TestFlight to the iPhone.
- Done: DefaultEngine.ini [IOSRuntimeSettings] landscape only, BundleIdentifier=com.zoogiroll.arena, BundleDisplayName=Zoogi Roll (not committed yet).
- Waiting on Evan: which MacinCloud plan / server address + username.

## Tablet crash at start (13:45 CT report)
- Install step was canceled while Unreal "Copy to device" (Do not package) was pushing ~10k loose UFS files to the app's external files dir
  (bUseExternalFilesDir=True); those partial files override the data inside the APK -> crash at startup (likely cause).
- Fix asked of Evan: run `Binaries\Android\Install_ZoogiRoll-Android-Shipping-arm64.bat` (adb uninstall wipes the external dir, then adb install).
- Fallback if it still crashes: APK is 1.07 GB with data inside (bPackageDataInsideApk); try OBB-next-to-APK packaging. Device Output Log only showed an
  Oct 24 tombstone, no live lines for today's run.
- Lesson: never use the launcher "Copy to device" with Do-not-package for this project; install the packaged APK with the generated .bat.

## iPhone - in progress (14:05 CT)
- MacinCloud server TX544.macincloud.com (216.39.74.244), user user294974 (password: Evan only, not stored anywhere).
- Project Settings > Platforms > iOS > Remote Build: Remote Server Name / RSync User Name set; Generate SSH Key clicked - Evan types the password
  in the console. SSH reachability could not be tested from Claude's side (cloud networks blocked).
- Blocker to watch: UE 5.6 Apple_SDK.json caps Xcode at 16.9; the Mac has Xcode 26.6. Editing the engine file was denied by the auto-mode
  folder-access policy - needs Evan's go-ahead (back up + raise MaxVersion to 26.9.0) if UBT rejects the Xcode version.
- Signing still needed: Apple Developer Team ID in Project Settings > iOS, and on the Mac (via RDP) Xcode signed in to Evan's Apple ID.
- Install route options: TestFlight (needs Xcode 26 build + App Store Connect record) or USB: an "Apple iPhone" shows in This PC on the PC.
- Tablet: Install_ZoogiRoll-Android-Shipping-arm64.bat double-clicked by Claude at 14:00 CT (clean reinstall) - result pending Evan's check.
- 14:15 CT: Evan could not type into Unreal's Generate SSH Key console (likely the Win11 default-terminal / cygwin ssh password-prompt issue).
  Workaround: Claude generated the key pair itself at `ZoogiRollUnreal-combined\Build\SSHKeys\TX544.macincloud.com\user294974\RemoteToolChainPrivate.key`
  (+ .pub; Build/ is gitignored, never commit). `C:\dev\mac_add_key.txt` = one Terminal command for the Mac that appends the public key to
  ~/.ssh/authorized_keys. Evan runs it on the Mac over Remote Desktop.
- 14:3x CT: SSH key auth to the Mac works (C:\dev\ios_ssh_test.txt). Mac: macOS 26.2, Xcode 26.6 (iOS 26.5 SDK), no signing identities,
  no Apple ID in Xcode, no sudo, openrsync. Helper: C:\dev\ios\mac.py runs C:\dev\ios\cmd.sh on the Mac (exec'd from the editor Python console).
- iOS compile attempt failed instantly: "Missing files required to build IOS targets" -> UE 5.6 iOS target platform not installed.
  Evan approved: Epic Launcher > UE 5.6.1 > Options > Target Platforms > IOS (1.73 GB) - installing 15:15 CT (editor closed for it).
- Evan approved the Apple_SDK.json edit: MaxVersion 16.9.0 -> 26.9.0 (folder is user-writable, no admin needed).
  Backups: Engine\Config\Apple\Apple_SDK.json.bak-claude and C:\dev\ios\Apple_SDK.orig.json.
- DefaultEngine.ini [IOSRuntimeSettings]: SSHPrivateKeyOverridePath=C:/dev/k_t.key (copy of the key with owner-only ACL).
- Xcode on the Mac: Settings > Apple Accounts > sign-in dialog left open for Evan's Apple ID (Claude does not type credentials).
- 15:25 CT: iOS add-on installed (Epic Launcher). UBT now reaches the Mac over SSH (base dir /Users/user294974/UE5/Builds/EVAN) but stops:
  "Forcing LEGACY XCODE because host OS is not Mac ... Unable to find mobile provision for ZoogiRoll". bAutomaticSigning=True added (not enough:
  legacy Windows remote builds need a .mobileprovision + signing certificate imported on Windows).
- Windows has no Apple Mobile Device Service (no iTunes / Apple Devices) -> UE cannot see or install to the iPhone over USB.
  Plan: TestFlight (no USB, no UDID): Evan signs into Xcode on the Mac -> Claude makes Apple Distribution cert + App Store profile on the Mac,
  exports them to Windows for UE, package For Distribution, upload from the Mac, Evan installs via the TestFlight app.
  Still needs an App Store Connect app record for com.zoogiroll.arena (web, Evan's login).
- Editor reopened (ZoogiRoll). Python helpers in C:\dev\ios\ (mac.py, build_ios.py, udid.py).
- 18:00-18:30 CT: Evan signed Xcode (Mac) into his Apple Developer account: team "Evan James" (7VL2B99BBP), Admin, 0 devices.
  Claude created an Apple Distribution certificate in Xcode; a dummy Xcode app (~/Documents/arena on the Mac) archived + exported with
  -allowProvisioningUpdates registered App ID com.zoogiroll.arena and made "iOS Team Store Provisioning Profile: com.zoogiroll.arena"
  (UUID 4df5c2f7-..., expires 2027-10-26). Copied to C:\dev\ios\ZoogiRoll_AppStore.mobileprovision, project Build\IOS\ZoogiRoll.mobileprovision
  and %LOCALAPPDATA%\Apple Computer\MobileDevice\Provisioning Profiles\ (ZoogiRoll.mobileprovision + <UUID>.mobileprovision).
- DefaultEngine.ini [IOSRuntimeSettings]: bAutomaticSigning=False, IOSTeamID=7VL2B99BBP, MobileProvision=ZoogiRoll.mobileprovision
  (SigningCertificate left empty on purpose: UBT 5.6 only accepts the provision when it is empty).
- UBT legacy remote build needs the signing identity on Windows (IPhonePackager ExportCertificate). With Evan's explicit OK, the Mac keychain
  identities were exported to ~/zr/all.p12 (password "A", what UBT's -ImportCertificatePassword uses; Evan entered the keychain password),
  copied to Intermediate\Remote\ZoogiRoll\IOS\{Development,Shipping}\Certificate.p12 (+Certificate.txt) and imported into the Windows
  user cert store (certutil -user). Delete ~/zr/all.p12 on the Mac and these copies when iOS work is done / rotate if leaked.
- 18:30 CT: started C:\dev\ios\pkg_ios.py = RunUAT BuildCookRun IOS Shipping -distribution ... -archivedirectory=C:\dev\pkg-ios (log C:\dev\ios\pkg_ios.log).
- Note: at 18:09 CT a paste of Evan's old MacinCloud password landed in the editor Python console (Saved\Logs\ZoogiRoll.log); he has reset it.
- 19:00-19:35 CT: App Store Connect record created by Evan: "Zoogi Roll Arena" (com.zoogiroll.arena).
  Remote build fixes: Mac UBT needed ~/Library/Application Support/Epic/UnrealEngine/Intermediate/Build/BuildRules/MarketplaceRules.dll
  (copied from %LOCALAPPDATA%\UnrealEngine\Intermediate\Build\BuildRules; Aura marketplace plugin). Code fix: local vars named `NO`
  (Objective-C macro) renamed to `NOrb` in ZoogiFrontProfile.cpp, ZoogiMatchValidate.cpp, ZoogiMenuClashProfile.cpp (uncommitted).
  Engine upload to the Mac ~16 GB (first time only). Game code now COMPILES for iOS (SDK 26.5).
  "Finalize .app with Xcode" failed: bundle id came from [/Script/MacTargetPlatform.XcodeProjectSettings] (com.YourCompany.ZoogiRoll),
  Development identity, and Xcode rejects Xcode-managed profiles in manual signing. Added XcodeProjectSettings (team, BundleIdentifier,
  IOSSigningIdentity=Apple Distribution, bUseAutomaticCodeSigning=False). Waiting on Evan: manual App Store profile "ZoogiRoll AppStore"
  on developer.apple.com, then Xcode > Download Manual Profiles, replace ZoogiRoll.mobileprovision on Windows, rerun C:\dev\ios\pkg_ios.py.
- 19:30-20:19 CT: Evan made manual profile "ZoogiRoll AppStore" (UUID 4eef96ba-b81a-4086-8fb2-52d50289b2cd, not Xcode-managed);
  now in C:\dev\ios\ZoogiRoll_AppStore.mobileprovision, Build\IOS\ZoogiRoll.mobileprovision, %LOCALAPPDATA%\Apple Computer\MobileDevice\Provisioning Profiles.
  UBT remote build deletes profiles it imported on the Mac afterwards -> fin.sh re-copies ~/zr/appstore.mobileprovision.
- Mac UBT reads the signing name from the FIRST cert in Certificate.p12 -> rebuilt it with only the distribution identity (~/zr/dist.p12, pw "A").
- iOS game binary built + signed stub OK. WARNING: UBT's "Downloading build products" rsync deleted everything in the PC's
  ZoogiRollUnreal-combined\Binaries except IOS (Win64 editor DLL rebuilt since; the mode-select Android APK in Binaries\Android is GONE - rebuild).
- Editor build blocked by Live Coding while the editor is open -> close the editor before BuildCookRun with -build.
- Cook + stage OK (Saved\StagedBuilds\IOS, 833 MB). Windows IPhonePackager signing FAILS ("CMS signature blob size is too large") with current
  Apple certs -> final packaging moved to the Mac: C:\dev\ios\fin.sh (copied to ~/zr/fin.sh): stub .app + ~/zr/stage (staged data, uploaded by
  C:\dev\ios\upstage2.py) -> embedded profile -> codesign via temp keychain (dist.p12) -> hand-made .xcarchive -> xcodebuild -exportArchive
  destination=upload (Evan's Xcode account). ITSAppUsesNonExemptEncryption=false added.
- 20:19 CT: **UPLOADED to App Store Connect: Zoogi Roll Arena 1.0 (build 1)**. Next uploads: rerun cook/stage (BuildCookRun -skipbuild if code
  unchanged), upstage2.py, then runfin.py with a higher build number (edit `fin.sh 1` -> `fin.sh 2`).
- Uncommitted: Source NO->NOrb renames (3 files), DefaultEngine.ini iOS/Xcode settings, Build\IOS\ZoogiRoll.mobileprovision is under gitignored Build\.
- Secrets on disk (delete/rotate when done with iOS): Mac ~/zr/all.p12, ~/zr/dist.p12; PC Intermediate\Remote\ZoogiRoll\IOS\*\Certificate.p12,
  Windows user cert store (Apple Development + Apple Distribution, imported 18:26).
- 20:45 CT: build 1 FAILED Apple processing: ITMS 90683 missing NSCameraUsageDescription (engine links camera APIs). fin.sh now adds
  camera/mic/photos/bluetooth/location/motion/speech purpose strings. Build 2 uploaded 20:52 CT -> "Processing" in TestFlight.
  TODO: move these strings into DefaultEngine.ini [IOSRuntimeSettings] AdditionalPlistData so UE adds them itself.
- 21:05 CT: build 2 processed (Complete). TestFlight internal group "Zoogi Testers" (automatic distribution) created by Claude with Evan's OK,
  tester evancljames Apple ID added, build 2 in group, 1 invite sent. Evan installs via the TestFlight app.
- 21:25 CT: **Zoogi Roll Arena runs on Evan's iPhone via TestFlight (build 2).** Invite emails had bounced because his iCloud storage was full.
- 21:33 CT: iPhone feedback: JUMP / TRANSFORM buttons did not respond. Cause: RoyaleInput overrode Touch1's position with the cursor
  position (PC mouse-as-touch fix); on iOS that is a stale copy of the previous touch. Now #if PLATFORM_DESKTOP; button hit circles 1.2 -> 1.5x.
  (ZoogiRoyaleInput.cpp, uncommitted.)
- New fast iOS path (no PC rsync, no Binaries wipe): C:\dev\ios\ios3.py uploads changed source to the Mac mirror and runs ~/zr/build3.sh
  (Mac UBT -> stub -> fin.sh sign + upload). Only for code changes; content changes still need cook/stage + upstage2.py.
- 21:40 CT: build 3 uploaded (auto-distributes to Zoogi Testers after processing).
- Oct 27 08:00 CT (Evan: "mimic the exact camera, bullet projectiles, hit indicators, hud" of the Marble Clash reference shots + "delete the right stick"):
  * Root cause of dead JUMP / TRANSFORM on iPhone: UE's stock virtual joysticks (DefaultInput.ini DefaultTouchInterface=DefaultVirtualJoysticks)
    sat over the right side and ate the taps (the faint "right stick"). Now ActivateTouchInterface(nullptr) in PlayerController BeginPlay and
    OnRoyaleStarted, and DefaultTouchInterface=None. Drag-to-aim on the right half stays (invisible, like the reference); the DRAG TO AIM ring is gone.
  * Camera: low chase cam (ChaseBack 300 / ChaseUp 115 / level view / FOV 95, robot 330/150), wall sweep pulls it in. ZoogiRoyaleInput.cpp.
  * Tracers: AZoogiRoyaleBolt is a 2.6 m thin white-hot streak (SetStreak), TracerSpeed 5200, yellow muzzle flash + impact sparks.
  * HP: MaxHP 1200 per life, bolt 32 (crit 18% x1.45), ram = closing speed x 0.09; 0 HP = destroyed (RoyaleLoseLife(B, true)), refills on respawn.
  * HUD rewritten (ZoogiRoyaleHUD.cpp): LEADERBOARD panel, clock + life dots, menu bars, HP bar, rival name/HP tags + red chevron,
    floating damage numbers, red hit marker, damage vignette, purple YOU DESTROY banner, white-ring buttons at the reference positions.
  * New [ZoogiRoll.Royale] keys in DefaultGame.ini (code defaults match, so the fast iOS path needs no recook). -ZoogiRoyaleBotHuman = CPU drives P1 (captures).
  * Scripts: C:\dev\ios\b4run.py (Win64 editor build + capture runs -> C:\dev\proofs\royale_oct27), ios4.py (Mac upload + build3.sh 4), tail4.py.
  * 09:00 CT: tuning after the first captures - chase cam 270/125, robot 470/175, tracers 11 cm wide + brighter, own stick drawing (RStick),
    sRGB-corrected HUD colours, damage numbers 40 ref px. Win64 editor build OK; captures in C:\dev\proofs\royale_oct27\{bot,idle}.
  * 09:02 CT: ios4.py uploaded the 10 changed files to the Mac mirror and started ~/zr/build3.sh 4 (log ~/zr/build4.log; C:\dev\ios\tail4.py reads it).
  * 09:04 CT: build 4 compiled + signed on the Mac, but the upload FAILED: Evan's Apple ID session in Xcode on the Mac expired
    ("exportArchive Failed to Use Accounts", rc 70). Evan must sign in again (Xcode > Settings > Accounts), then run C:\dev\ios\runfin4.py
    (fin.sh 4 only, no rebuild); C:\dev\ios\tailfin4.py reads ~/zr/fin4.log.
  * 09:20 CT: music off (Evan: "delete game music leave only sfx"): ZoogiUtil::GetMusicVolume() returns 0 unless [ZoogiRoll.Audio] bMusic=True
    or -ZoogiMusic; PlayMusic / StartArena / EndMatch / PlayShiftHit return early. ZoogiTypes.cpp, ZoogiGameMode.cpp, ZoogiMusicSubsystem.cpp.
    Uploaded via ios4b.py, Mac recompile of build 4 OK (same upload failure until Evan re-logs in).
  * 09:30 CT: with Evan's OK, App Store Connect API access requested (terms accepted) and team key "Zoogi Upload" (App Manager,
    Key ID VY8Z87BCDX, issuer = team id) generated; AuthKey .p8 in C:\dev\ios\keys\ (also Evan's Downloads) and ~/zr on the Mac.
    fin.sh now uploads with -authenticationKeyPath/-ID/-IssuerID: no Xcode Apple ID login needed any more. SECRET: keep out of git.
  * 09:39 CT: **build 4 uploaded** (EXPORT SUCCEEDED) -> TestFlight Processing, auto-distributes to Zoogi Testers.
- Oct 27 10:30 CT (Evan: buttons work; "turn training into a place only one player exists unless a bot is added from the in game menu ...
  add the battle royal controls so users can play interchangeable"):
  * Training now runs on the royale engine as a sandbox (AZoogiGameMode::RoyaleStartTraining / RoyaleTrainAction, ZoogiRoyale.cpp):
    solo on the Training (Teaser) island, endless lives, no surge, no countdown. Hamburger in training opens a TRAINING menu:
    ADD BOT / REMOVE BOT (0..3, restarts the sandbox), SWITCH CONTROLS, RESET, EXIT, CLOSE. A CONTROLS pill under the clock switches live.
  * MARBLE controls: pull back on the left half + release -> AZoogiBall::Launch with ZoogiConst::LaunchSpeedForPull (no hold);
    only from (nearly) rest and in ball form; camera turns to chase. BRAWL controls unchanged.
  * ClashStartTraining (ZoogiMenuClash.cpp) + Mode Select card 3 go here. Capture switches: -ZoogiTraining -ZoogiTrainingMarble
    -ZoogiTrainBots=N -ZoogiMarbleAutoTest -ZoogiTrainPanelTest. Proofs: C:\dev\proofs\training_oct27.
  * 10:54 CT: build 5 sent to the Mac (ios5.py -> build3.sh 5; upload uses the API key).
- Oct 27 audits for the DEV-mode spec (Evan's long prompt): existing ZoogiDevMode.cpp (bumpers/ship only, mouse, no shipping gate,
  C:/dev path), classic launch "pause" = the 0.45 s PowerUp hold + aim spin decaying after release (ZoogiTargeting/ZoogiBall),
  classic camera = ZoogiCamTune hard-coded profiles, TryPower dead in artifact mode, no in-match profile toggle. Plan doc next.
  * ~11:10 CT: build 5 (training sandbox) processed in TestFlight ("Ready to Submit"), auto-distributed to Zoogi Testers.
  * Dev Mode plan doc (audit + architecture + phases): https://claude.ai/artifact/SnWvHRxjSY6GCeRj7xqJSx . Open question to Evan:
    PIN-gate the DEV button for other TestFlight testers? Next: Phase 1 (config store first).
  * CORRECTION 11:35 CT: build 5 had NOT reached TestFlight (the 10:56 upload failed: "exportArchive Error Downloading App
    Information", transient). Re-ran fin.sh 5 via runfin5.py -> 11:36 CT "Upload succeeded". (The earlier "Ready to Submit"
    for build 5 was a misread of the page.)
- Oct 27 ~12:10 CT  DEV MODE PHASE 1 (Evan's spec; PIN chosen by Evan, stored only as an FNV hash in ZoogiDevConfig.cpp):
  * NEW ZoogiDevConfig.h/.cpp: versioned JSON store <Saved>/ZoogiDev/devconfig.json (atomic save), scopes global/mode:/level:/screen:/dev
    with precedence, Set/Remove with undo/redo stack + change history, ExportPackage -> <Saved>/ZoogiDev/exports/*.json + .txt and the
    clipboard. ZoogiDevGate: DEV button ([ZoogiRoll.Dev] bDevButton, default on; set False for an App Store build) + PIN, unlock
    remembered in GameUserSettings [ZoogiRoll.Dev] bUnlocked until LOCK. -ZoogiDev = unlocked + panel open (PC runs).
  * ZoogiDevMode.h/.cpp rewritten touch-first: DEV button bottom-right on every screen -> PIN pad -> panel (game paused): PLAY TEST,
    TOP VIEW, UNDO/REDO, SNAP, GRID (Slate-painted floor grid, works in Shipping), +BUMPER, DUPLICATE, DELETE, RESET OBJECT, X/Y/ROTATE/SIZE
    -/+ (FINE steps), SAVE, LOAD, EXPORT, RESET ALL, LOCK. Gestures: tap / long-press select, drag move, pinch size, twist rotate,
    one-finger pan + pinch zoom in top view. Mouse/keys still work on PC.
  * FZoogiDevProp now has a stable Id ("ship", "bumper_<n>"); AZoogiArena::BaselineProps = shipped layout (RESET). A dev-unlocked
    device applies its saved layout at arena build (ApplyLayoutOverrides).
  * PC test (-ZoogiTraining -ZoogiDev -ZoogiDevAuto): drag/rotate/size/undo/redo/save/export OK, restart reloads the layout. Proofs:
    C:\dev\proofs\devmode_oct27. NOTE test data: Saved\ZoogiDev\devconfig.json and C:\dev\zoogi-layouts\teaser.json (bumper_1 moved) -
    PC-only, only applied with -ZoogiDev; do NOT bake teaser.json.
  * Build 6 sent to the Mac (ios6.py -> build3.sh 6; tail6.py reads ~/zr/build6.log).
  * 12:13 CT verified on App Store Connect: build 5 Complete + in Zoogi Testers; build 6 (Dev Mode phase 1) uploaded 12:09, Processing.

- 2026-10-27 12:50 CT (Claude) - BUILD 7: Dev Mode touch fix + safe area + CAMERA tab (Phase 2 start)
  * Evan on build 6: panel buttons did nothing when pressed; DEV button cut off by the phone's rounded corner.
  * Cause: the overlay root handled every OnTouchStarted while the panel was open, so Slate never fell back to a mouse click for the
    SButtons (the PIN pad worked because editing was off). Fix (ZoogiDevMode.cpp): new SZoogiDevCatcher - a full-screen sibling layer
    UNDER the panel that takes the select/drag/pinch/twist touches; root is SelfHitTestInvisible. Buttons get their own touches now.
  * DEV button, PIN pad and panel are inside an SSafeZone (+6 px); DEV button 100x52, inset 40/26 px from the corner; panel 420 wide.
  * CAMERA tab (new ZoogiCamConfig.h/.cpp): per mode (classic/combo/royale/training) profiles A standard / B close / C overhead, -/+
    per value (distance, height, tilt, FOV, robot distance/height, follow, turn speed, shake; classic: angle, distance, FOV, smoothing),
    RESET THIS PROFILE. Stored in the dev store (scope mode:<mode>); players without the dev unlock always get shipped A.
    The camera keeps updating while paused (AZoogiPlayerController::SetFullTickWhenPaused). HUD / SPAWNS tabs: placeholders.
  * PC test b9run (proofs C:\dev\proofs\devmode2_oct27\auto|classic|button): builds, camera profiles switch, DEV button visible.
  * Mac: ios7.py's upload of ZoogiCamConfig.cpp failed (ssh 0xC0000142 when run detached) -> ios7b.py killed that build, re-sent the
    file and restarted build3.sh 7. Run ios scripts with exec() in the editor console, not detached. tail7.py reads ~/zr/build7.log.
  * 12:52 CT verified on App Store Connect: build 7 uploaded 12:47, Processing (builds 2-6 in Zoogi Testers).

- 2026-10-27 13:05-14:10 CT (Claude) - BUILD 8 + BUILD 9
  * Build 8 (uploaded 13:10): DEV button hidden while G->Phase == Menu (Evan could not pick a mode); SSafeZone + inner overlay
    explicitly SelfHitTestInvisible.
  * Build 9 (Evan: free roam in dev mode; Phase 3; "no delay for transforming or jumping"; remove the phase tracker and crystals):
    - Dev Mode FREE CAM (ZoogiDevMode.h/.cpp): FREE CAM / HIDE PANEL buttons; left 42% of the screen = move stick (drawn), drag
      elsewhere = look, pinch = fly forward/back, UP/DOWN hold buttons, SPEED slow/normal/fast; hidden panel -> top-right strip
      (PANEL / UP / DOWN / SPEED). Real-time movement while paused (FApp::GetDeltaTime). Tap on the left side still selects.
    - TRANSFORM / JUMP: ZoogiRoyaleFeel (ZoogiRoyale.h): morph clips sped up to 0.3 s, MorphCooldown <= 0.15, JumpCooldown <= 0.15,
      no drive slow-down while morphing (caps win over the old ini values; DefaultGame.ini updated too).
    - Classic HUD: phase tracker plate (< DRAG TO AIM > + bar) and the 5 charge crystals removed (ZoogiArenaHUDWidget BuildTray /
      BuildEpisode); canvas stage chips off (ZoogiHUD DrawStages). YOUR TURN + ROUND lines stay.
    - Phase 3: classic SUPERPOWER (TRANSFORM spot, star: CardsUseAwaken when charged, else TryPower) and REACTION (JUMP spot,
      arrow: hop while your shot rolls, once per shot) - AZoogiHUD::DrawClassicButtons (ZoogiRoyaleHUD.cpp), MenuAction
      ClassicPower / ClassicReaction, AZoogiGameMode::Classic* (ZoogiPowers.cpp). Classic only (not 2v2 / royale).
    - Strategic zoom: classic Rolling camera pulls out x1.45, +10 deg, pivot 30% to centre over 0.5 s (ZoogiPlayerController.cpp).
    - Launch on release: PowerUpHold() = 0 for a human classic shot (CPU / cinematic / 2v2 unchanged; -ZoogiPowerUpHold = old);
      waits for the aim if it is not settled. PC test: "INPUT drag launch" -> "fires" 2 ms later; REACTION hop logged.
    - NOTE: the old -ZoogiHumanTest lock+Space path never fires (v=0 even with the old hold) - pre-existing harness issue, not
      gameplay; -ZoogiClassicButtonsTest drives a real free-aim drag instead.
    - Proofs C:\dev\proofs\build9_oct27 (auto = free cam, classic / frozen = buttons + zoom). ios9.py -> build3.sh 9.

- 2026-10-27 16:00-16:55 CT (Claude) - BUILD 10: Evan's "Build 9 corrections + Frozen Ring gameplay" prompt (Frozen first)
  Evan's answers: bridge entry on the FIRST launch and again after a knock-out (later turns aim from where the Zoogi stops);
  remove the ship / launch pad; players start in front of the bleachers, placed precisely in Dev Mode.
  * NEW ZoogiFrozenRoam.h/.cpp: classic on the Frozen Ring (Ring 2 on): AZoogiArena::SetFrozenRoam - ship/pad destroyed, dev props
    "spawn" spawn_1..4 (Ring 2 floor r1950, yaws 125/56/212/325, face the court; coloured pads, dev-only) and "launch" launch_<yaw>
    on the 6 bridges (span A r1645, heading into the court; subtle blue ring + arrow for players, yellow gate bars dev-only).
    GetSpawnPoint/Yaw read the spawn props. States SPAWNING / FREE_ROAM / BRIDGE_APPROACH / LAUNCH_PREPARATION / LAUNCHED (logged).
    Free roam: ball drive (DriveInput) from the left stick, clamped to the Ring 2 band (no crevasse, ramps, islets) except down a
    bridge corridor; gate entry never launches; reaching the launch spot steadies the Zoogi there (settle time), faces it into
    the court, then only the right stick launches (HumanDrag through bRoamLaunchGate; CommitLock / drag / Space launches refused).
    CPU: steers round the ring to the nearest bridge, steadies, PlanCPUShot from the bridge. Knock-out -> FrozenRespawn on its
    spawn pad, re-enters via a bridge. Zoogis still outside are not targets. Every Frozen player gets the robot-ball model.
    Off switch: [ZoogiRoll.Frozen] bOuterRoam=False / -ZoogiFrozenRoam=Off. Spawn elevators, DropIntro and the launch flow skip.
  * NEW ZoogiFrozenInput.cpp (PC): Frozen human aim turns = left stick (roll / small aim turn on the bridge), right stick pull-back
    (power + heading, release launches), tap = orb / player target only (empty ground ignored). HUD buttons / UMG keep their presses.
    HandleInput hands over to it on the Frozen Ring. HUD (ZoogiRoyaleHUD DrawClassicButtons): both sticks drawn with ROLL / TURN /
    PULL BACK / LET GO labels + power arc; SUPERPOWER above the right stick, REACTION in its place while rolling. No aim arrow while roaming.
  * NEW ZoogiTune.h/.cpp: live tunables (dev store "global" tune.<key>): DEV BUTTON x/y/size/opacity, LEFT STICK x/y/size/opacity/
    sensitivity/dead zone/curve, RIGHT STICK x/y/size/opacity/min pull/power curve/max power, ROLLING speed/accel/camera turn,
    BRIDGE LAUNCH catch radius/settle/aim cone/gate width. Dev Mode HUD tab = -/+ rows + RESET per group. SPAWNS tab = TEST SPAWN 1-4.
  * DEV button: round, same size as TAC, just left of it (ZoogiArenaHUD::TacRect), own touch handling (SZoogiTouchWrap) so a press
    never reaches gameplay; every panel button wrapped the same way. Fallback bottom-right when no TAC (royale / training).
  * PC tests C:\dev\proofs\build10_oct27 (frozen = -ZoogiFrozenRoamTest -ZoogiFrozenKnockTest, devfrozen = Dev Mode tabs):
    spawn on the ground z22, FREE_ROAM -> BRIDGE_APPROACH -> LAUNCH_PREPARATION -> LAUNCHED for human and CPU, knocked-out P2 back on the
    ring and re-entering, arena turns via the right stick, Meadow classic unchanged. PC dev store has test edits (bumper_1 scaled up).
  * ios10.py -> build3.sh 10.


## Oct 27 evening - BUILD 11 (Claude): classic back to the early-October aim bar + cinematic camera, Frozen bridge entry removed
  Evan: "For marble mode go back to the original aim and shoot system ... cinematic view ... action close up". Answers: Classic (all
  maps), Early October aim bar, Cinematic by default, Frozen bridge entry = remove it (everyone starts in place, no ship, no rolling to gates).
  * AZoogiGameMode::ClassicAimBar() (ZoogiPowers.cpp) = classic, not combo / royale, not -ZoogiDragAim. When on: the AwakenWidget
    aim bar (<< < > >> AUTO CONTROL DRIVE LAUNCH NEXT CONFIRM CANCEL) shows on the human aim step (arena HUD branch no longer hides it),
    a tap on a rival / orb aims (AimAt), no slingshot drag, no aim arrow (ZoogiAimArrow::IsShown). Prompt "Tap a rival or an orb...".
  * Camera: bCinemaEnabled=true; StartMatch sets bCinematicCam unless -ZoogiTactical / -ZoogiNoCinematic. CAM button cycles
    CINE -> CAM -> TAC. PowerUpHold(): cinematic hero shot holds BattleDuration(). Combo match start keeps cinematic off.
  * Frozen: ZoogiFrozenRoam::BridgeEntryOn() default false ([ZoogiRoll.Frozen] bBridgeEntry / -ZoogiFrozenBridges=On brings the
    roam/bridge flow back). Off: no ship, spawn props on the original court sockets, no launch props, sticks / FrozenInput / roam gates off.
  * FIX ZoogiTargeting IsValidTarget: a rival parked under the launch pad (bParked) is no longer a target. It sat at the shooter's
    X/Y, so CONFIRM made a zero-length shot that was always cancelled ("FIRE P1 cancelled ... target at same point").
  * PC tests C:\dev\proofs\build11_oct27 (classic Meadow + frozen, -ZoogiClassicButtonsTest): aim bar + CINE label, "P1 fires at Orb",
    P2 fires, Resolve each turn; Frozen "4 spawn points (court start spots), 0 bridge launch points", P1/P2 on the court r980,
    cinematic close-ups (hero shot behind the ball, LIFT OFF / HALO BANK action cut). contact.jpg = summary sheet.
  * ios11.py -> build3.sh 11 (14 files).
  * Mac: EXPORT SUCCEEDED, upload succeeded 7:24 PM (EXPORT_UPLOAD_RC=0). App Store Connect check skipped: Chrome was signed out of ASC.


## Oct 28 morning - BUILD 12 (Claude): aim bar presets + drag arrow, SUPERPOWER under REACTION, Wolf Pack only, comic bubbles, portrait
  Evan: smaller reaction images as comment bubbles from the player's avatar; classic vertical phone play; remove the 4 arrows left
  of AUTO and the 3 buttons right of LAUNCH; drag-to-aim arrow back (arrow drags left/right for off-centre shots) keeping the
  buttons, restyled like MODE SELECTION; power button under REACTION; Wolfgang clone (Wolf Pack) the only power in classic + 2v2;
  menu converts to portrait. Answer: "Drag fires, buttons = presets".
  * Aim bar (ZoogiAwakenWidget::BuildAim, classic only): AUTO / CONTROL / DRIVE / LAUNCH plates (SlantPlate procedural texture +
    Lilita One via ZoogiModeSel::FontInfo), no backing panel. New MenuAction "AimPreset" (AZoogiGameMode::AimPreset): sets the
    strength, shoots at the picked target (CommitLock), "TAP A RIVAL OR AN ORB FIRST" otherwise. Landscape: bottom centre;
    portrait: bottom left (clear of the button column). 2v2 combo bar unchanged.
  * Drag aim back: PlayerController's ClassicAimBar tap-only branch removed, ZoogiAimArrow::IsShown no longer hides the arrow.
  * SUPERPOWER drawn straight under REACTION (ZoogiRoyaleHUD DrawClassicButtons), x<charges> badge. Buttons now also in 2v2.
  * Wolf Pack only: AZoogiGameMode::WolfPackOnly() = !bRoyaleMode; ApplyWolfPackOnly after InitZoogi (classic + combo) sets
    Def.PowerKind = WolfPack. CanUsePower ignores artifact mode then; CPU may use it; a gold star orb also adds a charge.
    Clones use Wolfgang's mesh again (/Game/Zoogis/wolfgang/SM_wolfgang), tinted by the caster.
  * Comic slices (ZoogiComicSliceOverlay::OnHUDPostRender): white comic bubble with tail out of the player's avatar ring
    (ZoogiArenaHUD::AvatarRect; shooter when the event names no one), ~19% of the short side, grows in / shrinks back.
  * Portrait: NEW ZoogiOrientation.h/.cpp - menus + classic allow portrait + landscape, royale / 2v2 landscape only (GameMode Tick).
    iOS sets IOSView->SupportedInterfaceOrientations = AllButUpsideDown + setNeedsUpdateOfSupportedInterfaceOrientations (the
    engine's FullSensor = MaskAll is treated as "no override" by IOSAppDelegate -> ini landscape only). Android: SetAllowedDeviceOrientation.
    MODE SELECTION portrait = 1080-wide 2-column card grid (vertical scroll), title on its own line. Board portrait HUD: logo band +
    phase stepper removed (matches build 9 landscape).
  * PC tests C:\dev\proofs\build12_oct28 (classic / portrait / combo / menu_p): AIM PRESET fires, wolf pack 3 clones (classic + 2v2),
    bubbles from avatars in both orientations, portrait menu grid (sheet_l.jpg, sheet_p.jpg, sheet_p2.jpg).
  * ios12.py -> build3.sh 12 (17 files incl. new ZoogiOrientation). The iOS-only Objective-C part is first compiled on the Mac.
  * Mac: compiled clean (incl. the iOS orientation code), EXPORT SUCCEEDED, upload succeeded 9:28 AM (EXPORT_UPLOAD_RC=0).


## Oct 28 afternoon - BUILD 13 (Claude): portrait chrome row, no arena-name plate, REACTION / SUPERPOWER touch buttons
  Evan (upright screenshot): TAC / DEV / cog centred under the round counter, animated in only when an avatar is tapped and the
  profiles show; remove the HUD overlay at the top (portrait) so it is see-through like landscape; leave landscape positions alone;
  restyle REACTION / SUPERPOWER and make them fire on every intended tap (centre taps missed, slightly off-centre worked).
  * Portrait Board HUD (BuildBoard): arena-name plate removed (its FROZEN RING line sat on ROUND n / 8). NEW BuildChromeRow:
    [DEV] [TAC] [cog] centred under ROUND, split Kind 4 = with the profiles (match start, avatar tap peek), slides up / fades,
    buttons collapse while hidden. Landscape BuildTopBar unchanged (cluster under the right plate).
  * ZoogiDevMode: DEV hides with TAC when the arena HUD is up and TAC is hidden, fades with it (ZoogiArenaHUD::TacOpacity),
    gap to TAC = 0.2 x TAC size.
  * NEW ZoogiClassicPad.h/.cpp: REACTION / SUPERPOWER are Slate touch widgets on the game viewport (ZOrder 55): fire on
    touch-down anywhere in a 2.7R x 2.8R box (circle + label), capture the touch so it never reaches aiming. Mode-select look:
    teal / magenta plates, thick dark rim, lit top, white icons (procedural), Lilita labels, gold charge badge; grey when not
    ready; pulse when ready. Canvas versions only remain for the Frozen bridge flow (bBridgeEntry).
  * PC: C:\dev\proofs\build13_oct28 (sheet13.jpg): portrait row in at start, gone when minimized, back on peek; clicks on the
    button centre and edge both log "CLASSIC PAD ... pressed". Note: Evan's PC now has a new primary monitor (EK240Y, 1080 tall):
    portrait test windows must be <= ~1000 px tall (600x1300 fell back to 1600x900); project browser Browse ~ (1010,695).
  * ios13.py -> build3.sh 13 (6 files).
  * Mac: compiled clean, EXPORT SUCCEEDED, upload succeeded 2:21 PM (EXPORT_UPLOAD_RC=0).


## Oct 29 - BUILD 14 (Claude): classic MAP SELECTION
  Evan: after CLASSIC, a map selection screen in the same style, real scene of each map as the card picture, BACK button.
  * ZoogiModeSelect.cpp: CLASSIC sets bMapSelect (ZoogiMenuRoot.h) -> the same row / portrait grid builds the map cards
    (ZoogiModeSel::Maps: Frozen, Meadow, Volcanic, Cosmic, Arabian, Tomb, Neon, Teaser); title ms_title_map; BACK returns to
    the modes; StartMapCard = Mode 0, 4 players (you + 3 CPU), Arena, Start. Missing art -> plain colour plate with the name.
  * Art: arena showcase captures (-ZoogiArenaShots -ZoogiNoHUD, C:\dev\proofs\build14_oct29\map_*), cards made by
    Scripts/make_map_cards.py + Scripts/map_cards_pick.json (same frame as make_mode_select.py) ->
    Content/Data/MenuArt/ModeSelect/ms_map_*.png + ms_title_map.png. Editor asked to import them: answered Don't Import (raw files).
  * iOS fast path: the staged MenuArt lives inside zoogiroll-ios.pak, so the new PNGs go in a patch pak
    C:\dev\ios\zoogiroll-ios_1_P.pak (mkpak14.py, UnrealPak, mount ../../../ZoogiRoll/Content/Data/MenuArt/ModeSelect/) copied
    into ~/zr/stage/cookeddata/zoogiroll/content/paks/ (stays there for later builds). First time this route is used: if the
    phone shows plain colour plates instead of pictures, the patch pak did not mount.
  * PC: menu landscape + portrait checked with the art (CLASSIC -> MAP SELECTION -> BACK; Volcanic Pit started a 4-player match).
  * Mac: EXPORT SUCCEEDED, upload 12:57 PM (build 14).
  * PC note: Evan's PC had rebooted - Unreal was started from the Epic launcher (Library > Launch 5.6.1) -> project browser ->
    Browse -> ZoogiRoll.uproject. computer_open_application("Unrealeditor") starts a NEW instance each call: do not call it
    while one is running (7 project browsers piled up and the editor crashed restoring from the taskbar). Editor now opens on EK240Y.


## Oct 29 afternoon - BUILD 15 (Claude): screen SHATTER, clone shockwave, thrust portals on every arch, Dev Mode obstacles + bumpers
  Evan (with the Vefects Easy Shockwaves picture): SHATTERED moments crack the screen in four pieces that fall away; Dev Mode can
  select any obstacle, make anything a bumper with a rebound-force scale; all arches get a blow-through door that is a thrust
  portal; the Wolf Pack clone detonates with a shockwave when its momentum stops, pushing orbs away.
  * NEW ZoogiScreenFx.h/.cpp (UTickableWorldSubsystem, draws in AHUD::OnHUDPostRender):
    - Shatter(World, Why): grabs the frame (one SceneCapture2D, FinalColorLDR, half res, exact live camera + FOV, captured in the HUD
      draw so it matches that frame), jagged cracks race from near the middle to the 4 screen edges (0.14 s) with hairline branches,
      the frozen frame holds, then the 4 pieces drop (bottom first) with gravity + spin, revealing the live game (~1.9 s total).
      Triggers: battle SHATTERED signal (SM->ShatterCount), rock SHATTERED! popup (OnRockEvent), GUARD SHATTERED callout
      (ZoogiEpisodePresent). 2.5 s cooldown. [ZoogiRoll.ScreenFx] bShatter / bShockwave, -ZoogiNoScreenShatter / -ZoogiNoScreenWave.
    - Shockwave(World, C, R, Tint): ground ring projected to the screen, refraction band (per-frame half-res capture while a wave
      is alive, UVs pushed toward the centre) + additive tinted crest, 0.62 s.
    - Test: -ZoogiScreenFxTest (+ -ZoogiShots=dir): shatter at 12 s, wave mid-view at 16 s, a Wolf Pack clone at an orb at 19 s.
  * ZoogiClone: after 0.6 s, planar speed < 140 for 0.18 s (or life end: 3.2 s, 5.5 s once it has hit something; the turn already
    waits for clones) -> Detonate: orbs within 430 cm get 420..1800 cm/s outward + a small hop, LastHitBy = caster; FX = flash +
    AZoogiFX ring + Vefects AZoogiFX::Shockwave Big (PC; no-op where not cooked) + ScreenFx wave + CritBoom. Glow charges while it stops.
  * ZoogiPortals: every map + every mode (2v2 too). Doors on every arena piece whose kind contains "arch" (no bridge-yaw filter;
    Frozen arch dims kept, others from bounds), standing floor Resonance Halos (round door) and Recovery Gate props (skipped when
    inside an arch). Rebuilds when the set changes; the frame follows its owner (forming, Dev Mode moves). Idle door: sheet 0.3 +
    pulse + 6 wind streaks blowing through. Fires only while Rolling (unchanged rules). CPU safe-run planner uses Owner now.
    Frozen: 6 arch doors + 4 prop doors (halo_w / halo_e etc.). Meadow / Volcanic etc. have no arch pieces in play (decor only).
  * Dev Mode: arena pieces (not stands / bridges / rim), obstacles, rocks and battle props on the court become "obstacle" dev props
    (ids arch_1, pillar_2, rock_3, halo_1...) once the stage has formed (AZoogiDevMode::Tick -> AZoogiArena::RegisterObstacleProps;
    rescans every 2 s while editing). Move / turn / size moves the real actor; RESET OBJECT / RESET ALL put it back; only changed
    obstacles are saved (layout JSON gets "bumper" / "bounce"; entries apply when the obstacle registers).
    OBJECTS tab: BUMPER ON/OFF + [-] REBOUND n/10 xM [+] for any star bumper or obstacle.
  * NEW ZoogiBumpers.h/.cpp (UZoogiBumperSubsystem): active bumper = OnComponentHit on its solid parts; out speed =
    max(v_in x (0.8 + 0.2 L), 250 + 110 L), cap 3200, along the reflection about the closest-point normal (flat for side hits),
    0.2 s cooldown per object, impact flash + Clack. Level 1 x1.0 ... 10 x2.8.
  * Fix: UZoogiCardSubsystem::BeginStandby null player state (crashed the -ZoogiDev capture run) -> warning + skip.
  * Not baked: bake-layouts.ps1 / ZoogiLayoutBaked.inc still only know star_bumper / ship (no obstacle / bumper fields yet).
  * Vefects Easy Shockwaves is in Content/Vefects but not in DirectoriesToAlwaysCook and loaded by soft path, so the iOS build
    most likely lacks it (ios15 probe greps the utoc); the procedural screen shockwave is what the phone shows. Adding it = an
    ini change + full cook (not the fast path).
  * PC: C:\dev\proofs\build15_oct29 (fx_frozen, fx_portrait, fx_meadow, devobs, combo).
    Checked: shatter cracks + falling pieces (landscape + 470x1000 portrait, colours match the live frame), screen shockwave
    (lens ring on Volcanic, subtle on bright Frozen; translucent crest + dark trough added for snow), Wolf Pack clones
    detonating in real CPU turns ("POWER wolf clone ... detonates (stopped) ... N orbs pushed"), PORTAL built 10 on frozen
    (6 arches + halos + gates) with halo fires, Dev Mode: 100 obstacle objects on Frozen, arch_1 made a bumper at 7, star
    bumpers at 8 (panel shot devobs_01_selected.png).
  * ZoogiCardSubsystem.cpp (null guard) was NOT in the ios15 upload - PC only for now; include it in the next iOS build.
  * Mac: compiled clean, EXPORT SUCCEEDED, upload ~3:35 PM (EXPORT_UPLOAD_RC=0), build 15. b15run.py chains ios15 + tail15 polling
    after the PC runs (arg 3 = ios).
  * Vefects probe on the Mac: VEFECTS_IN_UTOC=0 -> the shockwave Niagara is not in the iOS build (PC shows it).
  * Launcher note: Library > Launch sometimes ignores the first clicks after the editor was killed - click the launcher
    window, move onto Launch, click once, wait ~35 s; the project browser can open behind the launcher.


## Oct 29 evening - BUILD 17 (Claude): DUAL-MODE HOLOGRAPHIC SPIRE (build 16 was built but NOT uploaded - superseded)
  Evan (2 reference images): a central hologram spire on every classic arena. Image 1 SCOREBOARD by default / opponent turns,
  Image 2 AIM & LAUNCH (DRAG TO LAUNCH, animated hand, ball, arrow, floating Zoogi portraits) during the local player's own aim
  step, an animated morph between them, back to the scoreboard on launch. No left-side instruction HUD.
  * NEW ZoogiHoloSpire.h/.cpp + ZoogiHoloFontData.inl. UZoogiHoloSpireSys spawns AZoogiHoloSpire at the court centre (stands on
    the hub top, max 2.5 m up) once the stage has formed; every map except Training, every mode except Royale (classic + 2v2).
    Scale = court scale x [ZoogiRoll.HoloSpire] Scale (default 1.25); bSpire / -ZoogiNoSpire.
  * Parts: star emitter + rings on the floor, beam + sheath (FxMID), 5 rotating energy rings, chevrons rising, top orb with two
    orbit rings and an orbiting star (M_ZAFX_Add additive), curved drum (procedural arc) = scoreboard: ROUND n/N tab, one panel
    per Zoogi (portrait, live orb share % = OrbsScored / field orbs, name / OUT, bar, gold rim on the shooter); two side cards
    (orbs left bars / leader crown bars); aim panel. Front parts yaw to the camera.
  * Morph (0.9 s): 0-0.42 the drum collapses into the beam while spinning, side cards retract, orb sinks; then the drum re-forms
    as the ring of the other Zoogis' portraits and the AIM & LAUNCH panel builds out of the beam (scan-line reveal). Reverse on
    launch. Trigger: IsHumanAiming() && IsAimStep() (log "HOLOSPIRE -> AIM & LAUNCH / SCOREBOARD").
  * Rendering notes (hard-won): colour panels use M_FrozenDecal (unlit translucent, rgb x Tint, opacity = alpha) on canvas render
    targets cleared to transparent; solid shapes are drawn SE_BLEND_Opaque (writes alpha), light/glows/portraits/text are
    drawn SE_BLEND_Additive with vertex alpha 0 (adds colour, keeps alpha). SE_BLEND_AlphaHoldout zeroes colour - do not use.
    Slate fonts do NOT draw into canvas render targets: text uses a Lilita One glyph atlas embedded as PNG bytes
    (ZoogiHoloFontData.inl, generated from Content/Data/Fonts/LilitaOne-Regular.ttf at 80 px). Procedural quads must wind
    counter-clockwise as seen from the camera side (M_FrozenDecal is one-sided).
  * Test: -ZoogiSpireShots (+ -ZoogiShots) captures both modes and the morphs, and exports the render targets (rt_*.png) once.
    C:\dev\proofs\build16_oct29 / build17_oct29.
  * ios17.py: HoloSpire h/cpp, FontData.inl, ZoogiCardSubsystem.cpp (null guard from build 15). Probe: M_ZAFX_Add is in the iOS
    cook (ZAFX_ADD=1); M_FrozenDecal is under /Game/Env (always cooked).
  * Mac: build 17 EXPORT SUCCEEDED, upload ~4:52 PM.
  * BUILD 18 (same evening): the aim panel keeps itself inside the view - while it shows, its top is projected to the screen and
    the panel slides down the beam (min 4.2 m local) if the top is in the upper 10% (the Frozen Ring aim camera is close and low),
    back up to 7 m when there is room; the portrait ring stays under it. Aim panel z 700 (was 810), sheath fainter (0.12).
    PC: C:\dev\proofs\build18_oct29\spire_frozen (AIM & LAUNCH fully on screen). Mac: EXPORT SUCCEEDED, upload ~5:01 PM.

## Oct 29 evening - BUILD 19 (Claude): big COMBO badge back + cinematic aim drag follow
* Evan 5:35 PM "Bring back the larger stylize combo text that appeared when players build up combos": the big slanted COMBO +
  gold number (UZoogiArenaHUDWidget) was 2v2-only (team Resonance) and only on the landscape top bar. Now BuildComboBadge /
  TickComboBadge: classic shows the chain of scores in one shot from the 2nd (ZoogiScoringUI::ComboStreak), 2v2 keeps Resonance;
  the portrait board builds it too (left, under the profile row, y 372). Depth copy behind the number, overshoot pop on every
  step up, classic fades ~2.8 s after the last step. Centre callout now "COMBO xN  +bonus" (chain count matches the badge).
  Test: -ZoogiComboBadgeTest cycles 2/3/4.
* Evan 5:44 PM cinema cam: ECine::Main (cinematic aim step) for a human now follows Shooter->AimYaw like CAM mode at the same
  Main rear distance; as the drag pull grows (HumanDragFrac / 0.9, damped) the camera pushes in toward the pre-launch Battle pose
  (boom 680, pitch 24, FOV 72, 150 cm shoulder offset) and eases back out when the pull returns to zero. -ZoogiNoCineDrag = old
  fixed view. Test: -ZoogiCineDragTest (forces cinematic, scripted pull + arrow swing, cine_0..3 shots).
* Scripts: C:\dev\ios\b19run.py / go19.py / ios19.py / tail19.py. Proofs: C:\dev\proofs\build19_oct29.
* Verified on PC: C:\dev\proofs\build19_oct29\combo_land + combo_port (badge landscape + portrait board),
  C:\dev\proofs\build19c_oct29\cine_land + cine_port (rest 1150 -> half 1051 -> full 681 / FOV 72 -> back 1140; portrait
  2721 -> 1301 -> 2685; the camera follows the swung arrow). The cine test needs no -ZoogiClassicButtonsTest (its AIM PRESET
  launches at ~1.9 s); UpdateHumanTest now holds the shot while the cine test captures (test-only, not in the iOS upload).
* Mac: build 19 EXPORT SUCCEEDED, EXPORT_UPLOAD_RC=0 ~6:11 PM.

## Oct 29 night - BUILDS 20 + 21 (Claude): Frozen Ring fun pack, action cams, impact frames, fireworks, app icon
Evan (screenshot): remove the red bumpers and pillars (pillar orbs become power orbs on the court, not floating), two-layer igloo
blocks where the walls are that burst away with real physics, a big snowman on the raised hex platform that loses a limb per hit
then explodes, every old-style Zoogi in the stands with live animation + reactions, more action / side cameras while rolling,
Easy Impact Frames when a focus-line cut-in plays, more fireworks. Then: the new app icon (his ball-in-crown art).
* NEW ZoogiFrozenFun.h/.cpp:
  - AZoogiIglooWall (AZoogiArenaPiece subclass): each Ring 1 wall segment = 2 courses of snow blocks (Snow / SnowShade) with one
    collider per course. Any Zoogi contact over HitSpeedMin (or an orb > 520 cm/s, AZoogiOrb::OnSphereHit) blows the top course,
    the next contact the bottom one (collider off = the gap opens). Blocks become physics debris (block WorldStatic only, never
    touch Zoogis / orbs), shrink away after 3.6 s; ResetPiece rebuilds every match. -ZoogiNoIgloo.
  - AZoogiSnowman on the NE platform (PlatX/PlatY, faces the court): 330 cm, coal face + carrot + top hat + scarf + stick arms,
    idle sway / head look-around / waving arms. Hits: left arm, right arm, head + hat fly off (physics), 4th = explodes into 16
    snow chunks + bursts + fireworks + impact frame. -ZoogiNoSnowman.
  - AZoogiFrozenCrowd: 321 fans from the 7 old-style meshes (/Game/Zoogis/*) on the 6 bleachers (6 rows each), one ISM per mesh,
    transforms rewritten every frame: breathing, sway, watching the fastest ball/orb, hype bounce while rolling, cheer hops /
    spinners on scores, gasp (jolt back, shiver) on ring-outs / big hits / blasts, a stadium wave every 20-30 s; a broken stand
    section drops its fans. -ZoogiNoCrowd. NOTE: the old meshes' materials are glTF instances of the ENGINE plugin material
    /InterchangeAssets/gltf/M_Default - fixism.py set its "Used with Instanced Static Meshes" flag and saved it (inside
    C:\Program Files\Epic Games\UE_5.6\Engine\Plugins\...). Without it the fans render black.
  - UZoogiShowSys (UWorldSubsystem) + AZoogiShowTicker (the tickable-subsystem tick never ran in -game; an actor tick does):
    polls orbs out / ring-outs / combo streak / match start / game over -> crowd + fireworks (rockets with trails, spark-streak
    shells, crackle, confetti, Niagara bursts; ambient rockets every 7-12 s). ZoogiShow::Event / NoteImpact / ImpactFrame API.
  - ZoogiShow::ImpactFrame: PC = Vefects NS_Impact_Frame_Advanced_NN_Always (copied from ZoogiRollUnreal-fab, Demo/Maps
    excluded). MOBILE: those materials use SceneColor (SM5 only) and cook as Default Material on iOS, so feature level < SM5
    draws a canvas manga frame (black + white focus wedges -> white flash + black wedges -> fade, 0.26 s). -ZoogiMobileImpactFrame
    forces it on PC. Called from UZoogiAnimeSubsystem::Start for every cut-in except Miss / Infuse / Guard / Status.
* Frozen build (bFun, -ZoogiNoFrozenFun = old stage): no Resonance pillars / obelisks; PowerOrbSpots (4, Ember/Frost/Storm/Titan
  colours at HubR) -> GameMode::SpawnOrbs makes them AZoogiOrb::MakePower (scores like a star orb: x2 + a power charge).
  PlaceStarProps / ApplyLayout skip star bumpers on the Frozen Ring. SpawnPiece got an optional piece class.
* ACTION CAMERAS (PlayerController::UpdateCamera, Rolling, CINE + CAM modes, not TAC / royale; -ZoogiNoActionCam): from 0.55 s
  side tracking -> first impact (ZoogiShow::NoteImpact from OnShooterContact) side angle with push-in -> follow whatever got hit
  hardest -> wide low side angle over the stands -> slow orbit; hard cuts, side chosen so the sight line clears the centre spire.
  -ZoogiActionCamShots captures act_NN_<shot>.png.
* App icon: Build/IOS/Resources/Assets.xcassets/AppIcon.appiconset/Icon1024.png (Evan's art, rounded black corners cropped).
  fin.sh now compiles ~/zr/AppIcon.xcassets with actool (ICON_OK in the log), copies Assets.car + AppIcon PNGs into the .app and
  merges CFBundleIcons. ios20/ios21.py upload the catalog. (fin_pre20.sh = the old fin.sh.)
* iOS: build 20 = FULL content route (b20ios.py: BuildCookRun IOS -skipbuild -cook -iterate -stage -pak, ~2 min full cook with a
  warm DDC since the ini change invalidated the iterative cook; stage 866 MB tar over ssh in 87 s; then build3.sh). /Game/Vefects is
  now in DirectoriesToAlwaysCook (Easy Shockwaves now on the phone too; Demo / Maps never cook). Build 20 uploaded ~8:03 PM (it
  still had the Niagara impact frame = grey sprite on iOS + blob fireworks); build 21 = the fix (fast path, stage unchanged).
* Runner change: b20+/b21 runners reopen the editor at the end (UnrealEditor.exe <uproject>), so no more launcher dance.
  openue.bat / go20.bat exist in C:\dev\ios for Explorer double-clicks if ever needed.
* Proofs: C:\dev\proofs\build20_oct29, build20b_oct29 (show_*.png: snowman stages, igloo, crowd, fireworks, impact frame),
  build21_oct29 (spark fireworks + mobile impact frame).
* Mac: build 21 EXPORT SUCCEEDED, EXPORT_UPLOAD_RC=0 ~8:15 PM (ICON_OK).
* BUILD 21 FAILED ON LAUNCH: "Failed to open descriptor file ../../../ZoogiRoll/ZoogiRoll.uproject". Cause: BuildCookRun -iterate
  staged uecommandline.txt as -project=... -iterative (= loose-file deploy). Fixed on the Mac stage + PC stage (plain -project=...),
  b20ios.py now rewrites it after every cook. BUILD 22 = build 21 re-signed with the fixed stage (fix22.py -> fin.sh 22),
  EXPORT_UPLOAD_RC=0 ~8:39 PM.

## Oct 29 night - BUILD 23: action / fun / timing pack (Claude)
Evan: "Yes 1-10, for nine orbs are worth double" (my list of 11 ideas) + "after launch I liked the over head view ... more
different angles from previous distance set up not so much close ups".
* NEW Source/ZoogiRoll/{Public,Private}/ZoogiActionFun.{h,cpp}: UZoogiFunSys (UWorldSubsystem, ticked by AZoogiShowTicker,
  HUD via AHUD::OnHUDPostRender). Classic turn game only (not 2v2 / royale). -ZoogiNoActionFun = all off; -ZoogiNoTimingTap,
  -ZoogiNoBoosts, -ZoogiNoReplay; -ZoogiFunTest = auto taps + captures fun_*.png (boosts every shot, fire after 1 scoring turn).
  - #5 impact timing tap: predicts the shooter's first contact (2D time-to-contact vs every Zoogi/orb); 0.75 s before it a gold ring
    tightens on the target ("TAP!"). Tap within 0.075 s of contact = PERFECT (target knock x1.35, impact frame 8, crowd), 0.17 s =
    GREAT (x1.15). Velocity scaling is applied the frame after the hit (post-collision).
  - #6 parry: same machinery when a CPU shot is predicted to hit a human Zoogi (cyan ring, "BLOCK!"): BLOCKED keeps 35% of the knock,
    BRACED 60%.
  - #2 CPU fast-forward: a tap during a CPU turn = global time dilation 2x until the next human turn ("TAP TO SPEED UP" hint).
  - #7 boost gem: 45% of shots, 0.35 s after launch, 300-720 cm ahead of the shooter; first Zoogi within 115 cm gets HEAVY HIT
    (x1.3 knock on its next shot's first contact) or DOUBLE POINTS (orbs + KOs x2) for its next turn. Gone at Resolve.
  - #8 on fire: 3 scoring turns in a row -> fire ribbon (ArrowTrail Fire) + flame bursts + crowd hype + "ON FIRE" label, first
    contact x1.2; a non-scoring turn puts it out.
  - #9 final rounds: last two rounds (RoundNumber >= MenuRounds-1) every orb x2 (CheckKnockoffs), banner + fireworks, "ORBS x2" badge.
  - #10 round highlight replay: each shot is recorded at 30 Hz (all balls + orbs transforms/visibility); best shot of the round
    (points, KOs, perfect hit) replays at the round end (EndTurn waits; GameMode::Tick returns early while it plays) - impact-0.7 s
    .. +2.1 s at 0.5x, wide side camera ~2000 cm out, letterbox + REPLAY + "BEST OF THE ROUND", tap to skip; live state restored after.
* #1 early turn end (GameMode::Tick Rolling): after 1.4 s, when every body is < 130 cm/s and each one's stopping point
  (v^2/2a + r) is on the court for 0.3 s -> Resolve. CPU plans after the drift settles (bCPUPlanDeferred, 1.2 s max).
  SettleTime 0.8->0.45, ResolveMin 0.7->0.45, EndBeat 0.35->0.2. -ZoogiNoEarlySettle = old behaviour.
* #2 CPU beats: DrawCPU 0.35, AwakenCPU 0.45, CPULockAt 0.15, CPUMain 0.55, CPU power-up 0.3 s tactical / 0.65 s cine (-ZoogiOldPace).
* #3 CINE human launch 0.2 s after release (PowerUpHold; was the 1.35-1.9 s swing).
* #4 anime cut-ins: full card only the first time each event type happens per match, repeats 0.4 s (-ZoogiCutInAlways).
* Action cams rewritten to Evan's note: every shot at the Follow profile distance (2400 cm / 55 deg) - overhead (behind the roll),
  high side, impact three-quarter, follow-hit high side, near top-down turning, wide orbit. No low chase / close-ups.
* Hooks: GameMode.h (friend UZoogiFunSys, constants, 2 flags), GameMode.cpp (BeginTurn/EndTurn/Tick/CheckKnockoffs), Targeting.cpp
  (BattleDuration, PowerUpHold, SetStage Resolve), AnimeSubsystem, Ball.cpp (OnSphereHit), PlayerController.cpp (tap + action cams),
  FrozenFun.cpp (ticker). Backups of the pre-23 files: C:\dev\ios\bak23\.
* Proofs: C:\dev\proofs\build23_oct29 (fun_frozen, fun_meadow, act_port, fun_four).
* iOS: fast path (ios23.py -> build3.sh 23, stage from build 22).
* Build 23: EXPORT_UPLOAD_RC=0 ~9:43 PM.
* BUILD 24 (Evan 9:39 PM 'remove the transparent effect' -> picked 'see-through walls/stands'): ZoogiOcclusionFade CVar
  zoogi.Cam.OcclusionFade default 1 -> 0 (walls / stands / pillars no longer ghost between the camera and the Zoogi).
  ios24run.py waits for build 23, uploads the file, build3.sh 24. Backup of the old file in C:\dev\ios\bak23\.
  PC editor binaries not rebuilt for this one-line change yet (next editor build picks it up).

## Oct 29 late - BUILD 25: super power pack (Claude)
Evan: "From a super power standpoint what can bring this game over the top" -> my 8 ideas -> "Yes all".
Classic keeps Wolf Pack as everyone's regular SUPERPOWER (Oct 28 rule); the new ULTIMATE is each Zoogi's original signature power
(roster PowerKind via ZoogiUtil::FindZoogi, since ApplyWolfPackOnly overwrites Def.PowerKind).
* NEW Private/ZoogiSuperFun.cpp (more UZoogiFunSys members; header ZoogiActionFun.h). Classic only.
  #1 SUPER meter 0-100 per player: timing PERFECT +30 / GREAT +12, BLOCK +30 / BRACE +12, scoring 0.4 x points, KO +35, getting
     knocked off +15, clash / fusion +10. Full -> "ULTIMATE READY" + meter bar under the human's portrait. TryPower (SUPERPOWER
     button) and ClassicSuperpowerReady are hooked: full meter + rolling + Zoogi on the court -> ultimate (costs the meter, not a charge).
     Ultimates: Blue/Lars CHAIN RICOCHET (5 bounces), Tavi/Wolfgang WOLF STAMPEDE (6 clones), Vex/Hotstreak METEOR STRIKE (r720 blast),
     Moss/Pinpoint HOMING STRIKE (steers, x1.5 hit), Luma/Bolt THUNDER CHAIN (stun + knock every rival, throws power orbs),
     Master Orin/Wraps MUMMY WRAP, Sera RUNE CAGE, Dark Zoogy/Nightshade BLACKOUT (screen dark, rivals frozen + skip).
     Slanted ultimate banner + impact frame 9 + fireworks (no anime card: Evan's no-pause rule would play a 5 s card over the roll).
  #2 ultimates fire the moment the button is pressed mid-roll; Chain Ricochet / Homing Strike drop into bullet time (dilation 0.15,
     1.6 s) - tap the target (auto-pick on timeout). CPUs fire into crowds.
  #3 POWER CLASH: powered shooter (ultimate / Wolf Pack / on fire / awakened) hits a rival with a charge or >= 50 meter, a human on
     either side -> dilation 0.04 for 1.5 s, tap-off bars (CPU 6-9 taps/s). Win = knock x1.6 (OVERPOWERED), lose = defender keeps 30%
     and the shooter is thrown back (COUNTERED). Defender pays a charge (or 50 meter). -ZoogiNoClash.
  #4 Meteor Strike / Thunder Chain break igloo walls (both courses) and knock the snowman apart; Thunder throws the power orbs.
     Only AZoogiIglooWall / AZoogiSnowman are touched (never floor / stands).
  #5 AWAKENED: on fire + full meter at turn start -> mass x1.5, model x1.18, gold aura, first hit x1.25; ends with the turn.
  #6 FUSION: an ultimate hitting a rival Zoogi -> element burst named from both (PLASMA = fire+lightning, WILDFIRE, RAILGUN ...).
  #7 ARENA EVENTS at the start of rounds 2, 4, 6...: METEOR SHOWER (meteors push bodies, first 4.5 s of each roll), LOW GRAVITY
     (WorldGravityZ x0.42, restored), WHIRLWIND (tangential push on rolling bodies), POWER SURGE (+1 charge, +40 meter). -ZoogiNoArenaEvents.
  #8 ULTIMATE FINISH: a KO during an ultimate replays that turn immediately (EndTurn: bRoundEnd || WantsFinisher -> StartReplay(G, bRoundEnd)).
* Also in 25: callouts one at a time (newest replaces), see-through occlusion fade off (from build 24).
* Proofs: C:\dev\proofs\build25_oct29 (sup_frozen / sup_four / sup_port: fun_ult_*, fun_clash*, fun_fusion, fun_awakened, fun_event_*).
* -ZoogiFunTest now also starts every meter full and auto-fires the human ultimate / taps the clash.
* Build 25: EXPORT_UPLOAD_RC=0 ~11:28 PM (fast path ios25.py, stage from build 22).

## Oct 30 ~1 AM - Tripo ultimate forms rigged + animated (Claude)
* Evan's 4 ultimate forms in Tripo Studio (red fire armor, 2 white-gold foxes, blue ice crystal; ~1.9M faces each) -> Retopo 20k tris
  triangles (35 credits each) -> Auto Rig humanoid / Mixamo skeleton (20 each) -> 11 free presets each: idle, run, box_01, box_02,
  front_kick_01, slash, cast_a_spell, hit_to_body_01, fall, cheer, jump -> FBX (Mixamo flavour, 4k PNGs, all animations).
  220 credits used (24,765 -> 24,545).
* Files: C:\dev\zoogi_ultimates\ult_{ember_red,fox_gold_A,fox_gold_B,frost_blue}.zip + extracted folders (originals also in Downloads).
* Imported (C:\dev\ios\import_ult.py, Interchange defaults): /Game/Zoogis/Ultimates/<name>/ = skeletal mesh + skeleton + physics asset +
  material + 5 textures (max size capped 1024) + 11 AnimSequences each. Verified: kick plays in the animation editor (18,933 tris, ~1.2 m tall).
* Not yet used in gameplay - design discussion with Evan pending.

## Oct 30 ~1:30 AM - BUILD 26: ULTIMATE STRIKE (Claude)
Evan: keep rolling (Marble Clash core) but "transform can happen right before the hit"; automatic move selection.
* NEW Private/ZoogiStrikeFun.cpp (UZoogiFunSys members): ~0.35 s before a predicted contact (the timing-ring prediction) the ball
  bursts into its Tripo ultimate form (USkeletalMeshComponent on a temp actor riding the ball, feet at the ball's bottom, facing
  the target, scale 1.45), plays one move timed so the strike frame meets the contact (clip start skips windup / rate stretches),
  then curls back. The ball's visuals hide meanwhile (by visible FLAG - Zoogi Blue's morph mesh re-syncs to Visual's flag); the
  collision sphere keeps doing all the physics -> no gameplay change from the visual.
  Triggers: ultimate active this shot / Awakened + timing tap (CPU auto) / power-clash winner; a KO by a striking Zoogi -> the
  form stands and loops "cheer" for 1.8 s.
  Moves (auto): heavy (ultimate / awakened / >1300 cm/s) = signature (ember front_kick_01, foxes slash, frost box_02); glancing
  (>22 deg) = slash; 850-1300 = box_02; else box_01. Hit times in ZoogiStrikeData::HitTime.
  Forms: Vex/Hotstreak ember_red, Tavi/Wolfgang fox_gold_A, Sera fox_gold_B, Blue/Luma frost_blue (others: no transform).
  Assets preloaded at match start (the first on-demand load stalled while the skinned mesh got ready).
  -ZoogiNoStrike, -ZoogiStrikeTest (every predicted hit + a side test camera + fun_strike_* captures), -ZoogiStrikeYaw/-ZoogiStrikeScale.
* iOS: new cooked content -> full route b26ios.py (cook -iterate + stage + uecommandline rewrite + stage upload + ios26 + build3.sh 26).
* Build 26: EXPORT_UPLOAD_RC=0 ~1:55 AM (cook 64 s, stage upload 69 s). Proofs: C:\dev\proofs\build26_oct30 (strike_meadow / strike_frozen).
* Open: the white-gold fox form looked darker than in Tripo in the test shots (material / metallic map?) - check on the phone.

## Oct 30 ~06:45 - Build 27: POWER CLASH fist fight, AI transforms, launch without aiming, REACTION = tap button, clash on the scoreboard (Claude)
Evan's request (with the fire-vs-lightning fist clash art + the ULTIMATE STRIKE / POWER CLASH panel mockup).
- Power clash is now a watchable fight (ZoogiSuperFun.cpp Start/Tick/Finish/EndClash, ZoogiStrikeFun.cpp SpawnFighter/PlayFighter/TickFighters/EndFighters):
  both Zoogis burst into their Tripo ultimate forms, square up at the contact point and trade box_01/box_02 punches for 3.2 s real
  (world at UZoogiFunSys::ClashDilation 0.01, fighters CustomTimeDilation 100). Tug ratio slides the fight toward the weaker tapper.
  Then 1.5 s finish: winner throws its signature move (front kick / slash / box_02), loser hit_to_body_01 then fall; roll resumes with
  the old knock rules (x1.6, or x0.3 + shooter thrown back). Medium side camera (FOV 55) picks the open side (traces), goes high when
  both sides are walled in. Panel (DrawClash in ZoogiActionFun.cpp): split energy in each form's colour + crackling seam, fist burst,
  ULTIMATE STRIKE / POWER CLASH title, name plates with punch counts, tug bar with TAP/CPU icons at each end, "TAP FAST! PUNCH FOR PUNCH...".
- AI: CPU vs CPU clashes allowed (both transform). CPU strike roll per shot (on fire always, else 50% hard hits / 25%). Human on fire +
  timing tap also transforms. Zoogis without their own form borrow one (Dark Zoogy -> ember, Orin/Bolt/Wraps -> fox A, Moss/Nightshade/
  Pinpoint -> fox B, Lars -> frost) - FormIndex in ZoogiStrikeFun.cpp; replace when Evan makes their own forms.
- Launch without aiming (AZoogiGameMode::EnsureArrowAim, ZoogiTargeting.cpp): AUTO/CONTROL/DRIVE/LAUNCH with no aim fire along the
  arrow (pad: pad landing point); HumanDrag with no aim aims along the arrow; grabbing your Zoogi / the arrow and pulling BACK becomes the
  slingshot right away (ZoogiPlayerController.cpp arrow gesture); a press that finds nothing to aim at arms the slingshot along the arrow.
- REACTION button (ZoogiClassicPad.cpp): only visible when needed - clash taps (label TAP!), timing-tap ring (TAP!), parry ring (BLOCK!),
  or the hop while your shot rolls; shows on CPU turns too when you must tap. Presses go to UZoogiFunSys::OnTap first (TapButtonMode).
  SUPERPOWER stays on your own turn only.
- Scoreboard: clash winner +50 (ledger -> score at Resolve), feed lines at start + result, popup; live "CLASH!" on the two fighters'
  score units (ArenaHUDWidget), "POWER CLASH!" banner on the holo spire scoreboard, classic HUD rows show "POWER CLASH!" + "CLASH xN".
- Test flags: -ZoogiClashTest (every shot clashes with the nearest rival 2.2 s into the roll, captures fun_clashN_*), -ZoogiNoAimTest,
  -ZoogiClashScale= / -ZoogiClashGap= / -ZoogiClashCam= tuning. Scripts: C:\dev\ios\b27run.py, go27.py, ios27.py (code-only upload), tail27.py.
  Pre-edit copies: C:\dev\ios\bak27. Proofs: C:\dev\proofs\build27_oct30.
* Build 27: EXPORT_UPLOAD_RC=0 ~6:48 AM (fast path ios27.py, stage from build 26). Proof composite: C:\dev\proofs\build27_oct30\build27_power_clash.jpg.
  Verified on PC: human-vs-CPU clashes (both sides), CPU-vs-CPU clash logged, no-aim DRIVE launch logged. Not verified on a phone yet.
  Open: CPU-vs-CPU proof frame was camera-in-geometry before the HIGH-camera fallback (fallback added + seen working on a walled-in clash);
  frozen world FX (fire / ult sparks) hang in the air during the fight (world at 0.01 speed) - looks like a freeze-frame, left as is.

## Oct 30 ~09:40 - Build 28: stylized clash panel + ultimate avatars, field starts (ship only on Cosmic), instant pullback, opening animation, loser sent flying in form (Claude)
- Ultimate avatars (Evan's 4 face PNGs) -> Content/Data/MenuArt/ult_avi_{frost,fox,ember,lightning}.png (512 px, alpha; loaded at runtime
  like the other portraits, Data is UFS -> in the pak, so this build used the full cook route b28ios.py). Mapping (UZoogiFunSys::AvatarIndex):
  form 0 ember -> ember face, fox A/B -> white-gold fox face, frost -> frost face, Bolt -> yellow lightning face. Shown in the clash banners and
  swapped into the top HUD portrait (ArenaHUDWidget FUnitRefs.Avi) while that Zoogi is transformed (clash fighter / ultimate strike / flung).
  The editor keeps offering to import the 4 PNGs ("4 changes to source content files") - answer Don't Import (they must stay raw files).
- Clash panel restyled (DrawClash): Lilita One via a runtime UFont wrapper (FunTextL - canvas can't draw the bare composite font), slanted
  title plate, slanted fighter banners with the avatar medallion + name + comic "xN PUNCHES" counter that pops per tap, slanted energy meter
  with gold rim, gloss, stripes racing into a sparking VS star at the tug point, glossy TAP!/CPU buttons with tap ripples.
- Ship launch pad only on Cosmic (AZoogiArena::PlaceLaunchPad); every other map starts on the field (bLaunchFlow needs a pad).
  -ZoogiShipAllMaps restores the pad everywhere.
- Instant pullback: a press on open floor with no aim arms the slingshot along the arrow (tap without moving = old tap-to-aim).
- Opening stage-forming: AZoogiArena::AddLateForming adds star bumpers, obstacles not yet registered, the Frozen crowd and the Cosmic ship pad
  (drops in) to the forming animation; UZoogiStageForming::Contains avoids duplicates.
- Clash loser: launched away from the winner (1050-1400 cm/s + 480 up, LastHitPlayer = winner) and flies in its ultimate form
  (StartFlung/TickFlung/EndFlung in ZoogiStrikeFun.cpp: tumbles, looks back at the hit, stands up as it slows, ball shown again when it
  settles / 3.5 s; clash camera follows it for 1.6 s). Replaces the old x1.6 / -0.6 knock rules for the loser.
- Proofs: C:\dev\proofs\build28_oct30 (clash_meadow, opening_arabian, cosmic_ship). Scripts b28run.py, go28.py, go28ios.py, b28ios.py, ios28.py, tail28.py. Pre-edit copies C:\dev\ios\bak28.
* Build 28: EXPORT_UPLOAD_RC=0 ~9:39 AM (full route: cook 43 s, stage upload 68 s). Not verified on a phone yet.

## Oct 30 ~10:35 - Build 29: clash glare fix, cut-ins show the ultimate form, Persona 5 Royal style REACTION button (Claude)
- IMPORTANT: build 28's ios28.py upload list missed ZoogiArenaLaunchPad / LaunchFlowGame / DropIntroGame / ArenaProps / ZoogiArena.h /
  StageForming.h / GameMode.cpp, so build 28 on the phone still had the ship pad + old opening. ios29.py now uploads every source file
  changed since the build 26 upload (mtime > ios26_out.txt) - build 29 carries all of builds 27-29.
- Glare (Evan's phone shot, build 27): bursts frozen at full brightness under the 0.01 world speed. UZoogiFunSys::HideFxAround
  (ZoogiStrikeFun.cpp) at clash start / cut-in start: destroys AZoogiFX bursts, clears + mutes ShowSys spark balls (UZoogiShowSys::
  ClearSparks / bQuiet), hides Niagara within 32 m and glow parts of non-solid actors near the fight; RestoreHiddenFx at the end.
  The fighters' balls also hide their Sphere (a guard / boost bubble drew on it). Fight camera: bloom 0.25, no lens flares.
  Smaller, less white fist burst; softer split wash; the ultimate banner never draws over the clash panel.
- Forms get a fill point light (AddFormLight, 45 cd, no shadows) - they read dark under arena lighting (also the "fox looks darker" note).
- Anime cut-ins: the focused Zoogi comes out of its ball in its ultimate form for the whole cut-in (KO -> cheer, guard -> box_01,
  miss -> hit_to_body_01, others -> signature move), facing the camera; FX around it hidden while it plays.
- REACTION button (ZoogiClassicPad.cpp) restyled after Persona 5 Royal: rotating red starburst with black offset shadow (BurstTex, a
  rasterised texture - Slate custom-vert fans dropped triangles), white-rimmed black disc, red-shadowed white icon, ransom-note label
  beside the button (REACT / TAP! / BLOCK!, alternating black/white tilted letter boxes with red shadow); grows + pulses on tap moments.
  Still the single reaction button for clash taps, timing taps, blocks and the hop; screen taps keep working.
- Proofs: C:\dev\proofs\build29_oct30. Scripts b29run.py, go29.py, go29ios.py, ios29.py, tail29.py. Pre-edit copies C:\dev\ios\bak29.
* Build 29: EXPORT_UPLOAD_RC=0 ~10:35 AM (fast path, all files changed since build 26). Not verified on a phone yet.

## Oct 30 ~11:20 - P5-style branch started (Claude): C:\dev\ZoogiRollUnreal-p5style on feat/p5-style
- Evan's brief: Persona 5 Royal-STYLE menus / transitions / cut-ins / results for Zoogi Roll (style only, own art). Work in a fresh copy
  of feat/battle-royale, camera rules untouched. Evan picked "branch + current work": git worktree add -b feat/p5-style from
  feat/battle-royale (feat/battle-royale untouched, nothing committed or pushed), then today's uncommitted work from
  ZoogiRollUnreal-combined copied in (C:\dev\ios\p5_worktree.py) + the ignored Content/Vefects and Content/AuraModelGen.
- Fonts (Evan: free lookalike): SIL OFL Anton / Archivo Black / Abril Fatface / Bangers in Content/Data/Fonts (+ OFL_*.txt).
- Step 1 (for review): ZoogiP5Style.h/.cpp - UZoogiP5MenuWidget (Slate) MODE SELECT / MAP SELECT: black + hot-magenta accent, rotating
  rays, halftone, sliding stripes, ransom-note title slamming in, tilted list bars whipping in (selected = white bar + jittering colour
  slab), art in a torn-edge tilted frame with a GO! burst, caption tag, BACK / coins / gems tags; tap = select, tap again (or the art)
  = go. ZoogiP5::PlayWipe = angled shard wipe (~0.7 s, "LET'S ROLL!", Dash + Clack). -ZoogiClassicMenu = old menu, -ZoogiP5MenuTest =
  scripted capture. Hook: UZoogiMenuRoot::BuildP5Menu (ZoogiModeSelect.cpp).
- Evan's in-match notes, done in this branch only: SUPER meter moved below the avatars; status / instruction notes are now P5-style
  angled panels stacked lower left (slam in); ARC button right of LAUNCH (bArcShot, one shot): 52 deg lob to the aim point capped at
  ArcRange() (0.55 x knockoff radius, 9-16 m), magenta range ring on the ground while armed. -ZoogiArcTest for captures.
- Runner: C:\dev\ios\p5run.py (+ gop5.py all runs, gop5m.py menus, gop5b.py build only). Proofs + review videos: C:\dev\proofs\p5_style.
- Next (after Evan approves the menu): Blue crit cut-in, then the results screen.

## Oct 30 ~11:50 - new ult avatars + P5 step 2 (Claude)
- Evan sent 4 new ultimate-form faces: resized to 512 RGBA and replaced Content/Data/MenuArt/ult_avi_{lightning,ember,fox,frost}.png
  in BOTH ZoogiRollUnreal-combined and -p5style (same file names, so no code change). Not on TestFlight yet (needs the next upload).
- Meter (p5 branch): the SUPER meter now slides down under the avatar details while they are revealed (tap an avatar) and back up
  when they minimise: ZoogiArenaHUD::DetailsReveal(OutBottomPx) from the HUD widget's TickSplit, read in ZoogiActionFun.cpp.
- Menu approved by Evan ("menu looks good"). Step 2 = Blue's critical cut-in: ZoogiP5::PlayCutIn (ZoogiP5Style.cpp, Slate overlay,
  1.1 s, repeats 0.6 s): scene darkens behind a black slab, slanted band slams in (white rim, magenta offset strip, frost colour,
  halftone, racing speed lines), Blue's eyes close-up in a slanted window (menu portrait blue.jpg; the frost ult face when transformed),
  ransom name tag + "CRITICAL!" on a jagged burst, Dash + Clack sounds; shoots off left. UZoogiAnimeSubsystem::Start uses it for
  Critical events of Blue (ZoogiP5::CutInFor; -ZoogiP5CutInAll = everyone), the old clip / card is skipped for those. No world pause
  (Evan's Oct 24 "never pause" rule stays). -ZoogiP5CritTest = Blue crit 14 s into a match, frames p5crit_NN.
- Crit cut-in verified in PC captures (C:\dev\proofs\p5_style\crit_land, crit_port; review videos p5_crit_blue_land.mp4 / _port.mp4).
  Fixes after the first capture: AHUD::OnHUDPostRender draws on the debug canvas, which sits ABOVE viewport widgets, so
  UZoogiFunSys::DrawHud and the comic-slice overlay skip while ZoogiP5::CutInBusy(); impact text moved under the band (burst behind
  it) so the eyes stay clear; the close-up is always the ultimate face (matches the 3D form during cut-ins). Wide screens: text left,
  face right. Runner runs added: crit_land / crit_port (-ZoogiP5CritTest, 65 s - the game can take ~30 s to load after a rebuild).
  match_frozen no longer carries -ZoogiP5CritTest (its HUD test opens settings = pause at ~17 s).
- Meter slide verified: ahud_l_07_top_peek (meter under the opened details) / ahud_l_08_top_peek_after (back under the avatars).
- Next: Evan's review of the cut-in, then step 3 (results screen). The new avatars and all P5 work are not on TestFlight.

## Oct 30 ~12:45 - P5 step 3: RESULTS screen (Claude, feat/p5-style only)
- Evan reviewed the crit cut-in and said "move to phase 3". He re-sent the 4 ult avatars: pixel-identical to the ones already in
  Content/Data/MenuArt (no change needed).
- UZoogiP5ResultsWidget / SZoogiP5Results (ZoogiP5Style.h/.cpp), hooked in UZoogiArenaHUDWidget::BuildResults (kept in P5Results
  across HUD rebuilds; reset when the phase leaves GameOver). Game over plays a wipe ("FINISH!" / "GAME OVER" / "TIME!"), then:
  winner-colour rays + halftone, slanted black standings slab (magenta edge), ransom title VICTORY! / DEFEAT / DRAW! + white
  "<NAME> WINS!" tag, the winner's ultimate face (menu portrait in a torn frame if it has none) slamming in over bursts with a hard
  shadow + ransom name + P/team tag, standings bars whipping in one beat apart with count-up scores (winner bar white + jittering
  slab + WIN! sticker, YOU tag), YOUR MATCH stat tags (orbs / stars / KOs / best combo / perfects), REMATCH / CHANGE ZOOGI / MENU
  angled tags (each press = wipe with its own shout, then MenuAction). Tap during the entrance = skip to the end.
  ZoogiP5::ResultsOn() keeps FunSys DrawHud + the comic-slice overlay off while it shows. -ZoogiClassicMenu = old stone panel.
- -ZoogiP5ResultsTest: ends the match ~20 s in with test scores (P1 top; -ZoogiP5ResultsLose = P3 top), frames p5res_fNNN,
  auto-quit 9 s after game over. Runner runs results_land / results_port (gop5r.py). Patch sources in C:\dev\ios\p5patch.
- Verified in PC captures (results_land = VICTORY as Blue, results_port = DEFEAT, Vex wins): review videos
  C:\dev\proofs\p5_style\p5_results_land.mp4 / p5_results_port.mp4. Fixes after the first capture: title moved down (was clipped),
  shorter stat line under each name, the match HUD's canvas children are collapsed under the results (the ROUND line showed
  through), AZoogiHUD's arena path + the DEV button stay off while ZoogiP5::ResultsOn(). Portrait buttons sit right under the stat
  tags on tall phones. Waiting for Evan's review. Nothing from the P5 branch is on TestFlight.

## Oct 30 ~13:40 - TestFlight build 30 = the P5 branch (Claude; Evan: "Test flight")
- C:\dev\ios\b30ios.py (go30.py): seeds ZoogiRollUnreal-p5style\Saved\Cooked\IOS + Binaries\IOS receipt from the combined copy,
  iterative IOS cook + stage + pak of the P5 copy (new Content/Data/Fonts are in the pak), stage -> Mac ~/zr/stage, then uploads
  every P5 source file that differs from the combined copy into the Mac mirror (~/UE5/Builds/EVAN/.../ZoogiRollUnreal-combined),
  build3.sh 30 + poll (status b30ios_status.txt, tail30.txt).
- !! The Mac mirror now holds the P5 sources (list: C:\dev\ios\ios30_files.txt; ZoogiP5Style.h/.cpp are new there). Before any
  future TestFlight build from ZoogiRollUnreal-combined, re-upload those files from the combined copy (and delete the two P5 files
  on the Mac) - or keep building from the P5 copy. The combined copy and feat/battle-royale were not touched.
- First try stopped at staging (no Binaries\IOS receipt in the P5 copy) and the runner exited before reopening the editor; the
  editor was reopened from the Unreal project browser (it opens on the EK240Y monitor).
- !! Build 30 (uploaded, EXPORT_UPLOAD_RC=0) does NOT have the P5 code: b30ios.py quoted the Mac path ("~/..." in double quotes),
  so the sources went to a literal ~/~ folder and UBT rebuilt nothing (binary still 10:33 = build 29 code, with the P5 stage).
  Build 31 = ios31.py (go31.py): removed ~/~, uploaded the 18 files to $HOME/... (verified with ls), build3.sh 31.
  The Mac mirror now really holds the P5 sources (list ios31_files.txt).
- Build 31 compiled (UBT_RC=0, new binary 13:50) but export failed: the Mac's disk was full (101 MB free) - 58 GB of Xcode export
  leftovers in $TMPDIR (DTAppAnalyzerExtractorOutput-*.zip, XcodeDistPipeline.*, *.xcdistributionlogs from every upload).
  fin31.py deletes those and re-runs ~/zr/fin.sh 31 (no rebuild). Consider adding that cleanup to fin.sh.
- Build 31 uploaded 14:05 (EXPORT_UPLOAD_RC=0) after the temp cleanup: P5 code (binary 13:50) + P5 stage (fonts). This is the one
  to test. Not verified on a phone yet. Before the next build from ZoogiRollUnreal-combined: re-upload ios31_files.txt files from
  the combined copy to the Mac and delete Source/ZoogiRoll/{Public,Private}/ZoogiP5Style.* there; re-cook/upload the combined stage.

## Oct 30 ~14:30 - Frozen outer ring: ways back in (Claude, feat/p5-style; Evan agreed to all 3 + jump ends the turn)
- AZoogiGameMode::InOuterRing(B): Frozen + Ring 2 + the Zoogi outside Ring 1 (r > knock-off + 40; bridge-entry roam excluded).
- JUMP IN: at a turn start out there, humans get ARC auto-armed and the ARC button reads JUMP IN / JUMP IN! (magenta); CPUs always
  jump. FireShot: 72 deg lob over the Ring 1 wall to JumpInLanding() (the aim / lock point if it is on the court, else where the
  aim line reaches 60 % of the court radius, straight in when aiming along/away). It is that turn's shot. Landing cuts the slide
  to 40 % (JumpBody) so the ice doesn't carry it across. P5 note lower left: "OUTER RING - JUMP IN ARMED ...".
- Safety net: OuterTurns per Zoogi; still out at the start of a 3rd turn -> AutoJumpIn(): set on the nearest bridge launch point,
  catapulted in (same side, half way), costs the turn ("AUTO LAUNCH!").
- HALO PADS: glowing floor pads under the 4 Ring 2 halos (yaw 0/90/180/270, r1905); rolling over one (95 cm) launches that
  Zoogi back onto the court (2 s cooldown per Zoogi). TickHaloPads().
- -ZoogiOuterTest: P1/P2 start their first turn on the outer ring, P3 with two turns behind it (auto launch), P4 rolls into the
  yaw-90 pad. Runner run outer_frozen (gop5o.py). First capture: jump-ins work (P1 combo x12, P2 CPU); auto launch aimed at the
  centre and slid off the far side -> fixed (same side + 72 deg + landing cut).
- More fixes: CPU jump-ins were bent / flattened by card fantasy effects (precision homing, BOULDER up-clamp) -> skipped while
  JumpBody is in the air (ZoogiCardFantasy.cpp); card speed-ups after FireShot are undone for a jump (JumpV3 re-applied in
  ReleaseAim); every jump checks its arc against the arena (ZJumpPathClear) and fans to another landing if blocked; halo pads
  moved to r2000 (just outside the hoop), the Zoogi is snapped onto the pad and launched at 66 deg so it threads the halo
  (the game's own halo-pass cut-in plays). Auto launch no longer teleports to the bridge (the lob clipped the arch) - it jumps
  from where it stands. PC capture: P1 (human), P2 (CPU), P3 (auto) and P4 (halo pad) all landed on the court.
- TestFlight build 32 uploaded 15:21 (EXPORT_UPLOAD_RC=0): build 31 + the outer-ring ways back. ios32.py also clears the Mac's
  Xcode temp files first. (Its poll loop died on an ssh timeout; chk32.py confirmed the upload.)

## Oct 30 ~16:30 - Evan's answers + clash / superpower / REACTION changes (Claude, feat/p5-style, PC-built, not on TestFlight yet)
- Evan: "Free hop gone", clash win rate "65%", superpower moment = the bullet-time TAP A TARGET.
- REACTION: ClassicPad ReactionNeeded() = tap moments only (clash, timing tap / block); ClassicReactionReady() false unless
  -ZoogiFreeHop.
- Power clash: READY... 0.8 s (taps don't count), then tug-of-war Clash.Bar (+ = attacker): 0.05 per punch, x1.6 when behind
  by >15 %, x2 in the last second (FINAL PUSH); CPU 3.8-5.8 taps/s (was 6-9), eases to 70 % when ahead; winner = bar side;
  "COMEBACK!" callout when the winner threw fewer punches. Monte Carlo (human ~6 +- 1.3 taps/s, 0.3 s reaction): ~64-65 %.
  ClashFightSecs 3.8 (incl. READY), ClashReadySecs 0.8.
- Superpower aim (Chain Ricochet / Homing Strike): world at 0.05 speed, 5 s the first time each player uses it in a match
  (3 s after), P5 card with the power name + what it does, "TAKE YOUR TIME - TAP A GLOWING TARGET!".
- Verified in capture run clash_ult (C:\dev\proofs\p5_style\clash_ult): READY..., FINAL PUSH x2, the aim card.
- Relic models exported from Evan's Tripo account (FBX, 4K textures, ~2M triangles each - must be decimated before use):
  C:\dev\art\relics\{prism_grenade,crystal_blade,halo_ring,scepter}_fbx.zip.
- Story Mode: ChatGPT's "Complete Claude Build Package" filled in with the real state of the game as a Claude doc
  ("Zoogi Roll - Story Mode Build Package (filled in)"): built-systems inventory, corrections, 29-row traceability matrix,
  architecture, vertical slice (Prologue: The Frozen Ring), backlog, open questions. No Story code yet.

## Oct 30 evening (Claude) — STORY MODE (feat/p5-style copy, C:\dev\ZoogiRollUnreal-p5style; nothing pushed)
Evan: "complete autonomy for story mode ... more than a ring battle, create environments. Tap to travel. Fixed party. Cards on. Item 7 your call" -> built in the p5style copy.
* Story: "THE RINGS WERE NEVER A GAME" — 4 episodes / 12 stops / 12 battles / 21 scenes / 149 lines. Dark Zoogy = the First Champion, sealed under the Rings by the founders (Orin among them); he steals the five cores; Vex's mother, Tavi possessed and freed with Sera's Rune Seal, Orin's confession; finale: Blue breaks the seal instead of re-caging him ("THE END ... FOR NOW").
* Data (edit freely, '|' CSV): Content/Data/Story/Chapters.csv, Spots.csv (stop -> 3D arena env, scene, mission), Missions.csv (mode classic/combo/royale, fixed party, CPU tier, rounds, win scene, fail line, reward), Dialogue.csv (who/line/fx/sfx cues).
* Code: Public/ZoogiStory.h + Private/ZoogiStoryMode.inl (included at the end of ZoogiP5Style.cpp). Screens: HUB, episode CARD, MAP (tap a stop -> Blue's token hops there -> wipe -> the live 3D arena behind the UI becomes that place -> ROLL OUT), DIALOGUE (two-shot faces, typewriter with per-character voice blips, flash/shake/blackout/red/cold/crack fx, SKIP), VS, MISSION CLEAR/FAILED (RETRY / MAP), EPISODE CLEAR. Progress: Saved/SaveGames/ZoogiStory.txt.
* Hooks: ZoogiHUD.cpp (BlocksHud: no menu/canvas under story screens), ZoogiArenaHUDWidget::BuildResults (story shows its own results), ZoogiModeSelect STORY card -> ZoogiStory::Open (-ZoogiStoryAnime = old anime clip), ZoogiCPUBrain ActiveTier (mission tier), ZoogiGameMode tick (-ZoogiStoryTest finish).
* Music: arena themes are wired but stay silent (music is off per Evan's Oct 27 "SFX only"); sound design = SFX + announcer VO (arena intro on VS, Victory/Defeat).
* Proofs: C:\dev\proofs\p5_style\story_land (win path) + story_port (-ZoogiStoryLose); sheets story_land_sheet*.jpg, story_port_sheet.jpg, story_sheet3.jpg. Flags: -ZoogiStoryTest, -ZoogiStoryLose, -ZoogiStoryAll.
* TestFlight build 34: C:\dev\ios\b34ios.py (cook+stage p5style -> Mac stage, sources, build3.sh 34). Status: C:\dev\ios\b34ios_status.txt.
* Build 34 UPLOADED to TestFlight 17:54 (UBT_RC=0, EXPORT_UPLOAD_RC=0; Dialogue/Missions CSVs confirmed in zoogiroll-ios.pak). Not yet played on a phone.

## Oct 30 ~18:20 (Claude) — OLD BALL-FACE AVATARS REMOVED EVERYWHERE (p5style copy)
Evan: "Stop using the old avi profile pictures that look like soccer balls with eyes ... don't let me see the old avi again anywhere."
* New head art = Evan's Frosti (blue), Lumi (luma), Vex, Hotstreak (MenuArt/ult_avi_frost/fox/ember/lightning.png). Every other character gets a RECOLOUR of the closest new head as a stand-in until Evan makes theirs (tavi, sera, moss, orin, dark_zoogy, nightshade, lars, pinpoint, bolt, wraps, chompsy, coralie, geodie, helio). Script: C:\dev\art\newfaces\make_faces.py (+ fix_layout.py).
* Files: Content/Data/MenuArt/Faces/<key>.png (transparent; used by P5 menu/results/cut-ins/story via ZoogiP5::Portrait), MenuArt/<key>.jpg + .png (same new face on white, opaque, for the older screens that lay portraits over reference art), Art/Portraits/T_Portrait_*.png sources, ZoogiUI web portraits (p-*.png, portrait-p1..4, avatar.png).
* Reference screens with faces baked in were repainted: home_ref.png (four new BALL forms: Lumi, Vex, Hotstreak, Frosti), collection_ref_p/l.png (all 18 tiles + the hero panel), ModeSelect/ms_card_story.png (Lumi ball).
* Code: ZoogiP5::Portrait looks in MenuArt/Faces first (+ orin/darkzoogy aliases); cut-in eye strip defaults to the new heads; ZoogiHUD PortraitTex, ZoogiMenuWidget, ZoogiTeamColumnWidget use the new faces before the old /Game/UI/Portraits renders; PLAY screen always paints the picked Zoogis over play_ref art.
* Originals backed up in C:\dev\art\old_avis\ZoogiRollUnreal-p5style\ (nothing deleted). Combined copy untouched.
* buzzy.now: references staged in C:\dev\art\buzzy_refs (characters c_*, weapons w_*). Not signed in yet; Evan must sign in (no account creation / passwords by Claude; the 7-day trial is paid — not started).
* Build 35 UPLOADED to TestFlight 18:46 (new faces everywhere). buzzy episode plan: C:\dev\art\buzzy_refs\EPISODES.md (5 episodes x 6 shots); waiting on Evan to sign in to buzzy (Claude browser pane: not signed in; Chrome extension not connected).

## Oct 30 ~21:15 - buzzy Episode 1 "The Frozen Ring" (AI Anime Story agent, canvas agent/6ae540c9e7e2ffd0be85128f)
- Refs in agent: 4 relics + Frosti/Vex/Lumi/Lumi-blue/Hotstreak renders (also saved to C:\dev\art\buzzy_refs\chars\).
- Script 45 s / 3 chapters -> 9 x 5 s shots (Seedance 2.0 Fast 720p), storyboard regenerated text-free.
- First-pass Frosti/Lumi came out as four-legged foxes ("dogs"); regenerated as upright chibi robots. Old nodes still on canvas.
- Final: "Final Video with BGM" node, 45.8 s 1256x720, orchestral score. Known flaw: Vex runs on all fours in shots 1 and 3.
- Spend approx: images ~210, video 810 (360+360+90), audio 30.

## Oct 30 ~21:35 - Build 36: Story Mode rebuilt around the buzzy Episode 1 film (p5style)
- Evan: "Use what we have, rebuild the story with these and push to test flight" + "The full unchopped episode must be unlocked after completing it as a story mode, like now chop the episode up and tailor the game play to it".
- Film: Downloads\buzzy-canvas-1793413144363.mp4 -> art\buzzy_eps\ep1.mp4 (HEVC 1256x720 45.8 s). Re-encoded H.264 into Content/Anime/Movies:
  Story_EP1_P1 (0-15.3 s, final opens), P2 (15.3-30.6, trophy/Shadow/core), P3 (30.6-40.7, ring breaks, leap), P4 (40.7-end, Shadow holds core), Story_EP1_Full.
- Dialogue.csv who=film line=<movie id> plays a part inside a scene (tap SKIP after 1 s); Chapters.csv film column = full cut, shown as WATCH FULL EPISODE on the hub card once the episode is cleared; Clear screen says it unlocked.
- Film player in ZoogiStoryMode.inl (ZoogiStoryDir::GFilm): own UMediaPlayer/UMediaTexture + UMediaSoundComponent (film audio).
- Cast: Blue shown as FROSTI, Luma as LUMI, Tavi role -> Hotstreak (roster id hotstreak); portraits Data/MenuArt/Faces/story_{blue,luma,vex,hotstreak}.png cut from Evan's renders.
- Gameplay tailored: m1_final = classic 1v1 Frosti vs Vex (4 rounds); m1_escape = combo Frosti + Vex vs 2 Dark Zoogy.
- Test: -ZoogiStoryFilm (full-film player capture). Proofs C:\dev\proofs\p5_style\story_land, story_film. Backups C:\dev\backups\b36.
- iOS: b36ios.py / gob36.py (cook + stage + Mac build3.sh 36).

## Oct 30 ~23:30 - Build 37: Episode 2 film + full cast refs + Bolt/Hotstreak fix
- buzzy Episode 2 "Ember Rising" (same canvas): Downloads\buzzy-canvas-1793419754216.mp4 -> art\buzzy_eps\ep2.mp4 -> Story_EP2_P1 (dunes 0-10.2), P2 (Caldera + duel 10.2-20.4), P3 (Heart + Crystal Blade 20.4-35.6), P4 (Shadow takes Ember Core 35.6-end), Story_EP2_Full.
- Evan's full cast sheet cut into C:\dev\art\buzzy_refs\cast\<name>_{card,turnaround,front,head,ball,actions}.png (12: vex blue bolt luma sera tavi nightshade pinpoint lars moss hotstreak wraps).
- Evan: sheet is right -> the yellow electric robot is BOLT, Hotstreak is the orange lava one. Story best friend = bolt now (CSV ids/text); Faces/bolt.png = the yellow lightning head (was filed as hotstreak), Faces/hotstreak.png + MenuArt/hotstreak.* = lava recolour; story_bolt / story_hotstreak portraits. Backups C:\dev\backups\b37\menuart.
- Evan: Frosti and Lumi are separate characters (not Blue / Luma). Story hero is still the roster's "blue" id in battle (no Frosti 3D model) - for build 38.
- Evan: ship Ep2 as-is first; recast battles across the full roster in build 38 (Frosti vs Vex only once).

## Oct 31 ~09:40 - Build 38 UPLOADED to TestFlight (EXPORT_UPLOAD_RC=0, 09:36)
Evan: fix the Ep1 opening "dog" scene, Volcanic 2v2 story stall, Titan early knockback, recast, merge marble goal + RPG + anime.
- Ep1 dog shots cut for free (no buzzy spend): Story_EP1_P1 = 5.1 s shield clash only; Story_EP1_Full = 35.6 s. Source art\buzzy_eps\ep1.mp4, dog cuts in backups\b37.
- Recast (Missions.csv/Dialogue.csv): full cast across missions (bolt, hotstreak, nightshade, wraps, pinpoint, lars, sera, tavi, orin, moss); Frosti vs Vex duel only once. Portraits Faces/story_<name>.png cut from the cast sheet (head cards).
- VOLCANIC 2v2 STALL root cause: P2's SpawnActor failed (spawn collision) -> 3 balls, turn order jammed; then P2 spawned over a pit at (-700,-400) and fell (z -12000). Fix: SpawnActor uses AdjustIfPossibleButAlwaysSpawn + SpawnLocation = real location (ZoogiCombo.cpp + ZoogiGameMode.cpp), Volcanic combo P2 spawn hard-set to (-665,-892) (a logged stable spot). All search-the-floor code removed (it moved good spawns into pits). ComboFixSpawnsOnce stays as the post-forming guard. Verified stall_volc1: 4 balls stay, turns P1>P3>P2>P4 rotate, match finishes.
- Titan / size-up infuse (ZoogiAwaken.cpp ApplyInfuseImpact): blast radius capped at 1.6 x Zoogi radius and centred on the contact point, so orbs no longer fly before the collision. DefaultGame.ini HeavyScatterR 260 -> 60.
- Test flags: -ZoogiStoryMission=<id> (jump into a mission), -ZoogiStoryNoFinish (full rounds), -ZoogiStoryFilm. Runs in p5run.py: story_film, stall_volc1, stall_volc2.
- Battle-flow design doc (Marble + RPG + anime merge, decisions made per Evan "you call it", 2D anime videos yes): https://claude.ai/artifact/Q8XGQsGsujKUSuP2mmdWNc
- Runner: C:\dev\ios\b38ios.py / gob38.py; status b38ios_status.txt; tail38.txt.
- NEXT: Ancient Collision Meadows (brief + 3 refs in C:\dev\art\ancient_meadows\) - new independent arena, blockout + traversal validation first.

## Oct 31 ~10:15 - Ancient Collision Meadows: Phase 1 audit + Phase 2 PLAYABLE BLOCKOUT (p5style, not shipped yet)
Evan brief + refs: C:\dev\art\ancient_meadows\ (BRIEF_AncientCollisionMeadows.txt, ref1_overhead / ref2_rolling / ref3_elevated). Evan: "Yes to all".
- New arena EZoogiArena::Ancient (index 9, id 'ancient', name "Ancient Collision Meadows"); Meadow and all other arenas untouched. Map-select tile ANCIENT MEADOWS (art Content/Data/MenuArt/ModeSelect/ms_map_ancient.png = in-engine blockout overhead, redo after the art pass).
- Files: Private/ZoogiAncientLayout.h (layout table: hub r1900 z0, North Ruins +150, Sky Citadel +450, Emerald 0, Sunstone 0, North terrace +450, 4 hub bridges, 3 ring links, high bridge, 12 recovery islets z-320 + spring targets, island orb spots; OnFloor / FloorZ / Classify), Private/ZoogiArenaAncient.cpp (BuildAncient: AZoogiObstacle convex-hull collision + colour-coded blockout visuals, spire collider, 8 hub drum bumpers, Emerald edge rebounders, portal arches (visual), hub boost pads, momentum-lane slick zones, cloud sea + far islets; -ZoogiAncientTest traversal tester).
- Engine changes (targeted): AZoogiArena::IsOffCourt / DistanceToEdge use the island footprints on Ancient only; new obstacle trigger Kind 8 = aimed recovery spring (FZoogiObsTrigger::Target, ballistic throw, uses the shared ZoogiRelaunch guard: no endless loops); AddLateForming skips Ancient structural obstacles (the build-28 rise intro was lowering the floors -> everything fell before the first shot); star props skipped on Ancient; showcase poses for the 3 refs.
- Physics notes: real rolling decel ~1.3x the 450 constant; a rolling ball gets 5/7 g sin(slope) downhill -> east momentum ramp polish decel 60 (110 stalled). Curbs must be >= 60 cm (ball r50): 30 cm curbs were hit on the top edge and popped balls over.
- VALIDATION: anc_test 15/15 PASS (all hub->region routes incl. +450 Citadel climb, momentum ramp, ring links, terrace, high bridge (needs ~3100 cm/s), 3 spring recoveries, curb hold, bumper rebound, fall-out control). anc_match: CPU 4-player match, 2 rounds / 8 turns, no stalls, live spring save of P3. Proofs: C:\dev\proofs\p5_style\anc_shots (ref framings), anc_test (chase cam), anc_shots_sheet.jpg, anc_test_sheet.jpg. Runs in p5run.py: anc_shots, anc_test, anc_match (gop5anc.py).
- Known: Citadel ramp crest hops a fast Zoogi ~2.7 m (round the crest in Phase 3); cloud sea reads as snow; portals have no scoring trigger yet; no breakables / waterfalls / blimps / banners / spire hologram yet (Phases 3-5). Backups C:\dev\backups\b39_ancient.

## Oct 31 ~11:00 (Claude, session claude-fd) - NOTE FOR THE PARALLEL SESSION working on Ancient (owns the arena files + unreal.lock)
- Build 39 UPLOADED to TestFlight 10:38 (EXPORT_UPLOAD_RC=0): Ancient blockout + map card ANCIENT MEADOWS (CLASSIC -> MAP SELECT, 9th card).
- After build 39 I changed two files only (please keep them in build 40):
  * ZoogiFrontProfile.cpp: old crown "Winner!" overlay (UZoogiWinnerOverlay) no longer shows over the P5 results / story result screens
    (Evan: "Remove older result so we can see the p5 version"); it returns only with -ZoogiOldMenu.
  * ZoogiP5Style.cpp: menu lists with > 8 items (the 9-map list) use a tighter row step and put "TAP AGAIN TO ROLL" below the last bar
    (it covered ANCIENT MEADOWS). 8-item lists unchanged. Also ZoogiModeSelect.cpp: -ZoogiMapSelectTest (opens MAP SELECT on the last card).
- I am NOT touching ZoogiArenaAncient.cpp / ZoogiAncientLayout.h / ZoogiArena / GameMode / Combo or running Unreal jobs while you hold them.

## Oct 31 ~12:10 (Claude, session claude-fd) - took over the Ancient work (the parallel session stopped at a C1001 build failure 10:59)
- Fixed MSVC C1001 (internal compiler error) in BuildAncient: #pragma optimize off around it on MSVC only.
- Evan: "The larger field we are building is for battle royale only" -> BATTLE ROYALE card now plays Ancient Collision Meadows (ZoogiModeSelect case 0),
  Ancient removed from the CLASSIC map list (back to 8), ZoogiArenaRoute::ComboArena maps Ancient -> Meadow (2v2/Story/Trophy never land on it),
  route test updated. Royale on Ancient: RoyaleDanger = island footprints (ZoogiAncient::OnFloor), no "pushed past the ring" outs (falls + springs decide),
  portal orbs count in Royale, HoloSpire kept in Royale on Ancient, CPU routing along bridges (ZoogiAncient::AreaOf / RouteArea / PathStep).
  -ZoogiRoyaleFrameHz=<hz> sparse frame dumps. Runs: anc_royale (Royale on Ancient, CPU-driven P1, relic test).
- Grok (the "ZOOGI - VP Game Dev" agent in the Grok Bot app) generated art on request: C:\dev\art\ancient_meadows\grok\ (sky panorama, 5 tiles, banner).
  Imported via make_ancient_kit.py: T_AncientSky (AddPanoramaSky "Ancient"), Hub/Emerald/Citadel/North/Wall tiles (old procedural ones in
  SourceArt/Arenas/Ancient/_procedural). Tile scales raised (checker cells ~1.7 m). Bridge piers replaced by arched stone keels.
- Evan: Royale players = the new crystal-mech models, bots keep the old transform model and leave relics for pickup as weapons.
  ZoogiRoyaleHero.cpp: RoyaleHeroStart/Tick/Fire + RoyaleRelicDrop. Human = /Game/Zoogis/Ultimates ult_* rig (Frosti/Blue -> ult_frost_blue,
  Vex/Hotstreak -> ult_ember_red, Lumi -> ult_fox_gold_A), clips idle / run (slowed = walk) / jump / fall / cast (shoot) / box / slash / hit / cheer.
  Relics (Downloads relic zips -> C:\dev\art\relics -> Scripts\royale\import_relics.py -> /Game/Relics/*/SM_Relic*): Crystal Blade (melee slash x6),
  Sage Staff (bolts x2.5 power x10), Jewel Lantern (area blast x3), Rune Emblem (8 s shield). Bots drop one per life lost + one every 18 s (max 3).
  FZoogiRoyaleBolt::Power. -ZoogiNoHero / -ZoogiNoRelics / -ZoogiRelicTest / -ZoogiHeroYaw=. No Bolt (yellow) 3D model exists yet (needs Tripo).

## Oct 31 b41 P5 command menu (fork, unbuilt)
- NEW Source/ZoogiRoll/Public/ZoogiP5Command.h + Private/ZoogiP5Command.cpp: Slate overlay (self-registered FTSTicker) - ATTACK/SKILL/GUARD/ITEM/POWER/SHOWTIME fan beside the human Zoogi, skill list = hand cards (SP = card energy), speech bubble, target tag, party strip, SHOWTIME splash. Keys 1-6, Esc = back.
- Card visuals gated off by ZoogiP5Cmd::On(): ZoogiCardGlue (STANDBY popup), ZoogiAwakenWidget (bWantCards), ZoogiCardHandWidget (collapse), ZoogiArenaHUDWidget (COMMAND/GO! label), ZoogiPlayerController (1-7 card keys skipped while menu Showing()).
- Restore: -ZoogiOldCards / -ZoogiCardHandUI / -ZoogiOldMenu. Capture: -ZoogiP5CmdTest (p5cmd_t1_open / t1_skills / t2_open / t3_open .png in -ZoogiShots dir). Backups: C:\dev\backups\b41_p5cmd

## Oct 31 b41 ball forms + Ancient reference look (Claude)
- Ball skins: Evan's 5 ball renders -> SourceArt/Balls/T_Ball_{Frosti,Lumi,Hotstreak,Bolt,Vex}.png (equirect wraps) -> Scripts/royale/make_ball_kit.py
  (C:\dev\ios\ballkit_run.py, run DETACHED - it kills UnrealEditor.exe) -> /Game/Zoogis/Balls/T_Ball_* + M_ZoogiBallSkin (Tex param). Imported 12:34, RC=0.
- ZoogiRoyaleHero.cpp: human now starts the Royale fight as the crystal mech (walk/run/jump/shoot; MorphCD cleared); TRANSFORM rolls into the ball form =
  sphere with M_ZoogiBallSkin + that Zoogi's T_Ball (Luma/Lumi green, Hotstreak fire, Bolt yellow, Vex red, else Frosti blue), spinning with the physics ball; flash + cheer on change.
- ZoogiArenaAncientLook.cpp (BuildAncientLook, -ZoogiAncientOldLook skips): volcano + crown orb centrepiece, hub rim pillars, blue/red fortress walls,
  void towers, rope bridges + bunting, star pads (kind-8), crates, ruin valley at Z -1900, sky temples. Hub bumpers = red drums. HoloSpire off on Ancient.
- -ZoogiP5CmdTest also holds the HumanTest auto-shot while the command menu is open (ZoogiPlayerController::UpdateHumanTest).
- p5run runs added: p5cmd_land, p5cmd_port. Chain: C:\dev\run_anc.bat -> ios\gob41.py -> ios\chain41.py (ball kit, then p5run anc_shots,anc_royale,anc_test,p5cmd_land,p5cmd_port).
- Oct 31 b41 (Evan 12:50): (1) Ancient = Battle Royale only: StartMatch sends classic/episode/aerial on Ancient to the Meadow (dev: -ZoogiAncientClassic; harness flags exempt).
  (2) Royale on Ancient = COLLECT orbs (touch -> +orb, Awaken charge, +80/160 HP; respawn at OrbSpots after 8 s; FZoogiRoyalePlayer::Orbs; -ZoogiRoyaleOrbKnock = old knock-off).
  (3) Apple on-screen controller: NEW ZoogiApplePad.h/.cpp (GCVirtualController iOS 15+, GameController framework in Build.cs), shown during Royale fights; drawn stick/JUMP/TRANSFORM hide.
      Pad: L stick move, R stick look, A jump, B attack (MenuAction RoyaleAttack; E key), X transform, Y awaken (Q key). -ZoogiNoApplePad = off.
  (4) "14 hit combo instantly": AddLateForming lowered Frozen Ring 2 floors -> 12 orbs fell during the intro -> first shot scored COMBO x12+. Floors/ramps/curbs/wide flat colliders now skip the intro on every map.
  (5) ZoogiP5::Font clamps size >= 1 (ease-out-back pop went negative -> Slate ensure + 2 s hitch).
  (6) Royale AAA dynamic camera (Evan 13:14 refs, ZoogiRoyaleInput.cpp RoyaleCamera): critically damped vertical spring (no step/ramp jitter, faster on big drops),
      smart boom collision (r30 probe, orbs ignored, smooth 22/s retract + 2.2/s auto-recovery), landing shake + dip scaled with fall speed, speed framing
      (FOV +7, boom +14%, velocity look-ahead), rival-close adaptive pull-back (boom +32%, FOV +4).
- Oct 31 13:43: BUILD 41 UPLOADED to TestFlight (EXPORT_UPLOAD_RC=0, Mac UBT_RC=0 incl. ZoogiApplePad GCVirtualController). Captures 13:22-13:28: Frozen first shot 0 instant orb-outs
  (was 12), P5 font ensure gone, Ancient Royale orb collecting live. Mech hero framing: camera +30% back / +85 up in mech form (human).
- Oct 31 14:24: BUILD 42 UPLOADED (EXPORT_UPLOAD_RC=0): P5 command menu without the blank red/black slashes (-ZoogiP5CmdSlash restores) and without the bottom party strip (-ZoogiP5CmdParty restores).
- Oct 31 15:26: BUILD 43 UPLOADED. Epic Game Animation Sample (C:\dev\GASP56, free, Fab) -> 25 loops/jumps/lands retargeted onto the 4 ult_* crystal rigs
  (Content/Characters/UEFN_Mannequin = source skeleton/rig/anims copied in; Scripts/royale/retarget_gasp.py run in the FULL editor via ios\gasprt_run.py
  (-ExecutePythonScript; the commandlet asserts on Slate in the batch op); IK_/RTG_ assets in /Game/Zoogis/Ultimates/Retarget; output <rig>/GASP/GASP_M_Neutral_*).
  ZoogiRoyaleHero.cpp: direction-relative walk/run/strafe (8 dirs, 0.12 s hold), sprint > 950, jump start -> fall loop, land light/run/heavy/roll by impact. -ZoogiNoGasp = Tripo clips.
  Lyra Starter Game downloading to C:\dev\Lyra56 (next: rifle/aim/hit-react clips). Launcher "Create project" needs an EMPTY folder. C: has ~39 GB free (before Lyra).
- Oct 31 16:37: BUILD 45 UPLOADED (Mac log EXPORT_UPLOAD_RC=0; the local poller hung after a first launch had TX545 from a 44->45 sed - b45ios.py fixed to TX544.
  LESSON: when cloning bNNios.py only replace the build number in the named spots, never a blanket s/44/45/).
  Royale hero = generic stand-in: Epic UEFN mannequin coloured per Zoogi (MI_UEFN_Mannequin_Blue part params). -ZoogiHeroTripo = crystal rigs.
  Clips: GASP originals (/Game/Characters/UEFN_Mannequin/Animations), crystal actions retargeted (Actions/GEN_*, retarget_actions.py),
  Lyra rifle set retargeted (Rifle/R_*, retarget_rifle.py; Lyra source in /Game/Characters/Heroes + retargeters in /Game/Zoogis/Ultimates/Retarget are NeverCook).
  Gatling gun = procedural parts (receiver/drum/grip/rail + 6 spinning barrels + Zoogi-colour muzzle ring); 0.075 s cadence, Power x0.3, 1.7x speed, light spread,
  90 cm initial streak; hold B / RT / Left Ctrl (awaken = Y / G). Relics = gun power-ups (HEAVY ROUNDS x45, SHOCK BLAST x3, RUNE SHIELD), blade out, nothing in hand.
  Camera: ball = low close chase (back 290, up 70, pitch 4, FOV 84); standing = tight right-shoulder (back 150, up 140, side 66, pitch 6, FOV 72), may tuck to 18 cm.
- Oct 31 b46 map-realism worker (fork, code only, NOT built yet): NEW Private/ZoogiArenaRealism.cpp = visual-only dressing actor per arena (no collision,
  gameplay untouched; ticker removes it when the arena changes theme; -ZoogiNoRealism = off). Entry ZoogiRealismApply(Arena, Theme):
  Meadow = pollen motes + 900 grass tufts + 140 field stones on the plain (orb-exit ravines kept clear) + ~22 pine stands in the valley (north mesas avoided,
  far pines lighter/bluer); Frozen = light snowfall (420 flakes); Volcanic ash + embers, Arabian sand motes, Tomb dust, Cosmic star dust, Neon rain streaks
  (those five need a one-line hook in AZoogiArena::Build - core file, left to the main session). Called from BuildMeadow / BuildFrozen ends (+ forward decls).
  Backups: C:\dev\backups\b46_maps\ (ZoogiArenaMeadow.cpp, ZoogiArenaFrozen.cpp).
- Oct 31 b46 map-realism pass 2 (fork, NOT built): ZoogiArenaRealism.cpp now scales its dressing actor like the arena (all coords = arena layout units) and adds
  per-theme set dressing (instanced basic shapes, collision-free, all outside the court): Volcanic = 80 basalt columns rising from the lava round the island
  (tops below the court), 34 obsidian spires, 16 distant volcanoes (5 with glowing craters + smoke columns); Cosmic = 170-rock asteroid belt (crystal shards on
  every 6th) + 9 floating ruin fragments; Arabian = desert floor z-2200 + dunes + mesas; Tomb = desert floor at the pyramid base z-1500 + dunes + mesas +
  18 half-buried obelisks (pyramid wedge yaw 115-180 kept clear); Neon = dark street plane at the skyline base + glowing street grid + red rooftop beacons /
  masts on the real skyline towers (FRandomStream(555) replayed). Pass-1 backup: C:\dev\backups\b46_maps\ZoogiArenaRealism_pass1.cpp.
- Oct 31 ~18:00 (b46 prep, built 17:45 OK; latest edits after that NOT yet built: StageShift spot finder + softer eruption light + P5 WordmarkWall):
  * Stands bounce to centre: AZoogiObstacle::CenterBias (Ancient hub pillars / posts / crates / fortress walls, BounceGain 1.1-1.15).
  * Bots easier: human dmg x2 vs CPU (-ZoogiRoyaleHumanDmg=), CPU lives = Lives-1.
  * Menus: MenuRoot uses ModeSelect/ms_bg (ZOOGI ROLL wordmarks) behind every screen (-ZoogiOldMenuBg); P5 menu + results draw WordmarkWall (-ZoogiNoWordmark).
  * Royale cog button (top right, action Menu -> ReturnToMenu). Classic: tap the hard-locked target again -> ClearLock + ZoogiP5Cmd::Reopen().
  * NEW ZoogiStageShift.h/.cpp: classic halftime (round MenuRounds/2+1) P5 wipe "STAGE SHIFT!" -> dusk light + 4 star bumpers rise (removed next match);
    Royale Ancient eruption at 70 s (red dusk, crater bursts, lava bombs w/ 1.3 s warning ring, 140 dmg + push). -ZoogiNoStageShift / -ZoogiStageShiftTest.
    GameMode.h: PublicFeed/PublicPopup forwarders. p5run runs: shift_classic, shift_royale, menu_tour (-ZoogiMenuTour=dir).
  * Map realism worker (fork): NEW ZoogiArenaRealism.cpp (all classic maps dressing, -ZoogiNoRealism), hook in AZoogiArena::Build.
  * Grok concepts: C:\dev\art\concepts\royale_heroes\ (5 x turnaround + Gatling hero). Luma recoloured green/gold (ref was pink/white).
  * b46ios.py ready (TX544 checked). Launch: C:\dev\run_anc.bat (gob41 -> chain41: p5run captures then b46ios_HOLD - switch to b46ios.py to ship).
- Oct 31 18:31: BUILD 46 UPLOADED (Mac EXPORT_UPLOAD_RC=0, verified via ios\macpoll.py; the local poller in bNNios.py goes quiet during the Mac upload - use macpoll). Captures OK: P5 ZOOGI ROLL wallpaper, classic stage shift (dusk + 4 bumpers), Ancient eruption.
- Oct 31 map-realism pass 3 (fork, NOT built): play surfaces. ZoogiArenaRealism.cpp PlaySurfaces(): retunes the EXISTING floor materials in place (M_ZoogiArenaFloor
  kit MIDs; Cosmic/Tomb disc MIs get a MID) - only BaseBright / DetailStrength / DetailScale / GlowTint / Roughness / Specular (GlowStrength + forming ramps untouched):
  Volcanic brighter basalt + obsidian sheen, Arabian/Tomb sandstone grain, Cosmic/Neon calmer line paint (GlowTint .25) + polished / wet sheen, Meadow wear, Frozen gloss.
  Unknown disc materials log "REALISM floor <mat> params: ..." for the next pass. Worn edge lip (72 flat trim segments on the outer 22 cm, 2-3 cm proud, no collision)
  + thin seam: Volcanic (orange seam), Arabian, Tomb (gold seam), Neon (dark lip, pink rim stays the seam). Volcanic warm fill point light (no shadows).
  Pass-2 backup: C:\dev\backups\b46_maps\ZoogiArenaRealism_pass2.cpp.
- Oct 31 20:20: BUILD 47 UPLOADED (EXPORT_UPLOAD_RC=0): map realism pass 3 (play-surface material retune, edge lips, Volcanic fill light). p5run/bNNios no longer reopen the editor (it stole focus from Explorer launches). Tripo inputs: C:\dev\art\concepts\royale_heroes\tripo_input (+HOW_TO_TRIPO.txt), output -> C:\dev\art\tripo_out.

## Oct 31 night (Claude) — Royale hero models built from scratch (no Tripo)
- Script: C:\dev\art\hero_build\build_frosti.py (Blender 5.0, `blender -b -P`). Launcher: C:\dev\run_anc.bat (fixed; round/hero chosen in C:\dev\art\hero_build\tag.txt, e.g. `frosti r16` or `all h1`). Log: frosti_log.txt; renders: hero_build\renders\<hero>_<tag>_{front,side,back,headfront,head34}.png; .blend per hero.
- Built on SKM_UEFN_Mannequin (exported FBX) so every game animation works: body slimmed per bone (weight-blended), mannequin head removed, armor = shells cut from the body surface (keep skin weights), crystals/gems/head parts rigid-skinned to bones.
- Heads follow the user's mask art (C:\dev\art\concepts\royale_heroes\heads\*.png): white cat face, slit eyes, nose/mouth, white brow V, gem crest + crown, side blades, ringed ear discs, cheek guards, back crown ring.
- Palettes/shape tweaks per hero in PALS: frosti, bolt (yellow+black, ring eyes, cyan diamonds), hotstreak (red/orange, frown), luma (white+gold, big ears). Vex: no head art yet.
- Not yet done: export to UE (skeletal FBX on same armature) + swap into ZoogiRoyaleHero; UE materials for gem/crystal.
- Note: a persistent job-runner daemon was refused by the permission system; each Blender job is a one-shot double-click of run_anc.bat.

## Oct 31 ~23:20 (Claude) — Blender hero work DELETED per Evan
- Evan: "Forget and delete what we did in blender." C:\dev\art\hero_build removed entirely (build_frosti.py, build_chibi.py, renders, .blends, mannequin FBX export). The section above about Blender hero models is obsolete.
- Direction now: Fab "Mecha Robot: Aurex" (Fieldy Panda) is the model for ALL characters (Evan buying it). Ball forms = Evan's two ball sheets (Frosti, Lumi, Vex, Hotstreak, Blue, Terra, Zephyr, Ember; earlier sheet also Aero + red Vex). Generating the balls in buzzy.now.

## Nov 1 ~00:20 (Claude) — Aurex hero + buzzy ball forms in game (built, NOT uploaded)
- Fab "Mecha Robot: Aurex" (UE 4.26 assets, UE4 mannequin skeleton + plate bones) copied from E:\EpicGames\VaultCache\MechaRob609fbd0e5b04V1 -> Content/Mecha_Aurex (no demo map).
- Scripts/royale/aurex_setup.py (runner ios\aurex_run.py / goaurex.py): ball kit re-import + IK_Mecha_Aurex + RTG_UEFN_to_Aurex (/Game/Mecha_Aurex/Rig) + 50/54 hero clips retargeted into /Game/Mecha_Aurex/Anims/{Actions,Animations/*,Rifle} (same names). Material: MI_MechaAurex_CustomColor params Color_Zone_R/G/B + Emissive_Color.
- ZoogiRoyaleHero.cpp: hero mesh = SK_Mecha_Aurex (-ZoogiNoAurex = UEFN stand-in), clip root /Game/Mecha_Aurex/Anims, per-Zoogi zone palettes (frosti/lumi/vex/hotstreak/blue/terra(moss)/zephyr/ember/bolt). BallFor += blue/terra/zephyr/ember.
- Ball forms: buzzy canvas agent/6ae6be457f163a25e37cbdb9 (16 images: hero render + flat wrap per ball; ~106 credits). Files C:\dev\art\buzzy_out\balls (wrap2k = seamless 2048x1024) -> SourceArt/Balls/T_Ball_<Name>.png -> /Game/Zoogis/Balls.
- ZoogiBall.cpp: classic balls with a ball design roll as that ball (sphere + M_ZoogiBallSkin); -ZoogiOldBalls = models. Backups C:\dev\backups\b48.
- Proofs: C:\dev\proofs\p5_style\anc_motion, shift_classic. Downloads has 17 leftover *.tmp (browser download temps of the ball PNGs) - safe to delete.

## Nov 1 ~00:55 (Claude) — P5-style Battle Royale HUD + builds 48/49
- ZoogiRoyaleHUD.cpp: P5 restyle (ink/paper/magenta slanted panels, Anton/Bangers via runtime UFont wrappers of the ZoogiP5 composite fonts): LEADER/BOARD leaderboard + ZOOGIS LEFT stamp, slanted clock + diamond life pips, P5 rival tags (Bangers name, slanted HP bar, KO chip, red/white lock chevron), magenta slam kill banner, HP bar with magenta HP tag, burst countdown, ransom callouts, P5 results card, respawn/edge/spectating. -ZoogiOldRoyaleHUD = Marble Clash look. Backup C:\dev\backups\b48\ZoogiRoyaleHUD.cpp. Proof C:\dev\proofs\p5_style\anc_motion.
- Build 48 (ios\b48ios.py via chain48) shipped WITHOUT Aurex cooked (runtime-loaded path, not in AlwaysCook) -> hero falls back to UEFN mannequin; balls + P5 HUD are in.
- Fix: DefaultGame.ini +DirectoriesToAlwaysCook /Game/Mecha_Aurex, +NeverCook /Game/Mecha_Aurex/Demo. Build 49 queued (ios\chain49.py waits for 48 DONE, then b49ios.py).

## Nov 1 ~01:35 CT - build 50 (Claude)
- Scheduled check: builds 48, 49, 50 all EXPORT_UPLOAD_RC=0 / UBT_RC=0. 49 + 50 cook include /Game/Mecha_Aurex (Saved/Cooked/IOS/ZoogiRoll/Content/Mecha_Aurex: Anims, Materials, Meshes, Rig, Textures). 48 had no Aurex (fallback mannequin).
- ZoogiMorph.cpp/.h: bAurex mode - every Royale bot is SK_Mecha_Aurex (rifle clips mapped onto the morph clip keys, Calib -90 yaw, 178 cm, MI_MechaAurex_CustomColor per Zoogi via AurexPalette), ball form shows the ball skin on Visual, muzzle = hand_r + 45 cm forward. -ZoogiNoAurex = old SK_ZoogiBlue transform model. Log: 'MORPH Aurex body for <name>'.
- ZoogiBall.cpp: ball skins also for pinpoint (Frosti), nightshade/dark (Vex), wraps (Zephyr), sera (Ember), tavi (Bolt).
- ZoogiRoyaleInput.cpp (Evan: calibrate sticks, no passing centre): right stick POINTS (direction measured from the heading when the stick left centre, 900 deg/s swing; -ZoogiRateLook = old rate look); left pad stick radial dead zone 0.12 rescaled, full at 0.92; drawn touch ring follows a thumb that runs past the rim. Backups C:\dev\backups\b48\ZoogiMorph.* / ZoogiRoyaleInput.cpp.
- Proof C:\dev\proofs\p5_style\aurex_bots_sheet.jpg, aurex_bot_zoom.jpg. Scripts ios\chain50.py, gob50.py, b50ios.py. Build 50 uploaded 01:31.
- Evan: leave menus / character select alone. Build 51 = all the Marble Clash reference menus (Home, Inventory + item card, Shop). Refs: proofs\mc\a.mp4 -> art\mc_menu_refs\mc_*.png + key_sheet.jpg.

## Nov 1 ~07:00 CT - build 51 (Claude)
- Evan: right stick must move the camera as the thumb moves, like a screen swipe (not point-to-look / not wait for centre). ZoogiRoyaleInput.cpp: Apple on-screen right stick is now a TRACKPAD: yaw += stick X change x gain (150 deg per full travel, -ZoogiStickLookGain=), pitch from Y change; touch-down / lift (snap to centre) never turns; thumb pinned at the rim keeps turning 90-240 deg/s. Physical controllers: hold-to-turn with rescaled dead zone. Point-to-look removed.
- Build 51 = 50 + that (ios\chain51.py / b51ios.py / gob51.py). Menus moved to build 52.
- Menu mockups in art\mc_menu_refs: b51_marble_clash_version.jpg, b51_p5_version.jpg, b51_p5_straight_version.jpg (Evan: P5 tilt/crookedness is out, keep it uniform like Marble Clash), b51_*_mockup.png (blend). Waiting on his pick.
- 07:06 CT: build 51 uploaded (UBT_RC=0, EXPORT_UPLOAD_RC=0). Evan asked for menu backgrounds showing the Zoogi Roll arena in bright scenes: mockups b52_marble_clash_arena.jpg / b52_p5_straight_arena.jpg (Ancient Collision Meadows in-game shots + ZOOGI ROLL ARENA logo). Menu look still not picked; build 52 menus not started.
- 07:45 CT: new ZOOGI ROLL ARENA logo options (buzzy Nano Banana 2.1, green screen, keyed in Python): art\logo\zoogi_roll_arena_logo_1..4.png (1 blue/magenta/gold crown, 2 fire shield, 3 ice crown, 4 magenta comic shield), raw opt*_raw.png, preview logos_on_arena.jpg. Waiting on Evan's pick before swapping Content/Data/MenuArt/zoogi-roll-arena-logo.png.
- 07:55 CT: Evan picked ALL four logos, one per season. Content/Data/MenuArt/logo_spring.png (1 classic), logo_summer.png (2 fire), logo_fall.png (4 comic), logo_winter.png (3 ice), 720 px. ZoogiUtil::SeasonLogoKey()/MenuArtKey() (ZoogiTypes) map the old key zoogi-roll-arena-logo to the season (Mar-May / Jun-Aug / Sep-Nov / Dec-Feb; -ZoogiLogoSeason= forces). UZoogiMenuRoot::Art + UZoogiWinnerOverlay::LoadArt use it; MenuRoot header logo keeps aspect. NOT compiled yet (goes with build 52). Backups backups\b52.
- Mockup b52_p5_arena_v2.jpg: no white frame / magenta shadow behind the robot (Inventory) or NO ADS (Shop), NO ADS text centred in the starburst. Evan leans P5 but 'not sold yet'.

## Nov 1 ~08:55 CT - build 52 (Claude)
- NEW front end (Source/ZoogiRoll/Public/ZoogiFront.h, Private/ZoogiFront.cpp): HOME / INVENTORY (ZOOGIS, BALLS, WEAPON, CARDS + item card) / SHOP (offers, skins, chests, equipment deals, crystals, coins + tab bar) / 4 chest slots + chest opening / DAILY LOGIN (7 days). Straight P5 look over the LIVE Ancient Collision Meadows arena (AZoogiFrontStage: scenic camera per screen + equipped Aurex on a disc, drag to spin, tap = cheer). BATTLE -> P5 MODE SELECT (its BACK returns). -ZoogiNoFront = old flow; -ZoogiFrontTest = capture script.
- Profile (ZoogiFrontProfile): LoginDay/LoginStreak, EquippedZoogi/EquippedBall, OwnedSkins (json login_day, login_streak, equipped_zoogi, equipped_ball, owned_skins); LoginTable/ClaimLogin, OwnsSkin/SkinPrice/BuySkin, SetEquipped; chest slots 3 -> 4. Equipped Zoogi -> MenuPlayers[0].ZoogiIndex (Blue=blue, Frosti=pinpoint, Lumi=luma, Vex, Hotstreak, Terra=moss, Zephyr=wraps, Ember=sera); equipped ball overrides the human's ball skin (ZoogiBall).
- GameMode BeginPlay builds Ancient for the menu when the front is on (not -ZoogiAuto / -ZoogiArena / -ZoogiP5MenuTest). ZoogiAurexPalette exported from ZoogiMorph.
- Royale: P5 cog 1.3x (paper tile, magenta shadow), action RoyalePause -> P5 pause panel (RESUME / RESTART / MAIN MENU + SWIPE LOOK, RIGHT STICK LOOK, MOVE STICK, INVERT LOOK, SFX, MUSIC). ZoogiControls in GameUserSettings [ZoogiRoll.Controls]. Pitch sign flipped (Evan: up was down), pitch range -32..30 (camera -30..45). Apple pad hidden while paused.
- Art: Content/Data/MenuArt/Front/ball_*.png (8 icons). Proofs: proofs\p5_style\front_land, front_port, royale_pause, front_sheet.jpg, pause_sheet.jpg, front_check2.jpg. Scripts ios\chain52.py / b52ios.py / gob52.py.
- 09:05 CT: build 52 uploaded (UBT_RC=0, EXPORT_UPLOAD_RC=0).

## Nov 1 b53 UPLOADED to TestFlight 10:25 (EXPORT_UPLOAD_RC=0)
- Front buttons fire on touch start; TASKS / NEWS / SETTINGS popups with systems behind them (daily+weekly tasks in profile).
- Stands/bleachers/rafters solid (StandBlockR; orbs past it = knocked off, Zoogis bounced back to centre). -ZoogiNoStandColliders.
- Royale: hide left/right/both sticks setting. Classic: clean aim view (dotted squares, crosshair removed; yellow arrow, target line, floor target KEPT), drag-anywhere launch, P5 slash hint, small P5 bank preview, REACTION centred.
- Next: Ancient Meadows rebuild to the new reference (9e958d85-image.png).

## Nov 1 ~12:00 Ancient Meadows v2 (Claude) - Evan refs: meadow2\meadow_ref.png, meadow_ref_poly.png (low-poly, current target), meadow_ref_4side.png
- NEW Source/ZoogiRoll/Private/ZoogiArenaAncientMeadow.cpp: AZoogiArena::BuildAncientMeadow (called from BuildAncientLook when ZoogiAncient::Meadow2(); -ZoogiAncientV1 = Oct 31 look).
  Low-poly flat-colour look: faceted volcano on an arched drum (collision cone r927 + drum r968 h390), gold crown + purple orb, ring walkway, plaza cobble ring + Z medallion + blue spawn rings,
  rim lamp posts (old pillar hulls), front stairs (collision to z-500) + Z-banner gate + brick path + crystal pillars, balustrades (old wall hulls), star pads, yellow drums (old crate hulls),
  landmarks on outcrops (crystal shrine NW, gold-dome palace NE, temple + armillary beside the plaza), meadow at z-640 (below FallOutZ), water channels, rivers, palms, plants, rocks, low-poly sky.
- Layout (v2 only): no link_south; rec_hub_s -> (-1000,2650), rec_link_s -> (-1700,5100). BuildAncient v2 swaps: pavers on islands, purple decks, gold curbs, blue rune springs, alternating drums.
- Art: C:\dev\art\ancient_meadows\meadow2 (Buzzy Nano Banana): tiles, banner, medallion, skies -> SourceArt/Arenas/Ancient/T_Anc*.png + T_AncientDuskSky; make_ancient_kit.py also builds M_AncientDecal.
- Captures: p5run anc_shots poses meadow_ref / meadow_ref_b / meadow_ref_c (ZoogiArenaShowcase.cpp). Grok (ZOOGI VP Game Dev) reviews: round 1 34/100, round 2 notes applied.
- Tripo inputs for the whole arena: C:\dev\art\ancient_meadows\tripo_input (4 views + HOW_TO). Evan runs Tripo -> C:\dev\art\tripo_out\ancient_arena.glb; then import + align colliders.
- Backups of the pre-v2 files: C:\dev\backups\b54pre. Not in a TestFlight build yet.

## Nov 1 ~13:55 — Wolf Pack 2.0 (all 20 upgrades) + Grok design pack + map shopping list (Claude)
- Wolf Pack 2.0: new ZoogiWolfPack.h/.cpp (UZoogiWolfPackSys) + ZoogiClone/Powers/ClassicPad/PlayerController/FrontProfile/Front edits; S_Howl imported. Toggles -ZoogiOldWolfPack, -ZoogiBloodMoon/-ZoogiNoBloodMoon; test -ZoogiWolfTest (p5run 'wolf_test').
- wolf_test #1 (13:45) PASSED end to end: summon 4 wolves (Blood Moon), paint ALPHA + orb, pack out, charged blast, TRIPLE HUNT badge, tally x4. Shots in proofs/p5_style/wolf_test/wolf_0*.png.
- HUD pass 2 (13:52 build): text now P5 Anton font at real pixel size (was tiny grey engine font), cut-in shorter (1.1 s) + higher so paint targets stay visible, paint rings = rivals + 8 nearest orbs (was every orb), pack counter/score moved top-left under the portraits (clashed with centre banners), toasts lowered to 36%.
- Backup of pre-wolf files: C:\dev\backups\b54wolf. Not in a TestFlight build yet.
- Grok design pack for Ancient Meadows delivered (art/ancient_meadows/grok_design: palette, notes, prop sheets, Blender FBX) — not used yet.
- Maps: recommended Synty POLYGON packs (Adventure $29.99, Ancient Egypt $149.99, Elven Realm $199.99, Sci-Fi City $49.99) or SyntyPass; nothing purchased.
- wolf_test #2 (13:53) PASSED with the new HUD (hits 5, score x5). Follow-up (not yet built): pack panel Y 215->250 so "WOLF PACK" clears the SUPER label.

## Nov 1 ~14:32 — Wolf Pack wolves = Bitgem Micro Werewolf (Claude)
- Evan: "Make the wolf's this I bought in the past" (Bitgem Micro Monster & Heroes Pack). Found as a Unity Asset Store download: C:\Users\skybi\AppData\Roaming\Unity\Asset Store-5.x\BitGem\...\Micro Monster Heroes Pack Low Poly.unitypackage (not in the Fab library of the launcher account).
- Extracted Micro_Werewolf -> SourceArt/Characters/MicroWerewolf (fbx + T_MicroWerewolf.png). Scripts/import_werewolf.py (runner ios/werewolf_import_run.py): /Game/Characters/MicroWerewolf/SK_MicroWerewolf + skeleton, M_MicroWerewolf (Tex, Tint, TintAmt, Glow, BaseLit 0.45), 15 clips A_MW_* cut from the single take by Bitgem's frame list (walk, bite, idle, dizzy, laugh, jump, up_loop, fall_idle, land, die, panic, attack2, hit_front, getup, talk).
- ZoogiClone: SetupBody() swaps the marble for the werewolf (1.35x marble height, upright, faces its run, yaw offset 180 / -ZoogiWolfYaw=), walk anim play-rate follows speed, bite on every hit, attack2 while charged, fall pose in the air, pack tint 35% (Dire 55%), Glow = charge. Tail streak removed for it. -ZoogiOldWolfMesh = old marble.
- Tests: p5run 'wolf_look' (-ZoogiWolfLook turntable + no Blood Moon) and 'wolf_test'. Close-up proof: proofs/p5_style/wolf_look/wolf_04_close.png. First run after a material change shows grey (shader compile) — run twice.
- Also: pack panel Y 250 + HUD pass 2 are in this build. Backup of pre-werewolf clone files: backups/b54mw. Not in TestFlight yet.

## Nov 1 ~15:55 — Stylized VFX Pack + Inferno World island on Volcanic (Claude)
- Evan bought "Stylized VFX Pack" (VaultCache Stylizede214be7764a4V2) and showed Inferno World Free's knight-statue lava island (VaultCache InfernoWf31afb59e1d6V1). Both copied into Content/VFX_PackVol1 and Content/Inferno_World_Free; DefaultGame.ini always-cooks /Game/VFX_PackVol1/Particles + /Game/Inferno_World_Free/Meshes.
- AZoogiFX::Stylized(Name, Loc, Scale) / StylizedLoop (P_<Name>_Converted; -ZoogiNoStyFX). Hooks: Hotstreak Explosion power = Big_Explosion + Smoke_v2; Bolt Static Shock = ElectricShock on cast + ElectricShock_v2 on each stunned rival; wolf detonation = Mid_Explosion (charged/Dire Big_Explosion_v2, fire wolves Fire_v2); ring-out = Big_Explosion_v3.
- Volcanic: AddInfernoIsland (ZoogiArenaInferno.cpp + generated ZoogiInfernoIsland.inl from the pack's Demonstration map via Scripts/dump_inferno.py): 200 pieces / 62 ISMs at (2600,-6900,-640) scale 0.5 yaw 200 (far side of the match camera), 7 brazier fires, warm key light; islets within 42 m skipped. -ZoogiNoInferno, -ZoogiInfernoAt=X,Y, -ZoogiInfernoScale=, -ZoogiInfernoYaw=.
- Tests: p5run 'volc_vfx' (-ZoogiStyTest: one shot per effect) + map_volcanic poses inferno_side / inferno_close. Backups: backups/b54vfx.
- Build 54 attempt #1 15:13: cook OK, upload to the Mac timed out (Mac unreachable at 15:15, reachable again 15:31). Re-run with all of the above.

## Nov 1 b54 UPLOADED to TestFlight 16:06 (UBT_RC=0, EXPORT_UPLOAD_RC=0)
- Wolf Pack 2.0 (20 upgrades) with the Bitgem Micro Werewolf wolves + S_Howl + wolf HUD pass 2; Ancient Meadows v2 low-poly; Stylized VFX Pack blasts (Hotstreak, Bolt, wolves, ring-outs); Inferno World island + brazier fires beside the Volcanic court. Cooked content checked: VFX_PackVol1, Inferno_World_Free, Characters/MicroWerewolf.
- Not yet played on a phone.

## Nov 1 ~17:15 — b55 fixes (Claude) - Evan: remove red drag hint, shortcut = latest, blasts keep going, more target time, reaction slow/zoom/5 s
- Drag hint (ZoogiHUD DrawAimHint): magenta slash + magenta DRAG BACK tag removed; thin white guide line on ink, ink tags.
- Desktop shortcut C:\Users\skybi\OneDrive\Desktop\Zoogi Roll.lnk (was pkg-merge42\Win64\ZoogiRoll.exe; backup backups\b55\Zoogi Roll.lnk.bak) -> wscript C:\dev\ZoogiRoll_Latest.vbs = UnrealEditor -game on C:\dev\ZoogiRollUnreal-p5style (latest code/content of the last editor build; override project in C:\dev\zoogi_latest_project.txt). Note: p5run/bNNios taskkill UnrealEditor, so a running game closes when a build starts.
- Stylized VFX: the converted systems loop -> AZoogiFX::Stylized spawns unpooled, Deactivate after 0.8-1.8 s, DestroyComponent +2.5 s. Proof volc_vfx/wolf_13_sty_after.png (court clean).
- Wolf Pack paint window 5.0 s first / 4.0 s after (was 2.8 / 1.6).
- Reaction close-up (Public/ZoogiReactCam.h, Window 5 s): Fun Mode 2 BLOCK! (CPU shot predicted to hit the human's Zoogi) and rival-wolf PARRY (ParryDist 230 -> 420): time dilation scaled so contact lands ~5 real s after the prompt, instant cut to a side close-up (blend 0 - blends crawl under slow-mo), "INCOMING!" prompt. -ZoogiNoReactSlow = off. Proof volc_vfx/14_react_block_start.png + 16_react_block_mid.png (-ZoogiReactShots).

## Nov 1 b55 UPLOADED to TestFlight 17:23 (UBT_RC=0, EXPORT_UPLOAD_RC=0) - the b55 fixes above. Not yet played on a phone.

## Nov 1 ~19:20 — b56 (Claude): Grok v14 arena imported + menu inset + drag hint on the arrow tip
- Evan: drag hint = DRAG BACK TO LAUNCH label + ring around the TIP of the yellow aim arrow, thin white line KEPT. DrawAimHint: P0 = ZoogiAimArrow::Outline tip (Out[3]); line starts at the ring edge; the aim arrow is redrawn over the line.
- Grok v14 (C:\dev\art\ancient_meadows\grok_design\blender\arena, arena_report_v14.txt): Scripts/arenas/ancient/import_v14.py (runner ios/v14_import_run.py) -> /Game/Env/AncientV14/SM_Arena_* (16 pieces, FBX materials; complex-as-simple collision on all but Water / Nature / GatePortal).
  ZoogiArenaAncientV14.cpp BuildAncientV14 (default; -ZoogiAncientV2 = old arena): pieces at y -1600 (plaza centre on the origin), spawns r950, 14-orb plaza ring r1150 + 12 island orbs, golden-hour light. Rules layout ZoogiAncient::MakeV14 (ZoogiAncientLayout.h + generated ZoogiAncientV14Data.inl): plaza r1720, volcano platform, 4 octagon islands, 4x16 ramp segments, volcano ramp, front stairs, viaduct. No scoring portals / recovery islets in v14 yet. Showcase poses v14_front/plaza/left/top/player.
- Front menu (ZoogiFront.cpp): safe inset (portrait 5% sides / 4% top / 3.5% bottom; landscape 4 / 3.5 / 3%), hit-tests offset; Aurex robot only on INVENTORY (hidden on home); menu camera poses re-aimed at the v14 diorama.

## Nov 1 b56 UPLOADED 19:32 / b57 UPLOADED 20:14 (UBT_RC=0, EXPORT_UPLOAD_RC=0)
- b57: AZoogiLightning (Public/ZoogiLightning.h) - crit hit (ZoogiAwaken EmitAnime Critical): player-colour bolts from the shooter + blue bolts from the hit orb and up to 3 orbs near the hit; waits out an anime cut-in. Proof crit_shatter/16-17_crit_lightning_*.png.
- Shatter: UZoogiScreenFx::Shatter now only fires as the transition into an anime cut-in movie (ZoogiAnimeSubsystem, non-Infuse clips). Failed battle challenges (BattleStateMachine::Shatter, e.g. Strike first_contact != declared target) show "<KIND> FAILED / FIRST HIT WAS AN ORB - NEEDED P2" without the crack; rocks / callouts no longer crack the screen.
- Strike (boosted hit) timing tap: no REACTION button, no TAP! ring (TapButtonMode Mode 1 -> 0). BLOCK (Mode 2) + parry unchanged.
- Orbs: restitution 0.6 -> 0.8 + playful pop on Zoogi/orb/wolf hits (hop + +-14 deg fan + spin; -ZoogiNoPlayfulOrbs).
- AZoogiFX::bMuted during a power clash (Spawn / Niagara / Stylized / KFX / Shockwave). Proof clash_ult clash shots.
- Portrait home per Evan's mock (PaintTopBarPortrait + PaintHomePortrait). Proof front_port/02_front_03_home.png.
- NEXT (Evan, 19:58): clash loser "backstroke" flight in its form -> smoke wipe -> per-MAP anime crash clip (9 clips, tinted to the loser); bring back the Aurex transformation before the hit (visible).

## Nov 1 ~21:45 — b58 (Claude): Aurex parry guard, edge hang, backstroke, REACTION back
- NEW ZoogiAurexPuppet.h/.cpp: posed form puppet (UPoseableMeshComponent, aim-based limb solve; works on Aurex UE4 rig + Mixamo Tripo forms; real-time ticking). Poses Guard / Hang / Climb / Drop / Cheer / Backstroke / Stand; scale-pop + spin transform, palette MID, pop FX kept at real speed in slow-mo.
- Parry / BLOCK slow cut (FunSys UpdateReact) + Wolf parry focus: victim transforms into its Aurex in a guard, reverts at contact (-ZoogiNoReactAurex).
- NEW ZoogiEdgeHang.h/.cpp: human Zoogi dropping off the edge (not blasted >7 m) grabs the lip as its Aurex; world 0.03x; ledge camera (traced candidates); tap REACTION / anywhere (PULL!, mode 5) to fill the bar in 3.5 s; success climbs back (-2 SP; off-turn saves are charged on the next Standby via FZoogiCardRules::AddEnergyDebt); fail = lets go -> normal ring-out. Turn can't resolve mid-hang (AllAtRest / settle hooks). Tests -ZoogiHangTest / -ZoogiHangAll; -ZoogiNoEdgeHang.
- Clash loser backstroke: StartFlung hands the Tripo form to a puppet lying back, head first, arms windmilling (-ZoogiNoBackstroke).
- Strike REACTION (extra hit) restored (Evan 20:28) + TAP! ring; REACTION button now on the right edge, taps anywhere still count (Evan 20:32).
- Proofs: C:\dev\proofs\p5_style\hang_test (b58_hang_sheet*.jpg), clash_ult\b58_backstroke_sheet.jpg, hang_test\b58_guard_sheet.jpg.
- Build 58 chain: ios\chainb58.py -> b58ios.py (SOURCES_UP 21:43).
- NOTE: Grok (ZOOGI VP Game Dev) also edits this project (its skill -> crit -> shatter chain). Asked it to check before touching ActionFun / StrikeFun / EdgeHang / AurexPuppet / ClassicPad / camera files.

## Nov 1 ~21:50 — b59 in progress (Claude)
- Evan 21:30: realistic sounds for everything; ARC launch camera 160 deg back->front; anime fight slices exactly where the streak cut-ins show. 21:34: ask Grok to make the anime scenes in Buzzy; use Suno for sounds.
- Grok briefed (Grok Bot app): 9 per-map crash clips (Anime_Crash_<Map>_01) + 8 fight slices (Anime_Fight_Slice_NN) -> C:\dev\art\anime\buzzy\{crash,fight}\, DONE.txt when finished.
- Suno blocked: subscription renewal failed ("update billing ... to keep creating"). Evan must fix billing.
- Sounds: PlaySfx realistic map (ZoogiTypes.cpp) -> Lyra waves copied to /Game/Audio/Sounds/{Impacts,SFX,UI,Explosions,Foley_Misc} (90 waves, 15 MB; unused moved to C:\dev\_to_delete\lyra_unused_b59) + modal-synth SW_* in /Game/Audio/Sounds/Real (Scripts/synth_real_sfx.py, import_real_sfx.py, ios\real_sfx_import_run.py). Orb-orb clinks, OrbClink for Zoogi->orb, real rolling loop, Aurex/hang/arc sound hooks. -ZoogiSynthSfx = old set.
- Fight slices: AnimeSubsystem plays a random Anime_Fight_Slice_NN on every streak-only cut-in (no clip / short repeats; not Infuse / Miss); 6 interim slices cut from Evan's Story_EP1 + crit/KO clips (Movies.csv rows). -ZoogiNoFightSlices.
- NEW ZoogiArcCam.h/.cpp: ARC launch flight camera sweeps 160 deg from behind to the front while closing in (-ZoogiNoArcCam; shots arccam_N with -ZoogiArcTest).
- Build 58 uploaded 21:48; Build 59 uploaded 22:21 (sounds, ARC cam, interim fight slices).

## Nov 1 ~22:40 — b60 (Claude): Grok's Buzzy anime wired in
- Grok delivered C:\dev\art\anime\buzzy (DONE.txt 22:23): crash clips Meadow, FrozenRing, VolcanicPit, CosmicPlatform (_01,_02), PharaohsTomb, NightCircuit (_01,_02), AncientMeadows; 8 fight slices (0.9 s). Contact sheet: art\anime\buzzy\contact.jpg. Grok is remaking Arabian Nights, Training and Ancient (wrong scenes / no backwards flight) with its last ~305 Buzzy credits.
- Copied into Content/Anime/Movies (+ Movies.csv rows); interim slices moved to C:\dev\_to_delete\interim_fight_slices_b59.
- AnimeSubsystem: PlayCrash / CrashClipFor / DrawCrash - smoke wipe in (0.35 s, holds until the first clip frame), full-screen aspect-filled clip tinted 16% to the loser, smoky edges, smoke out (0.45 s); world 0.03x meanwhile; cut-ins blocked; IsBusy covers it. Arena -> clip by filename keywords (random variant, never twice). Arabian / Training / Teaser: no clip yet -> no crash.
- StrikeFun TickFlung: 0.5 s into the backstroke -> PlayCrash (-ZoogiNoCrashClip; test -ZoogiCrashTestClip=<id>). Proof: proofs\p5_style\crash_test\b59_crash2.jpg; fight slices volc_vfx\b60_slices.jpg.
- Fight slices: the HUD keeps the streak card until the clip has a frame (no black card); slice length waits out decoder warm-up.
- Build 60 uploaded ~22:48.

## Nov 1 ~23:50 — b61 (Claude): Grok's remade crash clips -> every map has a crash
- Grok redo (DONE.txt 22:51): Anime_Crash_ArabianNights_01, Training_01, Training_02 (old fight_A was really a dummy crash), AncientMeadows_01 (now flies backwards into the rune gate). Old Ancient kept only in art\ as _02_weak (not imported). Contact: art\anime\buzzy\contact_redo.jpg.
- Fight slices re-cut by Grok from the one real fight clip: 6 x 0.625 s (01-06); old 01-08 moved to C:\dev\_to_delete\old_buzzy_b60. Slice length clamp now 0.6-1.6 s.
- Movies.csv rebuilt for Anime_Fight_Slice_* / Anime_Crash_*. All 10 arenas resolve (Teaser -> Meadow). Proof: proofs\p5_style\crash_test\b61_arabian.jpg (arena 4).
- Grok is generating the second fight clip (fire vs lightning, 72 of its last 89 Buzzy credits) -> Anime_Fight_Slice_07+ in art\anime\buzzy\fight\ (import next build).
- Build 61: ios\chainb61.py -> b61ios.py.
- Build 61 uploaded 00:02 Nov 2 (EXPORT_UPLOAD_RC=0; the local b61ios poller stalled after the upload - confirmed with ios\chk61.py -> chk61.txt).
- Grok's second fight clip (fire vs lightning) sliced into Anime_Fight_Slice_07-10; copied into Content/Anime/Movies + Movies.csv (10 slices total). Not in a TestFlight build yet - rides on Build 62.

## Nov 2 ~06:50 — b62 (Claude): Evan's 05:58 batch
- Stand gap: AZoogiArena::WidenStandGap (+650 cm, -ZoogiStandGap=) on Arabian + Meadow — scenery/pieces whose centre is past the court rim move out, scatter between cleared (log "ARENA b62 stand gap"). Meadow terrace pushed out to match. Volcanic: no stand collider (no rebound). Proof map_meadow/arena_meadow_0_buzzy_top.png.
- Meadow pillars / crate benches / inner crates off (-ZoogiMeadowProps brings them back).
- Classic uses the Tripo forms (puppet + clones); bought Aurex / Micro Werewolf only in Battle Royale (-ZoogiAurexClassic, -ZoogiBoughtWolvesClassic). Blue auto-morph only with -ZoogiClassicMorph (the "arms showing").
- Physics: after an orb contact the Zoogi is damped (Z <= 60, 2D <= 0.85x pre-contact) -> no fly-away (-ZoogiOrbBounceOld). Orb pop only from Zoogi/wolf hits, no speed gain; a high-impact hit (>5000) nudges up to 3 nearest slow orbs within 320 cm.
- Clash: no SUPERPOWER / ultimate while a clash is on; after the clash cut-scene (+ flung + crash clip) the turn resolves -> next turn (ForceResolveNow; -ZoogiClashKeepRolling).
- Red star bumpers removed everywhere (props, saved layouts, Teaser pads, StageShift; -ZoogiStarProps).
- Power intros: UZoogiAnimeSubsystem::PlayPowerIntro plays Content/Anime/Movies/Anime_Awaken_<WolfPack|MeteorStrike>_01.mp4 (15 s, first use per player per match, tap to skip after 1 s). No-op until Grok's clips land (Buzzy needs ~720 credits top-up).
- Smart cam (PlayerController action cam): frames the shooter + up to 3 fast movers, zooms out fast / in slow (-ZoogiNoSmartCam).
- Portrait: opening "THE ARENA FORMS" fits 86% width; front menu on tall phones (>1.9:1) gets a deeper top/bottom inset (Dynamic Island / home bar) and the home groups spread over the extra height, sizes unchanged. Proofs front_iphone, classic_iphone (592x1280).
- Fight slices 07-10 (fire vs lightning) ride along.
- Blocked on Evan: Suno billing renewal failed; Buzzy credits for the two 15 s intros.
- Arabian: the palace stands come from the imported pack (PlaceIncoming, after Build) -> placements past the court line move out 650 cm too (log "Incoming pack b62 stand gap: 89 placements"). Proof map_arabian/arena_arabian_2_aerial.png.
- Clash -> next turn now waits until the crash clip / anime is idle for 0.25 s (log "FUN clash scene finished ... -> resolve" after "Anime crash ... done"). Proof crash_test log.
- b62 chain started 07:26 (run_anc.bat -> gob62.py): BUILD_RC=0, captures, COOK_RC=0 07:34.
- b62 UPLOADED 07:43 (UBT_RC=0, EXPORT_UPLOAD_RC=0).
- NEXT (Evan 08:29-08:39): Fab packs MeteorRain + StylizedToonVFX3 copied into Content/ (from E:\UnrealProjects\ZoogiFabAssets). Plan awaiting his yes: inventory CARDS tab -> SKILLS loadout (5 slots, tap select / tap again unselect), in-match SKILL list = selected skills only, 11 new skills (area / line / tap / multi-tap / while rolling / self), TRAINING -> map select -> solo classic, no bots.

## Nov 2 ~11:15 — b63 (Claude): Fab skills, SKILLS loadout, solo Training, standing forms, P5 banners
- Packs copied from E:\UnrealProjects\ZoogiFabAssets: Content/StylizedToonVFX3 (Hovl) + Content/MeteorRain (Gabriel Aguiar); AlwaysCook Effects/Audio/Particles, NeverCook their Maps (DefaultGame.ini).
- NEW ZoogiFabFX.h/.cpp: AZoogiFabFX::Play(path, loc, scale, real life, follow) - real speed in slow-mo, logs the measured footprint; StopAfter cuts a late stage.
- Parry / BLOCK + Wolf parry: Hovl wood-shield dome over the defender only (scale 0.2, stops at 0.9 s; -ZoogiParryShieldScale=, -ZoogiOldParryFx). Proof parry_land/*parry_shield*.
- Frozen stage shift (halftime): the 4 rising spots get the Hovl ice wall eruption, effect only - no collider / prop (ZoogiStageShift.cpp; -ZoogiShiftIceScale=). Proof shift_frozen/*shift_ice*.
- P5 corner banners: every FunSys callout + the ultimate banner = slanted P5 plate top-left; Callout(..., Speaker, Line) adds that Zoogi's speech bubble (face + line) at the top (ult ready / ult aim / ult fire / awakened / blocked / perfect hit). -ZoogiOldBanners = centred bands.
- NEW ZoogiStandForms: classic matches start with every Zoogi standing as its Tripo ultimate form; back to the ball on its own launch or when knocked (-ZoogiNoStandForms).
- NEW ZoogiSkills (11 skills, Cards.csv rows, art Data/CardArt/<id>.png from the pack thumbnails): meteor_rain / fire_meteors / energy_shockwave = area circle, ice_wall = swiped line (BlockAll box while the ice stands), water_cannon / golden_apple = tap, magic_rays = up to 3 taps, apple_bombs = 3 spots (mines), sword_slash / energy_buff / wood_shield = self. All aimed INSIDE the ARC range ring (G->ArcRange()); world 0.05x while aiming; FIRE = normal card commit, CANCEL = nothing spent. -ZoogiSkillTest casts all; -ZoogiSkillScale=id:x tunes sizes.
- Inventory: CARDS tab -> SKILLS = the hand loadout (DeckSize 5): tap adds to a slot (badge number), tap again frees it, HAND n/5 counter; the match hand is exactly that loadout (no top-up; -ZoogiRefillDeck = old).
- TRAINING: mode card -> map select -> AZoogiGameMode::ClassicStartTraining(): you alone on that map (no bots), orbs refill, no match end (-ZoogiOldTraining = the royale sandbox). Capture flag -ZoogiSoloTrain. Proof skills_land.
- Toolkit card backend keeps csv-only rows (skills) instead of falling back to native.
- b63 UPLOADED 11:27 (UBT_RC=0, EXPORT_UPLOAD_RC=0).

## Nov 2 ~11:50 — b64 (Claude)
- NEW ZoogiIceBarrier.h/.cpp (Evan 11:34): solid ice walls - SM_Icewall spires (M_Ice3) rise with NS_Ice_wall, BoxComponent BlockAll wall; ball/orb hits >260 cm/s add crack stages (glowing crack streaks on both faces, chips), stage 3 = burst into physics ice blocks (SM_HOVLCrystalPiese, ignore Pawn/PhysicsBody) that shrink away; bursts on its own after N rounds. Used by the Frozen stage shift (4 x 3 m walls, more clearance) and the ICE WALL skill.
- P5 banners moved to the bottom-right, sliding in from the right (Evan 11:38).
- Standing start form turns back into the ball the moment the human touches the aim (Evan 11:44).
- b64 UPLOADED 11:59 (UBT_RC=0, EXPORT_UPLOAD_RC=0); banners sit at 0.57 H landscape / 0.66 H portrait (clear of SUPERPOWER).

## Nov 2 ~14:15 — playtest + b65 (Claude)
- Playtest report (Claude review + Grok's review of 224 b64 captures): Doc "Zoogi Roll playtest" https://claude.ai/artifact/9cmAkcms5PMsJMwgEHtVEP ; Grok's raw file C:\dev\art\playtest\grok_feedback.md. Top asks: one message at a time / points first, CPU turns <= 8 s, first-match tutorial, front-end polish (shop blank cards, Tasks/News wiring, DEV button, "Preparing..." text), phone readability (Frozen contrast, pillar occlusion, reduce flashes).
- NEW ZoogiVortexRings.h/.cpp (Evan): Frozen's upright ice rings suck in a ball/orb flying within 2 m and fire it out of another random ring toward the centre (1150 cm/s, 2 s cooldown). -ZoogiVortexTest / -ZoogiNoVortexRings. Log "VORTEX ... sucked into ring 1 -> out of ring 3". Proof shift_frozen/*vortex*.
- Anime cut-in clips now play inside a big triangle sliding in from a side edge, tip at screen centre, sides alternate (ZoogiHUD DrawAnime; -ZoogiAnimeCard = old top card). Proof classic_iphone/35_anime_cutin1_Combo.
- Inventory SKILLS: tiles use full-bleed icons (Data/CardArt/Icons/<id>.png); tap = select/deselect + opens the details panel where the robot stands; tap the panel to close; tab change closes popups (likely cause of "kicks me out" = old item popup's tap-outside close). b65b: portrait panel art smaller, description full width.
- Grok's awaken clips WolfPack / MeteorStrike / BoltBalls in Content/Anime/Movies (+ Movies.csv); StaticShock+Bolt ult plays the BoltBalls intro.
- b65 chain started 14:14 (run_anc.bat -> gob65.py -> chainb65: front_iphone,wolf_front captures, then b65ios).
- 14:18 + 14:44-14:48: Mac (TX544.macincloud.com, 216.39.74.244) refuses SSH ("Connection closed by ... port 22", rc 255) on every try -> b65 NOT uploaded. Cook + stage are done (Saved\StagedBuilds\IOS). When the Mac answers again: run_anc.bat -> gob65up.py -> b65up.py (upload stage, sources, build3.sh 65, poll).
- 15:40 Evan: cancel the weekly Mac (TX544), move to the monthly one: TX242.macincloud.com, user user303094 (password Evan only).
  Prepared: C:\dev\ios\tx242.py (run_anc.bat -> gotx242.py): tests both Macs, TX242 rsyncs ~/zr (certs, profile, API key, fin.sh) +
  ~/UE5 mirror + Epic BuildRules straight from TX544 with a temp key (deleted after), rewrites /Users/user294974 paths, then chains
  b65up242.py (stage upload + build3.sh 65 + TestFlight). Status: ios\tx242_status.txt, ios\b65_242_status.txt.
  build3.sh now uses $HOME (old copy build3_pre242.sh). Needs first: C:\dev\mac_add_key.txt pasted into Terminal on TX242 by Evan.
  Cancel TX544 only AFTER MIGRATE_DONE (it holds the only copy of ~/zr/dist.p12 + all.p12). Later: DefaultEngine.ini RemoteServerName/RSyncUsername -> TX242/user303094; new bNN scripts -> TX242.
- 15:48 Evan: keep the old weekly Mac (TX544), drop the new monthly one (TX242) - Evan cancels TX242 in the portal (Claude has no portal sign-in).
  15:49-15:52 retry (run_anc -> gob65up -> b65up.py): TX544 still "Connection closed" before auth on all 4 tries, while TX242 answered
  normally at 15:44 -> TX544-side block (suspended after past-due, or sshd/IP block). Next: restart TX544 from the portal (refresh icon), then run_anc.bat.
- 15:56 Evan: use TX242 (monthly) for now; NO plan cancelled (Claude stopped at the cancel form). 15:55 reboot request for TX544 submitted in the portal.
  TX242 needs a fresh setup (TX544 unreachable): C:\dev\ios\setup242.py (run_anc.bat -> gosetup242.py) uploads ~/zr from PC copies
  (dist.p12 = combined Intermediate\Remote\...\Shipping\Certificate.p12, profile, API key, fin.sh, build3.sh, AppIcon), BuildRules dll,
  points DefaultEngine.ini (both projects; .tx544 backups) at TX242/user303094, waits for unreal.lock, backs up combined\Binaries
  (C:\dev\backups\combined_Binaries_tx242), UBT remote build IOS Shipping (engine mirror upload ~16 GB, log ios\build242.log), restores Binaries,
  then chains b65up242.py. Status ios\tx242_status.txt. Blocked on: our key on TX242 (Evan logs in via web RDP in the Claude browser pane; Claude pastes mac_add_key.txt in Terminal).
- 16:15 local git backup: ZoogiRollUnreal-p5style feat/p5-style commit dbb93f3e (b63-b65 + all untracked work, ~1.5 GB new files), working tree clean. Not pushed. .gitignore: + Content/StylizedToonVFX3/, Content/MeteorRain/ (licensed Fab packs), /Samples/ (recursive PixelStreaming2 copy, paths too long).
- MacInCloud billing note (Evan): login time counts from login to logout -> always log off TX242 from the Apple menu after a GUI session.
- 16:40 Evan: stop TX242, back to TX544. back544.py killed the TX242 UBT remote build (engine upload was running), restored DefaultEngine.ini (TX544) + Apple_SDK.json (26.9.0) + combined Binaries, removed unreal.lock. TX242 logged off. TX544 SSH WORKS again 16:40:55 (after the 15:55 reboot request) -> b65up.py uploading the stage (UPSTAGE_START 16:40:55).
- b65 UPLOADED 16:51 on TX544 (UBT_RC=0, EXPORT_UPLOAD_RC=0): vortex rings, triangle anime cut-ins, inventory skill icons + details panel (b65b portrait layout), BoltBalls awaken intro. run_anc.bat -> goback544.py (do not rerun; point it at the next gobNN).
- 16:58 TX242 Monthly plan CANCELED in the portal (Evan: "close and delete tx242") - expires at the end of the billing cycle. TX544 weekly stays. C:\dev\TX242.rdp + ios\setup242.py/tx242.py/b65up242.py are now unused.

## Nov 2 ~17:50 — Build 66 plan (Claude) — Evan's "movie" turn redesign
Evan 17:02 "All" (5 playtest items) then 17:04 "slow down gameplay I want this to be a movie" -> faster CPU turns DROPPED. Spec (17:48):
- Guided first turn = this flow. Launch always hits as tapped+boosted (the post-launch boost reaction is gone).
- Roll camera 65-90 deg top view (see destruction trails + smash hits).
- While the floor crosshair shows: tap REACTION -> pause -> pick a target inside the arc radius -> ball smashes that way; 1 crystal
  (circles under the profile avatar) per redirect; crystals accumulate over time; more crystals = more redirects = combo pattern.
- Redirect into an opposing player -> anime cutscene (<= 30 s): RPG stance (turn player lower-left near, opponent mid/top far);
  action options (ATTACK/SKILL/GUARD/ITEM) unless REACTION already fired the skill; skill -> animation + reaction timer/button,
  tap on time = skill add-on; 5 s phone vibration; response anime. Loser clash anime: hard 5 s vibration ("smashing against a wall").
- Wolf Pack = a SKILL only (extended target selection for a boosted combo); Meteor Strike clip -> one meteor skill; Bolt Balls skill
  uses the old electric effect (unity pack) + the BoltBalls clip. New awaken clips play out in full. CPU gets the same, must earn crystals.
- Done so far: ZoogiHaptics::Rumble (iOS CoreHaptics, Build.cs frameworks CoreHaptics+AudioToolbox), shop portrait card cap,
  front test step order (Tasks/News were a capture offset, not mis-wired), screen messages always hidden, "YOUR TURN Â·" mojibake fixed.
- b66 built (18:2x-18:4x, several rounds). Implemented:
  * NEW ZoogiRedirect.h/.cpp: crystals = AwakenCharge pips, +1 at each of that player's turn starts; REACTION (pad tap mode 6 "REACT!"/REDIRECT, or a tap
    anywhere) while the human ball rolls -> world 0.05x, range ring (max(800, ArcRange)), glowing targets (red = rival) -> tap -> ball smashes there
    (1 crystal; Wolf Pack free picks), homing 0.9 s, COMBO xN banner; rival target arms the DUEL; Bolt Balls zaps 3 nearest on launch + each redirect;
    CPU spends earned crystals (<=2/turn, prefers rivals). -ZoogiRedirectTest / -ZoogiNoRedirect.
  * NEW ZoogiDuel.cpp (UZoogiFunSys::StartDuel/TickDuel/DrawDuel, FClash.bDuel...): intro (REACTION here = straight to SKILL) -> ATTACK/SKILL/GUARD/ITEM
    menu (CPU picks) -> move + timing ring (REACTION on time = ADD-ON, 5 s rumble) -> rival response (anime cut-in) -> normal finisher / loser flung + crash
    clip (5 s rumble). RPG camera (near fighter lower left, rival far) + letterbox. -ZoogiDuelTest / -ZoogiNoDuel.
  * Auto boost: human first hit always PERFECT (+35%), strike TAP ring + REACTION prompt gone (-ZoogiOldBoostTap = old); BLOCK stays.
  * Wolf Pack = SKILL only (wolf_pack card: intro + 3 free redirects); WolfPack ultimates -> Chain Ricochet, WolfPack superpower off (-ZoogiWolfSuperpower = old);
    bolt_balls skill (intro + electric zaps); meteor_rain plays the Meteor Strike intro; ultimates no longer play the 15 s intros (-ZoogiUltIntros);
    Anime_Awaken_* intros not skippable (-ZoogiIntroSkip). Card art Data/CardArt/{wolf_pack,bolt_balls}.png (+Icons) from Grok's clip frames.
  * NEW ZoogiGuide.h/.cpp: guided first turn (first classic match, [ZoogiRoll.Controls] GuideDone, -ZoogiGuideTest/-ZoogiNoGuide): 4 crystals, P5 menu +
    mode bar hidden, cards 1-5 (ghost-hand pull, boosted, REACTION arrow + 0.3x slow-mo, chain, duel).
  * ZoogiHaptics::Rumble (iOS CoreHaptics). Roll camera 65-88 deg (default 72, -ZoogiRollPitch=, -ZoogiOldRollCam); redirect pick forces top view.
  * Playtest fixes: ROUND 1 banner, FINAL ROUNDS never in round 1, popups max 3 / no dupes / points 1.45x / dark outline, REDUCE FLASHES setting
    (flash 0.45 default), shop portrait cards capped, screen messages hidden, "YOUR TURN ·" fix, P5 command menu clamped to fit portrait,
    front test step order (Tasks/News were a capture offset).
  * Unity-build collisions after the file additions: renamed anon helpers (DevConfig S->DevStoreS, GloveCrane/HoloSpire/AwakenFX/CrackFX/SpawnElevator Ease,
    EaseOut, FrozenRoam Pol, WolfPack/Awaken Pos, CamConfig Scope, ArcCam/EdgeHang/ArenaAncient S -> GArcS/GHangS/GAncS).
  * Proofs: proofs\p5_style\turn_iphone, turn_land (p5run runs turn_iphone / turn_land).
- b66 chain started 18:5x (run_anc.bat -> gob66.py -> chainb66: editor build, b66ios on TX544).
- b66 UPLOADED 19:11 on TX544 (Mac compile fixes: PI macro clash in ZoogiHaptics, CoreHaptics loaded at run time via dlopen + NSClassFromString because build3.sh keeps cached UBT rules so the Build.cs framework never links).

## Nov 2 ~19:30-20:45 — Build 67 (Claude)
Evan's b67 asks (19:2x-20:29): full-screen anime, guide option on the first story mission, skills inventory must not exit on tap,
30 s target-pick freeze, brighter Awaken arena glow, P5 banter (AI talk to each other), skill CANCEL -> back to the skill list, classic
clash unchanged (duel only from a redirect into a rival), profiles stay visible (tap toggles), CPU plays like a real person (menus,
selections, mini games), no edge-hang mini game (animation only, always falls = KO, no zoom), transform sound bug (no transform before a
hit, stand again quietly at rest, transform sound only on that player's turn), clash loser into the stands = crack + star-limbed splat,
portrait scene cameras pull back.
- ZoogiHUD DrawAnime: full-screen aspect-fill clip (-ZoogiAnimeCard = old card); portrait = whole clip across the width (x1.18) over a
  dimmed fill (-ZoogiAnimePortraitFill = plain fill).
- ZoogiFront: menu rebuild after SaveProfile used to recreate SZoogiFront on HOME -> static FKeep (screen/tab/skill detail) restored < 2 s.
- 30 s freeze: Skills pick / Redirect pick / ult aim dilate 0.0001 with countdowns; timeouts cancel.
- Awaken glow: unbound APostProcessVolume (prio 50) blended by AwakenGlowK while anyone is awake / standby awaken.
- NEW ZoogiBanter.h/.cpp: two-line P5 exchanges (taunts, KO, big score, miss, duel, awaken, combo, clash result), 5 s gap.
- Guide: ZoogiGuide::Arm() from the story's first stop (map panel toggle "PLAY GUIDE: YES/NO", default YES until GuideDone); classic no longer auto-runs it.
- ZoogiP5Cmd::BackToSkills(): skill pick CANCEL reopens the menu on the SKILL list.
- ZoogiArenaHUDWidget: profiles never auto-minimize; avatar tap toggles (-ZoogiHudAutoMini = old peek).
- ZoogiEdgeHang: visual only by default (grab, slip 0.9 s, fall = normal ring-out KO; no taps/SP/camera/slow-mo; CPU too). -ZoogiEdgeHangGame = b58 mini game.
- CPU as a person (-ZoogiCpuInstant = old): ZoogiP5Cmd::CpuHold (from TickStages Battle Aim) opens the command menu for the CPU, scripted cursor
  SKILL -> committed cards -> ATTACK, then committed Fab skills go through ZoogiSkills::CpuCast (cursor glides to targets, FIRE, Execute).
  ZoogiRedirect: CPU redirects pause + cursor to the target (1.0-1.6 s). ZoogiDuel: CPU cursor over the menu, CPU attacker/defender timing taps
  (spread by tier), ring + grades shown for CPU too.
- Transform: react guard / wolf parry Aurex puppets off (-ZoogiReactAurex = old); ZoogiStandForms re-stands any Zoogi at rest 0.6 s quietly,
  knocked = quiet dismiss, AurexIn sound once at the turn player's turn start (-ZoogiStandOnce = match start only).
- Splat: TickFlung traces for a wall ahead (|N.z|<0.7) -> AZoogiCrackFX rotated onto the wall + EZoogiPuppetPose::Splat puppet (star limbs) +
  close-up cam 1.35 s, then back to play; crash clip skipped when the flight heads into the stands. -ZoogiNoSplat.
- Portrait: ZoogiPortrait::SceneFit() (AdaptFov + pull back) on the clash/duel cam, flung/splat cam, ZoogiReactCam::Frame, replay cam.
- Scripts: ios\gob67test.py (turn_iphone, turn_land, front_iphone, story_port), b67ios.py / chainb67.py / gob67.py cloned from b66.
- 20:37-20:41 Evan: duel COUNTER (rival's PERFECT guard tap -> its anime (Critical) -> classic tap clash; -ZoogiNoDuelCounter); anime: "keep what we had"
  = the b65 triangle again (-ZoogiAnimeFull = the b67 full screen + portrait band); cut-in clips play the whole clip looped to >= 15 s and cannot be
  skipped (-ZoogiAnimeShortClips = old 1.5-3 s; crash clips not skippable either); "build everything now and deploy" -> run_anc.bat -> ios\gob67.py
  (waits for the capture pass DONE, then chainb67 = editor build + b67ios on TX544). Next builds: keep shipping continuously (b68 ...).
- 21:02 b67 UPLOADED to TestFlight (UBT_RC=0, EXPORT_UPLOAD_RC=0). Android tablet package started 20:58 (ios\androidrun.py -> Scripts\package_android.ps1 -Install; tablet R8YL5170YXL was 'unauthorized' at 20:52 - Evan must allow USB debugging).
- 21:08 b68 = ZoogiClashSplit (NEW .h/.cpp): classic clash shows the two Zoogis' awakening clips (element Infuse clip else Anime_Any_Any_Infuse / Anime_Awaken_*), split by a jagged crackling seam at the tug (LRatio), winner sweeps to full screen on the finish; -ZoogiNoClashSplit. run_anc -> ios\gob68go.py (waits for the Android lock, p5run clash_ult,turn_iphone, then b68ios).
- 21:25 b69 (pending): HoloSpire up only at rounds 1/3/5/8 until that round's first launch, shrinks away otherwise (-ZoogiSpireAlways). Android APK built 21:08 (C:\Games\ZoogiRoll.apk); install failed - tablet unauthorized; ios\adbinstall.py installs + screenshots once allowed.
- 21:41 b68 UPLOADED (after b68fix.py: ZoogiActionFun.h upload had been cut off by a connection reset -> UHT EOF error; upload now retries + verifies size).
- 21:49 b69 sources up (spire rounds 1/3/5/8, SP on profiles, landscape aim TanV fix).
- b70 (21:50, ios\patch_b70.py + patch_b70b.py + P5 pause): skill videos = Anime_Awaken_* compilations are 4 x 3.77 s clips -> one random clip
  (never the last), world 0.0001 under PowerIntroOn(), skill effect runs from UZoogiAnimeSubsystem::AfterCrash when the clip ends
  (-ZoogiPowerIntroFull / -ZoogiPowerIntroOnce); redirect hit feel (Rumble 0.16 s, OnImpactFeel 0.9, PlaySfxShort RockCrack 0.3 s, faster 1.25x);
  REPLAY CAM toggle in Settings (off by default, -ZoogiReplayCam); stance only for the turn player at its turn start (-ZoogiStandAll);
  SKILL plate while the human aims; replays dismiss stand forms; P5 pause menu (BuildPauseP5, -ZoogiOldPause). run_anc -> ios\gob70go.py.
- Android tablet = Galaxy Tab A11 SM-X133 (Android 16, MediaTek mt6789, 800x1340). 21:36 APK installed + launched (ios\adbinstall.py, proofs\android\tab_*.png).
- 22:09 b70 sources up -> Mac build. b71 (22:15): patch_b71.py = Content/Data/FxTune.csv (System,Size,Height,Duration,TintR/G/B; AZoogiFabFX::Play
  applies it, re-reads on change, -ZoogiNoFxTune) + Grok Bot's Fab-demo comparison (water hit x2 size x1.3 tall from the floor 3 s, water cannon
  x1.5 at ball height, meteor rain x1.68 / life x1.33, meteor hits x1.6, magic ray x2, rays attack x1.29, energy buff x1.67, sword AoE/crater
  x1.43, apple bombs x1.5, ice wall clamp 0.3-0.6 + height 220 + burst 0.25, wood shield 0.3 / 1.4 s, duel crater 0.3) + AZoogiHUD draws
  nothing while paused (P5 pause was covered by the ult-aim card). run_anc -> ios\gob71go.py.
- 22:30 b71 sources up (stage upload slow: 469 s). b72 (22:35): anime cut-ins short again (-ZoogiAnimeLong = 15 s rule) and every clip plays as the
  edge comic panels (bPanelMode for all movies; -ZoogiAnimeCentreCard = card); ElecFlicker() in ZoogiRedirect: bolt zaps 5-6 flickers over 1.2-1.4 s
  and a 4-flicker electric burst on every redirect hit. run_anc -> ios\gob72go.py.
- 22:55 b72 UPLOADED to TestFlight (UBT_RC=0, EXPORT_UPLOAD_RC=0). Local commit e9d401fb (b68-b72). Builds 67-72 all uploaded tonight; run_anc.bat -> gogitbackup.py (repoint before reuse).
- 23:08 b73 by ANOTHER Claude session (skills stay available until launch: skill list returns after every skill + SKILL plate while aiming) - left to it.
- b74 (23:30, this session): NEW ZoogiCelebPip.h/.cpp - the anime 3D close-up no longer takes the main camera (bAnimeCam off); a SceneCapture2D
  (512x288, SCS_FinalColorLDR) orbits the focus and the HUD draws it in a small P5-framed window (-ZoogiAnimeFullCam = old). P5 command menu
  opacity follows G->HumanDragFrac (fades with the launch pull, back when the drag returns to the cancel zone). run_anc -> ios\gob74go.py.
- b75 (23:45): stand forms OFF by default (-ZoogiStandForms = old), CPU marbles show their portrait icon above them (ActionFun canvas; -ZoogiNoCpuIcons). run_anc -> ios\gob75go.py (b74ios/b75ios now refuse a second run via *.started markers).
- 23:57 b75 sources up (first b75ios try failed: the 74->75 sed had renamed the Mac user to user294975 - fixed; build scripts are now made with targeted replaces).
- b76 (00:02): cards stay in the hand after use and can be replayed while SP lasts (FZoogiCardCommitted.bKept, no once-per-turn check,
  no discard; -ZoogiCardsConsume = old); P5 menu + SKILL button hide while anime clips / skill effects (ZoogiSkills::EffectBusy 1.6 s, 4 s with
  pending events) / clash / redirect pick / edge hang play; SKILL button opens the skill list. run_anc -> ios\gob76go.py.
- b76 (00:16 sources up) also carries: P5 skill list sticky per turn (GS.SkillsTurn; BACK clears) + SKILL button beside the Zoogi in landscape;
  splat on wall corners (3-way sphere sweeps + a real bounce off a wall counts); SUPERPOWER pad in landscape at 76% / 80% of the screen (clear of
  REACTION; -ZoogiOldPowerPad); drag hint (TOUCH ANYWHERE / DRAG BACK) only until the human's first launch of the match (-ZoogiAimHintAlways).
  Local check build 00:19 BUILD_RC=0. OPEN QUESTION: Evan says the classic clash still shows the 3D models (split-screen clips not seen on device) - not yet diagnosed.
- 00:24 b76 UPLOADED to TestFlight (EXPORT_UPLOAD_RC=0). Builds 73 (other session), 74, 75, 76 uploaded after 23:00.

## b77 (Nov 3, 00:48, Claude) - building
- WOLF button: casting wolf_pack banks a summon (x count badge) and shows a WOLF button in the mode bar's 2nd slot (Slate, ZoogiClassicPad SZoogiWolfButton). Tap while rolling = old Wolf Pack summon (AZoogiGameMode::FireWolfSkill -> FireInstantPower Wolf branch, no charge spent; pack counter heads come back). CPU taps 0.75 s into its roll. -ZoogiWolfSkillRedirects = b66 skill.
- Mode bar = AUTO / [WOLF] / ARC (CONTROL, DRIVE, LAUNCH gone); AUTO = DRIVE force for the human (PowerStepVelocity). -ZoogiOldModes restores.
- All anime at the top: cut-ins = top card (-ZoogiAnimePanels side windows, -ZoogiAnimeTriangle, -ZoogiAnimeFull); crash + skill-summon clips drop into the top card, no smoke (-ZoogiCrashFull).
- Arabian Nights: pizza floor dressing off (-ZoogiPizza).
- HoloSpire fade hides every component too. Evan asked about a "white clear pole" - asked for a screenshot.
- P5 menu captures its touches (InputBlocked() -> AZoogiHUD::IsOverUI) so taps don't move camera / reset aim.
- CPU avatar icons moved to the main HUD canvas (drawn first, under menus/text), hidden in the top band.
- Open: Evan's "left over arms around players and p5 notes after launch" - asked what they are.
- b77d (Evan 00:57): WOLF button dropped (-ZoogiWolfButton restores). Banked Wolf Pack = SUPERPOWER's next press while rolling (ZoogiClassicPad Press -> ZoogiSkills::WolfGo), purple wolf badge xN on SUPERPOWER, label WOLF PACK!; then SUPERPOWER = ultimate as before. Applied after the Win64 compile, before the Mac source upload (Mac compile is its first check).
- b77 uploaded to TestFlight ~01:11.
## b78 (01:15) comments below profile band (banter Y 0.25/0.27, P5 bubble + target tag); wolf heads (procedural Tex "wolfhead") under profile pips = ZoogiSkills::WolfCount.
## b79 (01:21) pause: FunSys/WolfPack DrawHud, ClassicPad, P5Cmd hidden while paused; cog -> P5 MENU plate (-ZoogiOldChrome), pause rows CAMERA + DEV MODE; ZoogiArenaHUD::OverChrome in IsOverUI; sky taps ignored (FloorPoint fails, no target under finger); FIRE/self skill -> ZoogiP5Cmd::ToSkillButton(); Golden Apple Eden on caster + wolf-style blast at orb (-ZoogiOldGoldenApple); centre props (r 330*scale) lose collision + fade with spire (-ZoogiKeepCentre).
- b78 uploaded ~01:29. b79 includes b79b (landscape AimPitch 24 / AimScreenY 0.47) + b79c (P5 corner banners left side, -ZoogiBannersRight; P5 notes aiming only, -ZoogiNotesRolling) - those two were applied after the Win64 compile, before the Mac source upload.
- b79 uploaded ~01:43.
## b80 (01:47) Bolt Balls (ZoogiSkills ArmBolts/BoltGo/BoltTick: 3 straight missiles via SUPERPOWER while rolling, ElectricShock stems per bolt left, wolf-size blast; CPU fires through its roll; -ZoogiOldBoltBalls). KriptoFX Unity effect not ported - Unreal ElectricShock used. Awaken lasts 3 more turns of that player (-ZoogiAwakenTurns=N). ULTIMATE READY banner under the SUPER meter (-ZoogiUltNote = old note). b80d: post-clash fail crash clip stays full screen, only Anime_Awaken_ summons use the top card (applied after Win64 compile).
- b80 uploaded ~02:04 (incl. SUPERPOWER gold charges badge only with -ZoogiWolfSuperpower; profile wolf heads tinted transparent unless banked). Local commit 1d093bc6 (b73-b80), not pushed.
- Open: Evan's "left over arms around players" after launch (asked what it is); "white clear pole" screenshot (asked).
## b81 (Nov 3 07:37) purple lightning stems (ZoogiSkills MakeStem/StrikeStems, -ZoogiBoltSparkStems old sparks), clash split ult art per side (+ Anime_Zoogi_<Name>_Clash_01 lookup, -ZoogiClashClipsOnly), splat: ArenaPiece -> SetPieceState(Cracked) else ZoogiWallCracks generated slivers (-ZoogiOldSplatCrack), MENU straight + always visible + PressMenuAt from P5Cmd overlay, steady clash rumble (-ZoogiNoClashBuzz), Frozen launch rail removed (-ZoogiMapRail), stage-shift ice walls persist + grow 25%/round (-ZoogiIceWallsFixed).
- b81 uploaded to TestFlight 07:55.
## b82 (09:42) impact frame 0.5 s (-ZoogiImpactFrameSec=), yellow ElectricShock sparks back on top of purple arcs (-ZoogiNoBoltSparks), tap buzz via Slate input preprocessor in ZoogiClassicPad (-ZoogiNoTapBuzz).
- b82 uploaded to TestFlight 10:00.
## b83 (10:10) GUARD barrier (ZoogiSkills GuardArm/GuardTick/GuardHit; once per turn, up from turn end to next turn, mass x40 + damping 8, cracks per PERFECT 1 / CRITICAL 1.5, shatters at 6; -ZoogiOldGuard). KO ring-out blast = VFX_PackVol1 Big_Explosion_v2 (purple) + Mid_Explosion_v2 rays (-ZoogiOldKOBlast) - applied after the Win64 compile.
- b83 uploaded to TestFlight 10:30.
## b84 (11:05) Bolt Balls yellow ElectricShock only flashes on FIRE (cast) and on each bolt shot; constant sparks only with -ZoogiBoltSparks. gob84 waits for the android packaging lock. Grok asked to check b83 tablet screenshots -> art/notes_for_claude/b83_visual_check.txt
- b84 changed (11:30): yellow sparks reverted to constant (Evan likes them); b84 = GUARD dome 2x (GuardR), floor-seated, FxMID 0.9, NS_Ice_wall burst. Android packaging stopped (stopandroid.py), lock moved to ios/unreal.lock.android_stopped.
## b84 uploaded 11:56. b85 (11:58): no gameplay pause under anime (AnimeSubsystem Freeze only with -ZoogiAnimeFreeze; crash/skill-summon dilation only with -ZoogiAnimeSlow).
- b86 changes folded into b85 (12:12, after the Win64 compile started): bolt clear on new match/menu/GameOver (BoltClearAll), bolts lost on KO, bolt flicker haptics (-ZoogiNoBoltBuzz), bolt badge top-right of SUPERPOWER (Icon 2 lightning), skills fire with the clip (AfterCrash immediate unless -ZoogiSkillAfterClip).
## b85 restarted 13:04 (gob85 never launched b85ios). b86 (13:14): spire orb refill each round (SpireRefillOrbs + ZoogiSpireOrbs emit arcs, spire forced up while emitting; -ZoogiNoSpireOrbs), all powers unlocked (-ZoogiPowerLocks), precision homing release 2.2 s / 110 cm.
- b85 failed on the Mac (-Wshadow: BoltTick local LastTurn vs namespace LastTurn) -> renamed BcLastRound/BcLastTurn; b86 carries all of b85. Precision statics renamed PrecT/PrecTurn. b86 Win64 BUILD_RC=0 13:19, cook + upload to Mac OK 13:26.
## b86 uploaded to TestFlight 13:34 (EXPORT_UPLOAD_RC=0).
## b87 (13:40) Evan 13:26-13:31:
- Wolf Pack: SUPERPOWER spends every banked wolf (-ZoogiWolfOnePerPress = one per press); paint up to N targets (PaintMax = banked), pack = max(3+, banked) wolves; a rival's live pack is cleared first (UZoogiWolfPackSys::ForceClear) - it used to swallow the summon (bank spent, no wolves); own pack out -> SUPERPOWER recalls. Self-test -ZoogiWolfBankTest.
- Water Cannon kick = LaunchSpeedForPull(1) = LAUNCH speed (-ZoogiOldWaterCannon).
- GUARD: unbreakable, Zoogi pinned to its spot while the dome is up, dome = solid bumper (reflect + 650 out) for Zoogis/orbs/wolves (-ZoogiGuardBreakable = b83/b84 cracking dome).
- Spire "orb generators": 0-2 purple SP orbs per round (MakePower purple, ZoogiSpireOrbs::MarkSP); first Zoogi to touch one gets +2 SP (FZoogiCardRules::AddEnergyBonus -> next Standby or now if open). -ZoogiNoSPOrbs, -ZoogiSPOrbTest.
- Cosmic: Evan's rune marker (SourceArt/Props/CosmicMarker -> /Game/Env/CosmicMarker/SM_CosmicMarker + M_CosmicMarker, Scripts/import_cosmic_marker.py) rises from the floor each new round (ZoogiCosmicMarkers.cpp, 3 m, capsule body, stays all match). -ZoogiNoCosmicMarkers.
- p5run run 'b87_check' (Cosmic, 2 players, 4 rounds, wolf bank + SP orb tests).
- b87 Win64 OK 13:46 (b87_check: wolves banked x3 -> paint up to 3 -> pack out; 2 Cosmic markers visible r2/r3), Mac UBT_RC=0, uploaded to TestFlight 14:04.
## b88 uploaded 14:56. b88 = Wolf Pack 3 SP banks 3 wolves (Cards.csv cost 3; -ZoogiWolfOne); hologram purple Micro Werewolves in classic too (ZoogiClone HoloWolves, explode on impact with their target, re-target when it's gone; -ZoogiOldWolfClones); Energy Shockwave = 2 pulls + blast 2x wolf (-ZoogiOldShockwave); character clips play in the avatar circle: skill summons (AnimeSubsystem::AviClip + ArenaHUDWidget::SyncAviClip swaps the avatar brush to MovieTexture), combo/char cut-ins and comic slices (ComicSliceOverlay::AviSlice), profiles no longer split away for videos (-ZoogiAnimeTop / -ZoogiSliceBubbles / -ZoogiWolfClipTop restore).
## b89 (15:00): 1 awaken crystal per player per turn (CPU 4-card commit filled all 4; -ZoogiPipPerCard); SUPER meter gains halved and capped 25/event (-ZoogiEasyMeter); no launch ship on Cosmic (-ZoogiCosmicShip); critical clip plays on the HoloSpire scoreboard drum, spire pops back for it, Blue's P5 crit cut-in replaced by the clip (ZoogiCritSpire, -ZoogiCritTop); Cosmic marker floor = traced court floor (was the turn ball's Z - could be parked under the pad -> buried) + first marker 4 s into round 1; Water Cannon = DRIVE speed (LaunchSpeedForPull(DriveFrac) ~14 m/s).
## b89 uploaded 15:18.
## b90 (source ready 15:45, NOT BUILT - Explorer blocked by an "Unidentified app" window; run_anc.bat -> gob90go.py):
- Duel v2 (ZoogiDuelV2.cpp) replaces the tap-tug clash and the b66 duel: circle match (GOOD +3 SP / PERFECT +5 SP, miss = normal hit) -> ATTACK 2 SP 25 dmg / SKILL 3 SP 35 dmg / GUARD / ITEM -> rival spends its SP: DEFEND 2 (half) / COUNTER 2 (20 back) / BOTH 4 / TAKE IT -> HP (FPlayerFun::HP 100, per match); 0 HP = KNOCKOUT (HpKnockout: blasted off, back to 100). -ZoogiOldDuel / -ZoogiOldClash.
- Unspent Card Energy no longer zeroed at EndTurn (-ZoogiSpZeroOffTurn) so rivals have SP in a duel.
- b90b (written by ANOTHER Claude session working on the same folder at the same time!): WOOD SHIELD ends the turn, stays until the caster's next turn (-ZoogiOldWoodShield). b90e (same other session): orbs knocked off during aiming by a skill (Water Cannon) score for the caster at once.
- b90c: critical hits take 10 HP (UZoogiFunSys::HpDamage, not while GUARD is up; -ZoogiNoCritHp); profile SP line shows HP.
- b90d: anime clips / comic slices play in a square that springs out of that player's profile corner (ArenaHUDWidget::SyncAviClip ClipImg/ClipFrame), not inside the avatar circle.
- NEWS popup = tester checklist (ZoogiFront PaintNews).
- b90f/g (16:12-16:15): METEOR STRIKE ultimate adds a meteor rain over the arena (8 meteors, each ZoogiSkills::WolfBlast x1.2; -ZoogiNoUltMeteors); METEOR RAIN skill = one big blue electric meteor diving like a missile (blue electric trail, sonic boom ring, impact shockwave + yellow ElectricShock flash 0.3 s + WolfBlast x1; -ZoogiOldMeteorRain).
## b90 uploaded 16:38 (duel v2 seen working in turn_iphone: circle PERFECT +5 -> SKILL -> rival answer -> HP 58). b91 (16:40): Momentum Steal / Crystal Collapse / Double Impact swapped out of every deck (FZoogiCardRules::Init; -ZoogiCardsAll).
## b91 uploaded 17:00 (card locks). b92 uploaded 17:22: Meteor Rain = void meteor (purple portal disc + spinning ring, Inferno SM_RockBig_001 boulder in purple cylinder-arc lightning, ground shatters into physics rock shards, WolfBlast; portal lowered to 560 cm so the aim camera sees it).
## b93 (17:20): Settings > WATCH CPU PLAY (ZoogiControls::WatchCPU, key WatchCPU) - StartMatch makes every MenuPlayer a CPU, P1 back to human when off; settings rows compacted (RowH 62) so all rows fit.
- run_anc.bat now logs ios/runanc_hb.txt; double-click the ICON (x~601) - clicking the name of the selected file opens rename.
## b94 (17:36): duel ending = loser flight (StartFlung) + crash clip (verified in turn_iphone); DEFEND -> DuelV2GuardForm (14 shards slam into a dome, flash, shock ring, GuardBlock cut-in; not yet seen in a test); NEWS = TEST row + the newest build + the 3 before it (keep rolling this each build).
## b95 uploaded ~18:18 (first upstage failed: the b94->b95 '94'->'95' replace broke the Mac user user294974 -> fixed; WATCH OUT when generating bNNios: replace only build-number tokens). b95 = wolves latch on target until called (6 s max), Meteor Rain portal x2.7 higher/further + meteor cam + opaque void disc, watchdogs (clash 40 s, flung 8 s, rolling 20 s, CPU menu 12 s, CPU skill pick 10 s), duel_soak run (no stall in 9 CPU duel turns). Rivals have 0 SP in duels (CPUs spend all 5 on their turn) - proposed a 2 SP duel reserve; proposed one-clip-at-a-time full-screen duel endings (awaiting Evan).

## b96 (Nov 3 18:44) - TestFlight uploaded (EXPORT_UPLOAD_RC=0)
Duel finales (one full-screen clip per ending), 2 SP duel reserve, portrait NEWS full screen.

## b97 (Nov 3, prepared; launch = double-click C:\dev\run_anc.bat -> ios/gob97go.py)
- b96b guard: NS_Wood_shield plays once, paused closed at 0.9 s (FShield.Fx/CastAt/bFrozen, DropShieldFx on every removal); a hit = a flash, no replay.
- b96c Settings > TRAINING BOTS (key TrainBots 0..3, ZoogiControls::TrainBots): classic TRAINING = you + N CPU; Royale training starts with N bots.
- b96d Wolf Pack: skill level per player per match (Lv1 1 wolf -> Lv3 3, +1 per cast; -ZoogiWolfNoLevels); banked summon = exactly the banked count;
  human pick = paused (dil 0.0005) top-down BirdCam framing the knockoff circle, 10 s, must pick min(banked, targets) (time out = auto-fill),
  all targets ringed, ExtraLife += dist/1750 (<=2.5 s). -ZoogiWolfOldPaint = the old slow-mo paint.
- b97a Meteor Rain: real KriptoFX Effect5 (NS_KFX_Effect5 + AZoogiKFXExtras "Effect5") rotated/scaled so prefab portal (0,0,190) -> impact (-500,0,0)
  maps Portal -> Hit; KFX SM_Rock + MI_KFX_Effect5_MeshRock2mat boulder; S_PortalOut / S_RockExplosion / S_RockImpact. -ZoogiMeteorNoKFX = b95 look.
  Test run skills_meteor (-ZoogiSkillTest -ZoogiSkillOnly=meteor_rain) -> proofs/p5_style/skills_meteor/only_meteor_rain_*.png. VERIFY these frames vs the demo.
- NEWS: B97 row added, B93 dropped. Cards.csv wolf_pack text = levels + pick.

## b97 (Nov 3 19:26) - TestFlight uploaded
NOTE: shipped with the PAUSED top-down wolf pick (sources went up before Evan's change). KriptoFX Effect5 meteor is now OPT-IN (-ZoogiMeteorKFX):
the first capture (proofs/p5_style/skills_meteor) was black then all purple - the meteor cam sits inside the scaled-up (x3.5) prefab meshes.
TODO: fix KFX meteor (smaller scale / keep cam outside / drop the extras meshes) before turning it back on.
gob97 chain died after p5run (b97ios never started); ran it via gob97b.py.

## b98 (Nov 3, building) - Wolf pick back to slow-mo 0.2 (Evan: "go back to how we had it but at a more angle view and more time")
BirdCam = 55 deg three-quarter view from behind the player's yaw, Dd = 0.82*R/tan; 12 s; must pick min(banked, targets); NEWS B98 row, B94 dropped.

## b98 (19:38) uploaded. b99 (building) - duel camera fix (Evan: "doesn't return to normal gameplay after the duel clip until my turn")
Root cause: EndFighters handed the view back to Clash.PrevView (the cam live when the duel began - often a temp cam, gone/frozen);
the only CAMWATCH ran on the HUMAN aim step. Fix: EndFighters -> AZoogiPlayerController::RestoreMatchCam(0); FunSys::LastDuelEndAt;
PC watchdog also on CPU aim steps + 30 s after a duel unless SpecialCamOn / wolf pack / anime / skill effect / redirect pick / cinematic.
(Named RestoreMatchCam: a stale ZoogiGameMode.cpp.obj held an inline RestoreMatchView -> LNK2005.)

## b99 (20:02) uploaded - duel camera fix.
## b100 (building 20:10) - ZOOGIS = Awaken collection (Evan 19:51)
- Inventory tabs: BALLERS (robots, tab 0) / ZOOGIS (balls, tab 1). Zoogi detail = "AWAKEN: <power>" + description; equip says the power.
- ZoogiFront::AwakenForBall / AwakenFor(B): Hotstreak METEOR STRIKE (ult Explosion), Vex VOID PACK (wolf_pack x3 -> WolfGo), Blue TIDAL CANNON,
  Frosti GLACIER WALL, Lumi SOLAR BOLTS, Terra IRONWOOD DOME, Zephyr CYCLONE PULSE, Ember HEARTBREAKER (meteor_rain). Human = equipped ball,
  CPU = its roster id's hero ball; roster Zoogis outside the 8 (e.g. Tavi) keep their old signature ultimate. -ZoogiOldUltimates.
- TryUltimate -> ZoogiSkills::AwakenCast (full strength, auto-aim, no SP). ZoogiSkills GSkillPow: card 0.6 (-ZoogiSkillFullPower), awaken 1;
  scales Kick/WolfBlast/shockwave/bolt pushes + card wolves (BlastMul/HitMul). Card blasts: Mid_Explosion x0.8 instead of Big_Explosion_v2.
- Verified: clash_ult "AWAKEN power P1: water_cannon at full strength"; skills_land 13 casts at x0.60, no crash.

## b102 (21:14) uploaded - frozen-view watchdog (PC: any phase, view target static 2 s + no special owner -> match camera).
## b103 HELD (b103ios.started = "held for b104" - written 21:18 by another Claude session?) -> shipped as b104.
## b104 (building 21:34) - field duels + stuck-camera ROOT CAUSE + epic hits / debris
- ROOT CAUSE of "camera stuck until my turn": TickClash calls TickFighters after TickDuel in the frame the duel ends; Clash was
  reset, so TickFighters spawned a NEW duel camera + took the view; nothing ended it. Fix: TickFighters returns when !Clash.bOn.
- Field duels (Evan): classic = Clash.bField (no SpawnFighter / HideFxAround / duel cam / split; FX not muted; strike streaks between
  balls); story missions (ZoogiStory::InMission) keep the scene; -ZoogiDuelScene = scene in classic.
- ZoogiEpic: cinematic hits (EpicImpact/EpicBig/EpicRiser, "don't take the war drum literally"), physics DebrisBurst (cap 36 chunks),
  Debris/RockSmash/Rumble layers in the PlaySfx map. Assets: Scripts/synth_epic_sfx.py -> SourceArt/Audio/Epic -> import_epic_sfx.py
  (allowlist: Epic*, Debris, RockSmash, Rumble) -> /Game/Audio/Sounds/Epic. The literal WarDon/WarLoop wavs stay unimported.
- Film (paused): ZoogiFilm recorder + film_run.py scenes (proofs/film), editor ios/film/film_edit.py + plan.py. Game Bar route
  approved (UnrealEditor + Game Bar access, Captures folder connected); add -ZoogiFilmNoFrames runs when resuming.

## b104 uploaded; Android APK installed on the Galaxy Tab (22:39) for Grok's playtest (brief: art/notes_for_grok/b104_android_playtest.md).
## b105 uploaded - Water Cannon back to build 87 (full launch force, exempt from the 0.6 card scale; -ZoogiWaterCannonDrive = b100).
## b106 (23:06) uploaded - classic defend window (ZoogiDefendWin.cpp): shot about to hit a rival -> slow-mo, side-on two-marble cam,
   only the rival may DEFEND (2 SP), pure physics hit. Timeout runs on game time (GameT0/GameLimit), not real time.
## b107 (building) - SKILL LAB + duel ring rework + random CPU skills
- SKILL LAB (b107a): card skill_lab in Cards.csv/Defs ("in Zoogi disguise as a skill"). ZoogiSkillLab.h/.cpp: rows SKILL/STRENGTH/
  RADIUS/PULL/FX SIZE/COLOUR/BLAST/CAMERA, AUTO/PLAY/RESET/COPY/SAVE; Saved/SkillLab.ini (id=str,rad,pull,size,color,blast); Evan
  bakes tunings in later (COPY puts them on the clipboard). Hooks: Execute strength, Blast/WolfBlast/shockwave radius, pull, FabFX size/tint.
- Defend window b107b (Evan 23:00): stage 0 timing ring on the turn marble (CPU taps with spread), GOOD/PERFECT -> 0.004 dilation +
  0.7 s B&W (camera PP saturation 0) + DIRECT (x1.25 / x1.45 at the rival) / AROUND (bends to nearest other rival/orb, ends window);
  rival GUARD button (2 SP) until contact -> DuelV2GuardForm dome + mass x8; contact -> loser (rival, or attacker off a guard)
  SpawnFighter+StartFlung (form flies back, splat on walls), main camera. Flags: -ZoogiDefendNoRing, -ZoogiDefendNoFlung.
- Random CPU skills b107c (Evan 23:01): CardSubsystem gives each CPU its own DeckSize random distinct skills per match (seeded from
  GameSeed + index, no two CPUs equal); card tests / -ZoogiForceCard keep the starter; -ZoogiCpuStarterDeck = old.
- Pipeline note: the b106 Android package started while the b107 patch was being written -> its UBT read the OLD ZoogiActionFun.h
  and produced objs newer than the header -> later builds skipped them -> editor crashed in UZoogiFunSys::Deinitialize (layout mismatch).
  Fix: touched ZoogiActionFun.h / ZoogiSkills.h / ZoogiSkillLab.h. Never patch Source while any UBT run is live.
- Chain: gob107b waits for the Android package (23:26), then p5run duel_soak,skills_land, then b107ios.
## b107 (23:48) uploaded. PC soak verified: CPU random hands distinct per CPU; DEFEND ring CPU taps (MISS / GOOD), GUARD, DIRECT x1.25,
   "loser P3 flies back in its form"; skills_land 14 casts incl. SKILL LAB open + auto replays. Fixes on the way: bSame -> bDupHand
   (C4456), SkillLab k++ unsequenced (Android clang -Werror). NEWS: B107 row added (B102 dropped). Android package (gob107c) running 23:49.
## Grok b105 tablet report (art/notes_for_claude/b104_android_playtest.md): field duel PASS; targeted skills "cancel" after orb taps with
   0/3 picked - likely Grok's slow adb steps hitting the 30 s pick timeout + the fixed 110 px tap radius (small on the tablet).
   TODO b108: scale the skill tap radius with the viewport (~7% of height); show the timeout countdown on the pick.
## b108 (07:17) uploaded - SKILL LAB = black home button (not a skill): Cards.csv row + Defs entry gone, saved decks filter skill_lab;
   UZoogiMenuRoot::StartSkillLab -> RequestLab + ClassicStartTraining (0 bots); lab hides the caster (no collision/physics),
   ZoogiSkills::ClearField on PLAY / RESET / skill switch; swipe orbits the camera (yaw/pitch, presets set them); match HUD gated off
   (ZoogiHUD, ClassicButtonsOn, P5Command, ArenaHUD). Skill tap radius max(110, 11% of the short side). p5run run 'skill_lab'.
## b109 (queued via gob109 after the Android package): lab bottom row [RESET][<][NAME][>][PLAY] (b108's overlapped), 18-orb rack,
   BattleHUD Episode Energy panel hidden in the lab.
## b110 (08:06) uploaded - lab taps on every arena (HandleInput's Frozen-bridge return skipped while the lab is open), lab spot
   between the centre and the spawn (FindSpot), presets relative to it, Frozen/Ancient -> Meadow from the button, tap log line.
## b111 (08:26) uploaded - ROOT CAUSE "selected skills don't reach gameplay": FZoogiCardRules::Init validated every deck at exactly
   DeckSize (5); an inventory hand of 1-4 skills failed -> the starter deck. Now 1..DeckSize is legal. -ZoogiTestHand=a,b,c test hook
   (FParse::Value with bStopOnSeparator=false); p5run 'hand_test' -> "human deck = test hand (3)", no "deck invalid".
## b112 (08:43) uploaded - lab reach marks: floor ring at the farthest orb + dashed half-circle arc to the highest, REACH/HEIGHT labels.
## b113 (building) - rack spaced 1+6+11 at 125 cm (the 70 cm ring overlapped -> orbs burst apart by themselves), orbs re-racked at
   resting Z, ring hot pink (arena lines are gold), thicker arc, REACH label on the camera side.
## b113 (09:19) uploaded. b114 (12:4x) uploaded - Wolf Pack Energy Buff glow (caster at release + every wolf; visual only, -ZoogiWolfNoEnergy).
## b115 uploaded - Energy Buff costs 0 and gives +1 SP crystal (AddEnergyNow + "+1 SP CRYSTAL" popup); lab wolf pack auto-targets
   (BeginSummon with the caster flagged CPU for the call) -> lab_11 frame shows the 3 wolves with energy spikes.
## b116 (building) - banners: FCallout.Band alternates top (portrait 20% / land 19%) / bottom (87.5% / 86%), never mid-field;
   ZoogiArenaHUD::NoteTopBanner drives the existing split (profiles slide out) while a top banner shows; speech bubbles at 6 edge
   spots (hash of the line), never centred. Wolf Pack cast plays the ENERGY BUFF skill effect on the caster (0.18, 5 s, its sound).
   duel_soak frames: COMBO x2 in the bottom band, HEAVY HIT BOOST in the top band with the left profiles sliding out.
## CRASH found by duel_soak (random CPU skills cast ice_wall more): AZoogiIceBarrier::Burst -> SetMaterial on a GC'd static
   cached UMaterialInterface (IceMat / ChunkMesh / IceMesh were raw static pointers). Fixed: AddToRoot on first load.
## b117 (14:13) uploaded + Android installed - missile wolves: 30 s pick at 0.05 dilation, purple orb missiles (AZoogiClone::SetMissile
   strips the micro-werewolf body; -ZoogiMissileWolves keeps it), straight from the Zoogi, smoke (Smoke_v2) + ember trail, blow on
   impact, alpha = third tap (-ZoogiWolfRun = old run). duel_soak clean (ice wall crash fix holds). BUG: they blew at age 0.04 -
   sibling missiles / launch brushes counted as impacts.
## b118 (queued) - missiles ignore sibling clones and anything in the first 0.12-0.15 s (except their own target); launch spread 80 cm.
## b119 (queued - waiting for run_anc.bat; an unidentified window in front blocks computer-use clicks):
   - Wolf Pack back to the b116 run (V, latch, recall) as purple orbs (AZoogiClone::MakeOrb, no micro werewolf) with the purple
     wolf-head icon over each (DrawWolfIcon in DrawHud; alpha = bigger + crown). Missiles only with -ZoogiWolfMissile. 30 s pick kept.
   - PRECISION card = the b117 missile volley: UZoogiCardSubsystem::Commit -> UZoogiWolfPackSys::BeginPrecision(G, B, 3)
     (bShotNext/bShot: PRECISION cut-in, no WolfPack intro, missiles, alpha = third, no wolf icons, no energy glow); the missile
     trail lost its Smoke_v2 puffs (ember glows stay). Cards.csv text updated. p5run 'precision_test' (-ZoogiForceCard=precision).
   - Energy Buff: StopAfter 1.2 s on every NS_Energy_buff play (one loop); the caster's second glow at the pack's release removed.
   - (16:15) Wolf Pack field summon: BeginSummon spawns the pack right away (bFieldPack) in a ring around the caster, parked
     (AZoogiClone::Hold / bWaiting), each with a purple-tinted Energy Buff glow (one loop); every picked target sends the closest
     parked wolf (Strike); Release sends the rest to the nearest open targets. -ZoogiWolfLateSpawn = spawn at release (b116).
   - (16:20) Wolf Pack: the WOLF button works aiming or rolling (ZoogiSkills::WolfReady); no slow-down for the pack (PRECISION
     keeps it); casting Wolf Pack summons the orbs onto the field now (UZoogiWolfPackSys::SummonField / FieldWolves, circling the
     player via TickField, physics off until Strike); unsent wolves stay with the player + stay banked; player KO = they vanish.
   - (16:24) Energy Buff "+1 crystal" = +1 Awaken charge pip (the circles under SP) via CardsAddAwakenCharge; cost back to 1 SP.
   - (16:23) Frozen open ring (ZoogiFrozen::OpenRing, -ZoogiFrozenClassicRing reverts): crevasse filled with a snow slope
     (Ring2_GapFill wedge hulls), no bridges / Ring 1 walls / rim lips / rim block clusters / Ring 2 curb, bank walls, crevasse lip;
     Frozen roam off by default with it. p5run 'frozen_open'.
   - (16:30) Defend window / timing ring OFF (TickDefendWindow returns unless -ZoogiDefendWindow; ClassicDefendMode stays true so
     classic still skips the duel scene). Knockouts: UZoogiAnimeSubsystem::Start plays the arena crash clip (CrashClipFor) on a KO
     event when >= 2 turns since the last one (LastKOClipTurn); -ZoogiKOClipOff = old first-KO-only cut-in.
   - (16:33) "Keep skills menu visible": P5 command menu no longer hides for skill effects / anime clips (bBusyHide keeps redirect
     pick, edge hang, clash), and a played skill returns to the open skill list (ToSkillButton -> BackToSkills). -ZoogiMenuHides = old.
   - (16:40) REVERTED the 16:33 skills-menu change (Evan: "forget what I said about the skills menu"): b79 fold + b76 busy-hide are
     default again; -ZoogiMenuStays = the b119 stay-open.
   - (16:42) "Skills always open this menu, not straight to the list": BackToSkills / the SKILL plate (OnMini) / the SkillsTurn re-show
     all land on the main ATTACK/SKILL/GUARD/ITEM menu (Mode 0). -ZoogiSkillListDirect = the b76 straight-to-list.
   - (16:42) Wider aim arrow: ZoogiAimArrow FTune ShaftWidthD 0.30 -> 0.55, HeadWidthD 0.70 -> 1.15 (ini [ZoogiRoll.AimArrow] still overrides).
   - (16:45) "Fix horizontal camera": landscape human aim step with a TargetLock fell to the soft tactical view (pivot lerped toward
     the target, shooter under AUTO/ARC). New bHumanLandAim in ZoogiPlayerController: human + IsAimStep + landscape -> the
     behind-the-Zoogi aim view regardless of Lock (like combo). -ZoogiAimLockTac = old. p5run: new 'hand_wide' (1792x828) run; gob119
     runs precision_test,frozen_open,hand_wide,skill_lab.
   - (16:47) "Bolt Balls can be used before launch": ZoogiSkills::BoltReady allows Aiming (Battle stage, not mid skill-pick) as well
     as Rolling, so SUPERPOWER fires a bolt before the launch; CPU still fires only while rolling. Callout text updated.
   - (16:51) b119 first build FAILED: ZoogiWolfPack.cpp used UNiagaraComponent::SetVariableLinearColor without #include "NiagaraComponent.h" -> added.
   - (17:00) "Fix bolt ball yellow electrics flash to how they were before the continuous flashes": constant yellow ElectricShock sparks
     OFF by default again (b84 11:05 look; -ZoogiBoltSparks = constant); one yellow flash on the cast (new b80 path) + one per bolt shot.
   - (17:21) b119 SHIPPED: Win64 BUILD_RC=0 (17:01), p5 precision_test / frozen_open / hand_wide / skill_lab clean (no Fatal; hand_wide
     iPhone-aspect aim frames show the Zoogi above AUTO/ARC every turn), TestFlight EXPORT_UPLOAD_RC=0 (17:17), Android PACKAGE_EXIT=0
     (C:\Games\ZoogiRoll.apk).

## Nov 4 b120 (Claude) - weapons, portrait menus, map pictures, button taps
- (17:26) Inventory portrait fills the screen: ZoogiFront PaintInventory - every tab's grid full width in portrait (design space 1700 wide),
  tiles up to 1000 (was 250 cap; b120a first tried 520 - still top-half), centred, down to the Zoogi tag; skill detail panel overlays the grid bottom.
- (17:28) Weapons from Tripo (grenade, sword, staff, disc = the Oct 30 relic zips, already imported as /Game/Relics: Lantern = Prism Grenade,
  Sword = Crystal Blade, Staff = Scepter, Emblem = Halo Ring). WEAPON tab: GATLING / PRISM GRENADE / CRYSTAL SWORD / CRYSTAL STAFF / HALO DISC,
  all owned, EQUIP! -> FZoogiFrontProfile::EquippedWeapon ("weapon" in the save). Tile art Content/Data/MenuArt/Weapons/w_<key>.png (Evan's
  buzzy_refs renders, background keyed). ZoogiRoyaleHero.cpp: equipped weapon replaces the Gatling in robot form (mesh in the right hand,
  -ZoogiWeapon=<key>, -ZoogiWeaponCycle for captures): grenade lob (arc, burst r430 on landing), sword dash + slash (r460 arc), staff heavy bolts
  (x2.2, 0.32 s; staff mesh flipped - its orb is at -Z), disc boomerang (out 0.6 s, back, hits once per pass). p5run run weapons_royale.
- (17:38) Impact frame (black & white manga frame) 0.5 s -> 0.2 s (ZoogiFrozenFun FrameSec, -ZoogiImpactFrameSec= still overrides).
- (17:50) Button taps never count as field taps: ZoogiClassicPad::PointerOwned() (pad press held / 0.35 s) counts as UI in AZoogiHUD::IsOverUI;
  Wolf paint drags only from a field press; skill / redirect picks ignore button presses (pad, P5 cmd, AUTO/ARC).
  Target tapped while aiming (hard TargetLock) + SUPERPOWER: Wolf Pack sends the closest field wolf at it at once (PickActor + Release,
  -ZoogiWolfNoLockFire = old); Bolt Balls fire at it.
- (17:26) Map select: new landscape cards from fresh 1920x1080 arena shots (old cards in C:\dev\backups\b120_mapcards);
  portrait MAP SELECT = the selected map fills the screen (ms_map_<key>_p.jpg, TexCover, zoom-settle + flash, list in the lower half)
  - portrait shots = p5run pmap_<arena> at 720x1280 (864x1536 doesn't fit this 1440-tall monitor -> fell back to 1920x1080).
- Phases: gob120a (build + front_port, weapons_royale, pmap 1080p), gob120b (17:50 fixes build + portrait pmap), gob120c (map select proofs,
  b120ios, android). run_anc.bat points at the current phase go-script.
- (18:32) COMBO (b120e): P5 skill list COMBO toggle -> tick skills (order badges, SP sum) -> CAST n! queues them (GS.ComboQueue, one per 0.12 s,
  a target pick holds the queue, CANCEL / BackToSkills drops it). -ZoogiNoCombo. Evan wants to discuss skill launches before more changes.
- b120 SHIPPED 18:55: phase C BUILD_RC=0, map_select_port shows the full-screen portrait map pictures, TestFlight EXPORT_UPLOAD_RC=0, Android PACKAGE_EXIT=0.
- (18:53) Pause orbit reworked (b120f): NOT pause menu / anime freeze (Evan: forget that). ZoogiPauseOrbit::Active = ZoogiRedirect::Picking() ||
  UZoogiFunSys::UltAimOn(); a drag (>18 px) turns the camera round the turn player's Zoogi (Apply in UpdateCamera), eases back ~0.7 s after.
  REDIRECT and the superpower bullet-time pick now choose on RELEASE (UltAimRelease; Redirect Input bRelease && !WasDrag).
- (18:54) GUARD = 10 s force field, up immediately (FGuardDome.UpAt, world time; last 2 s flicker); pinned + bumps everything, except during the
  guarding player's own roll (rides along, bumps rival Zoogis only). -ZoogiGuardTurnBarrier = the b83/b87 next-turn barrier.
- b121 queued: gob121.py (hand_wide, precision_test -> b121ios -> android); run_anc.bat -> gob121go.py.
- (19:19) COMBO v2 (b120g, Evan likes b120 combo): CAST n! ARMS the ticked skills - AZoogiGameMode::CardsArmCombo commits each card (SP spent,
  GZoogiComboArming skips the PRECISION volley hook), Fab skills + precision go to ZoogiSkills::ComboArm; classic launch cards apply at once.
  After the launch each SUPERPOWER tap = ZoogiSkills::ComboGo -> AutoCast(id) from the Zoogi's current spot (precision -> BeginPrecision);
  pad label "CAST 1/3!"; aiming press = "LAUNCH, THEN TAP SUPERPOWER TO CAST". Armed casts stay until used; new match clears.

- (19:23) REACTION / REDIRECT fix (b120h): pick auto-closes when no target is left in the arc range (no 30 s stall), tap window 0.20*short+80 px,
  pad press blocks field taps only 0.15 s after lift, orbit drag threshold 26 px, guide REACTION slow-mo only with a target in range.
- (19:45) SHOWTIME sequence (b122a, folded into b121): new ZoogiShowtime.h/.cpp - on the human's SHOWTIME: ZoogiHaptics::Rumble 2 s, Frozen rune seal
  (T_FrozenSealHalo on M_FrozenDecal) spinning under the Zoogi + faint arena-wide seal (Awaken element tint), directional/sky lights x0.3/0.4 + unbound
  PPV exposure -1.5, NS_Energy_buff on the Zoogi; after 1.2 s 'TAP YOUR TARGET' (PC routes taps to ZoogiShowtime::Tap -> G->LockOn hard; 12 s -> nearest),
  then lights back + seals fade 0.8 s. -ZoogiOldShowtime = off. A persistent build-queue watcher was proposed but blocked (needs Evan's OK).
- (20:02) b121 first compile failed (ZoogiShowtime used protected AZoogiGameMode::LiveTargets) -> public wrapper PublicLiveTargets; rebuilt BUILD_RC=0,
  precision_test + hand_wide clean (no Fatal) 20:04; b121ios + android running.
- (20:19) Evan: "Just focus on TestFlight" -> from b122 on the gob scripts stop after bNNios.py (no androidrun.py). b121 TestFlight EXPORT_UPLOAD_RC=0 20:16.
- (20:55) b122b: REDIRECT circles untappable on phones since b121 pick-on-release - the release frame has no touch position (Ptr fell back to the mouse).
  PlayerController now uses the last down position on the release frame (GPtrLastDown). ZoogiRedirect::CancelPick; pad Press: an open redirect pick
  closes on REACTION (nothing spent) or SUPERPOWER (then SUPERPOWER does its job: combo cast / wolf / bolt / ultimate). b122 TestFlight only (gob122).
- b122 TestFlight EXPORT_UPLOAD_RC=0 21:16.
- (21:16) b122c: COMBO casts that aim (tap/line/area/spots) open the normal pick mid-roll from the Zoogi's spot (S.bComboCast); FIRE = Execute (already paid),
  CANCEL / 30 s timeout = ZoogiSkills::ComboPutBack (front of the queue). Self skills still AutoCast. -ZoogiComboAutoAim = old auto aim. b123 queued (gob123, TestFlight only).
- (21:22) b122d: mid-roll picks keep the roll camera angle (Evan: don't switch to the turn-start aiming look): REDIRECT zoom-out to the range ring only with
  -ZoogiRedirectWide; the smart roll camera (ActWideYaw / ActSmartC / ActSmartR) holds while ZoogiSkills / ZoogiRedirect / ult-aim picks are open.

## Nov 4 21:57 - b124 (Claude): smarter CPU + player ice walls (TestFlight only)
- b123 on TestFlight 21:41 (EXPORT_UPLOAD_RC=0).
- Evan 21:44 "Fix cpu ai they should play like a person would and be smarter" -> new ZoogiCPUSmart.h/.cpp (-ZoogiCPULegacy = b123 CPU):
  - PlanCPU: skills scored for the moment (cannon only with a target in reach, shield/ice wall/guard when exposed, meteors on bunches,
    per-CPU temperament per match), best value per SP, max 3, nothing pointless (was: most expensive first, all SP every turn).
  - Aiming skills (meteor/fire/cannon/rays/slash/shockwave/apples/golden apple/ice wall) cast MID-ROLL from where the CPU is
    (P5Command GS.CpuRollCasts, when a target is in reach or the roll is ending) - same rule as the player's COMBO. Buffs pre-launch.
  - CpuCast targets: no teammates, rivals near their edge / the leader first, area skills on the biggest bunch, ice wall = backstop
    on the CPU's own nearest edge.
  - Shot pick: Normal tier now uses the lookahead pick (cands 3, spread 220) + -150 cost on the score leader.
  - REDIRECT: self-save when the roll would carry it off the edge; rivals 85%, orbs 40% (was a 55% coin flip).
- Evan 21:45 "ice walls built from players stay to the end of the match unless destroyed after three hits":
  - ice_wall skill: no round limit (-ZoogiIceWallOneRound = old); player walls take exactly 3 Zoogi hits, orbs bounce off
    (-ZoogiIceWallOldDamage). Crystal Collapse walls: 3 hits shatter the whole wall (-ZoogiIceWallSpeedShatter = old).
- Proof run cpu_smart (p5run, 4 CPUs Arena 4, log p5_cpu_smart.log). gob124 -> b124ios.

## Nov 4 22:14 - b124 on TestFlight 22:11; b125 building (Claude)
- Evan 22:03-22:08: drag-back camera zoom broken; "tap to target still doesn't work"; "fires when I release - go back to when it worked";
  REDIRECT "barely moves"; CPU wood shield + bolt balls overlaying (skills a player can't do).
- b125a (ios/patch_b125a.py): REDIRECT + superpower bullet-time picks on the PRESS again (-ZoogiPickOnRelease = b120); ult tap window
  0.20*min+80; REDIRECT kick 2200..3400 cm/s (-ZoogiRedirectSoft = b70) + "REDIRECT check" speed log 0.25 s later; P5 menu / pad
  stuck-press caps 1.5 s; CPU casts EVERY skill after launch (ZoogiCPUSmart::CastMidRoll = true), max one aura (shield/bolt/buff) a turn.
- Drag-back zoom: only the classic (cinematic) camera has the push-in (CineDrag, Oct 29 b19). Asked Evan which mode / orientation.
  cine_land run added to compare with b19c (dist 1150 -> 681, fov 84 -> 72).
- 22:30 b125 on TestFlight (EXPORT_UPLOAD_RC=0). Proofs: cine_land identical to b19c (classic push-in works); CPUs cast all skills
  mid-roll; REDIRECT check showed 2210 -> 381..947 cm/s within 0.25 s before reaching the target.
- 22:37 b126 started (ios/patch_b126a.py): REDIRECT keeps >= 90% of its launch speed until it reaches its target (homing 1.2 s,
  -ZoogiRedirectSoft = b70). Drag-back zoom still waiting on Evan (which mode / orientation).

## Nov 5 02:30 - b127 (Claude): NEWS updated + scrolling (Evan 02:24: "why isnt news updating and make more and scroll on page"; "update pc")
- b126 on TestFlight 22:49 (REDIRECT keeps speed: checks 1880-2200 cm/s after 0.25 s vs 381-947 in b125).
- ios/patch_b127a.py (ZoogiFront PaintNews): entries for every build b108..b126 (b120-b126 new), rows sized to their wrapped text
  (ZWrapLines; B119 used to overlap B115), card fills the screen, clipped list scrolls: drag/swipe (DragKind 3, flick inertia NewsVel),
  mouse wheel (OnMouseWheel), magenta scrollbar + "SCROLL FOR MORE". OpenPop(News) resets to the top.
- PC: the Desktop "Zoogi Roll.lnk" runs the editor build (ZoogiRoll_Latest.vbs), so this Win64 build updates the PC game. gob127 -> b127ios (TestFlight).
- 02:48 b127 on TestFlight (EXPORT_UPLOAD_RC=0).
## Nov 5 02:54 - b128 (Claude) building
- patch_b128a: SHOWTIME pick pulse no longer on orbs (rivals still pulse; -ZoogiShowtimeOrbGlow = old).
- patch_b128b: Bolt Balls aim = closest target in reach (rivals x0.7 distance; -ZoogiOldBoltAim = nearest rival first), shot starts at the
  Zoogi centre + swept path check (orbs touching the Zoogi were skipped), blast placed just in front of the hit target.
- patch_b128c (Evan "yes and yes"): ZoogiSkills::PowerLeft - bolts / banked wolves / own pack out must be used up before ComboReady;
  SUPERPOWER falls through to WolfGo / BoltGo / pack; casts left at roll end keep waiting. CPU roll casts wait on PowerLeft too.
  -ZoogiComboFirst = old order.
- 03:05 b128 on TestFlight (EXPORT_UPLOAD_RC=0).
## Nov 5 03:06 - b129 (Claude) building (wolf_test, front_port, cosmic_markers -> TestFlight)
- patch_b129a: Wolf Pack - pick every target first, the pack runs together on Release (closest waiting wolf per picked target);
  the aim-lock target is pick 1 and the pick stays open (was: strike + close, every press re-picked the same lock).
  -ZoogiWolfStrikeOnPick = b119/b120. NEWS portrait text column x0.9.
- patch_b129b: recalled wolves ignore ECC_Pawn (never shove their caster) and their pop pushes no orbs. -ZoogiOldRecall.
- patch_b129c: Cosmic rune markers purple (SurfaceMID + emissive), no Smoke_v2, purple rise ring, first one at round 2, each round
  the standing ones grow x1.2 (cap 2x) one after another in rise order, then the new one rises. -ZoogiOldCosmicMarkers.
- 03:22 b129 on TestFlight. Grok note: art/notes_for_grok/ep3_style_answer.md (EP3 = robot-fox CG like EP1/EP2, heroes =
  art/refs/heroes/hero_1..5, new distinct rival -> art/refs/rival_ep3). New grenade tile Content/Data/MenuArt/Weapons/w_grenade.png.
## Nov 5 03:33 - b130 (Claude): PRISM GRENADE pickup (ZoogiGrenadeItem.h/.cpp, patch_b130a) - from round 2 the generator drops one when
  none is on the court; first Zoogi to touch it holds it; ITEM -> PRISM GRENADE -> ZoogiSkills::BeginGrenade (energy_shockwave Area pick,
  Range = 2 x ArcRange, dotted throw arc, title PRISM GRENADE) -> FIRE = ZoogiGrenade::Lob -> GrenadeBoom (Execute energy_shockwave at the
  landing). CPUs throw at a crowd (>= 3 weight) on their aim step. -ZoogiNoGrenadeDrops. Deeper purple Cosmic markers.
## b131 (queued after b130 upload): patch_b131a/b - smart roll camera frames only bodies over the court (ArR x1.1, Z < court+450),
  centre clamped to the court, Rad cap (-ZoogiSmartCamAll = old); SKILL plate -> note stack; SUPER/ULTIMATE label inside its bar;
  corner banner sub-line width x1.15 + left clamp; REDIRECT combo counter Ht 0.22 (0.2 portrait); skill pick hint plate x1.15 and
  "IS TARGETING" under the hint; ScoringUI callouts hidden during picks; notes hide while a bottom-band banner shows; grenade 110 cm + ring pulse.
- 04:05 b131 on TestFlight (EXPORT_UPLOAD_RC=0).
## Nov 5 04:3x - b132 (Claude): Grok delivered EP3 (art/notes_for_claude/ep3_storm_season_DONE.txt, rival TEMPEST at art/refs/rival_ep3).
  Story_EP3_Full + P1..P4 copied into Content/Anime/Movies; Dialogue.csv film rows: s3_storm P1 (after the narrator), P2 (before the fight),
  s3_freed P3 (after the Rune Seal line), P4 (after "Heh. Worth it."); Chapters.csv ep3 film = Story_EP3_Full. + patch_b132a (top banner).
  p5run story_ep3 (-ZoogiStoryMission=m3_tavi).
- b132 proofs: story_ep3 played Story_EP3_P3/P4 (0 Fatal).
## b133 (pending): patch_b133a - SHOWTIME plays Anime_Showtime_<PowerKey>_01.mp4 full screen (PlayCrash, smoke wipe) when present, seals/pick
  wait for it (Explosion=MeteorStrike, WolfPack, StaticShock=BoltBalls). -ZoogiNoShowtimeCut / -ZoogiShowtimeCut=<clip>.
  Grok asked (Grok Bot chat 06:5x) for art/notes_for_claude/showtime_movies/Anime_Showtime_MeteorStrike_01.mp4 (hero_4 blaze red, 8-12 s, 720p).
## Nov 5 07:13 - b133 (Claude): patch_b133a (SHOWTIME full-screen cutscene hook) + patch_b133b (TRAINING pause menu CPU OPPONENTS 0..3 ->
  MenuAction TrainBots restarts training; Rematch stays in training). Grok delivered Anime_Showtime_MeteorStrike_01.mp4 (10.0 s, 1280x720,
  Blaze hero, rune seal -> blood-orange sky -> meteor shower -> close-up; Buzzy 210 credits, ~119 left) -> Content/Anime/Movies + Movies.csv.
  Plays on SHOWTIME for a Zoogi with the Explosion power (Meteor Strike). Golden apple re-sequence: waiting for Evan's answers.

## Nov 5 07:25 - patch_b134b (golden apple) applied, ships in b134 with b134a
- golden_apple: Apple of Eden forms on the caster (0.7 s) -> SM_Apple lobbed in an arc (0.65-1.3 s by distance, gold sparks) to where the
  tapped orb was at cast time -> lands -> blast r 650 (was 430), push 520+1700K, includes the tapped orb; no gold orb any more.
  -ZoogiAppleInstant = b133 behaviour (instant blast + gold orb); -ZoogiOldGoldenApple = b79 behaviour. Log: "SKILL golden_apple: the apple lands".
- Evan asked what the ULTIMATE meter is vs Awaken (answered; waiting on his pick before merging the two meters).
- 07:39 b133 on TestFlight (EXPORT_UPLOAD_RC=0). Grok clip Anime_Showtime_MeteorStrike_02.mp4 (5.04 s) -> Content/Anime/Movies + Movies.csv row; sheet proofs/b134/clip02_sheet.jpg.
- 07:39 b134 launched (b134a SHOWTIME 01->02->sky cam + b134b golden apple), runs hand_wide,p5cmd_land.
- 07:53 b134 on TestFlight (EXPORT_UPLOAD_RC=0).
- patch_b135a (one AWAKEN meter: SUPER meter -> SHOWTIME at aim, no mid-roll ultimate, no AWAKENED buff, card casts +8, crystals = REDIRECT only; -ZoogiOldMeters)
  + patch_b135b (any fall empties the AWAKEN meter). b135 launched ~08:00.
- 08:16 b135 on TestFlight (EXPORT_UPLOAD_RC=0): one AWAKEN meter, any fall empties it. Open: Evan to say if self-roll-offs should also reset it; next feature pick (cosmos boss / stands crash / under-arena / per-hero KO clips - Buzzy has 14 credits).

## Nov 5 09:16 - b136 launched (TestFlight)
- b136a cosmos boss (ZoogiCosmosBoss.h/.cpp): SK_Mecha_Aurex_03 170 m titan, violet + red eyes, fixed beyond the arena from the opening
  camera, head 26 m over the court, faces the leader; reacts KO / Critical / Combo (REDIRECT x3+) / Miss; final round rises + roars.
  Gameplay cams never see the sky, so it gets a boss cam (reveal 3 s, KO 1.8 s, final round 2.6 s) - cuts only from the PC main cam
  (new AZoogiPlayerController::MainCam()). -ZoogiNoCosmosBoss / -ZoogiCosmosBossAll / -ZoogiNoBossCam. Debug log "COSMOS BOSS vp ..." every 3 s.
- b136b Skill Lab tunings reach every skill (FTuneScope in Later(), mines, golden apple capture, wolves/bolts StrengthOf/RadiusOf, ice wall height).
- b136c NEWS B127-B136. b136d menu press squash + haptic (-ZoogiNoPressFx). b136e IMPULSE replaces meteor_rain (Self aim, WolfBlast at caster,
  mandatory anime Anime_Awaken_Impulse_01 else WolfPack clip; Awaken casts keep the portal; -ZoogiOldMeteorRain). b136f ball card bg
  (card_bg_ballers*.png). b136g CPU 1 skill/turn (-ZoogiCPUMultiSkill), wood shield only when exposed, not in COMBO. b136h card how-to texts,
  bank_strike / orb_relay / recovery_roll LOCKED.
- Grok asked for a UI icon set (notes_for_grok/ui_icons_request.md) + roster-named KO clips. Buzzy 14 credits.
- 09:30 b137 patches APPLIED (not built yet; build after b136 uploads): b137a ZoogiItems (item collection in profile: ItemCounts / SelWeapons /
  bItemsSeeded = 3 each; WEAPON tab selects up to 3; ITEM list rows -100-index; one item per turn; save back at MatchOver/ReturnToMenu;
  weapons: grenade lob, sword=sword_slash, staff=water_cannon, gatling=magic_rays x3, disc=new halo_disc Line), b137b PRECISION = MultiTap 3
  water-cannon shots, no bullet time (-ZoogiOldPrecision), b137c Grok 3D icons (chests + GOLD/GEMS chips, -ZoogiFlatIcons), NEWS B137.
  gob137 runs front_port,p5cmd_land. run_anc still points at gob136go - switch to gob137go.
- Grok: icon set DONE (art/notes_for_claude/icons); asked for 15 old-card art redos (notes_for_grok/old_card_art_request.md -> notes_for_claude/card_art).
- 09:38 b136 on TestFlight (RC=0). b137 launched with b137a-d (items/weapons, precision, 3D icons, skills earned: starters bolt_balls/ice_wall/energy_buff, chests unlock skills, energy buff not in combo; -ZoogiAllSkills).
- 09:46 b137: Win64 OK after fix (OwnedCards private -> CardDefs+IsCardOwned). front_port: 3D chests in slots + chest reward unlocked orb_magnet; owned-skills hand 5->4. Fixed locked skills showing '- ON' (HandSlot only when owned) before SOURCES_UP. NOTE: the front test runs on Evan's PC editor profile -> it is now in earn mode (starters + orb_magnet).
- 09:56 b137 on TestFlight (RC=0). b138a card art studio applied (-ZoogiCardArtShoot, p5run card_art 1200x1000); run_anc -> gob138tgo (test only).
- 10:12 card art studio pass 4: picks in proofs/b138_cardart/*.png (sheet proofs/b134/cardart_picks.jpg) - shown to Evan, NOT installed. Weak: storm, bank_strike, orb_relay, momentum_steal, double_impact, recovery_roll. HUD box top-right still in frame. Grok's 15 cancelled - never use them (Evan 10:01).
- 10:30 Evan: BALLERS tiles = Zoogi FRONT views (style ref art/refs/zoogi_sheet_style_ref.png, proper Zoogi per our 8 balls). Request written: art/notes_for_grok/zoogi_front_views_request.md -> notes_for_claude/zoogi_fronts/. Couldn't type to Grok (its document panel = msedgewebview2 not granted). Grok made the 15 card art anyway (notes_for_claude/card_art) - Evan: DON'T use.
- 10:32 b138 launched: b138a card art studio (test only), b138b BALLERS zoogi_<ball>.png front views w/ ball fallback (-ZoogiBallerBalls), b138c golden apple left on court -> touch = 430 blast (-ZoogiNoAppleLeft), NEWS B138. Win64 OK. runs skill_lab,front_port.
- 10:47 b138 on TestFlight (RC=0, skill_lab: golden apple lands + stays OK). b139 launched: b139a volcano meteor event (5 strikes, chance 25%+15%/2 rounds, P5 warning circles, fire_meteors locked/out of hands; -ZoogiNoVolcanoMeteors/-ZoogiVolcanoMeteorsTest), b139b/c lab BLAST for every skill + BLAST SIZE row, b139d CPU hands = unlocked skills only. runs volcano_meteors,skill_lab.
- 10:52 b139 p5 runs done (BUILD_RC=0; volcano_meteors log: "VOLCANO meteors: 5 strike(s) this roll"). b139ios SOURCES_UP 10:57, Mac build running.
- Evan 10:54 asked why Hotstreak's awaken started: humans only via the SHOWTIME button (full AWAKEN meter); CPUs auto-awaken on a full meter via
  the OLD CardsUseAwaken infuse (no SHOWTIME for CPU - ZoogiShowtime::Begin returns for bCPU). Offered: CPU -> SHOWTIME path (awaiting Evan).
## b140 (launched 11:03, run_anc -> ios\gob140go.py; gob140 waits for b139ios DONE, then p5run frozen_open, then b140ios)
- patch_b140a: AZoogiBall::SetOut -> ZoogiHaptics::Rumble(2 s) on every Zoogi KO (-ZoogiNoKOBuzz); CheckKnockoffs KO blast / burst /
  shockwave / FELL OFF popup at the Zoogi's own spot (was EdgePoint on the court edge radial; -ZoogiOldKOSpot). Log "KO blast P%d at".
- patch_b140b: Frozen Zone 2 decor Slide tracks + hanger poles not built (Evan screenshot 10:59; -ZoogiAerialSlides = back);
  Misc/Parse.h + CommandLine.h added to ZoogiAerialProps.cpp; NEWS B140.
- 11:03 b139 on TestFlight (EXPORT_UPLOAD_RC=0). b140 p5 frozen_open verified: "aerial decor: unfinished slide tracks + hanger poles removed",
  "KO blast P4 at (-828,-2167,68) r2320 (edge point r1490)", "HAPTIC rumble knockout 2.0s". b140ios SOURCES_UP 11:11.
## b141 (launched 11:13, gob141 waits for b140ios DONE, p5run skill_lab, b141ios)
- patch_b141a (Evan 11:05 "Unlock the increase cap on the crystals energy buff gives"): Energy Buff always +1 crystal past the 4 cap and the
  one-per-turn limit; CardsAddAwakenCharge / REDIRECT turn +1 never lower a stack; HUD "+N" beside the 4 pips (FUnitRefs::PipMore);
  -ZoogiCrystalCap = old. NEWS B141.
- 11:33 b141 on TestFlight (EXPORT_UPLOAD_RC=0).
## b142 (Nov 5 ~12:10, Evan 11:23-12:08 batch) - patches ios/patch_b142a..m (+ new Public/ZoogiArenaReact.h, Private/ZoogiArenaReact.cpp)
- a golden apple solid metallic gold + floating card-art icon (-ZoogiOldAppleLook)
- b ZoogiArenaReact: Cosmos zero-g fly-out then KO blast (-ZoogiCosmicGravity); Cosmos rim opened (14 wings/bank arcs/re-entry pipes +
  22 rim bumpers off; -ZoogiCosmicRimSolid); stands = not out: crash FX + crowd (ZoogiShow BigScore) + CROWD TOKEN +1 crystal + lob back
  (-ZoogiNoStandsSave; Frozen via ZStand boxes, b53 arenas via StandBlockR); KO / orb explosions at the real spot via Skill Lab
  "ko_player"/"ko_orb" (ORB KNOCKOUT / PLAYER KNOCKOUT entries; BlastFor/BlastSizeFor/TintFor); -ZoogiTrainBotsTest.
- c/h water cannon: measured with -ZoogiWcYawTest (WC PROBE): yaw +90 sends the system's water ball down the cast; hit at
  1.2 s + dist/5000 (-ZoogiWaterCannonYaw= / -ZoogiWaterCannonHitT=).
- d/e/j Cosmos boss = 12 m posed Aurex puppet (Peek pose, head look-at) with hands on the R2 bowl-wall top in front of the human's
  camera, ducks + resurfaces each human turn, slam on KO, roar final round (-ZoogiBossTitan = b136 titan).
- f centre HoloSpire fades while the turn's Zoogi is within 8.5 m x scale of it.
- g/l SHOWTIME arms the Awaken; SUPERPOWER fires it (equipped ball: Hotstreak = 10-meteor rain), then Anime_Showtime_<key>_01/_02;
  unused by turn end = meter refunded (-ZoogiShowtimeCutFirst = b134 order). -ZoogiShowtimeTest auto-fires.
- i classic training CPU OPPONENTS fixed: MenuAction routed every "Train*" to RoyaleTrainAction (verified: 3 Zoogis, 2 CPU after restart).
- k tests -ZoogiShowtimeTest, -ZoogiStandsTest; p5run runs wc_test, train_bots, showtime_test, stands_test.
- m NEWS footer [TOP] / hint / [CLOSE], bigger popup X, NEW tag.
- DefaultGame.ini: +AlwaysCook /Game/Cosmic, /Game/Frozen/RR2, /Game/ArrowTrail/FX, /Game/Relics (the iOS cook never had the Cosmos R2
  floor / bowl wall etc. - Evan 12:02 "make the cosmos map the same as the pc version").
- 12:10 Cards.csv energy_buff cost 1 -> 4 (Evan).
- b142n volcano strikes: Zoogis only, LastHitPlayer cleared (no KO credit / combo); -ZoogiVolcanoHitsOrbs = old.
- 12:30 b142 SOURCES_UP (b142ios). b142 checks: stands_test "STANDS crash P1 ... not out: crowd token, thrown back"; train_bots OK;
  cosmos_boss: boss visible over the bowl wall in ahud frames (rim fallback = BowlRing bounds top z172, camera traces 0 hits);
  showtime_test ran out of time during the 13 s pick (b143 test picks after 1.5 s, runs 150 s).
## b143 (gob143 waits for b142ios DONE): patch_b143a - POWER removed from the P5 fan (-ZoogiPowerCmd), ITEM enabled with items left,
  inventory WEAPON -> ITEM, SHOWTIME test on the first human aim; NEWS B143.
- b143b weapon (ITEM) tiles get card_bg_ballers burst; BALLERS = Evan's crystal heads Content/Data/MenuArt/Front/zoogi_{frosti,hotstreak,vex,lumi}.png
  (512, transparent; ice=Frosti, yellow=Hotstreak, red=Vex, gold=Lumi); ZOOGIS tab always balls (ZBallIcon).
- b143c AUTO / ARC removed from the classic aim bar (JUMP IN stays on Frozen's outer ring; -ZoogiAimButtons).
- Grok: no new clips since showtime 02 / EP3; KO clips + Impulse anime still pending (Buzzy credits).
- 12:35 b143 launched (gob143 waits for b142ios DONE; runs showtime_test,p5cmd_land).
- b143d arrow grab shaft 95 / head 150 px + own Zoogi grabs (1.6x disc); press over orb/rival arms the slingshot (tap = aim at it);
  items MaxCarry 5, start 5 (sword/staff 3), one-time top-up via ItemCounts["seed_v2"], SaveBack clamps to 5.
- 12:36 b142 on TestFlight (EXPORT_UPLOAD_RC=0). The b142ios poller died at 12:32 (windows closed?) - ios/poll142.py re-polled the Mac
  (build3.sh runs on by itself under nohup) and wrote DONE so gob143 continued. If a poller dies again: copy poll142.py.
- b143 showtime_test VERIFIED: SHOWTIME (turn 1) -> armed -> test pick -> SUPERPOWER -> "Awaken METEOR RAIN (10 meteors)" ->
  Anime_Showtime_MeteorStrike_01 -> _02 (frames proofs/p5_style/showtime_test 01/04/05).
- 13:02 b143 on TestFlight (UBT_RC=0, EXPORT_UPLOAD_RC=0).

## 2026-11-05 13:30 b144 pending (Claude)
- Evan 13:19 "Grenades model never generated or launched": traced to ZoogiGrenade Sync() wiping a fresh lob when no orb-generator drop had synced the match yet (saved grenade thrown early). patch_b144a: Lob() calls Sync(G) first. patch_b144b: -ZoogiItemTest self-test + p5run item_test.
- Evan 13:29 retested on b143: "they work" - no test build launched (access declined). Fix stays in Source; ships with the next build. run_anc.bat points at gob144tgo.py (item_test only) - repoint before the next TestFlight.

## 2026-11-05 b144 (Claude) - SKY DIVE cam + stuck-high camera fix + grenade lob fix
- Evan 13:31 (Cosmos screenshot) "Camera gets stuck at high elevations how can we fix this and make it a fun part of the game action"; he chose Sky Dive cam. Cause: Bolt Balls banked high; PlayerController CourtZ tracked the airborne shooter up, the fall read as "below the floor" (IsLeavingPlay) so CourtZ froze at the apex and every grounded Zoogi after read as fallen -> camera held high forever.
- patch_b144c: CourtZ only follows a GROUNDED shooter (ZoogiPhysics::IsGrounded); grounded on-court shooter below CourtZ resets it (CAMWATCH log). New ZoogiSkyDiveCam (Public/Private): active Zoogi > 420 cm over its last floor (-ZoogiSkyDiveH=) -> camera actor swoops above + 0.3x slow-mo 0.9 s + SKY HIGH! callout, Dive phase rides it down looking at the predicted landing, Land = burst FX + Land sfx + haptic + shake, back to match cam. Skips pads (3 s), ARC cam, edge hang, Royale. -ZoogiNoSkyDive off, -ZoogiSkyDiveTest (p5run skydive_test, Cosmos). Ticked from ActionFun (RealDt) + dilation chain; in PC bOwner list.
- patch_b144d: NEWS B144. gob144 = skydive_test,item_test then b144ios.
- b144 uploaded to TestFlight (EXPORT_UPLOAD_RC=0). skydive_test verified: SKYDIVE swoop at 471 cm -> dive -> landed after 3.3 s -> cam off (frames proofs/p5_style/skydive_test/02,04,07). item_test: GRENADE P1 throws 1800 cm -> lands -> shockwave (round 1, saved grenade).

## 2026-11-05 b145 (Claude) - Hotstreak Awaken anime: one clip, top card
- Evan 15:28 "Remove the second anime clip for the hotstreak awaken; move from full screen to top card view". patch_b145a: ZoogiShowtime::FireArmed queues only Anime_Showtime_<Key>_01 and plays it with new UZoogiAnimeSubsystem::PlayTopCard (top-centre anime card, whole clip, game keeps running; AviClip never routes it to an avatar; HUD profile panels slide out via bTopCard). -ZoogiShowtimeFull = b143 full-screen _01+_02. patch_b145b NEWS B145. gob145 = showtime_test then b145ios.
- b145 uploaded (EXPORT_UPLOAD_RC=0). showtime_test: "Anime TOP CARD Anime_Showtime_MeteorStrike_01 for P1 (10.00s)" over the meteor rain, no _02, no full-screen crash for it (proofs/p5_style/showtime_test/12_anime_cutin1_Showtime.png).

## 2026-11-05 b146-b147 (Claude) - Cosmos boss attacks in the closing rounds
- Evan 16:34 "build the classic version" + 17:20 confirmed the full design. New ZoogiBossAttack (Public/Private): attack rounds = last 2 (MenuRounds>=4) else final; at the round first aim: Taunt + target (leader 67%) + HAND SWEEP (target r>0.5ER: 70 deg slice of the outer band) or GROUND SLAM (3 rings 300/560/820 at the spot); red pulses + countdown popup per turn; lands when the round last shot settles (GameMode bBusy/bBusyE include ZoogiBossAttack::Hold; 9 s fallback; 12 s watchdog). Sweep: CosmosBoss::Sweep drives the puppet hands into the court; cover = outward trace to the rim hits an obstacle; brace window = 0.12x slow-mo + tap. Slam: CosmosBoss::SlamToward, shockwave rim->spot, GUARD (ZoogiSkills::GuardBusy) x0.15. -ZoogiNoBossAttack, -ZoogiBossAttackTest (p5run boss_test).
- b146 verified (boss_test): 3 warnings, sweep / slam / sweep land and every turn resolved. b147a: brace -> ZoogiEdgeHang::Force (free b58 mini-game; CosmicEdgeOut skipped for forced balls), boss ring-outs credit the last hitter during the warning round (HitBy / OrbHitBy), CPU dodge in PlanCPUShot (CpuDodge: roll out / GUARD slam, 30% attack instead), boss attack camera (CamActive in PC bOwner), brighter zones. NEWS B146, B147.
- b147 uploaded (EXPORT_UPLOAD_RC=0). boss_test: brace -> "HANG P1 Blue grabbed the edge ... tap to pull up (free, cards off)"; attack camera frames the sweep / slam.

## 2026-11-05 b148 (Claude) - a boss for every map
- Evan 17:48 "for each map use the characters we bought as placeholders for now just tailor them for each map". patch_b148a: ZoogiCosmosBoss ThemeFor(arena) = Mecha Aurex mesh (SK_Mecha_Aurex / _02 / _03) + CustomColor MID zones + eye colour + name: GROVE (Meadow), FROST (Frozen), MAGMA (Volcanic), VOID (Cosmic), DJINN (Arabian), PHARAOH (Tomb), NEON (Neon), STAR (Teaser), TRAINING. ArenaHasBoss: every classic map except Ancient / Royale / Combo / solo Training; -ZoogiBossCosmicOnly = old. Attack callouts / hand trail use Name() / Tint(). p5run boss_<map> runs: all 6 maps spawn their titan + announce. NEWS B148.
- b148 uploaded (EXPORT_UPLOAD_RC=0).

## 2026-11-05 b149 (Claude) - BATTLE ROYALE mix + flight cap
- Evan 18:43 answers: classic rounds take turns like Classic, points per round, rotate through all arenas. New ZoogiRoyaleMix: StartMatch in royale mode -> Begin; N = MenuRounds; rounds alternate (last always classic), arena = Order[(seg+off)%8] (Ancient, Meadow, Frozen, Volcanic, Cosmic, Arabian, Tomb, Neon; Ancient always an action round). Classic round = RoyaleSetMode(false) + MenuRounds 1 + StartMatch, Score preset to totals; FinishGame hook -> totals, next round next frame (bSwitching guards repeat FinishGame calls); last classic round = the results screen. Action round = RoyaleSetMode(true) + RoyaleStartMatch, endless lives (RoyaleLoseLife), +ScorePlayer per KO / -PenaltyFall per fall, 45 s (-ZoogiMixActionSec=), RoyaleFinish hook; HUD clock + rule text. ReturnToMenu restores BATTLE ROYALE. -ZoogiRoyaleOld = b148. BossAttack + Grenade: new-match detection also by Balls[0] (GameSeed is per session). NEWS B149.
- Evan 21:03 flight cap (patch_b149b): ZoogiArenaReact::FlightCapOut - off court and 600 cm past the last on-court spot = out (mid-air too), unless a sweep ahead hits a building / wall; stands crash runs first. Sky Dive cam only over the court, stops when the Zoogi leaves it.
- b149 verified on PC (royale_mix 4 rounds: action Ancient -> classic Meadow -> action Frozen -> classic Volcanic, results "P3 Vex WINS" on the totals; stands_test still a crowd crash, not out; skydive cam lets go when the Zoogi leaves the arena). Uploaded (EXPORT_UPLOAD_RC=0).

## 2026-11-05 b150-b151 (Claude)
- b150 (Evan 21:47 "delete the left over arms from old models"): ZoogiBall never loads SM_<Name>_Nubs (old flipper arms re-shown by the drop-in / unhide paths); ShowFlipperNubs false unless -ZoogiNubs. Uploaded.
- b151 RAID MODE (Evan 22:48 "Let's do it"): new ZoogiRaid. MODE SELECT card RAID -> map select (GRaidMaps) -> you + 2 CPU allies, MenuRounds 10 (restored on menu), 5 team lives. Weak point = invisible BlockAll sphere (r150) on the court edge at the titan's angle (CosmosBoss::HoldAt) + gold pulse; Zoogi hits 6+v/120 (max 20), orbs 4+v/250 (max 12, star x2.5 max 25), SHOWTIME 20%. Titan moves each round (phase 2 <=66%: every turn), phase 3 <=33% enraged (BossAttack zones x1.3 via HalfW / SlamR*Rage). BossAttack every raid round. IsAlly true in raid (TeamIndex 0); CPU planner gets the hand as a 400-value candidate. HP bar + lives drawn from ActionFun. Win -> CosmosBoss::Defeat (sinks) + PublicFinishGame; lives 0 -> fail; round 10 -> fail. Results: RAID CLEARED / FAILED, MVP, damage + hits rows. raid_test (HP 90): cleared in round ~8, MVP Vex.
- b151 STORY: EP3 films moved to one per level (s3_archive P1, s3_broadcast P2, s3_storm P3, s3_freed P4); m3_neon BLACKOUT BRAWL royale -> classic (ModeTag "CLASSIC FREE-FOR-ALL" for 3+ players). Content/Data/Story CSVs.
- b151 uploaded (EXPORT_UPLOAD_RC=0). story_neon: "STORY mission m3_neon: classic, 4 players on Night Circuit" -> CLEAR.
- b152 (Evan 23:17 bolt balls from the air): FBoltShot GroundZ / b3D - fired >60 cm above the target, the bolt flies the 3-D line down to the target (ball / orb / floor 10 m ahead via trace), hits tested along the 3-D path, blast at the floor; caster velocity untouched (bolt_air test: vz 1183 -> 1183). -ZoogiFlatBolts = old.
- b152 uploaded (EXPORT_UPLOAD_RC=0).

## Nov 6 06:40-07:15 (Claude)
- b153 (Battle Royale classic-only) EXPORT_UPLOAD_RC=0.
- b154 (Win64 BUILD_RC=0; gob154 passed "menu_land raid_test" with a space -> no runs; p5run wants commas. Fixed in gob155):
  patch_b154a mode list (MULTIPLAYER pass&play, RAID under CLASSIC, fixes Subs/Elem OOB since b151) + new card art
  ModeSelect/ms_card_raid.png (was MISSING = white panel) and ms_card_multi.png (old ms_card_battle had "2V2 BATTLE" baked in);
  patch_b154b RAID CPU: weak point only, ally-corridor check w/ cuts up to 40 deg, else soft reposition, ~1700 cm/s impact power,
  no powers (-ZoogiRaidOldAI); SkyDive cam OFF by default (Evan "remove the zoom in camera ... air flight"; -ZoogiSkyDive on);
  EP3 films had NO audio track -> ios/ep3_sound.sh scores them from SourceArt/Audio (art/ep3_sound/), copied into Content/Anime/Movies.
- b155 queued (gob155 waits for b154 DONE; runs raid_test,boss_meadow,menu_land): patch_b155a titan out of sight during arena forming;
  classic shows it only in attack rounds (ZoogiBossAttack::Engaged) + 5 s cameo after KO/Combo; raid always after forming
  (-ZoogiBossAlways = old). NEWS B155.
- EP3 voices: Evan said yes -> art/notes_for_grok/ep3_voices_request.md (dialogue-only wav, then mix under ep3_score.wav and recut).
- Preview canvas "Zoogi Roll – Trick Shot & Shop Preview" (Daily Trick Shot x3, Shop, Buy pop-up, Collection, Level up) using real MenuArt assets.

## Nov 6 07:27-07:51 (Claude) - b156 queued (gob156 waits for b155 DONE; runs trick_test,menu_land,front_land,royale_mix)
- Evan: shop + levels (coins & gems only), Daily Trick Shot goes in, NO scoring fix; App Store Connect IAP = Evan's steps (IDs in
  GemPackages.csv: com.zoogiroll.gems.small/medium/large/huge/vault/treasure).
- patch_b156a: ZoogiTrickShot (Public/Private) - BeginTurn/EndTurn/FinishGame/ReturnToMenu hooks, PublicBeginTurn, ActionFun tick+draw,
  no titan in trick shot; mode list = STORY first, ..., DAILY TRICK SHOT (ms_card_trickshot.png) after RAID. Saves in
  GameUserSettings [ZoogiRoll.TrickShot]. Test: -ZoogiTrickShot (-ZoogiTrickDay=N). NEWS B156.
- patch_b156b (Front): coloured shop tiles (ZTile), gem/coin art, BUY pop-up (EPop::Confirm, GET GEMS/COINS when short), level locks
  (ZSkinLockLv / ZPackLockLv / ZChestLockLv, Hotstreak LV4), LEVEL UP pop-up (EPop::LevelUp), COINS naming, NEWS drag area = list only,
  anchor drag + 100 ms release velocity (news + shop), TOP eases, X 180x150.
- patch_b156c: UZoogiButton press fx (90% + haptic + pop back; -ZoogiNoPressFx), P5 list/back/results + P5 command plates squash + haptic.
- patch_b156d: Battle Royale lap opens on ANCIENT (redirect skipped while RoyaleMix active) + ZoogiThrone (Sky Citadel, basic shapes;
  loads /Game/ZoogiKing/SK_ZoogiKing or SM_ZoogiKing when Evan's model exists; -ZoogiNoThrone).
- patch scripts now handle mixed CRLF/LF files (regex fallback).
- b155 tests: raid_test CPU always aimed at the weak point (never at P1) but RAID FAILED (P1 bot fell 2x, CPU P3 fell right after a 1700 cm/s hit, 2 falls in the boss attack) -> b156 raid CPU impact speed 1300 cm/s. boss_meadow: 'COSMOS BOSS shows itself (attack round)'. menu_land OK.
- 08:05 b157 queued (Evan 08:03 'Replace meadows with ancient meadows for trick shot'): gob157 applies patch_b157a.py itself after b156 DONE (Windows path aware), runs trick_ancient,royale_mix. run_anc.bat had been pointing at repoll155go.py - reset to gob157go.
- 08:20 b156 Win64 FAILED (C1075): the RoyaleMix lap edit swallowed 'return Classic[S % 8]; }' into a comment. Fixed. Found RoyaleMix already edited by someone else ('Evan 08:02', Ancient replaces Meadow, Teaser kept) - kept that. gob157 had already applied patch_b157a (trick shot rotation) and failed on the same error -> everything now ships as b156 (B157 news folded into B156). b156ios_status emptied, gob156 relaunched (runs trick_ancient,menu_land,front_land,royale_mix).
- 08:34 b157 queued (gob157: waits b156 DONE + lock; trickprops_run.py = Blender 5 headless make_trick_props.py -> pieces_v14_trick/SM_Arena_PlazaPropsTrick.fbx (removes the 10-drum ring r7.2; curb ring, hazard drums, bollards stay) -> UE import_trick_props.py; then patch_b157b (trick shot props swap, titan colliders hands/arms/head while up, ZoogiFlags 2 waving Z flags per arena + Content/Data/MenuArt/Flags/zoogi_flag.png, ON FIRE bursts smaller/half as often, awakened ring fainter); runs trick_ancient,boss_meadow,raid_test.
- b156 tests OK: trick_ancient layout on Ancient (r1720) 3 tries, royale_mix round 1 classic on ANCIENT + ZOOGI KING throne built at (3293,-3842,1753), menu_land 7 items STORY first, front_land shop coloured + LV badges. Fix in b157b: a turn ending before the layout (cmd-line start) no longer counts as try 0; coin pack names COINS.
- 08:47 Evan: 'daily trick shot isn't a game mode, it's a pop up at start up main menu' -> patch_b157c (chained from patch_b157b since gob157 was already running): card removed from mode list, UZoogiMenuRoot::StartTrickShot, Front EPop::Trick pop-up once a day (GameUserSettings PopupDay; -ZoogiNoTrickPopup). Blender trick props: 30 drum parts (10 drums) removed, 9552 -> 7632 faces.
- 09:10 patch_b157b/c had a truncated run() (no write) -> b157 shipped WITHOUT them (= b156 + trick props asset). Fixed, applied after b157 SOURCES_UP; NEWS entry is B158; gob158 queued.
- b158 (11:05): Win64 OK, all runs OK (trick props swapped, 7 titan colliders, ZOOGI FLAGS on Ancient + Meadow, daily trick shot pop-up #1, MODE SELECT 6 items). First iOS stage failed: ssh to TX544.macincloud.com timed out -> b158retry.py (re-cook + stage) UPSTAGE_RC=0, Mac build running. Raid test still FAILED (teammates/bot falls) - the 1300 cm/s raid CPU change never shipped (lost with the truncated patches? check ZoogiGameMode VHit).
- 11:12 b159 queued (Evan 11:10 'Where's the daily trick shot pop up? For now ensure pops every time I open the app'): PopupDue always true (once per launch). Evan was on b157 (no pop-up); b158 has it.
- 11:20 b159a: NEWS/SHOP drag glides to finger target (28ms smoothing; -ZoogiRawScroll = old) - Evan 11:12 'tap hold to scroll ... stutters now on news and shop'
- 11:28 b158 + b159 uploaded (EXPORT_UPLOAD_RC=0)
- 12:30 b160 queued (Evan 12:25 'z flag on every map'): 4 flags per arena, 6.2m poles, 5x3.2m flag (b158 flags were above the camera). Tests boss_* on all 7 maps + trick_ancient.
- 13:00 Evan 12:43 "Update map image" + 12:57 (Ancient trick shot screenshot): see-through only the blocking barrier; remove the centre red/yellow
  pillars on Ancient; ring 2x bigger (AskUserQuestion -> "Grow ring, push islands out"); then STORY focus: feedback + finish the EP1 practice field
  (Training arena, m1_spar) - he picked "Show options first".
  * b161 (gob161): patch_b161a (Ancient always uses SM_Arena_PlazaPropsTrick, -ZoogiAncientDrums = old), patch_b161b (MPC_ZoogiCam.Zoogi set in
    ZoogiOcclusionFade::Tick, -ZoogiNoSeeThrough), Scripts/camera/make_seethrough.py (84 Ancient V14 materials -> Masked + Custom cut-out cone
    camera->Zoogi). gob161 waits for C:\dev\ios\b161_cards_ok (Claude makes ms_map_ancient.png + _p.jpg from anc_shots/pmap_ancient) then b161ios.
  * gobspar.py: p5run story_spar (-ZoogiStoryMission=m1_spar) while gob161 waits.
  * v15 arena: art\ancient_meadows\grok_design\blender\arena\build_arena_v15.py (v14 + override block: PR 35 m, islands/volcano/viaduct/moat/
    islets/base +17.5 m, no drum ring) -> pieces_v15, renders_v15, layout_v15.json. ios\v15_blender.py (status v15_status.txt).
- 13:24 ms_map_ancient.png (card, pmap_ancient pose 0 front, cy .6) + ms_map_ancient_p.jpg made; old card backups\ms_map_ancient_b160.png; b161_cards_ok -> b161ios
- 13:28 b162 queued (gob162): waits b161 SOURCES_UP -> patch_b162a (layout v15 numbers, ZoogiAncientV15Data.inl, V15 pieces from /Game/Env/AncientV15,
  spawns r19 m, orbs 20 @ r21 m, throne on the v15 citadel, NEWS B162; -ZoogiAncientV14 = old) -> after b161 DONE: import_v15.py + make_seethrough (V14+V15)
  -> p5run pmap_ancient,anc_classic,trick_ancient,royale_mix,front_land -> waits b162_cards_ok (new ms_map_ancient from pmap v15) -> b162ios.
- EP1 practice field options canvas: https://claude.ai/artifact/A9f3z5rUEPXNM5sw5fh9hN (Today + A Meadow Hollow Backyard / B League Training Court /
  C Bolt's Frozen Ring Replica). Waiting for Evan's pick.
- 13:52 b163 queued (gob163, waits b162 SOURCES_UP): Evan 13:32 "Finish the sound scape" -> Scripts/synth_ambience.py (SourceArt/Audio/Amb: 10
  per-map 30 s stereo loops + crowd bed + drumroll + accents, CrowdGroan, FallWhoosh, UnderBlast; numpy, project-owned) -> import_amb_sfx.py
  (/Game/Audio/Sounds/Amb) + ZoogiSoundscape (b163_files; ActionFun tick, ZoogiFeel::OnKnockOff; -ZoogiNoSoundscape). Evan 13:34/13:35 practice
  field = League Training Court using the Ancient ring only -> arena\build_court.py (pieces_court, renders_court) -> import_court.py
  (/Game/Env/LeagueCourt) + AZoogiArena::BuildLeagueCourt (Training arena; -ZoogiOldTraining), no titan in story spar. patch_b163a/b.
  Tests court_match, story_spar, boss_frozen, boss_volcano, front_land.
- 14:00 b162 cards (v15 pmap front, cy .52) -> b162_cards_ok. v15 verified in royale_mix/trick_ancient: court r3470, 4 flags, throne at (4432,-5171)
- 14:08 Evan 14:06: no Sound Check page; Grok as ears -> art\notes_for_grok\sound_review_request.md + sound_review\*.mp3 (39). Waiting for notes_for_claude\sound_review.md
- 14:18 b162 uploaded (EXPORT_UPLOAD_RC=0). b163 tests running. amb163 UE_RC=1 = a handled BINKA ensure in the NullRHI commandlet; all 39 SW_Amb assets imported.
- b164 queued (gob164, waits b163 SOURCES_UP): ANIME DIRECTOR (Evan 13:45, 14:03 answers: in the PC game, portrait separate, story clips too):
  b164_files ZoogiAnimeCues.h/.cpp (cue runtime, JSON Content/Data/Anime/AnimeCues.json, player + layer z96 with intro/exit templates
  none/fade/zoom/slide_*/wipe/p5slash/comic/shatter, borders none/p5/thin/comic, game pause/slow/play, duck, skip, trim), ZoogiAnimeDirector.cpp
  (Slate panel z97, Dev Mode button + F6, clips list + inbox C:\dev\anime_inbox IMPORT, cue list, all fields, drag/resize window on screen,
  landscape/portrait (phone frame), PREVIEW, autosave), patch_b164a hooks (Emit, ActionFun tick + dilation, IsBusy, BossAttack, TrickShot3Star,
  story StartLine/TickDialogue). AnimeCues.json seeded with the 12 Dialogue.csv film rows as story cues (film rows skipped when story cues exist).
- 14:55 b163 retry (gob163b): compile fixed (AnimeCues include order, CourtPieces, final-round check w/o private IsFinalMoments); NEWS B164 entry relabelled B163 - b163 = soundscape + league court + anime director. Tests: LEAGUE COURT 9/9, SOUNDSCAPE logs, story_ep3 played story cue story_s3_freed_1, no titan in spar.
- 14:58 Evan 14:55 'put ancient meadows back the way it was' -> picked normal size, no centre drums: b164 queued (gob164): patch_b164r (V15() only with -ZoogiAncientV15), pmap_ancient -> card (b164_cards_ok) -> b164ios.
- 15:22 b163 UPLOADED (EXPORT_UPLOAD_RC=0): soundscape + league court + anime director. b164 build failed 15:06 (C1076/C3859 compiler out of memory) -> gob164b retry (3 attempts).
- 15:29 b164 card = the b161 v14 card (proofs/flags/ms_map_ancient_new2.png); new pmap shot was grey (shaders still compiling). _p.jpg left as the v15 front portrait. b164_cards_ok.
- 15:46 b164 UPLOADED (EXPORT_UPLOAD_RC=0): Ancient back to normal size. Grok sound review not delivered yet.
- 17:19 b165 queued (gob165): Spots.csv ep1_a = ZOOGI LEAGUE COURT / 'Meadow Hollow: one practice roll before the Final' / env Training; mapshot_training -> training_map.png (Claude crops; b165_cards_ok) -> story_spar -> b165ios. Grok: notes_for_grok/sfx_orb_impact_request.md (Suno, orb = crystal-glass energy marble).

## 2026-11-06 17:25 b165
- Story stop 1 (ep1_a) = ZOOGI LEAGUE COURT, env Training; training_map.png = court top shot (old in backups/training_map_pre165.png). NEWS B165 added. Chest opening options canvas sent to Evan.

## 2026-11-06 18:28 b166
- Chest pop-ups open: common=classic burst, rare=card fan, epic=P5 slash (PaintChest in ZoogiFront.cpp; old = -ZoogiOldChest; force -ZoogiChestStyle=N). Lid/base PNGs MenuArt/Front/icon_chest_<k>_lid/_base. -ZoogiChestTest + p5run chest_land/chest_port.
- Story 2v2 (mode=combo) missions now run classic turn flow with teams (bClassicTeams; IsAlly by TeamOfIndex; team points decide). Old = -ZoogiStoryOldCombo. p5run story_final/story_dunes.
- gob166 waits on b166_cards_ok; gob166b = check runs.
- 18:50 b166 also has: crush hit layer (HitCrush + EpicImpact, -ZoogiSoftHits), TRAINING map LEAGUE COURT card (StartMapCard(100), ms_map_training.png), EP4 void_frosti head (Chapters.csv hero + map token), epic panels 2 rows in portrait. Grok briefs: sfx_zoogi_crush_request.md, skyward_plateau_request.md (+ art/skyward_plateau/skyward_plateau_ref.jpg).
- 19:03 b166 uploaded to TestFlight (EXPORT_UPLOAD_RC=0).

## 2026-11-06 20:36 Frozen Ring x Winter
- ithappy Winter files verified (7 GLB, SHA ok). Inspect: art/ithappy_winter/inspect (info.json, renders). Options mock-ups: art/ithappy_winter/options (A village / B skyline / C fortress), canvas https://claude.ai/artifact/WV9e1BTaQ1mgULXGcchfRa. Waiting for Evan to pick. Blender scripts ios/winter_inspect.py, ios/winter_options.py (run via run_anc -> winter_go2.py).

## 2026-11-06 21:25 b167 (Evan "Fix dev mode" -> "Won't open")
- Pause menu DEV MODE row only flipped ZoogiLayout flag; now AZoogiDevMode::ToggleFromMenu (PIN pad when locked, close on 2nd tap). OnDevButton creates the overlay for the PIN pad. Dev overlay z 60 -> 98 (P5 command menu at 60 covered the PIN pad); Anime Director 97 -> 99. Tests: p5run dev_menu / dev_menu_pin (-ZoogiDevMenuTest, -ZoogiDevLockedTest). NEWS B167.
- 21:45 b168: no PIN (ZoogiDevGate::IsUnlocked true unless -ZoogiDevPin / -ZoogiDevLockedTest); App Store builds still hide DEV via [ZoogiRoll.Dev] bDevButton=False. b167 uploaded 21:39.
- 21:56 b168 uploaded (EXPORT_UPLOAD_RC=0).
- 22:05 b169: ms_card_story.png = Evan arena key art (old in backups/ms_card_story_pre169.png), sub EPISODES 1 - 4, NEWS B169. b168 uploaded 21:56.
- 22:17 b169 uploaded. 22:30 Mode card options canvas https://claude.ai/artifact/Ui9evP1DY6zcLjMUhSUaLi ; 18 ready cards in proofs/modeart/cards (ms_card_<mode>_<a|b|c>.png, copy over ModeSelect/ms_card_<mode>.png on pick); frames from anime clips in proofs/modeart/full.
- 06:50 Nov 7 b170 STAGED, NOT BUILT (Evan: "Show me before you build"): new ms_card_*.png x6 from Evan art installed in ModeSelect (old in backups/modecards_pre170), subs changed (royale LAST ZOOGI STANDING, classic ORIGINAL ZOOGI BATTLES, raid CO-OP BOSS CHALLENGES, training LEARN - PRACTICE - MASTER), NEWS B170. run_anc -> gob170go.py ready. Preview proofs: outputs mode_cards_preview.png.
- 07:07 b170 restyled like Evan mockup: ms_card_* = full image + yellow strip/black italic Anton title (strip 70-84% so the torn edge does not cover it); ZoogiP5Style art frame goes wide (680 / 720 px) for art wider than 1.15. Checked menu_land/menu_port. gob170 waiting on b170_cards_ok = Evan go. NOTE: device_commit_files to an existing path with the same stagedPath name may not overwrite - use a fresh staged folder and check md5.
- 07:28 b170 draft v3 (Evan examples): cards 1000x800 (1.25) left-biased crop + yellow strip 2-78% x 85.5-96.5% y; P5Style wide art frame 620/680 px, NO white torn strip on the bottom edge (the first TornTex is the bottom one), black torn top kept; -ZoogiP5AllModes test + p5run modes_land/modes_port. Drafts proofs/modeart/draft_land2.jpg / draft_port2.jpg. Waiting Evan go -> touch b170_cards_ok (gob170 waiting).
- 07:41 b170 draft v5 (Evan: examples no exceptions + full image visible): ms_card_* = full uncropped 1100x750 art + white band 21% below with yellow strip (0-80%) + ArchivoBlack italic title; P5Style wide frame 680/720, tilt -1.5, plain black sub tag, GO! x1.3, mode list tilt -2.5, italic rows (TextItalic: FShear2D), purple ribbon. Subs = Evan examples (EPISODES 1 - 3, 2v2 ONLINE BATTLES, LEARN . PRACTICE . MASTER). gob170 still waiting on b170_cards_ok.
- 07:49 b170 draft v6: whole image visible - card band 14%, top torn strip moved above the frame (44 tall, +40 out), GO! below the picture beside the strip. Waiting for Evan go (touch ios/b170_cards_ok).
- 08:01 b170 UPLOADED (EXPORT_UPLOAD_RC=0): new mode select cards (full art + yellow strip below, Evan examples).

## 2026-11-07 KO examples (no build)
- gob171x ran ko_now / ko_new / ko_new_p (Frozen Ring, -ZoogiKOShow -ZoogiKOLine). Sheet: proofs/modeart/ko_compare.jpg. 3 m out line visible; roll tilt 72->60 is subtle; KO blast small. Waiting on Evan. Profile options A/B/C sent. Trick pop-up fill patched in ZoogiFront.cpp, not built.

## 2026-11-07 b171 work (not uploaded yet)
- patch_b171b: KO blast back on the court edge point (-ZoogiKOAtZoogi = b140 spot); AZoogiFX::BigBlastShake on every Big_Explosion + every PlayerKOBlast -> PC::OnBigBlast (no tactical cut, respects Settings CAMERA SHAKE, -ZoogiNoBlastShake). ko_shake run: KO at edge r1490 + BLAST SHAKE k=2.
- patch_b171c: -ZoogiTrickTest (trick pop shot). Trick pop image fill (cover crop) captured trick_land / trick_iphone, sent to Evan.
- patch_b171d: big corner badges (Evan picked A + SP + crystals): BuildBigBadge / SyncBigBadge in ZoogiArenaHUDWidget (landscape top bar + phone board); -ZoogiOldBadges. Backups backups/ZoogiArenaHUDWidget_b171pre.*
- patch_b171e + Scripts/arenas/meadow/import_meadowref.py: Grok meadow ref Rim + SpectatorStands L/R + ZoogiSpectators L/R -> /Game/Env/MeadowRef, spawned in BuildMeadow (scale FloorR/1720, rim no collision, height x0.2 -ZoogiMeadowRimZ=; old curb/stands skipped; -ZoogiOldMeadowRing).
- gob171z: import + build + meadow_shots, hud_land, hud_iphone, hud_iphone2. NEWS B171 added.

## 2026-11-07 b171 uploaded (10:36), b172 in the pipeline
- b172 patches (ios/patch_b172a..g): a chest slots colour by state; b marble finder (ring + face pin + YOU + trail, ZoogiFunCpuIcons, -ZoogiNoMarbleFinder); c arena weather replaces Blood Moon (Blizzard/Eruption/Bloom/Low Gravity, ZoogiWolfPack ApplyBloodMoon, -ZoogiWeather / -ZoogiNoWeather / -ZoogiWeatherChance=); d under-court rescue (StartTurn + Tick, RESCUE log); e Trainer Frosti hands-free guide v2 (ZoogiGuide Tick2/Draw2, Data/MenuArt/Faces/trainer_frosti.png, -ZoogiOldGuide; PC skips HumanTest while guide active; P5 menu hidden); f field-duel loser gets its form -> backstroke + splat (-ZoogiNoFieldFlung); g phone shop zig-zag category menu + wrapped category view + big BACK (ShopCat).
- Blood Moon off (ZoogiWolf::BloodMoon false unless -ZoogiBloodMoon). NEWS B172.
- Picks: chest C, marble A+B+C, guide A story box, weather option 2, shop C zig-zag.

## b173 uploaded 11:59 Nov 7
- Yellow ROUND n/n (BuildEpisode RoundTxt) and YOUR TURN line (TurnTagTxt) removed when big badges are on. NEWS B173.

## b174 uploaded 12:49; b175 staged (not launched)
- b174: marble finder projects with the canvas SceneView (iPhone safe-area offset fix), finder no longer hidden when DrawHud early-returns (GFinderOff).
- b175 (in Source, not built): edge-hang camera drift (PC UpdateCamera, ZoogiEdgeHang FView.At, -ZoogiNoHangDrift); YOU pin kept above its marble. gob175go.py set in run_anc.bat, b175_cards_ok NOT touched yet. Computer use blocked by an unidentified front window at 12:52.

## b175 (13:56) Stone Golem magma titan + edge-hang drift -> iOS running
- Content/StoneGolem copied from E:/UnrealProjects/ZoogiFabAssets (Mesh, Animations, Materials M_+Skin1, Textures minus Skin2-4). ZoogiCosmosBoss Spawn: Volcanic peek boss = SKM_StoneGolem + hammer/shield on weapon_r/l (-ZoogiOldMagmaBoss). boss_volc capture OK.
- Bought + on disk in ZoogiFabAssets: Dracoria, IceAttack, EarthbenderV1 (1.3 GB), StylizedSlashesVol1. Unity VFX Graph packs not usable in UE.
- Next: Dracoria frozen boss + awaken (fly down, mouth beam); VFX packs -> unlockable skills + world FX; mid-reaction superpower visibility (awaiting Evan OK on design).

## 2026-11-07 16:50 - b176 ready, waiting for Evan's GO (gate ios/b176_cards_ok NOT created)
- b176a 9 Fab-pack skills (ZoogiSkills ExecPack); b176b world FX (KO Earthbender V04 burst, clash finisher CrossSlash08)
- b176c 15 card-book originals retired (ZRetiredSkill; -ZoogiOldSkills); b176d ALL SKILLS = Dev panel toggle (Saved/ZoogiDevAllSkills.txt), earn mode default
- b176d trainer waits for tap (-ZoogiGuideTap test flag), finder trail off (-ZoogiFinderTrail); b176f finder ring off (-ZoogiFinderRing) - YOU pin only
- b176e/g CPU + AutoCast attack lines through target; Assist() = every pack skill hits (2.5x R); tap skills blast behind target
- b176h REACTION dashed reach ring drawn round the human marble while REACT is available (ZoogiRedirect::Draw)
- DefaultGame.ini always-cook + StoneGolem, IceAttack, EarthbenderV1, StylizedSlashesVol1 (golem missing on phones before); EarthbenderV1/Models copied (fbx parked in ios/_hold/fbx)
- Scales: GlacierLance 0.55, IceShards 0.25, RockRampart 0.9. Ground Slash card text = crimson.
- Skill bible doc: https://claude.ai/artifact/S3u1i6HG53hz4K5GwNaw1x ; sheets proofs/b176/
- NOTE: one ZoogiSkills edit (tap-direction) landed while a PC UBT build was running (16:25); later PC builds recompiled clean.
- b177 staged in ios/b177 + ios/patch_b177a.py (Dracoria Frozen boss, ICE BREATH dive+beam). Content/Dracoria copied (209 MB). Apply ONLY after b176 SOURCES_UP.
- 17:01 b176i Skill Lab: 9 pack skills added (24 entries); ExecPack honours STRENGTH/RADIUS/PULL(lift)/FX SIZE/COLOUR/BLAST+SIZE; LabCast attack lines. skill_lab run clean (proofs/b176/skill_lab.jpg). Still waiting for Evan's GO.
- 17:31 b176 EXPORT_UPLOAD_RC=0 (TestFlight). Cook had StoneGolem/IceAttack.
- 17:45 b177 applied after SOURCES_UP: ZoogiDragonBoss.{h,cpp} (Dracoria, R=0.9*ER, hover R*0.42), CosmosBoss hooks, BossAttack ICE BREATH lead 2.2s / beam x1.7 + dragon cam; Dracoria in always-cook; NEWS B177. p5run runs dragon_peek, dragon_breath (old 'boss_frozen' name already existed). proofs/b176/dragon_b177.jpg. Waiting for Evan's go on B177.
- 18:50 b177 (not built for iOS yet): Dracoria seat on Frozen islet 0 (bleacher+crowd removed), front-facing (MeshOff fix), round-start + combo boss cams, laser breath (FxMID cylinders + Vefects arcs + orbs/pulses); hanging + standing Resonance halos off on Frozen (-ZoogiHangingHalos); ZoogiGolemBoss (Volcanic full body 6.2 m, wanders rim + visual smashes, GoTo slam side, HAMMER SWEEP); combo = left badge only (-ZoogiComboSlab); KO rocks off (-ZoogiKORocks); Skill Lab HEIGHT row (Kick Z); ice pack smoke/snow emitters off (-ZoogiIceSnow); FX life 8 s; full Materials/Textures/Models copied for the 3 packs (src png/fbx -> ios/_hold/srcfiles); Wood Shield locks kinematic (-ZoogiWoodShieldHeavy); front Settings ALL SKILLS (DEV) row (not yet compiled).
- 19:10 b177 EXPORT_UPLOAD_RC=0 (TestFlight). Cook had Dracoria. run_anc -> gob177go.py.

## 2026-11-07 20:12 (Claude) - B178 in progress (not shipped)
- b178f Earth/ice/slash pack skills: PackSfx() cast + hit sounds (Rumble/RockCrack -> RockSmash/EpicImpact/Debris; crystal +RockShatter, gold +GuardClang). -ZoogiPackSfxOld = old.
- b178g Skill HOW TO USE card (Evan picked option C) above FIRE/CANCEL, gesture icon, ~2x text. -ZoogiSmallSkillHint = old strip. CPU picks shoot skillpick_<id>.
- b178h Dracoria fly-in intro (FlyIn/Landing/IntroRoar states, own camera, DRACORIA title, tap skips). GameMode Forming phase holds via ZoogiDragon::IntroHold(); SkipStageForming -> SkipIntro. -ZoogiNoDragonIntro. Episode 1 s1_arrival has 6 new Dracoria lines.
- b178i Volcano meteor warnings run on game time (frozen during REACTION / skill picks).
- b178j Earthbender line skills: FX yaw +90 (along the swipe, -ZoogiWallYaw=), staggered 0.11 s from start to tip; arrowhead on the drawn line.
- b178k Stone Pillar: MultiTap up to 3, range x2 (-ZoogiPillarRange=), pillars 0.2 s apart (all multi-target tap skills stagger).
- Option mockups sent: awaken meter / aim arrow / launch labels (waiting on Evan's picks). Runs: gob178w (done), gob178x queued.
- 21:11 Evan said Go: gob178 (item_test build check -> b178_cards_ok -> b178ios.py) running. Also b178l (Evan picked A/A/A): big segmented AWAKEN bar (-ZoogiSmallAwakenMeter), DrawPowerArrow thick yellow->red arrow + POWER % (-ZoogiNoPowerArrow, hidden during picks), 2x yellow aim plates (-ZoogiSmallAimTags). Frozen map art replaced (old in ios/_hold/menuart_b178). Dracoria intro camera A moved off the spire; landing shockwave ring instead of debris.

## 2026-11-07 22:10 (Claude) - B179 in progress (NOT built yet - PC clicks blocked by an unidentified front window)
- B178 shipped 21:32 (EXPORT_UPLOAD_RC=0).
- b179 ZoogiBounty.cpp/.h: Bounty (crown on the score leader, first hit +50) + Turn Challenges (2 orbs / 3 orbs / 2 rivals / rival off / orb then rival / bounty+orb -> +30 pts +25% awaken), human turns only, success-only. -ZoogiNoBounty / -ZoogiNoTurnChallenge.
- Strike combos: CPUs no longer buy them (-ZoogiStrikeCombos restores); CPU shatter banners hidden (-ZoogiCpuShatterBanner).
- b179 ZoogiBirds.cpp/.h: Meadow songbirds from engine basic shapes, perch/peck/hop, flee when a Zoogi rolls near (startle chain), return + land; start-of-match flyover flock. -ZoogiNoBirds / -ZoogiBirdsAll.
- NEWS B179 added. run_anc.bat -> gob179tgo.py (meadow_v3,item_test,dragon_peek).
- Waiting on Evan: inventory layout (big-card A/B/C), STRIKE replaced by 3+4 (done), Earthbender map events (keep skills too) - not started.
- b179: 43 Earthbender effects out of the skills list (ZoogiSkills::IsEventOnlyEarth; kept: sapphire_wall, pillar_field, long_rampart, fault_line; Skill Lab still has all). -ZoogiAllEarthSkills restores.
- b179c: SKILLS tab = layout C (big detail card + 3x2 big-card list, UP/DOWN + scroll bar like B, filter chips) in ZoogiFront.cpp PaintSkillsC; -ZoogiOldSkillGrid = old tiles. gob179t runs front_land,front_port,meadow_v3,item_test,dragon_peek. Still not built: PC front window blocks clicks.
- 00:30 b179d-f (not built yet): ground_slash out of skills -> Golem slam plays NS_GroundSlash03 rim->target (-ZoogiNoGolemSlash); S_DragonRoar/S_DragonRoarShort synthesized (SourceArt/Audio/Dragon, Scripts/import_dragon_sfx.py run by gob179u) + RoarSfx(); breath haptic pulse 0.22 s; Frozen RAID heart crystal (ZoogiRaid Dragon(), -ZoogiRaidHand); inventory robot removed (-ZoogiInvRobot); -ZoogiDragonPoster run (dragon_poster); Meadow CentreMedallion off (-ZoogiMeadowMedallion); Meadow outs sink through ground (AZoogiArena::SinkThroughGround/RestoreAfterSink, -ZoogiMeadowNoSink); AWAKEN bar +150U right in landscape (-ZoogiAwakenLeft); skill/item KO credit (Kick credits ActivePlayer, Blast credits balls, aiming-phase ball KO check).
- 00:10 Evan approved deleting old ZoogiRollUnreal-* copies (list ios/b179_deleted_worktrees.txt; kept p5style + combined). Deleting in batches.
- 01:05 b179 PC test OK (BUILD_RC=0): roar imported, poster -> frozen_map/featured, raid crystal callout, turn challenge card, birds wings fixed, portrait skills 2x3. Ship scripts gob179/b179ios ready (not started - waiting for Go).
- 06:40 b179c/f: Rock Rampart stagger reverted (all at once; -ZoogiWallStagger), Crescent Slash = MultiTap up to 3, FX flies caster->target (mid + target, oriented), Cards.csv text. B179 ship NOT started (two gob179 launches hung before p5run; masked console on screen).
- 07:00 b179 everything (earth events, crescent target, rampart revert, bounty, birds, etc.) PC-tested; gate b179_cards_ok set -> iOS.
- 07:13 B179 SHIPPED to TestFlight (EXPORT_UPLOAD_RC=0). Contents: earth events, crescent target x3, rampart revert, bounty+turn challenges, strike combos off, birds, skills big cards C, inventory robot off, ground slash->golem, dragon roar+breath haptics, frozen raid crystal, frozen map frontal, meadow sink+no medallion, awaken bar right, skill/item KO credit, 43 earth skills out of list. Old worktree deletion continuing (list ios/b179_deleted_worktrees.txt).

## Nov 8 ~10:00 - B180 code (Claude), NOT built yet (waiting for Evan's "build")
- Meadow hybrid (ZoogiArenaMeadow.cpp): old surroundings (scatter, batch1 bleachers/village/shrines/mountains/realism) + v3 ring only (Rim, Floor, Circuits, RuneRocks). -ZoogiMeadowV3Full = b179 look.
- Crowd: AZoogiFrozenCrowd on batch1 bleacher wood tiers (MEADOW CROWD log); -ZoogiNoMeadowCrowd. Bleacher colliders tagged ZStand (crowd throws Zoogi back via ZoogiArenaReact::StandsCheck, now Frozen+Meadow).
- Buildings: shrine/gatehouse colliders tagged ZStand+ZBuilding -> BuildingBlast (jet of explosions toward centre, cone push, 2 smashes break the building). -ZoogiPlainBuildings. Test -ZoogiBuildingTest.
- Earth rocks (ZoogiEarthEvents.cpp): visual placed in world explicitly (phone showed the model floating at the centre), per-map stone MID, bouncy phys mat, spread FreeSpot (best of 40, >=600 apart), breakable (hit >=450 cracks, >=1250 or skill Blast breaks) with hand-animated chunks. BlastAt() called from ZoogiSkills Blast.
- Redirect while airborne dives at the target in 3D (-ZoogiRedirectFlat old).
- Aim preview lines x1.3 (-ZoogiPreviewThick=). Turn-challenge card hides once aiming. F6 = Slate input pre-processor (any mode/menu).
- NEWS B180 added. Scripts: gob180go/gob180/b180ios.py, run_anc.bat -> gob180go. p5run runs meadow_build, meadow_stands. Backups ios/_hold/*.b179.
- Follow cam (smart cam): shooter weight 3.5, centre slides onto shooter near rim, faster catch-up, speed lead; -ZoogiOldFollowCam. Preview thick now x1.1.
- 10:25 Evan: event earthbender FX held (AZoogiFabFX::FreezeAt 1.1 s, -ZoogiEarthFreezeAt=) as the obstacle, model hidden (-ZoogiEarthRockModel shows it), max 5 formations (groups), crumbles (Release) on break. Rock test now round>=2 rolling. Meadow stone #9AA488. run_anc -> gob180bgo (earth_meadow only).
- 10:40 quick run OK: held formations show (frame 45), rock break chunks fly (48). Chunk/rock MID +emissive 0.45 (rendered black). gob180 still waiting at gate b180_cards_ok. run_anc back to gob180go.
- 11:15 Two match themes: SourceArt/Audio/Music/Themes/Theme_Arena1/2.wav, Scripts/import_themes.py -> /Game/Audio/Music/Themes/S_Theme_Arena1/2 (looping); ZoogiMusicSubsystem::StartArena picks one at random (no repeat) on the base stem slot, clock 156 bpm; -ZoogiNoMatchThemes. b180ios.py imports themes before COOK. NEWS updated.
- 11:26 Evan Go: b180_cards_ok created; b180ios running (themes imported OK, rc=1 is the usual handled ensure).
- 11:40 B180 SHIPPED (EXPORT_UPLOAD_RC=0).
- 11:45 B181 code (not built): incoming-hit (BLOCK) slow-mo now opens with 2 hard-cut attacker close-ups (head-on, low side, 1.0 s total, -ZoogiNoReactCloseups, -ZoogiReactCloseupSec=) before ZoogiReactCam side view. ZoogiActionFun.cpp/.h. NEWS not yet updated for B181.
- 11:55 B181: SP 10/turn (DefaultGame.ini EnergyPerTurn=10; HUD 10 segs). NEWS updated.
- 11:59 GATTLING / SHOWTIME design locked (not built): top tier of AWAKEN; only earned (no item); needs the lead + full meter (combo streak fills faster); match PAUSES during SHOWTIME (just you + music); ~8 bars @156 bpm; orb generator gushes orbs on the beat in waves, lights/flags/circuits (Frozen clouds) flash to the beat; taps snap to nearest beat + target, hold = auto-fire, wide windows (~0.2 s PERFECT / 0.35 GOOD), 2-miss combo shield, ball pulse; finale blast scales with combo (always pays off; max = SHOWDOWN). Build after B181 on "build gattling".
- 12:04 GATTLING rule: on AWAKEN activation - not in the lead = that Zoogi's normal Awaken only; in the lead = SHOWTIME first, then the Zoogi's own Awaken fires right after.
- 12:05 GATTLING CHANGE: AWAKEN and SHOWTIME are SEPARATE (replaces 'top tier of Awaken' + the 12:04 rule). SHOWTIME gets its own meter/button, still earned only, needs the lead, match pauses.
- 12:15 B180 CRASHES AT LAUNCH (Evan). Cause: ZoogiEarthEvents.cpp namespace-scope 'static const bool bShowModel = FParse::Param(FCommandLine::Get()...)' runs in static init before the command line exists -> Fatal on iOS. Fixed (lazy function). Ship as B181 asap. LESSON: never put FCommandLine reads in namespace-scope statics.
- 12:17 Evan STANDING RULE: 'Go to TestFlight after every build' - create the bNNN_cards_ok gate up front; no waiting for frame review.
- 12:43 SHOWTIME PICK: Chain Dash (H) mixed with Beat Floor (J) - preview SHOWTIME_chain_dash_floor.mp4. Hex dance-floor tiles flash to the beat; next target tile lights white (rival = magenta) with a closing ring one beat ahead; tap on the beat -> Zoogi dash-hops onto it through the target; landing blast pops nearby orbs; lightning chain + CHAIN counter. Own SHOWTIME meter, lead only, match paused, assists. A (Beat Rings) / E (Light Show) to reuse elsewhere (block timing / final round + victory).
- 13:10 B181 SHIPPED (EXPORT_UPLOAD_RC=0): launch-crash fix + attacker close-ups + 10 SP. Close-up frames not captured in the test run (no CPU shot on the human).
- 14:40 B182 code: SHOWTIME/Gattling (ZoogiGattling.cpp/.h; hooks Powers OnShooterContact, ActionFun Tick/Draw, PC input, AimArrow/HUD/P5Cmd/Bounty hidden while active). Beat maps Content/Data/Music/Theme_Arena1/2_beats.txt (DP tracker, 158.45 bpm); music subsystem plays theme unquantized + ThemeClock(). Runs gattling_test/gattling_port. gate pre-created.
- 15:57 B182 shipping (SHOWTIME v1 + birds Chirp GC crash fix: static raw USoundBase* -> weak). Frames: works but camera fell back to chase cam after ~2 s (CAMWATCH frozen-view guard) and points too high (959). B183 code: camera drifts + reclaims view target, points ~250. run_anc -> gob183go.
- 16:18 B183 +: SHOWDOWN tilt-up: camera pitch -68 -> -14, dist x0.62, look rises 520 cm over 0.7 s; two more blasts climb (300, 620 cm); show end +1 s (EndAt = FinaleAt+2.6). Shot gattling_tiltup.
- 16:50 B183 SHIPPED 16:40. B184: [ZoogiRoll.Audio] bMusic=True (master switch on); PlayMusic old loops blocked unless -ZoogiOldMusic; no stems when no theme; ShiftHit stinger off during themes. run_anc -> gob184go (gattling_test).
- 17:05 B184 SHIPPED (EXPORT_UPLOAD_RC=0): music on, themes only. Frames: SHOWTIME camera holds overhead, tilt-up at SHOWDOWN works, +213 pts for a 30-beat run.
- 17:16 B185: menu theme from launch (UZoogiMusicSubsystem::PlayMenuTheme via PlayMusic(MenuMusic); stops at match start, back on ReturnToMenu); themes set ForceInline (Scripts/themes_inline.py, run by gob185 first) - phone silent in B184 (PC log shows it playing); announcer VO off unless -ZoogiVO.
- 17:34 B185 SHIPPED (EXPORT_UPLOAD_RC=0): menu theme from launch, themes ForceInline, announcer muted. PC log: menu theme playing + match theme playing.
- 17:58 B186: SHOWTIME stuck on phone (5 min in: looping wave GetDuration=10000 -> beat map hours away). ThemeLen from USoundWave::Duration; gattling validates first beat <=4 s else clock; 32 s safety end. Old awaken P5 item/wipe/hint/callout renamed AWAKEN (Evan confused the two).
- 18:13 B186 SHIPPED (EXPORT_UPLOAD_RC=0).

## B187 (2026-11-08 18:56 CT) - started
- Music: GThemeBus 1.7 (themes ~5x louder: match + menu + end stinger); ThemeIdx set at pick.
- SHOWTIME: auto-starts on the human's next aim when the meter is full (no button, no lead needed to start; -ZoogiShowtimeButton = old). Bar always shown in a match (grey "TAKE THE LEAD").
- Skill pick -> aim wait: P5 menu Dismiss releases the held press (was blocking field taps 1.5 s). Unverified on device.
- CAMGUARD (ZoogiPlayerController): match start forces the match cam + stops episode shots; junk view (null/pawn/ball/hidden) restored in 0.3 s; any non-match view capped 6 s on aim / 10 s in roll/resolve (Gattling + skill lab exempt). ZoogiFront menu exit goes to MainCam(), never stale PrevView.
- B187 shipped to TestFlight 19:11 (EXPORT_UPLOAD_RC=0). PC gattling_test clean, no false CAMGUARD fires.

## B188 (2026-11-08 19:19 CT) - shipped 19:34 (EXPORT_UPLOAD_RC=0)
- NEWS: UP / DOWN arrow buttons in the footer between TOP and CLOSE (NewsGoal eases ~0.75 page per tap; grey at the ends). Swipe still works.

## B189 (2026-11-08 19:54 CT) - started
- Wood Shield: EMidRoll::Blocked (never after launch / in the air); ZoogiWoodConflict in CardsSlotPlayableNow - no other skill after it this turn, no shield after another skill ("WOOD SHIELD IS CAST ON ITS OWN").
- Golden Apple -> ITEM: retired as a skill (ZRetiredSkill + AllSkillIds), ZoogiItems "apple" (seed_v3 gives 3 once, max carry 5), always listed in ITEM; BeginWeapon apple -> golden_apple pick. Can target orbs OR rivals with a wood shield; every apple blast (landing / left apple / instant) calls BreakShieldsNear (dome gone, mass back, thrown out, "SHIELD BROKEN!").
- Energy Buff: Cards.csv cost 3; cast = exactly +3 crystals now (no cap); the tap-play pip is skipped for it.
- B189 shipped to TestFlight 20:09 (EXPORT_UPLOAD_RC=0).

## B190 (2026-11-08 23:36 CT) - started (Evan picked: fast fx / gattling beat+turret+6-shot / boss every 3 rds + grabs + invasions + anger + lock-on)
- FAST FX: ZoogiSkills FastD() compresses Later/LaterReal delays (<=0.25 x0.6, longer 0.15+(d-0.25)*0.55); EffectBusy 1.6->0.7 s; golden apple form/fly faster; P5 menu anim clocks x1.8 (P5Snap). -ZoogiSlowFx = old.
- GATTLING skills (Cards.csv + FDefs + Execute + GatTick in ZoogiSkills Tick): gattling_burst (Self, 6 shots), gattling_beat (Self, 8 shots on 158.45 bpm theme clock, tap = PERFECT x1.6), gattling_turret (Spots, MidRoll Blocked, fires at rolling rivals within 520 cm, 8 shots/turn, 2 owner turns). Always owned (IsCardOwned). CPU values in ZoogiCPUSmart.
- BOSS (ZoogiBossAttack, boss maps only): AttackRound also R%3==0 (-ZoogiBossFinaleOnly = old); InvasionRound (R>=2, not attack/grab) = mini slam (rings x0.6, speed x0.55, dodge = +1 crystal); GrabRound (R>=4, R%3==1) = leader grabbed, human tap duel (+3 SP / thrown), CPU instant 45%; ANGER meter (score gained /1800) -> filler's next turn reaction duel (3 rounds, win 2 = kept, lose = -50; CPU 50%); LOCK-ON: LockOnTarget/HitBoss - every other Gattling shot hits the boss (+10, attack x(1-0.1*hits) min 0.35). Draw hook in ActionFun, PC swallows taps while DuelActive. Test flags: -ZoogiBossGrabTest -ZoogiBossAngerTest -ZoogiNoBossGrab/Invasion/Anger.
- PC test b190_boss (Cosmos, 5 rds): invasion + dodge crystal, round-3 attack, anger -> duel, gattling burst OK. Beat/turret/grab not exercised yet (FParse stops at commas in -ZoogiSkillOnly).
- B190 shipped to TestFlight 00:02 (EXPORT_UPLOAD_RC=0). B191: P5 menu + turn challenge hidden during boss grab/duel; PC test b191_t2 (grab test + gattling_beat+turret).
- B191 PC test b191_t2 OK: gattling beat, turret, boss grab (thrown), lock-on hits. Lock-on aims at Dracoria/Golem head. Gate created.
- B191 shipped to TestFlight 00:25. B192: 3x CPU fast-forward (2nd tap), Earth events from round 2 + none on Cosmos, ORB KNOCKOUT blast on earth rock surface when Zoogi/orb hits (TickRocks SurfaceBlast).
- B192 shipped to TestFlight 00:47 (EXPORT_UPLOAD_RC=0).
## B193 (2026-11-09 00:48 CT)
- ATTACK (P5) = instant launch along AimYaw / free-aim point at 85% pull (SetAim+ReleaseAim), tunnel-vision canvas overlay 0.75 s (GZoogiAttackJoltAt in ActionFun), OnImpact shake + haptic; normal REACTION after. -ZoogiAttackAims = old.
- Boss cam: never starts during Rolling (request dropped) and ends when a roll starts (-ZoogiBossCamInRoll = old). Dracoria round roar every other round; no combo cut.
- Round 2 on Frozen: DragonHatesSnowman - ice breath at the platform, snowman bursts (4 hits), platform shattered (-ZoogiKeepSnowman).
- Earth rocks: FRockInfo.Born; at a round start with Dracoria, formations 2+ rounds old are broken (BreakRock) with ice breath + callout.
- Orb generator: SpireRefillOrbs(MaxNew, bRoundStart) - per-turn top-up up to 2 (pulse), round start full refill as before (-ZoogiSpireRoundOnly = old).
- REACTION after SUPERPOWER: Evan said keep it.
- B193 PC test b193_frozen OK (snowman+platform gone at round 2, Dracoria broke a 2-round-old rock at round 4, per-turn orb top-ups). After the PC compile (Mac-compiled only): snowman hits wait for the breath (2.1-4.4 s), platform at 4.6 s; old rocks break at 2.2 s; GATTLING BEAT -> GATTLING RAPID (3 s tap-fire, generator refills every 0.3 s); Stone Golem 900 cm.
- B193 shipped to TestFlight 01:21 (EXPORT_UPLOAD_RC=0) - it already carried the B194 SHOWTIME-button code (edited before its SOURCES_UP). B194: SHOWTIME manual again via gold P5 'Gat' command (under AWAKEN), ZoogiGattling::Ready/StartFromMenu, combo rows AWAKEN(-301)/SHOWTIME(-302) -> SpecialQueue in tick order; PC compile + gattling_test OK. Gate created.
- B194 shipped 01:42. B195: combo order - __awaken/__showtime markers in the SUPERPOWER combo queue (ComboGo -> ZoogiShowtime::FireArmed / ZoogiGattling::StartFromMenu(G,true)), Gattling CanStart allows Rolling via GMidRollOk, roll resolve waits while Gattling active.
- B195 shipped to TestFlight 05:56 (EXPORT_UPLOAD_RC=0). Mid-roll SHOWTIME from a combo is untested in automation.
- B196: Gattling burst/rapid/turret fire real Bolt Balls bolts (BoltFireAt split out of BoltGo), turret = big gold tower + purple bolt orb + reach ring; SHOWTIME portrait cam pitch -86; aim-step CAMWATCH skips while Gattling active; earth held-FX rock collider sized to the effect bounds (r100-240). PC test b196_t: turret + 6 bolts OK. Gate created.
- B196 shipped 06:42. B197: GATTLING RAPID = 6 s world pause (dilation 0.0001 via ZoogiSkills::RapidActive), tap = bolt, canvas counter/clock, PC taps swallowed; earth collider: fixed-bounds FX -> r150.
- B197 shipped to TestFlight 07:00 (EXPORT_UPLOAD_RC=0).
- B198: Content/Data/CardArt/Icons/gattling_beat.png (rapid icon, made from bolt_balls.png art + bolt fan + clock ring), drawn over the RAPID FIRE counter.
- B198 shipped to TestFlight 07:26 (EXPORT_UPLOAD_RC=0).
- B199: SUPERPOWER button shows the GATTLING RAPID counter (badge with rapid art + xN, RAPID xN! label, gold dot ring = 6 s clock); pad press during rapid = a bolt only.
- B199 first try failed (RapidIcon not declared before use - fixed with a forward declaration); restarted 07:49, PC compile OK. Also gattling_burst.png + gattling_turret.png icons (turret from Evan's turret concept art).

## Nov 9 08:50 (Claude) B199 on TestFlight; B200 in progress
- B199 EXPORT_UPLOAD_RC=0 (TestFlight).
- B200 code: GATTLING RAPID taps -> gold target circle on the court where you tapped (deproject to floor), a Bolt Balls bolt flies to it in real time while the world holds (BoltTick uses FApp delta during RapidActive; AZoogiFX gets CustomTimeDilation 1/dilation during rapid). BoltFireAt(..., AimPt). HUD draws the circles (ZoogiSkills::RapidMarks). -ZoogiRapidAuto = old auto-target.
- Diag: -ZoogiDumpTall logs tall components at each round start (looking for the Frozen "poles" Evan wants removed - thin dark lines rising into the sky near the stands, seen in b193/b196/b200_t frames). First dump (thin filter) found only the 12 under-floor pillars - not it. gob200b.py = compile + b200_d dump run (no iOS). gob200.py is parked waiting on b200_cards_ok.
- Raid demos sent: RAID_challenge_A-F + RAID_boss_attack (scratchpad st/raid.py), RAID_locator_A-F (st/locator.py). Waiting for Evan's picks.
- Tripo: turret = "cute robotic statue 3d model" (8a314842...). 2M tris. Retopology page set to Triangle/10k; spending 40 credits was blocked for Claude - Evan to press it or OK it.

## Nov 9 09:25 (Claude) B200 on TestFlight; B201 building
- B200 EXPORT_UPLOAD_RC=0. Contents: GATTLING RAPID = +10 crystals (10 REACTIONs, opens one at once if rolling; -ZoogiRapidBolts = the old tap-bolts + tap circles); raid locators (radar left, live boss window via ASceneCapture2D every 3rd frame, red edge glow + aim dashed line; -ZoogiNoRaidLocator); Frozen sky cables removed (AerialDecor hanger chains/cables; -ZoogiDecorCables = back).
- B201: raid turn challenges in ZoogiBounty (DOUBLE TAP / STAR STRIKE / BANK SHOT / INTERRUPT / ARMOR BREAK / SAFE ZONE) with raid payouts; ZoogiRaid BonusDamage/SpawnArmor/Expose, ZoogiBossAttack Pending/InDangerZone/Interrupt. -ZoogiRaidChallenge=<prefix> forces one; -ZoogiRaidOldChallenges = old pool. PC tests raid_ch_armor / raid_ch_bank (plates + shards seen; auto play did not complete one). Reward line on the plate edited after the PC compile (small).
- Note: sed 200->201 on gob*go.py breaks creationflags (0x00000200) - fix after copying.
- Evan: "build but always show the preview" - send preview frames with every build.

## Nov 9 10:35 (Claude) B201 on TestFlight; B202 in progress
- B201 EXPORT_UPLOAD_RC=0 (raid challenges).
- Tripo: turret retopo (35 cr, 9.2k tris), Dark Zoogi ("stylized dragon warrior" clone) retopo (35 cr, 9.9k), Tally Golem exported as is (27k quads). FBX 2k in C:\dev\art\{turret_tripo,dark_zoogi_tripo,meadow_golem_tripo}. Imported (ios/import_turret.py) -> /Game/Zoogis/Turret/SM_Turret, /Game/Zoogis/Dark/SM_DarkZoogi (static, no rig yet), /Game/Zoogis/MeadowGolem/SM_TallyGolem, /Game/Zoogis/Balls/T_Ball_Dark.
- B202 code: turret = Evan's model (-ZoogiOldTurret placeholder), turns to the nearest rival/orb, one slash at a time from the barrel tip (NS_EzrealSlash02 riding a carrier sphere, Blast on arrival), recoil jolt + rattle + haptic (-ZoogiTurretBolts = old bolts; -ZoogiTurretYaw= model facing offset, default -90). New hero "Dark" (roster darkzoogy, owned, Awaken CRYSTAL BARRAGE = gattling_burst, palette navy/orange, ball T_Ball_Dark, art zoogi_dark/ball_dark). Teaser Arena removed from map cards + royale rotation (-> Meadow).
- Git private: add_repo push denied for Claude; Evan to flip evanjamesuh/ZoogiRollUE to private in GitHub settings.
- Double-click launched gob202 twice: second died at the import guard (import_turret_run.py O_EXCL). p5run.py has a b202-only guard at the top (harmless).

## Nov 9 11:12 (Claude) B202 on TestFlight; B203 -> iOS
- B202 EXPORT_UPLOAD_RC=0 (turret model + slashes, Dark Zoogi, golem import, teaser removed).
- B203: inventory BALLERS "3D VIEW" toggle (SZoogiFront::b3D; AZoogiFrontStage gets a static Model comp - Dark shows SM_DarkZoogi, others the Aurex; -ZoogiModelYaw=); GATLING item = GATLING TURRET (BeginWeapon gatling -> gattling_turret; art Data/MenuArt/Weapons/w_gatling.png); gattling_turret + stone_pillar + earthquake + rock_rampart retired from SKILLS (ZRetiredSkill + AllSkillIds); DARK REALM map card = Cosmic + ZoogiUnderrealm::SetPicked (card art ms_map_darkrealm(.png/_p.jpg)); -ZoogiDarkRealm test flag. gob203go.py has an O_EXCL guard against double launches (delete ios/b203go.excl to relaunch).

## Nov 9 11:25 (Claude) B203 on TestFlight; B204 -> iOS
- B203 EXPORT_UPLOAD_RC=0.
- B204: BANK! P5 preview card x1.7 (ZoogiHUD DrawBankP5, -ZoogiBankPreview=<k>). Preview from b88_check aim frame.
- Skill-activated banner options A-F (demo videos being made: scratchpad st/BANNER_option_*.mp4) - waiting for Evan's pick.

## Nov 9 12:35 (Claude) B204 on TestFlight; B205 -> iOS
- B204 EXPORT_UPLOAD_RC=0 (BANK! card x1.7).
- B205: bank trail (path dots past the bank point) x2 and shown in clean aim too (ZoogiHUD DrawPreview, -ZoogiBankTrail=k); SKILL ACTIVATED banner = Evan's option F (edge flash in caster colour + side card with icon/name/caster/ACTIVATED!, ZoogiSkills::LastCast + ActionFun DrawHud, -ZoogiNoSkillBanner). Card name auto-fit edited after the PC preview (small).
- Not verified: the bank trail (no test aims into a wall).

## Nov 9 14:01 (Claude) B205 on TestFlight; B206 -> iOS
- B205 EXPORT_UPLOAD_RC=0 (skill banner F, bank trail x2).
- B206: Meadow titan = SM_TallyGolem riding the hidden peek puppet (ZoogiCosmosBoss Spawn; -ZoogiOldGroveTitan, -ZoogiGroveYaw=, -ZoogiGroveUp= default 700). Preview b206_meadow: golem seen behind the rim + in the raid boss window.
- Raid HUD clutter / how-to-hurt / loading screen option videos (RAIDHUD_option_A-C, RAIDHUD_D_how_to_hurt, LOADING_E_raid, LOADING_F_classic) sent - waiting for Evan's pick.

## B207 (Nov 9 15:02) - Raid HUD A + How-to-Hurt D + loading screens E/F (Evan: "Raid hud A, yes to both loading screens and how to hurt yes put these in game")
- ZoogiRaid.cpp: DrawPlate = ONE boss plate (face Data/MenuArt/Raid/face_*.png, name, HP w/ 66/33 ticks + white trail, SHOWTIME + ANGER thin meters, 5 hearts, ROUND pill) + one ticker slot (event line 3 s > BossAttack::Warning > anger ready > exposed). HowTo(kind) cards 3 s under the plate (queue, one at a time). ShowtimeHit: raid SHOWTIME finale grade = damage (6%..30% of max). -ZoogiOldRaidHud = old bar.
- ZoogiBossAttack: event callouts -> HowTo cards / Ticker in raid; Countdown popup off in raid; anger bar off in raid; new AngerFrac(), Warning().
- ZoogiGattling: SHOWTIME meter off in raid (plate shows it); HowTo(5) at start; ShowtimeHit at the showdown.
- NEW ZoogiLoadScreen.h/.cpp: post-render canvas overlay; StartMatch holds in Forming (GZoogiLoadHold) until TAP; AZoogiArena::IsForming() true while holding (dragon intro waits). Armed from ModeSelect StartMapCard; -ZoogiLoadScreen for cmdline tests (auto-start with -ZoogiAuto, shots load_*); -ZoogiNoLoadScreen off.
- Art: Content/Data/MenuArt/Raid/{raid_frozen,raid_generic,face_frozen,face_generic}.png
- Runs: b207_raid, b207_classic. NEWS B207 added.
- B206 iOS FAILED at UPSTAGE: "Connection closed by 216.39.74.244 port 22" - MacinCloud refusing SSH (likely unpaid balance). B206 never reached TestFlight; B207 iOS will fail the same until Evan pays.
- 15:20 B207 PC preview OK (load screens, plate, cards, ticker) -> preview sent. iOS: COOK ok, UPSTAGE refused (Mac still closed after Evan paid at 15:19).
- 16:03 launched ios/b207r.py (via run_anc -> b207rgo.py): retries ssh every 2 min up to 90 min, then ios/mac_backup.py (pulls ~/zr, provisioning profiles, login keychain, listing -> C:\dev\mac_backup\mac_pull), then cook/upstage/sources/build/upload. Log: ios/b207r_ssh.txt, ios/b207ios_status.txt.
- C:\dev\mac_backup already holds the PC-side copies: all_signing_identities.p12, AuthKey .p8, mobileprovisions, build3.sh, cert.py, ssh key.
- 16:37 B207 ON TESTFLIGHT (EXPORT_UPLOAD_RC=0). Mac back after Evan paid. mac_backup done (mac_pull/mac_home_small.tgz 3.4 GB + listing; 2 valid codesign identities).

## B208 (Nov 9 16:47) - Evan's Tripo Ballers in the 3D view ("add model to game in ballers 3d view")
- Tripo exports (FBX 2k, no skeleton, no credits spent): art/blue_tripo (ice crystal robot, 19k) -> FROSTI; art/pinpoint_tripo (ice crystal creature, 1.9M tris) -> BLUE; art/luma_tripo (fantasy armored fox, 19k) -> LUMI; art/vex_tripo (fiery armored character, 19k) -> VEX; art/bolt_tripo (stylized 3d character yellow, 2.0M) -> HOTSTREAK (model sheet = yellow/black Hotstreak; no Bolt in the Front heroes).
- ios/import_ballers.py -> /Game/Zoogis/Ballers/<Name>/SM_<Name>, textures 1024, LOD0 reduced to ~40k tris when huge.
- ZoogiFront ApplyHero: TripoBallers table (Dark + 5). -ZoogiNoTripoBallers = Aurex robot. Test -ZoogiBallerTour (front test step 7 cycles the 6, shots baller3d_*). Run b208_ballers. NEWS B208.
- 17:01 reduced in full editor (ios/reduce_ballers.py via -ExecutePythonScript): Blue 1.31M -> 29k verts, Hotstreak 1.30M -> 35k. Blue inventory icon = Content/Data/MenuArt/Front/zoogi_blue.png (head of the new Blue model, was falling back to the ball).
- 17:05 gate b208_cards_ok2 opened -> iOS 208.
- Home/splash/walkthrough option mockups sent: /mnt/user-data/outputs/home_opts (generator scratchpad/st/homeopts.py). Waiting for Evan's pick.
- 17:25 B208 ON TESTFLIGHT (EXPORT_UPLOAD_RC=0).
- 17:26 clean home background: -ZoogiFrontClean (Front OnPaint draws nothing) -> proofs/p5_style/home_clean/02_front_03_home.png (run home_clean, 70 s).
- Sent 6 splash options (/mnt/user-data/outputs/splash6, scratchpad/st/splashp5.py - Evan's Marble Clash layout in P5/Zoogi style) + 6 walkthrough options (/mnt/user-data/outputs/home6/WALK_*, on Home B).

## B209 (Nov 9 17:3x) - Evan picked HOME B (hero + LOADOUT card)
- ZoogiFront: ZHomeB() (-ZoogiOldHome = old). PaintLoadout(): LOADOUT card right (Baller/Zoogi/Item/Skill1/Skill2 rows, POWER total, green CHANGE -> Go(1)+Tab), SHOP slab under the card (NO ADS burst removed from home). Hero (stage robot / Tripo model) shown on Screen 0 at DW*0.40; name ransom over it. Landscape only (portrait home unchanged). NEWS B209.
- 17:39 B209 Home B preview OK (front_land 02_front_03_home) -> gate b209_cards_ok2 opened -> iOS 209. Portrait home still the old layout.
- 17:53 B209 ON TESTFLIGHT.
## B210 (Nov 9 18:45) - raid locator A EDGE ARROW (Evan: "Add edge arrow"): ZoogiRaid DrawEdgeArrow - off-screen: red plate (face, distance, HP) on the screen edge (kept between 0.24Ht and 0.86Ht) + arrow tip toward the weak point; on-screen: red lock-on brackets + WEAK POINT n m. -ZoogiNoEdgeArrow off. NEWS B210. Preview b207_raid OK -> gate opened 18:50.
- 19:03 B210 ON TESTFLIGHT.
## B211 (Nov 9 19:14) - Evan: "splash first then WALK 3 - PLAY TO LEARN (with skip), after WALK 6 - BLUE BUDDY"
- Splash: Evan's image -> Content/Data/MenuArt/splash_zoogi.png; SZoogiFront OnPaint draws it (contain-fit, black, fade) 3.4 s or tap, once per launch (GZSplash*). -ZoogiNoSplash off, -ZoogiSplashTest in front tests.
- NEW ZoogiTutorial.h/.cpp: first launch (GameUserSettings [ZoogiRoll.Controls] TutorialDone) -> classic you + 1 CPU with ZoogiGuide armed (Trainer Frosti guided first turn) + FIRST MATCH checklist (aim+roll / knock out an orb / use a power, fallbacks after 4 human turns) + SKIP TUTORIAL button -> YOU'RE READY! -> ReturnToMenu. GameOver also marks done. -ZoogiTutorialTest / -ZoogiNoTutorial.
- Blue buddy (PaintBuddy, landscape home, after TutorialDone or -ZoogiBuddyTest): Blue head + tip bubble, tap = next tip, the last tip replays the tutorial.
- Runs b211_first, b211_buddy. NEWS B211.
- B212: buddy bubble 3-line wrap fix (B211 bubble line 2 overflowed).
- 19:46 B211 ON TESTFLIGHT. 19:47 B212 buddy preview OK -> gate opened.

## Nov 9 20:03 - B213 (Claude): solid earth bends + side blast
- Evan: "earth bending effects ... solid while they're there ... bank off / smash an enemy into it ... blast from the side of the rock, not from the ground".
- ZoogiEarthEvents: AddSolid() (skill earth bends get a BlockAll box/post while their FX shows, armed once nothing is inside), TickSolids, SideBlast()
  (Big_Explosion / lab ko_orb blast turned to face OUT of the rock at the hit point + throw-back; credit kept). Event rocks use SideBlast too.
  Flags: -ZoogiEarthGroundBlast (old), -ZoogiNoSkillRockSolid, -ZoogiShowEarthCol (pink cores), -ZoogiEarthSlamTest.
- ZoogiSkills ExecPack: Earthbender Kind 1/2 posts, Kind 3 wall box along the swipe.
- Runs b213_slam, b213_wall. Backups ios/bak_*_b212.*. Gate b213_cards_ok (open only after B212 upload done).
- Pending: Awaken = sky vortex -> meteor touchdown -> current awaken -> multi explosions (B214; meteor on the picked target unless Evan says).
- 20:20 B213 iOS started (SOURCES_UP 20:19). Then added ZoogiDemoReel.cpp/.h (-ZoogiDemoReel previs: burnout roll + tire smoke,
  Blue/Vex Tripo model fight, fly, smash Vex into a rock with the side blast) hooked in ActionFun; run b213_demo via gob213d (film -> mp4).
  Evan also asked for a list of arena bosses to buy (Fab research, no purchases).
- 20:26 B213 on TestFlight (EXPORT_UPLOAD_RC=0). Demo reel: -ZoogiFilmNoUI added to ZoogiFilm (clean frames); demo hides props
  near the path; Smoke_v2 removed (rendered as giant purple swirls). Final reel proofs/film/zoogi_demo_reel_b213.mp4 (gob213d..g runs).
  The demo is test-only (flag-gated), not a game feature yet; tire smoke reads faint. Boss shopping list sent (Fab, nothing bought).

## Nov 9 23:40 (Claude) B214 staged (NOT launched) + trailer A
- Tripo exports (free, FBX 2k): stone golem -> art/golem_tripo (basecolor hue-shifted to purple, original kept as basecolor_original.jpg),
  crystal brute -> art/brute_tripo. ios/golem_import.py (commandlet) + golem_reduce.py (full editor, golem 1.9M faces -> ~45k).
- ZoogiCosmosBoss Spawn: Meadow titan = /Game/Zoogis/MeadowGolem2/SM_PurpleGolem (fallback Tally; -ZoogiOldGroveTitan),
  DARK REALM (Cosmic + Underrealm) titan = /Game/Zoogis/DarkRealmBoss/SM_CrystalBrute (-ZoogiOldDarkTitan). NEWS B214.
- gob214.py = import -> reduce -> p5run b214_meadow,b214_dark -> gate b214_cards_ok -> b214ios.py. run_anc.bat -> gob214go.py.
  NOT started: File Explorer windows stopped showing to computer use (23:30). Evan asked to double-click C:\dev\run_anc.
- Trailer option A (sizzle, 54 s, sound) proofs/trailer/zoogi_trailer_A_sizzle.mp4 via proofs/trailer/build_trailer.py (old Nov 4 film clips +
  b213 demo reel, cards in proofs/trailer/cards, Theme_Arena1 + SourceArt SFX at picture-detected impacts). Real gameplay audio capture not built yet.

## Nov 10 04:00 (Claude) B214 -> iOS; trailer footage with audio
- Evan clicked run_anc 03:29. Import OK (golem 1.76M -> 38k verts), previews b214_meadow/b214_dark OK (purple golem + crystal brute), iOS SOURCES_UP 03:56.
- ZoogiFilm: every clip records the master mix (UAudioMixerBlueprintLibrary Start/StopRecordingOutput -> <clip>/audio.wav, frame 0 = audio 0);
  -ZoogiFilmNoAudio, -ZoogiFilmNoUI also hides the canvas HUD, -ZoogiFilmAt/Len/Name timed clip, -ZoogiFilmBoss=<s> via ZoogiFilm::Moment on titan slam/sweep.
  Demo reel now plays SFX (burnout, hits, punch, rock smash) and uses FOV 50 in portrait.
- Trailer footage runs tr_* (p5run) after SOURCES_UP; cutter proofs/trailer/cut.py A|B|C|D|E (cards in proofs/trailer/cards, Evan's new logo = intro/end_logo/e_end).

## Nov 10 07:45 (Claude) B214 on TestFlight; trailers A-E; rock blast options
- B214 EXPORT_UPLOAD_RC=0 (purple Meadow golem, Dark Realm crystal brute, film audio).
- Trailers (proofs/trailer, cut.py A-E): A sizzle 45 s, B boss raid 40 s, C meet the Ballers 39 s, D how to play 43 s, E vertical teaser 12 s.
  Awaken clips re-shot via ZoogiShowtime Begin -> ZoogiFilm::Moment("Awaken") (-ZoogiFilmAwaken=11, -ZoogiShowtimeTest). Some clips still show HUD bits.
- Rock impact blast styles (Source, NOT in a phone build yet): ZoogiEarthEvents BlastStyle 1-8 (-ZoogiRockBlast=<n>, default 1 = BIG BLAST),
  BlastStyleAt(); demo rig -ZoogiDemoReel -ZoogiDemoBlasts films all 8 (run tr_blastopts, opts.py -> rock_blast_options.mp4).
  Also in Source from earlier: rock slam manga impact frame (Vefects V09 default, -ZoogiRockImpactFrame=0 off) + -ZoogiImpactFrameTour.
- Waiting on Evan's pick of blast option; then set the default + build B215.

## Nov 10 08:40 (Claude) B215 on TestFlight
- Meadow / Dark Realm Tripo titans faced sideways: Gc->SetRelativeRotation(GolYaw -90) had been left inside a // comment in ZoogiCosmosBoss Spawn. Fixed.
- Rock impact manga frame default OFF (-ZoogiRockImpactFrame=<1-10> to turn on; Evan meant the BLAST). Blast style default 1 until Evan picks 1-8.
- Preview b215_meadow (bosscam frames show the golem facing the court). EXPORT_UPLOAD_RC=0.

## Nov 10 (Claude) B216 on TestFlight
- Meadow golem back to the original Tripo basecolor; stands whole on the rim (6.5 m, -ZoogiGroveH= / -ZoogiGroveUp=), boss cam FOV + raid window
  frame the whole titan (TitanFocus). EXPORT_UPLOAD_RC=0.

## Nov 10 09:10 (Claude) B217 building - golem rig + banners hidden on the boss cam
- Tripo (Evan's OK "no exceptions"): retopo 18k (35 cr) + auto rig Humanoid / UE5 Mannequin skeleton (20 cr) + 9 preset anims
  (idle, look_around, angry_01, chop, slash, hit_to_body_01, defeat_02, cheer, hug). FBX in C:\dev\art\golem_rig.
- ios/golem_rig_import.py (run by gob217 before the editor build) -> /Game/Zoogis/MeadowGolemRig/SK_Golem + AS_Golem_<name>.
- ZoogiCosmosBoss: Meadow uses SK_Golem when present (-ZoogiStaticGolem = the b216 static mesh). Hidden driver USkeletalMeshComponent plays clips,
  visible UPoseableMeshComponent crossfades 0.15-0.4 s between them. idle loop + look_around every 8-14 s; Flinch=hit_to_body_01,
  SlamToward=chop, Sweep on=slash, Taunt/Threat=angry_01, KO=cheer, Defeat=defeat (held). -ZoogiGolemRigYaw= if it faces wrong.
- BossCamOn(): ZoogiActionFun DrawHud + ZoogiHUD DrawHUD return early while the boss cam is live (-ZoogiBossCamHud keeps them).
- Preview run b217_meadow films boss moments (-ZoogiFilmBoss=7).
- 09:22 B217 SOURCES_UP (golem rig + banners hidden). Preview showed the rigged golem near-black: Tripo's FBX material had NO textures.

## Nov 10 09:24 (Claude) B218 - golem stone material
- ios/golem_rig_mat.py: M_GolemRig (rig basecolor + normal, rough 0.85, metal 0, used_with_skeletal_mesh) on SK_Golem. Preview b218_meadow:
  golem brown again, anims playing (proofs/p5_style/b218_golem_animated.mp4 + _review/b218_golem_window_zoom.mp4).
- gob218 waits for b217ios_status DONE before b218ios (the Mac stage dir is shared).
- In Source for the next build (b219, not built): golem slash edge-triggered on Sweep (bSweepWasOn).
- B218 EXPORT_UPLOAD_RC=0 (B216, B217 also 0).

## Nov 10 10:06 (Claude) B219 - robot back, Z centre, no hub pillar, no Meadow raid
- Evan: "i dont like the golem go back to the robot, put the z icon in the center and remove the center pillar" + "remove meadows arena from raid".
- ZoogiCosmosBoss: Meadow titan = Aurex robot (Grove colours) again; -ZoogiGroveGolem = rigged golem, -ZoogiStaticGolem / -ZoogiOldGroveTitan = static ones.
- ZoogiArenaMeadow: SM_MeadowRef_CentreMedallion (Z) shown by default, also in the hybrid ring (-ZoogiNoMeadowMedallion = off);
  hub dais + pedestal + orb cup + AddHubCore crystal skipped (-ZoogiMeadowHub brings them back).
- ZoogiModeSelect: RAID map cards skip MEADOW ARENA (GRaidMaps).
- Also in: golem slash edge-trigger (bSweepWasOn). Preview proofs/p5_style/b219_sheet.jpg.
- B219 EXPORT_UPLOAD_RC=0.

## Nov 10 13:28 (Claude) B220 - item knockouts pay crystals
- Evan: "Knocking a opponent off using the staff or any item should award crystals". ZoogiGameMode CheckKnockoffs KO block: attacker used an
  item this turn (ZoogiItems::UsedThisTurn) -> AwakenCharge +2 (cap 4; -ZoogiItemKOCrystals=), popup "ITEM KNOCKOUT +2 CRYSTALS" + feed + log "ITEM KO".
- Test switch -ZoogiItemKOTest (every KO counts; b221+: also throws P2 off on P1's roll at TurnCounter>=3). Proof: p5_b221_itemko2.log
  "ITEM KO: P2 knocked out P3 ... crystals 2 -> 4".

## Nov 10 13:40 (Claude) B221 - turret fires straight out of its barrel + 6 shot styles
- Evan: "For the turret projectile ... shooting perpendicular and only straight, show me 6 top view options ... confirm bullets shoot from the barrel".
- ZoogiSkills: BarrelShot() - muzzle = turret floor + barrel dir (actor yaw - TurYawOff(-90), the Tripo model's barrel is local +Y)
  * TipFwd + TipUp; flies straight along the barrel (no homing; -ZoogiTurretOldSlash = b202). Styles -ZoogiTurretShot=1..6:
  1 crescent slash (default), 2 tracer burst x3, 3 plasma bolt, 4 laser beam, 5 rocket, 6 shock ring. TurSlashTick once per frame (GFrameCounter).
- Demo rig -ZoogiDemoReel -ZoogiDemoTurret (-ZoogiDemoTurretCamH=, top view): run tr_turretopts2; reel proofs/trailer/turret_shot_options.mp4
  (opts.py <run> <names> <out> _turret inset) + turret_shot_options_grid.jpg. Waiting on Evan's pick -> set the default.
- Scripts: gob221 (build+tests+iOS), gob221v (preview re-shoot only). run_anc currently -> gob221vgo (b221vgo.excl used).
- B220 EXPORT_UPLOAD_RC=0; B221 EXPORT_UPLOAD_RC=0.

## Nov 10 14:20-14:55 (Claude) B222 / B223 / B224
- B222: turret default shot = 5 ROCKET (Evan: "turret = rocket"). EXPORT_UPLOAD_RC=0 to check in tail222.
- B223 (Evan 14:28/14:30): turret 10 shots in all (FTurret::ShotsLeft, gone after the 10th lands, max 5 owner turns); targets the nearest orb /
  rival, + the boss in a RAID (ZoogiRaid::Active + BossAttack::LockOnTarget, reach 2600, BarrelShot bBoss -> CosmosBoss::Flinch + HitBoss);
  inventory item card for GATLING TURRET has an AMMO row (ROCKET / TRACERS / PLASMA / LASER / RING -> profile ItemCounts["turret_ammo"] 2..6,
  human only; CPUs rocket; -ZoogiTurretShot= forces). GATTLING RAPID = FDashRun: 10 dashes, no pause - tap the court (RapidTapPoint, not the pad)
  sets the spot, SUPERPOWER (ClassicPad Press -> ZoogiSkills::DashGo) dashes there at max(2000, speed) (-ZoogiRapidDashSpeed=, -ZoogiRapidDashes=);
  pad label "DASH xN"; CPU: 3 dashes at the nearest rival. -ZoogiRapidReaction = the b200 REACTION version. Tests: -ZoogiAmmoTour (front),
  -ZoogiRapidDashTest. Proofs b223_turret (10 shots then expires), b223_raidturret ("fires at the BOSS"), b223_ammo.
- B223 BUG (fixed in B224): DashActive() made the PlayerController ignore ALL taps - casting RAPID while aiming blocked the launch.
- B224: PC only gives the taps to the dash run while Rolling; shorter turret card text; -ZoogiRapidDashTest starts a run on the human's roll.
  Proof p5_b224_dash.log: 10 dashes 0.7 s apart. Film roll needs -ZoogiFilmRoll=<s> (bare flag = 0 s, no clips).
- B222, B223, B224 EXPORT_UPLOAD_RC=0. B224 = the build to test (B223 has the aim-block bug when RAPID is cast before launch).

## Nov 10 15:50-16:05 (Claude) B225 + loot card options
- B225: GATLING TURRET in the Skill Lab (Skills[] + rows FIRE RATE / SHOTS / AMMO when it's selected; FTune Shots/Ammo/Rate saved in
  SkillLab.ini as fields 9-11). ZoogiSkillLab::TurretShots/TurretAmmo/TurretRate/SizeOf. Turret: ShotsLeft = lab SHOTS, NextShot 0.35/RATE,
  lab AMMO (not INVENTORY) overrides, lab COLOUR + FX SIZE on the shots, fires in the lab while Aiming, lab spot between caster and rack.
  -ZoogiLabOnly=<skill> (with -ZoogiSkillLabTest) PLAYs one skill. Preview proofs/p5_style/b225_lab_turret.mp4.
- Inventory copies (xN) options A-D (corner pill / name-bar tab / element+count strip / upgrade progress bar), drawn with the art cropped from
  Evan's Grok mockups (Nov 10 15:47): proofs/loot_cards (cloud script make.py). Waiting on Evan's pick. Chest-droppable skills = 22
  (Cards.csv minus retired / event-only / fire_meteors), copies to level up 2/4/10/20/50/100/200 (UpgradeCopies).
- B225 EXPORT_UPLOAD_RC=0.
- B226 (16:13): GATTLING RAPID renamed ORB RELAY (Cards.csv gattling_beat name + rule, callout "ORB RELAY!"). Preview proofs/p5_style/b226_orb_relay.mp4.
- Evan 16:05: "Skills don't have counters only items do" -> item options redone (proofs/loot_cards/items_option_A-D). Evan picked ITEMS option A.
- B227 (16:26): item cards show the carried count in a gold pill top-right (FItem::Count, Tile + PaintItem); GOLDEN APPLE item card added to
  the ITEM tab (icon CardArt/golden_apple). Preview proofs/p5_style/b227_items.jpg.
- Crowd / award banner concept options sent (cloud scratch loot/crowd.py, banner.py): crowd A toon fans, B sign holders, C team sections +
  flags/foam fingers, D card stunt Z; banners 1 hanging stadium banner, 2 gold ribbon + laurel (MVP), 3 fan-held banner. Waiting on picks.
- B226 + B227 EXPORT_UPLOAD_RC=0.

## Nov 10 18:01 (Claude) B228 -> iOS (SOURCES_UP 17:58); B229 building
- B228 previews: toon crowd OK (281 fans) but the 76 signs rendered BLACK: M_CosmicPlanet has no bUsedWithInstancedStaticMeshes
  (log "Default Material will be used in game"). ITEM tab pill read dark; chest items were ignored when the profile was not seeded yet.
  b228_relay test hook dashed past 0 (-2 left) and restarted every frame once the ball was out (test-only).
- B229 (ios/b229_patch.py + direct edits):
  - Crowd signs = one plain UStaticMeshComponent each (SignComps), not ISM. -ZoogiCrowdCam = close camera on a sign fan, cheering.
  - ITEM pill solid gold, dark number (option A). RollChest seeds the starting items first + logs "CHEST item".
  - ORB RELAY: MaxLeft set before the copy; -ZoogiRapidDashTest aims at the nearest orb, stops at 0, once per round.
  - Evan picked ROCK BLAST option 4 = FIRE BLAST: ZoogiEarthEvents BlastStyle default 4 (-ZoogiRockBlast=<n> still overrides).
  - Evan approved the demo reel's orb spin + tire smoke for the game: AZoogiBall::TireSmoke (puffs behind the ball while the
    aim spin runs, burst on Launch; -ZoogiNoTireSmoke off). NEWS B229.
  - Runs b229_rock, b229_crowd, b229_relay, b229_front. run_anc.bat -> gob229go.py (started 18:01).

## Nov 10 18:30 (Claude) B229 -> iOS (SOURCES_UP 18:20); B230 + B231 (wall-smash cinematic)
- B228 on TestFlight (EXPORT_UPLOAD_RC=0). B229 previews: signs show words but upside down; gold pill + "+2 NEW" OK; relay refunds OK.
- Evan: "bring back the cinematic animation of a model getting blown back into a wall then provide video options of when people would see that".
  New ZoogiWallCine.h/.cpp (sources kept in ios/cine): Play(G, Attacker, Victim, Kind) - letterbox, both Tripo Baller models
  (Blue/Vex/Hotstreak/Frosti/Lumi/Dark by Def.Name), punch, victim flies into a spawned rock, hit-stop, BlastStyleAt (FIRE, option 4),
  bounce + KO, push-in on the winner. Balls frozen + restored, props/orbs hidden + restored, HUD off (ActionFun/ZoogiHUD), human test held.
  Kinds (trigger options): 1 MATCH POINT KO, 2 AWAKEN KO, 3 WALL SLAM, 4 FIRST KO, 5 MEGA HIT, 6 REPLAY. -ZoogiWallCineTour films 1-6
  (cine_<n>). NOT wired to real events yet - waiting on Evan's pick. -ZoogiNoWallCine off.
- B230 (b230_patch.py): cine + signs SignSpin default 180 (b230_crowd confirms WOW! / VEX #1 / 10/10 read right).
- b230_cine v1: UMG widgets (portraits, action menu, bubbles) + a big arch blocked the shot -> B231 (b231_patch.py): hide UMG widgets
  every frame (restored after), orbs hidden, props near the three camera lines hidden (stands/crowd kept). Clip length Len+1.4.

## Nov 10 19:10 (Claude) B231 + B232 (clean cine), wall cine options video, Voyager TPS download
- B228 B229 B230 B231 EXPORT_UPLOAD_RC=0. B232 (b232_patch.py: Slate OnPaint guards in P5Command / ClassicPad / P5Style / AnimeCues /
  FrameTrail -> nothing over the cine) SOURCES_UP 19:06.
- b232_cine (first try) had 0-frame clips 2-6 + "Slate: Had to block on waiting for a draw buffer" spam; the re-film b232_cine2 (gob232v)
  was clean (6 clips, no blocks). Watch for it.
- Options video proofs/trailer/wall_cine_options.mp4 (+_sheet) via opts.py b232_cine2 ... "" (6 trigger options). Waiting on Evan's pick.
- Evan bought Fab "Voyager: Third Person Shooter v3" (Game Dev Voyager, all Blueprint, GASP motion matching, 9 guns, AI, cover, traversal,
  Windows-only targets). Launcher > Unreal Engine > Library > Fab Library > Create Project (5.6) -> C:\dev\Voyager\VoyagerThirdPersonShooter.uproject
  (the Fab-tab Download dialog errors for this template; "Project files not associated" prompt answered No Thanks).
- ios/voyfilm.py (run_anc -> voyfilmgo.py): after B232 DONE, holds unreal.lock; per demo level warm-up -game until ShaderCompileWorkers quiet,
  then -benchmark -fps=30 -dumpmovie -> proofs/voyager/<Level>/f_*.png (no audio). Kills only its own PID. Status ios/voy_status.txt.

## Nov 10 20:00 (Claude) B232 on TestFlight; Voyager recorded
- B232 EXPORT_UPLOAD_RC=0. voyfilm.py: -dumpmovie wrote NO files (only "Tracing Screenshot" lines) - don't use it; warm-ups done for
  LevelWeapons, WaveAssault, ModularCharacter, AdvancedAI, Cover (shaders/anims cached in C:\dev\Voyager).
- Evan OK'd ~/Videos/Captures (connected). Played Voyager via computer use (UnrealEditor granted full + system key combos) with Game Bar
  Win+Alt+R; ios/voyplay.py (level from ios/voyplay_level.txt, stop file ios/voyplay.stop, excl voyplay_<Level>.excl). run_anc -> voyplaygo.py now.
- proofs/voyager/raw_weapons.mp4 (82 s), raw_wave1/2.mp4, voyager_demo_raw.mp4 (56 s cut) sent. Mouse look by absolute moves is clumsy.
- Waiting on Evan: which Voyager parts go where (cine characters + ragdoll, Royale hero, guns/VFX as items, spider boss/companions).

## Nov 10 20:37 (Claude) B233 - Voyager TPS in Zoogi Roll (Evan: "A,B,C,D implement all")
- Migration (ios/voy/voy_migrate.py run IN C:\dev\Voyager via voy_migrate_run.py; PythonScriptPlugin + EditorScriptingUtilities added to its uproject):
  393 roots + deps (850 uassets) -> ZoogiRoll /Game/Voyager/... ; 292 anim sequences duplicated to /Game/ZoogiExport/Anims with notify tracks
  stripped (Voyager notifies are BPs). Roots whose hard closure holds a Blueprint are skipped (none). Inventory proofs/voyager/inventory.json.
  Characters = UE4 mannequin skeleton (SK_MannequinMainUE4M); spider own skeleton. DefaultGame.ini AlwaysCook += the used Voyager folders.
- New ZoogiVoyager.h/.cpp: Character(name) (Blue->Hero, Vex/Dark->ShadowOps, Hotstreak->MurdockWasteland, Frosti->WraithLunarOps, Lumi->Murdock,
  else Trooper), Anim(rel), Gun(short), Fx(short) via AZoogiFabFX, Tick = RAID crew (Trooper + HMG, Voyager minigun turret; tracers,
  every 10th hit flinches the boss, no damage). -ZoogiNoVoyager / -ZoogiNoRaidCrew / -ZoogiRaidCrewOut=.
- A ZoogiWallCine: Voyager fighters (Fighting_Idle, Punch_Cross, Hit_Chest, Sword_Cheer), victim ragdolls off the rock (-ZoogiCineRagdoll=650,
  -ZoogiCineTripo = b230 statues).
- B ZoogiRoyaleHero: Voyager character + Soldier rifle clips (r_* keys), Voyager HMG on the gun root (-ZoogiVoyGunYaw=, -ZoogiOldGatling,
  -ZoogiNoVoyagerHero).
- C ZoogiSkills turret: Voyager MachinegunTrail / PlasmaTrail / RocketTrail / Tracer + ImpactConcrete / PlasmaBoom / ImpactMetal / RocketBoom.
- D ZoogiCosmosBoss: Tomb titan = SK_Greater_Spider on the rigged-golem path (GSpiderRig maps the clip names; -ZoogiSpiderH=420,
  -ZoogiSpiderYaw=, -ZoogiTombTitan = old). Banner TOMB SPIDER.
- First compile failed (a // comment ate the Tomb ThemeFor line) -> /* */; attempt 2 BUILD_RC=0 20:37. Runs b233_cine, b233_royale, b233_raid.

## Nov 10 22:20 (Claude) B234 - B237 (shooter modes, Zoogi look, Episode 5, Evan's 21:48 / 22:01 / 22:04 changes)
- B233 (Voyager A-D), B234 (shooter modes), B235 on TestFlight. b234ios poller crashed AFTER the upload (cp1252 write of tail234.txt) -> no DONE;
  fixed by hand + tail writes are utf-8/errors=replace from b235ios on. ios/ioscheck.py (run via run_anc) = ssh tail of the Mac build logs.
- B234: ZoogiShooter.h/.cpp on the royale engine: Kind 1 HOLD THE LINE (waves, wave 5 x1.6 HP), 2 SHOOTER ROYALE, 3 TEAM DEATHMATCH (P1+P2 vs P3+P4,
  first to 20, -ZoogiTdmGoal=). -ZoogiShooter=<n> with -ZoogiRoyale. Mode Select cards (reuse ms_card_royale art). Royale mix off in shooter.
- B235: ZoogiVoyager::Dress (shoulder pads + chest core in Def.Color/Secondary; Voyager mats only expose EffectColor/FlashColor so the armour
  itself is NOT tinted yet). STORY EPISODE 5 "RING GUARD" (Chapters/Spots/Missions/Dialogue.csv; mission modes shooter1/2/3 in ZoogiStoryMode.inl).
- B236 (Evan 21:48): spider boss off (-ZoogiTombSpider), raid crew off (-ZoogiRaidCrew), wall cine back to Tripo statues (-ZoogiCineVoyager),
  Voyager hero only in shooter modes; soldier head = the Zoogi's Tripo Baller sliced at 62% height at run time (UProceduralMeshComponent;
  ios/voy/head_cpu.py set allow_cpu_access on the 5 SM_ Ballers; -ZoogiHeadCut= / -ZoogiHeadCm= / -ZoogiHeadDown=); crystals: orbs collected ->
  3 crystals = TRANSFORM to ball dash 2 s per crystal (ZoogiShooter::AllowTransform; -ZoogiShooterDashTest); 5 lives in Hold the Line.
- B237 (Evan 22:01 / 22:04): six hill villages + far mountains around Meadow (ZoogiArenaMeadow, -ZoogiNoMeadowVillages); Battle Royale and the
  shooter modes on Meadow (not Ancient); shooter orbs collect on every arena (no respawn off Ancient); shooter aim = crosshair ray (no lock),
  30-round magazine + 1.5 s reload (ZoogiShooter::TryShot via RoyaleAttack), Voyager ImpactMetal on hits, wider shoulder camera.
- NOT verified: Tripo head cut height / facing per model (no clean close-up yet), villages from the gameplay camera, Episode 5 missions 1-2 played
  through, shooter controls on a phone. Evan still owes: wall-cine trigger pick (1-6), award banner pick (1-3).

## Nov 11 01:05-01:40 (Claude) B238 -> iOS
- B229-B237 all EXPORT_UPLOAD_RC=0 (see ios/tailNNN.txt). Voyager pack at C:\dev\Voyager (Fab), partial migration under Content/Voyager
  (meshes / anims / FX only - NO Blueprints). ZoogiVoyager / ZoogiShooter (3 shooter modes on the ROYALE engine), Episode 5, Meadow villages.
- Evan 01:04: "Wall smash moment 1-5. The award banner 1". 01:06: "the new inventory and card style, why aren't those updated?"
  01:14: shooter modes "nothing like the voyager demo ... I get moved by enemy shots ... can't hide behind cover or move like voyager".
- B238 (ios/b238_patch.py, ios/b238_inv_patch.py, files ios/b238/):
  - ZoogiWallCine::Offer / OfferKO / NoteAwaken: real triggers. KO block (ZoogiGameMode) -> kind 1 (last round) / 2 (attacker awakened this
    turn, CardsUseAwaken) / 4 (first KO); ZoogiEarthEvents SideBlast ROCK SLAM -> 3; fun Grade PERFECT HIT on a rival -> 5. Queued, plays
    at the next Aiming (PhaseTime > 0.35). Limits per match 1/2/2/1/2, 5 total, not two turns running. -ZoogiWallCineAlways, -ZoogiWallCineMax=.
    A KO on the very last turn is dropped (GameOver) - not yet played before the results.
  - ZoogiAwardBanner (new): OnRoundEnd from the round advance; KNOCKOUT KING / ORB HUNTER, drawn by the fun HUD canvas (FunTextL).
    -ZoogiNoAwardBanner, -ZoogiAwardBannerTest. KNOWN: canvas sits under the Slate HUD (turn challenge / showtime strip overlap it) and
    a cine queued for the same turn start hides it -> B239: hide those Slate layers while it shows, start it after the cine.
  - Inventory Grok look (ZInvNew, -ZoogiOldInv): Content/Data/MenuArt/Inv/*.png (generated, cloud scratch inv/gen.py), skill paintings cropped
    from Evan's Nov 10 mockups in Data/CardArt/IconsNew (20 skills; others fall back to Icons). TileN / SkillCardN / PaintSkillsN / InvChip /
    ZBox9 (Slate Box margins are TEXTURE pixels; *_2x frames when S > 1.45). NOT restyled yet: top bar (BACK / title / gems), item popup,
    home + shop. B239 tweaks: pick 3 or 4 columns by the biggest card, smaller LV text, SP row lower in the info panel.
  - Shooter: bolt hits no longer kick or stun in the shooter modes (-ZoogiShooterKnock = old).
  - Proofs: proofs/p5_style/b238_front(_port), b238_live (+ b238_live.mp4), proofs/loot_cards/b238_sheet.jpg.
- NEXT (Evan's 01:14 complaint, the real fix): port Voyager's actual gameplay. Findings: Voyager = Blueprint framework (7711 assets, 5.5 GB),
  UE 5.6 like Zoogi. Needs plugins CommonUI, EnhancedInput, AnimationWarping, PoseSearch, AnimationLocomotionLibrary, MotionWarping, Chooser,
  Mover, FieldSystem, RigLogic, LiveLink(+ControlRig), HairStrands; 9 custom collision channels (GameTraceChannel1-9: Ragdoll, DefaultCharMesh,
  MainCharMesh, Projectile, NonTargetable, WorldStaticOnly, FadingMesh, CoverPlane, TurretMesh) + ~12 profiles (Zoogi has none -> no clash);
  DefaultGameplayTags.ini, DefaultInput.ini; GameInstance BP_MyGameInstance; game mode BP_GameModeDefault; player BP_MainCharParent; cover =
  BP_ActionCover + CoverPlane channel; AI = Addons/AdvancedAI (BT_AdvancedAI). Plan: copy Content/Voyager whole, merge config + plugins,
  run a Voyager level from the SHOOTER cards on PC first, then touch controls, then iOS size / perf, then Zoogi heads + crystal ball dash.
- 01:46 B239 (ios/b239_patch.py) built + previews, queued behind b238ios: banner waits for the cine + P5Style / AnimeCues layers off under it
  (ZoogiWallCine::BannerUp), skills grid picks 3 or 4 columns, LV / SP text tweaks. STILL OPEN: the "SHOWTIME - TAKE THE LEAD" and
  "BOSS ANGER" strips (top centre, another HUD layer) draw over the banner - move or hide them while it shows.
  Proofs: proofs/p5_style/b239_front, b239_live, proofs/loot_cards/b239_sheet.jpg, b239_banner.jpg.
- run_anc.bat -> gob239go.py (do not rerun; next = gob240). Check ios/tail238.txt / tail239.txt for EXPORT_UPLOAD_RC=0.
- 05:26 Voyager port step 1 (ios/voyport.py via run_anc -> voyportgo.py, excl ios/voyport1.excl): Content/Voyager now = the WHOLE Voyager
  content (robocopy, 558 new files kept, existing skipped). Config layer Config/Custom/Voyager (read only with -CustomConfig=Voyager).
  Voyager plugins added to the .uproject only for the run (backup ZoogiRoll.uproject.pre_voy, restored after). RESULT: LevelCover loads
  inside the Zoogi project with BP_GameModeDefault, the Voyager hero and "Press SPACE to Take Cover" - the real Voyager framework runs.
  Screens proofs/voyager_port/LevelCover (a console window covered part of them). B238 + B239 EXPORT_UPLOAD_RC=0.
  NEXT: make it permanent (plugins in .uproject, collision channels + profiles merged into Config/DefaultEngine.ini, gameplay tags, input),
  open a Voyager-rules level from the SHOOTER mode cards, then touch controls + iOS cook size check.

## Nov 11 05:35-06:30 (Claude) Voyager port made permanent -> B240
- Evan 05:34 "Ok continue". Permanent merge (ios/voy2_patch.py, sources ios/voy2/; backups *.pre_voy2):
  .uproject += Voyager plugins (CommonUI, EnhancedInput, FieldSystem, AnimationWarping, RigLogic, LiveLink(+ControlRig), PoseSearch,
  AnimationLocomotionLibrary, MotionWarping, HairStrands, Chooser, Mover). DefaultEngine.ini: Voyager collision channels/profiles,
  PhysicalSurfaces, DDCvars, CrowdManager, renderer (CustomDepth=3, GPUSkin 16bit/unlimited, SkinCache), GameModeMapPrefixes "Level" ->
  BP_GameModeDefault, GameInstanceClass = Voyager BP_MyGameInstance (Zoogi menus + match verified with it). Gameplay tags merged.
  Gravity stays -981 (Voyager maps get -1800 per world). Build.cs + CommonUI/CommonInput/EnhancedInput; UZoogiViewportClient now
  derives from UCommonGameViewportClient.
- ZoogiVoyagerLevels (UGameInstanceSubsystem): SHOOTER cards -> OpenLevel Voyager level with ?game=BP_GameModeDefault
  (1 LevelWaveAssault, 2 LevelAdvancedAI, 3 LevelCover; LevelCity too heavy for phones). Per Voyager map: gravity -1800, luminance range
  cvar 1 (Zoogi maps keep theirs - without it Voyager maps were pure white), EXIT button (Slate, top-left; not clickable on PC while the
  mouse is captured), Voyager LevelMainMenu -> back to Meadow, any other Voyager level (death flow loads LevelCity) or a travel
  failure -> reopen this match's level, ZoogiApplePad::WantFull = Apple GCVirtualController with both sticks, ABXY, shoulders, triggers.
  Flags -ZoogiShooterOld, -ZoogiVoyLevel=n, -ZoogiVoyMap=, -ZoogiVoyShots=dir. p5run runs voy_wave / voy_ai / voy_city.
- PC proof: proofs/voyager_port (sheets, voyager_in_zoogi_cut.mp4 - played via computer use, Game Bar). ios/playvoy.py opens the game
  straight into a level for manual play (15 min cap). NOTE: computer_open_application "Unrealeditor" LAUNCHES A NEW EDITOR - don't.
- B240 = this + NEWS; MapsToCook += the 3 Voyager levels. RISKS: iOS compile of the new plugins, cook size / shader time, phone perf.
- 07:20 B240 FAILED on the Mac: UHT "Unable to find parent class type ... UCommonGameViewportClient" - the iOS scripts upload Source only,
  the Mac ZoogiRoll.uproject had no CommonUI. Fixed by ios/upuproj.py (uploaded the .uproject, Mac copy .pre_voy kept). All Voyager
  plugins exist in the Mac engine mirror (ios/macplug.txt). Future iOS scripts: use ios/b241ios.py.next (adds ZoogiRoll.uproject to the upload).
- B241 (ios/b241_patch.py): VOYAGER card on Mode Select -> HOLD THE LINE / SHOOTER ROYALE / TEAM DEATHMATCH (GVoySel; -ZoogiVoySelTest).
  Carries all of B240. Previews proofs/p5_style/b241_menu, b241_voy.
- 07:53 B241 EXPORT_UPLOAD_RC=0 (Mac-only rerun ios/b241m.py after dropping -SkipRulesCompile from build3.sh; backup build3.sh.pre_voy).
- 08:14 B242 = Voyager card art (Content/Data/MenuArt/ModeSelect/ms_card_voyager / voy_hold / voy_royale / voy_team, made from the PC
  Voyager screenshots; the old card showed the BATTLE ROYALE picture). Uses ios/b242ios.py (uproject upload included).
- Evan 08:22 "Remove the soldier still appearing on the classic, raid, battle royale arenas" -> B243 (ios/b243_patch.py): ZoogiVoyager::
  Character() returns nothing unless a shooter mode is set (-ZoogiCineVoyager / -ZoogiRaidCrew override); Battle Royale refuses the
  robot transform outside shooter modes (everyone stays a ball, -ZoogiRoyaleRobot = old); every non-shooter Mode Select card SetKind(0).
  Couldn't see a soldier in PC classic captures - asked Evan for a screenshot if it still shows.
- B242 EXPORT_UPLOAD_RC=0 (Voyager card art).
- Evan 08:31 arches (photo: green SM_MeadowHedgeArch) + 08:34 dark-lights toggle -> ios/b243b_patch.py, applied 08:38 while B243 cooked, so
  it rides B243's SOURCES_UP WITHOUT a PC compile first (Evan was using the PC screen): ZoogiArenaObstacles skips every obstacle whose
  name / mesh key has "arch" or "gate" (-ZoogiArches = back); ZoogiShowtime::DarkTick + Settings toggle DARK ARENA (setting DarkArena,
  -ZoogiDarkArena) keeps the SHOWTIME dim (lights 0.3x, sky 0.4x, exposure -1.5) for the whole match; Showtime::Begin hands lights back first.
  PC check pending: ios/gob243b.py (runs b243_dark, b239_front) - run_anc points at it.
- Evan 08:38: AI should knock orbs out AND block the opposing player with an earth-bending effect, showcasing using both superpowers in time
  (reaction, momentum, quick thinking). NOT started.
- B243 EXPORT_UPLOAD_RC=0 (no soldiers outside Voyager, BR stays ball, no arch/gate obstacles, DARK ARENA toggle - first version found 0 lights).
- B244 (ios/b244_patch.py + b244b + b244c) EXPORT_UPLOAD_RC=0: CPU earth block (ZoogiCPUSmart::IsEarthBlock; CardValue/WantCastNow;
  CpuCast places the wall across the nearest rival's way to its nearest orb, fires 0.35 s after pick, popup "EARTH WALL - BLOCKED!";
  every CPU hand gets one earth wall, -ZoogiCpuNoEarthBlock). Evan 08:58 "zoogi and orbs still phase through walls": ZoogiEarthEvents
  solids were destroyed on their FIRST frame (Niagara not active yet) - now live >= 4.5 s, arm at 0.25 s (-ZoogiSolidOldLife). Verified
  in p5_b213_slam.log. DARK ARENA now dims light COMPONENTS on any actor + PP SceneColorTint (verified dark on Meadow, proofs b243_dark).
- B245 (ios/b245_patch.py): Evan 09:05 "remove from full screen to pop out side window like the other celebrations but make it the flying
  ball anime" -> ZoogiWallCine::PlayPip: windowed anime cue (P5 border, game keeps playing) Anime_Any_Any_KO_01..03 (flying balls; map
  crash clips with -ZoogiWallPipCrash), title + "A > B" over it (DrawPip); -ZoogiWallCineFull = old 3D. Preview b245_pip_port (crash clip
  version). Portrait window overlaps the P5 command cards a bit. Landscape test run did not trigger (check -ZoogiWallPipTest timing).

## B246 (Nov 11 09:53, Claude) - Voyager touch pad + Zoogi hero
- B245 on TestFlight (UBT_RC=0, EXPORT_UPLOAD_RC=0).
- ios/b246_patch.py: SZoogiVoyPad (ZoogiVoyagerLevels.cpp) - move stick left, drag-look right, FIRE/AIM/JUMP/RELOAD/SWAP/BOMB/MELEE/RUN injected into Voyager IA_* via EnhancedInput InjectInputForAction. Replaces Apple's controller (-ZoogiNoVoyPad = Apple's again). -ZoogiVoyPadTest shows it on PC; -ZoogiPadLook=k, -ZoogiPadInvY.
- ZoogiVoyager::DressAs (Dress is now a wrapper). Voyager hero dressed as the equipped Zoogi (Tripo head + colours), other characters Void hollow. -ZoogiVoyNoDress = off.
- NEWS B246 added. Preview run b246_pad -> proofs/p5_style/b246_pad. Chain gob246go -> gob246 -> b246ios.

## B246 uploaded (EXPORT_UPLOAD_RC=0, 10:21). B247 (Nov 11 ~10:55, Claude)
- ios/b247_patch.py: Voyager pad auto cover (snap into BP_CoverPlane cover on stick release, AIM = IA_AimCover in cover, pull back = IA_LeaveCover, COVER hint; -ZoogiNoAutoCover). Dress skips *Flash* params (keeps hit flash). NEWS B247.
- New Voyager card art from Evan's 10:48 mockups (ms_card_voyager / voy_hold / voy_royale / voy_team; old ones in ios/b247/oldcards).
- Evan 10:51 direction sheet (Voyager robot Zoogi forms, Dark Zoogi creator, Z-Drone/Sentry/Scout/Bruiser/Mage/Guardian minions, "The Lost Transform" lore) - models need Tripo credits = Evan's OK per model.
- Win demo: Game Bar access could not be granted (resolver fails for unrealeditor.exe); win_demo2 records -ZoogiFilm clips instead after the B247 iOS step.
- Note: WaveAssault has 0 cover planes (auto cover log).

## B248 / B249 (Nov 11 11:45, Claude)
- B247 uploaded 11:18. Win demo 2: P1 Blue lost (Tavi 1008, Vex 558, Blue 51); film caught 0 frames. TODO: -ZoogiP1Tier flag + recording.
- B248 (ios/b248_patch.py): auto cover via CoverPlane object channel (ECC_GameTraceChannel8) overlap; long menu names shrink to fit.
- Evan 11:29 "start with the characters" (use existing Tripo models): B249 = ZoogiVoyHero.h/.cpp (ios/b249/) - crystal Zoogi hero
  (Content/Zoogis/Ultimates/ult_*) attached to Voyager's hidden UE4 hero, runtime retarget via FAnimNode_RetargetPoseFromMesh in a custom
  FAnimInstanceProxy (UZoogiVoyHeroAnim). Retargeters RTG_VoyUE4_to_ult_* built by Scripts/voyager/voy_hero_rtg.py (ios/voyhero_run.py).
  Build.cs + "IKRig"; uproject + IKRig, ControlRig. Flags -ZoogiVoyNoCrystal, -ZoogiVoyHeroRig=, -ZoogiVoyHeroYaw=.
- Chain: voyhero_run (waits "248 done") -> gob249 (waits voyhero DONE) -> p5run b249_hero -> b249ios.
- 11:51 Evan: B247 won't open on iPhone. B249 turned into LAUNCH FIX: crystal hero code parked (ios/b249/parked, IKRig/ControlRig removed
  from Build.cs + uproject), Voyager card PNGs re-saved RGBA (B247 wrote RGB). Waiting on Evan: does B246 open + iOS crash report.
- B249 (launch fix) EXPORT_UPLOAD result above; B248 uploaded 11:51. Cause of B247 not opening NOT confirmed.
- 12:17 Evan: "treat as if 246 didn't work". B249 (launch fix guess) uploaded 12:15 but likely same failure. B250 = ROLLBACK to B245 game
  (ios/b250_rollback.py; forward B249 files saved in ios/b250/fwd incl. new card PNGs; crystal hero code in ios/b249/parked; retargeters
  RTG_VoyUE4_to_ult_* exist in Content). If B250 opens -> re-add B246 pad in pieces. If not -> cause is not the 246+ code (launch time / Voyager GI?).
- 13:45 CRASH FOUND (Evan's crash log, read in another chat): launch SIGABRT in FEngineLoop::Init -> StaticLoadClass -> FUObjectArray::AllocateUObjectIndex
  fatal = UObject pool full (disregard-for-GC pool / MaxObjectsInGame) after the Voyager import. B251: Config/DefaultEngine.ini + Config/IOS/IOSEngine.ini
  [/Script/Engine.GarbageCollectionSettings] gc.MaxObjectsNotConsideredByGC=0, gc.MaxObjectsInGame=4194304; plus Voyager-save launch guard (b251_patch.py).
  Source = B250 rollback + guard. Forward work (B246-249) in ios/b250/fwd, ios/b249/parked - re-add after B251 opens.
- 14:31 B251 (gc settings) still crashes; 14:44 clean reinstall still crashes; no crash reports on the phone. Engine base: IOS/BaseIOSEngine.ini
  gc.MaxObjectsInGame=131072 (Android same) - likely the original limit hit after Voyager. B252 = launch diagnostics: ZoogiRoll.cpp custom module
  writes Documents/ZoogiLaunch.txt (pool numbers + GErrorHist on fatal via OnHandleSystemError); Files app sharing on (bSupportsFilesApp,
  bSupportsITunesFileSharing); b252ios uploads Config/DefaultEngine.ini + IOS/IOSEngine.ini to the Mac (Mac builds with -remoteini = its own Config!).
  Tablet test 2 stopped (Android shader compile too slow).
- 16:48 Evan: TestFlight 232-239 OPEN, 241-244 do NOT (240 never uploaded) -> the full Voyager import (B241) breaks launch.
  B253 = Voyager off: DefaultEngine/GameplayTags from .pre_voy2, uproject from .pre_voy (drops CommonUI, EnhancedInput, PoseSearch, Chooser,
  Mover, HairStrands, RigLogic, LiveLink, AnimationWarping, MotionWarping, FieldSystem, AnimationLocomotionLibrary), viewport back to
  UGameViewportClient, Build.cs without CommonUI/CommonInput/EnhancedInput, Voyager levels out of MapsToCook. Voyager-on copies in ios/b253/voyon.
- 17:37 Evan: B253 (Voyager off) OPENS on iPhone. Baseline restored. Next: Voyager back only after it opens on the tablet (Android Development
  build with logs) - re-add plugins/GI/config from ios/b253/voyon step by step; keep Voyager out of launch (load on demand).
- 18:51 Evan 'yes delete': 20 old ZoogiRollUnreal-* copies deleted (kept p5style, combined, tripo15). C: free 18 -> 58.8 GB.
- 19:05 B254 = Voyager back (voyon configs/code from ios/b253/voyon) MINUS RigLogic, LiveLink, LiveLinkControlRig, HairStrands; gc.MaxObjectsInGame=4194304
  (no MaxObjectsNotConsideredByGC change). Voyager-off copies in ios/b254/voyoff. If B254 crashes -> B255 = B253 + plugins only to split.
- Tablet: Development APK asks for Storage permission that Android 16 cannot grant (pm grant silent); B253 shipping-like test pending a fix.
- 19:45 B254 cook failed (GASP MetaHumans Face_AnimBP needs LiveLink). B255 = same + DirectoriesToNeverCook /Game/Voyager/Addons/GASP/MetaHumans. NOTE /Game/Zoogis/Ultimates/Retarget is NeverCook (matters for crystal heroes).
- 19:58 B255 cook failed: BP_MainCharMetahuman (refs MetaHumans) - parked it + BP_GameModeMetahuman (LevelCity only) in ios/b256/parked. B256 same otherwise.
- 20:57 B256 (Voyager minus MetaHuman plugins) does NOT open. B257 = B253 configs (no GI, no Voyager maps) + Voyager plugins only (CommonUI, EnhancedInput, FieldSystem, AnimationWarping, PoseSearch, AnimationLocomotionLibrary, MotionWarping, Chooser, Mover). Voyager-on configs in ios/b257/voyon256.
- 21:55 B258 (Voyager plugins only, B253 configs) does NOT open -> a plugin. B259 = only PoseSearch, AnimationLocomotionLibrary, MotionWarping, Chooser, Mover (no CommonUI/EnhancedInput/FieldSystem/AnimationWarping, viewport back to UGameViewportClient).
- 22:47 B259 crashes. B260 (no Mover) + B261 (no PoseSearch/Chooser) queued back to back via gob260go.py.
- 23:20 B260 (PoseSearch, ALL, MW, Chooser) crashes. Queued B262 (ALL+MW only), B263 (Chooser only) after B261.
- 23:36 B261 (ALL, MW, Mover) crashes. THEORY: iOS gc.MaxObjectsInGame=131072 exceeded by plugin native objects (fatal at first GC alloc in FEngineLoop::Init; checks compiled out in Shipping). B258-262 had the default limit again (voyoff IOSEngine.ini)! B263 repurposed = five plugins + gc.MaxObjectsInGame=2162688 (IOS ini + DefaultEngine). B253 has the ZoogiLaunch.txt diag - ask Evan for the numbers.
- Nov 12 ~00:30 B262/B263 (ALL+MW only; Chooser only) both crash on iPhone. DECISION (Evan): drop full Voyager. Every Voyager plugin combo crashes at launch. Rebuild the Voyager *feel* inside Zoogi's own shooter (ZoogiShooter / Royale engine), one piece per build: camera, shooting feel, movement anims, touch controls, simple cover.
- 04:56 Project reset to B253 working configs (ios/b254/voyoff -> Config + uproject). Plugins = B253 set (no Voyager plugins).
- 05:02 B264 = B253 + shooter AIM ZOOM (camera tucks in/narrows FOV while locked/shooting) + recoil pitch kick (GZoogiShotKick in ZoogiRoyale.cpp, camera in ZoogiRoyaleInput.cpp). NEWS cleaned (B259 test + VOYAGER IS BACK removed). Chain: run_anc -> gob264go.py (p5run b264_htl preview, then b264ios.py). Shots already never shove/stun in shooter modes (b238 code).
- 05:26 B264 UBT_RC=0, EXPORT_UPLOAD_RC=0 -> TestFlight. Waiting for Evan to confirm it opens before piece 2 (shooting feel).
- 06:11 B265 (shipped 06:33, UBT 0 / upload 0): chest cards one style (DrawTile), Awaken cinematic in AZoogiAwakenFX::OnInfuse (SkyVortex kind + Titan meteor + explosions), VOYAGER card commented out of ZoogiModeSelect Cards[], Ancient removed from ZoogiRoyaleMix Classic[] (7 maps, Frozen first). Evan's B264 chest screenshot = B264 opens.
- 06:35 B266: Awaken cinematic now also on the real path ZoogiShowtime::FireArmed (AZoogiAwakenFX::PlayCinematic(World, Ball, Then); -ZoogiNoAwakenCine); chest rows straight (rare/epic no fan/tilt) + dark backdrop; Meadow crowd seats traced down onto stand meshes (LineTraceComponent on QueryOnly-toggled comps; misses dropped; -ZoogiOldMeadowSeats); long_rampart + magic_rays retired (ZRetiredSkill + AllSkillIds); toasts top edge; banter bubbles y>=0.5; pad BLOCK burst smaller; NEW badge above name band; trick popup + detail text fit; Blue tip caption inside.
- OPEN: Evan wants "other SHOWTIME mini games" brought in (only Chain Dash+Beat Floor built; A Beat Rings / E Light Show were set aside) - ask which. Also "improve gameplay to be spectacular" ideas.
- 07:15 B267 shipped (UBT 0 / upload 0): Awaken camera (low cam behind Zoogi, looks up at vortex, follows meteor down, -ZoogiNoAwakenCam) - verified in b267_awaken frames; crowd traces wood-only (still 281/281 hit, looks seated); skill card Y 0.50; combo badge +120 lower. RULE REMINDER: I twice edited Source before SOURCES_UP this morning (reverted the 2nd time; B265's edits were reverted too).
- Nov 12 ~08:30 Evan: "build everything as mentioned in the review doc" (Claude Doc "Zoogi Roll Skills Review", https://claude.ai/artifact/Q3K4UgPFoFdmXoLc6fN87Z). Plan: B270 skill fixes (shipped 08:59, contains the review keep/buff/rework changes in ZoogiSkills.cpp b271 blocks: frost trap, cross-slash counter, sword spin dash, ice patches, glacier pierce, wall payoff, impulse speed, crescent shove, shockwave 450, energy buff 2 SP, CPU values; -ZoogiOldSkillReview = old), B271 element bonus + raid rescue/combo x2/MVP, B272 the 9 new skills (patch script ios/p272.py, icons ios/icons272/*.png), then modes + play-style loadouts.
- B269 shipped 08:37 (full crystals for humans, raid orb volley, CPU raid range). B268 08:13 (spectacle: ZoogiSpectacle.cpp KO moment/shot cam/chain rings/final pulse).
- 10:04 B272 shipped (UBT 0 / upload 0): 9 new skills (p272.py; NewFx/NK_* in ZoogiSkills.cpp; HEX hooks in ZoogiTargeting UpdatePreview + ZoogiGameMode::FireShot; owned via ZoogiFrontProfile IsCardOwned; icons Content/Data/CardArt/Icons). PC checks: grapple yanks, bounce pads place, hex caps power at 70%, tether links. B271 shipped 09:33 (element bonus, raid rescue/combo/MVP). NEXT: the 5 modes (KOTH, Hot Orb, Orb Storm, Boss Rush, Trick Course) + play-style loadouts; Pillar Field/Fault Line rework still open.
- 13:05 B273 shipped 12:58 (PARTY MODES card: KOTH/Hot Orb/Orb Storm/Boss Rush/Trick Course in ZoogiParty.cpp, -ZoogiParty=1..5 test flag; play-style buttons in Inventory>Skills; Pillar Field pinball pads, Fault Line tilt). B274 (hot orb toucher fix) PC: boss rush chain verified 3 titans w/ lives carried (-ZoogiRaidHP=25).

- Nov 12 14:31 (Claude): B276 on TestFlight (UBT 0, upload 0). Command menu centred on SUPERPOWER (ZoogiClassicPad::PowerCenterPx; -ZoogiMenuAtZoogi = old). REACTION pause shows queued COMBO skills as P5 plates (SP5Cmd bReact/ReactRows, Kind 7 -> ZoogiRedirect::CancelPick + ZoogiSkills::ComboGoAt), hidden after. Test: -ZoogiReactComboTest.
- Nov 12 14:15 (Claude): B277 queued (gob277go waits for B276): combo plates moved to the RIGHT of SUPERPOWER (offset +215/+30 X, -95-88i Y, clamped to screen). Backup ios/ZoogiP5Command_pre277.cpp. Preview run b277_react.
- Nov 12 14:48 (Claude): B277 on TestFlight (UBT 0, upload 0) - plates right of SUPERPOWER, but they covered REACTION.
- Nov 12 14:42 (Claude): B278 - plates 285x70, stack up from top-right of SUPERPOWER (+165/+18 X, -150-88i Y), clamped left of REACTION (new ZoogiClassicPad::ReactLeftPx). Backups ios/ZoogiP5Command_pre278.cpp, ZoogiClassicPad_pre278.cpp. Preview b278_react OK (REACTION drawn on top).
- Nov 12 15:07 (Claude): B278 on TestFlight (UBT 0, upload 0).
- Nov 12 16:53 (Claude): B279 - skill picked in a REACTION pause keeps the roll: OnReact stores ball speed/dir (GKeep*), after ZoogiSkills pick closes the speed is restored (if lower) and ZoogiRedirect::Begin reopens the pause. Off: -ZoogiNoReactKeep. Test tap in -ZoogiReactComboTest. Log check OK (904->907, reopened). Backup ios/ZoogiP5Command_pre279.cpp.
- Nov 12 17:10 (Claude): B279 on TestFlight.
- Nov 12 18:32 (Claude): B280 - landscape skill how-to band at the bottom (SP5Cmd DescName/DescRule/DescT; ShowDesc from DoSkill rows + COMBO plates; 6 s, held while ZoogiSkills::Picking). Test -ZoogiDescTest, run b280_desc OK. Backup ios/ZoogiP5Command_pre280.cpp.
- Nov 12 18:49 (Claude): B280 on TestFlight.
- Nov 12 18:43 (Claude): B281 - portrait: command menu back at the Zoogi (PlaceAnchor SUPERPOWER centre only when landscape). Preview b281_port OK. Backup ios/ZoogiP5Command_pre281.cpp.
- Nov 12 19:05 (Claude): B281 on TestFlight.
- Nov 12 18:59 (Claude): B282 - ZoogiSpectacle shot cam glides back to the match view in real time (0.45 s, CalcCamera of the previous view target) when the game is held for a reaction (redirect / skill pick / dilation < 0.15) or the ride ends. Off: -ZoogiNoReactCamReturn. Backup ios/ZoogiSpectacle_pre282.cpp.
- Nov 12 19:20 (Claude): B282 on TestFlight.
- Nov 12 19:14 (Claude): B283 - loading screen for every mode (ZoogiLoadScreen EMode: classic/raid/royale/multi/training/koth/hotorb/orbstorm/bossrush/trickcourse/trickshot; ArmMode() from mode cards; -ZoogiLoadMode=<key> preview). Tips rewritten. Royale = classic rounds only (IsActionSeg off by default). Backups ios/ZoogiLoadScreen_pre283.cpp, ZoogiModeSelect_pre283.cpp. NOTE: I edited ZoogiRedirect during the B283 editor build by mistake, reverted before SOURCES_UP; the swipe change is parked in ios/ZoogiRedirect_b284.cpp.
- Nov 12 19:37 (Claude): B283 on TestFlight.
- Nov 12 19:31 (Claude): B284 - REDIRECT press waits: swipe (>26 px, ZoogiPauseOrbit::WasDrag) looks around and picks nothing, tap/hold 0.2 s picks at the press spot (-ZoogiPickOnPress = b125). Portrait load screen TIP strip above the bar. Backups ios/ZoogiRedirect_pre283.cpp, ZoogiLoadScreen_pre284.cpp.
- Nov 12 19:54 (Claude): B284 on TestFlight.
- Nov 13 09:46 (Claude): B285 - fall guard (GameMode ZoogiFallGuardData: >3 m below court or 1.5 s falling = ring-out while rolling; other phases -> back on safe ground; P5 menu waits while ZoogiFallGuardAirborne; -ZoogiNoFallGuard) + raid/boss rush load screen 4 cards (KNOCK ORBS OFF = DAMAGE first). Patch ios/p285.py (written by the other chat, launched here after the PC was unlocked). Preview run b285_fall too short to reach the fall test - rerun after iOS.
- Nov 13 10:00 (Claude): B285 iOS compiled (UBT 0) but the TestFlight upload FAILED: EXPORT_UPLOAD_RC=70, Apple 403 FORBIDDEN.REQUIRED_AGREEMENTS_MISSING_OR_EXPIRED - Evan has to accept the new agreement in App Store Connect / developer account, then re-run the export. Fall test PC OK: dropped off while aiming -> rescued in 0.7 s ("RESCUED! BACK ON SAFE GROUND").
- Nov 13 10:11 (Claude): B286 - KOTH siege (ZoogiParty): fort of N ice walls round the ring (r ZoneR+90, rebuilt rounds 1/4/7..), a Zoogi ending its turn within 1.6 ZoneR builds a wall toward the nearest rival (2 each), +5 for breaking a wall you did not build (AZoogiIceWall::LastHitP). Off: -ZoogiKothNoFort. Preview b286_koth OK (7/7 fort walls, BARRIER UP x3). Backups ios/*_pre286.*
- Nov 13 10:25 (Claude): B286 iOS compiled (UBT 0); upload blocked again by the Apple agreement (RC=70). Once Evan accepts it, re-run ios\b286ios.py (contains B285 too).
- Nov 13 15:51 (Claude): B286 re-upload still blocked (RC=70 REQUIRED_AGREEMENTS_MISSING_OR_EXPIRED). Guard files renamed to *.old1/.old2 to rerun; next rerun: mv b286ios.started/.lock + b286u.excl aside, double-click run_anc (points at gob286u.py).
- Nov 13 16:38 (Claude): B286 on TestFlight (UBT 0, upload 0) after Evan accepted the Apple agreement. Includes B285 (fall guard + raid 4 cards) + B286 (KOTH siege).
