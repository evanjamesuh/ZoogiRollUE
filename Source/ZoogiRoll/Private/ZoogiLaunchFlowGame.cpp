// AZoogiGameMode side of the launch flow (see ZoogiLaunchFlow.h): opening launches from the pads, respawn to pad,
// glove-crane hook with a direct-placement fallback, pad-phase event for the pad camera, Episode Energy gate.
#include "ZoogiGameMode.h"
#include "ZoogiExpressionComponent.h"
#include "ZoogiLaunchFlow.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/OutputDeviceNull.h"
#include "HAL/PlatformTime.h"

FTransform AZoogiGameMode::GetLaunchPadTransform(int32 Player) const
{
	return ZoogiLaunchPads::GetPadTransform(GetWorld(), Arena, Player, Balls.Num());
}

bool AZoogiGameMode::IsEpisodeEnergyRunning() const
{
	// Gate only while the opening launches run; everything else (combo, old flow, tests without a match) is unchanged.
	return !bLaunchFlow || bComboMode || !LaunchFlow.bOpening || LaunchFlow.bGameplayStarted;
}

bool AZoogiGameMode::IsOnLaunchPad(const AZoogiBall* B) const
{
	return bLaunchFlow && B && OnPad.IsValidIndex(B->PlayerIndex) && OnPad[B->PlayerIndex];
}

bool AZoogiGameMode::IsParked(const AZoogiBall* B) const
{
	return bLaunchFlow && B && B->bParked;
}

bool AZoogiGameMode::LaunchFlowEnabled()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiNoLaunchFlow"))) return false;
	// Episode / battle drivers expect Episode Energy from turn 1: old flow for them.
	FString V;
	for (const TCHAR* K : { TEXT("ZoogiEpisodeTest="), TEXT("ZoogiBattleTest=") }) if (FParse::Value(Cmd, K, V)) return false;
	for (const TCHAR* K : { TEXT("ZoogiEpisodeTest"), TEXT("ZoogiEpisodeP5Test"), TEXT("ZoogiBattleTest") }) if (FParse::Param(Cmd, K)) return false;
	return true;
}

int32 AZoogiGameMode::ParseLaunchFirst()
{
	const TCHAR* Cmd = FCommandLine::Get();
	int32 First = -1;
	if (FParse::Value(Cmd, TEXT("ZoogiLaunchFirst="), First)) return FMath::Max(First, 0); // 0-based: 0 = P1
	// Older in-game automation assumes P1 opens the match: keep that for every test driver.
	TArray<FString> Tok; FString(Cmd).ParseIntoArrayWS(Tok);
	for (const FString& T : Tok)
		if (T.StartsWith(TEXT("-Zoogi")) && (T.Contains(TEXT("Test")) || T.Contains(TEXT("ClashTour")))) return 0;
	return -1;
}

