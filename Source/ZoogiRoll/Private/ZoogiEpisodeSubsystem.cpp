#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeCalloutWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiRock.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "CollisionQueryParams.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

// =====================================================================================================================
// lifecycle

bool UZoogiEpisodeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UZoogiEpisodeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Tune.Load();
	FString M;
	if (GConfig) GConfig->GetString(TEXT("ZoogiRoll.Episode"), TEXT("Mode"), M, GGameIni);
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Value(Cmd, TEXT("ZoogiEpisode="), M)) bModeExplicit = true;
	if (M.Equals(TEXT("Off"), ESearchCase::IgnoreCase)) Mode = EZoogiEpisodeMode::Off;
	else if (M.Equals(TEXT("Competitive"), ESearchCase::IgnoreCase)) Mode = EZoogiEpisodeMode::Competitive;
	else if (M.Equals(TEXT("Full"), ESearchCase::IgnoreCase)) Mode = EZoogiEpisodeMode::Full;
	else Mode = EZoogiEpisodeMode::Balanced;
	FParse::Value(Cmd, TEXT("ZoogiEpisodeTest="), TestVariant);
	TestVariant = TestVariant.ToUpper();
	if (TestVariant == TEXT("F") && !bModeExplicit) Mode = EZoogiEpisodeMode::Competitive; // F: same shot, Competitive (short)
	FParse::Value(Cmd, TEXT("ZoogiEpisodeShots="), ShotDir);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE engine: mode=%s%s%s"), ZoogiEpisode::ModeName(Mode), TestVariant.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" test=%s"), *TestVariant),
		ShotDir.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" shots=%s"), *ShotDir));
}

void UZoogiEpisodeSubsystem::Deinitialize()
{
	if (bFrozen) UnfreezeWorld();
	if (Fallback) { Fallback->RemoveFromParent(); Fallback = nullptr; }
	Super::Deinitialize();
}

TStatId UZoogiEpisodeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiEpisodeSubsystem, STATGROUP_Tickables);
}

AZoogiGameMode* UZoogiEpisodeSubsystem::GM() const
{
	const UWorld* W = GetWorld();
	return W ? Cast<AZoogiGameMode>(W->GetAuthGameMode()) : nullptr;
}

void UZoogiEpisodeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W || W->bIsTearingDown) return;
	AZoogiGameMode* G = GM();
	if (!G) return;
	const float RealDt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);
	RealClock += RealDt;
	BindBodies();
	if (!TestVariant.IsEmpty()) TickTest();
	const bool bPaused = W->IsPaused();
	if (bActive && !bPaused)
	{
		TrackAction(W->GetDeltaSeconds());
		const bool bResolve = G->Stage == EZoogiStage::Resolve || G->Phase == EZoogiPhase::Resolving;
		if (bResolve && !bSawResolve)
		{
			bSawResolve = true;
			UpdateLive();
			ShowEpilogue(); // outcome confirmed by the game (Resolve committed scores / KOs)
		}
		if ((bSawResolve && G->Stage != EZoogiStage::Resolve) || G->Phase == EZoogiPhase::GameOver || G->Phase == EZoogiPhase::Menu)
			FinalizeAction(G->Phase == EZoogiPhase::GameOver ? TEXT("game_over") : TEXT("resolved"));
	}
	if (Mode != EZoogiEpisodeMode::Off && G->Phase != EZoogiPhase::Menu) EnsureFallback();
	TickPresentation(RealDt);
}

// =====================================================================================================================
// bodies

void UZoogiEpisodeSubsystem::BindBodies()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	for (AZoogiBall* B : G->Balls)
		if (B && B->Sphere && !Bound.Contains(B->Sphere)) { B->Sphere->OnComponentHit.AddUniqueDynamic(this, &UZoogiEpisodeSubsystem::OnBodyHit); Bound.Add(B->Sphere); }
	for (AZoogiOrb* O : G->Orbs)
		if (O && O->Sphere && !Bound.Contains(O->Sphere)) { O->Sphere->OnComponentHit.AddUniqueDynamic(this, &UZoogiEpisodeSubsystem::OnBodyHit); Bound.Add(O->Sphere); }
}

int32 UZoogiEpisodeSubsystem::BodyCode(const AActor* A) const
{
	const AZoogiGameMode* G = GM();
	if (!G || !A) return -1;
	for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] == A) return i;
	for (int32 i = 0; i < G->Orbs.Num(); ++i) if (G->Orbs[i] == A) return 100 + i;
	return -1;
}

AActor* UZoogiEpisodeSubsystem::BodyActor(int32 Code) const
{
	const AZoogiGameMode* G = GM();
	if (!G || Code < 0) return nullptr;
	if (Code < 100) return G->Balls.IsValidIndex(Code) ? G->Balls[Code].Get() : nullptr;
	return G->Orbs.IsValidIndex(Code - 100) ? G->Orbs[Code - 100].Get() : nullptr;
}

