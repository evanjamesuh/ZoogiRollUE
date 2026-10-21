#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class UPhysicalMaterial;

/**
 * Marble feel on top of Chaos.
 * Chaos does the contacts, bounces, mass ratios and falls. The web game's
 * constant rolling deceleration (simFeel ROLLING_DECEL) and the "roll, don't
 * slide" spin are applied here once per tick while a body is on the ground.
 */
namespace ZoogiPhysics
{
	struct FRollResult
	{
		bool bGrounded = false;
		float PlanarSpeed = 0.f;
	};

	ZOOGIROLL_API bool IsGrounded(const UPrimitiveComponent* Body, float Radius);
	ZOOGIROLL_API FRollResult ApplyRolling(UPrimitiveComponent* Body, float Radius, float Decel, float DeltaSeconds, float MaxPlanarSpeed);
	ZOOGIROLL_API UPhysicalMaterial* MakeMaterial(UObject* Outer, float Friction, float Restitution, bool bOverrideRestitutionMin = false);
}
