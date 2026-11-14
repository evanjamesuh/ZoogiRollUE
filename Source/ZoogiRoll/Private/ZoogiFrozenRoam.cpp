// Oct 27: Frozen Ring outer-ring free roam, gate / bridge launch (see ZoogiFrozenRoam.h).
#include "ZoogiFrozenRoam.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiDevConfig.h"
#include "ZoogiDevMode.h"
#include "ZoogiTune.h"
#include "ZoogiFX.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

bool ZoogiFrozenRoam::ConfigOn()
{
	static const bool bOn = []
	{
		bool b = !ZoogiFrozen::OpenRing(); // b119: the open ring has no bridges to roam to (-ZoogiFrozenRoam=On still forces it)
		if (GConfig && !ZoogiFrozen::OpenRing()) GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bOuterRoam"), b, GGameIni);
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenRoam="), V)) b = !(V.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || V == TEXT("0"));
		return b;
	}();
	return bOn;
}

bool ZoogiFrozenRoam::BridgeEntryOn()
{
	static const bool bOn = []
	{
		bool b = false;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bBridgeEntry"), b, GGameIni);
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenBridges="), V)) b = V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V == TEXT("1");
		return b;
	}();
	return bOn;
}

const TCHAR* ZoogiFrozenRoam::StateName(EZoogiRoamState S)
{
	switch (S)
	{
	case EZoogiRoamState::Spawning: return TEXT("SPAWNING");
	case EZoogiRoamState::FreeRoam: return TEXT("FREE_ROAM");
	case EZoogiRoamState::BridgeApproach: return TEXT("BRIDGE_APPROACH");
	case EZoogiRoamState::LaunchPrep: return TEXT("LAUNCH_PREPARATION");
	case EZoogiRoamState::Launched: return TEXT("LAUNCHED");
	default: return TEXT("NONE");
	}
}

namespace
{
	FVector RoamPol(float R, float YawDeg, float Z) { const float A = FMath::DegreesToRadians(YawDeg); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); }
	float SmoothStep01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
}

// ------------------------------------------------------------------ arena side: props, markers, spawn points

int32 AZoogiArena::NumRoamProps(FName Type) const
{
	int32 N = 0;
	for (const FZoogiDevProp& P : DevProps) N += P.Type == Type ? 1 : 0;
	return N;
}

const AZoogiArena::FZoogiDevProp* AZoogiArena::FindRoamProp(FName Type, int32 Index) const
{
	int32 N = 0;
	for (const FZoogiDevProp& P : DevProps) if (P.Type == Type) { if (N == Index) return &P; ++N; }
	return nullptr;
}

