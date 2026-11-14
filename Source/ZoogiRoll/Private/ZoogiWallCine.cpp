// b230: the wall-smash cinematic - see ZoogiWallCine.h
#include "ZoogiWallCine.h"
#include "ZoogiAnimeCues.h" // b245
#include "ZoogiAwardBanner.h" // b239
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFilm.h"
#include "ZoogiEarthEvents.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "ZoogiVoyager.h" // b233
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "ZoogiArenaPiece.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

namespace ZoogiWallCine
{
	namespace
	{
		struct FFrozen { TWeakObjectPtr<AZoogiBall> B; FVector Lin = FVector::ZeroVector, Ang = FVector::ZeroVector; bool bSim = false, bHid = false; };
		struct FWid { TWeakObjectPtr<UUserWidget> Wd; ESlateVisibility Vis = ESlateVisibility::Visible; };
		struct FCine
		{
			bool bOn = false; int32 Kind = 3; double T0 = 0.0, Last = 0.0; float Tl = 0.f; int32 Ev = 0;
			TWeakObjectPtr<AZoogiGameMode> G; TWeakObjectPtr<AZoogiBall> Att, Vic;
			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> OldView; TWeakObjectPtr<AStaticMeshActor> MA, MB, Rock; TWeakObjectPtr<ASkeletalMeshActor> VA, VB; bool bVoy = false, bRag = false; // b233: Voyager fighters
			FVector Dir = FVector(1, 0, 0), Side = FVector(0, 1, 0), APos = FVector::ZeroVector, VPos = FVector::ZeroVector, RockAt = FVector::ZeroVector, Hit = FVector::ZeroVector;
			float FloorZ = 0.f, HA = 1.f, HB = 1.f, MinA = 0.f, MinB = 0.f, Shake = 0.f;
			double NextTrail = 0.0;
			TArray<FFrozen> Frozen; TArray<TWeakObjectPtr<AActor>> Hidden; TArray<FWid> Wids;
			FString AttName, VicName;
		};
		FCine C;
		const float ModelH = 250.f, Len = 5.0f, tPunch = 0.95f, tHit = 1.55f;

