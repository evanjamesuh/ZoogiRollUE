// Oct 14 (Evan): fantasy card layer (see ZoogiCardFantasy.h). Methods of UZoogiCardSubsystem; every entry is a no-op unless
// FanOn() (cards mode, [ZoogiRoll.CardFantasy] bOn, no scripted card test running).
#include "ZoogiCardFantasy.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const FName KCStorm(TEXT("storm")), KCBank(TEXT("bank_strike")), KCGuard(TEXT("guard")), KCRelay(TEXT("orb_relay")), KCHeavy(TEXT("heavy_roll")),
		KCEmber(TEXT("ember")), KCTitan(TEXT("titan")), KCFrost(TEXT("frost")), KCPrecision(TEXT("precision")), KCSteal(TEXT("momentum_steal")),
		KCCrystal(TEXT("crystal_collapse")), KCMagnet(TEXT("orb_magnet")), KCDouble(TEXT("double_impact")), KCAnchor(TEXT("anchor_guard")), KCRecovery(TEXT("recovery_roll"));
	const FLinearColor CStorm(1.f, 0.9f, 0.25f), CIce(0.55f, 0.85f, 1.f), CFire(1.f, 0.42f, 0.08f), CVoid(0.7f, 0.35f, 1.f), CShield(0.3f, 0.8f, 1.f),
		CGold(1.f, 0.75f, 0.2f), CGreen(0.35f, 1.f, 0.55f), CShadow(0.25f, 0.08f, 0.45f), CStone(0.75f, 0.6f, 0.45f);

	FVector2D Flat(const FVector& V) { return FVector2D(V.X, V.Y); }
	void AddVel(AZoogiBall* B, const FVector2D& Dir, float Speed, float Up = 0.f)
	{
		if (!B || !B->Sphere || B->bOut) return;
		B->Sphere->SetPhysicsLinearVelocity(B->GetBallVelocity() + FVector(Dir.X * Speed, Dir.Y * Speed, Up));
	}
	// Chain arrow trail (Evan's gameplay-system kit style: golden 0xffda8d aim arrow + pulsing aim ring at the target).
	// Swap point: set [ZoogiRoll.CardFantasy] ChainArrowFX to a Niagara system path to add that system along the arrow.
	const FLinearColor CArrow(1.f, 0.855f, 0.553f);
	void ChainArrow(UWorld* W, FVector A, FVector B, const FZoogiCardFantasyTune& T)
	{
		if (!W) return;
		A.Z += 40.f; B.Z += 40.f;
		const FVector D = B - A; const float Len = D.Size2D();
		if (Len < 30.f) return;
		const FVector Dir = FVector(D.X, D.Y, 0.f) / Len;
		const FVector Tip = B - Dir * (ZoogiConst::ZoogiRadius * 0.9f);
		const int32 N = FMath::Clamp(FMath::CeilToInt(Len / 30.f), 3, 80);
		for (int32 i = 0; i <= N; ++i) AZoogiFX::SpawnTrail(W, FMath::Lerp(A, Tip, (float)i / N), CArrow, 34.f, T.ChainArrowTime, 12.f);
		for (const float Ang : { 152.f, -152.f })
		{
			const FVector Back = Dir.RotateAngleAxis(Ang, FVector::UpVector);
			for (int32 i = 1; i <= 4; ++i) AZoogiFX::SpawnTrail(W, Tip + Back * (26.f * i), CArrow, 36.f, T.ChainArrowTime, 12.f);
		}
		AZoogiFX::Spawn(W, B - FVector(0, 0, 35.f), CArrow, ZoogiConst::ZoogiRadius * 1.8f, T.ChainArrowTime, true, 12.f);
		if (!T.ChainArrowFX.IsEmpty()) AZoogiFX::Niagara(W, *T.ChainArrowFX, Tip, Dir.Rotation());
	}
	void Beam(UWorld* W, const FVector& A, const FVector& B, const FLinearColor& C, int32 N = 8, float R = 38.f)
	{
		for (int32 i = 0; i <= N; ++i) AZoogiFX::SpawnTrail(W, FMath::Lerp(A, B, (float)i / N) + FVector(0, 0, 25.f), C, R, 0.45f, 5.f);
	}
}

void FZoogiCardFantasyTune::Load()
{
	if (!GConfig) return;
	const TCHAR* S = TEXT("ZoogiRoll.CardFantasy");
	auto F = [&](const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };
	auto I = [&](const TCHAR* K, int32& V) { GConfig->GetInt(S, K, V, GGameIni); };
	GConfig->GetBool(S, TEXT("bOn"), bOn, GGameIni);
	F(TEXT("PowerAmp"), PowerAmp);
	I(TEXT("StormLinks"), StormLinks); F(TEXT("StormChainR"), StormChainR); F(TEXT("StormPush"), StormPush);
	F(TEXT("BankStep"), BankStep); F(TEXT("BankMaxSpeed"), BankMaxSpeed);
	F(TEXT("GuardReflect"), GuardReflect);
	F(TEXT("RelayRange"), RelayRange); F(TEXT("RelayPull"), RelayPull); F(TEXT("RelayTime"), RelayTime);
	F(TEXT("HeavyScale"), HeavyScale); F(TEXT("HeavyMass"), HeavyMass); F(TEXT("HeavyScatterR"), HeavyScatterR); F(TEXT("HeavyScatterPush"), HeavyScatterPush);
	I(TEXT("EmberCount"), EmberCount); F(TEXT("EmberSpreadDeg"), EmberSpreadDeg); F(TEXT("EmberSpeed"), EmberSpeed); F(TEXT("EmberLife"), EmberLife); F(TEXT("EmberHitPush"), EmberHitPush); F(TEXT("EmberPatchR"), EmberPatchR);
	F(TEXT("TitanMeteorR"), TitanMeteorR); F(TEXT("TitanMeteorPush"), TitanMeteorPush); F(TEXT("TitanDelay"), TitanDelay);
	F(TEXT("FrostRange"), FrostRange); F(TEXT("FrostConeDeg"), FrostConeDeg); I(TEXT("FrostTurns"), FrostTurns);
	F(TEXT("PrecisionConeDeg"), PrecisionConeDeg); F(TEXT("PrecisionTurnDeg"), PrecisionTurnDeg); I(TEXT("PrecisionRedirects"), PrecisionRedirects);
	F(TEXT("StealRange"), StealRange); F(TEXT("StealTime"), StealTime); F(TEXT("StealRate"), StealRate); F(TEXT("StealOnHit"), StealOnHit);
	F(TEXT("CrystalSpikeR"), CrystalSpikeR); F(TEXT("CrystalSpikePush"), CrystalSpikePush);
	F(TEXT("MagnetR"), MagnetR); F(TEXT("MagnetTime"), MagnetTime); F(TEXT("MagnetAccel"), MagnetAccel); F(TEXT("MagnetSwirl"), MagnetSwirl);
	F(TEXT("DoubleCloneHit"), DoubleCloneHit);
	F(TEXT("ChainArrowTime"), ChainArrowTime); GConfig->GetString(S, TEXT("ChainArrowFX"), ChainArrowFX, GGameIni);
	F(TEXT("AnchorMass"), AnchorMass); F(TEXT("AnchorPushK"), AnchorPushK);
	F(TEXT("RecoveryDash"), RecoveryDash); F(TEXT("RecoveryUp"), RecoveryUp);
}

