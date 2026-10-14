#include "ZoogiShotTest.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "UnrealClient.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"


/** Draws a render target ignoring its alpha channel (scene-capture alpha is not opacity). Debug overlay only. */
class SZoogiShotRTView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiShotRTView) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	void SetBrush(const FSlateBrush* B) { Brush = B; Invalidate(EInvalidateWidgetReason::Paint); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (Brush) FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), Brush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
		return Layer;
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(480.f, 270.f); }
private:
	const FSlateBrush* Brush = nullptr;
};

namespace ZoogiShotTestPrivate
{
	// Survive the level-end case (the world is re-created; the next driver prints the summary and quits).
	int32 GShotTestPass = 0, GShotTestFail = 0;
	bool GLevelEndPending = false;
	bool GTestFinished = false;
	int32 GShotSeq = 0;
	FZoogiCameraSnapshot GLevelEndSnap;
	FString GLevelEndName;
	TArray<FString> GShotTestResults;

	float ShotTestPOVDev(const FMinimalViewInfo& A, const FMinimalViewInfo& B)
	{
		const float DLoc = FVector::Dist(A.Location, B.Location);
		const float DRot = FMath::RadiansToDegrees(A.Rotation.Quaternion().AngularDistance(B.Rotation.Quaternion()));
		return FMath::Max3(DLoc, DRot, FMath::Abs(A.FOV - B.FOV));
	}
}
using namespace ZoogiShotTestPrivate;

AZoogiShotTestDriver::AZoogiShotTestDriver()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorEnableCollision(false);
}

AZoogiGameMode* AZoogiShotTestDriver::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

UZoogiEpisodeCameraDirector* AZoogiShotTestDriver::Dir() const
{
	return UZoogiEpisodeCameraDirector::Get(this);
}

void AZoogiShotTestDriver::BeginPlay()
{
	Super::BeginPlay();
	FString DirArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiShotTestDir="), DirArg)) ShotDir = DirArg;
	else ShotDir = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE")), TEXT("Pictures"), TEXT("ZoogiShotTest"));
	IFileManager::Get().MakeDirectory(*ShotDir, true);
	ShotSeq = GShotSeq;
	BuildCases();
	BuildOverlay();
	if (GTestFinished)
	{
		Step = ZoogiShotTest::EStep::Summary;
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST new world after the level-end case"));
		return;
	}
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST start: %d cases, screenshots -> %s, arena %s"), Cases.Num(), *ShotDir, FCommandLine::Get());
	if (UZoogiEpisodeCameraDirector* D = Dir())
	{
		FinishedHandle = D->OnShotFinished.AddWeakLambda(this, [this](FZoogiShotHandle H, EZoogiShotEndReason R)
		{
			static const TCHAR* N[] = { TEXT("Completed"), TEXT("Stopped"), TEXT("Replaced"), TEXT("FocusLost"), TEXT("WorldEnding") };
			FinishLog.Add(FString::Printf(TEXT("%d:%s"), H.Id, N[FMath::Clamp((int32)R, 0, 4)]));
		});
		// Level-end case: verify at the exact moment the director has finished restoring (before any actor ends play,
		// while the player controller and the gameplay camera still exist).
		TearDownHandle = D->OnWorldTearDownRestored.AddWeakLambda(this, [this]()
		{
			if (GLevelEndPending) CheckLevelEnd(TEXT("checked at world tear-down right after the director restored"));
		});
	}
}

void AZoogiShotTestDriver::CheckLevelEnd(const TCHAR* When)
{
	GLevelEndPending = false;
	GTestFinished = true;
	UZoogiEpisodeCameraDirector* D = Dir();
	FZoogiCameraSnapshot Snap1 = D ? D->CaptureGameplaySnapshot() : FZoogiCameraSnapshot();
	AActor* VT = Snap1.ViewTarget.Get();
	// The camera manager's POV cache still holds the last rendered (shot) frame until its next update, which never comes
	// in a dying world; compare the view the restored view target produces right now instead.
	if (IsValid(VT))
	{
		FMinimalViewInfo V;
		VT->CalcCamera(0.f, V);
		Snap1.POV.Location = V.Location;
		Snap1.POV.Rotation = V.Rotation;
		Snap1.POV.FOV = V.FOV;
	}
	FString Report;
	bool bPass = UZoogiEpisodeCameraDirector::CompareSnapshots(GLevelEndSnap, Snap1, FZoogiCameraTolerance(), Report);
	const bool bIdle = D && D->NumActiveShots() == 0 && !D->IsTimeScaleActive() && !D->IsMainViewTaken();
	const bool bShotCamVT = D && D->GetShotCamera() && VT == D->GetShotCamera();
	bPass &= bIdle && !bShotCamVT;
	Result(bPass, FString::Printf(TEXT("[%s] camera restore on level end (MainView TopDown + RenderTarget CloseUp + time-scale ease, then OpenLevel)"), *GLevelEndName),
		FString::Printf(TEXT("%s | %s | cinematic dilation %.6f (prior %.6f) | director idle=%d"), *Report, When, Snap1.CinematicTimeDilation, GLevelEndSnap.CinematicTimeDilation, bIdle ? 1 : 0));
}

void AZoogiShotTestDriver::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GLevelEndPending) CheckLevelEnd(TEXT("checked at driver EndPlay (the tear-down hook did not fire)"));
	if (UZoogiEpisodeCameraDirector* D = Dir())
	{
		D->OnShotFinished.Remove(FinishedHandle);
		D->OnWorldTearDownRestored.Remove(TearDownHandle);
	}
	if (Overlay.IsValid() && GEngine && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
	Super::EndPlay(Reason);
}

