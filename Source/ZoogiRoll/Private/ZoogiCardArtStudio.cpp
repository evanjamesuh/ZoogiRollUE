// b138: the card art studio (see ZoogiCardArtStudio.h). Test-only.
#include "ZoogiCardArtStudio.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "Engine/Light.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiCardArtStudio
{
	namespace
	{
		struct FFx { const TCHAR* Path; FVector Off; FRotator Rot; float Scale; float Delay; };
		struct FShot { const TCHAR* Id; TArray<FFx> Fx; float CamYaw; float OrbSide; };
		const TCHAR* H(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/StylizedToonVFX3/Effects/%s.%s"), N, N); return *S; }
		const TCHAR* Pk(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/VFX_PackVol1/Particles/%s.%s"), N, N); return *S; }
		const TCHAR* Fg(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/FX/%s.%s"), N, N); return *S; }
		const TCHAR* Ck(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/VFX/Cracks/%s.%s"), N, N); return *S; }
		const FRotator R0 = FRotator::ZeroRotator;
		// the thought process per card: the effect that IS the card, readable at icon size, around the Zoogi ball
		TArray<FShot> Shots()
		{
			TArray<FShot> S;
			S.Add({ TEXT("storm"), { { Pk(TEXT("P_ElectricShock_Converted")), FVector(0, 0, 20), R0, 1.6f, 0.f }, { TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Extras/Particles/NS_Extra_Lightning_Example_VFX.NS_Extra_Lightning_Example_VFX"), FVector(0, 0, 40), R0, 0.6f, 0.f }, { Fg(TEXT("NS_Burst_Blue")), FVector(0, 0, 40), R0, 1.6f, 0.2f } }, 200.f, 0.f });  // sharper steering: a lightning-wrapped ball
			S.Add({ TEXT("bank_strike"), { { Pk(TEXT("P_ElectricShock_v2_Converted")), FVector(110, 0, 40), R0, 1.3f, 0.f }, { Fg(TEXT("NS_Burst_Yellow")), FVector(110, 0, 40), R0, 1.8f, 0.f }, { Fg(TEXT("NS_Sparks")), FVector(110, 0, 40), R0, 1.5f, 0.1f } }, 200.f, 0.f });  // a spark burst where the ball banks off
			S.Add({ TEXT("guard"), { { H(TEXT("NS_Energy_buff")), FVector(0, 0, -45), R0, 0.25f, 0.f }, { TEXT("/Game/VFX/KriptoFX/Effect11/Rings/NS_KFX_Effect11_Ring_Distortion.NS_KFX_Effect11_Ring_Distortion"), FVector(70, 0, 40), FRotator(0, 0, 90), 0.6f, 0.f }, { Fg(TEXT("NS_Burst_Blue")), FVector(90, 0, 40), R0, 1.4f, 0.1f } }, 200.f, 0.f });  // a hit absorbed by a shield
			S.Add({ TEXT("orb_relay"), { { Fg(TEXT("NS_Burst_Gold")), FVector(220, 160, 30), R0, 1.8f, 0.f }, { Fg(TEXT("NS_Sparks")), FVector(110, 80, 30), R0, 1.6f, 0.f }, { Fg(TEXT("NS_Burst_Gold")), FVector(0, 0, 40), R0, 1.4f, 0.15f } }, 215.f, 1.f });  // an orb passed in gold
			S.Add({ TEXT("heavy_roll"), { { Pk(TEXT("P_Big_Explosion_v2_Converted")), FVector(120, 0, 20), R0, 0.12f, 0.f }, { Fg(TEXT("NS_Debris")), FVector(120, 0, 20), R0, 1.2f, 0.f }, { Fg(TEXT("NS_Dust")), FVector(0, 0, -40), R0, 1.5f, 0.f } }, 205.f, 0.f });  // a heavy smash
			S.Add({ TEXT("ember"), { { Pk(TEXT("P_Fire_Converted")), FVector(0, 0, -30), R0, 0.4f, 0.f }, { TEXT("/Game/VFX/AwakenFX/NS_ZAFX_Embers.NS_ZAFX_Embers"), FVector(0, 0, 30), R0, 1.6f, 0.f }, { Ck(TEXT("NS_ZoogiCrackSparks_Fire")), FVector(80, 0, 20), R0, 2.f, 0.f }, { Fg(TEXT("NS_Burst_Red")), FVector(0, 0, 40), R0, 1.4f, 0.1f } }, 200.f, 0.f });  // a fire-charged ball
			S.Add({ TEXT("titan"), { { TEXT("/Game/VFX/TitanFX/NS_TitanDust.NS_TitanDust"), FVector(0, 0, -40), R0, 1.f, 0.f }, { TEXT("/Game/VFX/TitanFX/NS_TitanEmbers.NS_TitanEmbers"), FVector(0, 0, 10), R0, 1.f, 0.f }, { Ck(TEXT("NS_ZoogiCrackSparks_Titan")), FVector(0, 0, -30), R0, 1.4f, 0.f }, { H(TEXT("NS_Sword_crater")), FVector(0, 0, -48), R0, 0.25f, 0.f } }, 200.f, 0.f });  // a heavy armoured ball cracking the floor
			S.Add({ TEXT("frost"), { { H(TEXT("NS_Ice_wall_2")), FVector(-70, 40, -48), FRotator(0, 0, 0), 0.35f, 0.f }, { TEXT("/Game/VFX/KriptoFX/Effect22/NS_KFX_Effect22.NS_KFX_Effect22"), FVector(0, 0, -40), R0, 0.5f, 0.f }, { Ck(TEXT("NS_ZoogiCrackSparks_Frost")), FVector(0, 0, 0), R0, 2.2f, 0.f } }, 200.f, 0.f });  // ice forming under a sliding ball
			S.Add({ TEXT("momentum_steal"), { { TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Extras/Particles/NS_Extra_Lightning_Example_VFX.NS_Extra_Lightning_Example_VFX"), FVector(110, 60, 40), R0, 0.9f, 0.f }, { Pk(TEXT("P_ElectricShock_v3_Converted")), FVector(150, 90, 40), R0, 1.3f, 0.f }, { Pk(TEXT("P_ElectricShock_Converted")), FVector(0, 0, 30), R0, 1.f, 0.1f } }, 210.f, 1.f });  // speed siphoned off a glancing hit
			S.Add({ TEXT("crystal_collapse"), { { Ck(TEXT("NS_ZoogiCrackSparks_Crystal")), FVector(70, 0, 30), R0, 3.f, 0.f }, { H(TEXT("NS_Ice_wall")), FVector(70, 0, -48), FRotator(0, 90, 0), 0.25f, 0.f }, { Fg(TEXT("NS_Burst_Purple")), FVector(70, 0, 40), R0, 2.f, 0.2f } }, 205.f, 0.f });  // a crystal piece shattering
			S.Add({ TEXT("orb_magnet"), { { H(TEXT("NS_Energy_shockwave")), FVector(120, 0, -45), R0, 0.35f, 0.f }, { Fg(TEXT("NS_Burst_Purple")), FVector(120, 0, 40), R0, 1.6f, 0.3f } }, 200.f, 1.f });  // an orb pulled in by a magnetic swirl
			S.Add({ TEXT("double_impact"), { { Pk(TEXT("P_Mid_Explosion_Converted")), FVector(110, 0, 20), R0, 0.08f, 0.f }, { Pk(TEXT("P_Mid_Explosion_v2_Converted")), FVector(150, 40, 20), R0, 0.07f, 0.35f }, { Fg(TEXT("NS_Burst_White")), FVector(110, 0, 40), R0, 1.6f, 0.f }, { Fg(TEXT("NS_Burst_Yellow")), FVector(150, 40, 40), R0, 1.6f, 0.35f } }, 205.f, 0.f });  // two bursts on one hit
			S.Add({ TEXT("anchor_guard"), { { H(TEXT("NS_Sword_crater")), FVector(0, 0, -48), R0, 0.3f, 0.f }, { H(TEXT("NS_Energy_buff")), FVector(0, 0, -45), R0, 0.22f, 0.f }, { Fg(TEXT("NS_Burst_Green")), FVector(0, 0, 0), R0, 1.f, 0.2f } }, 200.f, 0.f });  // a ball anchored into the floor
			S.Add({ TEXT("recovery_roll"), { { Pk(TEXT("P_WheelSmoke_Converted")), FVector(-90, 0, -40), R0, 2.5f, 0.f }, { Fg(TEXT("NS_Dust")), FVector(-60, 0, -40), R0, 2.5f, 0.f }, { Fg(TEXT("NS_Burst_Green")), FVector(60, 0, -20), R0, 1.6f, 0.1f } }, 200.f, 0.f });  // hard braking at a safe spot
			S.Add({ TEXT("precision"), { { H(TEXT("NS_Water_cannon")), FVector(0, 0, 0), FRotator(0, -25, 0), 0.35f, 0.f }, { H(TEXT("NS_Water_cannon")), FVector(0, 0, 0), FRotator(0, 0, 0), 0.35f, 0.08f }, { H(TEXT("NS_Water_cannon")), FVector(0, 0, 0), FRotator(0, 25, 0), 0.35f, 0.16f } }, 205.f, 0.f });  // three water-cannon shots fanned out
			return S;
		}
		TWeakObjectPtr<AZoogiGameMode> GG; TWeakObjectPtr<ACameraActor> Cam; TArray<TWeakObjectPtr<UNiagaraComponent>> Live;
		int32 Idx = -1; double T0 = 0.0; FVector Spot = FVector::ZeroVector; TArray<int32> Fired; bool bShotA = false, bShotB = false, bCleaned = false; int32 NHid = 0;
		FTSTicker::FDelegateHandle Tick;

		void Clear() { for (TWeakObjectPtr<UNiagaraComponent>& N : Live) if (UNiagaraComponent* C = N.Get()) C->DestroyComponent(); Live.Reset(); }

		bool Step(float)
		{
			AZoogiGameMode* G = GG.Get();
			if (!G || !G->GetWorld()) { Tick.Reset(); return false; }
			UWorld* W = G->GetWorld();
			const double Now = FPlatformTime::Seconds();
			static const TArray<FShot> All = Shots();
			if (Idx < 0) { if (Now - T0 < 6.0) return true; Idx = 0; T0 = Now; Fired.Reset(); bShotA = bShotB = false; }
			if (Idx >= All.Num()) { Clear(); UE_LOG(LogZoogi, Log, TEXT("CARDART done (%d cards, %d props hidden)"), All.Num(), NHid); Tick.Reset(); return false; }
			const FShot& Sh = All[Idx];
			const float Ts = (float)(Now - T0);
			// pose the ball (held still every frame)
			AZoogiBall* B = nullptr; for (AZoogiBall* X : G->Balls) if (X && !X->bCPU) { B = X; break; }
			if (B && B->Ring) B->Ring->SetVisibility(false);
			if (B && B->Sphere)
			{
				B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				B->SetActorLocation(Spot + FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (Sh.OrbSide > 0.f) // an orb in the shot
				;
			else if (G->Orbs.Num() && G->Orbs[0]) G->Orbs[0]->SetActorHiddenInGame(true);
			if (Sh.OrbSide > 0.f)
				if (G->Orbs.Num() && G->Orbs[0] && G->Orbs[0]->Sphere) { AZoogiOrb* O = G->Orbs[0]; O->SetActorHiddenInGame(false); O->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); O->SetActorLocation(Spot + FVector(220, 160, ZoogiConst::OrbRadius + 2.f), false, nullptr, ETeleportType::TeleportPhysics); }
			// a clean stage: no HUD / menus, only the shot's orb, no props near the spot
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveAllViewportWidgets();
			if (APlayerController* Pc0 = W->GetFirstPlayerController()) if (Pc0->MyHUD) Pc0->MyHUD->bShowHUD = false;
			if (!bCleaned)
			{
				bCleaned = true;
				for (TActorIterator<AActor> It(W); It; ++It)
				{
					AActor* A = *It;
					if (!A || A == B || Cast<AZoogiBall>(A) || Cast<ACameraActor>(A) || A->IsA<APlayerController>()) continue;
					if (AZoogiOrb* O = Cast<AZoogiOrb>(A)) { if (O != G->Orbs[0]) O->SetActorHiddenInGame(true); continue; }
					FVector Org, Ext; A->GetActorBounds(true, Org, Ext);
					if (A->IsA<AZoogiArena>() || A->IsA<ALight>() || A->IsA<ASkyLight>() || A->IsA<AExponentialHeightFog>()) continue;
					if (FVector::Dist2D(Org, Spot) < 3000.f + Ext.Size2D() && Org.Z + Ext.Z > Spot.Z + 5.f && Ext.Z > 20.f && Ext.Size2D() < 6000.f) { A->SetActorHiddenInGame(true); ++NHid; }
				}
			}
			// camera
			if (!Cam.IsValid())
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Spot), SP);
				if (Cam.IsValid()) { Cam->GetCameraComponent()->bConstrainAspectRatio = false; Cam->GetCameraComponent()->SetFieldOfView(55.f); }
			}
			if (ACameraActor* C = Cam.Get())
			{
				const FVector Look = Spot + FVector(40, 0, 55);
				const FVector Eye = Look - FRotator(-18.f, 90.f + (Sh.CamYaw - 200.f), 0.f).Vector() * 560.f; // side-on: the ball left, its effect to the right
				C->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
				if (APlayerController* Pc = W->GetFirstPlayerController()) if (Pc->GetViewTarget() != C) Pc->SetViewTarget(C);
			}
			// effects
			for (int32 k = 0; k < Sh.Fx.Num(); ++k)
			{
				if (Fired.Contains(k) || Ts < 0.4f + Sh.Fx[k].Delay) continue;
				Fired.Add(k);
				const FFx& F = Sh.Fx[k];
				if (UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, F.Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					if (UNiagaraComponent* N = UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, Sys, Spot + FVector(0, 0, ZoogiConst::ZoogiRadius) + F.Off, F.Rot, FVector(F.Scale), false, true, ENCPoolMethod::None)) Live.Add(N);
				}
				else UE_LOG(LogZoogi, Warning, TEXT("CARDART %s: missing %s"), Sh.Id, F.Path);
			}
			if (!bShotA && Ts >= 1.05f) { bShotA = true; G->TakeShot(FString::Printf(TEXT("cardart_%s_a"), Sh.Id)); }
			if (!bShotB && Ts >= 1.7f) { bShotB = true; G->TakeShot(FString::Printf(TEXT("cardart_%s_b"), Sh.Id)); }
			if (Ts >= 2.6f) { Clear(); ++Idx; T0 = Now; Fired.Reset(); bShotA = bShotB = false; }
			return true;
		}
	}

	void OnMatchStart(AZoogiGameMode* G)
	{
		if (!G || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCardArtShoot"))) return;
		GG = G; Idx = -1; T0 = FPlatformTime::Seconds(); Clear(); bCleaned = false;
		Spot = G->Arena ? G->Arena->GetActorLocation() + FVector(700.f, -450.f, 0.f) : FVector(700.f, -450.f, 0.f);
		Spot.Z = G->Arena ? G->Arena->GetActorLocation().Z : 0.f;
		if (!Tick.IsValid()) Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step));
		UE_LOG(LogZoogi, Log, TEXT("CARDART studio armed at (%.0f, %.0f, %.0f)"), Spot.X, Spot.Y, Spot.Z);
	}
}
