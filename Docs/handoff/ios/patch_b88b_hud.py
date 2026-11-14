# b88b (Evan 14:36: "char anime clips after combo all play in that player's avi profile space"): every character clip - the
# cut-ins (combo etc.) and every skill summon - plays inside that player's avatar circle. -ZoogiAnimeTop = the top card.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
if False: ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('''	static const bool bTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfClipTop"));
	if (bTop || CrashState < 2 || !bCrashFrames || IntroPlayer < 0 || !CrashId.StartsWith(TEXT("Anime_Awaken_WolfPack"))) return false;
	FVector2D P, S;
	if (!ZoogiArenaHUD::AvatarRect(IntroPlayer, P, S)) return false;
	OutPlayer = IntroPlayer;
	return true;''',
  '''	static const bool bTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfClipTop")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTop"));
	if (bTop) return false;
	int32 Pl = -1;
	if (CrashState >= 2 && bCrashFrames && CrashId.StartsWith(TEXT("Anime_Awaken_"))) Pl = IntroPlayer; // skill summons
	else if (CrashState == 0 && bActive && bMovie && !bP5CutIn && !bPanelMode && MovieTexture && MovieHasFrames()) Pl = Current.Player; // b88b: combo / character cut-ins
	if (Pl < 0) return false;
	FVector2D P, S;
	if (!ZoogiArenaHUD::AvatarRect(Pl, P, S)) return false;
	OutPlayer = Pl;
	return true;'''),
])
ed(P + 'ZoogiHUD.cpp', [
 ('''	if (Anime->HasMovie() && Anime->MovieTexture && (Anime->MovieHasFrames() || !Anime->bSliceMovie)) // b59: no black card while a fight slice warms up''',
  '''	{ int32 Ap = -1; if (Anime->AviClip(Ap)) return; } // b88b (Evan): character clips play in that player's avatar circle (ZoogiArenaHUDWidget::SyncAviClip)
	if (Anime->HasMovie() && Anime->MovieTexture && (Anime->MovieHasFrames() || !Anime->bSliceMovie)) // b59: no black card while a fight slice warms up'''),
])
print('b88b ok')
