// Oct 30 (Evan, build 26): ULTIMATE STRIKE - the Zoogi keeps rolling (Marble Clash core), but right before certain hits it bursts
// out of the ball into its Tripo ultimate form, throws one move timed to land on contact, then curls back into the ball.
// The sphere keeps doing all the physics underneath (the form is visual only and rides the ball), so the hit lands exactly where
// the shot would have anyway. More UZoogiFunSys members (ZoogiActionFun.h).
//  When: the ultimate is active this shot / an Awakened Zoogi's timing tap (CPU: automatically) / the winner of a power clash;
//        a knockout by a striking Zoogi ends with the form standing up and cheering.
//  Which move (automatic): heavy hits (ultimate, awakened, fast) -> the form's signature move (fire = front kick, foxes = slash,
//        crystal = punch combo); glancing cut shots -> slash; medium -> two-punch combo; light -> jab.
//  Forms: Vex / Hotstreak -> ember red, Tavi / Wolfgang -> white-gold fox A, Sera -> white-gold fox B, Blue / Luma -> ice crystal.
//  -ZoogiNoStrike = off, -ZoogiStrikeTest = on every predicted hit (captures fun_strike_*), -ZoogiStrikeYaw=<deg> mesh yaw offset.
#include "ZoogiActionFun.h"
#include "ZoogiPlayerController.h"
#include "ZoogiHaptics.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFabFX.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiObstacle.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "UObject/UObjectIterator.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiCrackFX.h"
#include "ZoogiPortraitCam.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiArena.h"

namespace ZoogiStrikeData
{
	struct FForm { const TCHAR* Folder; const TCHAR* Prefix; const TCHAR* Signature; FLinearColor Col; };
	static const FForm Forms[] = {
		{ TEXT("ult_ember_red"),  TEXT("tripo_convert_8a2b4778-1099-43cf-9973-f7d95d0322e2"), TEXT("front_kick_01"), FLinearColor(1.f, 0.4f, 0.08f) },
		{ TEXT("ult_fox_gold_A"), TEXT("tripo_convert_8f3e854a-c2aa-4bf0-899e-b9f9ee139d53"), TEXT("slash"),         FLinearColor(1.f, 0.85f, 0.35f) },
		{ TEXT("ult_fox_gold_B"), TEXT("tripo_convert_d01c5bdd-e9ed-47d4-8a65-86c9e7665b68"), TEXT("slash"),         FLinearColor(0.85f, 0.75f, 1.f) },
		{ TEXT("ult_frost_blue"), TEXT("tripo_convert_c56d5f01-e4d4-43ca-98a0-612995efea9a"), TEXT("box_02"),        FLinearColor(0.35f, 0.75f, 1.f) },
	};
	// seconds into each clip where the strike connects (the windup before it is skipped / stretched to meet the contact)
	static float HitTime(const FString& Move)
	{
		if (Move == TEXT("box_01")) return 0.30f;
		if (Move == TEXT("box_02")) return 0.42f;
		if (Move == TEXT("front_kick_01")) return 0.50f;
		if (Move == TEXT("slash")) return 0.45f;
		if (Move == TEXT("cast_a_spell")) return 0.60f;
		return 0.4f;
	}
}

int32 UZoogiFunSys::FormIndex(const AZoogiBall* B)
{
	if (!B) return -1;
	const FString N = B->Def.Name.ToLower();
	if (N == TEXT("vex") || N == TEXT("hotstreak")) return 0;
	if (N == TEXT("tavi") || N == TEXT("wolfgang")) return 1;
	if (N == TEXT("sera")) return 2;
	if (N == TEXT("blue") || N == TEXT("luma") || N.Contains(TEXT("zoogi blue"))) return 3;
	// build 27: Zoogis without their own Tripo form yet borrow the closest one so every Zoogi (and every CPU) can transform
	if (N == TEXT("dark zoogy") || N == TEXT("darkzoogy")) return 0;
	if (N == TEXT("master orin") || N == TEXT("bolt") || N == TEXT("wraps")) return 1;
	if (N == TEXT("moss") || N == TEXT("nightshade") || N == TEXT("pinpoint")) return 2;
	if (N == TEXT("lars")) return 3;
	return (int32)(GetTypeHash(N) % 4);
}

USkeletalMesh* UZoogiFunSys::FormMeshFor(const AZoogiBall* B)
{
	const int32 F = FormIndex(B);
	if (F < 0 || F >= (int32)UE_ARRAY_COUNT(ZoogiStrikeData::Forms)) return nullptr;
	const TCHAR* Fo = ZoogiStrikeData::Forms[F].Folder;
	return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fo, Fo, Fo), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FLinearColor UZoogiFunSys::FormColor(int32 F)
{
	return (F >= 0 && F < (int32)UE_ARRAY_COUNT(ZoogiStrikeData::Forms)) ? ZoogiStrikeData::Forms[F].Col : FLinearColor(1.f, 0.55f, 0.95f);
}

FString UZoogiFunSys::FormSignature(int32 F)
{
	return (F >= 0 && F < (int32)UE_ARRAY_COUNT(ZoogiStrikeData::Forms)) ? FString(ZoogiStrikeData::Forms[F].Signature) : FString(TEXT("box_02"));
}

FString UZoogiFunSys::PickMove(AZoogiGameMode* G, AZoogiBall* B, AActor* Target, bool bHeavy) const
{
	const int32 F = FormIndex(B);
	const FString Sig = F >= 0 ? ZoogiStrikeData::Forms[F].Signature : TEXT("box_01");
	if (bHeavy) return Sig;
	const FVector V = B->GetBallVelocity();
	const FVector2D V2(V.X, V.Y);
	const float Sp = V2.Size();
	if (Target && Sp > 50.f)
	{
		const FVector T = Target->GetActorLocation() - B->BallLocation();
		const FVector2D T2 = FVector2D(T.X, T.Y).GetSafeNormal();
		const float Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(V2 / Sp, T2), -1.f, 1.f)));
		if (Ang > 22.f) return TEXT("slash"); // glancing cut shot
	}
	if (Sp > 1300.f) return Sig;
	if (Sp > 850.f) return TEXT("box_02");
	return TEXT("box_01");
}

