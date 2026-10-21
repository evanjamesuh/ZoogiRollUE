// Void-active look (Oct 14, Evan 9:34 PM). See ZoogiVoidLook.h.
#include "ZoogiVoidLook.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void FZoogiVoidLookTune::Load()
{
	const TCHAR* S = TEXT("ZoogiRoll.VoidLook");
	if (GConfig)
	{
		GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
		FString C;
		if (GConfig->GetString(S, TEXT("RingColor"), C, GGameIni) && !C.IsEmpty()) RingColor.InitFromString(C);
		if (GConfig->GetString(S, TEXT("CoreColor"), C, GGameIni) && !C.IsEmpty()) CoreColor.InitFromString(C);
		if (GConfig->GetString(S, TEXT("RimColor"), C, GGameIni) && !C.IsEmpty()) RimColor.InitFromString(C);
		GConfig->GetFloat(S, TEXT("RingRadius"), RingRadius, GGameIni);
		GConfig->GetInt(S, TEXT("FlameCount"), FlameCount, GGameIni);
		GConfig->GetFloat(S, TEXT("FlameHeight"), FlameHeight, GGameIni);
		GConfig->GetFloat(S, TEXT("RingIntensity"), RingIntensity, GGameIni);
		GConfig->GetFloat(S, TEXT("OverlayIntensity"), OverlayIntensity, GGameIni);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVoidLook"))) bEnabled = false;
	FlameCount = FMath::Clamp(FlameCount, 4, 48);
	RingRadius = FMath::Clamp(RingRadius, 40.f, 400.f);
	FlameHeight = FMath::Clamp(FlameHeight, 10.f, 300.f);
}

FString UZoogiVoidLook::VoidReason(const AZoogiGameMode* G, const AZoogiBall* B)
{
	if (!G || !B || B->bOut || B->bKnockedOut) return FString();
	if (B->Artifact && B->Artifact->Infused == EZoogiArtifact::Void) return TEXT("Void infusion (gravity pull armed)");
	if (G->CardsOn())
		if (const UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(G))
			if (const FZoogiCardPlayerState* S = Cards->Rules().Player(B->PlayerIndex))
				if (S->Turn == G->TurnCounter)
					for (const FZoogiCardCommitted& K : S->Committed)
						if (const FZoogiCardDef* D = Cards->Def(K.Card.CardId))
							if (D->Element == EZoogiCardElement::Void) return FString::Printf(TEXT("Void card %s"), *D->Name);
	return FString();
}

bool UZoogiVoidLook::IsApplied(int32 Player) const
{
	for (const FLook& L : Looks) if (L.bOn && L.Ball.IsValid() && L.Ball->PlayerIndex == Player) return true;
	return false;
}

void UZoogiVoidLook::Tick(float Dt)
{
	if (!bLoaded) { bLoaded = true; Tune.Load(); }
	Time += Dt;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const bool bLive = Tune.bEnabled && G && G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::Forming && G->Phase != EZoogiPhase::GameOver;
	if (G) for (AZoogiBall* B : G->Balls)
	{
		if (!B) continue;
		FLook* L = Looks.FindByPredicate([B](const FLook& X) { return X.Ball.Get() == B; });
		const FString Why = (bLive && B->PlayerIndex == G->ActivePlayer) ? VoidReason(G, B) : FString();
		if (!Why.IsEmpty())
		{
			if (!L) { FLook N; N.Ball = B; N.First = INDEX_NONE; L = &Looks.Add_GetRef(N); }
			if (!L->bOn) Apply(G, *L, Why);
			else if (L->Turn != G->TurnCounter) { Revert(*L, TEXT("turn over")); Apply(G, *L, Why); }
		}
		else if (L && L->bOn) Revert(*L, bLive ? TEXT("turn over") : TEXT("match not live"));
	}
	for (FLook& L : Looks)
	{
		if (!L.bOn) continue;
		if (!L.Ball.IsValid()) { Revert(L, TEXT("Zoogi gone")); continue; }
		UpdateRing(L, Dt);
	}
}

void UZoogiVoidLook::Apply(AZoogiGameMode* G, FLook& L, const FString& Why)
{
	AZoogiBall* B = L.Ball.Get();
	if (!B) return;
	if (MIDs.Num() == 0)
	{
		// overlay: the authored Void artifact instance (dark swirl), else the artifact base material, else the FX glow
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/Artifacts/MI_Artifact_Void.MI_Artifact_Void"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ZoogiArtifact.M_ZoogiArtifact"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* Ov = Base ? UMaterialInstanceDynamic::Create(Base, this) : ZoogiUtil::FxMID(this, Tune.RimColor, Tune.OverlayIntensity);
		Ov->SetVectorParameterValue(TEXT("Color"), Tune.RimColor);
		Ov->SetVectorParameterValue(TEXT("CoreColor"), Tune.CoreColor);
		Ov->SetScalarParameterValue(TEXT("Strength"), Tune.OverlayIntensity);
		Ov->SetScalarParameterValue(TEXT("Pattern"), (float)(int32)EZoogiArtifact::Void);
		MIDs.Add(Ov);
		MIDs.Add(ZoogiUtil::FxMID(this, Tune.RingColor, Tune.RingIntensity));
		UMaterialInstanceDynamic* Ring = ZoogiUtil::FxMID(this, Tune.RingColor, Tune.RingIntensity * 0.8f);
		Ring->SetScalarParameterValue(TEXT("Ring"), 1.f);
		MIDs.Add(Ring);
	}
	if (L.First == INDEX_NONE)
	{
		// pooled: one flame ring per Zoogi, built once, hidden between uses
		L.First = Comps.Num();
		for (int32 i = 0; i <= Tune.FlameCount; ++i)
		{
			const bool bFloor = i == Tune.FlameCount;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(B);
			C->SetStaticMesh(ZoogiUtil::BasicShape(bFloor ? TEXT("Cylinder") : TEXT("Cone")));
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->SetMaterial(0, MIDs[bFloor ? 2 : 1]);
			C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			C->RegisterComponent();
			Comps.Add(C);
		}
	}
	for (int32 i = 0; i <= Tune.FlameCount; ++i) if (UStaticMeshComponent* C = Comps.IsValidIndex(L.First + i) ? Comps[L.First + i].Get() : nullptr) C->SetVisibility(true);
	if (B->Visual)
	{
		L.Saved = B->Visual->GetOverlayMaterial();
		B->Visual->SetOverlayMaterial(MIDs[0]);
	}
	L.bOn = true;
	L.Turn = G ? G->TurnCounter : -1;
	L.Age = 0.f;
	UpdateRing(L, 0.f);
	ZoogiUtil::PlaySfx(B, TEXT("StingVoid"), 0.45f, 1.1f);
	UE_LOG(LogZoogi, Log, TEXT("VOIDLOOK on P%d %s: %s (purple ring of fire + void overlay)"), B->PlayerIndex + 1, *B->Def.Name, *Why);
	if (G) G->TakeShot(TEXT("voidlook_on"));
}

void UZoogiVoidLook::Revert(FLook& L, const TCHAR* Why)
{
	if (!L.bOn) return;
	L.bOn = false;
	if (L.First != INDEX_NONE) for (int32 i = 0; i <= Tune.FlameCount; ++i) if (UStaticMeshComponent* C = Comps.IsValidIndex(L.First + i) ? Comps[L.First + i].Get() : nullptr) C->SetVisibility(false);
	if (AZoogiBall* B = L.Ball.Get())
	{
		// restore only if nobody replaced our overlay in the meantime (the artifact look owns its own overlay)
		if (B->Visual && MIDs.Num() && B->Visual->GetOverlayMaterial() == MIDs[0]) B->Visual->SetOverlayMaterial(L.Saved.Get());
		UE_LOG(LogZoogi, Log, TEXT("VOIDLOOK off P%d (%s): overlay + ring reverted"), B->PlayerIndex + 1, Why);
	}
	L.Saved.Reset();
}

void UZoogiVoidLook::UpdateRing(FLook& L, float Dt)
{
	AZoogiBall* B = L.Ball.Get();
	if (!B || L.First == INDEX_NONE) return;
	L.Age += Dt;
	// keep our overlay on top if the artifact look re-applied its own (remember it for the revert)
	if (B->Visual && MIDs.Num() && B->Visual->GetOverlayMaterial() != MIDs[0]) { L.Saved = B->Visual->GetOverlayMaterial(); B->Visual->SetOverlayMaterial(MIDs[0]); }
	const float Grow = FMath::Clamp(L.Age / 0.3f, 0.f, 1.f);
	const float E = 1.f - FMath::Square(1.f - Grow);
	const FVector P = B->BallLocation();
	const float FloorZ = P.Z - ZoogiConst::ZoogiRadius + 1.f;
	const float R = Tune.RingRadius * E;
	const float Spin = Time * 1.3f;
	const int32 N = Tune.FlameCount;
	for (int32 i = 0; i < N; ++i)
	{
		UStaticMeshComponent* C = Comps.IsValidIndex(L.First + i) ? Comps[L.First + i].Get() : nullptr;
		if (!C) continue;
		const float A = Spin + 2.f * PI * i / N;
		const float Fl = 0.65f + 0.25f * FMath::Sin(Time * 11.f + i * 1.7f) + 0.1f * FMath::Sin(Time * 23.f + i * 0.9f);
		const float H = Tune.FlameHeight * Fl * E;
		const float Wd = 2.f * PI * Tune.RingRadius / N * 0.9f;
		// engine cone: 100 cm tall, pivot at its centre, tip up
		C->SetWorldLocationAndRotation(FVector(P.X + FMath::Cos(A) * R, P.Y + FMath::Sin(A) * R, FloorZ + H * 0.5f), FRotator(0.f, FMath::RadiansToDegrees(A), FMath::Sin(Time * 7.f + i) * 8.f));
		C->SetWorldScale3D(FVector(Wd / 100.f, Wd * 0.6f / 100.f, FMath::Max(0.01f, H / 100.f)));
	}
	if (UStaticMeshComponent* Floor = Comps.IsValidIndex(L.First + N) ? Comps[L.First + N].Get() : nullptr)
	{
		const float D = (Tune.RingRadius * 2.f + 30.f) * E;
		Floor->SetWorldLocationAndRotation(FVector(P.X, P.Y, FloorZ), FRotator::ZeroRotator);
		Floor->SetWorldScale3D(FVector(D / 100.f, D / 100.f, 0.01f));
	}
	if (MIDs.Num() > 1) MIDs[1]->SetScalarParameterValue(TEXT("Strength"), Tune.RingIntensity * (0.85f + 0.15f * FMath::Sin(Time * 13.f)));
	if (MIDs.Num() > 0) MIDs[0]->SetScalarParameterValue(TEXT("Strength"), Tune.OverlayIntensity * (0.85f + 0.15f * FMath::Sin(Time * 3.f)));
}

void UZoogiVoidLook::Deinitialize()
{
	for (FLook& L : Looks) Revert(L, TEXT("world end"));
	Looks.Reset();
	Super::Deinitialize();
}
