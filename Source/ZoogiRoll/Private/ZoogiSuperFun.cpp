// Oct 29 night (Evan, build 25): super power pack for the classic game - more members of UZoogiFunSys (ZoogiActionFun.h).
//  #1 SUPER meter (perfect taps, blocks, scoring, KOs, getting knocked off) -> each Zoogi's signature ULTIMATE (Wolf Pack stays
//     everyone's regular SUPERPOWER). Fired with the SUPERPOWER button while the shot rolls.
//  #2 mid-roll: the ultimate fires the moment you press; the aimed ones (Chain Ricochet, Homing Strike) drop into bullet time -
//     tap the target you want.
//  #3 POWER CLASH: a powered / burning Zoogi hitting a rival who has a charge (or half a meter) -> 1.5 s tap-off.
//  #4 arena damage: Meteor Strike / Thunder Chain blow igloo walls and the snowman apart, lightning throws the power orbs.
//  #5 AWAKENED: on fire + a full meter at turn start -> heavier, bigger, golden aura, harder hits for that turn.
//  #6 FUSION: an ultimate hitting a rival Zoogi fuses both Zoogis' elements into a burst (PLASMA = fire + lightning ...).
//  #7 ARENA EVENTS every second round: Meteor Shower, Low Gravity, Whirlwind, Power Surge.
//  #8 ULTIMATE FINISH: a knockout during an ultimate replays right away.
#include "ZoogiActionFun.h"
#include "ZoogiHaptics.h"
#include "HAL/PlatformTime.h"
#include "ZoogiRedirect.h"
#include "ZoogiBanter.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiClashSplit.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiSkills.h"
#include "ZoogiFront.h"
#include "ZoogiEpic.h"
#include "ZoogiStory.h"
namespace ZoogiDuelV2 { bool ReplacesClash(); }
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectIterator.h"
namespace ZoogiSuperTune
{
	constexpr float MeterFull = 100.f;
	constexpr float AimSecs = 1.6f;
	constexpr float AimDil = 0.15f;
}

EZoogiPower UZoogiFunSys::SignaturePower(const AZoogiBall* B)
{
	if (!B) return EZoogiPower::WolfPack;
	const FZoogiDef* D = ZoogiUtil::FindZoogi(B->Def.Id);
	const EZoogiPower P = D ? D->PowerKind : B->Def.PowerKind;
	// b66 (Evan): Wolf Pack is a SKILL only now - a Wolf Pack Zoogi's ultimate becomes the chain ricochet (-ZoogiWolfSuperpower = old)
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSuperpower"));
	return (P == EZoogiPower::WolfPack && !bOld) ? EZoogiPower::Ricochet : P;
}

const TCHAR* UZoogiFunSys::UltName(EZoogiPower P)
{
	switch (P)
	{
	case EZoogiPower::WolfPack: return TEXT("WOLF STAMPEDE");
	case EZoogiPower::Explosion: return TEXT("METEOR STRIKE");
	case EZoogiPower::Ricochet: return TEXT("CHAIN RICOCHET");
	case EZoogiPower::LockOn: return TEXT("HOMING STRIKE");
	case EZoogiPower::StaticShock: return TEXT("THUNDER CHAIN");
	case EZoogiPower::BandageBind: return TEXT("MUMMY WRAP");
	case EZoogiPower::ShadowStun: return TEXT("BLACKOUT");
	case EZoogiPower::RuneSeal: return TEXT("RUNE CAGE");
	}
	return TEXT("ULTIMATE");
}

static const TCHAR* ElementName(EZoogiPower P)
{
	switch (P)
	{
	case EZoogiPower::WolfPack: return TEXT("WILD");
	case EZoogiPower::Explosion: return TEXT("FIRE");
	case EZoogiPower::Ricochet: return TEXT("KINETIC");
	case EZoogiPower::LockOn: return TEXT("LASER");
	case EZoogiPower::StaticShock: return TEXT("LIGHTNING");
	case EZoogiPower::BandageBind: return TEXT("CURSE");
	case EZoogiPower::ShadowStun: return TEXT("SHADOW");
	case EZoogiPower::RuneSeal: return TEXT("RUNE");
	}
	return TEXT("POWER");
}

FLinearColor UZoogiFunSys::UltColor(EZoogiPower P)
{
	switch (P)
	{
	case EZoogiPower::WolfPack: return FLinearColor(0.45f, 0.65f, 1.f);
	case EZoogiPower::Explosion: return FLinearColor(1.f, 0.38f, 0.06f);
	case EZoogiPower::Ricochet: return FLinearColor(0.25f, 0.85f, 1.f);
	case EZoogiPower::LockOn: return FLinearColor(0.7f, 0.35f, 1.f);
	case EZoogiPower::StaticShock: return FLinearColor(1.f, 0.85f, 0.15f);
	case EZoogiPower::BandageBind: return FLinearColor(0.9f, 0.78f, 0.55f);
	case EZoogiPower::ShadowStun: return FLinearColor(0.45f, 0.15f, 0.95f);
	case EZoogiPower::RuneSeal: return FLinearColor(0.62f, 0.8f, 1.f);
	}
	return FLinearColor::White;
}

static FVector FunActorPos(const AActor* A)
{
	if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
	if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
	return A ? A->GetActorLocation() : FVector::ZeroVector;
}

static void FunPush(AActor* A, const FVector& Planar, float Up, int32 Credit)
{
	if (AZoogiBall* B = Cast<AZoogiBall>(A))
	{
		if (B->bOut || !B->Sphere || !B->Sphere->IsSimulatingPhysics()) return;
		const FVector V = B->GetBallVelocity();
		B->SetPlanarVelocity(FVector(V.X, V.Y, 0.f) + Planar, Up);
		if (Credit >= 0 && B->PlayerIndex != Credit) B->LastHitPlayer = Credit;
	}
	else if (AZoogiOrb* O = Cast<AZoogiOrb>(A))
	{
		if (O->bOut || !O->Sphere || !O->Sphere->IsSimulatingPhysics()) return;
		O->Sphere->WakeRigidBody();
		const FVector V = O->Sphere->GetPhysicsLinearVelocity();
		O->Sphere->SetPhysicsLinearVelocity(FVector(V.X + Planar.X, V.Y + Planar.Y, Up != 0.f ? Up : V.Z));
		if (Credit >= 0) O->LastHitBy = Credit;
	}
}

// ------------------------------------------------------------------ meter

float UZoogiFunSys::Meter(int32 Player) const { return Players.IsValidIndex(Player) ? Players[Player].Meter : 0.f; }

