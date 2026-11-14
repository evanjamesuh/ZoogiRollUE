// Oct 22: see ZoogiFeel.h
#include "ZoogiFeel.h"
#include "ZoogiEpic.h"
#include "ZoogiBall.h"
#include "ZoogiObstacle.h"
#include "ZoogiOrb.h"
#include "ZoogiPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "ZoogiSoundscape.h"

namespace ZoogiFeel
{
	struct FTune
	{
		bool bEnabled = true;
		bool bHitStop = true;
		bool bCameraShake = true;
		bool bWeightSounds = true;
		bool bTeeter = true;
		bool bFlail = true;
		bool bCrowd = true;
		bool bAimSpin = true;
		bool bLogAll = false;
		float HitStopMs = 50.f;
		float HitStopDilation = 0.12f;
		float HitStopMinImpulse = 4500.f;
		float HitStopFullImpulse = 28000.f;
		float HitStopMinInterval = 0.12f;
		float ShakeMinImpulse = 3500.f;
		float ShakeFullImpulse = 28000.f;
		float ShakeCap = 1.f;
		float SoundMinImpulse = 3500.f;
		float SoundMinInterval = 0.14f;
		// Item 47 â€” teeter near rim
		float TeeterDistCm = 160.f;       // start wobble/gasp this far inside the knock-off line
		float TeeterWobbleDeg = 4.5f;     // peak lean (Ball::Wobble clamps to 6)
		float TeeterWobbleInterval = 0.28f;
		float TeeterGaspCooldown = 1.6f;  // S_Inhale once per approach
		float TeeterMinSpeed = 80.f;      // ignore parked/idle marbles
		// Item 43 â€” comic flail
		float FlailSpinDps = 720.f;
		float FlailStretch = 0.12f;
		float FlailWobbleDeg = 6.f;
		float FlailVisibleSec = 0.45f;
		// Item 27 â€” crowd (mobile-friendly volumes)
		float CrowdGaspImpulse = 12000.f;
		float CrowdRoarImpulse = 22000.f;
		float CrowdVolCap = 0.42f;
		float CrowdMinInterval = 0.85f;
		// Sonic aim curl (mesh-only spin while pulling back)
		float AimSpinMinDps = 90.f;
		float AimSpinMaxDps = 1440.f;
		float AimSpinMinPull = 0.05f;
		FTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.Feel");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetBool(S, TEXT("bHitStop"), bHitStop, GGameIni);
				GConfig->GetBool(S, TEXT("bCameraShake"), bCameraShake, GGameIni);
				GConfig->GetBool(S, TEXT("bWeightSounds"), bWeightSounds, GGameIni);
				GConfig->GetBool(S, TEXT("bTeeter"), bTeeter, GGameIni);
				GConfig->GetBool(S, TEXT("bFlail"), bFlail, GGameIni);
				GConfig->GetBool(S, TEXT("bCrowd"), bCrowd, GGameIni);
				GConfig->GetBool(S, TEXT("bAimSpin"), bAimSpin, GGameIni);
				GConfig->GetFloat(S, TEXT("HitStopMs"), HitStopMs, GGameIni);
				GConfig->GetFloat(S, TEXT("HitStopDilation"), HitStopDilation, GGameIni);
				GConfig->GetFloat(S, TEXT("HitStopMinImpulse"), HitStopMinImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("HitStopFullImpulse"), HitStopFullImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("HitStopMinInterval"), HitStopMinInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("ShakeMinImpulse"), ShakeMinImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("ShakeFullImpulse"), ShakeFullImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("ShakeCap"), ShakeCap, GGameIni);
				GConfig->GetFloat(S, TEXT("SoundMinImpulse"), SoundMinImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("SoundMinInterval"), SoundMinInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("TeeterDistCm"), TeeterDistCm, GGameIni);
				GConfig->GetFloat(S, TEXT("TeeterWobbleDeg"), TeeterWobbleDeg, GGameIni);
				GConfig->GetFloat(S, TEXT("TeeterWobbleInterval"), TeeterWobbleInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("TeeterGaspCooldown"), TeeterGaspCooldown, GGameIni);
				GConfig->GetFloat(S, TEXT("TeeterMinSpeed"), TeeterMinSpeed, GGameIni);
				GConfig->GetFloat(S, TEXT("FlailSpinDps"), FlailSpinDps, GGameIni);
				GConfig->GetFloat(S, TEXT("FlailStretch"), FlailStretch, GGameIni);
				GConfig->GetFloat(S, TEXT("FlailWobbleDeg"), FlailWobbleDeg, GGameIni);
				GConfig->GetFloat(S, TEXT("FlailVisibleSec"), FlailVisibleSec, GGameIni);
				GConfig->GetFloat(S, TEXT("CrowdGaspImpulse"), CrowdGaspImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("CrowdRoarImpulse"), CrowdRoarImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("CrowdVolCap"), CrowdVolCap, GGameIni);
				GConfig->GetFloat(S, TEXT("CrowdMinInterval"), CrowdMinInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("AimSpinMinDps"), AimSpinMinDps, GGameIni);
				GConfig->GetFloat(S, TEXT("AimSpinMaxDps"), AimSpinMaxDps, GGameIni);
				GConfig->GetFloat(S, TEXT("AimSpinMinPull"), AimSpinMinPull, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFeel"))) bEnabled = false;
			bLogAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFeelLog"))
				|| FParse::Param(FCommandLine::Get(), TEXT("ZoogiFeelTest"));
			HitStopFullImpulse = FMath::Max(HitStopFullImpulse, HitStopMinImpulse + 1.f);
			ShakeFullImpulse = FMath::Max(ShakeFullImpulse, ShakeMinImpulse + 1.f);
			HitStopMs = FMath::Clamp(HitStopMs, 0.f, 120.f);
			HitStopDilation = FMath::Clamp(HitStopDilation, 0.02f, 0.5f);
			ShakeCap = FMath::Clamp(ShakeCap, 0.2f, 1.f);
			TeeterDistCm = FMath::Clamp(TeeterDistCm, 40.f, 400.f);
			TeeterWobbleDeg = FMath::Clamp(TeeterWobbleDeg, 1.f, 6.f);
			FlailVisibleSec = FMath::Clamp(FlailVisibleSec, 0.f, 1.2f);
			CrowdVolCap = FMath::Clamp(CrowdVolCap, 0.15f, 0.7f);
			CrowdRoarImpulse = FMath::Max(CrowdRoarImpulse, CrowdGaspImpulse + 1.f);
			AimSpinMinDps = FMath::Clamp(AimSpinMinDps, 0.f, 2160.f);
			AimSpinMaxDps = FMath::Clamp(AimSpinMaxDps, AimSpinMinDps, 2160.f);
			AimSpinMinPull = FMath::Clamp(AimSpinMinPull, 0.f, 0.5f);
		}
	};
	static const FTune& Tune() { static FTune T; return T; }

