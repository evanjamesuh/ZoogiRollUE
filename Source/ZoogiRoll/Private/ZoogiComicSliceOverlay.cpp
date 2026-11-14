// Comic-slice overlay (see ZoogiComicSliceOverlay.h). Visual only: reads the battle / episode state, never writes it.

#include "ZoogiComicSliceOverlay.h"
#include "ZoogiP5Style.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiArenaHUDWidget.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Misc/Parse.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace ZoogiComic
{
	constexpr float InT = 0.16f;      // slam in
	constexpr float HoldEnd = 1.2f;   // on screen until here
	constexpr float OutT = 0.26f;     // slide out
	constexpr float Total = HoldEnd + OutT;
	constexpr float Cooldown = 3.f;   // after a slice has left
	constexpr float PendingLife = 2.5f;

	static float EaseOutBack(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		const float C1 = 1.70158f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f);
	}
	static float EaseIn(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * X; }

	static const FName DemoOrder[] = {
		TEXT("panel_sky_bumper_impact"), TEXT("slice_eyes_blue"), TEXT("panel_halo_bank"), TEXT("panel_air_reaction"),
		TEXT("slice_eyes_orange_surprised"), TEXT("panel_orb_line"), TEXT("panel_partner_intercept"), TEXT("slice_eyes_red_angry"),
		TEXT("panel_air_vs_stone"), TEXT("panel_sky_bumper"), TEXT("slice_eyes_green_scan"), TEXT("panel_halo_rebound") };
}

bool UZoogiComicSliceOverlay::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

bool UZoogiComicSliceOverlay::Enabled()
{
	bool b = true;
	if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Comic"), TEXT("bComicSlices"), b, GGameIni);
	FString V;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiComicSlices="), V))
	{
		if (V.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || V == TEXT("0") || V.Equals(TEXT("False"), ESearchCase::IgnoreCase)) b = false;
		else if (V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V == TEXT("1") || V.Equals(TEXT("True"), ESearchCase::IgnoreCase)) b = true;
	}
	return b;
}

void UZoogiComicSliceOverlay::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bOn = Enabled();
	if (!bOn) { UE_LOG(LogZoogi, Log, TEXT("COMIC slices off")); return; }
	const TCHAR* Cmd = FCommandLine::Get();
	bDemo = FParse::Param(Cmd, TEXT("ZoogiComicDemo"));
	bShots = FParse::Param(Cmd, TEXT("ZoogiComicShots")) && FParse::Value(Cmd, TEXT("ZoogiShots="), ShotDir) && !ShotDir.IsEmpty();
	hHUD = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiComicSliceOverlay::OnHUDPostRender);
}

void UZoogiComicSliceOverlay::Deinitialize()
{
	if (hHUD.IsValid()) AHUD::OnHUDPostRender.Remove(hHUD);
	hHUD.Reset();
	Super::Deinitialize();
}

TStatId UZoogiComicSliceOverlay::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiComicSliceOverlay, STATGROUP_Tickables);
}

UTexture2D* UZoogiComicSliceOverlay::Tex(FName Slice)
{
	if (Slice.IsNone() || Missing.Contains(Slice)) return nullptr;
	if (TObjectPtr<UTexture2D>* T = Textures.Find(Slice)) return *T;
	const FString N = TEXT("T_") + Slice.ToString();
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/ComicSlices/%s.%s"), *N, *N));
	if (!T) { Missing.Add(Slice); UE_LOG(LogZoogi, Warning, TEXT("COMIC missing texture %s"), *N); return nullptr; }
#if WITH_EDITOR
	// uncooked runs build the texture on first load: finish it here (match start) so the HUD never draws a half-built one
	{ TArray<UTexture*> L; L.Add(T); FTextureCompilingManager::Get().FinishCompilation(L); }
#endif
	Textures.Add(Slice, T);
	return T;
}

