# b145a (Evan 15:28 "Remove the second anime clip for the hotstreak awaken; move from full screen to top card view"):
# the SUPERPOWER-fired Awaken now plays only Anime_Showtime_<Key>_01, as the top-centre anime card (the game keeps running under it)
# instead of the full-screen smoke-wipe crash. New UZoogiAnimeSubsystem::PlayTopCard (never routed into an avatar circle; the profile
# panels slide out of the way). -ZoogiShowtimeFull = the b143 full-screen _01 + _02.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiAnimeSubsystem.h'
ed1(H, '''	bool PlayCrash(const FString& Id, const FLinearColor& Tint);''', '''	bool PlayCrash(const FString& Id, const FLinearColor& Tint);
	/** b145a (Evan: the Hotstreak Awaken "from full screen to top card view"): plays Content/Anime/Movies/<Id>.mp4 once, whole, in the
	 *  top-centre anime card (never an avatar circle) with Caption under it; the game keeps running. False when the clip is missing. */
	bool PlayTopCard(const FString& Id, const FString& InCaption, int32 Player, const FLinearColor& Tint);
	bool bTopCard = false;''')
A = 'Private/ZoogiAnimeSubsystem.cpp'
ed1(A, '''bool UZoogiAnimeSubsystem::AviClip(int32& OutPlayer) const
{''', '''bool UZoogiAnimeSubsystem::PlayTopCard(const FString& Id, const FString& InCaption, int32 Player, const FLinearColor& Tint)
{
	if (CrashState > 0 || bMoviesOff) return false;
	if (bActive) { TArray<FZoogiCombatEvent> Pending = MoveTemp(Queue); Queue.Reset(); Finish(true); Queue = MoveTemp(Pending); }
	if (!TryStartMovie(Id)) return false;
	Current = FZoogiCombatEvent(); Current.Event = FName(TEXT("Showtime")); Current.Player = Player;
	CurrentId = Id; CurrentMovie = Id; CurrentTier = TEXT("S"); Color = Tint; Caption = InCaption;
	bShort = false; bMovie = true; bSliceMovie = false; SliceWait = 0.f; PrevAge = 0.f; bP5CutIn = false; bPanelMode = false; bCutInShot = false; PanelShots = 0;
	bTopCard = true;
	const float* Secs = MovieSeconds.Find(Id);
	Length = FMath::Clamp(Secs ? *Secs : 4.f, 1.5f, 15.f);
	if (MoviePlayer) MoviePlayer->SetLooping(false);
	SubCaption = FString::Printf(TEXT("%s   top card %.1fs"), *Id, Length);
	bActive = true; Age = 0.f; RealStart = FPlatformTime::Seconds();
	Panels.Hide();
	SyncPlaying();
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnAnimeStarted(Current);
	UE_LOG(LogZoogi, Log, TEXT("Anime TOP CARD %s for P%d (%.2fs) - %s"), *Id, Player + 1, Length, *InCaption);
	return true;
}

bool UZoogiAnimeSubsystem::AviClip(int32& OutPlayer) const
{
	if (bTopCard) return false; // b145a: always the top card''')
ed1(A, '''	if (!bActive) return;
	bActive = false;
	bPanelMode = false;''', '''	if (!bActive) return;
	bActive = false;
	bTopCard = false; // b145a
	bPanelMode = false;''')
W = 'Private/ZoogiArenaHUDWidget.cpp'
ed1(W, '''	const bool bVideo = (bAnimeTop && Anime && Anime->bPlaying && Anime->HasMovie()) || ZoogiArenaHUD::TopBannerOn();''', '''	const bool bVideo = ((bAnimeTop || (Anime && Anime->bTopCard)) && Anime && Anime->bPlaying && Anime->HasMovie()) || ZoogiArenaHUD::TopBannerOn(); // b145a: a top-card clip slides them out too''')
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '''	GFireClips.Reset();
	for (int32 k = 1; k <= 2; ++k)''', '''	GFireClips.Reset();
	static const bool bFullScr = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeFull")); // b145a: the b143 full-screen _01 + _02
	GFireTop = !bFullScr; GFireCap = Aw && Aw->Name ? FString(Aw->Name) : FString(TEXT("METEOR STRIKE")); GFireP = B->PlayerIndex;
	for (int32 k = 1; k <= (bFullScr ? 2 : 1); ++k) // b145a (Evan): "remove the second anime clip for the hotstreak awaken"''')
ed1(S, '''			if (An->CrashOn()) return true;
			const FString Next = GFireClips[0]; GFireClips.RemoveAt(0);
			if (An->PlayCrash(Next, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (full screen)"), *Next);''', '''			if (An->CrashOn()) return true;
			const FString Next = GFireClips[0]; GFireClips.RemoveAt(0);
			if (GFireTop) { if (An->PlayTopCard(Next, GFireCap, GFireP, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (top card)"), *Next); }
			else if (An->PlayCrash(Next, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (full screen)"), *Next);''')
ed1(S, '''	TArray<FString> GFireClips; double GFireT0 = -1.0;''', '''	bool GFireTop = true; FString GFireCap; int32 GFireP = -1; // b145a
	TArray<FString> GFireClips; double GFireT0 = -1.0;''')
print('b145a ok')