// b81: jagged branching cracks laid flat on the wall plane at P (normal N): thin dark slivers that fork outward, gone after 8 s
static void ZoogiWallCracks(UWorld* W, const FVector& P, const FVector& N0, float Radius)
{
	if (!W) return;
	const FVector N = N0.GetSafeNormal();
	FVector Ua = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
	if (Ua.IsNearlyZero()) Ua = FVector(1.f, 0.f, 0.f);
	const FVector Va = FVector::CrossProduct(N, Ua).GetSafeNormal();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(P), SP);
	if (!A) return;
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
	A->SetRootComponent(Root); Root->RegisterComponent();
	A->SetLifeSpan(8.f);
	UMaterialInterface* Dark = ZoogiUtil::SurfaceMID(A, FLinearColor(0.035f, 0.032f, 0.04f), 0.95f);
	UMaterialInterface* Edge = ZoogiUtil::SurfaceMID(A, FLinearColor(0.75f, 0.78f, 0.85f), 0.8f);
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	FRandomStream R((int32)(FPlatformTime::Cycles() & 0x7fffffff));
	int32 Segs = 0;
	auto Seg = [&](const FVector2D& A2, const FVector2D& B2, float Th)
	{
		if (Segs > 160) return;
		const FVector P0 = P + N * 1.5f + Ua * A2.X + Va * A2.Y, P1 = P + N * 1.5f + Ua * B2.X + Va * B2.Y;
		const FVector D = P1 - P0; const float L = D.Size(); if (L < 1.f) return;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetStaticMesh(Cube); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			C->SetupAttachment(Root); C->RegisterComponent();
			C->SetMaterial(0, Pass == 0 ? Edge : Dark);
			const float T2 = Pass == 0 ? Th * 1.8f : Th;
			C->SetWorldLocationAndRotation((P0 + P1) * 0.5f + N * (Pass == 0 ? 0.f : 0.6f), FRotationMatrix::MakeFromXZ(D / L, N).Rotator());
			C->SetWorldScale3D(FVector((L + T2 * 0.5f) / 100.f, T2 / 100.f, 0.012f));
			++Segs;
		}
	};
	TFunction<void(FVector2D, float, float, float, int32)> Crack = [&](FVector2D At, float Ang, float Len, float Th, int32 Depth)
	{
		float Left = Len;
		while (Left > 6.f)
		{
			const float Step = FMath::Min(Left, R.FRandRange(12.f, 26.f));
			Ang += FMath::DegreesToRadians(R.FRandRange(-24.f, 24.f));
			const FVector2D Nx = At + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Step;
			Seg(At, Nx, Th);
			At = Nx; Left -= Step; Th = FMath::Max(1.2f, Th * 0.9f);
			if (Depth < 2 && R.FRand() < 0.28f) Crack(At, Ang + FMath::DegreesToRadians(R.FRandRange(25.f, 55.f) * (R.FRand() < 0.5f ? -1.f : 1.f)), Left * R.FRandRange(0.35f, 0.6f), Th * 0.7f, Depth + 1);
		}
	};
	const int32 Arms = 7 + R.RandRange(0, 2);
	for (int32 k = 0; k < Arms; ++k) Crack(FVector2D::ZeroVector, UE_TWO_PI * k / Arms + R.FRandRange(-0.3f, 0.3f), Radius * R.FRandRange(0.55f, 1.f), R.FRandRange(4.f, 6.5f), 0);
	// a crushed ring round the impact
	FVector2D Prev(0.f, 0.f);
	for (int32 k = 0; k <= 10; ++k)
	{
		const float Ang = UE_TWO_PI * k / 10.f;
		const FVector2D Pt = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Radius * 0.22f * R.FRandRange(0.8f, 1.2f));
		if (k > 0) Seg(Prev, Pt, 3.f);
		Prev = Pt;
	}
	UE_LOG(LogZoogi, Log, TEXT("SPLAT wall cracks: %d slivers at (%.0f,%.0f,%.0f)"), Segs, P.X, P.Y, P.Z);
}

void UZoogiFunSys::StartStrike(AZoogiGameMode* G, AZoogiBall* B, AActor* Target, float TimeToHit, const FString& Move, bool bStand, float Life)
{
	UWorld* W = GetWorld();
	const int32 F = FormIndex(B);
	if (!W || !B || F < 0) return;
	if (Flung.Ball.Get() == B) return; // build 28: still flying in its form after a lost clash
	if (Strike.bOn) EndStrike(false);
	const ZoogiStrikeData::FForm& Fm = ZoogiStrikeData::Forms[F];
	const FString MeshPath = FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, Fm.Folder, Fm.Folder);
	const FString AnimName = FString::Printf(TEXT("%s_Anim_%s"), Fm.Prefix, *Move);
	const FString AnimPath = FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, *AnimName, *AnimName);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *AnimPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh || !Anim)
	{
		static int32 Warned = 0;
		if (Warned++ < 4) UE_LOG(LogZoogi, Warning, TEXT("FUN strike: missing %s%s"), Mesh ? TEXT("") : *MeshPath, Anim ? TEXT("") : *(TEXT(" ") + AnimPath));
		return;
	}
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(B->BallLocation()), SP);
	if (!A) return;
	USkeletalMeshComponent* SK = NewObject<USkeletalMeshComponent>(A, TEXT("Form"));
	SK->SetSkeletalMesh(Mesh);
	SK->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SK->SetCastShadow(true);
	SK->RegisterComponent();
	A->SetRootComponent(SK);
	static float Scale = 1.45f; static bool bScaleInit = false; // a touch bigger than the ball so the burst reads from the wide cameras
	if (!bScaleInit) { bScaleInit = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStrikeScale="), Scale); }
	SK->SetWorldScale3D(FVector(Scale));
	SK->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	if (bStand) AddFormLight(A, Fm.Col); // build 29: cut-in / cheer poses get a fill light (the forms read dark under arena lighting)
	const bool bLoop = bStand && Move == TEXT("cheer");
	SK->PlayAnimation(Anim, bLoop);
	float Rate = 1.f, Start = 0.f;
	if (!bStand)
	{
		const float Ht = ZoogiStrikeData::HitTime(Move);
		if (TimeToHit < Ht) Start = Ht - TimeToHit; // skip part of the windup
		else Rate = FMath::Clamp(Ht / FMath::Max(TimeToHit, 0.05f), 0.6f, 1.f); // stretch a slightly early start
		SK->SetPosition(Start, false);
		SK->SetPlayRate(Rate);
	}
	// the ball's own visuals hide while the form is out (the collision sphere keeps rolling underneath)
	Strike = FStrike();
	TInlineComponentArray<UPrimitiveComponent*> Prims(B);
	for (UPrimitiveComponent* P : Prims)
	{
		if (!P || P == B->Sphere.Get() || P == B->Ring.Get() || !P->GetVisibleFlag()) continue; // visible FLAG: Zoogi Blue's morph mesh re-syncs to Visual's flag every tick (Visual itself is hidden-in-game)
		P->SetVisibility(false, false);
		Strike.Hidden.Add(P);
	}
	Strike.bOn = true; Strike.Actor = A; Strike.Mesh = SK; Strike.Ball = B; Strike.Target = Target;
	Strike.Age = 0.f; Strike.Life = Life; Strike.bStand = bStand; Strike.Form = F; Strike.Move = Move;
	Strike.Yaw = (Target ? (Target->GetActorLocation() - B->BallLocation()).Rotation().Yaw : B->GetBallVelocity().Rotation().Yaw);
	if (bStand)
		if (APlayerController* PC = W->GetFirstPlayerController())
			if (PC->PlayerCameraManager) Strike.Yaw = (PC->PlayerCameraManager->GetCameraLocation() - B->BallLocation()).Rotation().Yaw;
	// burst: a ground ring + sparks only (a glow sphere here covered the form)
	AZoogiFX::Spawn(W, B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Fm.Col, 190.f, 0.4f, true, 7.f);
	AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), B->BallLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 0.5f);
	ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.6f, 0.75f);
	if (bStrikeTest && !bStand) // -ZoogiStrikeTest only: a side camera on the strike for the proof captures
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			const FVector L = B->BallLocation();
			const FVector Side = FRotator(0.f, Strike.Yaw + 90.f, 0.f).Vector();
			const FVector Eye = L + Side * 560.f + FVector(0, 0, 160.f);
			if (ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((L + FVector(0, 0, 40.f) - Eye).Rotation(), Eye), SP))
			{ Strike.TestCam = C; Strike.PrevView = PC->GetViewTarget(); PC->SetViewTarget(C); }
		}
	TickStrike(G, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("FUN ULTIMATE STRIKE P%d %s -> %s form, move %s (hit in %.2f s, clip start %.2f rate %.2f)%s"), B->PlayerIndex + 1, *B->Def.Name, Fm.Folder, *Move, TimeToHit, Start, Rate, bStand ? TEXT(" [stand]") : TEXT(""));
	if (bTest || bStrikeTest)
	{
		++StrikeShots;
		const FString Base = FString::Printf(TEXT("fun_strike_%02d_%s_%s"), StrikeShots, Fm.Folder, *Move);
		if (bStand) Shot(G, Base + TEXT("_stand"), 0.5f);
		else { Shot(G, Base + TEXT("_a"), FMath::Max(0.05f, TimeToHit - 0.12f)); Shot(G, Base + TEXT("_hit"), TimeToHit + 0.03f); Shot(G, Base + TEXT("_after"), TimeToHit + 0.25f); }
	}
}

