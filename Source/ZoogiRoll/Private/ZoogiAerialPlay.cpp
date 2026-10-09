// Battle Episode Engine T3: the state machine side of the Frozen Ring aerial props (Sky Bumper, Resonance Halo, Launch
// Rail), the Rail Release timing mini-game and the debug zone layers (refs frozen_zones_aerial, rail_release_timing,
// sky_bumper_detail, resonance_halo_detail), plus (Oct 8) the Frozen map layout, CPU prop plans and aerial captures.
// RULE ZERO: real physics velocity changes in the normal simulation (game time, no holds, no time dilation), only when
// props exist (Frozen map layout / -ZoogiBattleProps / a test); cinematics never touch them.
#include "ZoogiBattleStateMachine.h"
#include "ZoogiAerialProps.h"
#include "ZoogiMiniGame.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiRock.h"
#include "ZoogiOrb.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

// ------------------------------------------------------------------------------------------------- Rail Release

EZoogiRailWindow UZoogiMiniGameRailRelease::WindowAt(const FZoogiBattleTuning& T, float Time)
{
	if (Time < 0.f) return EZoogiRailWindow::None;
	if (Time < T.RailEarlyWindow) return EZoogiRailWindow::Early;
	if (Time < T.RailEarlyWindow + T.RailCenterWindow) return EZoogiRailWindow::Center;
	if (Time < T.RailEarlyWindow + T.RailCenterWindow + T.RailLateWindow) return EZoogiRailWindow::Late;
	return EZoogiRailWindow::None;
}

FVector UZoogiMiniGameRailRelease::LaunchVelocity(const FZoogiBattleTuning& T, EZoogiRailWindow W, const FVector& Forward, const FVector& Pos, float Speed)
{
	FVector2D F(Forward.X, Forward.Y);
	F = F.IsNearlyZero() ? FVector2D(1.f, 0.f) : F.GetSafeNormal();
	float Pitch = T.RailCenterPitch;
	if (W == EZoogiRailWindow::Early) Pitch = T.RailHighPitch;
	else if (W == EZoogiRailWindow::Late)
	{
		// bank toward the arena centre (the side the centre is on; deterministic tie -> left)
		const FVector2D ToC = -FVector2D(Pos.X, Pos.Y);
		const float Cross = F.X * ToC.Y - F.Y * ToC.X;
		F = F.GetRotated(Cross >= 0.f ? T.RailLateYaw : -T.RailLateYaw);
		Pitch = T.RailLatePitch;
	}
	const float P = FMath::DegreesToRadians(Pitch);
	return FVector(F.X * FMath::Cos(P), F.Y * FMath::Cos(P), FMath::Sin(P)) * Speed;
}

bool UZoogiMiniGameRailRelease::SubmitInput(UZoogiBattleStateMachine* SM, int32 Choice, const FString& Source)
{
	if (!bInputOpen || bReleaseRequested || !SM) return false;
	const EZoogiRailWindow W = WindowAt(SM->GetTuning(), Elapsed);
	if (W == EZoogiRailWindow::None) return false;
	Window = W;
	bReleaseRequested = true;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame RailRelease input P%d window=%s t=%.3fs src=%s"), Owner + 1, ZoogiBattle::RailWindowName(W), Elapsed, *Source);
	return true;
}

EZoogiChallengeResult UZoogiMiniGameRailRelease::TickGame(UZoogiBattleStateMachine* SM, float GameDt)
{
	if (!bInputOpen) return EZoogiChallengeResult::Pending;
	Elapsed += GameDt;
	const FZoogiBattleTuning& T = SM->GetTuning();
	if (bCPU && !bReleaseRequested)
	{
		// deterministic: the middle of the CPU's window (center unless a map-layout prop plan chose another, Oct 8)
		float At = T.RailEarlyWindow + T.RailCenterWindow * 0.5f;
		if (CPUWindow == EZoogiRailWindow::Early) At = T.RailEarlyWindow * 0.5f;
		else if (CPUWindow == EZoogiRailWindow::Late) At = T.RailEarlyWindow + T.RailCenterWindow + T.RailLateWindow * 0.5f;
		if (Elapsed >= At) SubmitInput(SM, 0, TEXT("cpu"));
	}
	if (bReleaseRequested) return EZoogiChallengeResult::Success;
	return Elapsed >= T.RailEarlyWindow + T.RailCenterWindow + T.RailLateWindow ? EZoogiChallengeResult::Timeout : EZoogiChallengeResult::Pending;
}

FString UZoogiMiniGameRailRelease::Apply(UZoogiBattleStateMachine* SM) const
{
	const AZoogiGameMode* G = SM->GetGame();
	AZoogiBall* B = G && G->Balls.IsValidIndex(Body) ? G->Balls[Body].Get() : nullptr;
	if (!B || !B->Sphere) return TEXT("no body");
	const FZoogiBattleTuning& T = SM->GetTuning();
	const float MinSpeed = (Rail.IsValid() && Rail->RailMinSpeed > 0.f) ? Rail->RailMinSpeed : T.RailMinSpeed; // map rail: MapRailMinSpeed
	const float Speed = FMath::Max(EntrySpeed * T.RailSpeedMul, MinSpeed);
	const FVector V = LaunchVelocity(T, Window, RailForward, B->BallLocation(), Speed);
	B->Sphere->SetPhysicsLinearVelocity(V);
	SM->LastRailVelocity = V;
	return FString::Printf(TEXT("%s v=(%.0f,%.0f,%.0f)|%.0f| pitch=%.1f at (%.0f,%.0f,%.0f)"), ZoogiBattle::RailWindowName(Window), V.X, V.Y, V.Z, V.Size(),
		FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(V.Z / FMath::Max(1.f, V.Size()), -1.f, 1.f))), B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z);
}

EZoogiChallengeResult UZoogiMiniGameRailRelease::Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const
{
	OutDetail = FString::Printf(TEXT("window=%s %s"), ZoogiBattle::RailWindowName(Seg.RailWindow), Seg.RailWindow == EZoogiRailWindow::None ? TEXT("(never released on a rail)") : *Seg.Detail);
	return Seg.RailWindow != EZoogiRailWindow::None ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
}