bool UZoogiCardSubsystem::FanOn() const { return Tune.bCards && FanTune.bOn && TestCase.IsEmpty(); }
float UZoogiCardSubsystem::FanAmp(const AZoogiBall* B, FName Card) const { return PowerRider(B, Card) ? FanTune.PowerAmp : 1.f; }

void UZoogiCardSubsystem::FanShot(AZoogiGameMode* G, FName Card, const TCHAR* Suffix)
{
	// stagger requests: two shots asked for in the same frame would share one capture slot and the first would be lost
	static float NextShotT = -1.f;
	if (!G || !GetWorld()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (NextShotT > Now + 5.f) NextShotT = -1.f; // new world / match
	const float Delay = FMath::Max(0.12f, NextShotT - Now);
	NextShotT = Now + Delay + 0.3f;
	G->TakeShotDelayed(FString::Printf(TEXT("cardfx_%s%s"), *Card.ToString(), Suffix), Delay);
}

int32 UZoogiCardSubsystem::CPUCardIntent(int32 P) const
{
	if (!FanOn()) return 0;
	if (Fx.Find(KCTitan, P)) return 2; // meteor lands where the shot stops: stop on a rival
	for (FName N : { KCStorm, KCFrost, KCPrecision, KCSteal, KCDouble, KCHeavy, KCEmber }) if (Fx.Find(N, P)) return 1; // wants a rival contact / line
	return 0;
}

// ------------------------------------------------------------------ arm / standby / settle

void UZoogiCardSubsystem::FanOnArm(AZoogiGameMode* G, int32 P, FName Card)
{
	if (!FanOn() || !G || !G->Balls.IsValidIndex(P) || !G->Balls[P]) return;
	AZoogiBall* B = G->Balls[P];
	UWorld* W = GetWorld();
	if (Card == KCGuard)
	{
		AZoogiFX::Spawn(W, B->BallLocation(), CShield, ZoogiConst::ZoogiRadius * 1.9f, 14.f, false, 3.f, B);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), B->BallLocation());
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy guard P%d: shield dome up (reflects the next rammer x%.2f)"), P + 1, FanTune.GuardReflect);
		FanShot(G, Card);
	}
	else if (Card == KCRecovery)
	{
		Fan.Recovery.Add(P, false);
		AZoogiFX::Spawn(W, B->BallLocation(), CGreen, ZoogiConst::ZoogiRadius * 1.6f, 1.2f, true, 4.f, B);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy recovery_roll P%d: jet pack ready until P%d's next turn (%s)"), P + 1, P + 1, B->bCPU ? TEXT("CPU dashes automatically") : TEXT("tap to dash when flying off"));
	}
}

void UZoogiCardSubsystem::FanAnchorArmed(AZoogiGameMode* G, int32 P)
{
	if (!FanOn() || !G || !G->Balls.IsValidIndex(P) || !G->Balls[P] || !G->Balls[P]->Sphere || Fan.Anchors.Contains(P)) return;
	AZoogiBall* B = G->Balls[P];
	const float M = B->Sphere->GetMass();
	Fan.Anchors.Add(P, M);
	B->Sphere->SetMassOverrideInKg(NAME_None, M * FanTune.AnchorMass, true);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), CGold, ZoogiConst::ZoogiRadius * 1.7f, 18.f, true, 4.f, B);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Gold"), B->BallLocation());
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy anchor_guard P%d: anchored for a full round (mass %.2f -> %.2f kg, received push x%.2f)"), P + 1, M, B->Sphere->GetMass(), FanTune.AnchorPushK);
	FanShot(G, KCAnchor);
}

void UZoogiCardSubsystem::FanStandby(AZoogiGameMode* G, int32 P)
{
	if (!G) return;
	if (const float* M = Fan.Anchors.Find(P))
	{
		if (G->Balls.IsValidIndex(P) && G->Balls[P] && G->Balls[P]->Sphere) G->Balls[P]->Sphere->SetMassOverrideInKg(NAME_None, *M, true);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy anchor_guard P%d: round over, mass restored %.2f kg"), P + 1, *M);
		Fan.Anchors.Remove(P);
	}
	Fan.Recovery.Remove(P);
	if (Fan.RecoveryPrompt == P) Fan.RecoveryPrompt = -1;
	Fan.Patches.RemoveAll([P](const FZoogiCardFantasyState::FPatch& X) { return X.Owner == P; });
}

