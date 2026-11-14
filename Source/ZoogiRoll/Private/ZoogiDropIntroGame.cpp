// AZoogiGameMode side of the live DROP INTRO (see ZoogiDropIntro.h).

// Music + SFX intentionally untouched for the whole sequence (Evan Oct 23 steer: keep music AND SFX).

#include "ZoogiGameMode.h"

#include "ZoogiDropIntro.h"

#include "ZoogiSpawnElevator.h"

#include "ZoogiBall.h"

#include "ZoogiArena.h"

#include "ZoogiFX.h"

#include "ZoogiFeel.h"

#include "ZoogiRoll.h"
#include "ZoogiAnimeSubsystem.h"

#include "ZoogiTypes.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"

#include "Components/SphereComponent.h"

#include "Engine/StaticMesh.h"

#include "Engine/World.h"

#include "GameFramework/Actor.h"

#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "Misc/Parse.h"

#include "HAL/PlatformTime.h"



static void DestroyDropShip(FZoogiDropIntro& D)

{

	if (AActor* S = D.ShipActor.Get()) { S->Destroy(); }

	D.ShipActor = nullptr;

	D.bShipSpawned = false;

}



static AActor* SpawnDropShip(UWorld* World, const AZoogiArena* Arena, FZoogiDropIntro& D)
{
	DestroyDropShip(D);
	if (!World) return nullptr;
	const FTransform X = ZoogiDropIntro::ShipWorldXform(Arena);
	AActor* Ship = World->SpawnActor<AActor>(AActor::StaticClass(), X);
	if (!Ship) return nullptr;

	// Root scene (no SharedPlatform / Hull-only stand-in). Attach EVERY modeled VIP PlatformShip Part.
	USceneComponent* Root = NewObject<USceneComponent>(Ship, TEXT("ShipRoot"));
	Root->SetMobility(EComponentMobility::Movable);
	Ship->SetRootComponent(Root);
	Root->RegisterComponent();

	TArray<UStaticMesh*> Parts;
	ZoogiDropIntro::CollectShipPartMeshes(Parts);
	int32 Attached = 0;
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMesh* M = Parts[i];
		if (!M) continue;
		const FString PartName = M->GetName();
		// Kill frosted white bulbs: Canopy (top dome), Nose (front-left bulb), SpotHousing, Dome/Frost names.
		if (PartName.Contains(TEXT("Canopy")) || PartName.Contains(TEXT("canopy"))
			|| PartName.Contains(TEXT("Dome")) || PartName.Contains(TEXT("dome"))
			|| PartName.Contains(TEXT("Frost")) || PartName.Contains(TEXT("frost"))
			|| PartName.Contains(TEXT("Hull_Nose")) || PartName.Contains(TEXT("SpotHousing")))
		{
			UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: SKIP white-bulb part %s"), *PartName);
			continue;
		}
		const FName CompName(*FString::Printf(TEXT("ShipPart_%d_%s"), i, *PartName));
		UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(Ship, CompName);
		SMC->SetMobility(EComponentMobility::Movable);
		SMC->SetupAttachment(Root);
		SMC->RegisterComponent();
		SMC->SetStaticMesh(M);
		SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SMC->SetCastShadow(true);
		SMC->SetVisibility(true);
		SMC->SetHiddenInGame(false);
		SMC->bReceivesDecals = false;
		++Attached;
	}

	Ship->SetActorTransform(X);
	D.ShipActor = Ship;
	D.bShipSpawned = true;
	// Cabin lights: interior fill (deck/glass readable from inside) + soft key on glass band.
	{
		UPointLightComponent* Fill = NewObject<UPointLightComponent>(Ship, TEXT("ShipCabinFill"));
		Fill->SetMobility(EComponentMobility::Movable);
		Fill->SetupAttachment(Root);
		Fill->RegisterComponent();
		Fill->SetRelativeLocation(FVector(0.f, 40.f, 140.f)); // inside cabin above deck
		Fill->SetIntensity(9000.f);
		Fill->SetAttenuationRadius(900.f);
		Fill->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
		Fill->SetCastShadows(false);
	}
	{
		UPointLightComponent* Key = NewObject<UPointLightComponent>(Ship, TEXT("ShipKeyLight"));
		Key->SetMobility(EComponentMobility::Movable);
		Key->SetupAttachment(Root);
		Key->RegisterComponent();
		Key->SetRelativeLocation(FVector(0.f, -200.f, 130.f)); // toward Glass_Panorama band
		Key->SetIntensity(7000.f);
		Key->SetAttenuationRadius(1400.f);
		Key->SetLightColor(FLinearColor(1.f, 0.92f, 0.78f));
		Key->SetCastShadows(false);
	}
UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: modeled PlatformShip spawned parts=%d at (%.0f, %.0f, %.0f)"),
		Attached, X.GetLocation().X, X.GetLocation().Y, X.GetLocation().Z);
	return Ship;
}





/** Anime land contact: squash + multi-layer smoke poof + shockwave + bounce kick (matches ZoogiAnime.mp4 feel). */

static void FireDropLandAnime(UWorld* World, AZoogiBall* B, int32 P, FZoogiDropIntro& D)

{

	if (!World || !B || !B->Sphere) return;

	const FVector L = B->BallLocation();

	const FVector Ground(L.X, L.Y, ZoogiConst::ZoogiRadius);

	const FLinearColor C = ZoogiUtil::PlayerColor(P);

	B->Squash(-0.15f, FVector::UpVector);

	AZoogiFX::Niagara(World, TEXT("NS_Dust"), Ground, FRotator::ZeroRotator, 1.95f);

	AZoogiFX::Niagara(World, TEXT("NS_Dust"), Ground + FVector(48.f, -36.f, 6.f), FRotator(0.f, 55.f, 0.f), 1.4f);

	AZoogiFX::Niagara(World, TEXT("NS_Burst_White"), Ground, FRotator::ZeroRotator, 1.55f);

	AZoogiFX::Niagara(World, TEXT("NS_Sparks"), Ground + FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator, 0.85f);

	AZoogiFX::KFX(World, TEXT("Effect6_Collision"), Ground, 0.95f, 2.4f, nullptr);

	AZoogiFX::Shockwave(World, Ground, EZoogiShockwaveSize::Small, EZoogiShockwaveKind::Flat, &C, false);

	AZoogiFX::Spawn(World, Ground, C, 160.f, 0.55f, true, 7.5f);

	AZoogiFX::SpawnConfetti(World, Ground + FVector(0.f, 0.f, 20.f), C, 18);

	AZoogiFX::SpawnMark(World, Ground, C * 0.55f, 90.f, 2.8f);

	ZoogiUtil::PlaySfx(B, TEXT("HitCrack"), 0.95f, 0.92f);

	ZoogiUtil::PlaySfx(B, TEXT("HitClick"), 0.7f, 1.35f);

	ZoogiUtil::PlaySfx(B, TEXT("Clack"), 0.45f, 1.55f);

	ZoogiFeel::TryHitStop(0.85f, World);

	ZoogiFeel::TryCameraShake(0.9f, World);

	const FVector Cur = B->Sphere->GetPhysicsLinearVelocity();

	const float Kick = D.BounceImpulseZ;

	B->Sphere->WakeRigidBody();

	B->Sphere->SetPhysicsLinearVelocity(FVector(Cur.X * 0.35f, Cur.Y * 0.35f, Kick));

	B->SpinBoost(420.f, FVector::CrossProduct(FVector::UpVector, FVector(Cur.X, Cur.Y, 0.f).GetSafeNormal() + FVector(0.2f, 0.1f, 0.f)));

	D.bLandPoofFired = true;

	D.bBounceSettling = true;

	D.LandContactAge = 0.f;

	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: ANIME LAND bounce+poof P%d at (%.0f, %.0f) kick=%.0f"), P + 1, Ground.X, Ground.Y, Kick);

}

void AZoogiGameMode::BeginDropIntro()