FName UZoogiComicSliceOverlay::EyesFor(const AZoogiGameMode* G, int32 Player) const
{
	static const FName Blue(TEXT("slice_eyes_blue")), Orange(TEXT("slice_eyes_orange_surprised")), Red(TEXT("slice_eyes_red_angry")), Green(TEXT("slice_eyes_green_scan"));
	if (!G || !G->Balls.IsValidIndex(Player) || !G->Balls[Player]) return Blue;
	const FLinearColor HSV = G->Balls[Player]->Def.Color.LinearRGBToHSV(); // R = hue (deg), G = saturation
	if (HSV.G < 0.25f) return Blue;
	const float H = HSV.R;
	if (H < 15.f || H >= 330.f) return Red;
	if (H < 65.f) return Orange;
	if (H < 165.f) return Green;
	return Blue; // cyan / blue, and purple falls back to cyan
}

void UZoogiComicSliceOverlay::Trigger(FName Slice, int32 Priority, const TCHAR* Why, int32 Player)
{
	if (!bOn || Slice.IsNone()) return;
	if (!PendSlice.IsNone() && Priority < PendPri) return;
	PendSlice = Slice;
	PendPlayer = Player;
	PendPri = Priority;
	PendAt = FPlatformTime::Seconds();
	UE_LOG(LogZoogi, Verbose, TEXT("COMIC queued %s (%s)"), *Slice.ToString(), Why);
}

bool UZoogiComicSliceOverlay::Suppressed(const AZoogiGameMode* G) const
{
	if (G && G->ClimaxLeft > 0.f) return true;
	if (const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this))
		if (const UZoogiEpisodeSubsystem* Ep = SM->GetEpisode())
			if (Ep->IsHeld()) return true;
	if (const UZoogiEpisodeCameraDirector* D = UZoogiEpisodeCameraDirector::Get(this))
		if (D->IsMainViewTaken()) return true;
	return false;
}

void UZoogiComicSliceOverlay::ResetSeen()
{
	bSeeded = false;
	LastTurnKey = INT32_MIN;
	LastBigAction = -1;
	FirstTurnSeen.Reset();
	bAirWas = false;
}

void UZoogiComicSliceOverlay::Observe(AZoogiGameMode* G, double Now)
{
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	if (!bSeeded)
	{
		bSeeded = true;
		for (const FName& N : ZoogiComic::DemoOrder) Tex(N); // preload once at match start (no mid-play loads)
		if (SM)
		{
			SeenBumper = SM->BumperRebounds; SeenHalo = SM->HaloPasses; SeenRims = SM->HaloRims;
			SeenShatter = SM->ShatterCount; SeenHistory = SM->GetHistory().Num();
		}
	}
	if (SM)
	{
		if (SM->BumperRebounds > SeenBumper)
		{
			SeenBumper = SM->BumperRebounds;
			Trigger((BumperFlip++ & 1) ? TEXT("panel_sky_bumper") : TEXT("panel_sky_bumper_impact"), 3, TEXT("sky_bumper"));
		}
		if (SM->HaloPasses > SeenHalo) { SeenHalo = SM->HaloPasses; Trigger(TEXT("panel_halo_bank"), 2, TEXT("halo_pass")); }
		if (SM->HaloRims > SeenRims) { SeenRims = SM->HaloRims; Trigger(TEXT("panel_halo_rebound"), 1, TEXT("halo_rim")); }
		const bool bAir = SM->IsAirReactionOpen();
		if (bAir && !bAirWas) Trigger(TEXT("panel_air_reaction"), 4, TEXT("air_offer"));
		bAirWas = bAir;
		if (SM->ShatterCount > SeenShatter) { SeenShatter = SM->ShatterCount; Trigger(TEXT("panel_air_vs_stone"), 4, TEXT("shatter")); }
		const TArray<FZoogiPendingSegment>& Hist = SM->GetHistory();
		if (Hist.Num() < SeenHistory) SeenHistory = 0;
		for (int32 i = SeenHistory; i < Hist.Num(); ++i)
		{
			const FZoogiPendingSegment& S = Hist[i];
			if (S.Result != EZoogiChallengeResult::Success && S.Comic != EZoogiComicState::Completed) continue;
			switch (S.Challenge)
			{
			case EZoogiChallenge::OrbLine: Trigger(TEXT("panel_orb_line"), 3, TEXT("orb_line")); break;
			case EZoogiChallenge::PartnerIntercept: Trigger(TEXT("panel_partner_intercept"), 3, TEXT("partner_intercept")); break;
			case EZoogiChallenge::SkyBumper: Trigger(TEXT("panel_sky_bumper"), 3, TEXT("sky_bumper_challenge")); break;
			case EZoogiChallenge::HaloBank: Trigger(TEXT("panel_halo_bank"), 3, TEXT("halo_bank_challenge")); break;
			default: break;
			}
		}
		SeenHistory = Hist.Num();
		if (const UZoogiEpisodeSubsystem* Ep = SM->GetEpisode())
		{
			const FZoogiCinematicPlan& P = Ep->GetPlan();
			if (P.Tier >= 4 && P.ActionId != LastBigAction) { LastBigAction = P.ActionId; Trigger(EyesFor(G, G->ActivePlayer), 2, TEXT("big_moment"), G->ActivePlayer); }
		}
	}
	// Oct 23 Evan: turn-start comic strip / eye pop-in REMOVED (never show on player turn).
	// Other comic slices (bumper / halo / shatter / challenges / big_moment) unchanged.
}

