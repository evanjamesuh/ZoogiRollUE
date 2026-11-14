#include "ZoogiLaunchFlow.h"
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiTypes.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ------------------------------------------------------------------ pads

AActor* ZoogiLaunchPads::FindPadActor(const UWorld* World, int32 Player, int32 NumPlayers)
{
	if (!World) return nullptr;
	TArray<AActor*> Pads;
	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A)) continue;
		if (A->GetClass()->GetName().Contains(TEXT("ZoogiLaunchPad")) || A->ActorHasTag(TEXT("ZoogiLaunchPad"))) Pads.Add(A);
	}
	if (Pads.Num() == 0) return nullptr;
	const FName T1(*FString::Printf(TEXT("P%d"), Player + 1)), T2(*FString::Printf(TEXT("Player%d"), Player + 1));
	for (AActor* A : Pads) if (A->ActorHasTag(T1) || A->ActorHasTag(T2)) return A;
	Pads.Sort([](const AActor& L, const AActor& R) { return L.GetName() < R.GetName(); });
	return Pads[Player % Pads.Num()];
}

FTransform ZoogiLaunchPads::StubPadTransform(float KnockoffRadius, int32 Player, int32 NumPlayers, const FVector& SpawnDir, float CentreZ)
{
	FVector2D D(SpawnDir.X, SpawnDir.Y);
	if (D.SizeSquared() < 1.f)
	{
		const float A = 2.f * PI * (float)Player / (float)FMath::Max(1, NumPlayers);
		D = FVector2D(FMath::Cos(A), FMath::Sin(A));
	}
	D.Normalize();
	const float R = FMath::Max(100.f, KnockoffRadius - EdgeInset);
	const FVector L(D.X * R, D.Y * R, CentreZ);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-D.Y, -D.X)); // face the centre
	return FTransform(FRotator(0.f, Yaw, 0.f), L);
}

FTransform ZoogiLaunchPads::GetPadTransform(const UWorld* World, const AZoogiArena* Arena, int32 Player, int32 NumPlayers)
{
	// arena/launch-pads: the arena's real pad (seat = LaunchPoint, facing = launch direction yaw)
	if (Arena) if (const AZoogiLaunchPad* LP = Arena->GetLaunchPad())
	{
		const FTransform T = LP->GetLaunchTransform();
		return FTransform(FRotator(0.f, T.Rotator().Yaw, 0.f), T.GetLocation());
	}
	if (AActor* Pad = FindPadActor(World, Player, NumPlayers))
	{
		const FVector L = Pad->GetActorLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 2.f);
		FVector2D In(-L.X, -L.Y);
		const float Yaw = In.SizeSquared() > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X)) : Pad->GetActorRotation().Yaw;
		return FTransform(FRotator(0.f, Yaw, 0.f), L);
	}
	const FVector Hint = Arena ? Arena->GetSpawnPoint(Player) : FVector::ZeroVector;
	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	return StubPadTransform(KR, Player, NumPlayers, FVector(Hint.X, Hint.Y, 0.f), ZoogiConst::ZoogiRadius + 2.f);
}

// ------------------------------------------------------------------ flow

void FZoogiLaunchFlow::BeginOpeningWithFirst(int32 InNum, int32 First)
{
	NumPlayers = FMath::Max(0, InNum);
	FirstPlayer = NumPlayers > 0 ? FMath::Clamp(First, 0, NumPlayers - 1) : 0;
	Launched.Init(false, NumPlayers);
	bOpening = NumPlayers > 0;
	bGameplayStarted = NumPlayers == 0;
	PendingCarries.Reset();
}

bool FZoogiLaunchFlow::AllLaunched() const
{
	for (bool b : Launched) if (!b) return false;
	return true;
}

int32 FZoogiLaunchFlow::OnOpeningTurnEnded(int32 Player)
{
	if (!bOpening) return NextInOrder(Player, NumPlayers);
	if (Launched.IsValidIndex(Player)) Launched[Player] = true;
	if (AllLaunched())
	{
		bOpening = false;
		bGameplayStarted = true;
		return FirstPlayer; // normal turn order resumes with the first launcher
	}
	int32 Next = Player;
	for (int32 i = 0; i < NumPlayers; ++i) { Next = NextInOrder(Next, NumPlayers); if (!Launched[Next]) return Next; }
	return FirstPlayer;
}

TArray<int32> FZoogiLaunchFlow::PadReturns(const TArray<bool>& OutFlags)
{
	TArray<int32> R;
	for (int32 i = 0; i < OutFlags.Num(); ++i) if (OutFlags[i]) R.Add(i);
	return R;
}

void FZoogiLaunchFlow::BeginCarry(int32 Player, float Timeout)
{
	for (FCarry& C : PendingCarries) if (C.Player == Player) { C.Left = Timeout; return; }
	FCarry C; C.Player = Player; C.Left = Timeout;
	PendingCarries.Add(C);
}

bool FZoogiLaunchFlow::CompleteCarry(int32 Player)
{
	return PendingCarries.RemoveAll([Player](const FCarry& C) { return C.Player == Player; }) > 0;
}

