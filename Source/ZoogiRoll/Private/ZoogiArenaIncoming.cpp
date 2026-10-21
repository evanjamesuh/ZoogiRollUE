// Oct 7 (Evan 10:20 / 11:24): places the reskin-incoming builder packs on Volcanic / Cosmic / Arabian / Tomb / Night Circuit /
// Training after the theme's own build. Court items, floors and gap pieces are skipped (the C++ court stays authoritative).
//  - Kind 1: solid props in reach (r 1550-3300 unscaled) are breakable pieces with a bounds collider; a prop whose bounds hit an
//    existing blocking part is placed visual-only. Kind 2: unbreakable pillars that rise at match start and collide once risen.
//  - Every glTF material slot becomes a SurfaceMID / GlowMID from the builder's own factors, so piece damage tints work.
//  - Destruction: the builder's Cracked / Shattered meshes (_Cracked/_Shattered, _s1/_s2, _Damaged/_Broken, _dmg1/_dmg2 or the
//    in-file _Damaged/_Destroyed nodes) swap in when a piece reaches CRACKED / BROKEN.
//  - Forming: pieces and dressing rise in from below, staggered from the court outwards (pack forming_timeline order).
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArenaPlayTest.h"
#include "ZoogiStageForming.h"
#include "ZoogiPhysics.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "ZoogiIncoming.inl"

namespace
{
	struct FSwap
	{
		TWeakObjectPtr<AZoogiArenaPiece> P;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> S[3];
		int32 Shown = 0;
	};
	FTimerHandle GSwapTimer;
}

