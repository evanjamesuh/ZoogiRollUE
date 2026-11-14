// Oct 31 b46: stage changes (see ZoogiStageShift.h).
#include "ZoogiStageShift.h"
#include "EngineUtils.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiIceBarrier.h"
#include "ZoogiTypes.h"
#include "ZoogiRoyale.h"
#include "ZoogiStageForming.h"
#include "ZoogiP5Style.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiCosmicMarkers.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiCardArtStudio.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiStageShift
{
	namespace
	{
		bool bShifted = false, bHaveLight = false;
		TWeakObjectPtr<AZoogiArena> LitArena;
		FLinearColor SunCol0 = FLinearColor::White; float SunI0 = 1.f, SkyI0 = 1.f; FRotator SunRot0 = FRotator::ZeroRotator; FLinearColor FogCol0 = FLinearColor::White; float FogD0 = 0.02f;
		FLinearColor SunCol1 = FLinearColor::White; float SunI1 = 1.f, SkyI1 = 1.f; FRotator SunRot1 = FRotator::ZeroRotator; FLinearColor FogCol1 = FLinearColor::White; float FogD1 = 0.02f;
		float LightT = 1.f, LightDur = 4.f;
		FTSTicker::FDelegateHandle LightTick;
		TArray<int32> AddedProps;
		// Royale eruption
		bool bErupt = false; float NextBomb = 0.f;
		struct FBomb { TWeakObjectPtr<AActor> Rock; FVector From, To; float T = 0.f, Dur = 1.3f; TWeakObjectPtr<AZoogiFX> Warn; };
		TArray<FBomb> Bombs;

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoStageShift")); return b; }
		bool Test() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStageShiftTest")); return b; }

		void ApplyLight(float A)
		{
			AZoogiArena* Ar = LitArena.Get();
			if (!Ar || !bHaveLight) return;
			const float E = A * A * (3.f - 2.f * A);
			if (Ar->Sun) { Ar->Sun->SetLightColor(FMath::Lerp(SunCol0, SunCol1, E)); Ar->Sun->SetIntensity(FMath::Lerp(SunI0, SunI1, E)); Ar->Sun->SetWorldRotation(FMath::Lerp(SunRot0, SunRot1, E)); }
			if (Ar->SkyLight) { Ar->SkyLight->SetIntensity(FMath::Lerp(SkyI0, SkyI1, E)); }
			if (Ar->Fog) { Ar->Fog->SetFogInscatteringColor(FMath::Lerp(FogCol0, FogCol1, E)); Ar->Fog->SetFogDensity(FMath::Lerp(FogD0, FogD1, E)); }
		}
		bool TickLight(float Dt)
		{
			LightT = FMath::Min(1.f, LightT + Dt / FMath::Max(LightDur, 0.1f));
			ApplyLight(LightT);
			if (LightT >= 1.f) { LightTick.Reset(); return false; }
			return true;
		}
		void Capture(AZoogiArena* Ar)
		{
			LitArena = Ar; bHaveLight = Ar != nullptr;
			if (!Ar) return;
			if (Ar->Sun) { SunCol0 = Ar->Sun->GetLightColor(); SunI0 = Ar->Sun->Intensity; SunRot0 = Ar->Sun->GetComponentRotation(); }
			if (Ar->SkyLight) SkyI0 = Ar->SkyLight->Intensity;
			if (Ar->Fog) { FogCol0 = Ar->Fog->FogInscatteringLuminance; FogD0 = Ar->Fog->FogDensity; }
		}
		void BlendTo(const FLinearColor& SunC, float SunMul, float SkyMul, float PitchDeg, const FLinearColor& FogC, float FogMul, float Dur)
		{
			SunCol1 = SunC; SunI1 = SunI0 * SunMul; SkyI1 = SkyI0 * SkyMul; SunRot1 = FRotator(PitchDeg, SunRot0.Yaw + 25.f, SunRot0.Roll); FogCol1 = FogC; FogD1 = FogD0 * FogMul;
			LightT = 0.f; LightDur = Dur;
			if (!LightTick.IsValid()) LightTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickLight));
		}
		void Restore()
		{
			if (LightTick.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(LightTick); LightTick.Reset(); }
			if (bHaveLight && LitArena.IsValid()) ApplyLight(0.f);
			bHaveLight = false; LitArena.Reset();
		}
	}

	bool Shifted() { return bShifted || bErupt; }

	void OnMatchStart(AZoogiGameMode* G)
	{
		ZoogiCosmicMarkers::OnMatchStart(G); // b87
		ZoogiCosmosBoss::OnMatchStart(G); // b136 (Evan): the boss watching over the cosmos arena
		ZoogiCardArtStudio::OnMatchStart(G); // b138: test-only card art captures (-ZoogiCardArtShoot)
		Restore();
		if (G && G->Arena)
			for (int32 k = AddedProps.Num() - 1; k >= 0; --k)
				if (G->Arena->DevProps.IsValidIndex(AddedProps[k]) && G->Arena->DevProps[AddedProps[k]].Id.StartsWith(TEXT("stageshift_"))) G->Arena->RemoveDevProp(AddedProps[k]); // only ours (a rebuilt arena has new props)
		AddedProps.Reset();
		for (FBomb& B : Bombs) { if (AActor* R = B.Rock.Get()) R->Destroy(); }
		if (G) AZoogiIceBarrier::DestroyAll(G->GetWorld()); // b63
		Bombs.Reset();
		bShifted = false; bErupt = false; NextBomb = 0.f;
	}

	void OnRoundStart(AZoogiGameMode* G)
	{
		ZoogiCosmicMarkers::OnRoundStart(G); // b87 (Evan): Cosmic grows one rune marker per round
		ZoogiCosmosBoss::OnRoundStart(G); // b136
		// b81 (Evan: "the ice walls that form as the stage changes increase in size each round"): +25% height / +10% thickness per round (cap 2.5x)
		if (bShifted && G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallsFixed")))
		{
			int32 Grown = 0;
			for (TActorIterator<AZoogiIceBarrier> It(G->GetWorld()); It; ++It)
			{
				const FVector Sc = It->GetActorScale3D();
				It->SetActorScale3D(FVector(Sc.X, FMath::Min(Sc.Y * 1.1f, 1.6f), FMath::Min(Sc.Z * 1.25f, 2.5f)));
				AZoogiFX::Spawn(G->GetWorld(), It->GetActorLocation(), FLinearColor(0.7f, 0.9f, 1.f), 220.f, 0.5f, true, 8.f);
				++Grown;
			}
			if (Grown) UE_LOG(LogZoogi, Log, TEXT("STAGE SHIFT: %d ice walls grow for round %d"), Grown, G->RoundNumber);
		}
		if (Off() || bShifted || !G || !G->Arena || G->bRoyaleMode || G->bComboMode) return;
		const int32 Trigger = Test() ? 2 : FMath::Max(2, G->MenuRounds / 2 + 1);
		if (G->MenuRounds < 3 || G->RoundNumber != Trigger) return;
		bShifted = true;
		AZoogiArena* Ar = G->Arena;
		UE_LOG(LogZoogi, Log, TEXT("STAGE SHIFT: round %d of %d - %s takes its second form"), G->RoundNumber, G->MenuRounds, *Ar->GetName());
		G->PublicFeed(TEXT("STAGE SHIFT!  THE ARENA CHANGES FORM"), FLinearColor(1.f, 0.6f, 0.25f));
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		auto Shift = [WG]()
		{
			AZoogiGameMode* GM = WG.Get();
			if (!GM || !GM->Arena) return;
			AZoogiArena* A2 = GM->Arena;
			Capture(A2);
			BlendTo(FLinearColor(1.f, 0.58f, 0.32f), 0.8f, 0.72f, -11.f, FLinearColor(0.85f, 0.55f, 0.4f), 1.35f, 3.5f); // golden-hour dusk
			// four new bumpers rise out of the floor, clear of every orb and Zoogi
			const float KR = A2->GetKnockoffRadius();
			int32 Made = 0;
			TArray<float> UsedYaw;
			static const float Radii[] = { 0.42f, 0.33f, 0.52f, 0.26f };
			for (int32 k = 0; k < 4 * 24 && Made < 4; ++k)
			{
				const float Rad = KR * Radii[k / 24];
				const float Yaw = 45.f + 15.f * (k % 24);
				bool bSpread = true; for (const float U : UsedYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(U, Yaw)) < 60.f) bSpread = false;
				if (!bSpread) continue;
				const FVector P(FMath::Cos(FMath::DegreesToRadians(Yaw)) * Rad, FMath::Sin(FMath::DegreesToRadians(Yaw)) * Rad, 0.f);
				bool bClear = !A2->IsOffCourt(FVector2D(P));
				const bool bIceSpot = A2->GetTheme() == EZoogiArena::Frozen; // b63: a solid 3 m ice wall needs more room
				for (const AZoogiOrb* O : GM->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < (bIceSpot ? 260.f : 170.f)) bClear = false;
				for (const AZoogiBall* B : GM->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), P) < (bIceSpot ? 320.f : 230.f)) bClear = false;
				if (!bClear) continue;
				if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarProps")))
				{
					// b62 (Evan): no red star bumpers anywhere. b63 (Evan: "For frozen use the ice wall effect ... when arena changes
					// replacing the cylinders that arise, ensure zero interference"): on the Frozen Ring the four spots get the Hovl
					// ice wall eruption instead - effect only (no collider, no prop), it plays out and is gone.
					if (A2->GetTheme() != EZoogiArena::Frozen) break;
					// b63 (Evan, 11:34): not visual only - solid ice walls that crack when hit and burst into ice blocks (AZoogiIceBarrier)
					const AZoogiBall* Ref = GM->GetActiveBall();
					const FVector Floor(P.X, P.Y, (Ref ? Ref->BallLocation().Z - ZoogiConst::ZoogiRadius : A2->GetActorLocation().Z) + 1.f);
					const FVector Tan(-FMath::Sin(FMath::DegreesToRadians(Yaw)), FMath::Cos(FMath::DegreesToRadians(Yaw)), 0.f);
					AZoogiIceBarrier::Spawn(GM->GetWorld(), Floor - Tan * 150.f, Floor + Tan * 150.f, 160.f, -1, 99); // b81: they stay for the rest of the match (growing each round)
					if (Made == 0) { GM->TakeShotDelayed(TEXT("shift_ice_a"), 0.5f); GM->TakeShotDelayed(TEXT("shift_ice_b"), 1.3f); GM->TakeShotDelayed(TEXT("shift_ice_c"), 2.6f); }
					UsedYaw.Add(Yaw);
					++Made;
					continue;
				}
				const int32 Idx = A2->AddStarBumperProp(P, Yaw, 1.f, FString::Printf(TEXT("stageshift_%d"), Made));
				AddedProps.Add(Idx);
				if (A2->Forming && A2->DevProps.IsValidIndex(Idx))
				{
					if (Made == 0) A2->Forming->Clear();
					for (const TWeakObjectPtr<UPrimitiveComponent>& C : A2->DevProps[Idx].Comps)
						if (UPrimitiveComponent* Pc = C.Get()) A2->Forming->AddComponent(Pc, EZoogiFormStyle::Rise, FVector(0.f, 0.f, -260.f), 0.25f + 0.18f * Made, 0.8f, 12.f);
				}
				AZoogiFX::Spawn(GM->GetWorld(), P + FVector(0, 0, 4.f), FLinearColor(1.f, 0.65f, 0.3f), 260.f, 0.9f, true, 9.f);
				UsedYaw.Add(Yaw);
				++Made;
			}
			if (A2->Forming && Made > 0) A2->Forming->Begin(1.f);
			UE_LOG(LogZoogi, Log, TEXT("STAGE SHIFT: dusk light + %d %s"), Made, (A2->GetTheme() == EZoogiArena::Frozen && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarProps"))) ? TEXT("solid ice walls") : TEXT("bumpers rising"));
		};
		ZoogiP5::PlayWipe(G->GetWorld(), Shift, FLinearColor(1.f, 0.45f, 0.15f), TEXT("STAGE SHIFT!"));
	}

	void RoyaleTick(AZoogiGameMode* G, float Dt)
	{
		if (Off() || !G || !G->Arena || G->Arena->GetTheme() != EZoogiArena::Ancient || G->Royale.Step != EZoogiRoyaleStep::Fight) return;
		UWorld* W = G->GetWorld();
		const float EruptAt = Test() ? 12.f : 70.f;
		if (!bErupt && G->Royale.FightTime >= EruptAt)
		{
			bErupt = true; NextBomb = G->Royale.FightTime + 1.5f;
			Capture(G->Arena);
			BlendTo(FLinearColor(1.f, 0.56f, 0.36f), 0.9f, 0.75f, -10.f, FLinearColor(0.9f, 0.5f, 0.35f), 1.3f, 6.f); // warm volcanic dusk, still readable
			G->PublicFeed(TEXT("THE VOLCANO ERUPTS!  WATCH FOR LAVA BOMBS"), FLinearColor(1.f, 0.4f, 0.15f));
			if (G->Balls.IsValidIndex(G->Royale.HumanIndex) && G->Balls[G->Royale.HumanIndex]) G->PublicPopup(TEXT("VOLCANO ERUPTING!"), G->Balls[G->Royale.HumanIndex]->BallLocation() + FVector(0, 0, 220.f), FLinearColor(1.f, 0.35f, 0.1f));
			for (int32 k = 0; k < 5; ++k) AZoogiFX::Spawn(W, FVector(0.f, 0.f, 1750.f + 120.f * k), FLinearColor(1.f, 0.45f, 0.1f), 420.f, 1.4f, false, 14.f);
			UE_LOG(LogZoogi, Log, TEXT("STAGE SHIFT: Ancient eruption at %.0f s"), G->Royale.FightTime);
		}
		if (!bErupt) return;
		// launch: aim near a random Zoogi still in (on the floor), telegraph with a red ring
		if (G->Royale.FightTime >= NextBomb)
		{
			NextBomb = G->Royale.FightTime + FMath::FRandRange(1.8f, 2.8f);
			TArray<int32> In;
			for (int32 i = 0; i < G->Balls.Num() && i < G->Royale.P.Num(); ++i) if (G->Balls[i] && !G->Balls[i]->bOut && G->Royale.P[i].IsIn() && G->Royale.P[i].RespawnIn < 0.f) In.Add(i);
			if (In.Num())
			{
				const FVector Near = G->Balls[In[FMath::RandRange(0, In.Num() - 1)]]->BallLocation();
				FVector2D T2(Near.X + FMath::FRandRange(-380.f, 380.f), Near.Y + FMath::FRandRange(-380.f, 380.f));
				if (!ZoogiAncient::OnFloor(T2, 60.f)) T2 = FVector2D(Near);
				const FVector To(T2, ZoogiAncient::FloorZ(T2) + 6.f);
				FBomb Bm; Bm.From = FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 1900.f); Bm.To = To;
				Bm.Warn = AZoogiFX::Spawn(W, To, FLinearColor(1.f, 0.15f, 0.05f), 260.f, Bm.Dur + 0.1f, true, 10.f);
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AActor* R = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Bm.From), SP))
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(R);
					C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->SetMaterial(0, ZoogiUtil::GlowMID(R, FLinearColor(1.f, 0.35f, 0.05f), 4.f)); C->SetWorldScale3D(FVector(0.9f));
					C->RegisterComponent(); R->SetRootComponent(C);
					Bm.Rock = R;
				}
				AZoogiFX::Spawn(W, FVector(0.f, 0.f, 1850.f), FLinearColor(1.f, 0.5f, 0.1f), 260.f, 0.6f, false, 10.f);
				Bombs.Add(Bm);
			}
		}
		// fly (a high arc) and land
		for (int32 k = Bombs.Num() - 1; k >= 0; --k)
		{
			FBomb& Bm = Bombs[k];
			Bm.T += Dt / Bm.Dur;
			const float T = FMath::Min(Bm.T, 1.f);
			const FVector P = FMath::Lerp(Bm.From, Bm.To, T) + FVector(0, 0, 900.f * 4.f * T * (1.f - T));
			if (AActor* R = Bm.Rock.Get()) R->SetActorLocationAndRotation(P, FRotator(T * 720.f, T * 360.f, 0.f));
			if (Bm.T < 1.f) continue;
			AZoogiFX::Spawn(W, Bm.To + FVector(0, 0, 30.f), FLinearColor(1.f, 0.4f, 0.08f), 300.f, 0.5f, false, 14.f);
			AZoogiFX::Spawn(W, Bm.To, FLinearColor(1.f, 0.25f, 0.05f), 300.f, 0.7f, true, 12.f);
			for (int32 i = 0; i < G->Balls.Num() && i < G->Royale.P.Num(); ++i)
			{
				AZoogiBall* B = G->Balls[i];
				if (!B || B->bOut || !G->Royale.P[i].IsIn() || G->Royale.P[i].RespawnIn >= 0.f) continue;
				const FVector D = B->BallLocation() - Bm.To;
				if (D.Size2D() > 270.f || FMath::Abs(D.Z) > 250.f) continue;
				FZoogiRoyaleKick K; K.Victim = i; K.DeltaV = D.GetSafeNormal2D() * 950.f + FVector(0, 0, 420.f); G->Royale.Kicks.Add(K);
				G->RoyaleDamage(i, -1, 140, false, B->BallLocation());
			}
			if (AActor* R = Bm.Rock.Get()) R->Destroy();
			Bombs.RemoveAtSwap(k);
		}
	}
}