	static double GLastHitStop = -10.0, GLastSound = -10.0, GLastLog = -10.0, GLastCrowd = -10.0;
	static int32 GCount[6] = { 0, 0, 0, 0, 0, 0 };
	// Per-player teeter state (PlayerIndex 0..7 safe)
	static double GLastTeeterWobble[8] = { -10, -10, -10, -10, -10, -10, -10, -10 };
	static double GLastTeeterGasp[8] = { -10, -10, -10, -10, -10, -10, -10, -10 };
	static bool GWasNearRim[8] = {};

	bool Enabled() { return Tune().bEnabled; }
	float HitStopSeconds() { return Tune().HitStopMs * 0.001f; }
	float HitStopScale() { return Tune().HitStopDilation; }
	float FlailVisibleSeconds() { return Enabled() && Tune().bFlail ? Tune().FlailVisibleSec : 0.f; }
	int32 Count(int32 Kind) { return Kind >= 0 && Kind < 6 ? GCount[Kind] : 0; }

	static double Now(UWorld* World) { return World ? World->GetRealTimeSeconds() : FPlatformTime::Seconds(); }

	static void Log(UWorld* World, bool bForce, const FString& Msg)
	{
		const double T = Now(World);
		if (!bForce && !Tune().bLogAll && T - GLastLog < 0.4) return;
		GLastLog = T;
		UE_LOG(LogZoogi, Log, TEXT("FEEL %s (hitstop %d shake %d sound %d teeter %d flail %d crowd %d)"),
			*Msg, GCount[0], GCount[1], GCount[2], GCount[3], GCount[4], GCount[5]);
	}