void UZoogiFunSys::AddMeter(AZoogiGameMode* G, int32 Player, float Amount, const TCHAR* Why)
{
	if (!Players.IsValidIndex(Player) || Amount == 0.f) return;
	static const bool bEasy = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEasyMeter"));
	if (Amount > 0.f && !bEasy) Amount = FMath::Min(Amount * 0.5f, 25.f); // b89 (Evan: "make filling the super meter harder"): half gain, at most 25 (a quarter) per event - one big score filled it; -ZoogiEasyMeter = full
	FPlayerFun& P = Players[Player];
	const float Was = P.Meter;
	P.Meter = FMath::Clamp(P.Meter + Amount, 0.f, ZoogiSuperTune::MeterFull);
	UE_LOG(LogZoogi, Log, TEXT("FUN meter P%d %+.0f (%s) -> %.0f"), Player + 1, Amount, Why, P.Meter);
	if (Was < ZoogiSuperTune::MeterFull && P.Meter >= ZoogiSuperTune::MeterFull && G && G->Balls.IsValidIndex(Player) && G->Balls[Player])
	{
		AZoogiBall* B = G->Balls[Player];
		const EZoogiPower Sig = SignaturePower(B);
		// b63: the Zoogi says it in its speech bubble at the top
		const ZoogiFront::FAwaken* AwR = ZoogiFront::AwakenFor(B, this); // b100: the Zoogi's Awaken power
		const FString Nm = AwR ? FString(AwR->Name) : FString(UltName(Sig));
		const FString LineReady = B->bCPU ? FString::Printf(TEXT("My %s is ready. Watch out!"), *Nm.ToLower()) : FString::Printf(TEXT("%s is charged - let's go!"), *Nm.Left(1).ToUpper().Append(Nm.Mid(1).ToLower()));
		if (ZoogiAwakenMeter())
		{	// b135: the AWAKEN meter
			if (!B->bCPU) Callout(TEXT("AWAKEN READY!"), FString::Printf(TEXT("%s  -  TAP SHOWTIME BEFORE YOUR SHOT"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);
			else Callout(FString::Printf(TEXT("%s'S AWAKEN IS READY"), *B->Def.Name.ToUpper()), Nm, UltColor(Sig), 1.3f, 0.7f, Player, LineReady);
		}
		else if (!B->bCPU) Callout(AwR ? TEXT("AWAKEN READY!") : TEXT("ULTIMATE READY!"), FString::Printf(TEXT("%s  -  TAP SUPERPOWER WHILE ROLLING"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);
		else Callout(FString::Printf(TEXT("%s'S %s IS READY"), *B->Def.Name.ToUpper(), AwR ? TEXT("AWAKEN") : TEXT("ULTIMATE")), Nm, UltColor(Sig), 1.3f, 0.7f, Player, LineReady);
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), UltColor(Sig), 180.f, 0.6f, true, 12.f, B);
	}
}

bool ZoogiAwakenMeter()
{
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeters"));
	return !bOld;
}

bool UZoogiFunSys::AwakenFull(int32 Player) const
{
	return Players.IsValidIndex(Player) && Players[Player].Meter >= ZoogiSuperTune::MeterFull;
}

void UZoogiFunSys::SpendAwaken(int32 Player)
{
	if (!Players.IsValidIndex(Player)) return;
	Players[Player].Meter = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("FUN AWAKEN meter P%d spent (SHOWTIME)"), Player + 1);
}

bool UZoogiFunSys::UltimateReady(const AZoogiGameMode* G, int32 Player) const
{
	if (ZoogiAwakenMeter()) return false; // b135: a full meter means SHOWTIME at aim, no mid-roll ultimate
	if (!IsActive(G) || !Players.IsValidIndex(Player) || Players[Player].Meter < ZoogiSuperTune::MeterFull) return false;
	if (Clash.bOn) return false; // b62 (Evan): no super while clashing
	if (bUltThisShot || bUltAim || G->ActivePlayer != Player || G->Phase != EZoogiPhase::Rolling || G->PhaseTime > 8.f) return false;
	const AZoogiBall* B = G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (!B || B->bOut || B->bParked || G->IsOnLaunchPad(B)) return false;
	// only once the Zoogi is in the arena (not still flying in from the launch pad / out on the outer ring)
	const FVector L = B->BallLocation();
	return !AZoogiArena::IsRingOut(L) && !(G->Arena && G->Arena->IsOffCourt(FVector2D(L.X, L.Y)));
}

// ------------------------------------------------------------------ ultimates

bool UZoogiFunSys::TryUltimate(AZoogiGameMode* G)
{
	if (!G) return false;
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !UltimateReady(G, B->PlayerIndex)) return false;
	EZoogiPower P = SignaturePower(B);
	// b100 (Evan): the Zoogi (ball) carries the Awaken power - Hotstreak METEOR STRIKE, the others one of the skills at full strength
	static const bool bOldUlts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldUltimates"));
	if (const ZoogiFront::FAwaken* Aw = bOldUlts ? nullptr : ZoogiFront::AwakenFor(B, this))
	{
		if (Aw->Skill && *Aw->Skill)
		{
			Players[B->PlayerIndex].Meter = 0.f;
			bUltThisShot = true; bUltThisTurn = true; UltPower = P;
			const FLinearColor Ac(1.f, 0.78f, 0.25f);
			Callout(Aw->Name, FString::Printf(TEXT("%s  AWAKEN"), *B->Def.Name.ToUpper()), Ac, 1.5f, 1.05f, B->PlayerIndex, FString::Printf(TEXT("Awaken - %s!"), Aw->Name));
			ZoogiShow::ImpactFrame(GetWorld(), B->BallLocation(), 9);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.85f);
			ZoogiEpic::Hit(G, ZoogiEpic::EHit::Big); // b103
			ZoogiSkills::AwakenCast(G, B, FName(Aw->Skill));
			return true;
		}
		P = EZoogiPower::Explosion; // METEOR STRIKE
	}
	Players[B->PlayerIndex].Meter = 0.f;
	bUltThisShot = true; bUltThisTurn = true; UltPower = P;
	if ((P == EZoogiPower::Ricochet || P == EZoogiPower::LockOn) && !B->bCPU)
	{
		bUltAim = true; UltAimAge = 0.f;
		UltAimLimit = 30.f; // b67 (Evan): the game stops until the target is picked - 30 s (was 5 s first time, then 3 s)
		UltAimSeen.Add(B->PlayerIndex);
		Callout(UltName(P), TEXT("TAP A TARGET!"), UltColor(P), 1.2f, 0.8f, B->PlayerIndex, TEXT("Who's first? Pick a target!"));
		UE_LOG(LogZoogi, Log, TEXT("FUN ULTIMATE %s: bullet-time aim"), UltName(P));
		Shot(G, TEXT("fun_ult_aim"), 0.f);
		return true;
	}
	FireUltimate(G, B, P, nullptr);
	return true;
}

AActor* UZoogiFunSys::PickUltTarget(AZoogiGameMode* G, AZoogiBall* B, const AActor* Exclude, bool bPreferBalls) const
{
	if (!G || !B) return nullptr;
	const FVector L = B->BallLocation();
	AActor* PickA = nullptr; float BestS = 1e9f;
	for (AActor* T : G->LiveTargets(B))
	{
		if (T == Exclude) continue;
		const float D = FVector::Dist2D(FunActorPos(T), L);
		if (D < 1.f || D > 2600.f) continue;
		const float S = D - ((bPreferBalls && Cast<AZoogiBall>(T)) ? 900.f : 0.f);
		if (S < BestS) { BestS = S; PickA = T; }
	}
	return PickA;
}