void UZoogiFunSys::TickStrike(AZoogiGameMode* G, float Dt)
{
	if (!Strike.bOn) return;
	Strike.Age += Dt;
	AZoogiBall* B = Strike.Ball.Get();
	AActor* A = Strike.Actor.Get();
	if (!B || !A || B->bOut || Strike.Age >= Strike.Life) { EndStrike(true); return; }
	static float YawOff = -90.f; static bool bInit = false; // Mixamo skeletons face +Y after import
	if (!bInit) { bInit = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStrikeYaw="), YawOff); }
	const FVector L = B->BallLocation();
	if (!Strike.bStand)
	{
		float Want = Strike.Yaw;
		if (AActor* T = Strike.Target.Get()) if (ContactT < 0.f) Want = (T->GetActorLocation() - L).Rotation().Yaw;
		Strike.Yaw = FRotator::NormalizeAxis(Strike.Yaw + FRotator::NormalizeAxis(Want - Strike.Yaw) * FMath::Clamp(12.f * Dt, 0.f, 1.f));
	}
	A->SetActorLocationAndRotation(L - FVector(0, 0, ZoogiConst::ZoogiRadius), FRotator(0.f, Strike.Yaw + YawOff, 0.f));
	if (ACameraActor* C = Cast<ACameraActor>(Strike.TestCam.Get()))
	{
		C->SetActorRotation((L + FVector(0, 0, 40.f) - C->GetActorLocation()).Rotation());
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
	}
	if (!bStrikeLogged && Strike.Age > 0.05f)
	{
		bStrikeLogged = true;
		if (USkeletalMeshComponent* SK = Strike.Mesh.Get())
			UE_LOG(LogZoogi, Log, TEXT("FUN strike form check: at (%.0f, %.0f, %.0f) visible %d bounds r%.0f anim pos %.2f / %.2f"), A->GetActorLocation().X, A->GetActorLocation().Y, A->GetActorLocation().Z,
				SK->IsVisible() ? 1 : 0, SK->Bounds.SphereRadius, SK->GetPosition(), SK->AnimationData.AnimToPlay ? SK->AnimationData.AnimToPlay->GetPlayLength() : -1.f);
	}
}

void UZoogiFunSys::EndStrike(bool bFx)
{
	if (AActor* C = Strike.TestCam.Get())
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) if (AActor* Pv = Strike.PrevView.Get()) PC->SetViewTarget(Pv);
		C->Destroy();
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& P : Strike.Hidden) if (P.IsValid()) P->SetVisibility(true, false);
	if (AActor* A = Strike.Actor.Get())
	{
		if (bFx && GetWorld() && Strike.Form >= 0)
		{
			const FLinearColor C = ZoogiStrikeData::Forms[Strike.Form].Col;
			AZoogiFX::Spawn(GetWorld(), A->GetActorLocation() + FVector(0, 0, ZoogiConst::ZoogiRadius), C, 140.f, 0.3f, false, 10.f);
		}
		A->Destroy();
	}
	Strike = FStrike();
}

void UZoogiFunSys::PreloadForms()
{
	// load every form + move up front (the first on-demand load stalled while the skinned mesh got ready, so the strike was over
	// before the form could render); kept referenced for the whole session
	bFormsLoaded = true;
	static const TCHAR* Moves[] = { TEXT("box_01"), TEXT("box_02"), TEXT("front_kick_01"), TEXT("slash"), TEXT("cheer"), TEXT("cast_a_spell"), TEXT("idle"), TEXT("hit_to_body_01"), TEXT("fall") };
	int32 N = 0;
	for (const ZoogiStrikeData::FForm& Fm : ZoogiStrikeData::Forms)
	{
		if (UObject* M = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, Fm.Folder, Fm.Folder), nullptr, LOAD_NoWarn | LOAD_Quiet)) { FormKeep.Add(M); ++N; }
		for (const TCHAR* Mv : Moves)
		{
			const FString An = FString::Printf(TEXT("%s_Anim_%s"), Fm.Prefix, Mv);
			if (UObject* A = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet)) { FormKeep.Add(A); ++N; }
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("FUN strike forms preloaded: %d assets"), N);
}

void UZoogiFunSys::MaybeStrike(AZoogiGameMode* G)
{
	static const bool bOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoStrike"));
	if (!bOn || Strike.bOn || bStrikeThisShot || G->Phase != EZoogiPhase::Rolling) return;
	AZoogiBall* Sh = Shooter.Get();
	if (!Sh || Sh->bOut || FormIndex(Sh) < 0 || ContactT >= 0.f) return;
	AActor* T = PredTarget.Get();
	if (!T || PredT <= 0.05f || PredT > 0.42f) return;
	const bool bAwake = Players.IsValidIndex(Sh->PlayerIndex) && Players[Sh->PlayerIndex].bAwake;
	const bool bFire = Players.IsValidIndex(Sh->PlayerIndex) && Players[Sh->PlayerIndex].bFire;
	// build 27 (Evan): the AI transforms too - on fire always, otherwise a per-shot roll that favours hard hits
	if (Sh->bCPU && !bCpuStrikeRolled)
	{
		bCpuStrikeRolled = true;
		const float Sp = Sh->GetBallVelocity().Size2D();
		bCpuStrikeYes = bFire || FMath::FRand() < (Sp > 1100.f ? 0.5f : 0.25f);
		UE_LOG(LogZoogi, Log, TEXT("FUN CPU strike roll P%d: speed %.0f fire %d -> %s"), Sh->PlayerIndex + 1, Sp, bFire ? 1 : 0, bCpuStrikeYes ? TEXT("TRANSFORM") : TEXT("no"));
	}
	const bool bTrigger = bUltThisShot || (bAwake && (Sh->bCPU || bTapUsed)) || (bFire && bTapUsed) || (Sh->bCPU && bCpuStrikeYes) || bStrikeTest;
	if (!bTrigger) return;
	bStrikeThisShot = true;
	const bool bHeavy = bUltThisShot || bAwake || bFire;
	StartStrike(G, Sh, T, PredT, PickMove(G, Sh, T, bHeavy), false, PredT + 0.5f);
}

// ------------------------------------------------------------------ build 27: power clash fighters

