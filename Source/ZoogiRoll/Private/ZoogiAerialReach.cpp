// Oct 8 (Evan 9:26 PM): "the high altitudes reached serve as a baseline for aerial stage reachability".
// Per action, every Zoogi's peak ball-centre z is logged with the launch type that produced it ("BATTLE peak"):
//   rail_early / rail_center / rail_late  - released from a Launch Rail in that window
//   ramp                                  - left the Frozen snow-chute ramp
//   bumper                                - rebounded off a Sky Bumper
//   floor                                 - anything else (rolling, bounces, collisions)
// The measured envelope ([ZoogiRoll.Aerial] ReachPeaks) caps the Zone 2 prop and decor heights (SpawnMapLayout) and the
// map_reach test asserts every placed prop is reachable by at least one launch type. Observation only: no physics writes.
#include "ZoogiBattleStateMachine.h"
#include "ZoogiAerialProps.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"

namespace
{
	bool OnFrozenRamp(const FVector& P)
	{
		using namespace ZoogiFrozen;
		const float A = FMath::DegreesToRadians(RampYaw);
		const FVector2D D(P.X - RampCX, P.Y - RampCY);
		const float Lx = D.X * FMath::Cos(A) + D.Y * FMath::Sin(A);
		const float Ly = -D.X * FMath::Sin(A) + D.Y * FMath::Cos(A);
		return FMath::Abs(Lx) <= RampLen * 0.5f + ZoogiConst::ZoogiRadius && FMath::Abs(Ly) <= RampWid * 0.5f;
	}
}

void UZoogiBattleStateMachine::NotePeakCause(int32 Body, const TCHAR* Cause)
{
	FZoogiPeakTrack& T = PeakTracks.FindOrAdd(Body);
	T.Cause = Cause;
	T.bAirSinceCause = false;
}

void UZoogiBattleStateMachine::TickPeaks()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	if (CurrentActionId != PeakActionId)
	{
		FlushPeaks(TEXT("next_action"));
		PeakActionId = CurrentActionId;
	}
	// ring-outs are confirmed after the roll (turn end): mark them in any phase until the next action flushes
	for (const AZoogiBall* B : G->Balls)
		if (B && B->bOut)
			if (FZoogiPeakTrack* Tr = PeakTracks.Find(B->PlayerIndex)) Tr->bWentOut = true;
	if (CurrentActionId < 0 || G->Phase != EZoogiPhase::Rolling) return;
	const bool bFrozen = G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen;
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B->bOut) continue;
		// the play area only (court, bridges, islet aprons): a ring-out falling past the rim or parked off court is not a launch
		if (FVector2D(B->BallLocation()).Size() > AZoogiArena::ActiveKnockoffR + 200.f) continue;
		FZoogiPeakTrack& T = PeakTracks.FindOrAdd(B->PlayerIndex);
		const FVector L = B->BallLocation();
		// a teleport (respawn / test placement) is not a flight: restart the track
		if (T.LastZ >= 0.f && FMath::Abs(L.Z - T.LastZ) > 150.f) { T.PeakZ = -1.f; T.Cause = TEXT("floor"); T.bAirSinceCause = false; }
		T.LastZ = L.Z;
		const bool bGround = L.Z < ZoogiConst::ZoogiRadius + 4.f;
		if (bFrozen && !bGround && OnFrozenRamp(L)) { T.Cause = TEXT("ramp"); T.bAirSinceCause = false; }
		else if (!bGround) T.bAirSinceCause = true;
		else if (T.bAirSinceCause) { T.Cause = TEXT("floor"); T.bAirSinceCause = false; } // landed: later bounces count as floor
		if (L.Z > T.PeakZ)
		{
			T.PeakZ = L.Z;
			T.PeakCause = T.Cause;
			T.PeakAt = L;
		}
	}
}

