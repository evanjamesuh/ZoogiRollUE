#include "ZoogiShotTest.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiPlayerController.h"
#include "ZoogiPortraitCam.h"
#include "ZoogiSpawnElevator.h"
#include "ZoogiAimView.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiStageForming.h"
#include "Camera/CameraComponent.h"
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
	Add(ZoogiShotTest::ECaseKind::RingOutSimLegacy, TEXT("RingOutFall_GameplayFollow_BEFORE_legacy_rule"), EZoogiShotType::Custom, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::RingOutSimFixed, TEXT("RingOutFall_GameplayFollow_AFTER_fixed_rule"), EZoogiShotType::Custom, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::RingOutDirector, TEXT("RingOutFall_TrickRingOutLine_MainView"), EZoogiShotType::TrickRingOutLine, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::RingOutDirector, TEXT("RingOutFall_Tracking_MainView"), EZoogiShotType::Tracking, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::RingOutDirector, TEXT("RingOutFall_Airborne_MainView"), EZoogiShotType::Airborne, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::ElevatorPush, TEXT("ElevatorPushIn_StageIntro_SpawnPads"), EZoogiShotType::Custom, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::Portrait, TEXT("Portrait_Framing_Court_Ring2_SafeBand"), EZoogiShotType::Custom, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AimView, TEXT("AimView_BehindZoogi_FollowsAim"), EZoogiShotType::Custom, EZoogiShotOutput::MainView);
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
			// Bank / power bank: a Zoogi-sized kinematic prop actually rolls in from P1's side, hits the contact during the
			// shot and leaves on the mirrored line (game time; no collision / physics).
			if (Type == EZoogiShotType::TrickBank || Type == EZoogiShotType::TrickPowerBank)
			{
				BankIn = (E1 - FL).GetSafeNormal2D();
				BankOut = BankIn - 2.f * FVector::DotProduct(BankIn, N1) * N1;
				BankE = E1; BankSpeed = 750.f; BankT = 0.f;
				BankTc = Type == EZoogiShotType::TrickPowerBank ? 0.5f : 0.7f;
				if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
				TempProp = SpawnProp(E1 - BankIn * BankSpeed * BankTc, 1.f);
				TintProp(TempProp, FLinearColor(0.3f, 0.6f, 1.f));
				if (TempProp) { UseF = TempProp; bBankMove = true; }
				OutSetup += FString::Printf(TEXT(", subject = prop rolling in at %.0f cm/s, contact at t=%.2f s, out line = %s"), BankSpeed, BankTc, *BankOut.ToString());
			}
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

// ------------------------------------------------------------------ ring-out fall

FVector AZoogiShotTestDriver::FallPos(float T) const
{
	// Rolls outward along the floor at 650 cm/s, leaves the rim at FallEdgeT, then falls under gravity while drifting out.
	if (T <= FallEdgeT) return FallStart + FallDir * 650.f * T;
	const float Tf = T - FallEdgeT;
	return FallStart + FallDir * (650.f * FallEdgeT + 560.f * Tf) + FVector(0.f, 0.f, -490.f * Tf * Tf);
}

void AZoogiShotTestDriver::StartRingOut(EZoogiShotType Type, bool bSim)
{
	UZoogiEpisodeCameraDirector* D = Dir();
	AActor* F = Focus();
	const FVector FL = F ? F->GetActorLocation() : FVector(-1000.f, 0.f, 50.f);
	// Toward the rim in front of the gameplay camera (so the fall happens where the Tactical view looks).
	const FVector2D U = RimDir(FL).GetRotated(20.f);
	const float R = AZoogiPlayerController::PlayRadius(GM() ? GM()->Arena : nullptr);
	FallDir = FVector(U.X, U.Y, 0.f);
	FallStart = FallDir * (R * 0.6f) + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius);
	FallEdgeT = (R * 0.95f - R * 0.6f) / 650.f;
	FallT = 0.f;
	bRingOut = true; bRingOutSim = bSim; bRingOutDone = false; bLateShot = false;
	RingLossNow = RingLossMax = 0.f; RingMinCamZ = 1e9f; RingFrames = RingOutFrames = 0; RingSeconds = 3.4f;
	SimCourtZ = ZoogiConst::ZoogiRadius;
	if (TempProp) { TempProp->Destroy(); TempProp = nullptr; }
	TempProp = SpawnProp(FallStart, 1.f);
	TintProp(TempProp, FLinearColor(0.3f, 0.6f, 1.f));
	CaseMid = FallEdgeT + 0.9f; // mid-fall frame
	if (bSim)
	{
		SimPivot = FVector(FL.X, FL.Y, SimCourtZ) * 0.f + FVector(0.f, 0.f, SimCourtZ);
		FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
		SimCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Snap0.POV.Location, Snap0.POV.Rotation, SP);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && SimCam) PC->SetViewTarget(SimCam);
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   ring-out sim (%s rule): prop from %s along %s, rim at t=%.2f s; Tactical follow simulated on a temp camera"),
			bRingOutLegacy ? TEXT("LEGACY") : TEXT("FIXED"), *FallStart.ToString(), *FallDir.ToString(), FallEdgeT);
	}
	else
	{
		const FZoogiShotSpec* Lib = D->GetShotLibrary()->FindByType(Type);
		FZoogiShotSpec Spec = Lib ? *Lib : UZoogiShotLibrary::MakeDefault(Type);
		Spec.Duration = FMath::Max(Spec.Duration, FallEdgeT + 2.2f); // long enough to still be running when the prop falls
		FZoogiShotPlayParams P;
		P.bHasDirection = true; P.Direction = FallDir;
		Handles.Add(D->PlayShot(Spec, TempProp, nullptr, EZoogiShotOutput::MainView, P));
		RingSeconds = Spec.Duration + 0.6f;
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   ring-out director shot %s (%.2f s) on a prop rolling off the rim at t=%.2f s"), ZoogiShot::TypeName(Type), Spec.Duration, FallEdgeT);
	}
}

void AZoogiShotTestDriver::CheckRingOutView(const FMinimalViewInfo& V, float Dt)
{
	const float Aspect = 16.f / 9.f;
	const FVector L = V.Rotation.UnrotateVector(FVector::ZeroVector - V.Location); // arena centre (floor centre)
	const float Th = FMath::Tan(FMath::DegreesToRadians(V.FOV * 0.5f));
	const bool bIn = L.X > 1.f && FMath::Abs(L.Y) / L.X <= Th && FMath::Abs(L.Z) / L.X <= Th / Aspect;
	RingFrames++;
	if (!bIn) { RingOutFrames++; RingLossNow += Dt; RingLossMax = FMath::Max(RingLossMax, RingLossNow); } else RingLossNow = 0.f;
	RingMinCamZ = FMath::Min(RingMinCamZ, V.Location.Z);
}