void UZoogiFunSys::Blast(AZoogiGameMode* G, const FVector& At, float R, float Base, float Extra, float Up, AZoogiBall* By, bool bBalls, bool bOrbs)
{
	const int32 Credit = By ? By->PlayerIndex : -1;
	int32 N = 0;
	auto Hit = [&](AActor* A)
	{
		const FVector D = FunActorPos(A) - At;
		const float Dist = FVector2D(D.X, D.Y).Size();
		if (Dist > R) return;
		const float K = 1.f - FMath::Clamp(Dist / R, 0.f, 1.f);
		const FVector Dir = FVector(D.X, D.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
		FunPush(A, Dir * (Base + Extra * K), Up * (0.5f + 0.5f * K), Credit);
		++N;
	};
	if (bBalls) for (AZoogiBall* Bb : G->Balls) if (Bb && Bb != By && !Bb->bOut && !G->IsAlly(Bb, By)) Hit(Bb);
	if (bOrbs) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Hit(O);
	UE_LOG(LogZoogi, Log, TEXT("FUN blast r%.0f at (%.0f, %.0f) pushed %d"), R, At.X, At.Y, N);
}

void UZoogiFunSys::DamageArena(AZoogiGameMode* G, const FVector& At, float R, AZoogiBall* By, int32 SnowmanHits)
{
	UWorld* W = GetWorld();
	int32 N = 0;
	for (TActorIterator<AZoogiArenaPiece> It(W); It; ++It)
	{
		AZoogiArenaPiece* P = *It;
		AZoogiSnowman* Sm = Cast<AZoogiSnowman>(P);
		AZoogiIglooWall* Ig = Cast<AZoogiIglooWall>(P);
		if (!Sm && !Ig) continue; // only the built-for-it destructibles (never the floor / stands)
		const FBox Bx = P->GetComponentsBoundingBox(true);
		if (!Bx.IsValid) continue;
		const float D = FMath::Sqrt(Bx.ComputeSquaredDistanceToPoint(FVector(At.X, At.Y, Bx.GetCenter().Z)));
		if (D > R) continue;
		const FVector Dir = (Bx.GetCenter() - At).GetSafeNormal2D();
		if (Sm) { for (int32 i = 0; i < SnowmanHits && Sm->GetStage() < 4; ++i) Sm->KnockNext(Dir.IsNearlyZero() ? FVector::ForwardVector : Dir); ++N; }
		else if (Ig && Ig->CoursesLeft() > 0)
		{
			Ig->ApplyDamage(500.f, Bx.GetCenter(), Dir, EZoogiArtifact::None, By);
			if (SnowmanHits >= 2) { FDelayedHit H; H.Piece = Ig; H.At = Now() + 0.35f; H.Dir = Dir; H.By = By; DelayedHits.Add(H); }
			++N;
		}
	}
	if (N) UE_LOG(LogZoogi, Log, TEXT("FUN ultimate hit %d arena pieces (r%.0f)"), N, R);
}

void UZoogiFunSys::UltBanner(AZoogiGameMode* G, AZoogiBall* B, EZoogiPower P)
{
	UltBannerAge = 0.f;
	UltBannerText = UltName(P);
	UltBannerSub = FString::Printf(TEXT("%s  ULTIMATE"), *B->Def.Name.ToUpper());
	UltBannerCol = UltColor(P);
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBanners")))
	{
		// b63: the ultimate banner is the P5 corner plate + the Zoogi shouting it at the top
		static const TCHAR* Shouts[] = { TEXT("Take this - %s!"), TEXT("Here it comes... %s!"), TEXT("No holding back! %s!"), TEXT("You're finished - %s!") };
		const FString Nm = FString(UltName(P));
		Callout(Nm, UltBannerSub, UltBannerCol, 1.5f, 1.05f, B->PlayerIndex, FString(Shouts[FMath::RandRange(0, 3)]).Replace(TEXT("%s"), *Nm.Left(1).ToUpper().Append(Nm.Mid(1).ToLower())));
	}
	ZoogiShow::ImpactFrame(GetWorld(), B->BallLocation(), 9);
	ZoogiShow::Event(GetWorld(), ZoogiShow::EEvent::BigScore, B->BallLocation(), 2.f);
	if (UZoogiShowSys* SS = GetWorld()->GetSubsystem<UZoogiShowSys>()) SS->Fireworks(3, 1.f);
	ZoogiUtil::PlaySfx(G, P == EZoogiPower::Explosion ? TEXT("Explosion") : TEXT("Zap"), 1.f, P == EZoogiPower::ShadowStun ? 0.6f : 0.85f);
	ZoogiUtil::PlaySfx(G, TEXT("CrowdWave1"), 0.7f);
}

void UZoogiFunSys::FireUltimate(AZoogiGameMode* G, AZoogiBall* B, EZoogiPower P, AActor* Target)
{
	UWorld* W = GetWorld();
	const FVector L = B->BallLocation();
	const FLinearColor C = UltColor(P);
	UE_LOG(LogZoogi, Log, TEXT("FUN ULTIMATE P%d %s: %s (target %s)"), B->PlayerIndex + 1, *B->Def.Name, UltName(P), Target ? *Target->GetName() : TEXT("auto"));
	ZoogiSkills::SetAwakenStrength(); // b100: the ultimate's meteors / blasts run at full strength
	// b62 (Evan): 15 s epic intro clip for the super power (Wolfgang's wolves, Meteor Strike for now)
	static const bool bUltIntros = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUltIntros")); // b66 (Evan): the 15 s intros belong to the SKILLS now
	if (UZoogiAnimeSubsystem* An = (W && bUltIntros) ? W->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr)
	{
		if (P == EZoogiPower::WolfPack) An->PlayPowerIntro(TEXT("WolfPack"), B->PlayerIndex, C);
		else if (P == EZoogiPower::Explosion) An->PlayPowerIntro(TEXT("MeteorStrike"), B->PlayerIndex, C);
		else if (P == EZoogiPower::StaticShock && B->Def.Name.Equals(TEXT("Bolt"), ESearchCase::IgnoreCase)) An->PlayPowerIntro(TEXT("BoltBalls"), B->PlayerIndex, C); // b65: Grok's Bolt intro
	}
	UltBanner(G, B, P);
	switch (P)
	{
	case EZoogiPower::Explosion: // METEOR STRIKE: a far bigger blast that also breaks igloo walls and the snowman
	{
		AZoogiFX::Spawn(W, L, C, 720.f, 0.9f, false, 18.f);
		AZoogiFX::Spawn(W, L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), FLinearColor(1.f, 0.6f, 0.1f), 820.f, 1.1f, true, 12.f);
		AZoogiFX::Niagara(W, TEXT("NS_Explosion"), L, FRotator::ZeroRotator, 2.4f);
		AZoogiFX::ShockwaveByMag(W, L, 60000.f, &C, false, false);
		AZoogiFX::SpawnConfetti(W, L + FVector(0, 0, 120.f), C, 30);
		Blast(G, L, 720.f, 650.f, 1500.f, 320.f, B, true, true);
		DamageArena(G, L, 800.f, B, 3);
		ZoogiEpic::DebrisBurst(W, L, 1.5f); ZoogiEpic::Hit(G, ZoogiEpic::EHit::Big); // b103
		// b90f (Evan: "add the meteor rain to Meteor Strike - the blasts that land on the arena equal Wolf Pack blasts, slightly more devastating")
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoUltMeteors")))
			ZoogiSkills::MeteorRain(G, G->Arena ? G->Arena->GetActorLocation() : L, (G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f) * 0.75f, B->PlayerIndex, 8, 1.2f);
		break;
	}
	case EZoogiPower::WolfPack: // WOLF STAMPEDE: six clones
	{
		TArray<AActor*> Targets = G->LiveTargets(B);
		Targets.Sort([&L](const AActor& A, const AActor& Bq) { return FVector::DistSquared2D(FunActorPos(&A), L) < FVector::DistSquared2D(FunActorPos(&Bq), L); });
		FVector V = B->GetBallVelocity(); V.Z = 0.f;
		FVector Fwd = V.GetSafeNormal(); if (Fwd.IsNearlyZero()) Fwd = FRotator(0.f, B->AimYaw, 0.f).Vector();
		UStaticMesh* Wolf = ZoogiUtil::LoadMesh(TEXT("/Game/Zoogis/wolfgang/SM_wolfgang.SM_wolfgang"));
		if (!Wolf && B->Visual) Wolf = B->Visual->GetStaticMesh();
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector Off = FRotator(0.f, 60.f * i, 0.f).RotateVector(Fwd) * 130.f;
			const FVector SpawnL = L + Off + FVector(0, 0, 8.f);
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AZoogiClone* Cl = W->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(Off.Rotation(), SpawnL), SP);
			if (!Cl) continue;
			AActor* T = Targets.Num() ? Targets[i % Targets.Num()] : nullptr;
			Cl->InitClone(Wolf, T, B->PlayerIndex, 1900.f, B->Def.Color);
			if (Cl->Sphere) Cl->Sphere->SetPhysicsLinearVelocity(Off.GetSafeNormal() * 700.f + Fwd * 500.f);
			G->Clones.Add(Cl);
			AZoogiFX::Spawn(W, SpawnL, C, 100.f, 0.45f, false, 7.f);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), SpawnL, FRotator::ZeroRotator, 0.7f);
		}
		AZoogiFX::Spawn(W, L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), C, 420.f, 0.8f, true, 10.f);
		break;
	}
	case EZoogiPower::Ricochet: // CHAIN RICOCHET: five bounces, each straight at the next target
	{
		UltBounces = 5;
		AActor* T = Target ? Target : PickUltTarget(G, B, nullptr, true);
		if (T) { const FVector Dir = (FunActorPos(T) - L).GetSafeNormal2D(); B->SetPlanarVelocity(Dir * 1700.f); UltHoming = T; UltHomingUntil = Now() + 0.9f; }
		AZoogiFX::Spawn(W, L, C, 200.f, 0.5f, true, 12.f, B);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Green"), L, FRotator::ZeroRotator, 1.1f);
		break;
	}
	case EZoogiPower::LockOn: // HOMING STRIKE: steers onto the target and hits 50% harder
	{
		AActor* T = Target ? Target : PickUltTarget(G, B, nullptr, true);
		if (T)
		{
			const FVector Dir = (FunActorPos(T) - L).GetSafeNormal2D();
			B->SetPlanarVelocity(Dir * 2300.f);
			UltHoming = T; UltHomingUntil = Now() + 1.6f; UltHitMul = 1.5f;
		}
		AZoogiFX::Spawn(W, L, C, 220.f, 0.5f, true, 14.f, B);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), L, FRotator::ZeroRotator, 1.f);
		break;
	}
	case EZoogiPower::StaticShock: // THUNDER CHAIN: lightning on every rival (stun + knock), throws the power orbs, cracks igloos
	{
		for (AZoogiBall* R : G->Balls)
		{
			if (!R || R == B || R->bOut || G->IsAlly(R, B)) continue;
			const FVector RL = R->BallLocation();
			R->StunTurns = FMath::Max(R->StunTurns, 1);
			FVector Side = (RL - L).GetSafeNormal2D(); if (Side.IsNearlyZero()) Side = FVector::ForwardVector;
			FunPush(R, Side * 720.f, 260.f, B->PlayerIndex);
			for (int32 k = 0; k < 6; ++k) AZoogiFX::Spawn(W, RL + FVector(0, 0, 90.f * k), C, 70.f, 0.35f + 0.05f * k, false, 14.f);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Yellow"), RL, FRotator::ZeroRotator, 1.2f);
			G->AddPopup(TEXT("ZAPPED"), RL + FVector(0, 0, 120.f), C);
			DamageArena(G, RL, 300.f, B, 1);
		}
		for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut || !O->bPower) continue;
			const FVector OL = O->OrbLocation();
			FVector Dir = FVector(OL.X, OL.Y, 0.f).GetSafeNormal(); if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
			FunPush(O, FRotator(0.f, FMath::FRandRange(-40.f, 40.f), 0.f).RotateVector(Dir) * 1100.f, 380.f, B->PlayerIndex);
			for (int32 k = 0; k < 5; ++k) AZoogiFX::Spawn(W, OL + FVector(0, 0, 100.f * k), C, 60.f, 0.4f, false, 14.f);
		}
		break;
	}
	case EZoogiPower::BandageBind: // MUMMY WRAP: binds every rival in reach and flings them away
	{
		for (AZoogiBall* R : G->Balls)
		{
			if (!R || R == B || R->bOut || G->IsAlly(R, B)) continue;
			const FVector RL = R->BallLocation();
			const float D = FVector::Dist2D(RL, L);
			if (D > 1100.f) continue;
			R->BoundShots = FMath::Max(R->BoundShots, 2);
			const float K = 1.f - D / 1100.f;
			FunPush(R, (RL - L).GetSafeNormal2D() * (650.f + 600.f * K), 160.f, B->PlayerIndex);
			AZoogiFX::Spawn(W, RL, C, 140.f, 0.8f, true, 8.f, R);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), RL, FRotator::ZeroRotator, 0.9f);
			G->AddPopup(TEXT("WRAPPED"), RL + FVector(0, 0, 120.f), C);
		}
		AZoogiFX::Spawn(W, L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), C, 1100.f, 1.f, true, 8.f);
		break;
	}
	case EZoogiPower::RuneSeal: // RUNE CAGE: every rival sealed, orbs blown out of the rune ring
	{
		for (AZoogiBall* R : G->Balls)
		{
			if (!R || R == B || R->bOut || G->IsAlly(R, B)) continue;
			R->BoundShots = FMath::Max(R->BoundShots, 2);
			R->LastHitPlayer = B->PlayerIndex;
			AZoogiFX::Spawn(W, R->BallLocation(), C, 130.f, 0.8f, true, 8.f, R);
			G->AddPopup(TEXT("CAGED"), R->BallLocation() + FVector(0, 0, 120.f), C);
		}
		Blast(G, L, 900.f, 450.f, 700.f, 120.f, B, false, true);
		AZoogiFX::Spawn(W, L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), C, 900.f, 1.1f, true, 12.f);
		AZoogiFX::Spawn(W, L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), FLinearColor(0.95f, 0.8f, 0.45f), 620.f, 1.f, true, 9.f);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), L, FRotator::ZeroRotator, 1.8f);
		break;
	}
	case EZoogiPower::ShadowStun: // BLACKOUT: the lights go out, every rival freezes and skips its next turn
	{
		BlackoutAge = 0.f;
		for (AZoogiBall* R : G->Balls)
		{
			if (!R || R == B || R->bOut || G->IsAlly(R, B)) continue;
			R->StunTurns = FMath::Max(R->StunTurns, 1);
			R->SetPlanarVelocity(FVector::ZeroVector);
			AZoogiFX::Spawn(W, R->BallLocation(), C, 140.f, 1.f, false, 8.f, R);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), R->BallLocation(), FRotator::ZeroRotator, 0.9f);
			G->AddPopup(TEXT("FROZEN"), R->BallLocation() + FVector(0, 0, 120.f), C);
		}
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), L, FRotator::ZeroRotator, 2.2f);
		break;
	}
	}
	Shot(G, FString::Printf(TEXT("fun_ult_%s"), *FString(UltName(P)).Replace(TEXT(" "), TEXT("_")).ToLower()), 0.35f);
}

