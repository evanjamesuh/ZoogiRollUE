// Reusable stage-forming intro (Frozen Ring, Oct 6). See ZoogiStageForming.h.
#include "ZoogiStageForming.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiRoll.h"
#include "Materials/MaterialInstanceDynamic.h"

UZoogiStageForming::UZoogiStageForming()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UZoogiStageForming::Clear()
{
	if (bPlaying) Skip();
	Entries.Reset();
	Ramps.Reset();
	Time = 0.f;
}

void UZoogiStageForming::AddActor(AActor* A, EZoogiFormStyle Style, const FVector& Offset, float Delay, float Duration, float Overshoot)
{
	if (!A || !A->GetRootComponent()) return;
	FEntry E;
	E.Actor = A;
	E.Style = Style;
	E.Offset = Offset;
	E.Delay = Delay;
	E.Duration = FMath::Max(0.05f, Duration);
	E.Overshoot = Overshoot;
	Entries.Add(E);
}

void UZoogiStageForming::AddComponent(USceneComponent* C, EZoogiFormStyle Style, const FVector& Offset, float Delay, float Duration, float Overshoot)
{
	if (!C) return;
	FEntry E;
	E.Comp = C;
	E.Style = Style;
	E.Offset = Offset;
	E.Delay = Delay;
	E.Duration = FMath::Max(0.05f, Duration);
	E.Overshoot = Overshoot;
	Entries.Add(E);
}

void UZoogiStageForming::AddParamRamp(UMaterialInstanceDynamic* M, FName Param, float From, float To, float Delay, float Duration)
{
	if (!M) return;
	FRamp R;
	R.M = M;
	R.Param = Param;
	R.From = From;
	R.To = To;
	R.Delay = Delay;
	R.Duration = FMath::Max(0.05f, Duration);
	Ramps.Add(R);
}

float UZoogiStageForming::GetLength() const
{
	float L = 0.f;
	for (const FEntry& E : Entries) L = FMath::Max(L, E.Delay + E.Duration);
	for (const FRamp& R : Ramps) L = FMath::Max(L, R.Delay + R.Duration);
	return L;
}

USceneComponent* UZoogiStageForming::Target(const FEntry& E) const
{
	if (E.Actor.IsValid()) return E.Actor->GetRootComponent();
	return E.Comp.Get();
}

void UZoogiStageForming::Begin(float TimeScale)
{
	if (bPlaying) Skip();
	Scale = FMath::Max(1.f, TimeScale);
	Time = 0.f;
	Entries.RemoveAll([this](const FEntry& E) { return Target(E) == nullptr; }); // pieces of a destroyed match (combo props)
	Ramps.RemoveAll([](const FRamp& R) { return !R.M.IsValid(); });
	for (FEntry& E : Entries)
	{
		USceneComponent* C = Target(E);
		if (!C) continue;
		E.Final = E.Actor.IsValid() ? E.Actor->GetActorTransform() : C->GetRelativeTransform();
		E.bSettled = false;
		if (AZoogiArenaPiece* P = Cast<AZoogiArenaPiece>(E.Actor.Get())) P->SetCollisionLive(false);
		else if (E.Actor.IsValid()) E.Actor->SetActorEnableCollision(false);
		Pose(E, 0.f);
	}
	for (FRamp& R : Ramps) if (R.M.IsValid()) R.M->SetScalarParameterValue(R.Param, R.From);
	bPlaying = Entries.Num() > 0 || Ramps.Num() > 0;
	UE_LOG(LogZoogi, Log, TEXT("Stage forming: %d pieces, %d ramps, %.2f s%s"), Entries.Num(), Ramps.Num(), GetLength() * Scale, Scale > 1.f ? TEXT(" (slowed for shots)") : TEXT(""));
	if (!bPlaying) return;
}

