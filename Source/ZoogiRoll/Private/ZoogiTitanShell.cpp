// Titan Awaken shell (Titan card art): stone slabs slam onto the sphere, orange-hot seams, embers / pebbles / dust while
// rolling, crack burst on hits, shatter when Titan ends. See ZoogiTitanShell.h.
#include "ZoogiTitanShell.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FLinearColor HotSeam(1.0f, 0.36f, 0.06f);
	constexpr float FormTime = 0.8f;
	constexpr float SeamBase = 1.6f, SeamAmp = 0.4f; // seams pulse 1.2 .. 2.0

	template <typename T> T* LoadTitan(const TCHAR* Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (UObject* O = Cache.FindRef(Path).Get()) return Cast<T>(O);
		T* O = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn);
		if (O) Cache.Add(Path, O);
		else UE_LOG(LogZoogi, Warning, TEXT("TitanShell: missing %s"), Path);
		return O;
	}
	bool NoNiagara() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara")); return b; }
	float EaseIn3(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * X; }
}

bool AZoogiTitanShell::Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKFX"));
	return !bOff;
}

AZoogiTitanShell::AZoogiTitanShell()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the ball's visual hooks moved the model
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(RootComponent);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetLightColor(HotSeam);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(260.f);
	Light->SetCastShadows(false);
}

AZoogiTitanShell* AZoogiTitanShell::Activate(AZoogiBall* InBall)
{
	if (!Enabled() || !InBall || !InBall->Visual) return nullptr;
	UWorld* W = InBall->GetWorld();
	if (!W || W->GetNetMode() == NM_DedicatedServer) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiTitanShell* S = W->SpawnActor<AZoogiTitanShell>(AZoogiTitanShell::StaticClass(), FTransform(InBall->BallLocation()), P);
	if (!S) return nullptr;
	S->Ball = InBall;
	S->Rng.Initialize(FMath::Rand());
	S->bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTitanTest")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiTitanShots"));
	S->Build();
	S->Suppress(true);
	UE_LOG(LogZoogi, Log, TEXT("TitanShell: P%d %s awakens (%d slabs)"), InBall->PlayerIndex + 1, *InBall->Def.Name, S->Slabs.Num());
	return S;
}

FVector AZoogiTitanShell::Center() const
{
	const AZoogiBall* B = Ball.Get();
	return B ? B->BallLocation() : GetActorLocation();
}

float AZoogiTitanShell::Ground() const { return Center().Z - ZoogiConst::ZoogiRadius + 2.f; }

int32 AZoogiTitanShell::CapSerial = 0;

void AZoogiTitanShell::Capture(const FString& Name, float Delay)
{
	if (!bTest) return;
	const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!GM || GM->ShotDir.IsEmpty()) return;
	if (!Cap)
	{
		CapRT = NewObject<UTextureRenderTarget2D>(this);
		CapRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
		CapRT->InitAutoFormat(1280, 720);
		CapRT->UpdateResourceImmediate(true);
		Cap = NewObject<USceneCaptureComponent2D>(this);
		Cap->SetupAttachment(RootComponent);
		Cap->SetUsingAbsoluteLocation(true); Cap->SetUsingAbsoluteRotation(true);
		Cap->bCaptureEveryFrame = false; Cap->bCaptureOnMovement = false;
		Cap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Cap->FOVAngle = 40.f;
		Cap->PostProcessSettings.bOverride_AutoExposureSpeedUp = true; Cap->PostProcessSettings.AutoExposureSpeedUp = 100.f;
		Cap->PostProcessSettings.bOverride_AutoExposureSpeedDown = true; Cap->PostProcessSettings.AutoExposureSpeedDown = 100.f;
		Cap->PostProcessBlendWeight = 1.f;
		Cap->TextureTarget = CapRT;
		Cap->RegisterComponent();
	}
	CapQueue.Add(TPair<float, FString>(Age + Delay, Name));
}