void AZoogiGameMode::BeginOpeningLaunches()
{
	const int32 N = Balls.Num();
	const TCHAR* Cmd = FCommandLine::Get();
	bLaunchFlow = LaunchFlowEnabled() && !FrozenRoamOn() && Arena && Arena->GetLaunchPad(); // Oct 27: no pad on the Frozen Ring (bridge launches); Oct 30: ship pad only on Cosmic, field starts elsewhere
	LaunchFirstOverride = ParseLaunchFirst();
	LaunchFlow.Reset();
	OnPad.Init(false, N);
	PadAnchor.Init(FVector::ZeroVector, N);
	PadPhasePlayer = -1;
	RoundStartPlayer = 0;
	if (!bLaunchFlow || N == 0) { BeginTurn(0); return; }
	// Own stream: picking the first player never shifts a GameRng draw. Reproducible under -ZoogiMatchSeed.
	int32 Seed = 0;
	const bool bFixed = FParse::Value(Cmd, TEXT("ZoogiMatchSeed="), Seed);
	FRandomStream R(bFixed ? (Seed ^ 0x1A0C4F1D) : (int32)(FPlatformTime::Cycles() ^ 0x1A0C4F1D));
	if (LaunchFirstOverride >= 0) LaunchFlow.BeginOpeningWithFirst(N, LaunchFirstOverride % N);
	else LaunchFlow.BeginOpening(N, R);
	RoundStartPlayer = LaunchFlow.FirstPlayer;
	// Only the active launcher sits on the pad deck. Waiting openers park UNDER the pad (same X/Y, Z below the deck) so they
	// are out of the behind-the-player pad camera's view toward the arena. Each later opener steps onto the pad at BeginTurn.
	for (int32 i = 0; i < N; ++i) if (i != LaunchFlow.FirstPlayer && Balls.IsValidIndex(i) && Balls[i]) ParkUnderPad(Balls[i]);
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH FLOW: opening launches, P%d first (random), Episode Energy waits until all %d have launched; waiters parked under pad"), LaunchFlow.FirstPlayer + 1, N);
	AddFeed(FString::Printf(TEXT("P%d launches first"), LaunchFlow.FirstPlayer + 1), ZoogiUtil::PlayerColor(LaunchFlow.FirstPlayer));
	BeginTurn(LaunchFlow.FirstPlayer);
}

void AZoogiGameMode::PlaceOnPad(AZoogiBall* Ball, bool bFX)
{
	if (!Ball) return;
	const int32 P = Ball->PlayerIndex;
	const FTransform T = GetLaunchPadTransform(P);
	const FVector L = T.GetLocation();
	Ball->PlaceAt(L, T.Rotator().Yaw);
	Ball->LastHitPlayer = -1;
	Ball->Squash(0.22f, FVector::UpVector); // Oct 24: toon pop up out of the ship (visual only)
	Ball->Wobble(5.f, T.GetRotation().GetForwardVector());
	if (bFX) AZoogiFX::KFX(GetWorld(), TEXT("Effect5"), L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), 0.45f, 2.5f); // same portal FX as the old respawn
	if (P >= 0)
	{
		if (OnPad.Num() <= P) { OnPad.SetNum(P + 1); PadAnchor.SetNum(P + 1); }
		OnPad[P] = true;
		Ball->bOnLaunchPad = true;
		const FVector C = Arena ? Arena->GetActorLocation() : FVector::ZeroVector; // aim into the arena
		const FVector To(C.X - L.X, C.Y - L.Y, 0.f);
		if (!To.IsNearlyZero()) Ball->AimYaw = To.Rotation().Yaw;
		PadAnchor[P] = L;
	}
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d on its pad at (%.0f, %.0f, %.0f)"), P + 1, L.X, L.Y, L.Z);
	if (P >= 0) LaunchFlow.PadPlaceWatch.Add(P, 0.f);
}


FVector AZoogiGameMode::GetParkLocation(int32 Player) const
{
	const AZoogiLaunchPad* Pad = Arena ? Arena->GetLaunchPad() : nullptr;
	const FVector Deck = Pad ? Pad->GetActorLocation() : GetLaunchPadTransform(Player).GetLocation();
	return ZoogiLaunchPads::ParkUnderPadLocation(Deck, Player, Balls.Num());
}

void AZoogiGameMode::ParkUnderPad(AZoogiBall* Ball)
{
	if (!Ball) return;
	const int32 P = Ball->PlayerIndex;
	const FVector L = GetParkLocation(P);
	const float Yaw = GetLaunchPadTransform(P).Rotator().Yaw;
	Ball->ParkAt(L, Yaw);
	if (P >= 0)
	{
		if (OnPad.Num() <= P) { OnPad.SetNum(P + 1); PadAnchor.SetNum(P + 1); }
		OnPad[P] = false;
		Ball->bOnLaunchPad = false;
		PadAnchor[P] = L;
		if (PadPhasePlayer == P) SetPadPhase(P, false);
	}
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d parked UNDER pad at (%.0f, %.0f, %.0f) (collision off)"), P + 1, L.X, L.Y, L.Z);
}

