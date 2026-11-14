# b116a (Evan, screenshot with two lines: "in between these lines is a banner free zone - animate the profile out of the way and
# place banners there [above the top line] and at the bottom. Comment pop-ups all around the screen, just not directly centre"):
#  - P5 corner banners alternate: TOP band (portrait 20% / landscape 19%) and BOTTOM band (88% / 86%), never the middle
#  - while a top banner shows, the profiles / score panels slide out sideways (the existing split, ZoogiArenaHUD::NoteTopBanner)
#  - speech bubbles (callout lines + banter) sit on the left / right edges at varied heights, never centred
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:70]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiArenaHUDWidget.h'
ed1(H, '	ZOOGIROLL_API float DetailsReveal(float& OutBottomPx);', '''	ZOOGIROLL_API float DetailsReveal(float& OutBottomPx);
	/** b116: a P5 banner is in the top band this frame - the profiles slide out of its way (the cut-in split) */
	ZOOGIROLL_API void NoteTopBanner();
	ZOOGIROLL_API bool TopBannerOn();''')
W = 'Private/ZoogiArenaHUDWidget.cpp'
ed1(W, '''	float DetailsReveal(float& OutBottomPx) { OutBottomPx = GDetBottom; return GDetReveal; }''', '''	float DetailsReveal(float& OutBottomPx) { OutBottomPx = GDetBottom; return GDetReveal; }
	static double GTopBannerUntil = 0.0;
	void NoteTopBanner() { GTopBannerUntil = FPlatformTime::Seconds() + 0.12; }
	bool TopBannerOn() { return FPlatformTime::Seconds() < GTopBannerUntil; }''')
ed1(W, '''	const bool bVideo = bAnimeTop && Anime && Anime->bPlaying && Anime->HasMovie(); // b88c: clips play in the avatars, so the profiles stay put''',
'''	const bool bVideo = (bAnimeTop && Anime && Anime->bPlaying && Anime->HasMovie()) || ZoogiArenaHUD::TopBannerOn(); // b88c: clips play in the avatars, so the profiles stay put; b116: a top banner slides them out''')
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '''static void P5CornerBanner(UCanvas* C, float Wd, float Ht, float U, const FString& Text, const FString& Sub, const FLinearColor& Col, float Age, float Life, float Size)
{''', '''static void P5CornerBanner(UCanvas* C, float Wd, float Ht, float U, const FString& Text, const FString& Sub, const FLinearColor& Col, float Age, float Life, float Size, int32 Band = 0)
{''')
ed1(A, '''	const float MaxLen = bPort ? Wd * 0.94f : Wd * 0.48f;''', '''	const float MaxLen = Band == 1 ? (bPort ? Wd * 0.70f : Wd * 0.42f) : (bPort ? Wd * 0.94f : Wd * 0.48f); // b116: the bottom band stays clear of SUPERPOWER''')
ed1(A, '''		: FVector2D(Len * 0.5f + 24.f * U - Slide, Ht * (bPort ? 0.62f : 0.55f)); // b79 (Evan: "move the combo text to the left - it overlays the REACTION button"): left side, sliding in from the left''',
'''		: FVector2D(Len * 0.5f + 24.f * U - Slide, Ht * (Band == 1 ? (bPort ? 0.875f : 0.86f) : (bPort ? 0.20f : 0.19f))); // b79: left side, sliding in from the left; b116 (Evan): the top band or the bottom band - never the middle of the field''')
ed1(A, '''			P5CornerBanner(C, Wd, Ht, U, Co.Text, Co.Sub, Co.Col, Co.Age, Co.Life, Co.Size);''', '''			P5CornerBanner(C, Wd, Ht, U, Co.Text, Co.Sub, Co.Col, Co.Age, Co.Life, Co.Size, Co.Band);
			if (Co.Band == 0 && Co.Age < Co.Life) ZoogiArenaHUD::NoteTopBanner(); // the profiles slide out of the way''')
ed1(A, '''	Callouts.Reset(); // one callout at a time: the newest takes the spot (two at once overlapped)
	FCallout C;''', '''	Callouts.Reset(); // one callout at a time: the newest takes the spot (two at once overlapped)
	static int32 GBandFlip = 0; // b116: banners take turns - top band, bottom band
	FCallout C; C.Band = (GBandFlip++) & 1;''')
ed1('Public/ZoogiActionFun.h', '''	struct FCallout { FString Text, Sub; FLinearColor Col; float Age = 0.f, Life = 1.4f; float Size = 1.f; int32 Speaker = -1; FString Line; };''',
'''	struct FCallout { FString Text, Sub; FLinearColor Col; float Age = 0.f, Life = 1.4f; float Size = 1.f; int32 Speaker = -1; FString Line; int32 Band = 0; };''')
# speech bubbles: left / right edges, varied heights, never centred
ed1(A, '''	const float MaxW = (bPort ? Wd * 0.92f : Wd * 0.46f) - F - 30.f * U;''', '''	const float MaxW = (bPort ? Wd * 0.62f : Wd * 0.36f) - F - 30.f * U; // b116: narrower - it hugs one side''')
ed1(A, '''	const float X0 = bPort ? (Wd - Tot) * 0.5f : Wd * 0.5f - Tot * 0.5f, Y0 = Ht * (bPort ? 0.25f : 0.27f) + Slot * ((bPort ? 120.f : 96.f) * U + 34.f * U); // b67: banter slot 1 sits under slot 0. b78 (Evan: "comments never over the avi - ok lower"): below the profile band''',
'''	// b116 (Evan: "comment pop-ups all around the screen, just not directly centre"): six edge spots (left / right, top to bottom),
	// picked from the line itself (+ the slot) so a comment keeps its spot for its whole life
	static const float SpotY[6] = { 0.34f, 0.42f, 0.52f, 0.60f, 0.70f, 0.78f };
	const int32 Spot = (int32)((GetTypeHash(Line) + (uint32)Slot * 7u) % 6u);
	const bool bRightSide = (Spot % 2) == 1;
	const float X0 = bRightSide ? Wd - 24.f * U - Tot : 24.f * U, Y0 = Ht * SpotY[Spot] - F * 0.5f;''')
print('b116a ok')
