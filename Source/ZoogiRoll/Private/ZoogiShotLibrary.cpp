#include "ZoogiShotLibrary.h"
#include "ZoogiEpisodeTypes.h"

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
		case EZoogiShotType::TrickDoubleBank: return TEXT("TrickDoubleBank");
		case EZoogiShotType::TrickPowerBank: return TEXT("TrickPowerBank");
		case EZoogiShotType::TrickRingOutLine: return TEXT("TrickRingOutLine");
		default: return TEXT("Custom");
		}
	}

	const TArray<EZoogiShotType>& AllTypes()
	{
		static const TArray<EZoogiShotType> All = {
			EZoogiShotType::CloseUp, EZoogiShotType::LowAngleHero, EZoogiShotType::TopDownImpact, EZoogiShotType::Tracking, EZoogiShotType::PartnerTwoShot,
			EZoogiShotType::ReactionEye, EZoogiShotType::ReactionSurprise, EZoogiShotType::ReactionComedy,
			EZoogiShotType::TrickBank, EZoogiShotType::TrickWallRedirect, EZoogiShotType::TrickAllyBump, EZoogiShotType::TrickOrbRelay, EZoogiShotType::TrickBoundarySave,
			EZoogiShotType::Airborne, EZoogiShotType::TrickDoubleBank, EZoogiShotType::TrickPowerBank, EZoogiShotType::TrickRingOutLine };
		return All;
	}

	int32 Tier(EZoogiShotType Type)
	{
		switch (Type)
		{
		case EZoogiShotType::ReactionEye: case EZoogiShotType::ReactionSurprise: case EZoogiShotType::ReactionComedy: return 1;
		case EZoogiShotType::TrickBank: case EZoogiShotType::TrickWallRedirect: case EZoogiShotType::TrickAllyBump:
		case EZoogiShotType::TrickOrbRelay: case EZoogiShotType::TrickBoundarySave:
		case EZoogiShotType::TrickDoubleBank: case EZoogiShotType::TrickPowerBank: case EZoogiShotType::TrickRingOutLine: return 2;
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
		return Type == EZoogiShotType::TrickBank || Type == EZoogiShotType::TrickWallRedirect || Type == EZoogiShotType::TrickBoundarySave
			|| Type == EZoogiShotType::TrickDoubleBank || Type == EZoogiShotType::TrickPowerBank || Type == EZoogiShotType::TrickOrbRelay;
	}

	EZoogiShotType ShotForTrickTag(EZoogiEpisodeTag Tag, bool bPower)
	{
		switch (Tag)
		{
		case EZoogiEpisodeTag::Bank: return bPower ? EZoogiShotType::TrickPowerBank : EZoogiShotType::TrickBank;
		case EZoogiEpisodeTag::DoubleBank: return bPower ? EZoogiShotType::TrickPowerBank : EZoogiShotType::TrickDoubleBank;
		case EZoogiEpisodeTag::WallRedirect: return EZoogiShotType::TrickWallRedirect;
		case EZoogiEpisodeTag::AllyBump: return EZoogiShotType::TrickAllyBump;
		case EZoogiEpisodeTag::OrbRelay: return EZoogiShotType::TrickOrbRelay;
		case EZoogiEpisodeTag::BoundarySave: return EZoogiShotType::TrickBoundarySave;
		case EZoogiEpisodeTag::RingOut: return EZoogiShotType::TrickRingOutLine;
		case EZoogiEpisodeTag::PowerHit: return EZoogiShotType::TrickPowerBank;
		default: return EZoogiShotType::LowAngleHero;
		}
	}

	EZoogiShotType ShotForTrickTags(const TArray<EZoogiEpisodeTag>& Tags)
	{
		const bool bPower = Tags.Contains(EZoogiEpisodeTag::PowerHit);
		// most specific first: a power bank, then the geometry tricks, then ring-out line; PowerHit alone is not a trick
		static const EZoogiEpisodeTag Order[] = { EZoogiEpisodeTag::DoubleBank, EZoogiEpisodeTag::Bank, EZoogiEpisodeTag::BoundarySave,
			EZoogiEpisodeTag::OrbRelay, EZoogiEpisodeTag::AllyBump, EZoogiEpisodeTag::WallRedirect, EZoogiEpisodeTag::RingOut };
		for (EZoogiEpisodeTag T : Order)
			if (Tags.Contains(T)) return ShotForTrickTag(T, bPower);
		return EZoogiShotType::LowAngleHero;
	}

	const TCHAR* AngleDescription(EZoogiShotType Type)
	{
		switch (Type)
		{
		case EZoogiShotType::TrickBank: return TEXT("on the wall normal, raised: in and out lines mirror around the contact");
		case EZoogiShotType::TrickDoubleBank: return TEXT("high, perpendicular to the zigzag: both contacts + the Z path in frame");
		case EZoogiShotType::TrickPowerBank: return TEXT("close on the Zoogi at / just after contact, contact + power colour as accent, jolt");
		case EZoogiShotType::TrickWallRedirect: return TEXT("behind the new line at the wall, looking down it");
		case EZoogiShotType::TrickAllyBump: return TEXT("low two-shot at the contact, ally in the foreground");
		case EZoogiShotType::TrickOrbRelay: return TEXT("chases the orb from behind along its path toward the receiver");
		case EZoogiShotType::TrickBoundarySave: return TEXT("outside the rim at floor height, lip in the foreground");
		case EZoogiShotType::TrickRingOutLine: return TEXT("behind the Zoogi down the exit vector, sinking toward the void");
		default: return TEXT("");
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
		// square-on (no yaw bias) so both eyes sit inside the frame; look point a little below the eye line so the eyes
		// ride in the upper-middle of the frame, clear of the main-view HUD band.
		S.Heading = EZoogiShotHeading::FocusFacing; S.YawOffset = 0.f; S.OrbitDegrees = 3.f;
		S.Pitch = 4.f; S.PitchEnd = 3.f; S.Distance = 255.f; S.DistanceEnd = 235.f;
		S.FOV = 31.f; S.FOVEnd = 28.f; S.PivotOffset = FVector(0, 0, 6.f); S.LookOffset = FVector(0, 0, -8.f);
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

	// ---------------------------------------------------------------- Tier 2: trick shots (each one its own angle)
	case EZoogiShotType::TrickBank:
		// Raised, out on the wall's normal through the contact, looking back at it: the incoming and outgoing lines open
		// as a symmetric V around the bank point (angle in = angle out reads at a glance). Shooter kept in frame.
		S.Duration = 2.1f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::EventNormal; S.YawOffset = 0.f; S.OrbitDegrees = 0.f;
		S.EventBias = 0.55f; S.bFrameBoth = false; S.bFrameEvent = true; S.SubjectRadius = 60.f; S.FramePadding = 1.15f; S.MaxFOV = 72.f;
		S.Pitch = 36.f; S.PitchEnd = 32.f; S.Distance = 620.f; S.DistanceEnd = 540.f;
		S.FOV = 46.f; S.FOVEnd = 44.f; S.TrackingLag = 5.f; S.LookOffset = FVector::ZeroVector;
		break;
	case EZoogiShotType::TrickDoubleBank:
		// High and square-on to the line between the two contacts: both bank points and the zigzag of the path in one frame.
		S.Duration = 2.4f; S.EaseIn = 0.35f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::EventChord; S.YawOffset = 0.f; S.OrbitDegrees = 6.f;
		S.EventBias = 0.5f; S.SecondEventBias = 0.34f; S.bFrameEvent = true; S.bFrameSecondEvent = true; S.SubjectRadius = 80.f; S.MaxFOV = 95.f;
		S.Pitch = 62.f; S.PitchEnd = 58.f; S.Distance = 1350.f; S.DistanceEnd = 1250.f;
		S.FOV = 55.f; S.FOVEnd = 52.f; S.LookOffset = FVector::ZeroVector;
		break;
	case EZoogiShotType::TrickPowerBank:
		// Tight and low on the contact itself, a little off the normal, in the power's colour with an impact jolt.
		S.Duration = 1.7f; S.EaseIn = 0.2f; S.EaseOut = 0.35f;
		// Anchored on the Zoogi (contact point as the accent, framed alongside), so it is in frame at / just after contact.
		S.Heading = EZoogiShotHeading::EventNormal; S.YawOffset = 35.f; S.OrbitDegrees = -6.f;
		S.EventBias = 0.3f; S.bFrameEvent = true; S.SubjectRadius = 55.f; S.FramePadding = 1.15f; S.MaxFOV = 70.f;
		S.PivotOffset = FVector(0, 0, 5.f); S.TrackingLag = 6.f;
		S.Pitch = 15.f; S.PitchEnd = 12.f; S.Distance = 400.f; S.DistanceEnd = 330.f;
		S.FOV = 48.f; S.FOVEnd = 44.f; S.MoveEase = EZoogiShotEase::ExpoOut; S.LookOffset = FVector::ZeroVector;
		S.ShakeDeg = 0.8f; S.ShakeHz = 9.f; S.ShakeDecay = 3.f;
		S.AccentStrength = 0.22f; S.ContrastMul = 1.06f; S.VignetteAdd = 0.1f;
		break;
	case EZoogiShotType::TrickWallRedirect:
		// At the wall, behind the new direction, looking down the new line the Zoogi leaves on.
		S.Duration = 2.0f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::ExitDirection; S.YawOffset = 0.f; S.OrbitDegrees = 0.f;
		S.EventBias = 0.9f; S.DirectionLead = 260.f; S.bFrameBoth = false; S.SubjectRadius = 60.f;
		S.Pitch = 13.f; S.PitchEnd = 17.f; S.Distance = 520.f; S.DistanceEnd = 600.f;
		S.FOV = 60.f; S.FOVEnd = 64.f; S.LookOffset = FVector(0, 0, 10.f);
		S.ContrastMul = 1.05f;
		break;
	case EZoogiShotType::TrickAllyBump:
		// Low two-shot at the contact from behind the ally (ally big in the foreground, shooter coming in behind), jolt.
		S.Duration = 1.8f; S.EaseIn = 0.25f; S.EaseOut = 0.35f;
		S.Heading = EZoogiShotHeading::FocusToSecondary; S.YawOffset = 180.f + 28.f; S.OrbitDegrees = 6.f;
		S.SecondaryBias = 0.62f; S.bFrameBoth = true; S.FramePadding = 1.2f; S.SubjectRadius = 55.f;
		S.Pitch = 4.f; S.PitchEnd = 5.f; S.Distance = 330.f; S.DistanceEnd = 290.f;
		S.FOV = 50.f; S.FOVEnd = 46.f; S.Roll = -2.f; S.LookOffset = FVector(0, 0, 20.f);
		S.ShakeDeg = 0.5f; S.ShakeHz = 8.f; S.ShakeDecay = 2.5f;
		break;
	case EZoogiShotType::TrickOrbRelay:
		// Chases the orb from behind along its path (shooter -> receiver), the receiver ahead in frame.
		S.Duration = 2.2f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::ExitDirection; S.YawOffset = 14.f;
		S.SecondaryBias = 1.f; S.EventBias = 0.3f; S.bFrameEvent = true; S.SubjectRadius = 55.f;
		S.Pitch = 20.f; S.PitchEnd = 16.f; S.Distance = 520.f; S.DistanceEnd = 600.f;
		S.FOV = 54.f; S.FOVEnd = 56.f; S.TrackingLag = 5.f; S.LookOffset = FVector(0, 0, 10.f);
		break;
	case EZoogiShotType::TrickBoundarySave:
		// Outside the rim, camera at floor height, looking back in: the lip of the arena fills the foreground.
		S.Duration = 2.0f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::FocusToEvent; S.YawOffset = 12.f; S.OrbitDegrees = -6.f;
		S.Pitch = 1.f; S.PitchEnd = 3.f; S.Distance = 280.f; S.DistanceEnd = 320.f; S.PivotOffset = FVector(0, 0, -15.f);
		S.FOV = 48.f; S.FOVEnd = 52.f; S.Roll = 3.f; S.LookOffset = FVector(0, 0, 18.f);
		S.VignetteAdd = 0.12f; S.SaturationMul = 1.05f;
		break;
	case EZoogiShotType::TrickRingOutLine:
		// Behind the Zoogi on its exit vector, looking out past the rim into the void, sinking toward the horizon.
		S.Duration = 2.1f; S.EaseIn = 0.3f; S.EaseOut = 0.4f;
		S.Heading = EZoogiShotHeading::ExitDirection; S.YawOffset = 0.f;
		S.DirectionLead = 160.f; S.Pitch = 24.f; S.PitchEnd = 14.f; S.Distance = 480.f; S.DistanceEnd = 580.f;
		S.FOV = 60.f; S.FOVEnd = 66.f; S.TrackingLag = 4.f; S.LookOffset = FVector(0, 0, -10.f);
		S.VignetteAdd = 0.15f; S.SaturationMul = 0.92f; S.ContrastMul = 1.06f;
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