void UZoogiFunSys::UltContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit)
{
	if (!bUltThisShot || !Sh || !Hit) return;
	if (UltPower == EZoogiPower::LockOn && UltHitMul > 1.f && Hit == UltHoming.Get())
	{
		FPendMul PM; PM.A = Hit; PM.Mul = UltHitMul; PendMul.Add(PM);
		UltHitMul = 1.f; UltHoming.Reset();
		AZoogiFX::Spawn(GetWorld(), FunActorPos(Hit), UltColor(UltPower), 220.f, 0.4f, true, 14.f);
	}
	if (UltPower == EZoogiPower::Ricochet && UltBounces > 0 && Now() - UltLastBounce > 0.12f)
	{
		UltLastBounce = Now();
		UltRedirectAt = Now() + 0.06f;
		UltLastHit = Hit;
	}
}

void UZoogiFunSys::TickUltimate(AZoogiGameMode* G, float Dt, float RealDt)
{
	UWorld* W = GetWorld();
	UltBannerAge += RealDt;
	BlackoutAge += RealDt;
	for (int32 i = DelayedHits.Num() - 1; i >= 0; --i)
	{
		if (Now() < DelayedHits[i].At) continue;
		if (AZoogiIglooWall* Ig = DelayedHits[i].Piece.Get()) if (Ig->CoursesLeft() > 0) Ig->ApplyDamage(500.f, Ig->GetComponentsBoundingBox(true).GetCenter(), DelayedHits[i].Dir, EZoogiArtifact::None, DelayedHits[i].By.Get());
		DelayedHits.RemoveAt(i);
	}
	AZoogiBall* B = G->GetActiveBall();
	// bullet-time aim (human Chain Ricochet / Homing Strike)
	if (bUltAim)
	{
		UltAimAge += RealDt;
		if (bTest && UltAimAge > 0.5f && B)
		{
			if (AActor* T = PickUltTarget(G, B, nullptr, true))
				if (APlayerController* PC = W->GetFirstPlayerController()) { FVector2D S; if (PC->ProjectWorldLocationToScreen(FunActorPos(T), S, false)) OnTap(S); }
		}
		if (bUltAim && (UltAimAge > UltAimLimit || !B || B->bOut || G->Phase != EZoogiPhase::Rolling))
		{
			bUltAim = false;
			if (B && !B->bOut) FireUltimate(G, B, UltPower, nullptr);
		}
	}
	if (G->Phase != EZoogiPhase::Rolling) return;
	// steering (Homing Strike, first Chain Ricochet leg)
	if (AActor* T = UltHoming.Get())
	{
		if (B && !B->bOut && Now() < UltHomingUntil && FunActorPos(T).Z > -5000.f)
		{
			const FVector V = B->GetBallVelocity();
			const float Sp = FMath::Max(FVector2D(V.X, V.Y).Size(), 900.f);
			const FVector Want = (FunActorPos(T) - B->BallLocation()).GetSafeNormal2D();
			const FVector Cur2 = FVector(V.X, V.Y, 0.f).GetSafeNormal();
			const FVector Dir = FMath::Lerp(Cur2, Want, FMath::Clamp(8.f * Dt, 0.f, 1.f)).GetSafeNormal();
			if (!Dir.IsNearlyZero()) B->Sphere->SetPhysicsLinearVelocity(FVector(Dir.X * Sp, Dir.Y * Sp, V.Z));
		}
		else UltHoming.Reset();
	}
	// chain ricochet redirect
	if (UltRedirectAt > 0.f && Now() >= UltRedirectAt)
	{
		UltRedirectAt = -1.f;
		if (B && !B->bOut && UltBounces > 0)
		{
			if (AActor* T = PickUltTarget(G, B, UltLastHit.Get(), false))
			{
				const FVector L = B->BallLocation();
				B->SetPlanarVelocity((FunActorPos(T) - L).GetSafeNormal2D() * 1600.f);
				UltHoming = T; UltHomingUntil = Now() + 0.6f;
				--UltBounces;
				AZoogiFX::Spawn(W, L, UltColor(EZoogiPower::Ricochet), 140.f, 0.35f, false, 10.f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Green"), L, FRotator::ZeroRotator, 0.8f);
				G->AddPopup(FString::Printf(TEXT("RICOCHET x%d"), 5 - UltBounces), L + FVector(0, 0, 120.f), UltColor(EZoogiPower::Ricochet));
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.5f, 1.6f + 0.1f * (5 - UltBounces));
				UE_LOG(LogZoogi, Log, TEXT("FUN chain ricochet -> %s (%d left)"), *T->GetName(), UltBounces);
			}
			else UltBounces = 0;
		}
	}
	// CPU ultimates: fire into a crowd, or soon after launch
	if (!ZoogiAwakenMeter() && B && B->bCPU && !bUltThisShot && Players.IsValidIndex(B->PlayerIndex) && Players[B->PlayerIndex].Meter >= ZoogiSuperTune::MeterFull && G->PhaseTime > 0.35f && G->PhaseTime < 3.f)
	{
		int32 Near = 0;
		for (AActor* T : G->LiveTargets(B)) if (FVector::Dist2D(FunActorPos(T), B->BallLocation()) < 750.f) ++Near;
		if (Near >= 2 || G->PhaseTime > 1.0f) TryUltimate(G);
	}
	// -ZoogiFunTest: the human fires its ultimate early in the roll
	if (bTest && B && !B->bCPU && !bUltThisShot && G->PhaseTime > 0.5f && UltimateReady(G, B->PlayerIndex)) TryUltimate(G);
}

