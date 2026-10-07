#include "ZoogiKFXPreview.h"
#include "ZoogiFX.h"
#include "ZoogiAwakenFX.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"

static FVector KFXBase(30000.f, 0.f, 0.f); // -ZoogiKFXBase=x,y,z moves the stage (e.g. up into open sky, away from the arena hills)

AZoogiKFXPreview::AZoogiKFXPreview()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	RootComponent = Floor;
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Floor);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = 70.f;
}

void AZoogiKFXPreview::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), ShotDir);
	if (ShotDir.IsEmpty()) ShotDir = FPaths::ProjectSavedDir() / TEXT("Shots/kfx");
	{
		FString B;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiKFXBase="), B)) { TArray<FString> P; B.ParseIntoArray(P, TEXT(",")); if (P.Num() == 3) KFXBase = FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])); }
	}
	SetActorLocation(KFXBase);
	if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		Floor->SetStaticMesh(Plane);
		Floor->SetWorldScale3D(FVector(400.f, 400.f, 1.f));
		// grey-beige packed dirt like the KriptoFX demo yard, so glow / decals read against a similar ground
		Floor->SetMaterial(0, ZoogiUtil::SurfaceMID(this, FLinearColor(0.20f, 0.18f, 0.16f), 0.9f));
	}
	RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
	RT->InitAutoFormat(1280, 720);
	RT->UpdateResourceImmediate(true);
	Capture->TextureTarget = RT;
	Capture->SetUsingAbsoluteScale(true);
	Capture->SetWorldLocationAndRotation(KFXBase + FVector(-720.f, -260.f, 380.f), (FVector(0.f, 0.f, 110.f) - FVector(-720.f, -260.f, 380.f)).Rotation());

	FString PlanFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiKFXPlan="), PlanFile) && LoadPlan(PlanFile))
	{
		UE_LOG(LogZoogi, Log, TEXT("KFX plan: %d shots -> %s"), Plan.Num(), *ShotDir);
		T = -2.f;
		return;
	}
	Prefabs = { TEXT("Effect4"), TEXT("Effect4_Hand"), TEXT("Effect5") }; // Oct 7 scope: only Effect4/Effect5 (+ Effect25 from kfx-vfx-ap) stay in the game
	{
		// two passes: the first warms up shaders compiled on demand (editor -game), the second pass's shots overwrite it
		const TArray<FString> Once = Prefabs;
		Prefabs.Append(Once);
	}
	UE_LOG(LogZoogi, Log, TEXT("KFX preview: %d systems -> %s"), Prefabs.Num(), *ShotDir);
	T = -2.f; // let the world settle
}

static FVector ParseVec(const FString& S)
{
	TArray<FString> P; S.ParseIntoArray(P, TEXT(","));
	return FVector(P.IsValidIndex(0) ? FCString::Atof(*P[0]) : 0.f, P.IsValidIndex(1) ? FCString::Atof(*P[1]) : 0.f, P.IsValidIndex(2) ? FCString::Atof(*P[2]) : 0.f);
}