void AZoogiArena::SetFrozenRoam(bool bOn)
{
	if (bOn && (Theme != EZoogiArena::Frozen || !ZoogiFrozen::Ring2On())) bOn = false;
	if (bOn == bFrozenRoam) return;
	const FTransform AT = GetActorTransform();
	if (bOn)
	{
		// the ship / launch pad leaves the Frozen Ring
		if (LaunchPad) { LaunchPad->Destroy(); LaunchPad = nullptr; }
		DevProps.RemoveAll([](const FZoogiDevProp& P) { return P.Type == TEXT("ship"); });
		BaselineProps.RemoveAll([](const FZoogiDevProp& P) { return P.Type == TEXT("ship"); });
		using namespace ZoogiFrozenRoam;
		const bool bBridges = BridgeEntryOn();
		// court sockets (the classic start spots) unless the bridge entry is on (then Ring 2, in front of the bleachers)
		FVector Sock[4]; float SockYaw[4];
		for (int32 i = 0; i < 4; ++i) { Sock[i] = GetSpawnPoint(i) - FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f); SockYaw[i] = GetSpawnYaw(i); }
		bFrozenRoam = true;
		for (int32 i = 0; i < 4; ++i)
		{
			FZoogiDevProp S; S.Type = TEXT("spawn"); S.Id = FString::Printf(TEXT("spawn_%d"), i + 1);
			S.Location = bBridges ? AT.TransformPosition(RoamPol(SpawnR, SpawnYaw[i], FloorZ)) : Sock[i];
			S.Yaw = bBridges ? FRotator::NormalizeAxis(SpawnYaw[i] + 180.f) : SockYaw[i]; // face the court
			DevProps.Add(S); BaselineProps.Add(S);
			UpdateRoamProp(DevProps.Num() - 1);
		}
		for (int32 k = 0; k < (bBridges ? NumBridges : 0); ++k)
		{
			FZoogiDevProp L; L.Type = TEXT("launch"); L.Id = FString::Printf(TEXT("launch_%d"), FMath::RoundToInt(BridgeYaw[k]));
			L.Location = AT.TransformPosition(RoamPol(LaunchR, BridgeYaw[k], LaunchDeckZ));
			L.Yaw = FRotator::NormalizeAxis(BridgeYaw[k] + 180.f); // launch direction: into the court
			DevProps.Add(L); BaselineProps.Add(L);
			UpdateRoamProp(DevProps.Num() - 1);
		}
		// a dev-unlocked device runs its own spawn / gate placement
		if (ZoogiDevGate::IsUnlocked())
		{
			TArray<FZoogiDevProp> Dev;
			if (ZoogiLayout::FromJson(ZoogiDevConfig::Get(TEXT("level:") + GetArenaSlug(), ZoogiLayout::LayoutKey()), Dev) && Dev.Num()) ApplyLayout(Dev);
		}
		SetDevMarkersVisible(ZoogiLayout::IsDevMode());
		UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: on - ship removed, %d spawn points (%s), %d bridge launch points"), NumRoamProps(TEXT("spawn")), bBridges ? TEXT("Ring 2, in front of the bleachers") : TEXT("court start spots"), NumRoamProps(TEXT("launch")));
	}
	else
	{
		for (int32 i = DevProps.Num() - 1; i >= 0; --i)
		{
			if (DevProps[i].Type != TEXT("spawn") && DevProps[i].Type != TEXT("launch")) continue;
			for (const TWeakObjectPtr<UPrimitiveComponent>& C : DevProps[i].Comps) if (C.IsValid()) { Built.Remove(C.Get()); C->DestroyComponent(); }
			DevProps.RemoveAt(i);
		}
		BaselineProps.RemoveAll([](const FZoogiDevProp& P) { return P.Type == TEXT("spawn") || P.Type == TEXT("launch"); });
		bFrozenRoam = false;
		if (!LaunchPad)
		{
			PlaceLaunchPad();
			if (LaunchPad)
			{
				FZoogiDevProp S; S.Type = TEXT("ship"); S.Id = TEXT("ship"); S.Location = LaunchPad->GetActorLocation(); S.Yaw = LaunchPad->GetActorRotation().Yaw; S.Scale = LaunchPad->ShipScale;
				DevProps.Add(S); BaselineProps.Add(S);
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: off (launch pad back)"));
	}
}

void AZoogiArena::UpdateRoamProp(int32 Index)
{
	if (!DevProps.IsValidIndex(Index)) return;
	FZoogiDevProp& P = DevProps[Index];
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : P.Comps) if (C.IsValid()) { Built.Remove(C.Get()); C->DestroyComponent(); }
	P.Comps.Reset();
	const bool bSpawn = P.Type == TEXT("spawn");
	auto Flat = [&](UStaticMesh* Mesh, const FLinearColor& Col, float Glow, const FVector& At, const FRotator& Rot, const FVector& Scale, bool bAlways)
	{
		UStaticMeshComponent* M = AddMesh(Mesh, ZoogiUtil::GlowMID(this, Col, Glow), false, false);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->ComponentTags.Add(TEXT("NoCamBlock"));
		M->ComponentTags.Add(bAlways ? FName(TEXT("ZoogiRoamMarker")) : FName(TEXT("ZoogiDevMarker")));
		M->SetWorldLocationAndRotation(At, Rot);
		M->SetWorldScale3D(Scale);
		if (!bAlways) M->SetVisibility(ZoogiLayout::IsDevMode());
		P.Comps.Add(M);
	};
	const FRotator Dir(0.f, P.Yaw, 0.f);
	const FVector Fwd = Dir.Vector();
	if (bSpawn)
	{
		// dev-only: player-coloured pad + facing arrow
		int32 Who = 0; FParse::Value(*P.Id, TEXT("spawn_"), Who);
		const FLinearColor Col = ZoogiUtil::PlayerColor(FMath::Max(0, Who - 1));
		Flat(ZoogiUtil::BasicShape(TEXT("Cylinder")), Col, 2.f, P.Location + FVector(0, 0, 2.f), FRotator::ZeroRotator, FVector(1.5f, 1.5f, 0.03f), false);
		Flat(ZoogiUtil::BasicShape(TEXT("Cube")), FLinearColor::White, 2.f, P.Location + Fwd * 95.f + FVector(0, 0, 4.f), Dir, FVector(0.7f, 0.18f, 0.04f), false);
	}
	else
	{
		// the subtle launch-spot ring every player sees (spec 8.6) + a small arrow into the court; the gate width bars are dev-only
		const FLinearColor Ice(0.35f, 0.85f, 1.f);
		Flat(ZoogiUtil::BasicShape(TEXT("Cylinder")), Ice, 1.2f, P.Location + FVector(0, 0, 14.f), FRotator::ZeroRotator, FVector(1.15f, 1.15f, 0.02f), true);
		Flat(ZoogiUtil::BasicShape(TEXT("Cone")), Ice, 1.6f, P.Location + Fwd * 80.f + FVector(0, 0, 20.f), FRotator(-90.f, P.Yaw, 0.f), FVector(0.35f, 0.35f, 0.3f), true);
		const float GateW = ZoogiTune::Get(TEXT("bridge.width")) * P.Scale;
		const FVector Side = FRotator(0.f, P.Yaw + 90.f, 0.f).Vector();
		for (int32 s = -1; s <= 1; s += 2)
			Flat(ZoogiUtil::BasicShape(TEXT("Cube")), FLinearColor(1.f, 0.8f, 0.2f), 2.f, P.Location - Fwd * 160.f + Side * (GateW * s) + FVector(0, 0, 40.f), Dir, FVector(3.4f, 0.06f, 0.06f), false);
	}
}