void AZoogiArena::BuildTraining()
{
	// Training Arena (Oct 7): the Meadow r22 layout at A 1.3 (knockout 2015, 6 orbs r 650 on yaws 15+60k, spawns r 1430,
	// 4 rune pillars r 800 x A). Plain round court; the builder pack (PlaceIncoming) dresses it.
	constexpr float TA = 1.3f;
	BuildScale = TA; SetActorScale3D(FVector(TA));
	KnockoffRadiusOverride = ZoogiConst::KnockoffRadius * TA; ActiveKnockoffR = KnockoffRadiusOverride;
	SpawnRadiusOverride = ZoogiConst::SpawnRadius * TA; OrbCountOverride = 6;
	OrbRingRadiusOverride = 650.f; OrbRingYawOffset = 15.f;
	UMaterialInterface* FloorMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#D9CDB4")), ZoogiUtil::Hex(TEXT("#C9BCA0")), 0.006f, 0.85f);
	// K1 kit floor (08 IMPORT.md: T_TrainingFloor + T_TrainingGlow runes/boundary #2CCFC4 x1.8), cosmetic damage only.
	UMaterialInterface* Kit = KitFloorMaterial(TEXT("Training"), ZoogiUtil::Hex(TEXT("#2CCFC4")), 1.8f, 1.0f);
	if (Kit) { FloorMat = Kit; FloorMID = Cast<UMaterialInstanceDynamic>(Kit); }
	BuildIsland(ZoogiUtil::Hex(TEXT("#D9CDB4")), ZoogiUtil::Hex(TEXT("#C9BCA0")), ZoogiUtil::Hex(TEXT("#8A8276")),
		ZoogiUtil::Hex(TEXT("#7A7771")), ZoogiUtil::Hex(TEXT("#5E5A54")), 0.3f, FloorMat);
	if (!Kit) AddCylinder(FVector2D::ZeroVector, ZoogiConst::KnockoffRadius - 5.f, 0.6f, 0.6f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#2CCFC4")), 1.8f), false)->SetCastShadow(false);
	if (!Kit) AddCylinder(FVector2D::ZeroVector, ZoogiConst::KnockoffRadius - 20.f, 0.8f, 0.8f, FloorMat, false)->SetCastShadow(false);
	AddPanoramaSky(TEXT("Training"), 0.6f, 0.f, FLinearColor::White); // _skies-lighting/08: emissive x0.9 -> 0.75 -> 0.6 (Oct 14 contrast + haze passes, sky blew out)
}

void AZoogiArena::PlaceIncoming()
{
	const bool bNoInc = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoIncoming")); // tests still run (playtester baseline)
	const int32 Th = (int32)Theme;
	const int32 NM = UE_ARRAY_COUNT(ZoogiInc::Meshes);
	TArray<TArray<UStaticMesh*>> Sets[3];
	for (int32 s = 0; s < 3; ++s) Sets[s].SetNum(NM);
	for (const ZoogiInc::FPart& Pt : ZoogiInc::Parts)
		if (ZoogiInc::MeshTheme[Pt.Mesh] == Th)
			if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Pt.Path)) Sets[Pt.Stage][Pt.Mesh].Add(SM);
	// Pack sky + lighting (_skies-lighting/06 and /07 LIGHTING.md, normal state).
	if (!bNoInc && Theme == EZoogiArena::Tomb)
	{
		Sun->SetRelativeRotation(FRotator(-50.f, 35.f, 0.f)); Sun->SetIntensity(3.2f); Sun->SetLightColor(FLinearColor(1.f, 0.90f, 0.75f));
		Atmosphere->SetRayleighScatteringScale(0.0331f); Atmosphere->SetMieScatteringScale(0.004f);
		SkyLight->SetIntensity(1.2f); SkyLight->SetLightColor(FLinearColor(1.f, 0.96f, 0.90f));
		Fog->SetFogDensity(0.01f); Fog->SetFogHeightFalloff(0.12f); Fog->SetFogInscatteringColor(FLinearColor(0.85f, 0.70f, 0.50f)); Fog->SetStartDistance(2400.f);
		FPostProcessSettings& PP = Post->Settings;
		PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.65f; PP.bOverride_BloomThreshold = true; PP.BloomThreshold = 0.9f;
		PP.bOverride_ColorSaturation = true; PP.ColorSaturation = FVector4(1.05f, 1.05f, 1.05f, 1.f);
		PP.bOverride_ColorContrast = true; PP.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.f);
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.25f;
		AddPanoramaSky(TEXT("Tomb"), 0.9f, 0.f, FLinearColor::Black);
		AddPointLight(FVector(0, 0, 270.f), ZoogiUtil::Hex(TEXT("#FFC64A")), 60.f, 900.f);
	}
	else if (!bNoInc && Theme == EZoogiArena::Neon)
	{
		Sun->SetRelativeRotation(FRotator(-40.f, 300.f, 0.f)); Sun->SetIntensity(2.4f); Sun->SetLightColor(ZoogiUtil::Hex(TEXT("#9AA8FF")));
		SkyLight->SetIntensity(2.2f); // Oct 14: was 0.8 / 0.5, which left the wood stands, pagodas and islets black SkyLight->SetLightColor(FLinearColor(0.62f, 0.58f, 1.f));
		Fog->SetFogDensity(0.008f); Fog->SetFogHeightFalloff(0.15f); Fog->SetFogInscatteringColor(FLinearColor(0.24f, 0.13f, 0.36f)); Fog->SetStartDistance(2600.f);
		FPostProcessSettings& PP = Post->Settings;
		PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 1.4f; PP.bOverride_BloomThreshold = true; PP.BloomThreshold = 0.95f;
		PP.bOverride_ColorContrast = true; PP.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
		PP.bOverride_ColorSaturation = true; PP.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.f);
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.35f;
		AddPanoramaSky(TEXT("NightCircuit"), 0.8f, 0.f, FLinearColor::Black);
		AddPointLight(FVector(0, 0, 280.f), ZoogiUtil::Hex(TEXT("#8FEFFF")), 40.f, 1400.f);
		// Oct 14: the real-time sky light captures a near-black sky, so the stands, pagodas and islets get soft cool fills
		// (shadowless, outside the court) to read like the target render.
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = FMath::DegreesToRadians(30.f + 60.f * k);
			AddPointLight(FVector(FMath::Cos(A) * 2700.f, FMath::Sin(A) * 2700.f, 900.f), ZoogiUtil::Hex(TEXT("#B4BEFF")), 500.f, 2600.f);
		}
	}
	TMap<FName, UMaterialInterface*> Mats;
	for (const ZoogiInc::FMat& M : ZoogiInc::Mats)
	{
		if (M.Theme != Th) continue;
		const FLinearColor Base(M.R, M.G, M.B), Em(M.ER, M.EG, M.EB);
		float EmS = Em.GetMax() * M.EStr;
		if (Th == (int32)EZoogiArena::Neon) // NC IMPORT.md pass 9 / 07 LIGHTING.md: core 1.2 > core-hot beam 1.0 > trims, beams, crystals 0.8 > column, screens 0.6 > crowd, holo 0.5
		{
			const FString Sl(M.Slot);
			const float Cap = Sl.Contains(TEXT("CoreSphere")) ? 1.2f : Sl.Contains(TEXT("CoreHot")) ? 1.0f : (Sl.Contains(TEXT("LightColumn")) || Sl.Contains(TEXT("Screen"))) ? 0.6f
				: (Sl.Contains(TEXT("Crowd")) || Sl.Contains(TEXT("Holo"))) ? 0.5f : 0.8f;
			EmS = FMath::Min(EmS, Cap);
		}
		else if (Th == (int32)EZoogiArena::Tomb) EmS = FMath::Min(EmS, 1.2f); // 06 LIGHTING.md: nothing brighter than the sun orb
		// Oct 14: NC IMPORT.md marks the beams, light column and holo slots translucent additive. As opaque GlowMIDs they
		// rendered as solid cyan blocks, so they get the additive FX material.
		const FString SlotS(M.Slot);
		if (Th == (int32)EZoogiArena::Neon && (SlotS.Contains(TEXT("Beam")) || SlotS.Contains(TEXT("LightColumn")) || SlotS.Contains(TEXT("Holo"))))
		{
			Mats.Add(FName(M.Slot), ZoogiUtil::FxMID(this, Em, FMath::Max(EmS, 0.3f)));
			continue;
		}
		// Oct 14: the NC albedos are near-black (wood 0.014, metal 0.03), which only reads under the pack's preview lights;
		// in game the stands, pagodas and islets went black. Lift the dark non-emissive albedos (hue kept, max x5).
		FLinearColor BaseL = Base;
		if (Th == (int32)EZoogiArena::Neon && Em.GetMax() <= 0.f)
			BaseL = FLinearColor(FMath::Min(Base.R * 5.f, 1.f), FMath::Min(Base.G * 5.f, 1.f), FMath::Min(Base.B * 5.f, 1.f));
		UMaterialInterface* Mi = (EmS > 0.6f && Base.GetLuminance() < Em.GetLuminance())
			? (UMaterialInterface*)ZoogiUtil::GlowMID(this, Em, FMath::Min(EmS, 6.f))
			: (UMaterialInterface*)ZoogiUtil::SurfaceMID(this, BaseL, M.Rough, Em, FMath::Min(EmS, 3.f), Th == (int32)EZoogiArena::Neon ? M.Metal * 0.5f : M.Metal);
		Mats.Add(FName(M.Slot), Mi);
	}
	if (Th == (int32)EZoogiArena::Neon && Mats.Contains(TEXT("M_NC_Beam"))) Mats.Add(TEXT("M_NC_BeamGlowCyan"), Mats[TEXT("M_NC_Beam")]); // slot missing from the table
	auto Dress = [&](UStaticMeshComponent* C, UStaticMesh* SM)
	{
		C->SetStaticMesh(SM);
		for (int32 k = 0; k < SM->GetStaticMaterials().Num(); ++k)
		{
			UMaterialInterface** F = Mats.Find(SM->GetStaticMaterials()[k].ImportedMaterialSlotName);
			if (!F) F = Mats.Find(SM->GetStaticMaterials()[k].MaterialSlotName);
			C->SetMaterial(k, F ? *F : SM->GetMaterial(k));
		}
	};
	TArray<FBox> Blocking;
	for (UActorComponent* C : Built)
		if (UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(C))
			if (PC->GetCollisionEnabled() != ECollisionEnabled::NoCollision) Blocking.Add(PC->Bounds.GetBox());
	for (AZoogiArenaPiece* P : Pieces) if (P) Blocking.Add(P->GetComponentsBoundingBox(true));
	TSharedRef<TArray<FSwap>> Swaps = MakeShared<TArray<FSwap>>();
	UPhysicalMaterial* PillarPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.6f, false);
	const float KR = ZoogiConst::KnockoffRadius;
	int32 NPieces = 0, NPillars = 0, NVis = 0, NMiss = 0, NSwap = 0, NHalo = 0;
	UMaterialInterface* PlanetMat = Th == (int32)EZoogiArena::Cosmic ? ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#2E2866")), 0.55f, ZoogiUtil::Hex(TEXT("#5B4BA8")), 0.08f, 0.f) : nullptr; // M_PlanetBody / BandDark
	for (const ZoogiInc::FPlace& Pl : ZoogiInc::Places)
	{
		if (Pl.Theme != Th || bNoInc) continue;
		const TArray<UStaticMesh*>& Set = Sets[0][Pl.Mesh];
		if (Set.Num() == 0) { ++NMiss; continue; }
		FVector L(Pl.X, Pl.Y, Pl.Z);
		FRotator R(0.f, Pl.Yaw, 0.f);
		// Oct 14: unit-length pieces (PLACEMENT.json "scale X = length") scale along X only; a uniform scale had turned the
		// Night Circuit light beams and cables into giant cyan slabs and shrunk the bridges.
		const FString MeshName(ZoogiInc::Meshes[Pl.Mesh]);
		FVector S3(Pl.Scale);
		if (MeshName.Contains(TEXT("LightBeam")) || MeshName.Contains(TEXT("_Cable")) || MeshName.StartsWith(TEXT("SM_NC_Bridge"))) S3 = FVector(Pl.Scale, 1.f, 1.f);
		else if (MeshName == TEXT("SM_ZArch")) S3 = FVector(1.f, 1.40f, 1.06f) * Pl.Scale; // 04 IMPORT.md art pass 2
		else if (MeshName == TEXT("SM_HaloRing"))
		{
			// 04 IMPORT.md art pass 2: two halo rings at yaw 241.5 / 298.5, r 6260 world (/1.3 kit), Z 0, tilt X 72 pitch 12,
			// each around a planet (r 468 world), framing the arch in the match camera.
			const float Yw = FMath::DegreesToRadians(NHalo++ == 0 ? 241.5f : 298.5f), Rk = 6260.f / 1.3f;
			L = FVector(FMath::Cos(Yw) * Rk, FMath::Sin(Yw) * Rk, 1400.f); // raised so they frame the arch in the match camera
			R = FRotator(12.f, FMath::RadiansToDegrees(Yw), 72.f);
			S3 = FVector(1.f);
			if (!bNoInc)
			{
				UStaticMeshComponent* Planet = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), PlanetMat, false, false);
				Planet->SetRelativeLocation(L);
				Planet->SetRelativeScale3D(FVector(468.f / 1.3f / 50.f));
			}
		}
		const float Rd = FVector2D(Pl.X, Pl.Y).Size();
		const float FormDelay = 0.35f + 1.6f * FMath::Clamp((Rd - KR * 0.5f) / (KR * 3.f), 0.f, 1.f);
		FBox Local(ForceInit);
		for (UStaticMesh* SM : Set) Local += SM->GetBoundingBox();
		int32 Kind = Local.IsValid ? Pl.Kind : 0;
		if (Kind == 1)
		{
			const FBox W = Local.TransformBy(FTransform(R, L, S3) * GetActorTransform()).ExpandBy(-4.f);
			for (const FBox& B : Blocking) if (B.Intersect(W)) { Kind = 0; break; }
		}
		if (Kind > 0)
		{
			const bool bPillar = Kind == 2;
			AZoogiArenaPiece* P = SpawnPiece(L, Pl.Yaw, bPillar ? FName(*FString::Printf(TEXT("pillar_%d"), NPillars)) : FName(TEXT("prop")), TEXT("Stone"), bPillar ? 1000.f : 200.f, bPillar);
			if (!P) continue;
			FSwap Sw; Sw.P = P;
			for (UStaticMesh* SM : Set)
			{
				UStaticMeshComponent* C = P->AddShape(TEXT("Cube"), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, S3), nullptr);
				Dress(C, SM);
				Sw.S[0].Add(C);
			}
			if (bPillar)
			{
				const float Rad = FMath::Max(Local.GetExtent().X, Local.GetExtent().Y) * Pl.Scale * 0.8f;
				P->AddColliderCyl(FVector2D::ZeroVector, Rad, 0.f, Local.Max.Z * Pl.Scale, TEXT("ZPillar"), PillarPM);
				P->SetPathBlock(FVector2D(P->GetActorLocation()), Rad * BuildScale);
			}
			else
			{
				P->AddColliderBox(Local.GetCenter() * Pl.Scale, Local.GetSize() * Pl.Scale * 0.85f, FRotator::ZeroRotator, TEXT("ZWall"), RockPM);
				P->RubbleKeepZ = 25.f;
			}
			P->FinishBuild();
			for (int32 s = 1; s < 3 && !bPillar; ++s)
				for (UStaticMesh* SM : Sets[s][Pl.Mesh])
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(P);
					C->SetupAttachment(P->GetRootComponent());
					C->SetRelativeScale3D(S3);
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->SetVisibility(false);
					C->RegisterComponent();
					Dress(C, SM);
					Sw.S[s].Add(C);
				}
			if (Sw.S[1].Num() || Sw.S[2].Num()) { Swaps->Add(Sw); ++NSwap; }
			Blocking.Add(Local.TransformBy(FTransform(R, L, S3) * GetActorTransform()));
			if (Forming)
			{
				const float H = FMath::Max(Local.Max.Z * Pl.Scale, 60.f) + 120.f;
				if (bPillar) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -H), 1.30f + 0.10f * NPillars, 1.0f, 14.f);
				else Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -H), FormDelay, 0.7f, 6.f);
			}
			if (bPillar) { UE_LOG(LogZoogi, Log, TEXT("Incoming pillar_%d (%s) at (%.0f, %.0f): unbreakable, rises at %.2f s, collision on once risen"), NPillars, ZoogiInc::Meshes[Pl.Mesh], P->GetActorLocation().X, P->GetActorLocation().Y, 1.30f + 0.10f * NPillars); }
			else { UE_LOG(LogZoogi, Log, TEXT("Incoming piece %s at (%.0f, %.0f): breakable, %d tintable materials, damage meshes %d/%d"), ZoogiInc::Meshes[Pl.Mesh], P->GetActorLocation().X, P->GetActorLocation().Y, Sw.S[0].Num(), Sw.S[1].Num(), Sw.S[2].Num()); }
			if (bPillar) ++NPillars; else ++NPieces;
		}
		else
		{
			for (UStaticMesh* SM : Set)
			{
				UStaticMeshComponent* C = AddMesh(SM, nullptr, false, true);
				Dress(C, SM);
				C->SetRelativeLocationAndRotation(L, R);
				C->SetRelativeScale3D(S3);
				if (Forming && Rd < KR * 4.f) Forming->AddComponent(C, EZoogiFormStyle::Rise, FVector(0, 0, -300.f), FormDelay, 0.8f, 0.f);
			}
			++NVis;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Incoming pack (theme %d): %d pieces, %d pillars, %d visual, %d with damage meshes, %d missing meshes, %d materials"), Th, NPieces, NPillars, NVis, NSwap, NMiss, Mats.Num());

	// Destruction mesh swap: poll the piece states (no Piece.cpp change).
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(GSwapTimer);
		if (Swaps->Num())
			W->GetTimerManager().SetTimer(GSwapTimer, FTimerDelegate::CreateLambda([Swaps]()
			{
				for (FSwap& Sw : *Swaps)
				{
					AZoogiArenaPiece* P = Sw.P.Get();
					if (!P) continue;
					const EZoogiPieceState St = P->GetPieceState();
					int32 Want = St == EZoogiPieceState::Broken ? 2 : (St == EZoogiPieceState::Cracked ? 1 : 0);
					while (Want > 0 && Sw.S[Want].Num() == 0) --Want;
					if (Want == Sw.Shown) continue;
					for (int32 s = 0; s < 3; ++s)
						for (auto& C : Sw.S[s]) if (C.IsValid()) C->SetVisibility(s == Want);
					Sw.Shown = Want;
				}
			}), 0.1f, true);
	}

	// -ZoogiArenaPlayTest on the non-Meadow maps: piece overlap + an orb launched out on 4 headings must still go ORB OUT.
	if (AZoogiArenaPlayTest* PT = AZoogiArenaPlayTest::MaybeSpawn(this))
	{
		{ FZoogiArenaCheck C; C.Name = TEXT("piece_overlap"); C.Kind = FZoogiArenaCheck::PieceOverlap; C.Wait = 0.2f; PT->Checks.Add(C); }
		for (int32 g = 0; g < 4; ++g)
		{
			const float Yw = 90.f * g, Rad = FMath::DegreesToRadians(Yw);
			const FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("gap_%03.0f"), Yw); C.Kind = FZoogiArenaCheck::OrbOut;
			C.From = FVector(Dir * 1000.f, 0.f); C.Vel = Dir * 2200.f; C.Wait = 3.0f; // Oct 7: fire from past the match orb rack (r 500-900)
			PT->Checks.Add(C);
		}
	}
}