bool AZoogiKFXPreview::LoadPlan(const FString& File)
{
	// Plan format, one directive per line ('#' comments):
	//   shot <name>
	//   cam <x,y,z> <lookx,looky,lookz> <fov>
	//   ev <prefab> <time> <x,y,z> <scale> [<vx,vy,vz>] [maxlife]
	//   prop <x,y,z> <height> [radius]
	//   times <t0,t1,...>
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *File)) { UE_LOG(LogZoogi, Warning, TEXT("KFX plan: cannot read %s"), *File); return false; }
	for (FString L : Lines)
	{
		L.TrimStartAndEndInline();
		if (L.IsEmpty() || L.StartsWith(TEXT("#"))) continue;
		TArray<FString> W; L.ParseIntoArrayWS(W);
		if (W[0] == TEXT("shot") && W.Num() >= 2) { FPlanShot S; S.Name = W[1]; Plan.Add(S); continue; }
		if (Plan.Num() == 0) continue;
		FPlanShot& S = Plan.Last();
		if (W[0] == TEXT("cam") && W.Num() >= 4) { S.Cam = ParseVec(W[1]); S.Look = ParseVec(W[2]); S.Fov = FCString::Atof(*W[3]); }
		else if (W[0] == TEXT("ev") && W.Num() >= 5)
		{
			FPlanEvent E; E.Prefab = W[1]; E.Time = FCString::Atof(*W[2]); E.Pos = ParseVec(W[3]); E.Scale = FCString::Atof(*W[4]);
			if (W.Num() >= 6) E.Vel = ParseVec(W[5]);
			if (W.Num() >= 7) E.MaxLife = FCString::Atof(*W[6]);
			S.Events.Add(E);
		}
		else if (W[0] == TEXT("prop") && W.Num() >= 3) { FPlanProp P; P.Pos = ParseVec(W[1]); P.Height = FCString::Atof(*W[2]); if (W.Num() >= 4) P.Radius = FCString::Atof(*W[3]); S.Props.Add(P); }
		else if (W[0] == TEXT("times") && W.Num() >= 2)
		{
			TArray<FString> P; W[1].ParseIntoArray(P, TEXT(","));
			for (const FString& X : P) S.Times.Add(FCString::Atof(*X));
			S.Times.Sort();
		}
	}
	return Plan.Num() > 0;
}

void AZoogiKFXPreview::ClearPlanShot()
{
	for (AActor* A : Spawned) if (IsValid(A)) A->Destroy();
	Spawned.Reset();
	Movers.Reset();
	// effects spawned at world locations (and their AZoogiKFXExtras) clean themselves up; kill leftovers near the stage
	TArray<UNiagaraComponent*> Kill;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		if (It->GetWorld() == GetWorld() && FVector::Dist(It->GetComponentLocation(), KFXBase) < 6000.f) Kill.Add(*It);
	for (UNiagaraComponent* C : Kill) C->DestroyComponent();
	for (TActorIterator<AZoogiAwakenFX> It(GetWorld()); It; ++It) It->Destroy();
}

void AZoogiKFXPreview::NextPlan()
{
	ClearPlanShot();
	++PlanIndex;
	if (!Plan.IsValidIndex(PlanIndex))
	{
		// pass 0 only warms the shader / PSO caches; pass 1 writes the PNGs that are compared
		if (PlanPass == 0) { PlanPass = 1; PlanIndex = 0; }
		else { bDone = true; UE_LOG(LogZoogi, Log, TEXT("KFX plan: done")); if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiKFXQuit"))) FPlatformMisc::RequestExit(false); return; }
	}
	FPlanShot& S = Plan[PlanIndex];
	for (FPlanEvent& E : S.Events) E.bFired = false;
	Shot = 0;
	T = -0.6f; // let the previous shot's leftovers vanish
	Capture->FOVAngle = S.Fov;
	Capture->SetWorldLocationAndRotation(KFXBase + S.Cam, (S.Look - S.Cam).Rotation());
	if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		for (const FPlanProp& P : S.Props)
		{
			// dark stand-in for the demo character (scale reference only)
			AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(KFXBase + P.Pos + FVector(0, 0, P.Height * 0.5f)));
			if (!A) continue;
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(Cyl);
			A->GetStaticMeshComponent()->SetWorldScale3D(FVector(P.Radius / 50.f, P.Radius / 50.f, P.Height / 100.f));
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->GetStaticMeshComponent()->bReceivesDecals = false;
			if (UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BM, A);
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.04f, 0.045f));
				A->GetStaticMeshComponent()->SetMaterial(0, MID);
			}
			Spawned.Add(A);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("KFX plan shot %s (pass %d)"), *S.Name, PlanPass);
}