// ------------------------------------------------------------------ power clash

void UZoogiFunSys::StartClash(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def)
{
	// build 27 (Evan): the clash is a real fight - both Zoogis burst into their ultimate forms, square up at the contact point
	// and trade punches (world nearly frozen, the forms run at full speed) while the players tap; the winner lands its
	// signature move, the loser takes the hit and falls, then the roll resumes with the clash result on the knock
	EndFighters();
	if (Strike.bOn) EndStrike(false);
	ZoogiClashSplit::End(); // b68
	Clash = FClash();
	Clash.bOn = true; Clash.Att = Att; Clash.Def = Def;
	Clash.At = (Att->BallLocation() + Def->BallLocation()) * 0.5f;
	Clash.Center = Clash.At;
	Clash.bHumanAtt = !Att->bCPU; Clash.bHumanDef = !Def->bCPU;
	// Oct 30 (Evan: "too hard to win after being down"): CPU 3.8-5.8 taps/s (was 6-9) - with the tug-of-war rules a typical
	// phone tapper (~6/s) wins about 65 % (Monte Carlo, C:\dev\HANDOFF.md)
	Clash.RateA = FMath::FRandRange(3.8f, 5.8f); Clash.RateD = FMath::FRandRange(3.8f, 5.8f);
	FVector Ax = Def->BallLocation() - Att->BallLocation(); Ax.Z = 0.f;
	if (!Ax.Normalize()) { Ax = Att->GetBallVelocity(); Ax.Z = 0.f; if (!Ax.Normalize()) Ax = FVector::ForwardVector; }
	Clash.Axis = Ax; Clash.AxisYaw = Ax.Rotation().Yaw;
	bClashThisShot = true;
	Callouts.Reset(); // the clash panel takes the screen
	// build 29 (Evan): "the glare is too much" - every burst / glow sphere / particle system that was mid-flight froze at its brightest
	// under the near-freeze: clear the bursts and hide the particle systems / glow parts around the fight until it is over
	RestoreHiddenFx();
	// b104 (Evan): classic duels play on the field - the cinematic duel scene (forms + duel camera) is for story missions
	Clash.bField = ZoogiDuelV2::ReplacesClash() && !ZoogiStory::InMission() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelScene"));
	if (Clash.bField)
	{
		for (AZoogiBall* Db : { Att, Def })
		{
			const FLinearColor Pc = ZoogiUtil::PlayerColor(Db->PlayerIndex);
			AZoogiFX::Spawn(GetWorld(), Db->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Pc, 230.f, 0.9f, true, 10.f);
			AZoogiFX::Spawn(GetWorld(), Db->BallLocation(), Pc, 150.f, 0.5f, false, 9.f, Db);
		}
		UE_LOG(LogZoogi, Log, TEXT("FUN duel on the field (no duel scene)"));
	}
	else
	{
		HideFxAround(Clash.At, 3200.f, Att, Def);
		SpawnFighter(0, Att, FormIndex(Att));
		SpawnFighter(1, Def, FormIndex(Def));
	}
	G->AddFeed(FString::Printf(TEXT("POWER CLASH!  P%d %s  vs  P%d %s"), Att->PlayerIndex + 1, *Att->Def.Name, Def->PlayerIndex + 1, *Def->Def.Name), FLinearColor(1.f, 0.55f, 0.95f));
	ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("FUN POWER CLASH P%d %s vs P%d %s (forms %d vs %d, %s)"), Att->PlayerIndex + 1, *Att->Def.Name, Def->PlayerIndex + 1, *Def->Def.Name, Clash.F[0].Form, Clash.F[1].Form,
		(Clash.bHumanAtt || Clash.bHumanDef) ? TEXT("human tapping") : TEXT("CPU vs CPU"));
}