void AZoogiShotTestDriver::TickRingOut(float Dt)
{
	if (bRingOutDone) return;
	FallT += Dt;
	const FVector Pos = FallPos(FallT);
	if (TempProp) TempProp->SetActorLocation(Pos);
	if (!bLateShot && FallT >= FallEdgeT + 2.2f) { bLateShot = true; Shot(Cases[CaseIdx].Name + TEXT("_late")); }
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (bRingOutSim && SimCam && PC)
	{
		AZoogiGameMode* G = GM();
		float Pitch = 55.f, Dist = 2400.f, Fov = 80.f, Lag = 3.f;
		AZoogiPlayerController::TacticalBoom(G, Pitch, Dist, Fov, Lag);
		const bool bSimOut = Pos.Z < -600.f; // the game marks a faller out well below the rim
		FVector Want;
		if (bRingOutLegacy)
		{
			// Pre-fix rule (ZoogiPlayerController.cpp before this commit): pivot = lerp(centre, shooter, 0.55) until bOut / z < -600,
			// court height tracking the shooter down to -200, probe swept from the (falling) pivot.
			if (!bSimOut && Pos.Z > -200.f) SimCourtZ = Pos.Z;
			const FVector C0(0.f, 0.f, SimCourtZ);
			Want = (bSimOut || Pos.Z < -600.f) ? C0 : FMath::Lerp(C0, Pos, 0.55f);
		}
		else
		{
			if (!AZoogiPlayerController::IsLeavingPlay(Pos, bSimOut, SimCourtZ, G ? G->Arena : nullptr) && Pos.Z > -200.f) SimCourtZ = Pos.Z;
			Want = AZoogiPlayerController::TacticalRollPivot(Pos, bSimOut, SimCourtZ, G ? G->Arena : nullptr);
		}
		SimPivot = SimPivot + (Want - SimPivot) * (1.f - FMath::Exp(-Lag * Dt));
		if (!bRingOutLegacy) SimPivot.Z = FMath::Max(SimPivot.Z, SimCourtZ - 100.f);
		const float Yaw = Snap0.POV.Rotation.Yaw;
		const float Pr = FMath::DegreesToRadians(Pitch);
		const FVector Desired = SimPivot - FRotator(0.f, Yaw, 0.f).Vector() * Dist * FMath::Cos(Pr) + FVector(0.f, 0.f, Dist * FMath::Sin(Pr));
		// collision probe like the controller's (sphere sweep pivot -> camera on ECC_Camera, Zoogis / orbs / props ignored)
		FVector From = Want + FVector(0.f, 0.f, 150.f);
		if (!bRingOutLegacy) From.Z = FMath::Max(From.Z, SimCourtZ + 60.f);
		FCollisionQueryParams Q(TEXT("ShotTestRingProbe"), false);
		for (TActorIterator<AZoogiBall> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
		for (TActorIterator<AZoogiOrb> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
		if (TempProp) Q.AddIgnoredActor(TempProp);
		FHitResult Hit;
		FVector Loc = Desired;
		if (GetWorld()->SweepSingleByChannel(Hit, From, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q) && !Hit.bStartPenetrating)
			Loc = From + (Desired - From) * FMath::Clamp(Hit.Time - 0.02f, 0.08f, 1.f);
		if (!bRingOutLegacy) Loc.Z = FMath::Max(Loc.Z, SimCourtZ);
		SimCam->SetActorLocation(Loc);
		SimCam->SetActorRotation((SimPivot - Loc).Rotation());
		SimCam->GetCameraComponent()->SetFieldOfView(Fov);
		FMinimalViewInfo V; V.Location = Loc; V.Rotation = (SimPivot - Loc).Rotation(); V.FOV = Fov;
		if (FallT > 0.1f) CheckRingOutView(V, Dt);
		if (FallT >= RingSeconds)
		{
			PC->SetViewTarget(Snap0.ViewTarget.Get());
			bRingOutDone = true;
		}
	}
	else if (PC && PC->PlayerCameraManager)
	{
		// Director shots may look down the exit line while the Zoogi is still rolling (RingOutLine's intended look);
		// what is checked is the fall: from the moment the prop leaves the rim.
		if (FallT > FallEdgeT) CheckRingOutView(PC->PlayerCameraManager->GetCameraCacheView(), Dt);
		if (FallT >= RingSeconds) bRingOutDone = true;
	}
}

// ------------------------------------------------------------------ portrait framing (Oct 17)

void AZoogiShotTestDriver::StartPortrait()
{
	bPortrait = true; bPortDone = false; bPortRestored = false; PortStage = 0; PortT = 0.f;
	PortCourtOut = PortCourtN = PortRing2Out = PortRing2N = PortShooterOut = PortShooterN = 0;
	PortCourtMaxX = 0.f; PortShooterMinY = 1e9f; PortShooterMaxY = -1e9f; PortLegacyOut = 0.f; PortVFov = 0.f;
	bPortModeOk = false; bPortBehind = false; bPortAimMode = false; PortAimPitch = 0.f; PortModeLine = TEXT("mode hook: not run");
	bChaseRan = false; ChaseShooterY = ChaseAimX = ChaseFarY = ChasePitchGot = ChaseCamH = ChaseBoomGot = 0.f; ChaseArm = 1.f;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	PortAspect = ZoogiPortrait::ViewAspect(PC);
	ZoogiPortrait::Band(PC, PortAspect, PortTop, PortBot);
	PortCourtR = AZoogiPlayerController::PortraitCourtRadius(GM());
	AZoogiGameMode* G = GM();
	const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	PortCourtZ = B ? B->BallLocation().Z : 50.f; // the controller's CourtZ (shooter centre height)
	CaseMid = 1e6f;
	FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
	SimCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Snap0.POV.Location, Snap0.POV.Rotation, SP);
	if (SimCam) SimCam->GetCameraComponent()->bConstrainAspectRatio = false;
	if (PC && SimCam) PC->SetViewTarget(SimCam);
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   portrait framing: view aspect %.3f (%s), safe band top %.2f bottom %.2f, court radius %.0f (Ring 2 outer %.0f)"),
		PortAspect, ZoogiPortrait::IsPortrait(PortAspect) ? TEXT("portrait") : TEXT("landscape"), PortTop, PortBot, PortCourtR, AZoogiArena::OuterOutR);
}