void AZoogiGameMode::SendToPad(AZoogiBall* Ball)
{
	if (!Ball) return;
	const int32 P = Ball->PlayerIndex;
	if (LaunchFlow.IsCarryPending(P)) return; // the crane already has it (timeout / BeginTurn force the placement)
	if (FZoogiLaunchFlow::ShouldWaitForCrane(OnZoogiNeedsCarryToPad.IsBound()))
	{
		LaunchFlow.BeginCarry(P, CraneTimeout);
		UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d needs carrying to its pad (crane hook, %.1f s fallback)"), P + 1, CraneTimeout);
		OnZoogiNeedsCarryToPad.Broadcast(Ball, FTransform(GetLaunchPadTransform(P).GetRotation(), GetParkLocation(P)));
		return;
	}
	ParkUnderPad(Ball); // waiting/returned park under the deck; BeginTurn seats the active launcher on the pad
}

void AZoogiGameMode::NotifyZoogiPlacedOnPad(AZoogiBall* Zoogi)
{
	if (!Zoogi) return;
	LaunchFlow.CompleteCarry(Zoogi->PlayerIndex);
	// Crane delivers to the park spot under the pad; the Zoogi steps onto the deck at its own BeginTurn.
	ParkUnderPad(Zoogi);
	// Oct 22 queue #38: pad entrance pose - soft land squash + flourish + small ring
	Zoogi->Squash(-0.09f, FVector::UpVector);
	if (Zoogi->Expression) Zoogi->Expression->Trigger(EZoogiExpr::Flourish, 1.6f, TEXT("pad entrance"));
	AZoogiFX::Spawn(GetWorld(), Zoogi->BallLocation(), ZoogiUtil::PlayerColor(Zoogi->PlayerIndex), 90.f, 0.45f, true, 3.5f);
	UE_LOG(LogZoogi, Log, TEXT("PAD entrance pose P%d"), Zoogi->PlayerIndex + 1);
}

void AZoogiGameMode::SetPadPhase(int32 Player, bool bOn)
{
	if (Player < 0) return;
	if (bOn)
	{
		if (PadPhasePlayer == Player) return;
		if (PadPhasePlayer >= 0) SetPadPhase(PadPhasePlayer, false);
		PadPhasePlayer = Player;
	}
	else
	{
		if (PadPhasePlayer != Player) return;
		PadPhasePlayer = -1;
	}
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: pad phase %s P%d"), bOn ? TEXT("START") : TEXT("END"), Player + 1);
	OnLaunchPadPhase.Broadcast(Player, bOn);
	if (UWorld* W = GetWorld())
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			FOutputDeviceNull Ar;
			PC->CallFunctionByNameWithArguments(*FString::Printf(TEXT("%s %d"), bOn ? TEXT("OnLaunchPadPhaseStarted") : TEXT("OnLaunchPadPhaseEnded"), Player), Ar, nullptr, true);
		}
}

void AZoogiGameMode::TickLaunchFlow(float Dt)
{
	TickPadFlights(Dt);
	for (int32 P : LaunchFlow.TickCarries(Dt))
	{
		UE_LOG(LogZoogi, Warning, TEXT("LAUNCH PAD: crane did not place P%d in %.1f s, parking under pad (no stall)"), P + 1, CraneTimeout);
		if (Balls.IsValidIndex(P) && Balls[P]) ParkUnderPad(Balls[P]);
	}
	for (int32 P = 0; P < OnPad.Num(); ++P)
	{
		if (!OnPad[P] || !Balls.IsValidIndex(P) || !Balls[P] || P != ActivePlayer) continue; // idle pad Zoogis keep the flag (re-snapped at their turn)
		if (FVector::Dist2D(Balls[P]->BallLocation(), PadAnchor[P]) > 60.f || Balls[P]->bOut)
		{
			OnPad[P] = false; // launched (or pushed) off the pad
			Balls[P]->bOnLaunchPad = false;
			SetPadPhase(P, false);
		}
	}
}