FVector UZoogiEpisodeSubsystem::BodyVelocity(int32 Code) const
{
	if (AZoogiBall* B = Cast<AZoogiBall>(BodyActor(Code))) return B->GetBallVelocity();
	if (AZoogiOrb* O = Cast<AZoogiOrb>(BodyActor(Code))) return O->Sphere ? O->Sphere->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	return FVector::ZeroVector;
}

int32 UZoogiEpisodeSubsystem::PieceSteps() const
{
	const AZoogiGameMode* G = GM();
	int32 N = 0;
	if (G && G->Arena) for (const AZoogiArenaPiece* P : G->Arena->GetPieces()) if (P) N += (int32)P->GetPieceState();
	return N;
}

int32 UZoogiEpisodeSubsystem::RockSteps() const
{
	const AZoogiGameMode* G = GM();
	int32 N = 0;
	if (G && G->Arena) for (const AZoogiRock* R : G->Arena->GetRocks()) if (R) N += (int32)R->GetState();
	return N;
}

// =====================================================================================================================
// capture

void UZoogiEpisodeSubsystem::OnShotFired(AZoogiBall* Ball)
{
	if (!Ball) return;
	BeginAction(Ball);
}

void UZoogiEpisodeSubsystem::SnapshotStart()
{
	AZoogiGameMode* G = GM();
	StartScore.Reset(); StartMarks.Reset(); StartOut.Reset(); StartOrbOut.Reset();
	for (AZoogiBall* B : G->Balls) { StartScore.Add(B ? B->Score : 0); StartMarks.Add(B ? B->RingMarks : 0); StartOut.Add(B ? B->bOut : true); }
	for (AZoogiOrb* O : G->Orbs) StartOrbOut.Add(O ? O->bOut : true);
	const int32 Team = FMath::Clamp(Truth.ActorTeam, 0, 1);
	StartResonance = G->bComboMode ? G->ComboResonance[Team] : 0;
	StartControl = G->bComboMode ? G->ArenaControl[Team] : 0.f;
	StartPieceSteps = PieceSteps();
	StartRockSteps = RockSteps();
	OutCounted.Reset();
}

void UZoogiEpisodeSubsystem::BeginAction(AZoogiBall* Ball)
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W) return;
	if (bActive) FinalizeAction(TEXT("next_shot"));
	Truth = FZoogiGameplayTruthRecord();
	Plan = FZoogiCinematicPlan();
	LiveRec = FZoogiEpisodeRecognitionRecord();
	Truth.ActionId = NextActionId++;
	Truth.Round = G->RoundNumber;
	Truth.Turn = G->TurnCounter;
	Truth.bCombo = G->bComboMode;
	Truth.Actor = BodyCode(Ball);
	Truth.ActorName = Ball->Def.Name;
	Truth.ActorTeam = G->bComboMode ? AZoogiGameMode::TeamOfIndex(Truth.Actor) : Truth.Actor;
	Truth.LaunchStart = Ball->BallLocation();
	Truth.LaunchVelocity = Ball->GetBallVelocity();
	Truth.LaunchPower = FVector2D(Truth.LaunchVelocity).Size() / ZoogiConst::FullLaunchSpeed;
	Truth.Awaken = Ball->Artifact ? Ball->Artifact->Infused : EZoogiArtifact::None;
	Truth.bFollowUp = G->bComboMode && G->bInFollowUp;
	Plan.ActionId = Truth.ActionId;
	Plan.Mode = Mode;
	SnapshotStart();
	bActive = true;
	bSawResolve = false;
	ActionStartGame = W->GetTimeSeconds();
	DepthMap.Reset();
	DepthMap.Add(Truth.Actor, 0);
	PairLast.Reset();
	Air.Reset();
	OpenBoundary.Reset();
	ShooterFirstContact = -1;
	bPrevInfusedContact = G->bInfusedContact;
	PresentedTier = 0;
	Announced.Reset();
	CalloutQueue.Reset();
	bEpilogueShown = false;
	bPowerSent = false;
	bArchetypePending = false;
	if (const AWorldSettings* WS = W->GetWorldSettings()) { StartCinematicDilation = WS->CinematicTimeDilation; StartGlobalDilation = WS->TimeDilation; }
	PrevVel.Reset();
	for (int32 i = 0; i < G->Balls.Num(); ++i) PrevVel.Add(i, BodyVelocity(i));
	for (int32 i = 0; i < G->Orbs.Num(); ++i) PrevVel.Add(100 + i, BodyVelocity(100 + i));
	UE_LOG(LogZoogi, Verbose, TEXT("EPISODE begin action=%d P%d %s power=%.2f awaken=%s"), Truth.ActionId, Truth.Actor + 1, *Truth.ActorName, Truth.LaunchPower, *ZoogiArtifacts::Name(Truth.Awaken));
}

