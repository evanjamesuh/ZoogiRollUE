#include "ZoogiTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

float ZoogiConst::LaunchSpeedForPull(float PullFraction)
{
	const float P = FMath::Clamp(PullFraction, 0.f, 1.f);
	float UnitsPerSec;
	if (P <= SlingshotPull)
	{
		UnitsPerSec = 33.5f * P - 16.875f * P * P;
	}
	else
	{
		const float T = (P - SlingshotPull) / (1.f - SlingshotPull);
		const float Smooth = T * T * (3.f - 2.f * T);
		UnitsPerSec = 16.f + (36.f - 16.f) * Smooth;
	}
	return UnitsPerSec * 100.f;
}

FLinearColor ZoogiUtil::Hex(const TCHAR* InHex)
{
	return FLinearColor(FColor::FromHex(InHex));
}

static TArray<FZoogiDef> BuildRoster()
{
	auto Make = [](FName Id, const TCHAR* Name, const TCHAR* Type, const TCHAR* C1, const TCHAR* C2,
		const TCHAR* Ability, const TCHAR* Desc, int32 Spd, int32 Pow, int32 Def, int32 Ctl)
	{
		FZoogiDef D;
		D.Id = Id; D.Name = Name; D.Type = Type;
		D.Color = ZoogiUtil::Hex(C1); D.Secondary = ZoogiUtil::Hex(C2);
		D.Ability = Ability; D.AbilityDesc = Desc;
		D.Speed = Spd; D.Power = Pow; D.Defense = Def; D.Control = Ctl;
		return D;
	};
	TArray<FZoogiDef> R;
	R.Add(Make("wolfgang", TEXT("Wolfgang"), TEXT("Wolf"), TEXT("#6B7280"), TEXT("#9CA3AF"), TEXT("Wolf Pack"),
		TEXT("While moving, send three clones that chase the nearest orb or opponent"), 85, 60, 50, 75));
	R.Add(Make("hotstreak", TEXT("Hotstreak"), TEXT("Lava"), TEXT("#EF4444"), TEXT("#F97316"), TEXT("Instant Explosion"),
		TEXT("Create a fiery explosion around you"), 55, 90, 70, 45));
	R.Add(Make("lars", TEXT("Lars"), TEXT("Bouncer"), TEXT("#3B82F6"), TEXT("#93C5FD"), TEXT("Ricochet"),
		TEXT("Ricochets toward the nearest target after a hit"), 70, 75, 60, 80));
	R.Add(Make("pinpoint", TEXT("Pinpoint"), TEXT("Precision"), TEXT("#8B5CF6"), TEXT("#A78BFA"), TEXT("Laser Trajectory"),
		TEXT("Lock on to an orb or enemy, then drag to set power - the shot auto-aims"), 60, 65, 45, 95));
	R.Add(Make("bolt", TEXT("Bolt"), TEXT("Electric"), TEXT("#FBBF24"), TEXT("#FDE047"), TEXT("Static Shock"),
		TEXT("Phase through enemies and stun them"), 80, 55, 40, 70));
	R.Add(Make("wraps", TEXT("Wraps"), TEXT("Mummy"), TEXT("#8B7355"), TEXT("#D4C4B0"), TEXT("Bandage Bind"),
		TEXT("Slow enemies on contact with ancient curses"), 50, 70, 85, 65));
	FZoogiDef N = Make("nightshade", TEXT("Nightshade"), TEXT("Shadow"), TEXT("#6B46C1"), TEXT("#9F7AEA"), TEXT("Shadow Stun"),
		TEXT("A short shadow pulse that freezes nearby opponents and skips their next turn"), 62, 68, 64, 84);
	N.bPivotFit = true;
	R.Add(N);
	return R;
}

const TArray<FZoogiDef>& ZoogiUtil::Roster()
{
	static TArray<FZoogiDef> R = BuildRoster();
	return R;
}

const FZoogiDef* ZoogiUtil::FindZoogi(FName Id)
{
	for (const FZoogiDef& D : Roster())
	{
		if (D.Id == Id) return &D;
	}
	return nullptr;
}

FLinearColor ZoogiUtil::PlayerColor(int32 PlayerIndex)
{
	static const FLinearColor Colors[] = {
		Hex(TEXT("#38BDF8")), // P1 sky
		Hex(TEXT("#F43F5E")), // P2 rose
		Hex(TEXT("#FACC15")), // P3 gold
		Hex(TEXT("#4ADE80")), // P4 green
	};
	return Colors[FMath::Abs(PlayerIndex) % 4];
}

FString ZoogiUtil::ArenaName(EZoogiArena Arena)
{
	switch (Arena)
	{
	case EZoogiArena::Meadow: return TEXT("Meadow Arena");
	case EZoogiArena::Frozen: return TEXT("Frozen Ring");
	case EZoogiArena::Volcanic: return TEXT("Volcanic Pit");
	case EZoogiArena::Cosmic: return TEXT("Cosmic Platform");
	default: return TEXT("Arena");
	}
}

static UMaterialInterface* LoadMat(const TCHAR* Path)
{
	return LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

static UMaterialInterface* BasicShapeMat()
{
	static UMaterialInterface* M = LoadMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return M;
}

UMaterialInstanceDynamic* ZoogiUtil::SurfaceMID(UObject* Outer, const FLinearColor& Color, float Roughness,
	const FLinearColor& Emissive, float EmissiveStrength, float Metallic)
{
	UMaterialInterface* Base = LoadMat(TEXT("/Game/Materials/M_ZoogiSurface.M_ZoogiSurface"));
	if (!Base) Base = BasicShapeMat();
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	MID->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
	MID->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
	return MID;
}

UMaterialInstanceDynamic* ZoogiUtil::GlowMID(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Base = LoadMat(TEXT("/Game/Materials/M_ZoogiGlow.M_ZoogiGlow"));
	if (!Base) return SurfaceMID(Outer, Color, 0.2f, Color, Strength);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Strength"), Strength);
	return MID;
}

UMaterialInstanceDynamic* ZoogiUtil::GroundMID(UObject* Outer, const FLinearColor& A, const FLinearColor& B, float NoiseScale, float Roughness)
{
	UMaterialInterface* Base = LoadMat(TEXT("/Game/Materials/M_ZoogiGround.M_ZoogiGround"));
	if (!Base) return SurfaceMID(Outer, A, Roughness);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetVectorParameterValue(TEXT("ColorA"), A);
	MID->SetVectorParameterValue(TEXT("ColorB"), B);
	MID->SetScalarParameterValue(TEXT("NoiseScale"), NoiseScale);
	MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	return MID;
}

UStaticMesh* ZoogiUtil::BasicShape(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
	return LoadObject<UStaticMesh>(nullptr, *Path);
}

UStaticMesh* ZoogiUtil::LoadMesh(const FString& Path)
{
	return LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}
