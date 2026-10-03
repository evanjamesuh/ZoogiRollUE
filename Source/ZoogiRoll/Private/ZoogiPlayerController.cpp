#include "ZoogiPlayerController.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiHUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/GameViewportClient.h"

namespace ZoogiCam
{
	// alpha-camera-spec.md D22. Distances in cm; FOV is horizontal 90 (about 60 vertical at 16:9).
	constexpr float AimPitch = 30.f;
	constexpr float AimDist = 700.f;
	constexpr float DragPitch = 56.f;
	constexpr float DragDist = 1800.f;
	constexpr float FollowPitch = 55.f;
	constexpr float FollowDist = 2400.f;
	constexpr float LRise = 0.7f;
	constexpr float LPos = 2.5f;
	constexpr float LLook = 10.f;
	constexpr float LYawFollow = 0.5f;
	constexpr float YawCap = 20.f;
	constexpr float LResetYaw = 4.6f;
	constexpr float LResetDolly = 1.9f;
	constexpr float LDrag = 6.f;
	constexpr float MinZ = -800.f;

	inline float Damp(float X, float Target, float Lambda, float Dt) { return X + (Target - X) * (1.f - FMath::Exp(-Lambda * Dt)); }
	inline FVector Damp(const FVector& X, const FVector& Target, float Lambda, float Dt) { return X + (Target - X) * (1.f - FMath::Exp(-Lambda * Dt)); }
	inline float DampAngle(float X, float Target, float Lambda, float Dt)
	{
		const float Delta = FRotator::NormalizeAxis(Target - X);
		return FRotator::NormalizeAxis(X + Delta * (1.f - FMath::Exp(-Lambda * Dt)));
	}
}

AZoogiPlayerController::AZoogiPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AZoogiGameMode* AZoogiPlayerController::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

void AZoogiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-35, 0, 0), FVector(-2500, 0, 1500)));
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(90.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		SetViewTarget(Cam);
	}
	Pitch = 38.f;
	Dist = 3600.f;
	bHumanTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHumanTest"));
}

bool AZoogiPlayerController::LmbPressed() const
{
	return bHumanTest ? (bSimDown && !bSimPrevDown) : WasInputKeyJustPressed(EKeys::LeftMouseButton);
}

bool AZoogiPlayerController::LmbDown() const
{
	return bHumanTest ? bSimDown : IsInputKeyDown(EKeys::LeftMouseButton);
}

void AZoogiPlayerController::UpdateHumanTest(float Dt)
{
	AZoogiGameMode* G = GM();
	bSimPrevDown = bSimDown;
	if (!G || !G->IsHumanAiming())
	{
		SimClock = 0.f;
		bSimDown = false;
		return;
	}
	SimClock += Dt;
	AZoogiBall* Ball = G->GetActiveBall();
	int32 VX = 0, VY = 0;
	GetViewportSize(VX, VY);
	FVector2D BallScreen;
	if (!Ball || !ProjectWorldLocationToScreen(Ball->BallLocation(), BallScreen)) return;
	const float Start = 2.0f, Pull = 1.1f;
	if (SimClock < Start) return;
	const float T = FMath::Clamp((SimClock - Start) / Pull, 0.f, 1.f);
	if (SimClock < Start + Pull + 0.25f)
	{
		if (!bSimDown) SimMouse = BallScreen + FVector2D(0.f, 10.f);
		bSimDown = true;
		// Pull straight back (down the screen) and a little to the side.
		SimMouse = BallScreen + FVector2D(0.f, 10.f) + FVector2D(-0.05f * VX, 0.30f * VY) * FMath::InterpEaseOut(0.f, 1.f, T, 2.f);
	}
	else
	{
		bSimDown = false;
	}
}

bool AZoogiPlayerController::CursorGround(FVector& Out, float PlaneZ) const
{
	float MX = 0.f, MY = 0.f;
	if (bHumanTest) { MX = SimMouse.X; MY = SimMouse.Y; }
	else if (!GetMousePosition(MX, MY)) return false;
	FVector O, D;
	if (!DeprojectScreenPositionToWorld(MX, MY, O, D)) return false;
	if (D.Z > -0.01f) return false;
	const float T = (PlaneZ - O.Z) / D.Z;
	Out = O + D * T;
	return true;
}

