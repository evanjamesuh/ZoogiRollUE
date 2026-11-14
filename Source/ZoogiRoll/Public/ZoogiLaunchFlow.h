// Match-start launch flow + respawn-to-pad (ui/launch-flow, Oct 22).
// After the map's intro, each player in turn sits on its launch pad and makes an opening launch (random first player,
// then normal order). Episode Energy starts only once everyone has launched. A knocked-off Zoogi goes back to its pad.
//
// Integration points (no dependency on other branches):
//  - Pads: ZoogiLaunchPads::GetPadTransform is the single swap point. It uses an actor whose class name contains
//    "ZoogiLaunchPad" (or tagged "ZoogiLaunchPad") if one exists, else a computed stub just inside the arena edge.
//  - Glove crane: AZoogiGameMode::OnZoogiNeedsCarryToPad (FZoogiCarryToPad) + AZoogiGameMode::NotifyZoogiPlacedOnPad.
//  - Pad camera: AZoogiGameMode::OnLaunchPadPhase (FZoogiPadPhase) + PlayerController->CallFunctionByNameWithArguments
//    ("OnLaunchPadPhaseStarted <Player>" / "OnLaunchPadPhaseEnded <Player>"), a silent no-op when not implemented.
#pragma once

#include "CoreMinimal.h"

class AZoogiBall;
class AZoogiArena;
class UWorld;

/** Fired when a knocked-off Zoogi needs carrying to its pad. Handler must later call AZoogiGameMode::NotifyZoogiPlacedOnPad(Zoogi). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FZoogiCarryToPad, AZoogiBall* /*Zoogi*/, const FTransform& /*PadTransform*/);
/** Pad phase for the behind-the-player pad camera: bStarted true when Player's Zoogi starts its turn on the pad, false when it leaves / the turn ends. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FZoogiPadPhase, int32 /*Player*/, bool /*bStarted*/);

namespace ZoogiLaunchPads
{
	/** Swap point: world transform of Player's launch pad (location = Zoogi centre when seated, yaw = facing into the arena). */
	ZOOGIROLL_API FTransform GetPadTransform(const UWorld* World, const AZoogiArena* Arena, int32 Player, int32 NumPlayers);
	/** Fallback pad launch when there is no real AZoogiLaunchPad: 30 deg elevation at ~1172 cm/s. */
	constexpr float StubLaunchSpeed = 1172.f;
	constexpr float StubLaunchElevationDeg = 30.f;
	/** Pad shot velocity: PadVelocity's speed and elevation, horizontal part turned to AimYaw (deg). Zero PadVelocity -> stub 30 deg / 1172. */
	ZOOGIROLL_API FVector PadLaunchVelocity(const FVector& PadVelocity, float AimYaw);
	/** Launch speed at ElevDeg that carries a ballistic body HorizDist (cm) while it drops DropZ (launch Z - landing Z) under gravity G (>0, cm/s^2).
	 *  Linear damping k (1/s) is compensated to first order (horizontal travel ~ v t (1 - k t / 2)). Returns 0 if unreachable. */
	ZOOGIROLL_API float SolvePadLaunchSpeed(float HorizDist, float DropZ, float G, float ElevDeg, float Damping = 0.f);
	/** Flight time of that solution (s), 0 if unreachable. */
	ZOOGIROLL_API float PadFlightTime(float HorizDist, float DropZ, float G, float ElevDeg);
	/** Landing distance from the launch point: pad LandingTarget distance if > 0, else 0.6 x launch->centre distance, clamped so the landing stays KnockoffR - Margin from the centre. */
	ZOOGIROLL_API float PadLandingDistance(float TargetDist, float DistToCentre, float KnockoffR, float Margin = 300.f);
	/** Landing distance along the aim ray (unit Dir) from the launch point: the point nearest Target, pulled inside radius
	 *  SafeR of the arena centre (origin) when the ray crosses that circle. Falls back to the distance to Target. */
	ZOOGIROLL_API float PadLandingDistanceAlong(const FVector2D& From, const FVector2D& Dir, const FVector2D& Target, float SafeR);
	constexpr float AirGravityScale = 1.5f;     // measured Oct 22 in matches: a pad-launched Zoogi comes down ~1.5x faster than world GravityZ predicts
	constexpr float LandingPlanarKeep = 0.3f;   // planar speed kept on touchdown so the launcher settles in the court
	constexpr float BallLinearDamping = 0.02f; // ZoogiBall.cpp Sphere->SetLinearDamping
	/** Runtime pad actor for Player (class name contains ZoogiLaunchPad or tag ZoogiLaunchPad; tag P<n>/Player<n> picks a player, else the n-th by name). */
	ZOOGIROLL_API AActor* FindPadActor(const UWorld* World, int32 Player, int32 NumPlayers);
	/** Computed stub: along SpawnDir (or an even split when zero), EdgeInset cm inside the knock-off radius, facing the centre. */
	ZOOGIROLL_API FTransform StubPadTransform(float KnockoffRadius, int32 Player, int32 NumPlayers, const FVector& SpawnDir, float CentreZ);
	constexpr float EdgeInset = 130.f;
	/** Depth (cm) below the pad deck to hide waiting/returned Zoogis from the pad camera. */
	constexpr float ParkDepthBelowDeck = 450.f;
	/** Vertical gap between parked slots under the pad (no two share a point). */
	constexpr float ParkSlotZStep = 120.f;
	/** Park spot under the pad platform: same X/Y as the deck, Z well below so the behind-the-player pad camera never sees them. */
	ZOOGIROLL_API FVector ParkUnderPadLocation(const FVector& PadDeckLoc, int32 Slot, int32 NumSlots = 8);
}

