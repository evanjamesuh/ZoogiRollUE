// Battle Episode Engine T3: the state machine side of the Frozen Ring aerial props (Sky Bumper, Resonance Halo, Launch
// Rail), the Rail Release timing mini-game and the debug zone layers (refs frozen_zones_aerial, rail_release_timing,
// sky_bumper_detail, resonance_halo_detail). Debug geometry only. RULE ZERO: real physics velocity changes in the normal
// simulation (game time, no holds, no time dilation), only when props exist (bAerialProps / -ZoogiBattleProps / a test).
#include "ZoogiBattleStateMachine.h"
#include "ZoogiAerialProps.h"
#include "ZoogiMiniGame.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

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
	if (bCPU && !bReleaseRequested && Elapsed >= T.RailEarlyWindow + T.RailCenterWindow * 0.5f) SubmitInput(SM, 0, TEXT("cpu")); // deterministic: center
	if (bReleaseRequested) return EZoogiChallengeResult::Success;
	return Elapsed >= T.RailEarlyWindow + T.RailCenterWindow + T.RailLateWindow ? EZoogiChallengeResult::Timeout : EZoogiChallengeResult::Pending;
}

FString UZoogiMiniGameRailRelease::Apply(UZoogiBattleStateMachine* SM) const
{
	const AZoogiGameMode* G = SM->GetGame();
	AZoogiBall* B = G && G->Balls.IsValidIndex(Body) ? G->Balls[Body].Get() : nullptr;
	if (!B || !B->Sphere) return TEXT("no body");
	const FZoogiBattleTuning& T = SM->GetTuning();
	const float Speed = FMath::Max(EntrySpeed * T.RailSpeedMul, T.RailMinSpeed);
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
	M->bSegment = Pending.Owner == Body && Pending.Challenge == EZoogiChallenge::RailRelease && Pending.ActionId == CurrentActionId;
	M->Enter(this, Body, Pending);
	M->bInputOpen = true;
	// the rail guides the Zoogi: velocity along the rail at its entry speed (horizontal)
	const FVector V = Ball->Sphere ? Ball->Sphere->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	M->EntrySpeed = FVector2D(V.X, V.Y).Size();
	if (Ball->Sphere) Ball->Sphere->SetPhysicsLinearVelocity(FVector(FVector2D(Rail->Forward.X, Rail->Forward.Y).GetSafeNormal() * M->EntrySpeed, V.Z));
	LastRailEntrySpeed = M->EntrySpeed;
	ActiveMiniGame = M;
	++RailEntries;
	Transition(EZoogiBattleState::MiniGame, FString::Printf(TEXT("rail_release P%d"), Body + 1));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame RailRelease enter P%d rail=%s speed=%.0f windows=%.2f/%.2f/%.2f cpu=%d segment=%d"), Body + 1, *Rail->Label(), M->EntrySpeed,
		Tune.RailEarlyWindow, Tune.RailCenterWindow, Tune.RailLateWindow, M->bCPU ? 1 : 0, M->bSegment ? 1 : 0);
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
		ActionPropTags.Add(FString::Printf(TEXT("%s:release_%s"), M->Rail.IsValid() ? *M->Rail->Label() : TEXT("rail"), ZoogiBattle::RailWindowName(M->Window)));
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

AZoogiAerialProp* UZoogiBattleStateMachine::SpawnProp(EZoogiPropKind Kind, const FVector& Location, const FVector& Forward, float Size)
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
	A->Build(Kind, Props.Num() + 1, Location, Forward, Size);
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
	if (Prop->Kind != EZoogiPropKind::LaunchRail) ActionZoneTags.AddUnique(TEXT("aerial")); // halos / bumpers are Zone 2 props
	ActionPropTags.Add(FString::Printf(TEXT("%s:%s:P%d"), *Prop->Label(), *Event.ToString(), Body + 1));
	if (Event == TEXT("pass")) ++HaloPasses;
	else if (Event == TEXT("rim")) ++HaloRims;
	else if (Event == TEXT("bumper")) { ++BumperRebounds; BumperCheckBall = Ball; BumperCheckFrames = 2; }
	if (Pending.Owner == Body && Pending.ActionId == CurrentActionId && CurrentActionId >= 0)
	{
		Pending.PropEvents.Add(FString::Printf(TEXT("%s:%s@%.2f"), *Prop->Label(), *Event.ToString(), T));
		if (Prop->Kind == EZoogiPropKind::ResonanceHalo && Pending.HaloAt < 0.f) { Pending.HaloAt = T; Pending.HaloEvent = Event.ToString(); }
		if (Prop->Kind == EZoogiPropKind::SkyBumper && Pending.BumperAt < 0.f) Pending.BumperAt = T;
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE prop event %s %s P%d t=%.2f zone=%s action=%d"), *Prop->Label(), *Event.ToString(), Body + 1, T, ZN, CurrentActionId);
	if (Event == TEXT("rail_enter")) StartRail(Prop, Ball);
}

void UZoogiBattleStateMachine::TickProps()
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W) return;
	if (bFlow && Tune.bAerialProps && !bDefaultPropsSpawned && TestCase.IsEmpty() && G->Phase == EZoogiPhase::Aiming && G->Arena) SpawnDefaultProps();
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
					if (A->Kind == EZoogiPropKind::ResonanceHalo)
					{
						const FVector E = A->Trigger->GetUnscaledBoxExtent();
						const bool bCross = (Prev->X < 0.f) != (L.X < 0.f);
						const FVector Mid = (*Prev + L) * 0.5f;
						if (bCross && FMath::Abs(Mid.Y) <= E.Y && FMath::Abs(Mid.Z) <= E.Z && A->Cooldown(B, 0.5f, 1))
							OnPropEvent(A, B, TEXT("pass"), B->BallLocation(), 0.f, 0.f);
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
			const float Speed = FMath::Max(M->EntrySpeed * Tune.RailSpeedMul, Tune.RailMinSpeed);
			for (EZoogiRailWindow Opt : { EZoogiRailWindow::Early, EZoogiRailWindow::Center, EZoogiRailWindow::Late })
			{
				const FVector V = UZoogiMiniGameRailRelease::LaunchVelocity(Tune, Opt, M->RailForward, B->BallLocation(), Speed);
				DrawDebugDirectionalArrow(W, B->BallLocation(), B->BallLocation() + V * 0.25f, 30.f, Opt == Wn ? C : FColor(90, 90, 90), false, -1.f, 0, Opt == Wn ? 4.f : 1.5f);
			}
		}
	}
#endif
}