void AZoogiShotTestDriver::TickPortrait(float Dt)
{
	if (bPortDone) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AZoogiPlayerController* ZPC = Cast<AZoogiPlayerController>(PC);
	AZoogiGameMode* G = GM();
	if (!SimCam || !PC) { bPortDone = true; return; }
	PortT += FMath::Max(Dt, 1.f / 120.f);
	const float Yaw = Snap0.POV.Rotation.Yaw;
	const FString& N = Cases[CaseIdx].Name;
	auto Place = [&](const FVector& L, const FRotator& R, float F) { SimCam->SetActorLocation(L); SimCam->SetActorRotation(R); SimCam->GetCameraComponent()->SetFieldOfView(F); };
	const float Y1 = 1.f - 2.f * PortTop + 0.01f, Y0 = -1.f + 2.f * PortBot - 0.01f;
	auto CountCircle = [&](const FVector& L, const FRotator& R, float F, float Rad, int32& Out, int32& Num, float* MaxX)
	{
		for (int32 k = 0; k < 48; ++k)
		{
			const float A = FMath::DegreesToRadians(7.5f * k);
			FVector2D S;
			const bool bFront = ZoogiPortrait::Project(L, R, F, PortAspect, FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, PortCourtZ), S);
			++Num;
			if (!bFront || FMath::Abs(S.X) > 1.f || S.Y > Y1 || S.Y < Y0) ++Out;
			if (MaxX) *MaxX = FMath::Max(*MaxX, FMath::Abs(S.X));
		}
	};
	if (PortStage == 0 && PortT >= 0.1f)
	{
		// BEFORE overview: the pre-portrait resolve / end view (landscape FOV as horizontal, no band) on this screen
		FVector L; FRotator R; float F;
		AZoogiPlayerController::PortraitOverviewPose(G, PC, Yaw, PortCourtZ, true, L, R, F);
		Place(L, R, F);
		int32 O = 0, Nn = 0; CountCircle(L, R, F, PortCourtR, O, Nn, nullptr);
		PortLegacyOut = Nn ? float(O) / Nn : 0.f;
		PortStage = 1;
	}
	else if (PortStage == 1 && PortT >= 0.4f) { Shot(N + TEXT("_before")); PortStage = 2; }
	else if (PortStage == 2 && PortT >= 0.7f)
	{
		// BEFORE aim view: the pre-portrait aim / rest pose (landscape pivot, pitch, boom and FOV-as-horizontal) at the live yaw
		const FMinimalViewInfo V = Snap0.POV;
		if (ZPC)
		{
			const FVector L = AZoogiPlayerController::BoomLocation(ZPC->PortDbgLegacyPivot, V.Rotation.Yaw, ZPC->PortDbgLegacyPitch, ZPC->PortDbgLegacyDist);
			Place(L, (ZPC->PortDbgLegacyPivot - L).Rotation(), ZPC->PortDbgFov);
		}
		else Place(V.Location, V.Rotation, V.FOV);
		PortStage = 3;
	}
	else if (PortStage == 3 && PortT >= 1.0f) { Shot(N + TEXT("_before_aim")); PortStage = 4; }
	else if (PortStage == 4 && PortT >= 1.3f)
	{
		// AFTER overview: what the controller frames now (court / Ring 2 across the width, inside the HUD-safe band)
		FVector L; FRotator R; float F;
		AZoogiPlayerController::PortraitOverviewPose(G, PC, Yaw, PortCourtZ, false, L, R, F);
		Place(L, R, F);
		CountCircle(L, R, F, PortCourtR, PortCourtOut, PortCourtN, &PortCourtMaxX);
		if (AZoogiArena::OuterOutR > 0.f) CountCircle(L, R, F, AZoogiArena::OuterOutR, PortRing2Out, PortRing2N, nullptr);
		PortVFov = 2.f * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(F * 0.5f)) / PortAspect));
		PortStage = 5;
	}
	else if (PortStage == 5 && PortT >= 1.6f) { Shot(N + TEXT("_after")); PortStage = 6; }
	else if (PortStage == 6 && PortT >= 1.9f)
	{
		// mode switch (SetPortraitCameraMode hook): forced landscape = the pre-portrait overview exactly, forced portrait = the
		// portrait overview, clear = back to auto-detect; all in this one frame (the live camera never ticks in between)
		if (ZPC)
		{
			const bool bAuto0 = ZPC->IsPortraitCameraMode();
			const bool bWantAuto = ZoogiPortrait::Enabled() && PortAspect < 1.f;
			FVector LL, LA, LB; FRotator RL, RA, RB; float FL, FA, FB;
			AZoogiPlayerController::PortraitOverviewPose(G, PC, Yaw, PortCourtZ, true, LL, RL, FL);
			ZPC->SetPortraitCameraMode(false);
			const bool bOffOk = !ZPC->IsPortraitCameraMode();
			AZoogiPlayerController::PortraitOverviewPose(G, PC, Yaw, PortCourtZ, false, LA, RA, FA);
			const bool bOffSame = LA.Equals(LL, 0.01f) && RA.Equals(RL, 0.001f) && FMath::IsNearlyEqual(FA, FL, 0.001f);
			ZPC->SetPortraitCameraMode(true);
			const bool bOnOk = ZPC->IsPortraitCameraMode();
			AZoogiPlayerController::PortraitOverviewPose(G, PC, Yaw, PortCourtZ, false, LB, RB, FB);
			const bool bOnDiff = PortAspect >= 1.f || !LB.Equals(LL, 1.f);
			ZPC->ClearPortraitCameraMode();
			const bool bAutoOk = ZPC->IsPortraitCameraMode() == bWantAuto && bAuto0 == bWantAuto;
			bPortModeOk = bOffOk && bOffSame && bOnOk && bOnDiff && bAutoOk;
			PortModeLine = FString::Printf(TEXT("mode hook: auto %d (want %d) | forced off %d, overview == pre-portrait %d | forced on %d, overview changed %d (boom %.0f vs %.0f) | cleared -> auto %d"),
				bAuto0, bWantAuto, bOffOk, bOffSame, bOnOk, bOnDiff, FVector::Dist(LB, FVector(0, 0, PortCourtZ)), FVector::Dist(LL, FVector(0, 0, PortCourtZ)), bAutoOk);
		}
		// AIM chase view (the controller's PortraitChasePose) for the shooter aiming at the arena centre: shooter at its screen
		// height, aim line straight up the screen, the far side of the court up top, the boom's collision probe
		const AZoogiBall* CB = G ? G->GetActiveBall() : nullptr;
		if (CB && ZoogiPortrait::IsPortrait(PortAspect))
		{
			const FVector S = CB->BallLocation();
			const FVector2D ToC(-S.X, -S.Y);
			const float CY = ToC.Size() > 150.f ? FMath::RadiansToDegrees(FMath::Atan2(ToC.Y, ToC.X)) : Yaw;
			FVector L; FRotator R; float F;
			AZoogiPlayerController::PortraitChasePose(G, PC, S, CY, L, R, F);
			Place(L, R, F);
			const FVector Dir = FRotator(0.f, CY, 0.f).Vector();
			FVector2D SS, AS, FS;
			ZoogiPortrait::Project(L, R, F, PortAspect, S, SS);
			ZoogiPortrait::Project(L, R, F, PortAspect, FVector(0.f, 0.f, S.Z), AS);  // the aim point (arena centre)
			const FVector Far = FVector(0.f, 0.f, S.Z) + Dir * AZoogiPlayerController::PlayRadius(G->Arena);
			const bool bFarFront = ZoogiPortrait::Project(L, R, F, PortAspect, Far, FS); // far rim along the aim line
			ChaseShooterY = (1.f - SS.Y) * 0.5f; ChaseAimX = FMath::Abs(AS.X); ChaseFarY = bFarFront ? FS.Y : -9.f; ChasePitchGot = -R.Pitch;
			ChaseCamH = L.Z - S.Z; ChaseBoomGot = FVector::Dist(L, S);
			FCollisionQueryParams Q(TEXT("ShotTestChaseProbe"), false);
			for (TActorIterator<AZoogiBall> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
			for (TActorIterator<AZoogiOrb> It(GetWorld()); It; ++It) Q.AddIgnoredActor(*It);
			FHitResult Hit; const FVector From = S + FVector(0, 0, 60.f);
			ChaseArm = (GetWorld()->SweepSingleByChannel(Hit, From, L, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(20.f), Q) && !Hit.bStartPenetrating) ? FMath::Clamp(Hit.Time - 0.02f, 0.08f, 1.f) : 1.f;
			bChaseRan = true;
		}
		PortStage = 20;
	}
	else if (PortStage == 20 && PortT >= 2.2f)
	{
		if (bChaseRan) Shot(N + TEXT("_after_aim_chase"));
		PortStage = 21;
	}
	else if (PortStage == 21 && PortT >= 2.5f)
	{
		PC->SetViewTarget(Snap0.ViewTarget.Get()); // the live portrait view
		bPortRestored = true;
		PortStage = 7;
	}
	else if (PortStage == 7 && PortT >= 2.9f && PC->PlayerCameraManager)
	{
		// shooter (centre and its silhouette extremes) inside the safe band of the live view
		const FMinimalViewInfo V = PC->PlayerCameraManager->GetCameraCacheView();
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (B)
		{
			const FVector C = B->BallLocation(); const float Rr = ZoogiConst::ZoogiRadius;
			// tablet framing: the camera behind the shooter, looking up the arena (shooter in front, below the look point)
			FVector2D SC; ZoogiPortrait::Project(V.Location, V.Rotation, V.FOV, PortAspect, C, SC);
			const FVector2D ToS = FVector2D(C - V.Location).GetSafeNormal(), Fwd = FVector2D(V.Rotation.Vector()).GetSafeNormal();
			PortAimPitch = -V.Rotation.Pitch;
			bPortBehind = FVector2D::DotProduct(ToS, Fwd) > 0.5f && SC.Y < (PortBot - PortTop); // shooter ahead, below the band centre
			bPortAimMode = ZPC && ZPC->PortDbgAim;
			if (ZPC) { PortAimArm = ZPC->PortDbgArm; PortAimWant = ZPC->PortDbgWantDist; PortAimGot = FVector::Dist(V.Location, ZPC->PortDbgLook2); }
			const FVector Right = FRotator(0.f, V.Rotation.Yaw, 0.f).RotateVector(FVector(0.f, Rr, 0.f));
			for (const FVector& P : { C, C + FVector(0, 0, Rr), C - FVector(0, 0, Rr), C + Right, C - Right })
			{
				FVector2D S; ++PortShooterN;
				const bool bFront = ZoogiPortrait::Project(V.Location, V.Rotation, V.FOV, PortAspect, P, S);
				if (!bFront || FMath::Abs(S.X) > 1.f || S.Y > Y1 || S.Y < Y0) ++PortShooterOut;
				PortShooterMinY = FMath::Min(PortShooterMinY, S.Y); PortShooterMaxY = FMath::Max(PortShooterMaxY, S.Y);
			}
		}
		Shot(N + TEXT("_after_aim"));
		PortStage = 8;
	}
	else if (PortStage == 8 && PortT >= 3.2f)
	{
		if (SimCam) { SimCam->Destroy(); SimCam = nullptr; }
		bPortDone = true;
	}
}

