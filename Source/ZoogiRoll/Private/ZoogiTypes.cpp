#include "ZoogiTypes.h"
#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
	R.Add(Make("darkzoogy", TEXT("Dark Zoogy"), TEXT("Ancient Mystery"), TEXT("#1C2154"), TEXT("#FF8A1E"), /* b202: the new navy + orange Dark Zoogi */ TEXT("Shadow Stun"),
		TEXT("A short shadow pulse that freezes nearby opponents and skips their next turn"), 62, 68, 64, 84)); P(EZoogiPower::ShadowStun, TEXT("DarkZoogy")); // was Nightshade
	// Oct 21: the 7 original Zoogis return alongside the 8 Buzzy ones (15 in all), with their original names, colours, stats
	// and powers. Every roster Zoogi now uses the game-ready Tripo model (/Game/Characters/Tripo/<MeshAsset>, see
	// Scripts/import_tripo_characters.py); the Bible models stay as the fallback.
	R.Add(Make("wolfgang", TEXT("Wolfgang"), TEXT("Wolf"), TEXT("#6B7280"), TEXT("#9CA3AF"), TEXT("Wolf Pack"),
		TEXT("While moving, send three clones that chase the nearest orb or opponent"), 85, 60, 50, 75)); P(EZoogiPower::WolfPack, TEXT("Wolfgang"));
	R.Add(Make("hotstreak", TEXT("Hotstreak"), TEXT("Lava"), TEXT("#EF4444"), TEXT("#F97316"), TEXT("Instant Explosion"),
		TEXT("Create a fiery explosion around you"), 55, 90, 62, 45)); P(EZoogiPower::Explosion, TEXT("Hotstreak"));
	R.Add(Make("lars", TEXT("Lars"), TEXT("Bouncer"), TEXT("#3B82F6"), TEXT("#93C5FD"), TEXT("Ricochet"),
		TEXT("Ricochets toward the nearest target after a hit"), 70, 75, 60, 80)); P(EZoogiPower::Ricochet, TEXT("Lars"));
	R.Add(Make("pinpoint", TEXT("Pinpoint"), TEXT("Precision"), TEXT("#8B5CF6"), TEXT("#A78BFA"), TEXT("Laser Trajectory"),
		TEXT("Laser-guided strike: the locked shot hits 30% harder"), 60, 65, 45, 95)); P(EZoogiPower::LockOn, TEXT("Pinpoint"));
	R.Add(Make("bolt", TEXT("Bolt"), TEXT("Electric"), TEXT("#FBBF24"), TEXT("#FDE047"), TEXT("Static Shock"),
		TEXT("Phase through enemies and stun them"), 80, 55, 40, 70)); P(EZoogiPower::StaticShock, TEXT("Bolt"));
	R.Add(Make("nightshade", TEXT("Nightshade"), TEXT("Shadow"), TEXT("#6B46C1"), TEXT("#9F7AEA"), TEXT("Shadow Stun"),
		TEXT("A short shadow pulse that freezes nearby opponents and skips their next turn"), 62, 68, 64, 84)); P(EZoogiPower::ShadowStun, TEXT("Nightshade"));
	R.Add(Make("wraps", TEXT("Wraps"), TEXT("Mummy"), TEXT("#8B7355"), TEXT("#D4C4B0"), TEXT("Bandage Bind"),
		TEXT("Curse enemies on contact: they take the hit 25% harder and their next 2 shots are weak"), 50, 70, 85, 65)); P(EZoogiPower::BandageBind, TEXT("Wraps"));
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