void UZoogiFunSys::TickClash(AZoogiGameMode* G, float RealDt)
{
	Clash.Age += RealDt;
	{	// b81 (Evan: "constant vibration during the clash"): a steady rumble for the whole clash, topped up every 0.4 s
		static double NextBuzz = 0.0;
		const double Nw = FPlatformTime::Seconds();
		if (Clash.Age < RealDt * 1.5f) NextBuzz = 0.0;
		if (Nw >= NextBuzz && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoClashBuzz"))) { NextBuzz = Nw + 0.4; ZoogiHaptics::Rumble(0.5f, Clash.bFinish ? 1.f : 0.65f, GetWorld(), TEXT("clash")); }
	}
	const float Rt = GetWorld()->GetRealTimeSeconds();
	if (!Clash.bFinish && Clash.bDuel) TickDuel(G, RealDt); // b66: duel stages instead of the tap tug
	else if (!Clash.bFinish)
	{
		if (Clash.bDCounter) if (UZoogiAnimeSubsystem* An = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (An->bPlaying && Clash.Age < 25.f) Clash.Age = FMath::Min(Clash.Age, 0.05f); // b67: the counter anime first
		if (!Clash.bSplit && Clash.Age > 0.05f && ZoogiClashSplit::Enabled()) // b68 (Evan): the two awakening clips split the screen
		{
			Clash.bSplit = true;
			AZoogiBall* Ab = Clash.Att.Get(); AZoogiBall* Db = Clash.Def.Get();
			ZoogiClashSplit::Begin(GetWorld(), Clash.bCamFlip ? Db : Ab, Clash.bCamFlip ? Ab : Db);
		}
		if (!Clash.bGo && Clash.Age >= ClashReadySecs) { Clash.bGo = true; ZoogiUtil::PlaySfx(G, TEXT("Clack"), 1.f, 1.1f); } // TAP!
		if (Clash.bGo)
		{
			// CPUs ease off while they are clearly ahead (Bar + = attacker side)
			if (!Clash.bHumanAtt) { Clash.AccA += Clash.RateA * (Clash.Bar > 0.2f ? 0.7f : 1.f) * RealDt; while (Clash.AccA >= 1.f) { Clash.AccA -= 1.f; ClashPush(true); } }
			if (!Clash.bHumanDef) { Clash.AccD += Clash.RateD * (Clash.Bar < -0.2f ? 0.7f : 1.f) * RealDt; while (Clash.AccD >= 1.f) { Clash.AccD -= 1.f; ClashPush(false); } }
			if (bTest) { Clash.TestAcc += 6.f * RealDt; while (Clash.TestAcc >= 1.f) { Clash.TestAcc -= 1.f; OnTap(); } }
		}
		Clash.Ratio = FMath::FInterpTo(Clash.Ratio, FMath::Clamp(0.5f + 0.5f * Clash.Bar, 0.04f, 0.96f), RealDt, 8.f);
		if (Clash.Age >= ClashFightSecs) FinishClash(G);
	}
	else
	{
		Clash.FinishAge += RealDt;
		Clash.Ratio = FMath::FInterpTo(Clash.Ratio, Clash.bAttWins ? 0.8f : 0.2f, RealDt, 3.f); // the winner drives through
		if (Clash.FinishAge >= ClashFinishSecs) { EndClash(G); return; }
	}
	TickFighters(G, RealDt);
	// proof captures (immediate: delayed shots count dilated game time)
	if ((bTest || bStrikeTest || bClashTestOn) && !G->ShotDir.IsEmpty())
	{
		static int32 ClashN = 0;
		auto At = [&](float T0, bool bFin, const TCHAR* Nm)
		{
			const float A0 = bFin ? Clash.FinishAge : Clash.Age;
			if ((Clash.bFinish == bFin) && A0 >= T0 && A0 - RealDt < T0) G->TakeShot(FString::Printf(TEXT("fun_clash%d_%s"), ClashN, Nm));
		};
		if (!Clash.bFinish && Clash.Age <= RealDt) ++ClashN;
		At(0.6f, false, TEXT("a_fight")); At(1.7f, false, TEXT("b_fight")); At(2.8f, false, TEXT("c_fight"));
		At(0.55f, true, TEXT("d_finish")); At(1.1f, true, TEXT("e_fall"));
	}
}

void UZoogiFunSys::ClashPush(bool bAtt)
{
	// Oct 30 (Evan): tug-of-war - each punch moves the bar; behind by more than 15 % = punches hit 1.6x harder; the last second
	// (FINAL PUSH) doubles every punch; nothing counts during READY...
	if (!Clash.bOn || Clash.bFinish || Clash.Age < ClashReadySecs) return;
	const bool bBehind = bAtt ? Clash.Bar < -0.15f : Clash.Bar > 0.15f;
	const bool bFinal = Clash.Age > ClashFightSecs - 1.f;
	const float Push = 0.05f * (bBehind ? 1.6f : 1.f) * (bFinal ? 2.f : 1.f);
	Clash.Bar = FMath::Clamp(Clash.Bar + (bAtt ? Push : -Push), -1.f, 1.f);
	const float Rt = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (bAtt) { ++Clash.TapsA; Clash.TapA = Rt; } else { ++Clash.TapsD; Clash.TapD = Rt; }
}

void UZoogiFunSys::FinishClash(AZoogiGameMode* G)
{
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	Clash.bFinish = true; Clash.FinishAge = 0.f;
	Clash.bAttWins = Clash.Bar >= 0.f; // Oct 30: the tug-of-war bar decides (not the total punch count)
	const int32 Wn = Clash.bAttWins ? 0 : 1;
	AZoogiBall* W = Clash.bAttWins ? A : D;
	AZoogiBall* L = Clash.bAttWins ? D : A;
	UE_LOG(LogZoogi, Log, TEXT("FUN clash result %d - %d, bar %+.2f -> %s, %d punches thrown"), Clash.TapsA, Clash.TapsD, Clash.Bar, Clash.bAttWins ? TEXT("attacker") : TEXT("defender"), Clash.Hits);
	// winner: signature finisher; loser: guard until it lands (TickFighters plays the hit + fall)
	const int32 Fw = Clash.F[Wn].Form;
	PlayFighter(Wn, FormSignature(Fw), 1.1f, false);
	PlayFighter(1 - Wn, TEXT("idle"), 1.f, true);
	if (A) AddMeter(G, A->PlayerIndex, 10.f, TEXT("clash"));
	if (D) AddMeter(G, D->PlayerIndex, 10.f, TEXT("clash"));
	// scoreboard: clash win + points (committed with the shot's ledger at Resolve)
	if (W && Players.IsValidIndex(W->PlayerIndex))
	{
		Players[W->PlayerIndex].ClashWins++;
		FZoogiLedgerEntry E; E.Player = W->PlayerIndex; E.Points = ClashPoints;
		G->Ledger.Add(E);
		G->AddFeed(FString::Printf(TEXT("P%d %s  won the POWER CLASH vs %s  %d - %d   +%d"), W->PlayerIndex + 1, *W->Def.Name, L ? *L->Def.Name : TEXT(""), FMath::Max(Clash.TapsA, Clash.TapsD), FMath::Min(Clash.TapsA, Clash.TapsD), ClashPoints), ZoogiUtil::PlayerColor(W->PlayerIndex));
		G->AddPopup(FString::Printf(TEXT("+%d POWER CLASH  P%d"), ClashPoints, W->PlayerIndex + 1), Clash.At + FVector(0, 0, 260.f), ZoogiUtil::PlayerColor(W->PlayerIndex));
	}
	if (W && L) ZoogiBanter::Exchange(G, W->PlayerIndex, L->PlayerIndex, ZoogiBanter::EKind::WinGloat, ZoogiBanter::EKind::LoseRetort, true); // b67
	const bool bComeback = (Clash.bAttWins ? Clash.TapsA : Clash.TapsD) < (Clash.bAttWins ? Clash.TapsD : Clash.TapsA);
	Callout(bComeback ? TEXT("COMEBACK!") : (Clash.bAttWins ? TEXT("OVERPOWERED!") : TEXT("COUNTERED!")), FString::Printf(TEXT("%s WINS THE POWER CLASH  %d - %d   +%d"), W ? *W->Def.Name.ToUpper() : TEXT(""), FMath::Max(Clash.TapsA, Clash.TapsD), FMath::Min(Clash.TapsA, Clash.TapsD), ClashPoints),
		Clash.bAttWins ? FLinearColor(1.f, 0.6f, 0.2f) : FLinearColor(0.4f, 0.85f, 1.f), ClashFinishSecs, 1.f);
}

void UZoogiFunSys::EndClash(AZoogiGameMode* G)
{
	ZoogiClashSplit::End(); // b68
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	if (!Clash.bFinish) FinishClash(G);
	bResolveAfterClash = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashKeepRolling")); // b62: the turn ends once the clash scene is over
	const bool bAttWins = Clash.bAttWins;
	if (D && D->Charges > 0) D->Charges--; else if (D && Players.IsValidIndex(D->PlayerIndex)) Players[D->PlayerIndex].Meter = FMath::Max(0.f, Players[D->PlayerIndex].Meter - 50.f);
	// build 28 (Evan): the loser is SENT - launched away from the winner (it flies in its ultimate form, see StartFlung)
	AZoogiBall* Lb = bAttWins ? D : A;
	AZoogiBall* Wb = bAttWins ? A : D;
	if (!bAttWins && D) { FPendMul PM; PM.A = D; PM.Mul = 0.3f; PendMul.Add(PM); } // the defender who won barely moves
	if (Lb && Lb->Sphere && !Lb->bOut)
	{
		const FVector Away = bAttWins ? Clash.Axis : -Clash.Axis;
		FVector V = Lb->Sphere->GetPhysicsLinearVelocity();
		const float Hs = FMath::Clamp(V.Size2D() * 1.6f, 1050.f, 1400.f);
		V = Away * Hs + FVector(0.f, 0.f, 480.f);
		Lb->Sphere->WakeRigidBody();
		Lb->Sphere->SetPhysicsLinearVelocity(V);
		if (Wb) Lb->LastHitPlayer = Wb->PlayerIndex; // a ring-out from this counts for the winner
		UE_LOG(LogZoogi, Log, TEXT("FUN clash loser P%d sent flying at %.0f cm/s"), Lb->PlayerIndex + 1, V.Size());
	}
	const FVector At = Clash.Center;
	{	// b172f (Evan: "bring back the model animation smashing into something"): a field duel (b104) has no fighters, so the loser got
		// no form for the flight - give it its form now so it backstrokes away and smashes into the wall / stands (-ZoogiNoFieldFlung)
		static const bool bNoFieldFlung = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFieldFlung"));
		const int32 LSide = bAttWins ? 1 : 0;
		if (!bNoFieldFlung && Lb && !Lb->bOut && !Clash.F[LSide].Actor.IsValid())
		{
			Clash.Height = 0.f;
			SpawnFighter(LSide, Lb, FormIndex(Lb));
			if (LSide == 1) Clash.Height *= 2.f; // SpawnFighter averages side 1 with side 0's height (none here)
			UE_LOG(LogZoogi, Log, TEXT("FUN field duel: loser P%d gets its form for the flight"), Lb->PlayerIndex + 1);
		}
	}
	StartFlung(bAttWins ? 1 : 0); // build 28: the loser's form takes the knock and flies (the ball stays hidden under it)
	{ static int32 FlungN = 0; ++FlungN; Shot(G, FString::Printf(TEXT("fun_clash%d_f_flung"), FlungN), 0.3f); Shot(G, FString::Printf(TEXT("fun_clash%d_g_flung"), FlungN), 0.8f); }
	EndFighters();
	Clash = FClash();
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 0.9f, 0.6f), 160.f, 0.35f, true, 10.f);
	ZoogiShow::Event(GetWorld(), ZoogiShow::EEvent::HardHit, At, 2.f);
	UE_LOG(LogZoogi, Log, TEXT("FUN clash over -> roll resumes (%s wins, loser sent flying)"), bAttWins ? TEXT("attacker") : TEXT("defender"));
}