// ------------------------------------------------------------------ spawn-elevator push-in

void AZoogiShotTestDriver::StartElevPush()
{
	AZoogiGameMode* G = GM();
	bElevPush = true; bElevDone = false; bElevShot = false; ElevPass = 0; ElevT = -0.3f;
	ElevPeakW = 0.f; ElevEndDev = 0.f; ElevMinZ = 1e9f; ElevSpreadBase = ElevSpreadPush = 0.f; ElevPadsOut = ElevPads = 0; bElevSpawned = false;
	ElevFormLen = (G && G->Arena && G->Arena->Forming && G->Arena->Forming->GetLength() > 1.f) ? G->Arena->Forming->GetLength() : 6.f;
	bElevGateLive = AZoogiPlayerController::ElevatorPushAllowed(G);
	ElevPadFloors.Reset();
	if (G) for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) ElevPadFloors.Add(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius));
	const float Scale = AZoogiPlayerController::ArenaCamScale(G);
	float Yaw, Pitch, Fov; FVector Pv;
	AZoogiPlayerController::FormingSweepPose(1.f, Scale, Yaw, Pitch, ElevBaseDist, Fov, Pv);
	// portrait: exactly the controller's Forming branch (PortraitBoom + band; identical to before on landscape)
	const APlayerController* EPC = UGameplayStatics::GetPlayerController(this, 0);
	const float EA = ZoogiPortrait::ViewAspect(EPC);
	float ET = 0.f, EB = 0.f; ZoogiPortrait::Band(EPC, EA, ET, EB);
	float EH = Fov;
	ElevBaseDist = AZoogiPlayerController::PortraitBoom(G, EA, ET, EB, Yaw, Pitch, Fov, ElevBaseDist, true, Pv, EH);
	ElevFitPivot = Pv; ElevFitPitch = Pitch; ElevFitDist = ElevBaseDist;
	AZoogiPlayerController::ElevatorPushFit(ElevPadFloors, Yaw, EH, ElevBaseDist, ElevFitPivot, ElevFitPitch, ElevFitDist, EA, ET, EB);
	CaseMid = 1e6f; // explicit _before / _after frames instead of the generic mid frame
	FActorSpawnParameters SP; SP.ObjectFlags |= RF_Transient;
	SimCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Snap0.POV.Location, Snap0.POV.Rotation, SP);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && SimCam) PC->SetViewTarget(SimCam);
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   elevator push-in: %d pads, forming %.2f s, fit pivot %s pitch %.1f dist %.0f (sweep end %.0f), live gate %d"),
		ElevPadFloors.Num(), ElevFormLen, *ElevFitPivot.ToString(), ElevFitPitch, ElevFitDist, ElevBaseDist, bElevGateLive);
}

