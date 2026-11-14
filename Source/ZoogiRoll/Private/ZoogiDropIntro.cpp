#include "ZoogiDropIntro.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

void FZoogiDropIntro::Reset()
{
	*this = FZoogiDropIntro();
}

void FZoogiDropIntro::Begin(int32 InNum, int32 First)
{
	NumPlayers = FMath::Max(0, InNum);
	FirstPlayer = NumPlayers > 0 ? FMath::Clamp(First, 0, NumPlayers - 1) : 0;
	Dropped.Init(false, NumPlayers);
	ActiveDrop = FirstPlayer;
	PickPoint = FVector::ZeroVector;
	bHasPick = false;
	StepAge = 0.f;
	bHardCutPending = false;
	Step = NumPlayers > 0 ? EZoogiDropStep::ShipHold : EZoogiDropStep::Done;
}

bool FZoogiDropIntro::AllDropped() const
{
	for (bool b : Dropped) if (!b) return false;
	return NumPlayers > 0 || Dropped.Num() == 0;
}

int32 FZoogiDropIntro::NextUndropped(int32 From) const
{
	if (NumPlayers <= 0) return -1;
	for (int32 k = 1; k <= NumPlayers; ++k)
	{
		const int32 i = (From + k) % NumPlayers;
		if (Dropped.IsValidIndex(i) && !Dropped[i]) return i;
	}
	return -1;
}

bool FZoogiDropIntro::OnLanded()
{
	if (Dropped.IsValidIndex(ActiveDrop)) Dropped[ActiveDrop] = true;
	bHasPick = false;
	PickPoint = FVector::ZeroVector;
	StepAge = 0.f;
	bLandPoofFired = false;
	bBounceSettling = false;
	LandContactAge = 0.f;
	FallTrailTicks = 0;
	const int32 N = NextUndropped(ActiveDrop);
	if (N < 0)
	{
		Step = EZoogiDropStep::Done;
		return true;
	}
	ActiveDrop = N;
	Step = EZoogiDropStep::TacPick;
	return false;
}

bool FZoogiDropIntro::Enabled()
{
	// t782u: elevator plan — empty-cabin DropIntro video match open is OFF by default.
	// -ZoogiNoDropIntro forces off; -ZoogiDropIntro forces on; [ZoogiRoll.DropIntro] bEnabled overrides.
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiNoDropIntro"))) return false;
	if (FParse::Param(Cmd, TEXT("ZoogiDropIntro"))) return true;
	bool b = false;
	if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.DropIntro"), TEXT("bEnabled"), b, GGameIni);
	return b;
}

bool FZoogiDropIntro::AutoDrive()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiDropAuto"))) return true;
	FString V;
	for (const TCHAR* K : { TEXT("ZoogiEpisodeTest="), TEXT("ZoogiBattleTest=") }) if (FParse::Value(Cmd, K, V)) return true;
	for (const TCHAR* K : { TEXT("ZoogiEpisodeTest"), TEXT("ZoogiEpisodeP5Test"), TEXT("ZoogiBattleTest"), TEXT("ZoogiAuto") })
		if (FParse::Param(Cmd, K)) return true;
	return false;
}

FVector ZoogiDropIntro::CabinSeatLocal(int32 Slot, int32 NumSlots)
{
	// Inside-cabin seats near panoramic windows (approx vs PlatformShip scale); facing toward cabin centre / camera.
	const int32 N = FMath::Max(1, NumSlots);
	const int32 S = ((Slot % N) + N) % N;
	static const FVector Seats4[4] = {
		FVector( 80.f, -180.f, 90.f),
		FVector(-80.f, -180.f, 90.f),
		FVector( 160.f, -40.f, 90.f),
		FVector(-160.f, -40.f, 90.f),
	};
	if (N <= 4) return Seats4[S];
	const float A = 2.f * PI * (float)S / (float)N;
	return FVector(FMath::Cos(A) * 160.f, -140.f + FMath::Sin(A) * 40.f, 90.f);
}

FTransform ZoogiDropIntro::ShipWorldXform(const AZoogiArena* Arena)
{
	const FVector C = Arena ? Arena->GetActorLocation() : FVector::ZeroVector;
	// Hover craft high over centre, yaw so cabin windows face a readable camera push.
	return FTransform(FRotator(0.f, -90.f, 0.f), C + FVector(0.f, 0.f, 2350.f), FVector(2.35f)); // anime-readable cabin scale
}