bool UZoogiBattleStateMachine::IsRailOpen() const
{
	const UZoogiMiniGameRailRelease* M = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame);
	return M && M->bInputOpen && !M->bReleaseRequested;
}

bool UZoogiBattleStateMachine::SubmitRailRelease(const FString& Source)
{
	UZoogiMiniGameRailRelease* M = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame);
	return M && M->SubmitInput(this, 0, Source);
}

void UZoogiBattleStateMachine::StartRail(AZoogiAerialProp* Rail, AZoogiBall* Ball)
{
	AZoogiGameMode* G = GM();
	const int32 Body = Ball ? Ball->PlayerIndex : -1;
	FString Skip;
	if (!bFlow) Skip = TEXT("flow_off");
	else if (!G || G->Phase != EZoogiPhase::Rolling) Skip = TEXT("not_rolling");
	else if (Body != CurrentActor) Skip = TEXT("not_the_shooter");
	else if (ActiveMiniGame) Skip = TEXT("busy");
	else if (State != EZoogiBattleState::PhysicsResolve) Skip = FString::Printf(TEXT("state_%s"), ZoogiBattle::StateName(State));
	if (!Skip.IsEmpty())
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE rail %s P%d no mini-game (%s)"), *Rail->Label(), Body + 1, *Skip);
		return;
	}
	UZoogiMiniGameRailRelease* M = Cast<UZoogiMiniGameRailRelease>(UZoogiMiniGame::Create(this, EZoogiChallenge::RailRelease));
	M->Body = Body;
	M->Rail = Rail;
	M->RailForward = Rail->Forward;
	M->bCPU = Ball->bCPU;
	if (M->bCPU && CPUPlan.IsFor(Body, G->TurnCounter) && CPUPlan.Kind == EZoogiPropKind::LaunchRail) M->CPUWindow = CPUPlan.RailWindow;
	M->bSegment = Pending.Owner == Body && Pending.Challenge == EZoogiChallenge::RailRelease && Pending.ActionId == CurrentActionId;
	M->Enter(this, Body, Pending);
	M->bInputOpen = true;
	// the rail guides the Zoogi: velocity along the rail at its entry speed (horizontal)
	const FVector V = Ball->Sphere ? Ball->Sphere->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	M->EntrySpeed = FVector2D(V.X, V.Y).Size();
	if (Rail->RailMinSpeed > 0.f && Ball->Sphere)
	{
		// map rail (Oct 8): the rail guide seats the Zoogi on its centre line (no lateral offset), so the windows read the
		// same from any entry side (EARLY threads the hanging halo). T3 debug / test rails keep the old entry behaviour.
		const FVector2D F2 = FVector2D(Rail->Forward.X, Rail->Forward.Y).GetSafeNormal();
		const FVector BL = Ball->BallLocation();
		const FVector2D Rel = FVector2D(BL.X, BL.Y) - FVector2D(Rail->GetActorLocation());
		const FVector2D On = FVector2D(Rail->GetActorLocation()) + F2 * FVector2D::DotProduct(Rel, F2);
		Ball->Sphere->SetWorldLocation(FVector(On.X, On.Y, BL.Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Ball->Sphere) Ball->Sphere->SetPhysicsLinearVelocity(FVector(FVector2D(Rail->Forward.X, Rail->Forward.Y).GetSafeNormal() * M->EntrySpeed, V.Z));
	LastRailEntrySpeed = M->EntrySpeed;
	ActiveMiniGame = M;
	++RailEntries;
	Transition(EZoogiBattleState::MiniGame, FString::Printf(TEXT("rail_release P%d"), Body + 1));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame RailRelease enter P%d rail=%s speed=%.0f windows=%.2f/%.2f/%.2f cpu=%d cpu_window=%s segment=%d"), Body + 1, *Rail->Label(), M->EntrySpeed,
		Tune.RailEarlyWindow, Tune.RailCenterWindow, Tune.RailLateWindow, M->bCPU ? 1 : 0, ZoogiBattle::RailWindowName(M->CPUWindow), M->bSegment ? 1 : 0);
	AerialShot(TEXT("rail_window"), 0.12f);
}

void UZoogiBattleStateMachine::TickRail(UZoogiMiniGameRailRelease* M)
{
	UWorld* W = GetWorld();
	if (!W) return;
	const float GameDt = W->IsPaused() ? 0.f : W->GetDeltaSeconds();
	const EZoogiChallengeResult R = M->TickGame(this, GameDt);
	if (R != EZoogiChallengeResult::Pending) FinishRail(M, R, R == EZoogiChallengeResult::Timeout ? TEXT("window_closed") : TEXT("released"));
}

void UZoogiBattleStateMachine::FinishRail(UZoogiMiniGameRailRelease* M, EZoogiChallengeResult R, const FString& Why)
{
	ActiveMiniGame = nullptr;
	FString Applied = TEXT("-");
	if (R == EZoogiChallengeResult::Success)
	{
		Applied = M->Apply(this);
		++RailReleases;
		LastRailWindow = M->Window;
		NotePeakCause(M->Body, *(FString(TEXT("rail_")) + FString(ZoogiBattle::RailWindowName(M->Window)).ToLower()));
		ActionPropTags.Add(FString::Printf(TEXT("%s:release_%s"), M->Rail.IsValid() ? *M->Rail->Label() : TEXT("rail"), ZoogiBattle::RailWindowName(M->Window)));
		AerialShot(TEXT("rail_release"), 0.3f);
		if (M->bSegment && Pending.Owner == M->Body) { Pending.RailWindow = M->Window; Pending.Detail = Applied; }
	}
	else if (R == EZoogiChallengeResult::Timeout) ++RailTimeouts;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame RailRelease result=%s P%d window=%s t=%.3fs applied=\"%s\" why=%s"), ZoogiBattle::ResultName(R), M->Body + 1,
		ZoogiBattle::RailWindowName(M->Window), M->Elapsed, *Applied, *Why);
	const bool bInMini = State == EZoogiBattleState::MiniGame;
	if (R == EZoogiChallengeResult::Timeout && M->bSegment && Pending.Owner == M->Body)
	{
		Pending.Result = R;
		FZoogiPendingSegment Seg = Pending;
		Pending = FZoogiPendingSegment();
		RecordShatter(Seg, TEXT("RailRelease_TIMEOUT"));
		if (bInMini)
		{
			Transition(EZoogiBattleState::ShatterFailure, TEXT("rail_release_timeout"));
			Transition(EZoogiBattleState::PhysicsResolve, TEXT("ordinary_resolution"));
		}
		return;
	}
	if (bInMini) Transition(EZoogiBattleState::PhysicsResolve, R == EZoogiChallengeResult::Success ? TEXT("rail_released") : TEXT("rail_missed"));
}