void UZoogiCardSubsystem::FanReset()
{
	Fan.ResetShot(); Fan.Patches.Reset(); Fan.Anchors.Reset(); Fan.Recovery.Reset(); Fan.RecoveryPrompt = -1; Fan.HeavyOwner = -1;
}

// ------------------------------------------------------------------ shot

void UZoogiCardSubsystem::FanOnShot(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V)
{
	if (!FanOn() || !G || !Ball || !Ball->Sphere) return;
	Fan.ResetShot();
	Fan.BankPrev = Ball->BallLocation();
	UWorld* W = GetWorld();
	const int32 P = Ball->PlayerIndex;
	const FVector Loc = Ball->BallLocation();
	const FVector2D Dir = V.GetSafeNormal();
	auto NearestRival = [&](float Range, float ConeDeg, float& OutD) -> AZoogiBall*
	{
		AZoogiBall* Best = nullptr; OutD = Range;
		for (AZoogiBall* O : G->Balls)
		{
			if (!O || O == Ball || O->bOut || O->bKnockedOut || !IsEnemy(G, Ball, O)) continue;
			const FVector2D To = Flat(O->BallLocation() - Loc); const float D = To.Size();
			if (D < 1.f || D > OutD) continue;
			if (ConeDeg < 180.f && FVector2D::DotProduct(To / D, Dir) < FMath::Cos(FMath::DegreesToRadians(ConeDeg))) continue;
			Best = O; OutD = D;
		}
		return Best;
	};
	if (Fx.Find(KCHeavy, P))
	{
		Fan.HeavyOwner = P; Fan.HeavySavedMass = Ball->Sphere->GetMass(); Fan.HeavySavedScale = Ball->GetModelScale();
		Ball->Sphere->SetMassOverrideInKg(NAME_None, Fan.HeavySavedMass * FanTune.HeavyMass, true);
		Ball->SetModelScale(Fan.HeavySavedScale * FanTune.HeavyScale);
		AZoogiFX::Niagara(W, TEXT("NS_Dust"), Loc, FRotator::ZeroRotator, 2.f);
		AZoogiFX::Spawn(W, Loc, CStone, 320.f, 0.6f, true, 5.f);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy heavy_roll P%d: BOULDER (model x%.1f, mass %.2f -> %.2f kg), scatters everyone in its path"), P + 1, FanTune.HeavyScale, Fan.HeavySavedMass, Ball->Sphere->GetMass());
		FanShot(G, KCHeavy);
	}
	if (Fx.Find(KCEmber, P))
	{
		const float Amp = FanAmp(Ball, KCEmber);
		const int32 N = FMath::Max(1, FMath::RoundToInt(FanTune.EmberCount * Amp));
		for (int32 i = 0; i < N; ++i)
		{
			const float A = N > 1 ? FMath::Lerp(-FanTune.EmberSpreadDeg, FanTune.EmberSpreadDeg, (float)i / (N - 1)) : 0.f;
			const FVector2D D = Dir.GetRotated(A);
			FZoogiCardFantasyState::FFireball F; F.P = Loc + FVector(D * 90.f, 0.f); F.V = FVector(D * FanTune.EmberSpeed, 0.f); F.Life = FanTune.EmberLife; F.Owner = P;
			Fan.Fireballs.Add(F);
		}
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), Loc);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy ember P%d: %d fireballs in a %.0f deg spread (burning patches where they land)"), P + 1, N, FanTune.EmberSpreadDeg * 2.f);
		FanShot(G, KCEmber);
	}
	if (Fx.Find(KCTitan, P) && G->Arena)
	{
		const float Amp = FanAmp(Ball, KCTitan);
		const float Roll = V.SizeSquared() / (2.f * FMath::Max(G->Arena->GetRollingDecel(), 1.f));
		const float Edge = G->Arena->DistanceToEdge(Flat(Loc), Dir);
		const float D = FMath::Clamp(Roll, 0.f, FMath::Max(0.f, Edge - 60.f));
		Fan.bMeteor = true; Fan.MeteorAt = Loc + FVector(Dir * D, 0.f); Fan.MeteorT = 0.f; Fan.MeteorR = FanTune.TitanMeteorR * Amp; Fan.MeteorPush = FanTune.TitanMeteorPush * Amp; Fan.MeteorOwner = P;
		AZoogiFX::Spawn(W, Fan.MeteorAt, FLinearColor(1.f, 0.2f, 0.1f), Fan.MeteorR, FanTune.TitanDelay, true, 3.f);
		AZoogiFX::Spawn(W, Fan.MeteorAt + FVector(0, 0, 900.f), FLinearColor(1.f, 0.5f, 0.15f), 140.f, FanTune.TitanDelay, false, 8.f);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy titan P%d: meteor called on the aim point (%.0f, %.0f), ring %.0f cm"), P + 1, Fan.MeteorAt.X, Fan.MeteorAt.Y, Fan.MeteorR);
	}
	if (Fx.Find(KCFrost, P) || FanFrostP == P) // the frost lane (FxAfterShot) consumes the entry before this runs
	{
		FanFrostP = -1;
		float D = 0.f;
		AZoogiBall* T = NearestRival(FanTune.FrostRange, FanTune.FrostConeDeg, D);
		if (!T) T = NearestRival(FanTune.FrostRange, 360.f, D);
		if (T)
		{
			const int32 Turns = FanTune.FrostTurns + (PowerRider(Ball, KCFrost) ? 1 : 0);
			T->StunTurns = FMath::Max(T->StunTurns, Turns);
			if (T->Sphere) T->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Beam(W, Loc, T->BallLocation(), CIce, 12, 45.f);
			AZoogiFX::Spawn(W, T->BallLocation(), CIce, ZoogiConst::ZoogiRadius * 1.35f, 8.f, false, 4.f, T);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), T->BallLocation());
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy frost P%d: ice beam froze P%d %s for %d turn(s) (%.0f cm)"), P + 1, T->PlayerIndex + 1, *T->Def.Name, Turns, D);
			FanShot(G, KCFrost);
		}
		else UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy frost P%d: no rival within %.0f cm for the ice beam"), P + 1, FanTune.FrostRange);
	}
	if (Fx.Find(KCPrecision, P))
	{
		float D = 0.f;
		AZoogiBall* T = NearestRival(3000.f, FanTune.PrecisionConeDeg, D);
		const float Amp = FanAmp(Ball, KCPrecision);
		Fan.HomingOwner = P; Fan.HomingTarget = T; Fan.RedirectsLeft = FanTune.PrecisionRedirects + (Amp > 1.f ? 1 : 0); Fan.HomingTurn = FanTune.PrecisionTurnDeg * Amp;
		if (T) AZoogiFX::Spawn(W, T->BallLocation(), CVoid, 150.f, 1.f, true, 4.f, T);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy precision P%d: homing shot%s (%.0f deg/s), %d redirect tap(s)"), P + 1, T ? *FString::Printf(TEXT(" locked on P%d"), T->PlayerIndex + 1) : TEXT(" (no rival in the cone yet)"), Fan.HomingTurn, Fan.RedirectsLeft);
		FanShot(G, KCPrecision);
	}
	if (Fx.Find(KCSteal, P))
	{
		float D = 0.f;
		if (AZoogiBall* T = NearestRival(FanTune.StealRange, 360.f, D))
		{
			Fan.TetherRival = T; Fan.TetherOwner = P; Fan.TetherT = 0.f; Fan.BeamT = 0.f;
			ChainArrow(W, T->BallLocation(), Loc, FanTune);
			Beam(W, Loc, T->BallLocation(), CGreen, 10);
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy momentum_steal P%d: tether on P%d (%.0f cm), drains its speed for %.1f s"), P + 1, T->PlayerIndex + 1, D, FanTune.StealTime);
			FanShot(G, KCSteal);
		}
	}
	if (Fx.Find(KCRelay, P))
	{
		AZoogiBall* To = G->bComboMode ? G->PartnerOf(Ball) : Ball;
		if (To && (To->bOut || To->bKnockedOut)) To = Ball;
		AZoogiOrb* Best = nullptr; float BD = FanTune.RelayRange;
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), Loc); if (D < BD) { BD = D; Best = O; } }
		if (Best && To)
		{
			Fan.RelayOrb = Best; Fan.RelayTo = To; Fan.RelayT = 0.f;
			ChainArrow(W, Loc, Best->OrbLocation(), FanTune); ChainArrow(W, Best->OrbLocation(), To->BallLocation(), FanTune);
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy orb_relay P%d: beam pulls the nearest orb (%.0f cm) to P%d"), P + 1, BD, To->PlayerIndex + 1);
			FanShot(G, KCRelay);
		}
	}
	if (Fx.Find(KCMagnet, P))
	{
		Fan.VortexOwner = P; Fan.VortexT = 0.f;
		AZoogiFX::Spawn(W, Loc, CVoid, FanTune.MagnetR, FanTune.MagnetTime, true, 2.5f, Ball);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy orb_magnet P%d: vortex pulls every orb within %.0f cm for %.1f s"), P + 1, FanTune.MagnetR, FanTune.MagnetTime);
		FanShot(G, KCMagnet);
	}
	if (Fx.Find(KCDouble, P))
	{
		AZoogiFX::Spawn(W, Loc, CShadow, ZoogiConst::ZoogiRadius * 1.1f, 2.5f, false, 2.5f, Ball);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy double_impact P%d: shadow clone follows (second hit x%.2f)"), P + 1, FanTune.DoubleCloneHit);
	}
	if (Fx.Find(KCStorm, P)) AZoogiFX::Spawn(W, Loc, CStorm, ZoogiConst::ZoogiRadius * 1.5f, 1.5f, false, 5.f, Ball);
	if (Fx.Find(KCBank, P)) AZoogiFX::Spawn(W, Loc, CGold, ZoogiConst::ZoogiRadius * 1.4f, 1.5f, true, 4.f, Ball);
	if (CollapseOwner == P && CollapseEffectId != 0) AZoogiFX::Spawn(W, Loc, CVoid, ZoogiConst::ZoogiRadius * 1.4f, 1.5f, false, 4.f, Ball);
}