/** Pure bookkeeping (no world) for the opening launches, round boundaries and crane carries. Unit tested (ZoogiRoll.LaunchFlow.*). */
struct ZOOGIROLL_API FZoogiLaunchFlow
{
	int32 NumPlayers = 0;
	int32 FirstPlayer = 0;          // random opening player; also starts every round afterwards
	bool bOpening = false;
	bool bGameplayStarted = false;  // all opening launches done: normal gameplay + Episode Energy
	TArray<bool> Launched;

	struct FCarry { int32 Player = -1; float Left = 0.f; };
	TArray<FCarry> PendingCarries;

	void Reset() { *this = FZoogiLaunchFlow(); }
	void BeginOpening(int32 InNum, FRandomStream& Rng) { BeginOpeningWithFirst(InNum, InNum > 0 ? Rng.RandRange(0, InNum - 1) : 0); }
	void BeginOpeningWithFirst(int32 InNum, int32 First);
	/** Opening turn of Player is over (it launched). Returns the next player to take a turn (next opener, or FirstPlayer once all launched). */
	int32 OnOpeningTurnEnded(int32 Player);
	bool AllLaunched() const;
	bool EnergyRunning() const { return bGameplayStarted; }
	static int32 NextInOrder(int32 Cur, int32 Num) { return (Cur + 1) % FMath::Max(1, Num); }
	/** Normal play: a new round begins when the turn comes back to the first player. */
	bool IsRoundStart(int32 Player) const { return Player == FirstPlayer; }
	/** The arena never switches between rounds (kept as an explicit rule the game mode asserts). */
	static bool ArenaSwitchesBetweenRounds() { return false; }

	/** Players to send back to their pads at a respawn point: exactly the knocked-off ones, nobody else moves. */
	static TArray<int32> PadReturns(const TArray<bool>& OutFlags);
	/** Exactly one Zoogi may sit on the pad deck: ActiveSeat when NeedsPad[ActiveSeat], else none (-1). */
	static int32 PadSeatPlayer(const TArray<bool>& NeedsPad, int32 ActiveSeat);
	/** Everyone in NeedsPad except the single PadSeatPlayer - they park under the deck (no stacking). */
	static TArray<int32> ParkSlots(const TArray<bool>& NeedsPad, int32 ActiveSeat);
	/** Crane hook: true when a handler is bound and should be waited for (with a timeout); false = place directly now. */
	static bool ShouldWaitForCrane(bool bHookBound) { return bHookBound; }
	void BeginCarry(int32 Player, float Timeout);
	bool CompleteCarry(int32 Player);
	bool IsCarryPending(int32 Player) const;
	/** Advances carry timeouts; returns players whose carry timed out (caller places them directly). */
	TArray<int32> TickCarries(float Dt);

	// ---- pad ballistic launch: no ring-out / knock-off for that Zoogi until its first landing (or the safety timeout)
	static constexpr float PadAirTimeout = 4.f;
	static constexpr float PadAirMinTime = 0.2f; // ignore ground contact right at the lip of the pad
	TMap<int32, float> PadAir;                    // player -> seconds airborne
	TMap<int32, float> PadPlaceWatch; // log only: ball vs pad top for 1 s after placement
	TSet<int32> PadHitLogged; // log only: first wall / Zoogi hit during a pad flight
	TMap<int32, float> PadPostLand; // log only: watch the launcher for 5 s after touchdown (ring-out check)
	void BeginPadAir(int32 Player) { PadAir.Add(Player, 0.f); }
	bool IsPadAirborne(int32 Player) const { return PadAir.Contains(Player); }
	/** Advances one player's flight; ends it on ground contact (after PadAirMinTime) or at PadAirTimeout. Returns true when it ended. */
	bool TickPadAir(int32 Player, float Dt, bool bGroundContact);
	/** Ground contact only counts as the landing once past the peak (Vz <= 0) and clear of the pad deck (horizontal distance from the pad origin > deck radius + Zoogi radius). */
	static bool PadLandingEligible(float VelZ, float HorizDistFromPad, float DeckRadius, float ZoogiRadius) { return VelZ <= 0.f && HorizDistFromPad > DeckRadius + ZoogiRadius; }
};
