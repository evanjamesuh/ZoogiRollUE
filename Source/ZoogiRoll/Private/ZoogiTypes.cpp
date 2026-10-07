#include "ZoogiTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/StrongObjectPtr.h"
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
	// Oct 6: the 8 Buzzy bible characters (Zoogy Roll Master Asset Bible, section 1) replace the old 7. Each one inherits
	// an existing power together with the stats (and so the mass) that power was balanced with; Sera's Rune Seal is a
	// small variant (Shadow Stun's pulse applying Bandage Bind's curse). Table: Docs/BIBLE_CHARACTERS.md.
	TArray<FZoogiDef> R;
	auto P = [&R](EZoogiPower Pw, const TCHAR* Asset) { R.Last().PowerKind = Pw; R.Last().MeshAsset = Asset; R.Last().bPivotFit = true; };
	R.Add(Make("blue", TEXT("Blue"), TEXT("Lead Zoogy"), TEXT("#22C3E6"), TEXT("#E2B866"), TEXT("Resonance Rebound"),
		TEXT("Arm while aiming: after each of your next 2 hits you rebound toward the best target"), 70, 75, 60, 80)); P(EZoogiPower::Ricochet, TEXT("Blue")); // was Lars
	R.Add(Make("tavi", TEXT("Tavi"), TEXT("Best Friend"), TEXT("#F08A1C"), TEXT("#F5C76B"), TEXT("Friend Squad"),
		TEXT("While moving, send three amber echoes that chase the nearest orb or opponent"), 85, 60, 50, 75)); P(EZoogiPower::WolfPack, TEXT("Tavi")); // was Wolfgang
	R.Add(Make("vex", TEXT("Vex"), TEXT("Rival"), TEXT("#D11A2A"), TEXT("#FF7A1A"), TEXT("Rival Blast"),
		TEXT("Create a fiery explosion around you that blasts everything away"), 55, 90, 62, 45)); P(EZoogiPower::Explosion, TEXT("Vex")); // was Hotstreak (balance Oct 4: Defense 62)
	R.Add(Make("moss", TEXT("Moss"), TEXT("Tactician"), TEXT("#2E6B45"), TEXT("#6CC69A"), TEXT("Tactical Lock"),
		TEXT("Laser-guided strike: the shot snaps onto the target nearest your aim and hits 30% harder"), 60, 65, 45, 95)); P(EZoogiPower::LockOn, TEXT("Moss")); // was Pinpoint
	R.Add(Make("orin", TEXT("Master Orin"), TEXT("Arena Keeper"), TEXT("#8E8E8E"), TEXT("#D9A441"), TEXT("Granite Bind"),
		TEXT("Curse enemies on contact: they take the hit 25% harder and their next 2 shots are weak"), 50, 70, 85, 65)); P(EZoogiPower::BandageBind, TEXT("MasterOrin")); // was Wraps
	R.Add(Make("sera", TEXT("Sera"), TEXT("Archivist"), TEXT("#C7B6F2"), TEXT("#D8B26A"), TEXT("Rune Seal"),
		TEXT("A rune ring seals nearby opponents: their next 2 shots are weak"), 58, 62, 66, 84)); P(EZoogiPower::RuneSeal, TEXT("Sera")); // new variant
	R.Add(Make("luma", TEXT("Luma"), TEXT("Announcer"), TEXT("#E81CC8"), TEXT("#F2E23A"), TEXT("Hype Shock"),
		TEXT("Phase through enemies and stun them"), 80, 55, 40, 70)); P(EZoogiPower::StaticShock, TEXT("Luma")); // was Bolt
	R.Add(Make("darkzoogy", TEXT("Dark Zoogy"), TEXT("Ancient Mystery"), TEXT("#7A0E26"), TEXT("#FF3048"), TEXT("Shadow Stun"),
		TEXT("A short shadow pulse that freezes nearby opponents and skips their next turn"), 62, 68, 64, 84)); P(EZoogiPower::ShadowStun, TEXT("DarkZoogy")); // was Nightshade
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