bool FZoogiLaunchFlow::IsCarryPending(int32 Player) const
{
	for (const FCarry& C : PendingCarries) if (C.Player == Player) return true;
	return false;
}

TArray<int32> FZoogiLaunchFlow::TickCarries(float Dt)
{
	TArray<int32> Expired;
	for (int32 i = PendingCarries.Num() - 1; i >= 0; --i)
	{
		PendingCarries[i].Left -= Dt;
		if (PendingCarries[i].Left <= 0.f) { Expired.Add(PendingCarries[i].Player); PendingCarries.RemoveAt(i); }
	}
	return Expired;
}

FVector ZoogiLaunchPads::PadLaunchVelocity(const FVector& PadVelocity, float AimYaw)
{
	float Speed = StubLaunchSpeed, Elev = StubLaunchElevationDeg;
	if (!PadVelocity.IsNearlyZero())
	{
		Speed = PadVelocity.Size();
		Elev = FMath::RadiansToDegrees(FMath::Atan2(PadVelocity.Z, FVector2D(PadVelocity.X, PadVelocity.Y).Size()));
	}
	const float E = FMath::DegreesToRadians(Elev), Y = FMath::DegreesToRadians(AimYaw);
	const float H = Speed * FMath::Cos(E);
	return FVector(H * FMath::Cos(Y), H * FMath::Sin(Y), Speed * FMath::Sin(E));
}

bool FZoogiLaunchFlow::TickPadAir(int32 Player, float Dt, bool bGroundContact)
{
	float* T = PadAir.Find(Player);
	if (!T) return false;
	*T += Dt;
	if ((bGroundContact && *T >= PadAirMinTime) || *T >= PadAirTimeout) { PadAir.Remove(Player); return true; }
	return false;
}
float ZoogiLaunchPads::PadFlightTime(float D, float DropZ, float G, float ElevDeg)
{
	const float Th = FMath::DegreesToRadians(ElevDeg), C = FMath::Cos(Th), Den = D * FMath::Tan(Th) + DropZ;
	if (D <= 0.f || G <= 0.f || C <= KINDA_SMALL_NUMBER || Den <= 0.f) return 0.f;
	return FMath::Sqrt(2.f * Den / G); // from y(t) = 0 with v cos = D / t
}

float ZoogiLaunchPads::SolvePadLaunchSpeed(float D, float DropZ, float G, float ElevDeg, float Damping)
{
	const float T = PadFlightTime(D, DropZ, G, ElevDeg);
	if (T <= 0.f) return 0.f;
	const float V = D / (T * FMath::Cos(FMath::DegreesToRadians(ElevDeg)));
	return V / FMath::Max(0.5f, 1.f - Damping * T * 0.5f);
}

float ZoogiLaunchPads::PadLandingDistance(float TargetDist, float DistToCentre, float KnockoffR, float Margin)
{
	if (TargetDist > 0.f) return TargetDist;
	const float In = FMath::Max(0.f, KnockoffR - Margin);
	return FMath::Clamp(0.6f * DistToCentre, FMath::Max(0.f, DistToCentre - In), DistToCentre + In);
}

float ZoogiLaunchPads::PadLandingDistanceAlong(const FVector2D& From, const FVector2D& Dir, const FVector2D& Target, float SafeR)
{
	const FVector2D U = Dir.GetSafeNormal();
	if (U.IsNearlyZero()) return FVector2D::Distance(From, Target);
	float D = FVector2D::DotProduct(Target - From, U);
	// ray vs circle |From + U t| = SafeR
	const float B = FVector2D::DotProduct(From, U), Cc = From.SizeSquared() - SafeR * SafeR, Disc = B * B - Cc;
	if (Disc > 0.f)
	{
		const float S = FMath::Sqrt(Disc), T0 = FMath::Max(0.f, -B - S), T1 = -B + S;
		if (T1 > 0.f) D = FMath::Clamp(D, T0, T1);
	}
	return D > 0.f ? D : FVector2D::Distance(From, Target);
}

FVector ZoogiLaunchPads::ParkUnderPadLocation(const FVector& PadDeckLoc, int32 Slot, int32 NumSlots)
{
	(void)NumSlots;
	const int32 S = FMath::Max(0, Slot);
	// Same X/Y as the pad deck; Z well below so the behind-the-player pad camera never sees them in its frustum toward the arena.
	return FVector(PadDeckLoc.X, PadDeckLoc.Y, PadDeckLoc.Z - ParkDepthBelowDeck - (float)S * ParkSlotZStep);
}

int32 FZoogiLaunchFlow::PadSeatPlayer(const TArray<bool>& NeedsPad, int32 ActiveSeat)
{
	if (NeedsPad.IsValidIndex(ActiveSeat) && NeedsPad[ActiveSeat]) return ActiveSeat;
	return -1;
}

TArray<int32> FZoogiLaunchFlow::ParkSlots(const TArray<bool>& NeedsPad, int32 ActiveSeat)
{
	TArray<int32> R;
	const int32 Seat = PadSeatPlayer(NeedsPad, ActiveSeat);
	for (int32 i = 0; i < NeedsPad.Num(); ++i) if (NeedsPad[i] && i != Seat) R.Add(i);
	return R;
}
