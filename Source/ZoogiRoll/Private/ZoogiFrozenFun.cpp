// Oct 29 evening (Evan, build 20): see ZoogiFrozenFun.h.
#include "ZoogiFrozenFun.h"
#include "ZoogiActionFun.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiScoringUI.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ImageUtils.h" // b228: crowd signs
#include "Engine/Texture2D.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "RHIDefinitions.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"

// ------------------------------------------------------------------ event hub
namespace ZoogiShow
{
	static int32 GImpactSerial = 0;
	static FVector GImpactAt = FVector::ZeroVector;
	static TWeakObjectPtr<AActor> GImpactOther;
	static double GLastFrame = -10.0;

	bool On(const TCHAR* NoSwitch)
	{
		return !FParse::Param(FCommandLine::Get(), NoSwitch);
	}

	void Event(UWorld* World, EEvent E, const FVector& At, float Strength)
	{
		if (!World) return;
		if (UZoogiShowSys* S = World->GetSubsystem<UZoogiShowSys>()) S->Push(E, At, Strength);
	}

	void NoteImpact(UWorld* World, const FVector& At, AActor* Other, float Strength)
	{
		++GImpactSerial;
		GImpactAt = At;
		GImpactOther = Other;
		Event(World, EEvent::HardHit, At, Strength);
	}
	int32 ImpactSerial() { return GImpactSerial; }
	FVector ImpactAt() { return GImpactAt; }
	AActor* ImpactOther() { return GImpactOther.Get(); }

	// Mobile (ES3.1 / Metal mobile): the Vefects frame materials need SceneColor (SM5 only), so phones get a canvas impact frame
	// in the same manga style: black frame + white focus wedges -> white flash + black wedges -> fading wedges (~0.26 s).
	static FDelegateHandle GHud;
	static double GFrameT0 = -10.0;
	static FVector GFrameAt = FVector::ZeroVector;
	static int32 GFrameSeed = 1;
	// b82 (Evan): the whole frame lasts 0.5 s (-ZoogiImpactFrameSec=N); the three beats keep their 0.07 / 0.07 / 0.12 proportions
	static double FrameSec() { static double S = -1.0; if (S < 0.0) { float F = 0.2f; /* b120 (Evan 17:38: "reduce the black and white effect on impact 0.2 s") */ FParse::Value(FCommandLine::Get(), TEXT("ZoogiImpactFrameSec="), F); S = FMath::Clamp(F, 0.1f, 2.f); } return S; }
	static void DrawMobileFrame(AHUD* HUD, UCanvas* Canvas)
	{
		if (!HUD || !Canvas || !HUD->GetWorld()) return;
		const double T = (HUD->GetWorld()->GetRealTimeSeconds() - GFrameT0) * (0.26 / FrameSec()); // mapped onto the original 0.26 s timeline
		if (T < 0.0 || T > 0.26) return;
		const float Wd = Canvas->ClipX, Ht = Canvas->ClipY, Mn = FMath::Min(Wd, Ht), Mx = FMath::Max(Wd, Ht);
		FVector2D F(Wd * 0.5f, Ht * 0.5f);
		if (APlayerController* PC = HUD->GetOwningPlayerController())
		{
			FVector2D S;
			if (PC->ProjectWorldLocationToScreen(GFrameAt, S, true)) F = FVector2D(FMath::Clamp(S.X, Wd * 0.2f, Wd * 0.8f), FMath::Clamp(S.Y, Ht * 0.2f, Ht * 0.8f));
		}
		const int32 Ph = T < 0.07 ? 0 : (T < 0.14 ? 1 : 2);
		const float Fade = Ph < 2 ? 1.f : (float)FMath::Clamp(1.0 - (T - 0.14) / 0.12, 0.0, 1.0);
		const FLinearColor Bg = Ph == 0 ? FLinearColor(0.f, 0.f, 0.f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, Ph == 1 ? 0.85f : 0.f);
		const FTexture* WhiteT = Canvas->DefaultTexture ? Canvas->DefaultTexture->GetResource() : nullptr;
		const FLinearColor Ln = Ph == 0 ? FLinearColor(1.f, 1.f, 1.f, 1.f) : FLinearColor(0.f, 0.f, 0.f, 0.95f * Fade);
		if (Bg.A > 0.f)
		{
			FCanvasTileItem Tile(FVector2D::ZeroVector, WhiteT, FVector2D(Wd, Ht), Bg);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
		FRandomStream R(GFrameSeed * 7919 + Ph * 13);
		const int32 N = 64;
		TArray<FCanvasUVTri> Tris;
		Tris.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			const float A = (i + R.FRandRange(-0.35f, 0.35f)) * 2.f * PI / N;
			const float Wdg = R.FRandRange(0.004f, 0.022f);
			const float R0 = Mn * R.FRandRange(0.16f, 0.34f), R1 = Mx * 1.6f;
			FCanvasUVTri Tr;
			Tr.V0_Pos = F + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R0;
			Tr.V1_Pos = F + FVector2D(FMath::Cos(A - Wdg), FMath::Sin(A - Wdg)) * R1;
			Tr.V2_Pos = F + FVector2D(FMath::Cos(A + Wdg), FMath::Sin(A + Wdg)) * R1;
			Tr.V0_Color = Tr.V1_Color = Tr.V2_Color = Ln;
			Tris.Add(Tr);
		}
		FCanvasTriangleItem TI(Tris, WhiteT);
		TI.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TI);
	}

	void ImpactFrame(UWorld* World, const FVector& At, int32 Variant)
	{
		static const bool bOn = On(TEXT("ZoogiNoImpactFrames")) && On(TEXT("ZoogiNoNiagara"));
		if (!World || !bOn) return;
		const double Now = World->GetRealTimeSeconds();
		if (Now - GLastFrame < FMath::Max(0.3, FrameSec() + 0.05)) return; // b82: one full frame before the next
		GLastFrame = Now;
		Variant = FMath::Clamp(Variant, 1, 10);
		static const bool bForceMobile = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMobileImpactFrame"));
		if (bForceMobile || World->GetFeatureLevel() < ERHIFeatureLevel::SM5)
		{
			GFrameT0 = Now; GFrameAt = At; ++GFrameSeed;
			if (!GHud.IsValid()) GHud = AHUD::OnHUDPostRender.AddStatic(&DrawMobileFrame);
			UE_LOG(LogZoogi, Log, TEXT("IMPACTFRAME mobile canvas frame at (%.0f, %.0f, %.0f)"), At.X, At.Y, At.Z);
			return;
		}
		const FString Path = FString::Printf(TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Frames/Particles/NS_Impact_Frame_Advanced_%02d_Always.NS_Impact_Frame_Advanced_%02d_Always"), Variant, Variant);
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sys)
		{
			static bool bWarned = false;
			if (!bWarned) { bWarned = true; UE_LOG(LogZoogi, Warning, TEXT("IMPACTFRAME missing %s (not cooked on this platform?)"), *Path); }
			return;
		}
		if (UNiagaraComponent* Nc = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, At, FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease))
			Nc->SetCustomTimeDilation((float)(0.26 / FrameSec())); // b82: same 0.5 s on PC
		UE_LOG(LogZoogi, Log, TEXT("IMPACTFRAME %02d at (%.0f, %.0f, %.0f)"), Variant, At.X, At.Y, At.Z);
	}
}

namespace
{
	UStaticMeshComponent* NewPart(AActor* Owner, USceneComponent* Parent, const TCHAR* Shape, UMaterialInterface* M, bool bShadow = true)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCastShadow(bShadow);
		C->SetCanEverAffectNavigation(false);
		C->RegisterComponent();
		if (M) C->SetMaterial(0, M);
		return C;
	}

	/** Free debris: tumbles with real physics against the static world only (never touches Zoogis / orbs). */
	void MakeDebris(UStaticMeshComponent* C, const FVector& V, const FVector& WDeg)
	{
		if (!C) return;
		if (C->GetAttachParent()) C->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		C->SetCollisionObjectType(ECC_PhysicsBody);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetSimulatePhysics(true);
		C->SetEnableGravity(true);
		C->SetLinearDamping(0.15f);
		C->SetAngularDamping(0.4f);
		C->SetPhysicsLinearVelocity(V);
		C->SetPhysicsAngularVelocityInDegrees(WDeg);
	}

	void Settle(UStaticMeshComponent* C, USceneComponent* Parent, const FTransform& Rel)
	{
		if (!C) return;
		if (C->IsSimulatingPhysics()) C->SetSimulatePhysics(false);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Parent && C->GetAttachParent() != Parent) C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeTransform(Rel);
		C->SetVisibility(true, true);
	}
}