	static float StrengthFromImpulse(float Impulse, float MinI, float FullI)
	{
		if (Impulse < MinI) return 0.f;
		return FMath::Clamp((Impulse - MinI) / (FullI - MinI), 0.f, 1.f);
	}

	static bool IsHardOther(AActor* Other, const FHitResult& Hit)
	{
		if (!Other) return false;
		if (Cast<AZoogiBall>(Other) || Cast<AZoogiOrb>(Other) || Cast<AZoogiRock>(Other) || Cast<AZoogiArenaPiece>(Other) || Cast<AZoogiObstacle>(Other))
			return true;
		if (Hit.ImpactNormal.Z > 0.7f) return false;
		return FMath::Abs(Hit.ImpactNormal.Z) < 0.6f;
	}

	/** Positive = cm inside the active ring-out edge; <=0 means at/past the line. */
	static float DistInsideRim(const FVector& Loc)
	{
		const FVector2D P(Loc.X, Loc.Y);
		const float R = P.Size();
		float EdgeR = AZoogiArena::ActiveKnockoffR;
		// Frozen Ring 2: once past the inner knock-off line, the danger edge is OuterOutR.
		if (AZoogiArena::OuterOutR > 0.f && R > AZoogiArena::ActiveKnockoffR)
			EdgeR = AZoogiArena::OuterOutR;
		return EdgeR - R;
	}