void UZoogiEpisodeSubsystem::OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = GM();
	if (!bActive || !W || !G || !HitComp || W->IsPaused()) return;
	const int32 Self = BodyCode(HitComp->GetOwner());
	if (Self < 0) return;
	const int32 Other = BodyCode(OtherActor);
	const FVector N = Hit.ImpactNormal;
	const float Now = W->GetTimeSeconds() - ActionStartGame;

	EZoogiContactKind Kind = EZoogiContactKind::Wall;
	int32 Inst = Self, Victim = Other;
	if (Other < 0)
	{
		if (OtherActor && OtherActor->IsA<AZoogiArenaPiece>()) Kind = EZoogiContactKind::Piece;
		else if (OtherActor && OtherActor->IsA<AZoogiRock>()) Kind = EZoogiContactKind::Rock;
		const bool bBumper = OtherComp && OtherComp->ComponentHasTag(TEXT("ZBumper"));
		if (bBumper) Kind = EZoogiContactKind::Bumper;
		else if (OtherComp && OtherComp->ComponentHasTag(TEXT("ZRamp"))) Kind = EZoogiContactKind::Ramp;
		if (!bBumper && N.Z > 0.7f) return; // rolling on a floor / platform top: not an event (landings come from air tracking)
		if (Kind == EZoogiContactKind::Wall && N.Z > 0.3f) Kind = EZoogiContactKind::Ramp;
	}
	else
	{
		// the body already in the causal chain (lower depth) is the instigator
		const int32* DS = DepthMap.Find(Self);
		const int32* DO = DepthMap.Find(Other);
		if (DO && (!DS || *DO < *DS)) { Inst = Other; Victim = Self; }
		AActor* IA = BodyActor(Inst);
		AActor* VA = BodyActor(Victim);
		Kind = Victim >= 100 ? EZoogiContactKind::Orb : ((Inst < 100 && G->IsAlly(IA, VA)) ? EZoogiContactKind::Ally : EZoogiContactKind::Zoogi);
	}

	// dedupe: both bodies report a body-body contact, and resting / sliding contacts repeat every substep
	const uint64 Key = Other >= 0 ? ((uint64)FMath::Min(Self, Other) << 32) | (uint64)(uint32)FMath::Max(Self, Other)
		: ((uint64)(uint32)Self << 32) ^ (uint64)(UPTRINT)OtherComp;
	if (float* Last = PairLast.Find(Key))
	{
		const bool bRepeat = Now - *Last < Tune.DedupeSeconds;
		*Last = Now;
		if (bRepeat) return;
	}
	else PairLast.Add(Key, Now);

	const FVector VS = PrevVel.Contains(Inst) ? PrevVel[Inst] : BodyVelocity(Inst);
	const FVector VO = Victim >= 0 ? (PrevVel.Contains(Victim) ? PrevVel[Victim] : BodyVelocity(Victim)) : FVector::ZeroVector;
	const float Rel = FMath::Abs(FVector::DotProduct(VS - VO, N));
	if (Rel < 0.4f * Tune.MeaningfulSpeed) return; // touches and nudges are not contacts (no reward for collision spam)

	const int32 InstDepth = DepthMap.Contains(Inst) ? DepthMap[Inst] : 1; // pushed by a power / indirect: depth 1
	if (Victim >= 0 && (!DepthMap.Contains(Victim) || DepthMap[Victim] > InstDepth + 1)) DepthMap.Add(Victim, InstDepth + 1);
	if (Inst == Truth.Actor && ShooterFirstContact < 0 && (Kind == EZoogiContactKind::Zoogi || Kind == EZoogiContactKind::Orb)) ShooterFirstContact = Victim;

	if (Truth.Contacts.Num() >= 256) return;
	FZoogiContactEvent E;
	E.Time = Now;
	E.Body = Inst;
	E.OtherBody = Victim;
	E.Kind = Kind;
	E.OtherName = OtherActor ? FName(*OtherActor->GetName()) : NAME_None;
	E.Location = Hit.ImpactPoint;
	E.Normal = N;
	E.RelSpeed = Rel;
	E.Impulse = NormalImpulse.Size();
	E.Depth = InstDepth;
	E.Weight = FMath::Clamp(Rel / Tune.FullWeightSpeed, 0.f, 1.5f);
	E.bMeaningful = Rel >= Tune.MeaningfulSpeed;
	E.Surface = Other < 0 ? OtherComp : nullptr;
	Truth.Contacts.Add(E);
	Truth.MaxRelSpeed = FMath::Max(Truth.MaxRelSpeed, Rel);
}