namespace ZoogiClashTune
{
	static float Scale() { static float S = 2.1f; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashScale="), S); } return S; }
	static float GapFrac() { static float S = 0.46f; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashGap="), S); } return S; }
	static float CamK() { static float S = 2.6f; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashCam="), S); } return S; }
	static float YawOff() { static float S = -90.f; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStrikeYaw="), S); } return S; }
	constexpr float PunchRate = 1.3f;
}

void UZoogiFunSys::SpawnFighter(int32 Side, AZoogiBall* B, int32 F)
{
	FFighter& Fi = Clash.F[Side];
	Fi = FFighter();
	Fi.Form = F;
	UWorld* W = GetWorld();
	if (!W || !B || F < 0) return;
	const ZoogiStrikeData::FForm& Fm = ZoogiStrikeData::Forms[F];
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, Fm.Folder, Fm.Folder), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh) { UE_LOG(LogZoogi, Warning, TEXT("FUN clash: form mesh %s missing"), Fm.Folder); return; }
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(B->BallLocation()), SP);
	if (!A) return;
	USkeletalMeshComponent* SK = NewObject<USkeletalMeshComponent>(A, TEXT("ClashForm"));
	SK->SetSkeletalMesh(Mesh);
	SK->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SK->SetCastShadow(true);
	SK->RegisterComponent();
	A->SetRootComponent(SK);
	SK->SetWorldScale3D(FVector(ZoogiClashTune::Scale()));
	SK->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	A->CustomTimeDilation = 1.f / ClashDilation; // the world is nearly frozen; the fighters move at full speed
	TInlineComponentArray<UPrimitiveComponent*> Prims(B);
	for (UPrimitiveComponent* P : Prims)
	{
		if (!P || !P->GetVisibleFlag()) continue; // build 29: the Sphere too (a guard / boost bubble drawn on it glared over the fight)
		P->SetVisibility(false, false);
		Fi.Hidden.Add(P);
	}
	AddFormLight(A, Fm.Col);
	Fi.Actor = A; Fi.Mesh = SK;
	SK->UpdateBounds();
	const float H = FMath::Clamp(Mesh->GetBounds().BoxExtent.Z * 2.f * ZoogiClashTune::Scale(), 80.f, 600.f);
	Clash.Height = Side == 0 ? H : 0.5f * (Clash.Height + H);
	PlayFighter(Side, Side == 0 ? TEXT("box_01") : TEXT("box_02"), ZoogiClashTune::PunchRate, false);
	if (AZoogiFX* Fx = AZoogiFX::Spawn(W, B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Fm.Col, 220.f, 0.45f, true, 8.f)) Fx->CustomTimeDilation = 1.f / ClashDilation;
	UE_LOG(LogZoogi, Log, TEXT("FUN clash fighter %d: P%d %s -> %s form (height %.0f, %d ball parts hidden)"), Side, B->PlayerIndex + 1, *B->Def.Name, Fm.Folder, H, Fi.Hidden.Num());
}

void UZoogiFunSys::PlayFighter(int32 Side, const FString& Move, float Rate, bool bLoop, float Start)
{
	FFighter& Fi = Clash.F[Side];
	USkeletalMeshComponent* SK = Fi.Mesh.Get();
	if (!SK || Fi.Form < 0) return;
	const ZoogiStrikeData::FForm& Fm = ZoogiStrikeData::Forms[Fi.Form];
	const FString An = FString::Printf(TEXT("%s_Anim_%s"), Fm.Prefix, *Move);
	UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Anim) { UE_LOG(LogZoogi, Warning, TEXT("FUN clash: anim %s missing"), *An); return; }
	SK->PlayAnimation(Anim, bLoop);
	SK->SetPosition(Start, false);
	SK->SetPlayRate(Rate);
	Fi.Move = Move; Fi.T = 0.f;
	const float Len = Anim->GetPlayLength();
	const bool bPunch = Move.StartsWith(TEXT("box"));
	const float Ht = ZoogiStrikeData::HitTime(Move);
	Fi.Dur = bLoop ? 1e6f : FMath::Max(0.2f, (bPunch ? FMath::Min(Len, Ht + 0.3f) : Len) - Start) / FMath::Max(Rate, 0.1f); // punches cut after the recoil so the exchange stays fast
	Fi.HitAt = (bPunch || Move == FormSignature(Fi.Form)) ? FMath::Max(0.f, Ht - Start) / FMath::Max(Rate, 0.1f) : -1.f;
}

