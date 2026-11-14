#pragma once
// BATTLE ROYALE (Evan Oct 25): a free-for-all ring-out brawl on any of the existing arenas. Unlike Classic / 2v2 Combo
// there are no turns: every Zoogi rolls at once. The human moves with the left touch circle (lower left; WASD / arrows on
// PC), looks / turns the camera by dragging on the right half (Q / E or right-mouse drag on PC) and taps AWAKEN (Space / F)
// to transform into its element for a few seconds (heavier, faster, harder hits + a shockwave burst). Knock rivals off the
// arena; each Zoogi has a few lives (stocks), the last Zoogi with lives left wins. The knockback surge grows over time so
// a match always ends.
//
// Game rules / CPU brain live in ZoogiRoyale.cpp (AZoogiGameMode), touch + camera in ZoogiRoyaleInput.cpp
// (AZoogiPlayerController), the HUD in ZoogiRoyaleHUD.cpp (AZoogiHUD). Tuning: [ZoogiRoll.Royale] in DefaultGame.ini.
// Command line: -ZoogiRoyale (mode on), -ZoogiRoyaleLives=N, -ZoogiRoyaleFrames (with -ZoogiShots=<dir>: frame dump).
//
// Oct 26 (Evan, Marble Clash controls): every Zoogi is the transforming Zoogi Blue model (ZoogiMorph.h). Left thumb moves,
// dragging anywhere else aims a crosshair fixed at the screen centre, the up-arrow button JUMPs, the swap button
// TRANSFORMs ball <-> robot (any time, short cooldown). In robot form the arm cannons point at the crosshair and fire on
// their own whenever it is on a rival (small aim-assist cone, no fire button). Bolts knock rivals back (ring-outs count),
// the ball form is faster and rams harder. The Awaken charge is not used in this mode any more.
#include "CoreMinimal.h"
#include "GameFramework/Actor.h" // FZoogiRoyaleBolt::Actor

// Oct 27 (Evan, build 9): TRANSFORM / JUMP respond with no delay (royale, training and classic share these numbers).
namespace ZoogiRoyaleFeel
{
	constexpr float MorphSeconds = 0.3f;   // ball <-> robot animation length (was 0.8 s; the clips are sped up to fit)
	constexpr float MorphCooldown = 0.15f; // between two TRANSFORM presses (was 1.0 s)
	constexpr float JumpCooldown = 0.15f;  // between two JUMPs (was 0.9 s); a jump still needs the floor
}