int32 AZoogiGameMode::OpeningNextTurn()
{
	const int32 Next = LaunchFlow.OnOpeningTurnEnded(ActivePlayer);
	if (LaunchFlow.bGameplayStarted)
	{
		RoundStartPlayer = LaunchFlow.FirstPlayer;
		UE_LOG(LogZoogi, Log, TEXT("LAUNCH FLOW: everyone launched - gameplay begins, Episode Energy starts, round 1 from P%d"), Next + 1);
	}
	return Next;
}

FVector AZoogiGameMode::GetPadLaunchVelocity(const AZoogiBall* Ball, float AimYaw) const
{
	// Fixed 30 deg elevation along the aim (arrow) exactly; speed solved so the arc lands on the aim ray at the point nearest
	// the pad's LandingTarget (else the arena centre), kept inside the knock-off radius, under the world gravity, dropping
	// from the real launch height to the court floor at the landing point.
	const AZoogiLaunchPad* Pad = Arena ? Arena->GetLaunchPad() : nullptr;
	const FVector C = Arena ? Arena->GetActorLocation() : FVector::ZeroVector;
	const FVector L = Ball ? Ball->BallLocation() : (Pad ? Pad->GetLaunchTransform().GetLocation() : C);
	const FVector Tgt = GetPadLandingPoint(Ball ? Ball->PlayerIndex : 0);
	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	const float Yr = FMath::DegreesToRadians(AimYaw), Th = FMath::DegreesToRadians(ZoogiLaunchPads::StubLaunchElevationDeg);
	const FVector2D U(FMath::Cos(Yr), FMath::Sin(Yr));
	const float D0 = ZoogiLaunchPads::PadLandingDistanceAlong(FVector2D(L - C), U, FVector2D(Tgt - C), FMath::Max(300.f, KR - 400.f));
	// Launch height: the ball centre, but never above the pad's launch point + one Zoogi (a stacked / bumped ball must not inflate it).
	float LaunchZ = L.Z;
	if (Pad) LaunchZ = FMath::Min(LaunchZ, Pad->GetLaunchTransform().GetLocation().Z + 2.f * ZoogiConst::ZoogiRadius);
	const float G = (GetWorld() ? -GetWorld()->GetGravityZ() : 980.f) * ZoogiLaunchPads::AirGravityScale;
	UWorld* W = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiPadFloor), false);
	for (AZoogiBall* O : Balls) if (O) Q.AddIgnoredActor(O);
	if (Pad) Q.AddIgnoredActor(Pad);
	// Court floor at a landing distance (ignores Zoogis and the pad); else the arena floor (Z of the arena actor).
	auto FloorAt = [&](float Dist) -> float
	{
		float FZ = C.Z;
		FHitResult Hit; const FVector P(L.X + U.X * Dist, L.Y + U.Y * Dist, LaunchZ);
		if (W && W->LineTraceSingleByChannel(Hit, P, P - FVector(0, 0, 4000.f), ECC_WorldStatic, Q) && Hit.ImpactPoint.Z < LaunchZ) FZ = Hit.ImpactPoint.Z;
		return FZ;
	};
	// Same 30 deg arc every time: if the arc to the target would clip a seatwall / rim / prop, land nearer (then farther) along the aim.
	auto ArcClear = [&](float Dist, float Drop, float Spd) -> bool
	{
		if (!W) return true;
		const float T = ZoogiLaunchPads::PadFlightTime(Dist, Drop, G, ZoogiLaunchPads::StubLaunchElevationDeg);
		if (T <= 0.f) return true;
		const float Vh = Spd * FMath::Cos(Th), Vz = Spd * FMath::Sin(Th);
		FVector Prev(L.X, L.Y, LaunchZ);
		for (int32 i = 1; i <= 16; ++i)
		{
			const float t = T * i / 16.f * 0.92f; // stop just before touchdown (the floor itself is not an obstacle)
			const FVector P(L.X + U.X * Vh * t, L.Y + U.Y * Vh * t, LaunchZ + Vz * t - 0.5f * G * t * t);
			FHitResult Hit;
			if (PadArcObstacleSweep(Prev, P, Hit, Ball))
			{
				UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: arc to %.0f blocked by %s / %s at (%.0f, %.0f, %.0f)"), Dist, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
				return false;
			}
			Prev = P;
		}
		return true;
	};
	float D = D0, FloorZ = FloorAt(D0), DropZ = FMath::Max(0.f, LaunchZ - (FloorZ + ZoogiConst::ZoogiRadius));
	float S = ZoogiLaunchPads::SolvePadLaunchSpeed(D, DropZ, G, ZoogiLaunchPads::StubLaunchElevationDeg, ZoogiLaunchPads::BallLinearDamping);
	if (!ArcClear(D, DropZ, S))
	{
		for (float K : { 0.85f, 0.7f, 0.55f, 1.15f, 1.3f })
		{
			const float Dk = D0 * K, Fk = FloorAt(Dk), Dropk = FMath::Max(0.f, LaunchZ - (Fk + ZoogiConst::ZoogiRadius));
			const float Sk = ZoogiLaunchPads::SolvePadLaunchSpeed(Dk, Dropk, G, ZoogiLaunchPads::StubLaunchElevationDeg, ZoogiLaunchPads::BallLinearDamping);
			if (Sk > 0.f && ArcClear(Dk, Dropk, Sk)) { D = Dk; FloorZ = Fk; DropZ = Dropk; S = Sk; break; }
		}
	}
	if (S <= 0.f) S = ZoogiLaunchPads::StubLaunchSpeed;
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: solve yaw %.0f dist=%.0f launchZ=%.0f floorZ=%.0f drop=%.0f g=%.0f -> speed %.0f"), AimYaw, D, LaunchZ, FloorZ, DropZ, G, S);
	return FVector(U.X * FMath::Cos(Th), U.Y * FMath::Cos(Th), FMath::Sin(Th)) * S;
}