void AZoogiTitanShell::TickCaptures()
{
	if (!Cap || CapQueue.Num() == 0) return;
	const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	for (int32 i = CapQueue.Num() - 1; i >= 0; --i)
	{
		if (Age < CapQueue[i].Key) continue;
		// framing: from the gameplay camera's side, close and low (card-like three-quarter view)
		const FVector C = State == EState::Shattering ? CapFrom : Center();
		FVector Dir(-1.f, 0.f, 0.f);
		if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)) { Dir = PCM->GetCameraLocation() - C; Dir.Z = 0.f; Dir = Dir.GetSafeNormal(); }
		const FVector Eye = C + Dir * 330.f + FVector(0, 0, 120.f);
		Cap->SetWorldLocationAndRotation(Eye, (C + FVector(0, 0, 5.f) - Eye).Rotation());
		Cap->CaptureScene(); Cap->CaptureScene();
		const FString File = FString::Printf(TEXT("titancap_%03d_%s.png"), CapSerial++, *CapQueue[i].Value);
		UKismetRenderingLibrary::ExportRenderTarget(this, CapRT, GM->ShotDir, File);
		UE_LOG(LogZoogi, Log, TEXT("TitanShell: captured %s"), *File);
		CapQueue.RemoveAt(i);
	}
}

void AZoogiTitanShell::Suppress(bool bOn)
{
	AZoogiBall* B = Ball.Get();
	if (!B || bOn == bSuppressed) return;
	bSuppressed = bOn;
	if (bOn)
	{
		TArray<UPrimitiveComponent*> Prims;
		B->GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* P : Prims)
		{
			if (!P || P == B->Sphere || P == B->Visual || P == B->Ring || !P->IsVisible()) continue;
			P->SetVisibility(false);
			Suppressed.Add(P);
		}
		// translucent / glass / visor slots of the model (the Eyes and Core slots drive the face and stay)
		HiddenMat = LoadTitan<UMaterialInterface>(TEXT("/Game/VFX/TitanFX/M_TitanHidden.M_TitanHidden"));
		const TArray<FName> Slots = B->Visual->GetMaterialSlotNames();
		for (int32 i = 0; i < B->Visual->GetNumMaterials(); ++i)
		{
			UMaterialInterface* M = B->Visual->GetMaterial(i);
			const FString Sn = Slots.IsValidIndex(i) ? Slots[i].ToString() : FString();
			const bool bFace = Sn == TEXT("Eyes") || Sn.EndsWith(TEXT("_Eyes")) || Sn == TEXT("Core") || Sn.EndsWith(TEXT("_Core"));
			const EBlendMode Bm = M ? M->GetBlendMode() : BLEND_Opaque;
			const bool bGlass = Bm == BLEND_Translucent || Bm == BLEND_Additive || Sn.Contains(TEXT("Visor")) || Sn.Contains(TEXT("Glass")) || Sn.Contains(TEXT("Dome")) || Sn.Contains(TEXT("Goggle")) || Sn.Contains(TEXT("Glow")) || Sn.EndsWith(TEXT("_Shell")); // the model's own outer shell / dome: the stone is the shell now
			UE_LOG(LogZoogi, Log, TEXT("TitanShell: P%d slot %d '%s' %s blend=%d%s"), B->PlayerIndex + 1, i, *Sn, M ? *M->GetName() : TEXT("-"), (int32)Bm, bGlass && !bFace && HiddenMat ? TEXT(" -> hidden") : TEXT(""));
			if (bGlass && !bFace && HiddenMat) { HiddenSlots.Add(TPair<int32, TWeakObjectPtr<UMaterialInterface>>(i, M)); B->Visual->SetMaterial(i, HiddenMat); }
		}
		if (B->TurnLight) TurnColor = B->TurnLight->GetLightColor();
	}
	else
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : Suppressed) if (P.IsValid()) P->SetVisibility(true);
		Suppressed.Reset();
		for (const auto& H : HiddenSlots) if (B->Visual && H.Value.IsValid()) B->Visual->SetMaterial(H.Key, H.Value.Get());
		HiddenSlots.Reset();
		if (B->TurnLight) B->TurnLight->SetLightColor(TurnColor);
	}
}

void AZoogiTitanShell::CaptureView(const FVector& Eye, const FVector& Look, float Fov, const FString& Name)
{
	const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!bTest || !GM || GM->ShotDir.IsEmpty()) return;
	Capture(TEXT("_warm"), 1e6f); // makes sure the capture component exists
	CapQueue.RemoveAll([](const TPair<float, FString>& Q) { return Q.Value == TEXT("_warm"); });
	const float OldFov = Cap->FOVAngle;
	Cap->FOVAngle = Fov;
	Cap->SetWorldLocationAndRotation(Eye, (Look - Eye).Rotation());
	Cap->CaptureScene(); Cap->CaptureScene();
	const FString File = FString::Printf(TEXT("titancap_%03d_%s.png"), CapSerial++, *Name);
	UKismetRenderingLibrary::ExportRenderTarget(this, CapRT, GM->ShotDir, File);
	UE_LOG(LogZoogi, Log, TEXT("TitanShell: captured %s"), *File);
	Cap->FOVAngle = OldFov;
}

