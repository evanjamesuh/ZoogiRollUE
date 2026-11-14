// b180: Earthbender map events - see ZoogiEarthEvents.h
#include "ZoogiEarthEvents.h"
#include "ZoogiWallCine.h" // b238
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArenaReact.h" // b192
#include "ZoogiDragonBoss.h" // b193
#include "Containers/Ticker.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiHaptics.h"
#include "ZoogiActionFun.h"
#include "ZoogiSkills.h"
#include "ZoogiSkillLab.h" // b213
#include "NiagaraComponent.h" // b213
#include "ZoogiRedirect.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "CanvasItem.h"
#include "CollisionQueryParams.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiPhysics.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiFilm.h" // b215
#include "ZoogiScreenFx.h" // b215 blast options
#include "ZoogiFabFX.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);

namespace ZoogiEarthEvents
{
	namespace
	{
		enum class EK : uint8 { Shift, Tremor, Cataclysm };
		struct FEv { const TCHAR* Name; int32 V; EK K; bool bWall; };
		struct FSet { TArray<FEv> Ev; FLinearColor Col = FLinearColor::White; bool bSnow = false; FLinearColor Stone = FLinearColor(0.45f, 0.43f, 0.4f); };

		FString FxPath(int32 V) { return FString::Printf(TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V%02d.NS_Earthbender_V%02d"), V, V); }

		const FSet& SetFor(const AZoogiGameMode* G)
		{
			static TMap<int32, FSet> M;
			if (M.Num() == 0)
			{
				auto Add = [](int32 Key, FLinearColor Col, bool bSnow, TArray<FEv> Ev) { FSet Fs; Fs.Ev = MoveTemp(Ev); Fs.Col = Col; Fs.bSnow = bSnow; M.Add(Key, MoveTemp(Fs)); };
				const FLinearColor Slate(0.55f, 0.6f, 0.8f), Jade(0.4f, 0.9f, 0.75f), Sap(0.25f, 0.45f, 1.f), Obs(0.55f, 0.4f, 0.75f), Sand(0.95f, 0.78f, 0.5f), Gold(1.f, 0.8f, 0.25f), Rune(0.3f, 0.75f, 1.f), Snow(0.85f, 0.95f, 1.f);
				Add((int32)EZoogiArena::Meadow, Jade, false, { { TEXT("JADE WALL"), 10, EK::Shift, true }, { TEXT("JADE COLUMNS"), 36, EK::Shift, true }, { TEXT("MOSS TOWER"), 13, EK::Shift, false },
					{ TEXT("JADE BOULDER"), 6, EK::Tremor, false }, { TEXT("BOULDER FOUNTAIN"), 8, EK::Tremor, false }, { TEXT("BOULDER FOUNTAIN"), 8, EK::Cataclysm, false } });
				Add((int32)EZoogiArena::Frozen, Snow, true, { { TEXT("SNOWCAP PILLAR"), 11, EK::Shift, false }, { TEXT("SAPPHIRE BLOCK"), 21, EK::Shift, false },
					{ TEXT("SAPPHIRE BURST"), 20, EK::Tremor, false }, { TEXT("FROSTROCK GROVE"), 14, EK::Cataclysm, false } });
				Add((int32)EZoogiArena::Volcanic, Obs, false, { { TEXT("OBSIDIAN WALL"), 16, EK::Shift, true }, { TEXT("OBSIDIAN SPIKE"), 15, EK::Shift, false },
					{ TEXT("OBSIDIAN CLUSTER"), 17, EK::Tremor, false }, { TEXT("ROCK GEYSER"), 33, EK::Tremor, false }, { TEXT("OBSIDIAN BURST"), 18, EK::Cataclysm, false } });
				Add((int32)EZoogiArena::Cosmic, Obs, false, { { TEXT("STARBURST SPIKES"), 32, EK::Shift, false },
					{ TEXT("SHATTER STAR"), 46, EK::Tremor, false }, { TEXT("OBSIDIAN STAR"), 39, EK::Tremor, false }, { TEXT("EARTH FLASH"), 40, EK::Cataclysm, false } });
				Add(100, Rune, false, { { TEXT("RUNE WALL"), 26, EK::Shift, true }, { TEXT("RUNE PILLAR"), 25, EK::Shift, false }, { TEXT("RUNE MONOLITH"), 27, EK::Shift, false },
					{ TEXT("RUNE PILLAR"), 25, EK::Tremor, false }, { TEXT("CRATER RING"), 34, EK::Cataclysm, false } }); // the Underrealm
				Add((int32)EZoogiArena::Arabian, Sand, false, { { TEXT("SANDSTONE RUINS"), 38, EK::Shift, true }, { TEXT("SANDSTONE SPIRE"), 3, EK::Shift, false }, { TEXT("TOPPLED COLUMNS"), 29, EK::Shift, true },
					{ TEXT("DESERT RUBBLE"), 5, EK::Tremor, false }, { TEXT("LANDSLIDE"), 31, EK::Cataclysm, false } });
				Add((int32)EZoogiArena::Tomb, Gold, false, { { TEXT("GOLD WALL"), 24, EK::Shift, true }, { TEXT("GOLD COLUMN"), 35, EK::Shift, false },
					{ TEXT("GOLD RUSH"), 22, EK::Tremor, false }, { TEXT("GOLD NUGGET"), 23, EK::Tremor, false }, { TEXT("STONE CAGE"), 43, EK::Cataclysm, false } });
				Add((int32)EZoogiArena::Neon, Rune, false, { { TEXT("SLAB ROW"), 9, EK::Shift, true }, { TEXT("SLAB SLAM"), 47, EK::Tremor, false }, { TEXT("RIFT CRACK"), 48, EK::Cataclysm, false } });
				const TArray<FEv> Train = { { TEXT("STONE BARRICADE"), 4, EK::Shift, true }, { TEXT("GRANITE BLOCK"), 12, EK::Shift, false },
					{ TEXT("BOULDER BLOCK"), 2, EK::Tremor, false }, { TEXT("BOULDER PILE"), 50, EK::Cataclysm, false } };
				Add((int32)EZoogiArena::Training, Slate, false, Train);
				Add((int32)EZoogiArena::Teaser, Slate, false, Train);
				Add((int32)EZoogiArena::Ancient, Sap, false, { { TEXT("STONE TEETH"), 28, EK::Shift, true }, { TEXT("SAPPHIRE PILLAR"), 19, EK::Shift, false },
					{ TEXT("ROCK BURST"), 7, EK::Tremor, false }, { TEXT("RUBBLE STORM"), 42, EK::Cataclysm, false } });
			}
			int32 Key = G && G->Arena ? (int32)G->Arena->GetTheme() : 0;
			if (Key == (int32)EZoogiArena::Cosmic && ZoogiUnderrealm::On()) Key = 100;
			static bool bStones = false;
			if (!bStones)
			{	// b180: every rock gets one solid stone colour per map (the pack's material showed white / lavender on the phone)
				bStones = true;
				auto St = [](int32 K, const TCHAR* Hx) { if (FSet* F = M.Find(K)) F->Stone = ZoogiUtil::Hex(Hx); };
				St((int32)EZoogiArena::Meadow, TEXT("#9AA488")); St((int32)EZoogiArena::Frozen, TEXT("#CFE0EE")); St((int32)EZoogiArena::Volcanic, TEXT("#3B3340"));
				St((int32)EZoogiArena::Cosmic, TEXT("#4B4466")); St(100, TEXT("#3E4D60")); St((int32)EZoogiArena::Arabian, TEXT("#C49A62")); St((int32)EZoogiArena::Tomb, TEXT("#A98A4E"));
				St((int32)EZoogiArena::Neon, TEXT("#596273")); St((int32)EZoogiArena::Training, TEXT("#7D8590")); St((int32)EZoogiArena::Teaser, TEXT("#7D8590")); St((int32)EZoogiArena::Ancient, TEXT("#6C7EA0"));
			}
			static const FSet Empty;
			const FSet* Fs = M.Find(Key);
			return Fs ? *Fs : Empty;
		}

		struct FRockInfo { TWeakObjectPtr<AZoogiFabFX> Fx; int32 Group = 0; TWeakObjectPtr<UStaticMeshComponent> Vis; FVector FxLoc = FVector::ZeroVector; float HP = 1.f, Rad = 75.f, H = 200.f, Shake = 0.f; double HitAt = -10.0; int32 V = 7; FVector VisLoc = FVector::ZeroVector; int32 Born = 0; };
		struct FChunk { TWeakObjectPtr<AStaticMeshActor> A; FVector P, Vel, Spin; FRotator Rot; float Age = 0.f, Life = 2.6f, Size = 40.f, Floor = 0.f; FVector Scale; };
		struct FWarn { FVector P = FVector::ZeroVector; float Rad = 300.f; double T0 = 0.0, FireAt = 0.0; int32 V = 0; FString Name; bool bFired = false; };
		// b213: a skill's earth bend, solid while it shows
		struct FSolid { TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiFabFX> Fx; FVector Base = FVector::ZeroVector; float Yaw = 0.f, HalfLen = 100.f, HalfThick = 0.f, H = 200.f; double ArmAt = 0.0, Until = 0.0, MadeAt = 0.0; bool bArmed = false, bHadFx = false; };
		struct FState
		{
			TWeakObjectPtr<AZoogiGameMode> G; int32 Seed = 0x7fffffff; AActor* FirstBall = nullptr;
			int32 Round = -1, TremorTurn = -1, Shifts = 0; bool bCata = false;
			TArray<TWeakObjectPtr<AActor>> Rocks; TArray<FVector> RockAt; TArray<FRockInfo> RockInfo; TArray<FChunk> Chunks; int32 Breaks = 0, NextGroup = 1;
			TArray<FSolid> Solids; // b213
			TArray<FWarn> Warns; float FloorZ = 0.f, CourtR = 1500.f; FVector Ctr = FVector::ZeroVector;
		};
		FState S;
		FRandomStream R(11);

		bool On(const AZoogiGameMode* G)
		{
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoEarthEvents"));
			if (G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Cosmic) return false; // b192 (Evan): no Earthbender events on Cosmos
			return !bOff && G && G->Arena && !G->bRoyaleMode && !G->bComboMode && SetFor(G).Ev.Num() > 0;
		}
		double Now(const AZoogiGameMode* G) { return G && G->GetWorld() ? G->GetWorld()->GetTimeSeconds() : 0.0; } // game time: frozen while a pick waits
		float GroundZ(const AZoogiGameMode* G, const FVector& P)
		{
			UWorld* W = G->GetWorld(); FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiEarth), false);
			for (const AZoogiBall* B : G->Balls) if (B) Q.AddIgnoredActor(B);
			for (const AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
			for (const TWeakObjectPtr<AActor>& Rk : S.Rocks) if (Rk.IsValid()) Q.AddIgnoredActor(Rk.Get());
			if (W && W->LineTraceSingleByChannel(H, FVector(P.X, P.Y, S.FloorZ + 500.f), FVector(P.X, P.Y, S.FloorZ - 500.f), ECC_WorldStatic, Q)) return H.ImpactPoint.Z;
			return S.FloorZ;
		}
		bool FreeSpot(const AZoogiGameMode* G, float MinR, float MaxR, float Clear, FVector& Out)
		{	// b180 (Evan: "all event rocks spawn center of the court on top of each other - should spawn randomly around the court"):
			// the best of 40 random spots all around the court = the one farthest from the rocks already standing
			float Best = -1.f; FVector BestP = FVector::ZeroVector;
			for (int32 Try = 0; Try < 40; ++Try)
			{
				const float A = R.FRandRange(0.f, 2.f * PI), Rr = S.CourtR * R.FRandRange(MinR, MaxR);
				const FVector P(S.Ctr.X + FMath::Cos(A) * Rr, S.Ctr.Y + FMath::Sin(A) * Rr, S.FloorZ);
				bool bOk = true;
				for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && (FVector::Dist2D(B->BallLocation(), P) < Clear + 120.f || FVector::Dist2D(B->SpawnLocation, P) < 380.f)) bOk = false;
				for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < Clear * 0.6f) bOk = false;
				float Near = 1.e6f;
				for (const FVector& Rk : S.RockAt) Near = FMath::Min(Near, FVector::Dist2D(Rk, P));
				if (Near < 600.f) bOk = false;
				if (bOk && Near > Best) { Best = Near; BestP = P; }
			}
			if (Best < 0.f) return false;
			Out = FVector(BestP.X, BestP.Y, GroundZ(G, BestP));
			return true;
		}
		void Knock(AZoogiGameMode* G, const FVector& P, float Rad, float Speed, float Up)
		{	// nobody's credit (like the volcano): a Zoogi thrown off by the ground itself is a fall, not someone's KO
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut || !B->Sphere || !B->Sphere->IsSimulatingPhysics()) continue;
				const FVector D = B->BallLocation() - P; const float Dd = D.Size2D(); if (Dd > Rad) continue;
				B->Sphere->WakeAllRigidBodies();
				B->Sphere->SetPhysicsLinearVelocity(B->Sphere->GetPhysicsLinearVelocity() + D.GetSafeNormal2D() * Speed * (1.f - 0.5f * Dd / Rad) + FVector(0, 0, Up));
				B->LastHitPlayer = -1;
			}
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere || !O->Sphere->IsSimulatingPhysics()) continue;
				const FVector D = O->OrbLocation() - P; const float Dd = D.Size2D(); if (Dd > Rad) continue;
				O->Sphere->WakeAllRigidBodies();
				O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() + D.GetSafeNormal2D() * Speed * 0.8f * (1.f - 0.5f * Dd / Rad) + FVector(0, 0, Up * 0.7f));
				O->LastHitBy = -1;
			}
		}
		UPhysicalMaterial* Bouncy()
		{	// b180 (Evan: "they also set up great bank shots"): a lively stone bounce
			static TWeakObjectPtr<UPhysicalMaterial> GBouncy;
			if (!GBouncy.IsValid()) { GBouncy = ZoogiPhysics::MakeMaterial(GetTransientPackage(), 0.15f, 0.88f, false); if (GBouncy.IsValid()) GBouncy->AddToRoot(); }
			return GBouncy.Get();
		}
		// b213 (Evan: "if I hit it it blasts from the side like that" / "not from the ground, but from the side of the rock"): a Zoogi or orb
		// that slams into a rock gets the Skill Lab BIG BLAST bursting OUT of the rock face where it struck (turned to face out, not up),
		// and the blast throws it back off the rock - so smashing a rival into a rock is a weapon (the credit stays with whoever hit it).
		// -ZoogiEarthGroundBlast = the old flat ORB KNOCKOUT blast.
		int32 GForceIF = 0; // b215: the impact-frame tour sets the variant for its next slam
		// b215 (Evan: "I wanted the black effect on impact when colliding with earth bending rock"): a Zoogi slamming a rock fires the
		// black-and-white manga IMPACT FRAME (Vefects Easy Impact Frames, variants 1-10). -ZoogiRockImpactFrame=<1-10> picks it (0 = off).
		int32 RockFrameVariant() { static int32 V = -1; if (V < 0) { V = 0; /* b215: off - Evan meant the BLAST, not a black frame (option 6 has it) */ FParse::Value(FCommandLine::Get(), TEXT("ZoogiRockImpactFrame="), V); } return GForceIF > 0 ? GForceIF : (GForceIF < 0 ? 0 : V); }
		// b215 (Evan Nov 10: "I wanted the blast effect on impact when colliding with earth bending rock, show me options"): the blast that
		// bursts out of the rock face. -ZoogiRockBlast=<1-8> picks it; -ZoogiRockBlastTour films all 8 (one slam each).
		int32 GForceBlast = 0;
		int32 BlastStyle() { static int32 V = -1; if (V < 0) { V = 4; /* b229: Evan picked option 4 (FIRE BLAST) */ FParse::Value(FCommandLine::Get(), TEXT("ZoogiRockBlast="), V); } return GForceBlast > 0 ? GForceBlast : V; }
		void SideBlast(AZoogiGameMode* G, const AActor* Who, UPrimitiveComponent* Body, const FVector& P, const FVector& Out, float Vn, bool bBall)
		{
			static TMap<TWeakObjectPtr<const AActor>, double> LastAt;
			const double Tn = FPlatformTime::Seconds();
			if (const double* Lt = LastAt.Find(Who)) if (Tn - *Lt < 0.4) return;
			LastAt.Add(Who, Tn);
			UWorld* W = G ? G->GetWorld() : nullptr; if (!W) return;
			const FVector N = Out.GetSafeNormal2D().IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Out.GetSafeNormal2D();
			static const bool bGround = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthGroundBlast"));
			if (bGround) { ZoogiArenaReact::OrbOutBlast(W, P, false); return; }
			if (bBall && Vn >= 600.f && RockFrameVariant() > 0 && (GForceBlast == 0 || GForceBlast == 6)) { ZoogiShow::ImpactFrame(W, P, RockFrameVariant()); UE_LOG(LogZoogi, Log, TEXT("EARTH rock impact frame V%02d"), RockFrameVariant()); }
			const float K = FMath::Clamp((Vn - 250.f) / 1100.f, 0.f, 1.f);
			const int32 Style = BlastStyle();
			const FRotator RotOut = FRotationMatrix::MakeFromZ((N + FVector(0.f, 0.f, 0.22f)).GetSafeNormal()).Rotator();
			const float Sk = FMath::Lerp(0.75f, 1.15f, K);
			bool bDone = true;
			switch (Style)
			{
			case 2: // ROCK BURST: the Earthbender eruption bursting sideways out of the face + flying rubble
				AZoogiFabFX::Play(W, *FxPath(7), P + N * 10.f, 0.42f * Sk, 3.f, nullptr, FVector::ZeroVector, FRotationMatrix::MakeFromZ(N).Rotator());
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), P + N * 20.f, 0.28f * Sk, RotOut);
				for (int32 q = 0; q < 6; ++q) AZoogiFX::Spawn(W, P + N * FMath::FRandRange(20.f, 90.f) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-30.f, 50.f)), FLinearColor(0.55f, 0.45f, 0.35f), 40.f, 0.6f, false, 2.f);
				ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 0.9f, 0.95f); ZoogiUtil::PlaySfx(G, TEXT("Debris"), 0.8f, 1.f);
				break;
			case 3: // SHOCKWAVE: a ring of air punching out of the face + a white burst + dust
				UZoogiScreenFx::Shockwave(W, P + N * 20.f, 260.f * Sk, FLinearColor(1.f, 1.f, 1.f));
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), P + N * 25.f, RotOut, 0.9f * Sk);
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v3"), P + N * 20.f, 0.25f * Sk, RotOut);
				AZoogiFX::Spawn(W, P + N * 10.f, FLinearColor(0.8f, 0.78f, 0.72f), 150.f * Sk, 0.5f, true, 4.f);
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 1.1f); ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.8f, 1.f);
				break;
			case 4: // FIRE BLAST: a fireball out of the face
				AZoogiFX::Stylized(W, TEXT("Fire_v2"), P + N * 25.f, 0.35f * Sk, RotOut);
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), P + N * 15.f, 0.32f * Sk, RotOut);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), P + N * 25.f, RotOut, 0.8f * Sk);
				ZoogiUtil::PlaySfx(G, TEXT("Explosion"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 0.6f, 1.1f);
				break;
			case 5: // ELECTRIC: a crackling shock arc off the face
				AZoogiFX::Stylized(W, TEXT("ElectricShock"), P + N * 20.f, 0.4f * Sk, RotOut);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Yellow"), P + N * 25.f, RotOut, 0.8f * Sk);
				AZoogiFX::Spawn(W, P + N * 15.f, FLinearColor(1.f, 0.9f, 0.3f), 110.f * Sk, 0.3f, false, 9.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("HitCrack"), 0.8f, 1.f);
				break;
			case 7: // CRYSTAL SHATTER: glassy shards bursting off the face
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), P + N * 25.f, RotOut, 1.f * Sk);
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v2"), P + N * 18.f, 0.26f * Sk, RotOut);
				AZoogiFX::Spawn(W, P + N * 12.f, FLinearColor(0.7f, 0.4f, 1.f), 120.f * Sk, 0.35f, false, 8.f);
				ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 1.f, 1.1f); ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.7f, 1.3f);
				break;
			case 8: // MEGA: the big blast + a second blast + shockwave + a screen shake
				AZoogiFX::Stylized(W, TEXT("Big_Explosion_v2"), P + N * 20.f, 0.45f * Sk, RotOut);
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v3"), P + N * 60.f + FVector(0, 0, 40.f), 0.3f * Sk, RotOut);
				UZoogiScreenFx::Shockwave(W, P + N * 20.f, 320.f * Sk, SetFor(G).Col);
				ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 0.9f, 0.9f);
				break;
			default: bDone = false; break; // 1 = BIG BLAST, 6 = BIG BLAST + the manga impact frame
			}
			if (bDone) { /* the throw-back below still runs */ }
			const TCHAR* Bx = bDone ? TEXT("") : ZoogiSkillLab::BlastFor(FName(TEXT("ko_orb")));
			if (!Bx) Bx = TEXT("Big_Explosion"); // Evan's pick: BIG BLAST
			const float Sz = FMath::Clamp(ZoogiSkillLab::BlastSizeFor(FName(TEXT("ko_orb"))), 0.5f, 2.f) * FMath::Lerp(0.32f, 0.6f, K);
			// the blast's "up" points out of the rock (a touch upward), so it bursts sideways from the face
			const FRotator Rot = FRotationMatrix::MakeFromZ((N + FVector(0.f, 0.f, 0.22f)).GetSafeNormal()).Rotator();
			const FLinearColor Col = SetFor(G).Col;
			if (!bDone)
			{	// 1 / 6: BIG BLAST (Evan's Skill Lab pick) out of the face
				if (*Bx) AZoogiFX::Stylized(W, Bx, P + N * 18.f, Sz, Rot);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), P + N * 25.f, Rot, 0.5f + 0.5f * K);
				AZoogiFX::Spawn(W, P + N * 10.f, Col, 90.f + 90.f * K, 0.45f, false, 5.f);
				ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 0.55f + 0.4f * K, 1.15f);
			}
			if (Body && Body->IsSimulatingPhysics() && Vn >= 450.f)
			{	// the blast throws it back off the rock face
				Body->WakeAllRigidBodies();
				const FVector V = Body->GetPhysicsLinearVelocity();
				const float Into = FVector::DotProduct(V, -N);
				const FVector Base = Into > 0.f ? V + N * Into * 1.8f : V; // still going in: bounce it first
				Body->SetPhysicsLinearVelocity(Base + N * FMath::Min(Vn * (bBall ? 0.45f : 0.35f), 650.f) + FVector(0.f, 0.f, 160.f + 160.f * K));
				if (bBall && Vn >= 800.f) G->PublicPopup(TEXT("ROCK SLAM!"), P + FVector(0.f, 0.f, 120.f), Col);
				if (bBall && Vn >= 800.f) if (AZoogiBall* Sb = const_cast<AZoogiBall*>(Cast<AZoogiBall>(Who))) if (G->Balls.IsValidIndex(Sb->LastHitPlayer) && Sb->LastHitPlayer != Sb->PlayerIndex) ZoogiWallCine::Offer(G, G->Balls[Sb->LastHitPlayer], Sb, 3); // b238: the WALL SLAM moment
				ZoogiHaptics::Kick(0.25f + 0.35f * K, W, TEXT("earth_side_blast"));
			}
			static int32 NShots = 0;
			if (!G->ShotDir.IsEmpty() && NShots < 4 && Vn >= 450.f) { ++NShots; G->TakeShotDelayed(FString::Printf(TEXT("rock_side_blast_%d_a"), NShots), 0.06f); G->TakeShotDelayed(FString::Printf(TEXT("rock_side_blast_%d_b"), NShots), 0.3f); }
			UE_LOG(LogZoogi, Log, TEXT("EARTH side blast %s at (%.0f, %.0f, %.0f) out (%.2f, %.2f) %.0f cm/s"), Who ? *Who->GetName() : TEXT("?"), P.X, P.Y, P.Z, N.X, N.Y, Vn);
		}
		void RemoveRock(int32 i)
		{
			if (!S.Rocks.IsValidIndex(i)) return;
			S.Rocks.RemoveAt(i); S.RockAt.RemoveAt(i); if (S.RockInfo.IsValidIndex(i)) S.RockInfo.RemoveAt(i);
		}
		// b180 (Evan: "let the event earth bending effect stay until destroyed - max 5 on the ring at a time"): the event's Earthbender
		// effect is held at its formed pose (AZoogiFabFX::FreezeAt) and IS the obstacle (the rock model hides); it crumbles when broken
		// b181 FIX: a namespace-scope static read the command line during static init = fatal "command line not initialized" at app launch on iOS (B180 crash). Read lazily.
		bool ShowModel() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthRockModel")); return b; }
		float FreezeSec() { static float F = -1.f; if (F < 0.f) { F = 1.1f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiEarthFreezeAt="), F); } return F; }
		void KillGroup(int32 Group, bool bFx)
		{
			for (int32 j = S.Rocks.Num() - 1; j >= 0; --j)
				if (S.RockInfo.IsValidIndex(j) && S.RockInfo[j].Group == Group)
				{
					if (bFx) if (AZoogiFabFX* Fx = S.RockInfo[j].Fx.Get()) Fx->Release(2.5f);
					if (AActor* Old = S.Rocks[j].Get()) Old->Destroy();
					RemoveRock(j);
				}
		}
		AActor* Rock(AZoogiGameMode* G, const FVector& At, float Yaw, float H, bool bSnow, int32 FxV = 7, AZoogiFabFX* HeldFx = nullptr, int32 Group = 0)
		{	// a rock that stays: the pack's rock (or snow rock) model + an invisible solid cylinder the marbles bounce off
			UWorld* W = G->GetWorld();
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), At), Sp);
			if (!A) return nullptr;
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* Col = A->GetStaticMeshComponent();
			Col->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
			Col->SetVisibility(false); Col->SetHiddenInGame(true);
			static const bool bShowCol = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowEarthCol")); // b213 test: see the solid core
			Col->SetCollisionProfileName(TEXT("BlockAll"));
			Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Col->SetRelativeScale3D(FVector(1.5f, 1.5f, H / 100.f)); Col->SetRelativeLocation(FVector(0, 0, H * 0.5f));
			if (UPhysicalMaterial* Bm = Bouncy()) Col->SetPhysMaterialOverride(Bm); // b180 bank shots
			FRockInfo Info; Info.H = H; Info.V = FxV; Info.Fx = HeldFx; Info.FxLoc = HeldFx ? HeldFx->GetActorLocation() : FVector::ZeroVector; Info.Group = Group > 0 ? Group : S.NextGroup++;
			const int32 Nm = bSnow ? 2 + R.RandRange(0, 2) : R.RandRange(0, 8);
			const FString Fixed = bSnow ? FString::Printf(TEXT("/Game/EarthbenderV1/Models/SM_Free_Snow_Model_%d.SM_Free_Snow_Model_%d"), Nm, Nm) : FString::Printf(TEXT("/Game/EarthbenderV1/Models/SM_Free_Rock_Model_%d.SM_Free_Rock_Model_%d"), Nm, Nm);
			if (UStaticMesh* Rm = LoadObject<UStaticMesh>(nullptr, *Fixed, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UStaticMeshComponent* Vis = NewObject<UStaticMeshComponent>(A);
				Vis->SetMobility(EComponentMobility::Movable);
				Vis->SetStaticMesh(Rm); Vis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const FBox Bb = Rm->GetBoundingBox(); const float Mh = FMath::Max(1.f, Bb.GetSize().Z), Mw = FMath::Max(1.f, FMath::Max(Bb.GetSize().X, Bb.GetSize().Y));
				const float Ks = FMath::Min(H * 1.05f / Mh, 210.f / Mw);
				const FRotator Vr(R.FRandRange(-6.f, 6.f), R.FRandRange(0.f, 360.f), R.FRandRange(-6.f, 6.f));
				// b180: centre the model on the collider (its pivot can sit off the rock) and place it in WORLD space explicitly - on the
				// phone the attached model stayed at the court centre, floating, while the invisible collider stood where it should
				const FVector Off = Vr.RotateVector(FVector(Bb.GetCenter().X, Bb.GetCenter().Y, 0.f) * Ks);
				const FVector Wl = At + FVector(-Off.X, -Off.Y, -Bb.Min.Z * Ks - 10.f);
				Vis->RegisterComponent();
				Vis->AttachToComponent(A->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				Vis->SetWorldScale3D(FVector(Ks));
				Vis->SetWorldLocationAndRotation(Wl, Vr);
				A->AddInstanceComponent(Vis);
				if (UMaterialInstanceDynamic* Mt = ZoogiUtil::SurfaceMID(A, SetFor(G).Stone, 0.85f, SetFor(G).Stone, 0.45f)) // b180: a self-lit share so the pack meshes never render black
					for (int32 k = 0; k < FMath::Max(1, Vis->GetNumMaterials()); ++k) Vis->SetMaterial(k, Mt);
				Info.Vis = Vis; Info.VisLoc = Wl;
				if (HeldFx && !ShowModel()) Vis->SetVisibility(false); // the held Earthbender effect is the rock
				Info.Rad = 75.f;
			}
			else { Col->SetVisibility(true); Col->SetHiddenInGame(false); } // no model: the cylinder shows
			if (HeldFx && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthThinCollider")))
			{	// b195 (Evan: "the earth bend effect should be a solid collider"): the solid cylinder grows to the held effect's footprint
				// (it was a fixed 1.5 m post inside a much bigger formation, so Zoogis and orbs rolled through the effect)
				const FBox Fb = HeldFx->GetComponentsBoundingBox(true);
				float Rr = 110.f;
				if (Fb.IsValid) { const FVector Ex = Fb.GetExtent(); Rr = FMath::Min(Ex.X, Ex.Y) > 700.f ? 150.f : FMath::Clamp(FMath::Min(Ex.X, Ex.Y) * 0.8f, 100.f, 240.f); } // b197: the pack's fixed 20 m bounds say nothing - a 3 m solid core then
				Col->SetRelativeScale3D(FVector(Rr / 50.f, Rr / 50.f, H / 100.f));
				Info.Rad = Rr;
				UE_LOG(LogZoogi, Log, TEXT("EARTH rock collider r%.0f (effect box %.0f x %.0f x %.0f)"), Rr, Fb.IsValid ? Fb.GetExtent().X * 2.f : 0.f, Fb.IsValid ? Fb.GetExtent().Y * 2.f : 0.f, Fb.IsValid ? Fb.GetExtent().Z * 2.f : 0.f);
			}
			if (bShowCol) { Col->SetVisibility(true); Col->SetHiddenInGame(false); Col->SetMaterial(0, ZoogiUtil::GlowMID(A, FLinearColor(1.f, 0.2f, 0.6f), 0.6f)); }
			Info.Born = G->RoundNumber; // b193
			S.Rocks.Add(A); S.RockAt.Add(At); S.RockInfo.Add(Info);
			// a cap so the court never clogs: the oldest rock crumbles away
			static const int32 Cap = [] { int32 C = 5; FParse::Value(FCommandLine::Get(), TEXT("ZoogiEarthRockCap="), C); return C; }(); // b180: 5 formations on the ring
			for (;;)
			{
				TArray<int32> Groups; for (const FRockInfo& Ri : S.RockInfo) Groups.AddUnique(Ri.Group);
				if (Groups.Num() <= Cap || !S.Rocks.Num()) break;
				if (AActor* Old = S.Rocks[0].Get()) AZoogiFX::Spawn(W, Old->GetActorLocation(), FLinearColor(0.6f, 0.55f, 0.5f), 200.f, 0.5f, true, 9.f);
				KillGroup(S.RockInfo[0].Group, true); // the oldest formation crumbles away
			}
			UE_LOG(LogZoogi, Log, TEXT("EARTH rock at (%.0f, %.0f, %.0f) h%.0f (%d standing)"), At.X, At.Y, At.Z, H, S.Rocks.Num());
			return A;
		}
		void Callout(AZoogiGameMode* G, const FString& Big, const FString& Sub, const FLinearColor& Col)
		{
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Big, Sub, Col, 1.8f, 1.1f);
		}
		AZoogiFabFX* Erupt(AZoogiGameMode* G, const FEv& E, const FVector& At, float Scl, float KnockR)
		{
			UWorld* W = G->GetWorld();
			AZoogiFabFX* Fx = AZoogiFabFX::Play(W, *FxPath(E.V), At, Scl, 8.f);
			AZoogiFX::Spawn(W, At + FVector(0, 0, 6.f), SetFor(G).Col, KnockR, 0.5f, true, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.9f, 0.85f); ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 1.f, 0.9f);
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			FTimerHandle Th; W->GetTimerManager().SetTimer(Th, [WG]() { if (AZoogiGameMode* Gm = WG.Get()) ZoogiUtil::PlaySfx(Gm, TEXT("Debris"), 0.7f, 1.f); }, 0.4f, false);
			ZoogiHaptics::Kick(0.7f, W, TEXT("earth_event"));
			Knock(G, At, KnockR, 950.f, 420.f);
			return Fx;
		}
		// ---- b180 (Evan: "event rocks that appear can be destroyed - great use for 3D animations"): breakable rocks
		void BreakRock(AZoogiGameMode* G, int32 i, const FVector& From, const FVector& Dir, float Speed, int32 Credit)
		{
			if (!G || !S.Rocks.IsValidIndex(i)) return;
			UWorld* W = G->GetWorld();
			AActor* A = S.Rocks[i].Get();
			const FRockInfo Info = S.RockInfo.IsValidIndex(i) ? S.RockInfo[i] : FRockInfo();
			const FVector At = S.RockAt[i];
			const FSet& Set = SetFor(G);
			UStaticMeshComponent* Vis = Info.Vis.Get();
			UStaticMesh* Mesh = Vis ? Vis->GetStaticMesh().Get() : ZoogiUtil::BasicShape(TEXT("Cube"));
			UMaterialInterface* Mat = Vis ? Vis->GetMaterial(0) : nullptr;
			const float Ks = Vis ? Vis->GetComponentScale().X : 1.f;
			const FVector Mid = At + FVector(0, 0, Info.H * 0.45f);
			FVector D = Dir.GetSafeNormal2D(); if (D.IsNearlyZero()) D = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			// the chunks: pieces of the same rock tumble away from the hit (hand-animated: arc, bounce, settle, shrink away)
			const int32 N = Set.bSnow ? 7 : 9;
			for (int32 k = 0; k < N && Mesh; ++k)
			{
				FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FVector P0 = Mid + FVector(R.FRandRange(-50.f, 50.f), R.FRandRange(-50.f, 50.f), R.FRandRange(-Info.H * 0.35f, Info.H * 0.35f));
				AStaticMeshActor* C = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, P0), Sp);
				if (!C) continue;
				C->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* Cm = C->GetStaticMeshComponent();
				Cm->SetStaticMesh(Mesh); Cm->SetCollisionEnabled(ECollisionEnabled::NoCollision); Cm->SetCastShadow(k < 4);
				if (Mat) for (int32 m = 0; m < FMath::Max(1, Cm->GetNumMaterials()); ++m) Cm->SetMaterial(m, Mat);
				FChunk Ch; Ch.A = C; Ch.P = P0;
				const float Sc = R.FRandRange(0.2f, 0.36f) * Ks;
				Ch.Scale = FVector(Sc * R.FRandRange(0.8f, 1.2f), Sc * R.FRandRange(0.8f, 1.2f), Sc * R.FRandRange(0.7f, 1.1f));
				Ch.Size = Info.H * 0.2f;
				Ch.Rot = FRotator(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f));
				Ch.Spin = FVector(R.FRandRange(-540.f, 540.f), R.FRandRange(-540.f, 540.f), R.FRandRange(-360.f, 360.f));
				const FVector Out = (P0 - Mid).GetSafeNormal2D();
				Ch.Vel = (D * 0.65f + Out * 0.55f + FVector(R.FRandRange(-0.25f, 0.25f), R.FRandRange(-0.25f, 0.25f), 0.f)).GetSafeNormal2D() * R.FRandRange(260.f, 420.f + Speed * 0.35f)
					+ FVector(0, 0, R.FRandRange(380.f, 760.f));
				Ch.Floor = At.Z; Ch.Life = R.FRandRange(2.2f, 3.0f);
				Cm->SetWorldScale3D(Ch.Scale); C->SetActorRotation(Ch.Rot);
				S.Chunks.Add(Ch);
			}
			if (AZoogiFabFX* Fx = Info.Fx.Get()) Fx->Release(2.5f); // the held effect crumbles
			if (A) A->Destroy();
			RemoveRock(i);
			if (Info.Group > 0) KillGroup(Info.Group, false); // the rest of a wall goes with it
			// the burst: the map's earth FX, dust ring, crack + shatter + rumble, a haptic, the crowd gasps
			AZoogiFabFX::Play(W, *FxPath(Info.V), At, 0.8f, 6.f);
			AZoogiFX::Spawn(W, At + FVector(0, 0, 8.f), Set.Col, 260.f, 0.5f, true, 9.f);
			AZoogiFX::Spawn(W, Mid, FLinearColor(0.75f, 0.7f, 0.62f), 200.f, 0.7f, false, 4.f);
			ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 1.f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 0.9f, 1.05f);
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			FTimerHandle Th; W->GetTimerManager().SetTimer(Th, [WG]() { if (AZoogiGameMode* Gm = WG.Get()) ZoogiUtil::PlaySfx(Gm, TEXT("Debris"), 0.8f, 1.f); }, 0.3f, false);
			ZoogiHaptics::Kick(0.65f, W, TEXT("earth_rock_break"));
			ZoogiShow::Event(W, ZoogiShow::EEvent::Blast, At, 1.f);
			G->PublicPopup(TEXT("ROCK SMASHED!"), At + FVector(0, 0, Info.H + 90.f), Set.Col);
			if (!G->ShotDir.IsEmpty() && S.Breaks < 3) { G->TakeShotDelayed(FString::Printf(TEXT("earth_rock_break_%d_a"), S.Breaks + 1), 0.12f); G->TakeShotDelayed(FString::Printf(TEXT("earth_rock_break_%d_b"), S.Breaks + 1), 0.7f); }
			++S.Breaks;
			UE_LOG(LogZoogi, Log, TEXT("EARTH rock BROKEN at (%.0f, %.0f) by %s (%.0f cm/s), %d chunks, %d rocks left"), At.X, At.Y, Credit >= 0 ? *FString::Printf(TEXT("P%d"), Credit + 1) : TEXT("a blast"), Speed, N, S.Rocks.Num());
		}
		void HitRock(AZoogiGameMode* G, int32 i, const FVector& From, const FVector& Dir, float Speed, int32 Credit)
		{	// a hard hit (>= 1250 cm/s) breaks a rock at once; softer hits crack it (two or three of them break it)
			if (!S.RockInfo.IsValidIndex(i)) return;
			FRockInfo& Ri = S.RockInfo[i];
			const double Nw = Now(G);
			if (Nw - Ri.HitAt < 0.35) return;
			Ri.HitAt = Nw;
			Ri.HP -= Speed >= 1250.f ? 1.f : FMath::Clamp(0.25f + (Speed - 450.f) / 1600.f, 0.25f, 0.75f);
			UE_LOG(LogZoogi, Log, TEXT("EARTH rock hit at %.0f cm/s -> hp %.2f"), Speed, Ri.HP);
			if (Ri.HP <= 0.f) { BreakRock(G, i, From, Dir, Speed, Credit); return; }
			Ri.Shake = 1.f;
			UWorld* W = G->GetWorld();
			AZoogiFX::Spawn(W, From + Dir.GetSafeNormal2D() * 60.f + FVector(0, 0, 40.f), FLinearColor(0.75f, 0.7f, 0.62f), 110.f, 0.4f, false, 3.f);
			ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.8f, 1.1f);
			ZoogiHaptics::Kick(0.3f, W, TEXT("earth_rock_crack"));
			if (UStaticMeshComponent* V = Ri.Vis.Get()) V->SetWorldScale3D(V->GetComponentScale() * 0.96f); // chipped
		}
		void TickRocks(AZoogiGameMode* G, float Dt)
		{
			const float ZR = ZoogiConst::ZoogiRadius;
			for (int32 i = S.Rocks.Num() - 1; i >= 0; --i)
			{
				if (!S.Rocks.IsValidIndex(i)) continue; // a broken wall takes its neighbours with it
				if (!S.Rocks[i].IsValid()) { RemoveRock(i); continue; }
				if (!S.RockInfo.IsValidIndex(i)) continue;
				FRockInfo& Ri = S.RockInfo[i];
				const FVector At = S.RockAt[i];
				// crack shake
				if (Ri.Shake > 0.f)
				{
					Ri.Shake = FMath::Max(0.f, Ri.Shake - Dt * 3.f);
					const FVector Jit = FVector(FMath::Sin(Ri.Shake * 60.f), FMath::Cos(Ri.Shake * 47.f), 0.f) * 9.f * Ri.Shake;
					if (UStaticMeshComponent* V = Ri.Vis.Get()) V->SetWorldLocation(Ri.VisLoc + Jit);
					if (AZoogiFabFX* Fx = Ri.Fx.Get()) Fx->SetActorLocation(Ri.FxLoc + Jit); // the held formation shudders
				}
				// b192 (Evan: "when Zoogis or orbs collide with an earth bend, show the ORB KNOCKOUT blast on the rock's surface"):
				// the Skill Lab ORB KNOCKOUT blast where it touches (once per 0.4 s per Zoogi / orb)
				// b213: now the BIG BLAST out of the rock's side (SideBlast), and it throws the hitter back off the rock
				auto SurfaceBlast = [&](const AActor* Who, UPrimitiveComponent* Body, const FVector& L, const FVector& Dn, float Vn, bool bBall)
				{
					const FVector P(At.X - Dn.X * Ri.Rad, At.Y - Dn.Y * Ri.Rad, FMath::Clamp(L.Z, At.Z + 30.f, At.Z + Ri.H - 20.f));
					SideBlast(G, Who, Body, P, -Dn, Vn, bBall);
				};
				for (AZoogiOrb* O : G->Orbs)
				{
					if (!O || O->bOut || !O->Sphere) continue;
					const FVector L = O->OrbLocation();
					if (L.Z > At.Z + Ri.H + ZoogiConst::OrbRadius || FVector::Dist2D(L, At) > Ri.Rad + ZoogiConst::OrbRadius + 22.f) continue;
					const FVector Dn = FVector(At.X - L.X, At.Y - L.Y, 0.f).GetSafeNormal();
					const float Vo = FVector::DotProduct(O->Sphere->GetPhysicsLinearVelocity(), Dn);
					if (Vo >= 250.f) SurfaceBlast(O, O->Sphere, L, Dn, Vo, false);
				}
				for (AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut || !B->Sphere) continue;
					const FVector L = B->BallLocation();
					if (L.Z > At.Z + Ri.H + ZR || FVector::Dist2D(L, At) > Ri.Rad + ZR + 22.f) continue;
					const FVector Dn = FVector(At.X - L.X, At.Y - L.Y, 0.f).GetSafeNormal();
					const float Vn = FMath::Max(FVector::DotProduct(B->PrevVelocity, Dn), FVector::DotProduct(B->GetBallVelocity(), Dn));
					if (Vn >= 250.f) SurfaceBlast(B, B->Sphere, L, Dn, Vn, true); // b192; b213 side blast
					if (Vn < 450.f) continue;
					HitRock(G, i, L, Dn, Vn, B->PlayerIndex);
					break;
				}
			}
			// the chunks fly, bounce, settle and shrink away
			const float Gz = -980.f * 1.5f;
			for (int32 k = S.Chunks.Num() - 1; k >= 0; --k)
			{
				FChunk& C = S.Chunks[k];
				AStaticMeshActor* A = C.A.Get();
				C.Age += Dt;
				if (!A || C.Age > C.Life) { if (A) A->Destroy(); S.Chunks.RemoveAt(k); continue; }
				C.Vel.Z += Gz * Dt; C.P += C.Vel * Dt;
				const float Bot = C.Floor + C.Size * 0.35f;
				if (C.P.Z < Bot)
				{
					C.P.Z = Bot;
					if (C.Vel.Z < -120.f) { C.Vel.Z *= -0.38f; C.Vel.X *= 0.62f; C.Vel.Y *= 0.62f; C.Spin *= 0.6f; }
					else { C.Vel = FVector(C.Vel.X * 0.9f, C.Vel.Y * 0.9f, 0.f); C.Spin *= 0.9f; }
				}
				C.Rot += FRotator(C.Spin.Y * Dt, C.Spin.Z * Dt, C.Spin.X * Dt);
				const float Fade = FMath::Clamp((C.Life - C.Age) / 0.6f, 0.f, 1.f);
				A->SetActorLocationAndRotation(C.P, C.Rot);
				if (UStaticMeshComponent* Cm = A->GetStaticMeshComponent()) Cm->SetWorldScale3D(C.Scale * FMath::Max(0.01f, Fade));
			}
		}

		// b213: the skill earth bends - solid while they show; a hit on any face blasts out of that face
		void TickSolids(AZoogiGameMode* G)
		{
			if (!S.Solids.Num()) return;
			const double Tn = FPlatformTime::Seconds();
			const float ZR = ZoogiConst::ZoogiRadius, OR = ZoogiConst::OrbRadius;
			// the closest point of a solid to L (2D), and whether L is within Pad of it / under its top
			auto Near = [](const FSolid& Sd, const FVector& L, float Pad, FVector& Cp) -> bool
			{
				if (L.Z > Sd.Base.Z + Sd.H + Pad) return false;
				if (Sd.HalfThick <= 0.f)
				{
					const FVector D = FVector(L.X - Sd.Base.X, L.Y - Sd.Base.Y, 0.f); const float Dd = D.Size();
					Cp = Sd.Base + (Dd > 1.f ? D / Dd : FVector(1.f, 0.f, 0.f)) * Sd.HalfLen;
					return Dd <= Sd.HalfLen + Pad;
				}
				const FRotator Yr(0.f, Sd.Yaw, 0.f);
				const FVector Loc = Yr.UnrotateVector(FVector(L.X - Sd.Base.X, L.Y - Sd.Base.Y, 0.f));
				FVector Cl(FMath::Clamp(Loc.X, -Sd.HalfLen, Sd.HalfLen), FMath::Clamp(Loc.Y, -Sd.HalfThick, Sd.HalfThick), 0.f);
				if (FMath::Abs(Loc.X) <= Sd.HalfLen && FMath::Abs(Loc.Y) <= Sd.HalfThick) Cl.Y = Loc.Y >= 0.f ? Sd.HalfThick : -Sd.HalfThick; // inside: the nearer long face
				Cp = Sd.Base + Yr.RotateVector(Cl);
				return FVector::Dist2D(Cp, L) <= Pad || (FMath::Abs(Loc.X) <= Sd.HalfLen && FMath::Abs(Loc.Y) <= Sd.HalfThick);
			};
			for (int32 i = S.Solids.Num() - 1; i >= 0; --i)
			{
				FSolid& Sd = S.Solids[i];
				AActor* A = Sd.A.Get();
				AZoogiFabFX* Fx = Sd.Fx.Get();
				bool bAlive = A && Tn < Sd.Until;
				const double Age = Tn - Sd.MadeAt;
				static const bool bOldLife = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSolidOldLife"));
				if (bAlive && Age > (bOldLife ? 0.0 : 4.5) && Sd.bHadFx && !Fx) bAlive = false; // b244: its effect was destroyed (not before 4.5 s)
				if (bAlive && Age > (bOldLife ? 0.0 : 4.5) && Fx && Fx->FreezeAt <= 0.f && (!Fx->NC || !Fx->NC->IsActive())) bAlive = false; // b244: the effect ended (was checked on frame 1: walls died at once)
				if (!bAlive)
				{
					if (A) A->Destroy();
					UE_LOG(LogZoogi, Log, TEXT("EARTH skill solid at (%.0f, %.0f) gone"), Sd.Base.X, Sd.Base.Y);
					S.Solids.RemoveAt(i); continue;
				}
				if (!Sd.bArmed)
				{	// solid once nothing stands inside it (a Zoogi or orb inside would be shot out)
					if (Tn < Sd.ArmAt) continue;
					bool bClear = true; FVector Cp;
					for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && Near(Sd, B->BallLocation(), ZR - 6.f, Cp) && FVector::Dist2D(Cp, B->BallLocation()) < ZR - 6.f) bClear = false;
					for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Near(Sd, O->OrbLocation(), OR - 6.f, Cp) && FVector::Dist2D(Cp, O->OrbLocation()) < OR - 6.f) bClear = false;
					if (!bClear) continue;
					if (UPrimitiveComponent* Pc = Cast<UPrimitiveComponent>(A->GetRootComponent())) Pc->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					Sd.bArmed = true;
					UE_LOG(LogZoogi, Log, TEXT("EARTH skill solid at (%.0f, %.0f) SOLID (%s %.0f x %.0f, h%.0f)"), Sd.Base.X, Sd.Base.Y, Sd.HalfThick > 0.f ? TEXT("wall") : TEXT("post"), Sd.HalfLen * 2.f, FMath::Max(Sd.HalfThick, Sd.HalfLen) * 2.f, Sd.H);
				}
				for (AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut || !B->Sphere) continue;
					const FVector L = B->BallLocation(); FVector Cp;
					if (!Near(Sd, L, ZR + 22.f, Cp)) continue;
					const FVector Out = FVector(L.X - Cp.X, L.Y - Cp.Y, 0.f).GetSafeNormal();
					const float Vn = FMath::Max(FVector::DotProduct(B->PrevVelocity, -Out), FVector::DotProduct(B->GetBallVelocity(), -Out));
					if (Vn >= 250.f) SideBlast(G, B, B->Sphere, FVector(Cp.X, Cp.Y, FMath::Clamp(L.Z, Sd.Base.Z + 30.f, Sd.Base.Z + Sd.H - 20.f)), Out, Vn, true);
				}
				for (AZoogiOrb* O : G->Orbs)
				{
					if (!O || O->bOut || !O->Sphere) continue;
					const FVector L = O->OrbLocation(); FVector Cp;
					if (!Near(Sd, L, OR + 22.f, Cp)) continue;
					const FVector Out = FVector(L.X - Cp.X, L.Y - Cp.Y, 0.f).GetSafeNormal();
					const float Vn = FVector::DotProduct(O->Sphere->GetPhysicsLinearVelocity(), -Out);
					if (Vn >= 250.f) SideBlast(G, O, O->Sphere, FVector(Cp.X, Cp.Y, FMath::Clamp(L.Z, Sd.Base.Z + 30.f, Sd.Base.Z + Sd.H - 20.f)), Out, Vn, false);
				}
			}
		}
		void SlamTest(AZoogiGameMode* G);
		void DoShift(AZoogiGameMode* G, bool bCata)
		{
			const FSet& Set = SetFor(G);
			TArray<const FEv*> Pool; for (const FEv& E : Set.Ev) if (E.K == (bCata ? EK::Cataclysm : EK::Shift)) Pool.Add(&E);
			if (!Pool.Num()) return;
			const FEv& E = *Pool[R.RandRange(0, Pool.Num() - 1)];
			if (bCata)
			{	// the big one: a huge burst from the centre ring + 3 rocks around it
				const FVector C(S.Ctr.X, S.Ctr.Y, GroundZ(G, S.Ctr));
				Erupt(G, E, C + FVector(S.CourtR * 0.42f, 0.f, 0.f).RotateAngleAxis(R.FRandRange(0.f, 360.f), FVector::UpVector), 2.2f, S.CourtR * 0.55f);
				int32 N = 0; FVector P;
				for (int32 k = 0; k < 6 && N < 3; ++k) if (FreeSpot(G, 0.3f, 0.75f, 300.f, P))
				{
					TArray<const FEv*> Sh; for (const FEv& Ev2 : Set.Ev) if (Ev2.K == EK::Shift && !Ev2.bWall) Sh.Add(&Ev2);
					const int32 Vh = Sh.Num() ? Sh[R.RandRange(0, Sh.Num() - 1)]->V : E.V;
					AZoogiFabFX* Hf = AZoogiFabFX::Play(G->GetWorld(), *FxPath(Vh), P, 1.2f, 8.f);
					if (Hf) { Hf->FreezeAt = FreezeSec(); Hf->DieAt = Hf->BornAt + 1.e6; }
					Rock(G, P, R.FRandRange(0.f, 360.f), 240.f, Set.bSnow, Vh, Hf); ++N;
				}
				Callout(G, FString::Printf(TEXT("CATACLYSM: %s!"), E.Name), TEXT("THE FINAL ROUND - THE ARENA BREAKS"), Set.Col);
				ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 1.f, 0.8f);
				ZoogiHaptics::Kick(1.f, G->GetWorld(), TEXT("earth_cataclysm"));
				if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("earth_cataclysm_a"), 0.6f); G->TakeShotDelayed(TEXT("earth_cataclysm_b"), 1.6f); }
				UE_LOG(LogZoogi, Log, TEXT("EARTH cataclysm %s (V%02d), %d rocks"), E.Name, E.V, N);
				return;
			}
			FVector P;
			if (!FreeSpot(G, 0.32f, 0.78f, 320.f, P)) { UE_LOG(LogZoogi, Log, TEXT("EARTH shift: no free spot")); return; }
			const float Yaw = (P - S.Ctr).Rotation().Yaw + 90.f; // walls run around the court (tangent)
			AZoogiFabFX* Fx = Erupt(G, E, P, E.bWall ? 1.1f : 1.3f, 300.f);
			if (Fx) { Fx->FreezeAt = FreezeSec(); Fx->DieAt = Fx->BornAt + 1.e6; } // b180: held until the rock breaks
			TWeakObjectPtr<AZoogiGameMode> WG = G; const FEv Ec = E; const bool bSnow = Set.bSnow;
			TWeakObjectPtr<AZoogiFabFX> WFx = Fx; const int32 Grp = S.NextGroup++;
			FTimerHandle Th;
			G->GetWorld()->GetTimerManager().SetTimer(Th, [WG, Ec, P, Yaw, bSnow, WFx, Grp]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				if (Ec.bWall) { const FVector T = FRotator(0.f, Yaw, 0.f).Vector(); for (int32 k = -1; k <= 1; ++k) Rock(Gm, P + T * 190.f * k, Yaw, 170.f, bSnow, Ec.V, WFx.Get(), Grp); }
				else Rock(Gm, P, Yaw, 260.f, bSnow, Ec.V, WFx.Get(), Grp);
			}, 0.45f, false);
			++S.Shifts;
			Callout(G, FString::Printf(TEXT("COURT SHIFT: %s"), E.Name), TEXT("THE ROCK STAYS FOR THE REST OF THE MATCH"), Set.Col);
			if (!G->ShotDir.IsEmpty()) { static int32 Ns = 0; ++Ns; G->TakeShotDelayed(FString::Printf(TEXT("earth_shift_%d_a"), Ns), 0.5f); G->TakeShotDelayed(FString::Printf(TEXT("earth_shift_%d_b"), Ns), 1.5f); }
			UE_LOG(LogZoogi, Log, TEXT("EARTH shift %s (V%02d) at (%.0f, %.0f)"), E.Name, E.V, P.X, P.Y);
		}
	}

	void Clear()
	{
		for (const TWeakObjectPtr<AActor>& A : S.Rocks) if (AActor* X = A.Get()) X->Destroy();
		for (const FChunk& C : S.Chunks) if (AStaticMeshActor* X = C.A.Get()) X->Destroy();
		for (const FRockInfo& Ri : S.RockInfo) if (AZoogiFabFX* Fx = Ri.Fx.Get()) Fx->Destroy();
		for (const FSolid& Sd : S.Solids) if (AActor* X = Sd.A.Get()) X->Destroy(); // b213
		const TWeakObjectPtr<AZoogiGameMode> G = S.G;
		S = FState(); S.G = G;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;
		AActor* First = G->Balls.Num() ? G->Balls[0].Get() : nullptr;
		if (S.G.Get() != G || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming || S.Seed != G->GameSeed || S.FirstBall != First)
		{	// a new match (or the menu): the court starts clean
			if (S.Rocks.Num() || S.Round >= 0) Clear();
			S.G = G; S.Seed = G->GameSeed; S.FirstBall = First;
			if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming) return;
		}
		TickSolids(G); // b213: skill earth bends work on every map
		SlamTest(G);
		if (!On(G)) return;
		if (S.Round < 0)
		{
			S.Ctr = G->Arena->GetActorLocation(); S.CourtR = G->Arena->GetKnockoffRadius();
			float Zs = 0.f; int32 N = 0; for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) { Zs += B->BallLocation().Z - ZoogiConst::ZoogiRadius; ++N; }
			S.FloorZ = N ? Zs / N : S.Ctr.Z;
			R.Initialize(GetTypeHash(G->GameSeed) ^ 0xEA27);
			S.Round = 0;
		}
		// start of a round (its first aim): COURT SHIFT - or, at the final round, the CATACLYSM
		if (G->Phase == EZoogiPhase::Aiming && G->RoundNumber != S.Round && G->PhaseTime > 0.6f)
		{
			S.Round = G->RoundNumber;
			const bool bFinal = G->RoundNumber >= G->MenuRounds && G->MenuRounds > 1;
			// b193 (Evan: "the dragon blasts earth bends that have been there two rounds without being destroyed"): Frozen - Dracoria
			// breathes ice on every formation that has stood 2 rounds, and it shatters
			if (ZoogiDragon::Active())
			{
				int32 Old = 0; FVector Aim = FVector::ZeroVector; TArray<TWeakObjectPtr<AActor>> Doomed;
				for (int32 i = 0; i < S.RockInfo.Num() && i < S.Rocks.Num(); ++i)
					if (S.Rocks[i].IsValid() && G->RoundNumber - S.RockInfo[i].Born >= 2) { if (Old == 0) Aim = S.RockAt[i]; Doomed.Add(S.Rocks[i]); ++Old; }
				if (Old > 0)
				{
					const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Aim.Y - S.Ctr.Y, Aim.X - S.Ctr.X));
					ZoogiDragon::Event(TEXT("Roar")); ZoogiDragon::Sweep(true, Ang);
					TWeakObjectPtr<AZoogiGameMode> WGd = G;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WGd, Doomed](float)
					{	// b194: the rocks shatter once the beam is on (after the ~2 s dive)
						AZoogiGameMode* Gd = WGd.Get();
						if (Gd) for (const TWeakObjectPtr<AActor>& Wr : Doomed)
						{
							const int32 Ix = S.Rocks.IndexOfByKey(Wr);
							if (Ix == INDEX_NONE || !Wr.IsValid() || !S.RockAt.IsValidIndex(Ix)) continue;
							const FVector Ra = S.RockAt[Ix]; const FVector Dir = FVector(Ra.X - S.Ctr.X, Ra.Y - S.Ctr.Y, 0.f).GetSafeNormal();
							BreakRock(Gd, Ix, Ra - Dir * 200.f, Dir, 1400.f, -1);
							AZoogiFX::Niagara(Gd->GetWorld(), TEXT("NS_Burst_Blue"), Ra + FVector(0, 0, 90.f), FRotator::ZeroRotator, 1.6f);
						}
						return false;
					}), 2.2f);
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Ang](float) { ZoogiDragon::Sweep(false, Ang); return false; }), 3.0f);
					if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("DRACORIA CLEARS THE ICE"), FString::Printf(TEXT("ICE BREATH SHATTERS %d OLD ROCK%s"), Old, Old == 1 ? TEXT("") : TEXT("S")), FLinearColor(0.55f, 0.85f, 1.f), 1.6f, 1.f);
					UE_LOG(LogZoogi, Log, TEXT("EARTH: Dracoria breathes on %d formation(s) 2+ rounds old (round %d)"), Old, G->RoundNumber);
				}
			}
			if (G->RoundNumber < 2 && !bFinal) {} // b192 (Evan): the Earthbender events start after round one
			else if (bFinal && !S.bCata) { S.bCata = true; DoShift(G, true); }
			else DoShift(G, false);
		}
		TickRocks(G, RealDt);
		// b180 test (-ZoogiEarthEventsTest): round 3 smashes the oldest rock so the break shows in the frames
		static const bool bRockTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthEventsTest"));
		if (bRockTest && S.Breaks == 0 && G->RoundNumber >= 2 && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.8f && S.Rocks.Num())
			BreakRock(G, 0, S.RockAt[0] - FVector(200.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), 1500.f, -1);
		// TREMOR: now and then during a roll - a warning circle first
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthEventsTest"));
		if (G->Phase == EZoogiPhase::Rolling && S.TremorTurn != G->TurnCounter && (G->RoundNumber >= 2 || bTest)) // b192: not in round one
		{
			S.TremorTurn = G->TurnCounter;
			const FSet& Set = SetFor(G);
			TArray<const FEv*> Pool; for (const FEv& E : Set.Ev) if (E.K == EK::Tremor) Pool.Add(&E);
			const bool bVolc = G->Arena->GetTheme() == EZoogiArena::Volcanic; // the volcano has its meteors too
			if (Pool.Num() && (bTest || R.FRand() < (bVolc ? 0.12f : 0.22f)))
			{
				const FEv& E = *Pool[R.RandRange(0, Pool.Num() - 1)];
				FWarn Wv; Wv.V = E.V; Wv.Name = E.Name; Wv.Rad = 300.f;
				FVector P;
				// near a Zoogi (they have to react), but not right under the roller's nose
				const AZoogiBall* Tg = nullptr; TArray<const AZoogiBall*> In; for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) In.Add(B);
				if (In.Num()) Tg = In[R.RandRange(0, In.Num() - 1)];
				if (Tg) { const FVector Off = FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f).Vector() * R.FRandRange(80.f, 260.f); P = Tg->BallLocation() + Off; }
				else if (!FreeSpot(G, 0.2f, 0.75f, 0.f, P)) P = S.Ctr;
				if (FVector::Dist2D(P, S.Ctr) > S.CourtR * 0.85f) P = S.Ctr + (P - S.Ctr).GetSafeNormal2D() * S.CourtR * 0.8f;
				Wv.P = FVector(P.X, P.Y, GroundZ(G, P) + 2.f);
				Wv.T0 = Now(G); Wv.FireAt = Wv.T0 + 1.5;
				S.Warns.Add(Wv);
				ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.6f, 1.1f);
				ZoogiHaptics::Kick(0.3f, G->GetWorld(), TEXT("earth_tremor_warn"));
				UE_LOG(LogZoogi, Log, TEXT("EARTH tremor %s warned at (%.0f, %.0f)"), E.Name, P.X, P.Y);
			}
		}
		for (int32 i = S.Warns.Num() - 1; i >= 0; --i)
		{
			FWarn& Wv = S.Warns[i];
			if (!Wv.bFired && Now(G) >= Wv.FireAt)
			{
				Wv.bFired = true;
				FEv E{ *Wv.Name, Wv.V, EK::Tremor, false };
				Erupt(G, E, Wv.P, 1.2f, Wv.Rad);
				if (!G->ShotDir.IsEmpty()) { static int32 Nt = 0; if (Nt++ < 3) G->TakeShotDelayed(FString::Printf(TEXT("earth_tremor_%d"), Nt), 0.4f); }
			}
			if (Now(G) > Wv.FireAt + 1.0) S.Warns.RemoveAt(i);
		}
	}

	void AddSolid(AZoogiGameMode* G, const FVector& Base, float Yaw, float HalfLen, float HalfThick, float H, float Life, AZoogiFabFX* Fx)
	{
		if (!G || !G->GetWorld()) return;
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSkillRockSolid"));
		if (bOff) return;
		UWorld* W = G->GetWorld();
		FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Base), Sp);
		if (!A) return;
		A->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* Col = A->GetStaticMeshComponent();
		const bool bWall = HalfThick > 0.f;
		Col->SetStaticMesh(ZoogiUtil::BasicShape(bWall ? TEXT("Cube") : TEXT("Cylinder")));
		static const bool bShow = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowEarthCol"));
		Col->SetVisibility(bShow); Col->SetHiddenInGame(!bShow);
		if (bShow) Col->SetMaterial(0, ZoogiUtil::GlowMID(A, FLinearColor(1.f, 0.2f, 0.6f), 0.6f));
		Col->SetCollisionProfileName(TEXT("BlockAll"));
		Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Col->SetCollisionEnabled(ECollisionEnabled::NoCollision); // armed once nothing stands inside it
		Col->SetWorldScale3D(bWall ? FVector(HalfLen / 50.f, HalfThick / 50.f, H / 100.f) : FVector(HalfLen / 50.f, HalfLen / 50.f, H / 100.f));
		A->SetActorLocation(Base + FVector(0.f, 0.f, H * 0.5f));
		if (UPhysicalMaterial* Bm = Bouncy()) Col->SetPhysMaterialOverride(Bm);
		FSolid Sd; Sd.A = A; Sd.Fx = Fx; Sd.bHadFx = Fx != nullptr; Sd.Base = Base; Sd.Yaw = Yaw; Sd.HalfLen = HalfLen; Sd.HalfThick = HalfThick; Sd.H = H;
		Sd.MadeAt = FPlatformTime::Seconds(); Sd.ArmAt = Sd.MadeAt + 0.25; Sd.Until = Sd.MadeAt + FMath::Max(Life, 4.5f); // b244: arms faster, lives its whole time
		S.Solids.Add(Sd);
		UE_LOG(LogZoogi, Log, TEXT("EARTH skill solid %s at (%.0f, %.0f) yaw %.0f %.0f x %.0f h%.0f, up to %.1f s"), bWall ? TEXT("wall") : TEXT("post"), Base.X, Base.Y, Yaw, HalfLen * 2.f, bWall ? HalfThick * 2.f : HalfLen * 2.f, H, Life);
	}

	namespace
	{
		// b213 test (-ZoogiEarthSlamTest): a skill rock wall + a court-shift rock go up, then two rivals are fired into them
		void SlamTest(AZoogiGameMode* G)
		{
			static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEarthSlamTest"));
			if (!bOn || !G || !G->Arena) return;
			static int32 Step = 0; static double T0 = 0.0; static TWeakObjectPtr<AZoogiGameMode> ForG;
			static FVector WallP, WallN;
			if (ForG.Get() != G) { ForG = G; Step = 0; }
			const double Tn = FPlatformTime::Seconds();
			TArray<AZoogiBall*> In; for (AZoogiBall* B : G->Balls) if (B && !B->bOut) In.Add(B);
			if (Step == 0 && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 1.0f && In.Num() >= 2)
			{
				Step = 1; T0 = Tn;
				const FVector Ctr = G->Arena->GetActorLocation();
				AZoogiBall* V = In[1];
				const FVector L = V->BallLocation();
				WallN = FVector(Ctr.X - L.X, Ctr.Y - L.Y, 0.f).GetSafeNormal(); if (WallN.IsNearlyZero()) WallN = FVector(1.f, 0.f, 0.f);
				WallP = FVector(L.X, L.Y, L.Z - ZoogiConst::ZoogiRadius) + WallN * 420.f;
				const float Yaw = WallN.Rotation().Yaw + 90.f; // the wall runs across the line
				AZoogiFabFX* Fx = AZoogiFabFX::Play(G->GetWorld(), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V37.NS_Earthbender_V37"), WallP, 0.9f, 60.f, nullptr, FVector::ZeroVector, FRotator(0.f, Yaw + 90.f, 0.f));
				if (Fx) { Fx->FreezeAt = 1.1f; Fx->DieAt = Fx->BornAt + 1.e6; }
				AddSolid(G, WallP, Yaw, 260.f, 55.f, 200.f, 60.f, Fx);
				if (In.Num() >= 3) DoShift(G, false);
				if (!G->ShotDir.IsEmpty()) G->TakeShotDelayed(TEXT("slam_0_walls"), 1.6f);
				UE_LOG(LogZoogi, Log, TEXT("EARTH slam test: wall at (%.0f, %.0f)"), WallP.X, WallP.Y);
			}
			auto Fire = [&](AZoogiBall* B, const FVector& Target)
			{
				if (!B || !B->Sphere) return;
				const FVector D = FVector(Target.X - B->BallLocation().X, Target.Y - B->BallLocation().Y, 0.f).GetSafeNormal();
				B->Sphere->SetSimulatePhysics(true); B->Sphere->WakeAllRigidBodies();
				B->Sphere->SetPhysicsLinearVelocity(D * 1500.f);
				B->LastHitPlayer = 0; // as if P1 smashed it
				UE_LOG(LogZoogi, Log, TEXT("EARTH slam test: %s fired at (%.0f, %.0f)"), *B->GetName(), Target.X, Target.Y);
			};
			if (Step == 1 && Tn - T0 > 2.5)
			{
				Step = 2; T0 = Tn;
				if (In.Num() >= 2) Fire(In[1], WallP);
			}
			if (Step == 2 && Tn - T0 > 2.5)
			{
				Step = 3; T0 = Tn;
				if (In.Num() >= 3 && S.RockAt.Num()) Fire(In[2], S.RockAt.Last());
			}
			// b215 (-ZoogiImpactFrameTour): the 10 impact-frame options, one per slam, a side camera on the wall, a film clip each
			static const bool bTour = FParse::Param(FCommandLine::Get(), TEXT("ZoogiImpactFrameTour")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiRockBlastTour"));
			static const bool bBlastTour = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRockBlastTour")); // b215: the 8 blast styles
			static TWeakObjectPtr<ACameraActor> TourCam; static int32 TourK = 0;
			if (bTour && Step >= 3)
			{
				const FVector Side(-WallN.Y, WallN.X, 0.f);
				if (!TourCam.IsValid())
				{
					FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					const FVector Eye = WallP - WallN * 260.f + Side * 720.f + FVector(0, 0, 230.f);
					if (ACameraActor* Ca = G->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((WallP + FVector(0, 0, 90.f) - WallN * 160.f - Eye).Rotation(), Eye), Sp))
					{ Ca->GetCameraComponent()->SetFieldOfView(60.f); Ca->GetCameraComponent()->bConstrainAspectRatio = false; TourCam = Ca; }
				}
				if (APlayerController* Pc = G->GetWorld()->GetFirstPlayerController()) if (TourCam.IsValid() && Pc->GetViewTarget() != TourCam.Get()) Pc->SetViewTarget(TourCam.Get());
				if (TourK < (bBlastTour ? 8 : 10) && Tn - T0 > 3.4 && In.Num() >= 2)
				{
					T0 = Tn; ++TourK;
					if (bBlastTour) { GForceBlast = TourK; GForceIF = TourK == 6 ? 9 : -1; } else GForceIF = TourK;
					AZoogiBall* B = In[1];
					if (B && B->Sphere)
					{
						B->Sphere->SetSimulatePhysics(true);
						B->Sphere->SetWorldLocation(WallP - WallN * 520.f + FVector(0, 0, ZoogiConst::ZoogiRadius + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
						B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
					}
					ZoogiFilm::Moment(TEXT("Rock"));
					Fire(B, WallP);
					UE_LOG(LogZoogi, Log, TEXT("EARTH impact frame tour: option %d"), TourK);
				}
			}
		}
	}

	void BlastStyleAt(AZoogiGameMode* G, const FVector& P, const FVector& Out, float Vn, int32 Style)
	{	// b215: one rock-face blast in a given style (the options reel) - visuals + sound, no throw
		const int32 PrevB = GForceBlast, PrevF = GForceIF;
		GForceBlast = FMath::Clamp(Style, 1, 8); GForceIF = Style == 6 ? 9 : -1;
		static TWeakObjectPtr<AActor> Dummy;
		if (!Dummy.IsValid() && G && G->GetWorld()) { FActorSpawnParameters Sp; Dummy = G->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Sp); }
		SideBlast(G, Dummy.Get(), nullptr, P, Out, Vn, true);
		GForceBlast = PrevB; GForceIF = PrevF;
	}

	void BlastAt(AZoogiGameMode* G, const FVector& P, float Radius, int32 Credit)
	{	// a skill / power blast breaks every rock it reaches
		if (!G || S.G.Get() != G) return;
		for (int32 i = S.Rocks.Num() - 1; i >= 0; --i)
			if (S.RockInfo.IsValidIndex(i) && FVector::Dist2D(S.RockAt[i], P) < Radius + S.RockInfo[i].Rad)
				BreakRock(G, i, P, S.RockAt[i] - P, 1400.f, Credit);
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!G || !C || !PC || !S.Warns.Num() || S.G.Get() != G) return;
		const double Nw = Now(G), Nr = FPlatformTime::Seconds();
		const float U = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
		const FLinearColor Col = SetFor(G).Col;
		for (const FWarn& Wv : S.Warns)
		{
			if (Wv.bFired) continue;
			const float K = FMath::Clamp((float)((Nw - Wv.T0) / (Wv.FireAt - Wv.T0)), 0.f, 1.f);
			const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin((float)Nr * 14.f));
			auto Ring = [&](float Rr, float Th, const FLinearColor& Cl)
			{
				FVector2D Prev; bool bHave = false;
				for (int32 k = 0; k <= 40; ++k)
				{
					const float A = 2.f * PI * k / 40.f; FVector2D Sp;
					if (PC->ProjectWorldLocationToScreen(Wv.P + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f), Sp, true))
					{ if (bHave) { FCanvasLineItem L(Prev, Sp); L.LineThickness = Th; L.SetColor(Cl); C->DrawItem(L); } Prev = Sp; bHave = true; }
					else bHave = false;
				}
			};
			Ring(Wv.Rad + 16.f, 15.f * U, FLinearColor(0.02f, 0.02f, 0.03f, 0.9f));
			Ring(Wv.Rad, 8.f * U, FLinearColor(Col.R, Col.G, Col.B, Pulse));
			Ring(Wv.Rad * (1.f - K) + 20.f, 5.f * U, FLinearColor(1.f, 0.85f, 0.2f, 0.95f));
			FVector2D Sp;
			if (PC->ProjectWorldLocationToScreen(Wv.P + FVector(0, 0, 40.f), Sp, true))
			{
				ZoogiFunCanvasText(C, TEXT("!"), Sp.X, Sp.Y - 30.f * U, 60.f * U, FLinearColor(1.f, 0.6f, 0.1f), 1, 0);
				ZoogiFunCanvasText(C, TEXT("TREMOR!"), Sp.X, Sp.Y + 26.f * U, 26.f * U, FLinearColor::White, 1, 0);
			}
		}
	}
}
