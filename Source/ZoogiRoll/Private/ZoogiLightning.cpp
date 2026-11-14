#include "ZoogiLightning.h"
#include "ZoogiTypes.h"
#include "ZoogiAnimeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/App.h"

AZoogiLightning::AZoogiLightning()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AZoogiLightning* AZoogiLightning::Burst(UWorld* World, AActor* InFrom, const FLinearColor& InColor, int32 InBolts, float InLen, float InLife)
{
	if (!World || !InFrom || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiLightning* L = World->SpawnActor<AZoogiLightning>(AZoogiLightning::StaticClass(), FTransform(InFrom->GetActorLocation()), SP);
	if (!L) return nullptr;
	L->From = InFrom; L->Origin = InFrom->GetActorLocation(); L->Color = InColor; L->Bolts = FMath::Clamp(InBolts, 1, 12);
	L->Len = InLen; L->Life = InLife; L->R.Initialize(FMath::Rand());
	L->MID = ZoogiUtil::GlowMID(L, InColor, 30.f);
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	for (int32 b = 0; b < L->Bolts; ++b)
	{
		const float A = 2.f * PI * (b + L->R.FRandRange(-0.3f, 0.3f)) / L->Bolts;
		L->Dirs.Add(FVector(FMath::Cos(A), FMath::Sin(A), L->R.FRandRange(0.15f, 0.85f)).GetSafeNormal());
		for (int32 s = 0; s < SegsPerBolt + 2; ++s) // + a 2-segment fork
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(L);
			C->SetStaticMesh(Cube); C->SetupAttachment(L->RootComponent);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetMaterial(0, L->MID); C->SetVisibility(false);
			C->RegisterComponent();
			L->Segs.Add(C);
		}
	}
	return L;
}

void AZoogiLightning::Rebuild(float K)
{
	if (AActor* F = From.Get()) Origin = F->GetActorLocation();
	const float W = FMath::Lerp(7.f, 2.f, K);   // bolt thickness (cm)
	int32 i = 0;
	auto Seg = [&](const FVector& A, const FVector& B, float Wd)
	{
		if (!Segs.IsValidIndex(i)) return;
		UStaticMeshComponent* C = Segs[i++];
		const FVector D = B - A; const float L = D.Size();
		C->SetWorldLocation((A + B) * 0.5f);
		C->SetWorldRotation(D.Rotation());
		C->SetWorldScale3D(FVector(L / 100.f, Wd / 100.f, Wd / 100.f));
		C->SetVisibility(L > 1.f);
	};
	for (int32 b = 0; b < Bolts; ++b)
	{
		const FVector Dir = Dirs[b];
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Side, Dir);
		const float BoltLen = Len * R.FRandRange(0.7f, 1.f) * FMath::Lerp(1.f, 0.6f, K);
		FVector P = Origin + Dir * 40.f;
		FVector Fork = P;
		for (int32 s = 0; s < SegsPerBolt; ++s)
		{
			const float J = BoltLen * 0.12f;
			const FVector Q = P + Dir * (BoltLen / SegsPerBolt) + Side * R.FRandRange(-J, J) + Up * R.FRandRange(-J, J) * 0.6f;
			Seg(P, Q, W * (1.f - 0.1f * s));
			if (s == 2) Fork = Q;
			P = Q;
		}
		// fork off the 3rd joint
		const FVector FD = (Dir + Side * R.FRandRange(-0.9f, 0.9f)).GetSafeNormal();
		const FVector F1 = Fork + FD * BoltLen * 0.22f + Up * R.FRandRange(-20.f, 20.f);
		Seg(Fork, F1, W * 0.6f);
		Seg(F1, F1 + FD * BoltLen * 0.16f + Side * R.FRandRange(-25.f, 25.f), W * 0.45f);
	}
	if (MID) MID->SetScalarParameterValue(TEXT("Strength"), FMath::Lerp(34.f, 6.f, K) * (0.75f + 0.25f * R.FRand()));
}

void AZoogiLightning::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min((float)FApp::GetDeltaTime(), 0.05f);
	// wait while an anime cut-in covers the screen (up to 4 s), so the lightning plays right after it
	if (UZoogiAnimeSubsystem* An = GetWorld() ? GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr)
		if (An->IsBusy() && Wait < 4.f) { Wait += Dt; for (UStaticMeshComponent* C : Segs) if (C) C->SetVisibility(false); return; }
	Age += Dt;
	if (Age > Life) { Destroy(); return; }
	NextJag -= Dt;
	if (NextJag <= 0.f) { NextJag = 0.045f; Rebuild(Age / Life); }
}