void AZoogiShotTestDriver::TickElevPush(float Dt)
{
	if (bElevDone) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AZoogiGameMode* G = GM();
	if (!SimCam || !PC) { bElevDone = true; return; }
	const float PrevT = ElevT;
	ElevT += Dt;
	if (PrevT < 0.f && ElevT >= 0.f && G && AZoogiSpawnElevator::IsEnabled()) { AZoogiSpawnElevator::PlayAll(G); bElevSpawned = true; } // the real elevators, as the GameMode starts them
	const float W = AZoogiPlayerController::ElevPushWindow;
	const float Pg = FMath::Clamp(1.f - (W - FMath::Min(ElevT, W)) / ElevFormLen, 0.f, 1.f);
	const float Scale = AZoogiPlayerController::ArenaCamScale(G);
	float Yaw, Pitch, Dist, Fov; FVector Pv;
	AZoogiPlayerController::FormingSweepPose(Pg, Scale, Yaw, Pitch, Dist, Fov, Pv);
	const float EA = ZoogiPortrait::ViewAspect(PC);
	float ET = 0.f, EB = 0.f; ZoogiPortrait::Band(PC, EA, ET, EB);
	{ float EH = Fov; Dist = AZoogiPlayerController::PortraitBoom(G, EA, ET, EB, Yaw, Pitch, Fov, Dist, true, Pv, EH); Fov = EH; }
	const FVector BaseLoc = AZoogiPlayerController::BoomLocation(Pv, Yaw, Pitch, Dist);
	const FRotator BaseRot = ZoogiPortrait::BandRotation(BaseLoc, Pv, Fov, EA, ET, EB);
	FVector Loc = BaseLoc; FRotator Rot = BaseRot;
	if (ElevPass == 1 && ElevT > 0.f && ElevT < W)
	{
		// exactly the controller's blend (UpdateCamera, Forming branch)
		const float Wt = AZoogiPlayerController::ElevatorPushWeight(ElevT);
		const FVector P2 = FMath::Lerp(Pv, ElevFitPivot, Wt);
		const float Pi2 = FMath::Lerp(Pitch, ElevFitPitch, Wt);
		const float D2 = FMath::Lerp(Dist, ElevFitDist, Wt);
		Loc = AZoogiPlayerController::BoomLocation(P2, Yaw, Pi2, D2);
		Rot = ZoogiPortrait::BandRotation(Loc, P2, Fov, EA, ET, EB);
		ElevPeakW = FMath::Max(ElevPeakW, Wt);
		if (ElevT >= AZoogiPlayerController::ElevPushOutEnd) ElevEndDev = FMath::Max(ElevEndDev, FMath::Max(FVector::Dist(Loc, BaseLoc), (Rot - BaseRot).GetNormalized().Euler().GetAbsMax()));
		ElevMinZ = FMath::Min(ElevMinZ, Loc.Z);
		if (Wt >= 0.999f)
		{
			const float TanH = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)), TanV = TanH / (ZoogiPortrait::IsPortrait(EA) ? EA : 16.f / 9.f);
			for (const FVector& P : ElevPadFloors)
				for (const FVector& O : { FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 130.f) })
				{
					const FVector L = Rot.UnrotateVector(P + O - Loc);
					ElevPads++;
					if (L.X < 1.f || FMath::Abs(L.Y) / L.X > TanH || L.Z / L.X > TanV * (1.f - 2.f * ET) || -L.Z / L.X > TanV * (1.f - 2.f * EB)) ElevPadsOut++;
				}
		}
	}
	// pad spread on screen (max angle between two pads), sweep vs push-in, at the same moment
	if (ElevT >= 0.6f && ElevT < 0.6f + Dt && ElevPadFloors.Num() >= 2)
	{
		float Spread = 0.f;
		for (int32 i = 0; i < ElevPadFloors.Num(); ++i) for (int32 j = i + 1; j < ElevPadFloors.Num(); ++j)
		{
			const FVector A = (ElevPadFloors[i] - Loc).GetSafeNormal(), B = (ElevPadFloors[j] - Loc).GetSafeNormal();
			Spread = FMath::Max(Spread, FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f)));
		}
		(ElevPass == 0 ? ElevSpreadBase : ElevSpreadPush) = Spread;
	}
	SimCam->SetActorLocation(Loc);
	SimCam->SetActorRotation(Rot);
	SimCam->GetCameraComponent()->SetFieldOfView(Fov);
	if (!bElevShot && ElevT >= 0.7f) { bElevShot = true; Shot(Cases[CaseIdx].Name + (ElevPass == 0 ? TEXT("_before") : TEXT("_after"))); }
	if (ElevPass == 1 && ElevT >= 1.20f && ElevT < 1.20f + Dt) Shot(Cases[CaseIdx].Name + TEXT("_after_end"));
	// each pass lasts until the elevators have finished (sink + destroy ~2.2 s after their stagger)
	bool bAny = false;
	for (TActorIterator<AZoogiSpawnElevator> It(GetWorld()); It; ++It) { bAny = true; break; }
	if (ElevT >= 1.6f && (!bAny || ElevT > 4.f))
	{
		if (ElevPass == 0) { ElevPass = 1; ElevT = -0.3f; bElevShot = false; }
		else { PC->SetViewTarget(Snap0.ViewTarget.Get()); bElevDone = true; }
	}
}