FVector AZoogiGameMode::GetPadLandingPoint(int32 PlayerIdx) const
{
	const AZoogiLaunchPad* Pad = Arena ? Arena->GetLaunchPad() : nullptr;
	const FVector C = Arena ? Arena->GetActorLocation() : FVector::ZeroVector;
	const FVector T = (Pad && !Pad->LandingTarget.IsNearlyZero()) ? Pad->LandingTarget : C;
	const FVector From = Pad ? Pad->GetActorLocation() : C;
	const FVector2D Fwd = FVector2D(T - From).GetSafeNormal();
	const int32 N = FMath::Max(1, Balls.Num());
	return T + FVector(-Fwd.Y, Fwd.X, 0.f) * ((PlayerIdx - (N - 1) * 0.5f) * 350.f);
}

void AZoogiGameMode::FirePadLaunch(AZoogiBall* Ball, const FVector2D& ShotVelocity)
{
	if (!Ball) return;
	// The pad's ballistic launch (its speed / elevation, shot power ignored) along the shot's aim.
	const float Yaw = ShotVelocity.IsNearlyZero() ? Ball->AimYaw : FMath::RadiansToDegrees(FMath::Atan2(ShotVelocity.Y, ShotVelocity.X));
	const FVector V = GetPadLaunchVelocity(Ball, Yaw);
	Ball->SetPlanarVelocity(FVector(V.X, V.Y, 0.f), V.Z);
	// Oct 23: Easy Shockwaves Small pop on pad fire.
	{
		const FVector At = Ball->BallLocation();
		const FLinearColor Tint = ZoogiUtil::PlayerColor(Ball->PlayerIndex);
		AZoogiFX::Shockwave(GetWorld(), At, EZoogiShockwaveSize::Small, EZoogiShockwaveKind::Flat, &Tint, false);
	}
	LaunchFlow.BeginPadAir(Ball->PlayerIndex);
	LaunchFlow.PadHitLogged.Remove(Ball->PlayerIndex);
	if (const AZoogiLaunchPad* LP = Arena ? Arena->GetLaunchPad() : nullptr) UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: pad at (%.0f, %.0f, %.0f) yaw %.0f r=%.0f, target (%.0f, %.0f) r=%.0f"), LP->GetActorLocation().X, LP->GetActorLocation().Y, LP->GetActorLocation().Z, LP->GetActorRotation().Yaw, FVector2D(LP->GetActorLocation()).Size(), GetPadLandingPoint(Ball->PlayerIndex).X, GetPadLandingPoint(Ball->PlayerIndex).Y, FVector2D(GetPadLandingPoint(Ball->PlayerIndex)).Size());
	UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d ballistic launch (%.0f, %.0f, %.0f) |v|=%.0f yaw=%.0f; ring-out off until landing"), Ball->PlayerIndex + 1, V.X, V.Y, V.Z, V.Size(), Yaw);
}

