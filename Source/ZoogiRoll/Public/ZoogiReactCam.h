// Nov 1 b55 (Evan: "When reaction is required slow down, zoom in to the incoming collision and give 5 seconds to decide"):
// shared framing for the reaction close-up (Fun BLOCK! timing tap, Wolf Pack PARRY). Header-only.
#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ZoogiPortraitCam.h"

namespace ZoogiReactCam
{
	constexpr float Window = 5.f; // real seconds the player gets to decide

	/** Side-on close-up of the attacker closing on the victim: both in frame, the victim a little nearer. */
	inline void Frame(ACameraActor* Cam, const FVector& Att, const FVector& Vic, float RealDt, bool bSnap)
	{
		if (!Cam) return;
		FVector Dir = (Vic - Att).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		const float Sep = FVector::Dist2D(Att, Vic);
		const float D = FMath::Clamp(Sep * 1.5f, 420.f, 1500.f);
		const FVector Mid = FMath::Lerp(Att, Vic, 0.6f);
		FVector Want = Mid - Dir * D * 0.25f + Side * D * 0.9f + FVector(0, 0, D * 0.55f);
		const float Fov = ZoogiPortrait::SceneFit(Cam->GetWorld(), Mid, 55.f, Want); // b67: portrait pulls back so both fit
		const FVector L = bSnap ? Want : FMath::VInterpTo(Cam->GetActorLocation(), Want, RealDt, 5.f);
		Cam->SetActorLocation(L);
		Cam->SetActorRotation((Mid + FVector(0, 0, 20.f) - L).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(Fov);
	}
}