// ------------------------------------------------------------------ aim view (Evan Oct 17)

static const float GAimYaws[] = { 0.f, 35.f, -50.f, 120.f, -170.f, 175.f, 4.f }; // offsets from the start yaw (incl. a wrap and a small input)

void AZoogiShotTestDriver::StartAimView()
{
	AZoogiGameMode* G = GM();
	AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AZoogiBall* S = G ? G->GetActiveBall() : nullptr;
	bAimView = true; bAimViewDone = !(PC && S); bAimViewSeen = true;
	AimIdx = -1; AimT = 0.f; AimHoldT = 0.f; AimSamples = AimBadYaw = AimBadY = AimBadX = AimBadArrow = 0;
	AimMaxYawErr = AimMaxYErr = AimMaxXErr = 0.f; AimEngineY = -1.f; AimReturnT = -1.f;
	CaseMid = 1e6f;
	if (bAimViewDone) return;
	AimYaw0 = S->AimYaw; bAimFreeAim0 = G->bFreeAim; AimFreePoint0 = G->FreeAimPoint;
	PC->bAimViewForce = true;
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   aim view: P%d aim yaw %.1f, pitch %.0f boom %.0f screen y %.2f yaw follow %.2f s, view aspect %.3f"),
		S->PlayerIndex + 1, AimYaw0, ZoogiAimView::Pitch(), ZoogiAimView::Boom(), ZoogiAimView::ScreenY(), ZoogiAimView::YawTime(), ZoogiFramed::ViewAspect(PC));
}