void AZoogiTitanShell::Shot(const TCHAR* Name, float Delay)
{
	if (!bTest) return;
	if (AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
		if (!GM->ShotDir.IsEmpty()) GM->TakeShotDelayed(Name, Delay);
}

void AZoogiTitanShell::Build()
{
	AZoogiBall* B = Ball.Get();
	Pivot->AttachToComponent(B->Visual, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const FVector VS = B->Visual->GetComponentScale();
	Pivot->SetRelativeScale3D(FVector(1.f / FMath::Max(VS.X, 0.01f), 1.f / FMath::Max(VS.Y, 0.01f), 1.f / FMath::Max(VS.Z, 0.01f)));

	if (UMaterialInterface* M = LoadTitan<UMaterialInterface>(TEXT("/Game/VFX/TitanFX/MI_TitanStone.MI_TitanStone")))
		Stone = UMaterialInstanceDynamic::Create(M, this);
	if (Stone) { Stone->SetScalarParameterValue(TEXT("SeamGlow"), 2.5f); Stone->SetScalarParameterValue(TEXT("CrackGlow"), 0.f); }

	// Fibonacci spread with jitter; the face window (model +X) stays open like the card (angry face peeking out).
	const float R = ZoogiConst::ZoogiRadius;
	constexpr int32 N = 30;
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = 1.f - 2.f * (i + 0.5f) / N;
		const float Rad = FMath::Sqrt(FMath::Max(0.f, 1.f - Y * Y));
		const float Th = i * 2.39996f + 0.4f;
		FVector D(FMath::Cos(Th) * Rad, FMath::Sin(Th) * Rad, Y);
		D = (D + Rng.VRand() * 0.08f).GetSafeNormal();
		if (D.X > 0.78f && FMath::Abs(D.Z) < 0.45f) continue;                   // eyes / mouth window
		FSlab S;
		S.Dir = D;
		S.Size = Rng.FRandRange(0.72f, 1.22f);
		S.Home = D * R * 1.05f;
		const FVector Out = (D + FVector(0, 0, 0.6f)).GetSafeNormal();
		S.From = S.Home + Out * Rng.FRandRange(170.f, 260.f) + Rng.VRand() * 40.f;
		S.Spin = FRotator(Rng.FRandRange(-160.f, 160.f), Rng.FRandRange(-160.f, 160.f), Rng.FRandRange(-160.f, 160.f));
		S.Delay = Rng.FRandRange(0.f, 0.22f);
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(Pivot);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->SetCastShadow(false); // no slab-on-slab self shadow: the card reads evenly lit
		BuildSlabMesh(PM, D, 0.31f * S.Size, Rng.FRandRange(0.7f, 1.45f), Rng.FRandRange(0.f, 90.f), R, false);
		PM->SetRelativeLocation(S.From);
		PM->SetRelativeRotation(S.Spin);
		PM->RegisterComponent();
		S.Mesh = PM;
		SlabMeshes.Add(PM);
		Slabs.Add(S);
	}
	// ground thump at activation
	const FVector G(Center().X, Center().Y, Ground());
	AZoogiFX::Spawn(GetWorld(), G, FLinearColor(0.55f, 0.42f, 0.3f), 170.f, 0.5f, true, 4.f);
	Dust(G, 0.8f);
	if (USoundBase* Snd = LoadTitan<USoundBase>(TEXT("/Game/Audio/S_RockCrack.S_RockCrack"))) UGameplayStatics::PlaySoundAtLocation(this, Snd, G, 0.7f, 0.8f);
	Shot(TEXT("titan_1_activation"), 0.55f);
	Capture(TEXT("act_a_fly"), 0.25f); Capture(TEXT("act_b_slam"), 0.52f); Capture(TEXT("act_c_flare"), 0.7f); Capture(TEXT("act_d_locked"), 1.4f);
	Shot(TEXT("titan_1b_aimface"), 1.6f);
}

void AZoogiTitanShell::BuildSlabMesh(UProceduralMeshComponent* M, const FVector& Dir, float Half, float Aspect, float Twist, float Radius, bool bChip)
{
	// Card-style stone brick: chamfered-rectangle footprint on the sphere, FLAT (slightly tilted) top, bevelled top edge,
	// straight side walls down to the ball. Flat-shaded. Vertex colour R = seam heat (side walls / underside),
	// G = top face (crack overlay + ambient). Geometry is local to the slab centre C.
	FVector T1, T2;
	Dir.FindBestAxisVectors(T1, T2);
	const FQuat Tw(Dir, FMath::DegreesToRadians(Twist));
	T1 = Tw.RotateVector(T1); T2 = Tw.RotateVector(T2);
	const float Ro = Radius * (bChip ? 1.f : Rng.FRandRange(1.17f, 1.30f));
	const float Ri = Radius * (bChip ? 0.55f : 0.97f);
	const FVector C = bChip ? FVector::ZeroVector : Dir * Radius * 1.05f;
	const float Ta = FMath::Tan(Half);
	const FVector Nt = (Dir + Rng.VRand() * (bChip ? 0.35f : 0.13f)).GetSafeNormal(); // tilted top plane
	const float Bev = Radius * (bChip ? 0.12f : 0.045f);                                   // bevel height
	// chamfered rectangle footprint (8 corners), half extents in tangent units
	const float Ha = Ta * Aspect, Hb = Ta / Aspect;
	const float Ch = 0.22f + Rng.FRandRange(0.f, 0.15f);
	TArray<FVector2D> Foot;
	const FVector2D Corners[4] = { { Ha, Hb }, { -Ha, Hb }, { -Ha, -Hb }, { Ha, -Hb } };
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D P = Corners[k], Prev = Corners[(k + 3) % 4], Next = Corners[(k + 1) % 4];
		const float Ck = Ch * Rng.FRandRange(0.6f, 1.3f);
		Foot.Add(P + (Prev - P) * Ck * 0.5f + FVector2D(Rng.FRandRange(-0.02f, 0.02f), Rng.FRandRange(-0.02f, 0.02f)));
		Foot.Add(P + (Next - P) * Ck * 0.5f + FVector2D(Rng.FRandRange(-0.02f, 0.02f), Rng.FRandRange(-0.02f, 0.02f)));
	}
	const int32 K = Foot.Num();
	const FVector TopC = Dir * (Ro / FMath::Max(FVector::DotProduct(Dir, Nt), 0.5f));
	TArray<FVector> Top, Edge, Bot;
	for (const FVector2D& F : Foot)
	{
		const FVector D = (Dir + F.X * T1 + F.Y * T2).GetSafeNormal();
		const float Dn = FMath::Max(FVector::DotProduct(D, Nt), 0.45f);
		const FVector OnTop = D * (Ro / Dn);
		Edge.Add(D * ((Ro - Bev) / Dn) - C);
		Top.Add(TopC + (OnTop - TopC) * 0.86f - C);   // bevel: top face inset
		Bot.Add(D * Ri - C);
	}
	TArray<FVector> V; TArray<int32> Tri; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<FProcMeshTangent> Tan;
	auto Face = [&](const FVector& A, const FVector& B, const FVector& Cc, float Ra, float Rb, float Rc, float Gf)
	{
		const FVector Nn = FVector::CrossProduct(Cc - A, B - A).GetSafeNormal();
		const int32 I = V.Num();
		V.Add(A); V.Add(B); V.Add(Cc);
		N.Add(Nn); N.Add(Nn); N.Add(Nn);
		// planar UV from the slab tangent frame (stone texture keeps a consistent scale)
		auto U = [&](const FVector& P) { return FVector2D(FVector::DotProduct(P, T1), FVector::DotProduct(P, T2)) / (Radius * 0.9f) + FVector2D(0.5f, 0.5f); };
		UV.Add(U(A)); UV.Add(U(B)); UV.Add(U(Cc));
		Col.Add(FLinearColor(Ra, Gf, 0, 1)); Col.Add(FLinearColor(Rb, Gf, 0, 1)); Col.Add(FLinearColor(Rc, Gf, 0, 1));
		Tri.Add(I); Tri.Add(I + 1); Tri.Add(I + 2);
	};
	const FVector TopMid = TopC - C, BotMid = Dir * Ri - C;
	for (int32 k = 0; k < K; ++k)
	{
		const int32 j = (k + 1) % K;
		Face(TopMid, Top[k], Top[j], 0, 0, 0, 1.f);                       // flat top
		Face(Top[k], Edge[k], Edge[j], 0, 0, 0, 0.6f);                     // bevel
		Face(Top[k], Edge[j], Top[j], 0, 0, 0, 0.6f);
		Face(Edge[k], Bot[k], Bot[j], 0.f, 1.f, 1.f, 0.f);                // side wall: stone outside, seam glow only at the inner rim
		Face(Edge[k], Bot[j], Edge[j], 0.f, 1.f, 0.f, 0.f);
		Face(BotMid, Bot[j], Bot[k], 1, 1, 1, 0.f);                         // underside
	}
	// outward winding: flip any face whose normal points toward the slab's interior
	const FVector Mid = Dir * (Ro + Ri) * 0.5f - C;
	for (int32 t = 0; t < Tri.Num(); t += 3)
	{
		const FVector Cen = (V[Tri[t]] + V[Tri[t + 1]] + V[Tri[t + 2]]) / 3.f;
		if (FVector::DotProduct(N[Tri[t]], Cen - Mid) < 0.f)
		{
			Swap(Tri[t + 1], Tri[t + 2]);
			for (int32 jj = 0; jj < 3; ++jj) N[Tri[t] + jj] = -N[Tri[t] + jj];
		}
	}
	M->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, Tan, false);
	if (Stone) M->SetMaterial(0, Stone);
}

