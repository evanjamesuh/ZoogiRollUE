// b136: the cosmos boss watcher (see ZoogiCosmosBoss.h).
#include "ZoogiCosmosBoss.h"
#include "ZoogiVoyager.h" // b233
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "ZoogiEpic.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "ZoogiPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Containers/Ticker.h"
#include "ZoogiFrozenFun.h" // b193: the snowman
#include "ZoogiActionFun.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArtifacts.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiDragonBoss.h"
#include "ZoogiGolemBoss.h"
#include "ZoogiBossAttack.h"
#include "ZoogiRaid.h"
#include "ZoogiTrickShot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "ZoogiStory.h"
#include "ZoogiFilm.h" // b214
#include "Engine/StaticMesh.h"
#include "Components/PoseableMeshComponent.h"

namespace ZoogiCosmosBoss
{
	namespace
	{
		TWeakObjectPtr<UPrimitiveComponent> GTitanSM; // b216: the Tripo titan riding the hidden puppet (Meadow / Dark Realm); b217: or the rigged one
		// b217 (Evan: "build animation and a skeleton for golem, no exceptions, I want fluent movements"): the Meadow golem is rigged (Tripo auto
		// rig, UE5 Mannequin skeleton) with 9 Tripo animations. It idles, looks around now and then, and acts on every boss beat:
		// slam = chop, sweep = slash, hit = hit_to_body_01, taunt / roar = angry_01, a KO = cheer, beaten = defeat (held). -ZoogiStaticGolem = old.
		// Fluent: a hidden driver plays the clips; the visible poseable copy CROSSFADES from the last pose into each new clip (0.3 s), so
		// nothing snaps. A hit never cuts off a chop / slash / roar, and the same clip is not restarted while it still has most of its run.
		struct FTA { TWeakObjectPtr<USkeletalMeshComponent> Sk; TWeakObjectPtr<UPoseableMeshComponent> Po; TArray<FTransform> From; float BlendT = 1.f, BlendLen = 0.3f;
			FString Cur; float OneLeft = 0.f, OneLen = 0.f, IdleT = 0.f, NextLook = 9.f; bool bDead = false; };
		FTA GTA;
		bool GSpiderRig = false; // b233: the Tomb titan is Voyager's Greater Spider (same rig path, its own clips)
		UAnimSequence* GolemAnim(const TCHAR* N)
		{
			static TMap<FString, TWeakObjectPtr<UAnimSequence>> Cache;
			const FString Key = GSpiderRig ? FString(TEXT("spider_")) + N : FString(N);
			if (TWeakObjectPtr<UAnimSequence>* C = Cache.Find(Key)) if (C->IsValid()) return C->Get();
			UAnimSequence* Sq = nullptr;
			if (GSpiderRig)
			{
				static const TCHAR* Map[][2] = { { TEXT("idle"), TEXT("Idle") }, { TEXT("look_around"), TEXT("Walk_Left") }, { TEXT("hit_to_body_01"), TEXT("React_Heavy_Front") },
					{ TEXT("slash"), TEXT("Attack_Melee_B") }, { TEXT("chop"), TEXT("Attack_Melee") }, { TEXT("angry_01"), TEXT("Cast_Roar") }, { TEXT("cheer"), TEXT("Cast_Roar") }, { TEXT("defeat"), TEXT("Death") } };
				const TCHAR* Sp = TEXT("Idle"); for (const auto& Mp : Map) if (FCString::Strcmp(Mp[0], N) == 0) Sp = Mp[1];
				Sq = ZoogiVoyager::Anim(*FString::Printf(TEXT("Spider/Anim_ExoGame_Greater_Spider_%s"), Sp));
				Cache.Add(Key, Sq); return Sq;
			}
			Sq = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/MeadowGolemRig/AS_Golem_%s.AS_Golem_%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Sq) UE_LOG(LogZoogi, Warning, TEXT("GOLEM RIG: no anim %s"), N);
			Cache.Add(Key, Sq); return Sq;
		}
		void GolemFadeFrom(float Len)
		{	// snapshot the pose on screen; the poseable blends from it into whatever the driver plays next
			USkeletalMeshComponent* Sk = GTA.Sk.Get(); if (!Sk) return;
			GTA.From = Sk->GetComponentSpaceTransforms(); GTA.BlendT = 0.f; GTA.BlendLen = Len;
		}
		void GolemLoop(const TCHAR* N)
		{
			USkeletalMeshComponent* Sk = GTA.Sk.Get(); if (!Sk || GTA.bDead) return;
			GTA.OneLeft = 0.f;
			if (GTA.Cur == N) return;
			if (UAnimSequence* Sq = GolemAnim(N)) { GolemFadeFrom(0.35f); Sk->PlayAnimation(Sq, true); Sk->SetPlayRate(1.f); GTA.Cur = N; }
		}
		void GolemOnce(const TCHAR* N, float Rate = 1.f, bool bHold = false)
		{
			USkeletalMeshComponent* Sk = GTA.Sk.Get(); if (!Sk || GTA.bDead) return;
			const bool bHit = FCString::Strcmp(N, TEXT("hit_to_body_01")) == 0;
			if (!bHold && GTA.OneLeft > 0.f)
			{
				if (bHit && GTA.Cur != TEXT("hit_to_body_01") && GTA.Cur != TEXT("look_around")) return; // don't cut an attack / roar with a flinch
				if (GTA.Cur == N && GTA.OneLeft > GTA.OneLen * 0.4f) return; // already mid-way through this one
			}
			if (UAnimSequence* Sq = GolemAnim(N))
			{
				GolemFadeFrom(bHold ? 0.4f : bHit ? 0.15f : 0.25f);
				Sk->PlayAnimation(Sq, false); Sk->SetPlayRate(Rate); GTA.Cur = N;
				GTA.OneLeft = bHold ? 1.e6f : Sq->GetPlayLength() / FMath::Max(0.1f, Rate) - 0.3f; // hand back to idle just before the end, under a crossfade
				GTA.OneLen = GTA.OneLeft;
				if (bHold) GTA.bDead = true;
				UE_LOG(LogZoogi, Log, TEXT("GOLEM RIG: %s (%.1f s)"), N, Sq->GetPlayLength());
			}
		}
		void GolemTick(float Dt)
		{
			USkeletalMeshComponent* Sk = GTA.Sk.Get(); if (!Sk) return;
			if (UPoseableMeshComponent* Po = GTA.Po.Get())
			{	// copy the driver's pose onto the visible golem, crossfading out of the previous clip
				const TArray<FTransform>& To = Sk->GetComponentSpaceTransforms();
				if (GTA.BlendT < GTA.BlendLen && GTA.From.Num() == To.Num() && To.Num() > 0)
				{
					GTA.BlendT += Dt;
					const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(GTA.BlendT / FMath::Max(0.01f, GTA.BlendLen), 0.f, 1.f));
					for (int32 Bi = 0; Bi < To.Num(); ++Bi)
					{
						FTransform Tb; Tb.Blend(GTA.From[Bi], To[Bi], A);
						Po->SetBoneTransformByName(Po->GetBoneName(Bi), Tb, EBoneSpaces::ComponentSpace);
					}
				}
				else Po->CopyPoseFromSkeletalComponent(Sk);
			}
			if (GTA.bDead) return;
			if (GTA.OneLeft > 0.f) { GTA.OneLeft -= Dt; if (GTA.OneLeft <= 0.f) { Sk->SetPlayRate(1.f); GTA.Cur.Reset(); GolemLoop(TEXT("idle")); } return; }
			if ((GTA.IdleT += Dt) >= GTA.NextLook) { GTA.IdleT = 0.f; GTA.NextLook = FMath::FRandRange(8.f, 14.f); GolemOnce(TEXT("look_around")); }
		}
		struct FBoss
		{
			TWeakObjectPtr<AActor> A;
			TWeakObjectPtr<USkeletalMeshComponent> Mesh;
			TWeakObjectPtr<UMaterialInstanceDynamic> MID;
			TWeakObjectPtr<AZoogiGameMode> G;
			float Angle = 0.f, Yaw = 0.f, Lean = 0.f, LeanTo = 0.f, Rise = 0.f, RiseTo = 0.f;
			float Eye = 1.f, EyeBase = 1.f, ReactLeft = 0.f, Flicker = 0.f, Cool = 0.f, T = 0.f;
			float H = 11000.f, Dist = 15500.f;
			int32 NoProj = 0, OffScr = 0, Hid = 0, Vxl = 0, Vyl = 0; float FixedAngle = 0.f, LogT = 0.f, Search = 0.f, WantAngle = 0.f, HeadZ = 0.f, WantHeadZ = 0.f;
			bool bSeated = false, bFinal = false;
			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> CamPrev; float CamLeft = 0.f, CamT = 0.f, CamWant = 0.f, CamWait = 0.f; // b136 v6: the boss cam
			// b142 peek: the puppet on the rim
			TWeakObjectPtr<AZoogiAurexPuppet> Pup; float PeekAng = 0.f, WantPeek = 0.f, Dip = 1.f, Slam = 0.f, RoarLeft = 0.f, PeekLog = 0.f; bool bMoving = false; int32 PeekTurn = -1;
		};
		FBoss B;
		bool GSweepOn = false; float GSweepAng = 0.f; // b146: the hand sweep drives the peek angle
		bool GHold = false; float GHoldAng = 0.f; bool GDefeated = false; // b151 RAID: the boss stays where the raid puts it / sinks when beaten
		FTSTicker::FDelegateHandle Tick;
		FLinearColor EyeRed(1.f, 0.12f, 0.08f); // b148a: the map boss's eye colour (set by Spawn)
		float GCameo = 0.f; bool GWasShown = false; // b155a: a short appearance after a KO / combo
		TArray<TWeakObjectPtr<USphereComponent>> GCol; // b157: the titan is solid (bank off it)
		void BossCollision(AZoogiAurexPuppet* P, bool bOn)
		{	// b157 (Evan: "you can bank off the map boss never phase threw it")
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossNoCollide"));
			if (!P || !P->Body) return;
			struct FB { const TCHAR* Bone; float R; };
			static const FB Bs[] = { { TEXT("hand_l"), 130.f }, { TEXT("hand_r"), 130.f }, { TEXT("lowerarm_l"), 105.f }, { TEXT("lowerarm_r"), 105.f },
				{ TEXT("upperarm_l"), 120.f }, { TEXT("upperarm_r"), 120.f }, { TEXT("head"), 210.f } };
			if (GCol.Num() != (int32)UE_ARRAY_COUNT(Bs) || !GCol[0].IsValid() || GCol[0]->GetOwner() != P)
			{
				for (TWeakObjectPtr<USphereComponent>& Old : GCol) if (USphereComponent* Os = Old.Get()) Os->DestroyComponent();
				GCol.Reset();
				for (const FB& Bb : Bs)
				{
					USphereComponent* Sc = NewObject<USphereComponent>(P);
					Sc->SetSphereRadius(Bb.R);
					Sc->SetCollisionObjectType(ECC_WorldDynamic);
					Sc->SetCollisionResponseToAllChannels(ECR_Block);
					Sc->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
					Sc->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
					Sc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Sc->SetHiddenInGame(true);
					Sc->RegisterComponent();
					GCol.Add(Sc);
				}
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: %d solid colliders (hands, arms, head)"), GCol.Num());
			}
			const bool bWant = bOn && !bOff;
			for (int32 i = 0; i < GCol.Num(); ++i)
			{
				USphereComponent* Sc = GCol[i].Get(); if (!Sc) continue;
				if (bWant) Sc->SetWorldLocation(P->Body->GetBoneLocationByName(Bs[i].Bone, EBoneSpaces::WorldSpace), false, nullptr, ETeleportType::TeleportPhysics);
				const ECollisionEnabled::Type Want = bWant ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
				if (Sc->GetCollisionEnabled() != Want) Sc->SetCollisionEnabled(Want);
			}
		}
		bool Shown(const AZoogiGameMode* G)
		{	// b155a (Evan): out of sight during the arena-forming shot; RAID = always after it; CLASSIC = attack rounds + cameos
			static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossAlways"));
			if (!G) return false;
			if (G->Arena && G->Arena->IsForming()) return false;
			if (bAlways || ZoogiRaid::Active()) return true;
			return ZoogiBossAttack::Engaged(G) || GCameo > 0.f;
		}
		// b148a: one boss per map - the bought Mecha Aurex meshes, tailored (placeholders until the real bosses)
		struct FTheme { const TCHAR* Name; const TCHAR* Mesh; FLinearColor R, G, B, Eye, Tint; };
		FTheme GTheme = { TEXT("VOID TITAN"), TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.22f, 0.07f, 0.42f), FLinearColor(0.04f, 0.02f, 0.08f), FLinearColor(0.45f, 0.12f, 0.7f), FLinearColor(1.f, 0.12f, 0.08f), FLinearColor(0.6f, 0.25f, 1.f) };
		FTheme ThemeFor(EZoogiArena A)
		{	// armour (R zone), joints (G zone), trim (B zone), eyes, the puppet / FX tint
			switch (A)
			{
			case EZoogiArena::Meadow:   return { TEXT("GROVE TITAN"),    TEXT("SK_Mecha_Aurex"),    FLinearColor(0.10f, 0.32f, 0.08f), FLinearColor(0.12f, 0.07f, 0.03f), FLinearColor(0.55f, 0.42f, 0.12f), FLinearColor(1.f, 0.7f, 0.1f),   FLinearColor(0.45f, 0.9f, 0.3f) };
			case EZoogiArena::Frozen:   return { TEXT("FROST TITAN"),    TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.75f, 0.88f, 1.0f),  FLinearColor(0.08f, 0.16f, 0.3f), FLinearColor(0.2f, 0.6f, 1.0f),   FLinearColor(0.2f, 0.9f, 1.f),   FLinearColor(0.5f, 0.85f, 1.f) };
			case EZoogiArena::Volcanic: return { TEXT("MAGMA TITAN"),    TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.07f, 0.05f, 0.04f), FLinearColor(0.02f, 0.01f, 0.01f), FLinearColor(1.0f, 0.32f, 0.04f), FLinearColor(1.f, 0.45f, 0.05f), FLinearColor(1.f, 0.4f, 0.1f) };
			case EZoogiArena::Arabian:  return { TEXT("DJINN TITAN"),    TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.8f, 0.6f, 0.18f),  FLinearColor(0.05f, 0.18f, 0.22f), FLinearColor(0.1f, 0.75f, 0.7f), FLinearColor(0.15f, 1.f, 0.85f), FLinearColor(0.2f, 0.9f, 0.8f) };
			case EZoogiArena::Tomb:     return { (FParse::Param(FCommandLine::Get(), TEXT("ZoogiTombSpider")) /* b236: Evan - no spider boss */) ? TEXT("TOMB SPIDER") : TEXT("PHARAOH TITAN") /* b233 */,  TEXT("SK_Mecha_Aurex"),    FLinearColor(0.62f, 0.5f, 0.3f),  FLinearColor(0.1f, 0.07f, 0.04f), FLinearColor(0.9f, 0.7f, 0.15f),  FLinearColor(0.3f, 1.f, 0.35f),  FLinearColor(0.95f, 0.8f, 0.3f) };
			case EZoogiArena::Neon:     return { TEXT("NEON TITAN"),     TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.03f, 0.03f, 0.05f), FLinearColor(0.01f, 0.01f, 0.02f), FLinearColor(1.0f, 0.1f, 0.6f),  FLinearColor(0.1f, 0.95f, 1.f),  FLinearColor(1.f, 0.2f, 0.75f) };
			case EZoogiArena::Teaser:   return { TEXT("STAR TITAN"),     TEXT("SK_Mecha_Aurex"),    FLinearColor(0.9f, 0.75f, 0.2f),  FLinearColor(0.1f, 0.1f, 0.25f),  FLinearColor(1.0f, 1.0f, 0.9f),   FLinearColor(1.f, 0.95f, 0.5f),  FLinearColor(1.f, 0.9f, 0.4f) };
			case EZoogiArena::Training: return { TEXT("TRAINING TITAN"), TEXT("SK_Mecha_Aurex"),    FLinearColor(0.35f, 0.38f, 0.42f), FLinearColor(0.05f, 0.05f, 0.06f), FLinearColor(0.2f, 0.85f, 0.4f),  FLinearColor(0.3f, 1.f, 0.4f),   FLinearColor(0.4f, 1.f, 0.5f) };
			default:                    return { TEXT("VOID TITAN"),     TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.22f, 0.07f, 0.42f), FLinearColor(0.04f, 0.02f, 0.08f), FLinearColor(0.45f, 0.12f, 0.7f), FLinearColor(1.f, 0.12f, 0.08f), FLinearColor(0.6f, 0.25f, 1.f) };
			}
		}
		bool ArenaHasBoss(const AZoogiGameMode* G)
		{
			if (!G || !G->Arena || G->bRoyaleMode || G->bComboMode || G->bSoloTraining) return false;
			const EZoogiArena A = G->Arena->GetTheme();
			if (A == EZoogiArena::Ancient || ZoogiTrickShot::Active()) return false; // b156: no titan in the trick shot
			if (A == EZoogiArena::Training && ZoogiStory::InMission()) return false; // b163: the EP1 practice spar is just two friends
			static const bool bCosmicOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCosmicOnly"));
			return A == EZoogiArena::Cosmic || !bCosmicOnly || FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmosBossAll"));
		}

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCosmosBoss")); return b; }
		bool Titan() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossTitan")); return b; } // b142: the b136 far titan
		constexpr float PeekH = 1200.f; // b142j: 12 m - hands on the bowl wall top, the head just above it in the gameplay camera
		// b142j: the top of the wall / stands beyond the court edge on a heading: (distance from the centre, height); cached per heading
		FVector2D RimTop(AZoogiGameMode* G, float Yaw, float ER)
		{
			static float LastYaw = -999.f; static FVector2D Last(0.f, 0.f); static TWeakObjectPtr<AZoogiGameMode> LastG;
			if (LastG.Get() == G && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, LastYaw)) < 1.5f) return Last;
			LastG = G; LastYaw = Yaw;
			const FVector C = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBossRim), true);
			for (AZoogiBall* Zb : G->Balls) if (Zb) Q.AddIgnoredActor(Zb);
			if (AActor* Pa = B.A.Get()) Q.AddIgnoredActor(Pa);
			float BestZ = C.Z, BestR = ER;
			TArray<FVector2D> Hits;
			for (float Rr = ER + 20.f; Rr <= ER + 1500.f; Rr += 40.f)
			{
				FHitResult H; const FVector P = C + Dir * Rr;
				if (G->GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 4000.f), P - FVector(0.f, 0.f, 600.f), ECC_Camera, Q)) Hits.Add(FVector2D(Rr, H.ImpactPoint.Z));
			}
			for (const FVector2D& Hh : Hits) if (Hh.Y > BestZ + 1.f && Hh.Y < C.Z + 2500.f) BestZ = Hh.Y;
			for (const FVector2D& Hh : Hits) if (Hh.Y >= BestZ - 40.f) { BestR = Hh.X; break; } // the inner edge of the top
			if (BestZ < C.Z + 60.f && G->Arena)
			{	// fallback: the R2 bowl wall's bounds
				TInlineComponentArray<UStaticMeshComponent*> Sms(G->Arena);
				for (UStaticMeshComponent* Sm : Sms) if (Sm && Sm->GetStaticMesh() && Sm->GetStaticMesh()->GetName().Contains(TEXT("BowlRing"))) { BestZ = FMath::Max(BestZ, (float)Sm->Bounds.GetBox().Max.Z); BestR = ER + 250.f; }
			}
			if (BestZ < C.Z + 60.f) { BestZ = C.Z; BestR = ER; } // nothing stands there: the court lip
			Last = FVector2D(BestR, BestZ);
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS rim at yaw %.0f: wall top r%.0f z%.0f (court edge r%.0f, %d hits)"), Yaw, BestR, BestZ, ER, Hits.Num());
			return Last;
		}
		UAnimSequence* Anim(const TCHAR* Path) { return LoadObject<UAnimSequence>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }
		// b136 fix: looked up when needed (a raw pointer kept in this static went stale after GC -> crash)
		UAnimSequence* IdleAnim() { return Anim(TEXT("/Game/Mecha_Aurex/Anims/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop")); }

		void EndBossCam(float Blend);
		void Clear()
		{
			if (B.Cam.IsValid()) EndBossCam(0.f);
			if (AActor* A = B.A.Get()) A->Destroy();
			if (Tick.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Tick); Tick.Reset(); }
			B = FBoss();
		}

		FVector Centre(AZoogiGameMode* G) { return G && G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; }

		void SetEyes(float K)
		{
			if (UMaterialInstanceDynamic* M = B.MID.Get()) M->SetVectorParameterValue(TEXT("Emissive_Color"), EyeRed * K);
		}

		void Play(UAnimSequence* S, float Rate = 0.8f)
		{
			USkeletalMeshComponent* Mc = B.Mesh.Get();
			if (!Mc || !S) return;
			Mc->PlayAnimation(S, false);
			Mc->SetPlayRate(Rate);
			B.ReactLeft = S->GetPlayLength() / FMath::Max(0.1f, Rate);
		}

		// b136 v6: the gameplay cameras look down at the court and almost never see the sky, so the boss gets its moments: a short
		// cut to a low camera on the arena rim looking up at it (match start, knockouts, final round), then back. -ZoogiNoBossCam = off.
		void BossCam(float Dur)
		{
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBossCam"));
			if (bOff || B.CamLeft > 0.f) return;
			// b193 (Evan: "never during a roll - it kills my momentum"): no boss cut-away while a shot is rolling (a request then is dropped)
			if (const AZoogiGameMode* Gr = B.G.Get()) if (Gr->Phase == EZoogiPhase::Rolling && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCamInRoll"))) return;
			B.CamWant = FMath::Max(B.CamWant, Dur);
		}
		void EndBossCam(float Blend)
		{
			AZoogiGameMode* G = B.G.Get();
			APlayerController* Pc = G && G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr;
			if (Pc && B.Cam.IsValid() && Pc->GetViewTarget() == B.Cam.Get()) Pc->SetViewTargetWithBlend(B.CamPrev.IsValid() ? B.CamPrev.Get() : Pc->GetPawn(), Blend, VTBlend_Cubic);
			if (ACameraActor* Cm = B.Cam.Get()) Cm->SetLifeSpan(Blend + 0.5f);
			B.Cam.Reset(); B.CamPrev.Reset(); B.CamLeft = 0.f;
		}

		bool PeekStep(AZoogiGameMode* G, float Dt)
		{
			AZoogiAurexPuppet* P = B.Pup.Get();
			UWorld* W = G->GetWorld();
			const FVector C = Centre(G);
			const float ER = G->Arena->GetKnockoffRadius();
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			const AZoogiBall* Act = G->GetActiveBall();
			const bool bHumanAim = Act && !Act->bCPU && G->Phase == EZoogiPhase::Aiming;
			FVector CamL = C + FVector(0.f, 0.f, 1500.f);
			if (Pc && Pc->PlayerCameraManager)
			{
				CamL = Pc->PlayerCameraManager->GetCameraLocation();
				if (bHumanAim)
				{	// in front of the player: where the camera's forward ray meets the far rim
					const FVector Fw = Pc->PlayerCameraManager->GetCameraRotation().Vector();
					FVector2D O(CamL.X - C.X, CamL.Y - C.Y), D(Fw.X, Fw.Y);
					if (D.Normalize())
					{
						const float Bq = FVector2D::DotProduct(O, D), Cq = O.SizeSquared() - ER * ER, Disc = Bq * Bq - Cq;
						if (Disc > 0.f) { const FVector2D Hp = O + D * (-Bq + FMath::Sqrt(Disc)); B.WantPeek = FMath::RadiansToDegrees(FMath::Atan2(Hp.Y, Hp.X)); }
					}
					if (G->TurnCounter != B.PeekTurn) { B.PeekTurn = G->TurnCounter; if (FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek)) > 20.f) B.bMoving = true; }
				}
			}
			if (GHold && !GSweepOn) B.WantPeek = GHoldAng; // b151 RAID: it holds its spot (moves when the raid says)
			if (GSweepOn) { B.PeekAng = B.WantPeek = GSweepAng; B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 5.f); } // b146
			const float Dd = FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek));
			if (!B.bMoving && Dd > 40.f) B.bMoving = true; // the aim turned a lot: duck and come up in front again
			if (B.bMoving)
			{	// ducks under the rim, moves, comes back up
				B.Dip = FMath::FInterpConstantTo(B.Dip, 1.f, Dt, 2.4f);
				if (B.Dip >= 0.99f) { B.PeekAng = B.WantPeek; B.bMoving = false; UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peek: comes up at yaw %.0f (in front of the camera)"), B.PeekAng); }
			}
			else
			{
				B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 0.9f);
				B.PeekAng += FMath::Clamp(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek), -30.f * Dt, 30.f * Dt); // slides along the rim
			}
			const FVector Dir = FRotator(0.f, B.PeekAng, 0.f).Vector();
			B.Slam = FMath::Max(0.f, B.Slam - Dt * 1.6f);
			B.Lean = FMath::FInterpTo(B.Lean, B.LeanTo, Dt, 1.6f);
			if (B.LeanTo > 0.f && (B.Cool <= 1.5f)) B.LeanTo = 0.f;
			if (B.RoarLeft > 0.f) { B.RoarLeft -= Dt; P->Pose = EZoogiPuppetPose::Cheer; P->bPinHands = false; }
			else { P->Pose = EZoogiPuppetPose::Peek; P->bPinHands = true; }
			const float Slam01 = B.Slam > 0.5f ? (1.f - B.Slam) * 2.f : B.Slam * 2.f; // up, then down on the rim
			P->Pull = Slam01;
			P->Lean = 0.35f + B.Lean / 25.f; // b142j: well over the edge
			P->FaceDir = -Dir;
			GCameo = FMath::Max(0.f, GCameo - Dt);
			const bool bShow = GSweepOn || Shown(G);
			if (!GDefeated && !bShow) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.8f); B.CamWant = 0.f; B.RoarLeft = 0.f; } // b155a: below the rim, out of sight
			else if (!GDefeated && !GWasShown && B.Dip > 0.9f) { B.RoarLeft = 1.8f; B.Eye = 8.f; UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS shows itself (%s)"), ZoogiRaid::Active() ? TEXT("raid") : (GCameo > 0.f ? TEXT("cameo") : TEXT("attack round"))); }
			if (!GDefeated && bShow && B.Dip > 0.3f && !B.bMoving) B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 1.6f); // rises quickly into view
			GWasShown = bShow;
			if (GDefeated) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.35f); B.EyeBase = 0.f; B.Eye = FMath::Min(B.Eye, 0.3f); } // b151: beaten - sinks below the rim, eyes out
			const FVector2D Rt = RimTop(G, B.PeekAng, ER); // b142j: over the bowl wall, not behind it
			P->HandPin = FVector(C.X, C.Y, 0.f) + Dir * (Rt.X + 15.f) + FVector(0.f, 0.f, Rt.Y + 10.f - B.Dip * PeekH * 0.85f + B.Rise * 30.f);
			if (GSweepOn) { P->HandPin = FVector(C.X, C.Y, C.Z) + Dir * (ER * 0.8f) + FVector(0.f, 0.f, 70.f); P->Lean = 1.2f; P->Pose = EZoogiPuppetPose::Peek; P->bPinHands = true; } // b146: both hands reach in and drag across the court
			P->bLookAt = true;
			FVector Look = CamL;
			if (!bHumanAim && Act && G->Phase == EZoogiPhase::Rolling) Look = Act->BallLocation(); // eyes on a CPU / rolling shot
			P->LookAt = Look;
			if (ZoogiDragon::Active()) ZoogiDragon::Tick(G, Dt, C, ER, Rt.X, Rt.Y, B.PeekAng, bShow, GDefeated, Look); // b177a
			if (ZoogiGolem::Active()) ZoogiGolem::Tick(G, Dt, C, Rt.X, Rt.Y, B.PeekAng, B.Dip, GDefeated); // b177g
			GolemTick(Dt); // b217: the rigged Meadow golem's animations
			BossCollision(P, !ZoogiDragon::Active() && !ZoogiGolem::Active() && !GDefeated && !GSweepOn && B.Dip < 0.45f); // b157: solid while it is up on the rim
			B.Rise = FMath::FInterpTo(B.Rise, B.RiseTo, Dt, 0.6f);
			// eyes
			B.Eye = FMath::FInterpTo(B.Eye, B.EyeBase, Dt, 1.2f);
			float K = B.Eye * (0.85f + 0.15f * FMath::Sin(B.T * 1.7f));
			if (B.Flicker > 0.f) { B.Flicker -= Dt; K *= (FMath::Fmod(B.T * 14.f, 1.f) < 0.5f) ? 0.15f : 1.f; }
			SetEyes(K);
			B.Cool = FMath::Max(0.f, B.Cool - Dt);
			// the boss cam: from the court, low, looking up at its face
			FVector GolP, GolL; const bool bGol = ZoogiGolem::CamSpot(GolP, GolL); // b177g: the golem cam frames the whole body
			FVector TitanC; float TitanHalf = 0.f; const bool bTitan = TitanFocus(TitanC, TitanHalf); // b216: the whole Tripo titan in frame
			const FVector HeadP = ZoogiDragon::Active() ? ZoogiDragon::HeadLoc() : bGol ? GolL : bTitan ? TitanC : (P->Body ? P->Body->GetBoneLocationByName(TEXT("head"), EBoneSpaces::WorldSpace) : P->GetActorLocation());
			if (B.CamWant > 0.f && !B.Cam.IsValid() && Pc && B.Dip < 0.2f)
			{
				AActor* Vt = Pc->GetViewTarget();
				const AZoogiPlayerController* Zpc = Cast<AZoogiPlayerController>(Pc);
				if (Vt && (Zpc ? Vt == Zpc->MainCam() : !Cast<ACameraActor>(Vt)))
				{
					FVector CamP = C + Dir * (ER * 0.15f) + FVector(-Dir.Y, Dir.X, 0.f) * (ER * 0.3f) + FVector(0.f, 0.f, 230.f);
					{ FVector Lk; if (!ZoogiDragon::CamSpot(CamP, Lk)) ZoogiGolem::CamSpot(CamP, Lk); } // b177e/g: in front of the boss's face
					FActorSpawnParameters SPc; SPc.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((HeadP - CamP).Rotation(), CamP), SPc))
					{
						Cm->GetCameraComponent()->SetFieldOfView(60.f); Cm->GetCameraComponent()->bConstrainAspectRatio = false;
						B.Cam = Cm; B.CamPrev = Vt; B.CamLeft = B.CamWant; B.CamT = 0.f; B.CamWait = 0.f;
						Pc->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
						UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS cam %.1f s (peek)"), B.CamWant);
						if (!G->ShotDir.IsEmpty()) { static int32 Nbc = 0; ++Nbc; G->TakeShotDelayed(FString::Printf(TEXT("bosscam_%d_a"), Nbc), 0.7f); G->TakeShotDelayed(FString::Printf(TEXT("bosscam_%d_b"), Nbc), FMath::Min(1.6f, B.CamWant * 0.85f)); } // b177e: check the front view
					}
				}
				if (B.Cam.IsValid()) B.CamWant = 0.f;
				else if ((B.CamWait += Dt) > 15.f) { B.CamWant = 0.f; B.CamWait = 0.f; }
			}
			if (ACameraActor* Cm = B.Cam.Get())
			{
				B.CamT += Dt; B.CamLeft -= Dt; if (const AZoogiGameMode* Gx = B.G.Get()) if (Gx->Phase == EZoogiPhase::Rolling && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCamInRoll"))) B.CamLeft = FMath::Min(B.CamLeft, 0.f); // b193: a roll starts - back to play
				if (Pc && Pc->GetViewTarget() != Cm && B.CamT > 0.4f && B.CamT < 0.9f) Pc->SetViewTarget(Cm);
				Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (HeadP - Cm->GetActorLocation()).Rotation(), Dt, 6.f));
				if (bTitan)
				{	// b216: wide enough for the head to the feet (+25 %), no slow zoom-in that crops it
					const float Dc = FMath::Max(300.f, FVector::Dist(Cm->GetActorLocation(), TitanC));
					Cm->GetCameraComponent()->SetFieldOfView(FMath::Clamp(2.f * FMath::RadiansToDegrees(FMath::Atan(TitanHalf * 1.25f / Dc * 16.f / 9.f)), 45.f, 95.f));
				}
				else Cm->GetCameraComponent()->SetFieldOfView(FMath::Lerp(60.f, 42.f, FMath::Clamp(B.CamT / 2.5f, 0.f, 1.f)));
				if (B.CamLeft <= 0.f) EndBossCam(0.5f);
			}
			if ((B.PeekLog -= Dt) <= 0.f)
			{
				B.PeekLog = 3.f;
				FVector2D Sp(-1.f, -1.f); int32 Vx = 0, Vy = 0; if (Pc) { Pc->GetViewportSize(Vx, Vy); Pc->ProjectWorldLocationToScreen(HeadP, Sp, true); }
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peek yaw %.0f (want %.0f) dip %.2f head (%.0f,%.0f,%.0f) on screen (%.2f, %.2f) human aim %d"), B.PeekAng, B.WantPeek, B.Dip, HeadP.X, HeadP.Y, HeadP.Z, Vx ? Sp.X / Vx : -1.f, Vy ? Sp.Y / Vy : -1.f, bHumanAim ? 1 : 0);
			}
			return true;
		}

		bool Step(float Dt)
		{
			AZoogiGameMode* G = B.G.Get();
			AActor* A = B.A.Get();
			if (!G || !A || !IsValid(A)) { Tick.Reset(); B = FBoss(); return false; }
			Dt = FMath::Min(Dt, 0.1f);
			B.T += Dt;
			if (B.Pup.IsValid()) return PeekStep(G, Dt); // b142
			UWorld* W = G->GetWorld();
			const FVector C = Centre(G);
			// b136 v4: the gameplay cameras look down at the court, so the only open space is the void around the arena. Every 0.4 s
			// it looks for a spot (24 angles x 4 heights, 80 m out) whose head point is on screen and not behind the arena disc, and
			// glides there - the titan always peers in from the open void in frame.
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			const float ArR = 1500.f * FMath::Max(1.f, G->Arena->GetActorScale3D().X);
			B.Search -= Dt;
			if (Pc && Pc->PlayerCameraManager && B.Search <= 0.f)
			{
				B.Search = 0.4f;
				const FVector CamL = Pc->PlayerCameraManager->GetCameraLocation();
				int32 Vx = 0, Vy = 0; Pc->GetViewportSize(Vx, Vy); B.Vxl = Vx; B.Vyl = Vy; B.NoProj = B.OffScr = B.Hid = 0;
				float BestS = -1e9f; float BestA = B.Angle, BestZ = B.HeadZ;
				for (int32 ai = 0; ai < 24; ++ai)
					for (int32 hi = 0; hi < 5; ++hi)
					{
						const float Ang = ai * 15.f;
						const float Hz = C.Z + (-600.f + hi * 1500.f); // head from 6 m under the court to 39 m over it
						const FVector P = C + FRotator(0.f, Ang, 0.f).Vector() * B.Dist + FVector(0.f, 0.f, Hz - C.Z);
						FVector2D Sp;
						if (!Pc->ProjectWorldLocationToScreen(P, Sp, true) || Vx <= 0 || Vy <= 0) { ++B.NoProj; continue; }
						const float X = Sp.X / Vx, Y = Sp.Y / Vy;
						if (X < 0.06f || X > 0.94f || Y < 0.03f || Y > 0.75f) { ++B.OffScr; continue; }
						// behind the arena? (b136 v5: the court disc AND the stands ring around it, about 10 m tall)
						bool bHidden = false;
						for (int32 k = 1; k < 24 && !bHidden; ++k)
						{
							const FVector Q = CamL + (P - CamL) * (k / 24.f);
							const float Rq = FVector::Dist2D(Q, C);
							if ((Rq < ArR * 1.25f && Q.Z < C.Z + 60.f) || (Rq < ArR * 3.2f && Q.Z < C.Z + 1100.f)) bHidden = true;
						}
						if (bHidden) { ++B.Hid; continue; }
						float Sc = -FMath::Abs(Y - 0.22f) * 2.f - FMath::Abs(X - 0.5f) * 0.8f - hi * 0.05f;
						Sc -= FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, B.Angle)) / 180.f * 0.9f; // stay near where it is
						if (Sc > BestS) { BestS = Sc; BestA = Ang; BestZ = Hz; }
					}
				if (BestS > -1e8f) { B.WantAngle = BestA; B.WantHeadZ = BestZ; if (!B.bSeated) { B.Angle = BestA; B.bSeated = true; } }
			}
			B.WantAngle = B.FixedAngle; B.WantHeadZ = C.Z + 2600.f; // b136 v6: fixed, towering 26 m over the court (the boss cam shows it)
			B.Angle = FMath::FixedTurn(B.Angle, B.WantAngle, 22.f * Dt);
			B.Rise = FMath::FInterpTo(B.Rise, B.RiseTo, Dt, 0.6f);
			B.HeadZ = FMath::FInterpTo(B.HeadZ, B.WantHeadZ, Dt, 1.2f);
			const float HeadZ = B.HeadZ + B.Rise * 120.f; // B.Rise: -14 at the start (rises in), +3 in the final round
			FVector Pos = C + FRotator(0.f, B.Angle, 0.f).Vector() * B.Dist; Pos.Z = A->GetActorLocation().Z;
			if (USkeletalMeshComponent* Mc = B.Mesh.Get())
			{
				const float HeadNow = Mc->GetSocketLocation(TEXT("head")).Z;
				const float Err = HeadZ - HeadNow;
				Pos.Z += FMath::Abs(Err) > 30000.f ? Err : Err * FMath::Min(1.f, Dt * 1.8f);
			}
			// watches the leader
			FVector Look = C;
			int32 Best = -1;
			for (const AZoogiBall* Zb : G->Balls) if (Zb && !Zb->bOut && Zb->Score > Best) { Best = Zb->Score; Look = FMath::Lerp(C, Zb->GetActorLocation(), 0.8f); }
			if (const AZoogiBall* Act = G->GetActiveBall()) if (G->Phase == EZoogiPhase::Rolling) Look = FMath::Lerp(Look, Act->GetActorLocation(), 0.6f); // eyes on the rolling shot
			const float WantYaw = (Look - Pos).Rotation().Yaw - 90.f; // the Aurex rig faces +Y
			B.Yaw = FMath::FixedTurn(B.Yaw, WantYaw, 10.f * Dt);
			B.Lean = FMath::FInterpTo(B.Lean, B.LeanTo, Dt, 1.6f);
			if (B.LeanTo > 0.f && B.ReactLeft <= 0.f) B.LeanTo = 0.f;
			A->SetActorLocationAndRotation(Pos, FRotator(0.f, B.Yaw, -B.Lean)); // roll on the rig's side axis = a lean toward the arena
			if (B.CamWant > 0.f && !B.Cam.IsValid() && Pc)
			{	// cut to the boss cam (not over another special camera, e.g. the SHOWTIME sky cam)
				AActor* Vt = Pc->GetViewTarget();
				const AZoogiPlayerController* Zpc = Cast<AZoogiPlayerController>(Pc);
				if (Vt && (Zpc ? Vt == Zpc->MainCam() : !Cast<ACameraActor>(Vt))) // only from the gameplay camera (not a replay / SHOWTIME / intro camera)
				{
					const FVector Dir = FRotator(0.f, B.Angle, 0.f).Vector();
					const FVector CamP = C + Dir * (-ArR * 0.85f) + FVector(-Dir.Y, Dir.X, 0.f) * (ArR * 0.55f) + FVector(0.f, 0.f, 260.f); // the far side of the court, low, off to one side of the centre spire
					const FVector HeadP = Pos + FVector(0.f, 0.f, HeadZ - Pos.Z);
					FActorSpawnParameters SPc; SPc.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((HeadP - CamP).Rotation(), CamP), SPc))
					{
						Cm->GetCameraComponent()->SetFieldOfView(62.f);
						Cm->GetCameraComponent()->bConstrainAspectRatio = false;
						B.Cam = Cm; B.CamPrev = Vt; B.CamLeft = B.CamWant; B.CamT = 0.f; B.CamWait = 0.f;
						Pc->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
						UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS cam %.1f s"), B.CamWant);
					}
				}
				if (B.Cam.IsValid()) B.CamWant = 0.f;
				else if ((B.CamWait += Dt) > 15.f) { B.CamWant = 0.f; B.CamWait = 0.f; } // another camera owns the view: wait for it (15 s at most)
			}
			if (ACameraActor* Cm = B.Cam.Get())
			{	// a slow push-in on its face, then back to the game
				B.CamT += Dt; B.CamLeft -= Dt; if (const AZoogiGameMode* Gx = B.G.Get()) if (Gx->Phase == EZoogiPhase::Rolling && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCamInRoll"))) B.CamLeft = FMath::Min(B.CamLeft, 0.f); // b193: a roll starts - back to play
				if (Pc && Pc->GetViewTarget() != Cm && B.CamT > 0.4f && B.CamT < 0.9f) Pc->SetViewTarget(Cm); // hold it against the turn camera
				const FVector HeadP = Pos + FVector(0.f, 0.f, HeadZ - Pos.Z - 300.f);
				const FVector P0 = Cm->GetActorLocation();
				Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (HeadP - P0).Rotation(), Dt, 6.f));
				Cm->GetCameraComponent()->SetFieldOfView(FMath::Lerp(62.f, 48.f, FMath::Clamp(B.CamT / 2.5f, 0.f, 1.f)));
				if (B.CamLeft <= 0.f) EndBossCam(0.5f);
			}
			B.LogT -= Dt;
			if (B.LogT <= 0.f)
			{	// b136 debug: where it is and whether it rendered
				B.LogT = 3.f;
				USkeletalMeshComponent* Mc = B.Mesh.Get();
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS vp %dx%d noproj %d off %d hid %d at (%.0f,%.0f,%.0f) ang %.0f->%.0f head %.0f want %.0f socket %d rendered %d bounds %.0f mesh %s"),
					B.Vxl, B.Vyl, B.NoProj, B.OffScr, B.Hid, Pos.X, Pos.Y, Pos.Z, B.Angle, B.WantAngle, Mc ? Mc->GetSocketLocation(TEXT("head")).Z : 0.f, HeadZ, Mc && Mc->DoesSocketExist(TEXT("head")) ? 1 : 0,
					Mc && Mc->WasRecentlyRendered(0.5f) ? 1 : 0, Mc ? Mc->Bounds.SphereRadius : 0.f, Mc ? *GetNameSafe(Mc->GetSkinnedAsset()) : TEXT("-"));
			}
			// eyes: base glow, flares decay back, a scoffing flicker
			B.Eye = FMath::FInterpTo(B.Eye, B.EyeBase, Dt, 1.2f);
			float K = B.Eye * (0.85f + 0.15f * FMath::Sin(B.T * 1.7f));
			if (B.Flicker > 0.f) { B.Flicker -= Dt; K *= (FMath::Fmod(B.T * 14.f, 1.f) < 0.5f) ? 0.15f : 1.f; }
			SetEyes(K);
			// back to the idle loop after a reaction
			if (B.ReactLeft > 0.f)
			{
				B.ReactLeft -= Dt;
				if (B.ReactLeft <= 0.f) if (USkeletalMeshComponent* Mc = B.Mesh.Get()) if (UAnimSequence* Id = IdleAnim()) { Mc->PlayAnimation(Id, true); Mc->SetPlayRate(0.45f); }
			}
			B.Cool = FMath::Max(0.f, B.Cool - Dt);
			return true;
		}

		void Spawn(AZoogiGameMode* G)
		{
			UWorld* W = G ? G->GetWorld() : nullptr;
			if (!W || !G->Arena) return;
			GTheme = ThemeFor(G->Arena->GetTheme()); EyeRed = GTheme.Eye; // b148a
			USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Mecha_Aurex/Meshes/%s.%s"), GTheme.Mesh, GTheme.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex_03.SK_Mecha_Aurex_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			// b175b (Evan: "make this model the volcano boss"): Volcanic's MAGMA TITAN is the Stone Golem (own materials, hammer + shield)
			static const bool bOldMagma = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMagmaBoss"));
			bool bGolem = false;
			if (!bOldMagma && !Titan() && G->Arena->GetTheme() == EZoogiArena::Volcanic)
				if (USkeletalMesh* Gs = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/StoneGolem/Mesh/SKM_StoneGolem.SKM_StoneGolem"), nullptr, LOAD_NoWarn | LOAD_Quiet)) { SM = Gs; bGolem = true; }
			if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: no Aurex mesh")); return; }
			if (!Titan())
			{	// b142: the peek boss on the rim
				float Ang0 = 0.f;
				if (APlayerController* Pc0 = W->GetFirstPlayerController()) if (Pc0->PlayerCameraManager) Ang0 = Pc0->PlayerCameraManager->GetCameraRotation().Yaw;
				AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnBoss(W, SM, Centre(G) + FVector(0.f, 0.f, -3000.f), Ang0 + 180.f, PeekH);
				if (!P) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: peek puppet failed")); return; }
				P->Tint = GTheme.Tint;
				if (bGolem)
				{	// the golem keeps its own stone materials; the hammer and shield go in its hands
					struct FW { const TCHAR* Mesh; const TCHAR* Sock; const TCHAR* Bone; };
					static const FW Ws[] = { { TEXT("SM_StoneGolem_Hammer"), TEXT("weapon_rSocket"), TEXT("weapon_r") }, { TEXT("SM_StoneGolem_Shield"), TEXT("weapon_lSocket"), TEXT("weapon_l") } };
					for (const FW& Wp : Ws)
						if (UStaticMesh* Wm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/StoneGolem/Mesh/%s.%s"), Wp.Mesh, Wp.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet))
						{
							UStaticMeshComponent* Wc = NewObject<UStaticMeshComponent>(P);
							Wc->SetStaticMesh(Wm); Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wc->SetCastShadow(false); Wc->SetBoundsScale(3.f);
							const FName Sk = P->Body->DoesSocketExist(FName(Wp.Sock)) ? FName(Wp.Sock) : FName(Wp.Bone);
							Wc->SetupAttachment(P->Body, Sk); Wc->RegisterComponent();
						}
					UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: MAGMA TITAN = Stone Golem (hammer + shield)"));
				}
				else if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, P);
					MID->SetVectorParameterValue(TEXT("Color_Zone_R"), GTheme.R); // b148a: the map's colours
					MID->SetVectorParameterValue(TEXT("Color_Zone_G"), GTheme.G);
					MID->SetVectorParameterValue(TEXT("Color_Zone_B"), GTheme.B);
					MID->SetVectorParameterValue(TEXT("Emissive_Color"), EyeRed);
					for (int32 m = 0; m < P->Body->GetNumMaterials(); ++m) P->Body->SetMaterial(m, MID);
					B.MID = MID;
				}
				B.A = P; B.Pup = P; B.G = G; B.H = PeekH;
				static const bool bOldGrove = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGroveTitan"));
				// b214 (Evan Nov 9: "download this model from tripo then make it the meadow arena boss also change its body color to purple" /
				// "... make it the dark realm arena boss"): Meadow = his Tripo stone golem (body hue-shifted to purple), the DARK REALM
				// (Cosmic + Underrealm) = his Tripo crystal brute. -ZoogiOldGroveTitan = the Tally Golem again.
				const bool bDarkRealm = G->Arena->GetTheme() == EZoogiArena::Cosmic && ZoogiUnderrealm::On() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldDarkTitan"));
				UStaticMesh* TitanSM = nullptr;
				// b219 (Evan Nov 10: "i dont like the golem go back to the robot"): the Meadow titan is the mecha robot again (the Aurex puppet in
				// the Grove colours). -ZoogiGroveGolem brings back the rigged Tripo golem (b217/b218), -ZoogiStaticGolem the static one.
				static const bool bGroveGolem = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGroveGolem")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiStaticGolem")) || bOldGrove;
				if (G->Arena->GetTheme() == EZoogiArena::Meadow && bGroveGolem)
				{
					if (!bOldGrove) TitanSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/MeadowGolem2/SM_PurpleGolem.SM_PurpleGolem"), nullptr, LOAD_NoWarn | LOAD_Quiet);
					if (!TitanSM) TitanSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/MeadowGolem/SM_TallyGolem.SM_TallyGolem"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				}
				else if (bDarkRealm) TitanSM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/DarkRealmBoss/SM_CrystalBrute.SM_CrystalBrute"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				static const bool bStaticGolem = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStaticGolem"));
				USkeletalMesh* GolemSK = (bGroveGolem && !bOldGrove && !bStaticGolem && G->Arena->GetTheme() == EZoogiArena::Meadow) ? LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Zoogis/MeadowGolemRig/SK_Golem.SK_Golem"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
				GSpiderRig = false;
				if (!GolemSK && G->Arena->GetTheme() == EZoogiArena::Tomb && FParse::Param(FCommandLine::Get(), TEXT("ZoogiTombSpider")) /* b236: Evan - no spider boss */)
				{	// b233 (Evan: Voyager "D"): the Greater Spider climbs onto the Tomb rim (-ZoogiTombTitan = the Pharaoh mech)
					GolemSK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Voyager/Demo/Characters/Spider/SK_Greater_Spider.SK_Greater_Spider"), nullptr, LOAD_NoWarn | LOAD_Quiet);
					GSpiderRig = GolemSK != nullptr;
				}
				if (GolemSK)
				{	// b217: the rigged, animated golem - whole on the rim like b216 (6.5 m), facing the court
					USkeletalMeshComponent* Gk = NewObject<USkeletalMeshComponent>(P); // the hidden driver (plays the clips)
					Gk->SetSkeletalMeshAsset(GolemSK); Gk->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gk->SetCastShadow(false); Gk->SetBoundsScale(3.f);
					Gk->SetAnimationMode(EAnimationMode::AnimationSingleNode); Gk->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					Gk->bEnableUpdateRateOptimizations = false;
					Gk->SetupAttachment(P->GetRootComponent()); Gk->RegisterComponent();
					UPoseableMeshComponent* Gp = NewObject<UPoseableMeshComponent>(P); // what you see (crossfaded pose)
					Gp->SetSkinnedAssetAndUpdate(GolemSK, true); Gp->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gp->SetCastShadow(false); Gp->SetBoundsScale(3.f);
					Gp->SetupAttachment(P->GetRootComponent()); Gp->RegisterComponent();
					const FBox Bb = GolemSK->GetImportedBounds().GetBox();
					float Hk = 650.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGroveH="), Hk);
					if (GSpiderRig) { Hk = 420.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiSpiderH="), Hk); } // b233: ~4 m tall, ~9 m long
					const float Sk = Hk / FMath::Max(1.f, (float)Bb.GetSize().Z);
					float Up = 1020.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGroveUp="), Up);
					float Yw = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGolemRigYaw="), Yw); // the UE5 Mannequin faces +Y
					if (GSpiderRig) FParse::Value(FCommandLine::Get(), TEXT("ZoogiSpiderYaw="), Yw);
					for (USceneComponent* Gc2 : { (USceneComponent*)Gk, (USceneComponent*)Gp })
					{ Gc2->SetRelativeScale3D(FVector(Sk)); Gc2->SetRelativeLocation(FVector(0.f, 0.f, -Bb.Min.Z * Sk + Up)); Gc2->SetRelativeRotation(FRotator(0.f, Yw, 0.f)); }
					Gk->SetVisibility(false, false);
					P->Body->SetVisibility(false, false);
					GTitanSM = Gp; GTA = FTA(); GTA.Sk = Gk; GTA.Po = Gp; GolemLoop(TEXT("idle"));
					TitanSM = nullptr;
					UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: %s, RIGGED + animated (%.1f m, bounds %.0f)"), GSpiderRig ? TEXT("Tomb = Voyager GREATER SPIDER") : TEXT("Meadow = Evan's Tripo golem"), Hk / 100.f, Bb.GetSize().Z);
				}
				if (TitanSM)
					if (UStaticMesh* Tg = TitanSM)
					{	// b206 (Evan: "I don't see the new meadow titan"): Meadow's titan is Evan's Tripo Tally Golem (lantern core) - the
						// Aurex puppet still steers (peek angle, rise / dip, bob) but is hidden; the golem rides on its root, ~11 m tall
						UStaticMeshComponent* Gc = NewObject<UStaticMeshComponent>(P);
						Gc->SetStaticMesh(Tg); Gc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gc->SetCastShadow(false); Gc->SetBoundsScale(3.f);
						Gc->SetupAttachment(P->GetRootComponent()); Gc->RegisterComponent();
						// b216 (Evan: "resize golem to fit entire body because it's always a cut off view"): the purple golem stands WHOLE on the rim -
						// 6.5 m tall, its feet at the rim top (the others keep 11 m, chest over the rim). -ZoogiGroveH= / -ZoogiGroveUp= tune it.
						const bool bWhole = Tg->GetName().Contains(TEXT("PurpleGolem"));
						float Hg = bWhole ? 650.f : 1100.f; if (bWhole) FParse::Value(FCommandLine::Get(), TEXT("ZoogiGroveH="), Hg);
						const FBox Bb = Tg->GetBoundingBox(); const float Sg = Hg / FMath::Max(1.f, (float)Bb.GetSize().Z);
						static float GolYaw = -1000.f; if (GolYaw < -999.f) { GolYaw = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGroveYaw="), GolYaw); }
						Gc->SetRelativeScale3D(FVector(Sg)); static float GolUp = -1.f; if (GolUp < 0.f) { GolUp = bWhole ? 1020.f : 700.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGroveUp="), GolUp); } Gc->SetRelativeLocation(FVector(0.f, 0.f, -Bb.Min.Z * Sg + GolUp)); // the puppet's feet are ~10 m under the rim: up 7 m = chest and lantern over the rim
						Gc->SetRelativeRotation(FRotator(0.f, GolYaw, 0.f)); // b215 (Evan: "fix the boss on meadow to face towards the center"): the turn sat inside the comment above, so the Tripo model faced sideways; its front (+Y) now turns to the puppet's facing (the court centre)
						P->Body->SetVisibility(false, false);
						GTitanSM = Gc; // b216
						UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: %s = Evan's Tripo %s (%.0f m, the puppet only steers)"), bDarkRealm ? TEXT("DARK REALM") : TEXT("Meadow"), *Tg->GetName(), Hg / 100.f);
					}
				if (ZoogiGolem::Wanted(G) && ZoogiGolem::Spawn(G)) { P->SetActorHiddenInGame(true); UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: Volcanic = Stone Golem full body (the puppet only steers)")); } // b177g
				if (ZoogiDragon::Wanted(G) && ZoogiDragon::Spawn(G)) { P->SetActorHiddenInGame(true); UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: Frozen = DRACORIA (the titan puppet only steers)")); } // b177a
				B.PeekAng = B.WantPeek = Ang0; B.Dip = 1.f; B.bMoving = false; B.PeekTurn = -1;
				B.CamWant = 3.f; B.Eye = 0.f; B.EyeBase = 2.5f; B.Rise = 0.f; B.RiseTo = 0.f;
				if (!Tick.IsValid()) Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step));
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peeks: %s (%s), %.0f m tall, hands on the rim at yaw %.0f"), *SM->GetName(), GTheme.Name, PeekH / 100.f, Ang0);
				return;
			}
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(Centre(G) + FVector(0.f, 0.f, -30000.f)), SP);
			if (!A) return;
			USkeletalMeshComponent* Mc = A->GetSkeletalMeshComponent();
			Mc->SetMobility(EComponentMobility::Movable);
			Mc->SetSkeletalMeshAsset(SM);
			Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mc->SetCastShadow(false);
			Mc->SetBoundsScale(4.f);
			Mc->bEnableUpdateRateOptimizations = false;
			Mc->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			const float Hm = FMath::Max(50.f, SM->GetBounds().BoxExtent.Z * 2.f);
			B.H = 17000.f; // 170 m
			A->SetActorScale3D(FVector(B.H / Hm));
			if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{	// dark violet armour, black joints, red glow lines / eyes
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, A);
				MID->SetVectorParameterValue(TEXT("Color_Zone_R"), FLinearColor(0.22f, 0.07f, 0.42f));
				MID->SetVectorParameterValue(TEXT("Color_Zone_G"), FLinearColor(0.04f, 0.02f, 0.08f));
				MID->SetVectorParameterValue(TEXT("Color_Zone_B"), FLinearColor(0.45f, 0.12f, 0.7f));
				MID->SetVectorParameterValue(TEXT("Emissive_Color"), EyeRed);
				for (int32 m = 0; m < Mc->GetNumMaterials(); ++m) Mc->SetMaterial(m, MID);
				B.MID = MID;
			}
			Mc->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UAnimSequence* Id = IdleAnim()) { Mc->PlayAnimation(Id, true); Mc->SetPlayRate(0.45f); } // slow = huge
			B.A = A; B.Mesh = Mc; B.G = G;
			B.HeadZ = B.WantHeadZ = Centre(G).Z + 900.f;
			if (APlayerController* Pc0 = W->GetFirstPlayerController()) if (Pc0->PlayerCameraManager) B.FixedAngle = Pc0->PlayerCameraManager->GetCameraRotation().Yaw; // beyond the arena from the opening camera
			B.Angle = B.WantAngle = B.FixedAngle; B.bSeated = true;
			B.CamWant = 3.f; // the reveal
			B.Rise = -14.f; B.RiseTo = 0.f; B.Eye = 0.f; B.EyeBase = 2.5f; // rises in (degrees under its spot), eyes kindle
			B.Dist = 11000.f * FMath::Max(1.f, G->Arena->GetActorScale3D().X);
			if (!Tick.IsValid()) Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step));
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS rises: %s, %.0f m tall, %.0f m out"), *SM->GetName(), B.H / 100.f, B.Dist / 100.f);
		}
	}

	void OnMatchStart(AZoogiGameMode* G)
	{
		Clear();
		ZoogiDragon::Clear(); // b177a
		ZoogiGolem::Clear(); // b177g
		GHold = false; GDefeated = false; GSweepOn = false; // b151
		GCameo = 0.f; GWasShown = false; B.Dip = 1.4f; // b155a: starts out of sight (the arena forms first)
		if (Off() || !G || !G->Arena) return;
		if (!ArenaHasBoss(G)) return; // b148a: every classic map
		Spawn(G);
	}

	// b193 (Evan: "the snowman and its platform go away after round one - the dragon shoots it because it doesn't like it"):
	// at round 2 Dracoria breathes ice at the snowman's platform - the snowman bursts limb by limb, then the platform shatters
	static void DragonHatesSnowman(AZoogiGameMode* G)
	{
		UWorld* W = G ? G->GetWorld() : nullptr; if (!W || !G->Arena) return;
		TWeakObjectPtr<AZoogiArenaPiece> Snow, Plat;
		for (TActorIterator<AZoogiArenaPiece> It(W); It; ++It)
		{
			if (It->GetKind() == FName(TEXT("snowman"))) Snow = *It;
			else if (It->GetKind() == FName(TEXT("platform"))) Plat = *It;
		}
		const AActor* Tgt = Snow.IsValid() ? (AActor*)Snow.Get() : (AActor*)Plat.Get();
		if (!Tgt) return;
		const FVector C = G->Arena->GetActorLocation(), P = Tgt->GetActorLocation();
		const float Ang = FMath::RadiansToDegrees(FMath::Atan2(P.Y - C.Y, P.X - C.X));
		ZoogiDragon::Event(TEXT("Roar"));
		ZoogiDragon::Sweep(true, Ang);
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("DRACORIA HATES SNOWMEN"), TEXT("ICE BREATH ON THE PLATFORM!"), FLinearColor(0.55f, 0.85f, 1.f), 1.8f, 1.f);
		UE_LOG(LogZoogi, Log, TEXT("DRAGON breathes at the snowman platform (yaw %.0f)"), Ang);
		const double T0 = FPlatformTime::Seconds(); int32 Hits = 0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Snow, Plat, T0, Ang, Hits](float) mutable -> bool
		{
			const double T = FPlatformTime::Seconds() - T0;
			ZoogiDragon::Sweep(true, Ang);
			if (T > 2.1 && T < 4.4)
			{	// b194: the beam is on after Dracoria's dive (~2 s) - hit until the snowman bursts (it takes a hit every 0.45 s)
				if (AZoogiArenaPiece* Sp = Snow.Get()) { const FVector Sl = Sp->GetActorLocation(); if (Sp->ApplyDamage(50.f, Sl + FVector(0, 0, 120.f), Sl.GetSafeNormal2D(), EZoogiArtifact::None, nullptr)) ++Hits; }
			}
			if (T > 4.6)
			{
				if (AZoogiArenaPiece* Pp = Plat.Get()) { const FVector Pl = Pp->GetActorLocation(); Pp->ApplyDamage(100000.f, Pl + FVector(0, 0, 60.f), Pl.GetSafeNormal2D(), EZoogiArtifact::None, nullptr); AZoogiFX::Niagara(Pp->GetWorld(), TEXT("NS_Burst_Blue"), Pl + FVector(0, 0, 80.f), FRotator::ZeroRotator, 2.f); }
				ZoogiDragon::Sweep(false, Ang);
				UE_LOG(LogZoogi, Log, TEXT("DRAGON: the snowman and its platform are gone"));
				return false;
			}
			return true;
		}));
	}

	void OnRoundStart(AZoogiGameMode* G)
	{
		if (!G || !B.A.IsValid()) { if (G && !B.A.IsValid() && !Off() && ArenaHasBoss(G)) Spawn(G); return; }
		if (ZoogiDragon::Active() && G->RoundNumber == 2 && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiKeepSnowman"))) DragonHatesSnowman(G); // b193
		const bool bFinal = G->RoundNumber >= G->MenuRounds;
		// b193 (Evan: "show the dragon slightly less"): Dracoria's round-opening roar every other round (was every round)
		if (ZoogiDragon::Active() && !(bFinal && !B.bFinal) && !(G->RoundNumber <= 1 && ZoogiDragon::IntroPlayed()) && G->RoundNumber % 2 == 0) // b178h: round 1 just had the fly-in
		{	// b177e (Evan: "show dragon more than most bosses"): every Frozen round opens on Dracoria's face, roaring
			GCameo = FMath::Max(GCameo, 4.f); ZoogiDragon::Event(TEXT("Roar")); BossCam(2.2f);
			UE_LOG(LogZoogi, Log, TEXT("DRAGON round %d intro shot"), G->RoundNumber);
		}
		if (bFinal && !B.bFinal)
		{	// the final round: it rises higher and roars, the eyes stay hot
			B.bFinal = true; B.RiseTo = 3.f; B.EyeBase = 4.5f; B.Eye = 9.f;
			if (B.Pup.IsValid()) B.RoarLeft = 2.4f; // b142: rears up, arms high
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.6f);
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.55f);
			ZoogiHaptics::Kick(0.7f, G->GetWorld(), TEXT("cosmos_boss_final"));
			ZoogiEpic::Hit(G, ZoogiEpic::EHit::Riser);
			BossCam(2.6f);
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: final round - it rises and roars"));
		}
	}

	bool Present(const AZoogiGameMode* G) { return G && B.G.Get() == G && B.A.IsValid() && B.Pup.IsValid(); }
	FString Name() { return ZoogiDragon::Active() ? FString(TEXT("DRACORIA")) : FString(GTheme.Name); } // b177a
	float Angle() { return B.PeekAng; }
	bool BossCamOn() { return B.Cam.IsValid(); } // b217 (Evan: "hide banners when showing golem")
	bool TitanFocus(FVector& OutCenter, float& OutHalfH)
	{	// b216: the middle and half height of the Tripo titan (when one rides the puppet and is up)
		UPrimitiveComponent* Gc = GTitanSM.Get();
		if (!Gc || !Gc->IsVisible() || !Gc->GetOwner() || Gc->GetOwner()->IsHidden()) return false;
		OutCenter = Gc->Bounds.Origin; OutHalfH = FMath::Max(100.f, (float)Gc->Bounds.BoxExtent.Z / 3.f); // Bounds were scaled x3 (SetBoundsScale)
		return true;
	}
	void HoldAt(bool bOn, float AngDeg) { GHold = bOn; GHoldAng = AngDeg; if (!bOn) GDefeated = false; }
	void Flinch() { ZoogiDragon::Event(TEXT("Hit")); ZoogiGolem::Event(TEXT("Hit")); GolemOnce(TEXT("hit_to_body_01"), 1.2f); B.Eye = 10.f; B.Flicker = 0.35f; B.Slam = FMath::Max(B.Slam, 0.6f); }
	void Defeat() { GolemOnce(TEXT("defeat"), 1.f, true); /* b217: the FBX clip defeat_02 -> AS_Golem_defeat */ GDefeated = true; B.RoarLeft = 0.f; Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.4f); }
	FLinearColor Tint() { return GTheme.Tint; }
	void Sweep(bool bOn, float AngDeg)
	{
		const bool bSweepWasOn = GSweepOn; // b219: Sweep(true) arrives every frame - the golem's slash starts once per sweep
		GSweepOn = bOn; GSweepAng = AngDeg;
		ZoogiDragon::Sweep(bOn, AngDeg); // b177a
		ZoogiGolem::Sweep(bOn, AngDeg); // b177g
		if (bOn) { B.Eye = 10.f; B.Dip = FMath::Min(B.Dip, 0.4f); ZoogiFilm::Moment(TEXT("Boss")); if (!bSweepWasOn) GolemOnce(TEXT("slash")); } // b214: trailer footage; b217 the golem swings
	}
	void SlamToward(float AngDeg)
	{
		B.PeekAng = B.WantPeek = AngDeg; B.bMoving = false; B.Dip = FMath::Min(B.Dip, 0.25f);
		B.Slam = 1.f; B.Cool = 2.5f; B.Eye = 10.f; B.LeanTo = 8.f;
		ZoogiDragon::Event(TEXT("Stomp")); ZoogiGolem::GoTo(AngDeg); ZoogiGolem::Event(TEXT("Slam"));
		ZoogiFilm::Moment(TEXT("Boss")); // b214: trailer footage
		GolemOnce(TEXT("chop")); // b217: the golem's overhead smash
		Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.9f);
	}
	void Taunt()
	{
		B.Eye = 9.f; B.EyeBase = FMath::Max(B.EyeBase, 4.5f);
		if (B.Pup.IsValid()) B.RoarLeft = 1.6f;
		ZoogiDragon::Event(TEXT("Taunt")); ZoogiGolem::Event(TEXT("Taunt")); GolemOnce(TEXT("angry_01")); // b217
		Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.6f);
		BossCam(1.6f);
	}

	void OnEvent(UWorld* W, const FString& Ev)
	{
		AZoogiGameMode* G = B.G.Get();
		if (!G || !B.A.IsValid() || G->GetWorld() != W) return;
		if (Ev == TEXT("KO") || Ev == TEXT("Combo")) GCameo = FMath::Max(GCameo, 5.f); // b155a: comes up to watch
		if (Ev == TEXT("KO"))
		{	// a slow power gesture, eyes blaze, rumble
			B.Eye = 10.f; B.LeanTo = 6.f;
			if (B.Pup.IsValid()) { B.Slam = 1.f; B.Cool = 2.5f; } // b142: slams the rim
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.7f);
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.6f, 0.5f);
			ZoogiHaptics::Kick(0.5f, W, TEXT("cosmos_boss_ko"));
			BossCam(1.8f);
			ZoogiDragon::Event(TEXT("Roar")); ZoogiGolem::Event(TEXT("Roar")); GolemOnce(TEXT("cheer")); // b217
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: KO"));
		}
		else if (Ev == TEXT("Combo"))
		{	// leans in and pumps a fist
			if (B.Cool > 0.f) return;
			B.Cool = 3.f; B.Eye = 7.f; B.LeanTo = 10.f;
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cheer.GEN_cheer")), 0.7f);
			ZoogiDragon::Event(TEXT("Threat")); ZoogiGolem::Event(TEXT("Threat")); GolemOnce(TEXT("angry_01")); // b217
			// b193: no extra Dracoria cut on combos (Evan: show it a bit less)
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: combo - leans in"));
		}
		else if (Ev == TEXT("Critical"))
		{
			B.Eye = 8.f;
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: critical"));
		}
		else if (Ev == TEXT("Miss"))
		{	// scoffs
			if (B.Cool > 0.f) return;
			B.Cool = 4.f; B.Flicker = 0.6f;
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Rifle/R_MF_Emote_FingerGuns.R_MF_Emote_FingerGuns")), 0.6f);
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: miss - scoffs"));
		}
	}
}