{

	const int32 N = Balls.Num();

	DropIntro.Reset();

	if (!FZoogiDropIntro::Enabled() || N == 0 || FrozenRoamOn())

	{

		// t782u elevator plan: DropIntro off. Keep Zoogis on individual spawn spots so AZoogiSpawnElevator

		// can rise them (PlayAll already fired / fires alongside). Skip pad-opening ParkUnderPad which would

		// yank waiters mid-rise; mark opening complete and start the first turn like FinishDropIntro handoff.

		UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: skipped (disabled or no players) -> spawn elevators / turn"));

		if (AZoogiSpawnElevator::IsEnabled())

		{

			const TCHAR* Cmd = FCommandLine::Get();

			int32 Seed = 0;

			const bool bFixed = FParse::Value(Cmd, TEXT("ZoogiMatchSeed="), Seed);

			FRandomStream R(bFixed ? (Seed ^ 0xD20F11) : (int32)(FPlatformTime::Cycles() ^ 0xD20F11));

			int32 First = ParseLaunchFirst();

			if (First < 0) First = (N > 0) ? R.RandRange(0, N - 1) : 0;

			else First = First % N;

			bLaunchFlow = LaunchFlowEnabled() && Arena && Arena->GetLaunchPad(); // Oct 30: no pad (field start) except Cosmic

			LaunchFlow.Reset();

			OnPad.Init(false, N);

			PadAnchor.Init(FVector::ZeroVector, N);

			PadPhasePlayer = -1;

			if (bLaunchFlow && N > 0)

			{

				LaunchFlow.BeginOpeningWithFirst(N, First);

				for (int32 i = 0; i < N; ++i) LaunchFlow.Launched[i] = true;

				LaunchFlow.bOpening = false;

				LaunchFlow.bGameplayStarted = true;

			}

			RoundStartPlayer = First;

			BeginTurn(First);

		}

		else

		{

			BeginOpeningLaunches();

		}

		return;

	}

	// First dropper: same seed rules as launch flow (reproducible under -ZoogiMatchSeed).

	const TCHAR* Cmd = FCommandLine::Get();

	int32 Seed = 0;

	const bool bFixed = FParse::Value(Cmd, TEXT("ZoogiMatchSeed="), Seed);

	FRandomStream R(bFixed ? (Seed ^ 0xD20F11) : (int32)(FPlatformTime::Cycles() ^ 0xD20F11));

	int32 First = ParseLaunchFirst();

	if (First < 0) First = R.RandRange(0, N - 1);

	else First = First % N;

	DropIntro.Begin(N, First);

	Phase = EZoogiPhase::DropIntro;

	PhaseTime = 0.f;

	ActivePlayer = First;

	bCinematicCam = false; // Tac path for the handoff (straight-down variant owns framing)



// t773u/t774u: VIDEO-ONLY DropIntro when DropIntro.mp4 exists (no live 3D PlatformShip).
	// ZERO Zoogis in the intro video. SpawnDropShip only as silent fallback if movie missing.
	DropIntro.bIntroMovie = false;
	DropIntro.bFadeToTac = false;
	DropIntro.ShipHoldSec = 5.0f;
	if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>())
	{
		const FString Clip = Anime->PlayMenuMovieById(TEXT("DropIntro"));
		if (!Clip.IsEmpty())
		{
			DropIntro.bIntroMovie = true;
			DropIntro.ShipHoldSec = 5.0f; // Movies.csv / encoded length
			UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: playing empty-cabin video %s.mp4 (no 3D ship)"), *Clip);
		}
	}
	if (!DropIntro.bIntroMovie)
	{
		UE_LOG(LogZoogi, Warning, TEXT("DROP INTRO: DropIntro.mp4 missing - live empty ship hold %.1fs"), DropIntro.ShipHoldSec);
		SpawnDropShip(GetWorld(), Arena, DropIntro);
	}

	for (int32 i = 0; i < N; ++i)
	{
		AZoogiBall* B = Balls.IsValidIndex(i) ? Balls[i] : nullptr;
		if (!B) continue;
		// Skip character model at start: park off-camera, hidden until Tac / Drop.
		B->ParkAt(FVector(0.f, 0.f, 5000.f + i * 120.f), 0.f);
		B->SetActorHiddenInGame(true);
		if (B->Visual) B->Visual->SetVisibility(false);
		if (B->Nubs) B->Nubs->SetVisibility(false);
		B->SetActiveTurn(i == First);
	}
	// Music already started at MatchStart — do NOT PlayMusic/DuckMusic/mute here (Evan: keep music + SFX).
	ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.35f);
	AddFeed(TEXT("DROP INTRO"), ZoogiUtil::PlayerColor(First));
	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: empty-cabin intro (movie=%s), P%d first of %d"), DropIntro.bIntroMovie ? TEXT("yes") : TEXT("no"), First + 1, N);
}




void AZoogiGameMode::SetDropPick(const FVector& WorldPoint)