// Nov 1 (Evan): the four ZOOGI ROLL ARENA logos rotate with the seasons - spring = classic blue / pink / gold crown,
// summer = fire shield, fall = comic magenta shield, winter = ice crown (Content/Data/MenuArt/logo_<season>.png).
// -ZoogiLogoSeason=spring|summer|fall|winter forces one (captures).
FString ZoogiUtil::SeasonLogoKey()
{
	FString Forced;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiLogoSeason="), Forced) && !Forced.IsEmpty()) return FString(TEXT("logo_")) + Forced.ToLower();
	const int32 M = FDateTime::Now().GetMonth();
	const TCHAR* S = (M >= 3 && M <= 5) ? TEXT("spring") : (M >= 6 && M <= 8) ? TEXT("summer") : (M >= 9 && M <= 11) ? TEXT("fall") : TEXT("winter");
	return FString(TEXT("logo_")) + S;
}

FString ZoogiUtil::MenuArtKey(const FString& Key)
{
	return Key == TEXT("zoogi-roll-arena-logo") ? SeasonLogoKey() : Key;
}

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
	case EZoogiArena::Teaser: return TEXT("Teaser Arena");
	case EZoogiArena::Ancient: return TEXT("Ancient Collision Meadows");
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
		else bShow = false; // b150a: the old flipper arms are gone
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
static float GMusicVolume = 1.f;
// Oct 27 (Evan: "delete game music, leave only sfx"): music is off unless [ZoogiRoll.Audio] bMusic=True or -ZoogiMusic.
// Every music path (menu / match tracks, arena stems, stingers) reads this volume and stays silent at 0.
static bool ZoogiMusicOn()
{
	static const bool bOn = []
	{
		bool b = false;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Audio"), TEXT("bMusic"), b, GGameIni);
		return b || FParse::Param(FCommandLine::Get(), TEXT("ZoogiMusic"));
	}();
	return bOn;
}
float ZoogiUtil::GetMusicVolume() { return ZoogiMusicOn() ? GMusicVolume : 0.f; }
void ZoogiUtil::SetMusicVolume(float V) { GMusicVolume = FMath::Clamp(V, 0.f, 1.f); }