void AZoogiShotTestDriver::BuildCases()
{
	Cases.Reset();
	int32 N = 0;
	for (EZoogiShotType T : ZoogiShot::AllTypes())
	{
		for (EZoogiShotOutput O : { EZoogiShotOutput::RenderTarget, EZoogiShotOutput::MainView })
		{
			ZoogiShotTest::FCase C; C.Kind = ZoogiShotTest::ECaseKind::Shot; C.Type = T; C.Output = O;
			C.Name = FString::Printf(TEXT("%02d_%s_%s"), ++N, ZoogiShot::TypeName(T), ZoogiShot::OutputName(O));
			Cases.Add(C);
		}
	}
	auto Add = [&](ZoogiShotTest::ECaseKind K, const TCHAR* Name, EZoogiShotType T, EZoogiShotOutput O)
	{
		ZoogiShotTest::FCase C; C.Kind = K; C.Type = T; C.Output = O; C.Name = FString::Printf(TEXT("%02d_%s"), ++N, Name); Cases.Add(C);
	};
	Add(ZoogiShotTest::ECaseKind::Cancel, TEXT("Cancel_TopDown_MainView"), EZoogiShotType::TopDownImpact, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::StopEased, TEXT("StopEased_CloseUp_MainView"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::Overlap, TEXT("Overlap_CloseUp_TwoShot_plus_RT"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::FocusDestroyed, TEXT("FocusDestroyed_MainView_plus_RT"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::TimeScaleRefused, TEXT("TimeScale_PolicyDisabled"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::TimeScale, TEXT("TimeScale_NeverDilates_with_LowAngle_MainView"), EZoogiShotType::LowAngleHero, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::TimeScaleCancel, TEXT("TimeScale_NeverDilates_Cancel"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AirborneSlowMoCancel, TEXT("Airborne_SlowLook_Cancel_MainView"), EZoogiShotType::Airborne, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AirborneSlowMoRefused, TEXT("Airborne_SlowLook_PolicyDisabled_RenderTarget"), EZoogiShotType::Airborne, EZoogiShotOutput::RenderTarget);
	Add(ZoogiShotTest::ECaseKind::AvoidParticles, TEXT("AvoidParticles_Tracking_MainView"), EZoogiShotType::Tracking, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AvoidOcclusion, TEXT("AvoidOcclusion_CloseUp_RenderTarget"), EZoogiShotType::CloseUp, EZoogiShotOutput::RenderTarget);
	Add(ZoogiShotTest::ECaseKind::LevelEnd, TEXT("LevelEnd"), EZoogiShotType::TopDownImpact, EZoogiShotOutput::MainView);
}

// ------------------------------------------------------------------ overlay (debug only, not ZoogiHUD)

void AZoogiShotTestDriver::BuildOverlay()
{
	UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!VC) return;
	TSharedRef<SOverlay> O = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 96.f, 20.f, 0.f))
		[
			SAssignNew(PanelBox, SBox).WidthOverride(488.f).HeightOverride(306.f).Visibility(EVisibility::Collapsed)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f)).Padding(4.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 0.f, 2.f, 3.f)
					[
						SAssignNew(PanelCaption, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f))
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(PanelView, SZoogiShotRTView)
					]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(16.f, 0.f, 0.f, 12.f))
		[
			SAssignNew(StatusText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.9f)).ShadowOffset(FVector2D(1.f, 1.f))
		];
	Overlay = O;
	VC->AddViewportWidgetContent(O, 50);
}

void AZoogiShotTestDriver::ShowPanel(UTextureRenderTarget2D* RT, const FString& Caption)
{
	if (!RT || !PanelBox.IsValid()) return;
	KeepTargets.AddUnique(RT);
	TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
	B->SetResourceObject(RT);
	B->ImageSize = FVector2D(RT->SizeX, RT->SizeY);
	B->DrawAs = ESlateBrushDrawType::Image;
	Brushes.Add(B);
	PanelView->SetBrush(B.Get());
	PanelCaption->SetText(FText::FromString(Caption));
	PanelBox->SetVisibility(EVisibility::HitTestInvisible);
}

void AZoogiShotTestDriver::HidePanel()
{
	if (PanelBox.IsValid()) PanelBox->SetVisibility(EVisibility::Collapsed);
}

void AZoogiShotTestDriver::SetStatus(const FString& S)
{
	if (StatusText.IsValid()) StatusText->SetText(FText::FromString(S));
}

// ------------------------------------------------------------------ screenshots

void AZoogiShotTestDriver::Shot(const FString& Name)
{
	PendingShots.Add(FPaths::Combine(ShotDir, FString::Printf(TEXT("%02d_%s.png"), ShotSeq++, *Name)));
	GShotSeq = ShotSeq;
	PumpScreenshots();
}

void AZoogiShotTestDriver::PumpScreenshots()
{
	if (PendingShots.Num() == 0 || FScreenshotRequest::IsScreenshotRequested()) return;
	const FString Path = PendingShots[0];
	PendingShots.RemoveAt(0);
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST screenshot %s"), *Path);
}

void AZoogiShotTestDriver::ExportRT(UTextureRenderTarget2D* RT, const FString& Name)
{
	FTextureRenderTargetResource* Res = RT ? RT->GameThread_GetRenderTargetResource() : nullptr;
	if (!Res) return;
	TArray<FColor> Px;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(RT->RenderTargetFormat == RTF_RGBA16f);
	if (!Res->ReadPixels(Px, ReadFlags) || Px.Num() != RT->SizeX * RT->SizeY) return;
	double R = 0, G = 0, B = 0, A = 0;
	for (FColor& C : Px) { R += C.R; G += C.G; B += C.B; A += C.A; C.A = 255; }
	const double Inv = 1.0 / FMath::Max(1, Px.Num());
	const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("%02d_%s.png"), ShotSeq++, *Name));
	GShotSeq = ShotSeq;
	FImageView View(Px.GetData(), RT->SizeX, RT->SizeY, EGammaSpace::sRGB);
	const bool bOk = FImageUtils::SaveImageByExtension(*Path, View);
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST render target %dx%d fmt=%d exported %s (%s): mean RGB %.1f %.1f %.1f, mean raw alpha %.1f"),
		RT->SizeX, RT->SizeY, (int32)RT->RenderTargetFormat, *Path, bOk ? TEXT("ok") : TEXT("FAILED"), R * Inv, G * Inv, B * Inv, A * Inv);
}

// ------------------------------------------------------------------ helpers

AActor* AZoogiShotTestDriver::Focus() const
{
	AZoogiGameMode* G = GM();
	return G ? G->GetActiveBall() : nullptr;
}

AActor* AZoogiShotTestDriver::SecondaryFor(AActor* F) const
{
	AZoogiGameMode* G = GM();
	if (!G || !F) return nullptr;
	AActor* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B == F || B->bOut) continue;
		const float D = FVector::Dist(B->BallLocation(), F->GetActorLocation());
		if (D < BestD) { BestD = D; Best = B; }
	}
	if (!Best)
	{
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { Best = O; break; }
	}
	return Best;
}

AStaticMeshActor* AZoogiShotTestDriver::SpawnProp(const FVector& At, float Scale)
{
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, At, FVector(Scale)), P);
	if (!A) return nullptr;
	UStaticMeshComponent* M = A->GetStaticMeshComponent();
	M->SetMobility(EComponentMobility::Movable);
	M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	// Test prop only: no collision, no physics, so it cannot touch the match.
	M->SetSimulatePhysics(false);
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->SetGenerateOverlapEvents(false);
	M->SetCastShadow(true);
	return A;
}

void AZoogiShotTestDriver::TintProp(AStaticMeshActor* A, const FLinearColor& C)
{
	if (!A) return;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base) return;
	UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(Base, A);
	MI->SetVectorParameterValue(TEXT("Color"), C);
	A->GetStaticMeshComponent()->SetMaterial(0, MI);
}

