// Animations & Powers: TITAN METEOR archetype (Episode Engine spec v1.1 section 11, priority 10). Presentation only.
#include "ZoogiTitanMeteorFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* TM_TombRock = TEXT("/Game/Env/Incoming/tomb/SM_Boulder_A/SM_Boulder_A/StaticMeshes/SM_Boulder_A");
	const TCHAR* TM_TombRockMat = TEXT("/Game/Env/Incoming/tomb/SM_Boulder_A/SM_Boulder_A/Materials/Tomb_CanyonRock");
	const TCHAR* TM_KfxRock = TEXT("/Game/VFX/KriptoFX/Effect5/Meshes/SM_Rock");
	const TCHAR* TM_KfxRock2 = TEXT("/Game/VFX/KriptoFX/Effect11/Meshes/SM_Rock2");
	const TCHAR* TM_KfxRock3 = TEXT("/Game/VFX/KriptoFX/Effect11/Meshes/SM_Rock3");
	const TCHAR* TM_Sphere = TEXT("/Engine/BasicShapes/Sphere");
	const TCHAR* TM_Plane = TEXT("/Engine/BasicShapes/Plane");
	const TCHAR* TM_DustMat = TEXT("/Game/Materials/M_ZoogiDust");
	const TCHAR* TM_DistortMat = TEXT("/Game/VFX/KriptoFX/Effect11/Materials/MI_KFX_Effect11_distortDistortionmatDistortion1png");
	// Titan palette: warm stone / amber, kept well below orb brightness (orb core emissive ~12) and away from the cyan
	// Frozen ice cracks.
	const FLinearColor TM_Amber(1.0f, 0.52f, 0.20f);
	const FLinearColor TM_Stone(0.92f, 0.84f, 0.72f);
	constexpr int32 TM_NumChunks = 14, TM_NumPebbles = 18, TM_NumPuffs = 12, TM_NumDust = 16;
}

AZoogiTitanMeteorFX::AZoogiTitanMeteorFX()
{
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Perception"));
	Post->SetupAttachment(RootComponent);
	Post->bUnbound = true;
	Post->Priority = 10000.f;   // above the arena volume; grade uses the *Midtones/*Shadows channels the game camera does not override
	Post->BlendWeight = 0.f;
}

AZoogiTitanMeteorFX* AZoogiTitanMeteorFX::PlayMeteor(UWorld* World, const FVector& Start, const FVector& Impact, float InSeconds, const FZoogiMeteorOptions& InOptions)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiTitanMeteorFX* X = World->SpawnActor<AZoogiTitanMeteorFX>(AZoogiTitanMeteorFX::StaticClass(), FTransform(Impact), P);
	if (!X) return nullptr;
	X->Options = InOptions;
	X->Play(Start, Impact, InSeconds);
	return X;
}

void AZoogiTitanMeteorFX::Play(FVector Start, FVector Impact, float InSeconds)
{
	if (bPlaying) return;
	Seconds = FMath::Clamp(InSeconds, 0.6f, 8.f);
	bShort = Seconds < 1.6f;
	ImpactU = Options.ImpactFraction > 0.f ? FMath::Clamp(Options.ImpactFraction, 0.3f, 0.95f) : (bShort ? 0.62f : 0.80f);
	StartP = Start; ImpactP = Impact;
	Dir = FVector(Impact.X - Start.X, Impact.Y - Start.Y, 0.f).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
	FloorZv = FloorZ(Impact, Impact.Z - ZoogiConst::ZoogiRadius);
	Contact = FVector(Impact.X, Impact.Y, FloorZv);
	SetActorLocation(Contact);
	Rng.Initialize(int32(Impact.X * 7.f + Impact.Y * 13.f) ^ 0x51F0);
	Build();
	const float H = Options.SkyHeight * (bShort ? 0.6f : 1.f);
	RockEnd = Contact + FVector(0, 0, RockR * 0.88f);
	// comes in down the attack line (from behind the attacker), biased toward the far side of the current view so a
	// high gameplay camera sees it arrive instead of having it fall past the lens
	FVector Away = Contact - ViewPos(); Away.Z = 0.f; Away = Away.GetSafeNormal();
	From = (-Dir * 0.6f + Away).GetSafeNormal();
	if (From.IsNearlyZero()) From = -Dir;
	SkyFrom = RockEnd + From * H * 0.75f + FVector(0, 0, H * 0.8f);
	RockPos = PrevRockPos = SkyFrom;
	RockRot0 = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f));
	BuildCracks();
	Age = 0.f;
	bPlaying = true;
	if (Options.bAudio)
	{
		const float Vol = ZoogiUtil::GetSfxVolume();
		if (USoundBase* S = ZoogiUtil::LoadSound(TEXT("ClimaxRumble")))
		{
			Rumble = UGameplayStatics::CreateSound2D(this, S, 0.9f * Vol, 0.8f, 0.f, nullptr, false, true);
			if (Rumble) { Rumble->bIsUISound = true; Rumble->FadeIn(FMath::Min(0.6f, Seconds * 0.2f), 0.9f * Vol); }
		}
		Sfx(TEXT("StingTitan"), 0.55f, 0.85f);
	}
	UE_LOG(LogZoogi, Log, TEXT("APFX meteor play start=(%.0f,%.0f,%.0f) impact=(%.0f,%.0f,%.0f) floor=%.0f seconds=%.2f impact_at=%.2f short=%d rock=%.0fcm"),
		Start.X, Start.Y, Start.Z, Impact.X, Impact.Y, Impact.Z, FloorZv, Seconds, Seconds * ImpactU, bShort ? 1 : 0, RockR * 2.f);
}