// ------------------------------------------------------------------ igloo wall
AZoogiIglooWall::AZoogiIglooWall()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZoogiIglooWall::BuildIgloo(float R0, float T, float A0, float A1, float CourseH, UMaterialInterface* BlockA, UMaterialInterface* BlockB, UMaterialInterface* Mortar, UPhysicalMaterial* PM)
{
	// colliders: one per course (the arc chord of the segment, like the old wall)
	const float Am = 0.5f * (A0 + A1);
	const float Rad = FMath::DegreesToRadians(Am);
	const float Rc = R0 + T * 0.5f;
	const float SegLen = Rc * FMath::DegreesToRadians(A1 - A0) * 1.04f;
	const FVector Cxy(FMath::Cos(Rad) * Rc, FMath::Sin(Rad) * Rc, 0.f);
	ColLow = AddColliderBox(Cxy + FVector(0, 0, CourseH * 0.5f - 6.f), FVector(T, SegLen, CourseH + 12.f), FRotator(0.f, Am, 0.f), TEXT("ZWall"), PM);
	ColHigh = AddColliderBox(Cxy + FVector(0, 0, CourseH * 1.5f), FVector(T, SegLen, CourseH), FRotator(0.f, Am, 0.f), TEXT("ZWall"), PM);
	SetPathBlock(FVector2D(Cxy), 40.f);
	// snowy footing strip (stays as rubble)
	{
		UStaticMeshComponent* F = AddBox(Cxy + FVector(0, 0, 3.f), FVector(T + 26.f, SegLen, 10.f), FRotator(0.f, Am, 0.f), Mortar, false);
		(void)F;
	}
	FinishBuild();
	// blocks: 2 courses, staggered half a block, small gaps so every block reads; slight random size / tilt (hand-cut snow)
	FRandomStream Rng((int32)(A0 * 113.f) ^ 0x5ee1);
	const float ArcLen = Rc * FMath::DegreesToRadians(A1 - A0);
	const int32 N = FMath::Max(2, FMath::RoundToInt(ArcLen / 58.f));
	for (int32 c = 0; c < 2; ++c)
	{
		const float Shift = c == 1 ? 0.5f : 0.f;
		for (int32 i = 0; i < N + (c == 1 ? 1 : 0); ++i)
		{
			float F0 = ((float)i - Shift) / N, F1 = ((float)i + 1.f - Shift) / N;
			F0 = FMath::Clamp(F0, 0.f, 1.f); F1 = FMath::Clamp(F1, 0.f, 1.f);
			if (F1 - F0 < 0.2f / N) continue;
			const float Aa = FMath::Lerp(A0, A1, F0), Ab = FMath::Lerp(A0, A1, F1), Ac = 0.5f * (Aa + Ab);
			const float Len = Rc * FMath::DegreesToRadians(Ab - Aa) - 4.f;
			const float Hh = CourseH - 4.f + Rng.FRandRange(-2.f, 2.f);
			const float Rr = FMath::DegreesToRadians(Ac);
			const FVector P(FMath::Cos(Rr) * Rc, FMath::Sin(Rr) * Rc, c * CourseH + 2.f + Hh * 0.5f);
			const FRotator Rot(Rng.FRandRange(-1.5f, 1.5f), Ac + Rng.FRandRange(-1.5f, 1.5f), Rng.FRandRange(-1.f, 1.f) - (c == 1 ? 3.f : 0.f)); // top course leans in a touch (igloo)
			UStaticMeshComponent* B = NewPart(this, Root, TEXT("Cube"), ((i + c) & 1) ? BlockA : BlockB, true);
			const FTransform X(Rot, P, FVector((T + 8.f) / 100.f, FMath::Max(Len, 8.f) / 100.f, Hh / 100.f));
			B->SetRelativeTransform(X);
			FBrick Bk; Bk.C = B; Bk.Base = X; Bk.Course = c;
			Bricks.Add(Bk);
		}
	}
	Courses = 2;
}

bool AZoogiIglooWall::ApplyDamage(float Amount, const FVector& At, const FVector& Dir, EZoogiArtifact Element, AZoogiBall* By)
{
	if (!IsBuilt() || Courses <= 0 || Amount <= 0.f) return false;
	if (!ZoogiUtil::DestructionEnabled()) return false;
	if (PowerMultiplier(GetSurface(), Element) < 0.f) return false; // Frost on snow: no refreeze of blasted blocks
	if (Clock2 - LastBlast < 0.3f) return false;
	LastBlast = Clock2;
	BlastCourse(Courses - 1, At, Dir, FMath::Clamp(Amount / 30.f, 0.6f, 1.6f));
	if (By) UE_LOG(LogZoogi, Log, TEXT("IGLOO %s blasted by P%d -> %d course(s) left"), *GetName(), By->PlayerIndex + 1, Courses);
	return true;
}

void AZoogiIglooWall::OnOrbHit(AZoogiOrb* Orb, const FVector& At, const FVector& Normal, float Speed)
{
	if (Speed < 520.f || !Orb) return;
	ApplyDamage(10.f + (Speed - 520.f) * 0.05f, At, -Normal, EZoogiArtifact::None, nullptr);
}

void AZoogiIglooWall::BlastCourse(int32 Course, const FVector& At, const FVector& Dir0, float Power)
{
	FVector Dir = Dir0.GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = (GetActorLocation() - At).GetSafeNormal2D();
	FRandomStream Rng((int32)(Clock2 * 1000.f) + Course * 7);
	for (FBrick& B : Bricks)
	{
		if (B.Course != Course || !B.C.IsValid() || B.FlyAge >= 0.f) continue;
		UStaticMeshComponent* C = B.C.Get();
		const FVector P = C->GetComponentLocation();
		const FVector Away = (P - At).GetSafeNormal2D();
		const float Near = FMath::Clamp(1.f - FVector::Dist2D(P, At) / 320.f, 0.3f, 1.f);
		const FVector V = (Dir * 520.f + Away * 260.f) * Power * Near + FVector(0, 0, Rng.FRandRange(320.f, 620.f) * Power) + Rng.VRand() * 90.f;
		MakeDebris(C, V, Rng.VRand() * Rng.FRandRange(180.f, 540.f));
		B.FlyAge = 0.f;
	}
	if (Course == 1 && ColHigh) ColHigh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Course == 0 && ColLow) ColLow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Courses = Course;
	if (Courses <= 0) { State = EZoogiPieceState::Broken; bCollisionLive = false; HP = 0.f; }
	UWorld* W = GetWorld();
	AZoogiFX::Spawn(W, At + FVector(0, 0, 40.f), FLinearColor(0.9f, 0.96f, 1.f), 170.f, 0.45f, false, 4.f);
	AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), At + FVector(0, 0, 50.f), FRotator::ZeroRotator, 0.9f);
	AZoogiFX::Niagara(W, TEXT("NS_Dust"), At, FRotator::ZeroRotator, 1.2f);
	ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), 0.75f, 1.15f + 0.1f * Course);
	ZoogiShow::Event(W, ZoogiShow::EEvent::Blast, At, Power);
}

void AZoogiIglooWall::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock2 += Dt;
	for (FBrick& B : Bricks)
	{
		if (B.FlyAge < 0.f || !B.C.IsValid()) continue;
		B.FlyAge += Dt;
		UStaticMeshComponent* C = B.C.Get();
		if (B.FlyAge > 3.6f && C->IsVisible())
		{
			const float K = FMath::Clamp(1.f - (B.FlyAge - 3.6f) / 0.5f, 0.f, 1.f);
			if (K <= 0.f) { C->SetSimulatePhysics(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetVisibility(false); }
			else C->SetWorldScale3D(B.Base.GetScale3D() * GetActorScale3D() * K);
		}
	}
}

void AZoogiIglooWall::ResetPiece()
{
	bCollisionLive = true;
	State = EZoogiPieceState::Intact;
	Super::ResetPiece();
	for (FBrick& B : Bricks) { Settle(B.C.Get(), Root, B.Base); B.FlyAge = -1.f; }
	if (ColLow) ColLow->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (ColHigh) ColHigh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Courses = 2;
	bCollisionLive = true;
}

// ------------------------------------------------------------------ snowman
AZoogiSnowman::AZoogiSnowman()
{
	PrimaryActorTick.bCanEverTick = true;
}

UStaticMeshComponent* AZoogiSnowman::Part(const TCHAR* Shape, USceneComponent* Parent, const FVector& Loc, const FRotator& Rot, const FVector& Scale, UMaterialInterface* M)
{
	// Loc / Scale are in the snowman's own (unscaled) space; convert into the parent's relative space.
	const FVector PS = (Parent && Parent != Body) ? Parent->GetRelativeScale3D() : FVector(1.f);
	UStaticMeshComponent* C = NewPart(this, Parent ? Parent : Body.Get(), Shape, M, true);
	const FTransform X(Rot, Loc / PS, Scale / PS);
	C->SetRelativeTransform(X);
	FRest R; R.C = C; R.Rel = X; R.Parent = Parent ? Parent : Body.Get();
	Rest.Add(R);
	return C;
}

