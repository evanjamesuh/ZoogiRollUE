# b81 (Claude, Evan 07:14 / 07:17): Bolt Balls purple lightning arcs (KriptoFX-style stems, generated in-engine), clash split shows each
# Zoogi's own ultimate art (+ per-Zoogi clip lookup), splat cracks generated along the real wall (stage pieces crack their own material)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
# ---------------- Bolt Balls: purple lightning stems
ed(P + 'ZoogiSkills.cpp', [
 ('''	static TArray<FBoltShot> GBoltShots;
	static float GStemT = 0.f;''',
  '''	static TArray<FBoltShot> GBoltShots;
	static float GStemT = 0.f;
	// b81 (Evan: "Bolt Balls look amazing but still not the intended purple lightning"): KriptoFX-style purple ground-strike arcs,
	// generated live: jagged cylinder chains from the Zoogi's top arcing down to the floor, re-struck every 50 ms
	static const FLinearColor GBoltPurple(0.62f, 0.32f, 1.f);
	struct FStem { TWeakObjectPtr<AActor> A; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f; };
	static TMap<int32, FStem> GStems;
	constexpr int32 StemSegs = 9, StemMax = 3;
	static FStem MakeStem(UWorld* W, const FVector& At)
	{
		FStem S;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), SP);
		if (!A) return S;
		USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		A->SetRootComponent(Root); Root->RegisterComponent();
		UMaterialInterface* Core = ZoogiUtil::FxMID(A, FLinearColor(0.85f, 0.7f, 1.f), 40.f);
		UMaterialInterface* Glow = ZoogiUtil::FxMID(A, GBoltPurple, 18.f);
		UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
		for (int32 i = 0; i < StemMax * StemSegs * 2; ++i)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetStaticMesh(Cyl); C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			C->SetupAttachment(Root); C->RegisterComponent();
			C->SetMaterial(0, (i % 2) ? Glow : Core);
			C->SetVisibility(false);
			S.Segs.Add(C);
		}
		S.A = A;
		return S;
	}
	static void SetSeg(UStaticMeshComponent* C, const FVector& P0, const FVector& P1, float Th)
	{
		const FVector D = P1 - P0; const float L = D.Size();
		if (!C || L < 1.f) { if (C) C->SetVisibility(false); return; }
		C->SetVisibility(true);
		C->SetWorldLocationAndRotation((P0 + P1) * 0.5f, FRotationMatrix::MakeFromZ(D / L).Rotator());
		C->SetWorldScale3D(FVector(Th / 100.f, Th / 100.f, L / 100.f));
	}
	static void StrikeStems(UWorld* W, FStem& S, const AZoogiBall* B, int32 N, float Tm)
	{
		const FVector Top = B->BallLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.55f);
		const float FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius + 2.f;
		for (int32 k = 0; k < StemMax; ++k)
		{
			const bool bOn = k < N;
			const float Ang = FMath::DegreesToRadians(Tm * 40.f + k * 120.f + FMath::FRandRange(-12.f, 12.f));
			const float Reach = FMath::FRandRange(150.f, 210.f);
			const FVector End(B->BallLocation().X + FMath::Cos(Ang) * Reach, B->BallLocation().Y + FMath::Sin(Ang) * Reach, FloorZ);
			const FVector Mid = (Top + End) * 0.5f + FVector(0.f, 0.f, FMath::FRandRange(70.f, 120.f));
			FVector Prev = Top;
			for (int32 i = 1; i <= StemSegs; ++i)
			{
				const float t = (float)i / StemSegs;
				FVector Pt = FMath::Lerp(FMath::Lerp(Top, Mid, t), FMath::Lerp(Mid, End, t), t); // quadratic arc
				if (i < StemSegs) Pt += FVector(FMath::FRandRange(-16.f, 16.f), FMath::FRandRange(-16.f, 16.f), FMath::FRandRange(-12.f, 12.f));
				UStaticMeshComponent* CoreC = S.Segs.IsValidIndex((k * StemSegs + i - 1) * 2) ? S.Segs[(k * StemSegs + i - 1) * 2].Get() : nullptr;
				UStaticMeshComponent* GlowC = S.Segs.IsValidIndex((k * StemSegs + i - 1) * 2 + 1) ? S.Segs[(k * StemSegs + i - 1) * 2 + 1].Get() : nullptr;
				if (bOn) { SetSeg(CoreC, Prev, Pt, 2.6f); SetSeg(GlowC, Prev, Pt, 7.5f); }
				else { if (CoreC) CoreC->SetVisibility(false); if (GlowC) GlowC->SetVisibility(false); }
				Prev = Pt;
			}
			if (bOn && FMath::FRand() < 0.35f) AZoogiFX::Spawn(W, End + FVector(0.f, 0.f, 4.f), GBoltPurple, 70.f, 0.18f, true, 10.f); // ground strike flash
		}
	}'''),
 ('''		// the lightning stems: one per bolt left, round the Zoogi that holds them
		GStemT += Dt;
		if (GStemT >= 0.3f)''',
  '''		// b81: purple lightning arcs, one per bolt left (re-struck every 50 ms); gone with the last bolt
		for (auto It = GStems.CreateIterator(); It; ++It)
		{
			const int32 N = BoltCount(G, It.Key());
			AZoogiBall* Hb = nullptr; for (AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == It.Key()) Hb = B;
			if (N <= 0 || !Hb || Hb->bOut || !It.Value().A.IsValid()) { if (AActor* A = It.Value().A.Get()) A->Destroy(); It.RemoveCurrent(); continue; }
		}
		for (const TPair<int32, int32>& Kv : GBolts)
			for (AZoogiBall* B : G->Balls)
				if (B && B->PlayerIndex == Kv.Key && !B->bOut)
				{
					FStem* S = GStems.Find(Kv.Key);
					if (!S) S = &GStems.Add(Kv.Key, MakeStem(W, B->BallLocation()));
					S->T += Dt;
					if (S->T >= 0.05f) { S->T = 0.f; StrikeStems(W, *S, B, FMath::Min(Kv.Value, StemMax), W->GetTimeSeconds()); }
				}
		// the lightning stems: one per bolt left, round the Zoogi that holds them
		GStemT += Dt;
		static const bool bOldStems = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltSparkStems"));
		if (bOldStems && GStemT >= 0.3f)'''),
 ('M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.55f, 0.6f, 1.f), 12.f));', 'M->SetMaterial(0, ZoogiUtil::FxMID(A, GBoltPurple, 16.f)); // b81: purple'),
 ('if (Bs.Spark > 0.07f) { Bs.Spark = 0.f; AZoogiFX::Spawn(W, Bs.P, FLinearColor(0.6f, 0.65f, 1.f), 60.f, 0.25f, false, 8.f); }',
  'if (Bs.Spark > 0.05f) { Bs.Spark = 0.f; AZoogiFX::Spawn(W, Bs.P, GBoltPurple, 70.f, 0.25f, false, 10.f); }'),
 ('		const FLinearColor Col(0.55f, 0.6f, 1.f);\n', '		const FLinearColor Col = GBoltPurple;\n'),
])
# ---------------- clash split: each side shows its own Zoogi
ed(P + 'ZoogiClashSplit.cpp', [
 ('''		TStrongObjectPtr<UFileMediaSource> Src;
		FString Clip;
	};''', '''		TStrongObjectPtr<UFileMediaSource> Src;
		FString Clip;
		TWeakObjectPtr<UTexture2D> Art; // b81: the Zoogi's own ultimate art over its half
	};'''),
 ('''		TArray<FString> Pool;
		if (B)
		{''', '''		TArray<FString> Pool;
		if (B) Pool.Add(FString::Printf(TEXT("Anime_Zoogi_%s_Clash_01"), *B->Def.Name.Replace(TEXT(" "), TEXT("")))); // b81: a clip made for this Zoogi wins when it exists
		if (B)
		{'''),
 ('''		SplitOpen(GSplitSides[0], L);
		SplitOpen(GSplitSides[1], R);
		bSplitOn = !L.IsEmpty() || !R.IsEmpty();''', '''		SplitOpen(GSplitSides[0], L);
		SplitOpen(GSplitSides[1], R);
		// b81 (Evan: "for the clash the opposing player needs to be generated, not the ball-class anime clip"): each half shows that
		// Zoogi's own ultimate art, big, over its (dimmed) clip. -ZoogiClashClipsOnly = clips alone
		static const bool bClipsOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashClipsOnly"));
		UZoogiFunSys* Fs = UZoogiFunSys::Get(W);
		GSplitSides[0].Art = (!bClipsOnly && Fs && Left) ? Fs->UltAvatar(Left) : nullptr;
		GSplitSides[1].Art = (!bClipsOnly && Fs && Right) ? Fs->UltAvatar(Right) : nullptr;
		bSplitOn = !L.IsEmpty() || !R.IsEmpty() || GSplitSides[0].Art.IsValid() || GSplitSides[1].Art.IsValid();'''),
 ('''			const FSide& Sd = GSplitSides[s];
			if (!Sd.Tex.IsValid() || !Sd.Tex->GetResource()) continue;
			const FTexture* Res = Sd.Tex->GetResource();
			for (int32 i = 0; i < N; ++i)
			{
				const float Y0 = H * i / N, Y1 = H * (i + 1) / N;
				const float Xe = (Seam[i].X + Seam[i + 1].X) * 0.5f;      // this row's edge
				const float X0 = s == 0 ? 0.f : FMath::Max(Xe, 0.f), X1 = s == 0 ? FMath::Min(Xe, W) : W;
				if (X1 - X0 < 1.f) continue;
				const FVector2D UV0(0.5f + (X0 - Cx[s]) / VW, 0.5f + (Y0 - H * 0.5f) / VH), UV1(0.5f + (X1 - Cx[s]) / VW, 0.5f + (Y1 - H * 0.5f) / VH);
				FCanvasTileItem Tile(FVector2D(X0, Y0), Res, FVector2D(X1 - X0, Y1 - Y0 + 0.5f), UV0, UV1, Wt);
				Tile.BlendMode = SE_BLEND_Translucent;
				C->DrawItem(Tile);
			}''',
  '''			const FSide& Sd = GSplitSides[s];
			const FTexture* Res = (Sd.Tex.IsValid() && Sd.Tex->GetResource() && !Sd.Clip.IsEmpty()) ? Sd.Tex->GetResource() : nullptr;
			UTexture2D* Art = Sd.Art.Get();
			const FTexture* ARes = Art ? Art->GetResource() : nullptr;
			if (!Res && !ARes) continue;
			const FLinearColor Ec = s == 0 ? LCol : RCol;
			const float Zm = 1.f + 0.05f * FMath::Sin(T * 1.7f + s * 1.3f);
			const float As = FMath::Min(H * 0.8f, (s == 0 ? FMath::Max(Xs, W * 0.3f) : FMath::Max(W - Xs, W * 0.3f)) * 0.95f) * Zm;
			const FVector2D AC(Cx[s] + 6.f * U * FMath::Sin(T * 23.f + s * 2.f), H * 0.54f + 5.f * U * FMath::Sin(T * 19.f + s));
			const float AX0 = AC.X - As * 0.5f, AY0 = AC.Y - As * 0.5f;
			for (int32 i = 0; i < N; ++i)
			{
				const float Y0 = H * i / N, Y1 = H * (i + 1) / N;
				const float Xe = (Seam[i].X + Seam[i + 1].X) * 0.5f;      // this row's edge
				const float X0 = s == 0 ? 0.f : FMath::Max(Xe, 0.f), X1 = s == 0 ? FMath::Min(Xe, W) : W;
				if (X1 - X0 < 1.f) continue;
				if (Res)
				{
					const FVector2D UV0(0.5f + (X0 - Cx[s]) / VW, 0.5f + (Y0 - H * 0.5f) / VH), UV1(0.5f + (X1 - Cx[s]) / VW, 0.5f + (Y1 - H * 0.5f) / VH);
					FCanvasTileItem Tile(FVector2D(X0, Y0), Res, FVector2D(X1 - X0, Y1 - Y0 + 0.5f), UV0, UV1, Wt);
					Tile.BlendMode = SE_BLEND_Translucent;
					C->DrawItem(Tile);
				}
				if (!ARes) continue;
				// the clip goes dim and takes the Zoogi's colour; the Zoogi's own art fills its half
				FCanvasTileItem Dim(FVector2D(X0, Y0), FVector2D(X1 - X0, Y1 - Y0 + 0.5f), FLinearColor(Ec.R * 0.22f, Ec.G * 0.22f, Ec.B * 0.22f, (Res ? 0.55f : 0.95f) * Alpha));
				Dim.BlendMode = SE_BLEND_Translucent; C->DrawItem(Dim);
				const float Ix0 = FMath::Max(X0, AX0), Ix1 = FMath::Min(X1, AX0 + As), Iy0 = FMath::Max(Y0, AY0), Iy1 = FMath::Min(Y1 + 0.5f, AY0 + As);
				if (Ix1 - Ix0 < 1.f || Iy1 - Iy0 < 0.5f) continue;
				FCanvasTileItem ArtT(FVector2D(Ix0, Iy0), ARes, FVector2D(Ix1 - Ix0, Iy1 - Iy0), FVector2D((Ix0 - AX0) / As, (Iy0 - AY0) / As), FVector2D((Ix1 - AX0) / As, (Iy1 - AY0) / As), Wt);
				ArtT.BlendMode = SE_BLEND_Translucent;
				C->DrawItem(ArtT);
			}'''),
], inc='#include "ZoogiActionFun.h"\n#include "Engine/Texture2D.h"', after='#include "ZoogiClashSplit.h"')
# ---------------- splat: generated cracks along the wall
ed(P + 'ZoogiStrikeFun.cpp', [
 ('''			// the stands crack: a crater turned to face out of the wall
			if (AZoogiCrackFX* Cr = AZoogiCrackFX::Spawn(W, H.ImpactPoint, 170.f, AZoogiCrackFX::StyleForMap(W), 3.5f, 1.3f, TEXT("splat on the stands")))
				Cr->SetActorLocationAndRotation(H.ImpactPoint + Flung.SplatN * 3.f, FRotationMatrix::MakeFromZ(Flung.SplatN).Rotator());''',
  '''			// b81 (Evan: "the wall should have generated cracks like smashing the snowman platform, not a crack form that doesn't align"):
			// a stage piece cracks its own surface (the piece crack material + shards); anything else gets cracks generated along the wall
			static const bool bOldCrack = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSplatCrack"));
			AZoogiArenaPiece* HitPiece = Cast<AZoogiArenaPiece>(H.GetActor());
			if (bOldCrack)
			{
				if (AZoogiCrackFX* Cr = AZoogiCrackFX::Spawn(W, H.ImpactPoint, 170.f, AZoogiCrackFX::StyleForMap(W), 3.5f, 1.3f, TEXT("splat on the stands")))
					Cr->SetActorLocationAndRotation(H.ImpactPoint + Flung.SplatN * 3.f, FRotationMatrix::MakeFromZ(Flung.SplatN).Rotator());
			}
			else if (HitPiece && !HitPiece->IsIndestructible() && !HitPiece->IsBroken())
				HitPiece->SetPieceState(EZoogiPieceState::Cracked, H.ImpactPoint, -Flung.SplatN, true);
			else
				ZoogiWallCracks(W, H.ImpactPoint, Flung.SplatN, 190.f);'''),
 ('''void UZoogiFunSys::''', '''// b81: jagged branching cracks laid flat on the wall plane at P (normal N): thin dark slivers that fork outward, gone after 8 s
static void ZoogiWallCracks(UWorld* W, const FVector& P, const FVector& N0, float Radius)
{
	if (!W) return;
	const FVector N = N0.GetSafeNormal();
	FVector Ua = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
	if (Ua.IsNearlyZero()) Ua = FVector(1.f, 0.f, 0.f);
	const FVector Va = FVector::CrossProduct(N, Ua).GetSafeNormal();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(P), SP);
	if (!A) return;
	USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
	A->SetRootComponent(Root); Root->RegisterComponent();
	A->SetLifeSpan(8.f);
	UMaterialInterface* Dark = ZoogiUtil::SurfaceMID(A, FLinearColor(0.035f, 0.032f, 0.04f), 0.95f);
	UMaterialInterface* Edge = ZoogiUtil::SurfaceMID(A, FLinearColor(0.75f, 0.78f, 0.85f), 0.8f);
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	FRandomStream R((int32)(FPlatformTime::Cycles() & 0x7fffffff));
	int32 Segs = 0;
	auto Seg = [&](const FVector2D& A2, const FVector2D& B2, float Th)
	{
		if (Segs > 160) return;
		const FVector P0 = P + N * 1.5f + Ua * A2.X + Va * A2.Y, P1 = P + N * 1.5f + Ua * B2.X + Va * B2.Y;
		const FVector D = P1 - P0; const float L = D.Size(); if (L < 1.f) return;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetStaticMesh(Cube); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			C->SetupAttachment(Root); C->RegisterComponent();
			C->SetMaterial(0, Pass == 0 ? Edge : Dark);
			const float T2 = Pass == 0 ? Th * 1.8f : Th;
			C->SetWorldLocationAndRotation((P0 + P1) * 0.5f + N * (Pass == 0 ? 0.f : 0.6f), FRotationMatrix::MakeFromXZ(D / L, N).Rotator());
			C->SetWorldScale3D(FVector((L + T2 * 0.5f) / 100.f, T2 / 100.f, 0.012f));
			++Segs;
		}
	};
	TFunction<void(FVector2D, float, float, float, int32)> Crack = [&](FVector2D At, float Ang, float Len, float Th, int32 Depth)
	{
		float Left = Len;
		while (Left > 6.f)
		{
			const float Step = FMath::Min(Left, R.FRandRange(12.f, 26.f));
			Ang += FMath::DegreesToRadians(R.FRandRange(-24.f, 24.f));
			const FVector2D Nx = At + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Step;
			Seg(At, Nx, Th);
			At = Nx; Left -= Step; Th = FMath::Max(1.2f, Th * 0.9f);
			if (Depth < 2 && R.FRand() < 0.28f) Crack(At, Ang + FMath::DegreesToRadians(R.FRandRange(25.f, 55.f) * (R.FRand() < 0.5f ? -1.f : 1.f)), Left * R.FRandRange(0.35f, 0.6f), Th * 0.7f, Depth + 1);
		}
	};
	const int32 Arms = 7 + R.RandRange(0, 2);
	for (int32 k = 0; k < Arms; ++k) Crack(FVector2D::ZeroVector, UE_TWO_PI * k / Arms + R.FRandRange(-0.3f, 0.3f), Radius * R.FRandRange(0.55f, 1.f), R.FRandRange(4.f, 6.5f), 0);
	// a crushed ring round the impact
	FVector2D Prev(0.f, 0.f);
	for (int32 k = 0; k <= 10; ++k)
	{
		const float Ang = UE_TWO_PI * k / 10.f;
		const FVector2D Pt = FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (Radius * 0.22f * R.FRandRange(0.8f, 1.2f));
		if (k > 0) Seg(Prev, Pt, 3.f);
		Prev = Pt;
	}
	UE_LOG(LogZoogi, Log, TEXT("SPLAT wall cracks: %d slivers at (%.0f,%.0f,%.0f)"), Segs, P.X, P.Y, P.Z);
}

void UZoogiFunSys::'''),
])
print('b81 ok')
# ---------------- MENU: straight, always tappable (even under the P5 command menu)
H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiArenaHUDWidget.h', [
 ('''public:
	/** Spawns (or keeps) the one HUD layer for this world. Called every frame by the viewport client. */
	static void Ensure(UWorld* World);
''', '''public:
	/** Spawns (or keeps) the one HUD layer for this world. Called every frame by the viewport client. */
	static void Ensure(UWorld* World);
	/** b81: the MENU button's action (also reached from overlays that sit above it) */
	void ToggleMenu() { SetPaused(!bPaused); }
'''),
 ('	ZOOGIROLL_API bool OverChrome(const UWorld* World, const FVector2D& ViewportPx);\n',
  '	ZOOGIROLL_API bool OverChrome(const UWorld* World, const FVector2D& ViewportPx);\n	/** b81: absolute Slate point on the MENU button -> open / close the pause menu; true when it was */\n	ZOOGIROLL_API bool PressMenuAt(const FVector2D& AbsPos);\n'),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('''	static TWeakObjectPtr<UWidget> GMenuPlate; // b79''',
  '''	static TWeakObjectPtr<UWidget> GMenuPlate; // b79
	static TWeakObjectPtr<UZoogiArenaHUDWidget> GMenuOwner; // b81
	bool PressMenuAt(const FVector2D& Abs)
	{
		const UWidget* W = GMenuPlate.Get();
		UZoogiArenaHUDWidget* O = GMenuOwner.Get();
		if (!W || !O || !W->GetCachedWidget().IsValid()) return false;
		const FGeometry& G = W->GetCachedGeometry();
		const FVector2D P = FVector2D(G.GetAbsolutePosition()), S = FVector2D(G.GetAbsoluteSize());
		const float Pad = 14.f;
		if (S.X < 4.f || Abs.X < P.X - Pad || Abs.Y < P.Y - Pad || Abs.X > P.X + S.X + Pad || Abs.Y > P.Y + S.Y + Pad) return false;
		O->ToggleMenu();
		return true;
	}'''),
 ('''		const float MW = 150.f, MH = 62.f, MX = CogX + Sz - MW, MY = CogY + (Sz - MH) * 0.5f;
		auto Tilt = [](UWidget* Wg, float Deg) { if (Wg) { Wg->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); Wg->SetRenderTransformAngle(Deg); } return Wg; };
		Tilt(At(Box(ZCHex(TEXT("0A0A0C")), FLinearColor::Transparent, 0.f, 0.f), MX + 7.f, MY + 7.f, MW, MH), -6.f);
		Tilt(At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 0.f), MX - 4.f, MY - 4.f, MW + 8.f, MH + 8.f), -6.f);
		UWidget* Plate = Tilt(At(Box(ZCHex(TEXT("E0101E")), FLinearColor::Transparent, 0.f, 0.f), MX, MY, MW, MH), -6.f);
		Tilt(Text(TEXT("MENU"), MX, MY - 2.f, MW, MH, 34.f, FLinearColor::White, 2.f, 1), -6.f);
		ZoogiArenaHUD::GMenuPlate = Plate;''',
  '''		// b81 (Evan: "make MENU selectable no matter what, straight, label not tilted"): no tilt, never part of the profile fade
		const float MW = 150.f, MH = 62.f, MX = CogX + Sz - MW, MY = CogY + (Sz - MH) * 0.5f;
		At(Box(ZCHex(TEXT("0A0A0C")), FLinearColor::Transparent, 0.f, 0.f), MX + 6.f, MY + 6.f, MW, MH);
		At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 0.f), MX - 4.f, MY - 4.f, MW + 8.f, MH + 8.f);
		UWidget* Plate = At(Box(ZCHex(TEXT("E0101E")), FLinearColor::Transparent, 0.f, 0.f), MX, MY, MW, MH);
		Text(TEXT("MENU"), MX, MY - 2.f, MW, MH, 34.f, FLinearColor::White, 2.f, 1);
		ZoogiArenaHUD::GMenuPlate = Plate;
		ZoogiArenaHUD::GMenuOwner = this;'''),
 ('''	const int32 Mark = Canvas->GetChildrenCount();
	BuildChromeCluster(CogX, Y, true, false);
	(void)TacX;
	MarkSplit(Mark, FVector2D(0.f, -46.f), 4);''',
  '''	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome"))) { BuildChromeCluster(CogX, Y, true, false); (void)TacX; return; } // b81: MENU never fades away with the profiles
	const int32 Mark = Canvas->GetChildrenCount();
	BuildChromeCluster(CogX, Y, true, false);
	(void)TacX;
	MarkSplit(Mark, FVector2D(0.f, -46.f), 4);'''),
])
ed(P + 'ZoogiP5Command.cpp', [
 ('''			if (!bMenu || bPuppet) return FReply::Unhandled();
			int32 Kind = -1;''',
  '''			if (ZoogiArenaHUD::PressMenuAt(Screen)) return Took(); // b81: MENU works even under the command menu
			if (!bMenu || bPuppet) return FReply::Unhandled();
			int32 Kind = -1;'''),
], inc='#include "ZoogiArenaHUDWidget.h"', after='#include "ZoogiP5Command.h"')
# ---------------- constant vibration through the clash
ed(P + 'ZoogiSuperFun.cpp', [
 ('''void UZoogiFunSys::TickClash(AZoogiGameMode* G, float RealDt)
{
	Clash.Age += RealDt;''',
  '''void UZoogiFunSys::TickClash(AZoogiGameMode* G, float RealDt)
{
	Clash.Age += RealDt;
	{	// b81 (Evan: "constant vibration during the clash"): a steady rumble for the whole clash, topped up every 0.4 s
		static double NextBuzz = 0.0;
		const double Nw = FPlatformTime::Seconds();
		if (Clash.Age < RealDt * 1.5f) NextBuzz = 0.0;
		if (Nw >= NextBuzz && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoClashBuzz"))) { NextBuzz = Nw + 0.4; ZoogiHaptics::Rumble(0.5f, Clash.bFinish ? 1.f : 0.65f, GetWorld(), TEXT("clash")); }
	}'''),
])
print('b81 menu/buzz ok')
