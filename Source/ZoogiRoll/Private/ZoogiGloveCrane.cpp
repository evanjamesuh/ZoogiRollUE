// See ZoogiGloveCrane.h.
#include "ZoogiGloveCrane.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiLaunchFlow.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

int32 AZoogiGloveCrane::GCount = 0;

namespace
{
	float CraneEase(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	FVector Bez(const FVector& A, const FVector& B, const FVector& C, float T) { const float U = 1.f - T; return U * U * A + 2.f * U * T * B + T * T * C; }
	UStaticMesh* Shape(const TCHAR* N) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), N, N)); }
	// timeline (s)
	constexpr float TGrab = 0.30f;  // drop from in-frame (above/behind the Zoogi) onto the Zoogi, fingers close
	constexpr float TIn = 0.62f;    // swoop carrying it to above the pad
	constexpr float TDown = 0.95f;  // lowered onto the pad
	constexpr float TOpen = 1.10f;  // fingers open = release (Placed fires)
	constexpr float TEnd = 1.50f;   // glove gone back up
}

AZoogiGloveCrane::AZoogiGloveCrane()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool AZoogiGloveCrane::IsEnabled()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiNoGloveCrane"))) return false;
	if (FParse::Param(Cmd, TEXT("ZoogiGloveCrane")) || FParse::Param(Cmd, TEXT("ZoogiGloveTest"))) return true;
	bool b = true;
	if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.GloveCrane"), TEXT("bEnabled"), b, GGameIni);
	FString V; // auto / test / parity runs: instant place
	if (FParse::Param(Cmd, TEXT("ZoogiAuto")) || FParse::Value(Cmd, TEXT("ZoogiBattleTest="), V) || FParse::Param(Cmd, TEXT("ZoogiNoForm")) || GIsAutomationTesting) return false;
	return b;
}

void AZoogiGloveCrane::Bind(AZoogiGameMode* GM)
{
	if (!GM || !IsEnabled() || GM->GetNetMode() == NM_DedicatedServer) return;
	GM->OnZoogiNeedsCarryToPad.AddStatic(&AZoogiGloveCrane::HandleRequest, TWeakObjectPtr<AZoogiGameMode>(GM));
	UE_LOG(LogZoogi, Log, TEXT("GloveCrane: bound to OnZoogiNeedsCarryToPad"));
}

void AZoogiGloveCrane::HandleRequest(AZoogiBall* B, const FTransform& PadT, TWeakObjectPtr<AZoogiGameMode> GMW)
{
	AZoogiGameMode* GM = GMW.Get();
	UWorld* W = B ? B->GetWorld() : nullptr;
	if (!GM || !W) return; // the game's 6 s fallback places it
	if (const AZoogiArena* A = GM->Arena) if (const AZoogiLaunchPad* LP = A->GetLaunchPad())
		if (AActor* Occ = LP->GetOccupant()) if (Occ != B) UE_LOG(LogZoogi, Log, TEXT("GloveCrane: pad occupied by %s (placing anyway, game rules decide)"), *Occ->GetName());
	for (TActorIterator<AZoogiGloveCrane> It(W); It; ++It) if (It->Ball == B) It->Destroy(); // one crane per Zoogi
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AZoogiGloveCrane* C = W->SpawnActor<AZoogiGloveCrane>(AZoogiGloveCrane::StaticClass(), FTransform(PadT.GetLocation()), SP))
	{ C->GameMode = GM; C->Init(B, PadT.GetLocation()); }
	else GM->NotifyZoogiPlacedOnPad(B);
}

bool AZoogiGloveCrane::StillPending() const
{
	const AZoogiGameMode* GM = GameMode.Get(); const AZoogiBall* B = Ball.Get();
	return GM && B && GM->LaunchFlow.IsCarryPending(B->PlayerIndex);
}

UStaticMeshComponent* AZoogiGloveCrane::Part(UStaticMesh* M, USceneComponent* Parent, const FVector& Loc, const FVector& Scale, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent);
	C->SetStaticMesh(M);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(false); // cheap (mobile / RX 6600): no dynamic shadow casters
	C->RegisterComponent();
	return C;
}