void UZoogiFunSys::TickFighters(AZoogiGameMode* G, float RealDt)
{
	UWorld* W = GetWorld();
	if (!W) return;
	// b104 ROOT CAUSE of the stuck camera (Evan: "the cam still gets stuck on the last camera view"): TickClash runs this right after
	// the duel ended in the same frame - Clash was already reset, so it spawned a NEW duel camera, took the view and nobody ever
	// ended it (until the human's aim-step watchdog). No clash = no fighters, no camera.
	if (!Clash.bOn) return;
	if (Clash.bField)
	{	// b104: a field duel - no forms, no duel camera; the HUD burst sits between the two balls
		Clash.HitScreen = FVector2D(-1.f, -1.f);
		if (APlayerController* PCf = W->GetFirstPlayerController()) { FVector2D Sf; if (PCf->ProjectWorldLocationToScreen(Clash.Center, Sf, true)) Clash.HitScreen = Sf; }
		return;
	}
	const float H = Clash.Height;
	const float Gap = H * ZoogiClashTune::GapFrac() * ((Clash.bDuel && !Clash.bFinish) ? 3.4f : 1.f); // b66: duel = RPG stance, apart
	const float FeetZ = Clash.At.Z - ZoogiConst::ZoogiRadius;
	// the clash point slides with the tap tug: the stronger tapper drives the other one back
	Clash.Center = Clash.At + Clash.Axis * (Clash.Ratio - 0.5f) * H * 0.9f;
	for (int32 s = 0; s < 2; ++s)
	{
		FFighter& Fi = Clash.F[s];
		AActor* A = Fi.Actor.Get();
		if (!A) continue;
		FVector P = Clash.Center + Clash.Axis * (s == 0 ? -Gap : Gap); P.Z = FeetZ;
		A->SetActorLocationAndRotation(P, FRotator(0.f, (s == 0 ? Clash.AxisYaw : Clash.AxisYaw + 180.f) + ZoogiClashTune::YawOff(), 0.f));
		Fi.T += RealDt;
		if (!Clash.bMeasured && s == 1 && Fi.T > 0.06f) // the posed (component) bounds once the anim has ticked: spacing + camera follow the real size
			if (USkeletalMeshComponent* SK = Fi.Mesh.Get())
			{
				Clash.bMeasured = true;
				const float Hm = SK->Bounds.BoxExtent.Z * 2.f;
				UE_LOG(LogZoogi, Log, TEXT("FUN clash fighters posed height %.0f (asset bounds said %.0f)"), Hm, Clash.Height);
				if (Hm > 60.f && Hm < 800.f) Clash.Height = FMath::Lerp(Clash.Height, Hm, 0.5f);
			}
		if (Clash.bFinish) continue;
		if (Fi.HitAt >= 0.f && Fi.T >= Fi.HitAt)
		{
			Fi.HitAt = -1.f; ++Clash.Hits; Clash.Hit = 1.f;
			ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 0.55f, FMath::FRandRange(0.8f, 1.15f));
		}
		if (Fi.T >= Fi.Dur && Clash.bDuel) { PlayFighter(s, TEXT("idle"), 1.f, true); continue; } // b66: duel moves are scripted (ZoogiDuel.cpp)
		if (Fi.T >= Fi.Dur) { ++Fi.Punch; PlayFighter(s, (Fi.Punch % 2) ? (s == 0 ? TEXT("box_02") : TEXT("box_01")) : (s == 0 ? TEXT("box_01") : TEXT("box_02")), ZoogiClashTune::PunchRate, false); }
	}
	if (Clash.bFinish)
	{
		const int32 Wn = Clash.bAttWins ? 0 : 1, Ls = 1 - Wn;
		FFighter& Fw = Clash.F[Wn];
		FFighter& Fl = Clash.F[Ls];
		if (!Clash.bFinishHit && (Fw.HitAt < 0.f || Fw.T >= Fw.HitAt))
		{
			Clash.bFinishHit = true; Clash.Hit = 1.8f;
			PlayFighter(Ls, TEXT("hit_to_body_01"), 1.f, false);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.55f);
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWorldFx"))) if (AActor* La = Fl.Actor.Get()) // b176b: the Cross Slash pack on every finisher
				AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_CrossSlash08.NS_CrossSlash08"), La->GetActorLocation() + FVector(0, 0, 60.f), 1.6f, 2.f, nullptr, FVector::ZeroVector, FRotator(0.f, Clash.AxisYaw, 0.f));
			UE_LOG(LogZoogi, Log, TEXT("FUN clash finisher lands (%s)"), *Fw.Move);
		}
		else if (Clash.bFinishHit && Fl.Move == TEXT("hit_to_body_01") && Fl.T > 0.4f) PlayFighter(Ls, TEXT("fall"), 1.1f, false);
	}
	Clash.Hit = FMath::Max(0.f, Clash.Hit - RealDt * 4.f);
	// medium side camera on the fight (attacker on the left, defender on the right)
	APlayerController* PC = W->GetFirstPlayerController();
	FVector Look = FVector(Clash.Center.X, Clash.Center.Y, FeetZ + H * 0.5f);
	const float CamDist = H * ZoogiClashTune::CamK() + Gap;
	ACameraActor* Cam = Cast<ACameraActor>(Clash.Cam.Get());
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiClashCam), false);
	for (const FFighter& Fi : Clash.F) if (Fi.Actor.IsValid()) Q.AddIgnoredActor(Fi.Actor.Get());
	if (Clash.Att.IsValid()) Q.AddIgnoredActor(Clash.Att.Get());
	if (Clash.Def.IsValid()) Q.AddIgnoredActor(Clash.Def.Get());
	if (!Cam)
	{
		// pick the open side: the side whose camera line is not blocked, else the one toward the arena centre
		FHitResult H0, H1;
		const FVector F0 = FRotator(0.f, Clash.AxisYaw - 90.f, 0.f).Vector();
		const bool bB0 = W->LineTraceSingleByChannel(H0, Look, Look - F0 * CamDist, ECC_Camera, Q);
		const bool bB1 = W->LineTraceSingleByChannel(H1, Look, Look + F0 * CamDist, ECC_Camera, Q);
		const float D0 = bB0 ? H0.Distance : CamDist, D1 = bB1 ? H1.Distance : CamDist;
		Clash.bCamFlip = D1 > D0 + 1.f || (FMath::IsNearlyEqual(D0, D1, 1.f) && (Look + F0 * CamDist).Size2D() < (Look - F0 * CamDist).Size2D());
		// both sides walled in (crates, stands, the pad): go up to a high side angle looking down on the fight
		Clash.bCamHigh = FMath::Max(D0, D1) < CamDist * 0.6f;
		UE_LOG(LogZoogi, Log, TEXT("FUN clash camera side %s%s (open %.0f / %.0f of %.0f)"), Clash.bCamFlip ? TEXT("flipped") : TEXT("normal"), Clash.bCamHigh ? TEXT(" HIGH") : TEXT(""), D0, D1, CamDist);
	}
	const FVector Fwd = FRotator(0.f, Clash.AxisYaw - 90.f + (Clash.bCamFlip ? 180.f : 0.f), 0.f).Vector();
	FVector Eye = Clash.bCamHigh ? Look - Fwd * CamDist * 0.8f + FVector(0, 0, H * 2.f) : Look - Fwd * CamDist + FVector(0, 0, H * 0.18f);
	if (Clash.bDuel && !Clash.bFinish)
	{
		// b66 (Evan): RPG stance - the turn player near, lower left; the rival far, middle to top (camera over the near one's right shoulder)
		const bool bNearAtt = Clash.bHumanAtt || !Clash.bHumanDef;
		AActor* NA = Clash.F[bNearAtt ? 0 : 1].Actor.Get(); AActor* FA = Clash.F[bNearAtt ? 1 : 0].Actor.Get();
		if (NA && FA)
		{
			const FVector Np = NA->GetActorLocation(), Fp = FA->GetActorLocation();
			FVector Dir = Fp - Np; Dir.Z = 0.f; if (!Dir.Normalize()) Dir = Clash.Axis;
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
			Eye = Np - Dir * H * 2.2f + Right * H * 0.9f + FVector(0, 0, H * 1.5f);
			Look = Fp + FVector(0, 0, H * 0.45f) - Right * H * 0.05f;
		}
	}
	{
		FHitResult Hc; // never behind a wall / stand (a trace that starts inside something is ignored)
		const FVector From = Look + FVector(0, 0, Clash.bCamHigh ? H * 0.8f : 0.f);
		if (W->LineTraceSingleByChannel(Hc, From, Eye, ECC_Camera, Q) && !Hc.bStartPenetrating && Hc.Distance > CamDist * 0.35f) Eye = Hc.Location + (Look - Eye).GetSafeNormal() * 30.f;
	}
	if (!Cam && PC)
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - Eye).Rotation(), Eye), SP);
		if (Cam)
		{
			Cam->GetCameraComponent()->SetFieldOfView(55.f);
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			// build 29: tame the glare on the fight camera (less bloom, no lens flares, exposure held)
			FPostProcessSettings& PP = Cam->GetCameraComponent()->PostProcessSettings;
			PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.25f;
			PP.bOverride_LensFlareIntensity = true; PP.LensFlareIntensity = 0.f;
			Cam->GetCameraComponent()->PostProcessBlendWeight = 1.f;
			Clash.Cam = Cam; Clash.PrevView = PC->GetViewTarget();
		}
	}
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(ZoogiPortrait::SceneFit(W, Look, 55.f, Eye)); // b67: portrait pulls back (whole scene in view)
		const FVector L = FMath::VInterpTo(Cam->GetActorLocation(), Eye, RealDt, 6.f);
		Cam->SetActorLocationAndRotation(L, (Look - L).Rotation());
		if (PC && PC->GetViewTarget() != Cam) PC->SetViewTarget(Cam);
	}
	// where the fists meet, for the HUD energy burst
	Clash.HitScreen = FVector2D(-1.f, -1.f);
	if (PC) { FVector2D S; if (PC->ProjectWorldLocationToScreen(FVector(Clash.Center.X, Clash.Center.Y, FeetZ + H * 0.66f), S, true)) Clash.HitScreen = S; }
}

void UZoogiFunSys::EndFighters()
{
	UWorld* W = GetWorld();
	RestoreHiddenFx();
	if (AActor* C = Clash.Cam.Get())
	{
		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		{	// b98b: the match camera, not whatever cam was live when the duel began (it could be gone or frozen)
			if (AZoogiPlayerController* ZPC = Cast<AZoogiPlayerController>(PC)) ZPC->RestoreMatchCam(0.f);
			else if (AActor* Pv = Clash.PrevView.Get()) PC->SetViewTarget(Pv);
		}
		C->Destroy();
		LastDuelEndAt = FPlatformTime::Seconds();
	}
	Clash.Cam.Reset(); Clash.PrevView.Reset();
	for (FFighter& Fi : Clash.F)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : Fi.Hidden) if (P.IsValid()) P->SetVisibility(true, false);
		Fi.Hidden.Reset();
		if (AActor* A = Fi.Actor.Get()) A->Destroy();
		Fi.Actor.Reset(); Fi.Mesh.Reset();
	}
}

// ------------------------------------------------------------------ build 28: the clash loser flies off in its form