void AZoogiArena::SetDevMarkersVisible(bool bOn)
{
	for (const FZoogiDevProp& P : DevProps)
		for (const TWeakObjectPtr<UPrimitiveComponent>& C : P.Comps)
			if (C.IsValid() && C->ComponentHasTag(TEXT("ZoogiDevMarker"))) C->SetVisibility(bOn);
}

// ------------------------------------------------------------------ game mode side

bool AZoogiGameMode::FrozenRoamOn() const
{
	return Arena && Arena->IsFrozenRoam() && !bComboMode && !bRoyaleMode;
}

bool AZoogiGameMode::FrozenNeedsEntry(int32 Player) const
{
	return FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn() && RoamEntered.IsValidIndex(Player) && !RoamEntered[Player];
}

bool AZoogiGameMode::FrozenRoamActive() const
{
	const AZoogiBall* B = GetActiveBall();
	return B && !B->bOut && FrozenNeedsEntry(ActivePlayer) && IsAimStep();
}

bool AZoogiGameMode::FrozenPrepReady() const
{
	return FrozenRoamActive() && RoamState == EZoogiRoamState::LaunchPrep && RoamStateT >= ZoogiTune::Get(TEXT("bridge.settle"));
}

bool AZoogiGameMode::FrozenCanLaunch() const
{
	const AZoogiBall* B = GetActiveBall();
	if (!FrozenRoamOn() || !B || B->bCPU || !IsAimStep()) return false;
	return FrozenNeedsEntry(ActivePlayer) ? FrozenPrepReady() : true;
}