void AZoogiSnowman::BuildSnowman(float Height, UMaterialInterface* SnowMat)
{
	H = Height;
	SnowM = SnowMat;
	const float K = Height / 330.f;
	Body = NewObject<USceneComponent>(this);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetupAttachment(Root);
	Body->RegisterComponent();
	UMaterialInterface* Coal = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#17181D")), 0.35f);
	UMaterialInterface* Carrot = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FF7A1A")), 0.45f, ZoogiUtil::Hex(TEXT("#FF5A00")), 0.6f);
	UMaterialInterface* Stick = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#6B4423")), 0.8f);
	UMaterialInterface* Scarf = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E0283F")), 0.6f);
	UMaterialInterface* Band = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#2EB8F0")), 0.4f, ZoogiUtil::Hex(TEXT("#19C3E6")), 1.2f);
	// stacked snow balls (+X faces the court centre)
	Bottom = Part(TEXT("Sphere"), Body, FVector(0, 0, 88.f * K), FRotator::ZeroRotator, FVector(1.82f, 1.82f, 1.66f) * K, SnowMat);
	Middle = Part(TEXT("Sphere"), Body, FVector(0, 0, 208.f * K), FRotator::ZeroRotator, FVector(1.34f) * K, SnowMat);
	Head = Part(TEXT("Sphere"), Body, FVector(0, 0, 302.f * K), FRotator::ZeroRotator, FVector(0.94f) * K, SnowMat);
	// face (children of the head: they fly with it)
	const float Hr = 47.f * K;
	for (int32 s = -1; s <= 1; s += 2)
		Part(TEXT("Sphere"), Head, FVector(Hr * 0.86f, s * Hr * 0.36f, Hr * 0.28f), FRotator::ZeroRotator, FVector(0.15f * K), Coal);
	Part(TEXT("Cone"), Head, FVector(Hr * 1.25f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f), FVector(0.16f * K, 0.16f * K, 0.5f * K), Carrot);
	for (int32 m = 0; m < 5; ++m)
	{
		const float A = FMath::DegreesToRadians(-34.f + m * 17.f);
		Part(TEXT("Sphere"), Head, FVector(FMath::Cos(A) * Hr * 0.9f, FMath::Sin(A) * Hr * 0.9f, -Hr * 0.35f - FMath::Abs(FMath::Sin(A)) * -Hr * 0.18f), FRotator::ZeroRotator, FVector(0.08f * K), Coal);
	}
	// top hat: crown is the flying part, brim + band ride on it
	HatTop = Part(TEXT("Cylinder"), Head, FVector(0, 0, Hr * 1.55f), FRotator(0.f, 0.f, -6.f), FVector(0.66f * K, 0.66f * K, 0.52f * K), Coal);
	{
		const FVector HS = HatTop->GetRelativeScale3D() * Head->GetRelativeScale3D();
		auto HatChild = [&](const FVector& L, const FVector& S, UMaterialInterface* M)
		{
			UStaticMeshComponent* C = NewPart(this, HatTop, TEXT("Cylinder"), M, true);
			const FTransform X(FRotator::ZeroRotator, L / HS, S / HS);
			C->SetRelativeTransform(X);
			FRest R; R.C = C; R.Rel = X; R.Parent = HatTop; Rest.Add(R);
		};
		HatChild(FVector(0, 0, -24.f * K), FVector(1.12f * K, 1.12f * K, 0.05f * K), Coal);
		HatChild(FVector(0, 0, -14.f * K), FVector(0.68f * K, 0.68f * K, 0.1f * K), Band);
	}
	// scarf + buttons (children of the middle)
	{
		UStaticMeshComponent* S1 = Part(TEXT("Cylinder"), Middle, FVector(0, 0, 58.f * K), FRotator::ZeroRotator, FVector(1.02f * K, 1.02f * K, 0.2f * K), Scarf);
		UStaticMeshComponent* S2 = Part(TEXT("Cube"), Middle, FVector(52.f * K, 22.f * K, 24.f * K), FRotator(-12.f, 0.f, 8.f), FVector(0.06f * K, 0.24f * K, 0.62f * K), Scarf);
		BodyBits.Add(S1); BodyBits.Add(S2);
		for (int32 b = 0; b < 3; ++b)
		{
			const float Z = (22.f - b * 22.f) * K;
			const float X = FMath::Sqrt(FMath::Max(1.f, FMath::Square(66.f * K) - Z * Z));
			BodyBits.Add(Part(TEXT("Sphere"), Middle, FVector(X, 0.f, Z), FRotator::ZeroRotator, FVector(0.13f * K), Coal));
		}
	}
	// stick arms (+ a twig each), shoulders on the middle ball
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector Dir = FVector(0.f, (float)s, 0.62f).GetSafeNormal();
		const FVector Shoulder(0.f, s * 58.f * K, 222.f * K);
		const float Len = 128.f * K;
		const FRotator R = FRotationMatrix::MakeFromZ(Dir).Rotator();
		UStaticMeshComponent* A = Part(TEXT("Cylinder"), Body, Shoulder + Dir * Len * 0.5f, R, FVector(0.07f * K, 0.07f * K, Len / 100.f), Stick);
		const FVector AS = A->GetRelativeScale3D();
		UStaticMeshComponent* Tw = NewPart(this, A, TEXT("Cylinder"), Stick, true);
		const FTransform TX(FRotator(0.f, 0.f, 35.f * s), FVector(0.f, 0.f, 22.f), FVector(0.045f * K, 0.045f * K, 0.4f * K) / AS);
		Tw->SetRelativeTransform(TX);
		FRest TR; TR.C = Tw; TR.Rel = TX; TR.Parent = A; Rest.Add(TR);
		if (s < 0) ArmL = A; else ArmR = A;
	}
	// collider (body + head), path block
	Col = AddColliderCyl(FVector2D::ZeroVector, 88.f * K, 0.f, 350.f * K, TEXT("ZSnowman"), nullptr);
	if (Col) { ColRel = Col->GetRelativeTransform(); bColRel = true; }
	SetPathBlock(FVector2D(GetActorLocation()), 92.f * K);
	// snow chunks for the explosion (hidden until then)
	for (int32 i = 0; i < 16; ++i)
	{
		UStaticMeshComponent* C = NewPart(this, Root, TEXT("Sphere"), SnowMat, true);
		C->SetVisibility(false);
		Chunks.Add(C);
	}
	FinishBuild();
	RubbleKeepZ = -1000.f; // the base parts' rubble pass never flattens anything here
}

void AZoogiSnowman::Launch(UStaticMeshComponent* C, const FVector& Dir, float Speed, float Up)
{
	if (!C) return;
	FRandomStream Rng((int32)(T * 977.f) + Stage);
	const FVector V = Dir.GetSafeNormal() * Speed + FVector(0, 0, Up) + Rng.VRand() * 60.f;
	MakeDebris(C, V, Rng.VRand() * Rng.FRandRange(220.f, 620.f));
	FFly F; F.C = C; F.Age = 0.f; Flying.Add(F);
}

void AZoogiSnowman::KnockNext(const FVector& Dir)
{
	LastKnock = -10.f;
	ApplyDamage(50.f, GetActorLocation() + FVector(0, 0, 150.f) - Dir * 80.f, Dir, EZoogiArtifact::None, nullptr);
}

bool AZoogiSnowman::ApplyDamage(float Amount, const FVector& At, const FVector& Dir0, EZoogiArtifact Element, AZoogiBall* By)
{
	if (!IsBuilt() || Stage >= 4 || Amount <= 0.f) return false;
	if (T - LastKnock < 0.45f) return false;
	LastKnock = T;
	FVector Dir = Dir0.GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = (GetActorLocation() - At).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
	WobbleDir = Dir; Wobble = 1.f;
	UWorld* W = GetWorld();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	++Stage;
	const TCHAR* What = TEXT("");
	switch (Stage)
	{
	case 1: Launch(ArmL, Dir * 0.6f + Right * -0.8f, 620.f, 480.f); What = TEXT("left arm"); break;
	case 2: Launch(ArmR, Dir * 0.6f + Right * 0.8f, 620.f, 480.f); What = TEXT("right arm"); break;
	case 3:
		Launch(HatTop, Dir + FVector(0, 0, 0.2f), 420.f, 900.f);
		Launch(Head, Dir, 560.f, 620.f);
		if (Col) { Col->SetRelativeScale3D(Col->GetRelativeScale3D() * FVector(1.f, 1.f, 0.78f)); }
		What = TEXT("head");
		break;
	default:
	{
		// boom: the two body balls burst into tumbling snow chunks
		const FVector C0 = GetActorLocation() + FVector(0, 0, 150.f * H / 330.f);
		for (UStaticMeshComponent* B : { Bottom.Get(), Middle.Get() }) if (B) B->SetVisibility(false, true);
		FRandomStream Rng(4242 + (int32)T);
		for (UStaticMeshComponent* C : Chunks)
		{
			if (!C) continue;
			const float R = Rng.FRandRange(0.28f, 0.62f) * H / 330.f;
			C->SetWorldScale3D(FVector(R) * GetActorScale3D());
			C->SetWorldLocation(C0 + Rng.VRand() * 70.f * H / 330.f);
			C->SetVisibility(true);
			const FVector Out = (C->GetComponentLocation() - C0).GetSafeNormal();
			MakeDebris(C, (Out * Rng.FRandRange(500.f, 900.f) + Dir * 300.f) + FVector(0, 0, Rng.FRandRange(400.f, 850.f)), Rng.VRand() * 400.f);
			FFly F; F.C = C; F.Age = 0.f; Flying.Add(F);
		}
		if (Col) Col->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		State = EZoogiPieceState::Broken; bCollisionLive = false; HP = 0.f;
		AZoogiFX::Spawn(W, C0, FLinearColor(0.92f, 0.97f, 1.f), 420.f, 0.7f, false, 8.f);
		AZoogiFX::Spawn(W, GetActorLocation() + FVector(0, 0, 5.f), FLinearColor(0.6f, 0.9f, 1.f), 520.f, 0.6f, true, 7.f);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), C0, FRotator::ZeroRotator, 2.4f);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), C0 + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.6f);
		AZoogiFX::SpawnConfetti(W, C0 + FVector(0, 0, 60.f), FLinearColor(1.f, 0.4f, 0.2f), 36);
		ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.8f, 1.2f);
		What = TEXT("EXPLODES");
		ZoogiShow::Event(W, ZoogiShow::EEvent::Snowman, C0, 2.f);
		ZoogiShow::ImpactFrame(W, C0, 4);
		break;
	}
	}
	if (Stage < 4)
	{
		AZoogiFX::Spawn(W, At, FLinearColor(0.92f, 0.97f, 1.f), 160.f, 0.35f, false, 4.f);
		AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), At, FRotator::ZeroRotator, 0.8f);
		ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), 0.7f, 1.3f);
		ZoogiShow::Event(W, ZoogiShow::EEvent::Snowman, At, 0.6f);
		if (UZoogiShowSys* S = W ? W->GetSubsystem<UZoogiShowSys>() : nullptr) S->Fireworks(1);
	}
	UE_LOG(LogZoogi, Log, TEXT("SNOWMAN hit%s -> stage %d (%s)"), By ? *FString::Printf(TEXT(" by P%d"), By->PlayerIndex + 1) : TEXT(""), Stage, What);
	return true;
}