void AZoogiShotTestDriver::StartArc(const FVector& From)
{
	if (!ArcProp) return;
	// Across the player's view so the flight reads side-on in the gameplay framing too.
	const FVector Side = FRotator(0.f, Snap0.POV.Rotation.Yaw + 90.f, 0.f).Vector();
	ArcStart = From - Side * 260.f + FVector(0.f, 0.f, 10.f);
	ArcVel = Side * 430.f + FVector(0.f, 0.f, 820.f);
	ArcTime = 0.f;
	MaxArcHeight = 0.f;
	ArcProp->SetActorLocation(ArcStart);
	ArcProp->SetActorHiddenInGame(false);
}

static FVector2D RimDir(const FVector& FL)
{
	FVector2D U(FL.X, FL.Y);
	if (U.Size() < 1.f) U = FVector2D(1.f, 0.f);
	return U.GetSafeNormal();
}

FZoogiShotHandle AZoogiShotTestDriver::PlayCaseShot(EZoogiShotType Type, EZoogiShotOutput Output, FString& OutSetup)
{
	UZoogiEpisodeCameraDirector* D = Dir();
	AZoogiGameMode* G = GM();
	AActor* F = Focus();
	const FZoogiShotSpec* Lib = D->GetShotLibrary()->FindByType(Type);
	const FZoogiShotSpec Spec = Lib ? *Lib : UZoogiShotLibrary::MakeDefault(Type);
	AActor* UseF = F;
	AActor* UseS = nullptr;
	FZoogiShotPlayParams P;
	switch (Type)
	{
	case EZoogiShotType::Tracking:
		if (TrackProp)
		{
			TrackCenter = F ? F->GetActorLocation() : FVector::ZeroVector;
			TrackProp->SetActorHiddenInGame(false);
			UseF = TrackProp;
			OutSetup = TEXT("focus = test prop circling P1");
		}
		break;
	case EZoogiShotType::Airborne:
		if (ArcProp)
		{
			StartArc(F ? F->GetActorLocation() : FVector::ZeroVector);
			UseF = ArcProp;
			OutSetup = TEXT("focus = kinematic test prop on a ballistic arc (game time, so slow-mo slows it)");
		}
		break;
	case EZoogiShotType::TrickAllyBump:
		if (F)
		{
			// The bumped ally right at contact: a Zoogi-sized prop touching P1 (centres 100 cm apart), off the view axis.
			const FVector FL = F->GetActorLocation();
			const FVector Side = FRotator(0.f, Snap0.POV.Rotation.Yaw + 60.f, 0.f).Vector();
			if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
			TempProp = SpawnProp(FL + Side * 100.f, 1.f);
			TintProp(TempProp, FLinearColor(1.f, 0.55f, 0.75f));
			UseS = TempProp;
			if (UseS) { P.bHasEventLocation = true; P.EventLocation = FL + Side * 50.f; }
			OutSetup = FString::Printf(TEXT("ally = Zoogi-sized prop touching P1 at %s, contact = %s"), *(FL + Side * 100.f).ToString(), *P.EventLocation.ToString());
		}
		break;
	case EZoogiShotType::PartnerTwoShot:
		UseS = SecondaryFor(F);
		OutSetup = FString::Printf(TEXT("secondary = %s"), *GetNameSafe(UseS));
		break;
	case EZoogiShotType::TrickOrbRelay:
	{
		// Orb = the relayed orb nearest P1; receiver = the Zoogi nearest that orb; orb path = orb -> receiver.
		float BestD = TNumericLimits<float>::Max();
		if (G && F)
		{
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut) continue;
				const float Dd = FVector::Dist(O->OrbLocation(), F->GetActorLocation());
				if (Dd < BestD) { BestD = Dd; UseS = O; }
			}
		}
		if (!UseS) UseS = SecondaryFor(F);
		AActor* Recv = nullptr;
		if (G && UseS)
		{
			float RB = TNumericLimits<float>::Max();
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B == F || B->bOut) continue;
				const float Dd = FVector::Dist(B->BallLocation(), UseS->GetActorLocation());
				if (Dd < RB) { RB = Dd; Recv = B; }
			}
		}
		if (UseS && Recv)
		{
			P.bHasEventLocation = true; P.EventLocation = Recv->GetActorLocation();
			P.bHasDirection = true; P.Direction = Recv->GetActorLocation() - UseS->GetActorLocation();
		}
		OutSetup = FString::Printf(TEXT("orb = %s, receiver = %s, orb path = %s"), *GetNameSafe(UseS), *GetNameSafe(Recv), *P.Direction.GetSafeNormal().ToString());
		break;
	}
	case EZoogiShotType::TrickBank:
	case EZoogiShotType::TrickPowerBank:
	case EZoogiShotType::TrickDoubleBank:
	case EZoogiShotType::TrickWallRedirect:
		if (F)
		{
			// Contacts on the rim wall off to the side of P1 (rim normal points at the arena centre).
			const FVector FL = F->GetActorLocation();
			// Real wall contact: trace from P1 toward the rim (visibility), ball centre one radius off the hit surface.
			auto Rim = [&](float Deg, FVector* OutN)
			{
				const FVector2D U = RimDir(FL).GetRotated(Deg);
				const FVector To(U.X * AZoogiArena::ActiveKnockoffR * 1.15f, U.Y * AZoogiArena::ActiveKnockoffR * 1.15f, FL.Z);
				FHitResult Hit;
				FCollisionQueryParams Q(TEXT("ShotTestWall"), false);
				for (TActorIterator<AZoogiBall> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
				for (TActorIterator<AZoogiOrb> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
				FVector N = FVector(-U.X, -U.Y, 0.f);
				FVector L = FVector(U.X, U.Y, 0.f) * AZoogiArena::ActiveKnockoffR * 0.92f + FVector(0, 0, FL.Z);
				if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(U.X * 300.f, U.Y * 300.f, FL.Z), To, ECC_Visibility, Q) && Hit.bBlockingHit)
				{
					N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
					if (N.IsNearlyZero()) N = FVector(-U.X, -U.Y, 0.f);
					L = Hit.ImpactPoint + N * 50.f; L.Z = FL.Z;
				}
				if (OutN) *OutN = N;
				return L;
			};
			FVector N1;
			const FVector E1 = Rim(Type == EZoogiShotType::TrickWallRedirect ? -50.f : 35.f, &N1);
			P.bHasEventLocation = true; P.EventLocation = E1;
			P.bHasEventNormal = true; P.EventNormal = N1;
			OutSetup = FString::Printf(TEXT("bank contact (rim) = %s normal = %s"), *E1.ToString(), *P.EventNormal.ToString());
			// the core's contact glow ring at the real contact (as FallbackGlow spawns it; power colour for a power bank)
			if (TempFX) { TempFX->Destroy(); TempFX = nullptr; }
			TempFX = AZoogiFX::Spawn(GetWorld(), FVector(E1.X, E1.Y, 6.f), Type == EZoogiShotType::TrickPowerBank ? FLinearColor(0.35f, 0.75f, 1.f) : FLinearColor(1.f, 0.82f, 0.32f), 110.f, 4.f, true, 5.f);
			if (Type == EZoogiShotType::TrickDoubleBank)
			{
				P.bHasSecondEventLocation = true; P.SecondEventLocation = Rim(100.f, nullptr);
				OutSetup += FString::Printf(TEXT(", 2nd contact = %s"), *P.SecondEventLocation.ToString());
			}
			if (Type == EZoogiShotType::TrickPowerBank)
			{
				P.bHasAccentColor = true; P.AccentColor = FLinearColor(0.35f, 0.75f, 1.f);
				OutSetup += TEXT(", power colour = frost blue");
			}
			if (Type == EZoogiShotType::TrickWallRedirect)
			{
				const FVector In = (E1 - FL).GetSafeNormal2D();
				const FVector N = P.EventNormal;
				P.bHasDirection = true; P.Direction = In - 2.f * FVector::DotProduct(In, N) * N;
				OutSetup += FString::Printf(TEXT(", new line = %s"), *P.Direction.ToString());
			}
		}
		break;
	case EZoogiShotType::TrickBoundarySave:
	case EZoogiShotType::TrickRingOutLine:
		if (F)
		{
			// Kinematic, collision-free Zoogi-sized prop: at the lip (boundary save) or heading out (ring-out line).
			const FVector FL = F->GetActorLocation();
			const bool bSave = Type == EZoogiShotType::TrickBoundarySave;
			const FVector2D U = RimDir(FL).GetRotated(bSave ? 0.f : 25.f);
			const float R = AZoogiArena::ActiveKnockoffR * (bSave ? 0.9f : 0.72f);
			if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
			TempProp = SpawnProp(FVector(U.X * R, U.Y * R, FL.Z), 1.f);
			TintProp(TempProp, bSave ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(1.f, 0.35f, 0.3f));
			if (TempProp) UseF = TempProp;
			if (!bSave)
			{
				const FVector2D Out = U.GetRotated(12.f);
				P.bHasDirection = true; P.Direction = FVector(Out.X, Out.Y, 0.f);
			}
			OutSetup = FString::Printf(TEXT("focus = Zoogi-sized prop at %s (r=%.0f)%s"), *(UseF ? UseF->GetActorLocation().ToString() : FString()), R,
				bSave ? TEXT(", event = arena centre (default)") : *FString::Printf(TEXT(", exit vector = %s"), *P.Direction.ToString()));
		}
		break;
	default: break;
	}
	return D->PlayShot(Spec, UseF, UseS, Output, P);
}