void AZoogiTitanMeteorFX::Sfx(const TCHAR* Name, float Vol, float Pitch)
{
	if (Options.bAudio) ZoogiUtil::PlaySfx(this, Name, Vol, Pitch);
}

void AZoogiTitanMeteorFX::Build()
{
	int32 Variant = Options.RockVariant;
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiMeteorRock="), Variant);
	if (Variant < 0) Variant = 0;
	// --- the rock
	UMaterialInterface* ChunkMat = nullptr;
	if (Variant == 0)
	{
		Rock = MakeMesh(TM_TombRock, nullptr, false);
		ChunkMat = Cast<UMaterialInterface>(LoadAsset(TM_TombRockMat));
	}
	if (!Rock || !Rock->GetStaticMesh())
	{
		UMaterialInstanceDynamic* M = MID(TEXT("MI_ZAFX_Rock"));
		if (M) { M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.20f, 0.17f, 0.14f)); M->SetVectorParameterValue(TEXT("RimColor"), FLinearColor(0.10f, 0.045f, 0.015f)); }
		Rock = MakeMesh(TM_KfxRock, M, false);
		ChunkMat = M;
	}
	Rock->bCastDynamicShadow = false; // Oct 14: no hard sun shadow (read as a solid black blob); the soft disc below is the shadow
	Rock->SetCastShadow(false);
	RockR = Options.RockDiameter * (bShort ? 0.85f : 1.f) * 0.5f;
	if (UStaticMesh* SM = Rock->GetStaticMesh()) RockUnit = (RockR * 2.f) / FMath::Max(1.f, 2.f * SM->GetBounds().BoxExtent.GetMax());

	for (int32 i = 0; i < TM_NumChunks; ++i) { Chunks.Add(MakeMesh(i % 2 ? TM_KfxRock2 : TM_KfxRock3, ChunkMat, false)); ChunkB.AddDefaulted(); }
	for (int32 i = 0; i < TM_NumPebbles; ++i) { Pebbles.Add(MakeMesh(i % 2 ? TM_KfxRock3 : TM_KfxRock2, ChunkMat, false)); PebbleB.AddDefaulted(); }
	auto Dust = [&](const FLinearColor& C)
	{
		UMaterialInstanceDynamic* M = MID(TM_DustMat);
		if (M) { M->SetVectorParameterValue(TEXT("Color"), C); M->SetScalarParameterValue(TEXT("Opacity"), 0.f); M->SetScalarParameterValue(TEXT("Soft"), 1.f); M->SetScalarParameterValue(TEXT("FadeDistance"), 60.f); }
		return MakeMesh(TM_Sphere, M, false);
	};
	for (int32 i = 0; i < TM_NumPuffs; ++i) { Puffs.Add(Dust(FLinearColor(0.32f, 0.29f, 0.26f))); PuffB.AddDefaulted(); PuffB.Last().Born = -99.f; }
	for (int32 i = 0; i < TM_NumDust; ++i) { DustRing.Add(Dust(FLinearColor(0.34f, 0.30f, 0.26f))); DustB.AddDefaulted(); }
	if (UMaterialInstanceDynamic* M = MID(TM_DistortMat)) { Refract = MakeMesh(TM_Plane, M, false); M->SetScalarParameterValue(TEXT("Strength"), 0.f); }

	// Frozen floor takes no deferred decals: a flat unlit alpha card (M_FrozenDecal, the arena's own scar material) with
	// the Titan scar texture, dark stone + faint amber, fades away after the impact (no fake terrain damage left).
	if (UMaterialInterface* FD = Cast<UMaterialInterface>(LoadAsset(TEXT("/Game/Env/Arenas/M_FrozenDecal"))))
	{
		UTexture* T = Cast<UTexture>(LoadAsset(TEXT("/Game/Env/Arenas/T_FrozenDisp_Titan")));
		if (!T) T = Cast<UTexture>(LoadAsset(TEXT("/Game/Env/Arenas/T_FrozenDmg_Crater")));
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(FD, this);
		if (M && T)
		{
			MIDs.Add(M);
			M->SetTextureParameterValue(TEXT("Tex"), T);
			M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.30f, 0.26f, 0.22f));
			M->SetScalarParameterValue(TEXT("Bright"), 0.55f);
			M->SetScalarParameterValue(TEXT("Opacity"), 0.f);
			M->SetVectorParameterValue(TEXT("EmissiveTint"), TM_Amber);
			M->SetScalarParameterValue(TEXT("Emissive"), 0.f);
			Scar = MakeMesh(TM_Plane, M, false);
		}
	}
	// section 4: lit translucent dark dust (M_ZoogiDust; vertex alpha fades) for the soft shadow, crack backing, dust skirt
	if (UMaterialInstanceDynamic* M = MID(TM_DustMat))
	{
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		M->SetScalarParameterValue(TEXT("Opacity"), 1.f); M->SetScalarParameterValue(TEXT("Soft"), 0.f); M->SetScalarParameterValue(TEXT("FadeDistance"), 1.f);
		Quads->SetMaterial(4, M);
	}
	ImpactLight = MakeLight(TM_Stone, 3200.f);
	UnderLight = MakeLight(TM_Amber, RockR * 2.6f);

	// --- quad sections: 0 cracks (bolt), 1 flash / heat glow, 2 shock ring, 3 motes
	if (UMaterialInstanceDynamic* M = MID(TEXT("MI_ZAFX_Bolt")))
	{
		M->SetVectorParameterValue(TEXT("Tint"), TM_Amber); M->SetScalarParameterValue(TEXT("Glow"), 1.5f);
		M->SetScalarParameterValue(TEXT("CoreGlow"), 0.7f); M->SetVectorParameterValue(TEXT("CoreTint"), FLinearColor(1.f, 0.86f, 0.62f));
		Quads->SetMaterial(0, M);
	}
	if (UMaterialInstanceDynamic* M = MID(TEXT("MI_ZAFX_Flash"))) { M->SetVectorParameterValue(TEXT("Tint"), TM_Stone); M->SetScalarParameterValue(TEXT("Glow"), 1.2f); Quads->SetMaterial(1, M); }
	if (UMaterialInstanceDynamic* M = MID(TEXT("MI_ZAFX_Orb")))
	{
		if (UTexture* T = Cast<UTexture>(LoadAsset(TEXT("/Game/VFX/AwakenFX/Textures/T_ZAFX_Ring")))) M->SetTextureParameterValue(TEXT("Tex"), T);
		M->SetVectorParameterValue(TEXT("Tint"), TM_Stone); M->SetScalarParameterValue(TEXT("Glow"), 1.1f);
		Quads->SetMaterial(2, M);
	}
	if (UMaterialInstanceDynamic* M = MID(TEXT("MI_ZAFX_Mote"))) { M->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.f, 0.78f, 0.52f)); M->SetScalarParameterValue(TEXT("Glow"), 1.2f); Quads->SetMaterial(3, M); }

	// --- pebbles waiting on the floor in a loose ring around the impact (kept off the target itself)
	for (int32 i = 0; i < TM_NumPebbles; ++i)
	{
		FBody& B = PebbleB[i];
		const float A = (i + Rng.FRandRange(-0.4f, 0.4f)) / TM_NumPebbles * 2.f * PI;
		const float Rad = Rng.FRandRange(160.f, 720.f);
		B.P = Contact + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rad;
		B.P.Z = FloorZ(B.P, FloorZv) + 4.f;
		B.S0 = Rng.FRandRange(7.f, 20.f);
		B.R = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f);
		B.W = FRotator(Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(-120.f, 120.f), 0.f);
		B.Born = Rng.FRandRange(0.f, 0.25f);   // lift delay (fraction of the descent)
		B.V = FVector(0, 0, Rng.FRandRange(60.f, 190.f));   // lift height
	}
}

