#include "ZoogiAimView.h"
#include "HAL/IConsoleManager.h"

namespace ZoogiAimView
{
	static TAutoConsoleVariable<int32> CVarAimView(TEXT("zoogi.Cam.AimView"), 1, TEXT("Aim step: camera behind and above the acting Zoogi looking up the field along the aim arrow (0 = the Oct 4 tactical aim view)."));
	static TAutoConsoleVariable<float> CVarAimPitch(TEXT("zoogi.Cam.AimPitch"), 24.f, TEXT("Aim view: camera pitch, degrees down (b79: 24 - lower behind the Zoogi, was 38; fixed; teaser-lower: 38 just under Tac 40, was 45; teaser-bible: 45 chase feel, was Oct 22: 36 / prior 38)."));
	static TAutoConsoleVariable<float> CVarAimHorizonY(TEXT("zoogi.Cam.AimHorizonY"), 0.33f, TEXT("Unused (the horizon auto pitch was dropped Oct 19; kept for scripts)."));
	static TAutoConsoleVariable<float> CVarAimBoom(TEXT("zoogi.Cam.AimBoom"), 800.f, TEXT("Aim view: camera distance from the Zoogi (cm; teaser-lower 800 was 900; Oct 22: 900, +29% from 700 - further back, camera only ~10% higher)."));
	static TAutoConsoleVariable<float> CVarAimScreenY(TEXT("zoogi.Cam.AimScreenY"), 0.47f, TEXT("Aim view: the Zoogi's screen height from the top (b79: 0.47 - drag-back room below, was 0.56; fraction of the full view; teaser-lower 0.56 slightly below center, was 0.50; Oct 22: 0.50, was 0.72 - the framed HUD's arena window ends at ~0.70 and the turn banner sits at ~0.57-0.67, so 0.60 hid the Zoogi; 0.50 shows it with drag-back room below)."));
	static TAutoConsoleVariable<float> CVarAimFov(TEXT("zoogi.Cam.AimFov"), 0.f, TEXT("Aim view: horizontal FOV in degrees (0 = the tactical profile's)."));

	static TAutoConsoleVariable<float> CVarAimYawTime(TEXT("zoogi.Cam.AimYawTime"), 0.12f, TEXT("Aim view: seconds for the critically damped yaw follow to settle on the aim heading."));
	float YawTime() { return FMath::Clamp(CVarAimYawTime.GetValueOnGameThread(), 0.03f, 2.f); }
	bool Enabled() { return CVarAimView.GetValueOnGameThread() != 0; }
	float Pitch() { return FMath::Clamp(CVarAimPitch.GetValueOnGameThread(), 3.f, 80.f); }
	float HorizonY() { return FMath::Clamp(CVarAimHorizonY.GetValueOnGameThread(), 0.05f, 0.5f); }
	float ViewPitch(float TanV)
	{
		// fixed AimPitch (the horizon auto pitch was too low; Evan Oct 19 wants the f98c01b angle back)
		return Pitch();
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