void AZoogiTitanShell::Dust(const FVector& P, float Scale)
{
	if (NoNiagara()) return;
	if (UNiagaraSystem* NS = LoadTitan<UNiagaraSystem>(TEXT("/Game/VFX/TitanFX/NS_TitanDust.NS_TitanDust")))
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NS, P, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::None);
}

void AZoogiTitanShell::Embers(const FVector& P, float Scale)
{
	if (NoNiagara()) return;
	if (UNiagaraSystem* NS = LoadTitan<UNiagaraSystem>(TEXT("/Game/VFX/TitanFX/NS_TitanEmbers.NS_TitanEmbers")))
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NS, P, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::None);
}

void AZoogiTitanShell::SpawnChip(const FVector& P, const FVector& Vel, float Size)
{
	if (Chips.Num() > 40) return;
	UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
	PM->SetupAttachment(RootComponent);
	PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PM->SetCastShadow(false);
	PM->SetUsingAbsoluteLocation(true); PM->SetUsingAbsoluteRotation(true);
	BuildSlabMesh(PM, Rng.VRand(), 0.5f, Rng.FRandRange(0.7f, 1.3f), Rng.FRandRange(0.f, 90.f), 6.f * Size, true);
	PM->SetWorldLocation(P);
	PM->RegisterComponent();
	FChip C; C.Mesh = PM; C.Vel = Vel; C.AngVel = Rng.VRand() * 540.f; C.Life = Rng.FRandRange(0.9f, 1.5f); C.Size = 1.f;
	Chips.Add(C);
	ChipMeshes.Add(PM);
}