/** Tuning ([ZoogiRoll.Royale], read once). Speeds in cm/s, accelerations in cm/s^2, times in seconds. */
struct ZOOGIROLL_API FZoogiRoyaleTune
{
	int32 Lives = 3;                 // stocks per Zoogi
	float Accel = 1900.f;            // drive acceleration at full stick
	float MaxSpeed = 1000.f;         // drive never pushes a Zoogi past this (knockback can)
	float BrakeMul = 1.25f;           // stick against the roll brakes this much harder
	float Grip = 1.1f;               // 1/s: sideways slip removed while driving under the speed cap (steering)
	float AirControl = 0.35f;        // fraction of the drive while airborne
	float EdgeMargin = 75.f;         // ring-out: the Zoogi's centre within this of the court edge (walled arenas too)
	float KnockFrac = 0.65f;         // extra knockback = closing speed x this ...
	float KnockMin = 200.f;          // ... once the closing speed passes this
	float KnockMax = 1300.f;         // ... capped here (before awaken / surge)
	float HitStun = 0.65f;           // a hard hit takes the victim's drive away for up to this long (momentum carries it)
	float OverSpeedControl = 0.35f;  // drive strength while flying faster than the drive cap (knocked back)
	float KnockLift = 140.f;         // small hop on a big hit
	float CreditSeconds = 5.f;       // a ring-out within this long of a hit counts as that rival's KO
	float AwakenFillSeconds = 14.f;  // passive charge time 0 -> full
	float ChargePerHit = 0.08f;      // landing a hard hit
	float ChargePerKO = 0.35f;       // ringing a rival out
	float ChargePerOrb = 0.12f;      // knocking an orb off
	float AwakenSeconds = 6.f;       // transformation length
	float AwakenSpeedMul = 1.3f;
	float AwakenAccelMul = 1.5f;
	float AwakenMassMul = 2.2f;
	float AwakenKnockMul = 2.0f;     // knockback dealt while awakened
	float AwakenTakenMul = 0.55f;    // knockback taken while awakened
	float AwakenModelScale = 1.14f;
	float BurstRadius = 460.f;       // transformation shockwave
	float BurstSpeed = 1300.f;
	float SurgeStart = 45.f;         // knockback surge: x1 until here ...
	float SurgeFull = 150.f;         // ... ramping to SurgeMax here
	float SurgeMax = 1.9f;
	float RespawnDelay = 1.6f;
	float ShieldSeconds = 2.f;       // after a respawn: phasing, no knockback taken
	float CountdownSeconds = 3.3f;
	float CpuSkill = 0.8f;           // 0..1: CPU lead / edge sense / reaction
	float LookSensitivity = 0.22f;   // deg per pixel (1080p) of right-side drag
	float KeyLookRate = 120.f;       // deg/s for Q / E
	float CamDist = 1150.f, CamPitch = 30.f, CamFov = 80.f, CamHeight = 60.f;
	float PortraitDistMul = 1.45f;
	// ---- transform / robot / jump (Oct 26)
	float MorphCooldown = 1.0f;      // between two TRANSFORM taps
	float RobotSpeedMul = 0.72f;     // hovering robot: slower ...
	float RobotAccelMul = 1.1f;      // ... but snappier
	float RobotMassMul = 1.6f;       // heavier to push around
	float RobotTakenMul = 0.75f;     // knockback taken from rams while a robot
	float MorphDriveMul = 0.25f;     // drive while ToBot / ToBall plays
	float FireInterval = 0.26f;      // seconds between shots (cannons alternate right / left)
	float BoltSpeed = 3400.f;
	float BoltRange = 2600.f;        // bolts fizzle after this far
	float BoltRadius = 22.f;
	float BoltKnock = 430.f;         // knockback per bolt (x surge)
	float BoltStun = 0.28f;
	float AimAssistDeg = 9.f;        // crosshair lock cone (plus the target's own angular size)
	float AutoFireRange = 2300.f;    // auto-fire only at rivals closer than this
	float CpuAimError = 3.5f;        // degrees of CPU spread at skill 0 (x (1 - skill))
	float JumpSpeed = 640.f;         // upward speed of a jump
	float JumpCooldown = 0.9f;
	float RobotCamDist = 820.f, RobotCamPitch = 14.f, RobotCamHeight = 150.f; // over-the-shoulder aim camera (unused since Oct 27)
	// ---- Oct 27 (Evan, Marble Clash look): low chase camera, tracers, HP + damage numbers
	float ChaseBack = 270.f;         // camera this far behind the Zoogi (horizontal, cm) ...
	float ChaseUp = 125.f;           // ... and this high above its centre; the view looks level ahead (crosshair = screen centre)
	float ChasePitch = 1.f;          // degrees the view looks down (before the player's drag)
	float ChaseFov = 95.f;           // horizontal FOV, landscape
	float RobotChaseBack = 470.f, RobotChaseUp = 175.f; // robot form: a little further / higher (the robot is taller)
	float TracerSpeed = 5200.f;      // bolt flight speed (cm/s)
	float TracerLength = 2.6f;       // tracer streak length (m) trailing the bolt head
	int32 MaxHP = 1200;              // health per life; 0 = destroyed (loses a life, like a ring-out)
	int32 BoltDamage = 32;           // per cannon bolt (white number)
	float CritChance = 0.18f;        // crit bolts: yellow number ...
	float CritMul = 1.45f;           // ... this much damage (32 -> 46)
	float RamDamagePerSpeed = 0.09f; // ram damage = closing speed x this (900 cm/s -> 81)

	static const FZoogiRoyaleTune& Get();
};

enum class EZoogiRoyaleStep : uint8
{
	Off,
	Countdown, // Zoogis placed, no driving, 3-2-1
	Fight,
	Over       // a winner (or a draw); Phase = GameOver
};

/** Per-Zoogi royale state (index = player index = AZoogiGameMode::Balls index). */
struct ZOOGIROLL_API FZoogiRoyalePlayer
{
	int32 Lives = 3;
	int32 KOs = 0;
	int32 Falls = 0;
	int32 Orbs = 0;                      // Oct 31 b41: orbs collected (Ancient Royale: collect, not knock off)
	int32 Place = 0;                     // final placing (1 = winner); 0 while still in
	float Charge = 0.f;                  // Awaken charge 0..1
	float AwakenLeft = 0.f;              // > 0 while transformed
	float AuraClock = 0.f;
	float RespawnIn = -1.f;              // >= 0 while waiting to drop back in
	float ShieldLeft = 0.f;              // respawn protection
	float StunLeft = 0.f;                // > 0: knocked back, no drive
	int32 LastHitBy = -1;                // rival credited with a ring-out
	float LastHitAt = -100.f;
	FVector2D Move = FVector2D::ZeroVector; // drive input this frame (world XY, length <= 1)
	// CPU brain
	int32 Target = -1;
	float Rethink = 0.f;
	float Wander = 0.f;
	float AwakenWant = 0.f;
	FVector2D SmoothMove = FVector2D::ZeroVector;
	// transform / robot (Oct 26)
	bool bRobot = false;
	float MorphCD = 0.f;                 // TRANSFORM cooldown left
	float MorphT = 10.f;                 // seconds since the last transform (ToBot / ToBall play for the first ~0.8 s)
	float FireCD = 0.f;
	bool bFireLeft = false;              // next shot from the left cannon
	float JumpCD = 0.f;
	float FacingYaw = 0.f;               // robot body / cannon yaw
	FVector AimPoint = FVector::ZeroVector;
	int32 Lock = -1;                     // rival under the crosshair (auto-fire target), -1 none
	float FormThink = 0.f;               // CPU: next form decision
	float StrafeSign = 1.f;              // CPU robot: circling direction
	float DodgeCD = 0.f;                 // CPU: next allowed dodge jump
	// HP (Oct 27)
	int32 HP = 1200;
	float HurtAt = -100.f;               // world seconds of the last damage taken (HP bar flash, red vignette)
	bool bDestroyPending = false;        // HP hit 0 (maybe inside a physics callback): blown up on the next tick