void AZoogiShotTestDriver::TickAimView(float Dt)
{
	if (bAimViewDone) return;
	AZoogiGameMode* G = GM();
	AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AZoogiBall* S = G ? G->GetActiveBall() : nullptr;
	if (!PC || !S || !PC->PlayerCameraManager) { bAimViewDone = true; return; }
	AimT += Dt;
	const int32 N = UE_ARRAY_COUNT(GAimYaws);
	if (AimIdx < N)
	{
		bAimViewSeen &= PC->bAimViewActive || AimT < 0.05f;
		AimHoldT += Dt;
		const float Hold = AimIdx < 0 ? 2.5f : 1.0f; // first: settle into the view from the Awaken view; then one aim yaw per second
		if (AimHoldT >= Hold)
		{
			{
				// settled on this yaw: camera yaw on the aim yaw, the Zoogi at AimScreenY centred, the arrow straight up the screen
				const FVector CL = PC->PlayerCameraManager->GetCameraLocation();
				const FRotator CR = PC->PlayerCameraManager->GetCameraRotation();
				const float HF = PC->PlayerCameraManager->GetFOVAngle();
				const float A = ZoogiFramed::ViewAspect(PC);
				const FVector SL = S->BallLocation();
				const FVector Dir = FRotator(0.f, S->AimYaw, 0.f).Vector();
				FVector2D PS, PT;
				const bool bS = ZoogiAimView::Project(CL, CR, HF, A, SL, PS);
				const bool bT = ZoogiAimView::Project(CL, CR, HF, A, SL + Dir * 300.f, PT);
				const float YawErr = FMath::Abs(FRotator::NormalizeAxis(CR.Yaw - S->AimYaw));
				const float YTop = (1.f - PS.Y) * 0.5f;
				const float YErr = FMath::Abs(YTop - ZoogiAimView::ScreenY());
				const float XErr = FMath::Abs(PS.X) * 0.5f;
				const bool bArrow = bS && bT && PT.Y > PS.Y + 0.05f && FMath::Abs(PT.X - PS.X) * 0.5f <= 0.02f;
				AimSamples++;
				AimBadYaw += YawErr > 3.f; AimBadY += !bS || YErr > 0.03f; AimBadX += !bS || XErr > 0.02f; AimBadArrow += !bArrow;
				AimMaxYawErr = FMath::Max(AimMaxYawErr, YawErr); AimMaxYErr = FMath::Max(AimMaxYErr, YErr); AimMaxXErr = FMath::Max(AimMaxXErr, XErr);
				AimPitchGot = -CR.Pitch; AimBoomGot = FVector::Dist(CL, SL);
				FVector2D Scr; int32 VX = 0, VY = 0; PC->GetViewportSize(VX, VY);
				if (AimIdx == -1 && VY > 0 && PC->ProjectWorldLocationToScreen(SL, Scr)) AimEngineY = Scr.Y / VY;
				UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   aim view yaw #%d: aim %.1f cam %.1f (err %.2f) | Zoogi at x %.3f y %.3f of the view (want 0.5 / %.2f) | arrow tip x %.3f y %.3f | pitch %.1f boom %.0f fov %.1f"),
					AimIdx + 1, S->AimYaw, CR.Yaw, YawErr, 0.5f + PS.X * 0.5f, YTop, ZoogiAimView::ScreenY(), 0.5f + PT.X * 0.5f, (1.f - PT.Y) * 0.5f, AimPitchGot, AimBoomGot, HF);
				if (AimIdx == -1) Shot(Cases[CaseIdx].Name + TEXT("_aim"));
				if (AimIdx == 3) Shot(Cases[CaseIdx].Name + TEXT("_aim_turned"));
			}
			++AimIdx; AimHoldT = 0.f;
			if (AimIdx < N) S->AimYaw = FRotator::NormalizeAxis(AimYaw0 + GAimYaws[AimIdx]); // the drag arrow's heading
			else
			{
				// restore the aim and hand the camera back to the Awaken view
				S->AimYaw = AimYaw0; G->bFreeAim = bAimFreeAim0; G->FreeAimPoint = AimFreePoint0;
				PC->bAimViewForce = false; AimReturnT = 0.f;
			}
		}
		return;
	}
	// back to the exact pre-case view (the generic restore check compares it at 0.05 cm / 0.01 deg)
	AimReturnT += Dt;
	const FMinimalViewInfo& V0 = Snap0.POV;
	const bool bBack = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), V0.Location) <= 0.04f
		&& (PC->PlayerCameraManager->GetCameraRotation() - V0.Rotation).GetNormalized().Euler().GetAbsMax() <= 0.008f
		&& FMath::Abs(PC->PlayerCameraManager->GetFOVAngle() - V0.FOV) <= 0.008f;
	if ((bBack && AimReturnT > 1.f) || AimReturnT > 9.f) bAimViewDone = true;
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
	case ZoogiShotTest::ECaseKind::RingOutSimLegacy:
	case ZoogiShotTest::ECaseKind::RingOutSimFixed:
		bRingOutLegacy = C.Kind == ZoogiShotTest::ECaseKind::RingOutSimLegacy;
		StartRingOut(C.Type, true);
		break;
	case ZoogiShotTest::ECaseKind::RingOutDirector:
		StartRingOut(C.Type, false);
		break;
	case ZoogiShotTest::ECaseKind::ElevatorPush:
		StartElevPush();
		break;
	case ZoogiShotTest::ECaseKind::Portrait:
		StartPortrait();
		break;
	case ZoogiShotTest::ECaseKind::AimView:
		StartAimView();
		break;
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
	if (bBankMove && TempProp)
	{
		BankT += GetWorld()->GetDeltaSeconds();
		const float Dd = BankSpeed * (BankT - BankTc);
		TempProp->SetActorLocation(Dd < 0.f ? BankE + BankIn * Dd : BankE + BankOut * Dd);
		if (!bBankHitLogged && Dd >= 0.f) { bBankHitLogged = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI SHOTTEST   subject passed the contact at %.2f s game time"), BankT); }
	}
	if (ArcProp && !ArcProp->IsHidden())
	{
		ArcTime += GetWorld()->GetDeltaSeconds();
		const float Flight = 2.f * ArcVel.Z / 980.f;
		if (ArcTime > Flight) ArcTime -= Flight;
		const FVector Pos = ArcStart + ArcVel * ArcTime + FVector(0.f, 0.f, -490.f * ArcTime * ArcTime);
		MaxArcHeight = FMath::Max(MaxArcHeight, Pos.Z - ArcStart.Z);
		ArcProp->SetActorLocation(Pos);
	}
	if (bRingOut) TickRingOut(GetWorld()->GetDeltaSeconds());
	if (bElevPush) TickElevPush(GetWorld()->GetDeltaSeconds());
	if (bPortrait) TickPortrait(RealDt);
	if (bAimView) TickAimView(GetWorld()->GetDeltaSeconds());
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
	const float Dur = Cases[CaseIdx].Kind == ZoogiShotTest::ECaseKind::Shot ? CaseMid / 0.55f : Cases[CaseIdx].Kind == ZoogiShotTest::ECaseKind::AimView ? 12.f : 3.f;
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
	const bool bRingWait = (bRingOut && !bRingOutDone) || (bElevPush && !bElevDone) || (bPortrait && !bPortDone) || (bAimView && !bAimViewDone);
	if (bIdle && !bRingWait && StepTime > 0.2f && (C.Kind != ZoogiShotTest::ECaseKind::LevelEnd))
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
	if (C.Kind == ZoogiShotTest::ECaseKind::RingOutSimLegacy || C.Kind == ZoogiShotTest::ECaseKind::RingOutSimFixed || C.Kind == ZoogiShotTest::ECaseKind::RingOutDirector)
	{
		// Arena centre in the view frustum (gameplay follow: every frame; director shots: during the fall, at most 1 s at a time)
		// and the camera never below the court (floor top + one Zoogi radius).
		const float MaxLoss = C.Kind == ZoogiShotTest::ECaseKind::RingOutDirector ? 1.0f : 0.f;
		const bool bFrame = RingLossMax <= MaxLoss + 1e-3f;
		const bool bHeight = RingMinCamZ >= ZoogiConst::ZoogiRadius - 1.f;
		const bool bFell = FallT > FallEdgeT + 1.5f;
		const FString Line = FString::Printf(TEXT("ring-out fall %.2f s (edge at %.2f s, prop fell to z=%.0f): arena centre outside the frustum %d/%d frames, longest %.2f s (allowed %.2f) | lowest camera z=%.0f (court %.0f)"),
			FallT, FallEdgeT, FallPos(FallT).Z, RingOutFrames, RingFrames, RingLossMax, MaxLoss, RingMinCamZ, ZoogiConst::ZoogiRadius);
		if (C.Kind == ZoogiShotTest::ECaseKind::RingOutSimLegacy)
		{
			// BEFORE: the pre-fix rule, recorded for comparison only (expected to lose the arena / dive).
			Extra.Add(FString::Printf(TEXT("BEFORE (legacy rule, informational: %s) %s"), (bFrame && bHeight) ? TEXT("would pass") : TEXT("reproduces the bug"), *Line));
			bPass &= bFell;
		}
		else
		{
			bPass &= bFrame && bHeight && bFell;
			Extra.Add(Line);
			if (C.Kind == ZoogiShotTest::ECaseKind::RingOutSimFixed)
			{
				// Oct 16: the out-of-play test uses this arena's own court (knock-off radius / Frozen Ring 2 outer ring), not a fixed 1550.
				const AZoogiGameMode* G = GM();
				const AZoogiArena* A = G ? G->Arena : nullptr;
				const float PR = AZoogiPlayerController::PlayRadius(A);
				const float OutR = FMath::Max(PR, AZoogiArena::OuterOutR);
				bool bIn = true, bOff = true;
				for (int32 k = 0; k < 8; ++k)
				{
					const FVector2D U = FVector2D(1.f, 0.f).GetRotated(45.f * k + 10.f);
					bIn &= !AZoogiPlayerController::IsOffPlayArea(A, U * (PR - 20.f));
					bOff &= AZoogiPlayerController::IsOffPlayArea(A, U * (OutR + 20.f));
				}
				const bool bRadOK = A && FMath::IsNearlyEqual(PR, A->GetKnockoffRadius()) && bIn && bOff;
				bPass &= bRadOK;
				Extra.Add(FString::Printf(TEXT("play radius %.0f (arena knock-off %.0f, outer %.0f): in at r-20 %d, off past the outer edge %d"), PR, A ? A->GetKnockoffRadius() : -1.f, AZoogiArena::OuterOutR, bIn, bOff));
			}
		}
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::ElevatorPush)
	{
		// The push-in reaches full weight, keeps every pad (and its rising Zoogi) in frame, makes the pads read bigger, never dips
		// below the court, and is back at the exact sweep pose for the intro's last frames; the live game skips it in test runs.
		const bool bPeak = ElevPeakW >= 0.999f;
		const bool bPadsIn = ElevPads > 0 && ElevPadsOut == 0;
		const float Gain = ElevSpreadBase > 1e-3f ? ElevSpreadPush / ElevSpreadBase : 0.f;
		const bool bBigger = Gain >= 1.3f;
		const bool bExact = ElevEndDev == 0.f;
		const bool bHigh = ElevMinZ >= ZoogiConst::ZoogiRadius;
		const bool bGate = !bElevGateLive;
		bPass &= bElevDone && bPeak && bPadsIn && bBigger && bExact && bHigh && bGate;
		Extra.Add(FString::Printf(TEXT("elevator push-in: fit pivot %s pitch %.1f dist %.0f (sweep end %.0f) | peak weight %.3f | pads out of frame %d/%d samples | pad spread x%.2f (need 1.30) | sweep-pose deviation over the last %.2f s: %.4f (need 0) | lowest camera z=%.0f | live gate in this test run: %s | elevators spawned %d"),
			*ElevFitPivot.ToString(), ElevFitPitch, ElevFitDist, ElevBaseDist, ElevPeakW, ElevPadsOut, ElevPads, Gain, AZoogiPlayerController::ElevPushWindow - AZoogiPlayerController::ElevPushOutEnd, ElevEndDev, ElevMinZ,
			bElevGateLive ? TEXT("ON (should be skipped)") : TEXT("skipped"), bElevSpawned));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::Portrait)
	{
		const bool bPort = ZoogiPortrait::IsPortrait(PortAspect);
		const bool bCourt = PortCourtN > 0 && PortCourtOut == 0;
		const bool bRing2 = AZoogiArena::OuterOutR <= 0.f || (PortRing2N > 0 && PortRing2Out == 0);
		const bool bShooter = PortShooterN > 0 && PortShooterOut == 0;
		const bool bVFov = PortVFov <= ZoogiPortrait::VFovMax() + 0.5f;
		const bool bTablet = bPortAimMode && bPortBehind && PortAimPitch >= 40.f && PortAimPitch <= 62.f;
		bool bOk = bPortDone && bPortRestored && bPortModeOk;
		// AIM chase view: shooter at PortraitAimScreenY (+-0.03), aim line straight up (|x| <= 0.02), the far rim in the upper half
		// of the frame below the top band, look pitch = PortraitAimPitch
		const float WantY = ZoogiPortrait::ChaseScreenY();
		const bool bChase = bChaseRan && FMath::Abs(ChaseShooterY - WantY) <= 0.03f && ChaseAimX <= 0.02f && ChaseFarY > 0.f && ChaseFarY <= 1.f - 2.f * PortTop
			&& FMath::Abs(ChasePitchGot - ZoogiPortrait::ChasePitch()) <= 0.5f;
		if (bPort) bOk &= bCourt && bRing2 && bShooter && bVFov && bTablet && bChase;
		bPass &= bOk;
		Extra.Add(FString::Printf(TEXT("portrait framing: view aspect %.3f (%s) band top %.2f bottom %.2f | court r%.0f samples outside width/band %d/%d (widest |x| %.3f) | Ring 2 %s | shooter samples outside the band %d/%d (y %.3f..%.3f, band %.2f..%.2f) | overview vertical FOV %.1f (cap %.0f) | BEFORE rule: %.0f%% of the court outside (informational)"),
			PortAspect, bPort ? TEXT("portrait") : TEXT("landscape: checks informational"), PortTop, PortBot, PortCourtR, PortCourtOut, PortCourtN, PortCourtMaxX,
			AZoogiArena::OuterOutR > 0.f ? *FString::Printf(TEXT("r%.0f outside %d/%d"), AZoogiArena::OuterOutR, PortRing2Out, PortRing2N) : TEXT("n/a (no outer ring)"),
			PortShooterOut, PortShooterN, PortShooterMinY, PortShooterMaxY, -1.f + 2.f * PortBot, 1.f - 2.f * PortTop, PortVFov, ZoogiPortrait::VFovMax(), PortLegacyOut * 100.f));
		Extra.Add(FString::Printf(TEXT("aim view: portrait aim framing %d, camera behind the shooter looking up the arena %d, pitch %.1f (need 40..62), boom %.0f of %.0f wanted (probe arm %.2f) | %s"),
			bPortAimMode, bPortBehind, PortAimPitch, PortAimGot, PortAimWant, PortAimArm, *PortModeLine));
		Extra.Add(FString::Printf(TEXT("AIM chase view: ran %d | shooter at %.3f of the screen height (want %.2f) | aim point |x| %.3f | far rim y %.3f (upper half, below the top band) | look pitch %.1f (want %.1f) | camera %.0f cm from the shooter, %.0f cm above | collision probe arm %.2f (1 = clear; the live camera pulls in to it)"),
			bChaseRan, ChaseShooterY, WantY, ChaseAimX, ChaseFarY, ChasePitchGot, ZoogiPortrait::ChasePitch(), ChaseBoomGot, ChaseCamH, ChaseArm));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::AimView)
	{
		// The live aim view: behind the Zoogi along the arrow at every aim yaw (camera yaw within 3 deg once settled), the Zoogi
		// at AimScreenY centred, the arrow straight up the screen, then back to the exact pre-case view.
		const int32 Want = UE_ARRAY_COUNT(GAimYaws) + 1;
		const bool bOK = bAimViewDone && bAimViewSeen && AimSamples == Want && AimBadYaw == 0 && AimBadY == 0 && AimBadX == 0 && AimBadArrow == 0 && AimReturnT >= 0.f && AimReturnT <= 9.f;
		bPass &= bOK;
		Extra.Add(FString::Printf(TEXT("aim view: active throughout %d | %d/%d aim yaws sampled | camera yaw off the aim yaw: max %.2f deg (need <= 3), %d bad | Zoogi height off %.2f: max %.3f (need <= 0.03), %d bad | off centre max %.3f (need <= 0.02), %d bad | arrow not straight up: %d | pitch %.1f boom %.0f | engine projection y %.3f | back at the pre-case view after %.2f s"),
			bAimViewSeen, AimSamples, Want, AimMaxYawErr, AimBadYaw, ZoogiAimView::ScreenY(), AimMaxYErr, AimBadY, AimMaxXErr, AimBadX, AimBadArrow, AimPitchGot, AimBoomGot, AimEngineY, AimReturnT));
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
			bBankMove = false; bBankHitLogged = false; bRingOut = false; bElevPush = false; bPortrait = false;
			if (bAimView) { if (AZoogiPlayerController* APC = Cast<AZoogiPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) APC->bAimViewForce = false; bAimView = false; }
			if (SimCam) { SimCam->Destroy(); SimCam = nullptr; }
			bElevPush = false;
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