static bool GComboColors = false;
void ZoogiUtil::SetComboColors(bool bOn) { GComboColors = bOn; }

FLinearColor ZoogiUtil::PlayerColor(int32 PlayerIndex)
{
	if (GComboColors)
	{
		static const FLinearColor Team[] = { Hex(TEXT("#38BDF8")), Hex(TEXT("#818CF8")), Hex(TEXT("#F43F5E")), Hex(TEXT("#FB923C")) };
		return Team[FMath::Abs(PlayerIndex) % 4];
	}
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
	case EZoogiArena::Arabian: return TEXT("Arabian Nights");
	case EZoogiArena::Tomb: return TEXT("Pharaoh's Tomb");
	case EZoogiArena::Neon: return TEXT("Night Circuit");
	case EZoogiArena::Training: return TEXT("Training Arena");
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

UMaterialInstanceDynamic* ZoogiUtil::FxMID(UObject* Outer, const FLinearColor& Color, float Strength)
{
	UMaterialInterface* Base = LoadMat(TEXT("/Game/Materials/M_ZoogiFX.M_ZoogiFX"));
	if (!Base) return GlowMID(Outer, Color, Strength);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Strength"), Strength);
	return MID;
}

bool ZoogiUtil::ShowFlipperNubs()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		bool bShow = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Characters"), TEXT("ShowFlipperNubs"), bShow, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNubs"))) bShow = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNubs"))) bShow = true;
		Cached = bShow ? 1 : 0;
	}
	return Cached == 1;
}

bool ZoogiUtil::IsArmedPower(EZoogiPower Power)
{
	return Power == EZoogiPower::Ricochet || Power == EZoogiPower::LockOn || Power == EZoogiPower::StaticShock || Power == EZoogiPower::BandageBind;
}

USoundBase* ZoogiUtil::LoadSound(const TCHAR* Name)
{
	static TMap<FString, TStrongObjectPtr<USoundBase>> Cache;
	if (TStrongObjectPtr<USoundBase>* Found = Cache.Find(Name)) return Found->Get();
	USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/S_%s.S_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Cache.Add(Name, TStrongObjectPtr<USoundBase>(S));
	return S;
}

namespace
{
	// Per-asset loudness trim, measured from the source WAVs (active RMS, Oct 3): brings every SFX to about
	// -14 dBFS before the call-site volume so call sites only express importance. Music plays at 0.35 (~-23 dB),
	// so key events (score, knock-off, win) sit ~6-9 dB above the music and UI ticks just above it.
	float SfxTrim(const TCHAR* Name)
	{
		static const TMap<FString, float> Trim = {
			{ TEXT("Score"), 2.4f },     // -22.6 dB source, quiet chime: +7.6 dB
			{ TEXT("HitClick"), 1.8f },  // -21.8 dB short tick: +5 dB
			{ TEXT("Clack"), 1.25f },    // -18.7 dB transient
			{ TEXT("Fall"), 0.6f },      // -7.4 dB, loudest asset: -4.4 dB
			{ TEXT("Arm"), 0.8f },       // -10.2 dB
			{ TEXT("Zap"), 0.8f },       // -10.1 dB
			{ TEXT("Win"), 0.9f },
			{ TEXT("Turn"), 1.0f },
			{ TEXT("Launch"), 1.0f },
			{ TEXT("Explosion"), 1.0f },
		};
		const float* T = Trim.Find(Name);
		return T ? *T : 1.f;
	}
}

static float GSfxVolume = 1.f;
float ZoogiUtil::GetSfxVolume() { return GSfxVolume; }
void ZoogiUtil::SetSfxVolume(float V) { GSfxVolume = FMath::Clamp(V, 0.f, 1.f); }

void ZoogiUtil::PlaySfx(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float StartTime)
{
	if (GSfxVolume <= 0.001f) return;
	if (USoundBase* S = LoadSound(Name)) UGameplayStatics::PlaySound2D(WorldContext, S, FMath::Min(Volume * SfxTrim(Name) * GSfxVolume, 3.f), Pitch, StartTime, nullptr, nullptr, true);
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
