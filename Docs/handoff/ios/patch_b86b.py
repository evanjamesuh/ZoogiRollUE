# b86 (Evan 13:02: "use the spire as a visual to generate new orbs at the start of every round"): from round 2 on, the spire
# lights up and fires the missing orbs (up to 6) out of its top in glowing arcs onto free spots. -ZoogiNoSpireOrbs = off
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiHoloSpire.h', [
 ('	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;\n	virtual void Tick(float DeltaTime) override;',
  '	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;\n	virtual void Tick(float DeltaTime) override;\n	void TickSpireOrbs(float Dt); // b86'),
], inc='''class AZoogiOrb;
/** b86 (Evan): the spire emits the new orbs at the start of every round (glowing arcs from its top to each orb's spot). */
namespace ZoogiSpireOrbs
{
	ZOOGIROLL_API void Emit(AZoogiOrb* O, float Delay);
	ZOOGIROLL_API void Pulse();
	ZOOGIROLL_API bool Emitting();
}''', after='#pragma once')
ed(P + 'ZoogiHoloSpire.cpp', [
 ('void UZoogiHoloSpireSys::Tick(float DeltaTime)\n{',
  '''namespace ZoogiSpireOrbs
{
	struct FEm { TWeakObjectPtr<AZoogiOrb> O; FVector To = FVector::ZeroVector; float T = 0.f, Spark = 0.f; TWeakObjectPtr<AActor> Vis; };
	static TArray<FEm> GEms;
	static double GEmitUntil = 0.0;
	void Emit(AZoogiOrb* O, float Delay)
	{
		if (!O) return;
		O->SetActorHiddenInGame(true);
		FEm E; E.O = O; E.To = O->GetActorLocation(); E.T = -Delay;
		GEms.Add(E);
		GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + Delay + 1.8);
	}
	void Pulse() { GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + 1.6); }
	bool Emitting() { return FPlatformTime::Seconds() < GEmitUntil || GEms.Num() > 0; }
}

void UZoogiHoloSpireSys::TickSpireOrbs(float Dt)
{
	using namespace ZoogiSpireOrbs;
	if (GEms.Num() == 0) return;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	FVector Src = (G && G->Arena) ? G->Arena->GetActorLocation() + FVector(0.f, 0.f, 800.f) : FVector(0.f, 0.f, 800.f);
	if (AZoogiHoloSpire* Sp = Spire.Get()) Src = Sp->GetActorTransform().TransformPosition(FVector(0.f, 0.f, 850.f));
	const FLinearColor Cy(0.45f, 0.9f, 1.f), Gd(1.f, 0.82f, 0.3f);
	for (int32 i = GEms.Num() - 1; i >= 0; --i)
	{
		FEm& E = GEms[i];
		AZoogiOrb* O = E.O.Get();
		if (!O || !W) { if (AActor* V = E.Vis.Get()) V->Destroy(); GEms.RemoveAt(i); continue; }
		E.T += Dt;
		if (E.T < 0.f) continue;
		const FLinearColor Col = O->bStar ? Gd : Cy;
		if (!E.Vis.IsValid())
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Src), SP))
			{
				A->SetMobility(EComponentMobility::Movable);
				A->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				A->GetStaticMeshComponent()->SetCastShadow(false);
				A->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(A, Col, 10.f));
				A->SetActorScale3D(FVector(0.45f));
				E.Vis = A;
			}
			AZoogiFX::Spawn(W, Src, Col, 110.f, 0.3f, false, 10.f); // the spire top flashes as it throws one
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.5f, 1.4f);
		}
		const float K = FMath::Clamp(E.T / 0.85f, 0.f, 1.f), Ke = 1.f - FMath::Square(1.f - K);
		const FVector Mid = (Src + E.To) * 0.5f + FVector(0.f, 0.f, 380.f);
		const FVector P = FMath::Lerp(FMath::Lerp(Src, Mid, Ke), FMath::Lerp(Mid, E.To, Ke), Ke);
		if (AActor* V = E.Vis.Get()) V->SetActorLocation(P);
		E.Spark += Dt;
		if (E.Spark > 0.05f) { E.Spark = 0.f; AZoogiFX::Spawn(W, P, Col, 45.f, 0.25f, false, 8.f); }
		if (K >= 1.f)
		{
			if (AActor* V = E.Vis.Get()) V->Destroy();
			O->SetActorHiddenInGame(false);
			AZoogiFX::Spawn(W, E.To, Col, 140.f, 0.4f, true, 8.f);
			AZoogiFX::Niagara(W, O->bStar ? TEXT("NS_Burst_Yellow") : TEXT("NS_Burst_White"), E.To, FRotator::ZeroRotator, 0.5f);
			GEms.RemoveAt(i);
		}
	}
}

void UZoogiHoloSpireSys::Tick(float DeltaTime)
{
	TickSpireOrbs(FMath::Min(DeltaTime, 0.05f));'''),
 ('		const bool bShow = bAlways || (bShowRound && !bLaunched);',
  '		const bool bShow = bAlways || (bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting(); // b86: up while it throws the new orbs'),
], inc='#include "ZoogiFX.h"\n#include "Engine/StaticMeshActor.h"\n#include "HAL/PlatformTime.h"', after='#include "ZoogiHoloSpire.h"')
ed(H + 'ZoogiGameMode.h', [
 ('	void FireWolfSkill(AZoogiBall* Ball);\n',
  '	void FireWolfSkill(AZoogiBall* Ball);\n	/** b86: round start - the spire throws in fresh orbs (the missing ones, up to 6) */\n	void SpireRefillOrbs();\n	int32 SpawnedOrbCount = 0;\n'),
])
ed(P + 'ZoogiGameMode.cpp', [
 ('			ZoogiStageShift::OnRoundStart(this); // Oct 31 b46: halfway -> the arena takes its second form',
  '			ZoogiStageShift::OnRoundStart(this); // Oct 31 b46: halfway -> the arena takes its second form\n			SpireRefillOrbs(); // b86 (Evan): the spire generates new orbs every round'),
 ('''	for (AZoogiOrb* O : Orbs) ZoogiUtil::ApplyOrbStyle(O); // Oct 8: [ZoogiRoll.Characters] OrbStyle
	for (AZoogiOrb* O : PowerOrbs) O->MakePower(O->PowerColor);
}''',
  '''	for (AZoogiOrb* O : Orbs) ZoogiUtil::ApplyOrbStyle(O); // Oct 8: [ZoogiRoll.Characters] OrbStyle
	for (AZoogiOrb* O : PowerOrbs) O->MakePower(O->PowerColor);
	SpawnedOrbCount = Orbs.Num(); // b86
}

void AZoogiGameMode::SpireRefillOrbs()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSpireOrbs"));
	if (bOff || bRoyaleMode || bComboMode || !Arena || RoundNumber <= 1) return;
	const int32 Want = FMath::Clamp(FMath::Max(SpawnedOrbCount, 6) - OrbsLeft(), 0, 6);
	if (Want <= 0) { ZoogiSpireOrbs::Pulse(); return; }
	const float Decel = Arena->GetRollingDecel();
	const float RingR = Arena->OrbRingRadiusOverride > 0.f ? Arena->OrbRingRadiusOverride : ZoogiConst::OrbRingRadius;
	int32 Made = 0;
	for (int32 Try = 0; Try < 120 && Made < Want; ++Try)
	{
		const float A = FMath::FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(FMath::FRandRange(260.f * 260.f, RingR * RingR));
		FVector L(FMath::Cos(A) * D, FMath::Sin(A) * D, ZoogiConst::OrbRadius + 1.f);
		if (!OrbSpot(L)) continue;
		bool bBad = false;
		for (const AZoogiOrb* E : Orbs) if (E && !E->bOut && FVector::Dist2D(E->GetActorLocation(), L) < ZoogiConst::OrbRadius * 3.f) bBad = true;
		for (const AZoogiBall* B : Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), L) < 180.f) bBad = true;
		if (bBad) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (!O) continue;
		O->InitOrb(OrbPM, Decel);
		if (Made % 3 == 2) O->MakeStar();
		ZoogiUtil::ApplyOrbStyle(O);
		Orbs.Add(O);
		ZoogiSpireOrbs::Emit(O, 0.3f + 0.2f * Made);
		++Made;
	}
	if (Made) PublicFeed(FString::Printf(TEXT("ROUND %d  -  THE SPIRE SENDS %d NEW ORBS"), RoundNumber, Made), FLinearColor(0.5f, 0.9f, 1.f));
	UE_LOG(LogZoogi, Log, TEXT("SPIRE ORBS round %d: %d new (wanted %d, live before %d)"), RoundNumber, Made, Want, OrbsLeft() - Made);
}'''),
], inc='#include "ZoogiHoloSpire.h"', after='#include "ZoogiStageShift.h"')
print('b86b ok')
