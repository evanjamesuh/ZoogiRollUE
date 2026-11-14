// Oct 31 b46 (map-realism worker, Evan: "every map constantly improving to be realistic and more intuitive"):
// visual-only set dressing for the classic arenas, in its own actor so nothing touches gameplay (no collision, no
// colliders moved, no floors / knock-off radii / spawn or orb rings changed). The dressing actor follows its arena:
// a ticker removes it when the arena is rebuilt as another theme or goes away. -ZoogiNoRealism = off.
//   - ambient air: drifting snow (Frozen), pollen motes (Meadow), ash + embers (Volcanic), sand motes (Arabian),
//     dust motes (Tomb), star dust (Cosmic), rain streaks (Neon) - instanced, ~100-400 tiny instances, CPU-wrapped
//   - Meadow ground realism: grass tufts + field stones on the plain around the stands, pine forests in the valley
// Entry: ZoogiRealismApply(Arena, Theme) - called at the end of BuildMeadow / BuildFrozen; the other themes need the
// same one-line call from AZoogiArena::Build (core file) to get their ambient air.
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiMeadowLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiRealism
{
	struct FAir
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Ism;
		TArray<FVector> Pos;      // actor-local
		TArray<float> Phase;
		FVector Half = FVector(2600.f, 2600.f, 0.f);
		float Bottom = 0.f, Top = 1600.f;
		float Fall = 120.f;       // cm/s, negative = rises
		float Sway = 40.f;        // cm amplitude
		float Scale = 0.07f;
		FVector Stretch = FVector(1.f); // per-axis instance scale multiplier (rain streaks)
	};
	struct FDress
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<AZoogiArena> Arena;
		EZoogiArena Theme = EZoogiArena::Meadow;
		TArray<FAir> Air;
		float Time = 0.f;
	};
	static TArray<FDress> GDress;
	static FTSTicker::FDelegateHandle GTick;

	static bool Tick(float Dt)
	{
		Dt = FMath::Min(Dt, 0.1f);
		for (int32 i = GDress.Num() - 1; i >= 0; --i)
		{
			FDress& D = GDress[i];
			AActor* Ac = D.Actor.Get();
			AZoogiArena* Ar = D.Arena.Get();
			if (!Ac || !Ar || Ar->GetTheme() != D.Theme || !IsValid(Ar))
			{
				if (Ac) Ac->Destroy();
				GDress.RemoveAtSwap(i);
				continue;
			}
			D.Time += Dt;
			for (FAir& A : D.Air)
			{
				UInstancedStaticMeshComponent* Ism = A.Ism.Get();
				if (!Ism || A.Pos.Num() == 0) continue;
				TArray<FTransform> Xf;
				Xf.Reserve(A.Pos.Num());
				const float Span = FMath::Max(A.Top - A.Bottom, 10.f);
				for (int32 k = 0; k < A.Pos.Num(); ++k)
				{
					FVector& P = A.Pos[k];
					P.Z -= A.Fall * Dt;
					if (P.Z < A.Bottom) P.Z += Span;
					if (P.Z > A.Top) P.Z -= Span;
					const float Ph = A.Phase[k];
					const FVector Off(FMath::Sin(D.Time * 0.7f + Ph) * A.Sway, FMath::Cos(D.Time * 0.55f + Ph * 1.3f) * A.Sway, 0.f);
					Xf.Add(FTransform(FRotator(0.f, Ph * 57.3f, 0.f), P + Off, A.Stretch * A.Scale));
				}
				Ism->BatchUpdateInstancesTransforms(0, Xf, false, true, true);
			}
		}
		if (GDress.Num() == 0) { GTick.Reset(); return false; }
		return true;
	}

	static AActor* Begin(AZoogiArena* Ar, EZoogiArena Theme)
	{
		for (int32 i = GDress.Num() - 1; i >= 0; --i)
			if (GDress[i].Arena.Get() == Ar || !GDress[i].Arena.IsValid())
			{
				if (AActor* Old = GDress[i].Actor.Get()) Old->Destroy();
				GDress.RemoveAtSwap(i);
			}
		UWorld* W = Ar->GetWorld();
		if (!W) return nullptr;
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Ac = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ar->GetActorLocation()), SP);
		if (!Ac) return nullptr;
		USceneComponent* Root = NewObject<USceneComponent>(Ac, TEXT("RealismRoot"));
		Ac->SetRootComponent(Root);
		Root->RegisterComponent();
		Root->SetWorldLocation(Ar->GetActorLocation());
		Ac->SetActorScale3D(Ar->GetActorScale3D()); // same units as the arena builders (Meadow / Neon / the 1.3 courts are scaled actors)
		FDress D; D.Actor = Ac; D.Arena = Ar; D.Theme = Theme;
		GDress.Add(D);
		if (!GTick.IsValid()) GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		return Ac;
	}

	static UInstancedStaticMeshComponent* MakeIsm(AActor* Ac, const TCHAR* Shape, UMaterialInterface* Mat, bool bShadow)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Ac);
		C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bShadow);
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(Ac->GetRootComponent());
		C->RegisterComponent();
		return C;
	}

	static void AddAir(AActor* Ac, FDress& D, const TCHAR* Shape, UMaterialInterface* Mat, int32 Count, const FVector& Half, float Bottom, float Top,
		float Fall, float Sway, float Scale, const FVector& Stretch, int32 Seed)
	{
		FAir A;
		A.Ism = MakeIsm(Ac, Shape, Mat, false);
		A.Half = Half; A.Bottom = Bottom; A.Top = Top; A.Fall = Fall; A.Sway = Sway; A.Scale = Scale; A.Stretch = Stretch;
		FRandomStream R(Seed);
		for (int32 k = 0; k < Count; ++k)
		{
			const FVector P(R.FRandRange(-Half.X, Half.X), R.FRandRange(-Half.Y, Half.Y), R.FRandRange(Bottom, Top));
			A.Pos.Add(P); A.Phase.Add(R.FRandRange(0.f, 6.2831f));
			if (UInstancedStaticMeshComponent* Ism = A.Ism.Get()) Ism->AddInstance(FTransform(FRotator::ZeroRotator, P, Stretch * Scale), false);
		}
		D.Air.Add(MoveTemp(A));
	}

	static bool NearGap(float YawDeg, float Half)
	{
		for (float G : ZoogiMeadow::OrbExitGapYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(YawDeg, G)) < Half) return true;
		return false;
	}

	static void Inst(UInstancedStaticMeshComponent* C, const FVector& P, const FRotator& R, const FVector& S) { if (C) C->AddInstance(FTransform(R, P, S), false); }
	static FVector Polar(float Rad, float YawDeg, float Z) { const float A = FMath::DegreesToRadians(YawDeg); return FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z); }

	// Volcanic: basalt columns rising out of the lava round the island (tops below the court), obsidian spires, a ring of
	// distant volcanoes with glowing craters and smoke plumes. Court r1550 / lava plane z -700 (BuildVolcanic, unscaled actor).
	static void VolcanicSet(AActor* Ac)
	{
		FRandomStream R(9101);
		const float LavaZ = -700.f;
		UInstancedStaticMeshComponent* Col = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#2A1D19")), ZoogiUtil::Hex(TEXT("#1A1210")), 0.01f, 0.85f), false);
		for (int32 k = 0; k < 80; ++k)
		{
			const float Top = R.FRandRange(-320.f, -130.f), Rad = R.FRandRange(40.f, 120.f);
			const FVector P = Polar(R.FRandRange(1850.f, 2900.f), R.FRandRange(0.f, 360.f), (Top + LavaZ) * 0.5f);
			Inst(Col, P, FRotator(R.FRandRange(-6.f, 6.f), 0.f, R.FRandRange(-6.f, 6.f)), FVector(Rad / 50.f, Rad / 50.f, (Top - LavaZ) / 100.f));
		}
		UInstancedStaticMeshComponent* Spire = MakeIsm(Ac, TEXT("Cone"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#15100F")), 0.3f), false);
		for (int32 k = 0; k < 34; ++k)
		{
			const float H = R.FRandRange(900.f, 2600.f);
			Inst(Spire, Polar(R.FRandRange(6200.f, 9500.f), R.FRandRange(0.f, 360.f), LavaZ + H * 0.5f), FRotator(R.FRandRange(-8.f, 8.f), 0.f, R.FRandRange(-8.f, 8.f)), FVector(H * 0.22f / 100.f, H * 0.22f / 100.f, H / 100.f));
		}
		UInstancedStaticMeshComponent* Mtn = MakeIsm(Ac, TEXT("Cone"), ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#2E221E")), ZoogiUtil::Hex(TEXT("#40302A")), 0.0006f, 0.95f), false);
		UInstancedStaticMeshComponent* Crater = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#FF5A14")), 1.6f), false);
		UInstancedStaticMeshComponent* Smoke = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#3B3634")), 1.f), false);
		for (int32 k = 0; k < 16; ++k)
		{
			const float H = R.FRandRange(2800.f, 6200.f), W = H * R.FRandRange(1.5f, 2.1f);
			const float Yaw = k * 22.5f + R.FRandRange(-6.f, 6.f), Dist = R.FRandRange(11500.f, 17500.f);
			Inst(Mtn, Polar(Dist, Yaw, LavaZ + H * 0.5f), FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(W / 100.f, W / 100.f * R.FRandRange(0.85f, 1.15f), H / 100.f));
			if (k % 3 == 0) // glowing crater + a rising smoke column on every third peak
			{
				const FVector Peak = Polar(Dist, Yaw, LavaZ + H * 0.93f);
				Inst(Crater, Peak, FRotator::ZeroRotator, FVector(W * 0.09f / 100.f, W * 0.09f / 100.f, W * 0.03f / 100.f));
				for (int32 s = 0; s < 5; ++s)
				{
					const float Sz = W * (0.07f + 0.05f * s);
					Inst(Smoke, Peak + FVector(s * 160.f, s * 90.f, 300.f + s * Sz * 0.75f), FRotator::ZeroRotator, FVector(Sz / 100.f, Sz / 100.f, Sz * 0.8f / 100.f));
				}
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("REALISM Volcanic set: basalt columns, %d spires, 16 volcanoes"), Spire->GetInstanceCount());
	}

	// Cosmic: an asteroid belt round the court (some with glowing crystal shards) and floating ruin fragments far out.
	static void CosmicSet(AActor* Ac)
	{
		FRandomStream R(9202);
		UInstancedStaticMeshComponent* Rock = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#3B3550")), ZoogiUtil::Hex(TEXT("#26213A")), 0.01f, 0.9f), false);
		UInstancedStaticMeshComponent* Shard = MakeIsm(Ac, TEXT("Cone"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#9B7BFF")), 1.5f), false);
		for (int32 k = 0; k < 170; ++k)
		{
			const float Sc = R.FRandRange(0.8f, 1.f) * FMath::Pow(R.FRandRange(0.2f, 1.f), 2.f) * 6.f + 0.6f;
			const FVector P = Polar(R.FRandRange(3300.f, 9500.f), R.FRandRange(0.f, 360.f), R.FRandRange(-2400.f, 900.f));
			const FRotator Rt(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f));
			Inst(Rock, P, Rt, FVector(Sc * R.FRandRange(0.8f, 1.4f), Sc, Sc * R.FRandRange(0.6f, 1.f)));
			if (k % 6 == 0) for (int32 s = 0; s < 3; ++s) Inst(Shard, P + FVector(R.FRandRange(-30.f, 30.f), R.FRandRange(-30.f, 30.f), Sc * 38.f), FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 360.f), R.FRandRange(-25.f, 25.f)), FVector(Sc * 0.18f, Sc * 0.18f, Sc * 0.7f));
		}
		UMaterialInterface* RuinM = ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#5A5478")), ZoogiUtil::Hex(TEXT("#433D5E")), 0.01f, 0.85f);
		UInstancedStaticMeshComponent* Slab = MakeIsm(Ac, TEXT("Cylinder"), RuinM, false);
		UInstancedStaticMeshComponent* Under = MakeIsm(Ac, TEXT("Cone"), RuinM, false);
		UInstancedStaticMeshComponent* Pillar = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#7A73A0")), 0.7f), false);
		for (int32 k = 0; k < 9; ++k)
		{
			const float Rd = R.FRandRange(280.f, 520.f);
			const FVector C = Polar(R.FRandRange(5200.f, 8200.f), k * 40.f + R.FRandRange(-12.f, 12.f), R.FRandRange(-900.f, 500.f));
			Inst(Slab, C, FRotator::ZeroRotator, FVector(Rd / 50.f, Rd / 50.f, 0.6f));
			Inst(Under, C - FVector(0, 0, Rd * 0.75f + 30.f), FRotator(180.f, 0.f, 0.f), FVector(Rd * 0.95f / 50.f, Rd * 0.95f / 50.f, Rd * 1.5f / 100.f));
			const int32 NPil = R.RandRange(2, 4);
			for (int32 p = 0; p < NPil; ++p)
			{
				const float PH = R.FRandRange(180.f, 520.f);
				const FVector PP = C + FVector(FMath::Cos(p * 1.7f) * Rd * 0.6f, FMath::Sin(p * 1.7f) * Rd * 0.6f, 30.f + PH * 0.5f);
				Inst(Pillar, PP, FRotator(R.FRandRange(-4.f, 4.f), 0.f, R.FRandRange(-4.f, 4.f)), FVector(0.55f, 0.55f, PH / 100.f));
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("REALISM Cosmic set: %d asteroids, 9 floating ruins"), Rock->GetInstanceCount());
	}

	// Desert floor far below a floating island + dunes + mesas (Arabian / Tomb). SkipYaw: keep a wedge clear (Tomb pyramids).
	static void DesertSet(AActor* Ac, float FloorZ, const FLinearColor& SandA, const FLinearColor& SandB, const FLinearColor& Rock, float SkipYaw0, float SkipYaw1, int32 Seed)
	{
		FRandomStream R(Seed);
		UInstancedStaticMeshComponent* Floor = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::GroundMID(Ac, SandA, SandB, 0.0004f, 0.95f), false);
		Inst(Floor, FVector(0, 0, FloorZ - 25.f), FRotator::ZeroRotator, FVector(700.f, 700.f, 0.5f)); // r35000 disc
		UInstancedStaticMeshComponent* DuneA = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::GroundMID(Ac, SandA, SandB, 0.0008f, 0.95f), false);
		UInstancedStaticMeshComponent* DuneB = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::GroundMID(Ac, SandB * 0.92f, SandA * 0.9f, 0.0008f, 0.95f), false);
		auto Skip = [&](float Yaw) { return SkipYaw1 > SkipYaw0 && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, (SkipYaw0 + SkipYaw1) * 0.5f)) < (SkipYaw1 - SkipYaw0) * 0.5f; };
		for (int32 k = 0; k < 46; ++k)
		{
			const float Yaw = R.FRandRange(0.f, 360.f);
			const float Dist = R.FRandRange(4500.f, 24000.f);
			if (Dist > 6000.f && Skip(Yaw)) continue;
			const float L = R.FRandRange(2500.f, 7500.f), Ht = R.FRandRange(300.f, 1300.f);
			Inst(k % 2 ? DuneA : DuneB, Polar(Dist, Yaw, FloorZ), FRotator(0.f, Yaw + R.FRandRange(60.f, 120.f), 0.f), FVector(L / 50.f, L * R.FRandRange(0.35f, 0.6f) / 50.f, Ht / 50.f));
		}
		UInstancedStaticMeshComponent* Mesa = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::GroundMID(Ac, Rock, Rock * 0.8f, 0.002f, 0.92f), false);
		for (int32 k = 0; k < 9; ++k)
		{
			const float Yaw = k * 40.f + R.FRandRange(-10.f, 10.f);
			if (Skip(Yaw)) continue;
			const float Rd = R.FRandRange(900.f, 2400.f), H = R.FRandRange(1200.f, 2600.f);
			Inst(Mesa, Polar(R.FRandRange(14000.f, 21000.f), Yaw, FloorZ + H * 0.5f), FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(Rd / 50.f, Rd * R.FRandRange(0.7f, 1.f) / 50.f, H / 100.f));
		}
	}

	// Tomb extra: half-buried broken obelisks / columns in the sand (sandstone, tilted).
	static void TombRuins(AActor* Ac, float FloorZ)
	{
		FRandomStream R(9404);
		UInstancedStaticMeshComponent* Ob = MakeIsm(Ac, TEXT("Cube"), ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#CDA977")), ZoogiUtil::Hex(TEXT("#A98656")), 0.004f, 0.9f), false);
		UInstancedStaticMeshComponent* Cap = MakeIsm(Ac, TEXT("Cone"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#D9B26A")), 0.5f, ZoogiUtil::Hex(TEXT("#D9B26A")), 0.1f, 0.6f), false);
		for (int32 k = 0; k < 18; ++k)
		{
			const float Yaw = R.FRandRange(0.f, 360.f);
			if (Yaw > 115.f && Yaw < 180.f) continue; // the pyramid horizon stays clean
			const float H = R.FRandRange(500.f, 1600.f), Wd = H * 0.12f;
			const FVector B = Polar(R.FRandRange(4800.f, 9500.f), Yaw, FloorZ);
			const FRotator Rt(R.FRandRange(-22.f, 22.f), R.FRandRange(0.f, 360.f), R.FRandRange(-22.f, 22.f));
			Inst(Ob, B + Rt.RotateVector(FVector(0, 0, H * 0.35f)), Rt, FVector(Wd / 100.f, Wd / 100.f, H / 100.f));
			if (k % 2 == 0) Inst(Cap, B + Rt.RotateVector(FVector(0, 0, H * 0.85f + Wd * 0.4f)), Rt, FVector(Wd * 1.05f / 100.f, Wd * 1.05f / 100.f, Wd * 0.8f / 100.f));
		}
	}

	// Neon: dark streets far below with a glowing grid, and red rooftop beacons + antennas on the real skyline towers
	// (same FRandomStream(555) draw order as BuildNeon's skyline, so the beacons sit exactly on those roofs; actor scale 1.3).
	static void NeonSet(AActor* Ac, float S)
	{
		const float Base = -4500.f;
		UInstancedStaticMeshComponent* Ground = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#07080F")), 0.4f), false);
		Inst(Ground, FVector(0, 0, Base * S - 30.f), FRotator::ZeroRotator, FVector(14000.f * S / 50.f, 14000.f * S / 50.f, 0.5f));
		UInstancedStaticMeshComponent* StreetC = MakeIsm(Ac, TEXT("Cube"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#22E7FF")), 0.9f), false);
		UInstancedStaticMeshComponent* StreetM = MakeIsm(Ac, TEXT("Cube"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#FF3EC8")), 0.8f), false);
		for (int32 g = -10; g <= 10; ++g)
		{
			const float Off = g * 1300.f * S, HalfLen = 12500.f * S;
			Inst(g % 3 ? StreetC : StreetM, FVector(Off, 0.f, Base * S - 4.f), FRotator::ZeroRotator, FVector(0.5f, HalfLen / 50.f, 0.06f));
			Inst(g % 3 ? StreetM : StreetC, FVector(0.f, Off, Base * S - 4.f), FRotator::ZeroRotator, FVector(HalfLen / 50.f, 0.5f, 0.06f));
		}
		UInstancedStaticMeshComponent* Beacon = MakeIsm(Ac, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#FF2A2A")), 3.f), false);
		UInstancedStaticMeshComponent* Mast = MakeIsm(Ac, TEXT("Cylinder"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#1A1C28")), 0.4f), false);
		FRandomStream Rng(555);
		int32 NB = 0;
		for (int32 i = 0; i < 70; ++i)
		{
			const float A = Rng.FRandRange(0.f, 2.f * PI);
			const float D = Rng.FRandRange(4000.f, 11000.f);
			const float W = Rng.FRandRange(300.f, 900.f), H = Rng.FRandRange(1500.f, 6000.f);
			(void)Rng.FRandRange(0.6f, 1.0f); // window glow strength draw
			const int32 Bands = Rng.RandRange(2, 6);
			for (int32 b = 0; b < Bands; ++b) { (void)Rng.FRandRange(0.35f, 0.97f); (void)Rng.FRandRange(15.f, 50.f); }
			if (i % 2) continue;
			const FVector Roof(FMath::Cos(A) * D * S, FMath::Sin(A) * D * S, (Base + H) * S);
			const float MastH = FMath::Clamp(W * 0.6f, 200.f, 520.f) * S;
			Inst(Mast, Roof + FVector(0, 0, MastH * 0.5f), FRotator::ZeroRotator, FVector(0.12f, 0.12f, MastH / 100.f));
			Inst(Beacon, Roof + FVector(0, 0, MastH + 18.f), FRotator::ZeroRotator, FVector(0.38f));
			++NB;
		}
		UE_LOG(LogZoogi, Log, TEXT("REALISM Neon set: street grid + %d rooftop beacons"), NB);
	}

	// ---- pass 3 (play surfaces): per-theme tuning of the EXISTING floor materials (same meshes / colliders), a worn edge lip
	struct FParam { const TCHAR* Name; float Value; };
	static int32 TuneFloors(AZoogiArena* Ar, EZoogiArena Theme)
	{
		TArray<FParam> Ps;
		switch (Theme)
		{
		case EZoogiArena::Volcanic: Ps = { { TEXT("BaseBright"), 1.75f }, { TEXT("DetailStrength"), 0.65f }, { TEXT("DetailScale"), 140.f }, { TEXT("GlowTint"), 0.3f }, { TEXT("Roughness"), 0.62f }, { TEXT("Specular"), 0.5f } }; break; // readable basalt with a faint obsidian sheen
		case EZoogiArena::Arabian:  Ps = { { TEXT("DetailStrength"), 0.55f }, { TEXT("DetailScale"), 120.f }, { TEXT("GlowTint"), 0.4f }, { TEXT("Roughness"), 0.88f } }; break;           // weathered sandstone grain
		case EZoogiArena::Tomb:     Ps = { { TEXT("DetailStrength"), 0.55f }, { TEXT("DetailScale"), 130.f }, { TEXT("GlowTint"), 0.4f }, { TEXT("Roughness"), 0.86f } }; break;
		case EZoogiArena::Cosmic:   Ps = { { TEXT("GlowTint"), 0.25f }, { TEXT("DetailStrength"), 0.5f }, { TEXT("Roughness"), 0.45f }, { TEXT("Specular"), 0.55f } }; break;       // calmer line paint, polished stone depth
		case EZoogiArena::Neon:     Ps = { { TEXT("GlowTint"), 0.25f }, { TEXT("DetailStrength"), 0.5f }, { TEXT("BaseBright"), 1.25f }, { TEXT("Roughness"), 0.35f }, { TEXT("Specular"), 0.65f } }; break; // wet-asphalt sheen
		case EZoogiArena::Meadow:   Ps = { { TEXT("DetailStrength"), 0.5f }, { TEXT("Roughness"), 0.85f } }; break;
		case EZoogiArena::Frozen:   Ps = { { TEXT("Roughness"), 0.3f }, { TEXT("Specular"), 0.6f } }; break;
		default: break;
		}
		if (Ps.Num() == 0) return 0;
		int32 N = 0;
		TArray<UStaticMeshComponent*> Comps;
		Ar->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			if (!C) continue;
			for (int32 m = 0; m < C->GetNumMaterials(); ++m)
			{
				UMaterialInterface* Mi = C->GetMaterial(m);
				if (!Mi) continue;
				const UMaterial* BaseM = Mi->GetBaseMaterial();
				const bool bKit = BaseM && BaseM->GetName() == TEXT("M_ZoogiArenaFloor");
				const bool bDisc = Mi->GetName().Contains(TEXT("CosmicFloor")) || Mi->GetName().Contains(TEXT("TombFloor"));
				if (!bKit && !bDisc) continue;
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mi);
				if (!MID) { MID = UMaterialInstanceDynamic::Create(Mi, C); C->SetMaterial(m, MID); }
				int32 Hit = 0;
				for (const FParam& P : Ps)
				{
					float Old = 0.f;
					if (MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(P.Name)), Old)) { MID->SetScalarParameterValue(FName(P.Name), P.Value); ++Hit; }
				}
				if (bDisc && Hit == 0) // unknown material: list its parameters once so the next pass can tune it
				{
					TArray<FMaterialParameterInfo> Infos; TArray<FGuid> Ids;
					MID->GetAllScalarParameterInfo(Infos, Ids);
					FString Names; for (const FMaterialParameterInfo& In : Infos) Names += In.Name.ToString() + TEXT(" ");
					UE_LOG(LogZoogi, Log, TEXT("REALISM floor %s params: %s"), *Mi->GetName(), *Names);
				}
				++N;
			}
		}
		return N;
	}

	// Worn edge lip: a dark trim band on the outermost floor ring (visual, 2 cm proud, no collision) + an optional thin seam
	// just inside it, so the drop-off reads from every camera. Layout units (the dressing actor carries the arena scale).
	static void EdgeLip(AActor* Ac, float KR, const FLinearColor& Lip, const FLinearColor& Seam, float SeamGlow, float Z)
	{
		constexpr int32 Segs = 72;
		UInstancedStaticMeshComponent* L = MakeIsm(Ac, TEXT("Cube"), ZoogiUtil::GroundMID(Ac, Lip, Lip * 0.75f, 0.02f, 0.9f), false);
		UInstancedStaticMeshComponent* Sm = SeamGlow > 0.f ? MakeIsm(Ac, TEXT("Cube"), ZoogiUtil::GlowMID(Ac, Seam, SeamGlow), false) : nullptr;
		const float Step = 360.f / Segs;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float Yaw = (i + 0.5f) * Step;
			const float RMid = KR - 15.f, Len = 2.f * PI * RMid / Segs * 1.02f;
			Inst(L, Polar(RMid, Yaw, Z), FRotator(0.f, Yaw, 0.f), FVector(22.f / 100.f, Len / 100.f, 2.f / 100.f));
			if (Sm) { const float RS = KR - 31.f, LS = 2.f * PI * RS / Segs * 1.02f; Inst(Sm, Polar(RS, Yaw, Z), FRotator(0.f, Yaw, 0.f), FVector(4.f / 100.f, LS / 100.f, 2.f / 100.f)); }
		}
	}

	static void PlaySurfaces(AActor* Ac, AZoogiArena* Ar, EZoogiArena Theme)
	{
		const int32 NF = TuneFloors(Ar, Theme);
		const float KR = ZoogiConst::KnockoffRadius; // layout radius of every classic disc (the actor scale does the rest)
		switch (Theme)
		{
		case EZoogiArena::Volcanic:
			EdgeLip(Ac, KR, ZoogiUtil::Hex(TEXT("#3A2A24")), ZoogiUtil::Hex(TEXT("#FF6A1A")), 1.1f, 2.f);
			if (UPointLightComponent* Fill = NewObject<UPointLightComponent>(Ac)) // warm fill so the basalt court is not a black disc
			{
				Fill->SetupAttachment(Ac->GetRootComponent()); Fill->SetRelativeLocation(FVector(0.f, 0.f, 900.f));
				Fill->SetIntensityUnits(ELightUnits::Candelas); Fill->SetIntensity(1800.f); Fill->SetAttenuationRadius(3200.f);
				Fill->SetLightColor(FLinearColor(1.f, 0.62f, 0.38f)); Fill->SetCastShadows(false); Fill->RegisterComponent();
			}
			break;
		case EZoogiArena::Arabian: EdgeLip(Ac, KR, ZoogiUtil::Hex(TEXT("#8E5A2E")), FLinearColor::Black, 0.f, 2.f); break;
		case EZoogiArena::Tomb:    EdgeLip(Ac, KR, ZoogiUtil::Hex(TEXT("#8C6A44")), ZoogiUtil::Hex(TEXT("#E8B84A")), 0.7f, 3.f); break;
		case EZoogiArena::Neon:    EdgeLip(Ac, KR, ZoogiUtil::Hex(TEXT("#1C1F2C")), FLinearColor::Black, 0.f, 2.f); break; // its pink rim glow stays the seam
		default: break;
		}
		UE_LOG(LogZoogi, Log, TEXT("REALISM %s play surface: %d floor materials tuned"), *ZoogiUtil::ArenaName(Theme), NF);
	}

	// Meadow: grass tufts + field stones on the plain, pine forests in the valley (all far outside the kill zone, no collision)
	static void MeadowGround(AActor* Ac, AZoogiArena* Ar)
	{
		const float S = 1.f; // the dressing actor carries the arena scale: layout units here
		const float PlainZ = -45.f * S; // top of the visual plain (ArcSlab -80..-45 in layout units)
		FRandomStream R(7319);
		// grass tufts: three greens, squashed cones (clumps), denser near the stands, thinning outward
		UMaterialInterface* GA = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#6E9A34")), 0.9f);
		UMaterialInterface* GB = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#86AE3E")), 0.9f);
		UMaterialInterface* GC = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#5A8530")), 0.92f);
		UInstancedStaticMeshComponent* Tuft[3] = { MakeIsm(Ac, TEXT("Cone"), GA, false), MakeIsm(Ac, TEXT("Cone"), GB, false), MakeIsm(Ac, TEXT("Cone"), GC, false) };
		int32 NT = 0;
		for (int32 k = 0; k < 900; ++k)
		{
			const float U = R.FRandRange(0.f, 1.f);
			const float Rad = FMath::Lerp(2480.f, 9000.f, U * U) * S; // more near the arena
			const float Yaw = R.FRandRange(0.f, 360.f);
			if (NearGap(Yaw, 13.f)) continue; // the orb-exit ravines stay clean (and tufts would float there)
			const float A = FMath::DegreesToRadians(Yaw);
			const float Sc = R.FRandRange(0.25f, 0.6f);
			const FVector P(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, PlainZ + 14.f * Sc);
			Tuft[k % 3]->AddInstance(FTransform(FRotator(R.FRandRange(-8.f, 8.f), R.FRandRange(0.f, 360.f), R.FRandRange(-8.f, 8.f)), P, FVector(Sc * R.FRandRange(0.9f, 1.6f), Sc, Sc * 0.55f)), false);
			++NT;
		}
		// field stones: weathered grey-brown, half sunk
		UMaterialInterface* St = ZoogiUtil::GroundMID(Ac, ZoogiUtil::Hex(TEXT("#8C8172")), ZoogiUtil::Hex(TEXT("#6E6458")), 0.02f, 0.93f);
		UInstancedStaticMeshComponent* Stones = MakeIsm(Ac, TEXT("Sphere"), St, true);
		for (int32 k = 0; k < 140; ++k)
		{
			const float Rad = R.FRandRange(2600.f, 8000.f) * S, Yaw = R.FRandRange(0.f, 360.f);
			if (NearGap(Yaw, 14.f)) continue;
			const float A = FMath::DegreesToRadians(Yaw);
			const float Sc = R.FRandRange(0.35f, 1.4f);
			Stones->AddInstance(FTransform(FRotator(R.FRandRange(-20.f, 20.f), R.FRandRange(0.f, 360.f), 0.f), FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, PlainZ - 8.f), FVector(Sc * 1.3f, Sc, Sc * 0.55f)), false);
		}
		// pine forests: clustered stands in the valley (south and side arcs - the north is the waterfall mesas)
		UMaterialInterface* Trunk = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#5A4232")), 0.9f);
		UMaterialInterface* NearN = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#2F5A2C")), 0.9f);
		UMaterialInterface* FarN = ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#4F7A5A")), 0.92f); // aerial perspective: farther = bluer, lighter
		UInstancedStaticMeshComponent* Tr = MakeIsm(Ac, TEXT("Cylinder"), Trunk, true);
		UInstancedStaticMeshComponent* NeedleN = MakeIsm(Ac, TEXT("Cone"), NearN, true);
		UInstancedStaticMeshComponent* NeedleF = MakeIsm(Ac, TEXT("Cone"), FarN, false);
		int32 NP = 0;
		for (int32 c = 0; c < 22; ++c)
		{
			float CY = R.FRandRange(0.f, 360.f);
			if (CY > 200.f && CY < 340.f) CY = R.FRandRange(-20.f, 200.f); // keep off the north mesas / waterfalls
			const float CR = R.FRandRange(9500.f, 13300.f);
			const int32 Trees = R.RandRange(6, 13);
			for (int32 t = 0; t < Trees; ++t)
			{
				const float Yaw = CY + R.FRandRange(-4.5f, 4.5f);
				if (NearGap(Yaw, 15.f)) continue;
				const float Rad = (CR + R.FRandRange(-650.f, 650.f)) * S;
				const float A = FMath::DegreesToRadians(Yaw);
				const FVector B(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, PlainZ);
				const float H = R.FRandRange(900.f, 1700.f);
				const bool bFar = Rad > 11800.f * S;
				Tr->AddInstance(FTransform(FRotator::ZeroRotator, B + FVector(0, 0, H * 0.12f), FVector(0.22f * H / 1000.f, 0.22f * H / 1000.f, H * 0.24f / 100.f)), false);
				UInstancedStaticMeshComponent* Nd = bFar ? NeedleF : NeedleN;
				for (int32 l = 0; l < 3; ++l) // three stacked tiers, narrower upward
				{
					const float Wd = H * (0.62f - 0.16f * l), Ht = H * (0.48f - 0.08f * l);
					Nd->AddInstance(FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), B + FVector(0, 0, H * (0.42f + 0.2f * l)), FVector(Wd / 100.f, Wd / 100.f, Ht / 100.f)), false);
				}
				++NP;
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("REALISM Meadow ground: %d grass tufts, %d stones, %d pines"), NT, Stones->GetInstanceCount(), NP);
		(void)Ar;
	}
}

void ZoogiRealismApply(AZoogiArena* Arena, EZoogiArena Theme)
{
	using namespace ZoogiRealism;
	if (!Arena || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRealism"))) return;
	if (Theme == EZoogiArena::Ancient || Theme == EZoogiArena::Training || Theme == EZoogiArena::Teaser) return; // own art passes
	AActor* Ac = Begin(Arena, Theme);
	if (!Ac) return;
	FDress& D = GDress.Last();
	const float S = 1.f; // actor-relative units (the dressing actor is scaled like the arena)
	const FVector Half(2700.f, 2700.f, 0.f);
	switch (Theme)
	{
	case EZoogiArena::Frozen:
		// steady light snowfall over the court and the outer ring (soft white, slight sway)
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, FLinearColor(0.92f, 0.96f, 1.f), 0.9f), 420, Half, -200.f, 1700.f, 140.f, 55.f, 0.075f, FVector(1.f), 501);
		break;
	case EZoogiArena::Volcanic:
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#4A4440")), 0.95f), 260, Half, -300.f, 1800.f, 60.f, 70.f, 0.06f, FVector(1.f), 502);      // falling ash
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#FF7A1E")), 2.2f), 90, Half, -300.f, 1600.f, -90.f, 50.f, 0.05f, FVector(1.f), 503);         // rising embers
		VolcanicSet(Ac);
		break;
	case EZoogiArena::Meadow:
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#FFF4C8")), 0.8f), 110, Half, 60.f, 900.f, 12.f, 90.f, 0.045f, FVector(1.f), 504);          // pollen / seed fluff
		MeadowGround(Ac, Arena);
		break;
	case EZoogiArena::Arabian:
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::SurfaceMID(Ac, ZoogiUtil::Hex(TEXT("#E2C28A")), 0.9f), 180, Half, 20.f, 500.f, 25.f, 160.f, 0.04f, FVector(1.f), 505);       // drifting sand motes
		DesertSet(Ac, -2200.f, ZoogiUtil::Hex(TEXT("#E2B56C")), ZoogiUtil::Hex(TEXT("#C9924A")), ZoogiUtil::Hex(TEXT("#B26E3C")), 0.f, 0.f, 9303);
		break;
	case EZoogiArena::Tomb:
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#D8C9A0")), 0.6f), 160, Half, 40.f, 700.f, 8.f, 60.f, 0.04f, FVector(1.f), 506);           // dust in the light
		DesertSet(Ac, -1500.f, ZoogiUtil::Hex(TEXT("#E3C595")), ZoogiUtil::Hex(TEXT("#CDA776")), ZoogiUtil::Hex(TEXT("#B98C5C")), 115.f, 180.f, 9304); // floor at the pyramid base
		TombRuins(Ac, -1500.f);
		break;
	case EZoogiArena::Cosmic:
		AddAir(Ac, D, TEXT("Sphere"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#B8C8FF")), 2.f), 200, Half, -600.f, 1500.f, -18.f, 80.f, 0.04f, FVector(1.f), 507);       // floating star dust
		CosmicSet(Ac);
		break;
	case EZoogiArena::Neon:
		AddAir(Ac, D, TEXT("Cylinder"), ZoogiUtil::GlowMID(Ac, ZoogiUtil::Hex(TEXT("#9FD8FF")), 0.7f), 260, Half, -100.f, 1800.f, 1300.f, 6.f, 0.02f, FVector(0.6f, 0.6f, 30.f), 508); // rain streaks
		NeonSet(Ac, S);
		break;
	default: break;
	}
	PlaySurfaces(Ac, Arena, Theme); // pass 3
	UE_LOG(LogZoogi, Log, TEXT("REALISM %s: dressing actor up (%d ambient layers)"), *ZoogiUtil::ArenaName(Theme), D.Air.Num());
}