void AZoogiTitanShell::TickChips(float Dt)
{
	const float Gz = Ground() - 2.f;
	for (int32 i = Chips.Num() - 1; i >= 0; --i)
	{
		FChip& C = Chips[i];
		C.Age += Dt;
		if (!C.Mesh || C.Age >= C.Life) { if (C.Mesh) C.Mesh->DestroyComponent(); ChipMeshes.Remove(C.Mesh); Chips.RemoveAtSwap(i); continue; }
		C.Vel.Z -= 980.f * Dt;
		FVector P = C.Mesh->GetComponentLocation() + C.Vel * Dt;
		if (P.Z < Gz + 3.f && C.Vel.Z < 0.f) { P.Z = Gz + 3.f; C.Vel.Z *= -0.3f; C.Vel.X *= 0.6f; C.Vel.Y *= 0.6f; C.AngVel *= 0.5f; }
		C.Mesh->SetWorldLocation(P);
		C.Mesh->AddWorldRotation(FRotator(C.AngVel.X * Dt, C.AngVel.Y * Dt, C.AngVel.Z * Dt));
		const float K = 1.f - FMath::Clamp((C.Age - (C.Life - 0.35f)) / 0.35f, 0.f, 1.f);
		C.Mesh->SetWorldScale3D(FVector(FMath::Max(K, 0.01f)));
	}
}

