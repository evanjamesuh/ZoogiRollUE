#include "ZoogiOcclusionFade.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiOcclusionFade
{
	static TAutoConsoleVariable<int32> CVarOn(TEXT("zoogi.Cam.OcclusionFade"), 0, TEXT("(Oct 29 build 24, Evan: removed - default off) Fade meshes between the camera and the active Zoogi / near aim line to a see-through ghost (0 = off). Presentation only."));
	static TAutoConsoleVariable<float> CVarOpacity(TEXT("zoogi.Cam.OcclusionOpacity"), 0.25f, TEXT("Occlusion fade: opacity of a blocking mesh (0..1)."));
	static TAutoConsoleVariable<float> CVarSpeed(TEXT("zoogi.Cam.OcclusionSpeed"), 4.f, TEXT("Occlusion fade: opacity change per second (fade out and restore)."));
	static TAutoConsoleVariable<float> CVarRadius(TEXT("zoogi.Cam.OcclusionRadius"), 45.f, TEXT("Occlusion fade: sweep sphere radius (cm)."));
	static TAutoConsoleVariable<float> CVarAhead(TEXT("zoogi.Cam.OcclusionAhead"), 400.f, TEXT("Occlusion fade: also clear the aim line this far ahead of the Zoogi (cm)."));

	struct FFaded
	{
		TArray<TStrongObjectPtr<UMaterialInterface>> Orig; // strong: once ghosted, nothing else holds a per-mesh MID and GC freed it (merge39 Frozen AV in SetMaterial)
		TArray<TObjectPtr<UMaterialInstanceDynamic>> Ghost;
		float Alpha = 1.f;
		bool bBlocking = false;
		bool bShadow = true;
	};
	static TMap<TWeakObjectPtr<UMeshComponent>, FFaded> GFaded;
	static TArray<TWeakObjectPtr<UMeshComponent>> GCandidates;
	static TWeakObjectPtr<UWorld> GWorld;
	static float GRescan = 0.f;

	bool Enabled() { return CVarOn.GetValueOnGameThread() != 0; }
	int32 NumFaded() { int32 N = 0; for (auto& K : GFaded) if (K.Value.Alpha < 0.99f) ++N; return N; }

	static UMaterialInterface* GhostBase()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid()) M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ZoogiDust.M_ZoogiDust"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		return M.Get();
	}

	static FLinearColor TintOf(UMaterialInterface* M)
	{
		FLinearColor C(0.55f, 0.5f, 0.45f, 1.f);
		if (!M) return C;
		static const FName Names[] = { TEXT("Color"), TEXT("BaseColor"), TEXT("Tint"), TEXT("A"), TEXT("Albedo") };
		for (const FName& N : Names) { FLinearColor V; if (M->GetVectorParameterValue(FHashedMaterialParameterInfo(N), V)) return FLinearColor(V.R, V.G, V.B, 1.f); }
		return C;
	}

	// Oct 24 (Evan 1:08 PM): only vertical walls ever go see-through. Floors, ground, islands, bumpers, props, orbs never do.
	// A wall is a mesh tagged CamFadeAlways / CamFadeWall (rim chunks) or a tall, thin box: height >= 120 cm, thinnest
	// horizontal side <= 0.6 x height, height >= 0.25 x longest side. Orbs are never walls. Sweep hits also need a
	// mostly-vertical surface (|ImpactNormal.Z| < 0.3).
	static bool IsVerticalWall(const UMeshComponent* M)
	{
		if (M->ComponentHasTag(TEXT("CamFadeAlways")) || M->ComponentHasTag(TEXT("CamFadeWall"))) return true;
		const AActor* O = M->GetOwner();
		if (O && O->IsA<AZoogiOrb>()) return false;
		const FVector S = M->Bounds.GetBox().GetSize();
		const float H = S.Z, Thin = FMath::Min(S.X, S.Y), Long = FMath::Max(S.X, S.Y);
		return H >= 120.f && Thin <= 0.6f * H && H >= 0.25f * Long;
	}

	static bool Skippable(const UMeshComponent* M)
	{
		if (!M || !M->IsVisible() || M->bHiddenInGame || Cast<USkeletalMeshComponent>(M)) return true;
		const AActor* O = M->GetOwner();
		if (!O || O->IsA<AZoogiBall>() || O->IsA<APlayerCameraManager>()) return true;
		if (M->ComponentHasTag(TEXT("NoCamFade"))) return true;
		return !IsVerticalWall(M);
	}

	void RestoreAll()
	{
		for (auto& K : GFaded)
		{
			if (UMeshComponent* M = K.Key.Get())
			{
				for (int32 i = 0; i < K.Value.Orig.Num(); ++i) M->SetMaterial(i, K.Value.Orig[i].Get());
				M->SetCastShadow(K.Value.bShadow);
			}
		}
		GFaded.Reset();
	}

	static void Rescan(UWorld* W)
	{
		GCandidates.Reset();
		for (TObjectIterator<UMeshComponent> It; It; ++It)
		{
			UMeshComponent* M = *It;
			if (!IsValid(M) || M->GetWorld() != W || !M->IsRegistered() || Skippable(M)) continue;
			const float R = M->Bounds.SphereRadius;
			if (R < 30.f || R > 20000.f) continue;
			GCandidates.Add(M);
		}
	}

	// segment vs the mesh's world box; the floor / anything whose top is under the Zoogi and any box that contains the
	// Zoogi (court rings, the floor under it) never counts.
	static bool Blocks(const UMeshComponent* M, const FVector& A, const FVector& B, const FVector& Zoogi, float Pad)
	{
		const FBox Box = M->Bounds.GetBox();
		if (Box.Max.Z < Zoogi.Z + ZoogiConst::ZoogiRadius * 0.6f) return false;
		// a box around the Zoogi is the floor / court ring unless it is a tall rim wall (top well above the Zoogi) seen from outside
		// CamFadeAlways (teaser rim chunks): a tall wall never counts as the floor / ring, even when its box holds both the camera and the Zoogi (ship pad outside the rim, merge36 Aim block)
		if (!M->ComponentHasTag(TEXT("CamFadeAlways")) && Box.ExpandBy(ZoogiConst::ZoogiRadius).IsInside(Zoogi) && (Box.Max.Z < Zoogi.Z + 150.f || Box.IsInside(A))) return false;
		const FBox Ex = Box.ExpandBy(Pad);
		if (Ex.IsInside(A)) return true; // camera inside the mesh's box
		const FVector D = B - A;
		return FMath::LineBoxIntersection(Ex, A, B, D);
	}

	void Tick(APlayerController* PC, AZoogiGameMode* G, float Dt)
	{
		UWorld* W = PC ? PC->GetWorld() : nullptr;
		if (!W) return;
		if (GWorld.Get() != W) { GFaded.Reset(); GCandidates.Reset(); GWorld = W; GRescan = 0.f; }
		{	// b161: the per-pixel see-through hole (Ancient materials read MPC_ZoogiCam.Zoogi)
			static const bool bNo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSeeThrough"));
			static TWeakObjectPtr<UMaterialParameterCollection> GMpc; static bool bTried = false;
			if (!bTried) { bTried = true; GMpc = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_ZoogiCam.MPC_ZoogiCam"), nullptr, LOAD_NoWarn | LOAD_Quiet); UE_LOG(LogZoogi, Log, TEXT("SEE-THROUGH: MPC_ZoogiCam %s"), GMpc.IsValid() ? TEXT("loaded") : TEXT("missing")); }
			if (UMaterialParameterCollection* Mpc = GMpc.Get())
			{
				AZoogiBall* Act = (!bNo && G && PC->PlayerCameraManager) ? G->GetActiveBall() : nullptr;
				const FVector L = Act ? Act->GetActorLocation() : FVector(0.f, 0.f, -100000.f);
				UKismetMaterialLibrary::SetVectorParameterValue(W, Mpc, TEXT("Zoogi"), FLinearColor(L.X, L.Y, L.Z, Act ? 1.f : 0.f));
			}
		}
		AZoogiBall* Me = (G && Enabled() && PC->PlayerCameraManager) ? G->GetActiveBall() : nullptr;

		for (auto& K : GFaded) K.Value.bBlocking = false;
		if (Me)
		{
			GRescan -= Dt;
			if (GRescan <= 0.f) { Rescan(W); GRescan = 2.f; }
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Z = Me->GetActorLocation();
			const FVector Ahead = Z + FRotator(0.f, Me->AimYaw, 0.f).Vector() * CVarAhead.GetValueOnGameThread();
			const float Pad = FMath::Clamp(CVarRadius.GetValueOnGameThread(), 0.f, 300.f);
			// stop just short of the Zoogi so its own floor / pad under it never counts
			const FVector ToZ = Z - Cam;
			const FVector End = Cam + ToZ.GetSafeNormal() * FMath::Max(0.f, ToZ.Size() - ZoogiConst::ZoogiRadius * 1.2f);
			const FVector Mid = FMath::Lerp(Cam, Ahead, 0.f);
			auto Mark = [&](UMeshComponent* M)
			{
				FFaded& F = GFaded.FindOrAdd(M);
				F.bBlocking = true;
				if (F.Ghost.Num() == 0)
				{
					UMaterialInterface* Base = GhostBase();
					if (!Base) { GFaded.Remove(M); return; }
					F.bShadow = M->CastShadow;
					const int32 N = M->GetNumMaterials();
					for (int32 i = 0; i < N; ++i)
					{
						UMaterialInterface* O = M->GetMaterial(i);
						F.Orig.Emplace(O);
						UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, M);
						D->SetVectorParameterValue(TEXT("Color"), TintOf(O));
						D->SetScalarParameterValue(TEXT("Opacity"), 1.f);
						D->SetScalarParameterValue(TEXT("Soft"), 0.f);
						D->SetScalarParameterValue(TEXT("FadeDistance"), 1.f);
						F.Ghost.Add(D);
						M->SetMaterial(i, D);
					}
				}
			};
			// Segments to keep clear: camera -> Zoogi, camera -> AimYaw ahead, and (aim view) camera -> every few dots of the
			// HUD's real predicted shot path + ghost contact. The dotted path is often far longer than OcclusionAhead and not
			// along AimYaw, so a rock covering its far / near-camera stretch was never tested (merge35 r4 AIM fail).
			TArray<TPair<FVector, FVector>> Segs;
			Segs.Emplace(Cam, End);
			Segs.Emplace(Cam, Ahead);
			if (G->bAimActive && G->Preview.bValid)
			{
				const TArray<FVector>& P = G->Preview.Path;
				const int32 Step = FMath::Max(1, P.Num() / 12);
				const FVector Lift(0.f, 0.f, ZoogiConst::ZoogiRadius);
				for (int32 i = 0; i < P.Num(); i += Step) Segs.Emplace(Cam, P[i] + Lift);
				if (P.Num() > 0) Segs.Emplace(Cam, P.Last() + Lift);
				if (G->Preview.bContact) Segs.Emplace(Cam, G->Preview.Ghost + Lift);
			}
			// 1) collidable blockers (walls, rocks): sphere sweep on the camera + visibility channels (queries only)
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiOcclusionFade), false);
			for (TActorIterator<AZoogiBall> It(W); It; ++It) Q.AddIgnoredActor(*It);
			for (ECollisionChannel Ch : { ECC_Camera, ECC_Visibility })
			{
				for (const TPair<FVector, FVector>& Seg : Segs)
				{
					TArray<FHitResult> Hits;
					W->SweepMultiByChannel(Hits, Seg.Key, Seg.Value, FQuat::Identity, Ch, FCollisionShape::MakeSphere(Pad), Q);
					for (const FHitResult& H : Hits)
					{
						UMeshComponent* M = Cast<UMeshComponent>(H.GetComponent());
						if (!M || Skippable(M)) continue;
						if (H.ImpactNormal.Z > 0.7f && H.ImpactPoint.Z < Z.Z + ZoogiConst::ZoogiRadius) continue; // floor
						if (FMath::Abs(H.ImpactNormal.Z) >= 0.3f && !M->ComponentHasTag(TEXT("CamFadeAlways"))) continue; // Oct 24: only mostly-vertical faces
						if (!M->ComponentHasTag(TEXT("CamFadeAlways"))) { const FBox Bx = M->Bounds.GetBox(); if (Bx.ExpandBy(ZoogiConst::ZoogiRadius).IsInside(Z) && (Bx.Max.Z < Z.Z + 150.f || Bx.IsInside(Cam))) continue; } // floor / ring the Zoogi stands in
						Mark(M);
					}
				}
			}
			// 2) no-collision dressing (rocks, trees, kit walls): segment vs bounds
			for (const TWeakObjectPtr<UMeshComponent>& Wk : GCandidates)
			{
				UMeshComponent* M = Wk.Get();
				if (!M || Skippable(M)) continue;
				for (int32 s = 0; s < Segs.Num(); ++s) if (Blocks(M, Segs[s].Key, Segs[s].Value, Z, s == 0 ? Pad : Pad * 0.5f)) { Mark(M); break; }
			}
			(void)Mid;
		}

		const float Lo = FMath::Clamp(CVarOpacity.GetValueOnGameThread(), 0.f, 1.f);
		const float Sp = FMath::Max(0.1f, CVarSpeed.GetValueOnGameThread());
		for (auto It = GFaded.CreateIterator(); It; ++It)
		{
			UMeshComponent* M = It.Key().Get();
			FFaded& F = It.Value();
			if (!IsValid(M) || M->IsBeingDestroyed() || !M->IsRegistered() || !IsValid(M->GetOwner()) || M->GetOwner()->IsActorBeingDestroyed()) { It.RemoveCurrent(); continue; } // Oct 24 merge39: Frozen AV in SetMaterial on a dying mesh
			F.Alpha = FMath::FInterpConstantTo(F.Alpha, F.bBlocking ? Lo : 1.f, Dt, Sp * (1.f - Lo));
			for (UMaterialInstanceDynamic* D : F.Ghost) if (D) D->SetScalarParameterValue(TEXT("Opacity"), F.Alpha);
			M->SetCastShadow(F.bShadow && F.Alpha > 0.6f);
			if (!F.bBlocking && F.Alpha >= 0.999f)
			{
				for (int32 i = 0; i < F.Orig.Num(); ++i) M->SetMaterial(i, F.Orig[i].Get());
				M->SetCastShadow(F.bShadow);
				It.RemoveCurrent();
			}
		}
	}
}