// Nov 1 b59 (Evan: "Time to add the most realist sounds for everything"): every PlaySfx name can map to recorded / physically modelled
// sounds - Epic's Lyra sound library (licensed for Unreal projects, copied to /Game/Audio/Sounds/<Dir>) and the modal-synthesis
// impacts in /Game/Audio/Sounds/Real (Scripts/synth_real_sfx.py: heavy ball clacks, glass orb clinks, wall knocks, landing thuds).
// A random variant plays (never the same one twice in a row); Extra adds a second real layer; KeepSynth keeps the old anime
// layer under it at that volume (0 = replaced). -ZoogiSynthSfx = the old synthesized set only.
namespace
{
	struct FRealSfx { TArray<FString> Main, Extra; float Vol = 1.f, ExtraVol = 0.f, KeepSynth = 0.f, ExtraMin = 0.f; };
	TArray<FString> Seq(const TCHAR* Dir, const TCHAR* Stem, int32 N, bool bTwoDigit = true)
	{
		TArray<FString> R;
		for (int32 i = 1; i <= N; ++i)
		{
			const FString A = bTwoDigit ? FString::Printf(TEXT("%s_%02d"), Stem, i) : FString(Stem);
			R.Add(FString::Printf(TEXT("/Game/Audio/Sounds/%s/%s.%s"), Dir, *A, *A));
			if (!bTwoDigit) break;
		}
		return R;
	}
	const TMap<FString, FRealSfx>& RealSfx()
	{
		static TMap<FString, FRealSfx> M;
		if (M.Num()) return M;
		auto Add = [](const TCHAR* Name, TArray<FString> Main, float Vol, float KeepSynth = 0.f, TArray<FString> Extra = {}, float ExtraVol = 0.f, float ExtraMin = 0.f)
		{
			FRealSfx R; R.Main = MoveTemp(Main); R.Vol = Vol; R.KeepSynth = KeepSynth; R.Extra = MoveTemp(Extra); R.ExtraVol = ExtraVol; R.ExtraMin = ExtraMin;
			M.Add(Name, MoveTemp(R));
		};
		// impacts (physically modelled + Lyra body hits)
		static const bool bSoftHits = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSoftHits"));
		Add(TEXT("HitCrack"), Seq(TEXT("Real"), TEXT("SW_BallClack"), 6), 1.0f, 0.f, Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), bSoftHits ? 0.45f : 0.8f, bSoftHits ? 0.55f : 0.35f); // heavy hits get a body thump (b166: louder, on more hits)
		Add(TEXT("HitCrush"), Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 0.7f); // b166 (Evan: "more crushing sounds hard hitting")
		Add(TEXT("HitClick"), Seq(TEXT("Real"), TEXT("SW_BallClack"), 6), 0.8f);
		Add(TEXT("Clack"), Seq(TEXT("Real"), TEXT("SW_WallKnock"), 5), 0.9f);
		Add(TEXT("WallKnock"), Seq(TEXT("Real"), TEXT("SW_WallKnock"), 5), 0.9f);
		Add(TEXT("OrbClink"), Seq(TEXT("Real"), TEXT("SW_OrbClink"), 6), 0.75f);
		Add(TEXT("Land"), Seq(TEXT("Real"), TEXT("SW_Land"), 3), 0.9f);
		Add(TEXT("GuardClang"), Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), 0.7f, 0.5f);
		Add(TEXT("RockCrack"), Seq(TEXT("Impacts"), TEXT("Lyra_ImpactPlaster"), 9), 0.9f, 0.f, Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 0.75f); // b103: + a real rock smash
		// b103 (Evan: "epic devastating ... realistic rock and debris"; "don't take the war drum literally") - Scripts/synth_epic_sfx.py
		Add(TEXT("EpicImpact"), Seq(TEXT("Epic"), TEXT("SW_EpicImpact"), 3), 1.0f);
		Add(TEXT("EpicBig"), Seq(TEXT("Epic"), TEXT("SW_EpicBig"), 2), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.6f);
		Add(TEXT("EpicRiser"), Seq(TEXT("Epic"), TEXT("SW_EpicRiser"), 2), 0.9f);
		Add(TEXT("Debris"), Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.9f);
		Add(TEXT("RockSmash"), Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.55f);
		Add(TEXT("Rumble"), Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.9f);
		Add(TEXT("RockShatter"), Seq(TEXT("Impacts"), TEXT("Lyra_ImpactPlasterDebris"), 7), 1.0f, 0.f, Seq(TEXT("Impacts"), TEXT("Lyra_ImpactGlassDebris"), 7), 0.5f);
		// movement
		Add(TEXT("Dash"), Seq(TEXT("SFX"), TEXT("Lyra_Dash"), 2), 0.9f);
		Add(TEXT("Launch"), Seq(TEXT("SFX"), TEXT("Lyra_JumpPad_Whoosh"), 2), 0.9f, 0.f, Seq(TEXT("SFX"), TEXT("Lyra_JumpPad"), 3), 0.45f);
		Add(TEXT("Whoosh"), Seq(TEXT("SFX"), TEXT("Lyra_MeleeWhoosh"), 4), 0.85f);
		Add(TEXT("ArcWhoosh"), Seq(TEXT("SFX"), TEXT("Lyra_JumpPad_Whoosh"), 2), 0.8f);
		// blasts
		Add(TEXT("Explosion"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.9f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.7f); // b103: + a sub rumble
		Add(TEXT("CritBoom"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.8f, 0.45f, Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.6f); // b103: + falling debris
		Add(TEXT("Fall"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Distant"), 4), 0.8f, 0.35f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.6f);
		// game / UI
		Add(TEXT("Score"), Seq(TEXT("SFX"), TEXT("Lyra_BasePickup"), 3), 0.9f);
		Add(TEXT("Win"), Seq(TEXT("SFX"), TEXT("Lyra_EndOfRound-Victory_01"), 1, false), 0.9f);
		Add(TEXT("Arm"), Seq(TEXT("Foley_Misc"), TEXT("Lyra_WeaponSwap"), 3), 0.8f);
		Add(TEXT("UiClick"), Seq(TEXT("UI"), TEXT("ButtonClick"), 1, false), 0.8f);
		Add(TEXT("UiConfirm"), Seq(TEXT("UI"), TEXT("Lyra_UI_MainMenu_Select"), 1, false), 0.8f);
		Add(TEXT("UiCancel"), Seq(TEXT("UI"), TEXT("Lyra_UI_SubMenu_Back"), 1, false), 0.8f);
		// Aurex form
		Add(TEXT("AurexIn"), Seq(TEXT("SFX"), TEXT("Lyra_Plyr_Spawn"), 2), 0.8f);
		Add(TEXT("Grab"), Seq(TEXT("Real"), TEXT("SW_Land"), 3), 0.8f, 0.f, Seq(TEXT("Foley_Misc"), TEXT("Lyra_FoleyStep"), 9), 0.6f);
		Add(TEXT("Pull"), Seq(TEXT("Foley_Misc"), TEXT("Lyra_FoleyStep"), 9), 0.7f);
		return M;
	}
	USoundBase* RealVariant(const TArray<FString>& Paths, const FString& Key)
	{
		static TMap<FString, TStrongObjectPtr<USoundBase>> Cache;
		static TMap<FString, int32> Last;
		if (Paths.Num() == 0) return nullptr;
		int32 I = FMath::RandRange(0, Paths.Num() - 1);
		if (Paths.Num() > 1) { int32& L = Last.FindOrAdd(Key, -1); if (I == L) I = (I + 1) % Paths.Num(); L = I; }
		const FString& P = Paths[I];
		if (TStrongObjectPtr<USoundBase>* F = Cache.Find(P)) return F->Get();
		USoundBase* S = LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Cache.Add(P, TStrongObjectPtr<USoundBase>(S));
		return S;
	}
}

void ZoogiUtil::PlaySfx(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float StartTime)
{
	if (GSfxVolume <= 0.001f) return;
	static const bool bSynthOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSynthSfx"));
	float SynthVol = 1.f;
	if (!bSynthOnly)
		if (const FRealSfx* R = RealSfx().Find(Name))
		{
			if (USoundBase* S = RealVariant(R->Main, Name))
			{
				UGameplayStatics::PlaySound2D(WorldContext, S, FMath::Min(Volume * R->Vol * GSfxVolume, 3.f), Pitch * FMath::FRandRange(0.97f, 1.03f), 0.f, nullptr, nullptr, true);
				if (R->Extra.Num() && Volume >= R->ExtraMin)
					if (USoundBase* X = RealVariant(R->Extra, FString(Name) + TEXT("+")))
						UGameplayStatics::PlaySound2D(WorldContext, X, FMath::Min(Volume * R->ExtraVol * GSfxVolume, 3.f), Pitch, 0.f, nullptr, nullptr, true);
				SynthVol = R->KeepSynth;
				if (SynthVol <= 0.f) return;
			}
		}
	if (USoundBase* S = LoadSound(Name)) UGameplayStatics::PlaySound2D(WorldContext, S, FMath::Min(Volume * SynthVol * SfxTrim(Name) * GSfxVolume, 3.f), Pitch, StartTime, nullptr, nullptr, true);
}

void ZoogiUtil::PlaySfxShort(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float MaxSeconds)
{
	if (GSfxVolume <= 0.001f) return;
	USoundBase* S = nullptr; float V = Volume;
	if (const FRealSfx* R = RealSfx().Find(Name)) { S = RealVariant(R->Main, Name); V *= R->Vol; }
	if (!S) S = LoadSound(Name);
	if (!S) return;
	UAudioComponent* AC = UGameplayStatics::SpawnSound2D(WorldContext, S, FMath::Min(V * GSfxVolume, 3.f), Pitch, 0.f, nullptr, true, true);
	if (!AC) return;
	TWeakObjectPtr<UAudioComponent> WAC = AC;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WAC](float) { if (UAudioComponent* A = WAC.Get()) A->FadeOut(0.08f, 0.f); return false; }), FMath::Max(0.05f, MaxSeconds));
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