void UZoogiStageForming::Pose(FEntry& E, float T)
{
	USceneComponent* C = Target(E);
	if (!C) return;
	const float U = FMath::Clamp((T - E.Delay) / E.Duration, 0.f, 1.f);
	FTransform X = E.Final;
	switch (E.Style)
	{
	case EZoogiFormStyle::Rise:
	case EZoogiFormStyle::Slide:
	{
		// Ease-out cubic; with Overshoot the piece passes its final spot by Overshoot (cm along -Offset) then settles back.
		float K = 1.f - FMath::Pow(1.f - U, 3.f);
		FVector P = X.GetLocation() + E.Offset * (1.f - K);
		if (E.Overshoot > 0.f && U > 0.55f && U < 1.f)
		{
			const float B = FMath::Sin((U - 0.55f) / 0.45f * PI);
			P -= E.Offset.GetSafeNormal() * E.Overshoot * B;
		}
		X.SetLocation(P);
		break;
	}
	case EZoogiFormStyle::Drop:
	{
		// Accelerating fall, then a small bounce that ends exactly on the final pose.
		float Zf;
		if (U < 0.75f) { const float A = U / 0.75f; Zf = 1.f - A * A; }
		else { const float A = (U - 0.75f) / 0.25f; Zf = 0.06f * FMath::Sin(A * PI); }
		X.SetLocation(X.GetLocation() + E.Offset * Zf);
		break;
	}
	case EZoogiFormStyle::Grow:
	{
		const float K = 1.f - FMath::Pow(1.f - U, 2.5f);
		FVector S = X.GetScale3D();
		X.SetScale3D(FVector(S.X, S.Y, S.Z * FMath::Max(0.02f, K))); // grows up from its base (Z only, so arc pieces stay on their radius)
		X.SetLocation(X.GetLocation() + E.Offset * (1.f - K));
		break;
	}
	}
	const bool bVisible = U > 0.f || E.Style == EZoogiFormStyle::Rise; // risers start hidden under the floor
	if (E.Actor.IsValid())
	{
		E.Actor->SetActorTransform(X, false, nullptr, ETeleportType::TeleportPhysics);
		E.Actor->SetActorHiddenInGame(!bVisible);
	}
	else
	{
		C->SetRelativeTransform(X, false, nullptr, ETeleportType::TeleportPhysics);
		C->SetVisibility(bVisible, true);
	}
}

void UZoogiStageForming::Settle(FEntry& E)
{
	USceneComponent* C = Target(E);
	E.bSettled = true;
	if (!C) return;
	if (E.Actor.IsValid())
	{
		E.Actor->SetActorTransform(E.Final, false, nullptr, ETeleportType::TeleportPhysics);
		E.Actor->SetActorHiddenInGame(false);
		if (AZoogiArenaPiece* P = Cast<AZoogiArenaPiece>(E.Actor.Get())) P->SetCollisionLive(true);
		else E.Actor->SetActorEnableCollision(true);
	}
	else
	{
		C->SetRelativeTransform(E.Final, false, nullptr, ETeleportType::TeleportPhysics);
		C->SetVisibility(true, true);
	}
}

void UZoogiStageForming::Skip()
{
	if (!bPlaying) return;
	for (FEntry& E : Entries) if (!E.bSettled) Settle(E);
	for (FRamp& R : Ramps) if (R.M.IsValid()) R.M->SetScalarParameterValue(R.Param, R.To);
	bPlaying = false;
	UE_LOG(LogZoogi, Log, TEXT("Stage forming: skipped / snapped at %.2f s (all pieces final, colliders on)"), Time * Scale);
}

void UZoogiStageForming::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bPlaying) return;
	Time += FMath::Min(DeltaTime, 0.05f) / Scale;
	bool bAll = true;
	for (FEntry& E : Entries)
	{
		if (E.bSettled) continue;
		if (Time >= E.Delay + E.Duration) Settle(E);
		else { Pose(E, Time); bAll = false; }
	}
	for (FRamp& R : Ramps)
	{
		if (!R.M.IsValid()) continue;
		const float U = FMath::Clamp((Time - R.Delay) / R.Duration, 0.f, 1.f);
		R.M->SetScalarParameterValue(R.Param, FMath::Lerp(R.From, R.To, U * U * (3.f - 2.f * U)));
		if (Time < R.Delay + R.Duration) bAll = false;
	}
	if (bAll)
	{
		bPlaying = false;
		UE_LOG(LogZoogi, Log, TEXT("Stage forming: complete at %.2f s (all pieces settled, colliders on)"), Time * Scale);
	}
}