// ------------------------------------------------------------------------------------------------- props / zones

AZoogiAerialProp* UZoogiBattleStateMachine::SpawnProp(EZoogiPropKind Kind, const FVector& Location, const FVector& Forward, float Size, bool bAerialRing, FName Tag)
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiAerialProp* A = W->SpawnActor<AZoogiAerialProp>(AZoogiAerialProp::StaticClass(), Location, FRotator::ZeroRotator, P);
	if (!A) return nullptr;
	if (Size <= 0.f) Size = Kind == EZoogiPropKind::SkyBumper ? Tune.SkyBumperRadius : (Kind == EZoogiPropKind::ResonanceHalo ? Tune.HaloRadius : 60.f);
	A->ReboundMul = Tune.SkyBumperRebound;
	A->ReboundMaxSpeed = Tune.SkyBumperMaxSpeed;
	A->bAerialRing = bAerialRing;
	A->Tag = Tag;
	A->Build(Kind, Props.Num() + 1, Location, Forward, Size);
	A->BuildVisuals(GM() ? GM()->Arena.Get() : nullptr); // placeholder visuals, no collision
	Props.Add(A);
	return A;
}

void UZoogiBattleStateMachine::ClearProps()
{
	for (AZoogiAerialProp* A : Props) if (A) A->Destroy();
	Props.Reset();
	PropPrevLocal.Reset();
}