void AZoogiTitanMeteorFX::BuildCracks()
{
	// Titan-shaped fracture: 6 heavy jagged branches from the impact with forks (laid flat on the floor)
	const int32 NMain = 6;
	const float Base = Rng.FRandRange(0.f, 60.f);
	for (int32 b = 0; b < NMain; ++b)
	{
		FCrack C;
		const float A = FMath::DegreesToRadians(Base + b * 360.f / NMain + Rng.FRandRange(-18.f, 18.f));
		FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
		const float Len = Rng.FRandRange(0.75f, 1.15f) * RockR * 1.05f;
		FVector P = Contact;
		C.Pts.Add(P);
		const int32 N = 11;
		for (int32 i = 1; i <= N; ++i)
		{
			D = D.RotateAngleAxis(Rng.FRandRange(-24.f, 24.f), FVector::UpVector);
			P += D * Len / N;
			C.Pts.Add(P);
		}
		C.Width = Rng.FRandRange(30.f, 44.f);
		Cracks.Add(C);
		for (int32 f = 0; f < 2; ++f)
		{
			FCrack F;
			const int32 ForkAt = Rng.RandRange(3, 7);
			FVector Q = C.Pts[ForkAt];
			FVector E = (C.Pts[ForkAt + 1] - C.Pts[ForkAt]).GetSafeNormal().RotateAngleAxis((f ? 1.f : -1.f) * Rng.FRandRange(30.f, 60.f), FVector::UpVector);
			F.Pts.Add(Q);
			const float FL = Len * Rng.FRandRange(0.25f, 0.45f);
			for (int32 i = 1; i <= 6; ++i) { E = E.RotateAngleAxis(Rng.FRandRange(-30.f, 30.f), FVector::UpVector); Q += E * FL / 6.f; F.Pts.Add(Q); }
			F.Width = C.Width * 0.45f;
			F.Delay = 0.05f + ForkAt * 0.012f;
			Cracks.Add(F);
		}
	}
	for (FCrack& C : Cracks) for (FVector& P : C.Pts) P.Z = FloorZ(P, FloorZv) + 2.5f;
}