// ------------------------------------------------------------------ contacts

void UZoogiCardSubsystem::FanBallContact(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* Att, const FHitResult& Hit)
{
	if (!FanOn() || !G || !Victim || !Att) return;
	UWorld* W = GetWorld();
	const int32 A = Att->PlayerIndex;
	if (Fx.Find(KCStorm, A) && !Fan.bStormDone)
	{
		Fan.bStormDone = true;
		const float Amp = FanAmp(Att, KCStorm);
		const int32 Links = FanTune.StormLinks + (Amp > 1.f ? 1 : 0);
		TSet<AZoogiBall*> Hitset;
		AZoogiBall* Cur = Victim; FVector From = Att->BallLocation();
		int32 N = 0;
		while (Cur && N < Links)
		{
			Hitset.Add(Cur); ++N;
			FVector2D Away = Flat(Cur->BallLocation() - Att->BallLocation()).GetSafeNormal();
			if (Away.IsNearlyZero()) Away = FVector2D(1.f, 0.f);
			AddVel(Cur, Away, FanTune.StormPush * Amp, 150.f); Cur->LastHitPlayer = A;
			Beam(W, From + FVector(0, 0, 60.f), Cur->BallLocation() + FVector(0, 0, 60.f), CStorm, 10, 32.f);
			ChainArrow(W, From, Cur->BallLocation(), FanTune);
			AZoogiFX::Niagara(W, TEXT("NS_Sparks"), Cur->BallLocation());
			AZoogiFX::Spawn(W, Cur->BallLocation(), CStorm, 160.f, 0.35f, false, 8.f);
			From = Cur->BallLocation();
			AZoogiBall* Next = nullptr; float BD = FanTune.StormChainR;
			for (AZoogiBall* O : G->Balls)
				if (O && !Hitset.Contains(O) && O != Att && !O->bOut && IsEnemy(G, Att, O)) { const float D = FVector::Dist2D(O->BallLocation(), From); if (D < BD) { BD = D; Next = O; } }
			Cur = Next;
		}
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy storm P%d: chain lightning hit %d rival(s), knockback %.0f"), A + 1, N, FanTune.StormPush * Amp);
		FanShot(G, KCStorm);
	}
	if (CollapseOwner == A && CollapseEffectId != 0 && !Fan.bSpikesDone) FanSpikes(G, Att, Hit.ImpactPoint.IsNearlyZero() ? Victim->BallLocation() : FVector(Hit.ImpactPoint));
	if (Fan.TetherOwner == A && Fan.TetherRival.Get() == Victim && Victim->Sphere && Att->Sphere)
	{
		const FVector VV = Victim->GetBallVelocity();
		const FVector2D Drain = Flat(VV) * FanTune.StealOnHit;
		Victim->Sphere->SetPhysicsLinearVelocity(VV - FVector(Drain, 0.f));
		const FVector2D AD = Flat(Att->GetBallVelocity()).GetSafeNormal();
		AddVel(Att, AD.IsNearlyZero() ? Drain.GetSafeNormal() : AD, Drain.Size());
		AZoogiFX::Spawn(W, Att->BallLocation(), CGreen, 180.f, 0.4f, true, 5.f);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy momentum_steal P%d: drained %.0f cm/s from P%d on contact"), A + 1, Drain.Size(), Victim->PlayerIndex + 1);
	}
	if (Fan.HomingOwner == A) { Fan.HomingTarget.Reset(); Fan.bHomingPoint = false; }
}

