# b88c (Evan 14:36: "char anime clips after combo all play in that player's avi profile space"): the comic slices (the character
# panels after combos / trick shots) also play in the avatar circle, and the profiles no longer slide away for a cut-in video.
# -ZoogiAnimeTop / -ZoogiSliceBubbles = the old placements.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiComicSliceOverlay.h', [
 ('	bool ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const;\n',
  '	bool ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const;\n	/** b88c: the slice on screen plays inside that player\'s avatar circle (ZoogiArenaHUDWidget::SyncAviClip) - who and which texture */\n	bool AviSlice(int32& OutPlayer, UTexture2D*& OutTex) const;\n'),
])
ed(P + 'ZoogiComicSliceOverlay.cpp', [
 ('''bool UZoogiComicSliceOverlay::ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const''',
  '''bool UZoogiComicSliceOverlay::AviSlice(int32& OutPlayer, UTexture2D*& OutTex) const
{
	static const bool bBubbles = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSliceBubbles")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTop"));
	if (bBubbles || !bOn || Cur.IsNone() || bEyes) return false;
	const float t = float(FPlatformTime::Seconds() - CurStart);
	if (t < 0.f || t >= ZoogiComic::Total) return false;
	const TObjectPtr<UTexture2D>* Tp = Textures.Find(Cur);
	UTexture2D* T = Tp ? Tp->Get() : nullptr;
	if (!T || !T->GetResource()) return false;
	int32 Who = CurPlayer;
	if (Who < 0) if (const UWorld* W = GetWorld()) if (const AZoogiGameMode* GM = W->GetAuthGameMode<AZoogiGameMode>()) Who = GM->ActivePlayer;
	FVector2D Ap, As;
	if (Who < 0 || !ZoogiArenaHUD::AvatarRect(Who, Ap, As)) return false;
	OutPlayer = Who; OutTex = T;
	return true;
}

bool UZoogiComicSliceOverlay::ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const'''),
 ('''		if (bEyes && CurPlayer >= 0 && ZoogiFramed::ProfileRect(CurPlayer, PMin, PSize)) return;
	}''',
  '''		if (bEyes && CurPlayer >= 0 && ZoogiFramed::ProfileRect(CurPlayer, PMin, PSize)) return;
	}
	{ int32 Ap = -1; UTexture2D* At = nullptr; if (AviSlice(Ap, At)) return; } // b88c (Evan): it plays in the avatar circle instead'''),
])
ed(H + 'ZoogiArenaHUDWidget.h', [
 ('bool bAviClip = false; FSlateBrush AviSaved; };', 'bool bAviClip = false; FSlateBrush AviSaved; TWeakObjectPtr<UObject> AviRes; };'),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('#include "MediaTexture.h"\n', '#include "MediaTexture.h"\n#include "ZoogiComicSliceOverlay.h"\n'),
 ('''	int32 Who = -1;
	const bool bOn = An && An->MovieTexture && An->AviClip(Who) && Who == i;
	if (bOn == U.bAviClip) return;
	if (bOn)
	{	// the 16:9 clip centre-cropped into the round portrait
		U.AviSaved = Av->GetBrush();
		FSlateBrush Br = U.AviSaved;
		Br.SetResourceObject(An->MovieTexture.Get());
		Br.SetUVRegion(FBox2f(FVector2f(0.21875f, 0.f), FVector2f(0.78125f, 1.f)));
		Av->SetBrush(Br);
		UE_LOG(LogZoogi, Log, TEXT("ANIME wolf summon clip -> P%d avatar"), i + 1);
	}
	else Av->SetBrush(U.AviSaved);
	U.bAviClip = bOn;''',
  '''	int32 Who = -1;
	UObject* Want = nullptr; float Asp = 16.f / 9.f;
	if (An && An->MovieTexture && An->AviClip(Who) && Who == i) Want = An->MovieTexture.Get(); // anime clip / skill summon
	else if (const UZoogiComicSliceOverlay* Co = G->GetWorld()->GetSubsystem<UZoogiComicSliceOverlay>())
	{	// b88c: the comic slice after a combo / trick shot
		UTexture2D* St = nullptr;
		if (Co->AviSlice(Who, St) && Who == i && St) { Want = St; Asp = St->GetSurfaceWidth() / FMath::Max(1.f, St->GetSurfaceHeight()); }
	}
	if (Want == U.AviRes.Get() && (Want != nullptr) == U.bAviClip) return;
	if (Want)
	{	// centre-cropped into the round portrait
		if (!U.bAviClip) U.AviSaved = Av->GetBrush();
		FSlateBrush Br = U.AviSaved;
		Br.SetResourceObject(Want);
		if (Asp >= 1.f) { const float Uw = 1.f / Asp; Br.SetUVRegion(FBox2f(FVector2f(0.5f - Uw * 0.5f, 0.f), FVector2f(0.5f + Uw * 0.5f, 1.f))); }
		else Br.SetUVRegion(FBox2f(FVector2f(0.f, 0.5f - Asp * 0.5f), FVector2f(1.f, 0.5f + Asp * 0.5f)));
		Av->SetBrush(Br);
		U.bAviClip = true; U.AviRes = Want;
		UE_LOG(LogZoogi, Log, TEXT("ANIME %s -> P%d avatar"), *Want->GetName(), i + 1);
	}
	else { Av->SetBrush(U.AviSaved); U.bAviClip = false; U.AviRes = nullptr; }'''),
 ('''	const bool bVideo = Anime && Anime->bPlaying && Anime->HasMovie();
	const float Prev = SplitT;''',
  '''	static const bool bAnimeTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTop"));
	const bool bVideo = bAnimeTop && Anime && Anime->bPlaying && Anime->HasMovie(); // b88c: clips play in the avatars, so the profiles stay put
	const float Prev = SplitT;'''),
])
print('b88c ok')