FVector AZoogiTitanMeteorFX::GetRockLocation() const { return RockPos; }

float AZoogiTitanMeteorFX::GetShake() const
{
	const float U = GetProgress();
	if (!bImpacted) { const float D = FMath::Clamp((U - 0.05f) / FMath::Max(0.05f, ImpactU - 0.05f), 0.f, 1.f); return 0.35f * D * D; }
	return FMath::Exp(-(Age - ImpactAge) * 5.f);
}

bool AZoogiTitanMeteorFX::GetSuggestedView(FVector& L, FRotator& R, float& Fov) const
{
	if (!bPlaying) return false;
	const float U = GetProgress();
	const FVector Side = FVector::CrossProduct(From, FVector::UpVector);
	FVector Look;
	if (U < ImpactU * 0.4f)
	{
		// A / D: low angle beside the target, looking up the attack line at the incoming mass
		L = Contact - From * 560.f + Side * 240.f + FVector(0, 0, 170.f);
		Look = FMath::Lerp(Contact + FVector(0, 0, 150.f), RockPos, 0.32f);
		Fov = 78.f;
	}
	else if (U < ImpactU - 0.05f)
	{
		// E: pull far wider, side-on, so the arena and the rock read at the same time (scale reveal)
		L = Contact + Side * 5200.f - From * 1400.f + FVector(0, 0, 900.f);
		Look = FMath::Lerp(Contact, RockPos, 0.36f);
		Fov = 70.f;
	}
	else
	{
		// impact: above and behind the attack line, the whole ring in frame for the shockwave
		L = Contact - From * 2600.f + Side * 900.f + FVector(0, 0, 1700.f);   // facing the incoming mass
		Look = Contact + FVector(0, 0, 120.f);
		Fov = 72.f;
	}
	R = (Look - L).Rotation();
	return true;
}

void AZoogiTitanMeteorFX::Stop(float FadeSeconds)
{
	if (StopAt >= 0.f) return;
	StopAt = Age; StopLen = FMath::Max(0.05f, FadeSeconds);
	if (Rumble) Rumble->FadeOut(StopLen, 0.f);
}

float AZoogiTitanMeteorFX::GlobalFade() const
{
	return StopAt < 0.f ? 1.f : 1.f - FMath::Clamp((Age - StopAt) / StopLen, 0.f, 1.f);
}