bool AZoogiShotTestDriver::TrackSettle(float RealDt)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;
	const FMinimalViewInfo& V = PC->PlayerCameraManager->GetCameraCacheView();
	if (bHaveLastPOV && ShotTestPOVDev(V, LastPOV) < 0.005f) StableTime += RealDt; else StableTime = 0.f;
	LastPOV = V;
	bHaveLastPOV = true;
	return StableTime >= 0.75f;
}

void AZoogiShotTestDriver::Result(bool bPass, const FString& What, const FString& Detail)
{
	(bPass ? GShotTestPass : GShotTestFail)++;
	const FString Line = FString::Printf(TEXT("ZOOGI SHOTTEST %s: %s | %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *What, *Detail);
	GShotTestResults.Add(Line);
	UE_LOG(LogZoogi, Log, TEXT("%s"), *Line);
}

// ------------------------------------------------------------------ trick lookup (pure, no Unreal job state)

void AZoogiShotTestDriver::CheckTrickLookup()
{
	using T = EZoogiEpisodeTag;
	struct FRow { TArray<T> Tags; EZoogiShotType Want; };
	const FRow Rows[] = {
		{ { T::Bank }, EZoogiShotType::TrickBank },
		{ { T::DoubleBank, T::Bank }, EZoogiShotType::TrickDoubleBank },
		{ { T::Bank, T::PowerHit }, EZoogiShotType::TrickPowerBank },
		{ { T::DoubleBank, T::PowerHit }, EZoogiShotType::TrickPowerBank },
		{ { T::WallRedirect }, EZoogiShotType::TrickWallRedirect },
		{ { T::AllyBump }, EZoogiShotType::TrickAllyBump },
		{ { T::OrbRelay }, EZoogiShotType::TrickOrbRelay },
		{ { T::BoundarySave, T::NearMiss }, EZoogiShotType::TrickBoundarySave },
		{ { T::RingOut }, EZoogiShotType::TrickRingOutLine },
		{ { T::PowerHit }, EZoogiShotType::LowAngleHero },
		{ { T::FunnyBounce }, EZoogiShotType::LowAngleHero },
	};
	FString Bad;
	for (const FRow& R : Rows)
	{
		const EZoogiShotType Got = ZoogiShot::ShotForTrickTags(R.Tags);
		FString TagStr;
		for (T Tg : R.Tags) TagStr += FString(ZoogiEpisode::TagName(Tg)) + TEXT("+");
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   lookup %s -> %s"), *TagStr.LeftChop(1), ZoogiShot::TypeName(Got));
		if (Got != R.Want) Bad += FString::Printf(TEXT(" %s->%s(want %s)"), *TagStr.LeftChop(1), ZoogiShot::TypeName(Got), ZoogiShot::TypeName(R.Want));
	}
	Result(Bad.IsEmpty(), TEXT("TrickLookup_ShotForTrickTags"), Bad.IsEmpty() ? FString::Printf(TEXT("%d tag sets mapped as expected"), (int32)UE_ARRAY_COUNT(Rows)) : Bad);

	// FillTrickParams from a synthetic truth record (two shooter wall contacts, then a floor roll).
	FZoogiGameplayTruthRecord Tr;
	Tr.Actor = 0; Tr.LaunchVelocity = FVector(1000, 0, 0);
	auto Add = [&Tr](float Tm, int32 Body, EZoogiContactKind K, const FVector& L, const FVector& N)
	{ FZoogiContactEvent E; E.Time = Tm; E.Body = Body; E.Kind = K; E.Location = L; E.Normal = N; E.bMeaningful = true; Tr.Contacts.Add(E); };
	Add(0.4f, 0, EZoogiContactKind::Wall, FVector(1400, 0, 50), FVector(-1, 0, 0));
	Add(0.9f, 0, EZoogiContactKind::Wall, FVector(0, 1400, 50), FVector(0, -1, 0));
	Add(1.5f, 0, EZoogiContactKind::Zoogi, FVector(-600, 600, 50), FVector::ZeroVector);
	FZoogiShotPlayParams PB, PD, PW;
	UZoogiEpisodeCameraDirector::FillTrickParams(EZoogiShotType::TrickBank, Tr, PB);
	UZoogiEpisodeCameraDirector::FillTrickParams(EZoogiShotType::TrickDoubleBank, Tr, PD);
	UZoogiEpisodeCameraDirector::FillTrickParams(EZoogiShotType::TrickWallRedirect, Tr, PW);
	const bool bOk = PB.bHasEventLocation && PB.EventLocation.X == 1400.f && PB.bHasEventNormal
		&& PD.bHasSecondEventLocation && PD.SecondEventLocation.Y == 1400.f
		&& PW.bHasEventLocation && PW.EventLocation.Y == 1400.f && PW.bHasDirection && PW.Direction.X < 0.f;
	Result(bOk, TEXT("TrickLookup_FillTrickParams"), FString::Printf(TEXT("bank=%s n=%s | dbl 2nd=%s | redirect at %s dir=%s"),
		*PB.EventLocation.ToString(), *PB.EventNormal.ToString(), *PD.SecondEventLocation.ToString(), *PW.EventLocation.ToString(), *PW.Direction.ToString()));
}

// ------------------------------------------------------------------ cases

void AZoogiShotTestDriver::StartCase()
{
	UZoogiEpisodeCameraDirector* D = Dir();
	const ZoogiShotTest::FCase& C = Cases[CaseIdx];
	if (CaseIdx == 0) CheckTrickLookup();
	Snap0 = D->CaptureGameplaySnapshot();
	Handles.Reset();
	FinishLog.Reset();
	bMidDone = bActionDone = bAction2Done = false;
	MaxMainDev = 0.f;
	bTimeScaleStarted = false;
	MinEffDilation = 1.f;
	StepTime = 0.f;
	AActor* F = Focus();
	AActor* S = SecondaryFor(F);
	const FZoogiShotSpec Spec = D->GetShotLibrary()->FindByType(C.Type) ? *D->GetShotLibrary()->FindByType(C.Type) : UZoogiShotLibrary::MakeDefault(C.Type);
	CaseMid = Spec.Duration * 0.55f;
	SetStatus(FString::Printf(TEXT("ZoogiShotTest  [%d/%d]  %s"), CaseIdx + 1, Cases.Num(), *C.Name));
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST case %s: focus %s, secondary %s, gameplay vt %s pov %s / %s / fov %.3f"), *C.Name, *GetNameSafe(F), *GetNameSafe(S),
		*GetNameSafe(Snap0.ViewTarget.Get()), *Snap0.POV.Location.ToString(), *Snap0.POV.Rotation.ToString(), Snap0.POV.FOV);
	auto Panel = [&](FZoogiShotHandle H, const FString& Label)
	{
		if (UTextureRenderTarget2D* RT = D->GetShotRenderTarget(H))
			ShowPanel(RT, FString::Printf(TEXT("LIVE RENDER TARGET %dx%d  -  %s"), RT->SizeX, RT->SizeY, *Label));
	};
	switch (C.Kind)
	{
	case ZoogiShotTest::ECaseKind::Shot:
	case ZoogiShotTest::ECaseKind::AirborneSlowMoCancel:
	case ZoogiShotTest::ECaseKind::AirborneSlowMoRefused:
	{
		// Airborne: slow-mo entry allowed while nothing simulated is awake (the match holds still on P1's input);
		// the refused case checks that the shot still plays, at normal speed, under the default Disabled policy.
		if (C.Type == EZoogiShotType::Airborne)
		{
			D->SetTimeScalePolicy(C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused ? EZoogiTimeScalePolicy::Disabled : EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest);
		}
		FString Setup;
		const FZoogiShotHandle H = PlayCaseShot(C.Type, C.Output, Setup);
		Handles.Add(H);
		if (!Setup.IsEmpty()) UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   %s"), *Setup);
		const int32 Tier = ZoogiShot::Tier(C.Type);
		if (C.Output == EZoogiShotOutput::RenderTarget)
			Panel(H, Tier > 0 ? FString::Printf(TEXT("%s (tier %d)"), ZoogiShot::TypeName(C.Type), Tier) : FString(ZoogiShot::TypeName(C.Type)));
		break;
	}
	case ZoogiShotTest::ECaseKind::Cancel:
	case ZoogiShotTest::ECaseKind::StopEased:
		Handles.Add(D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::MainView));
		break;
	case ZoogiShotTest::ECaseKind::Overlap:
		Handles.Add(D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::MainView));
		CaseMid = Spec.Duration * 0.3f + 1.1f;
		break;
	case ZoogiShotTest::ECaseKind::FocusDestroyed:
	{
		const FVector Side = F ? (F->GetActorLocation() + FVector(0.f, 0.f, 10.f) + FRotator(0.f, Snap0.POV.Rotation.Yaw + 90.f, 0.f).Vector() * 220.f) : FVector::ZeroVector;
		TempProp = SpawnProp(Side, 0.7f);
		Handles.Add(D->PlayShot(Spec, TempProp, nullptr, EZoogiShotOutput::MainView));
		const FZoogiShotHandle H2 = D->PlayShotType(EZoogiShotType::LowAngleHero, TempProp, nullptr, EZoogiShotOutput::RenderTarget);
		Handles.Add(H2);
		Panel(H2, TEXT("LowAngleHero on a prop that gets destroyed"));
		CaseMid = Spec.Duration * 0.3f;
		break;
	}
	case ZoogiShotTest::ECaseKind::TimeScaleRefused:
	{
		D->SetTimeScalePolicy(EZoogiTimeScalePolicy::Disabled);
		FZoogiTimeScaleSpec TSpec;
		bTimeScaleStarted = D->EaseTimeScale(TSpec);
		break;
	}
	case ZoogiShotTest::ECaseKind::TimeScale:
	{
		D->SetTimeScalePolicy(EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest);
		FZoogiTimeScaleSpec TSpec;
		TSpec.Target = 0.25f; TSpec.EaseIn = 0.4f; TSpec.Hold = 0.8f; TSpec.EaseOut = 0.4f;
		bTimeScaleStarted = D->EaseTimeScale(TSpec);
		Handles.Add(D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::MainView));
		break;
	}
	case ZoogiShotTest::ECaseKind::TimeScaleCancel:
	{
		D->SetTimeScalePolicy(EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest);
		FZoogiTimeScaleSpec TSpec;
		TSpec.Target = 0.2f; TSpec.EaseIn = 0.3f; TSpec.Hold = -1.f; TSpec.EaseOut = 0.3f;
		bTimeScaleStarted = D->EaseTimeScale(TSpec);
		break;
	}
	case ZoogiShotTest::ECaseKind::LevelEnd:
	{
		D->SetTimeScalePolicy(EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest);
		Handles.Add(D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::MainView));
		const FZoogiShotHandle H2 = D->PlayShotType(EZoogiShotType::CloseUp, F, nullptr, EZoogiShotOutput::RenderTarget);
		Handles.Add(H2);
		Panel(H2, TEXT("CloseUp (level ends mid-shot)"));
		FZoogiTimeScaleSpec TSpec;
		TSpec.Target = 0.3f; TSpec.EaseIn = 0.3f; TSpec.Hold = -1.f; TSpec.EaseOut = 0.3f;
		bTimeScaleStarted = D->EaseTimeScale(TSpec);
		break;
	}
	case ZoogiShotTest::ECaseKind::AvoidParticles:
	{
		// A white glow burst (+ a Niagara white burst) right where the tracking camera would sit; the camera must stay out of it.
		const AZoogiBall* B = Cast<AZoogiBall>(F);
		const FVector Pv = B ? B->BallLocation() : (F ? F->GetActorLocation() : FVector::ZeroVector);
		const float Pr = FMath::DegreesToRadians(17.f);
		const FVector Cam = Pv - FRotator(0.f, Snap0.POV.Rotation.Yaw, 0.f).Vector() * 530.f * FMath::Cos(Pr) + FVector(0.f, 0.f, 530.f * FMath::Sin(Pr));
		TempFX = AZoogiFX::Spawn(GetWorld(), Cam, FLinearColor::White, 230.f, 4.0f, false, 4.f);
		if (UNiagaraSystem* NS = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/FX/NS_Burst_White.NS_Burst_White"), nullptr, LOAD_NoWarn))
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NS, Cam, FRotator::ZeroRotator, FVector(2.f), true, true, ENCPoolMethod::None);
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   burst at the predicted tracking camera %s (glow actor %s)"), *Cam.ToString(), *GetNameSafe(TempFX));
		Handles.Add(D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::MainView));
		break;
	}
	case ZoogiShotTest::ECaseKind::AvoidOcclusion:
	{
		// A camera-only blocker (query only, blocks ECC_Camera, ignores every other channel: no physics contact) between
		// P1 and the predicted close-up camera, low enough to stay below the high gameplay view.
		const AZoogiBall* B = Cast<AZoogiBall>(F);
		const FVector Pv = B ? B->BallLocation() : (F ? F->GetActorLocation() : FVector::ZeroVector);
		const float Yw = Snap0.POV.Rotation.Yaw + Spec.YawOffset + Spec.OrbitDegrees * 0.5f;
		const FVector Out = -FRotator(0.f, Yw, 0.f).Vector();
		TempProp = SpawnProp(Pv + Out * 150.f + FVector(0.f, 0.f, 5.f), 1.f);
		if (TempProp)
		{
			UStaticMeshComponent* M = TempProp->GetStaticMeshComponent();
			M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			TempProp->SetActorRotation(FRotator(0.f, Yw, 0.f));
			TempProp->SetActorScale3D(FVector(0.15f, 2.6f, 1.1f));
			M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			M->SetCollisionResponseToAllChannels(ECR_Ignore);
			M->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		}
		const FZoogiShotHandle H = D->PlayShot(Spec, F, nullptr, EZoogiShotOutput::RenderTarget);
		Handles.Add(H);
		Panel(H, TEXT("CloseUp behind a camera blocker"));
		break;
	}
	default: break;
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoCancel) CaseMid = 0.6f; // screenshot mid slow-mo, before the cancel
}

