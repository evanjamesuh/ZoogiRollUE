#pragma once
#include "CoreMinimal.h"

/**
 * Aim view (Evan Oct 17, Imagine Cup clip 0:10-0:20): during the aim step the camera sits behind and above the acting Zoogi,
 * looking up the field ahead of it along the aim arrow.
 * The chase-boom math here is shared with the portrait low aim view (which keeps its own tunables).
 *   zoogi.Cam.AimView     1    landscape aim view on (0 = the Oct 4 tactical aim view)
 *   zoogi.Cam.AimPitch    38   camera pitch (deg down)
 *   zoogi.Cam.AimBoom     700  camera distance from the Zoogi (cm, ~7 Zoogi diameters)
 *   zoogi.Cam.AimScreenY  0.72 the Zoogi's screen height from the top (fraction of the view)
 *   zoogi.Cam.AimFov      0    horizontal FOV (0 = the tactical profile's)
 *   zoogi.Cam.AimYawTime  0.12 seconds for the critically damped yaw follow to settle on the aim heading
 * The camera orbits the Zoogi with its aim arrow (Evan Oct 17 2:46 PM), so the arrow always points straight up the screen.
 */
namespace ZoogiAimView
{
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API float Pitch();
	ZOOGIROLL_API float Boom();
	ZOOGIROLL_API float ScreenY();
	ZOOGIROLL_API float Fov(float ProfileFov);
	ZOOGIROLL_API float YawTime();
	/** Boom elevation (deg, from the subject) and the look point's distance ahead of the subject (cm, along the heading, at the
	 *  subject's height) so a camera BoomLen away, pitched ViewPitch down, shows the subject at ScreenY (0 top .. 1 bottom) of a
	 *  view whose vertical half-FOV tangent is TanV, centred horizontally. */
	ZOOGIROLL_API void ChaseBoom(float BoomLen, float ViewPitch, float ScreenY, float TanV, float& OutElev, float& OutAhead);
	/** Normalised screen point (x right, y up, -1..1) of P for a camera at Loc / Rot with horizontal FOV HFov and aspect Aspect. */
	ZOOGIROLL_API bool Project(const FVector& Loc, const FRotator& Rot, float HFov, float Aspect, const FVector& P, FVector2D& Out);
}
