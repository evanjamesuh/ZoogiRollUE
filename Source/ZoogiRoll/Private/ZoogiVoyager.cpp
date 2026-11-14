// b233: the Voyager pieces - see ZoogiVoyager.h
#include "ZoogiVoyager.h"
#include "KismetProceduralMeshLibrary.h" // b236
#include "ProceduralMeshComponent.h" // b236
#include "Materials/MaterialInstanceDynamic.h" // b235
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiRaid.h"
#include "ZoogiBossAttack.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiShooter.h"
#include "ZoogiRoyale.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace ZoogiVoyager
{
	bool On() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVoyager")); return b; }

	USkeletalMesh* Character(const FString& ZoogiName, int32 Fallback, FString* OutName)
	{
		if (!On()) return nullptr;
		static const bool bAny = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCineVoyager")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidCrew"));
		if (ZoogiShooter::Kind() == 0 && !bAny) return nullptr; // b243 (Evan): no soldiers in classic / raid / battle royale
		static const TCHAR* Order[][2] = { { TEXT("Hero"), TEXT("SK_Hero") }, { TEXT("ShadowOps"), TEXT("SK_ShadowOps") }, { TEXT("MurdockWasteland"), TEXT("SK_MurdockWasteland") },
			{ TEXT("WraithLunarOps"), TEXT("SK_WraithLunarOps") }, { TEXT("Murdock"), TEXT("SK_Murdock") }, { TEXT("Trooper"), TEXT("SK_Trooper") } };
		const FString N = ZoogiName.ToLower();
		int32 Pick = N == TEXT("blue") ? 0 : (N == TEXT("vex") || N.Contains(TEXT("dark"))) ? 1 : N == TEXT("hotstreak") ? 2 : N.Contains(TEXT("fros")) ? 3
			: (N == TEXT("lumi") || N == TEXT("luma")) ? 4 : FMath::Abs(Fallback) % 6;
		if (N == TEXT("-")) Pick = FMath::Abs(Fallback) % 6;
		static TMap<int32, TWeakObjectPtr<USkeletalMesh>> Cache;
		USkeletalMesh* M = nullptr;
		if (TWeakObjectPtr<USkeletalMesh>* C = Cache.Find(Pick)) M = C->Get();
		if (!M)
		{
			M = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Voyager/Demo/Characters/%s/%s.%s"), Order[Pick][0], Order[Pick][1], Order[Pick][1]), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (M) { M->AddToRoot(); Cache.Add(Pick, M); }
			else UE_LOG(LogZoogi, Warning, TEXT("VOYAGER: no character %s"), Order[Pick][1]);
		}
		if (OutName) *OutName = Order[Pick][0];
		return M;
	}

	UAnimSequence* Anim(const TCHAR* Rel)
	{
		static TMap<FString, TWeakObjectPtr<UAnimSequence>> Cache;
		if (TWeakObjectPtr<UAnimSequence>* C = Cache.Find(Rel)) if (C->IsValid()) return C->Get();
		const FString R(Rel); const FString Nm = FPaths::GetBaseFilename(R);
		UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/ZoogiExport/Anims/%s.%s"), Rel, *Nm), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (S) S->AddToRoot(); else UE_LOG(LogZoogi, Warning, TEXT("VOYAGER: no anim %s"), Rel);
		Cache.Add(R, S);
		return S;
	}

	void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDress"));
		if (!Sk || bOff || !On()) return;
		AActor* Ow = Sk->GetOwner(); if (!Ow) return;
		const FLinearColor Pri = bHollow ? FLinearColor(0.035f, 0.02f, 0.06f) : (B ? B->Def.Color : FLinearColor(0.2f, 0.6f, 1.f));
		const FLinearColor Sec = bHollow ? FLinearColor(0.55f, 0.12f, 1.f) : (B ? B->Def.Secondary : FLinearColor::White);
		// 1a: the armour - every colour / tint vector parameter of the Voyager materials
		int32 NSet = 0;
		for (int32 m = 0; m < Sk->GetNumMaterials(); ++m)
		{
			UMaterialInstanceDynamic* Mi = Sk->CreateDynamicMaterialInstance(m); if (!Mi) continue;
			TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids; Mi->GetAllVectorParameterInfo(Infos, Ids);
			FString Names;
			for (const FMaterialParameterInfo& In : Infos)
			{
				const FString Nm = In.Name.ToString(); Names += Nm + TEXT(" ");
				const bool bEm = Nm.Contains(TEXT("Emis")) || Nm.Contains(TEXT("Glow"));
				if (bEm) { Mi->SetVectorParameterValueByInfo(In, Sec); ++NSet; }
				else if (Nm.Contains(TEXT("Color")) || Nm.Contains(TEXT("Colour")) || Nm.Contains(TEXT("Tint")) || Nm.Contains(TEXT("Albedo")) || Nm.Contains(TEXT("Base")))
				{ Mi->SetVectorParameterValueByInfo(In, (m % 2) == 0 ? Pri : FMath::Lerp(Pri, Sec, 0.35f)); ++NSet; }
			}
			static int32 Logged = 0;
			if (Logged < 12) { ++Logged; UE_LOG(LogZoogi, Log, TEXT("VOYAGER DRESS material %d %s: vector params [ %s]"), m, *GetNameSafe(Mi->Parent), *Names); }
		}
		// 1b: shoulder pads + a glowing chest core in the Zoogi's colours (always there, whatever the materials allow)
		const float Ks = Sk->GetComponentScale().X;
		auto Part = [&](const TCHAR* Bone, const TCHAR* Shape, UMaterialInterface* Mat, const FVector& Size)
		{
			if (Sk->GetBoneIndex(Bone) == INDEX_NONE) return;
			UStaticMeshComponent* Cp = NewObject<UStaticMeshComponent>(Ow);
			Cp->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); Cp->SetMaterial(0, Mat); Cp->SetCollisionEnabled(ECollisionEnabled::NoCollision); Cp->SetCastShadow(false);
			Cp->SetUsingAbsoluteScale(true); Cp->RegisterComponent();
			Cp->AttachToComponent(Sk, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
			Cp->SetWorldScale3D(Size * Ks / 100.f);
		};
		UMaterialInterface* PadM = ZoogiUtil::SurfaceMID(Ow, Pri, 0.4f, Pri, 0.25f, 0.5f);
		Part(TEXT("upperarm_l"), TEXT("Sphere"), PadM, FVector(26.f)); Part(TEXT("upperarm_r"), TEXT("Sphere"), PadM, FVector(26.f));
		Part(TEXT("spine_03"), TEXT("Sphere"), ZoogiUtil::GlowMID(Ow, Sec, bHollow ? 6.f : 3.5f), FVector(13.f, 26.f, 13.f));
		// 2: the Zoogi's own ball (its face and all) as the head
		bool bHead = false;
		if (!bHollow && B && Sk->GetBoneIndex(TEXT("head")) != INDEX_NONE)
		{	// b236 (Evan: "Use the tripo 3d model heads not the ball"): the Zoogi's Tripo Baller model, everything under the neck sliced off
			static const TCHAR* Tm[] = { TEXT("Blue"), TEXT("Vex"), TEXT("Hotstreak"), TEXT("Frosti"), TEXT("Lumi") };
			const FString N = B->Def.Name.ToLower();
			const int32 Ti = N == TEXT("blue") ? 0 : (N == TEXT("vex") || N.Contains(TEXT("dark"))) ? 1 : N == TEXT("hotstreak") ? 2 : N.Contains(TEXT("fros")) ? 3 : (N == TEXT("lumi") || N == TEXT("luma")) ? 4 : (B->PlayerIndex + 3) % 5;
			UStaticMesh* Hm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ballers/%s/SM_%s.SM_%s"), Tm[Ti], Tm[Ti], Tm[Ti]), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Hm && (Hm->bAllowCPUAccess || GIsEditor))
			{
				static float CutK = -1.f, HeadCm = 46.f, HeadDown = 6.f;
				if (CutK < 0.f) { CutK = 0.62f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiHeadCut="), CutK); FParse::Value(FCommandLine::Get(), TEXT("ZoogiHeadCm="), HeadCm); FParse::Value(FCommandLine::Get(), TEXT("ZoogiHeadDown="), HeadDown); }
				const FBox Bb = Hm->GetBoundingBox(); const float Hh = FMath::Max(1.f, (float)Bb.GetSize().Z);
				const float CutZ = Bb.Min.Z + CutK * Hh;
				UStaticMeshComponent* Tmp = NewObject<UStaticMeshComponent>(Ow); Tmp->SetStaticMesh(Hm);
				UProceduralMeshComponent* Pm = NewObject<UProceduralMeshComponent>(Ow, TEXT("ZoogiTripoHead"));
				Pm->SetCollisionEnabled(ECollisionEnabled::NoCollision); Pm->RegisterComponent(); Pm->SetWorldTransform(FTransform::Identity);
				UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(Tmp, 0, Pm, false);
				UProceduralMeshComponent* Other = nullptr;
				UKismetProceduralMeshLibrary::SliceProceduralMesh(Pm, FVector(0.f, 0.f, CutZ), FVector(0.f, 0.f, 1.f), false, Other, EProcMeshSliceCapOption::UseLastSectionForCap, nullptr);
				Tmp->DestroyComponent();
				if (Pm->GetNumSections() > 0)
				{
					const float Sc = HeadCm * Ks / FMath::Max(1.f, (1.f - CutK) * Hh);
					const FQuat Rq = Sk->GetComponentQuat(); // the soldiers and the Tripo models both face local +Y
					const FVector Neck = Sk->GetSocketLocation(TEXT("head")) - FVector(0.f, 0.f, HeadDown * Ks);
					const FVector Cut(Bb.GetCenter().X, Bb.GetCenter().Y, CutZ);
					Pm->SetWorldTransform(FTransform(Rq, Neck - Rq.RotateVector(Cut * Sc), FVector(Sc)));
					Pm->AttachToComponent(Sk, FAttachmentTransformRules::KeepWorldTransform, TEXT("head"));
					Pm->SetCastShadow(false);
					bHead = true;
				}
				else Pm->DestroyComponent();
			}
			UE_LOG(LogZoogi, Log, TEXT("VOYAGER DRESS head: Tripo %s %s"), Tm[Ti], bHead ? TEXT("on") : (Hm ? TEXT("no CPU access / empty slice - own head") : TEXT("missing")));
		}
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER DRESS %s: %d tint params set, pads + core, %s"), bHollow ? TEXT("VOID HOLLOW") : (B ? *B->Def.Name : TEXT("-")), NSet, bHead ? TEXT("Tripo head on") : TEXT("own head"));
	}

	USkeletalMesh* Gun(const TCHAR* Short)
	{
		if (!On()) return nullptr;
		static TMap<FString, TWeakObjectPtr<USkeletalMesh>> Cache;
		if (TWeakObjectPtr<USkeletalMesh>* C = Cache.Find(Short)) if (C->IsValid()) return C->Get();
		USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Voyager/Demo/Meshes/%s/SK_%s.SK_%s"), Short, Short, Short), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (M) M->AddToRoot(); else UE_LOG(LogZoogi, Warning, TEXT("VOYAGER: no gun %s"), Short);
		Cache.Add(Short, M);
		return M;
	}

	void Fx(UWorld* W, const TCHAR* Short, const FVector& At, float Scale, float Life, AActor* Follow, const FRotator& Rot)
	{
		if (!W || !On()) return;
		static const TCHAR* Map[][2] = {
			{ TEXT("RocketTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/RocketRibbon/NS_RocketTrailV2") },
			{ TEXT("PlasmaTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/ProjectileTrail/NS_PlasmaTrail") },
			{ TEXT("MachinegunTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/ProjectileTrail/NS_MachinegunTrail") },
			{ TEXT("SniperTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/ProjectileTrail/NS_SniperTrail") },
			{ TEXT("ProjectileTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/ProjectileTrail/NS_ProjectileTrail") },
			{ TEXT("RocketBoom"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Explosion/NS_Rocket_Explosion") },
			{ TEXT("PlasmaBoom"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Explosion/NS_BarrelPlasma_Explosion") },
			{ TEXT("GrenadeBoom"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Explosion/NS_Grenade_Explosion") },
			{ TEXT("BarrelBoom"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Explosion/NS_Barrel_Explosion") },
			{ TEXT("ImpactMetal"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Impacts/NS_ImpactMetal") },
			{ TEXT("ImpactConcrete"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Impacts/NS_ImpactConcrete") },
			{ TEXT("Tracer"), TEXT("/Game/Voyager/Addons/VoyagerVFX/StylizedVFX/Effects/Particles/Weapons/NS_WeaponFire_Tracer") },
			{ TEXT("Flamethrower"), TEXT("/Game/Voyager/Addons/VoyagerVFX/FlameThrower/NS_Flamethrower2") },
			{ TEXT("Shield"), TEXT("/Game/Voyager/Addons/VoyagerVFX/Barrier/NS_Shield") },
			{ TEXT("Powerup"), TEXT("/Game/Voyager/Addons/VoyagerVFX/Powerup/NS_Powerup") },
			{ TEXT("SwordTrail"), TEXT("/Game/Voyager/Addons/VoyagerVFX/FireSlash/NS_SwordTrail") },
			{ TEXT("Slash"), TEXT("/Game/Voyager/Addons/VoyagerVFX/FireSlash/NS_SlashParticles") } };
		for (const auto& M : Map)
			if (FCString::Stricmp(M[0], Short) == 0)
			{
				const FString P = FString::Printf(TEXT("%s.%s"), M[1], *FPaths::GetBaseFilename(M[1]));
				AZoogiFabFX::Play(W, *P, At, Scale, Life, Follow, FVector::ZeroVector, Rot);
				return;
			}
	}

	// ---------------------------------------------------------------- D: raid companions
	namespace
	{
		struct FShot { TWeakObjectPtr<AActor> C; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f, Dur = 0.2f; int32 Victim = -1; };
		struct FCrew
		{
			TWeakObjectPtr<AZoogiGameMode> G; TWeakObjectPtr<ASkeletalMeshActor> Soldier; TWeakObjectPtr<AActor> Turret; TWeakObjectPtr<USceneComponent> TurHead;
			FVector SoldierAt = FVector::ZeroVector, TurretAt = FVector::ZeroVector; double NextSoldier = 0.0, NextTurret = 0.0; int32 Burst = 0, Shots = 0; bool bTried = false;
			TArray<FShot> Shots2; bool bHtl = false; int32 Tgt = -1; // b234: HOLD THE LINE - the crew shoots the enemy soldiers
		};
		FCrew Crew;
		void Clear()
		{
			if (AActor* A = Crew.Soldier.Get()) A->Destroy();
			if (AActor* A = Crew.Turret.Get()) A->Destroy();
			for (FShot& S : Crew.Shots2) if (AActor* C = S.C.Get()) C->Destroy();
			Crew = FCrew();
		}
		AStaticMeshActor* Round(UWorld* W, const FVector& At, const FVector& Dir, const FLinearColor& Col)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* C = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Dir.Rotation(), At), SP);
			if (!C) return nullptr;
			C->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* Mc = C->GetStaticMeshComponent();
			Mc->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mc->SetCastShadow(false);
			Mc->SetMaterial(0, ZoogiUtil::FxMID(C, Col, 20.f));
			C->SetActorScale3D(FVector(0.5f, 0.08f, 0.08f));
			return C;
		}
		void Fire(AZoogiGameMode* G, const FVector& From, const FVector& To, const FLinearColor& Col, int32 Victim = -1)
		{
			UWorld* W = G->GetWorld();
			const FVector Dir = (To - From).GetSafeNormal();
			AZoogiFX::Spawn(W, From, Col, 60.f, 0.08f, false, 16.f); // muzzle flash
			FShot S; S.From = From; S.To = To + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-40.f, 40.f));
			S.Dur = FMath::Clamp(FVector::Dist(From, S.To) / 6000.f, 0.08f, 0.7f);
			S.Victim = Victim;
			if (AStaticMeshActor* C = Round(W, From, Dir, Col)) { S.C = C; Fx(W, TEXT("MachinegunTrail"), From, 1.f, S.Dur + 0.6f, C, Dir.Rotation()); }
			Crew.Shots2.Add(S);
		}
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld() || !On()) return;
		static const bool bOff = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidCrew")); // b236 (Evan): no soldiers / Voyager turret on the arena floor
		if (Crew.G.Get() != G) Clear();
		FVector Bp = FVector::ZeroVector;
		const bool bHtl = ZoogiShooter::On(G) && ZoogiShooter::Kind() == 1 && G->Royale.Step != EZoogiRoyaleStep::Over && G->Balls.IsValidIndex(G->Royale.HumanIndex);
		if (bHtl)
		{	// b234 HOLD THE LINE: aim at the nearest enemy soldier (none between waves - the crew holds fire)
			const FVector Mid = Crew.bTried ? (Crew.SoldierAt + Crew.TurretAt) * 0.5f : G->Balls[G->Royale.HumanIndex]->SpawnLocation;
			Crew.Tgt = ZoogiShooter::CrewTarget(G, Mid);
			Bp = Crew.Tgt >= 0 ? G->Balls[Crew.Tgt]->BallLocation() + FVector(0.f, 0.f, 40.f) : (G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector);
		}
		const bool bWant = !bOff && G->Arena && (bHtl || (ZoogiRaid::Active() && ZoogiBossAttack::LockOnTarget(G, Bp)));
		if (!bWant) { if (Crew.Soldier.IsValid() || Crew.Turret.IsValid()) Clear(); return; }
		UWorld* W = G->GetWorld();
		const double Tn = FPlatformTime::Seconds();
		if (!Crew.bTried)
		{	// spawn once: the trooper on the left, the turret on the right, between the court centre and the boss
			Crew.bTried = true; Crew.G = G;
			const FVector Ctr = G->Arena->GetActorLocation();
			float FloorZ = Ctr.Z; for (const AZoogiBall* B : G->Balls) if (B) { FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius; break; }
			const FVector Dir = FVector(Bp.X - Ctr.X, Bp.Y - Ctr.Y, 0.f).GetSafeNormal(), Side(-Dir.Y, Dir.X, 0.f);
			static float Out = -1.f; if (Out < 0.f) { Out = 420.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRaidCrewOut="), Out); }
			Crew.SoldierAt = FVector(Ctr.X, Ctr.Y, FloorZ) + Dir * Out + Side * 520.f;
			Crew.TurretAt = FVector(Ctr.X, Ctr.Y, FloorZ) + Dir * Out - Side * 520.f;
			if (bHtl)
			{	// b234: beside your start spot, a little toward the middle
				const FVector Hs = G->Balls[G->Royale.HumanIndex]->SpawnLocation; const FVector In = FVector(Ctr.X - Hs.X, Ctr.Y - Hs.Y, 0.f).GetSafeNormal(), Sd(-In.Y, In.X, 0.f);
				Crew.SoldierAt = FVector(Hs.X, Hs.Y, FloorZ) + In * 160.f + Sd * 320.f; Crew.TurretAt = FVector(Hs.X, Hs.Y, FloorZ) + In * 160.f - Sd * 320.f;
			}
			Crew.bHtl = bHtl;
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FString Nm;
			if (USkeletalMesh* M = Character(TEXT("-"), 5, &Nm))
				if (ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(FRotator(0.f, Dir.Rotation().Yaw - 90.f, 0.f), Crew.SoldierAt), SP))
				{
					USkeletalMeshComponent* Sk = A->GetSkeletalMeshComponent();
					Sk->SetMobility(EComponentMobility::Movable); Sk->SetSkeletalMeshAsset(M); Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Sk->SetAnimationMode(EAnimationMode::AnimationSingleNode); Sk->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					if (UAnimSequence* Id = Anim(TEXT("Soldier/Anim_Idle_Rifle_Hip"))) Sk->PlayAnimation(Id, true);
					A->SetActorScale3D(FVector(1.15f));
					if (USkeletalMesh* Gm = Gun(TEXT("HeavyMachineGun")))
					{
						USkeletalMeshComponent* Gc = NewObject<USkeletalMeshComponent>(A);
						Gc->SetSkeletalMeshAsset(Gm); Gc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gc->RegisterComponent();
						Gc->AttachToComponent(Sk, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_rSocket"));
					}
					Crew.Soldier = A;
				}
			{	// the Voyager minigun turret: stand + body + minigun
				AActor* T = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Crew.TurretAt), SP);
				if (T)
				{
					USceneComponent* Root = NewObject<USceneComponent>(T); T->SetRootComponent(Root); Root->RegisterComponent(); Root->SetWorldLocation(Crew.TurretAt);
					auto Part = [&](USceneComponent* Parent, const TCHAR* P, const FVector& Loc) -> UStaticMeshComponent*
					{
						UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, P, nullptr, LOAD_NoWarn | LOAD_Quiet); if (!M) return nullptr;
						UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(T); C->SetStaticMesh(M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						C->SetupAttachment(Parent); C->SetRelativeLocation(Loc); C->RegisterComponent(); return C;
					};
					Part(Root, TEXT("/Game/Voyager/Demo/Meshes/TurretMachineGun/SM_TurretStand.SM_TurretStand"), FVector::ZeroVector);
					USceneComponent* Head = NewObject<USceneComponent>(T); Head->SetupAttachment(Root); Head->SetRelativeLocation(FVector(0.f, 0.f, 72.f)); Head->RegisterComponent();
					Part(Head, TEXT("/Game/Voyager/Demo/Meshes/TurretMachineGun/SM_TurretMain.SM_TurretMain"), FVector::ZeroVector);
					Part(Head, TEXT("/Game/Voyager/Demo/Meshes/TurretMachineGun/SM_TurretMinigun.SM_TurretMinigun"), FVector(30.f, 0.f, 0.f));
					T->SetActorScale3D(FVector(1.6f));
					Crew.Turret = T; Crew.TurHead = Head;
				}
			}
			Crew.NextSoldier = Tn + 2.0; Crew.NextTurret = Tn + 3.0;
			UE_LOG(LogZoogi, Log, TEXT("VOYAGER RAID CREW: trooper %s + minigun turret beside the court (boss at %.0f, %.0f)"), Crew.Soldier.IsValid() ? TEXT("in") : TEXT("missing"), Bp.X, Bp.Y);
		}
		const bool bHide = ZoogiCosmosBoss::BossCamOn();
		if (ASkeletalMeshActor* S = Crew.Soldier.Get())
		{
			S->SetActorHiddenInGame(bHide);
			S->SetActorRotation(FRotator(0.f, (Bp - Crew.SoldierAt).GetSafeNormal2D().Rotation().Yaw - 90.f, 0.f));
			if (Tn >= Crew.NextSoldier)
			{	// a 4-round burst every ~2.4 s
				const FVector Mz = Crew.SoldierAt + FVector(0.f, 0.f, 140.f) + (Bp - Crew.SoldierAt).GetSafeNormal2D() * 70.f;
				if (!Crew.bHtl || Crew.Tgt >= 0) Fire(G, Mz, Bp, FLinearColor(1.f, 0.82f, 0.35f), Crew.bHtl ? Crew.Tgt : -1);
				Crew.NextSoldier = Tn + ((++Crew.Burst % 4) == 0 ? 2.4 : 0.11);
			}
		}
		if (AActor* T = Crew.Turret.Get())
		{
			T->SetActorHiddenInGame(bHide);
			const FVector Td = (Bp - Crew.TurretAt).GetSafeNormal();
			if (USceneComponent* H = Crew.TurHead.Get()) H->SetWorldRotation(FRotator(FMath::Clamp(Td.Rotation().Pitch, -10.f, 40.f), Td.Rotation().Yaw, 0.f));
			if (Tn >= Crew.NextTurret)
			{
				const FVector Mz = Crew.TurretAt + FVector(0.f, 0.f, 72.f * 1.6f) + Td * 110.f;
				if (!Crew.bHtl || Crew.Tgt >= 0) Fire(G, Mz, Bp, FLinearColor(0.4f, 0.9f, 1.f), Crew.bHtl ? Crew.Tgt : -1);
				static int32 Tb = 0;
				Crew.NextTurret = Tn + ((++Tb % 8) == 0 ? 3.0 : 0.09);
			}
		}
		for (int32 i = Crew.Shots2.Num() - 1; i >= 0; --i)
		{
			FShot& S = Crew.Shots2[i];
			S.T += RealDt;
			const float K = FMath::Clamp(S.T / S.Dur, 0.f, 1.f);
			if (AActor* C = S.C.Get()) C->SetActorLocation(FMath::Lerp(S.From, S.To, K));
			if (K < 1.f) continue;
			if (AActor* C = S.C.Get()) C->Destroy();
			Fx(W, TEXT("ImpactMetal"), S.To, 1.f, 1.5f);
			if (S.Victim >= 0) G->RoyaleDamage(S.Victim, -1, 12, false, S.To); // b234 HOLD THE LINE: a crew round stings a soldier (no credit)
			else if ((++Crew.Shots % 10) == 0) ZoogiCosmosBoss::Flinch(); // the crew only stings - your hits still do the damage
			Crew.Shots2.RemoveAt(i);
		}
	}
}
