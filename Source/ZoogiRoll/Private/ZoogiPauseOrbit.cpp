// b120 pick-pause camera orbit - see ZoogiPauseOrbit.h
#include "ZoogiPauseOrbit.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRedirect.h"
#include "ZoogiActionFun.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
	float GOrbYaw = 0.f, GOrbPitch = 0.f;
	FVector2D GOrbPending = FVector2D::ZeroVector;
	bool GOrbDown = false, GOrbDragging = false, GOrbWasDrag = false, GOrbActiveSeen = false;
	FVector2D GOrbPress = FVector2D::ZeroVector, GOrbLast = FVector2D::ZeroVector;

	bool OrbOff() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPauseOrbit")); return b; }
	UWorld* OrbWorld() { return (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr; }
	AZoogiBall* OrbBall(UWorld* W)
	{
		AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return (B && !B->bCPU) ? B : nullptr;
	}
	void OrbitRig(const FVector& Pivot, FVector& Loc, FRotator& Rot, float Yaw, float Pitch)
	{
		const FVector Right = FRotationMatrix(FRotator(0.f, Rot.Yaw, 0.f)).GetScaledAxis(EAxis::Y);
		const FQuat Q = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * FQuat(Right, FMath::DegreesToRadians(Pitch));
		const float Roll = Rot.Roll;
		Loc = Pivot + Q.RotateVector(Loc - Pivot);
		Loc.Z = FMath::Max(Loc.Z, Pivot.Z + 40.f);
		Rot = (Q * Rot.Quaternion()).Rotator();
		Rot.Roll = Roll;
	}

	class FOrbitInput : public IInputProcessor
	{
	public:
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override
		{
			GOrbWasDrag = false;
			if (ZoogiPauseOrbit::Active(OrbWorld()) && (E.IsTouchEvent() || E.GetEffectingButton() == EKeys::LeftMouseButton))
			{ GOrbDown = true; GOrbDragging = false; GOrbPress = GOrbLast = E.GetScreenSpacePosition(); }
			return false; // never consumes
		}
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override
		{
			if (!GOrbDown) return false;
			const FVector2D P = E.GetScreenSpacePosition();
			if (!GOrbDragging && FVector2D::Distance(P, GOrbPress) > 26.f) { GOrbDragging = true; GOrbWasDrag = true; } // a tap is not a drag
			if (GOrbDragging) GOrbPending += P - GOrbLast;
			GOrbLast = P;
			return false;
		}
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { GOrbDown = GOrbDragging = false; return false; }
		virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiPauseOrbit"); }
	};
}

void ZoogiPauseOrbit::Ensure()
{
	static bool bDone = false;
	if (bDone || OrbOff() || !FSlateApplication::IsInitialized()) return;
	bDone = true;
	static TSharedPtr<FOrbitInput> GIn = MakeShared<FOrbitInput>();
	FSlateApplication::Get().RegisterInputPreProcessor(GIn);
	UE_LOG(LogZoogi, Log, TEXT("PICK ORBIT on: drag during a REDIRECT / superpower target pick to turn the camera"));
}

bool ZoogiPauseOrbit::Active(UWorld* World)
{
	if (OrbOff() || !World || !OrbBall(World)) return false;
	if (ZoogiRedirect::Picking()) return true;
	if (const UZoogiFunSys* F = UZoogiFunSys::Get(World)) if (F->UltAimOn()) return true;
	return false;
}

bool ZoogiPauseOrbit::WasDrag() { return GOrbWasDrag; }

void ZoogiPauseOrbit::Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt)
{
	const bool bOn = Active(World);
	AZoogiBall* Hb = OrbBall(World);
	if (bOn && !GOrbPending.IsZero())
	{
		FIntPoint Vp(1920, 1080); if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) Vp = GEngine->GameViewport->Viewport->GetSizeXY();
		const float K = 300.f / FMath::Max(1, FMath::Min(Vp.X, Vp.Y)); // ~ one screen width = a full turn
		GOrbYaw = FMath::Fmod(GOrbYaw + GOrbPending.X * K, 360.f);
		GOrbPitch = FMath::Clamp(GOrbPitch + GOrbPending.Y * K * 0.5f, -15.f, 40.f);
		if (!GOrbActiveSeen) UE_LOG(LogZoogi, Log, TEXT("PICK ORBIT: camera turning round P%d"), Hb ? Hb->PlayerIndex + 1 : 0);
		GOrbActiveSeen = true;
	}
	GOrbPending = FVector2D::ZeroVector;
	if (GOrbYaw == 0.f && GOrbPitch == 0.f) { GOrbActiveSeen = false; return; }
	if (!Hb) { GOrbYaw = GOrbPitch = 0.f; return; }
	if (GOrbYaw > 180.f) GOrbYaw -= 360.f; else if (GOrbYaw < -180.f) GOrbYaw += 360.f; // the short way round
	OrbitRig(Hb->BallLocation(), Loc, Rot, GOrbYaw, GOrbPitch);
	if (!bOn)
	{	// the pick is over: ease back into normal play (~0.7 s)
		GOrbYaw = FMath::FInterpTo(GOrbYaw, 0.f, Dt, 4.5f); GOrbPitch = FMath::FInterpTo(GOrbPitch, 0.f, Dt, 4.5f);
		if (FMath::Abs(GOrbYaw) < 0.05f && FMath::Abs(GOrbPitch) < 0.05f) GOrbYaw = GOrbPitch = 0.f;
	}
}