void UZoogiBattleStateMachine::SpawnDefaultProps()
{
	// [ZoogiRoll.Aerial] bAerialProps: one of each on clear court spots (debug layout for manual play; tests place their own)
	AZoogiGameMode* G = GM();
	if (!G || !G->Arena) return;
	bDefaultPropsSpawned = true;
	const float R = AZoogiArena::ActiveKnockoffR;
	const EZoogiPropKind Kinds[] = { EZoogiPropKind::SkyBumper, EZoogiPropKind::ResonanceHalo, EZoogiPropKind::LaunchRail };
	for (int32 k = 0; k < 3; ++k)
	{
		for (int32 a = 0; a < 24; ++a)
		{
			const float Th = FMath::DegreesToRadians(120.f * k + 15.f * a);
			const FVector2D P = FVector2D(FMath::Cos(Th), FMath::Sin(Th)) * (0.5f * R);
			if (G->Arena->IsOffCourt(P) || !G->Arena->IsSpotClear(P, 160.f)) continue;
			bool bNearBall = false;
			for (const AZoogiBall* B : G->Balls) if (B && FVector2D::Distance(FVector2D(B->BallLocation()), P) < 260.f) bNearBall = true;
			if (bNearBall) continue;
			const FVector Tangent(-FMath::Sin(Th), FMath::Cos(Th), 0.f);
			SpawnProp(Kinds[k], FVector(P, Kinds[k] == EZoogiPropKind::SkyBumper ? ZoogiConst::ZoogiRadius : 0.f), Tangent);
			break;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE props default layout spawned=%d"), Props.Num());
}

// ------------------------------------------------------------------------------------------------- Frozen map layout
// Oct 8 (refs frozen_zones_aerial / minigame_layout_frozen / rail_release_timing, LAYOUT ONLY): the aerial layer as part of
// the real Frozen Ring in normal matches. Props are indestructible fixtures (mini-game objectives, not arena pieces: the
// pillars stay the only unbreakable PIECES); placeholder visuals never collide. Placement keeps clear of the spawn pads
// (r980), the orb ring (r500) / centre triangle (r90), the pillar ring (r680), rocks and the platform; 2v2 combo keeps its
// own Frozen bumpers / gates (no map props there).

namespace
{
	FVector FzPol(float R, float YawDeg, float Z)
	{
		const float A = FMath::DegreesToRadians(YawDeg);
		return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
	}
	const TCHAR* ArenaKey(EZoogiArena A)
	{
		switch (A)
		{
		case EZoogiArena::Meadow: return TEXT("Meadow");
		case EZoogiArena::Frozen: return TEXT("Frozen");
		case EZoogiArena::Volcanic: return TEXT("Volcanic");
		case EZoogiArena::Cosmic: return TEXT("Cosmic");
		case EZoogiArena::Arabian: return TEXT("Arabian");
		case EZoogiArena::Tomb: return TEXT("Tomb");
		case EZoogiArena::Neon: return TEXT("Neon");
		case EZoogiArena::Training: return TEXT("Training");
		default: return TEXT("-");
		}
	}
	const TCHAR* PropKindName(EZoogiPropKind K)
	{
		switch (K)
		{
		case EZoogiPropKind::SkyBumper: return TEXT("bumper");
		case EZoogiPropKind::ResonanceHalo: return TEXT("halo");
		case EZoogiPropKind::LaunchRail: return TEXT("rail");
		default: return TEXT("gate");
		}
	}
}

bool UZoogiBattleStateMachine::WantsMapProps() const
{
	const AZoogiGameMode* G = GM();
	if (!bFlow || !Tune.bAerialProps || !G || !G->Arena || G->bComboMode) return false;
	if (!TestCase.IsEmpty() && !TestCase.StartsWith(TEXT("map_"))) return false; // the T5 cases place their own props
	if (Tune.AerialArenas.Equals(TEXT("All"), ESearchCase::IgnoreCase)) return true;
	TArray<FString> Parts;
	Tune.AerialArenas.ParseIntoArray(Parts, TEXT(","), true);
	const FString Key = ArenaKey(G->Arena->GetTheme());
	for (const FString& P : Parts) if (P.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase)) return true;
	return false;
}

void UZoogiBattleStateMachine::ClearMapLayout(const TCHAR* Why)
{
	ClearProps();
	if (Decor) Decor->Destroy();
	Decor = nullptr;
	bMapLayout = false;
	MapArena = nullptr;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE props map layout cleared (%s)"), Why);
}

void UZoogiBattleStateMachine::SpawnMapLayout()
{
	AZoogiGameMode* G = GM();
	if (!G || !G->Arena) return;
	AZoogiArena* A = G->Arena;
	ClearProps();
	bMapLayout = true;
	MapArena = A;
	MapTheme = (uint8)A->GetTheme();
	if (A->GetTheme() != EZoogiArena::Frozen)
	{
		SpawnDefaultProps(); // other listed arenas: the T3 debug layout
		return;
	}
	using namespace ZoogiFrozen;
	// Launch Rail (Zone 2 entry): along the E edge, released toward the W wall. EARLY = high arc through the hanging halo,
	// CENTER = flat shot across the court, LATE = bank toward the NW snow chute (+ the chute bumper).
	const FVector S = FzPol(1200.f, 35.f, 0.f);
	const FVector Wt = FzPol(WallR, 184.f, 0.f);
	const FVector F = FVector(Wt.X - S.X, Wt.Y - S.Y, 0.f).GetSafeNormal();
	if (AZoogiAerialProp* Rail = SpawnProp(EZoogiPropKind::LaunchRail, S, F, 60.f, false, TEXT("map_rail")))
	{
		Rail->RailMinSpeed = Tune.MapRailMinSpeed;
		// curved approach track hugging the edge (visual): cubic from (r1360, yaw 12) heading CCW into the rail entry
		const FVector P0 = FzPol(1360.f, 12.f, 0.f);
		const FVector T0(-FMath::Sin(FMath::DegreesToRadians(12.f)), FMath::Cos(FMath::DegreesToRadians(12.f)), 0.f);
		const FVector P3 = S - F * (Rail->Size + 12.f);
		const FVector P1 = P0 + T0 * 300.f, P2 = P3 - F * 300.f;
		for (int32 i = 0; i < 24; ++i)
		{
			const float T = i / 24.f, U = 1.f - T;
			Rail->VisualPath.Add(P0 * (U * U * U) + P1 * (3.f * U * U * T) + P2 * (3.f * U * T * T) + P3 * (T * T * T));
		}
		Rail->BuildVisuals(A);
	}
	// Zone 2 hanging Resonance Halo on the EARLY arc (apex ~z250 at 580 along the rail for MapRailMinSpeed 1100).
	// Oct 8 (Evan 9:26 PM): never above the measured rail_early envelope (ReachPeaks) minus ReachMargin.
	float HaloZ = Tune.AerialHaloZ;
	const float EnvEarly = ReachEnvelope(TEXT("rail_early"));
	if (EnvEarly > 0.f) HaloZ = FMath::Min(HaloZ, EnvEarly - Tune.ReachMargin);
	SpawnProp(EZoogiPropKind::ResonanceHalo, S + F * Tune.AerialHaloAlong + FVector(0, 0, HaloZ), F, 110.f, true, TEXT("aerial_halo"));
	// Zone 1 standing halos facing the W / E pads (radial pass axis) between the centre triangle and the orb ring
	SpawnProp(EZoogiPropKind::ResonanceHalo, FzPol(340.f, 180.f, 0.f), FVector(-1.f, 0.f, 0.f), 150.f, false, TEXT("halo_w"));
	SpawnProp(EZoogiPropKind::ResonanceHalo, FzPol(340.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 150.f, false, TEXT("halo_e"));
	// Sky Bumpers: a floor node between the pillar ring and the pads (SW), and the chute bumper past the NW ramp lip
	SpawnProp(EZoogiPropKind::SkyBumper, FzPol(830.f, 135.f, ZoogiConst::ZoogiRadius), FVector(1.f, 0.f, 0.f), Tune.SkyBumperRadius, false, TEXT("floor_bumper"));
	SpawnProp(EZoogiPropKind::SkyBumper, FzPol(1480.f, RampYaw, 110.f), FVector(1.f, 0.f, 0.f), Tune.SkyBumperRadius, false, TEXT("chute"));
	// Zone 3 recovery gates on the front bridges (red: yaw 140 islet, blue: yaw 36 islet), across the deck midpoint
	const float RA = A->GetKnockoffRadius() - 15.f;
	for (int32 k = 0; k < 4; ++k)
	{
		const bool bRed = FMath::IsNearlyEqual(IsletYaw[k], 140.f), bBlue = FMath::IsNearlyEqual(IsletYaw[k], 36.f);
		if (!bRed && !bBlue) continue;
		const float RB = IsletDist[k] - IsletR + 25.f;
		const FVector L = FzPol((RA + RB) * 0.5f, IsletYaw[k], IsletTopZ * 0.5f);
		SpawnProp(EZoogiPropKind::Gate, L, FVector(L.X, L.Y, 0.f).GetSafeNormal(), 130.f, false, bRed ? TEXT("red") : TEXT("blue"));
	}
	// set dressing (visual only)
	if (UWorld* W = GetWorld())
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Decor = W->SpawnActor<AZoogiAerialDecor>(AZoogiAerialDecor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
		TArray<AZoogiAerialProp*> Raw;
		for (AZoogiAerialProp* Pr : Props) Raw.Add(Pr);
		if (Decor) { Decor->MaxZ = Tune.ReachPeaks.IsEmpty() ? -1.f : ReachEnvelopeMax(); Decor->Build(A, Raw); }
	}
	FString List;
	for (const AZoogiAerialProp* Pr : Props) if (Pr) List += FString::Printf(TEXT("%s%s[%s]"), List.IsEmpty() ? TEXT("") : TEXT(" "), *Pr->Label(), *Pr->Tag.ToString());
	UE_LOG(LogZoogi, Log, TEXT("BATTLE props map layout Frozen spawned=%d decor=%d rail_min_speed=%.0f cpu_chance=%.2f: %s"), Props.Num(), Decor ? 1 : 0, Tune.MapRailMinSpeed, Tune.CPUPropChance, *List);
	LogReach(); // Oct 8: every placed prop vs the measured launch envelope
	// the first CPU turn was planned at turn start, before the layout existed: plan it again with the props
	if (TestCase.IsEmpty()) G->ReplanCPUShot();
}

void UZoogiBattleStateMachine::AerialShot(const FString& Name, float Delay)
{
	if (AerialShotDir.IsEmpty()) return;
	const double At = RealClock + FMath::Max(0.f, Delay);
	AerialLater.Add(TPair<FString, double>(Name, At));
}

void UZoogiBattleStateMachine::TickAerialShots()
{
	for (int32 i = AerialLater.Num() - 1; i >= 0; --i)
	{
		if (RealClock < AerialLater[i].Value) continue;
		const FString Name = AerialLater[i].Key;
		AerialLater.RemoveAt(i);
		int32& N = AerialShotCounts.FindOrAdd(Name);
		if (N >= 2) continue;
		++N;
		const FString Path = FPaths::Combine(AerialShotDir, FString::Printf(TEXT("aerial_a%02d_%s_%d.png"), FMath::Max(0, CurrentActionId), *Name, N));
		FScreenshotRequest::RequestScreenshot(Path, true, false); // engine viewport (+ HUD), never the desktop
		UE_LOG(LogZoogi, Log, TEXT("BATTLE aerial screenshot requested: %s"), *Path);
	}
}

EZoogiZone UZoogiBattleStateMachine::ZoneOf(const FVector& P) const
{
	if (P.Z > Tune.ZoneAerialZ) return EZoogiZone::Aerial;
	if (FVector2D(P.X, P.Y).Size() > Tune.ZoneRecoveryFrac * AZoogiArena::ActiveKnockoffR) return EZoogiZone::Recovery;
	return EZoogiZone::Court;
}

void UZoogiBattleStateMachine::OnBallHitProp(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AZoogiAerialProp* Prop = Cast<AZoogiAerialProp>(OtherActor);
	AZoogiBall* B = HitComp ? Cast<AZoogiBall>(HitComp->GetOwner()) : nullptr;
	if (Prop && B) Prop->OnSolidHit(OtherComp, B, HitComp, -NormalImpulse, Hit);
}

void UZoogiBattleStateMachine::OnPropEvent(AZoogiAerialProp* Prop, AZoogiBall* Ball, FName Event, const FVector& Where, float SpeedIn, float SpeedOut)
{
	UWorld* W = GetWorld();
	if (!Prop || !Ball || !W) return;
	const int32 Body = Ball->PlayerIndex;
	const float T = (float)(W->GetTimeSeconds() - ActionStartGame);
	const EZoogiZone Z = ZoneOf(Where);
	const TCHAR* ZN = Z == EZoogiZone::Aerial ? TEXT("aerial") : (Z == EZoogiZone::Recovery ? TEXT("recovery") : TEXT("court"));
	ActionZoneTags.AddUnique(ZN);
	if (Prop->Kind == EZoogiPropKind::Gate)
	{
		// Zone 3 recovery gate: tag + a small Episode Energy grant (no physics)
		++GatePasses;
		ActionZoneTags.AddUnique(TEXT("recovery"));
		ActionPropTags.Add(FString::Printf(TEXT("%s_%s:gate:P%d"), *Prop->Label(), *Prop->Tag.ToString(), Body + 1));
		if (bFlow) AddEnergy(Body, Tune.Gain(TEXT("RecoveryGate")), TEXT("RecoveryGate"), CurrentActionId);
		UE_LOG(LogZoogi, Log, TEXT("BATTLE prop event %s(%s) gate P%d t=%.2f zone=%s action=%d"), *Prop->Label(), *Prop->Tag.ToString(), Body + 1, T, ZN, CurrentActionId);
		AerialShot(TEXT("gate"), 0.05f);
		return;
	}
	if (Prop->Kind != EZoogiPropKind::LaunchRail) ActionZoneTags.AddUnique(TEXT("aerial")); // halos / bumpers are Zone 2 props
	ActionPropTags.Add(FString::Printf(TEXT("%s:%s:P%d"), *Prop->Label(), *Event.ToString(), Body + 1));
	if (Event == TEXT("pass")) ++HaloPasses;
	else if (Event == TEXT("rim")) { ++HaloRims; NotePeakCause(Body, TEXT("halo_rim")); }
	else if (Event == TEXT("bumper")) { ++BumperRebounds; BumperCheckBall = Ball; BumperCheckFrames = 2; NotePeakCause(Body, TEXT("bumper")); }
	if (Pending.Owner == Body && Pending.ActionId == CurrentActionId && CurrentActionId >= 0)
	{
		Pending.PropEvents.Add(FString::Printf(TEXT("%s:%s@%.2f"), *Prop->Label(), *Event.ToString(), T));
		if (Prop->Kind == EZoogiPropKind::ResonanceHalo && Pending.HaloAt < 0.f) { Pending.HaloAt = T; Pending.HaloEvent = Event.ToString(); }
		if (Prop->Kind == EZoogiPropKind::SkyBumper && Pending.BumperAt < 0.f) Pending.BumperAt = T;
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE prop event %s %s P%d t=%.2f zone=%s action=%d"), *Prop->Label(), *Event.ToString(), Body + 1, T, ZN, CurrentActionId);
	if (Event == TEXT("pass")) AerialShot(Prop->bAerialRing ? TEXT("aerial_halo_pass") : TEXT("halo_pass"), 0.04f);
	else if (Event == TEXT("bumper")) AerialShot(TEXT("bumper"), 0.06f);
	if (Event == TEXT("rail_enter")) StartRail(Prop, Ball);
}

void UZoogiBattleStateMachine::TickProps()
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W) return;
	// Oct 8: the map layout follows the arena / mode (spawned at the first aim of a match, cleared on arena change / combo)
	const bool bWantMap = WantsMapProps();
	if (bMapLayout && (!bWantMap || MapArena.Get() != G->Arena.Get() || (G->Arena && (uint8)G->Arena->GetTheme() != MapTheme))) ClearMapLayout(TEXT("arena_or_mode_changed"));
	if (bWantMap && !bMapLayout && G->Phase == EZoogiPhase::Aiming) SpawnMapLayout();
	Props.RemoveAll([](const TObjectPtr<AZoogiAerialProp>& P) { return P == nullptr; });
	if (Props.Num() == 0) return;
	// the Zoogies report prop hits too (one event per hit: the prop's cooldown)
	for (AZoogiBall* B : G->Balls)
		if (B && B->Sphere && !PropBoundBalls.Contains(B))
		{
			B->Sphere->OnComponentHit.AddDynamic(this, &UZoogiBattleStateMachine::OnBallHitProp);
			PropBoundBalls.Add(B);
		}
	if (!W->IsPaused())
	{
		// trigger volumes: halo pass = crossing the ring plane inside the opening; rail = entering the rail volume
		for (int32 p = 0; p < Props.Num(); ++p)
		{
			AZoogiAerialProp* A = Props[p];
			if (!A->Trigger) continue;
			const FTransform X = A->Trigger->GetComponentTransform();
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut) continue;
				const FVector L = X.InverseTransformPositionNoScale(B->BallLocation());
				const int64 Key = (int64)p * 64 + B->PlayerIndex;
				const FVector* Prev = PropPrevLocal.Find(Key);
				if (Prev)
				{
					if (A->Kind == EZoogiPropKind::ResonanceHalo || A->Kind == EZoogiPropKind::Gate)
					{
						const FVector E = A->Trigger->GetUnscaledBoxExtent();
						const bool bCross = (Prev->X < 0.f) != (L.X < 0.f);
						const FVector Mid = (*Prev + L) * 0.5f;
						if (bCross && FMath::Abs(Mid.Y) <= E.Y && FMath::Abs(Mid.Z) <= E.Z && A->Cooldown(B, 0.5f, 1))
							OnPropEvent(A, B, A->Kind == EZoogiPropKind::Gate ? TEXT("gate") : TEXT("pass"), B->BallLocation(), 0.f, 0.f);
					}
					else if (A->Kind == EZoogiPropKind::LaunchRail && !A->InTrigger(*Prev) && A->InTrigger(L) && A->Cooldown(B, 0.5f, 1))
						OnPropEvent(A, B, TEXT("rail_enter"), B->BallLocation(), 0.f, 0.f);
				}
				PropPrevLocal.Add(Key, L);
			}
		}
		if (BumperCheckFrames > 0 && --BumperCheckFrames == 0 && BumperCheckBall.IsValid())
		{
			const FVector V = BumperCheckBall->GetBallVelocity();
			BumperPostSpeed = FVector2D(V.X, V.Y).Size();
			UE_LOG(LogZoogi, Log, TEXT("BATTLE skybumper post P%d speed=%.0f"), BumperCheckBall->PlayerIndex + 1, BumperPostSpeed);
		}
		// zone layers for the current action (debug tags)
		if (G->Phase == EZoogiPhase::Rolling)
			for (const AZoogiBall* B : G->Balls)
				if (B && !B->bOut)
				{
					const EZoogiZone Z = ZoneOf(B->BallLocation());
					if (Z == EZoogiZone::Aerial) ActionZoneTags.AddUnique(TEXT("aerial"));
					else if (Z == EZoogiZone::Recovery) ActionZoneTags.AddUnique(TEXT("recovery"));
				}
	}
	if (bFlow && Tune.bDebugOverlay) DrawZones();
}