void ZoogiDropIntro::TacBirdEyePose(float ArenaScale, float& OutYaw, float& OutPitch, float& OutDist, float& OutFov, FVector& OutPivot)
{
	OutYaw = -90.f; // screen-up convention used elsewhere
	OutPitch = 89.5f; // straight-down bird's-eye (Evan: Tac for this drop phase)
	OutDist = 3200.f * FMath::Max(0.5f, ArenaScale);
	OutFov = 72.f;
	OutPivot = FVector(0.f, 0.f, ZoogiConst::ZoogiRadius);
}

void ZoogiDropIntro::CollectShipPartMeshes(TArray<UStaticMesh*>& Out)
{
	Out.Reset();
	// Evan's modeled VIP PlatformShip: multipart Parts under Content/Ships/PlatformShip/Parts
	// (SM_PlatformShip alone is only a Hull_Lower alias from the place script � do NOT use as the ship).
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> As;
	AR.GetAssetsByPath(FName(TEXT("/Game/Ships/PlatformShip/Parts")), As, true);
	As.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	for (const FAssetData& A : As)
	{
		if (A.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName()) continue;
		const FString N = A.AssetName.ToString();
		if (N.StartsWith(TEXT("UCX")) || N.StartsWith(TEXT("UBX")) || N.StartsWith(TEXT("UCP")) || N.StartsWith(TEXT("USP"))) continue;
		// Evan REJECT merge22/24: kill EVERY frosted white bulb in ShipHold.
		// Hull_Canopy = top dome; Hull_Nose = front-left opaque white bulb; SpotHousing sat on canopy.
		// Keep Glass_Panorama (dark glass) + gold trim + Hull_Lower / RearFin + lights.
		if (N.Contains(TEXT("Canopy")) || N.Contains(TEXT("canopy"))
			|| N.Contains(TEXT("Dome")) || N.Contains(TEXT("dome"))
			|| N.Contains(TEXT("Frost")) || N.Contains(TEXT("frost"))
			|| N.Contains(TEXT("Hull_Nose")) || N.Contains(TEXT("SpotHousing")))
		{
			continue;
		}
		if (UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset())) Out.Add(M);
	}
	if (Out.Num() > 0)
	{
		UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: PlatformShip Parts loaded count=%d (modeled VIP ship)"), Out.Num());
		return;
	}
	// Explicit fallback list if AssetRegistry has not scanned yet (Shipping / early load).
	static const TCHAR* PartNames[] = {
		TEXT("Deck_Floor"), TEXT("Glass_Panorama"), /* Canopy+Nose+SpotHousing SKIPPED */ TEXT("Hull_Lower"), TEXT("Hull_RearFin"),
		TEXT("Light_Blue_BellyRing"), TEXT("Light_Blue_Core_0"), TEXT("Light_Blue_Core_1"), TEXT("Light_Blue_Core_2"),
		TEXT("Light_Blue_Core_3"), TEXT("Light_Blue_Core_4"), TEXT("Light_Blue_Core_5"),
		TEXT("Light_Blue_FinStrip_-1"), TEXT("Light_Blue_FinStrip_1"),
		TEXT("Light_Blue_Thruster_0"), TEXT("Light_Blue_Thruster_1"), TEXT("Light_Blue_Thruster_2"),
		TEXT("Light_Blue_Thruster_3"), TEXT("Light_Blue_Thruster_4"), TEXT("Light_Blue_Thruster_5"),
		TEXT("Light_Gold_Belly"), TEXT("Light_SpotLens_0"), TEXT("Light_SpotLens_1"), TEXT("Light_SpotLens_2"), TEXT("Light_SpotLens_3"),
		TEXT("Trim_Gold_BellyPlate"), TEXT("Trim_Gold_DeckInlay_0"), TEXT("Trim_Gold_DeckInlay_1"), TEXT("Trim_Gold_Emblem"),
		TEXT("Trim_Gold_Lower"), TEXT("Trim_Gold_Rail"), TEXT("Trim_Gold_RearFin"), TEXT("Trim_Gold_Upper"),
	};
	for (const TCHAR* Name : PartNames)
	{
		const FString P = FString::Printf(TEXT("/Game/Ships/PlatformShip/Parts/%s.%s"), Name, Name);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *P)) Out.Add(M);
	}
	if (Out.Num() > 0)
	{
		UE_LOG(LogZoogi, Log, TEXT("DROP INTRO: PlatformShip Parts via explicit LoadObject count=%d"), Out.Num());
		return;
	}
	UE_LOG(LogZoogi, Warning, TEXT("DROP INTRO: no PlatformShip Parts � cabin hold seats only (no SharedPlatform stand-in)"));
}

UStaticMesh* ZoogiDropIntro::FindShipMesh()
{
	TArray<UStaticMesh*> Parts;
	CollectShipPartMeshes(Parts);
	return Parts.Num() > 0 ? Parts[0] : nullptr;
}
