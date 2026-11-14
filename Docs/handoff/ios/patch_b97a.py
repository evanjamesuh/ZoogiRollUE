# b97a (Evan 18:32: "The vortex needs work and should look like the demo no exceptions"): the KriptoFX Realistic Effects 4 portal
# meteor is already in the project as /Game/VFX/KriptoFX/Effect5 (Niagara port + portal disc + rings + rock). Meteor Rain now plays
# that real prefab, aimed so its rock lane runs from the sky portal straight onto the target (prefab: portal at (0,0,190), impact
# ~(-500,0,0) -> rotated / scaled to Portal -> Hit). The KFX rock mesh + material replace the Inferno boulder, the KFX portal-out /
# rock-explosion / rock-impact sounds play, my black void stays behind the KFX swirl disc. -ZoogiMeteorNoKFX = the b95 look.
# Test: -ZoogiSkillOnly=<id> (with -ZoogiSkillTest) casts only that skill, every 6 s, with frames through the strike.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('#include "NiagaraComponent.h"\n', '#include "NiagaraComponent.h"\n#include "NiagaraFunctionLibrary.h"\n#include "NiagaraSystem.h"\n#include "ZoogiKFXExtras.h"\n'),
 ('''			V->Disc = Shape(TEXT("Cylinder"), FLinearColor(0.08f, 0.02f, 0.18f), 1.f, Portal, Face);''',
  '''			// b97a: the real KriptoFX Effect5 prefab, its rock lane laid from the portal onto the target
			static const bool bNoKfx5 = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMeteorNoKFX"));
			float Kfx5Scale = 0.f;
			if (!bNoKfx5)
				if (UNiagaraSystem* Sys5 = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/NS_KFX_Effect5.NS_KFX_Effect5")))
				{
					const FVector D5 = Hit - Portal;
					Kfx5Scale = FMath::Clamp((float)D5.Size() / 535.f, 1.2f, 4.5f);
					const float ElevA = FMath::RadiansToDegrees(FMath::Atan2(-190.f, 500.f)), ElevD = FMath::RadiansToDegrees(FMath::Atan2((float)D5.Z, (float)D5.Size2D()));
					const FRotator R5(ElevA - ElevD, Dir.Rotation().Yaw + 180.f, 0.f);
					const FVector O5 = Portal - R5.RotateVector(FVector(0.f, 0.f, 190.f * Kfx5Scale));
					if (UNiagaraComponent* N5 = UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, Sys5, O5, R5, FVector(Kfx5Scale), true, true, ENCPoolMethod::None))
					{
						TWeakObjectPtr<UNiagaraComponent> WN(N5); FTimerHandle H1, H2;
						W->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WN]() { if (WN.IsValid()) WN->Deactivate(); }), 3.2f, false);
						W->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WN]() { if (WN.IsValid()) WN->DestroyComponent(); }), 7.f, false);
					}
					if (AZoogiKFXExtras* X5 = AZoogiKFXExtras::Spawn(W, TEXT("Effect5"), O5, Kfx5Scale, 3.f)) X5->SetActorRotation(R5);
					if (USoundBase* So = LoadObject<USoundBase>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_PortalOut.S_PortalOut"))) UGameplayStatics::PlaySoundAtLocation(W, So, Portal, 1.f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL meteor_rain: KriptoFX Effect5 at scale %.2f, pitch %.1f"), Kfx5Scale, R5.Pitch);
				}
			V->Disc = Shape(TEXT("Cylinder"), FLinearColor(0.08f, 0.02f, 0.18f), 1.f, Portal, Face);'''),
 ('''			V->Ring = Shape(TEXT("Cylinder"), Pu, 6.f, Portal - (Hit - Portal).GetSafeNormal() * 4.f, Face);''',
  '''			V->Ring = Shape(TEXT("Cylinder"), Pu, 6.f, Portal - (Hit - Portal).GetSafeNormal() * 4.f, Face);
			if (Kfx5Scale > 0.f) { if (AActor* Rg = V->Ring.Get()) Rg->SetActorHiddenInGame(true); V->DiscMul = 1.54f * Kfx5Scale / 3.6f; } // the KFX swirl disc is the rim now'''),
 ('''				UStaticMesh* RockM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockBig_001.SM_RockBig_001"));
				Mc->SetStaticMesh(RockM ? RockM : ZoogiUtil::BasicShape(TEXT("Sphere")));''',
  '''				UStaticMesh* RockM = Kfx5Scale > 0.f ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Meshes/SM_Rock.SM_Rock")) : nullptr; // b97a: the demo's rock
				if (!RockM) RockM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockBig_001.SM_RockBig_001"));
				Mc->SetStaticMesh(RockM ? RockM : ZoogiUtil::BasicShape(TEXT("Sphere")));
				if (Kfx5Scale > 0.f) if (UMaterialInterface* RkMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Materials/MI_KFX_Effect5_MeshRock2mat.MI_KFX_Effect5_MeshRock2mat"))) for (int32 m = 0; m < FMath::Max(1, Mc->GetNumMaterials()); ++m) Mc->SetMaterial(m, RkMat);'''),
 ('''			struct FVoid { TWeakObjectPtr<AActor> Disc, Ring, Rock, Arcs, Cam, PrevView; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f, Spark = 0.f, Arc = 0.f; bool bHit = false, bCamBack = false; FVector Side = FVector::RightVector, Fwd = FVector::ForwardVector; };''',
  '''			struct FVoid { TWeakObjectPtr<AActor> Disc, Ring, Rock, Arcs, Cam, PrevView; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f, Spark = 0.f, Arc = 0.f, DiscMul = 1.f; bool bHit = false, bCamBack = false; FVector Side = FVector::RightVector, Fwd = FVector::ForwardVector; };'''),
 ('''				if (AActor* D = V->Disc.Get()) D->SetActorScale3D(FVector(3.6f * Open * Wob, 3.6f * Open / Wob, 0.03f));''',
  '''				if (AActor* D = V->Disc.Get()) D->SetActorScale3D(FVector(3.6f * V->DiscMul * Open * Wob, 3.6f * V->DiscMul * Open / Wob, 0.03f));'''),
 ('''					ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 0.9f, 0.6f);
					WolfBlast(Gm, Hit, Pi, 1.f);''',
  '''					ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 0.9f, 0.6f);
					for (const TCHAR* Sp5 : { TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_RockExplosion.S_RockExplosion"), TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_RockImpact.S_RockImpact") }) // b97a: the demo's impact
						if (USoundBase* So = LoadObject<USoundBase>(nullptr, Sp5)) UGameplayStatics::PlaySoundAtLocation(Wm, So, Hit, 1.f);
					WolfBlast(Gm, Hit, Pi, 1.f);'''),
 # test: one skill only, repeated, frames through it
 ('''		if (bTest && !Picking() && TestIdx < (int32)UE_ARRAY_COUNT(Defs) && G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && T >= NextTest)
		{
			NextTest = T + 3.2f;
			const FName Id(Defs[TestIdx++].Id);''',
  '''		static FString OnlySkill; static bool bOnlyRead = false; if (!bOnlyRead) { bOnlyRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiSkillOnly="), OnlySkill); }
		if (bTest && !OnlySkill.IsEmpty() && !Picking() && G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && T >= NextTest && TestIdx < 4)
		{	// b97a: -ZoogiSkillOnly=<id> - that skill only, every 6 s, frames through the effect
			NextTest = T + 6.f; ++TestIdx;
			UE_LOG(LogZoogi, Log, TEXT("SKILL_TEST only %s #%d"), *OnlySkill, TestIdx);
			AutoCast(G, FName(*OnlySkill));
			if (!G->ShotDir.IsEmpty()) for (float St : { 0.3f, 0.6f, 0.9f, 1.15f, 1.4f, 1.7f, 2.2f }) G->TakeShotDelayed(FString::Printf(TEXT("only_%s_%d_%03d"), *OnlySkill, TestIdx, FMath::RoundToInt(St * 100.f)), St);
		}
		if (bTest && OnlySkill.IsEmpty() && !Picking() && TestIdx < (int32)UE_ARRAY_COUNT(Defs) && G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && T >= NextTest)
		{
			NextTest = T + 3.2f;
			const FName Id(Defs[TestIdx++].Id);'''),
])
print('b97a ok')