void AZoogiGameMode::TickPadFlights(float Dt)
{
	for (auto It = LaunchFlow.PadPostLand.CreateIterator(); It; ++It)
	{
		It.Value() += Dt;
		AZoogiBall* PB = Balls.IsValidIndex(It.Key()) ? Balls[It.Key()] : nullptr;
		if (PB && PB->bOut) { UE_LOG(LogZoogi, Warning, TEXT("LAUNCH PAD: P%d RING OUT %.1f s after landing"), It.Key() + 1, It.Value()); It.RemoveCurrent(); }
		else if (It.Value() > 5.f) { if (PB) UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d still in court 5 s after landing at (%.0f, %.0f) r=%.0f"), It.Key() + 1, PB->BallLocation().X, PB->BallLocation().Y, FVector2D(PB->BallLocation()).Size()); It.RemoveCurrent(); }
	}
	for (auto It = LaunchFlow.PadPlaceWatch.CreateIterator(); It; ++It)
	{
		const float T0 = It.Value(), T1 = T0 + Dt; It.Value() = T1;
		AZoogiBall* PB = Balls.IsValidIndex(It.Key()) ? Balls[It.Key()] : nullptr;
		if (!PB || LaunchFlow.IsPadAirborne(It.Key())) { It.RemoveCurrent(); continue; }
		for (float Mark : { 0.1f, 0.5f, 1.0f }) if (T0 < Mark && T1 >= Mark)
		{
			const AZoogiLaunchPad* LP = Arena ? Arena->GetLaunchPad() : nullptr;
			const FVector A = GetLaunchPadTransform(It.Key()).GetLocation(), BL = PB->BallLocation();
			UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d placement check %.1f s: ball (%.0f, %.0f, %.0f) vs launch point Z %.0f, pad actor Z %.0f, slid %.0f cm, vz %.0f, onPad=%d"), It.Key() + 1, Mark, BL.X, BL.Y, BL.Z, A.Z, LP ? LP->GetActorLocation().Z : 0.f, FVector::Dist2D(BL, A), PB->GetBallVelocity().Z, IsOnLaunchPad(PB) ? 1 : 0);
		}
		if (T1 >= 1.0f) It.RemoveCurrent();
	}
	if (LaunchFlow.PadAir.Num() == 0) return;
	TArray<int32> Keys; LaunchFlow.PadAir.GetKeys(Keys);
	for (int32 P : Keys)
	{
		AZoogiBall* B = Balls.IsValidIndex(P) ? Balls[P] : nullptr;
		bool bGround = false;
		if (B && !B->bOut && GetWorld())
		{
			const FVector L = B->BallLocation();
			const AZoogiLaunchPad* Pad = Arena ? Arena->GetLaunchPad() : nullptr;
			const FVector PadO = Pad ? Pad->GetActorLocation() : GetLaunchPadTransform(P).GetLocation();
			const float DeckR = ZoogiLaunch::DeckRadius;
			if (FZoogiLaunchFlow::PadLandingEligible(B->GetBallVelocity().Z, FVector::Dist2D(L, PadO), DeckR, ZoogiConst::ZoogiRadius))
			{
				FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiPadLanding), false, B);
				for (AZoogiBall* O : Balls) if (O) Q.AddIgnoredActor(O);
				if (Pad) Q.AddIgnoredActor(Pad);
				bGround = GetWorld()->LineTraceSingleByChannel(Hit, L, L - FVector(0, 0, ZoogiConst::ZoogiRadius + 15.f), ECC_WorldStatic, Q);
			}
		}
		if (B && !B->bOut && GetWorld() && !LaunchFlow.PadHitLogged.Contains(P))
		{
			// diagnostics: first thing the arc runs into (sphere sweep along this frame's motion)
			const FVector L = B->BallLocation(), E = L + B->GetBallVelocity() * FMath::Max(Dt, 0.02f) * 1.5f;
			FHitResult Hit;
			if (PadArcObstacleSweep(L, E, Hit, B))
			{
				LaunchFlow.PadHitLogged.Add(P);
				UE_LOG(LogZoogi, Warning, TEXT("LAUNCH PAD: P%d flight hit %s / %s at (%.0f, %.0f, %.0f) r=%.0f after %.2f s"), P + 1, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, FVector2D(Hit.ImpactPoint).Size(), LaunchFlow.PadAir.FindRef(P));
			}
		}
		if (!B || B->bOut) { if (B) UE_LOG(LogZoogi, Warning, TEXT("LAUNCH PAD: P%d RING OUT during pad flight"), P + 1); LaunchFlow.PadAir.Remove(P); continue; }
		const float AirT = LaunchFlow.PadAir.FindRef(P) + Dt;
		if (LaunchFlow.TickPadAir(P, Dt, bGround)) { LaunchFlow.PadPostLand.Add(P, 0.f); if (bGround) { const FVector V = B->GetBallVelocity(); B->SetPlanarVelocity(FVector(V.X, V.Y, 0.f) * ZoogiLaunchPads::LandingPlanarKeep); B->Squash(-0.34f, FVector::UpVector); B->Wobble(6.f, FVector(V.X, V.Y, 0.f).GetSafeNormal()); AZoogiFX::KFX(GetWorld(), TEXT("Effect6_Collision"), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), 0.4f, 1.2f); } /* Oct 24: toon touchdown (visual only) */ UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: P%d landed (%s) after %.2f s at (%.0f, %.0f, %.0f) r=%.0f; ring-out checks back on"), P + 1, bGround ? TEXT("ground contact") : TEXT("timeout"), AirT, B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z, FVector2D(B->BallLocation()).Size()); }
	}
}

bool AZoogiGameMode::PadArcObstacleSweep(const FVector& Start, const FVector& End, FHitResult& OutHit, const AZoogiBall* Self) const
{
	UWorld* W = GetWorld();
	if (!W) return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiPadArc), false, Self);
	if (const AZoogiLaunchPad* Pad = Arena ? Arena->GetLaunchPad() : nullptr) Q.AddIgnoredActor(Pad);
	for (AZoogiBall* O : Balls) if (O) Q.AddIgnoredActor(O); // launcher + parked + others
	FCollisionObjectQueryParams OQ;
	OQ.AddObjectTypesToQuery(ECC_WorldStatic);
	OQ.AddObjectTypesToQuery(ECC_WorldDynamic); // arena pieces (incl. ZoogiArenaPiece_*)
	TArray<FHitResult> Hits;
	if (!W->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, OQ, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius), Q)) return false;
	for (const FHitResult& H : Hits)
	{
		if (H.bBlockingHit && H.Normal.Z < 0.7f) { OutHit = H; return true; } // ignore floors
	}
	return false;
}

bool AZoogiGameMode::IsPadAirborne(const AZoogiBall* B) const
{
	return B && LaunchFlow.IsPadAirborne(B->PlayerIndex);
}
