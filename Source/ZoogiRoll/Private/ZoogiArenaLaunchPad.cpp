// Oct 22: per-map launch pad placement (shared rules from ZoogiLaunch in ZoogiLaunchPad.h).
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
void AZoogiArena::PlaceLaunchPad()
{
	UWorld* W = GetWorld();
	if (!W) return;
	// Oct 30 (Evan, build 28): the ship launch pad only on Cosmic - every other map starts on the field (-ZoogiShipAllMaps = old behaviour)
	// b89 (Evan: "delete launch ship from cosmos, make players start on field"): no ship anywhere (-ZoogiCosmicShip = Cosmic keeps it)
	if ((Theme != EZoogiArena::Cosmic || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmicShip"))) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiShipAllMaps")))
	{
		UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: none on this map - players start on the field"));
		return;
	}
	using namespace ZoogiLaunch;
	auto Pol = [](float R, float YawDeg) { const float A = FMath::DegreesToRadians(YawDeg); return FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R); };
	// Far-side flank of the match camera (camera on +Y looking -Y): seen in low34 / hero, never between the camera and the court.
	// Clear of the 0/90/180/270 spawn + Meadow orb-exit yaws.
	static const float Yaws[] = { 140.f, 130.f, 150.f, 125.f, 155.f, 40.f, 50.f, 30.f, 115.f, 165.f };
	static const float Extra[] = { 0.f, 300.f, 600.f };
	FCollisionQueryParams Q(TEXT("ZoogiLaunchPad"), false, this);
	const FCollisionObjectQueryParams All(FCollisionObjectQueryParams::AllObjects);
	FVector BestPad = FVector::ZeroVector, BestLand = FVector::ZeroVector; float BestYaw = 0.f; bool bHave = false, bClean = false;
	for (float Extra0 : Extra)
	{
		for (float Yaw : Yaws)
		{
			bool bGap = false;
			for (float G : TerraceGapYaw) bGap |= FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, G)) < TerraceGapHalfDeg + 8.f;
			if (bGap) continue;
			float Edge = 6000.f; // outermost on-court radius along this yaw (Frozen Ring 2 / Neon rect aware)
			while (Edge > 300.f && IsOffCourt(Pol(Edge, Yaw))) Edge -= 25.f;
			const float PadR = Edge + EdgeGap + Extra0;
			const FVector2D Pad2 = Pol(PadR, Yaw), Land2 = Pol(PadR - FlightDist, Yaw);
			if (IsOffCourt(Land2) || Land2.Size() < 250.f) continue;
			const FVector Pad(Pad2, PadHeight), Land(Land2, ZoogiConst::ZoogiRadius);
			if (!bHave) { BestPad = Pad; BestLand = Land; BestYaw = Yaw; bHave = true; }
			bool bOk = true;
			for (int32 s = 0; s < 4 && bOk; ++s) bOk = FVector2D::Distance(Land2, FVector2D(GetSpawnPoint(s))) > 300.f;
			if (!bOk) continue;
			// landing spot: nothing taller than ~50 cm (obstacles, pieces, hub) within 160 cm
			if (W->OverlapAnyTestByObjectType(FVector(Land2, 210.f), FQuat::Identity, All, FCollisionShape::MakeSphere(160.f), Q)) continue;
			// pad volume: clear of stands / terraces / dressing that has collision or blocks the camera
			const FCollisionShape Box = FCollisionShape::MakeBox(FVector(DeckRadius + 40.f, DeckRadius + 40.f, 250.f));
			const FQuat Rot = FRotator(0, Yaw, 0).Quaternion();
			if (W->OverlapAnyTestByObjectType(Pad + FVector(0, 0, 100), Rot, All, Box, Q)) continue;
			if (W->OverlapAnyTestByChannel(Pad + FVector(0, 0, 100), Rot, ECC_Camera, Box, Q)) continue;
			BestPad = Pad; BestLand = Land; BestYaw = Yaw; bClean = true;
			break;
		}
		if (bClean) break;
	}
	if (!bHave) { UE_LOG(LogZoogi, Warning, TEXT("LaunchPad: no on-court landing found for %s"), *ZoogiUtil::ArenaName(Theme)); return; }
	if (!bClean) UE_LOG(LogZoogi, Warning, TEXT("LaunchPad: %s fell back to yaw %.0f (no fully clear candidate)"), *ZoogiUtil::ArenaName(Theme), BestYaw);
	FActorSpawnParameters P; P.Owner = this; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	LaunchPad = W->SpawnActor<AZoogiLaunchPad>(AZoogiLaunchPad::StaticClass(), BestPad, FRotator(0, BestYaw + 180.f, 0), P);
	if (LaunchPad) LaunchPad->Setup(Theme, BestLand);
}