void UZoogiEpisodeSubsystem::TrackAction(float GameDt)
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds() - ActionStartGame;
	Truth.Duration = Now;

	// Awaken power: the infused first contact (ApplyInfuseImpact) flips bInfusedContact.
	if (G->bInfusedContact && !bPrevInfusedContact)
	{
		Truth.bPowerHit = true;
		Truth.bCrit = G->bLastHitCrit;
		Truth.PowerHitTarget = ShooterFirstContact;
		Truth.PowerHitTime = Now;
		if (AZoogiBall* S = Cast<AZoogiBall>(BodyActor(Truth.Actor))) Truth.PowerHitLocation = S->BallLocation();
		const bool bRival = ShooterFirstContact >= 0 && ShooterFirstContact < 100 && !G->IsAlly(BodyActor(Truth.Actor), BodyActor(ShooterFirstContact));
		if (Mode != EZoogiEpisodeMode::Off && Truth.Awaken == EZoogiArtifact::Titan && bRival)
		{
			bArchetypePending = true;
			ArchetypeHitGame = W->GetTimeSeconds();
			ArchetypeShooter = Cast<AZoogiBall>(BodyActor(Truth.Actor));
			ArchetypeTarget = Cast<AZoogiBall>(BodyActor(ShooterFirstContact));
		}
	}
	bPrevInfusedContact = G->bInfusedContact;
	if (G->bComboMode)
	{
		Truth.ComboTriggers |= G->ShotTriggers;
		if (G->ShotTriggers & ZoogiComboTrigger::GuardBreak) Truth.bGuardBreak = true;
	}

	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiEpisodeGround), false);
	for (AZoogiBall* B : G->Balls) if (B) Q.AddIgnoredActor(B);
	for (AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
	FCollisionObjectQueryParams OQ;
	OQ.AddObjectTypesToQuery(ECC_WorldStatic);
	OQ.AddObjectTypesToQuery(ECC_WorldDynamic);

	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		AZoogiBall* B = G->Balls[i];
		if (!B) continue;
		if (StartOut.IsValidIndex(i) && !StartOut[i] && B->bOut && !OutCounted.Contains(i))
		{
			OutCounted.Add(i);
			Truth.RingOuts.Add(i);
			if (int32* Bi = OpenBoundary.Find(i)) { Truth.Boundary[*Bi].bFellOut = true; Truth.Boundary[*Bi].bCrossed = true; }
			else
			{
				FZoogiBoundaryEvent E; E.Body = i; E.Time = Now; E.bCrossed = true; E.bFellOut = true; E.Location = B->BallLocation();
				OpenBoundary.Add(i, Truth.Boundary.Add(E));
			}
		}
		if (B->bOut) continue;
		const FVector L = B->BallLocation();
		const FVector V = B->GetBallVelocity();
		const float Speed2D = FVector2D(V).Size();
		// airborne: gap under the ball
		FAirTrack& A = Air.FindOrAdd(i);
		if (Speed2D > 20.f || FMath::Abs(V.Z) > 20.f || A.bAir)
		{
			const float R = B->Sphere ? B->Sphere->GetScaledSphereRadius() : ZoogiConst::ZoogiRadius;
			FHitResult H;
			float Gap = 99999.f;
			if (W->LineTraceSingleByObjectType(H, L, L - FVector(0, 0, R + 600.f), OQ, Q)) Gap = (L.Z - R) - H.ImpactPoint.Z;
			const bool bAirNow = Gap > Tune.AirborneGap && Gap < 5000.f;
			if (bAirNow)
			{
				if (!A.bAir) { A.bAir = true; A.Start = Now; A.Peak = 0.f; }
				A.Peak = FMath::Max(A.Peak, Gap);
			}
			else if (A.bAir)
			{
				A.bAir = false;
				if (Now - A.Start >= Tune.AirborneMinTime && A.Peak >= Tune.AirborneMinPeak)
				{
					FZoogiAirSegment S; S.Body = i; S.Start = A.Start; S.End = Now; S.PeakGap = A.Peak;
					Truth.Air.Add(S);
				}
			}
		}
		// boundary: near misses at speed, crossings
		if (G->Arena)
		{
			FVector2D P(L.X, L.Y);
			const FVector2D Rad = P.IsNearlyZero() ? FVector2D(1.f, 0.f) : P.GetSafeNormal();
			const bool bOff = G->Arena->IsOffCourt(P);
			const float Edge = bOff ? 0.f : G->Arena->DistanceToEdge(P, Rad);
			const float Outward = FVector2D::DotProduct(FVector2D(V), Rad);
			int32* Bi = OpenBoundary.Find(i);
			if (bOff || (Edge < Tune.EdgeNearDist && Outward > Tune.EdgeNearSpeed * 0.5f))
			{
				if (!Bi)
				{
					FZoogiBoundaryEvent E; E.Body = i; E.Time = Now; E.MinEdgeDist = Edge; E.Location = L;
					Bi = &OpenBoundary.Add(i, Truth.Boundary.Add(E));
				}
				FZoogiBoundaryEvent& E = Truth.Boundary[*Bi];
				if (Edge <= E.MinEdgeDist) { E.MinEdgeDist = Edge; E.Location = L; }
				E.SpeedAtEdge = FMath::Max(E.SpeedAtEdge, Speed2D);
				E.bCrossed |= bOff;
				E.bSettledIn = false;
			}
			else if (Bi && !Truth.Boundary[*Bi].bFellOut && Speed2D < 40.f && !bOff)
			{
				Truth.Boundary[*Bi].bSettledIn = true;
			}
		}
	}
	for (int32 j = 0; j < G->Orbs.Num(); ++j)
	{
		AZoogiOrb* O = G->Orbs[j];
		if (!O || !StartOrbOut.IsValidIndex(j) || StartOrbOut[j] || !O->bOut || OutCounted.Contains(100 + j)) continue;
		OutCounted.Add(100 + j);
		Truth.OrbOuts++;
		FZoogiBoundaryEvent E; E.Body = 100 + j; E.Time = Now; E.bCrossed = true; E.bFellOut = true; E.Location = O->OrbLocation();
		Truth.Boundary.Add(E);
	}
	Truth.PiecesDamaged = FMath::Max(0, PieceSteps() - StartPieceSteps);
	Truth.RocksDamaged = FMath::Max(0, RockSteps() - StartRockSteps);

	for (int32 i = 0; i < G->Balls.Num(); ++i) PrevVel.Add(i, BodyVelocity(i));
	for (int32 i = 0; i < G->Orbs.Num(); ++i) PrevVel.Add(100 + i, BodyVelocity(100 + i));
	UpdateLive();
}

