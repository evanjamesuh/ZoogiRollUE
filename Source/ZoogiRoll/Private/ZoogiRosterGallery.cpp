// Test-only roster gallery. -ZoogiRosterGallery=<dir> (use with -game -ZoogiAuto -ZoogiNoHUD): ~8 s after BeginPlay
// it lines the 15 Tripo models (/Game/Characters/Tripo/<Name>/SM_<Name> + _Nubs) up high above the arena, then for each
// one points a camera at its face and saves <dir>/<Name>_<NN>_<expr>.png for a few expressions (ExprIndex on the
// M_<Name>_Eyes slot, the parameter UZoogiExpressionComponent drives), plus a yaw check of Blue and a lineup shot.
// Writes <dir>/gallery_done.txt at the end. Nothing happens without the flag.
#include "ZoogiRosterGallery.h"
#include "ZoogiTypes.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static const TCHAR* GRosterNames[] = { TEXT("Blue"), TEXT("Tavi"), TEXT("Vex"), TEXT("Moss"), TEXT("MasterOrin"), TEXT("Sera"),
	TEXT("Luma"), TEXT("DarkZoogy"), TEXT("Wolfgang"), TEXT("Hotstreak"), TEXT("Lars"), TEXT("Pinpoint"), TEXT("Bolt"),
	TEXT("Nightshade"), TEXT("Wraps") };
static const TCHAR* GExprNames[] = { TEXT("neutral"), TEXT("blink"), TEXT("focused"), TEXT("determined"), TEXT("excited"),
	TEXT("dizzy"), TEXT("worried"), TEXT("ko"), TEXT("happy"), TEXT("sad"), TEXT("guard"), TEXT("flourish") };
static const FVector GBase(0.f, 0.f, 4000.f);
static constexpr float GSpacing = 400.f;

bool UZoogiRosterGallery::ShouldCreateSubsystem(UObject* Outer) const
{
	FString D;
	return FParse::Value(FCommandLine::Get(), TEXT("ZoogiRosterGallery="), D);
}

void UZoogiRosterGallery::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld()) return;
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiRosterGallery="), Dir);
	IFileManager::Get().MakeDirectory(*Dir, true);
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UZoogiRosterGallery::Build), 8.f, false);
}

void UZoogiRosterGallery::Build()
{
	UWorld* W = GetWorld();
	for (int32 i = 0; i < UE_ARRAY_COUNT(GRosterNames); ++i)
	{
		const FString N = GRosterNames[i];
		UStaticMesh* M = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Tripo/%s/SM_%s.SM_%s"), *N, *N, *N));
		UStaticMesh* Nub = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Tripo/%s/SM_%s_Nubs.SM_%s_Nubs"), *N, *N, *N));
		FZoogiGalleryEntry E; E.Name = N;
		if (!M) { UE_LOG(LogTemp, Warning, TEXT("[RosterGallery] missing mesh %s"), *N); Entries.Add(E); continue; }
		const FVector P = GBase + FVector(0.f, i * GSpacing, 0.f);
		E.Actor = W->SpawnActor<AStaticMeshActor>(P, FRotator::ZeroRotator);
		E.Actor->SetMobility(EComponentMobility::Movable);
		E.Actor->GetStaticMeshComponent()->SetStaticMesh(M);
		if (Nub)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(E.Actor);
			C->SetStaticMesh(Nub); C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(E.Actor->GetRootComponent()); C->RegisterComponent();
		}
		UStaticMeshComponent* SM = E.Actor->GetStaticMeshComponent();
		const TArray<FName> Slots = SM->GetMaterialSlotNames();
		for (int32 s = 0; s < Slots.Num(); ++s)
			if (Slots[s].ToString().EndsWith(TEXT("_Eyes"))) E.Eyes = SM->CreateDynamicMaterialInstance(s, SM->GetMaterial(s));
		UE_LOG(LogTemp, Display, TEXT("[RosterGallery] %s tris=%d eyes=%d"), *N, M->GetNumTriangles(0), E.Eyes ? 1 : 0);
		Entries.Add(E);
	}
	Cam = W->SpawnActor<ACameraActor>(GBase, FRotator::ZeroRotator);
	Cam->GetCameraComponent()->SetFieldOfView(40.f);
	Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	for (int32 y = 0; y < 4; ++y) Plan.Add({0, -2 - y});
	static const int32 Ex[] = { 0, 1, 2, 4, 5, 7, 8, 9, 10, 11 };
	for (int32 i = 0; i < Entries.Num(); ++i) for (int32 e : Ex) Plan.Add({i, e});
	Plan.Add({0, -1});
	W->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UZoogiRosterGallery::Step), 0.35f, true);
}

void UZoogiRosterGallery::Step()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PlanIdx >= Plan.Num())
	{
		GetWorld()->GetTimerManager().ClearTimer(Timer);
		FFileHelper::SaveStringToFile(TEXT("done"), *(Dir / TEXT("gallery_done.txt")));
		return;
	}
	const TPair<int32, int32> S = Plan[PlanIdx];
	FZoogiGalleryEntry& E = Entries[S.Key];
	FString File;
	if (!bArmed)
	{
		// set up this shot; capture on the next tick (one frame for the material / camera to land)
		FVector P = GBase + FVector(0.f, S.Key * GSpacing, 0.f);
		FVector Eye; FVector Look = P + FVector(0, 0, 15.f);
		if (S.Value == -1) { Look = GBase + FVector(0.f, 7.f * GSpacing, 0.f); Eye = Look + FVector(2400.f * 3.4f, 0.f, 400.f); }
		else if (S.Value <= -2) { const float Yaw = (-2 - S.Value) * 90.f; Eye = P + FRotator(0.f, Yaw, 0.f).RotateVector(FVector(420.f, 0.f, 60.f)); }
		else { Eye = P + FVector(420.f, 0.f, 70.f); if (E.Eyes) E.Eyes->SetScalarParameterValue(TEXT("ExprIndex"), (float)S.Value); }
		Cam->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
		if (PC) PC->SetViewTarget(Cam);
		bArmed = true;
		return;
	}
	if (PC) PC->SetViewTarget(Cam);
	if (S.Value == -1) File = TEXT("lineup.png");
	else if (S.Value <= -2) File = FString::Printf(TEXT("yawcheck_Blue_cam%03d.png"), (-2 - S.Value) * 90);
	else File = FString::Printf(TEXT("%s_%02d_%s.png"), *E.Name, S.Value, GExprNames[S.Value]);
	if (E.Actor || S.Value == -1) FScreenshotRequest::RequestScreenshot(Dir / File, false, false);
	bArmed = false;
	++PlanIdx;
}