void UZoogiBattleStateMachine::DrawZones() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* W = GetWorld();
	if (!W) return;
	const float R = AZoogiArena::ActiveKnockoffR;
	// Zone 3 recovery ring (outer band), Zone 2 aerial layer (a ring at the aerial height), Zone 1 court is the floor
	DrawDebugCircle(W, FVector(0, 0, 4.f), R * Tune.ZoneRecoveryFrac, 96, FColor(80, 140, 255), false, -1.f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	DrawDebugCircle(W, FVector(0, 0, 4.f), R, 96, FColor(255, 60, 60), false, -1.f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	DrawDebugCircle(W, FVector(0, 0, Tune.ZoneAerialZ), R * 0.8f, 64, FColor(120, 255, 255, 120), false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	for (const AZoogiAerialProp* A : Props) if (A) A->DrawDebug();
	if (const UZoogiMiniGameRailRelease* M = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame))
	{
		const AZoogiGameMode* G = GM();
		const AZoogiBall* B = G && G->Balls.IsValidIndex(M->Body) ? G->Balls[M->Body].Get() : nullptr;
		if (B)
		{
			const EZoogiRailWindow Wn = UZoogiMiniGameRailRelease::WindowAt(Tune, M->Elapsed);
			const FColor C = Wn == EZoogiRailWindow::Early ? FColor::Yellow : (Wn == EZoogiRailWindow::Center ? FColor::Green : FColor::Red);
			const float Speed = FMath::Max(M->EntrySpeed * Tune.RailSpeedMul, (M->Rail.IsValid() && M->Rail->RailMinSpeed > 0.f) ? M->Rail->RailMinSpeed : Tune.RailMinSpeed);
			for (EZoogiRailWindow Opt : { EZoogiRailWindow::Early, EZoogiRailWindow::Center, EZoogiRailWindow::Late })
			{
				const FVector V = UZoogiMiniGameRailRelease::LaunchVelocity(Tune, Opt, M->RailForward, B->BallLocation(), Speed);
				DrawDebugDirectionalArrow(W, B->BallLocation(), B->BallLocation() + V * 0.25f, 30.f, Opt == Wn ? C : FColor(90, 90, 90), false, -1.f, 0, Opt == Wn ? 4.f : 1.5f);
			}
		}
	}
#endif
}

// ------------------------------------------------------------------------------------------------- CPU prop planning
// Oct 8: with the map layout up, a CPU sometimes (GameRng < CPUPropChance) plays a prop route instead of its normal shot:
// Launch Rail (picks a release window), a floor Resonance Halo bank onto a target behind it, or a Sky Bumper rebound.
// Only reached when bMapLayout (so the off runs / tests without the layout draw no extra GameRng numbers).

namespace
{
	float PropInvertLaunch(float Speed)
	{
		float Lo = 0.f, Hi = 1.f;
		for (int32 i = 0; i < 30; ++i)
		{
			const float Mid = 0.5f * (Lo + Hi);
			if (ZoogiConst::LaunchSpeedForPull(Mid) < Speed) Lo = Mid; else Hi = Mid;
		}
		return 0.5f * (Lo + Hi);
	}
	float SegPointDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D D = B - A;
		const float L2 = D.SizeSquared();
		const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - A, D) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + D * T);
	}
}

