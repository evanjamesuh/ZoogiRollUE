#include "ZoogiShotTest.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
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
	Add(ZoogiShotTest::ECaseKind::TimeScale, TEXT("TimeScale_Ease_with_LowAngle_MainView"), EZoogiShotType::LowAngleHero, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::TimeScaleCancel, TEXT("TimeScale_Cancel"), EZoogiShotType::CloseUp, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AirborneSlowMoCancel, TEXT("Airborne_SlowMo_Cancel_MainView"), EZoogiShotType::Airborne, EZoogiShotOutput::MainView);
	Add(ZoogiShotTest::ECaseKind::AirborneSlowMoRefused, TEXT("Airborne_SlowMo_PolicyDisabled_RenderTarget"), EZoogiShotType::Airborne, EZoogiShotOutput::RenderTarget);
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
	FReadSurfaceDataFlags Flags(RCM_UNorm);
	Flags.SetLinearToGamma(RT->RenderTargetFormat == RTF_RGBA16f);
	if (!Res->ReadPixels(Px, Flags) || Px.Num() != RT->SizeX * RT->SizeY) return;
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
	case EZoogiShotType::PartnerTwoShot:
	case EZoogiShotType::TrickAllyBump:
		UseS = SecondaryFor(F);
		OutSetup = FString::Printf(TEXT("secondary = %s"), *GetNameSafe(UseS));
		break;
	case EZoogiShotType::TrickOrbRelay:
	{
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
		OutSetup = FString::Printf(TEXT("secondary (relay target) = %s"), *GetNameSafe(UseS));
		break;
	}
	case EZoogiShotType::TrickBank:
	case EZoogiShotType::TrickWallRedirect:
		if (F)
		{
			// A point on the rim off to one side of P1, standing in for the bank / wall contact point.
			const FVector FL = F->GetActorLocation();
			FVector2D U(FL.X, FL.Y);
			if (U.Size() < 1.f) U = FVector2D(FRotator(0.f, Snap0.POV.Rotation.Yaw, 0.f).Vector());
			U.Normalize();
			const FVector2D R = U.GetRotated(Type == EZoogiShotType::TrickBank ? 35.f : -50.f) * (AZoogiArena::ActiveKnockoffR * 0.92f);
			P.bHasEventLocation = true;
			P.EventLocation = FVector(R.X, R.Y, FL.Z);
			OutSetup = FString::Printf(TEXT("event point (rim) = %s"), *P.EventLocation.ToString());
		}
		break;
	case EZoogiShotType::TrickBoundarySave:
		OutSetup = TEXT("event point = arena centre (default)");
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

// ------------------------------------------------------------------ cases

void AZoogiShotTestDriver::StartCase()
{
	UZoogiEpisodeCameraDirector* D = Dir();
	const ZoogiShotTest::FCase& C = Cases[CaseIdx];
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
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused) && C.Output == EZoogiShotOutput::RenderTarget)
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
		if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused) && C.Output == EZoogiShotOutput::RenderTarget && Handles.Num())
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
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused) && C.Output == EZoogiShotOutput::RenderTarget)
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
		if (!bTimeScaleStarted) Extra.Add(TEXT("ease REFUSED by the physics-at-rest gate (see CAMDIR line)"));
		else Extra.Add(FString::Printf(TEXT("ease ran: min effective dilation %.3f, cinematic dilation back to %.6f (prior %.6f)"), MinEffDilation, Snap1.CinematicTimeDilation, Snap0.CinematicTimeDilation));
		bPass &= bTimeScaleStarted && MinEffDilation < 0.5f;
	}
	if ((C.Kind == ZoogiShotTest::ECaseKind::Shot && C.Type == EZoogiShotType::Airborne) || C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoCancel)
	{
		const bool bSlowed = MinEffDilation < (C.Kind == ZoogiShotTest::ECaseKind::Shot ? 0.5f : 0.9f);
		bPass &= bSlowed && !D->IsTimeScaleActive();
		Extra.Add(FString::Printf(TEXT("slow-mo entry %s: min effective dilation %.3f, cinematic dilation back to %.6f (prior %.6f), arc peak %.0f cm"),
			bSlowed ? TEXT("ran") : TEXT("DID NOT RUN"), MinEffDilation, Snap1.CinematicTimeDilation, Snap0.CinematicTimeDilation, MaxArcHeight));
	}
	if (C.Kind == ZoogiShotTest::ECaseKind::AirborneSlowMoRefused)
	{
		const bool bNormal = MinEffDilation > 0.999f;
		bPass &= bNormal;
		Extra.Add(bNormal ? TEXT("slow-mo refused under the Disabled policy, shot played at normal speed") : TEXT("time dilation CHANGED under the Disabled policy"));
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