void UZoogiCardSubsystem::FanGuardReflect(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* Att)
{
	if (!FanOn() || !Victim || !Att || !Att->Sphere) return;
	const FVector In = Prev(Att);
	Att->Sphere->SetPhysicsLinearVelocity(FVector(-In.X * FanTune.GuardReflect, -In.Y * FanTune.GuardReflect, FMath::Max(In.Z, 0.f) + 120.f));
	AZoogiFX::Spawn(GetWorld(), Victim->BallLocation(), CShield, ZoogiConst::ZoogiRadius * 2.4f, 0.5f, false, 9.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Blue"), Victim->BallLocation(), FRotator::ZeroRotator, 1.6f);
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy guard P%d: dome REFLECTED P%d back at %.0f cm/s"), Victim->PlayerIndex + 1, Att->PlayerIndex + 1, In.Size2D() * FanTune.GuardReflect);
	FanShot(G, KCGuard, TEXT("_reflect"));
}

void UZoogiCardSubsystem::FanWall(AZoogiGameMode* G, AZoogiBall* Ball, const FHitResult& Hit)
{
	if (!FanOn() || !G || !Ball || !Ball->Sphere || Ball->PlayerIndex != FxShooter || !Fx.Find(KCBank, Ball->PlayerIndex)) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - Fan.BankLastT < 0.15f) return;
	Fan.BankLastT = Now;
	++Fan.BankBounces;
	const FVector V = Ball->GetBallVelocity();
	const float Sp = V.Size2D();
	float NewSp = FMath::Min(Sp * (1.f + FanTune.BankStep * Fan.BankBounces), FMath::Max(Sp, FanTune.BankMaxSpeed));
	// CPU planner guard: a CPU's pinball speed-up never carries it past the ring edge on its new line
	if (Ball->bCPU && G->Arena && Sp > 1.f)
	{
		const float Edge = G->Arena->DistanceToEdge(Flat(Ball->BallLocation()), Flat(V) / Sp);
		NewSp = FMath::Max(FMath::Min(NewSp, FMath::Sqrt(2.f * G->Arena->GetRollingDecel() * FMath::Max(0.f, Edge - 180.f))), FMath::Min(Sp, NewSp));
	}
	if (Sp > 1.f) Ball->Sphere->SetPhysicsLinearVelocity(FVector(V.X / Sp * NewSp, V.Y / Sp * NewSp, V.Z));
	ChainArrow(GetWorld(), Fan.BankPrev, Ball->BallLocation(), FanTune); Fan.BankPrev = Ball->BallLocation();
	AZoogiFX::Spawn(GetWorld(), Hit.ImpactPoint, CGold, 150.f + 40.f * Fan.BankBounces, 0.4f, true, 7.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Sparks"), Hit.ImpactPoint);
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy bank_strike P%d: pinball bounce %d, %.0f -> %.0f cm/s"), Ball->PlayerIndex + 1, Fan.BankBounces, Sp, NewSp);
	FanShot(G, KCBank);
}

void UZoogiCardSubsystem::FanSpikes(AZoogiGameMode* G, AZoogiBall* Owner, const FVector& At)
{
	if (!FanOn() || !G || !Owner || Fan.bSpikesDone) return;
	Fan.bSpikesDone = true;
	UWorld* W = GetWorld();
	const float Amp = FanAmp(Owner, KCCrystal);
	const float R = FanTune.CrystalSpikeR * Amp;
	int32 N = 0;
	for (AZoogiBall* O : G->Balls)
	{
		if (!O || O == Owner || O->bOut || O->bKnockedOut) continue;
		const FVector2D D = Flat(O->BallLocation() - At); const float Dist = D.Size();
		if (Dist > R) continue;
		AddVel(O, Dist > 1.f ? D / Dist : FVector2D(1.f, 0.f), FanTune.CrystalSpikePush * (0.5f + 0.5f * (1.f - Dist / R)), 250.f); O->LastHitPlayer = Owner->PlayerIndex;
		++N;
	}
	for (int32 i = 0; i < 10; ++i) { const FVector2D D = FVector2D(1.f, 0.f).GetRotated(36.f * i); AZoogiFX::Spawn(W, At + FVector(D * R * 0.55f, 30.f), CVoid, 70.f, 0.9f, false, 6.f); }
	AZoogiFX::Spawn(W, At, CVoid, R, 0.6f, true, 6.f);
	AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), At, FRotator::ZeroRotator, 1.5f);
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy crystal_collapse P%d: crystal spikes burst (r %.0f) knocked %d Zoogi(s) away (no extra destruction)"), Owner->PlayerIndex + 1, R, N);
	FanShot(G, KCCrystal, TEXT("_spikes"));
}