bool UZoogiComicSliceOverlay::AviSlice(int32& OutPlayer, UTexture2D*& OutTex) const
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

bool UZoogiComicSliceOverlay::ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const
{
	using namespace ZoogiComic;
	if (!bOn || Cur.IsNone() || !bEyes || CurPlayer != Player || Player < 0) return false;
	const TObjectPtr<UTexture2D>* T = Textures.Find(Cur);
	if (!T || !*T || !(*T)->GetResource()) return false;
	const float t = float(FPlatformTime::Seconds() - CurStart);
	if (t < 0.f || t >= Total) return false;
	OutTex = *T;
	OutAlpha = t < InT ? t / InT : (t > HoldEnd ? 1.f - (t - HoldEnd) / OutT : 1.f);
	OutScale = t < InT ? FMath::Lerp(1.25f, 1.f, EaseOutBack(t / InT)) : 1.f;
	return true;
}

void UZoogiComicSliceOverlay::Start(double Now)
{
	Cur = PendSlice;
	CurPlayer = PendPlayer;
	PendPlayer = -1;
	PendSlice = NAME_None;
	PendPri = -1;
	CurStart = Now;
	bCurShot = false;
	bEyes = Cur.ToString().StartsWith(TEXT("slice_eyes"));
	++SideFlip;
	++Shown;
	FVector2D PMin = FVector2D::ZeroVector, PSize = FVector2D::ZeroVector;
	const bool bProf = bEyes && CurPlayer >= 0 && ZoogiFramed::ProfileRect(CurPlayer, PMin, PSize);
	UE_LOG(LogZoogi, Log, TEXT("COMIC slice %s (#%d)%s"), *Cur.ToString(), Shown,
		bProf ? *FString::Printf(TEXT(" over P%d profile card (%.0f,%.0f %.0fx%.0f)"), CurPlayer + 1, PMin.X, PMin.Y, PSize.X, PSize.Y) : TEXT(""));
}

void UZoogiComicSliceOverlay::Tick(float DeltaTime)
{
	if (!bOn) return;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? Cast<AZoogiGameMode>(W->GetAuthGameMode()) : nullptr;
	if (!G) return;
	const double Now = FPlatformTime::Seconds();
	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming)
	{
		ResetSeen();
		Cur = NAME_None; PendSlice = NAME_None; PendPri = -1;
		return;
	}
	if (G->Phase != EZoogiPhase::GameOver) Observe(G, Now);

	const bool bSup = Suppressed(G);
	if (!Cur.IsNone())
	{
		if (bSup) { Cur = NAME_None; NextAllowed = Now + ZoogiComic::Cooldown; }               // a cinematic took over: cut
		else if (Now - CurStart >= ZoogiComic::Total) { Cur = NAME_None; NextAllowed = Now + ZoogiComic::Cooldown; }
		else if (bShots && !bCurShot && ShotsTaken < 8 && Now - CurStart >= 0.45)
		{
			bCurShot = true;
			++ShotsTaken;
			const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("comic_%02d_%s.png"), ShotsTaken, *Cur.ToString()));
			FScreenshotRequest::RequestScreenshot(Path, true, false); // engine viewport (+ HUD), never the desktop
			UE_LOG(LogZoogi, Log, TEXT("COMIC screenshot requested: %s"), *Path);
		}
	}
	if (!PendSlice.IsNone() && Now - PendAt > ZoogiComic::PendingLife) { PendSlice = NAME_None; PendPri = -1; }
	if (bDemo && Cur.IsNone() && PendSlice.IsNone() && Now >= NextAllowed - 0.5)
		Trigger(ZoogiComic::DemoOrder[DemoIdx++ % UE_ARRAY_COUNT(ZoogiComic::DemoOrder)], 0, TEXT("demo"));
	if (Cur.IsNone() && !PendSlice.IsNone() && Now >= NextAllowed && !bSup && G->Phase != EZoogiPhase::GameOver && Tex(PendSlice))
		Start(Now);
	else if (!PendSlice.IsNone() && !Tex(PendSlice)) { PendSlice = NAME_None; PendPri = -1; }
}