void UZoogiBattleStateMachine::FlushPeaks(const TCHAR* Why)
{
	const AZoogiGameMode* G = GM();
	for (TPair<int32, FZoogiPeakTrack>& P : PeakTracks)
	{
		const FZoogiPeakTrack& T = P.Value;
		if (T.PeakZ < ZoogiConst::ZoogiRadius + 5.f) continue; // never left the floor
		// ring-outs: the flight ended off the stage (wall / rim / arch ricochets fling some balls tens of metres up); logged
		// as cause=ringout_<type> and never part of the envelope
		const FString Cause = T.bWentOut ? FString(TEXT("ringout_")) + T.PeakCause : T.PeakCause;
		float& M = RunPeaks.FindOrAdd(Cause, 0.f);
		M = FMath::Max(M, T.PeakZ);
		RunPeakCounts.FindOrAdd(Cause)++;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE peak action=%d P%d z=%.0f rise=%.0f cause=%s at=(%.0f,%.0f) arena=%d why=%s"), PeakActionId, P.Key + 1, T.PeakZ,
			T.PeakZ - ZoogiConst::ZoogiRadius, *Cause, T.PeakAt.X, T.PeakAt.Y, G && G->Arena ? (int32)G->Arena->GetTheme() : -1, Why);
	}
	PeakTracks.Reset();
}

float UZoogiBattleStateMachine::ReachEnvelope(const FString& Cause) const
{
	TArray<FString> Parts;
	Tune.ReachPeaks.ParseIntoArray(Parts, TEXT(","), true);
	for (const FString& Pt : Parts)
	{
		FString K, V;
		if (Pt.Split(TEXT("="), &K, &V) && K.TrimStartAndEnd().Equals(Cause, ESearchCase::IgnoreCase)) return FCString::Atof(*V.TrimStartAndEnd());
	}
	return Cause == TEXT("floor") ? ZoogiConst::ZoogiRadius : -1.f; // a rolling Zoogi always reaches floor height
}

float UZoogiBattleStateMachine::ReachEnvelopeMax() const
{
	float M = -1.f;
	for (const TCHAR* C : { TEXT("rail_early"), TEXT("rail_center"), TEXT("rail_late"), TEXT("ramp"), TEXT("bumper"), TEXT("floor") }) M = FMath::Max(M, ReachEnvelope(C));
	return M;
}

float UZoogiBattleStateMachine::PropReqZ(const AZoogiAerialProp* P) const
{
	if (!P) return 0.f;
	const FVector L = P->GetActorLocation();
	switch (P->Kind)
	{
	case EZoogiPropKind::ResonanceHalo:
		// hanging ring: pass trigger +-0.6 Size around the centre; standing ring: trigger 10..100 above its base
		return P->bAerialRing ? L.Z - P->Size * 0.6f : L.Z + 10.f;
	case EZoogiPropKind::SkyBumper:
		// a solid contact (not a graze): the ball centre at least half way up the bumper's lower half
		return L.Z - (P->Size + ZoogiConst::ZoogiRadius) * 0.5f;
	case EZoogiPropKind::Gate:
		return L.Z - P->Size * 0.5f; // trigger spans Size * (-0.5 .. 1.5) above the deck midpoint
	default:
		return L.Z; // rail: entered rolling on the floor
	}
}

FString UZoogiBattleStateMachine::BestLaunchFor(float ReqZ, float& OutPeak) const
{
	FString Best;
	OutPeak = -1.f;
	for (const TCHAR* C : { TEXT("floor"), TEXT("ramp"), TEXT("bumper"), TEXT("rail_late"), TEXT("rail_center"), TEXT("rail_early") })
	{
		const float E = ReachEnvelope(C);
		if (E >= ReqZ && (Best.IsEmpty() || E < OutPeak)) { Best = C; OutPeak = E; }
	}
	return Best;
}

int32 UZoogiBattleStateMachine::LogReach(FString* OutDetail) const
{
	int32 Bad = 0;
	FString D;
	for (const AZoogiAerialProp* P : Props)
	{
		if (!P) continue;
		const float Req = PropReqZ(P);
		float Peak = -1.f;
		const FString Best = BestLaunchFor(Req, Peak);
		if (Best.IsEmpty()) ++Bad;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE reach %s[%s] z=%.0f req_z=%.0f launch=%s envelope=%.0f %s"), *P->Label(), *P->Tag.ToString(), P->GetActorLocation().Z, Req,
			Best.IsEmpty() ? TEXT("-") : *Best, Peak, Best.IsEmpty() ? TEXT("UNREACHABLE") : TEXT("ok"));
		D += FString::Printf(TEXT("%s%s:%.0f<=%s"), D.IsEmpty() ? TEXT("") : TEXT(" "), *P->Tag.ToString(), Req, Best.IsEmpty() ? TEXT("NONE") : *Best);
	}
	if (OutDetail) *OutDetail = D;
	return Bad;
}