void AZoogiPlayerController::OnTurnStarted(AZoogiBall* Ball)
{
	// C18: hard cut, keep the current offset, then swoop into the aim pose.
	CamTarget = Ball;
	CamMode = EZoogiCamMode::Aim;
	bHoldPose = false;
	bDragging = false;
	if (Ball)
	{
		Pivot = Ball->BallLocation();
		Look = Pivot;
	}
}

void AZoogiPlayerController::OnShotFired(AZoogiBall* Ball)
{
	CamTarget = Ball;
	CamMode = EZoogiCamMode::Follow;
	bDragging = false;
	if (Ball) Yaw = Ball->AimYaw;
}

void AZoogiPlayerController::OnGameOver(AZoogiBall* Winner)
{
	CamMode = EZoogiCamMode::Menu;
	bDragging = false;
}

void AZoogiPlayerController::OnMenu()
{
	CamMode = EZoogiCamMode::Menu;
	bDragging = false;
}

void AZoogiPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	if (bHumanTest) UpdateHumanTest(Dt);
	HandleInput(Dt);
	UpdateCamera(Dt);
}

void AZoogiPlayerController::HandleInput(float Dt)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	AZoogiHUD* Hud = Cast<AZoogiHUD>(GetHUD());

	float MX = 0.f, MY = 0.f;
	GetMousePosition(MX, MY);
	if (bHumanTest) { MX = SimMouse.X; MY = SimMouse.Y; }

	// Buttons (menu, results, in-match corner buttons).
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && Hud)
	{
		FName Action; int32 Index = 0;
		if (Hud->HitTest(FVector2D(MX, MY), Action, Index))
		{
			G->MenuAction(Action, Index);
			return;
		}
	}

	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)
	{
		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) G->StartMatch();
		if (WasInputKeyJustPressed(EKeys::Escape) && G->Phase == EZoogiPhase::GameOver) G->ReturnToMenu();
		return;
	}

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (bDragging) { bDragging = false; G->SetAim(false, FVector::ZeroVector); }
		else { G->ReturnToMenu(); }
		return;
	}
	if (WasInputKeyJustPressed(EKeys::R) && IsInputKeyDown(EKeys::LeftControl)) { G->StartMatch(); return; }

	if (!G->IsHumanAiming())
	{
		bDragging = false;
		return;
	}

	// Rotate the aim heading (camera orbits) when not pulling: A/D, arrows, or right-mouse drag.
	if (!bDragging)
	{
		float Turn = 0.f;
		if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Turn -= 1.f;
		if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Turn += 1.f;
		if (Turn != 0.f) G->RotateAim(Turn * 90.f * Dt);
		if (WasInputKeyJustPressed(EKeys::RightMouseButton)) { bRotating = true; LastMouseX = MX; }
		if (bRotating)
		{
			G->RotateAim((MX - LastMouseX) * 0.25f);
			LastMouseX = MX;
			if (!IsInputKeyDown(EKeys::RightMouseButton)) bRotating = false;
		}
	}

	// Slingshot: press, pull back, release. Pull = press point minus current point on the ground.
	if (LmbPressed())
	{
		FVector P;
		if (CursorGround(P))
		{
			bDragging = true;
			DragStart = P;
			DragCurrent = P;
			G->SetAim(true, FVector::ZeroVector);
		}
	}
	if (bDragging)
	{
		FVector P;
		if (CursorGround(P)) DragCurrent = P;
		G->SetAim(true, DragStart - DragCurrent);
		if (WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			bDragging = false;
			G->SetAim(false, FVector::ZeroVector);
		}
		else if (!LmbDown())
		{
			bDragging = false;
			const bool bShot = G->ReleaseAim();
			UE_LOG(LogZoogi, Log, TEXT("Mouse release: pull %.0f cm on the ground -> %s"), (DragStart - DragCurrent).Size2D(), bShot ? TEXT("shot") : TEXT("cancelled (too short)"));
		}
	}
}