void AZoogiTitanMeteorFX::Impact()
{
	bImpacted = true;
	ImpactAge = Age;
	if (Rock) Rock->SetVisibility(false);
	if (Rumble) Rumble->FadeOut(0.05f, 0.f);
	Sfx(TEXT("CritBoom"), 1.0f, 0.7f);
	Sfx(TEXT("RockShatter"), 0.9f, 0.75f);
	Sfx(TEXT("ClimaxRumble"), 0.45f, 0.6f);   // low ringing aftermath
	// the mass collapses into chunks that blow out and shrink away ("the perception space collapses into particles")
	for (int32 i = 0; i < ChunkB.Num(); ++i)
	{
		FBody& B = ChunkB[i];
		const float A = (i + Rng.FRandRange(-0.35f, 0.35f)) / ChunkB.Num() * 2.f * PI;
		const FVector H(FMath::Cos(A), FMath::Sin(A), 0.f);
		B.P = Contact + H * RockR * Rng.FRandRange(0.15f, 0.45f) + FVector(0, 0, RockR * Rng.FRandRange(0.15f, 0.5f));
		B.V = (H + FVector(0, 0, Rng.FRandRange(0.2f, 0.65f))).GetSafeNormal() * Rng.FRandRange(900.f, 1700.f) * (bShort ? 1.3f : 1.f);
		B.S0 = RockR * Rng.FRandRange(0.07f, 0.16f);
		B.R = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f);
		B.W = FRotator(Rng.FRandRange(-200.f, 200.f), Rng.FRandRange(-260.f, 260.f), 0.f);
	}
	for (int32 i = 0; i < DustB.Num(); ++i)
	{
		FBody& B = DustB[i];
		const float A = (i + Rng.FRandRange(-0.3f, 0.3f)) / DustB.Num() * 2.f * PI;
		B.V = FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
		B.P = Contact + B.V * RockR * 0.25f;
		B.S0 = Rng.FRandRange(0.8f, 1.25f);
	}
	for (FBody& B : PebbleB)
	{
		// blown outward from the impact
		FVector Out = B.P - Contact; Out.Z = 0.f;
		B.W *= 3.f;
		B.Born = -1.f; // flag: flying
		B.V = Out.GetSafeNormal() * Rng.FRandRange(900.f, 1600.f) + FVector(0, 0, Rng.FRandRange(250.f, 600.f));
	}
	OnImpact.Broadcast(Contact);
	UE_LOG(LogZoogi, Log, TEXT("APFX meteor impact at (%.0f,%.0f,%.0f) t=%.2fs"), Contact.X, Contact.Y, Contact.Z, Age);
}