void UZoogiFunSys::StartFlung(int32 Side)
{
	EndFlung(false);
	if (Side < 0 || Side > 1) return;
	FFighter& Fi = Clash.F[Side];
	AActor* A = Fi.Actor.Get();
	AZoogiBall* B = Side == 0 ? Clash.Att.Get() : Clash.Def.Get();
	if (!A || !B || B->bOut) return;
	// take the fighter over from the clash (EndFighters must not destroy it or show the ball again)
	Flung.Actor = A; Flung.Mesh = Fi.Mesh; Flung.Ball = B; Flung.Form = Fi.Form;
	Flung.Hidden = MoveTemp(Fi.Hidden);
	Fi.Actor.Reset(); Fi.Mesh.Reset(); Fi.Hidden.Reset();
	A->CustomTimeDilation = 1.f; // the world is back to normal speed
	// the clash camera follows the flying body for a moment (then hands back to the match camera)
	if (AActor* C = Clash.Cam.Get())
	{
		Flung.Cam = C; Flung.PrevView = Clash.PrevView;
		Flung.CamOff = (C->GetActorLocation() - B->BallLocation()) * 1.35f;
		Clash.Cam.Reset(); Clash.PrevView.Reset();
	}
	const float S = 1.6f;
	if (USkeletalMeshComponent* SK = Flung.Mesh.Get())
	{
		SK->SetWorldScale3D(FVector(S));
		const ZoogiStrikeData::FForm& Fm = ZoogiStrikeData::Forms[FMath::Clamp(Flung.Form, 0, 3)];
		const FString An = FString::Printf(TEXT("%s_Anim_fall"), Fm.Prefix);
		if (UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{ SK->PlayAnimation(Anim, false); SK->SetPlayRate(1.2f); }
	}
	Flung.Half = Clash.Height * (S / 2.1f) * 0.45f;
	Flung.Yaw = Clash.AxisYaw + (Side == 0 ? 0.f : 180.f);
	// b58 (Evan): "instead of flying randomly like a ball ... like he's doing a backstroke, but he's flying backwards" - the same form
	// is handed to a posed puppet that lies back and windmills its arms, head first along the knock
	static const bool bNoStroke = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBackstroke"));
	Flung.Away = (Side == 0 ? -Clash.Axis : Clash.Axis).GetSafeNormal2D(); // the loser flies away from the winner
	if (Flung.Away.IsNearlyZero()) Flung.Away = B->GetBallVelocity().GetSafeNormal2D();
	if (!bNoStroke && Flung.Mesh.IsValid() && Cast<USkeletalMesh>(Flung.Mesh->GetSkinnedAsset()))
	{
		const float Hgt = FMath::Max(60.f, (float)Cast<USkeletalMesh>(Flung.Mesh->GetSkinnedAsset())->GetBounds().BoxExtent.Z * 2.f * S);
		if (AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(GetWorld(), B, B->BallLocation(), Flung.Yaw, EZoogiPuppetPose::Backstroke, Hgt, Flung.Mesh.Get(), false))
		{
			P->Travel = Flung.Away; P->Center = B->BallLocation() + FVector(0.f, 0.f, 40.f);
			Flung.Pup = P;
			ZoogiUtil::PlaySfx(B, TEXT("Whoosh"), 0.9f, 0.85f); // b59
			A->SetActorHiddenInGame(true);
			UE_LOG(LogZoogi, Log, TEXT("FUN clash loser P%d backstrokes away (%.0f cm form)"), B->PlayerIndex + 1, Hgt);
			if (AZoogiGameMode* Gm = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) { Shot(Gm, TEXT("backstroke_a"), 0.2f); Shot(Gm, TEXT("backstroke_b"), 0.45f); }
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("FUN clash loser P%d %s flies off in its %s form"), B->PlayerIndex + 1, *B->Def.Name, ZoogiStrikeData::Forms[FMath::Clamp(Flung.Form, 0, 3)].Folder);
}

void UZoogiFunSys::TickFlung(AZoogiGameMode* G, float Dt)
{
	AActor* A = Flung.Actor.Get();
	AZoogiBall* B = Flung.Ball.Get();
	if (!A || !B) { EndFlung(false); return; }
	if (B->bOut || B->IsHidden()) { EndFlung(false); return; } // rang out: the knockout flow takes it from here
	Flung.Age += Dt;
	FVector V = B->GetBallVelocity(); // b76: not const - a wall bounce restores the flight direction
	FHitResult SplatForce; // b76: set when it bounced off a wall / corner
	const float Sp = V.Size();
	if (Sp > 60.f) Flung.Yaw = FRotator::NormalizeAxis(FMath::FixedTurn(Flung.Yaw, V.Rotation().Yaw + 180.f, 360.f * Dt)); // looks back at the hit it took
	Flung.Spin += FMath::Clamp(Sp, 0.f, 2400.f) * Dt * 0.35f; // tumbles while it flies
	Flung.Calm = Sp < 140.f ? Flung.Calm + Dt : 0.f;
	// b67 (Evan: "knocked back after losing a clash into the stands: the stands crack and that player star-limbs flat on the wall for a
	// moment, then back to gameplay"). -ZoogiNoSplat = off.
	static const bool bNoSplat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSplat"));
	auto WallAhead = [&](float Dist, FHitResult& H) -> bool
	{
		const FVector Dir = V.GetSafeNormal2D();
		if (Dir.IsNearlyZero()) return false;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiSplat), false, B);
		QP.AddIgnoredActor(A); if (AActor* Pp = Flung.Pup.Get()) QP.AddIgnoredActor(Pp);
		for (AZoogiBall* O : G->Balls) if (O) QP.AddIgnoredActor(O);
		for (AZoogiOrb* O : G->Orbs) if (O) QP.AddIgnoredActor(O);
		const FVector P0 = B->BallLocation() + FVector(0.f, 0.f, 20.f);
		// b76 (Evan: "the loser should splat even if it hits a corner of the wall"): a sphere sweep straight on plus two angled ones
		static const float Fan[3] = { 0.f, 28.f, -28.f };
		for (float Ang : Fan)
		{
			const FVector D = Dir.RotateAngleAxis(Ang, FVector::UpVector);
			if (GetWorld()->SweepSingleByChannel(H, P0, P0 + D * Dist * (Ang == 0.f ? 1.f : 0.75f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius * 0.7f), QP)
				&& !Cast<AZoogiOrb>(H.GetActor()) && FMath::Abs(H.ImpactNormal.Z) < 0.85f) return true; // a wall / corner / seat bank, not the floor
		}
		return false;
	};
	// b76: it already bounced off something hard (the physics turned it around) = a wall / corner hit
	{
		static FVector PrevV = FVector::ZeroVector; static TWeakObjectPtr<AZoogiBall> PrevB;
		if (PrevB.Get() != B) { PrevB = B; PrevV = V; }
		const FVector Pv = PrevV; PrevV = V;
		if (!bNoSplat && !Flung.bSplat && Flung.Age > 0.1f && Pv.Size2D() > 350.f && FVector::DotProduct(Pv.GetSafeNormal2D(), V.GetSafeNormal2D()) < 0.2f && G)
		{
			FHitResult Hb;
			FCollisionQueryParams Qb(SCENE_QUERY_STAT(ZoogiSplatB), false, B); Qb.AddIgnoredActor(A);
			for (AZoogiBall* O : G->Balls) if (O) Qb.AddIgnoredActor(O);
			const FVector P0 = B->BallLocation() + FVector(0.f, 0.f, 20.f);
			for (AZoogiOrb* O : G->Orbs) if (O) Qb.AddIgnoredActor(O);
			if (GetWorld()->SweepSingleByChannel(Hb, P0, P0 + Pv.GetSafeNormal2D() * (ZoogiConst::ZoogiRadius * 2.5f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius * 0.9f), Qb)
				&& FMath::Abs(Hb.ImpactNormal.Z) < 0.85f)
			{
				V = Pv; // splat toward where it was flying
				SplatForce = Hb;
			}
		}
	}
	if (Flung.bSplat)
	{
		Flung.SplatAge += Dt;
		if (B->Sphere) B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); // it is stuck on the wall
		A->SetActorHiddenInGame(true);
		if (AActor* C = Flung.Cam.Get())
		{	// a look at the dent: in front of the wall (further back in portrait so the whole splat fits)
			int32 Vx = 16, Vy = 9; if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->GetViewportSize(Vx, Vy);
			const float Back = 420.f; (void)Vx; (void)Vy; // portrait: SceneFit below pulls back
			FVector Want = Flung.SplatAt + Flung.SplatN * Back + FVector(0.f, 0.f, 90.f);
			if (ACameraActor* Ca = Cast<ACameraActor>(C)) if (Ca->GetCameraComponent()) Ca->GetCameraComponent()->SetFieldOfView(ZoogiPortrait::SceneFit(GetWorld(), Flung.SplatAt, 70.f, Want));
			const FVector L = FMath::VInterpTo(C->GetActorLocation(), Want, Dt, 6.f);
			C->SetActorLocationAndRotation(L, (Flung.SplatAt + FVector(0.f, 0.f, 60.f) - L).Rotation());
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
		}
		if (AActor* Sp2 = Flung.SplatPup.Get()) // a small slide down the wall at the end
			if (Flung.SplatAge > 1.0f) Sp2->SetActorLocation(Sp2->GetActorLocation() - FVector(0.f, 0.f, 120.f * Dt));
		if (Flung.SplatAge > 1.35f)
		{
			if (AActor* Sp2 = Flung.SplatPup.Get()) Sp2->Destroy();
			if (B->Sphere) B->Sphere->SetPhysicsLinearVelocity(Flung.SplatN * 160.f);
			UE_LOG(LogZoogi, Log, TEXT("FUN splat over -> back to the game"));
			EndFlung(true);
		}
		return;
	}
	if (!bNoSplat && G && (Sp > 250.f || SplatForce.ImpactNormal.SizeSquared() > 0.f) && Flung.Age > 0.1f)
	{
		FHitResult H;
		if (SplatForce.bBlockingHit || SplatForce.ImpactNormal.SizeSquared() > 0.f) H = SplatForce;
		if ((SplatForce.ImpactNormal.SizeSquared() > 0.f) || WallAhead(ZoogiConst::ZoogiRadius + 30.f + Sp * Dt * 2.f, H))
		{
			Flung.bSplat = true; Flung.bCrash = true; Flung.SplatAge = 0.f;
			Flung.SplatN = FVector(H.ImpactNormal.X, H.ImpactNormal.Y, 0.f).GetSafeNormal();
			if (Flung.SplatN.IsNearlyZero()) Flung.SplatN = -V.GetSafeNormal2D();
			Flung.SplatAt = H.ImpactPoint;
			if (AActor* Pp = Flung.Pup.Get()) { Pp->Destroy(); Flung.Pup.Reset(); }
			UWorld* W = GetWorld();
			// b81 (Evan: "the wall should have generated cracks like smashing the snowman platform, not a crack form that doesn't align"):
			// a stage piece cracks its own surface (the piece crack material + shards); anything else gets cracks generated along the wall
			static const bool bOldCrack = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSplatCrack"));
			AZoogiArenaPiece* HitPiece = Cast<AZoogiArenaPiece>(H.GetActor());
			if (bOldCrack)
			{
				if (AZoogiCrackFX* Cr = AZoogiCrackFX::Spawn(W, H.ImpactPoint, 170.f, AZoogiCrackFX::StyleForMap(W), 3.5f, 1.3f, TEXT("splat on the stands")))
					Cr->SetActorLocationAndRotation(H.ImpactPoint + Flung.SplatN * 3.f, FRotationMatrix::MakeFromZ(Flung.SplatN).Rotator());
			}
			else if (HitPiece && !HitPiece->IsIndestructible() && !HitPiece->IsBroken())
				HitPiece->SetPieceState(EZoogiPieceState::Cracked, H.ImpactPoint, -Flung.SplatN, true);
			else
				ZoogiWallCracks(W, H.ImpactPoint, Flung.SplatN, 190.f);
			AZoogiFX::Spawn(W, H.ImpactPoint, FLinearColor(1.f, 0.9f, 0.7f), 190.f, 0.3f, true, 12.f);
			// star-limbed, flat on the wall, facing the court
			const float Hgt = FMath::Max(120.f, Flung.Half * 2.f);
			if (AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, B, H.ImpactPoint + Flung.SplatN * 22.f - FVector(0.f, 0.f, Hgt * 0.35f), Flung.SplatN.Rotation().Yaw, EZoogiPuppetPose::Splat, Hgt, Flung.Mesh.Get(), false))
				Flung.SplatPup = P;
			A->SetActorHiddenInGame(true);
			ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 1.f, 0.85f);
			ZoogiUtil::PlaySfx(G, TEXT("HitCrack"), 1.f, 0.7f);
			ZoogiHaptics::Rumble(5.f, 1.f, W, TEXT("clash loser splat")); // the hard 5 s rumble of the loser scene
			if (!Flung.Cam.IsValid())
			{	// a fresh close-up for the splat
				FActorSpawnParameters SPc; SPc.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((-Flung.SplatN).Rotation(), H.ImpactPoint + Flung.SplatN * 700.f + FVector(0.f, 0.f, 160.f)), SPc))
				{
					if (C->GetCameraComponent()) { C->GetCameraComponent()->SetConstraintAspectRatio(false); C->GetCameraComponent()->SetFieldOfView(70.f); }
					if (APlayerController* PC = W->GetFirstPlayerController()) { Flung.PrevView = PC->GetViewTarget(); PC->SetViewTargetWithBlend(C, 0.f); }
					Flung.Cam = C;
				}
			}
			Flung.Age = FMath::Min(Flung.Age, 1.0f); // keep the close-up through the splat
			UE_LOG(LogZoogi, Log, TEXT("FUN clash loser P%d SPLAT on the stands at (%.0f, %.0f, %.0f)"), B->PlayerIndex + 1, H.ImpactPoint.X, H.ImpactPoint.Y, H.ImpactPoint.Z);
			if (G->ShotDir.Len()) { G->TakeShotDelayed(TEXT("splat_a"), 0.25f); G->TakeShotDelayed(TEXT("splat_b"), 0.9f); }
			return;
		}
	}
	if (AZoogiAurexPuppet* P = Cast<AZoogiAurexPuppet>(Flung.Pup.Get()))
	{
		// b58 backstroke: lies back, head first along the flight, arms windmilling; sits up into the old landing as it slows
		if (Sp > 200.f) { const FVector Tr = V.GetSafeNormal2D(); if (!Tr.IsNearlyZero()) P->Travel = Tr; }
		P->Center = B->BallLocation() + FVector(0.f, 0.f, 40.f * FMath::Clamp(Sp / 600.f, 0.f, 1.f));
		// b59 (Evan): "the screen transitions into Smoke and transitions into the anime clip" of the loser crashing into this map's landmark
		FHitResult Hw;
		if (!Flung.bCrash && Flung.Age > 0.5f && !bNoSplat && WallAhead(1100.f, Hw)) Flung.bCrash = true; // b67: headed into the stands - the splat plays instead of the crash clip
		if (!Flung.bCrash && Flung.Age > 0.5f)
		{
			Flung.bCrash = true;
			if (UZoogiAnimeSubsystem* An = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>())
			{
				FString Id = (G && G->Arena) ? An->CrashClipFor((uint8)G->Arena->GetTheme()) : FString();
				FString TestId; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiCrashTestClip="), TestId) && Id.IsEmpty()) Id = TestId;
				static const bool bNoCrash = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCrashClip"));
				if (!bNoCrash && !Id.IsEmpty() && An->PlayCrash(Id, ZoogiUtil::PlayerColor(B->PlayerIndex)))
				{
					UE_LOG(LogZoogi, Log, TEXT("FUN clash loser P%d crash clip %s"), B->PlayerIndex + 1, *Id);
					ZoogiHaptics::Rumble(5.f, 1.f, GetWorld(), TEXT("clash loser crash")); // b66 (Evan): hard 5 s rumble - smashed against a wall
				}
			}
		}
		if (Sp < 260.f && Flung.Age > 0.4f) { A->SetActorHiddenInGame(false); P->Destroy(); Flung.Pup.Reset(); } // back to the form's own fall clip for the landing
	}
	static float YawOff = -90.f; static bool bInit = false;
	if (!bInit) { bInit = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStrikeYaw="), YawOff); }
	const float SpinK = FMath::Clamp(Sp / 600.f, 0.f, 1.f); // stands back up as it slows
	const FRotator R(0.f, Flung.Yaw + YawOff, FMath::Fmod(Flung.Spin, 360.f) * SpinK);
	A->SetActorLocationAndRotation(B->BallLocation() - R.RotateVector(FVector(0.f, 0.f, Flung.Half)), R);
	if (AActor* C = Flung.Cam.Get())
	{
		if (Flung.Age > 1.6f) ReleaseFlungCam();
		else
		{
			FVector Want = B->BallLocation() + Flung.CamOff;
			if (ACameraActor* Ca = Cast<ACameraActor>(C)) if (Ca->GetCameraComponent()) Ca->GetCameraComponent()->SetFieldOfView(ZoogiPortrait::SceneFit(GetWorld(), B->BallLocation(), 55.f, Want)); // b67: portrait
			const FVector L = FMath::VInterpTo(C->GetActorLocation(), Want, Dt, 7.f);
			C->SetActorLocationAndRotation(L, (B->BallLocation() - L).Rotation());
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
		}
	}
	if (Flung.Calm > 0.35f || Flung.Age > 3.5f) EndFlung(true);
}