void UZoogiCardSubsystem::FanCloneHit(AZoogiGameMode* G, AZoogiBall* Victim)
{
	if (!FanOn() || !Victim) return;
	AZoogiFX::Spawn(GetWorld(), Victim->BallLocation(), CShadow, ZoogiConst::ZoogiRadius * 2.f, 0.5f, false, 7.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Purple"), Victim->BallLocation());
	if (G && G->Balls.IsValidIndex(FxShooter) && G->Balls[FxShooter]) ChainArrow(GetWorld(), G->Balls[FxShooter]->BallLocation(), Victim->BallLocation(), FanTune);
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy double_impact: shadow clone strikes P%d"), Victim->PlayerIndex + 1);
	FanShot(G, KCDouble);
}

// ------------------------------------------------------------------ input (human taps)

bool UZoogiCardSubsystem::FantasyTap(AZoogiGameMode* G, const FVector& World)
{
	if (!FanOn() || !G) return false;
	if (G->Balls.IsValidIndex(Fan.RecoveryPrompt) && G->Balls[Fan.RecoveryPrompt] && !G->Balls[Fan.RecoveryPrompt]->bCPU)
	{
		FanDash(G, G->Balls[Fan.RecoveryPrompt]);
		return true;
	}
	if (G->Phase == EZoogiPhase::Rolling && G->Balls.IsValidIndex(Fan.HomingOwner) && G->Balls[Fan.HomingOwner] && !G->Balls[Fan.HomingOwner]->bCPU && Fan.RedirectsLeft > 0)
	{
		AZoogiBall* Me = G->Balls[Fan.HomingOwner];
		AZoogiBall* Best = nullptr; float BD = 400.f;
		for (AZoogiBall* O : G->Balls) if (O && O != Me && !O->bOut) { const float D = FVector::Dist2D(O->BallLocation(), World); if (D < BD) { BD = D; Best = O; } }
		Fan.HomingTarget = Best; Fan.bHomingPoint = Best == nullptr; Fan.HomingPoint = World;
		--Fan.RedirectsLeft;
		AZoogiFX::Spawn(GetWorld(), World, CVoid, 140.f, 0.5f, true, 6.f);
		ChainArrow(GetWorld(), Me->BallLocation(), Best ? Best->BallLocation() : World, FanTune);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy precision P%d: redirect -> %s (%d left)"), Me->PlayerIndex + 1, Best ? *FString::Printf(TEXT("P%d"), Best->PlayerIndex + 1) : *FString::Printf(TEXT("point (%.0f, %.0f)"), World.X, World.Y), Fan.RedirectsLeft);
		return true;
	}
	return false;
}

void UZoogiCardSubsystem::FanDash(AZoogiGameMode* G, AZoogiBall* B)
{
	if (!B || !B->Sphere || B->bOut) return;
	bool* Used = Fan.Recovery.Find(B->PlayerIndex);
	if (!Used || *Used) return;
	*Used = true;
	if (Fan.RecoveryPrompt == B->PlayerIndex) Fan.RecoveryPrompt = -1;
	const FVector2D ToC = (-Flat(B->BallLocation())).GetSafeNormal();
	const float Amp = FanAmp(B, KCRecovery);
	B->Sphere->SetPhysicsLinearVelocity(FVector(ToC * FanTune.RecoveryDash * Amp, FanTune.RecoveryUp));
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), CGreen, 220.f, 0.5f, false, 8.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Green"), B->BallLocation());
	UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy recovery_roll P%d: mid-air DASH back toward the centre (%.0f cm/s)"), B->PlayerIndex + 1, FanTune.RecoveryDash * Amp);
	FanShot(G, KCRecovery);
}

// ------------------------------------------------------------------ tick