void AZoogiSnowman::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	Wobble = FMath::Max(0.f, Wobble - Dt * 1.6f);
	if (Body)
	{
		// idle sway + a springy wobble away from the last hit
		const float Sw = 1.6f * FMath::Sin(T * 1.3f);
		const float Wb = 11.f * Wobble * FMath::Sin(T * 17.f) * Wobble;
		const FVector Ax = FVector::CrossProduct(FVector::UpVector, WobbleDir).GetSafeNormal();
		const FQuat Q = FQuat(Ax.IsNearlyZero() ? FVector::RightVector : GetActorTransform().InverseTransformVectorNoScale(Ax), FMath::DegreesToRadians(Wb)) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Sw));
		Body->SetRelativeRotation(Q);
	}
	// alive: the head looks around, the arms wave (only while attached)
	if (Head && !Head->IsSimulatingPhysics() && Stage < 3)
	{
		const FRest* R = Rest.FindByPredicate([this](const FRest& X) { return X.C.Get() == Head; });
		if (R) Head->SetRelativeRotation(FRotator(3.f * FMath::Sin(T * 2.1f), 14.f * FMath::Sin(T * 0.7f) + (Stage > 0 ? 8.f * FMath::Sin(T * 9.f) : 0.f), 0.f));
	}
	for (UStaticMeshComponent* A : { ArmL.Get(), ArmR.Get() })
	{
		if (!A || A->IsSimulatingPhysics() || !A->IsVisible()) continue;
		const FRest* R = Rest.FindByPredicate([A](const FRest& X) { return X.C.Get() == A; });
		if (!R) continue;
		const float S = A == ArmL ? -1.f : 1.f;
		const FRotator Wave(0.f, 0.f, S * 10.f * FMath::Sin(T * 3.2f + (S > 0 ? 1.f : 0.f)));
		A->SetRelativeRotation((Wave.Quaternion() * R->Rel.GetRotation()).Rotator());
	}
	for (FFly& F : Flying)
	{
		if (F.Age < 0.f || !F.C.IsValid()) continue;
		F.Age += Dt;
		if (F.Age > 4.f && F.C->IsVisible())
		{
			F.C->SetSimulatePhysics(false);
			F.C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F.C->SetVisibility(false, true);
			F.Age = -1.f;
		}
	}
}

void AZoogiSnowman::ResetPiece()
{
	bCollisionLive = true;
	State = EZoogiPieceState::Intact;
	Super::ResetPiece();
	for (const FRest& R : Rest) Settle(R.C.Get(), R.Parent.Get(), R.Rel);
	for (UStaticMeshComponent* C : Chunks) if (C) { if (C->IsSimulatingPhysics()) C->SetSimulatePhysics(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform); C->SetVisibility(false); }
	Flying.Reset();
	if (Col)
	{
		if (bColRel) Col->SetRelativeTransform(ColRel);
		Col->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	Stage = 0; Wobble = 0.f; LastKnock = -10.f;
	bCollisionLive = true;
}

// ------------------------------------------------------------------ crowd
AZoogiFrozenCrowd::AZoogiFrozenCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
}

bool AZoogiFrozenCrowd::BuildToon(const TArray<FVector>& Seats, const TArray<int32>& Islet)
{
	auto NewISM = [&](const TCHAR* Shape, UMaterialInterface* M) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
		I->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); I->SetMobility(EComponentMobility::Movable); I->SetupAttachment(Root);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision); I->SetCastShadow(false); I->SetCanEverAffectNavigation(false); I->SetGenerateOverlapEvents(false);
		I->RegisterComponent(); if (M) I->SetMaterial(0, M); ToonISMs.Add(I); return I;
	};
	static const TCHAR* Cols[] = { TEXT("#4696FF"), TEXT("#FF5046"), TEXT("#FFC832"), TEXT("#AA5AFF"), TEXT("#5AD278"), TEXT("#FF78C8") };
	NCol = UE_ARRAY_COUNT(Cols);
	for (const TCHAR* C : Cols) NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(C), 0.5f));
	NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, FLinearColor(0.95f, 0.95f, 0.97f), 0.3f)); // eye whites
	NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, FLinearColor(0.02f, 0.02f, 0.03f), 0.2f)); // pupils
	NewISM(TEXT("Cylinder"), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#6E4F32")), 0.8f)); // sign sticks
	UMaterialInterface* SignBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicPlanet.M_CosmicPlanet"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	NSign = 0; TArray<UMaterialInstanceDynamic*> SignMats; // b229
	for (int32 s = 0; s < 8 && SignBase; ++s)
	{
		const FString Pth = FPaths::ProjectContentDir() / FString::Printf(TEXT("Data/Crowd/sign_%d.png"), s);
		UTexture2D* Tx = FPaths::FileExists(Pth) ? FImageUtils::ImportFileAsTexture2D(Pth) : nullptr;
		if (!Tx) continue;
		UMaterialInstanceDynamic* Mi = UMaterialInstanceDynamic::Create(SignBase, this);
		Mi->SetTextureParameterValue(TEXT("Tex"), Tx); Mi->SetScalarParameterValue(TEXT("Bright"), 1.15f);
		SignMats.Add(Mi); ++NSign;
	}
	TArray<AZoogiArenaPiece*> Stands;
	for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It) if (It->GetKind() == TEXT("stand")) Stands.Add(*It);
	FRandomStream Rng(31337);
	static float SignChance = -1.f; if (SignChance < 0.f) { SignChance = 0.3f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiCrowdSigns="), SignChance); }
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		FMember M;
		M.Islet = Islet.IsValidIndex(i) ? Islet[i] : 0;
		M.Base = Seats[i]; M.Phase = Rng.FRandRange(0.f, 2.f * PI); M.Energy = Rng.FRandRange(0.6f, 1.25f); M.Scale = Rng.FRandRange(0.88f, 1.1f);
		M.bSpinner = Rng.FRand() < 0.12f; M.Ang = FMath::Atan2(Seats[i].Y, Seats[i].X);
		M.Col = Rng.RandRange(0, NCol - 1);
		float Best = 1.e9f;
		for (AZoogiArenaPiece* S : Stands) { const float D = FVector::Dist2D(S->GetComponentsBoundingBox().GetCenter(), Seats[i]); if (D < Best) { Best = D; M.Stand = S; } }
		const FTransform Z(FRotator::ZeroRotator, M.Base, FVector(0.01f));
		M.BodyI = ToonISMs[M.Col]->GetInstanceCount(); for (int32 k = 0; k < 5; ++k) ToonISMs[M.Col]->AddInstance(Z, true); // body, 2 ears, 2 hands
		M.EyeI = ToonISMs[NCol]->GetInstanceCount(); for (int32 k = 0; k < 2; ++k) ToonISMs[NCol]->AddInstance(Z, true);
		M.PupI = ToonISMs[NCol + 1]->GetInstanceCount(); for (int32 k = 0; k < 2; ++k) ToonISMs[NCol + 1]->AddInstance(Z, true);
		if (NSign > 0 && Rng.FRand() < SignChance)
		{
			M.Sign = Rng.RandRange(0, NSign - 1);
			M.StickI = ToonISMs[NCol + 2]->AddInstance(Z, true);
			UStaticMeshComponent* SignC = NewObject<UStaticMeshComponent>(this); // b229: a plain mesh per sign
			SignC->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane"))); SignC->SetMobility(EComponentMobility::Movable); SignC->SetupAttachment(Root);
			SignC->SetCollisionEnabled(ECollisionEnabled::NoCollision); SignC->SetCastShadow(false); SignC->SetCanEverAffectNavigation(false); SignC->SetGenerateOverlapEvents(false);
			SignC->RegisterComponent(); SignC->SetMaterial(0, SignMats[M.Sign]); SignC->SetWorldTransform(Z);
			M.BoardI = SignComps.Add(SignC);
		}
		Members.Add(M);
	}
	ToonXf.SetNum(ToonISMs.Num());
	for (int32 m = 0; m < ToonISMs.Num(); ++m) ToonXf[m].SetNum(ToonISMs[m]->GetInstanceCount());
	bToon = true;
	FinishBuild();
	int32 Ns = 0; for (const FMember& M : Members) if (M.Sign >= 0) ++Ns;
	UE_LOG(LogZoogi, Log, TEXT("CROWD: %d toon Zoogi fans (%d holding signs, %d sign kinds) on %d stands"), Members.Num(), Ns, NSign, Stands.Num());
	return true;
}