void AZoogiGameMode::FrozenStartMatch()
{
	RoamEntered.Init(!FrozenRoamOn(), Balls.Num());
	RoamState = EZoogiRoamState::None;
	RoamStateT = 0.f;
	RoamBridge = -1;
	RoamAimOffset = 0.f;
	RoamMoveInput = FVector2D::ZeroVector;
	if (!FrozenRoamOn()) return;
	// alpha transforming robot-ball for every player while the Frozen Ring is under test (roster untouched)
	for (AZoogiBall* B : Balls) if (B) B->EnableMorph();
	for (AZoogiBall* B : Balls)
		if (B) UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: P%d spawns on the ground at (%.0f, %.0f, %.0f) r=%.0f yaw %.0f (state %s)%s"), B->PlayerIndex + 1,
			B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z, FVector2D(B->BallLocation()).Size(), B->SpawnYaw, ZoogiFrozenRoam::StateName(EZoogiRoamState::Spawning), B->Morph ? TEXT(", robot-ball model") : TEXT(""));
}

void AZoogiGameMode::FrozenRespawn(AZoogiBall* Ball)
{
	if (!Ball || !Arena) return;
	const int32 P = Ball->PlayerIndex;
	const FVector Want = Arena->GetSpawnPoint(P);
	const FVector C = Arena->GetActorLocation();
	auto Free = [&](const FVector& L)
	{
		for (const AZoogiBall* O : Balls) if (O && O != Ball && !O->bOut && FVector::Dist2D(O->BallLocation(), L) < ZoogiConst::ZoogiRadius * 2.6f) return false;
		return true;
	};
	FVector At = Want;
	if (!Free(At))
	{
		const FVector2D D(Want - C);
		const float R = D.Size(), A0 = FMath::Atan2(D.Y, D.X);
		for (int32 k = 1; k <= 8; ++k)
		{
			const float A = A0 + FMath::DegreesToRadians((k % 2 ? 1.f : -1.f) * ((k + 1) / 2) * 7.f);
			const FVector L(C.X + FMath::Cos(A) * R, C.Y + FMath::Sin(A) * R, Want.Z);
			if (Free(L)) { At = L; break; }
		}
	}
	Ball->PlaceAt(At, Arena->GetSpawnYaw(P));
	Ball->LastHitPlayer = -1;
	Ball->DriveAccel = 0.f; Ball->DriveInput = FVector2D::ZeroVector;
	AZoogiFX::KFX(GetWorld(), TEXT("Effect5"), At - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), 0.45f, 2.5f);
	if (RoamEntered.IsValidIndex(P)) RoamEntered[P] = false;
	UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: P%d knocked out -> back on its spawn spot at (%.0f, %.0f)%s"), P + 1, At.X, At.Y, ZoogiFrozenRoam::BridgeEntryOn() ? TEXT(" (re-enters through a bridge)") : TEXT(""));
}

void AZoogiGameMode::FrozenTestSpawn(int32 SpawnIdx)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !Arena || !FrozenRoamOn()) return;
	const AZoogiArena::FZoogiDevProp* S = Arena->FindRoamProp(TEXT("spawn"), SpawnIdx);
	if (!S) return;
	B->PlaceAt(FVector(S->Location.X, S->Location.Y, S->Location.Z + ZoogiConst::ZoogiRadius + 2.f), S->Yaw);
	if (RoamEntered.IsValidIndex(ActivePlayer)) RoamEntered[ActivePlayer] = false;
	RoamState = EZoogiRoamState::FreeRoam; RoamStateT = 0.f; RoamBridge = -1;
	UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: TEST SPAWN %s -> P%d placed at (%.0f, %.0f, %.0f) yaw %.0f"), *S->Id, ActivePlayer + 1, S->Location.X, S->Location.Y, S->Location.Z, S->Yaw);
}

void AZoogiGameMode::FrozenSetMoveInput(const FVector2D& WorldInput)
{
	RoamMoveInput = WorldInput.GetClampedToMaxSize(1.f);
}