void UZoogiCardSubsystem::FanTick(AZoogiGameMode* G, float Dt)
{
	if (!FanOn() || !G) return;
	UWorld* W = GetWorld();
	// Recovery Roll: flying off the edge -> CPU dashes, a human gets a tap prompt
	for (TPair<int32, bool>& R : Fan.Recovery)
	{
		if (R.Value || !G->Balls.IsValidIndex(R.Key)) continue;
		AZoogiBall* B = G->Balls[R.Key];
		if (!B || B->bOut || !G->Arena) continue;
		const bool bOff = G->Arena->IsOffCourt(Flat(B->BallLocation()));
		if (bOff && B->bCPU) FanDash(G, B);
		else if (bOff && Fan.RecoveryPrompt != R.Key) { Fan.RecoveryPrompt = R.Key; G->CardPopup(TEXT("TAP TO DASH!"), B->BallLocation() + FVector(0, 0, 160.f), CGreen); UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy recovery_roll P%d: flying off - TAP TO DASH"), R.Key + 1); }
		else if (!bOff && Fan.RecoveryPrompt == R.Key) Fan.RecoveryPrompt = -1;
		if (R.Value) break; // FanDash changed the map value; stop iterating this frame
	}
	const bool bRolling = G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Rolling;
	if (!bRolling || !G->Balls.IsValidIndex(FxShooter) || !G->Balls[FxShooter]) return;
	AZoogiBall* S = G->Balls[FxShooter];
	const FVector SL = S->BallLocation();
	// Ember fireballs
	for (int32 i = Fan.Fireballs.Num() - 1; i >= 0; --i)
	{
		FZoogiCardFantasyState::FFireball& F = Fan.Fireballs[i];
		F.P += F.V * Dt; F.Life -= Dt; F.PuffT -= Dt;
		if (F.PuffT <= 0.f) { AZoogiFX::SpawnTrail(W, F.P, CFire, 60.f, 0.35f, 7.f); F.PuffT = 0.04f; }
		AZoogiBall* Owner = G->Balls.IsValidIndex(F.Owner) ? G->Balls[F.Owner].Get() : nullptr;
		AZoogiBall* Hit = nullptr;
		for (AZoogiBall* O : G->Balls) if (O && O != Owner && !O->bOut && (!Owner || IsEnemy(G, Owner, O)) && FVector::Dist2D(O->BallLocation(), F.P) < ZoogiConst::ZoogiRadius + 50.f) { Hit = O; break; }
		const bool bOff = G->Arena && G->Arena->IsOffCourt(Flat(F.P));
		if (Hit || F.Life <= 0.f || bOff)
		{
			if (Hit) { AddVel(Hit, Flat(F.V).GetSafeNormal(), FanTune.EmberHitPush * FanAmp(Owner, KCEmber), 120.f); Hit->LastHitPlayer = F.Owner; UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy ember: fireball hit P%d"), Hit->PlayerIndex + 1); }
			AZoogiFX::Niagara(W, TEXT("NS_Explosion"), F.P, FRotator::ZeroRotator, 0.6f);
			if (!bOff)
			{
				FZoogiCardFantasyState::FPatch Pt; Pt.At = FVector(F.P.X, F.P.Y, SL.Z); Pt.R = FanTune.EmberPatchR; Pt.Owner = F.Owner;
				Fan.Patches.Add(Pt);
				AZoogiFX::Spawn(W, Pt.At, CFire, Pt.R, 6.f, true, 4.f);
			}
			Fan.Fireballs.RemoveAt(i);
		}
	}
	// burning patches: a rival rolling into one is Bound (weak next shot) and hops out
	for (FZoogiCardFantasyState::FPatch& Pt : Fan.Patches)
	{
		AZoogiBall* Owner = G->Balls.IsValidIndex(Pt.Owner) ? G->Balls[Pt.Owner].Get() : nullptr;
		for (AZoogiBall* O : G->Balls)
		{
			if (!O || O == Owner || O->bOut || Pt.Burned.Contains(O->PlayerIndex) || (Owner && !IsEnemy(G, Owner, O))) continue;
			if (FVector::Dist2D(O->BallLocation(), Pt.At) > Pt.R + ZoogiConst::ZoogiRadius * 0.5f) continue;
			Pt.Burned.Add(O->PlayerIndex);
			O->BoundShots = FMath::Max(O->BoundShots, 1);
			AddVel(O, Flat(O->BallLocation() - Pt.At).GetSafeNormal(), 350.f, 300.f); O->LastHitPlayer = Pt.Owner;
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), O->BallLocation());
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy ember: P%d burned on a patch (Bound)"), O->PlayerIndex + 1);
		}
	}
	// Titan meteor
	if (Fan.bMeteor)
	{
		Fan.MeteorT += Dt;
		if (Fan.MeteorT >= FanTune.TitanDelay)
		{
			Fan.bMeteor = false;
			int32 N = 0;
			for (AZoogiBall* O : G->Balls)
			{
				if (!O || O->PlayerIndex == Fan.MeteorOwner || O->bOut) continue;
				const FVector2D D = Flat(O->BallLocation() - Fan.MeteorAt); const float Dist = D.Size();
				if (Dist > Fan.MeteorR) continue;
				AddVel(O, Dist > 1.f ? D / Dist : FVector2D(1.f, 0.f), Fan.MeteorPush * (0.4f + 0.6f * (1.f - Dist / Fan.MeteorR)), 300.f); O->LastHitPlayer = Fan.MeteorOwner;
				++N;
			}
			AZoogiFX::Niagara(W, TEXT("NS_Explosion"), Fan.MeteorAt, FRotator::ZeroRotator, 2.2f);
			AZoogiFX::Spawn(W, Fan.MeteorAt, FLinearColor(1.f, 0.45f, 0.1f), Fan.MeteorR * 1.2f, 0.8f, true, 8.f);
			AZoogiFX::Spawn(W, Fan.MeteorAt, FLinearColor(1.f, 0.6f, 0.2f), 260.f, 0.5f, false, 10.f);
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy titan: METEOR impact at (%.0f, %.0f), shockwave hit %d Zoogi(s)"), Fan.MeteorAt.X, Fan.MeteorAt.Y, N);
			FanShot(G, KCTitan);
		}
	}
	// Precision homing
	if (Fan.HomingOwner == FxShooter && S->Sphere && (Fan.HomingTarget.IsValid() || Fan.bHomingPoint))
	{
		const FVector V = S->GetBallVelocity();
		const float Sp = V.Size2D();
		const FVector Goal = Fan.HomingTarget.IsValid() ? Fan.HomingTarget->GetActorLocation() : Fan.HomingPoint;
		const FVector2D To = Flat(Goal - SL);
		if (Sp > 60.f && To.Size() > 30.f)
		{
			const FVector2D Cur = Flat(V) / Sp;
			const float Want = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), Have = FMath::RadiansToDegrees(FMath::Atan2(Cur.Y, Cur.X));
			const float Delta = FMath::Clamp(FRotator::NormalizeAxis(Want - Have), -Fan.HomingTurn * Dt, Fan.HomingTurn * Dt);
			const FVector2D N = Cur.GetRotated(Delta) * Sp;
			S->Sphere->SetPhysicsLinearVelocity(FVector(N.X, N.Y, V.Z));
			Fan.BeamT -= Dt;
			if (Fan.BeamT <= 0.f) { AZoogiFX::SpawnTrail(W, SL, CVoid, 50.f, 0.4f, 6.f); Fan.BeamT = 0.06f; }
		}
	}
	// Momentum Steal tether
	if (Fan.TetherOwner == FxShooter && Fan.TetherRival.IsValid() && S->Sphere)
	{
		Fan.TetherT += Dt;
		AZoogiBall* R = Fan.TetherRival.Get();
		if (Fan.TetherT > FanTune.StealTime || R->bOut || !R->Sphere) Fan.TetherRival.Reset();
		else
		{
			const FVector RV = R->GetBallVelocity();
			const FVector2D Drain = Flat(RV) * FMath::Min(1.f, FanTune.StealRate * Dt);
			if (Drain.SizeSquared() > 1.f)
			{
				R->Sphere->SetPhysicsLinearVelocity(RV - FVector(Drain, 0.f));
				const FVector2D AD = Flat(S->GetBallVelocity()).GetSafeNormal();
				if (!AD.IsNearlyZero()) AddVel(S, AD, Drain.Size());
			}
			Fan.BeamT -= Dt;
			if (Fan.BeamT <= 0.f) { Beam(W, SL, R->BallLocation(), CGreen, 6, 30.f); Fan.BeamT = 0.12f; }
		}
	}
	// Orb Relay beam
	if (Fan.RelayT >= 0.f && Fan.RelayOrb.IsValid() && Fan.RelayTo.IsValid())
	{
		Fan.RelayT += Dt;
		AZoogiOrb* O = Fan.RelayOrb.Get();
		if (Fan.RelayT > FanTune.RelayTime || O->bOut || !O->Sphere) { Fan.RelayT = -1.f; Fan.RelayOrb.Reset(); }
		else
		{
			const FVector2D To = Flat(Fan.RelayTo->BallLocation() - O->OrbLocation());
			if (To.Size() > ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 20.f)
			{
				const FVector OV = O->Sphere->GetPhysicsLinearVelocity();
				const FVector2D N = To.GetSafeNormal() * FanTune.RelayPull;
				O->Sphere->SetPhysicsLinearVelocity(FVector(N.X, N.Y, OV.Z));
			}
			Fan.BeamT -= Dt;
			if (Fan.BeamT <= 0.f) { Beam(W, O->OrbLocation(), Fan.RelayTo->BallLocation(), CGold, 6, 28.f); Fan.BeamT = 0.12f; }
		}
	}
	// Orb Magnet vortex
	if (Fan.VortexOwner == FxShooter && Fan.VortexT >= 0.f)
	{
		Fan.VortexT += Dt;
		if (Fan.VortexT > FanTune.MagnetTime) Fan.VortexT = -1.f;
		else for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut || !O->Sphere) continue;
			const FVector2D D = Flat(SL - O->OrbLocation()); const float Dist = D.Size();
			if (Dist > FanTune.MagnetR || Dist < 60.f) continue;
			const FVector2D Rad = D / Dist, Tan(-Rad.Y, Rad.X);
			const FVector2D Add = (Rad + Tan * FanTune.MagnetSwirl) * FanTune.MagnetAccel * Dt;
			O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() + FVector(Add, 0.f));
		}
	}
	// Heavy Roll boulder: scatter rivals in the path
	if (Fan.HeavyOwner == FxShooter && S->GetBallVelocity().Size2D() > 200.f)
	{
		const FVector2D Fwd = Flat(S->GetBallVelocity()).GetSafeNormal();
		for (AZoogiBall* O : G->Balls)
		{
			if (!O || O == S || O->bOut || Fan.Scattered.Contains(O->PlayerIndex)) continue;
			const FVector2D D = Flat(O->BallLocation() - SL);
			if (D.Size() > FanTune.HeavyScatterR + ZoogiConst::ZoogiRadius) continue;
			Fan.Scattered.Add(O->PlayerIndex);
			const float Side = FVector2D::CrossProduct(Fwd, D) >= 0.f ? 1.f : -1.f;
			const FVector2D Lat(-Fwd.Y * Side, Fwd.X * Side);
			AddVel(O, (Lat * 0.8f + Fwd * 0.6f).GetSafeNormal(), FanTune.HeavyScatterPush, 200.f); O->LastHitPlayer = FxShooter;
			AZoogiFX::Niagara(W, TEXT("NS_Dust"), O->BallLocation(), FRotator::ZeroRotator, 1.5f);
			UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy heavy_roll: boulder scattered P%d"), O->PlayerIndex + 1);
		}
	}
}

void UZoogiCardSubsystem::FanLeadActionEnd(AZoogiGameMode* G)
{
	if (Fan.HeavyOwner >= 0 && G && G->Balls.IsValidIndex(Fan.HeavyOwner) && G->Balls[Fan.HeavyOwner])
	{
		AZoogiBall* B = G->Balls[Fan.HeavyOwner];
		if (B->Sphere) B->Sphere->SetMassOverrideInKg(NAME_None, Fan.HeavySavedMass, true);
		B->SetModelScale(Fan.HeavySavedScale);
		UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy heavy_roll P%d: boulder back to normal (mass %.2f kg)"), Fan.HeavyOwner + 1, Fan.HeavySavedMass);
	}
	Fan.HeavyOwner = -1;
	Fan.ResetShot();
}