void AZoogiPlayerController::UpdateCamera(float Dt)
{
	using namespace ZoogiCam;
	AZoogiGameMode* G = GM();
	if (!Cam || !G) return;

	AZoogiBall* T = CamTarget.Get();
	FVector Target = T ? T->BallLocation() : FVector::ZeroVector;

	switch (CamMode)
	{
	case EZoogiCamMode::Menu:
	{
		MenuOrbit += Dt * 6.f;
		Yaw = DampAngle(Yaw, MenuOrbit, 2.f, Dt);
		Pitch = Damp(Pitch, 38.f, 1.f, Dt);
		Dist = Damp(Dist, G->Phase == EZoogiPhase::GameOver ? 2600.f : 3600.f, 1.f, Dt);
		Target = (G->Phase == EZoogiPhase::GameOver && T) ? T->BallLocation() : FVector(0, 0, -150.f);
		Pivot = Damp(Pivot, Target, 2.f, Dt);
		Look = Damp(Look, Target, 4.f, Dt);
		break;
	}
	case EZoogiCamMode::Aim:
	{
		const bool bPull = G->bAimActive;
		const float AimYaw = T ? T->AimYaw : Yaw;
		if (!bPull) Yaw = DampAngle(Yaw, AimYaw, LResetYaw, Dt); // A7: do not orbit during a drag
		Pitch = Damp(Pitch, bPull ? DragPitch : AimPitch, bPull ? LDrag : LResetDolly, Dt);
		Dist = Damp(Dist, bPull ? DragDist : AimDist, bPull ? LDrag : LResetDolly, Dt);
		Pivot = Damp(Pivot, Target, 12.f, Dt);
		Look = Damp(Look, Target + FVector(0, 0, 25.f), LLook, Dt);
		break;
	}
	case EZoogiCamMode::Follow:
	{
		const bool bGone = !T || T->bOut || T->BallLocation().Z < -1200.f;
		if (bGone)
		{
			bHoldPose = true; // B15: hold the last pose until the turn changes
		}
		if (!bHoldPose)
		{
			Pitch = Damp(Pitch, FollowPitch, LRise, Dt);
			Dist = Damp(Dist, FollowDist, LRise, Dt);
			// B14: drift the heading toward travel after bounces, capped.
			const FVector V = T->GetBallVelocity();
			const FVector2D V2(V.X, V.Y);
			if (V2.Size() > 50.f)
			{
				const float Want = FMath::RadiansToDegrees(FMath::Atan2(V2.Y, V2.X));
				const float Delta = FRotator::NormalizeAxis(Want - Yaw);
				if (FMath::Abs(Delta) < 170.f)
				{
					const float Step = FMath::Clamp(Delta * (1.f - FMath::Exp(-LYawFollow * Dt)), -YawCap * Dt, YawCap * Dt);
					Yaw = FRotator::NormalizeAxis(Yaw + Step);
				}
			}
			FVector PT = Target;
			Pivot.X = Damp(Pivot.X, PT.X, LPos, Dt);
			Pivot.Y = Damp(Pivot.Y, PT.Y, LPos, Dt);
			Pivot.Z = Damp(Pivot.Z, PT.Z, LPos, Dt);
			Look = Damp(Look, Target, LLook, Dt);
		}
		break;
	}
	}

	const FRotator Heading(0.f, Yaw, 0.f);
	const float P = FMath::DegreesToRadians(Pitch);
	FVector Loc = Pivot - Heading.Vector() * Dist * FMath::Cos(P) + FVector(0, 0, Dist * FMath::Sin(P));
	Loc.Z = FMath::Max(Loc.Z, MinZ);
	if (!(CamMode == EZoogiCamMode::Follow && bHoldPose))
	{
		Cam->SetActorLocation(Loc);
		Cam->SetActorRotation((Look - Loc).Rotation());
	}
}