void AZoogiTitanShell::Impact(float Strength)
{
	if (State == EState::Shattering || Age - LastImpact < 0.25f) return;
	LastImpact = Age;
	Strength = FMath::Clamp(Strength, 0.3f, 1.5f);
	Impulse = 3.f * Strength;
	Crack = 3.f * Strength;
	const AZoogiBall* B = Ball.Get();
	const FVector V = B ? B->GetBallVelocity() : FVector::ZeroVector;
	const FVector Fwd = V.Size2D() > 10.f ? FVector(V.X, V.Y, 0.f).GetSafeNormal() : FVector::ForwardVector;
	const FVector P = Center() + Fwd * ZoogiConst::ZoogiRadius * 0.9f;
	AZoogiFX::KFX(GetWorld(), TEXT("Effect17_Collision"), FVector(P.X, P.Y, Ground()), 0.35f * Strength, 2.f);
	Dust(FVector(P.X, P.Y, Ground()), 0.6f);
	for (int32 i = 0; i < 8; ++i)
		SpawnChip(P + Rng.VRand() * 12.f, (-Fwd * 0.3f + Rng.VRand() + FVector(0, 0, 0.9f)).GetSafeNormal() * Rng.FRandRange(250.f, 520.f), Rng.FRandRange(0.7f, 1.4f));
	if (USoundBase* Snd = LoadTitan<USoundBase>(TEXT("/Game/Audio/S_HitCrack.S_HitCrack"))) UGameplayStatics::PlaySoundAtLocation(this, Snd, P, 0.6f, 0.85f);
	Shot(TEXT("titan_2b_impact"), 0.08f);
	Capture(TEXT("impact"), 0.06f);
}

void AZoogiTitanShell::Shatter()
{
	if (State == EState::Shattering) return;
	State = EState::Shattering;
	StateAge = 0.f;
	Suppress(false);
	Crack = 4.f;
	Impulse = 3.f;
	const FVector C = Center();
	for (FSlab& S : Slabs)
	{
		if (!S.Mesh) continue;
		const FVector WP = S.Mesh->GetComponentLocation();
		S.Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		S.Mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		S.Mesh->SetUsingAbsoluteLocation(true); S.Mesh->SetUsingAbsoluteRotation(true); S.Mesh->SetUsingAbsoluteScale(true);
		S.Mesh->SetWorldLocation(WP);
		const FVector Out = (WP - C).GetSafeNormal();
		S.Vel = Out * Rng.FRandRange(160.f, 340.f) + FVector(0, 0, Rng.FRandRange(120.f, 280.f));
		S.AngVel = Rng.VRand() * Rng.FRandRange(120.f, 360.f);
		S.Delay = Rng.FRandRange(0.f, 0.12f);
	}
	Pivot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	const FVector G(C.X, C.Y, Ground());
	AZoogiFX::KFX(GetWorld(), TEXT("Effect17_Collision"), G, 0.4f, 2.f);
	Dust(G, 0.9f);
	for (int32 i = 0; i < 10; ++i) SpawnChip(C + Rng.VRand() * 40.f, (Rng.VRand() + FVector(0, 0, 1.f)).GetSafeNormal() * Rng.FRandRange(200.f, 450.f), Rng.FRandRange(0.8f, 1.5f));
	if (USoundBase* Snd = LoadTitan<USoundBase>(TEXT("/Game/Audio/S_RockShatter.S_RockShatter"))) UGameplayStatics::PlaySoundAtLocation(this, Snd, C, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("TitanShell: shatter"));
	Shot(TEXT("titan_3_shatter"), 0.3f);
	CapFrom = C;
	Capture(TEXT("end_a_crack"), 0.08f); Capture(TEXT("end_b_fall"), 0.45f); Capture(TEXT("end_c_fade"), 1.2f);
}

void AZoogiTitanShell::EndPlay(const EEndPlayReason::Type Reason)
{
	Suppress(false);
	Super::EndPlay(Reason);
}