bool UZoogiBattleStateMachine::ComputePropPlan(int32 Player, EZoogiPropKind Kind, FVector2D& OutDir, float& OutFrac, AActor*& OutTarget, AZoogiAerialProp*& OutProp, FString& OutDesc) const
{
	const AZoogiGameMode* G = GM();
	if (!G || !G->Arena || !G->Balls.IsValidIndex(Player) || !G->Balls[Player]) return false;
	const AZoogiBall* Me = G->Balls[Player];
	const FVector2D B(Me->BallLocation());
	const float Decel = G->Arena->GetRollingDecel();
	const float BallR = ZoogiConst::ZoogiRadius;
	// clear lane test (orbs, Zoogis, rocks, small breakable pieces, solid props), ignoring the listed actors
	auto Clear = [&](const FVector2D& From, const FVector2D& To, const AActor* IgnoreA, const AActor* IgnoreB) -> bool
	{
		auto Hit = [&](const FVector2D& Q, float R) { return SegPointDist(Q, From, To) < R + BallR + 8.f; };
		for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O != IgnoreA && O != IgnoreB && Hit(FVector2D(O->OrbLocation()), ZoogiConst::OrbRadius)) return false;
		for (const AZoogiBall* O : G->Balls) if (O && O != Me && !O->bOut && O != IgnoreA && O != IgnoreB && Hit(FVector2D(O->BallLocation()), BallR)) return false;
		for (const AZoogiRock* Rk : G->Arena->GetRocks()) if (Rk && Rk->IsStanding() && Hit(FVector2D(Rk->GetActorLocation()), Rk->GetRadius())) return false;
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
		{
			if (!Pc || Pc->IsBroken()) continue;
			FVector C, Ext;
			Pc->GetActorBounds(true, C, Ext);
			const float PR = FVector2D(Ext.X, Ext.Y).Size();
			if (PR < 1.f || PR > 450.f) continue; // big floor slabs / walls are not lane blockers
			if (C.Z - Ext.Z > 140.f) continue;     // overhead
			if (Hit(FVector2D(C), PR * 0.7f)) return false;
		}
		for (const AZoogiAerialProp* P : Props)
		{
			if (!P || P == IgnoreA || P == IgnoreB) continue;
			const FVector L = P->GetActorLocation();
			if (P->Kind == EZoogiPropKind::SkyBumper && L.Z - P->Size < 100.f && Hit(FVector2D(L), P->Size)) return false;
			if (P->Kind == EZoogiPropKind::ResonanceHalo && !P->bAerialRing)
			{
				const FVector Side = FVector::CrossProduct(FVector::UpVector, P->Forward).GetSafeNormal();
				if (Hit(FVector2D(L + Side * P->Size * 0.64f), 20.f) || Hit(FVector2D(L - Side * P->Size * 0.64f), 20.f)) return false;
			}
		}
		return true;
	};
	auto InCourt = [&](const FVector2D& P) { return P.Size() < G->Arena->GetKnockoffRadius() - 140.f; };
	auto Targets = [&](TArray<AActor*>& Out)
	{
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Out.Add(O);
		for (AZoogiBall* O : G->Balls) if (O && O != Me && !O->bOut && !G->IsAlly(Me, O)) Out.Add(O);
	};
	auto Pos = [](const AActor* A) { const AZoogiOrb* O = Cast<AZoogiOrb>(A); return FVector2D(O ? O->OrbLocation() : Cast<AZoogiBall>(A)->BallLocation()); };
	float BestScore = -1.f;
	for (AZoogiAerialProp* P : Props)
	{
		if (!P || P->Kind != Kind) continue;
		const FVector2D C(P->GetActorLocation());
		if (Kind == EZoogiPropKind::LaunchRail)
		{
			// roll into the rail entry slowly (a miss must not carry the Zoogi off the edge)
			const float D = FVector2D::Distance(B, C);
			if (D < 250.f || !Clear(B, C, P, nullptr)) continue;
			const float V0 = FMath::Sqrt(260.f * 260.f + 2.f * Decel * D);
			const float Frac = PropInvertLaunch(V0);
			if (Frac > 0.98f || Frac < 0.15f) continue;
			const float Score = 2000.f - D;
			if (Score <= BestScore) continue;
			BestScore = Score; OutDir = (C - B).GetSafeNormal(); OutFrac = Frac; OutTarget = nullptr; OutProp = P;
			OutDesc = FString::Printf(TEXT("rail %s dist=%.0f"), *P->Label(), D);
		}
		else if (Kind == EZoogiPropKind::ResonanceHalo)
		{
			if (P->bAerialRing) continue; // the hanging halo is reached from the rail's EARLY window
			const FVector2D N(P->Forward.X, P->Forward.Y);
			const FVector2D Side(-N.Y, N.X);
			TArray<AActor*> Ts;
			Targets(Ts);
			for (AActor* T : Ts)
			{
				const FVector2D TP = Pos(T);
				const FVector2D Dv = TP - B;
				const float Dist = Dv.Size();
				if (Dist < 300.f) continue;
				const FVector2D Dn = Dv / Dist;
				const float DN = FVector2D::DotProduct(Dn, N);
				if (FMath::Abs(DN) < 0.82f) continue;            // through the opening, roughly along the halo axis
				const float S = FVector2D::DotProduct(C - B, N) / DN; // distance along the shot to the halo plane
				if (S < 120.f || S > Dist - 110.f) continue;      // the halo between the shooter and the target
				const float Lat = FMath::Abs(FVector2D::DotProduct(B + Dn * S - C, Side));
				if (Lat > 22.f) continue;
				if (!Clear(B, TP, T, P)) continue;
				const float V0 = FMath::Sqrt(450.f * 450.f + 2.f * Decel * Dist);
				const float Frac = PropInvertLaunch(V0);
				if (Frac > 0.97f) continue;
				const float Score = 1000.f - Lat * 10.f - Dist * 0.2f + (Cast<AZoogiBall>(T) ? 60.f : 0.f);
				if (Score <= BestScore) continue;
				BestScore = Score; OutDir = Dn; OutFrac = Frac; OutTarget = T; OutProp = P;
				OutDesc = FString::Printf(TEXT("halo %s -> %s lat=%.0f dist=%.0f"), *P->Label(), *T->GetName(), Lat, Dist);
			}
		}
		else if (Kind == EZoogiPropKind::SkyBumper)
		{
			if (P->GetActorLocation().Z - P->Size > 100.f) continue;
			TArray<AActor*> Ts;
			Targets(Ts);
			const float RR = P->Size + BallR;
			for (AActor* T : Ts)
			{
				const FVector2D TP = Pos(T);
				// mirror bounce: contact normal n bisects (out - in); contact point Q = C + n * (R + r)
				FVector2D N = (B - C).GetSafeNormal() + (TP - C).GetSafeNormal();
				N = N.GetSafeNormal();
				if (N.IsNearlyZero()) continue;
				FVector2D Q = C + N * RR, Din, Dout;
				bool bOk = false;
				for (int32 It = 0; It < 12; ++It)
				{
					Din = (Q - B).GetSafeNormal();
					Dout = (TP - Q).GetSafeNormal();
					const FVector2D NN = (Dout - Din).GetSafeNormal();
					if (NN.IsNearlyZero()) break;
					Q = C + NN * RR;
					N = NN;
					bOk = true;
				}
				if (!bOk) continue;
				Din = (Q - B).GetSafeNormal();
				if (FVector2D::DotProduct(Din, N) > -0.25f) continue; // a real bounce, not a graze
				const float D1 = FVector2D::Distance(B, Q), D2 = FVector2D::Distance(Q, TP);
				if (D1 < 150.f || D2 < 120.f || D2 > 1300.f || !InCourt(Q)) continue;
				if (!Clear(B, Q, P, nullptr) || !Clear(Q, TP, T, P)) continue;
				// aim at the ball centre position that touches the bumper at Q (centre of the Zoogi = Q)
				const float V0 = FMath::Sqrt(500.f * 500.f + 2.f * Decel * D1);
				const float Frac = PropInvertLaunch(V0);
				if (Frac > 0.97f) continue;
				const float Score = 1000.f + FVector2D::DotProduct(-Din, N) * 100.f - D1 * 0.2f - D2 * 0.2f + (Cast<AZoogiBall>(T) ? 60.f : 0.f);
				if (Score <= BestScore) continue;
				BestScore = Score; OutDir = Din; OutFrac = Frac; OutTarget = T; OutProp = P;
				OutDesc = FString::Printf(TEXT("bumper %s -> %s in=%.0f out=%.0f"), *P->Label(), *T->GetName(), D1, D2);
			}
		}
	}
	return BestScore > -1.f;
}