void UZoogiFunSys::ReleaseFlungCam()
{
	if (AActor* C = Flung.Cam.Get())
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) if (AActor* Pv = Flung.PrevView.Get()) PC->SetViewTarget(Pv);
		C->Destroy();
	}
	Flung.Cam.Reset(); Flung.PrevView.Reset();
}

void UZoogiFunSys::EndFlung(bool bFx)
{
	ReleaseFlungCam();
	if (Flung.Actor.IsValid()) LastDuelEndAt = FPlatformTime::Seconds(); // b98b
	if (AActor* Sp = Flung.SplatPup.Get()) Sp->Destroy();
	if (AActor* P = Flung.Pup.Get()) P->Destroy();
	for (const TWeakObjectPtr<UPrimitiveComponent>& P : Flung.Hidden) if (P.IsValid()) P->SetVisibility(true, false);
	if (AActor* A = Flung.Actor.Get())
	{
		if (bFx && GetWorld())
		{
			const FLinearColor C = FormColor(Flung.Form);
			if (AZoogiBall* B = Flung.Ball.Get()) AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 150.f, 0.35f, false, 10.f);
		}
		A->Destroy();
	}
	Flung = FFlung();
}

// ------------------------------------------------------------------ build 29: glare control + form fill light

void UZoogiFunSys::AddFormLight(AActor* A, const FLinearColor& Tint)
{
	if (!A || !A->GetRootComponent()) return;
	UPointLightComponent* L = NewObject<UPointLightComponent>(A, TEXT("FormFill"));
	L->SetupAttachment(A->GetRootComponent());
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(45.f);
	L->SetAttenuationRadius(520.f);
	L->SetLightColor(FMath::Lerp(FLinearColor::White, Tint, 0.25f));
	L->SetCastShadows(false);
	L->RegisterComponent();
	L->SetWorldLocation(A->GetActorLocation() + FVector(0.f, 0.f, 170.f));
}