	void TryHitStop(float Strength01, UWorld* World)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bHitStop || !World || Strength01 <= 0.02f) return;
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(World->GetFirstPlayerController());
		if (!PC || PC->IsMenuCam()) return;
		const double Tn = Now(World);
		if (Tn - GLastHitStop < T.HitStopMinInterval) return;
		const float Dur = HitStopSeconds() * (0.55f + 0.45f * Strength01);
		if (Dur <= 0.f) return;
		// Oct 24 (Evan): hit-stop removed - no world time dilation; physics keeps running at 1x
		if (!ZoogiUtil::ReactionPauses()) return;
		GLastHitStop = Tn;
		++GCount[0];
		Log(World, false, FString::Printf(TEXT("hitstop k=%.2f dur=%.3fs dil=%.2f"), Strength01, Dur, HitStopScale()));
	}

	void TryCameraShake(float Strength01, UWorld* World)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bCameraShake || !World || Strength01 <= 0.02f) return;
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(World->GetFirstPlayerController());
		if (!PC || PC->IsMenuCam()) return;
		const float K = FMath::Min(Strength01, T.ShakeCap);
		PC->OnImpactFeel(K);
		++GCount[1];
		Log(World, false, FString::Printf(TEXT("camshake k=%.2f"), K));
	}

	void PlayWeightCollision(AZoogiBall* Self, AActor* Other, float Impulse)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bWeightSounds || !Self || Impulse < T.SoundMinImpulse) return;
		UWorld* World = Self->GetWorld();
		if (!World) return;
		const double Tn = Now(World);
		if (Tn - GLastSound < T.SoundMinInterval) return;

		const float Mass = Self->Def.MassRatio();
		float OtherMass = Mass;
		if (const AZoogiBall* OB = Cast<AZoogiBall>(Other)) OtherMass = OB->Def.MassRatio();
		const float W = FMath::Sqrt(Mass * OtherMass);

		const TCHAR* Name = TEXT("HitCrack");
		float Pitch = 1.f;
		float Vol = FMath::Clamp(Impulse / 14000.f, 0.3f, 1.f);
		if (W < 0.85f)
		{
			Name = TEXT("HitClick");
			Pitch = FMath::Clamp(1.15f / FMath::Sqrt(W), 1.05f, 1.45f);
			Vol *= 0.85f;
		}
		else if (W > 1.25f)
		{
			Name = TEXT("HitCrack");
			Pitch = FMath::Clamp(0.85f / FMath::Sqrt(W / 1.2f), 0.55f, 0.92f);
			Vol = FMath::Min(Vol * 1.15f, 1.f);
		}
		else
		{
			Name = Cast<AZoogiBall>(Other) ? TEXT("HitCrack") : (Cast<AZoogiOrb>(Other) ? TEXT("OrbClink") : TEXT("Clack")); // b59: glass orbs clink, walls knock
			Pitch = FMath::Clamp(1.f / FMath::Sqrt(W), 0.75f, 1.2f) * FMath::FRandRange(0.96f, 1.06f);
		}

		ZoogiUtil::PlaySfx(Self, Name, Vol, Pitch);
		static const bool bSoftHits = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSoftHits"));
		if (!bSoftHits && Cast<AZoogiBall>(Other) && Impulse > 5000.f)
		{	// b166 (Evan: "I want more crushing sounds hard hitting"): a crunch that grows with the hit, a low boom on the hardest
			const float Kc = FMath::Clamp((Impulse - 5000.f) / 11000.f, 0.f, 1.f);
			ZoogiUtil::PlaySfx(Self, TEXT("HitCrush"), FMath::Lerp(0.45f, 1.f, Kc), FMath::Lerp(0.95f, 0.72f, Kc) * FMath::FRandRange(0.96f, 1.04f));
			if (Impulse > 11000.f) ZoogiUtil::PlaySfx(Self, TEXT("EpicImpact"), FMath::Lerp(0.35f, 0.75f, FMath::Clamp((Impulse - 11000.f) / 9000.f, 0.f, 1.f)), FMath::FRandRange(0.78f, 0.88f));
		}
		if (Impulse > 15000.f && Cast<AZoogiBall>(Other)) ZoogiEpic::DebrisBurst(World, (Self->BallLocation() + Cast<AZoogiBall>(Other)->BallLocation()) * 0.5f - FVector(0, 0, ZoogiConst::ZoogiRadius - 5.f), 0.3f); // b103: the hardest hits chip the floor
		if (W > 1.35f && Impulse > 9000.f)
		{
			ZoogiUtil::PlaySfx(Self, TEXT("GuardClang"), Vol * 0.35f, FMath::Clamp(Pitch * 0.85f, 0.5f, 0.95f));
		}
		GLastSound = Tn;
		++GCount[2];
		Log(World, false, FString::Printf(TEXT("sound %s w=%.2f vol=%.2f pitch=%.2f imp=%.0f"), Name, W, Vol, Pitch, Impulse));
	}

	void TryCrowd(AActor* Host, float Strength01, bool bRoar)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bCrowd || !Host || Strength01 <= 0.02f) return;
		UWorld* World = Host->GetWorld();
		if (!World) return;
		const double Tn = Now(World);
		if (Tn - GLastCrowd < T.CrowdMinInterval) return;
		const float Vol = FMath::Clamp(0.22f + 0.28f * Strength01, 0.18f, T.CrowdVolCap);
		const float Pitch = bRoar ? FMath::FRandRange(0.92f, 1.05f) : FMath::FRandRange(1.05f, 1.22f);
		// Reuse existing project audio only (no new packs): CrowdWave1 = short gasp bed, CrowdWave2 = fuller roar.
		ZoogiUtil::PlaySfx(Host, bRoar ? TEXT("CrowdWave2") : TEXT("CrowdWave1"), Vol, Pitch);
		GLastCrowd = Tn;
		++GCount[5];
		Log(World, false, FString::Printf(TEXT("crowd %s vol=%.2f k=%.2f"), bRoar ? TEXT("roar") : TEXT("gasp"), Vol, Strength01));
	}

	void TickNearRim(AZoogiBall* Ball, float DeltaSeconds)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bTeeter || !Ball || Ball->bOut || Ball->bParked || Ball->bOnLaunchPad) return;
		UWorld* World = Ball->GetWorld();
		if (!World || DeltaSeconds <= 0.f) return;
		const int32 Idx = FMath::Clamp(Ball->PlayerIndex, 0, 7);
		const float Inside = DistInsideRim(Ball->BallLocation());
		const bool bNear = Inside > 0.f && Inside < T.TeeterDistCm;
		const float Spd = Ball->PlanarSpeed();
		if (!bNear || Spd < T.TeeterMinSpeed)
		{
			GWasNearRim[Idx] = bNear && Spd >= T.TeeterMinSpeed;
			return;
		}
		const double Tn = Now(World);
		const float K = 1.f - (Inside / T.TeeterDistCm); // 0 at threshold, 1 at lip
		// Radial outward lean direction (away from arena centre).
		const FVector2D P(Ball->BallLocation().X, Ball->BallLocation().Y);
		const FVector OutDir = P.SizeSquared() > 1.f
			? FVector(P.X, P.Y, 0.f).GetSafeNormal()
			: FVector::ForwardVector;

		if (Tn - GLastTeeterWobble[Idx] >= T.TeeterWobbleInterval)
		{
			GLastTeeterWobble[Idx] = Tn;
			Ball->Wobble(T.TeeterWobbleDeg * (0.45f + 0.55f * K), OutDir);
			++GCount[3];
			Log(World, false, FString::Printf(TEXT("teeter P%d inside=%.0fcm k=%.2f"), Ball->PlayerIndex + 1, Inside, K));
		}
		// Gasp once when entering the teeter band (or after cooldown if still teetering hard).
		const bool bEnter = !GWasNearRim[Idx];
		if ((bEnter || K > 0.75f) && Tn - GLastTeeterGasp[Idx] >= T.TeeterGaspCooldown)
		{
			GLastTeeterGasp[Idx] = Tn;
			const float Vol = FMath::Clamp(0.28f + 0.22f * K, 0.25f, 0.5f);
			ZoogiUtil::PlaySfx(Ball, TEXT("Inhale"), Vol, FMath::Lerp(1.15f, 1.35f, K));
			TryCrowd(Ball, 0.35f + 0.25f * K, false); // soft crowd gasp under the inhale
			Log(World, true, FString::Printf(TEXT("teeter-gasp P%d k=%.2f"), Ball->PlayerIndex + 1, K));
		}
		GWasNearRim[Idx] = true;
	}

	void OnKnockOff(AZoogiBall* Ball)
	{
		ZoogiSoundscape::OnKnockOff(Ball); // b163: whoosh over the edge + the blast far below
		const FTune& T = Tune();
		if (!Enabled() || !Ball) return;
		UWorld* World = Ball->GetWorld();
		if (!World) return;

		const FVector Loc = Ball->BallLocation();
		const FVector V = Ball->PrevVelocity.SizeSquared() > 1.f ? Ball->PrevVelocity : Ball->GetBallVelocity();
		const FVector Dir = FVector(V.X, V.Y, 0.f).SizeSquared() > 1.f
			? FVector(V.X, V.Y, 0.f).GetSafeNormal()
			: FVector(Loc.X, Loc.Y, 0.f).GetSafeNormal();
		const bool bRivalKO = Ball->LastHitPlayer >= 0 && Ball->LastHitPlayer != Ball->PlayerIndex;

		if (T.bFlail)
		{
			// Limbless comic tumble: max visual spin + stretch + wobble (hooks still legal â€” bOut not set yet).
			Ball->SpinBoost(T.FlailSpinDps, FVector::UpVector);
			if (!Dir.IsNearlyZero())
			{
				Ball->Squash(T.FlailStretch, Dir);
				Ball->Wobble(T.FlailWobbleDeg, -Dir);
			}
			// Short Niagara spin cue (existing burst; no SpawnTrail touch).
			AZoogiFX::Niagara(World, TEXT("NS_Burst_Yellow"), Loc, FRotator::ZeroRotator, 1.15f);
			++GCount[4];
			Log(World, true, FString::Printf(TEXT("flail P%d ko=%d spin=%.0f"), Ball->PlayerIndex + 1, bRivalKO ? 1 : 0, T.FlailSpinDps));
		}

		// Crowd on knock-off / fall: roar for rival KO, gasp for self roll-off.
		TryCrowd(Ball, bRivalKO ? 0.95f : 0.55f, bRivalKO);
	}


	void StopAimSpin(AZoogiBall* Ball)
	{
		if (Ball) Ball->SetAimSpin(0.f, FVector::UpVector);
	}

	void TickAimSpin(AZoogiBall* Ball, float Pull01, const FVector& ShotDir)
	{
		const FTune& T = Tune();
		if (!Enabled() || !T.bAimSpin || !Ball || Ball->bOut) return;
		const float P = FMath::Clamp(Pull01, 0.f, 1.f);
		const FVector Dir = FVector(ShotDir.X, ShotDir.Y, 0.f);
		if (P < T.AimSpinMinPull || Dir.IsNearlyZero()) { StopAimSpin(Ball); return; }
		// Oct 23 (Evan t781u): further pull = faster Sonic spin. Mild ease-in so mid-pull still reads.
		const float E = FMath::Pow(P, 1.35f);
		const float Dps = FMath::Lerp(T.AimSpinMinDps, T.AimSpinMaxDps, E);
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Dir.GetSafeNormal());
		if (Axis.IsNearlyZero()) { StopAimSpin(Ball); return; }
		Ball->SetAimSpin(Dps, Axis);
	}
	void OnHardContact(AZoogiBall* Self, AActor* Other, float Impulse, const FHitResult& Hit)
	{
		if (!Enabled() || !Self || !IsHardOther(Other, Hit)) return;
		if (const AZoogiBall* OB = Cast<AZoogiBall>(Other))
		{
			if (Self->PlayerIndex > OB->PlayerIndex) return;
		}
		UWorld* World = Self->GetWorld();
		const FTune& T = Tune();
		const float StopK = StrengthFromImpulse(Impulse, T.HitStopMinImpulse, T.HitStopFullImpulse);
		const float ShakeK = StrengthFromImpulse(Impulse, T.ShakeMinImpulse, T.ShakeFullImpulse);
		if (StopK > 0.f) TryHitStop(StopK, World);
		if (ShakeK > 0.f) TryCameraShake(ShakeK, World);
		PlayWeightCollision(Self, Other, Impulse);

		// Item 27: crowd on big hits (Zoogi-Zoogi preferred for "highlight" read).
		if (T.bCrowd && Impulse >= T.CrowdGaspImpulse)
		{
			const bool bRoar = Impulse >= T.CrowdRoarImpulse && Cast<AZoogiBall>(Other) != nullptr;
			const float K = StrengthFromImpulse(Impulse, T.CrowdGaspImpulse, T.CrowdRoarImpulse * 1.25f);
			TryCrowd(Self, K, bRoar);
		}
	}
}
