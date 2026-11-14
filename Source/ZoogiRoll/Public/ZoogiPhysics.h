#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UPhysicalMaterial;
class AActor;

/**
 * Marble feel on top of Chaos.
 * Chaos does the contacts, bounces, mass ratios and falls. The web game's
 * constant rolling deceleration (simFeel ROLLING_DECEL) and the "roll, don't
 * slide" spin are applied here once per tick while a body is on the ground.
 *
 * Feel items 21 / 23 / 25 (flick spin curve, angle-sensitive rails, heavy grind
 * vs light skid) live in Feel::* and the helpers below — same rules on every map.
 */
namespace ZoogiPhysics
{
	/** Named feel knobs (cm/s Unreal space). Tunable without hunting call sites. */
	namespace Feel
	{
		// ---- 21 flick spin curve (gameplay yaw spin from a lateral slingshot flick)
		constexpr float FlickSpinMaxRad = 7.5f;       // yaw spin (rad/s) at |SideFrac| = 1
		constexpr float FlickCurveGain = 0.28f;       // lateral accel ~= Spin * Speed * Gain
		constexpr float FlickDecayPerSec = 1.35f;     // exponential decay rate (~0.75 s feel)
		constexpr float FlickMaxLatAccel = 720.f;     // cm/s^2 cap — no U-turn / self-KO from center
		constexpr float FlickSideDeadZone = 0.08f;    // |side| below this = pure back-pull (no spin)

		// ---- 23 angle-sensitive rails / bumpers (stage only; Zoogi-vs-Zoogi untouched)
		constexpr float HeadOnIncidence = 0.82f;      // |N·Vdir| above = head-on
		constexpr float GlanceIncidence = 0.38f;      // below = glancing grind
		constexpr float HeadOnNormalScale = 1.06f;    // slight snappier rebound
		constexpr float GlanceNormalScale = 0.32f;    // damp explosive normal kick
		constexpr float HeadOnTangentScale = 1.0f;
		constexpr float GlanceTangentScale = 0.62f;   // more tangential stick / ride-along

		// ---- 25 heavy grind vs light skid (MassRatio = Defense/60 clamped 0.55..1.8)
		constexpr float MassMin = 0.55f;
		constexpr float MassMax = 1.8f;
		constexpr float HeavyRollBlend = 1.0f;        // per-frame blend toward perfect roll
		constexpr float LightRollBlend = 0.20f;       // light keeps slip longer
		constexpr float HeavyDecelMul = 1.10f;
		constexpr float LightDecelMul = 0.90f;
	}

	struct FRollResult
	{
		bool bGrounded = false;
		float PlanarSpeed = 0.f;
	};

	struct FRailResponse
	{
		float NormalScale = 1.f;
		float TangentScale = 1.f;
	};

	ZOOGIROLL_API bool IsGrounded(const UPrimitiveComponent* Body, float Radius);

	/**
	 * Grounded rolling feel. MassRatio softens perfect-roll lock (item 25).
	 * Optional InOutFlickSpinYaw (rad/s about world +Z) bends the path while
	 * grounded and decays (item 21). Orbs pass MassRatio=1 and no flick pointer.
	 */
	ZOOGIROLL_API FRollResult ApplyRolling(UPrimitiveComponent* Body, float Radius, float Decel, float DeltaSeconds, float MaxPlanarSpeed, float MassRatio = 1.f, float* InOutFlickSpinYaw = nullptr);

	ZOOGIROLL_API UPhysicalMaterial* MakeMaterial(UObject* Outer, float Friction, float Restitution, bool bOverrideRestitutionMin = false);

	/** 0 at MassMin (light skid) .. 1 at MassMax (heavy grind). */
	ZOOGIROLL_API float MassFeelT(float MassRatio);
	/** Per-frame blend of angular velocity toward perfect rolling. */
	ZOOGIROLL_API float MassRollBlend(float MassRatio);
	/** Multiplier on RollingDecel for grind vs skid. */
	ZOOGIROLL_API float MassDecelMul(float MassRatio);

	/** Signed SideFrac (-1..1) from slingshot lateral / max-drag → gameplay yaw spin (rad/s). Dead-zone zeroes pure back-pull. */
	ZOOGIROLL_API float FlickSpinFromSide(float SideFrac);
	/** Lateral planar acceleration (cm/s^2) from remaining flick spin × planar velocity. Capped. */
	ZOOGIROLL_API FVector2D FlickLateralAccel(const FVector2D& PlanarVel, float FlickSpinYaw);
	ZOOGIROLL_API float FlickDecaySpin(float FlickSpinYaw, float DeltaSeconds);

	/** Incidence = |N·VelDir| in 0..1 → normal/tangent exit scales for rails/bumpers. */
	ZOOGIROLL_API FRailResponse RailResponse(float Incidence);
	/** True for stage walls / bumpers / rocks / arena pieces (not Zoogis or orbs). */
	ZOOGIROLL_API bool IsStageHit(const AActor* OtherActor, const UPrimitiveComponent* OtherComp);
	/** Post-Chaos tweak on stage contacts: head-on snappier, glancing grind. No-op for Zoogi/orb. */
	ZOOGIROLL_API void ApplyRailBumperFeel(UPrimitiveComponent* Body, const FVector& HitNormal, const FVector& ApproachVel, const AActor* OtherActor, const UPrimitiveComponent* OtherComp);
}
