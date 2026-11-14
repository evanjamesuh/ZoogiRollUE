// Oct 31 b41: Apple GCVirtualController for Battle Royale (see ZoogiApplePad.h).
#include "ZoogiApplePad.h"
#include "ZoogiRoll.h"
#include "ZoogiFront.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#if PLATFORM_IOS
#import <GameController/GameController.h>
#endif

namespace
{
	bool GWant = false;   // last request (game thread)
	bool GActive = false; // controller connected (game thread view)
#if PLATFORM_IOS
	id GVirtual = nil;    // GCVirtualController (retained, main thread only)
#endif
	bool Disabled()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoApplePad"));
		return bOff;
	}
}

void ZoogiApplePad::Want(bool bOn)
{
	if (Disabled()) bOn = false;
	if (bOn == GWant) return;
	GWant = bOn;
#if PLATFORM_IOS
	if (@available(iOS 15.0, *))
	{
		GActive = bOn;
		// Nov 1 b53 (Evan: "add a setting to hide left / right or both sticks"): the hidden side's thumbstick is left out of the
		// controller (that side then works by touch, invisible: left half = floating move stick, right half = swipe look)
		const int32 Hide = ZoogiControls::StickHide();
		const bool bLeft = (Hide & 1) == 0, bRight = (Hide & 2) == 0;
		UE_LOG(LogZoogi, Log, TEXT("APPLEPAD %s (GCVirtualController)"), bOn ? TEXT("show") : TEXT("hide"));
		dispatch_async(dispatch_get_main_queue(), ^{
			if (bOn)
			{
				if (!GVirtual)
				{
					GCVirtualControllerConfiguration* Cfg = [[GCVirtualControllerConfiguration alloc] init];
					NSMutableSet* Els = [NSMutableSet setWithObjects: GCInputButtonA, GCInputButtonB, GCInputButtonX, GCInputButtonY, nil];
					if (bLeft) [Els addObject: GCInputLeftThumbstick];
					if (bRight) [Els addObject: GCInputRightThumbstick];
					Cfg.elements = Els;
					GVirtual = [[GCVirtualController alloc] initWithConfiguration: Cfg];
					[Cfg release];
				}
				[(GCVirtualController*)GVirtual connectWithReplyHandler: ^(NSError* Err) { if (Err) NSLog(@"ZOOGI APPLEPAD connect failed: %@", Err); }];
			}
			else if (GVirtual)
			{
				[(GCVirtualController*)GVirtual disconnect];
				[(GCVirtualController*)GVirtual release]; // rebuilt on the next show with the current stick setting
				GVirtual = nil;
			}
		});
		return;
	}
#endif
	GActive = false;
	UE_LOG(LogZoogi, Log, TEXT("APPLEPAD %s request - not available here, drawn Royale controls stay"), bOn ? TEXT("show") : TEXT("hide"));
}

bool ZoogiApplePad::Active() { return GActive; }

// Nov 11 voyager port: the full controller on Voyager maps (own controller object, separate from the Royale one)
void ZoogiApplePad::WantFull(bool bOn)
{
	static bool bWas = false;
	if (Disabled()) bOn = false;
	if (bOn == bWas) return;
	bWas = bOn;
	UE_LOG(LogZoogi, Log, TEXT("APPLEPAD full %s"), bOn ? TEXT("show") : TEXT("hide"));
#if PLATFORM_IOS
	if (@available(iOS 15.0, *))
	{
		static id GFull = nil;
		dispatch_async(dispatch_get_main_queue(), ^{
			if (bOn)
			{
				if (!GFull)
				{
					GCVirtualControllerConfiguration* Cfg = [[GCVirtualControllerConfiguration alloc] init];
					Cfg.elements = [NSSet setWithObjects: GCInputLeftThumbstick, GCInputRightThumbstick, GCInputButtonA, GCInputButtonB, GCInputButtonX, GCInputButtonY,
						GCInputLeftShoulder, GCInputRightShoulder, GCInputLeftTrigger, GCInputRightTrigger, nil];
					GFull = [[GCVirtualController alloc] initWithConfiguration: Cfg];
					[Cfg release];
				}
				[(GCVirtualController*)GFull connectWithReplyHandler: ^(NSError* Err) { if (Err) NSLog(@"ZOOGI APPLEPAD full connect failed: %@", Err); }];
			}
			else if (GFull)
			{
				[(GCVirtualController*)GFull disconnect];
				[(GCVirtualController*)GFull release]; GFull = nil;
			}
		});
	}
#endif
}