void AZoogiKFXPreview::TickPlan(float Dt)
{
	if (PlanIndex < 0) { if (T >= 0.f) NextPlan(); return; }
	FPlanShot& S = Plan[PlanIndex];
	AZoogiAwakenFX::ViewOverride = Capture->GetComponentLocation();
	for (FMover& M : Movers) if (AActor* A = M.Actor.Get()) A->SetActorLocation(A->GetActorLocation() + M.Vel * Dt);
	for (FPlanEvent& E : S.Events)
	{
		if (E.bFired || T < E.Time) continue;
		E.bFired = true;
		if (E.Prefab.StartsWith(TEXT("AFX_")))
		{
			// Awaken FX (launch ring / shock chain / void portal): 'scale' is the size, the velocity field carries the parameters
			const EZoogiAwakenFX K = E.Prefab == TEXT("AFX_Ring") ? EZoogiAwakenFX::LaunchRing : E.Prefab == TEXT("AFX_Shock") ? EZoogiAwakenFX::ShockChain
				: E.Prefab == TEXT("AFX_ShockHit") ? EZoogiAwakenFX::ShockHit : EZoogiAwakenFX::VoidPortal;
			if (AZoogiAwakenFX* X = AZoogiAwakenFX::SpawnFX(GetWorld(), K, KFXBase + E.Pos, E.Scale, nullptr, E.Vel)) Spawned.Add(X);
			continue;
		}
		AActor* Attach = nullptr;
		if (!E.Vel.IsNearlyZero())
		{
			Attach = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(KFXBase + E.Pos));
			if (Attach)
			{
				USceneComponent* R = NewObject<USceneComponent>(Attach, TEXT("Root"));
				Attach->SetRootComponent(R); R->RegisterComponent(); R->SetMobility(EComponentMobility::Movable);
				Attach->SetActorLocation(KFXBase + E.Pos);
				Movers.Add({ Attach, E.Vel });
				Spawned.Add(Attach);
			}
		}
		AZoogiFX::KFX(GetWorld(), *E.Prefab, KFXBase + E.Pos, E.Scale, E.MaxLife, Attach);
	}
	if (PlanPass == 1 && S.Times.IsValidIndex(Shot) && T >= S.Times[Shot])
	{
		Capture->CaptureScene();
		const FString Name = FString::Printf(TEXT("%s_%05d.png"), *S.Name, FMath::RoundToInt(S.Times[Shot] * 1000.f));
		UKismetRenderingLibrary::ExportRenderTarget(this, RT, ShotDir, Name);
		++Shot;
	}
	const float End = (S.Times.Num() ? S.Times.Last() : 2.f) + 0.1f;
	if ((PlanPass == 1 && Shot >= S.Times.Num() && T >= End) || (PlanPass == 0 && T >= FMath::Min(End, 2.5f))) NextPlan();
}

void AZoogiKFXPreview::Next()
{
	if (Current) { Current->DestroyComponent(); Current = nullptr; }
	++Index;
	T = 0.f;
	Shot = 0;
	if (!Prefabs.IsValidIndex(Index)) { bDone = true; UE_LOG(LogZoogi, Log, TEXT("KFX preview: done")); return; }
	Current = AZoogiFX::KFX(GetWorld(), *Prefabs[Index], KFXBase + FVector(0, 0, 1.f), 1.f, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("KFX preview: %s %s"), *Prefabs[Index], Current ? TEXT("spawned") : TEXT("MISSING"));
}

void AZoogiKFXPreview::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bDone) return;
	T += Dt;
	if (Plan.Num() > 0) { TickPlan(Dt); return; }
	if (Index < 0) { if (T >= 0.f) Next(); return; }
	static const float At[] = { 0.45f, 1.3f };
	if (Shot < 2 && T >= At[Shot])
	{
		Capture->CaptureScene();
		const FString Name = FString::Printf(TEXT("kfx_%s_%c.png"), *Prefabs[Index], TCHAR('a' + Shot));
		UKismetRenderingLibrary::ExportRenderTarget(this, RT, ShotDir, Name);
		UE_LOG(LogZoogi, Log, TEXT("KFX preview shot %s"), *Name);
		++Shot;
	}
	if (T >= 3.2f) Next();
}