bool UZoogiBattleStateMachine::PlanCPUProp(int32 Player, FVector2D& Dir, float& Frac, TWeakObjectPtr<AActor>& Lock)
{
	AZoogiGameMode* G = GM();
	if (!G || !bFlow || !bMapLayout || Props.Num() == 0) return false;
	if (!G->Balls.IsValidIndex(Player) || !G->Balls[Player] || !G->Balls[Player]->bCPU) return false;
	++CPUPropRolls;
	const float Roll = G->GameRng.FRand();
	if (Roll >= Tune.CPUPropChance) return false;
	struct FOpt { EZoogiPropKind Kind; FName Combo; FVector2D Dir; float Frac; AActor* Target; AZoogiAerialProp* Prop; FString Desc; };
	TArray<FOpt> Opts;
	const EZoogiPropKind Kinds[] = { EZoogiPropKind::LaunchRail, EZoogiPropKind::ResonanceHalo, EZoogiPropKind::SkyBumper };
	const TCHAR* Combos[] = { TEXT("rail_release"), TEXT("halo_bank"), TEXT("sky_bumper") };
	for (int32 k = 0; k < 3; ++k)
	{
		if (TestForceProp >= 0 && (int32)Kinds[k] != TestForceProp) continue; // map_* test cases
		FOpt O{ Kinds[k], FName(Combos[k]), FVector2D::ZeroVector, 0.f, nullptr, nullptr, FString() };
		if (ComputePropPlan(Player, O.Kind, O.Dir, O.Frac, O.Target, O.Prop, O.Desc)) Opts.Add(O);
	}
	if (Opts.Num() == 0)
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE cpu prop plan P%d roll=%.2f no feasible prop route"), Player + 1, Roll);
		return false;
	}
	const FOpt& O = Opts[G->GameRng.RandRange(0, Opts.Num() - 1)];
	const float Err = G->GameRng.FRandRange(-1.f, 1.f) * (100.f - G->Balls[Player]->Def.Control) / FMath::Max(1.f, Tune.CPUPropAimErrDiv);
	EZoogiRailWindow Wn = EZoogiRailWindow::Center;
	if (O.Kind == EZoogiPropKind::LaunchRail)
	{
		const float R = G->GameRng.FRand();
		Wn = R < 0.4f ? EZoogiRailWindow::Early : (R < 0.75f ? EZoogiRailWindow::Center : EZoogiRailWindow::Late);
		if (TestForceWindow != EZoogiRailWindow::None) Wn = TestForceWindow;
	}
	Dir = O.Dir.GetRotated(Err);
	Frac = FMath::Clamp(O.Frac, 0.15f, 1.f);
	Lock = O.Target; // rail: no lock
	CPUPlan = FZoogiCPUPropPlan();
	CPUPlan.Player = Player;
	CPUPlan.Turn = G->TurnCounter;
	CPUPlan.Kind = O.Kind;
	CPUPlan.Combo = O.Combo;
	CPUPlan.Prop = O.Prop;
	CPUPlan.Target = O.Target;
	CPUPlan.RailWindow = Wn;
	CPUPlan.Desc = O.Desc;
	++CPUPropPlans;
	CPUPlanKinds.FindOrAdd(PropKindName(O.Kind))++;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE cpu prop plan P%d turn=%d %s combo=%s window=%s frac=%.2f err=%+.1f options=%d"), Player + 1, CPUPlan.Turn, *O.Desc, *O.Combo.ToString(),
		O.Kind == EZoogiPropKind::LaunchRail ? ZoogiBattle::RailWindowName(Wn) : TEXT("-"), Frac, Err, Opts.Num());
	return true;
}
