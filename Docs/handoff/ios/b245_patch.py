# Nov 11 (Claude): B245 - Evan 09:05 (photo of the full-screen FIRST KNOCKOUT! wall smash): "remove from full screen to pop out side window
# like the other celebrations but make it the flying ball anime". The wall-smash moments now play the arena's flying-ball crash anime
# (Anime_Crash_<map>) in the side pop-out window (where the celebration close-up shows), with the moment's title, and the game keeps going.
# -ZoogiWallCineFull = the old full-screen 3D cinematic.
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs, incs=()):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    for inc in incs:
        if '#include "%s"' % inc not in t:
            k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b245\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Public/ZoogiWallCine.h', [('	ZOOGIROLL_API void NoteAwaken(AZoogiGameMode* G, int32 Player);', '''	ZOOGIROLL_API void NoteAwaken(AZoogiGameMode* G, int32 Player);
	/** b245: the moment's title over the side-window anime (fun HUD canvas) */
	ZOOGIROLL_API void DrawPip(UCanvas* C, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text);''')])
patch('Private/ZoogiWallCine.cpp', [
('''				AZoogiBall* Pa = Pd.A.Get(); AZoogiBall* Pv = Pd.V.Get();
				if (Pa && Pv && !Pa->bOut && !Pv->bOut && Play(G, Pa, Pv, Pd.Kind))''',
'''				AZoogiBall* Pa = Pd.A.Get(); AZoogiBall* Pv = Pd.V.Get();
				static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallCineFull"));
				if (!bFull && Pa && Pv && PlayPip(G, Pa, Pv, Pd.Kind)) { ++M.Count[Pd.Kind]; ++M.Total; M.LastPlayTurn = G->TurnCounter; }
				else if (bFull && Pa && Pv && !Pa->bOut && !Pv->bOut && Play(G, Pa, Pv, Pd.Kind))'''),
('''	void NoteAwaken(AZoogiGameMode* G, int32 Player)''', '''	// ---- b245: the side-window flying-ball anime
	namespace
	{
		double GPipAt = -100.0; float GPipLen = 2.6f; int32 GPipKind = 0; FString GPipWho;
		const TCHAR* CrashClip(const AZoogiGameMode* G)
		{
			switch (G && G->Arena ? G->Arena->GetTheme() : EZoogiArena::Meadow)
			{
			case EZoogiArena::Frozen: return TEXT("Anime_Crash_FrozenRing_01"); case EZoogiArena::Volcanic: return TEXT("Anime_Crash_VolcanicPit_01");
			case EZoogiArena::Cosmic: return TEXT("Anime_Crash_CosmicPlatform_01"); case EZoogiArena::Arabian: return TEXT("Anime_Crash_ArabianNights_01");
			case EZoogiArena::Tomb: return TEXT("Anime_Crash_PharaohsTomb_01"); case EZoogiArena::Neon: return TEXT("Anime_Crash_NightCircuit_01");
			case EZoogiArena::Training: return TEXT("Anime_Crash_Training_01"); case EZoogiArena::Ancient: return TEXT("Anime_Crash_AncientMeadows_01");
			default: return TEXT("Anime_Crash_Meadow_01");
			}
		}
	}
	bool PlayPip(AZoogiGameMode* G, AZoogiBall* A, AZoogiBall* V, int32 Kind)
	{
		if (!G || !G->GetWorld() || ZoogiAnimeCues::Busy()) return false;
		ZoogiAnimeCues::FCue Cu;
		Cu.Id = FString::Printf(TEXT("wallsmash_%d"), Kind); Cu.Clip = CrashClip(G);
		// the side window where the celebration close-up shows (left, lower half; portrait: centre, lower half)
		Cu.Land.bFull = false; Cu.Land.X = 0.175f; Cu.Land.Y = 0.66f; Cu.Land.W = 0.31f; Cu.Land.H = 0.31f;
		Cu.Port.bFull = false; Cu.Port.X = 0.5f; Cu.Port.Y = 0.68f; Cu.Port.W = 0.6f; Cu.Port.H = 0.16f;
		Cu.Border = TEXT("p5"); Cu.Intro = TEXT("slide_left"); Cu.Outro = TEXT("fade"); Cu.IntroSec = 0.25f; Cu.OutroSec = 0.25f;
		Cu.Game = TEXT("play"); Cu.Duck = 0.4f; Cu.SkipAfter = 0.3f; Cu.Volume = 0.9f;
		if (!ZoogiAnimeCues::Play(G->GetWorld(), Cu)) return false;
		GPipAt = FPlatformTime::Seconds(); GPipKind = FMath::Clamp(Kind, 1, 6);
		GPipWho = FString::Printf(TEXT("%s  >  %s"), A ? *A->Def.Name.ToUpper() : TEXT("?"), V ? *V->Def.Name.ToUpper() : TEXT("?"));
		UE_LOG(LogZoogi, Log, TEXT("WALL SMASH pop-out kind %d: %s (%s)"), Kind, *Cu.Clip, *GPipWho);
		return true;
	}
	void DrawPip(UCanvas* Cv, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text)
	{
		const float t = (float)(FPlatformTime::Seconds() - GPipAt);
		if (!Cv || t < 0.f || t > GPipLen || !ZoogiAnimeCues::Playing()) return;
		static const TCHAR* Title[] = { TEXT("MATCH POINT KO!"), TEXT("AWAKEN KNOCKOUT!"), TEXT("WALL SLAM!"), TEXT("FIRST KNOCKOUT!"), TEXT("MEGA HIT!"), TEXT("REPLAY") };
		static const FLinearColor Col[] = { FLinearColor(1.f, 0.78f, 0.2f), FLinearColor(0.75f, 0.45f, 1.f), FLinearColor(1.f, 0.5f, 0.15f), FLinearColor(1.f, 0.35f, 0.4f), FLinearColor(0.35f, 0.9f, 1.f), FLinearColor::White };
		const float Wd = Cv->ClipX, Ht = Cv->ClipY, U = FMath::Min(Wd, Ht) / 1080.f; const bool bPort = Ht > Wd;
		const float Cx = (bPort ? 0.5f : 0.175f) * Wd, Top = (bPort ? 0.68f - 0.08f : 0.66f - 0.155f) * Ht;
		const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - t / 0.2f, 0.f, 1.f);
		Text(Title[GPipKind - 1], Cx, Top - 34.f * U, 52.f * U * Pop, Col[GPipKind - 1]);
		Text(GPipWho, Cx, Top - 2.f * U, 24.f * U, FLinearColor::White);
	}

	void NoteAwaken(AZoogiGameMode* G, int32 Player)'''),
], incs=('ZoogiAnimeCues.h',))
patch('Private/ZoogiActionFun.cpp', [('	ZoogiAwardBanner::Draw(G, C,', '	ZoogiWallCine::DrawPip(C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunTextL(C, Ts, Tx, Ty, Tp, Tc, 1); }); // b245\n	ZoogiAwardBanner::Draw(G, C,')])
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B244"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B245"), TEXT("WALL SMASH POP-OUT"), TEXT("The wall-smash moments (FIRST KNOCKOUT!, MEGA HIT! ...) no longer take over the screen: the arena\'s flying-ball crash anime pops out in the side window like the other celebrations, with the title on top, and the game keeps going.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