void AZoogiTitanShell::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	Age += Dt; StateAge += Dt;
	AZoogiBall* B = Ball.Get();
	if (!B && State != EState::Shattering) { Shatter(); }
	if (B) SetActorLocation(B->BallLocation());
	if (B && B->Visual && State != EState::Shattering)
	{
		const FVector VS = B->Visual->GetComponentScale();
		Pivot->SetRelativeScale3D(FVector(1.f / FMath::Max(VS.X, 0.01f), 1.f / FMath::Max(VS.Y, 0.01f), 1.f / FMath::Max(VS.Z, 0.01f)));
	}
	Impulse = FMath::Max(0.f, Impulse - Dt * 6.f);
	Crack = FMath::Max(0.f, Crack - Dt * 4.f);
	float Seam = SeamBase + SeamAmp * FMath::Sin(Age * UE_TWO_PI * 0.9f) + Impulse;
	const FVector C = Center();

	if (State == EState::Forming)
	{
		bool bAll = true;
		for (FSlab& S : Slabs)
		{
			const float T = (StateAge - S.Delay) / 0.45f;
			const float E = EaseIn3(T);
			if (T < 1.f) bAll = false;
			S.Mesh->SetRelativeLocation(FMath::Lerp(S.From, S.Home, E));
			S.Mesh->SetRelativeRotation(FQuat::Slerp(S.Spin.Quaternion(), FQuat::Identity, FMath::Clamp(T, 0.f, 1.f)));
			// slam overshoot: dips into the ball and settles
			if (T >= 1.f && T < 1.4f) S.Mesh->SetRelativeLocation(S.Home - S.Dir * 3.f * FMath::Sin((T - 1.f) / 0.4f * PI));
		}
		if (!bSlammed && StateAge > 0.5f)
		{
			bSlammed = true;
			const FVector G(C.X, C.Y, Ground());
			AZoogiFX::KFX(GetWorld(), TEXT("Effect17_Collision"), G, 0.3f, 1.6f);
			AZoogiFX::Spawn(GetWorld(), G, HotSeam, 150.f, 0.45f, true, 6.f);
			Dust(G, 1.f);
			B->Squash(0.12f, FVector::UpVector);
			for (int32 i = 0; i < 6; ++i) SpawnChip(C + Rng.VRand() * 45.f, (Rng.VRand() + FVector(0, 0, 0.8f)).GetSafeNormal() * Rng.FRandRange(150.f, 320.f), Rng.FRandRange(0.6f, 1.1f));
			if (USoundBase* Snd = LoadTitan<USoundBase>(TEXT("/Game/Audio/S_RockShatter.S_RockShatter"))) UGameplayStatics::PlaySoundAtLocation(this, Snd, C, 0.5f, 0.7f);
		}
		// seams flare white-hot as the plates lock, then cool to the pulse
		Seam = StateAge < 0.55f ? 2.2f : FMath::Lerp(6.f, Seam, FMath::Clamp((StateAge - 0.55f) / 0.5f, 0.f, 1.f));
		if (bAll && StateAge >= FormTime + 0.3f) { State = EState::Active; StateAge = 0.f; }
	}
	else if (State == EState::Active && B)
	{
		const float Spd = B->PlanarSpeed();
		NextEmber -= Dt; NextPebble -= Dt * (Spd > 150.f ? 2.5f : 1.f); NextDust -= Dt;
		if (NextEmber <= 0.f) { NextEmber = Spd > 120.f ? Rng.FRandRange(0.06f, 0.12f) : Rng.FRandRange(0.15f, 0.32f); Embers(C + Rng.VRand() * ZoogiConst::ZoogiRadius * 1.1f, 0.55f); }
		if (NextPebble <= 0.f)
		{
			NextPebble = Rng.FRandRange(0.8f, 1.6f);
			SpawnChip(C + Rng.VRand() * ZoogiConst::ZoogiRadius * 1.05f, Rng.VRand() * 60.f + FVector(0, 0, 40.f), Rng.FRandRange(0.5f, 0.9f));
		}
		if (Spd > 90.f && NextDust <= 0.f)
		{
			NextDust = 0.06f;
			const FVector Back = -FVector(B->GetBallVelocity().X, B->GetBallVelocity().Y, 0.f).GetSafeNormal() * ZoogiConst::ZoogiRadius * 0.6f;
			Dust(FVector(C.X, C.Y, Ground()) + Back, 0.55f);
		}
		if (!bShotRolling && Spd > 250.f) { bShotRolling = true; Shot(TEXT("titan_2_rolling"), 0.25f); }
		if (Spd > 200.f && RollCaps < 3 && CapQueue.Num() == 0) { ++RollCaps; Capture(FString::Printf(TEXT("roll_%d"), RollCaps), 0.12f * RollCaps); }
		if (const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
		{
			if (GM->Phase == EZoogiPhase::Aiming && Spd < 20.f && AimCaps < 4 && CapQueue.Num() == 0 && StateAge > 0.6f * (AimCaps + 1)) { ++AimCaps; Capture(FString::Printf(TEXT("aimface_%d"), AimCaps), 0.f); }
			// front shot: camera straight in front of the face (model +X), face visible through the open window
			if (GM->Phase == EZoogiPhase::Aiming && Spd < 20.f && FrontCaps < 2 && StateAge > 0.9f + 0.9f * FrontCaps && B->Visual)
			{
				++FrontCaps;
				FVector F = B->Visual->GetForwardVector(); F.Z = 0.f; F = F.GetSafeNormal();
				CaptureView(C + F * 260.f + FVector(0, 0, 45.f), C + FVector(0, 0, 8.f), 40.f, FString::Printf(TEXT("front_P%d_%d"), B->PlayerIndex + 1, FrontCaps));
			}
			// facing check from the gameplay camera: every other Zoogi, close crop (only the local one should look at the camera)
			if (GM->Phase == EZoogiPhase::Aiming && FacingCaps < 1 && StateAge > 1.5f)
				if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
				{
					++FacingCaps;
					for (AZoogiBall* O : GM->Balls)
					{
						if (!O || O == B || O->bOut) continue;
						// along the gameplay camera's line of sight to that Zoogi, but close (no props in between)
						const FVector Oc = O->BallLocation();
						const FVector Eye = Oc + (PCM->GetCameraLocation() - Oc).GetSafeNormal() * 280.f;
						CaptureView(Eye, Oc, 40.f,
							FString::Printf(TEXT("facing_P%d_%s_seenby_cam"), O->PlayerIndex + 1, O->PlayerIndex == 0 ? TEXT("local") : TEXT("nonlocal")));
					}
				}
		}
	}
	else if (State == EState::Shattering)
	{
		const float Gz = Ground();
		for (FSlab& S : Slabs)
		{
			if (!S.Mesh || StateAge < S.Delay) continue;
			S.Vel.Z -= 980.f * Dt;
			FVector P = S.Mesh->GetComponentLocation() + S.Vel * Dt;
			if (P.Z < Gz + 6.f && S.Vel.Z < 0.f) { P.Z = Gz + 6.f; S.Vel.Z *= -0.25f; S.Vel.X *= 0.5f; S.Vel.Y *= 0.5f; S.AngVel *= 0.4f; }
			S.Mesh->SetWorldLocation(P);
			S.Mesh->AddWorldRotation(FRotator(S.AngVel.X * Dt, S.AngVel.Y * Dt, S.AngVel.Z * Dt));
			const float K = 1.f - FMath::Clamp((StateAge - 0.9f) / 0.7f, 0.f, 1.f);
			S.Mesh->SetWorldScale3D(FVector(FMath::Max(K, 0.01f)));
		}
		Seam = FMath::Max(0.f, 1.f - StateAge / 1.2f) * (2.f + Impulse);
		if (StateAge > 1.8f && Chips.Num() == 0 && CapQueue.Num() == 0) { Destroy(); return; }
		if (StateAge > 3.f) { Destroy(); return; }
	}

	if (bSuppressed && State != EState::Shattering && B)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& P : Suppressed) if (P.IsValid() && P->IsVisible()) P->SetVisibility(false);
		TArray<UPrimitiveComponent*> Prims;
		B->GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* P : Prims)
			if (P && P != B->Sphere && P != B->Visual && P != B->Ring && P->IsVisible()) { P->SetVisibility(false); Suppressed.AddUnique(P); }
		if (B->TurnLight) { B->TurnLight->SetLightColor(HotSeam); B->TurnLight->SetIntensity(FMath::Min(B->TurnLight->Intensity, 25.f)); }
	}
	TickChips(Dt);
	TickCaptures();
	if (Stone)
	{
		Stone->SetScalarParameterValue(TEXT("SeamGlow"), Seam);
		Stone->SetScalarParameterValue(TEXT("CrackGlow"), Crack);
	}
	Light->SetWorldLocation(C + FVector(0, 0, 20.f));
	Light->SetIntensity(State == EState::Shattering ? FMath::Max(0.f, 1.f - StateAge) * 2500.f : 900.f * Seam);
}