void AZoogiGloveCrane::Init(AZoogiBall* InBall, const FVector& InPad)
{
	Ball = InBall; Pad = InPad;
	++GCount;
	UWorld* W = GetWorld();
	// grab spot: where the Zoogi went out, pulled back to just outside the knock-off edge at court height (it may have
	// fallen far below / outside the view by the time the turn resolves)
	const AZoogiGameMode* GM = GameMode.Get();
	const FVector Ctr = (GM && GM->Arena) ? GM->Arena->GetActorLocation() : FVector::ZeroVector;
	const float KR = (GM && GM->Arena) ? GM->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	FVector O = InBall ? InBall->OutLocation : InPad;
	if (O.IsNearlyZero()) O = InBall ? InBall->GetActorLocation() : InPad;
	FVector2D D(O.X - Ctr.X, O.Y - Ctr.Y);
	if (D.Size() > KR + 150.f) D = D.GetSafeNormal() * (KR + 150.f);
	Grab = FVector(Ctr.X + D.X, Ctr.Y + D.Y, Ctr.Z + ZoogiConst::ZoogiRadius);
	// glove start: above and BEHIND the Zoogi as seen from the gameplay camera, kept inside the frame
	FVector CamL = Grab + FVector(0, 1500, 1500), CamF = (Grab - CamL).GetSafeNormal();
	APlayerController* PC = W ? UGameplayStatics::GetPlayerController(W, 0) : nullptr;
	if (PC && PC->PlayerCameraManager) { CamL = PC->PlayerCameraManager->GetCameraLocation(); CamF = PC->PlayerCameraManager->GetCameraRotation().Vector(); }
	const FVector Away = (Grab - CamL).GetSafeNormal2D(UE_SMALL_NUMBER, CamF.GetSafeNormal2D());
	Up = FVector(InPad.X, InPad.Y, InPad.Z + 380.f);
	From = Grab + Away * 450.f + FVector(0, 0, 540.f);
	// the pad and the knock-off spot are often below / behind the gameplay camera: keep the whole grab + carry in frame by
	// moving the (presentation-only) grab spot, start and carry waypoint onto screen; the last leg heads off-frame to the pad
	int32 SX = 0, SY = 0; if (PC) PC->GetViewportSize(SX, SY);
	auto OnScr = [&](const FVector& L, float M) { FVector2D S; return PC && SX > 0 && PC->ProjectWorldLocationToScreen(L, S) && S.X > SX * M && S.X < SX * (1.f - M) && S.Y > SY * M && S.Y < SY * (1.f - M); };
	auto AtScr = [&](float FX, float FY, float Z, FVector& Out) { FVector O, Dr; if (!PC || SX <= 0 || !PC->DeprojectScreenPositionToWorld(SX * FX, SY * FY, O, Dr) || Dr.Z > -0.05f) return false; Out = FMath::RayPlaneIntersection(O, Dr, FPlane(FVector(0, 0, Z), FVector::UpVector)); return true; };
	if (!OnScr(Grab, 0.12f)) AtScr(0.40f, 0.62f, Grab.Z, Grab);
	if (!OnScr(From, 0.06f)) { FVector F; if (AtScr(0.40f, 0.10f, Grab.Z + 520.f, F)) From = F; }
	if (!OnScr(Up, 0.08f)) { FVector U; if (AtScr(0.60f, 0.60f, FMath::Max(Grab.Z, InPad.Z) + 300.f, U)) Up = U; }
	bPadOnScr = OnScr(InPad, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("GloveCrane: framing grab %d start %d carry %d pad %d"), OnScr(Grab, 0.f) ? 1 : 0, OnScr(From, 0.f) ? 1 : 0, OnScr(Up, 0.f) ? 1 : 0, OnScr(InPad, 0.f) ? 1 : 0);

	// size the glove to the Zoogi (Tripo models differ): visual bounds radius vs the 50 cm reference
	Size = 1.f;
	if (InBall && InBall->Visual) Size = FMath::Clamp(InBall->Visual->Bounds.SphereRadius / 55.f, 0.8f, 1.6f);
	Size *= 1.7f; // read at gameplay-camera distance (the glove is presentation only)

	UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Mat = [&](const FLinearColor& Col) { UMaterialInstanceDynamic* D = BM ? UMaterialInstanceDynamic::Create(BM, this) : nullptr; if (D) D->SetVectorParameterValue(TEXT("Color"), Col); return D; };
	UMaterialInstanceDynamic* White = Mat(FLinearColor(0.85f, 0.85f, 0.82f));
	UMaterialInstanceDynamic* Steel = Mat(FLinearColor(0.18f, 0.19f, 0.21f));
	UMaterialInstanceDynamic* Gold = Mat(FLinearColor(0.85f, 0.55f, 0.12f));
	UStaticMesh* Sph = Shape(TEXT("Sphere")); UStaticMesh* Cyl = Shape(TEXT("Cylinder")); UStaticMesh* Cube = Shape(TEXT("Cube"));

	Glove = NewObject<USceneComponent>(this); Glove->SetupAttachment(RootComponent); Glove->RegisterComponent();
	Glove->SetRelativeScale3D(FVector(Size));
	// palm: a squashed sphere over the Zoogi; cuff: a short ringed cylinder above it (cartoon glove look, no outlines)
	Part(Sph, Glove, FVector(0, 0, 0), FVector(1.15f, 0.95f, 0.55f))->SetMaterial(0, White);
	Part(Cyl, Glove, FVector(0, 0, 38.f), FVector(0.78f, 0.78f, 0.32f))->SetMaterial(0, White);
	Part(Cyl, Glove, FVector(0, 0, 54.f), FVector(0.84f, 0.84f, 0.08f))->SetMaterial(0, White);
	// 4 fingers + thumb hang down around the Zoogi and curl in to grip
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bThumb = i == 4;
		const float Yaw = bThumb ? 180.f : -54.f + 36.f * i;
		USceneComponent* Pv = NewObject<USceneComponent>(this); Pv->SetupAttachment(Glove); Pv->RegisterComponent();
		Pv->SetRelativeLocationAndRotation(FRotator(0, Yaw, 0).RotateVector(FVector(44.f, 0, -8.f)), FRotator(0, Yaw, 0));
		Part(Sph, Pv, FVector(4.f, 0, -26.f), FVector(0.30f, 0.27f, bThumb ? 0.50f : 0.62f))->SetMaterial(0, White); // finger
		Part(Sph, Pv, FVector(-2.f, 0, -52.f), FVector(0.24f, 0.22f, 0.26f))->SetMaterial(0, White);               // fingertip
		Fingers.Add(Pv);
	}
	Hold = NewObject<USceneComponent>(this); Hold->SetupAttachment(Glove); Hold->RegisterComponent();
	Hold->SetRelativeLocation(FVector(0, 0, -(ZoogiConst::ZoogiRadius + 12.f)));
	// crane: a trolley and a steel cable rising out of frame (a floating stage crane overhead)
	Trolley = Part(Cube, RootComponent, FVector::ZeroVector, FVector(0.9f, 0.5f, 0.35f)); Trolley->SetMaterial(0, Steel);
	Trolley->SetUsingAbsoluteLocation(true); Trolley->SetUsingAbsoluteRotation(true);
	Part(Cyl, Trolley, FVector(0, 0, -60.f), FVector(0.5f, 0.5f, 0.25f))->SetMaterial(0, Gold); // hook hub
	Cable = Part(Cyl, RootComponent, FVector::ZeroVector, FVector(0.06f, 0.06f, 1.f)); Cable->SetMaterial(0, Steel);
	Cable->SetUsingAbsoluteLocation(true); Cable->SetUsingAbsoluteRotation(true); Cable->SetUsingAbsoluteScale(true);

	// the carried Zoogi: hide the real one (already placed by the game), hang a visual copy in the glove
	if (InBall)
	{
		const FTransform Inv = InBall->GetActorTransform().Inverse();
		TArray<UStaticMeshComponent*> Src; InBall->GetComponents<UStaticMeshComponent>(Src);
		for (UStaticMeshComponent* S : Src)
		{
			if (!S || !S->GetStaticMesh() || !S->IsVisible() || S == InBall->Ring) continue;
			UStaticMeshComponent* PD = NewObject<UStaticMeshComponent>(this);
			PD->SetStaticMesh(S->GetStaticMesh());
			for (int32 m = 0; m < S->GetNumMaterials(); ++m) PD->SetMaterial(m, S->GetMaterial(m));
			PD->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PD->SetCastShadow(false);
			PD->SetUsingAbsoluteLocation(true); PD->SetUsingAbsoluteRotation(true); PD->SetUsingAbsoluteScale(true);
			PD->SetupAttachment(RootComponent);
			PD->RegisterComponent();
			Proxies.Add({ S, PD, S->GetComponentTransform() * Inv }); ProxyComps.Add(PD);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("GloveCrane: carrying %s to pad %s (size %.2f)"), InBall ? *InBall->Def.Name : TEXT("?"), *Pad.ToString(), Size);
	Tick(0.f);
}