void AZoogiFrozenCrowd::TickToon(float Dt)
{
	Clock3 += Dt;
	static const bool bCrowdCam = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrowdCam")); // b229 preview: a close camera on a sign-holding fan, cheering
	if (bCrowdCam)
	{
		static TWeakObjectPtr<ACameraActor> CCam; static int32 CIx = -1;
		if (CIx < 0) for (int32 i = 0; i < Members.Num(); ++i) if (Members[i].Sign >= 0 && Members[i].Base.Y < 0.f) { CIx = i; break; }
		if (CIx < 0) for (int32 i = 0; i < Members.Num(); ++i) if (Members[i].Sign >= 0) { CIx = i; break; }
		if (Members.IsValidIndex(CIx))
		{
			const FVector Tg = Members[CIx].Base + FVector(0.f, 0.f, 70.f);
			const FVector In = (FVector(Look.X, Look.Y, Tg.Z) - Tg).GetSafeNormal();
			const FVector Cp = Tg + In * 700.f + FVector(0.f, 0.f, 120.f);
			if (!CCam.IsValid()) { FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; CCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator::ZeroRotator, Cp), Sp); }
			if (ACameraActor* Ca = CCam.Get())
			{
				Ca->SetActorLocationAndRotation(Cp, (Tg - Cp).Rotation());
				if (APlayerController* Pc = GetWorld()->GetFirstPlayerController()) if (Pc->GetViewTarget() != Ca) Pc->SetViewTarget(Ca);
			}
			Cheer = FMath::Max(Cheer, 0.5f + 0.5f * FMath::Sin(Clock3 * 1.3f));
		}
	}
	const float Tm = Clock3;
	static float SignSpin = -1000.f; if (SignSpin < -999.f) { SignSpin = 180.f; /* b230: the B229 signs read upside down */ FParse::Value(FCommandLine::Get(), TEXT("ZoogiSignSpin="), SignSpin); }
	static const bool bSignFlip = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSignFlip"));
	for (FMember& M : Members)
	{
		FVector P = M.Base;
		const FVector ToLook = Look - M.Base;
		float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToLook.Y, ToLook.X));
		float Pitch = 0.f, Roll = 0.f; float Sq = 0.f;
		const float Ph = M.Phase, E = M.Energy;
		P.Z += 2.5f * FMath::Sin(Tm * 1.7f + Ph);
		Yaw += 7.f * FMath::Sin(Tm * 0.8f + Ph * 1.3f);
		if (Hype > 0.01f) P.Z += 7.f * Hype * E * FMath::Abs(FMath::Sin(Tm * 6.5f + Ph));
		if (Cheer > 0.01f)
		{
			P.Z += 36.f * Cheer * E * FMath::Abs(FMath::Sin(Tm * 9.5f * (0.85f + 0.3f * E) + Ph));
			Yaw += 18.f * Cheer * FMath::Sin(Tm * 13.f + Ph);
			if (M.bSpinner && M.Sign < 0) Yaw += Cheer * FMath::Fmod(Tm * 520.f + Ph * 57.f, 360.f);
			Sq = 0.14f * Cheer * FMath::Sin(Tm * 19.f + Ph);
		}
		if (Gasp > 0.01f) { P += FVector(-ToLook.X, -ToLook.Y, 0.f).GetSafeNormal() * 16.f * Gasp; Pitch -= 16.f * Gasp; Roll += 5.f * Gasp * FMath::Sin(Tm * 31.f + Ph); }
		if (WaveA > -100.f)
		{
			float D = FMath::Fmod(M.Ang - WaveA, 2.f * PI); if (D > PI) D -= 2.f * PI; if (D < -PI) D += 2.f * PI;
			const float Wv = FMath::Exp(-D * D / (2.f * 0.12f * 0.12f)); P.Z += 48.f * Wv; Pitch += 10.f * Wv;
		}
		float Sc = M.Scale;
		if (M.Stand.IsValid() && M.Stand->IsBroken()) { if (M.Fall < 0.f) M.Fall = 0.f; M.Fall += Dt; P.Z -= 0.5f * 980.f * M.Fall * M.Fall; Roll += M.Fall * 400.f; if (M.Fall > 1.2f) Sc = 0.001f; }
		else M.Fall = -1.f;
		const FRotator R(Pitch, Yaw, Roll); const FQuat Q(R);
		const FVector C = P + FVector(0.f, 0.f, 34.f * Sc);
		auto At = [&](const FVector& L) { return C + Q.RotateVector(L * Sc); };
		auto PutX = [&](int32 Isl, int32 Idx, const FVector& L, const FVector& S, const FQuat& Rq) { if (ToonXf[Isl].IsValidIndex(Idx)) ToonXf[Isl][Idx] = FTransform(Rq, At(L), S * Sc); };
		const float Up = FMath::Clamp(Cheer * 1.4f, 0.f, 1.f);
		const bool bSign = M.Sign >= 0;
		PutX(M.Col, M.BodyI + 0, FVector::ZeroVector, FVector(0.6f, 0.6f, 0.6f * (1.f + Sq)), Q);
		PutX(M.Col, M.BodyI + 1, FVector(-3.f, -17.f, 26.f), FVector(0.2f), Q);
		PutX(M.Col, M.BodyI + 2, FVector(-3.f, 17.f, 26.f), FVector(0.2f), Q);
		const FVector HandDn(6.f, 31.f, -6.f), HandUp(6.f, 30.f, 34.f + 6.f * FMath::Sin(Tm * 14.f + Ph));
		PutX(M.Col, M.BodyI + 3, FMath::Lerp(HandDn, HandUp, Up) * FVector(1.f, -1.f, 1.f), FVector(0.16f), Q);
		PutX(M.Col, M.BodyI + 4, bSign ? FVector(8.f, 30.f, 22.f) : FMath::Lerp(HandDn, HandUp, Up), FVector(0.16f), Q);
		PutX(NCol, M.EyeI + 0, FVector(24.f, -10.f, 8.f), FVector(0.1f, 0.13f, 0.15f), Q);
		PutX(NCol, M.EyeI + 1, FVector(24.f, 10.f, 8.f), FVector(0.1f, 0.13f, 0.15f), Q);
		PutX(NCol + 1, M.PupI + 0, FVector(29.f, -10.f, 7.f), FVector(0.07f), Q);
		PutX(NCol + 1, M.PupI + 1, FVector(29.f, 10.f, 7.f), FVector(0.07f), Q);
		if (bSign)
		{	// the sign goes up (and shakes) on big moments
			const float Lift = 18.f + 34.f * Up, Shake = 6.f * Up * FMath::Sin(Tm * 16.f + Ph);
			PutX(NCol + 2, M.StickI, FVector(10.f, 30.f, 22.f + Lift * 0.5f + 25.f), FVector(0.035f, 0.035f, 0.55f), Q);
			const FVector Fwd = Q.GetForwardVector(), Right = Q.GetRightVector();
			FQuat Bq = FRotationMatrix::MakeFromZX(Fwd, bSignFlip ? -Right : Right).ToQuat();
			if (FMath::Abs(SignSpin) > 0.1f) Bq = FQuat(Fwd, FMath::DegreesToRadians(SignSpin)) * Bq;
			Bq = FQuat(Fwd, FMath::DegreesToRadians(Shake)) * Bq;
			if (SignComps.IsValidIndex(M.BoardI) && SignComps[M.BoardI]) SignComps[M.BoardI]->SetWorldTransform(FTransform(Bq, At(FVector(12.f, 30.f, 62.f + Lift)), FVector(0.92f, 0.46f, 1.f) * Sc)); // b229
		}
	}
	for (int32 m = 0; m < ToonISMs.Num(); ++m)
		if (ToonISMs[m] && ToonXf[m].Num() > 0) ToonISMs[m]->BatchUpdateInstancesTransforms(0, ToonXf[m], true, true, true);
}