void AZoogiShotTestDriver::RunCase(float RealDt)
{
	UZoogiEpisodeCameraDirector* D = Dir();
	const ZoogiShotTest::FCase& C = Cases[CaseIdx];
	StepTime += RealDt;
	if (AWorldSettings* WS = GetWorld()->GetWorldSettings()) MinEffDilation = FMath::Min(MinEffDilation, WS->GetEffectiveTimeDilation());

	// Tracking prop: circles the focus Zoogi (visual only).
	if (TrackProp && !TrackProp->IsHidden())
	{
		TrackAngle += RealDt * 1.3f;
		TrackProp->SetActorLocation(TrackCenter + FVector(FMath::Cos(TrackAngle) * 380.f, FMath::Sin(TrackAngle) * 380.f, 20.f));
	}
	// Airborne prop: ballistic arc in game time (dilated, frozen while paused), looping.
	if (ArcProp && !ArcProp->IsHidden())
	{
		ArcTime += GetWorld()->GetDeltaSeconds();
		const float Flight = 2.f * ArcVel.Z / 980.f;
		if (ArcTime > Flight) ArcTime -= Flight;
		const FVector Pos = ArcStart + ArcVel * ArcTime + FVector(0.f, 0.f, -490.f * ArcTime * ArcTime);
		MaxArcHeight = FMath::Max(MaxArcHeight, Pos.Z - ArcStart.Z);
		ArcProp->SetActorLocation(Pos);
	}
	// RenderTarget output must leave the main view completely alone, every frame.
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused || C.Kind == ZoogiShotTest::ECaseKind::AvoidOcclusion) && C.Output == EZoogiShotOutput::RenderTarget)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC && PC->PlayerCameraManager)
		{
			MaxMainDev = FMath::Max(MaxMainDev, ShotTestPOVDev(PC->PlayerCameraManager->GetCameraCacheView(), Snap0.POV));
			if (PC->GetViewTarget() != Snap0.ViewTarget.Get()) MaxMainDev = 1e6f;
		}
	}
	const float Dur = Cases[CaseIdx].Kind == ZoogiShotTest::ECaseKind::Shot ? CaseMid / 0.55f : 3.f;
	switch (C.Kind)
	{
	case ZoogiShotTest::ECaseKind::Cancel:
		if (!bActionDone && StepTime >= 0.8f) { bActionDone = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST cancel (immediate) at %.2f s"), StepTime); if (Handles.Num()) D->StopShot(Handles[0], true); }
		break;
	case ZoogiShotTest::ECaseKind::StopEased:
		if (!bActionDone && StepTime >= 0.8f) { bActionDone = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST eased stop at %.2f s"), StepTime); if (Handles.Num()) D->StopShot(Handles[0], false); }
		break;
	case ZoogiShotTest::ECaseKind::Overlap:
		if (!bActionDone && StepTime >= 0.6f)
		{
			bActionDone = true;
			AActor* F = Focus();
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST overlap: second MainView shot + a RenderTarget shot at %.2f s"), StepTime);
			Handles.Add(D->PlayShotType(EZoogiShotType::PartnerTwoShot, F, SecondaryFor(F), EZoogiShotOutput::MainView));
			const FZoogiShotHandle H3 = D->PlayShotType(EZoogiShotType::LowAngleHero, F, nullptr, EZoogiShotOutput::RenderTarget);
			Handles.Add(H3);
			if (UTextureRenderTarget2D* RT = D->GetShotRenderTarget(H3)) ShowPanel(RT, FString::Printf(TEXT("LIVE RENDER TARGET %dx%d  -  LowAngleHero (overlapping)"), RT->SizeX, RT->SizeY));
		}
		break;
	case ZoogiShotTest::ECaseKind::FocusDestroyed:
		if (!bActionDone && StepTime >= 0.8f)
		{
			bActionDone = true;
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST destroying the focus actor %s at %.2f s"), *GetNameSafe(TempProp), StepTime);
			if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
		}
		break;
	case ZoogiShotTest::ECaseKind::TimeScaleCancel:
		if (!bActionDone && StepTime >= 0.7f) { bActionDone = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST time-scale cancel at %.2f s (effective %.3f)"), StepTime, MinEffDilation); D->StopTimeScale(true); }
		break;
	case ZoogiShotTest::ECaseKind::TimeScale:
		if (!bAction2Done && StepTime >= 0.9f)
		{
			bAction2Done = true;
			AWorldSettings* WS = GetWorld()->GetWorldSettings();
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST mid-ease: TimeDilation %.4f, CinematicTimeDilation %.4f, effective %.4f, world dt %.5f vs real %.5f"),
				WS->TimeDilation, WS->CinematicTimeDilation, WS->GetEffectiveTimeDilation(), GetWorld()->GetDeltaSeconds(), GetWorld()->DeltaRealTimeSeconds);
		}
		break;
	case ZoogiShotTest::ECaseKind::AirborneSlowMoCancel:
		if (!bActionDone && StepTime >= 0.75f)
		{
			bActionDone = true;
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST airborne cancel (immediate) at %.2f s mid slow-mo: effective dilation %.3f, owned by the shot=%d"),
				StepTime, GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), (Handles.Num() && D->IsTimeScaleOwnedBy(Handles[0])) ? 1 : 0);
			if (Handles.Num()) D->StopShot(Handles[0], true);
		}
		break;
	case ZoogiShotTest::ECaseKind::LevelEnd:
		if (!bActionDone && StepTime >= 0.9f)
		{
			bActionDone = true;
			GLevelEndSnap = Snap0;
			GLevelEndName = C.Name;
			GLevelEndPending = true;
			const FString Map = GetWorld()->GetOutermost()->GetName();
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST level end mid-shot: %d shot(s) active, main view taken=%d, time-scale active=%d, effective dilation %.3f; opening %s"),
				D->NumActiveShots(), D->IsMainViewTaken() ? 1 : 0, D->IsTimeScaleActive() ? 1 : 0, GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), *Map);
			UGameplayStatics::OpenLevel(this, FName(*Map));
			return;
		}
		break;
	default: break;
	}
	if (!bMidDone && StepTime >= CaseMid && C.Kind != ZoogiShotTest::ECaseKind::TimeScaleRefused && C.Kind != ZoogiShotTest::ECaseKind::TimeScaleCancel)
	{
		bMidDone = true;
		Shot(C.Name + TEXT("_shot"));
		if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused || C.Kind == ZoogiShotTest::ECaseKind::AvoidOcclusion) && C.Output == EZoogiShotOutput::RenderTarget && Handles.Num())
			ExportRT(D->GetShotRenderTarget(Handles[0]), C.Name + TEXT("_rendertarget"));
		if (C.Kind == ZoogiShotTest::ECaseKind::Overlap && Handles.Num() >= 3) ExportRT(D->GetShotRenderTarget(Handles[2]), C.Name + TEXT("_rendertarget"));
	}
	const bool bIdle = D->NumActiveShots() == 0 && !D->IsTimeScaleActive();
	if (bIdle && StepTime > 0.2f && (C.Kind != ZoogiShotTest::ECaseKind::LevelEnd))
	{
		Step = ZoogiShotTest::EStep::WaitEnd;
		WaitFrames = 4;
	}
	else if (StepTime > Dur + 12.f)
	{
		Result(false, C.Name, TEXT("timed out waiting for the director to finish"));
		D->StopAllShots(true);
		D->StopTimeScale(true);
	}
}

