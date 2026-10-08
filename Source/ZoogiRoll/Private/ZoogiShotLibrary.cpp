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
		case EZoogiShotType::ReactionEye: return TEXT("ReactionEye");
		case EZoogiShotType::ReactionSurprise: return TEXT("ReactionSurprise");
		case EZoogiShotType::ReactionComedy: return TEXT("ReactionComedy");
		case EZoogiShotType::TrickBank: return TEXT("TrickBank");
		case EZoogiShotType::TrickWallRedirect: return TEXT("TrickWallRedirect");
		case EZoogiShotType::TrickAllyBump: return TEXT("TrickAllyBump");
		case EZoogiShotType::TrickOrbRelay: return TEXT("TrickOrbRelay");
		case EZoogiShotType::TrickBoundarySave: return TEXT("TrickBoundarySave");
		case EZoogiShotType::Airborne: return TEXT("Airborne");
		default: return TEXT("Custom");
		}
	}

	const TArray<EZoogiShotType>& AllTypes()
	{
		static const TArray<EZoogiShotType> All = {
			EZoogiShotType::CloseUp, EZoogiShotType::LowAngleHero, EZoogiShotType::TopDownImpact, EZoogiShotType::Tracking, EZoogiShotType::PartnerTwoShot,
			EZoogiShotType::ReactionEye, EZoogiShotType::ReactionSurprise, EZoogiShotType::ReactionComedy,
			EZoogiShotType::TrickBank, EZoogiShotType::TrickWallRedirect, EZoogiShotType::TrickAllyBump, EZoogiShotType::TrickOrbRelay, EZoogiShotType::TrickBoundarySave,
			EZoogiShotType::Airborne };
		return All;
	}

	int32 Tier(EZoogiShotType Type)
	{
		switch (Type)
		{
		case EZoogiShotType::ReactionEye: case EZoogiShotType::ReactionSurprise: case EZoogiShotType::ReactionComedy: return 1;
		case EZoogiShotType::TrickBank: case EZoogiShotType::TrickWallRedirect: case EZoogiShotType::TrickAllyBump:
		case EZoogiShotType::TrickOrbRelay: case EZoogiShotType::TrickBoundarySave: return 2;
		case EZoogiShotType::Airborne: return 3;
		default: return 0;
		}
	}

	bool UsesSecondary(EZoogiShotType Type)
	{
		return Type == EZoogiShotType::PartnerTwoShot || Type == EZoogiShotType::TrickAllyBump || Type == EZoogiShotType::TrickOrbRelay;
	}

	bool UsesEventPoint(EZoogiShotType Type)
	{
		return Type == EZoogiShotType::TrickBank || Type == EZoogiShotType::TrickWallRedirect || Type == EZoogiShotType::TrickBoundarySave;
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
		S.Pitch = 8.f; S.PitchEnd = 6.f; S.Distance = 300.f; S.DistanceEnd = 245.f;
		S.FOV = 38.f; S.FOVEnd = 33.f; S.Roll = -3.f; S.LookOffset = FVector(0, 0, 10.f);
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

	// ---------------------------------------------------------------- Tier 1: reaction close-ups (face = Visual +X)
	case EZoogiShotType::ReactionEye:
		// Extreme close-up on the eyes, almost square-on, tiny drift.
		S.Duration = 1.5f; S.EaseIn = 0.15f; S.EaseOut = 0.3f; S.Ease = EZoogiShotEase::CubicInOut;
		S.Heading = EZoogiShotHeading::FocusFacing; S.YawOffset = 10.f; S.OrbitDegrees = 4.f;
		S.Pitch = 4.f; S.PitchEnd = 3.f; S.Distance = 165.f; S.DistanceEnd = 140.f;
		S.FOV = 24.f; S.FOVEnd = 21.f; S.PivotOffset = FVector(0, 0, 6.f); S.LookOffset = FVector(0, 0, 4.f);
		S.SubjectRadius = 30.f; S.VignetteAdd = 0.22f; S.SaturationMul = 1.06f; S.ContrastMul = 1.05f;
		break;
	case EZoogiShotType::ReactionSurprise:
		// Snap zoom from a medium to a close-up (expo-out move) with a quick decaying jolt.
		S.Duration = 1.4f; S.EaseIn = 0.1f; S.EaseOut = 0.3f; S.MoveEase = EZoogiShotEase::ExpoOut;
		S.Heading = EZoogiShotHeading::FocusFacing; S.YawOffset = -8.f;
		S.Pitch = 7.f; S.PitchEnd = 4.f; S.Distance = 420.f; S.DistanceEnd = 190.f;
		S.FOV = 46.f; S.FOVEnd = 34.f; S.LookOffset = FVector(0, 0, 8.f);
		S.ShakeDeg = 0.8f; S.ShakeHz = 9.f; S.ShakeDecay = 3.f;
		S.VignetteAdd = 0.12f; S.ContrastMul = 1.06f;
		break;
	case EZoogiShotType::ReactionComedy:
		// Slightly low three-quarter close-up with a rocking dutch tilt.
		S.Duration = 1.9f; S.EaseIn = 0.25f; S.EaseOut = 0.35f;
		S.Heading = EZoogiShotHeading::FocusFacing; S.YawOffset = -24.f; S.OrbitDegrees = -6.f;
		S.Pitch = -3.f; S.PitchEnd = -2.f; S.Distance = 250.f; S.DistanceEnd = 225.f;
		S.FOV = 40.f; S.FOVEnd = 38.f; S.Roll = 6.f; S.RollWobbleDeg = 5.f; S.RollWobbleHz = 1.4f;
		S.LookOffset = FVector(0, 0, 6.f); S.SaturationMul = 1.12f; S.ContrastMul = 1.03f;
		break;

	// ---------------------------------------------------------------- Tier 2: trick shots
	case EZoogiShotType::TrickBank:
		// High side-on view across the shot path, framing the shooter and the bank point.
		S.Duration = 2.2f; S.EaseIn = 0.35f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToEvent; S.YawOffset = 90.f; S.OrbitDegrees = 10.f;
		S.EventBias = 0.5f; S.bFrameEvent = true; S.SubjectRadius = 70.f; S.MaxFOV = 90.f;
		S.Pitch = 40.f; S.PitchEnd = 34.f; S.Distance = 820.f; S.DistanceEnd = 700.f;
		S.FOV = 52.f; S.FOVEnd = 48.f; S.TrackingLag = 4.f; S.LookOffset = FVector::ZeroVector;
		break;
	case EZoogiShotType::TrickWallRedirect:
		// Low, near the wall, looking back at the approach; pulls out as the Zoogi leaves on the new line.
		S.Duration = 2.0f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToEvent; S.YawOffset = 135.f; S.OrbitDegrees = -10.f;
		S.EventBias = 0.7f; S.bFrameEvent = true; S.SubjectRadius = 60.f;
		S.Pitch = 9.f; S.PitchEnd = 12.f; S.Distance = 420.f; S.DistanceEnd = 480.f;
		S.FOV = 58.f; S.FOVEnd = 62.f; S.TrackingLag = 5.f; S.Roll = -3.f; S.LookOffset = FVector(0, 0, 10.f);
		S.ContrastMul = 1.05f;
		break;
	case EZoogiShotType::TrickAllyBump:
		// Tight side two-shot on the contact with a small jolt.
		S.Duration = 1.8f; S.EaseIn = 0.25f; S.EaseOut = 0.35f;
		S.Heading = EZoogiShotHeading::FocusToSecondary; S.YawOffset = 70.f; S.OrbitDegrees = 8.f;
		S.SecondaryBias = 0.55f; S.bFrameBoth = true;
		S.Pitch = 10.f; S.PitchEnd = 9.f; S.Distance = 470.f; S.DistanceEnd = 400.f;
		S.FOV = 46.f; S.FOVEnd = 42.f; S.Roll = -2.f; S.LookOffset = FVector(0, 0, 10.f);
		S.ShakeDeg = 0.5f; S.ShakeHz = 8.f; S.ShakeDecay = 2.5f;
		break;
	case EZoogiShotType::TrickOrbRelay:
		// Over the relaying Zoogi's shoulder toward the orb / partner, easing back as it travels.
		S.Duration = 2.2f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToSecondary; S.YawOffset = 20.f;
		S.SecondaryBias = 0.3f; S.bFrameBoth = true; S.PivotOffset = FVector(0, 0, 15.f);
		S.Pitch = 15.f; S.PitchEnd = 13.f; S.Distance = 380.f; S.DistanceEnd = 450.f;
		S.FOV = 50.f; S.FOVEnd = 54.f; S.TrackingLag = 5.f; S.LookOffset = FVector(0, 0, 10.f);
		break;
	case EZoogiShotType::TrickBoundarySave:
		// From just outside the rim, low, looking back in at the Zoogi teetering on the edge with the arena behind.
		S.Duration = 2.0f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToEvent; S.YawOffset = 25.f; S.OrbitDegrees = -8.f;
		S.Pitch = 5.f; S.PitchEnd = 8.f; S.Distance = 320.f; S.DistanceEnd = 360.f;
		S.FOV = 50.f; S.FOVEnd = 55.f; S.Roll = 3.f; S.LookOffset = FVector(0, 0, 12.f);
		S.VignetteAdd = 0.12f; S.SaturationMul = 1.05f;
		break;

	// ---------------------------------------------------------------- Tier 3
	case EZoogiShotType::Airborne:
		// Side-on to the flight, from slightly below, drifting around the Zoogi; slow-mo entry (policy permitting).
		S.Duration = 2.8f; S.EaseIn = 0.3f; S.EaseOut = 0.45f;
		S.Heading = EZoogiShotHeading::FocusVelocity; S.YawOffset = 90.f; S.OrbitDegrees = 30.f;
		S.Pitch = -6.f; S.PitchEnd = 2.f; S.Distance = 520.f; S.DistanceEnd = 600.f;
		S.FOV = 54.f; S.FOVEnd = 60.f; S.TrackingLag = 6.f; S.LookOffset = FVector::ZeroVector;
		S.bSlowMoEntry = true; S.SlowMoTarget = 0.3f; S.SlowMoDelay = 0.05f; S.SlowMoEaseIn = 0.25f; S.SlowMoHold = 1.2f; S.SlowMoEaseOut = 0.5f;
		S.VignetteAdd = 0.1f; S.SaturationMul = 1.05f;
		break;
	default:
		break;
	}
	return S;
}

void UZoogiShotLibrary::FillDefaults()
{
	Shots.Reset();
	for (EZoogiShotType T : ZoogiShot::AllTypes())
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