void AZoogiFrozenCrowd::BuildCrowd(const TArray<FVector>& Seats, const TArray<int32>& Islet)
{
	static const bool bOldCrowd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldCrowd")); // b228: the old-style meshes (rendered as black balls)
	if (!bOldCrowd && BuildToon(Seats, Islet)) return;
	static const TCHAR* Ids[] = { TEXT("bolt"), TEXT("hotstreak"), TEXT("lars"), TEXT("nightshade"), TEXT("pinpoint"), TEXT("wolfgang"), TEXT("wraps") };
	TArray<UStaticMesh*> Meshes;
	for (const TCHAR* Id : Ids)
	{
		UStaticMesh* M = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Zoogis/%s/SM_%s.SM_%s"), Id, Id, Id));
		if (!M) continue;
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
		I->SetStaticMesh(M);
		I->SetMobility(EComponentMobility::Movable);
		I->SetupAttachment(Root);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCastShadow(false);
		I->SetCanEverAffectNavigation(false);
		I->SetGenerateOverlapEvents(false);
		I->RegisterComponent();
		ISMs.Add(I);
		const FBoxSphereBounds B = M->GetBounds();
		const float S = 72.f / FMath::Max(2.f * B.BoxExtent.GetMax(), 1.f); // ~72 cm tall fans
		MeshScale.Add(S);
		MeshOff.Add(-B.Origin * S);
		Meshes.Add(M);
	}
	if (ISMs.Num() == 0) { UE_LOG(LogZoogi, Warning, TEXT("CROWD: no old-style Zoogi meshes found")); return; }
	// stand sections (a broken section drops its fans)
	TArray<AZoogiArenaPiece*> Stands;
	for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It) if (It->GetKind() == TEXT("stand")) Stands.Add(*It);
	FRandomStream Rng(31337);
	Xf.SetNum(ISMs.Num());
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		FMember M;
		M.Mesh = Rng.RandRange(0, ISMs.Num() - 1);
		M.Islet = Islet.IsValidIndex(i) ? Islet[i] : 0;
		M.Base = Seats[i];
		M.Phase = Rng.FRandRange(0.f, 2.f * PI);
		M.Energy = Rng.FRandRange(0.6f, 1.25f);
		M.Scale = Rng.FRandRange(0.85f, 1.12f);
		M.bSpinner = Rng.FRand() < 0.18f;
		M.Ang = FMath::Atan2(Seats[i].Y, Seats[i].X);
		float Best = 1.e9f;
		for (AZoogiArenaPiece* S : Stands)
		{
			const float D = FVector::Dist2D(S->GetComponentsBoundingBox().GetCenter(), Seats[i]);
			if (D < Best) { Best = D; M.Stand = S; }
		}
		M.Inst = ISMs[M.Mesh]->AddInstance(FTransform(FRotator::ZeroRotator, M.Base, FVector(MeshScale[M.Mesh] * M.Scale)), true);
		Members.Add(M);
	}
	for (int32 m = 0; m < ISMs.Num(); ++m) Xf[m].SetNum(ISMs[m]->GetInstanceCount());
	FinishBuild();
	UE_LOG(LogZoogi, Log, TEXT("CROWD: %d old-style Zoogi fans on %d stands (%d meshes)"), Members.Num(), Stands.Num(), ISMs.Num());
}

void AZoogiFrozenCrowd::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Members.Num() == 0) return;
	if (bToon) { TickToon(Dt); return; } // b228
	Clock3 += Dt;
	const float Tm = Clock3;
	const float WaveSpeed = 3.2f; // rad/s around the arena
	const float WaveFront = WaveA > -100.f ? WaveA : -1000.f;
	for (FMember& M : Members)
	{
		FVector P = M.Base;
		const FVector ToLook = Look - M.Base;
		float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToLook.Y, ToLook.X));
		float Pitch = 0.f, Roll = 0.f;
		FVector S(M.Scale);
		const float Ph = M.Phase, E = M.Energy;
		// breathing + a little sway (always)
		P.Z += 2.5f * FMath::Sin(Tm * 1.7f + Ph);
		Yaw += 7.f * FMath::Sin(Tm * 0.8f + Ph * 1.3f);
		// hype while a shot rolls: bouncing on the seat
		if (Hype > 0.01f) P.Z += 7.f * Hype * E * FMath::Abs(FMath::Sin(Tm * 6.5f + Ph));
		// cheer: hops, wiggle, squash and stretch; spinners turn full circles
		if (Cheer > 0.01f)
		{
			const float Hp = FMath::Abs(FMath::Sin(Tm * 9.5f * (0.85f + 0.3f * E) + Ph));
			P.Z += 42.f * Cheer * E * Hp;
			Yaw += 22.f * Cheer * FMath::Sin(Tm * 13.f + Ph);
			if (M.bSpinner) Yaw += Cheer * FMath::Fmod(Tm * 520.f + Ph * 57.f, 360.f);
			const float Sq = 0.16f * Cheer * FMath::Sin(Tm * 19.f + Ph);
			S *= FVector(1.f - Sq * 0.5f, 1.f - Sq * 0.5f, 1.f + Sq);
		}
		// gasp: jolt back, lean away, shiver
		if (Gasp > 0.01f)
		{
			const FVector Back = FVector(-ToLook.X, -ToLook.Y, 0.f).GetSafeNormal();
			P += Back * 16.f * Gasp;
			Pitch -= 16.f * Gasp;
			Yaw += 9.f * Gasp * FMath::Sin(Tm * 38.f + Ph);
			Roll += 5.f * Gasp * FMath::Sin(Tm * 31.f + Ph);
		}
		// stadium wave
		if (WaveFront > -100.f)
		{
			float D = FMath::Fmod(M.Ang - WaveFront, 2.f * PI);
			if (D > PI) D -= 2.f * PI; if (D < -PI) D += 2.f * PI;
			const float W = FMath::Exp(-D * D / (2.f * 0.12f * 0.12f));
			P.Z += 48.f * W;
			S.Z *= 1.f + 0.22f * W;
			Pitch += 10.f * W;
		}
		// a broken stand section drops its fans
		if (M.Stand.IsValid() && M.Stand->IsBroken())
		{
			if (M.Fall < 0.f) M.Fall = 0.f;
			M.Fall += Dt;
			P.Z -= 0.5f * 980.f * M.Fall * M.Fall;
			Roll += M.Fall * 400.f;
			if (M.Fall > 1.2f) S = FVector(0.001f);
		}
		else M.Fall = -1.f;
		const FRotator R(Pitch, Yaw, Roll);
		const FVector Sc = S * MeshScale[M.Mesh];
		const FVector Off = R.RotateVector(MeshOff[M.Mesh] * (Sc / MeshScale[M.Mesh]));
		Xf[M.Mesh][M.Inst] = FTransform(R, P + Off + FVector(0, 0, 36.f * S.Z), Sc);
	}
	for (int32 m = 0; m < ISMs.Num(); ++m)
		if (ISMs[m] && Xf[m].Num() > 0) ISMs[m]->BatchUpdateInstancesTransforms(0, Xf[m], true, true, true);
	(void)WaveSpeed;
}

// ------------------------------------------------------------------ show (fireworks + crowd driver)
void UZoogiShowSys::Push(ZoogiShow::EEvent E, const FVector& At, float Strength)
{
	using ZoogiShow::EEvent;
	EnsureTicker();
	UE_LOG(LogZoogi, Log, TEXT("SHOW event %d (%.1f)"), (int32)E, Strength);
	AZoogiFrozenCrowd* C = Crowd.Get();
	auto Roar = [&](float V) { if (CrowdSfx <= 0.f) { CrowdSfx = 5.f; ZoogiUtil::PlaySfx(GetWorld(), TEXT("CrowdWave1"), V); } };
	switch (E)
	{
	case EEvent::Score: if (C) C->Cheer = FMath::Max(C->Cheer, 0.75f); Fireworks(1); break;
	case EEvent::BigScore: if (C) C->Cheer = 1.f; Fireworks(FMath::Clamp(1 + (int32)Strength, 2, 5)); Roar(0.32f); break;
	case EEvent::RingOut: if (C) { C->Gasp = 1.f; C->Cheer = FMath::Max(C->Cheer, 0.8f); } Fireworks(3); Roar(0.38f); break;
	case EEvent::HardHit: if (C) C->Gasp = FMath::Max(C->Gasp, 0.55f * FMath::Clamp(Strength, 0.f, 1.f)); break;
	case EEvent::Blast: if (C) { C->Gasp = FMath::Max(C->Gasp, 0.6f); C->Cheer = FMath::Max(C->Cheer, 0.4f); } break;
	case EEvent::Snowman: if (C) { C->Gasp = FMath::Max(C->Gasp, 0.7f); C->Cheer = FMath::Max(C->Cheer, Strength > 1.f ? 1.f : 0.6f); } if (Strength > 1.f) { Fireworks(6, 1.2f); Roar(0.42f); } break;
	case EEvent::MatchStart: if (C) C->Cheer = 0.9f; Fireworks(6, 1.3f); break;
	case EEvent::Win: if (C) C->Cheer = 1.f; for (int32 k = 0; k < 5; ++k) Fireworks(3, 1.4f, 0.8f * k); Roar(0.5f); break;
	}
}

void UZoogiShowSys::Fireworks(int32 Count, float Spread, float Delay)
{
	static const bool bOn = ZoogiShow::On(TEXT("ZoogiNoFireworks"));
	if (!bOn) return;
	for (int32 i = 0; i < Count; ++i)
	{
		FPending P; P.Wait = Delay + i * FMath::FRandRange(0.08f, 0.22f); P.Spread = Spread;
		Pending.Add(P);
	}
}