void AZoogiShotTestDriver::Verify()
{
	UZoogiEpisodeCameraDirector* D = Dir();
	const ZoogiShotTest::FCase& C = Cases[CaseIdx];
	const FZoogiCameraSnapshot Snap1 = D->CaptureGameplaySnapshot();
	FString Report;
	bool bPass = UZoogiEpisodeCameraDirector::CompareSnapshots(Snap0, Snap1, FZoogiCameraTolerance(), Report);
	TArray<FString> Extra;
	APlayerController* PC = Snap1.PC.Get();
	if (PC && D->GetShotCamera() && PC->GetViewTarget() == D->GetShotCamera()) { bPass = false; Extra.Add(TEXT("shot camera still the view target")); }
	if (D->IsMainViewTaken() || D->NumActiveCaptures() > 0) { bPass = false; Extra.Add(TEXT("director not idle")); }
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused || C.Kind == ZoogiShotTest::ECaseKind::AvoidOcclusion) && C.Output == EZoogiShotOutput::RenderTarget)
	{
		const bool bUntouched = MaxMainDev <= 0.01f;
		bPass &= bUntouched;
		Extra.Add(FString::Printf(TEXT("main view during capture: max dev %.5f (%s)"), MaxMainDev, bUntouched ? TEXT("untouched") : TEXT("DISTURBED")));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::TimeScaleRefused)
	{
		bPass &= !bTimeScaleStarted;
		Extra.Add(bTimeScaleStarted ? TEXT("ease ran under Disabled policy") : TEXT("ease refused under Disabled policy as designed"));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::TimeScale || C.Kind == ZoogiShotTest::ECaseKind::TimeScaleCancel)
	{
		// Core rule: physics is never dilated. EaseTimeScale must refuse even with an opt-in policy.
		const bool bNever = !bTimeScaleStarted && MinEffDilation > 0.999f;
		Extra.Add(bNever ? FString::Printf(TEXT("ease refused, world never dilated (min effective %.4f)"), MinEffDilation)
			: FString::Printf(TEXT("WORLD DILATED: started=%d min effective %.4f"), bTimeScaleStarted ? 1 : 0, MinEffDilation));
		bPass &= bNever;
	}
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot && C.Type == EZoogiShotType::Airborne) || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoCancel || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused)
	{
		FZoogiShotAvoidStats St;
		const bool bHave = Handles.Num() && D->GetShotAvoidStats(Handles[0], St);
		const bool bLook = bHave && St.SlowLookFrames > 0 && St.MinMotionRate < 0.6f;
		const bool bNoDil = MinEffDilation > 0.999f && !D->IsTimeScaleActive();
		bPass &= bLook && bNoDil;
		Extra.Add(FString::Printf(TEXT("slow-mo look %s (presentation only: %d frames, slowest motion rate %.2f) | world dilation min %.4f (%s) | arc peak %.0f cm"),
			bLook ? TEXT("ran") : TEXT("DID NOT RUN"), St.SlowLookFrames, St.MinMotionRate, MinEffDilation, bNoDil ? TEXT("never dilated") : TEXT("DILATED"), MaxArcHeight));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::AvoidParticles || C.Kind == ZoogiShotTest::ECaseKind::AvoidOcclusion || C.Kind == ZoogiShotTest::ECaseKind::Shot)
	{
		FZoogiShotAvoidStats St;
		if (Handles.Num() && D->GetShotAvoidStats(Handles[0], St))
		{
			bool bOk = St.FinalInsideFXFrames == 0;
			if (C.Kind == ZoogiShotTest::ECaseKind::AvoidParticles) bOk &= St.RawInsideFXFrames > 0;
			if (C.Kind == ZoogiShotTest::ECaseKind::AvoidOcclusion) bOk &= St.OccludedFrames > 0 && St.FinalOccludedFrames == 0;
			bPass &= bOk;
			Extra.Add(FString::Printf(TEXT("avoid: frames %d, raw-in-burst %d, final-in-burst %d, occluded %d (final %d), max offset %.0f cm, max pull-in %.0f cm, largest burst half-extent %.0f cm"),
				St.Frames, St.RawInsideFXFrames, St.FinalInsideFXFrames, St.OccludedFrames, St.FinalOccludedFrames, St.MaxAvoidOffsetCm, St.MaxPullInCm, St.MaxVolumeExtentCm));
		}
		else if (C.Kind != ZoogiShotTest::ECaseKind::Shot) { bPass = false; Extra.Add(TEXT("no avoid stats")); }
	}
	if (FinishLog.Num()) Extra.Add(TEXT("ends ") + FString::Join(FinishLog, TEXT(",")));
	if (AZoogiGameMode* G = GM()) Extra.Add(FString::Printf(TEXT("stage %d phase %d"), (int32)G->Stage, (int32)G->Phase));
	Result(bPass, FString::Printf(TEXT("[%s] camera restore"), *C.Name), Report + TEXT(" | ") + FString::Join(Extra, TEXT(" | ")));
}