// ------------------------------------------------------------------ fusion + contacts (called from OnBallContact)

namespace ZoogiDuelV2 { bool ReplacesClash(); }

void UZoogiFunSys::SuperContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit, bool bFirst)
{
	UltContact(G, Sh, Hit);
	AZoogiBall* R = Cast<AZoogiBall>(Hit);
	if (!R || R == Sh || G->IsAlly(R, Sh) || Clash.bOn) return;
	const bool bClassicDef = ClassicDefendMode(G); // b106: classic = the defend window + a pure physics hit, no duel
	if (!bClashThisShot && ZoogiRedirect::TakeDuel(Sh, R) && !bClassicDef) { StartDuel(G, Sh, R); return; } // b66 (Evan): a redirect into a rival = DUEL
	const int32 Si = Sh->PlayerIndex, Ri = R->PlayerIndex;
	const bool bPowered = bUltThisShot || G->bPowerUsed || (Players.IsValidIndex(Si) && (Players[Si].bFire || Players[Si].bAwake));
	const bool bCanAnswer = R->Charges > 0 || Meter(Ri) >= 50.f;
	static const bool bClashOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoClash"));
	static const bool bClashTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTest")); // proofs: every Zoogi-vs-Zoogi hit clashes
	if (bClashOn && !bClassicDef && !bClashThisShot && ((bPowered && bCanAnswer) || bClashTest)) { if (ZoogiDuelV2::ReplacesClash()) StartDuel(G, Sh, R); else StartClash(G, Sh, R); return; } // b90 (Evan): no more button-mash - the new duel // build 27: CPU vs CPU clashes too (both transform)
	if (bUltThisShot && !bFusedThisShot)
	{
		bFusedThisShot = true;
		const EZoogiPower A = SignaturePower(Sh), Bp = SignaturePower(R);
		const FString Ea = ElementName(A), Eb = ElementName(Bp);
		FString Name;
		auto Has = [&](const TCHAR* X, const TCHAR* Y) { return (Ea == X && Eb == Y) || (Ea == Y && Eb == X); };
		if (Has(TEXT("FIRE"), TEXT("LIGHTNING"))) Name = TEXT("PLASMA BURST");
		else if (Has(TEXT("FIRE"), TEXT("SHADOW"))) Name = TEXT("HELLFIRE");
		else if (Has(TEXT("LIGHTNING"), TEXT("SHADOW"))) Name = TEXT("STORM NIGHT");
		else if (Has(TEXT("RUNE"), TEXT("CURSE"))) Name = TEXT("ANCIENT SEAL");
		else if (Has(TEXT("WILD"), TEXT("FIRE"))) Name = TEXT("WILDFIRE");
		else if (Has(TEXT("KINETIC"), TEXT("LASER"))) Name = TEXT("RAILGUN");
		else if (Ea == Eb) Name = FString::Printf(TEXT("DOUBLE %s"), *Ea);
		else Name = FString::Printf(TEXT("%s %s"), *Ea, *Eb);
		const FVector At = (Sh->BallLocation() + R->BallLocation()) * 0.5f;
		const FLinearColor Ca = UltColor(A), Cb = UltColor(Bp);
		AZoogiFX::Spawn(GetWorld(), At, Ca, 560.f, 0.7f, false, 16.f);
		AZoogiFX::Spawn(GetWorld(), At - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), Cb, 620.f, 0.9f, true, 12.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Explosion"), At, FRotator::ZeroRotator, 1.4f);
		Blast(G, At, 560.f, 450.f, 1100.f, 220.f, Sh, true, true);
		Callout(FString::Printf(TEXT("%s FUSION!"), *Name), FString::Printf(TEXT("%s + %s"), *Ea, *Eb), FMath::Lerp(Ca, Cb, 0.5f), 1.4f, 1.f);
		ZoogiShow::ImpactFrame(GetWorld(), At, 7);
		AddMeter(G, Si, 10.f, TEXT("fusion"));
		UE_LOG(LogZoogi, Log, TEXT("FUN FUSION %s (%s + %s)"), *Name, *Ea, *Eb);
		Shot(G, TEXT("fun_fusion"), 0.15f);
	}
}

