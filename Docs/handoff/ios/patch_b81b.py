# b81 (Claude, Evan 07:14 / 07:17): Bolt Balls purple lightning arcs (KriptoFX-style stems, generated in-engine), clash split shows each
# Zoogi's own ultimate art (+ per-Zoogi clip lookup), splat cracks generated along the real wall (stage pieces crack their own material)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
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
 ('''void UZoogiFunSys::StartStrike(''', '''// b81: jagged branching cracks laid flat on the wall plane at P (normal N): thin dark slivers that fork outward, gone after 8 s
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

void UZoogiFunSys::StartStrike('''),
])
print('b81 strike ok')
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