{

	if (!IsDropPick()) return;

	FVector P = WorldPoint;

	P.Z = ZoogiConst::ZoogiRadius;

	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;

	const float R = FVector2D(P.X, P.Y).Size();

	const float MaxR = FMath::Max(200.f, KR - 180.f);

	if (R > MaxR)

	{

		const FVector2D N2 = FVector2D(P.X, P.Y).GetSafeNormal() * MaxR;

		P.X = N2.X; P.Y = N2.Y;

	}

	if (Arena && Arena->IsOffCourt(FVector2D(P.X, P.Y)))

	{

		UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: pick rejected off-court (%.0f, %.0f)"), P.X, P.Y);

		return;

	}

	P = FindFreeSpot(P);

	DropIntro.PickPoint = P;

	DropIntro.bHasPick = true;

	AZoogiFX::Spawn(GetWorld(), P, ZoogiUtil::PlayerColor(DropIntro.ActiveDrop), 80.f, 0.85f, true, 4.5f);

	ZoogiUtil::PlaySfx(this, TEXT("UiClick"), 0.35f, 1.2f);

	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: P%d pick (%.0f, %.0f)"), DropIntro.ActiveDrop + 1, P.X, P.Y);

}



bool AZoogiGameMode::ConfirmDrop()

{

	if (!IsDropPick()) return false;

	const int32 P = DropIntro.ActiveDrop;

	if (!Balls.IsValidIndex(P) || !Balls[P]) return false;

	if (!DropIntro.bHasPick)

	{

		// Default: spawn-ish free spot toward a ring if they hit Drop without picking.

		const float A = 2.f * PI * (float)P / (float)FMath::Max(1, Balls.Num());

		const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;

		SetDropPick(FVector(FMath::Cos(A) * KR * 0.45f, FMath::Sin(A) * KR * 0.45f, ZoogiConst::ZoogiRadius));

		if (!DropIntro.bHasPick) return false;

	}

	AZoogiBall* B = Balls[P];

	const FVector Land = DropIntro.PickPoint;

	const FVector Release(Land.X, Land.Y, Land.Z + DropIntro.DropHeight);

	B->SetActorHiddenInGame(false);
	if (B->Visual) { B->Visual->SetVisibility(true); B->Visual->SetHiddenInGame(false); }
	if (B->Nubs) B->Nubs->SetVisibility(B->Nubs->GetStaticMesh() != nullptr && ZoogiUtil::ShowFlipperNubs());

	B->PlaceAt(Release, B->AimYaw); // enables physics + restores Visual (see PlaceAt)
	// t776u: shot from ship — lateral + down impulse so orbs bounce/react, not placed resting.
	{
		const float Spread = 420.f + FMath::FRandRange(0.f, 260.f);
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Shot(FMath::Cos(Ang) * Spread, FMath::Sin(Ang) * Spread, -480.f + FMath::FRandRange(-120.f, 60.f));
		if (B->Sphere)
		{
			B->Sphere->SetPhysicsLinearVelocity(Shot);
			B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector(
				FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-180.f, 180.f)));
		}
	}

	B->SetActiveTurn(true);

	B->Squash(0.12f, FVector::UpVector);

	B->SpinBoost(280.f, FVector(0.2f, 1.f, 0.f));

	ActivePlayer = P;

	DropIntro.Step = EZoogiDropStep::Falling;

	DropIntro.StepAge = 0.f;

	DropIntro.bLandPoofFired = false;

	DropIntro.bBounceSettling = false;

	DropIntro.LandContactAge = 0.f;

	DropIntro.FallTrailTicks = 0;

	ZoogiUtil::PlaySfx(this, TEXT("UiConfirm"), 0.5f);

	ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.35f, 1.4f);

	AZoogiFX::Spawn(GetWorld(), Land, ZoogiUtil::PlayerColor(P), 90.f, 0.45f, true, 3.0f);

	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Sparks"), Release, FRotator::ZeroRotator, 0.7f);

	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: P%d DROP from z=%.0f onto (%.0f, %.0f)"), P + 1, Release.Z, Land.X, Land.Y);

	return true;

}



void AZoogiGameMode::FinishDropIntro()