void UZoogiShowSys::Launch(float Spread)
{
	UWorld* W = GetWorld();
	if (!W) return;
	EnsureTicker();
	if (!Holder) return;
	if (Rockets.Num() > 18) return;
	static const FLinearColor Cols[] = { FLinearColor(1.f, 0.3f, 0.25f), FLinearColor(0.3f, 0.75f, 1.f), FLinearColor(1.f, 0.82f, 0.2f), FLinearColor(0.45f, 1.f, 0.45f), FLinearColor(0.8f, 0.4f, 1.f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 0.55f, 0.15f) };
	FRocket R;
	R.Col = Cols[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Cols) - 1)];
	const AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
	const float KR = (G && G->Arena) ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	const float A = FMath::FRandRange(0.f, 2.f * PI);
	const bool bLow = FMath::FRand() < 0.4f;
	const float Rr = bLow ? KR + FMath::FRandRange(250.f, 700.f) : KR + FMath::FRandRange(800.f, 1900.f) * Spread;
	R.P = FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, bLow ? 0.f : -150.f);
	const FVector In = -FVector(FMath::Cos(A), FMath::Sin(A), 0.f);
	R.V = FVector(0, 0, bLow ? FMath::FRandRange(1250.f, 1500.f) : FMath::FRandRange(1750.f, 2250.f)) + In * FMath::FRandRange(60.f, 220.f) + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 0.f);
	R.Fuse = bLow ? FMath::FRandRange(0.6f, 0.8f) : FMath::FRandRange(0.95f, 1.25f);
	UStaticMeshComponent* C = nullptr;
	for (UStaticMeshComponent* P : Pool) if (P && !P->IsVisible()) { C = P; break; }
	if (!C)
	{
		C = NewObject<UStaticMeshComponent>(Holder);
		C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(Holder->GetRootComponent());
		C->RegisterComponent();
		C->SetWorldScale3D(FVector(0.22f));
		Pool.Add(C);
	}
	C->SetMaterial(0, ZoogiUtil::GlowMID(C, R.Col, 10.f));
	C->SetWorldLocation(R.P);
	C->SetVisibility(true);
	R.C = C;
	Rockets.Add(R);
	ZoogiUtil::PlaySfx(W, TEXT("AwakenCharge"), 0.12f, 2.2f);
}

void UZoogiShowSys::Poll(AZoogiGameMode* G, float Dt)
{
	using ZoogiShow::EEvent;
	// scores / ring-outs (counted from the live actors, so every mode feeds it)
	int32 OrbsOut = 0, BallsOut = 0, StarsOut = 0;
	for (const AZoogiOrb* O : G->Orbs) if (O && O->bOut) { ++OrbsOut; if (O->bStar) ++StarsOut; }
	for (const AZoogiBall* B : G->Balls) if (B && B->bOut) ++BallsOut;
	if (SeenOrbsOut >= 0 && OrbsOut > SeenOrbsOut)
		for (int32 k = SeenOrbsOut; k < OrbsOut; ++k) Push(StarsOut > 0 && k == OrbsOut - 1 ? EEvent::BigScore : EEvent::Score, FVector::ZeroVector, 1.f);
	if (SeenBallsOut >= 0 && BallsOut > SeenBallsOut) Push(EEvent::RingOut, FVector::ZeroVector, 1.f);
	SeenOrbsOut = OrbsOut; SeenBallsOut = BallsOut;
	const int32 Streak = ZoogiScoringUI::ComboStreak();
	if (Streak >= 3 && Streak > SeenStreak) Push(EEvent::BigScore, FVector::ZeroVector, (float)Streak);
	SeenStreak = Streak;
	// match start (the stage has formed) / the win
	const int32 Ph = (int32)G->Phase;
	if (SeenPhase == (int32)EZoogiPhase::Forming && Ph != (int32)EZoogiPhase::Forming && Ph != (int32)EZoogiPhase::Menu) Push(EEvent::MatchStart, FVector::ZeroVector, 1.f);
	if (G->Phase == EZoogiPhase::GameOver) { if (!bWinShown) { bWinShown = true; Push(EEvent::Win, FVector::ZeroVector, 1.f); } }
	else bWinShown = false;
	SeenPhase = Ph;
	// ambient rockets while the match is live
	if (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling)
	{
		Ambient -= Dt;
		if (Ambient <= 0.f) { Ambient = FMath::FRandRange(7.f, 12.f); Fireworks(FMath::RandRange(1, 2)); }
	}
	// crowd drivers
	if (AZoogiFrozenCrowd* C = Crowd.Get())
	{
		const AZoogiBall* Sh = G->GetActiveBall();
		FVector Want = Sh ? Sh->BallLocation() : FVector::ZeroVector;
		float Hype = 0.f;
		if (G->Phase == EZoogiPhase::Rolling)
		{
			float Best = 0.f;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) { const float V = B->GetBallVelocity().Size2D(); if (V > Best) { Best = V; Want = B->BallLocation(); } }
			for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float V = O->GetVelocity().Size2D(); if (V > Best * 1.2f) { Best = V; Want = O->OrbLocation(); } }
			Hype = FMath::Clamp(Best / 1400.f, 0.f, 1.f);
		}
		C->Look = FMath::Lerp(C->Look, Want, 1.f - FMath::Exp(-4.f * Dt));
		C->Hype = FMath::Lerp(C->Hype, Hype, 1.f - FMath::Exp(-3.f * Dt));
		C->Cheer = FMath::Max(0.f, C->Cheer - Dt * 0.55f);
		C->Gasp = FMath::Max(0.f, C->Gasp - Dt * 1.1f);
		// a stadium wave every ~25 s between shots
		static float WaveT = 12.f;
		if (C->WaveA > -100.f) { C->WaveA += Dt * 3.2f; if (C->WaveA > 2.f * PI * 1.5f) C->WaveA = -1000.f; }
		else if (G->Phase == EZoogiPhase::Aiming) { WaveT -= Dt; if (WaveT <= 0.f) { WaveT = FMath::FRandRange(20.f, 30.f); C->WaveA = FMath::FRandRange(0.f, 2.f * PI) - 2.f * PI; } }
	}
}

AZoogiShowTicker::AZoogiShowTicker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	USceneComponent* R = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	R->SetMobility(EComponentMobility::Movable);
	RootComponent = R;
}

void AZoogiShowTicker::Tick(float Dt)
{
	Super::Tick(Dt);
	if (UWorld* W = GetWorld()) if (UZoogiShowSys* S = W->GetSubsystem<UZoogiShowSys>()) S->ShowTick(Dt);
	if (UWorld* W = GetWorld()) if (UZoogiFunSys* F = W->GetSubsystem<UZoogiFunSys>()) F->FunTick(Dt); // build 23 action / fun pack
}

void UZoogiShowSys::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnsureTicker();
}

void UZoogiShowSys::EnsureTicker()
{
	UWorld* W = GetWorld();
	if (Holder || !W || !W->IsGameWorld()) return;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Holder = W->SpawnActor<AZoogiShowTicker>(AZoogiShowTicker::StaticClass(), FTransform::Identity, SP);
	UE_LOG(LogZoogi, Log, TEXT("SHOWSYS ticker %s"), Holder ? TEXT("up") : TEXT("FAILED"));
}

void UZoogiShowSys::ShowTick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W || W->bIsTearingDown) return;
	Clock += Dt;
	CrowdSfx = FMath::Max(0.f, CrowdSfx - Dt);
	if (AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>()) { Poll(G, Dt); TickTest(G, Dt); }
	for (int32 i = Pending.Num() - 1; i >= 0; --i)
	{
		Pending[i].Wait -= Dt;
		if (Pending[i].Wait <= 0.f) { const float S = Pending[i].Spread; Pending.RemoveAt(i); Launch(S); }
	}
	static const TCHAR* Bursts[] = { TEXT("NS_Burst_Red"), TEXT("NS_Burst_Blue"), TEXT("NS_Burst_Gold"), TEXT("NS_Burst_Green"), TEXT("NS_Burst_Purple"), TEXT("NS_Burst_White"), TEXT("NS_Burst_Yellow") };
	static double LastPop = 0.0;
	TArray<FRocket> Crackles;
	for (int32 i = Rockets.Num() - 1; i >= 0; --i)
	{
		FRocket& R = Rockets[i];
		R.Age += Dt;
		if (R.Burst == 2)
		{
			// crackle: small white pops around the main burst
			if (R.Age >= R.Wait)
			{
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), R.P, FRotator::ZeroRotator, 1.2f);
				Burst(R.P, FLinearColor(1.f, 0.95f, 0.8f), 10, 300.f);
				Rockets.RemoveAt(i);
			}
			continue;
		}
		R.V.Z -= 980.f * Dt;
		R.P += R.V * Dt;
		if (UStaticMeshComponent* C = R.C.Get()) C->SetWorldLocation(R.P);
		R.Trail -= Dt;
		if (R.Trail <= 0.f) { R.Trail = 0.045f; AZoogiFX::SpawnTrail(W, R.P, R.Col, 26.f, 0.5f, 5.f); }
		if (R.Age >= R.Fuse)
		{
			int32 Bi = 5;
			if (R.Col.R > 0.9f && R.Col.G < 0.4f) Bi = 0; else if (R.Col.B > 0.9f && R.Col.R < 0.4f) Bi = 1; else if (R.Col.G > 0.75f && R.Col.R > 0.9f && R.Col.B < 0.3f) Bi = 2;
			else if (R.Col.G > 0.9f && R.Col.R < 0.5f) Bi = 3; else if (R.Col.B > 0.9f && R.Col.R > 0.7f) Bi = 4; else if (R.Col.R > 0.9f && R.Col.G > 0.5f && R.Col.G < 0.7f) Bi = 6;
			// real firework: a shell of glowing spark streaks (gravity + drag), a bright core flash, Niagara sparkle, confetti
			Burst(R.P, R.Col, 34, FMath::FRandRange(620.f, 820.f));
			AZoogiFX::Niagara(W, Bursts[Bi], R.P, FRotator::ZeroRotator, 4.5f);
			AZoogiFX::Spawn(W, R.P, FLinearColor(1.f, 1.f, 1.f), 120.f, 0.18f, false, 16.f);
			AZoogiFX::Spawn(W, R.P, R.Col, 220.f, 0.35f, false, 6.f);
			AZoogiFX::SpawnConfetti(W, R.P, R.Col, 24);
			const double Now = W->GetRealTimeSeconds();
			if (Now - LastPop > 0.12) { LastPop = Now; ZoogiUtil::PlaySfx(W, TEXT("CritBoom"), 0.2f, 1.75f + FMath::FRandRange(-0.15f, 0.15f)); }
			for (int32 k = 0; k < 3; ++k)
			{
				FRocket Cr; Cr.Burst = 2; Cr.P = R.P + FMath::VRand() * FMath::FRandRange(180.f, 320.f); Cr.Wait = 0.28f + 0.09f * k; Cr.Col = R.Col;
				Crackles.Add(Cr);
			}
			if (UStaticMeshComponent* C = R.C.Get()) C->SetVisibility(false);
			Rockets.RemoveAt(i);
		}
	}
	Rockets.Append(Crackles);
	// spark streaks
	for (int32 i = Sparks.Num() - 1; i >= 0; --i)
	{
		FSpark& S = Sparks[i];
		S.Age += Dt;
		UStaticMeshComponent* C = S.C.Get();
		if (!C || S.Age >= S.Life) { if (C) C->SetVisibility(false); Sparks.RemoveAtSwap(i); continue; }
		S.V *= FMath::Exp(-1.6f * Dt);
		S.V.Z -= 420.f * Dt;
		S.P += S.V * Dt;
		const float K = FMath::Clamp(1.f - (S.Age / S.Life - 0.55f) / 0.45f, 0.f, 1.f);
		const float Len = (0.15f + S.V.Size() * 0.0009f) * S.Size;
		C->SetWorldLocationAndRotation(S.P, FRotationMatrix::MakeFromX(S.V.GetSafeNormal()).Rotator());
		C->SetWorldScale3D(FVector(Len, 0.12f * S.Size, 0.12f * S.Size) * FMath::Max(K, 0.01f));
	}
}