// ------------------------------------------------------------------ awakened

void UZoogiFunSys::SetAwakened(AZoogiGameMode* G, int32 Player, bool bOn)
{
	if (!Players.IsValidIndex(Player) || Players[Player].bAwake == bOn) return;
	AZoogiBall* B = G && G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	FPlayerFun& P = Players[Player];
	P.bAwake = bOn;
	if (!B) return;
	if (bOn) { const int32 Rv = ZoogiBanter::RandomRival(G, Player); if (Rv >= 0) ZoogiBanter::Exchange(G, Player, Rv, ZoogiBanter::EKind::Awaken, ZoogiBanter::EKind::AwakenReact, true); } // b67
	B->SetMassScale(bOn ? 1.5f : 1.f);
	if (B->Visual)
	{
		if (bOn) { P.VisualScale = B->Visual->GetRelativeScale3D(); B->Visual->SetRelativeScale3D(P.VisualScale * 1.18f); }
		else if (!P.VisualScale.IsNearlyZero()) B->Visual->SetRelativeScale3D(P.VisualScale);
	}
	if (bOn)
	{
		Callout(FString::Printf(TEXT("%s AWAKENED!"), *B->Def.Name.ToUpper()), TEXT("ON FIRE + FULL METER  -  HEAVIER, HARDER HITS"), FLinearColor(1.f, 0.85f, 0.3f), 1.8f, 1.f, Player, TEXT("I'm awake now. Feel this power!"));
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), FLinearColor(1.f, 0.85f, 0.3f), 320.f, 0.8f, true, 14.f, B);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Gold"), B->BallLocation(), FRotator::ZeroRotator, 1.6f);
		ZoogiShow::Event(GetWorld(), ZoogiShow::EEvent::BigScore, B->BallLocation(), 2.f);
		ZoogiUtil::PlaySfx(G, TEXT("Inhale"), 0.8f, 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("FUN P%d %s AWAKENED"), Player + 1, *B->Def.Name);
		Shot(G, TEXT("fun_awakened"), 0.5f);
	}
}

// ------------------------------------------------------------------ arena events

void UZoogiFunSys::StartArenaEvent(AZoogiGameMode* G)
{
	static const bool bOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoArenaEvents"));
	if (!bOn) return;
	EventKind = 1 + (EventSerial++ + FMath::RandRange(0, 3)) % 4;
	if (bTest) EventKind = 1 + (EventSerial - 1) % 4;
	const TCHAR* Names[] = { TEXT(""), TEXT("METEOR SHOWER"), TEXT("LOW GRAVITY"), TEXT("WHIRLWIND"), TEXT("POWER SURGE") };
	const TCHAR* Subs[] = { TEXT(""), TEXT("ROCKS RAIN ON THE COURT THIS ROUND"), TEXT("EVERYTHING FLIES HIGHER THIS ROUND"), TEXT("THE ARENA SPINS EVERYTHING THAT ROLLS"), TEXT("EVERYONE +1 CHARGE  +40 SUPER") };
	EventName = Names[EventKind];
	Callout(FString::Printf(TEXT("ARENA EVENT: %s"), Names[EventKind]), Subs[EventKind], FLinearColor(1.f, 0.5f, 0.85f), 2.2f, 0.9f);
	if (UZoogiShowSys* SS = GetWorld()->GetSubsystem<UZoogiShowSys>()) SS->Fireworks(4, 1.2f);
	ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.45f, 0.9f);
	if (EventKind == 2)
	{
		if (AWorldSettings* WS = GetWorld()->GetWorldSettings())
		{
			SavedGravity = WS->GetGravityZ(); bSavedWorldGravitySet = WS->bWorldGravitySet; bGravityChanged = true;
			WS->bWorldGravitySet = true; WS->WorldGravityZ = SavedGravity * 0.42f;
		}
	}
	if (EventKind == 4)
		for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) { G->Balls[i]->Charges = FMath::Min(3, G->Balls[i]->Charges + 1); AddMeter(G, i, 40.f, TEXT("power surge")); }
	UE_LOG(LogZoogi, Log, TEXT("FUN ARENA EVENT round %d: %s"), G->RoundNumber, Names[EventKind]);
	Shot(G, FString::Printf(TEXT("fun_event_%d"), EventKind), 0.6f);
}

void UZoogiFunSys::EndArenaEvent()
{
	if (bGravityChanged && GetWorld())
		if (AWorldSettings* WS = GetWorld()->GetWorldSettings()) { WS->WorldGravityZ = SavedGravity; WS->bWorldGravitySet = bSavedWorldGravitySet; }
	bGravityChanged = false;
	EventKind = 0; EventName.Reset();
}

void UZoogiFunSys::TickArenaEvent(AZoogiGameMode* G, float Dt)
{
	if (EventKind == 0 || G->Phase != EZoogiPhase::Rolling) return;
	UWorld* W = GetWorld();
	if (EventKind == 1 && G->PhaseTime > 0.4f && G->PhaseTime < 4.5f && Now() >= MeteorNext)
	{
		MeteorNext = Now() + 0.65f;
		const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI), R = FMath::Sqrt(FMath::FRand()) * (KR - 250.f);
			const FVector2D P2(FMath::Cos(A) * R, FMath::Sin(A) * R);
			if (G->Arena && G->Arena->IsOffCourt(P2)) continue;
			const FVector P(P2.X, P2.Y, G->GetActiveBall() ? G->GetActiveBall()->BallLocation().Z - ZoogiConst::ZoogiRadius : 0.f);
			const FLinearColor C(1.f, 0.45f, 0.1f);
			AZoogiFX::Spawn(W, P + FVector(0, 0, 20.f), C, 230.f, 0.55f, false, 14.f);
			AZoogiFX::Spawn(W, P + FVector(0, 0, 4.f), FLinearColor(0.2f, 0.12f, 0.08f), 200.f, 2.5f, true, 3.f);
			AZoogiFX::Niagara(W, TEXT("NS_Explosion"), P + FVector(0, 0, 30.f), FRotator::ZeroRotator, 0.7f);
			Blast(G, P, 240.f, 450.f, 650.f, 260.f, nullptr, true, true);
			ZoogiUtil::PlaySfx(G, TEXT("Explosion"), 0.35f, 1.3f);
			break;
		}
	}
	if (EventKind == 3)
	{
		auto Spin = [&](UPrimitiveComponent* S)
		{
			if (!S || !S->IsSimulatingPhysics()) return;
			const FVector V = S->GetPhysicsLinearVelocity();
			if (FVector2D(V.X, V.Y).Size() < 80.f) return;
			const FVector L = S->GetComponentLocation();
			const FVector Tan = FVector(-L.Y, L.X, 0.f).GetSafeNormal();
			S->SetPhysicsLinearVelocity(V + Tan * 240.f * Dt);
		};
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Spin(B->Sphere);
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Spin(O->Sphere);
	}
}
