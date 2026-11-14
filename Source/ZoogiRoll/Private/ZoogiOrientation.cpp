// Oct 28 (Evan): runtime orientation switching (see ZoogiOrientation.h).
#include "ZoogiOrientation.h"
#include "ZoogiRoll.h"
#include "HAL/PlatformMisc.h"
#if PLATFORM_IOS
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#endif

void ZoogiOrientation::Allow(bool bPortraitToo, const TCHAR* Why)
{
	static int32 Last = -1;
	const int32 Want = bPortraitToo ? 1 : 0;
	if (Want == Last) return;
	Last = Want;
	UE_LOG(LogZoogi, Log, TEXT("ORIENTATION %s (%s)"), bPortraitToo ? TEXT("portrait + landscape") : TEXT("landscape only"), Why);
#if PLATFORM_IOS
	// The engine's FullSensor maps to UIInterfaceOrientationMaskAll, which IOSAppDelegate reads as "no runtime override" and
	// answers with the app settings' landscape-only mask. So the view's mask is set directly: portrait + both landscapes.
	dispatch_async(dispatch_get_main_queue(), ^{
		IOSAppDelegate* D = [IOSAppDelegate GetDelegate];
		if (!D || !D.IOSView) return;
		D.IOSView->SupportedInterfaceOrientations = Want ? UIInterfaceOrientationMaskAllButUpsideDown : UIInterfaceOrientationMaskLandscape;
		if (@available(iOS 16.0, *))
		{
			[D.IOSController setNeedsUpdateOfSupportedInterfaceOrientations];
		}
		else
		{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			[UIViewController attemptRotationToDeviceOrientation];
#pragma clang diagnostic pop
		}
	});
#elif PLATFORM_ANDROID
	FPlatformMisc::SetAllowedDeviceOrientation(bPortraitToo ? EDeviceScreenOrientation::FullSensor : EDeviceScreenOrientation::LandscapeSensor);
#endif
}
