#include "ZoogiFabFX.h"
#include "ZoogiSkills.h"
#include "ZoogiSkillLab.h"
#include "ZoogiRoll.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

AZoogiFabFX::AZoogiFabFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	NC = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FX"));
	NC->SetupAttachment(RootComponent);
	NC->bAutoActivate = false;
	NC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// b71 (Evan: "how can I control how this effect looks?"): Content/Data/FxTune.csv - one row per Niagara system (file name, e.g.
// NS_Water_hit): Size, Height (extra vertical stretch), Duration multipliers and an optional tint (R,G,B; 0 0 0 = none). The file is
// re-read when it changes (editor / desktop), so a tweak shows on the next cast. -ZoogiNoFxTune ignores it.
namespace
{
	struct FFxTuneRow { float Size = 1.f, Height = 1.f, Life = 1.f; FLinearColor Tint = FLinearColor::Black; };
	const FFxTuneRow* FxTuneFor(const FString& System)
	{
		static TMap<FString, FFxTuneRow> Rows; static FDateTime Stamp; static double NextCheck = 0.0;
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFxTune"));
		if (bOff) return nullptr;
		const double Now = FPlatformTime::Seconds();
		if (Now >= NextCheck)
		{
			NextCheck = Now + 1.0;
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/FxTune.csv"));
			const FDateTime T = IFileManager::Get().GetTimeStamp(*Path);
			if (T != Stamp)
			{
				Stamp = T; Rows.Reset();
				TArray<FString> Lines;
				if (FFileHelper::LoadFileToStringArray(Lines, *Path))
					for (const FString& L0 : Lines)
					{
						const FString L = L0.TrimStartAndEnd();
						if (L.IsEmpty() || L.StartsWith(TEXT("#")) || L.StartsWith(TEXT("System"))) continue;
						TArray<FString> C; L.ParseIntoArray(C, TEXT(","), false);
						if (C.Num() < 2) continue;
						FFxTuneRow R;
						auto F = [&](int32 i, float D) { return C.IsValidIndex(i) && !C[i].TrimStartAndEnd().IsEmpty() ? FCString::Atof(*C[i]) : D; };
						R.Size = FMath::Clamp(F(1, 1.f), 0.05f, 10.f); R.Height = FMath::Clamp(F(2, 1.f), 0.05f, 10.f); R.Life = FMath::Clamp(F(3, 1.f), 0.1f, 10.f);
						R.Tint = FLinearColor(F(4, 0.f), F(5, 0.f), F(6, 0.f), 1.f);
						Rows.Add(C[0].TrimStartAndEnd(), R);
					}
				UE_LOG(LogZoogi, Log, TEXT("FABFX tune: %d rows from Data/FxTune.csv"), Rows.Num());
			}
		}
		return Rows.Find(System);
	}
}

AZoogiFabFX* AZoogiFabFX::Play(UWorld* W, const TCHAR* Path, const FVector& Loc, float Scale, float RealLife, AActor* InFollow, const FVector& InOffset, const FRotator& Rot)
{
	if (!W) return nullptr;
	const FFxTuneRow* Tune = FxTuneFor(FPaths::GetBaseFilename(FString(Path)));
	float HeightMul = 1.f;
	if (Tune) { Scale *= Tune->Size; HeightMul = Tune->Height; RealLife *= Tune->Life; }
	const bool bLab = ZoogiSkills::EffectBusy(); // b107 SKILL LAB: the playing skill's FX size / colour
	if (bLab) Scale *= ZoogiSkillLab::FxSize();
	UNiagaraSystem* S = LoadObject<UNiagaraSystem>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!S) { UE_LOG(LogZoogi, Warning, TEXT("FABFX missing %s"), Path); return nullptr; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiFabFX* A = W->SpawnActor<AZoogiFabFX>(AZoogiFabFX::StaticClass(), FTransform(Rot, Loc, FVector(FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale * HeightMul))), P);
	if (!A) return nullptr;
	A->Follow = InFollow; A->Offset = InOffset;
	A->BornAt = FPlatformTime::Seconds(); A->DieAt = A->BornAt + FMath::Max(0.2f, RealLife);
	A->Tag = FPaths::GetBaseFilename(FString(Path));
	A->NC->SetAsset(S);
	A->NC->Activate(true);
	FLinearColor LabTint; if (bLab && ZoogiSkillLab::Tint(LabTint)) { A->NC->SetVariableLinearColor(FName(TEXT("User.Color")), LabTint); A->NC->SetVariableLinearColor(FName(TEXT("Color")), LabTint); A->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), LabTint); }
	else if (Tune && (Tune->Tint.R + Tune->Tint.G + Tune->Tint.B) > 0.f) { A->NC->SetVariableLinearColor(FName(TEXT("User.Color")), Tune->Tint); A->NC->SetVariableLinearColor(FName(TEXT("Color")), Tune->Tint); A->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), Tune->Tint); }
	const float Gd = W->GetWorldSettings() ? FMath::Max(0.001f, W->GetWorldSettings()->GetEffectiveTimeDilation()) : 1.f;
	A->CustomTimeDilation = 1.f / Gd;
	UE_LOG(LogZoogi, Log, TEXT("FABFX %s at (%.0f, %.0f, %.0f) scale %.2f life %.1f s%s"), *A->Tag, Loc.X, Loc.Y, Loc.Z, Scale, RealLife, InFollow ? TEXT(" (follows)") : TEXT(""));
	return A;
}

void AZoogiFabFX::Sound(UWorld* W, const TCHAR* Path, const FVector& Loc, float Volume)
{
	if (USoundBase* S = W ? LoadObject<USoundBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr)
		UGameplayStatics::PlaySoundAtLocation(W, S, Loc, Volume);
}

void AZoogiFabFX::Release(float After)
{
	FreezeAt = 0.f;
	if (bFrozen && NC) NC->SetPaused(false);
	bFrozen = false;
	DieAt = FPlatformTime::Seconds() + FMath::Max(0.2f, After);
}

void AZoogiFabFX::Tick(float Dt)
{
	Super::Tick(Dt);
	UWorld* W = GetWorld();
	const float Gd = W && W->GetWorldSettings() ? FMath::Max(0.001f, W->GetWorldSettings()->GetEffectiveTimeDilation()) : 1.f;
	CustomTimeDilation = 1.f / Gd; // real speed in slow-mo
	if (Follow.IsValid()) SetActorLocation(Follow->GetActorLocation() + Offset);
	if (NC) MaxR = FMath::Max(MaxR, FVector2D(NC->Bounds.BoxExtent.X, NC->Bounds.BoxExtent.Y).Size() * 0.7071f);
	if (StopAfter > 0.f && !bStopped && NC && FPlatformTime::Seconds() >= BornAt + StopAfter) { bStopped = true; NC->Deactivate(); }
	if (FreezeAt > 0.f && !bFrozen && NC && FPlatformTime::Seconds() >= BornAt + FreezeAt) { bFrozen = true; NC->SetPaused(true); }
	if (bFrozen) return; // b180: held until Release()
	if (FPlatformTime::Seconds() >= DieAt)
	{
		UE_LOG(LogZoogi, Log, TEXT("FABFX %s done: measured footprint radius %.0f cm at scale %.2f (%.0f cm at scale 1)"), *Tag, MaxR, GetActorScale3D().X, MaxR / FMath::Max(0.01f, GetActorScale3D().X));
		Destroy();
	}
}
