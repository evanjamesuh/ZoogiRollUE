#include "ZoogiAimView.h"
#include "HAL/IConsoleManager.h"

namespace ZoogiAimView
{
	static TAutoConsoleVariable<int32> CVarAimView(TEXT("zoogi.Cam.AimView"), 1, TEXT("Aim step: camera behind and above the acting Zoogi looking up the field along the aim arrow (0 = the Oct 4 tactical aim view)."));
	static TAutoConsoleVariable<float> CVarAimPitch(TEXT("zoogi.Cam.AimPitch"), 0.f, TEXT("Aim view: camera pitch, degrees down (0 = auto: the horizon at zoogi.Cam.AimHorizonY for the view's real vertical FOV)."));
	static TAutoConsoleVariable<float> CVarAimHorizonY(TEXT("zoogi.Cam.AimHorizonY"), 0.33f, TEXT("Aim view (auto pitch): the horizon's screen height from the top (fraction of the view; reference clip ~1/3)."));
	static TAutoConsoleVariable<float> CVarAimBoom(TEXT("zoogi.Cam.AimBoom"), 450.f, TEXT("Aim view: camera distance from the Zoogi (cm; ~4.5 Zoogi diameters, low behind it)."));
	static TAutoConsoleVariable<float> CVarAimScreenY(TEXT("zoogi.Cam.AimScreenY"), 0.72f, TEXT("Aim view: the Zoogi's screen height from the top (fraction of the view)."));
	static TAutoConsoleVariable<float> CVarAimFov(TEXT("zoogi.Cam.AimFov"), 0.f, TEXT("Aim view: horizontal FOV in degrees (0 = the tactical profile's)."));

	static TAutoConsoleVariable<float> CVarAimYawTime(TEXT("zoogi.Cam.AimYawTime"), 0.12f, TEXT("Aim view: seconds for the critically damped yaw follow to settle on the aim heading."));
	float YawTime() { return FMath::Clamp(CVarAimYawTime.GetValueOnGameThread(), 0.03f, 2.f); }
	bool Enabled() { return CVarAimView.GetValueOnGameThread() != 0; }
	float Pitch() { const float P = CVarAimPitch.GetValueOnGameThread(); return P > 0.f ? FMath::Clamp(P, 3.f, 80.f) : 0.f; }
	float HorizonY() { return FMath::Clamp(CVarAimHorizonY.GetValueOnGameThread(), 0.05f, 0.5f); }
	float ViewPitch(float TanV)
	{
		// the horizon (0 deg) sits (1 - 2*HorizonY) of the half-height above the centre: pitch = atan((1 - 2*HY) * tanV)
		if (Pitch() > 0.f) return Pitch();
		return FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan((1.f - 2.f * HorizonY()) * TanV)), 3.f, 45.f);
	}
	float Boom() { return FMath::Clamp(CVarAimBoom.GetValueOnGameThread(), 150.f, 5000.f); }
	float ScreenY() { return FMath::Clamp(CVarAimScreenY.GetValueOnGameThread(), 0.3f, 0.95f); }
	float Fov(float ProfileFov) { const float F = CVarAimFov.GetValueOnGameThread(); return F > 1.f ? FMath::Clamp(F, 30.f, 120.f) : ProfileFov; }

	void ChaseBoom(float BoomLen, float ViewPitch, float SY, float TanV, float& OutElev, float& OutAhead)
	{
		const float Th = FMath::DegreesToRadians(ViewPitch);
		const float E = FMath::Clamp(Th + FMath::Atan((2.f * SY - 1.f) * TanV), FMath::DegreesToRadians(2.f), FMath::DegreesToRadians(88.f));
		OutElev = FMath::RadiansToDegrees(E);
		OutAhead = BoomLen * FMath::Sin(E) / FMath::Max(FMath::Tan(Th), 0.01f) - BoomLen * FMath::Cos(E);
	}

	bool Project(const FVector& Loc, const FRotator& Rot, float HFov, float Aspect, const FVector& P, FVector2D& Out)
	{
		const FVector L = Rot.UnrotateVector(P - Loc);
		if (L.X < 1.f) { Out = FVector2D(10.f, 10.f); return false; }
		const float TanH = FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f));
		const float TanV = TanH / FMath::Max(Aspect, 0.01f);
		Out = FVector2D((L.Y / L.X) / TanH, (L.Z / L.X) / TanV);
		return true;
	}
}