void UZoogiFunSys::HideFxAround(const FVector& At, float Radius, const AActor* Keep0, const AActor* Keep1)
{
	UWorld* W = GetWorld();
	if (!W) return;
	int32 Gone = 0, Hid = 0;
	for (TActorIterator<AZoogiFX> It(W); It; ++It) { It->Destroy(); ++Gone; }
	if (UZoogiShowSys* Sh = W->GetSubsystem<UZoogiShowSys>()) { Sh->ClearSparks(); Sh->bQuiet = true; } // the glowing spark balls (impact bursts)
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* N = *It;
		if (!N || N->GetWorld() != W || !N->IsRegistered() || !N->GetVisibleFlag()) continue;
		if (FVector::DistSquared(N->GetComponentLocation(), At) > FMath::Square(Radius)) continue;
		N->SetVisibility(false, true); ClashHiddenFx.Add(N); ++Hid;
	}
	// glow spheres / shields / aura meshes of any other actor right around the fight (clones, card bubbles, boost auras)
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* Ac = *It;
		if (!Ac || Ac == Keep0 || Ac == Keep1 || Cast<AZoogiArena>(Ac) || Cast<AZoogiArenaPiece>(Ac) || Cast<AZoogiObstacle>(Ac) || Cast<AZoogiOrb>(Ac) || Cast<AZoogiBall>(Ac) || Cast<APlayerController>(Ac)) continue;
		if (Ac->GetActorEnableCollision()) { bool bSolid = false; TInlineComponentArray<UPrimitiveComponent*> Cp(Ac); for (UPrimitiveComponent* P : Cp) bSolid |= P && P->IsCollisionEnabled(); if (bSolid) continue; } // props with collision (rocks, toppings) stay
		if (!Ac->GetRootComponent() || FVector::DistSquared(Ac->GetActorLocation(), At) > FMath::Square(Radius * 0.35f)) continue;
		bool bFighter = false;
		for (const FFighter& Fi : Clash.F) bFighter |= Fi.Actor.Get() == Ac;
		if (bFighter || Ac == Strike.Actor.Get() || Ac == Flung.Actor.Get()) continue;
		TInlineComponentArray<UPrimitiveComponent*> Prims(Ac);
		for (UPrimitiveComponent* P : Prims) if (P && P->GetVisibleFlag()) { P->SetVisibility(false, false); ClashHiddenFx.Add(P); ++Hid; }
	}
	UE_LOG(LogZoogi, Log, TEXT("FUN glare control: cleared %d bursts, hid %d FX / glow parts within %.0f cm"), Gone, Hid, Radius);
}

void UZoogiFunSys::RestoreHiddenFx()
{
	for (const TWeakObjectPtr<USceneComponent>& C : ClashHiddenFx) if (C.IsValid()) C->SetVisibility(true, C->IsA<UNiagaraComponent>());
	ClashHiddenFx.Reset();
	if (UWorld* W = GetWorld()) if (UZoogiShowSys* Sh = W->GetSubsystem<UZoogiShowSys>()) Sh->bQuiet = false;
}