void UZoogiShowSys::Burst(const FVector& P, const FLinearColor& Col, int32 N, float Speed)
{
	if (!Holder || Sparks.Num() > 220 || bQuiet) return;
	UMaterialInstanceDynamic* M = ZoogiUtil::GlowMID(Holder, Col, 14.f);
	for (int32 i = 0; i < N; ++i)
	{
		UStaticMeshComponent* C = nullptr;
		for (UStaticMeshComponent* X : SparkPool) if (X && !X->IsVisible()) { C = X; break; }
		if (!C)
		{
			C = NewObject<UStaticMeshComponent>(Holder);
			C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->SetupAttachment(Holder->GetRootComponent());
			C->RegisterComponent();
			SparkPool.Add(C);
		}
		C->SetMaterial(0, M);
		C->SetVisibility(true);
		FSpark S; S.C = C; S.P = P;
		S.V = FMath::VRand() * Speed * FMath::FRandRange(0.75f, 1.05f);
		S.Life = FMath::FRandRange(1.0f, 1.5f);
		S.Size = FMath::FRandRange(0.8f, 1.2f);
		C->SetWorldLocation(P);
		Sparks.Add(S);
	}
}

// -ZoogiShowTest (+ -ZoogiShots=dir): scripted captures of the snowman, igloo blocks, crowd, fireworks and an impact frame
void UZoogiShowSys::TickTest(AZoogiGameMode* G, float Dt)
{
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowTest"));
	if (!bTest || G->ShotDir.IsEmpty() || TestStep < 0) return;
	if (TestT < 0.f) { if (G->Phase == EZoogiPhase::Aiming) TestT = 0.f; else return; }
	TestT += Dt;
	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC) return;
	auto View = [&](const FVector& Eye, const FVector& At)
	{
		ACameraActor* C = Cast<ACameraActor>(TestCam.Get());
		if (!C) { C = W->SpawnActor<ACameraActor>(Eye, (At - Eye).Rotation()); TestCam = C; PrevView = PC->GetViewTarget(); }
		if (!C) return;
		C->SetActorLocationAndRotation(Eye, (At - Eye).Rotation());
		PC->SetViewTarget(C);
	};
	auto Shot = [&](const TCHAR* N)
	{
		const FString F = FPaths::Combine(G->ShotDir, FString::Printf(TEXT("show_%02d_%s.png"), TestStep, N));
		FScreenshotRequest::RequestScreenshot(F, true, false);
		UE_LOG(LogZoogi, Log, TEXT("SHOWTEST shot %s"), *F);
	};
	AZoogiSnowman* Sn = nullptr; AZoogiIglooWall* Ig = nullptr;
	for (TActorIterator<AZoogiSnowman> It(W); It; ++It) Sn = *It;
	float BestIg = 1.e9f;
	for (TActorIterator<AZoogiIglooWall> It(W); It; ++It)
	{
		const FVector C = It->GetComponentsBoundingBox().GetCenter();
		const float A = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(C.Y, C.X)), 100.f));
		if (A < BestIg) { BestIg = A; Ig = *It; }
	}
	struct FStep { float At; int32 Kind; };
	// kinds: 0 snowman view, 1 knock, 2 shot, 3 igloo view, 4 igloo hit, 5 crowd view, 6 cheer, 7 gasp, 8 wave, 9 sky view, 10 fireworks, 11 restore, 12 impact frame, 13 end
	static const FStep Steps[] = {
		{ 2.0f, 0 }, { 2.4f, 2 }, { 3.0f, 1 }, { 3.35f, 2 }, { 4.2f, 1 }, { 4.55f, 2 }, { 5.4f, 1 }, { 5.75f, 2 }, { 6.6f, 1 }, { 6.75f, 2 }, { 7.4f, 2 },
		{ 8.4f, 3 }, { 8.8f, 2 }, { 9.2f, 4 }, { 9.42f, 2 }, { 10.4f, 4 }, { 10.62f, 2 }, { 11.8f, 2 },
		{ 12.6f, 5 }, { 12.9f, 2 }, { 13.2f, 6 }, { 13.55f, 2 }, { 14.4f, 7 }, { 14.6f, 2 }, { 15.6f, 8 }, { 16.5f, 2 },
		{ 17.4f, 9 }, { 17.6f, 10 }, { 18.9f, 2 }, { 19.4f, 2 },
		{ 20.2f, 11 }, { 20.6f, 12 }, { 20.68f, 2 }, { 20.85f, 2 }, { 21.5f, 13 } };
	static const TCHAR* Names[] = { TEXT("snowman_whole"), TEXT("snowman_arm1"), TEXT("snowman_arm2"), TEXT("snowman_head"), TEXT("snowman_boom"), TEXT("snowman_after"),
		TEXT("igloo_whole"), TEXT("igloo_top_blown"), TEXT("igloo_open"), TEXT("igloo_after"), TEXT("crowd_idle"), TEXT("crowd_cheer"), TEXT("crowd_gasp"), TEXT("crowd_wave"),
		TEXT("fireworks_1"), TEXT("fireworks_2"), TEXT("impactframe_1"), TEXT("impactframe_2") };
	static int32 ShotN = 0;
	if (TestStep >= (int32)UE_ARRAY_COUNT(Steps)) { TestStep = -1; return; }
	const FStep& S = Steps[TestStep];
	if (TestT < S.At) return;
	AZoogiFrozenCrowd* Cr = Crowd.Get();
	switch (S.Kind)
	{
	case 0: if (Sn) { const FVector P = Sn->GetActorLocation(); const FVector In = (-P).GetSafeNormal2D(); View(P + In * 760.f + FVector(0, 0, 260.f), P + FVector(0, 0, 160.f)); } break;
	case 1: if (Sn) Sn->KnockNext((Sn->GetActorLocation()).GetSafeNormal2D() + FVector(0.f, 0.3f, 0.f)); break;
	case 2: Shot(Names[FMath::Min(ShotN++, (int32)UE_ARRAY_COUNT(Names) - 1)]); break;
	case 3: if (Ig) { const FVector C = Ig->GetComponentsBoundingBox().GetCenter(); const FVector In = (-C).GetSafeNormal2D(); View(C + In * 620.f + FVector(0, 0, 230.f), C + FVector(0, 0, 40.f)); } break;
	case 4: if (Ig) { const FVector C = Ig->GetComponentsBoundingBox().GetCenter(); Ig->ApplyDamage(30.f, C, C.GetSafeNormal2D(), EZoogiArtifact::None, nullptr); } break;
	case 5:
	{
		// the yaw-0 islet's bleachers from the court edge
		const FVector St(2750.f, 0.f, 120.f);
		View(FVector(1200.f, -250.f, 420.f), St);
		if (Cr) Cr->Look = FVector(1200.f, -250.f, 0.f);
		break;
	}
	case 6: if (Cr) Cr->Cheer = 1.f; break;
	case 7: if (Cr) { Cr->Cheer = 0.f; Cr->Gasp = 1.f; } break;
	case 8: if (Cr) { Cr->Gasp = 0.f; Cr->WaveA = -0.9f; } break;
	case 9: View(FVector(-900.f, 2200.f, 700.f), FVector(1500.f, -2600.f, 1100.f)); break;
	case 10: Fireworks(10, 1.f); break;
	case 11: if (PrevView.IsValid()) PC->SetViewTarget(PrevView.Get()); break;
	case 12: if (const AZoogiBall* B = G->GetActiveBall()) ZoogiShow::ImpactFrame(W, B->BallLocation(), 3); break;
	case 13: if (TestCam.IsValid()) TestCam->Destroy(); break;
	}
	++TestStep;
}