{

	DestroyDropShip(DropIntro);

	const int32 First = DropIntro.FirstPlayer;

	const int32 N = Balls.Num();

	// Ensure every Zoogi is on the court (safety if a fall timed out mid-air).

	for (int32 i = 0; i < N; ++i)

	{

		AZoogiBall* B = Balls.IsValidIndex(i) ? Balls[i] : nullptr;

		if (!B) continue;

		if (B->bParked || B->BallLocation().Z > ZoogiConst::ZoogiRadius + 200.f)

		{

			const float A = 2.f * PI * (float)i / (float)FMath::Max(1, N);

			const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;

			const FVector Spot = FindFreeSpot(FVector(FMath::Cos(A) * KR * 0.4f, FMath::Sin(A) * KR * 0.4f, ZoogiConst::ZoogiRadius));

			B->PlaceAt(Spot, B->AimYaw);

		}

		B->bParked = false;

		B->bOnLaunchPad = false;

		B->SetActiveTurn(false);

	}

	// Hand off: skip pad opening launches — everyone already dropped into the arena.

	bLaunchFlow = LaunchFlowEnabled() && Arena && Arena->GetLaunchPad(); // Oct 30: no pad (field start) except Cosmic

	LaunchFlow.Reset();

	if (bLaunchFlow && N > 0)

	{

		LaunchFlow.BeginOpeningWithFirst(N, First);

		for (int32 i = 0; i < N; ++i) LaunchFlow.Launched[i] = true;

		LaunchFlow.bOpening = false;

		LaunchFlow.bGameplayStarted = true;

	}

	RoundStartPlayer = First;

	DropIntro.Step = EZoogiDropStep::Done;

	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: DONE — handoff to normal round from P%d (no further cinematic; music+sfx continue)"), First + 1);

	AddFeed(TEXT("FIGHT!"), FLinearColor(1.f, 0.85f, 0.3f));

	BeginTurn(First);

}



void AZoogiGameMode::SkipDropIntro()

{

	if (Phase != EZoogiPhase::DropIntro) return;

	UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: skipped"));

	// Place everyone on free court spots and finish.

	const int32 N = Balls.Num();

	for (int32 i = 0; i < N; ++i)

	{

		if (!Balls.IsValidIndex(i) || !Balls[i]) continue;

		const float A = 2.f * PI * (float)i / (float)FMath::Max(1, N);

		const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;

		Balls[i]->PlaceAt(FindFreeSpot(FVector(FMath::Cos(A) * KR * 0.4f, FMath::Sin(A) * KR * 0.4f, ZoogiConst::ZoogiRadius)), Balls[i]->AimYaw);

		if (DropIntro.Dropped.IsValidIndex(i)) DropIntro.Dropped[i] = true;

	}

	FinishDropIntro();

}



void AZoogiGameMode::TickDropIntro(float Dt)

