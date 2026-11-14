#pragma once
// Live match DROP INTRO (Evan Oct 23 t772u): empty-cabin intro VIDEO (DropIntro.mp4) -> FADE into Tac
// bird's-eye -> each player picks a spot and Drops (physics fall) -> anime land bounce + smoke poof -> hand off.
// SKIP modeled Zoogis in the cabin (hide/park off-camera). Real PlatformShip still spawned as fallback.
// Music + SFX stay audible for the whole sequence (no mute / duck-to-silence / strip).
#include "CoreMinimal.h"

class AZoogiBall;
class AZoogiArena;
class AActor;
class UWorld;
class UStaticMeshComponent;

enum class EZoogiDropStep : uint8
{
	Idle = 0,
	ShipHold,   // intro VIDEO (empty cabin / no Zoogi seats) then fade to Tac
	TacPick,    // straight-down Tac; active player picks spot
	Falling,    // physics fall -> contact fires bounce+poof, then settle
	Done        // all dropped; GameMode yields to normal play
};

/** Pure bookkeeping for the drop intro (no UObject). */
struct ZOOGIROLL_API FZoogiDropIntro
{
	EZoogiDropStep Step = EZoogiDropStep::Idle;
	int32 NumPlayers = 0;
	int32 FirstPlayer = 0;
	int32 ActiveDrop = 0;
	TArray<bool> Dropped;
	FVector PickPoint = FVector::ZeroVector;
	bool bHasPick = false;
	float StepAge = 0.f;
	// Timings tuned to anime clip feel (ship beat, snappy auto-pick, readable bounce settle).
	float ShipHoldSec = 5.0f;      // DropIntro.mp4 length (Movies.csv); fallback if no movie
	float DropHeight = 1550.f;     // cm above court when released (taller fall reads more anime)
	float FallTimeout = 6.f;
	float AutoPickDelay = 0.72f;   // CPU / -ZoogiDropAuto
	float BounceHoldSec = 0.95f;   // after first contact: bounce + smoke must read before next dropper
	float BounceImpulseZ = 640.f;  // toon upward kick on land (cm/s)
	float CutFlashSec = 0.85f;     // t772u: black FADE length into Tac (was 0.18 hard white flash)
	float FadeSec = 0.85f;         // alias used by HUD fade hook (same as CutFlashSec)
	bool bHardCutPending = false;  // controller: snap Tac pose once (under fade)
	bool bIntroMovie = false;      // DropIntro.mp4 playing via Anime MediaPlayer
	bool bFadeToTac = false;       // ShipHold ended; fading into TacPick
	bool bShipSpawned = false;
	bool bLandPoofFired = false;   // first ground contact this fall
	bool bBounceSettling = false;  // waiting BounceHoldSec after poof
	float LandContactAge = 0.f;    // age since first contact
	float CutFlashAge = -1.f;      // <0 inactive; else seconds since hard cut
	int32 FallTrailTicks = 0;
	TWeakObjectPtr<AActor> ShipActor;

	void Reset();
	void Begin(int32 InNum, int32 First);
	bool AllDropped() const;
	int32 NextUndropped(int32 From) const;
	/** Advance ActiveDrop after a successful land; returns true when sequence finished. */
	bool OnLanded();
	static bool Enabled(); // t782u: off by default ([ZoogiRoll.DropIntro] bEnabled / -ZoogiDropIntro); -ZoogiNoDropIntro forces off
	static bool AutoDrive(); // -ZoogiDropAuto or in-game test drivers
};

namespace ZoogiDropIntro
{
	/** Cabin window local offsets (cm) relative to ship origin - four seats facing inward/camera. */
	ZOOGIROLL_API FVector CabinSeatLocal(int32 Slot, int32 NumSlots);
	/** Ship world transform above arena centre (hover craft). */
	ZOOGIROLL_API FTransform ShipWorldXform(const AZoogiArena* Arena);
	/** Straight-down Tac boom: pitch ~89.5, high dist, pivot arena centre. */
	ZOOGIROLL_API void TacBirdEyePose(float ArenaScale, float& OutYaw, float& OutPitch, float& OutDist, float& OutFov, FVector& OutPivot);
	/** Load Evan's modeled VIP PlatformShip Parts (Content/Ships/PlatformShip/Parts). Never SharedPlatform. */
	ZOOGIROLL_API void CollectShipPartMeshes(TArray<UStaticMesh*>& Out);
	/** First part mesh (compat); prefer CollectShipPartMeshes for the full ship. */
	ZOOGIROLL_API UStaticMesh* FindShipMesh();
}