void AZoogiShotTestDriver::Finish()
{
	const FString Line = FString::Printf(TEXT("ZOOGI SHOTTEST SUMMARY: %d passed, %d failed"), GShotTestPass, GShotTestFail);
	UE_LOG(LogZoogi, Log, TEXT("%s"), *Line);
	for (const FString& R : GShotTestResults) UE_LOG(LogZoogi, Log, TEXT("  %s"), *R);
	SetStatus(Line);
}

void AZoogiShotTestDriver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float RealDt = FMath::Clamp(GetWorld()->DeltaRealTimeSeconds, 0.f, 0.1f);
	Clock += RealDt;
	PumpScreenshots();
	AZoogiGameMode* G = GM();
	UZoogiEpisodeCameraDirector* D = Dir();
	if (!G || !D) return;
	if (Clock > 600.f && Step != ZoogiShotTest::EStep::Done && Step != ZoogiShotTest::EStep::Summary)
	{
		Result(false, TEXT("overall"), TEXT("test timed out after 600 s"));
		Step = ZoogiShotTest::EStep::Done;
		StepTime = 0.f;
		Finish();
	}
	switch (Step)
	{
	case ZoogiShotTest::EStep::StartMatch:
		if (G->Phase == EZoogiPhase::Menu && Clock > 2.f)
		{
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST starting a match (P1 human, the rest CPU)"));
			G->MenuAction(TEXT("Start"), 0);
			Step = ZoogiShotTest::EStep::WaitLive;
			StepTime = 0.f;
		}
		else if (G->Phase != EZoogiPhase::Menu) { Step = ZoogiShotTest::EStep::WaitLive; StepTime = 0.f; }
		break;
	case ZoogiShotTest::EStep::WaitLive:
		StepTime += RealDt;
		if (G->Phase == EZoogiPhase::Forming && StepTime > 1.5f) G->SkipStageForming();
		if (G->IsHumanAiming() && ((G->Stage == EZoogiStage::Awaken && !G->bAwakenDone) || G->IsAimStep()))
		{
			if (TrackSettle(RealDt))
			{
				UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST live match holding on P1's input (stage %d), gameplay camera settled; awake body: '%s'"), (int32)G->Stage, *D->FindAwakePhysicsBody());
				TrackProp = SpawnProp(FVector::ZeroVector, 0.55f);
				if (TrackProp) TrackProp->SetActorHiddenInGame(true);
				ArcProp = SpawnProp(FVector::ZeroVector, 0.6f);
				if (ArcProp) ArcProp->SetActorHiddenInGame(true);
				Shot(TEXT("00_gameplay_before"));
				CaseIdx = 0;
				Step = ZoogiShotTest::EStep::WaitSettle;
				StableTime = 0.f;
				StepTime = 0.f;
			}
		}
		else StableTime = 0.f;
		if (StepTime > 240.f) { Result(false, TEXT("setup"), TEXT("no live human turn reached in 240 s")); Step = ZoogiShotTest::EStep::Done; Finish(); }
		break;
	case ZoogiShotTest::EStep::WaitSettle:
		StepTime += RealDt;
		if ((TrackSettle(RealDt) && PendingShots.Num() == 0 && !FScreenshotRequest::IsScreenshotRequested()) || StepTime > 8.f)
		{
			if (StepTime > 8.f) UE_LOG(LogZoogi, Warning, TEXT("ZOOGI SHOTTEST gameplay camera not settled after 8 s (stage %d), starting anyway"), (int32)G->Stage);
			StartCase();
			Step = ZoogiShotTest::EStep::Run;
		}
		break;
	case ZoogiShotTest::EStep::Run:
		RunCase(RealDt);
		break;
	case ZoogiShotTest::EStep::WaitEnd:
		if (--WaitFrames <= 0) { Verify(); Step = ZoogiShotTest::EStep::Restored; StepTime = 0.f; WaitFrames = 0; }
		break;
	case ZoogiShotTest::EStep::Restored:
		StepTime += RealDt;
		if (WaitFrames == 0)
		{
			HidePanel();
			if (TrackProp) TrackProp->SetActorHiddenInGame(true);
			if (ArcProp) ArcProp->SetActorHiddenInGame(true);
			if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
			if (TempFX) { TempFX->Destroy(); TempFX = nullptr; }
			WaitFrames = 1;
		}
		else if (WaitFrames == 1 && StepTime > 0.15f)
		{
			Shot(Cases[CaseIdx].Name + TEXT("_restored"));
			WaitFrames = 2;
		}
		else if (WaitFrames == 2 && PendingShots.Num() == 0 && !FScreenshotRequest::IsScreenshotRequested() && StepTime > 0.4f)
		{
			++CaseIdx;
			StableTime = 0.f;
			StepTime = 0.f;
			Step = CaseIdx < Cases.Num() ? ZoogiShotTest::EStep::WaitSettle : ZoogiShotTest::EStep::Done;
			if (Step == ZoogiShotTest::EStep::Done) Finish();
		}
		break;
	case ZoogiShotTest::EStep::Summary:
		StepTime += RealDt;
		if (StepTime > 1.5f && WaitFrames == 0) { Finish(); WaitFrames = 1; }
		if (StepTime > 4.f && WaitFrames == 1)
		{
			WaitFrames = 2;
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST done, quitting"));
			UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		}
		if (StepTime > 12.f) FPlatformMisc::RequestExit(false);
		break;
	case ZoogiShotTest::EStep::Done:
		StepTime += RealDt;
		if (StepTime > 3.f && WaitFrames != 7) { WaitFrames = 7; UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); }
		if (StepTime > 12.f) FPlatformMisc::RequestExit(false);
		break;
	}
}