	bool IsIn() const { return Place == 0; }
	bool IsAwake() const { return AwakenLeft > 0.f; }
};

/** A deferred knockback (applied on the next GameMode tick, outside the physics hit callback). */
struct FZoogiRoyaleKick
{
	int32 Victim = -1;
	FVector DeltaV = FVector::ZeroVector;
};

/** A robot plasma bolt in flight (the actor is only its look). */
struct FZoogiRoyaleBolt
{
	TWeakObjectPtr<AActor> Actor;
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	int32 Owner = -1;
	float Travel = 0.f;
	float Power = 1.f; // Oct 31: relic staff bolts hit harder (knock + damage x Power)
};

/** A floating damage number (Marble Clash): white = normal, yellow = crit, red = taken by the human. */
struct FZoogiRoyaleDmg
{
	FVector World = FVector::ZeroVector; // where it pops (the victim)
	int32 Amount = 0;
	uint8 Kind = 0;                      // 0 dealt, 1 crit, 2 taken by the human, 3 dealt by a CPU to a CPU (small, grey)
	float Born = 0.f;                    // world seconds
	FVector2D Jitter = FVector2D::ZeroVector; // screen-space scatter so a burst does not overprint
};

/** Kill banner ("YOU DESTROY" / "DESTROYED BY") for the human. */
struct FZoogiRoyaleBanner
{
	FString Top, Name;
	float Born = -100.f;
};

/** Pure bookkeeping for one Battle Royale match (no UObject). */
struct ZOOGIROLL_API FZoogiRoyale
{
	EZoogiRoyaleStep Step = EZoogiRoyaleStep::Off;
	float StepTime = 0.f;
	float FightTime = 0.f;               // seconds since GO (drives the surge)
	float Surge = 1.f;
	int32 SurgeShown = 100;              // last announced surge x100
	int32 Winner = -1;
	int32 HumanIndex = -1;               // first human Zoogi (camera / touch controls), -1 = all CPU (spectate)
	FVector2D HumanMove = FVector2D::ZeroVector; // from the controller (world XY, length <= 1)
	FVector HumanCamLoc = FVector::ZeroVector;   // crosshair ray (camera through the screen centre), from the controller
	FVector HumanCamDir = FVector::ForwardVector;
	bool bHumanAim = false;
	float HumanHitAt = -100.f;                   // last time a human bolt hit a rival (crosshair hit marker)
	TArray<FZoogiRoyaleBolt> Bolts;
	TArray<FZoogiRoyaleDmg> Dmg;          // floating damage numbers (HUD)
	FZoogiRoyaleBanner KillBanner;
	TArray<FZoogiRoyalePlayer> P;
	TArray<FZoogiRoyaleKick> Kicks;
	TMap<uint32, float> PairHitAt;       // pair key -> last processed contact time (one boost per contact)
	int32 FrameShots = 0;                // -ZoogiRoyaleFrames dump counter
	float FrameClock = 0.f;

	void Reset(int32 NumPlayers, int32 Lives);
	int32 NumIn() const;
	bool IsLive() const { return Step == EZoogiRoyaleStep::Countdown || Step == EZoogiRoyaleStep::Fight; }
};

/** Touch layout shared by the controller (hit tests) and the HUD (drawing). Viewport pixels. */
namespace ZoogiRoyaleUI
{
	struct FPad
	{
		float U = 1.f;                                // 1 at 1080 px on the short side
		FVector2D StickHome = FVector2D::ZeroVector;  // left move circle (lower left)
		float StickR = 150.f, KnobR = 62.f;
		FVector2D JumpC = FVector2D::ZeroVector;      // JUMP (up arrow), right side
		float JumpR = 80.f;
		FVector2D MorphC = FVector2D::ZeroVector;     // TRANSFORM (swap arrows), lower right
		float MorphR = 80.f;
		FBox2D Menu = FBox2D(ForceInit);              // top-right MENU button
	};
	ZOOGIROLL_API FPad Layout(const FVector2D& View);
}