bool AZoogiTitanMeteorFX::TickFX(float Dt, float U)
{
	const float G = GlobalFade();
	if (StopAt >= 0.f && G <= 0.f) return false;
	const float D0 = 0.05f;
	const float Desc = FMath::Clamp((U - D0) / FMath::Max(0.05f, ImpactU - D0), 0.f, 1.f);   // 0..1 descent
	if (!bImpacted && U >= ImpactU && StopAt < 0.f) Impact();
	const float After = bImpacted ? Age - ImpactAge : -1.f;
	const float Tail = FMath::Max(0.1f, Seconds * (1.f - ImpactU));                         // real seconds after impact
	const float AfterU = bImpacted ? FMath::Clamp(After / Tail, 0.f, 1.f) : 0.f;

	// ---- perception space: darken + warm-desaturate, lift after impact so the real arena returns clean
	if (Post)
	{
		float W = Options.bPerceptionSpace ? Window(U, 0.f, bShort ? 0.12f : 0.16f) : 0.f;
		if (bImpacted) W *= 1.f - Smooth(AfterU / 0.75f);
		Post->BlendWeight = W * G;
		FPostProcessSettings& S = Post->Settings;
		// the gameplay camera overrides global Saturation/Gain/Vignette, so grade through the tonal-range channels
		const FVector4 Sat(0.6f, 0.6f, 0.6f, 1.f), Gain(0.9f, 0.78f, 0.64f, 1.f);
		S.bOverride_ColorSaturationShadows = S.bOverride_ColorSaturationMidtones = S.bOverride_ColorSaturationHighlights = true;
		S.ColorSaturationShadows = S.ColorSaturationMidtones = S.ColorSaturationHighlights = Sat;
		S.bOverride_ColorGainShadows = S.bOverride_ColorGainMidtones = S.bOverride_ColorGainHighlights = true;
		S.ColorGainShadows = S.ColorGainMidtones = S.ColorGainHighlights = Gain;
		S.bOverride_AutoExposureBias = true;
		const float Flash = bImpacted ? FMath::Exp(-After * 14.f) : 0.f;
		S.AutoExposureBias = -1.2f + Flash * 0.7f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.85f;
	}

	// ---- the rock: slow far reveal, then accelerating down the attack line; slow heavy tumble
	if (Rock && !bImpacted)
	{
		const float S = FMath::Pow(Desc, 2.1f);
		PrevRockPos = RockPos;
		RockPos = FMath::Lerp(SkyFrom, RockEnd, S);
		Rock->SetVisibility(U >= D0 * 0.5f && G > 0.f);
		Rock->SetWorldLocationAndRotation(RockPos, RockRot0 + FRotator(Age * 5.f, Age * 7.f, Age * 3.f));
		Rock->SetWorldScale3D(FVector(RockUnit * (G > 0.f ? 1.f : 0.001f)));
		if (UnderLight)
		{
			UnderLight->SetWorldLocation(RockPos - FVector(0, 0, RockR * 1.05f) - Dir * RockR * 0.2f);
			UnderLight->SetIntensity(14.f * Window(Desc, 0.2f, 0.9f) * G);
		}
		// smoke trail shed from the back of the rock while it is high
		PuffAcc += Dt * (bShort ? 18.f : 11.f) * (Desc < 0.92f ? 1.f : 0.f);
		while (PuffAcc >= 1.f && Puffs.Num())
		{
			PuffAcc -= 1.f;
			FBody& B = PuffB[NextPuff];
			const FVector Travel = (RockEnd - SkyFrom).GetSafeNormal();
			B.P = RockPos - Travel * RockR * 0.7f + Rng.GetUnitVector() * RockR * 0.3f;
			B.Born = Age; B.S0 = RockR * Rng.FRandRange(0.55f, 0.85f);
			NextPuff = (NextPuff + 1) % Puffs.Num();
		}
	}
	else if (UnderLight) UnderLight->SetIntensity(0.f);
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		const FBody& B = PuffB[i];
		const float A = Age - B.Born;
		const float Life = 1.1f;
		const bool bOn = A >= 0.f && A < Life;
		Puffs[i]->SetVisibility(bOn);
		if (!bOn) continue;
		const float K = A / Life;
		Puffs[i]->SetWorldLocation(B.P + FVector(0, 0, 120.f * A));
		Puffs[i]->SetWorldScale3D(FVector(B.S0 * (1.f + 0.9f * K) / 50.f));
		SetScalar(Puffs[i], TEXT("Opacity"), 0.32f * (1.f - K) * Window(K, 0.f, 0.15f) * G);
	}

	// ---- scar card: the Titan fracture stain appears with the impact, then fades (nothing persists)
	if (Scar)
	{
		const float K = bImpacted ? After : 0.f;
		const float Op = bImpacted ? Ease(K / 0.12f) * (1.f - Smooth((AfterU - 0.35f) / 0.6f)) : 0.f;
		Scar->SetVisibility(Op * G > 0.01f);
		const float Half = RockR * 1.25f;
		Scar->SetWorldLocationAndRotation(Contact + FVector(0, 0, 3.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(From.Y, From.X)), 0.f));
		Scar->SetWorldScale3D(FVector(Half / 50.f, Half / 50.f, 1.f));
		SetScalar(Scar, TEXT("Opacity"), 0.85f * Op * G);
		SetScalar(Scar, TEXT("Emissive"), 0.35f * FMath::Exp(-K * 2.5f) * G);
	}

	// ---- pebbles lift and vibrate before the impact, blow out after
	for (int32 i = 0; i < Pebbles.Num(); ++i)
	{
		FBody& B = PebbleB[i];
		UStaticMeshComponent* C = Pebbles[i];
		float Scale = B.S0;
		FVector P = B.P;
		if (B.Born >= 0.f)
		{
			const float L = Window(Desc, 0.1f + B.Born, 0.75f + B.Born * 0.3f);
			P.Z += B.V.Z * L;
			const float J = (2.f + 10.f * Desc * Desc) * L;
			P += FVector(FMath::Sin(Age * 41.f + i), FMath::Cos(Age * 37.f + i * 1.7f), FMath::Sin(Age * 53.f + i * 0.3f)) * J;
			B.R += B.W * Dt * L * 0.4f;
			Scale *= Window(U, 0.f, 0.12f);
		}
		else
		{
			B.V.Z -= 980.f * Dt;
			B.P += B.V * Dt;
			B.R += B.W * Dt;
			P = B.P;
			Scale *= 1.f - Smooth(AfterU / 0.6f);
		}
		C->SetVisibility(Scale * G > 0.5f);
		if (UStaticMesh* SM = C->GetStaticMesh()) C->SetWorldScale3D(FVector(Scale / FMath::Max(1.f, SM->GetBounds().BoxExtent.GetMax())));
		C->SetWorldLocationAndRotation(P, B.R);
	}

	// ---- impact: chunks, dust wall, refraction ring
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		UStaticMeshComponent* C = Chunks[i];
		if (!bImpacted) { C->SetVisibility(false); continue; }
		FBody& B = ChunkB[i];
		B.V.Z -= 1400.f * Dt;
		B.V *= FMath::Exp(-1.2f * Dt);
		B.P += B.V * Dt;
		B.R += B.W * Dt;
		const float S = B.S0 * (1.f - Smooth(After / FMath::Min(0.6f, Tail * 0.8f)));
		C->SetVisibility(S * G > 2.f);
		if (UStaticMesh* SM = C->GetStaticMesh()) C->SetWorldScale3D(FVector(S / FMath::Max(1.f, SM->GetBounds().BoxExtent.GetMax())));
		C->SetWorldLocationAndRotation(B.P, B.R);
	}
	for (int32 i = 0; i < DustRing.Num(); ++i)
	{
		UStaticMeshComponent* C = DustRing[i];
		if (!bImpacted) { C->SetVisibility(false); continue; }
		const FBody& B = DustB[i];
		const float K = FMath::Clamp(After / FMath::Min(1.2f, Tail), 0.f, 1.f);
		const float Rad = RockR * (0.25f + 1.35f * Ease(K));
		const float Sz = (110.f + 260.f * Ease(K)) * B.S0;
		C->SetVisibility(K < 1.f && G > 0.f);
		C->SetWorldLocation(Contact + B.V * Rad + FVector(0, 0, Sz * 0.35f));
		C->SetWorldScale3D(FVector(Sz / 50.f, Sz / 50.f, Sz * 0.6f / 50.f));
		SetScalar(C, TEXT("Opacity"), 0.6f * (1.f - K) * Window(K, 0.f, 0.08f) * G);
	}
	if (Refract)
	{
		const float K = bImpacted ? FMath::Clamp(After / 0.6f, 0.f, 1.f) : 1.f;
		Refract->SetVisibility(bImpacted && K < 1.f);
		const float Rad = 300.f + 2600.f * Ease(K);
		Refract->SetWorldLocationAndRotation(Contact + FVector(0, 0, 25.f), FRotator::ZeroRotator);
		Refract->SetWorldScale3D(FVector(Rad / 50.f, Rad / 50.f, 1.f));
		SetScalar(Refract, TEXT("Strength"), 0.06f * (1.f - K) * G);
	}
	if (ImpactLight)
	{
		ImpactLight->SetWorldLocation(Contact + FVector(0, 0, 350.f));
		ImpactLight->SetIntensity(bImpacted ? 45.f * FMath::Exp(-After * 5.f) * G : 0.f);
	}

	// ---- quads
	BFlat.Reset(); BGlow.Reset(); BRing.Reset(); BMote.Reset(); BDark.Reset();
	// soft shadow of the incoming mass pooling over the target (dark, never glowing), then the crater shadow lifting
	{
		const float S = FMath::Pow(Desc, 1.5f);
		float Op = (0.03f + 0.32f * S) * Window(U, 0.02f, 0.2f); // Oct 14: faint, grows as the rock nears
		float Rad = RockR * (0.7f + 1.0f * S);
		if (bImpacted) { Op = 0.35f * (1.f - Smooth(AfterU / 0.5f)); Rad *= 1.f + 0.5f * AfterU; }
		Op *= G;
		if (Op > 0.01f) Disc(BDark, Contact + FVector(0, 0, 4.f), Rad, FLinearColor(0.025f, 0.022f, 0.02f, Op), FLinearColor(0.025f, 0.022f, 0.02f, 0.f), 28);
	}
	if (bImpacted)
	{
		// Titan fracture lines rip out across the floor, then cool and fade
		const float Cool = 1.f - Smooth((After - 0.25f) / FMath::Max(0.2f, Tail * 0.7f));
		for (const FCrack& C : Cracks)
		{
			const float Rv = Ease((After - C.Delay) / 0.14f);
			if (Rv <= 0.f || Cool <= 0.f) continue;
			const float Fn = Rv * (C.Pts.Num() - 1);
			const int32 Whole = FMath::FloorToInt(Fn);
			TArray<FVector> P;
			for (int32 i = 0; i <= Whole && i < C.Pts.Num(); ++i) P.Add(C.Pts[i]);
			if (Whole + 1 < C.Pts.Num()) P.Add(FMath::Lerp(C.Pts[Whole], C.Pts[Whole + 1], Fn - Whole));
			const float I = Cool * (0.75f + 0.25f * FMath::Exp(-After * 6.f)) * G;
			Ribbon(BFlat, P, C.Width * 0.55f * (1.f + 0.4f * FMath::Exp(-After * 8.f)), FLinearColor(I, I, I, 1.f), true, FVector::UpVector);
			// dark split under the amber seam so it reads on white ice
			for (FVector& Q : P) Q.Z -= 0.8f;
			Ribbon(BDark, P, C.Width * 1.3f, FLinearColor(0.03f, 0.027f, 0.024f, 0.85f * Cool * G), true, FVector::UpVector);
		}
		// white / stone flash
		if (After < 0.3f)
		{
			const float F = (1.f - After / 0.3f) * G;
			Billboard(BGlow, Contact + FVector(0, 0, RockR * 0.2f), RockR * (1.0f + After * 2.f), RockR * (1.0f + After * 2.f), FLinearColor(F * 0.6f, F * 0.5f, F * 0.4f, 1.f), 0.f, false);
		}
		// shockwave ring along the floor
		const float K = FMath::Clamp(After / 0.55f, 0.f, 1.f);
		if (K < 1.f)
		{
			const float Rad = 150.f + RockR * 1.9f * Ease(K);
			const float I = (1.f - K) * G;
			{
				TArray<FVector> Circle;
				for (int32 s = 0; s <= 40; ++s) { const float A = 2.f * PI * s / 40.f; Circle.Add(Contact + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 6.f)); }
				Ribbon(BDark, Circle, 90.f + 260.f * K, FLinearColor(0.20f, 0.18f, 0.16f, 0.55f * (1.f - K) * G), true, FVector::UpVector);
			}
			FlatQuad(BRing, Contact + FVector(0, 0, 12.f), FVector::UpVector, Rad / 0.86f, FLinearColor(I, I, I, 1.f));
			const float K2 = FMath::Clamp((After - 0.08f) / 0.7f, 0.f, 1.f);
			if (K2 > 0.f && K2 < 1.f) { const float I2 = 0.5f * (1.f - K2) * G; FlatQuad(BRing, Contact + FVector(0, 0, 10.f), FVector::UpVector, (100.f + RockR * 1.3f * Ease(K2)) / 0.86f, FLinearColor(I2, I2, I2, 1.f), 40.f); }
		}
	}
	else if (Rock)
	{
		// faint heat glow on the leading underside as it gets close (amber, low)
		const float H = Window(Desc, 0.45f, 0.95f) * 0.35f * G;
		if (H > 0.01f) Billboard(BGlow, RockPos - FVector(0, 0, RockR * 0.8f), RockR * 1.7f, RockR * 1.0f, FLinearColor(H, H * 0.9f, H * 0.8f, 1.f), 0.f, false);
	}
	// dust motes lifting off the floor around the target before the impact (pressure wave), blown out after
	if (!bShort || bImpacted)
	{
		for (int32 k = 0; k < 40; ++k)
		{
			FRandomStream R(k * 977 + 3);
			const float A = R.FRandRange(0.f, 2.f * PI) + Age * 0.15f;
			float Rad = R.FRandRange(120.f, 900.f);
			const float Ph = R.FRand();
			float Z = FMath::Fmod(Ph + Age * R.FRandRange(0.15f, 0.35f), 1.f);
			float I = Window(Desc, 0.15f, 0.5f) * FMath::Sin(Z * PI) * 0.8f;
			if (bImpacted) { Rad += 1800.f * After; I *= 1.f - Smooth(AfterU / 0.5f); }
			I *= G;
			if (I < 0.02f) continue;
			const FVector P = Contact + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rad + FVector(0, 0, 10.f + Z * 220.f);
			Billboard(BMote, P, 6.f, 6.f, FLinearColor(I, I, I, 1.f), 0.f, false);
		}
	}
	DoubleSide(BFlat); DoubleSide(BDark);
	Commit(0, BFlat); Commit(1, BGlow); Commit(2, BRing); Commit(3, BMote); Commit(4, BDark);

	// near silence just before the impact (spec 24: silence before the final hit)
	if (!bSilenced && !bImpacted && Rumble && Age >= GetImpactTime() - FMath::Min(0.35f, Seconds * 0.1f)) { bSilenced = true; Rumble->FadeOut(0.12f, 0.f); }

	return U < 1.f || (StopAt >= 0.f && G > 0.f);
}
