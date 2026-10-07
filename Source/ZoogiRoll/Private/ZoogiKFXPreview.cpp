#include "ZoogiKFXPreview.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

static const FVector KFXBase(30000.f, 0.f, 0.f);

AZoogiKFXPreview::AZoogiKFXPreview()
{
	PrimaryActorTick.bCanEverTick = true;
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
	SetActorLocation(KFXBase);
	if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		Floor->SetStaticMesh(Plane);
		Floor->SetWorldScale3D(FVector(40.f, 40.f, 1.f));
	}
	RT = NewObject<UTextureRenderTarget2D>(this);
	RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
	RT->InitAutoFormat(1280, 720);
	RT->UpdateResourceImmediate(true);
	Capture->TextureTarget = RT;
	Capture->SetUsingAbsoluteScale(true);
	Capture->SetWorldLocationAndRotation(KFXBase + FVector(-720.f, -260.f, 380.f), (FVector(0.f, 0.f, 110.f) - FVector(-720.f, -260.f, 380.f)).Rotation());
	Prefabs = { TEXT("Effect4"), TEXT("Effect4_Hand"), TEXT("Effect5") }; // Oct 7 scope: only Effect4/Effect5 (+ Effect25 from kfx-vfx-ap) stay in the game
	{
		// two passes: the first warms up shaders compiled on demand (editor -game), the second pass's shots overwrite it
		const TArray<FString> Once = Prefabs;
		Prefabs.Append(Once);
	}
	UE_LOG(LogZoogi, Log, TEXT("KFX preview: %d systems -> %s"), Prefabs.Num(), *ShotDir);
	T = -2.f; // let the world settle
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