// =====================================================================================================================
// recognition (deterministic)

FZoogiEpisodeRecognitionRecord UZoogiEpisodeSubsystem::Recognize(const FZoogiGameplayTruthRecord& T, const FZoogiEpisodeTuning& Tune)
{
	constexpr int32 NT = (int32)EZoogiEpisodeTag::Num;
	float W[NT];      // best significance weight of the tag (single-instance tags)
	bool Has[NT];
	for (int32 i = 0; i < NT; ++i) { W[i] = 0.f; Has[i] = false; }
	auto Mark = [&](EZoogiEpisodeTag Tag, float Weight) { const int32 k = (int32)Tag; Has[k] = true; W[k] = FMath::Max(W[k], Weight); };
	auto TeamOf = [&T](int32 P) { return T.bCombo ? P / 2 : P; };
	auto IsRival = [&](int32 Code) { return Code >= 0 && Code < 100 && Code != T.Actor && TeamOf(Code) != TeamOf(T.Actor); };

	TMap<int32, int32> WallsSince;
	TMap<int32, float> WallW;
	TSet<int32> FellOut;
	for (const FZoogiBoundaryEvent& B : T.Boundary) if (B.bFellOut) FellOut.Add(B.Body);
	int32 MaxDepth = 0;
	for (const FZoogiContactEvent& E : T.Contacts)
	{
		if (!E.bMeaningful) continue;
		const bool bSurface = E.Kind == EZoogiContactKind::Wall || E.Kind == EZoogiContactKind::Bumper || E.Kind == EZoogiContactKind::Ramp
			|| E.Kind == EZoogiContactKind::Piece || E.Kind == EZoogiContactKind::Rock;
		if (bSurface)
		{
			WallsSince.FindOrAdd(E.Body)++;
			WallW.FindOrAdd(E.Body) = FMath::Max(WallW.FindRef(E.Body), E.Weight);
			// a struck body (not the shooter) deflected hard by a wall
			if (E.Body != T.Actor && E.Depth >= 1 && E.RelSpeed >= 1.4f * Tune.MeaningfulSpeed) Mark(EZoogiEpisodeTag::WallRedirect, E.Weight);
			if (E.Kind == EZoogiContactKind::Ramp || E.Kind == EZoogiContactKind::Bumper) Mark(EZoogiEpisodeTag::Terrain, E.Weight * 0.8f);
			continue;
		}
		MaxDepth = FMath::Max(MaxDepth, E.Depth + 1);
		const int32 Walls = WallsSince.FindRef(E.Body);
		if (Walls > 0)
		{
			const float BW = FMath::Min(E.Weight, WallW.FindRef(E.Body)) + 0.25f;
			if (E.Body == T.Actor && E.Depth == 0) Mark(Walls >= 2 ? EZoogiEpisodeTag::DoubleBank : EZoogiEpisodeTag::Bank, BW);
			else Mark(EZoogiEpisodeTag::WallRedirect, BW);
		}
		WallsSince.Add(E.Body, 0);
		WallW.Add(E.Body, 0.f);
		if (E.Kind == EZoogiContactKind::Ally) Mark(EZoogiEpisodeTag::AllyBump, E.Weight);
		// relay: a moving orb passes the hit on and the receiver goes out
		if (E.Body >= 100 && E.Depth >= 1 && E.OtherBody >= 0 && FellOut.Contains(E.OtherBody)) Mark(EZoogiEpisodeTag::OrbRelay, E.Weight);
	}
	if (Has[(int32)EZoogiEpisodeTag::DoubleBank]) Has[(int32)EZoogiEpisodeTag::Bank] = false;
	// 2v2 combo rules already judged these (same definitions as the follow-up triggers)
	if ((T.ComboTriggers & ZoogiComboTrigger::BankSetup) && !Has[(int32)EZoogiEpisodeTag::DoubleBank]) Mark(EZoogiEpisodeTag::Bank, 1.f);
	if (T.ComboTriggers & ZoogiComboTrigger::AllyBump) Mark(EZoogiEpisodeTag::AllyBump, 1.f);
	if (T.ComboTriggers & ZoogiComboTrigger::OrbRelay) Mark(EZoogiEpisodeTag::OrbRelay, 1.f);
	if (T.ComboTriggers & ZoogiComboTrigger::AirborneSetup) Mark(EZoogiEpisodeTag::Airborne, 1.f);
	if (T.ComboTriggers & ZoogiComboTrigger::TerrainSetup) Mark(EZoogiEpisodeTag::Terrain, 1.f);
	if ((T.ComboTriggers & ZoogiComboTrigger::GuardBreak) || T.bGuardBreak) Mark(EZoogiEpisodeTag::GuardBreak, 1.f);
	for (const FZoogiAirSegment& S : T.Air) Mark(EZoogiEpisodeTag::Airborne, FMath::Clamp(S.PeakGap / 60.f, 0.5f, 1.5f));
	for (const FZoogiBoundaryEvent& B : T.Boundary)
		if (B.Body < 100 && !B.bFellOut && B.bSettledIn && B.MinEdgeDist < Tune.EdgeNearDist && B.SpeedAtEdge >= Tune.EdgeNearSpeed)
			Mark(EZoogiEpisodeTag::BoundarySave, FMath::Clamp(1.f - B.MinEdgeDist / Tune.EdgeNearDist + B.SpeedAtEdge / 1500.f, 0.5f, 1.5f));
	if (T.bPowerHit) Mark(EZoogiEpisodeTag::PowerHit, 1.f);
	if (T.PiecesDamaged + T.RocksDamaged > 0) Mark(EZoogiEpisodeTag::Terrain, 1.f);
	if (T.bFollowUp) Mark(EZoogiEpisodeTag::PartnerFollowUp, 1.f);
	int32 RivalOuts = 0;
	for (int32 P : T.RingOuts) if (IsRival(P)) ++RivalOuts;
	if (RivalOuts > 0) Mark(EZoogiEpisodeTag::RingOut, 1.f);
	if (T.OrbOuts > 0) Mark(EZoogiEpisodeTag::OrbOut, 1.f);

	// points
	auto Base = [&Tune](EZoogiEpisodeTag Tag)
	{
		switch (Tag)
		{
		case EZoogiEpisodeTag::Bank: return Tune.PBank;
		case EZoogiEpisodeTag::DoubleBank: return Tune.PDoubleBank;
		case EZoogiEpisodeTag::AllyBump: return Tune.PAllyBump;
		case EZoogiEpisodeTag::OrbRelay: return Tune.POrbRelay;
		case EZoogiEpisodeTag::WallRedirect: return Tune.PWallRedirect;
		case EZoogiEpisodeTag::BoundarySave: return Tune.PBoundarySave;
		case EZoogiEpisodeTag::Airborne: return Tune.PAirborne;
		case EZoogiEpisodeTag::PowerHit: return Tune.PPowerHit;
		case EZoogiEpisodeTag::GuardBreak: return Tune.PGuardBreak;
		case EZoogiEpisodeTag::OrbOut: return Tune.POrbOut;
		case EZoogiEpisodeTag::RingOut: return Tune.PRingOut;
		case EZoogiEpisodeTag::Terrain: return Tune.PTerrain;
		case EZoogiEpisodeTag::PartnerFollowUp: return Tune.PPartnerFollowUp;
		default: return 0.f;
		}
	};
	auto Repeat = [&Tune](float B, int32 N) { float S = 0.f, K = 1.f; for (int32 i = 0; i < N; ++i) { S += B * K; K *= Tune.RepeatFalloff; } return S; };

	FZoogiEpisodeRecognitionRecord R;
	R.ActionId = T.ActionId;
	float Score = 0.f;
	float Best = -1.f;
	for (int32 k = 0; k < NT; ++k)
	{
		if (!Has[k]) continue;
		const EZoogiEpisodeTag Tag = (EZoogiEpisodeTag)k;
		float P = Base(Tag);
		if (Tag == EZoogiEpisodeTag::OrbOut) P = Repeat(P, T.OrbOuts);
		else if (Tag == EZoogiEpisodeTag::RingOut) P = Repeat(P, RivalOuts);
		else if (Tag == EZoogiEpisodeTag::Terrain) P = FMath::Min(P + Tune.PTerrainExtra * FMath::Max(0, T.PiecesDamaged + T.RocksDamaged - 1), 2.f * P) * FMath::Clamp(0.6f + 0.4f * W[k], 0.6f, 1.2f);
		else if (Tag == EZoogiEpisodeTag::PowerHit) P += T.bCrit ? Tune.PCrit : 0.f;
		else P *= FMath::Clamp(0.6f + 0.4f * W[k], 0.6f, 1.2f); // significance: soft taps are worth less
		R.Tags.Add(Tag);
		R.TagPoints.Add(P);
		Score += P;
		R.Breakdown += FString::Printf(TEXT("%s:%.1f "), ZoogiEpisode::TagName(Tag), P);
		if (P > Best) { Best = P; R.Headline = ZoogiEpisode::TagLabel(Tag); }
	}
	const int32 Cats = R.Tags.Num();
	const float Syn = Cats > 2 ? (Cats - 2) * Tune.SynergyPer : 0.f;
	const float Chain = Cats > 0 ? FMath::Min(Tune.ChainBonusMax, FMath::Max(0, MaxDepth - 1) * Tune.ChainBonus) : 0.f;
	const float Swing = (T.ScoreDelta.IsValidIndex(T.Actor) && T.ScoreDelta[T.Actor] >= Tune.ScoreSwingPoints) ? Tune.ScoreSwingBonus : 0.f;
	Score += Syn + Chain + Swing;
	R.Breakdown += FString::Printf(TEXT("synergy:%.1f chain:%.1f swing:%.1f"), Syn, Chain, Swing);
	R.Score = Score;

	int32 Tier = 0;
	if (Score >= Tune.T1) Tier = 1;
	if (Score >= Tune.T2) Tier = 2;
	if (Score >= Tune.T3) Tier = 3;
	if (Score >= Tune.T4 && T.bPowerHit) Tier = 4;                                                      // tier 4 = power escalation: needs a real power hit
	if (Score >= Tune.T5 && T.bPowerHit && Cats >= Tune.T5MinCategories && (RivalOuts > 0 || T.OrbOuts >= 2)) Tier = 5;
	if (T.Awaken == EZoogiArtifact::Titan && T.bPowerHit && IsRival(T.PowerHitTarget))
	{
		R.Archetype = EZoogiEpisodeArchetype::TitanMeteor;
		Tier = FMath::Max(Tier, 4); // the archetype is the tier-4 power escalation for Titan
	}
	R.Tier = Tier;
	return R;
}