		float Ease(float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
		float Seg(float t, float a, float b) { return FMath::Clamp((t - a) / FMath::Max(0.001f, b - a), 0.f, 1.f); }
		void Once(int32 Id, TFunctionRef<void()> Fn) { if (!(C.Ev & (1 << Id))) { C.Ev |= (1 << Id); Fn(); } }

		const TCHAR* ModelPath(const FString& Name)
		{	// the Tripo Ballers (b208) + the Dark Zoogi (b202)
			static const TCHAR* Known[][2] = {
				{ TEXT("Blue"), TEXT("/Game/Zoogis/Ballers/Blue/SM_Blue.SM_Blue") }, { TEXT("Vex"), TEXT("/Game/Zoogis/Ballers/Vex/SM_Vex.SM_Vex") },
				{ TEXT("Frosti"), TEXT("/Game/Zoogis/Ballers/Frosti/SM_Frosti.SM_Frosti") }, { TEXT("Lumi"), TEXT("/Game/Zoogis/Ballers/Lumi/SM_Lumi.SM_Lumi") },
				{ TEXT("Hotstreak"), TEXT("/Game/Zoogis/Ballers/Hotstreak/SM_Hotstreak.SM_Hotstreak") }, { TEXT("Dark"), TEXT("/Game/Zoogis/Dark/SM_DarkZoogi.SM_DarkZoogi") } };
			for (const auto& K : Known) if (Name.Contains(K[0], ESearchCase::IgnoreCase)) return K[1];
			return nullptr;
		}
		AStaticMeshActor* Model(UWorld* W, const FString& Name, int32 Fallback, float& OutScale, float& OutMinZ, FString& OutName)
		{
			static const TCHAR* Order[] = { TEXT("Blue"), TEXT("Vex"), TEXT("Hotstreak"), TEXT("Frosti"), TEXT("Lumi"), TEXT("Dark") };
			const TCHAR* P = ModelPath(Name);
			OutName = P ? Name : FString(Order[FMath::Abs(Fallback) % 6]);
			if (!P) P = ModelPath(OutName);
			UStaticMesh* M = P ? LoadObject<UStaticMesh>(nullptr, P, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
			if (!M) return nullptr;
			const FBox Bb = M->GetBoundingBox();
			OutScale = ModelH / FMath::Max(1.f, (float)Bb.GetSize().Z); OutMinZ = Bb.Min.Z;
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FVector(0, 0, -5000.f)), Sp);
			if (!A) return nullptr;
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(M);
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->GetStaticMeshComponent()->SetWorldScale3D(FVector(OutScale)); A->SetActorHiddenInGame(true);
			return A;
		}
		FRotator Face(const FVector& Dir, float Pitch = 0.f, float Roll = 0.f) { return FRotator(Pitch, Dir.Rotation().Yaw - 90.f, Roll); } // the Tripo models face local +Y
		void Place(AStaticMeshActor* A, const FVector& Feet, const FRotator& Rot, float Scale, float MinZ, float SquashZ = 1.f)
		{
			if (!A) return;
			A->SetActorHiddenInGame(false);
			A->GetStaticMeshComponent()->SetWorldScale3D(FVector(Scale, Scale, Scale * SquashZ));
			A->SetActorLocationAndRotation(Feet - Rot.RotateVector(FVector(0, 0, MinZ * Scale * SquashZ)), Rot);
		}
		void Look(const FVector& Eye, const FVector& At)
		{
			ACameraActor* Cm = C.Cam.Get(); if (!Cm) return;
			const FVector J = C.Shake > 0.f ? FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * 14.f * C.Shake : FVector::ZeroVector;
			Cm->SetActorLocationAndRotation(Eye + J, (At - Eye).Rotation());
		}
		void HideWidgets(UWorld* W)
		{	// the match UI (portraits, action menu, bubbles) is hidden while the cinematic plays - re-checked every frame
			TArray<UUserWidget*> Ws; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W, Ws, UUserWidget::StaticClass(), true);
			for (UUserWidget* Uw : Ws)
			{
				if (!Uw || !Uw->IsInViewport()) continue;
				const ESlateVisibility Vz = Uw->GetVisibility();
				if (Vz == ESlateVisibility::Hidden || Vz == ESlateVisibility::Collapsed) continue;
				if (!C.Wids.ContainsByPredicate([Uw](const FWid& X) { return X.Wd.Get() == Uw; })) { FWid F; F.Wd = Uw; F.Vis = Vz; C.Wids.Add(F); }
				Uw->SetVisibility(ESlateVisibility::Hidden);
			}
		}
		// b233 (Evan: "A" - the Voyager characters): animated fighters - punch, hit react, ragdoll off the rock
		ASkeletalMeshActor* VoyFighter(UWorld* W, const FString& Name, int32 Fallback, float& OutScale, float& OutMinZ, FString& OutName)
		{
			USkeletalMesh* M = ZoogiVoyager::Character(Name, Fallback, &OutName); if (!M) return nullptr;
			const FBox Bb = M->GetBounds().GetBox();
			OutScale = ModelH * 0.92f / FMath::Max(1.f, (float)Bb.GetSize().Z); OutMinZ = Bb.Min.Z;
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(FVector(0, 0, -5000.f)), Sp);
			if (!A) return nullptr;
			USkeletalMeshComponent* Sk = A->GetSkeletalMeshComponent();
			Sk->SetMobility(EComponentMobility::Movable); Sk->SetSkeletalMeshAsset(M); Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sk->SetAnimationMode(EAnimationMode::AnimationSingleNode); Sk->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Sk->bEnableUpdateRateOptimizations = false;
			A->SetActorScale3D(FVector(OutScale)); A->SetActorHiddenInGame(true);
			return A;
		}
		void PlayV(ASkeletalMeshActor* A, const TCHAR* Rel, bool bLoop, float Rate = 1.f)
		{
			if (!A) return;
			if (UAnimSequence* S = ZoogiVoyager::Anim(Rel)) { USkeletalMeshComponent* Sk = A->GetSkeletalMeshComponent(); Sk->PlayAnimation(S, bLoop); Sk->SetPlayRate(Rate); }
		}
		void PlaceV(ASkeletalMeshActor* A, const FVector& Feet, const FRotator& Rot, float Scale, float MinZ)
		{
			if (!A) return;
			A->SetActorHiddenInGame(false);
			A->SetActorLocationAndRotation(Feet - Rot.RotateVector(FVector(0, 0, MinZ * Scale)), Rot);
		}
		void Ragdoll(ASkeletalMeshActor* A, const FVector& Vel)
		{
			if (!A) return;
			USkeletalMeshComponent* Sk = A->GetSkeletalMeshComponent();
			Sk->SetCollisionProfileName(TEXT("Ragdoll")); Sk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Sk->SetAllBodiesSimulatePhysics(true); Sk->SetSimulatePhysics(true); Sk->WakeAllRigidBodies();
			Sk->SetAllPhysicsLinearVelocity(Vel);
			Sk->SetAllPhysicsAngularVelocityInRadians(FVector(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-2.f, 2.f)));
		}
		void Burst(UWorld* W, const TCHAR* Name, const FVector& Pos, float Scl) { AZoogiFX::Stylized(W, Name, Pos, Scl, FRotator::ZeroRotator); }

		void Finish()
		{
			for (const FFrozen& F : C.Frozen)
				if (AZoogiBall* B = F.B.Get())
				{
					if (!B->bOut) B->SetActorHiddenInGame(F.bHid);
					if (B->Sphere && F.bSim && !B->bOut) { B->Sphere->SetSimulatePhysics(true); B->Sphere->SetPhysicsLinearVelocity(F.Lin); B->Sphere->SetPhysicsAngularVelocityInRadians(F.Ang); }
				}
			for (const TWeakObjectPtr<AActor>& H : C.Hidden) if (AActor* A = H.Get()) A->SetActorHiddenInGame(false);
			for (const FWid& Fw : C.Wids) if (UUserWidget* Uw = Fw.Wd.Get()) Uw->SetVisibility(Fw.Vis);
			if (AZoogiGameMode* G = C.G.Get()) if (UWorld* W = G->GetWorld()) if (APlayerController* Pc = W->GetFirstPlayerController())
				if (AActor* Ov = C.OldView.Get()) Pc->SetViewTargetWithBlend(Ov, 0.25f);
			for (AActor* A : { (AActor*)C.MA.Get(), (AActor*)C.MB.Get(), (AActor*)C.Rock.Get(), (AActor*)C.VA.Get(), (AActor*)C.VB.Get() }) if (A) A->Destroy();
			if (ACameraActor* Cm = C.Cam.Get()) Cm->SetLifeSpan(0.6f);
			UE_LOG(LogZoogi, Log, TEXT("WALL CINE done (kind %d)"), C.Kind);
			C.bOn = false; C.Frozen.Reset(); C.Hidden.Reset(); C.Wids.Reset();
		}
	}

	bool Active() { return C.bOn; }
	bool BannerUp() { return ZoogiAwardBanner::Showing(); } // b239

	bool Play(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWallCine"));
		if (bOff || C.bOn || !G || !G->GetWorld() || !G->Arena || !Attacker || !Victim || Attacker == Victim) return false;
		UWorld* W = G->GetWorld();
		const FCine Fresh; C = Fresh;
		C.G = G; C.Att = Attacker; C.Vic = Victim; C.Kind = FMath::Clamp(Kind, 1, 6);
		const FVector Ctr = G->Arena->GetActorLocation();
		FVector Vp = Victim->BallLocation();
		C.FloorZ = Attacker->BallLocation().Z - ZoogiConst::ZoogiRadius;
		C.Dir = FVector(Vp.X - Ctr.X, Vp.Y - Ctr.Y, 0.f).GetSafeNormal(); // blown back outward, toward the rim
		if (C.Dir.IsNearlyZero()) C.Dir = (Vp - Attacker->BallLocation()).GetSafeNormal2D();
		if (C.Dir.IsNearlyZero()) C.Dir = FVector(1, 0, 0);
		// keep the whole set on the court: the rock no further out than the rim allows
		const float Rv = FVector::Dist2D(Vp, Ctr);
		static float CourtR = -1.f; if (CourtR < 0.f) { CourtR = 1500.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWallCineCourt="), CourtR); }
		if (Rv + 760.f > CourtR) Vp = Ctr + C.Dir * FMath::Max(0.f, CourtR - 760.f);
		C.Side = FVector(-C.Dir.Y, C.Dir.X, 0.f);
		C.VPos = FVector(Vp.X, Vp.Y, C.FloorZ); C.APos = C.VPos - C.Dir * 300.f; C.RockAt = C.VPos + C.Dir * 640.f; C.RockAt.Z = C.FloorZ;
		C.Hit = C.RockAt - C.Dir * 150.f + FVector(0, 0, 130.f);
		// freeze the court
		for (AZoogiBall* B : G->Balls) if (B)
		{
			FFrozen F; F.B = B; F.bHid = B->IsHidden();
			if (B->Sphere) { F.bSim = B->Sphere->IsSimulatingPhysics(); if (F.bSim) { F.Lin = B->Sphere->GetPhysicsLinearVelocity(); F.Ang = B->Sphere->GetPhysicsAngularVelocityInRadians(); B->Sphere->SetSimulatePhysics(false); } }
			C.Frozen.Add(F);
			if (B == Attacker || B == Victim) B->SetActorHiddenInGame(true);
		}
		for (AZoogiOrb* O : G->Orbs) if (O && !O->IsHidden()) { O->SetActorHiddenInGame(true); C.Hidden.Add(O); } // a clean court for the shot
		{	// the film set: props near the path or between it and the cameras are hidden for the shot (shown again after)
			const FVector S0 = C.APos - C.Dir * 400.f, S1 = C.RockAt + C.Dir * 300.f;
			const FVector CMid = (C.APos + C.VPos) * 0.5f, CSide = (C.VPos + C.RockAt) * 0.5f, CHero = C.APos + C.Dir * 120.f;
			const FVector Lines[3][2] = { { CMid + C.Side * 520.f - C.Dir * 260.f, CMid }, { CSide + C.Side * 980.f - C.Dir * 120.f, CSide }, { CHero - C.Dir * 400.f + C.Side * 230.f, CHero } };
			for (TActorIterator<AActor> It(W); It; ++It)
			{
				AActor* Ac = *It;
				if (!Ac || Ac == G->Arena || Ac->IsHidden() || Ac->IsA<AZoogiBall>() || Ac->IsA<AZoogiOrb>() || Ac->IsA<ACameraActor>() || Ac->IsA<APlayerController>() || Ac->FindComponentByClass<ULightComponent>()) continue;
				if (AZoogiArenaPiece* Pc = Cast<AZoogiArenaPiece>(Ac)) if (Pc->GetKind() == TEXT("stand")) continue;
				if (Ac->GetClass()->GetName().Contains(TEXT("Crowd"))) continue;
				FVector Org, Ext; Ac->GetActorBounds(false, Org, Ext);
				const float Er = FMath::Max(Ext.X, Ext.Y);
				if (Ext.IsNearlyZero() || Er > 900.f) continue;
				if (Org.Z + Ext.Z < C.FloorZ + 30.f || Org.Z - Ext.Z > C.FloorZ + 900.f) continue;
				const FVector O2(Org.X, Org.Y, 0.f);
				bool bNear = FVector::Dist2D(FMath::ClosestPointOnSegment(O2, FVector(S0.X, S0.Y, 0.f), FVector(S1.X, S1.Y, 0.f)), O2) < 700.f + 0.5f * Er;
				for (int32 Li = 0; Li < 3 && !bNear; ++Li) bNear = FVector::Dist2D(FMath::ClosestPointOnSegment(O2, FVector(Lines[Li][0].X, Lines[Li][0].Y, 0.f), FVector(Lines[Li][1].X, Lines[Li][1].Y, 0.f)), O2) < 160.f + Er;
				if (!bNear) continue;
				Ac->SetActorHiddenInGame(true); C.Hidden.Add(Ac);
			}
		}
		static const bool bTripoCine = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCineVoyager")); // b236 (Evan): two-legged soldiers only in the shooter modes // b233: the b230 Tripo statues
		if (!bTripoCine && ZoogiVoyager::On())
		{	// b233 (Evan: Voyager "A"): the Voyager character for each Zoogi, animated
			FString Va, Vb;
			C.VA = VoyFighter(W, Attacker->Def.Name, Attacker->PlayerIndex, C.HA, C.MinA, Va);
			C.VB = VoyFighter(W, Victim->Def.Name, Victim->PlayerIndex + 1, C.HB, C.MinB, Vb);
			if (C.VB.IsValid() && Va == Vb) { C.VB->Destroy(); C.VB = VoyFighter(W, TEXT("-"), Victim->PlayerIndex + 3, C.HB, C.MinB, Vb); }
			C.bVoy = C.VA.IsValid() && C.VB.IsValid();
			if (C.bVoy) { ZoogiVoyager::Dress(C.VA->GetSkeletalMeshComponent(), Attacker); ZoogiVoyager::Dress(C.VB->GetSkeletalMeshComponent(), Victim); } // b235
			if (!C.bVoy) { if (C.VA.IsValid()) C.VA->Destroy(); if (C.VB.IsValid()) C.VB->Destroy(); }
			else { C.AttName = Attacker->Def.Name; C.VicName = Victim->Def.Name; UE_LOG(LogZoogi, Log, TEXT("WALL CINE fighters: Voyager %s vs %s"), *Va, *Vb); }
		}
		if (!C.bVoy)
		{
		C.MA = Model(W, Attacker->Def.Name, Attacker->PlayerIndex, C.HA, C.MinA, C.AttName);
		C.MB = Model(W, Victim->Def.Name, Victim->PlayerIndex + 1, C.HB, C.MinB, C.VicName);
		if (C.VicName == C.AttName) { if (AStaticMeshActor* Old = C.MB.Get()) Old->Destroy(); C.MB = Model(W, TEXT("-"), Victim->PlayerIndex + 2, C.HB, C.MinB, C.VicName); }
		}
		// the rock the victim is smashed into
		if (UStaticMesh* Rm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/EarthbenderV1/Models/SM_Free_Rock_Model_3.SM_Free_Rock_Model_3"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Ra = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(C.RockAt), Sp))
			{
				const FBox Bb = Rm->GetBoundingBox(); const float K = 340.f / FMath::Max(1.f, (float)Bb.GetSize().Z);
				Ra->SetMobility(EComponentMobility::Movable); Ra->GetStaticMeshComponent()->SetStaticMesh(Rm); Ra->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Ra->GetStaticMeshComponent()->SetWorldScale3D(FVector(K * 1.25f, K * 1.25f, K));
				Ra->SetActorLocation(C.RockAt - FVector(Bb.GetCenter().X * K * 1.25f, Bb.GetCenter().Y * K * 1.25f, Bb.Min.Z * K + 8.f));
				if (UMaterialInstanceDynamic* Mt = ZoogiUtil::SurfaceMID(Ra, ZoogiUtil::Hex(TEXT("#8A8F7A")), 0.85f, ZoogiUtil::Hex(TEXT("#8A8F7A")), 0.4f))
					for (int32 m = 0; m < FMath::Max(1, Ra->GetStaticMeshComponent()->GetNumMaterials()); ++m) Ra->GetStaticMeshComponent()->SetMaterial(m, Mt);
				C.Rock = Ra;
			}
		}
		FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACameraActor* Ca = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(C.APos + FVector(0, 0, 300.f)), Sp))
		{
			FVector2D Vs(16.f, 9.f); if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Vs);
			Ca->GetCameraComponent()->SetFieldOfView(Vs.Y > Vs.X ? 62.f : 70.f); Ca->GetCameraComponent()->bConstrainAspectRatio = false; C.Cam = Ca;
		}
		if (APlayerController* Pc = W->GetFirstPlayerController()) { C.OldView = Pc->GetViewTarget(); if (C.Cam.IsValid()) Pc->SetViewTarget(C.Cam.Get()); }
		C.bOn = true; C.T0 = C.Last = FPlatformTime::Seconds(); C.Tl = 0.f;
		HideWidgets(W);
		ZoogiFilm::Moment(TEXT("WallCine"));
		UE_LOG(LogZoogi, Log, TEXT("WALL CINE kind %d: %s (P%d) blasts %s (P%d) into the rock at (%.0f, %.0f)"), C.Kind, *C.AttName, Attacker->PlayerIndex + 1, *C.VicName, Victim->PlayerIndex + 1, C.RockAt.X, C.RockAt.Y);
		return true;
	}

	// ---- b238: the real triggers (moments 1-5)
	namespace
	{
		struct FOffer { int32 Kind = 0; TWeakObjectPtr<AZoogiBall> A, V; };
		struct FMatch { TWeakObjectPtr<AZoogiGameMode> G; int32 LastRound = 0, Total = 0, KOs = 0, LastPlayTurn = -100, AwPlayer = -1, AwTurn = -1; int32 Count[7] = { 0, 0, 0, 0, 0, 0, 0 }; FOffer Pend; };
		FMatch M;
		void Sync(AZoogiGameMode* G)
		{
			if (M.G.Get() != G || G->RoundNumber < M.LastRound) { const FMatch Fresh; M = Fresh; M.G = G; }
			M.LastRound = G->RoundNumber;
		}
		int32 Prio(int32 K) { return K == 1 ? 5 : K == 2 ? 4 : K == 4 ? 3 : K == 3 ? 2 : 1; }
	}
	// ---- b245: the side-window flying-ball anime
	namespace
	{
		double GPipAt = -100.0; float GPipLen = 2.6f; int32 GPipKind = 0; FString GPipWho;
		const TCHAR* CrashClip(const AZoogiGameMode* G)
		{
			switch (G && G->Arena ? G->Arena->GetTheme() : EZoogiArena::Meadow)
			{
			case EZoogiArena::Frozen: return TEXT("Anime_Crash_FrozenRing_01"); case EZoogiArena::Volcanic: return TEXT("Anime_Crash_VolcanicPit_01");
			case EZoogiArena::Cosmic: return TEXT("Anime_Crash_CosmicPlatform_01"); case EZoogiArena::Arabian: return TEXT("Anime_Crash_ArabianNights_01");
			case EZoogiArena::Tomb: return TEXT("Anime_Crash_PharaohsTomb_01"); case EZoogiArena::Neon: return TEXT("Anime_Crash_NightCircuit_01");
			case EZoogiArena::Training: return TEXT("Anime_Crash_Training_01"); case EZoogiArena::Ancient: return TEXT("Anime_Crash_AncientMeadows_01");
			default: return TEXT("Anime_Crash_Meadow_01");
			}
		}
	}
	bool PlayPip(AZoogiGameMode* G, AZoogiBall* A, AZoogiBall* V, int32 Kind)
	{
		if (!G || !G->GetWorld() || ZoogiAnimeCues::Busy()) return false;
		ZoogiAnimeCues::FCue Cu;
		static int32 KoN = 0; static const bool bCrash = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallPipCrash"));
		Cu.Id = FString::Printf(TEXT("wallsmash_%d"), Kind); Cu.Clip = bCrash ? FString(CrashClip(G)) : FString::Printf(TEXT("Anime_Any_Any_KO_%02d"), 1 + (KoN++ % 3)); // the flying-ball KO anime (-ZoogiWallPipCrash = the map crash clips)
		// the side window where the celebration close-up shows (left, lower half; portrait: centre, lower half)
		Cu.Land.bFull = false; Cu.Land.X = 0.175f; Cu.Land.Y = 0.66f; Cu.Land.W = 0.31f; Cu.Land.H = 0.31f;
		Cu.Port.bFull = false; Cu.Port.X = 0.5f; Cu.Port.Y = 0.68f; Cu.Port.W = 0.6f; Cu.Port.H = 0.16f;
		Cu.Border = TEXT("p5"); Cu.Intro = TEXT("slide_left"); Cu.Outro = TEXT("fade"); Cu.IntroSec = 0.25f; Cu.OutroSec = 0.25f;
		Cu.Game = TEXT("play"); Cu.Duck = 0.4f; Cu.SkipAfter = 0.3f; Cu.Volume = 0.9f;
		if (!ZoogiAnimeCues::Play(G->GetWorld(), Cu)) return false;
		GPipAt = FPlatformTime::Seconds(); GPipKind = FMath::Clamp(Kind, 1, 6);
		GPipWho = FString::Printf(TEXT("%s  >  %s"), A ? *A->Def.Name.ToUpper() : TEXT("?"), V ? *V->Def.Name.ToUpper() : TEXT("?"));
		UE_LOG(LogZoogi, Log, TEXT("WALL SMASH pop-out kind %d: %s (%s)"), Kind, *Cu.Clip, *GPipWho);
		return true;
	}
	void DrawPip(UCanvas* Cv, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text)
	{
		const float t = (float)(FPlatformTime::Seconds() - GPipAt);
		if (!Cv || t < 0.f || t > GPipLen || !ZoogiAnimeCues::Playing()) return;
		static const TCHAR* Title[] = { TEXT("MATCH POINT KO!"), TEXT("AWAKEN KNOCKOUT!"), TEXT("WALL SLAM!"), TEXT("FIRST KNOCKOUT!"), TEXT("MEGA HIT!"), TEXT("REPLAY") };
		static const FLinearColor Col[] = { FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(0.75f, 0.45f, 1.f), FLinearColor(1.f, 0.5f, 0.15f), FLinearColor(1.f, 0.35f, 0.4f), FLinearColor(0.35f, 0.9f, 1.f), FLinearColor::White };
		const float Wd = Cv->ClipX, Ht = Cv->ClipY, U = FMath::Min(Wd, Ht) / 1080.f; const bool bPort = Ht > Wd;
		const float Cx = (bPort ? 0.5f : 0.175f) * Wd, Top = (bPort ? 0.68f - 0.08f : 0.66f - 0.155f) * Ht;
		const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - t / 0.2f, 0.f, 1.f);
		Text(Title[GPipKind - 1], Cx, Top - 34.f * U, 52.f * U * Pop, Col[GPipKind - 1]);
		Text(GPipWho, Cx, Top - 2.f * U, 24.f * U, FLinearColor::White);
	}

	void NoteAwaken(AZoogiGameMode* G, int32 Player) { if (!G) return; Sync(G); M.AwPlayer = Player; M.AwTurn = G->TurnCounter; }
	bool Offer(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWallCine"));
		static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallCineAlways"));
		static int32 MaxAll = -1; if (MaxAll < 0) { MaxAll = 5; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWallCineMax="), MaxAll); }
		if (bOff || !G || !Attacker || !Victim || Attacker == Victim || Kind < 1 || Kind > 5 || G->bSoloTraining) return false;
		Sync(G);
		static const int32 MaxKind[] = { 0, 1, 2, 2, 1, 2 };
		if (!bAlways && (M.Count[Kind] >= MaxKind[Kind] || M.Total >= MaxAll || (Kind != 1 && G->TurnCounter - M.LastPlayTurn < 2))) return false;
		if (M.Pend.Kind && Prio(M.Pend.Kind) >= Prio(Kind)) return false;
		M.Pend.Kind = Kind; M.Pend.A = Attacker; M.Pend.V = Victim;
		UE_LOG(LogZoogi, Log, TEXT("WALL CINE offer kind %d: P%d -> P%d (round %d, turn %d)"), Kind, Attacker->PlayerIndex + 1, Victim->PlayerIndex + 1, G->RoundNumber, G->TurnCounter);
		return true;
	}
	void OfferKO(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim)
	{
		if (!G || !Attacker || !Victim) return;
		Sync(G); ++M.KOs;
		const int32 Kind = G->RoundNumber >= G->MenuRounds ? 1 : (M.AwPlayer == Attacker->PlayerIndex && M.AwTurn == G->TurnCounter) ? 2 : M.KOs == 1 ? 4 : 0;
		if (Kind) Offer(G, Attacker, Victim, Kind);
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;
		{	// b245 preview (-ZoogiWallPipTest): one pop-out at the first aim, filmed
			static const bool bPt = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallPipTest")); static int32 PtN = 0;
			if (bPt && PtN < 2 && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 1.2f && FPlatformTime::Seconds() - GPipAt > 6.0 && G->Balls.Num() >= 2 && G->Balls[0] && G->Balls[1])
				if (PlayPip(G, G->Balls[0], G->Balls[1], PtN == 0 ? 4 : 5)) { ++PtN; ZoogiFilm::Begin(FString::Printf(TEXT("wallpip_%d"), PtN), 3.4f); }
		}
		if (M.Pend.Kind && !C.bOn && M.G.Get() == G)
		{	// b238: a queued moment plays at the start of the next turn
			if (G->Phase == EZoogiPhase::GameOver) M.Pend = FOffer();
			else if (G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.35f)
			{
				const FOffer Pd = M.Pend; M.Pend = FOffer();
				AZoogiBall* Pa = Pd.A.Get(); AZoogiBall* Pv = Pd.V.Get();
				static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallCineFull"));
				if (!bFull && Pa && Pv && PlayPip(G, Pa, Pv, Pd.Kind)) { ++M.Count[Pd.Kind]; ++M.Total; M.LastPlayTurn = G->TurnCounter; }
				else if (bFull && Pa && Pv && !Pa->bOut && !Pv->bOut && Play(G, Pa, Pv, Pd.Kind))
				{
					++M.Count[Pd.Kind]; ++M.Total; M.LastPlayTurn = G->TurnCounter;
					ZoogiFilm::Begin(FString::Printf(TEXT("cine_live_%d"), Pd.Kind), Len + 1.4f);
				}
			}
		}
		UWorld* W = G->GetWorld();
		const double Tn = FPlatformTime::Seconds();
		{	// preview (-ZoogiWallCineTour): the six trigger options one after another, each filmed (cine_<n>)
			static const bool bTour = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallCineTour"));
			static int32 Next = 1; static double Gap = 0.0;
			if (bTour && !C.bOn && Next <= 6 && Tn >= Gap && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.4f)
			{
				AZoogiBall* Hb = nullptr; for (AZoogiBall* B : G->Balls) if (B && !B->bOut && !B->bCPU) { Hb = B; break; }
				if (!Hb) for (AZoogiBall* B : G->Balls) if (B && !B->bOut) { Hb = B; break; }
				AZoogiBall* Vb = nullptr; float Nd = 1e9f;
				for (AZoogiBall* B : G->Balls) if (B && B != Hb && !B->bOut) { const float Dd = FVector::Dist2D(B->BallLocation(), Hb ? Hb->BallLocation() : FVector::ZeroVector); if (Dd < Nd) { Nd = Dd; Vb = B; } }
				if (Hb && Vb && Play(G, Hb, Vb, Next)) { ZoogiFilm::Begin(FString::Printf(TEXT("cine_%d"), Next), Len + 1.4f); ++Next; }
			}
			if (bTour && C.bOn) Gap = Tn + 1.5;
		}
		if (!C.bOn || C.G.Get() != G) return;
		const float Dt = FMath::Clamp((float)(Tn - C.Last), 0.f, 0.1f); C.Last = Tn;
		const bool bHitStop = C.Tl > tHit && C.Tl < tHit + 0.28f; // the impact holds (slow-mo)
		C.Tl += Dt * (bHitStop ? 0.25f : 1.f);
		const float t = C.Tl;
		C.Shake = FMath::Max(0.f, C.Shake - Dt * 2.2f);
		if (APlayerController* Pc = W->GetFirstPlayerController()) if (C.Cam.IsValid() && Pc->GetViewTarget() != C.Cam.Get()) Pc->SetViewTarget(C.Cam.Get());
		HideWidgets(W);
		AStaticMeshActor* MA = C.MA.Get(); AStaticMeshActor* MB = C.MB.Get();
		const FVector Mid = (C.APos + C.VPos) * 0.5f;
		static const FLinearColor KindCol[] = { FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(0.7f, 0.35f, 1.f), FLinearColor(1.f, 0.45f, 0.1f), FLinearColor(1.f, 0.25f, 0.3f), FLinearColor(0.3f, 0.9f, 1.f), FLinearColor(0.95f, 0.95f, 1.f) };
		const FLinearColor Kc = KindCol[C.Kind - 1];

		Once(0, [&]()
		{	// both pop into their 3D models
			Burst(W, TEXT("Mid_Explosion"), C.APos + FVector(0, 0, 90.f), 0.2f);
			AZoogiFX::Spawn(W, C.APos + FVector(0, 0, 100.f), Kc, 90.f, 0.25f, false, 6.f);
			AZoogiFX::Spawn(W, C.VPos + FVector(0, 0, 100.f), FLinearColor(1.f, 0.4f, 0.15f), 90.f, 0.25f, false, 6.f);
			ZoogiUtil::PlaySfx(G, TEXT("AurexIn"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("EpicRiser"), 0.5f, 1.2f);
			if (C.bVoy) { PlayV(C.VA.Get(), TEXT("Soldier/MotionCC0/Anim_Fighting_Idle"), true); PlayV(C.VB.Get(), TEXT("Soldier/MotionCC0/Anim_Fighting_Idle"), true, 0.9f); }
		});
		if (C.bVoy)
		{	// b233: the Voyager clips - jab-cross at the punch, a chest hit on the victim, the winner cheers
			if (t >= tPunch - 0.45f) Once(10, [&]() { PlayV(C.VA.Get(), TEXT("Soldier/MotionCC0/Anim_Punch_Cross"), false, 1.25f); });
			if (t >= tPunch) Once(11, [&]() { PlayV(C.VB.Get(), TEXT("CC0Animations/Anim_AL_Standard_Anim_Rig_Hit_Chest"), false, 0.7f); });
			if (t >= 2.9f) Once(12, [&]() { PlayV(C.VA.Get(), TEXT("ActionRPG/Anim_Sword_Cheer"), false, 1.35f); });
		}
		// ---- the attacker: wind up, lunge, PUNCH, then a hop and a spin
		FVector PA = C.APos; float TiltA = 0.f, SqA = 1.f, YawA = 0.f;
		if (t < tPunch)
		{
			const float w = Seg(t, 0.3f, 0.7f), l = Seg(t, 0.7f, tPunch);
			SqA = 1.f - 0.16f * Ease(w) * (1.f - l); PA -= C.Dir * 30.f * Ease(w) * (1.f - l);
			PA += C.Dir * 190.f * Ease(l); TiltA = 30.f * Ease(l);
		}
		else
		{
			const float k = Ease(Seg(t, tPunch, tPunch + 0.4f));
			PA += C.Dir * 190.f * (1.f - 0.4f * k); TiltA = 30.f * (1.f - k);
			const float c = Seg(t, 2.9f, 3.6f);
			if (c > 0.f && c < 1.f) { PA.Z += 120.f * FMath::Sin(c * PI); YawA = 360.f * Ease(c); }
		}
		if (C.bVoy) PlaceV(C.VA.Get(), C.APos + C.Dir * (t < tPunch - 0.45f ? 0.f : 150.f * Ease(Seg(t, tPunch - 0.45f, tPunch))), Face(C.Dir), C.HA, C.MinA); // b233: the punch clip carries the body
		else Place(MA, PA, Face(C.Dir, -TiltA) + FRotator(0.f, YawA, 0.f), C.HA, C.MinA, SqA);
		// ---- the victim: braces, is punched back into the rock (spinning), the blast bursts out of the rock face, bounces off, KO
		FVector PV = C.VPos; FRotator RV = Face(-C.Dir, 6.f * FMath::Sin(t * 6.f));
		if (t >= tPunch)
		{
			Once(1, [&]()
			{
				const FVector Hp = C.VPos - C.Dir * 60.f + FVector(0, 0, 140.f);
				Burst(W, TEXT("Mid_Explosion_v2"), Hp, 0.32f);
				AZoogiFX::Spawn(W, C.VPos + FVector(0, 0, 6.f), Kc, 360.f, 0.5f, true, 9.f);
				C.Shake = 1.f; ZoogiFilm::Mark(TEXT("punch"));
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 1.f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.8f, 1.f);
			});
			if (t < tHit)
			{
				const float k = Seg(t, tPunch, tHit);
				PV = FMath::Lerp(C.VPos, C.Hit - FVector(0, 0, 100.f), k) + FVector(0, 0, 90.f * FMath::Sin(k * PI) + 60.f * k);
				RV = Face(-C.Dir, -40.f - 400.f * k);
				if (Tn >= C.NextTrail) { C.NextTrail = Tn + 0.03; AZoogiFX::Spawn(W, PV + FVector(0, 0, 110.f), FLinearColor(1.f, 0.5f, 0.15f), 80.f, 0.4f, false, 7.f); }
			}
			else
			{
				Once(2, [&]()
				{	// the wall explosion: the game's rock blast (FIRE BLAST, option 4 - -ZoogiRockBlast=<n> follows) out of the rock face
					static int32 Style = -1; if (Style < 0) { Style = 4; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRockBlast="), Style); }
					ZoogiEarthEvents::BlastStyleAt(G, C.Hit - C.Dir * 20.f, -C.Dir, 1400.f, Style);
					AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), C.Hit - C.Dir * 30.f, 0.38f, FRotationMatrix::MakeFromZ((-C.Dir + FVector(0, 0, 0.22f)).GetSafeNormal()).Rotator());
					AZoogiFX::Spawn(W, C.Hit, FLinearColor(0.75f, 0.7f, 0.6f), 260.f, 0.6f, false, 6.f);
					C.Shake = 1.4f; ZoogiFilm::Mark(TEXT("wall_smash"));
					ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 1.f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 0.7f, 1.f);
				});
				if (t > tHit + 0.2f) Once(3, [&]() { AZoogiFX::Stylized(W, TEXT("Fire_v2"), C.Hit - C.Dir * 60.f + C.Side * 60.f + FVector(0, 0, 50.f), 0.3f, FRotator::ZeroRotator); });
				if (t > tHit + 0.38f) Once(4, [&]() { AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v3"), C.Hit - C.Dir * 50.f - C.Side * 70.f - FVector(0, 0, 40.f), 0.28f, FRotator::ZeroRotator); ZoogiUtil::PlaySfx(G, TEXT("Debris"), 0.9f, 1.f); });
				const float k = Ease(Seg(t, tHit, tHit + 0.85f));
				const FVector Land = C.RockAt - C.Dir * 400.f;
				PV = FMath::Lerp(C.Hit - FVector(0, 0, 100.f), FVector(Land.X, Land.Y, C.FloorZ), k) + FVector(0, 0, 130.f * FMath::Sin(k * PI) * (1.f - k));
				PV.Z = FMath::Max(PV.Z, C.FloorZ);
				RV = k >= 1.f ? Face(-C.Dir, -80.f) : Face(-C.Dir, FMath::Lerp(-440.f, -800.f, k));
				if (k >= 1.f) Once(5, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Land"), 1.f, 0.8f); ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.6f, 1.f); AZoogiFX::Spawn(W, FVector(Land.X, Land.Y, C.FloorZ + 4.f), FLinearColor(0.75f, 0.7f, 0.6f), 200.f, 0.5f, true, 6.f); C.Shake = 0.5f; });
				if (AStaticMeshActor* Ra = C.Rock.Get()) if (t < tHit + 0.6f) Ra->AddActorWorldOffset(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 4.f * (1.f - Seg(t, tHit, tHit + 0.6f)));
			}
		}
		if (C.bVoy)
		{	// b233: flies back leaning, then ragdolls off the rock face (bounces back toward the court and crumples)
			if (t < tHit) PlaceV(C.VB.Get(), t < tPunch ? C.VPos : PV, t < tPunch ? Face(-C.Dir) : FRotator(0.f, Face(-C.Dir).Yaw, -70.f * Seg(t, tPunch, tHit)), C.HB, C.MinB);
			else if (!C.bRag) { C.bRag = true; static float RagV = -1.f; if (RagV < 0.f) { RagV = 650.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiCineRagdoll="), RagV); } Ragdoll(C.VB.Get(), -C.Dir * RagV + FVector(0.f, 0.f, 260.f)); }
		}
		else Place(MB, PV, RV, C.HB, C.MinB, 1.f);
		// ---- camera: low 3/4 on the face-off, whips wide to the side for the flight + the wall blast, then pushes in on the winner
		const FVector SideMid = (C.VPos + C.RockAt) * 0.5f;
		const FVector Hero = C.APos + C.Dir * 120.f + FVector(0, 0, 140.f);
		if (t < tPunch) Look(FMath::Lerp(Mid + C.Side * 520.f - C.Dir * 260.f + FVector(0, 0, 120.f), Mid + C.Side * 470.f - C.Dir * 120.f + FVector(0, 0, 150.f), Ease(t / tPunch)), Mid + FVector(0, 0, 120.f));
		else if (t < 2.7f) Look(FMath::Lerp(Mid + C.Side * 470.f - C.Dir * 120.f + FVector(0, 0, 150.f), SideMid + C.Side * 980.f - C.Dir * 120.f + FVector(0, 0, 220.f), Ease(Seg(t, tPunch, tPunch + 0.35f))), SideMid + FVector(0, 0, 110.f));
		else
		{
			const float k = Ease(Seg(t, 2.7f, 4.6f));
			Look(FMath::Lerp(SideMid + C.Side * 980.f - C.Dir * 120.f + FVector(0, 0, 220.f), Hero - C.Dir * 400.f + C.Side * 230.f + FVector(0, 0, 30.f), k), FMath::Lerp(SideMid + FVector(0, 0, 110.f), Hero, k));
		}
		if (t > Len) Finish();
	}

	void DrawHud(UCanvas* Cv, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text)
	{
		if (!C.bOn || !Cv) return;
		const float Wd = Cv->ClipX, Ht = Cv->ClipY, U = FMath::Min(Wd, Ht) / 1080.f, t = C.Tl;
		// letterbox bars slide in
		const float Bar = Ht * 0.1f * Ease(Seg(t, 0.f, 0.25f)) * (1.f - Ease(Seg(t, Len - 0.25f, Len)));
		FCanvasTileItem Tb(FVector2D(0.f, 0.f), FVector2D(Wd, Bar), FLinearColor::Black); Cv->DrawItem(Tb);
		FCanvasTileItem Bb(FVector2D(0.f, Ht - Bar), FVector2D(Wd, Bar), FLinearColor::Black); Cv->DrawItem(Bb);
		static const TCHAR* Title[] = { TEXT("MATCH POINT KO!"), TEXT("AWAKEN KNOCKOUT!"), TEXT("WALL SLAM!"), TEXT("FIRST KNOCKOUT!"), TEXT("MEGA HIT!"), TEXT("REPLAY") };
		static const FLinearColor KindCol[] = { FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(0.75f, 0.45f, 1.f), FLinearColor(1.f, 0.5f, 0.15f), FLinearColor(1.f, 0.3f, 0.35f), FLinearColor(0.35f, 0.9f, 1.f), FLinearColor(1.f, 1.f, 1.f) };
		const FLinearColor Kc = KindCol[C.Kind - 1];
		if (C.Kind == 6)
		{	// the replay look: REC dot + REPLAY top-left, the whole thing as a highlight
			if (FMath::Fmod(t, 1.f) < 0.6f) { FCanvasTileItem Dot(FVector2D(40.f * U, Bar * 0.3f), FVector2D(22.f * U, 22.f * U), FLinearColor(1.f, 0.15f, 0.15f)); Cv->DrawItem(Dot); }
			Text(TEXT("REPLAY"), 150.f * U, Bar * 0.5f, 40.f * U, FLinearColor::White);
			Text(FString::Printf(TEXT("%s  >  %s"), *C.AttName.ToUpper(), *C.VicName.ToUpper()), Wd * 0.5f, Ht - Bar * 0.5f, 34.f * U, FLinearColor(0.85f, 0.85f, 0.9f));
			return;
		}
		// the banner slams in on the wall hit
		const float a = Seg(t, tHit, tHit + 0.18f);
		if (a <= 0.f) { Text(FString::Printf(TEXT("%s  VS  %s"), *C.AttName.ToUpper(), *C.VicName.ToUpper()), Wd * 0.5f, Ht - Bar * 0.5f, 34.f * U, FLinearColor(0.9f, 0.9f, 0.95f)); return; }
		const float Sc = 1.f + 0.6f * (1.f - Ease(a));
		const float Yb = Ht * 0.24f;
		FCanvasTileItem Band(FVector2D(0.f, Yb - 62.f * U), FVector2D(Wd, 124.f * U), FLinearColor(0.f, 0.f, 0.f, 0.55f * a)); Band.BlendMode = SE_BLEND_Translucent; Cv->DrawItem(Band);
		FCanvasTileItem Edge(FVector2D(0.f, Yb + 58.f * U), FVector2D(Wd, 6.f * U), Kc); Cv->DrawItem(Edge);
		Text(Title[C.Kind - 1], Wd * 0.5f, Yb, 96.f * U * Sc, Kc);
		Text(FString::Printf(TEXT("%s  BLASTS  %s  INTO THE WALL"), *C.AttName.ToUpper(), *C.VicName.ToUpper()), Wd * 0.5f, Ht - Bar * 0.5f, 34.f * U, FLinearColor::White);
	}
}