void AZoogiGameMode::FrozenAdjustAim(float DeltaDeg)
{
	const float Cone = ZoogiTune::Get(TEXT("bridge.cone"));
	RoamAimOffset = FMath::Clamp(RoamAimOffset + DeltaDeg, -Cone, Cone);
}

float AZoogiGameMode::FrozenPrepYaw() const
{
	return FRotator::NormalizeAxis(RoamPrepYaw + RoamAimOffset);
}

bool AZoogiGameMode::FrozenHumanLaunch(float Yaw, float Power)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !FrozenCanLaunch()) { UE_LOG(LogZoogi, Log, TEXT("FROZEN LAUNCH refused: not in a launch state (roam %s, aim step %d)"), ZoogiFrozenRoam::StateName(RoamState), IsAimStep() ? 1 : 0); return false; }
	Power = FMath::Clamp(Power, 0.08f, 1.f);
	const bool bEntry = FrozenNeedsEntry(ActivePlayer);
	if (bEntry || !(TargetLock.IsValid() && !bSoftLock))
	{
		// bridge (or no target picked): straight along the chosen heading
		if (bEntry) ClearLock();
		const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
		SetFreeAim(B->BallLocation() + Dir * 900.f);
	}
	B->AimYaw = Yaw;
	bRoamLaunchGate = true;
	const bool bOk = HumanDrag(Power, true);
	bRoamLaunchGate = false;
	UE_LOG(LogZoogi, Log, TEXT("FROZEN LAUNCH P%d: %s yaw %.0f power %.0f%% -> %s"), ActivePlayer + 1, bEntry ? TEXT("bridge") : (TargetLock.IsValid() ? TEXT("at target") : TEXT("free")), Yaw, Power * 100.f, bOk ? TEXT("released") : TEXT("refused"));
	return bOk;
}

void AZoogiGameMode::FrozenOnLaunched()
{
	if (!FrozenRoamOn()) return;
	if (FrozenNeedsEntry(ActivePlayer) && RoamState == EZoogiRoamState::LaunchPrep)
	{
		RoamEntered[ActivePlayer] = true;
		RoamState = EZoogiRoamState::Launched;
		RoamStateT = 0.f;
		UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: P%d %s -> %s (into the central arena)"), ActivePlayer + 1, ZoogiFrozenRoam::StateName(EZoogiRoamState::LaunchPrep), ZoogiFrozenRoam::StateName(EZoogiRoamState::Launched));
	}
	if (AZoogiBall* B = GetActiveBall()) { B->DriveAccel = 0.f; B->DriveInput = FVector2D::ZeroVector; }
}

bool AZoogiGameMode::FrozenBlocksCPUShot() const
{
	// a CPU that still has to enter waits until it is steadied on a bridge launch point (then PlanCPUShot runs from there)
	return FrozenRoamOn() && FrozenNeedsEntry(ActivePlayer) && !(RoamState == EZoogiRoamState::LaunchPrep && bRoamCpuPlanned);
}

void AZoogiGameMode::SetRoamState(EZoogiRoamState S, const TCHAR* Why)
{
	if (S == RoamState) return;
	UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: P%d %s -> %s (%s)"), ActivePlayer + 1, ZoogiFrozenRoam::StateName(RoamState), ZoogiFrozenRoam::StateName(S), Why);
	RoamState = S;
	RoamStateT = 0.f;
	static int32 NShots = 0; // capture runs: one frame per state change (first few)
	if (NShots < 10) TakeShotDelayed(FString::Printf(TEXT("roam_%02d_p%d_%s"), ++NShots, ActivePlayer + 1, ZoogiFrozenRoam::StateName(S)), S == EZoogiRoamState::LaunchPrep ? 0.7f : 0.4f);
}

