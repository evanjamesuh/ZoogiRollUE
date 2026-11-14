# b122b (Evan 20:55, REDIRECT screenshot: "I'm unable to tap any of the circles to select the target. If I hit REACTION that should be a
# reaction; if I hit SUPERPOWER it should be that superpower ability; after it's activated gameplay starts again, then I can hit REACT or
# another SUPERPOWER for the next cast - interchangeable at my own will, and it has to know which is which"):
#  - the bug: on a phone a lifted finger reports no position, so every pick-on-release (REDIRECT, superpower bullet-time pick, since
#    b121) used a junk point. A release now uses the last position the finger was down at.
#  - REACTION and SUPERPOWER keep their own jobs at any time of the roll:
#      REACTION while a REDIRECT pick is open = closes the pick (nothing spent), play resumes
#      SUPERPOWER while a REDIRECT pick is open = closes the pick, then does the SUPERPOWER job (next COMBO cast / wolf / bolt / ultimate)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
P = 'Private/ZoogiPlayerController.cpp'
ed1(P, '''	const FVector2D Ptr = bTouch ? FVector2D(TchX, TchY) : Mouse;''', '''	// b122b (Evan: "I'm unable to tap any of the circles"): on a phone the frame the finger lifts has no touch position (Ptr fell back
	// to the mouse point) - a release uses the last spot the finger was down at, so pick-on-release lands where it was tapped
	const FVector2D PtrRaw = bTouch ? FVector2D(TchX, TchY) : Mouse;
	static FVector2D GPtrLastDown = FVector2D::ZeroVector;
	const bool bPtrNowDown = IsInputKeyDown(EKeys::LeftMouseButton) || bTouch;
	const FVector2D Ptr = (!bPtrNowDown && bPtrWasDown) ? GPtrLastDown : PtrRaw;
	if (bPtrNowDown) GPtrLastDown = PtrRaw;''')
RH = 'Public/ZoogiRedirect.h'; RC = 'Private/ZoogiRedirect.cpp'
ed1(RH, '''	ZOOGIROLL_API bool Picking();''', '''	ZOOGIROLL_API bool Picking();
	ZOOGIROLL_API void CancelPick(const TCHAR* Why); // b122b: closes an open pick (nothing spent)''')
ed1(RC, '''	bool Picking() { return bPick; }''', '''	bool Picking() { return bPick; }
	void CancelPick(const TCHAR* Why) { if (!bPick) return; UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick closed by %s (nothing spent)"), Why); Close(); }''')
CP = 'Private/ZoogiClassicPad.cpp'
ed1(CP, '''				const int32 TapMode = bPower ? 0 : PadTapMode();''', '''				// b122b (Evan: "REACTION is the reaction, SUPERPOWER is the superpower ability - interchangeable at my own will"):
				// an open REDIRECT pick never swallows the other button - REACTION closes it, SUPERPOWER closes it and does its own job
				if (ZoogiRedirect::Picking())
				{
					ZoogiRedirect::CancelPick(bPower ? TEXT("SUPERPOWER") : TEXT("REACTION"));
					if (!bPower) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return FReply::Handled().CaptureMouse(SharedThis(this)); }
				}
				const int32 TapMode = bPower ? 0 : PadTapMode();''')
print('b122b ok')