// =====================================================================================================================
// finalize + log

void UZoogiEpisodeSubsystem::FinalizeAction(const TCHAR* Why)
{
	AZoogiGameMode* G = GM();
	if (!bActive || !G) { bActive = false; return; }
	bActive = false;
	UWorld* W = GetWorld();
	const float Now = W ? W->GetTimeSeconds() - ActionStartGame : Truth.Duration;
	for (TPair<int32, FAirTrack>& A : Air)
		if (A.Value.bAir && Now - A.Value.Start >= Tune.AirborneMinTime && A.Value.Peak >= Tune.AirborneMinPeak)
		{
			FZoogiAirSegment S; S.Body = A.Key; S.Start = A.Value.Start; S.End = Now; S.PeakGap = A.Value.Peak;
			Truth.Air.Add(S);
		}
	Truth.ScoreDelta.Reset(); Truth.RingMarkDelta.Reset(); Truth.FinalBallPos.Reset(); Truth.FinalBallOut.Reset();
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		AZoogiBall* B = G->Balls[i];
		Truth.ScoreDelta.Add(B && StartScore.IsValidIndex(i) ? B->Score - StartScore[i] : 0);
		Truth.RingMarkDelta.Add(B && StartMarks.IsValidIndex(i) ? B->RingMarks - StartMarks[i] : 0);
		Truth.FinalBallPos.Add(B ? B->BallLocation() : FVector::ZeroVector);
		Truth.FinalBallOut.Add(B ? (B->bOut || OutCounted.Contains(i)) : true);
	}
	if (G->bComboMode)
	{
		const int32 Team = FMath::Clamp(Truth.ActorTeam, 0, 1);
		Truth.ResonanceDelta = G->ComboResonance[Team] - StartResonance;
		Truth.ControlDelta = G->ArenaControl[Team] - StartControl;
	}
	const FZoogiEpisodeRecognitionRecord R = Recognize(Truth, Tune);
	if (bFrozen) UnfreezeWorld();
	if (!bEpilogueShown) ShowEpilogue();
	bArchetypePending = false;
	// RULE ZERO: the cinematic time dilation in effect at launch must be back (receivers ease slow-mo in / out and restore)
	float CineNow = StartCinematicDilation, GlobalNow = StartGlobalDilation;
	if (const AWorldSettings* WS = W ? W->GetWorldSettings() : nullptr) { CineNow = WS->CinematicTimeDilation; GlobalNow = WS->TimeDilation; }
	Plan.bDilationRestored = CineNow == StartCinematicDilation;

	FString Tags;
	for (EZoogiEpisodeTag T : R.Tags) Tags += (Tags.IsEmpty() ? TEXT("") : TEXT(",")) + FString(ZoogiEpisode::TagName(T));
	if (Tags.IsEmpty()) Tags = TEXT("-");
	int32 Meaningful = 0;
	for (const FZoogiContactEvent& E : Truth.Contacts) if (E.bMeaningful) ++Meaningful;
	FString Deltas, Marks, Final;
	for (int32 i = 0; i < Truth.ScoreDelta.Num(); ++i)
	{
		if (Truth.ScoreDelta[i] != 0) Deltas += FString::Printf(TEXT("%sP%d%+d"), Deltas.IsEmpty() ? TEXT("") : TEXT(","), i + 1, Truth.ScoreDelta[i]);
		if (Truth.RingMarkDelta[i] != 0) Marks += FString::Printf(TEXT("%sP%d%+d"), Marks.IsEmpty() ? TEXT("") : TEXT(","), i + 1, Truth.RingMarkDelta[i]);
		Final += FString::Printf(TEXT("%sP%d(%.0f,%.0f%s)"), Final.IsEmpty() ? TEXT("") : TEXT(" "), i + 1, Truth.FinalBallPos[i].X, Truth.FinalBallPos[i].Y, Truth.FinalBallOut[i] ? TEXT(",out") : TEXT(""));
	}
	FString AirS;
	for (const FZoogiAirSegment& S : Truth.Air) AirS += FString::Printf(TEXT("%s%s%d:%.0fcm/%.2fs"), AirS.IsEmpty() ? TEXT("") : TEXT(","), S.Body >= 100 ? TEXT("O") : TEXT("P"), S.Body >= 100 ? S.Body - 99 : S.Body + 1, S.PeakGap, S.End - S.Start);
	FString Ring;
	for (int32 P : Truth.RingOuts) Ring += FString::Printf(TEXT("%sP%d"), Ring.IsEmpty() ? TEXT("") : TEXT(","), P + 1);

	UE_LOG(LogZoogi, Log, TEXT("EPISODE action=%d tags=%s score=%.0f tier=%d archetype=%s result_unchanged=%d mode=%s actor=P%d(%s) presented_tier=%d layout=%s panels=%d callouts=%d cues=%d/%d shots=%d slowmo_req=%d power=%d meteor=%d freezes=%d drift_cm=%.3f dilation_restored=%d(cine=%.3f global=%.3f) epilogue=\"%s\" why=%s"),
		Truth.ActionId, *Tags, R.Score, R.Tier, ZoogiEpisode::ArchetypeName(R.Archetype), Plan.ResultUnchanged() ? 1 : 0, ZoogiEpisode::ModeName(Mode), Truth.Actor + 1, *Truth.ActorName,
		Plan.Tier, ZoogiEpisode::LayoutName(Plan.Layout), Plan.Panels, Plan.Callouts, Plan.CuesHandled, Plan.Cues, Plan.Shots.Num(), Plan.bSlowMo ? 1 : 0, Plan.bPowerEscalation ? 1 : 0, Plan.bMeteor ? 1 : 0, Plan.Freezes, Plan.FreezeDriftCm,
		Plan.bDilationRestored ? 1 : 0, CineNow, GlobalNow, *Plan.Epilogue, Why);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE_TRUTH action=%d round=%d turn=%d combo=%d power=%.2f awaken=%s power_hit=%d crit=%d power_target=%d contacts=%d meaningful=%d max_rel=%.0f air=%d[%s] boundary=%d ring_outs=%s orb_outs=%d score_delta=%s ringmark_delta=%s resonance=%+d control=%+.0f triggers=0x%x pieces=%d rocks=%d follow_up=%d dur=%.2f final=%s"),
		Truth.ActionId, Truth.Round, Truth.Turn, Truth.bCombo ? 1 : 0, Truth.LaunchPower, *ZoogiArtifacts::Name(Truth.Awaken), Truth.bPowerHit ? 1 : 0, Truth.bCrit ? 1 : 0, Truth.PowerHitTarget,
		Truth.Contacts.Num(), Meaningful, Truth.MaxRelSpeed, Truth.Air.Num(), *AirS, Truth.Boundary.Num(), Ring.IsEmpty() ? TEXT("-") : *Ring, Truth.OrbOuts,
		Deltas.IsEmpty() ? TEXT("-") : *Deltas, Marks.IsEmpty() ? TEXT("-") : *Marks, Truth.ResonanceDelta, Truth.ControlDelta, Truth.ComboTriggers, Truth.PiecesDamaged, Truth.RocksDamaged,
		Truth.bFollowUp ? 1 : 0, Truth.Duration, *Final);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE_SCORE action=%d %s"), Truth.ActionId, *R.Breakdown);
	LiveRec = R;
	OnActionFinalized.Broadcast(Truth, R, Plan);
	if (Truth.ActionId == TestActionId) TestFinalize();
}