{

	if (Phase != EZoogiPhase::DropIntro) return;

	DropIntro.StepAge += Dt;

	if (DropIntro.CutFlashAge >= 0.f) DropIntro.CutFlashAge += Dt;

	switch (DropIntro.Step)

	{

	case EZoogiDropStep::ShipHold:
	{
		// Empty ship bob only (no Zoogi cabin seats). Video covers when bIntroMovie.
		if (AActor* Ship = DropIntro.ShipActor.Get())
		{
			const FTransform Base = ZoogiDropIntro::ShipWorldXform(Arena);
			const float Bob = FMath::Sin(DropIntro.StepAge * 2.1f) * 12.f;
			const float Sway = FMath::Sin(DropIntro.StepAge * 1.35f) * 6.f;
			FTransform X = Base;
			X.SetLocation(Base.GetLocation() + FVector(Sway, 0.f, Bob));
			Ship->SetActorTransform(X);
		}
		// Keep Zoogis hidden for the whole intro video.
		for (int32 i = 0; i < Balls.Num(); ++i)
		{
			AZoogiBall* B = Balls.IsValidIndex(i) ? Balls[i] : nullptr;
			if (!B) continue;
			B->SetActorHiddenInGame(true);
		}

		const bool bVideoDone = DropIntro.StepAge >= DropIntro.ShipHoldSec;
		if (bVideoDone && !DropIntro.bFadeToTac)
		{
			// Stop intro movie; start black FADE into Tac (not hard cut with cabin Zoogis).
			if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>())
				Anime->StopMenuMovie();
			DropIntro.bIntroMovie = false;
			DropIntro.bFadeToTac = true;
			DropIntro.CutFlashSec = DropIntro.FadeSec;
			DropIntro.CutFlashAge = 0.f;
			DropIntro.bHardCutPending = true; // snap Tac under the fade
			DropIntro.Step = EZoogiDropStep::TacPick;
			DropIntro.StepAge = 0.f;
			DropIntro.bHasPick = false;
			ActivePlayer = DropIntro.ActiveDrop;
			DestroyDropShip(DropIntro);
			for (int32 i = 0; i < Balls.Num(); ++i)
			{
				AZoogiBall* B = Balls[i];
				if (!B) continue;
				if (DropIntro.Dropped.IsValidIndex(i) && DropIntro.Dropped[i]) continue;
				B->ParkAt(FVector(0.f, 0.f, 5000.f + i * 120.f), 0.f);
				B->SetActorHiddenInGame(true);
				B->SetActiveTurn(i == DropIntro.ActiveDrop);
			}
			if (AZoogiBall* Cur = Balls.IsValidIndex(DropIntro.ActiveDrop) ? Balls[DropIntro.ActiveDrop] : nullptr)
				Cur->SetActorHiddenInGame(false);
			ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.4f, 1.05f);
			UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: FADE -> Tac bird's-eye, P%d picks Drop spot"), DropIntro.ActiveDrop + 1);
			ZoogiUtil::PlaySfx(this, TEXT("UiClick"), 0.45f, 0.85f);
		}
		break;
	}

	case EZoogiDropStep::TacPick:

	{

		AZoogiBall* Cur = Balls.IsValidIndex(DropIntro.ActiveDrop) ? Balls[DropIntro.ActiveDrop] : nullptr;

		const bool bDropAuto = FZoogiDropIntro::AutoDrive() || (Cur && Cur->bCPU);

		if (bDropAuto && DropIntro.StepAge >= DropIntro.AutoPickDelay)

		{

			if (!DropIntro.bHasPick)

			{

				const float A = 2.f * PI * (float)DropIntro.ActiveDrop / (float)FMath::Max(1, Balls.Num()) + DropIntro.ActiveDrop * 0.35f;

				const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;

				SetDropPick(FVector(FMath::Cos(A) * KR * 0.42f, FMath::Sin(A) * KR * 0.42f, ZoogiConst::ZoogiRadius));

			}

			ConfirmDrop();

		}

		break;

	}

	case EZoogiDropStep::Falling:

	{

		AZoogiBall* B = Balls.IsValidIndex(DropIntro.ActiveDrop) ? Balls[DropIntro.ActiveDrop] : nullptr;

		if (!B)

		{

			if (DropIntro.OnLanded()) FinishDropIntro();

			break;

		}

		const FVector L = B->BallLocation();

		const FVector V = B->GetBallVelocity();

		// Fall trail (anime freefall streak).

		if (!DropIntro.bLandPoofFired && L.Z > ZoogiConst::ZoogiRadius + 40.f)

		{

			if ((++DropIntro.FallTrailTicks % 3) == 0)

				AZoogiFX::SpawnTrail(GetWorld(), L, FLinearColor(0.25f, 0.65f, 1.f), 32.f, 0.32f, 4.0f); // Evan: blue trail (Effects Free Arrow Trail bar)

		}

		// First ground contact -> anime bounce + smoke poof (do NOT PlaceAt yet).

		if (!DropIntro.bLandPoofFired)

		{

			const bool bContact = (L.Z <= ZoogiConst::ZoogiRadius + 14.f && V.Z < 40.f)

				|| (L.Z <= ZoogiConst::ZoogiRadius + 6.f);

			const bool bTimeout = DropIntro.StepAge >= DropIntro.FallTimeout;

			if (bContact || bTimeout)

				FireDropLandAnime(GetWorld(), B, DropIntro.ActiveDrop, DropIntro);

		}

		else if (DropIntro.bBounceSettling)

		{

			DropIntro.LandContactAge += Dt;

			if (DropIntro.LandContactAge > 0.12f && DropIntro.LandContactAge < 0.18f && V.Z > 80.f)

				B->Squash(0.1f, FVector::UpVector);

			if (DropIntro.LandContactAge >= DropIntro.BounceHoldSec || DropIntro.StepAge >= DropIntro.FallTimeout)

			{

				FVector Spot = B->BallLocation();

				Spot.Z = ZoogiConst::ZoogiRadius;

				Spot = FindFreeSpot(Spot);

				B->PlaceAt(Spot, B->AimYaw);

				B->SetActorHiddenInGame(false);

				DropIntro.bBounceSettling = false;

				ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.35f, 1.1f);

				if (DropIntro.OnLanded()) FinishDropIntro();

				else

				{

					ActivePlayer = DropIntro.ActiveDrop;

					for (int32 i = 0; i < Balls.Num(); ++i)

						if (Balls[i]) Balls[i]->SetActiveTurn(i == DropIntro.ActiveDrop);

					if (AZoogiBall* Nxt = Balls.IsValidIndex(DropIntro.ActiveDrop) ? Balls[DropIntro.ActiveDrop] : nullptr)

						Nxt->SetActorHiddenInGame(false);

					UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: next dropper P%d"), DropIntro.ActiveDrop + 1);

				}

			}

		}

		break;

	}

	default: break;

	}

}