void AZoogiGloveCrane::Release()
{
	if (bReleased) return;
	bReleased = true;
	AZoogiBall* B = Ball.Get();
	const bool bPending = StillPending();
	for (UStaticMeshComponent* D : ProxyComps) if (D) D->DestroyComponent();
	ProxyComps.Reset(); Proxies.Reset();
	const FVector G = Pad - FVector(0, 0, ZoogiConst::ZoogiRadius - 2.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), G, FRotator::ZeroRotator, 0.6f);
	AZoogiFX::Spawn(GetWorld(), G + FVector(0, 0, 1.f), FLinearColor(1.f, 0.95f, 0.85f), 120.f, 0.35f, true, 4.f);
	UE_LOG(LogZoogi, Log, TEXT("GloveCrane: placed %s at %.2f s (notify=%d)"), B ? *B->Def.Name : TEXT("?"), Age, bPending ? 1 : 0);
	if (bPending) if (AZoogiGameMode* GM = GameMode.Get()) GM->NotifyZoogiPlacedOnPad(B); // the placed callback: game seats it exactly on the pad
}

void AZoogiGloveCrane::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Glove) return;
	Age += Dt;
	// glove path: drop from in-frame start onto the Zoogi (grab), arc to over the pad, lower, open, rise back out
	FVector P;
	const FVector GrabHold = Grab + FVector(0, 0, (ZoogiConst::ZoogiRadius + 12.f) * Size);
	if (!bReleased && Age > TGrab && !StillPending()) { bReleased = true; for (UStaticMeshComponent* D : ProxyComps) if (D) D->DestroyComponent(); ProxyComps.Reset(); Proxies.Reset(); } // game already placed it (fallback / turn start): just leave
	if (Age < TGrab) P = FMath::Lerp(From, GrabHold, CraneEase(Age / TGrab));
	else if (Age < TIn) P = Bez(GrabHold, FVector((GrabHold.X + Up.X) * 0.5f, (GrabHold.Y + Up.Y) * 0.5f, FMath::Max(GrabHold.Z, Up.Z) + (bPadOnScr ? 260.f : 90.f)), Up, CraneEase((Age - TGrab) / (TIn - TGrab)));
	else if (Age < TDown) P = FMath::Lerp(Up, Pad + FVector(0, 0, ZoogiConst::ZoogiRadius + 12.f) * Size + FVector(0, 0, 2.f), CraneEase((Age - TIn) / (TDown - TIn)));
	else if (Age < TOpen) P = Pad + FVector(0, 0, (ZoogiConst::ZoogiRadius + 12.f) * Size + 2.f);
	else P = FMath::Lerp(Pad + FVector(0, 0, (ZoogiConst::ZoogiRadius + 12.f) * Size + 2.f), Pad + FVector(0, 0, 1100.f), CraneEase((Age - TOpen) / (TEnd - TOpen)));
	// slight swing: lean into the motion while swooping, settle while lowering
	const float Lean = (Age > TGrab && Age < TIn) ? 18.f * FMath::Sin(PI * (Age - TGrab) / (TIn - TGrab)) : 0.f;
	const FVector Dir = (Pad - Grab).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FRotator R = FRotationMatrix::MakeFromX(Dir).Rotator() + FRotator(-Lean, 0, 0);
	Glove->SetWorldLocationAndRotation(P, FRotator(R.Pitch, R.Yaw, 0.f));
	// fingers: closed (gripping) until TDown, open by TOpen, stay open while leaving
	const float Open = Age < TGrab ? 1.f - CraneEase((Age - TGrab * 0.6f) / (TGrab * 0.4f)) : (Age < TDown ? 0.f : CraneEase((Age - TDown) / (TOpen - TDown)));
	for (USceneComponent* F : Fingers) { FRotator FR = F->GetRelativeRotation(); FR.Pitch = FMath::Lerp(28.f, -30.f, Open); F->SetRelativeRotation(FR); }
	// crane trolley high overhead, cable down to the cuff
	const FVector Top = FVector(P.X, P.Y, FMath::Max(P.Z + 700.f, Pad.Z + 1300.f));
	Trolley->SetWorldLocationAndRotation(Top, FRotator(0, R.Yaw, 0));
	const FVector CuffTop = P + FVector(0, 0, 60.f * Size);
	const float Len = FMath::Max(1.f, (Top - CuffTop).Size());
	Cable->SetWorldLocationAndRotation((Top + CuffTop) * 0.5f, FRotationMatrix::MakeFromZ(Top - CuffTop).Rotator());
	Cable->SetWorldScale3D(FVector(0.05f, 0.05f, Len / 100.f));
	// carried copy hangs at the Hold point (keeps its own facing and squash pose, offset from the real Zoogi's spot)
	if (!bReleased)
	{
		const FVector HoldP = Age < TGrab ? Grab : Hold->GetComponentLocation(); // waits at the knock-off spot until grabbed
		for (FProxy& X : Proxies)
			if (UStaticMeshComponent* S = X.Src.Get())
			{
				FTransform T = X.Rel * FTransform(FRotator(0.f, S->GetOwner()->GetActorRotation().Yaw, 0.f), HoldP);
				X.Dst->SetWorldTransform(T);
			}
	}
	if (!bReleased && Age >= TOpen) Release();
	if (AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) // capture runs (-ZoogiShots): every 0.15 s
		if (Shot < 11 && Age >= Shot * 0.15f) { GM->TakeShot(FString::Printf(TEXT("glove%d_%02d_t%03d"), GCount, Shot, Shot * 15)); ++Shot;
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0)) { FVector2D S1(-1, -1), S2(-1, -1); PC->ProjectWorldLocationToScreen(P, S1); PC->ProjectWorldLocationToScreen(Pad, S2); UE_LOG(LogZoogi, Log, TEXT("GloveCrane: dbg t=%.2f P=%s scr=%s pad scr=%s cam=%s vis=%d"), Age, *P.ToString(), *S1.ToString(), *S2.ToString(), PC->PlayerCameraManager ? *PC->PlayerCameraManager->GetCameraLocation().ToString() : TEXT("-"), Glove->IsVisible() ? 1 : 0); } }
	if (Age >= TEnd) Destroy();
}

void AZoogiGloveCrane::EndPlay(const EEndPlayReason::Type Reason)
{
	// destroyed before release (level change, replaced): hand the Zoogi to the game now rather than waiting for the 6 s fallback
	if (!bReleased && Reason == EEndPlayReason::Destroyed) { bReleased = true; if (StillPending()) if (AZoogiGameMode* GM = GameMode.Get()) GM->NotifyZoogiPlacedOnPad(Ball.Get()); }
	Super::EndPlay(Reason);
}
