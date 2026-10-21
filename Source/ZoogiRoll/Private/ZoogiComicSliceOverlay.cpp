// Comic-slice overlay (see ZoogiComicSliceOverlay.h). Visual only: reads the battle / episode state, never writes it.

#include "ZoogiComicSliceOverlay.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiFramedLayout.h"
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
	// the shooter's turn start: each player's first turn, and every Awakened turn
	const int32 Key = G->TurnCounter * 8 + G->ActivePlayer;
	if (Key != LastTurnKey && G->Phase == EZoogiPhase::Aiming)
	{
		LastTurnKey = Key;
		const int32 P = G->ActivePlayer;
		bool bFirst = false;
		FirstTurnSeen.Add(P, &bFirst);
		bFirst = !bFirst;
		if (bFirst || (SM && SM->IsAwakened(P))) Trigger(EyesFor(G, P), 1, TEXT("turn_start"), P);
	}
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

	const float Wd = Canvas->ClipX, Ht = Canvas->ClipY, U = Ht / 1080.f;
	const float TW = FMath::Max(1.f, T->GetSurfaceWidth()), TH = FMath::Max(1.f, T->GetSurfaceHeight());
	const bool bLeft = (SideFlip & 1) != 0;

	// fit box + resting centre: eye strips across the upper middle (below the Episode callout line), panels on a side
	const FVector2D Box = bEyes ? FVector2D(0.56f * Wd, 0.30f * Ht) : FVector2D(0.40f * Wd, 0.40f * Ht);
	const float Fit = FMath::Min(Box.X / TW, Box.Y / TH);
	FVector2D Size(TW * Fit, TH * Fit);
	const FVector2D Rest = bEyes ? FVector2D(0.5f * Wd, 0.40f * Ht) : FVector2D((bLeft ? 0.26f : 0.74f) * Wd, (bLeft ? 0.56f : 0.60f) * Ht);
	// diagonal travel: from the lower-left (or lower-right) screen edge up into place, back out the same way
	const FVector2D Dir = FVector2D(bLeft ? 1.f : -1.f, -0.5f).GetSafeNormal();
	const float Dist = 0.5f * Wd + 0.5f * Size.X;

	FVector2D C = Rest;
	float Scale = 1.f, Ang = bLeft ? -3.f : 3.f;
	if (t < InT)
	{
		C = Rest - Dir * Dist * (1.f - EaseOutBack(t / InT));
		Scale = 1.12f;
	}
	else if (t < HoldEnd)
	{
		const float k = t - InT;
		Scale = FMath::Lerp(1.16f, 1.f, EaseOutBack(FMath::Clamp(k / 0.22f, 0.f, 1.f)));
		if (k < 0.3f)
		{
			const float Amp = 14.f * U * (1.f - k / 0.3f);
			C += FVector2D(FMath::Sin(k * 95.f) * Amp, FMath::Cos(k * 77.f) * Amp * 0.7f);
			Ang += FMath::Sin(k * 60.f) * 1.6f * (1.f - k / 0.3f);
		}
	}
	else
	{
		C = Rest - Dir * Dist * EaseIn((t - HoldEnd) / OutT);
	}
	Size *= Scale;
	const FVector2D Pos = C - Size * 0.5f;

	auto Draw = [&](const FVector2D& P, const FLinearColor& Col, ESimpleElementBlendMode Blend)
	{
		FCanvasTileItem I(P, T->GetResource(), Size, Col);
		I.BlendMode = Blend;
		I.Rotation = FRotator(0.f, Ang, 0.f);
		I.PivotPoint = FVector2D(0.5f, 0.5f);
		Canvas->DrawItem(I);
	};
	Draw(Pos + FVector2D(10.f, 12.f) * U, FLinearColor(0.f, 0.f, 0.f, 0.45f), SE_BLEND_Translucent); // drop shadow
	Draw(Pos, FLinearColor::White, SE_BLEND_Translucent);
}
