#include "ZoogiShotLibrary.h"

namespace ZoogiShot
{
	float ApplyEase(EZoogiShotEase Ease, float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		switch (Ease)
		{
		case EZoogiShotEase::Linear: return T;
		case EZoogiShotEase::CubicInOut: return T < 0.5f ? 4.f * T * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 3.f) * 0.5f;
		case EZoogiShotEase::ExpoOut: return T >= 1.f ? 1.f : 1.f - FMath::Pow(2.f, -10.f * T);
		case EZoogiShotEase::SmoothStep:
		default: return T * T * (3.f - 2.f * T);
		}
	}

	const TCHAR* TypeName(EZoogiShotType Type)
	{
		switch (Type)
		{
		case EZoogiShotType::CloseUp: return TEXT("CloseUp");
		case EZoogiShotType::LowAngleHero: return TEXT("LowAngleHero");
		case EZoogiShotType::TopDownImpact: return TEXT("TopDownImpact");
		case EZoogiShotType::Tracking: return TEXT("Tracking");
		case EZoogiShotType::PartnerTwoShot: return TEXT("PartnerTwoShot");
		default: return TEXT("Custom");
		}
	}

	const TCHAR* OutputName(EZoogiShotOutput Output)
	{
		return Output == EZoogiShotOutput::MainView ? TEXT("MainView") : TEXT("RenderTarget");
	}
}

FZoogiShotSpec UZoogiShotLibrary::MakeDefault(EZoogiShotType Type)
{
	FZoogiShotSpec S;
	S.Type = Type;
	S.Name = FName(ZoogiShot::TypeName(Type));
	switch (Type)
	{
	case EZoogiShotType::CloseUp:
		// Tight on the face side, slight push-in and drift, small dutch.
		S.Duration = 2.0f; S.EaseIn = 0.3f; S.EaseOut = 0.35f;
		S.Heading = EZoogiShotHeading::GameplayCamera; S.YawOffset = 25.f; S.OrbitDegrees = 8.f;
		S.Pitch = 8.f; S.PitchEnd = 6.f; S.Distance = 270.f; S.DistanceEnd = 215.f;
		S.FOV = 38.f; S.FOVEnd = 32.f; S.Roll = -3.f; S.LookOffset = FVector(0, 0, 10.f);
		S.VignetteAdd = 0.15f; S.ContrastMul = 1.04f;
		break;
	case EZoogiShotType::LowAngleHero:
		// Camera near floor level looking up at the Zoogi, slow push-in with a counter-orbit.
		S.Duration = 2.2f; S.EaseIn = 0.35f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::GameplayCamera; S.YawOffset = 35.f; S.OrbitDegrees = -12.f;
		S.PivotOffset = FVector(0, 0, -38.f); S.LookOffset = FVector(0, 0, 75.f);
		S.Pitch = 3.f; S.PitchEnd = 4.f; S.Distance = 430.f; S.DistanceEnd = 340.f;
		S.FOV = 62.f; S.FOVEnd = 55.f; S.Roll = 4.f;
		S.VignetteAdd = 0.1f; S.SaturationMul = 1.05f;
		break;
	case EZoogiShotType::TopDownImpact:
		// Near-vertical view onto the impact point, slow rotation and drop-in.
		S.Duration = 1.8f; S.EaseIn = 0.3f; S.EaseOut = 0.35f;
		S.Heading = EZoogiShotHeading::GameplayCamera; S.YawOffset = 0.f; S.OrbitDegrees = 20.f;
		S.Pitch = 84.f; S.PitchEnd = 86.f; S.Distance = 1000.f; S.DistanceEnd = 760.f;
		S.FOV = 55.f; S.FOVEnd = 50.f; S.LookOffset = FVector::ZeroVector;
		break;
	case EZoogiShotType::Tracking:
		// Chase from behind the focus' motion with a damped pivot.
		S.Duration = 2.4f; S.EaseIn = 0.35f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusVelocity; S.YawOffset = 0.f;
		S.Pitch = 18.f; S.PitchEnd = 16.f; S.Distance = 560.f; S.DistanceEnd = 500.f;
		S.FOV = 62.f; S.FOVEnd = 60.f; S.TrackingLag = 5.f; S.LookOffset = FVector(0, 0, 20.f);
		break;
	case EZoogiShotType::PartnerTwoShot:
		// Side-on to the line between the two, both framed.
		S.Duration = 2.2f; S.EaseIn = 0.35f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToSecondary; S.YawOffset = 90.f; S.OrbitDegrees = 6.f;
		S.SecondaryBias = 0.5f; S.bFrameBoth = true;
		S.Pitch = 14.f; S.PitchEnd = 12.f; S.Distance = 620.f; S.DistanceEnd = 560.f;
		S.FOV = 50.f; S.FOVEnd = 48.f; S.LookOffset = FVector(0, 0, 10.f);
		break;
	default:
		break;
	}
	return S;
}

void UZoogiShotLibrary::FillDefaults()
{
	Shots.Reset();
	for (EZoogiShotType T : { EZoogiShotType::CloseUp, EZoogiShotType::LowAngleHero, EZoogiShotType::TopDownImpact, EZoogiShotType::Tracking, EZoogiShotType::PartnerTwoShot })
	{
		Shots.Add(MakeDefault(T));
	}
}

const FZoogiShotSpec* UZoogiShotLibrary::FindByType(EZoogiShotType Type) const
{
	return Shots.FindByPredicate([Type](const FZoogiShotSpec& S) { return S.Type == Type; });
}

const FZoogiShotSpec* UZoogiShotLibrary::FindByName(FName Name) const
{
	return Shots.FindByPredicate([Name](const FZoogiShotSpec& S) { return S.Name == Name; });
}
