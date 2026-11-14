// Oct 23 (Evan): Zoogi Roll 0.1.2 legacy LerpzShip (/Game/Legacy012/SM_LegacyShip) parked OUTSIDE the Teaser ring by default (like level0 of the 0.1.2 demo: ship ~31.8u from the star-bumper cluster, just beyond the arena edge, slightly below floor).
// (The Teaser star pads themselves use /Game/Legacy012/SM_StarBumper, see ZoogiArenaTeaser.cpp.) Other arenas unchanged.
// Does NOT touch the VIP PlatformShip / drop intro. Positions are WORLD cm and are divided by BuildScale (actor scale).
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"

void AZoogiArena::PlaceLegacy012()
{
	if (Theme != EZoogiArena::Teaser) return;
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiLegacy012"))) return; // Oct 23: opt-in only (default ship blocked the Tac camera in Training/teaser)
	UStaticMesh* Ship = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Legacy012/SM_LegacyShip.SM_LegacyShip"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Ship) return;
	const float S = 1.f / FMath::Max(BuildScale, 0.01f);
	// Asset is native 0.1.2 size (~1268 x 1150 x 670 cm). 0.75 => ~950 x 860 x 500 cm world platform craft.
	constexpr float LegacyShipScale = 0.75f;
	const float MS = LegacyShipScale * S;
	// Teaser theta +95 (camera-right open water, visible from match cam; world yaw 270 + theta), r2650: beyond the rim (r2015) / outer wall. 0.1.2 ratio: 31.8u vs pads <=13.7u -> ~2650cm.
	const float A = FMath::DegreesToRadians(270.f + 95.f);
	const FVector2D P = FVector2D(FMath::Cos(A), FMath::Sin(A)) * 2650.f * S;
	const FBoxSphereBounds B = Ship->GetBounds();
	UStaticMeshComponent* C = AddMesh(Ship, nullptr, false); // out of bounds: no collision, never blocks play
	C->SetRelativeScale3D(FVector(MS));
	C->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
	C->SetRelativeLocation(FVector(P.X, P.Y, (B.BoxExtent.Z - B.Origin.Z) * MS - 120.f * S)); // floats just below the arena floor line, as in 0.1.2
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCanEverAffectNavigation(false);
	C->ComponentTags.Add(TEXT("NoCamBlock"));
	UE_LOG(LogZoogi, Warning, TEXT("LEGACY012: Teaser legacy ship placed (scale %.2f)"), LegacyShipScale);
}