void UZoogiComicSliceOverlay::OnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	using namespace ZoogiComic;
	if (!bOn || Cur.IsNone() || !HUD || !Canvas || HUD->GetWorld() != GetWorld()) return;
	if (ZoogiP5::CutInBusy() || ZoogiP5::ResultsOn()) return; // Oct 30: above the viewport widgets - off during the P5 cut-in
	UTexture2D* T = Tex(Cur);
	if (!T || !T->GetResource() || !T->GetResource()->IsInitialized()) return;
#if WITH_EDITOR
	if (T->IsCompiling()) return;
#endif
	const float t = float(FPlatformTime::Seconds() - CurStart);
	if (t < 0.f || t >= Total) return;
	{
		FVector2D PMin, PSize; // the eye strip lives on the player's profile card (team column paints it)
		if (bEyes && CurPlayer >= 0 && ZoogiFramed::ProfileRect(CurPlayer, PMin, PSize)) return;
	}
	{ int32 Ap = -1; UTexture2D* At = nullptr; if (AviSlice(Ap, At)) return; } // b88c (Evan): it plays in the avatar circle instead

	// Oct 28 (Evan): no more big slanted panels across the view - a small comment bubble streams out of that player's avatar
	// (the shooter's when the event names no one): it grows out of the avatar ring, holds ~1 s, then shrinks back into it.
	const float Wd = Canvas->ClipX, Ht = Canvas->ClipY, Sh = FMath::Min(Wd, Ht), U = Sh / 1080.f;
	const float TW = FMath::Max(1.f, T->GetSurfaceWidth()), TH = FMath::Max(1.f, T->GetSurfaceHeight());
	int32 Who = CurPlayer;
	if (Who < 0) if (const AZoogiGameMode* GM = HUD->GetWorld()->GetAuthGameMode<AZoogiGameMode>()) Who = GM->ActivePlayer;
	FVector2D AvC(0.f, 0.f);
	float AvR = 0.f;
	{
		FVector2D AP, AS, P0, P1, VP;
		if (Who >= 0 && ZoogiArenaHUD::AvatarRect(Who, AP, AS))
		{
			USlateBlueprintLibrary::AbsoluteToViewport(HUD, AP, P0, VP);
			USlateBlueprintLibrary::AbsoluteToViewport(HUD, AP + AS, P1, VP);
			AvC = (P0 + P1) * 0.5f;
			AvR = FMath::Max(8.f, (P1.X - P0.X) * 0.5f);
		}
	}
	if (AvR <= 0.f) { const bool bR = Who >= 0 && (Who % 2) == 1; AvC = FVector2D(bR ? Wd - 100.f * U : 100.f * U, 100.f * U); AvR = 64.f * U; } // no arena HUD: top corner
	const bool bRightSide = AvC.X > Wd * 0.5f;
	// size: the slice fits in ~19% of the short side tall / 28% of the width (was 40% x 40% of the screen)
	const float Fit = FMath::Min(0.28f * Wd / TW, 0.19f * Sh / TH);
	const FVector2D ImgSz(TW * Fit, TH * Fit);
	const float Pad = 9.f * U, Ol = 4.f * U, Rad = 20.f * U;
	const FVector2D BoxSz = ImgSz + FVector2D(Pad, Pad) * 2.f;
	// rest: just under the avatar, leaning toward the middle of the screen
	FVector2D TL(bRightSide ? AvC.X + AvR * 0.5f - BoxSz.X : AvC.X - AvR * 0.5f, AvC.Y + AvR * 1.45f + 18.f * U);
	TL.X = FMath::Clamp(TL.X, 12.f * U, FMath::Max(12.f * U, Wd - BoxSz.X - 12.f * U));
	TL.Y = FMath::Clamp(TL.Y, 12.f * U, FMath::Max(12.f * U, Ht - BoxSz.Y - 12.f * U));
	float K = 1.f, A = 1.f;
	if (t < InT * 1.6f) K = FMath::Max(0.05f, EaseOutBack(t / (InT * 1.6f)));
	else if (t > HoldEnd) { const float o = FMath::Clamp((t - HoldEnd) / OutT, 0.f, 1.f); K = 1.f - 0.92f * EaseIn(o); A = 1.f - o; }
	else TL.Y += FMath::Sin((t - InT) * 7.f) * 2.5f * U; // a tiny bob while it holds
	const FVector2D Tip(AvC.X, AvC.Y + AvR * 0.92f); // tail tip: the bottom of the avatar ring
	auto Xf = [&](const FVector2D& P) { return Tip + (P - Tip) * K; };
	const FTexture* White = Canvas->DefaultTexture ? Canvas->DefaultTexture->GetResource() : nullptr;
	if (!White) return;
	auto Tri = [](TArray<FCanvasUVTri>& Out, const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FLinearColor& C)
	{
		FCanvasUVTri Tr;
		Tr.V0_Pos = P0; Tr.V1_Pos = P1; Tr.V2_Pos = P2;
		Tr.V0_Color = Tr.V1_Color = Tr.V2_Color = C;
		Out.Add(Tr);
	};
	// rounded box + tail as one fan (no overlapping triangles, so translucent layers stay even)
	auto Bubble = [&](const FVector2D& Off, float Grow, const FLinearColor& C)
	{
		const FVector2D P = TL - FVector2D(Grow, Grow) + Off, S = BoxSz + FVector2D(Grow, Grow) * 2.f;
		const float R = FMath::Min(Rad + Grow, FMath::Min(S.X, S.Y) * 0.5f);
		TArray<FVector2D> Pts;
		const FVector2D Cn[4] = { FVector2D(P.X + S.X - R, P.Y + R), FVector2D(P.X + S.X - R, P.Y + S.Y - R), FVector2D(P.X + R, P.Y + S.Y - R), FVector2D(P.X + R, P.Y + R) };
		for (int32 c = 0; c < 4; ++c)
			for (int32 k = 0; k <= 6; ++k)
			{
				const float Ang = FMath::DegreesToRadians(-90.f + 90.f * c + 15.f * k);
				Pts.Add(Cn[c] + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R);
			}
		TArray<FCanvasUVTri> Tris;
		const FVector2D Mid = Xf(P + S * 0.5f);
		for (int32 k = 0; k < Pts.Num(); ++k) Tri(Tris, Mid, Xf(Pts[k]), Xf(Pts[(k + 1) % Pts.Num()]), C);
		// tail: a wedge from the box's top edge (avatar side) up to the avatar
		const float BaseC = FMath::Clamp(AvC.X, P.X + R + 22.f * U, P.X + S.X - R - 22.f * U), HalfW = 17.f * U + Grow;
		Tri(Tris, Xf(FVector2D(BaseC - HalfW, P.Y + 1.f)), Xf(FVector2D(BaseC + HalfW, P.Y + 1.f)), Xf(Tip + Off + FVector2D(0.f, -Grow * 0.6f)), C);
		FCanvasTriangleItem It(Tris, White);
		It.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(It);
	};
	Bubble(FVector2D(3.f, 6.f) * U, Ol, FLinearColor(0.f, 0.f, 0.f, 0.35f * A)); // drop shadow
	Bubble(FVector2D::ZeroVector, Ol, FLinearColor(0.06f, 0.07f, 0.09f, A));     // dark comic outline
	Bubble(FVector2D::ZeroVector, 0.f, FLinearColor(1.f, 1.f, 1.f, A));          // white bubble
	FCanvasTileItem I(Xf(TL + FVector2D(Pad, Pad)), T->GetResource(), ImgSz * K, FLinearColor(1.f, 1.f, 1.f, A));
	I.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(I);
}