void AZoogiGameMode::TickFrozenRoam(float Dt)
{
	if (!FrozenRoamOn() || !Arena || !ZoogiFrozenRoam::BridgeEntryOn()) return;
	{
		// capture runs (-ZoogiFrozenKnockTest): during the 3rd shot P2 is knocked out once - it must come back on the outer ring
		static const bool bKnock = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrozenKnockTest"));
		static bool bKnocked = false;
		if (bKnock && !bKnocked && ShotsFired >= 3 && Phase == EZoogiPhase::Rolling && Balls.IsValidIndex(1) && Balls[1] && ActivePlayer != 1 && !FrozenNeedsEntry(1))
		{
			bKnocked = true;
			Balls[1]->SetOut();
			UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM TEST: P2 knocked out"));
		}
	}
	AZoogiBall* B = GetActiveBall();
	const bool bActive = FrozenRoamActive();
	for (AZoogiBall* O : Balls) if (O && (O != B || !bActive) && O->DriveAccel > 0.f) { O->DriveAccel = 0.f; O->DriveInput = FVector2D::ZeroVector; }
	if (!bActive)
	{
		if (!FrozenNeedsEntry(ActivePlayer) && RoamState != EZoogiRoamState::Launched) RoamState = EZoogiRoamState::None;
		return;
	}
	if (RoamState == EZoogiRoamState::None || RoamState == EZoogiRoamState::Launched || RoamState == EZoogiRoamState::Spawning)
	{
		SetRoamState(EZoogiRoamState::FreeRoam, B->bCPU ? TEXT("CPU turn") : TEXT("left stick rolls"));
		RoamBridge = -1; RoamAimOffset = 0.f; bRoamCpuPlanned = false; RoamCpuBridge = -1;
		if (!B->bCPU) { BigText = TEXT("ROLL TO A BRIDGE"); BigColor = FLinearColor(0.55f, 0.9f, 1.f); BigAge = 0.f; }
	}
	RoamStateT += Dt;
	const FTransform AT = Arena->GetActorTransform();
	const FVector BW = B->BallLocation();
	const FVector BL = AT.InverseTransformPosition(BW);
	const FVector2D P2(BL.X, BL.Y);
	const float R = P2.Size();
	if (R < ZoogiFrozen::R2CrevIn - 10.f && RoamState != EZoogiRoamState::LaunchPrep)
	{
		// pushed into the court by someone else's shot without ringing out: it is in the arena already
		RoamEntered[ActivePlayer] = true;
		SetRoamState(EZoogiRoamState::Launched, TEXT("already inside the court"));
		B->DriveAccel = 0.f; B->DriveInput = FVector2D::ZeroVector;
		return;
	}
	const float Tw = ZoogiTune::Get(TEXT("bridge.width")), Catch = ZoogiTune::Get(TEXT("bridge.catch"));

	// which bridge corridor the Zoogi is in (launch point + its outward axis)
	int32 Corr = -1; float Lat = 0.f, Along = 0.f; FVector2D CorrL = FVector2D::ZeroVector, CorrOut = FVector2D::ZeroVector; FVector CorrW = FVector::ZeroVector;
	const int32 NL = Arena->NumRoamProps(TEXT("launch"));
	for (int32 k = 0; k < NL; ++k)
	{
		const AZoogiArena::FZoogiDevProp* L = Arena->FindRoamProp(TEXT("launch"), k);
		if (!L) continue;
		const FVector LL = AT.InverseTransformPosition(L->Location);
		const FVector2D L2(LL.X, LL.Y);
		const FVector2D Out = -FVector2D(FRotator(0.f, L->Yaw, 0.f).Vector()).GetSafeNormal(); // the launch heading points in; the gate opens outward
		const FVector2D Rel = P2 - L2;
		const float A = FVector2D::DotProduct(Rel, Out), S = FVector2D::CrossProduct(Out, Rel);
		if (A > -Catch - 60.f && A < 700.f && FMath::Abs(S) < Tw * L->Scale) { Corr = k; Lat = S; Along = A; CorrL = L2; CorrOut = Out; CorrW = L->Location; break; }
	}
	const bool bOnDeck = Corr >= 0 && R < ZoogiFrozenRoam::RoamRMin + 25.f;

	// input: the human's left stick, or the CPU / test driver steering to its bridge
	static const bool bRoamTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrozenRoamTest"));
	FVector2D In = RoamMoveInput;
	if (B->bCPU || bRoamTest)
	{
		if (RoamCpuBridge < 0 || RoamCpuBridge >= NL)
		{
			float Best = 1e9f;
			const float MyA = FMath::RadiansToDegrees(FMath::Atan2(P2.Y, P2.X));
			for (int32 k = 0; k < NL; ++k)
				if (const AZoogiArena::FZoogiDevProp* L = Arena->FindRoamProp(TEXT("launch"), k))
				{
					const FVector LL = AT.InverseTransformPosition(L->Location);
					const float D = FMath::Abs(FMath::FindDeltaAngleDegrees(MyA, FMath::RadiansToDegrees(FMath::Atan2(LL.Y, LL.X))));
					if (D < Best) { Best = D; RoamCpuBridge = k; }
				}
		}
		if (const AZoogiArena::FZoogiDevProp* L = Arena->FindRoamProp(TEXT("launch"), RoamCpuBridge))
		{
			const FVector LL = AT.InverseTransformPosition(L->Location);
			const FVector2D L2(LL.X, LL.Y);
			const float La = FMath::RadiansToDegrees(FMath::Atan2(L2.Y, L2.X)), Ma = FMath::RadiansToDegrees(FMath::Atan2(P2.Y, P2.X));
			const float dA = FMath::FindDeltaAngleDegrees(Ma, La);
			const FVector2D Rad = P2.GetSafeNormal(), Tan(-Rad.Y, Rad.X);
			FVector2D Want;
			if (bOnDeck || FMath::Abs(dA) * UE_PI / 180.f * R < Tw * 0.5f) Want = (L2 - P2).GetSafeNormal();            // line up, then straight in
			else Want = (Tan * FMath::Sign(dA) + Rad * ((1905.f - R) / 120.f)).GetSafeNormal();                      // round the ring
			const float Near = FVector2D::Distance(P2, L2);
			In = FVector2D(AT.TransformVectorNoScale(FVector(Want, 0.f))) * FMath::Clamp(Near / 260.f, 0.45f, 1.f);
		}
	}

	if (RoamState == EZoogiRoamState::FreeRoam || RoamState == EZoogiRoamState::BridgeApproach)
	{
		B->DriveAccel = ZoogiTune::Get(TEXT("roam.accel"));
		B->DriveMaxSpeed = ZoogiTune::Get(TEXT("roam.speed")) * (bOnDeck ? 0.7f : 1.f);
		B->DriveInput = In;
		// boundaries: the Ring 2 band, except down a bridge deck toward its launch point (no crevasse, no ramps, no islets)
		if (USphereComponent* Sp = B->Sphere)
		{
			FVector V = Sp->GetPhysicsLinearVelocity();
			FVector Fix = BL;
			bool bFix = false;
			const FVector2D Rad = P2.GetSafeNormal();
			if (bOnDeck)
			{
				const float LimLat = Tw * 0.85f;
				if (FMath::Abs(Lat) > LimLat) { const FVector2D Side(-CorrOut.Y, CorrOut.X); const FVector2D N = CorrL + CorrOut * Along + Side * FMath::Sign(Lat) * LimLat; Fix.X = N.X; Fix.Y = N.Y; bFix = true; }
			}
			else if (R < ZoogiFrozenRoam::RoamRMin || R > ZoogiFrozenRoam::RoamRMax)
			{
				const float RR = FMath::Clamp(R, ZoogiFrozenRoam::RoamRMin, ZoogiFrozenRoam::RoamRMax);
				Fix.X = Rad.X * RR; Fix.Y = Rad.Y * RR; bFix = true;
				const FVector RadW = AT.TransformVectorNoScale(FVector(Rad, 0.f));
				const float Vr = FVector::DotProduct(V, RadW);
				if ((R < ZoogiFrozenRoam::RoamRMin && Vr < 0.f) || (R > ZoogiFrozenRoam::RoamRMax && Vr > 0.f)) V -= RadW * Vr;
			}
			if (bFix)
			{
				Sp->SetWorldLocation(AT.TransformPosition(Fix), false, nullptr, ETeleportType::TeleportPhysics);
				Sp->SetPhysicsLinearVelocity(V);
			}
		}
		// camera heading follows the roll (rate-limited so a child's stick wobble does not swing the view)
		const FVector Vel = B->GetBallVelocity();
		if (FVector2D(Vel).Size() > 80.f)
		{
			const float Want = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X));
			const float Rate = ZoogiTune::Get(TEXT("roam.camturn")) * Dt;
			B->AimYaw = FRotator::NormalizeAxis(B->AimYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(B->AimYaw, Want), -Rate, Rate));
		}
		if (RoamState == EZoogiRoamState::FreeRoam && bOnDeck)
		{
			RoamBridge = Corr;
			SetRoamState(EZoogiRoamState::BridgeApproach, *FString::Printf(TEXT("gate %d entered - no launch, still rolling"), Corr));
		}
		else if (RoamState == EZoogiRoamState::BridgeApproach && !bOnDeck) SetRoamState(EZoogiRoamState::FreeRoam, TEXT("rolled back off the bridge"));
		if (bOnDeck && FVector2D::Distance(P2, CorrL) < Catch)
		{
			RoamBridge = Corr;
			RoamPrepStart = BW;
			RoamPrepAnchor = FVector(CorrW.X, CorrW.Y, BW.Z);
			const AZoogiArena::FZoogiDevProp* L = Arena->FindRoamProp(TEXT("launch"), Corr);
			RoamPrepYaw = L ? L->Yaw : B->AimYaw;
			RoamAimOffset = 0.f;
			B->DriveAccel = 0.f; B->DriveInput = FVector2D::ZeroVector;
			SetRoamState(EZoogiRoamState::LaunchPrep, *FString::Printf(TEXT("reached %s - free roll ends"), L ? *L->Id : TEXT("launch point")));
			AZoogiFX::Spawn(GetWorld(), RoamPrepAnchor - FVector(0, 0, ZoogiConst::ZoogiRadius - 6.f), FLinearColor(0.35f, 0.85f, 1.f), 110.f, 0.5f, true, 5.f);
			ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.6f, 0.9f);
			if (!B->bCPU) { BigText = TEXT("PULL BACK & LET GO!"); BigColor = FLinearColor(1.f, 0.85f, 0.3f); BigAge = 0.f; }
		}
	}
	else if (RoamState == EZoogiRoamState::LaunchPrep)
	{
		// steady on the launch point, facing into the court; the left stick only turns the aim a little (FrozenAdjustAim)
		B->DriveAccel = 0.f; B->DriveInput = FVector2D::ZeroVector;
		const float Settle = ZoogiTune::Get(TEXT("bridge.settle"));
		const float A = Settle > 0.f ? SmoothStep01(RoamStateT / Settle) : 1.f;
		if (USphereComponent* Sp = B->Sphere)
		{
			const FVector At = FMath::Lerp(RoamPrepStart, RoamPrepAnchor, A);
			Sp->SetWorldLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			Sp->SetPhysicsLinearVelocity(Sp->GetPhysicsLinearVelocity() * (1.f - A));
			if (A >= 1.f) { Sp->SetPhysicsLinearVelocity(FVector::ZeroVector); Sp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); }
		}
		B->AimYaw = FrozenPrepYaw();
		if (A >= 1.f && B->bCPU && !bRoamCpuPlanned)
		{
			bRoamCpuPlanned = true;
			PlanCPUShot();      // aims from the bridge now
			StepTime = 0.f;
			UE_LOG(LogZoogi, Log, TEXT("FROZEN ROAM: CPU P%d steady on the bridge - plans its launch"), ActivePlayer + 1);
		}
		if (A >= 1.f && bRoamTest && !B->bCPU && RoamStateT > Settle + 1.0f) FrozenHumanLaunch(FrozenPrepYaw(), 0.7f); // capture runs: a right-stick pull of 70 %
	}
}
