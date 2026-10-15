#include "ZoogiHUD.h"
#include "ZoogiStageForming.h"
#include "ZoogiArena.h"
#include "ZoogiCombo.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiPlayerController.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiMenuRoot.h"
#include "ZoogiAwakenWidget.h"
#include "ZoogiOrb.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCardHandWidget.h"
#include "MediaTexture.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "ZoogiUIFx.h"
#include "CanvasTypes.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ZoogiEpisodeDirector.h"
#include "ZoogiUIStyle.h"
#include "ZoogiFramedLayout.h"
#include "SceneView.h"

static const FLinearColor GPanel = ZoogiUIStyle::PanelNavy; // uistyle: saturated navy (was near-black 0.02/0.03/0.08 @ 0.72)
static const FLinearColor GOrbBlue(0.53f, 0.81f, 0.92f, 1.f);
// Engine fonts are small at 1080p; scale everything up so labels read at a glance.
static constexpr float FlyTime = 0.65f; // orb pickup flight to the scoreboard
static float TextMul(bool bBig) { return bBig ? 1.75f : 2.3f; }

void AZoogiHUD::Panel(float X, float Y, float W, float H, const FLinearColor& C)
{
	// uistyle: the standard HUD panel colour draws as a rounded navy-outlined cartoon panel; thin bars / overlays stay flat
	if (C.Equals(GPanel) && W > 40.f * U && H > 30.f * U) { ZoogiUIStyle::CPanel(Canvas, X, Y, W, H, U, C); return; }
	DrawRect(C, X, Y, W, H);
}

// Crisp text: draw with a Slate font sized to the final pixel size instead of scaling up the small legacy glyph bitmaps
// (which looked soft at 1080p). Big labels use Roboto Bold.
static FSlateFontInfo HudFont(float FinalScale, bool bBig)
{
	UFont* F = bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
	Fi.Size = FMath::Max(6.f, FMath::RoundToFloat(Fi.Size * FinalScale));
	if (bBig) Fi.TypefaceFontName = FName(TEXT("Bold"));
	ZoogiUIStyle::AddOutline(Fi, FMath::Max(1.5f, Fi.Size * 0.07f)); // uistyle: navy text outline
	return Fi;
}

FVector2D AZoogiHUD::TextSize(const FString& S, float Scale, bool bBig)
{
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return M->Measure(S, HudFont(Scale * U * TextMul(bBig), bBig));
	}
	float W = 0.f, H = 0.f;
	GetTextSize(S, W, H, bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont(), Scale * U * TextMul(bBig));
	return FVector2D(W, H);
}

void AZoogiHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter, bool bBig)
{
	if (bCenter) X -= TextSize(S, Scale, bBig).X * 0.5f;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(S), HudFont(Scale * U * TextMul(bBig), bBig), C);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.06f, 0.75f * C.A), FVector2D(2.f * U, 3.f * U)); // uistyle: navy drop shadow
	Canvas->DrawItem(Item);
}

bool AZoogiHUD::Button(const FString& Label, float X, float Y, float W, float H, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected)
{
	float MX = 0.f, MY = 0.f;
	const bool bMouse = PlayerOwner && PlayerOwner->GetMousePosition(MX, MY);
	MX -= FrameOrg.X; MY -= FrameOrg.Y; // framed: canvas space = arena view rect
	const bool bHover = bMouse && MX >= X && MX <= X + W && MY >= Y && MY <= Y + H;
	// uistyle: chunky rounded plate (saturated navy idle, accent when selected), navy outline, gloss + accent strip
	FLinearColor Bg = bSelected ? Accent * 0.8f : ZoogiUIStyle::ButtonNavy;
	if (bHover) Bg = bSelected ? Accent * 0.95f : FLinearColor(0.08f, 0.26f, 0.72f, 1.f);
	Bg.A = 0.95f;
	const float BR = FMath::Min(12.f * U, H * 0.5f), BO = 3.5f * U;
	ZoogiUIStyle::CRoundRect(Canvas, X + 2.f * U, Y + 4.f * U, W, H, BR, FLinearColor(0.f, 0.f, 0.05f, 0.35f));
	ZoogiUIStyle::CRoundRect(Canvas, X, Y, W, H, BR, ZoogiUIStyle::Outline);
	ZoogiUIStyle::CRoundRect(Canvas, X + BO, Y + BO, W - 2.f * BO, H - 2.f * BO, FMath::Max(0.f, BR - BO), Bg);
	ZoogiUIStyle::CRect(Canvas, X + BR, Y + BO + 2.f * U, W - 2.f * BR, 4.f * U, FLinearColor(1.f, 1.f, 1.f, 0.28f));
	ZoogiUIStyle::CRect(Canvas, X + BR, Y + H - BO - 4.f * U, W - 2.f * BR, 4.f * U, Accent);
	const FVector2D TS = TextSize(Label, 0.55f, true);
	Text(Label, X + W * 0.5f, Y + (H - TS.Y) * 0.5f, FLinearColor::White, 0.55f, true, true);
	FZoogiButton B;
	B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	B.Action = Action;
	B.Index = Index;
	Buttons.Add(B);
	return bHover;
}

void AZoogiHUD::UpdateAwakenWidget(AZoogiGameMode* G, bool bHide)
{
	if (CanvasAwakenMode < 0) CanvasAwakenMode = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCanvasAwaken")) ? 1 : 0;
	if (CanvasAwakenMode != 0 || !PlayerOwner) return;
	if (!AwakenWidget)
	{
		if (bHide) return; // created lazily on the first match frame
		AwakenWidget = CreateWidget<UZoogiAwakenWidget>(PlayerOwner, UZoogiAwakenWidget::StaticClass());
		if (!AwakenWidget) return;
		AwakenWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AwakenWidget->AddToViewport(5);
		UE_LOG(LogZoogi, Log, TEXT("UMG Awaken controls up (Canvas fallback: -ZoogiCanvasAwaken)"));
	}
	AwakenWidget->Refresh(FApp::GetDeltaTime(), bHide);
}

bool AZoogiHUD::AwakenWidgetOverPanels(const FVector2D& P) const { return AwakenWidget && AwakenWidget->IsOverPanels(P); }

bool AZoogiHUD::UIConsumedClick() const
{
	return AwakenWidget && AwakenWidget->ConsumedClickRecently();
}

bool AZoogiHUD::IsOverUI(const FVector2D& ViewportPos) const
{
	return (AwakenWidget && AwakenWidget->IsOverPanels(ViewportPos)) || ZoogiFramed::IsOutsideArena(GetWorld(), ViewportPos); // framed: margins are UI, never world picks
}

bool AZoogiHUD::HitTest(const FVector2D& Mouse, FName& OutAction, int32& OutIndex) const
{
	for (const FZoogiButton& B : Buttons)
	{
		if (B.Rect.IsInside(Mouse - FrameOrg))
		{
			OutAction = B.Action;
			OutIndex = B.Index;
			return true;
		}
	}
	return false;
}

void AZoogiHUD::DrawHUD()
{
	Super::DrawHUD();
	Buttons.Reset();
	AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!G || !Canvas) return;
	U = Canvas->ClipY / 1080.f;
	FrameOrg = Canvas->SceneView ? FVector2D(Canvas->SceneView->UnscaledViewRect.Min.X, Canvas->SceneView->UnscaledViewRect.Min.Y) : FVector2D::ZeroVector;
	Time += FApp::GetDeltaTime(); // real time (independent of pause / fixed step)
	static const bool bNoHUD = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHUD")); // clean captures (portraits)
	if (bNoHUD && G->Phase != EZoogiPhase::Menu) { UpdateAwakenWidget(G, true); return; }

	if (CanvasMenuMode < 0) CanvasMenuMode = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCanvasMenu")) ? 1 : 0;
	UpdateAwakenWidget(G, G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming);
	if (G->Phase == EZoogiPhase::Aiming) DrawDragPower(G);
	if (G->Phase == EZoogiPhase::Menu)
	{
		if (CanvasMenuMode == 0 && PlayerOwner)
		{
			if (!MenuWidget)
			{
				MenuWidget = CreateWidget<UZoogiMenuRoot>(PlayerOwner, UZoogiMenuRoot::StaticClass());
				if (MenuWidget) MenuWidget->AddToViewport(10);
			}
			if (MenuWidget)
			{
				if (MenuWidget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
				{
					MenuWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					MenuWidget->MarkDirty();
					MenuWidget->SetBackdropPlaying(true);
				}
				MenuWidget->Refresh(GetWorld()->GetDeltaSeconds());
				return;
			}
		}
		DrawMenu(G);
		return;
	}
	if (MenuWidget && MenuWidget->GetVisibility() != ESlateVisibility::Collapsed) { MenuWidget->SetVisibility(ESlateVisibility::Collapsed); MenuWidget->SetBackdropPlaying(false); }
	TickClipFrames(G);
	TickUIPolish(G);
	if (G->Phase == EZoogiPhase::Forming) { DrawForming(G); return; }
	DrawMatch(G);
	if (G->Phase == EZoogiPhase::GameOver) { DrawResults(G); DrawProgressionBadge(G); }
}

void AZoogiHUD::TickClipFrames(AZoogiGameMode* G)
{
	// -ZoogiClipFrames -ZoogiShots=<dir>: dump Awaken panel frames as clip_awaken_####.png (pair with -UseFixedTimeStep -FPS=20).
	static const bool bClip = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClipFrames"));
	if (!bClip || G->ShotDir.IsEmpty()) return;
	if (!(AwakenWidget && AwakenWidget->ShowsAwaken() && ClipAwakenFrames < 70)) return;
	if (ClipAwakenT < 0.f) ClipAwakenT = 0.f;
	ClipAwakenT += FApp::GetDeltaTime();
	if (ClipAwakenT <= 0.05f) return;
	ClipAwakenT = 0.f;
	const FString Name = FString::Printf(TEXT("clip_awaken_%04d.png"), ClipAwakenFrames++);
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(G->ShotDir, Name), true, false);
}

void AZoogiHUD::TickUIPolish(AZoogiGameMode* G)
{
	// Battle-UI review only: -ZoogiUIPolish (-ZoogiUIPolish makes P0 human) so Awaken stays up for spinning stones.
	static const bool bPolish = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUIPolish"));
	if (!bPolish || !G) return;
	if (AwakenWidget && AwakenWidget->ShowsAwaken() && PolishStep < 2)
	{
		PolishAwakenT += FApp::GetDeltaTime();
		if (PolishStep == 0 && PolishAwakenT > 2.8f)
		{
			G->TakeShot(TEXT("awaken_stones"));
			PolishStep = 1;
		}
		if (PolishStep == 1 && PolishAwakenT > 5.5f)
		{
			G->MenuAction(TEXT("Infuse"), 0); // direct (QueueControlClick only ticks under -ZoogiUiTest)
			// -ZoogiPolishArt=Ember|Storm|Frost|Titan|Void: force the infused artifact so each Awaken colour can be captured.
			FString ArtKey;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiPolishArt="), ArtKey))
				if (AZoogiBall* PB = G->GetActiveBall())
					if (PB->Artifact)
						for (EZoogiArtifact A : { EZoogiArtifact::Ember, EZoogiArtifact::Storm, EZoogiArtifact::Frost, EZoogiArtifact::Titan, EZoogiArtifact::Void })
							if (ZoogiArtifacts::Key(A) == ArtKey) PB->Artifact->SetInfused(A);
			PolishStep = 2;
			UE_LOG(LogZoogi, Log, TEXT("UIPolish: Infuse after stone spin hold"));
		}
		return;
	}
	if (PolishStep == 2 && G->IsAimStep())
	{
		static float AimHold = 0.f;
		AimHold += FApp::GetDeltaTime();
		if (AimHold > 0.35f && !G->HasAim())
		{
			const TArray<AActor*> Cands = G->TargetCandidates();
			int32 CandIdx = 0; // -ZoogiPolishCand=N: lock a different target (e.g. for a bank shot)
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiPolishCand="), CandIdx);
			if (Cands.Num() > 0) G->LockOn(Cands[FMath::Clamp(CandIdx, 0, Cands.Num() - 1)], false);
		}
		if (AimHold > 1.4f && G->Preview.bValid && G->bAimActive)
		{
			G->TakeShot(TEXT("traj_aim"));
			PolishStep = 3;
			PolishBeamT = 0.f;
			UE_LOG(LogZoogi, Log, TEXT("UIPolish: trajectory aim shot (contact=%d)"), G->Preview.bContact ? 1 : 0);
		}
	}
	// -ZoogiClipFrames: ~5 s of the aim trajectory lines (clip_beam_####.png at 20 fps), aim held.
	static const bool bBeamClip = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClipFrames"));
	if (PolishStep == 3 && bBeamClip && !G->ShotDir.IsEmpty() && ClipBeamFrames < 100 && G->Preview.bValid && G->bAimActive)
	{
		PolishBeamT += FApp::GetDeltaTime();
		if (PolishBeamT > 0.04f)
		{
			PolishBeamT = 0.f;
			FScreenshotRequest::RequestScreenshot(FPaths::Combine(G->ShotDir, FString::Printf(TEXT("clip_beam_%04d.png"), ClipBeamFrames++)), true, false);
		}
	}
}

void AZoogiHUD::DrawForming(AZoogiGameMode* G)
{
	// Stage forming intro (Frozen Ring, Oct 6): title, progress, SKIP.
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Prog = (G->Arena && G->Arena->Forming) ? G->Arena->Forming->GetProgress() : 1.f;
	const float A = FMath::Clamp(G->PhaseTime * 3.f, 0.f, 1.f);
	const FLinearColor Cy(0.55f, 0.95f, 1.f, A);
	const bool bFrozen = G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen;
	Text(bFrozen ? TEXT("THE FROZEN RING AWAKENS") : TEXT("THE ARENA FORMS"), W * 0.5f, H * 0.12f, Cy, 1.1f, true);
	const float BW = 360.f * U;
	Panel((W - BW) * 0.5f, H * 0.9f, BW, 6.f * U, FLinearColor(0.05f, 0.1f, 0.18f, 0.8f * A));
	Panel((W - BW) * 0.5f, H * 0.9f, BW * FMath::Clamp(Prog, 0.f, 1.f), 6.f * U, Cy);
	if (G->PhaseTime > 0.25f)
	{
		Button(TEXT("SKIP"), W - 190.f * U, H * 0.9f - 10.f * U, 150.f * U, 52.f * U, "FormSkip", 0, FLinearColor(0.8f, 0.9f, 1.f));
		Text(TEXT("click / SPACE to skip"), W * 0.5f, H * 0.9f + 14.f * U, FLinearColor(0.8f, 0.9f, 1.f, 0.7f * A), 0.32f, true, false);
	}
}

void AZoogiHUD::DrawMenu(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	Panel(0, 0, W, H, FLinearColor(0.f, 0.02f, 0.06f, 0.35f));
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 2.f);
	Text(TEXT("ZOOGI ROLL"), W * 0.5f, 70.f * U, FLinearColor::LerpUsingHSV(GOrbBlue, FLinearColor::White, Pulse * 0.4f), 3.2f, true);
	Text(TEXT("draw an artifact - awaken it - aim - knock orbs and rivals off the edge - gold stars give extra draws"), W * 0.5f, 190.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.7f, true, true);

	const float PW = 980.f * U;
	const float X0 = (W - PW) * 0.5f;
	float Y = 260.f * U;
	Panel(X0, Y, PW, 600.f * U, GPanel);
	Y += 24.f * U;

	Text(TEXT("PLAYERS"), X0 + 30.f * U, Y + 8.f * U, FLinearColor(0.7f, 0.8f, 1.f), 0.6f);
	for (int32 n = 2; n <= 4; ++n)
	{
		Button(FString::FromInt(n), X0 + 260.f * U + (n - 2) * 90.f * U, Y, 76.f * U, 52.f * U, "Players", n, GOrbBlue, G->MenuNumPlayers == n);
	}
	Text(TEXT("ROUNDS"), X0 + 560.f * U, Y + 8.f * U, FLinearColor(0.7f, 0.8f, 1.f), 0.6f);
	Button(TEXT("-"), X0 + 720.f * U, Y, 56.f * U, 52.f * U, "Rounds", G->MenuRounds - 1, GOrbBlue);
	Text(FString::FromInt(G->MenuRounds), X0 + 815.f * U, Y + 6.f * U, FLinearColor::White, 0.75f, true);
	Button(TEXT("+"), X0 + 856.f * U, Y, 56.f * U, 52.f * U, "Rounds", G->MenuRounds + 1, GOrbBlue);
	{
		const int32 Vol = FMath::RoundToInt(ZoogiUtil::GetSfxVolume() * 100.f);
		const float SY = Y - 100.f * U; // just above the panel, right side
		Text(TEXT("SFX"), X0 + PW - 330.f * U, SY + 8.f * U, FLinearColor(0.7f, 0.8f, 1.f), 0.6f);
		Button(TEXT("-"), X0 + PW - 250.f * U, SY, 56.f * U, 52.f * U, "SfxVol", FMath::Max(0, Vol - 25), GOrbBlue);
		Text(FString::Printf(TEXT("%d%%"), Vol), X0 + PW - 140.f * U, SY + 6.f * U, FLinearColor::White, 0.6f, true);
		Button(TEXT("+"), X0 + PW - 90.f * U, SY, 56.f * U, 52.f * U, "SfxVol", FMath::Min(100, Vol + 25), GOrbBlue);
	}
	Y += 80.f * U;

	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	for (int32 i = 0; i < G->MenuNumPlayers; ++i)
	{
		const FZoogiPlayerSetup& S = G->MenuPlayers[i];
		const FZoogiDef& D = Roster[S.ZoogiIndex % Roster.Num()];
		const FLinearColor PC = ZoogiUtil::PlayerColor(i);
		Panel(X0 + 20.f * U, Y, PW - 40.f * U, 66.f * U, FLinearColor(0.05f, 0.07f, 0.14f, 0.9f));
		Panel(X0 + 20.f * U, Y, 10.f * U, 66.f * U, PC);
		Text(FString::Printf(TEXT("P%d"), i + 1), X0 + 50.f * U, Y + 12.f * U, PC, 0.7f);
		Button(TEXT("<"), X0 + 130.f * U, Y + 8.f * U, 50.f * U, 50.f * U, "ZoogiPrev", i, PC);
		Text(D.Name, X0 + 320.f * U, Y + 6.f * U, D.Color.Desaturate(-0.2f) + FLinearColor(0.25f, 0.25f, 0.25f, 0.f), 0.7f, true);
		Text(FString::Printf(TEXT("%s  -  %s  -  mass %.2f"), *D.Type, *D.Ability, D.MassRatio()), X0 + 320.f * U, Y + 42.f * U, FLinearColor(0.75f, 0.8f, 0.9f), 0.42f, true, false);
		Button(TEXT(">"), X0 + 460.f * U, Y + 8.f * U, 50.f * U, 50.f * U, "ZoogiNext", i, PC);
		Button(S.bCPU ? TEXT("CPU") : TEXT("HUMAN"), X0 + 540.f * U, Y + 8.f * U, 150.f * U, 50.f * U, "ToggleCPU", i, PC, !S.bCPU);
		Text(FString::Printf(TEXT("SPD %d  PWR %d  DEF %d  CTL %d"), D.Speed, D.Power, D.Defense, D.Control), X0 + 715.f * U, Y + 20.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.42f, false, false);
		Y += 78.f * U;
	}
	Y = 260.f * U + 24.f * U + 80.f * U + 4 * 78.f * U + 6.f * U;
	Text(TEXT("ARENA"), X0 + 30.f * U, Y + 10.f * U, FLinearColor(0.7f, 0.8f, 1.f), 0.6f);
	for (int32 a = 0; a < (int32)EZoogiArena::MAX; ++a)
	{
		const int32 Col = a % 3, Row = a / 3;
		Button(ZoogiUtil::ArenaName((EZoogiArena)a), X0 + 160.f * U + Col * 268.f * U, Y + Row * 60.f * U, 258.f * U, 52.f * U, "Arena", a, FLinearColor(0.4f, 0.9f, 0.4f), G->MenuArena == (EZoogiArena)a);
	}

	Button(TEXT("START MATCH"), W * 0.5f - 200.f * U, 890.f * U, 400.f * U, 80.f * U, "Start", 0, FLinearColor(1.f, 0.75f, 0.2f), true);
	Text(TEXT("Awaken: I / G / S   LMB: target or floor   A/D or arrows: cut   SPACE: confirm   RMB/Esc: cancel   C: camera"), W * 0.5f, 1000.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.45f, true, false);
	Button(TEXT("QUIT"), W - 160.f * U, 30.f * U, 130.f * U, 50.f * U, "Quit", 0, FLinearColor(0.9f, 0.3f, 0.3f));
}

void AZoogiHUD::DrawScoreboard(AZoogiGameMode* G)
{
	float Y = 24.f * U;
	const float X = 24.f * U;
	const float RowH = 58.f * U;
	const bool bFramed = ZoogiFramed::IsActive(GetWorld()); // framed: player rows live in the side columns
	if (!bFramed) Panel(X, Y, 400.f * U, RowH * G->Balls.Num() + 16.f * U, GPanel);
	Y += bFramed ? -22.f * U : 8.f * U;
	for (AZoogiBall* B : G->Balls)
	{
		if (bFramed) break;
		if (!B) continue;
		const bool bActive = B->PlayerIndex == G->ActivePlayer && G->Phase != EZoogiPhase::GameOver;
		const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
		if (bActive) Panel(X + 6.f * U, Y, 388.f * U, RowH - 6.f * U, PC * FLinearColor(1, 1, 1, 0.25f));
		// Orb pickup arriving: flash the row.
		float Flash = 0.f;
		for (const FZoogiFlyer& F : G->Flyers) if (F.Player == B->PlayerIndex && F.Age > FlyTime) Flash = FMath::Max(Flash, 1.f - (F.Age - FlyTime) / (1.2f - FlyTime));
		if (Flash > 0.f) Panel(X + 6.f * U, Y, 388.f * U, RowH - 6.f * U, FLinearColor(1.f, 1.f, 1.f, 0.35f * Flash));
		Panel(X + 6.f * U, Y, 8.f * U, RowH - 6.f * U, PC);
		Text(FString::Printf(TEXT("P%d %s%s"), B->PlayerIndex + 1, *B->Def.Name, B->bCPU ? TEXT(" (CPU)") : TEXT("")), X + 26.f * U, Y + 4.f * U, bActive ? FLinearColor::White : FLinearColor(0.8f, 0.85f, 0.95f), 0.55f);
		FString Status = G->bComboMode ? FString::Printf(TEXT("%s   "), *AZoogiGameMode::TeamName(B->TeamIndex)) : FString::Printf(TEXT("orbs %d  KOs %d  "), B->OrbsScored, B->KnockOuts);
		if (G->bComboMode) RingMarks(B->RingMarks, X + 262.f * U, Y + 12.f * U, 13.f * U, B->bKnockedOut);
		if (G->bArtifactMode && B->Artifact)
		{
			Status += FString::Printf(TEXT("hand %d"), B->Artifact->Hand.Num());
			if (B->Artifact->Guard != EZoogiArtifact::None) Status += TEXT("  GUARD");
			if (B->Artifact->Status != EZoogiStatus::None) Status += FString::Printf(TEXT("  %s"), ZoogiArtifacts::StatusName(B->Artifact->Status));
		}
		else for (int32 c = 0; c < 3; ++c) Status += c < B->Charges ? TEXT("*") : TEXT("-");
		if (B->bKnockedOut) Status += TEXT("  KO");
		else if (B->bOut) Status += TEXT("  OUT");
		if (B->StunTurns > 0) Status += TEXT("  STUNNED");
		if (B->BoundShots > 0) Status += TEXT("  BOUND");
		Text(Status, X + 26.f * U, Y + 32.f * U, (B->StunTurns > 0 || B->BoundShots > 0) ? FLinearColor(0.85f, 0.7f, 1.f) : FLinearColor(0.7f, 0.88f, 1.f), 0.38f, false, false);
		if (G->bArtifactMode && B->Artifact)
		{
			// Hand gems next to the score.
			float GX = X + 300.f * U;
			for (int32 h = B->Artifact->Hand.Num() - 1; h >= 0; --h) { Gem((uint8)B->Artifact->Hand[h], GX, Y + 44.f * U, 9.f * U, 0.9f); GX -= 22.f * U; }
			if (B->Artifact->Infused != EZoogiArtifact::None) Gem((uint8)B->Artifact->Infused, X + 330.f * U, Y + 44.f * U, 11.f * U, 0.6f + 0.4f * FMath::Sin(Time * 8.f));
		}
		Text(FString::FromInt(B->Score), X + 380.f * U - TextSize(FString::FromInt(B->Score), 0.75f).X, Y + 6.f * U, PC + FLinearColor(0.2f, 0.2f, 0.2f, 0.f), 0.75f);
		Y += RowH;
	}
	if (bFramed) return; // framed: the event feed is drawn in the team-column margin, never over the arena
	// Event feed under the scoreboard: newest on top, fades out after a few seconds.
	Y += 22.f * U;
	for (const FZoogiFeedItem& F : G->Feed)
	{
		const float A = FMath::Clamp(1.f - (F.Age - 4.5f) / 1.5f, 0.f, 1.f) * FMath::Clamp(F.Age * 6.f, 0.f, 1.f);
		if (A <= 0.f) continue;
		const float Slide = (1.f - FMath::Clamp(F.Age * 5.f, 0.f, 1.f)) * -40.f * U;
		const float W = TextSize(F.Text, 0.42f, false).X + 30.f * U;
		Panel(X + Slide, Y, W, 34.f * U, FLinearColor(GPanel.R, GPanel.G, GPanel.B, GPanel.A * A));
		Panel(X + Slide, Y, 5.f * U, 34.f * U, FLinearColor(F.Color.R, F.Color.G, F.Color.B, A));
		Text(F.Text, X + Slide + 16.f * U, Y + 6.f * U, FLinearColor(F.Color.R * 0.4f + 0.6f, F.Color.G * 0.4f + 0.6f, F.Color.B * 0.4f + 0.6f, A), 0.42f, false, false);
		Y += 40.f * U;
	}
}

void AZoogiHUD::DrawMatch(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	// Battle: cinematic letterbox bars slide in (and out again as the shot rolls).
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	const bool bBattle = (G->bCinematicCam && G->IsPowerUpStep()) || (Anime && Anime->bPlaying); // letterbox: cinematic power-up or anime only
	const float Dt = FApp::GetDeltaTime();
	Letterbox = FMath::FInterpTo(Letterbox, bBattle ? 1.f : 0.f, Dt, bBattle ? 7.f : 4.f);
	if (Letterbox > 0.01f)
	{
		const float BarH = H * 0.075f * Letterbox;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, 0.f, W, BarH);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.92f), 0.f, H - BarH, W, BarH);
	}
	DrawScoreboard(G);
	const bool bFramed = ZoogiFramed::IsActive(GetWorld()); // framed: header / prompt / buttons are drawn in the margins

	// Top centre: round, orbs left, turn banner.
	const FString Info = G->bComboMode
		? FString::Printf(TEXT("2v2 COMBO   ROUND %d / %d     TEAM A %d  -  %d TEAM B"), FMath::Min(G->RoundNumber, G->MenuRounds), G->MenuRounds, G->TeamScore(0), G->TeamScore(1))
		: FString::Printf(TEXT("ROUND %d / %d     ORBS LEFT %d"), FMath::Min(G->RoundNumber, G->MenuRounds), G->MenuRounds, G->OrbsLeft());
	if (!bFramed) Panel(W * 0.5f - 260.f * U, 20.f * U, 520.f * U, 46.f * U, GPanel);
	if (!bFramed) Text(Info, W * 0.5f, 28.f * U, GOrbBlue, 0.55f, true);
	if (AZoogiBall* B = G->GetActiveBall())
	{
		if (G->Phase != EZoogiPhase::GameOver)
		{
			const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
			const float A = FMath::Clamp(1.6f - G->BannerTime * 0.5f, 0.f, 1.f);
			if (A > 0.f)
			{
				const FString T = FString::Printf(TEXT("%s'S TURN"), *G->Banner.ToUpper());
				Text(T, W * 0.5f, 122.f * U, FLinearColor(PC.R, PC.G, PC.B, A), 1.1f, true);
			}
			FString Status;
			if (G->Phase == EZoogiPhase::Rolling) Status = TEXT("rolling...");
			else if (G->Phase == EZoogiPhase::Resolving) Status = TEXT("resolving...");
			else if (G->Stage == EZoogiStage::Draw) Status = TEXT("Drawing an artifact...");
			else if (G->Stage == EZoogiStage::Awaken) Status = B->bCPU ? TEXT("CPU is awakening...") : TEXT("");
			else if (B->bCPU) Status = G->TargetLock.IsValid() ? FString::Printf(TEXT("CPU is targeting %s"), *G->TargetName(G->TargetLock.Get())) : TEXT("CPU is choosing a target");
			else if (G->IsPowerUpStep()) Status = TEXT("POWER UP - launching!");
			else if (G->HasAim()) Status = TEXT("");
			else Status = TEXT("Click a rival or orb to lock on - or click the floor to aim at a point");
			if (!Status.IsEmpty() && !bFramed) Text(Status, W * 0.5f, H - 70.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.5f, true, false);
		}
	}

	// Player tags: a small pill in the player's colour over each Zoogi so it's always clear who is who
	// (skipped during the hero intro and the letterboxed Battle close-up).
	if (!(G->bCinematicCam && (G->Stage == EZoogiStage::Draw || G->IsPowerUpStep())) && G->Phase != EZoogiPhase::GameOver && !(Anime && Anime->bPlaying))
	{
		for (AZoogiBall* Bt : G->Balls)
		{
			if (!Bt || Bt->bOut) continue;
			const FVector S = Project(Bt->BallLocation() + FVector(0, 0, ZoogiConst::ZoogiRadius + 70.f), true);
			if (S.Z <= 0.f) continue;
			if (bFramed && ZoogiFramed::IsOutsideArena(GetWorld(), FVector2D(S.X, S.Y - 30.f * U))) continue; // hud-v3: no tags over the frame / name plate
			const FLinearColor PC = ZoogiUtil::PlayerColor(Bt->PlayerIndex);
			FString Tag = FString::Printf(TEXT("P%d"), Bt->PlayerIndex + 1);
			if (Bt->StunTurns > 0) Tag += TEXT("  STUN");
			else if (Bt->BoundShots > 0) Tag += TEXT("  BOUND");
			if (Bt->Artifact)
			{
				if (Bt->Artifact->Status != EZoogiStatus::None) Tag += FString::Printf(TEXT("  %s"), ZoogiArtifacts::StatusName(Bt->Artifact->Status));
				if (Bt->Artifact->Guard != EZoogiArtifact::None) Tag += TEXT("  GUARD");
			}
			const bool bActive = Bt == G->GetActiveBall();
			const float Sc = bActive ? 0.8f : 0.66f;
			const FVector2D TS = TextSize(Tag, Sc, true);
			const float PadX = 8.f * U, PadY = 2.f * U;
			const float X0 = S.X - TS.X * 0.5f - PadX, Y0 = S.Y - TS.Y - PadY * 2.f;
			Panel(X0, Y0, TS.X + PadX * 2.f, TS.Y + PadY * 2.f, FLinearColor(PC.R * 0.35f, PC.G * 0.35f, PC.B * 0.35f, 0.8f));
			Panel(X0, Y0 + TS.Y + PadY * 2.f - 3.f * U, TS.X + PadX * 2.f, 3.f * U, PC);
			Text(Tag, S.X, Y0 + PadY, FLinearColor(1.f, 1.f, 1.f, 0.95f), Sc, true, true);
		}
	}

	if (G->Phase == EZoogiPhase::Aiming) { if (G->bArtifactMode || G->Preview.bValid) DrawPreview(G); else DrawAim(G); }
	if (G->bArtifactMode) DrawArtifacts(G); else DrawPower(G);
	DrawPopups(G);
	DrawFlyers(G);
	// Knockout callout: pops in, holds, fades (about 1.6 s).
	if (G->BigAge < 1.6f && !G->BigText.IsEmpty())
	{
		const float A = FMath::Clamp((1.6f - G->BigAge) / 0.35f, 0.f, 1.f);
		const float Pop = 1.f + 0.6f * FMath::Clamp(1.f - G->BigAge / 0.12f, 0.f, 1.f);
		FString L1, L2;
		if (!G->BigText.Split(TEXT("\n"), &L1, &L2)) L1 = G->BigText;
		const FLinearColor C(FMath::Min(1.f, G->BigColor.R + 0.25f), FMath::Min(1.f, G->BigColor.G + 0.25f), FMath::Min(1.f, G->BigColor.B + 0.25f), A);
		const float Y = Canvas->ClipY * 0.30f;
		Panel(0.f, Y - 14.f * U, Canvas->ClipX, 150.f * U, FLinearColor(0.f, 0.f, 0.f, 0.35f * A));
		Text(L1, Canvas->ClipX * 0.5f, Y, C, 1.9f * Pop, true);
		if (!L2.IsEmpty()) Text(L2, Canvas->ClipX * 0.5f, Y + 92.f * U, FLinearColor(1.f, 1.f, 1.f, 0.9f * A), 0.6f, true);
	}

	DrawStages(G);
	DrawResolve(G);
	if (G->bComboMode) DrawCombo(G);
	// Awaken panel / aim bar: UMG when it's up (AZoogiHUD::UpdateAwakenWidget), Canvas otherwise (-ZoogiCanvasAwaken).
	const bool bUmgAwaken = AwakenWidget && AwakenWidget->ShowsAwaken();
	const bool bUmgAim = AwakenWidget && AwakenWidget->ShowsAim();
	if (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && G->CardsOn()) { if (!(AwakenWidget && AwakenWidget->ShowsCards()) && !UZoogiCardHandWidget::IsEnabled(G)) DrawCards(G); } // UMG card hand replaces the Canvas panel
	else if (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && !bUmgAwaken) DrawAwaken(G);
	if (G->IsHumanAiming() && G->IsAimStep() && !bUmgAim) DrawAimControls(G);
	DrawEpisodeP5(G);
	if (bFramed) {}
	else if (Letterbox < 0.3f && G->bComboMode) Text(TEXT("2v2 COMBO   Awaken: card + I / G / S   Battle: Q switch Zoogi, click target / floor, A/D cut, 1 CONTROL 2 DRIVE 3 LAUNCH, SPACE confirm   Follow-Up: F accept / X skip   C camera"), 24.f * U, H - 36.f * U, FLinearColor(0.7f, 0.88f, 1.f, 0.9f), 0.36f, false, false);
	else if (Letterbox < 0.3f && G->CardsOn()) Text(TEXT("Standby: click cards (1-5) + COMMIT (Enter), Backspace clears   Aim: click target or floor, A/D cut, Up/Down force CONTROL/DRIVE/LAUNCH, SPACE confirm   C camera"), 24.f * U, H - 36.f * U, FLinearColor(0.7f, 0.88f, 1.f, 0.9f), 0.36f, false, false);
	else if (Letterbox < 0.3f) Text(TEXT("Awaken: click a card + INFUSE / GUARD / SAVE (I/G/S)   Aim: click target or floor, A/D cut, Up/Down power, SPACE confirm   RMB/Esc cancel   C camera   Ctrl+R restart"), 24.f * U, H - 36.f * U, FLinearColor(0.7f, 0.88f, 1.f, 0.9f), 0.36f, false, false);
	if (!bFramed) Button(TEXT("MENU"), W - 150.f * U, 24.f * U, 126.f * U, 46.f * U, "Menu", 0, GOrbBlue);
	if (!bFramed) Button(G->bCinematicCam ? TEXT("CAM: CINE") : TEXT("CAM: TACT"), W - 340.f * U, 24.f * U, 176.f * U, 46.f * U, "CamStyle", 0, FLinearColor(0.6f, 0.9f, 0.6f), G->bCinematicCam);
	DrawAnime(G);
	// Climax impact frame (drawn even with Anime=Off): 1-2 frames of white on the hit, fading over ~0.1 s.
	{
		const float Since = G->GetClock() - G->ClimaxFlashAt;
		if (Since >= 0.f && Since < 0.11f) Panel(0, 0, Canvas->ClipX, Canvas->ClipY, FLinearColor(1.f, 1.f, 1.f, 0.85f * (1.f - Since / 0.11f)));
	}
}

void AZoogiHUD::DrawAim(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !G->bAimActive) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	float Frac = 0.f;
	const FVector2D V = AZoogiGameMode::PullToVelocity(G->GetEffectivePull(), Frac);
	const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
	const FVector Start = B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f);

	// Aim line from the shooter to the locked target.
	if (AActor* TL = (G->TargetLock.IsValid() && !G->bSoftLock) ? G->TargetLock.Get() : nullptr)
	{
		const FVector TP = Cast<AZoogiOrb>(TL) ? Cast<AZoogiOrb>(TL)->OrbLocation() : (Cast<AZoogiBall>(TL) ? Cast<AZoogiBall>(TL)->BallLocation() : TL->GetActorLocation());
		const FVector S0 = Project(B->BallLocation(), true), S1 = Project(TP, true);
		// Battle aim: the trajectory line (DrawPreview) already shows the shot, so skip the plain lock line under it.
		const bool bTrajShown = G->Stage == EZoogiStage::Battle && G->bAimActive && G->Preview.bValid;
		if (S0.Z > 0.f && S1.Z > 0.f && !bTrajShown)
		{
			const bool bBattle = G->Stage == EZoogiStage::Battle;
			const float Pulse = 0.65f + 0.35f * FMath::Sin(Time * (bBattle ? 18.f : 8.f));
			const FLinearColor LC = bBattle ? FLinearColor(1.f, 0.35f, 0.2f, 0.9f * Pulse) : FLinearColor(PC.R, PC.G, PC.B, 0.8f * Pulse);
			DrawLine(S0.X, S0.Y, S1.X, S1.Y, FLinearColor(0.f, 0.f, 0.f, 0.35f), 7.f * U);
			DrawLine(S0.X, S0.Y, S1.X, S1.Y, LC, 3.5f * U);
		}
	}

	// Pinpoint lock-on: laser to the target and a reticle.
	if (AActor* LockT = G->GetLockTarget())
	{
		const FVector TL = LockT->GetActorLocation();
		const FVector S0 = Project(Start + FVector(0, 0, 30.f), true), S1 = Project(TL, true);
		if (S0.Z > 0.f && S1.Z > 0.f)
		{
			const float Pulse = 0.6f + 0.4f * FMath::Sin(Time * 12.f);
			DrawLine(S0.X, S0.Y, S1.X, S1.Y, FLinearColor(0.85f, 0.4f, 1.f, 0.9f * Pulse), 4.f * U);
			const float R = 34.f * U;
			for (int32 k = 0; k < 4; ++k)
			{
				const float A = Time * 2.f + k * UE_HALF_PI;
				const FVector2D D(FMath::Cos(A), FMath::Sin(A));
				DrawLine(S1.X + D.X * R, S1.Y + D.Y * R, S1.X + D.X * R * 0.45f, S1.Y + D.Y * R * 0.45f, FLinearColor(1.f, 0.5f, 1.f), 4.f * U);
			}
			Text(TEXT("LOCKED"), S1.X, S1.Y - 70.f * U, FLinearColor(0.95f, 0.6f, 1.f), 0.45f, true);
		}
	}

	if (V.IsNearlyZero()) return;
	// Predicted path: constant rolling deceleration -> stopping distance v^2 / 2a.
	const float Speed = V.Size();
	const float Decel = G->Arena ? G->Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	const float Stop = FMath::Min(Speed * Speed / (2.f * Decel), 2600.f);
	const FVector Dir(V.X / Speed, V.Y / Speed, 0.f);
	const int32 Dots = FMath::Clamp(FMath::RoundToInt(Stop / 55.f), 2, 48);
	for (int32 i = 1; i <= Dots; ++i)
	{
		const float D = Stop * i / Dots;
		const FVector P = Start + Dir * D;
		const bool bOff = G->Arena ? G->Arena->IsOffCourt(FVector2D(P.X, P.Y)) : FVector2D(P.X, P.Y).Size() > ZoogiConst::KnockoffRadius;
		const FVector S = Project(P, true);
		if (S.Z <= 0.f) continue;
		const float T = (float)i / Dots;
		const float Size = FMath::Lerp(13.f, 5.f, T) * U;
		FLinearColor C = bOff ? FLinearColor(1.f, 0.25f, 0.2f, 0.9f) : FMath::Lerp(PC, FLinearColor::White, T * 0.6f);
		C.A = FMath::Lerp(0.95f, 0.35f, T);
		DrawRect(C, S.X - Size * 0.5f, S.Y - Size * 0.5f, Size, Size);
	}

	// Power meter (hidden under the Battle letterbox; the target panel still shows shot % and hit speed).
	if (G->Stage == EZoogiStage::Battle) return;
	const float BW = 460.f * U, BH = 22.f * U;
	const float BX = (W - BW) * 0.5f, BY = H - 130.f * U;
	Panel(BX - 6.f * U, BY - 34.f * U, BW + 12.f * U, BH + 44.f * U, GPanel);
	const FLinearColor Fill = Frac < 0.5f ? FMath::Lerp(FLinearColor(0.3f, 0.9f, 0.4f), FLinearColor(1.f, 0.85f, 0.2f), Frac * 2.f)
		: FMath::Lerp(FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(1.f, 0.25f, 0.2f), (Frac - 0.5f) * 2.f);
	Panel(BX, BY, BW, BH, FLinearColor(0.1f, 0.12f, 0.2f, 0.9f));
	Panel(BX, BY, BW * Frac, BH, Fill);
	Panel(BX + BW * ZoogiConst::SlingshotPull, BY - 4.f * U, 3.f * U, BH + 8.f * U, FLinearColor::White);
	const FString Label = Frac > ZoogiConst::SlingshotPull ? FString::Printf(TEXT("SHOT POWER %d%% (MAX)   %.0f u/s"), FMath::RoundToInt(Frac * 100.f), Speed / 100.f)
		: FString::Printf(TEXT("SHOT POWER %d%%   %.0f u/s"), FMath::RoundToInt(Frac * 100.f), Speed / 100.f);
	Text(Label, W * 0.5f, BY - 30.f * U, FLinearColor::White, 0.45f, true);
}

void AZoogiHUD::DrawPower(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || G->Phase == EZoogiPhase::GameOver) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 420.f * U, PH = 128.f * U;
	const float X = W - PW - 24.f * U, Y = H - PH - 70.f * U;
	const bool bCan = G->CanUsePower();
	const bool bArmed = G->bPowerArmed;
	const FLinearColor Accent = bArmed ? FLinearColor(1.f, 0.8f, 0.2f) : (bCan ? GOrbBlue : ZoogiUIStyle::Muted);
	Panel(X, Y, PW, PH, GPanel);
	Panel(X, Y, 8.f * U, PH, Accent);
	Text(B->Def.Ability.ToUpper(), X + 24.f * U, Y + 10.f * U, Accent + FLinearColor(0.15f, 0.15f, 0.15f, 0.f), 0.6f);
	FString Charges;
	for (int32 c = 0; c < 3; ++c) Charges += c < B->Charges ? TEXT("*") : TEXT("-");
	Text(Charges, X + PW - 24.f * U - TextSize(Charges, 0.7f).X, Y + 6.f * U, FLinearColor(1.f, 0.8f, 0.3f), 0.7f);
	FString Hint;
	if (bArmed) Hint = TEXT("ARMED - fires with your shot");
	else if (G->bPowerUsed) Hint = TEXT("used this turn");
	else if (B->Charges <= 0) Hint = TEXT("no charges - knock off a gold star orb");
	else if (bCan) Hint = B->bCPU ? TEXT("CPU may use it") : TEXT("press E / Q or click");
	else Hint = ZoogiUtil::IsArmedPower(B->Def.PowerKind) ? TEXT("arm it while aiming") : TEXT("use it while rolling");
	Text(Hint, X + 24.f * U, Y + 50.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.4f, false, false);
	if (!B->bCPU && bCan)
	{
		Button(bArmed ? TEXT("DISARM") : TEXT("POWER"), X + 24.f * U, Y + PH - 46.f * U, 160.f * U, 38.f * U, "Power", 0, Accent, bArmed);
	}
	Text(B->Def.AbilityDesc, X + 200.f * U, Y + PH - 40.f * U, FLinearColor(0.65f, 0.85f, 1.f), 0.3f, false, false);
}

void AZoogiHUD::DrawFlyers(AZoogiGameMode* G)
{
	for (const FZoogiFlyer& F : G->Flyers)
	{
		if (F.Age > FlyTime) continue;
		const FVector S = Project(F.World, true);
		if (S.Z <= 0.f) continue;
		const int32 Row = G->Balls.IndexOfByPredicate([&F](const AZoogiBall* B) { return B && B->PlayerIndex == F.Player; });
		const FVector2D End(24.f * U + 340.f * U, 24.f * U + 8.f * U + 58.f * U * FMath::Max(Row, 0) + 24.f * U);
		auto PosAt = [&](float A)
		{
			const float T = FMath::Clamp(A / FlyTime, 0.f, 1.f);
			const float E = T * T * (3.f - 2.f * T);
			FVector2D P = FMath::Lerp(FVector2D(S.X, S.Y), End, E);
			P.Y -= FMath::Sin(PI * T) * 140.f * U;
			return P;
		};
		// Comet: a few fading ghosts behind the orb.
		for (int32 k = 4; k >= 0; --k)
		{
			const FVector2D P = PosAt(F.Age - k * 0.03f);
			const float R = (14.f - k * 2.2f) * U;
			FCanvasNGonItem Glow(P, FVector2D(R * 1.8f, R * 1.8f), 16, FLinearColor(F.Color.R, F.Color.G, F.Color.B, 0.18f * (1.f - k * 0.18f)));
			Glow.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Glow);
			FCanvasNGonItem Core(P, FVector2D(R, R), 16, FLinearColor(FMath::Min(1.f, F.Color.R + 0.4f), FMath::Min(1.f, F.Color.G + 0.4f), FMath::Min(1.f, F.Color.B + 0.4f), 0.9f * (1.f - k * 0.2f)));
			Core.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Core);
		}
		const FVector2D P = PosAt(F.Age);
		Text(FString::Printf(TEXT("+%d"), F.Points), P.X, P.Y - 44.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f), 0.5f, true);
	}
}

void AZoogiHUD::DrawPopups(AZoogiGameMode* G)
{
	// Screen-space de-overlap: popups spawned at the same spot (e.g. KNOCKOUT + FELL OFF) stack upward instead of overprinting.
	TArray<FBox2D> Placed;
	for (int32 i = G->Popups.Num() - 1; i >= 0; --i)
	{
		const FZoogiPopup& P = G->Popups[i];
		const FVector S = Project(P.World + FVector(0, 0, P.Age * 80.f), true);
		if (S.Z <= 0.f) continue;
		FLinearColor C = P.Color + FLinearColor(0.15f, 0.15f, 0.15f, 0.f);
		C.A = FMath::Clamp(2.6f - P.Age, 0.f, 1.f);
		const float Pop = 1.f + 0.4f * FMath::Max(0.f, 1.f - P.Age * 4.f);
		const FVector2D TS = TextSize(P.Text, 0.8f * Pop, false);
		float Y = S.Y;
		for (int32 Guard = 0; Guard < 6; ++Guard)
		{
			const FBox2D R(FVector2D(S.X - TS.X * 0.5f, Y), FVector2D(S.X + TS.X * 0.5f, Y + TS.Y));
			const FBox2D* Hit = Placed.FindByPredicate([&R](const FBox2D& O) { return O.Intersect(R); });
			if (!Hit) break;
			Y = Hit->Min.Y - TS.Y - 2.f * U;
		}
		Placed.Add(FBox2D(FVector2D(S.X - TS.X * 0.5f, Y), FVector2D(S.X + TS.X * 0.5f, Y + TS.Y)));
		Text(P.Text, S.X, Y, C, 0.8f * Pop, true);
	}
}

static UTexture2D* PortraitTex(FName Id)
{
	static TMap<FName, TWeakObjectPtr<UTexture2D>> Cache;
	if (TWeakObjectPtr<UTexture2D>* C = Cache.Find(Id)) if (C->IsValid()) return C->Get();
	const FString Path = FString::Printf(TEXT("/Game/UI/Portraits/T_Portrait_%s.T_Portrait_%s"), *Id.ToString(), *Id.ToString());
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path);
	Cache.Add(Id, T);
	return T;
}

void AZoogiHUD::DrawResults(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	// Dim the match behind the results so the panel reads cleanly.
	Panel(0.f, 0.f, W, H, FLinearColor(0.f, 0.01f, 0.04f, 0.5f));
	const float PW = 780.f * U;
	const float X0 = (W - PW) * 0.5f;
	const float RowH = 84.f * U, RowGap = 10.f * U;
	float Y = 170.f * U;
	const float PH = 150.f * U + (RowH + RowGap) * G->Balls.Num() + 120.f * U;
	const FLinearColor WC = G->Balls.IsValidIndex(G->WinnerIndex) ? ZoogiUtil::PlayerColor(G->WinnerIndex) : FLinearColor(0.85f, 0.9f, 1.f);
	Panel(X0, Y, PW, PH, FLinearColor(0.02f, 0.03f, 0.08f, 0.92f));
	Panel(X0, Y, PW, 6.f * U, WC);
	Text(TEXT("MATCH OVER"), W * 0.5f, Y + 22.f * U, FLinearColor(0.7f, 0.78f, 0.95f), 0.45f, true, false);
	Text(G->Banner, W * 0.5f, Y + 56.f * U, WC + FLinearColor(0.2f, 0.2f, 0.2f, 0.f), 1.3f, true);
	Y += 150.f * U;
	TArray<AZoogiBall*> Sorted;
	for (AZoogiBall* B : G->Balls) Sorted.Add(B);
	if (G->bComboMode)
	{
		const int32 WT = G->ComboWinner;
		Sorted.Sort([WT](const AZoogiBall& A, const AZoogiBall& B) { const bool WA = A.TeamIndex == WT, WB = B.TeamIndex == WT; return WA != WB ? WA : (A.TeamIndex != B.TeamIndex ? A.TeamIndex < B.TeamIndex : A.Score > B.Score); });
		Text(FString::Printf(TEXT("%s     TEAM A %d  -  %d TEAM B"), *G->ComboOverReason, G->TeamScore(0), G->TeamScore(1)), W * 0.5f, Y - 40.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.42f, true, false);
	}
	else Sorted.Sort([](const AZoogiBall& A, const AZoogiBall& B) { return A.Score > B.Score; });
	const int32 Top = Sorted.Num() ? Sorted[0]->Score : 0;
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		AZoogiBall* B = Sorted[i];
		const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
		const bool bWin = G->bComboMode ? B->TeamIndex == G->ComboWinner : (B->Score == Top && G->Balls.IsValidIndex(G->WinnerIndex) && B->PlayerIndex == G->WinnerIndex);
		const float RX = X0 + 30.f * U, RW = PW - 60.f * U;
		Panel(RX, Y, RW, RowH, bWin ? FLinearColor(PC.R * 0.25f, PC.G * 0.25f, PC.B * 0.25f, 0.95f) : FLinearColor(0.05f, 0.07f, 0.14f, 0.92f));
		Panel(RX, Y, 8.f * U, RowH, PC);
		// Place and portrait.
		Text(FString::Printf(TEXT("%d"), i + 1), RX + 34.f * U, Y + RowH * 0.5f - 22.f * U, bWin ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.75f, 0.85f), 0.8f, true);
		const float PS = RowH - 14.f * U;
		const float PX = RX + 64.f * U, PY = Y + 7.f * U;
		Panel(PX - 3.f * U, PY - 3.f * U, PS + 6.f * U, PS + 6.f * U, PC);
		if (UTexture2D* T = PortraitTex(B->Def.Id)) DrawTexture(T, PX, PY, PS, PS, 0.f, 0.f, 1.f, 1.f);
		else Panel(PX, PY, PS, PS, B->Def.Color);
		const float TX = PX + PS + 20.f * U;
		Text(FString::Printf(TEXT("P%d  %s"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), TX, Y + 12.f * U, FLinearColor::White, 0.62f);
		Text(G->bComboMode ? FString::Printf(TEXT("%s   %d orbs   %d ring-outs   %s%s"), *AZoogiGameMode::TeamName(B->TeamIndex), B->OrbsScored, B->KnockOuts, B->bKnockedOut ? TEXT("KNOCKED OUT") : *FString::Printf(TEXT("%d ring mark%s"), B->RingMarks, B->RingMarks == 1 ? TEXT("") : TEXT("s")), B->bCPU ? TEXT("   CPU") : TEXT(""))
			: FString::Printf(TEXT("%d orbs   %d KOs%s"), B->OrbsScored, B->KnockOuts, B->bCPU ? TEXT("   CPU") : TEXT("")), TX, Y + 50.f * U, FLinearColor(0.72f, 0.78f, 0.9f), 0.42f, false, false);
		if (bWin)
		{
			// Gold WINNER badge, pulsing gently.
			const FString Badge = TEXT("WINNER");
			const float Pulse = 1.f + 0.06f * FMath::Sin(Time * 5.f);
			const FVector2D BS = TextSize(Badge, 0.7f * Pulse, true);
			const float BX = RX + RW - 150.f * U - BS.X, BY = Y + (RowH - BS.Y) * 0.5f - 4.f * U;
			Panel(BX - 16.f * U, BY - 4.f * U, BS.X + 32.f * U, BS.Y + 8.f * U, FLinearColor(0.45f, 0.3f, 0.02f, 0.95f));
			Panel(BX - 13.f * U, BY - 1.f * U, BS.X + 26.f * U, BS.Y + 2.f * U, FLinearColor(1.f, 0.78f, 0.2f, 1.f));
			Text(Badge, BX, BY, FLinearColor(0.12f, 0.07f, 0.f), 0.7f * Pulse);
		}
		const FString Sc = FString::FromInt(B->Score);
		Text(Sc, RX + RW - 30.f * U - TextSize(Sc, 0.9f).X, Y + RowH * 0.5f - 26.f * U, PC + FLinearColor(0.15f, 0.15f, 0.15f, 0.f), 0.9f);
		Y += RowH + RowGap;
	}
	Y += 20.f * U;
	Button(TEXT("REMATCH"), W * 0.5f - 300.f * U, Y, 280.f * U, 70.f * U, "Rematch", 0, FLinearColor(1.f, 0.75f, 0.2f), true);
	Button(TEXT("MENU"), W * 0.5f + 20.f * U, Y, 280.f * U, 70.f * U, "Menu", 0, GOrbBlue);
}

void AZoogiHUD::DrawStages(AZoogiGameMode* G)
{
	if (G->Phase != EZoogiPhase::Aiming && G->Phase != EZoogiPhase::Rolling && G->Phase != EZoogiPhase::Resolving) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	AZoogiBall* B = G->GetActiveBall();
	const FLinearColor PC = B ? ZoogiUtil::PlayerColor(B->PlayerIndex) : GOrbBlue;

	// Phase tracker: DRAW > AWAKEN > BATTLE > RESOLVE > END.
	const int32 N = 5;
	const float BW = 128.f * U, BH = 30.f * U, Gap = 8.f * U;
	const float X0 = W * 0.5f - (N * BW + (N - 1) * Gap) * 0.5f, Y = 74.f * U;
	const int32 Cur = (int32)G->Stage;
	for (int32 i = 0; i < N && !ZoogiFramed::IsActive(GetWorld()); ++i) // framed: the stepper is in the header
	{
		const float X = X0 + i * (BW + Gap);
		const bool bNow = i == Cur, bDone = i < Cur;
		// uistyle: saturated navy chips with a navy outline (was dim gray-black 0.03/0.04/0.1)
		const FLinearColor Bg = bNow ? FLinearColor(PC.R * 0.8f, PC.G * 0.8f, PC.B * 0.8f, 0.95f) : FLinearColor(0.04f, 0.14f, 0.46f, bDone ? 0.88f : 0.7f);
		ZoogiUIStyle::CRoundRect(Canvas, X, Y, BW, BH, 9.f * U, ZoogiUIStyle::Outline);
		ZoogiUIStyle::CRoundRect(Canvas, X + 3.f * U, Y + 3.f * U, BW - 6.f * U, BH - 6.f * U, 6.f * U, Bg);
		if (bNow) Panel(X + 9.f * U, Y + BH - 6.f * U, BW - 18.f * U, 3.f * U, FLinearColor::White);
		FString L = AZoogiGameMode::StageName((EZoogiStage)i);
		if (bNow && G->Stage == EZoogiStage::Battle) L = FString::Printf(TEXT("%s: %s"), *L, AZoogiGameMode::StepName(G->BattleStep));
		Text(L, X + BW * 0.5f, Y + 4.f * U, bNow ? FLinearColor::White : FLinearColor(0.7f, 0.88f, 1.f, bDone ? 0.95f : 0.75f), 0.38f, true, false);
	}

	// Big phase banner on every stage change.
	const float A = FMath::Clamp(1.f - (G->StageTime - 0.55f) / 0.45f, 0.f, 1.f) * FMath::Clamp(G->StageTime / 0.12f, 0.f, 1.f);
	if (A > 0.f && G->Phase != EZoogiPhase::GameOver)
	{
		static const TCHAR* Subs[] = { TEXT("an artifact is drawn"), TEXT("infuse - guard - save"), TEXT("aim - confirm - power up - launch"), TEXT("results are final"), TEXT("next player") };
		const float Pop = 1.f + 0.25f * FMath::Clamp(1.f - G->StageTime / 0.15f, 0.f, 1.f);
		const FString T = FString::Printf(TEXT("%s PHASE"), AZoogiGameMode::StageName(G->Stage));
		Text(T, W * 0.5f, H * 0.24f, FLinearColor(1.f, 0.95f, 0.85f, A), 1.5f * Pop, true);
		Text(Subs[FMath::Clamp(Cur, 0, 4)], W * 0.5f, H * 0.24f + 92.f * U, FLinearColor(0.85f, 0.9f, 1.f, A * 0.9f), 0.5f, true, false);
	}

	// Target panel (locked target): name, distance, estimated hit power.
	AActor* T = G->TargetLock.Get();
	if (T && B && G->Phase == EZoogiPhase::Aiming && !ZoogiFramed::IsActive(GetWorld())) // hud-v3: framed shows the target in the action box
	{
		const FVector TP = Cast<AZoogiOrb>(T) ? Cast<AZoogiOrb>(T)->OrbLocation() : (Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation());
		const float Dist = FVector::Dist2D(TP, B->BallLocation());
		float Frac = 0.f, Impact = 0.f;
		if (B->bCPU) G->LockShotVelocity(Frac, Impact); else G->ComputeShotVelocity(Frac, Impact);
		const float PW = 470.f * U, PH = 104.f * U;
		const float PX = W - PW - 24.f * U, PY = 90.f * U;
		const bool bBattle = G->IsPowerUpStep();
		const FLinearColor Acc = bBattle ? FLinearColor(1.f, 0.35f, 0.2f) : (G->bSoftLock ? FLinearColor(0.7f, 0.8f, 0.95f) : PC);
		Panel(PX, PY, PW, PH, GPanel);
		Panel(PX, PY, 8.f * U, PH, Acc);
		Text(G->bSoftLock ? TEXT("TARGET (soft)") : (bBattle ? TEXT("POWER UP - LAUNCHING") : TEXT("TARGET LOCKED")), PX + 24.f * U, PY + 8.f * U, Acc, 0.36f, false, false);
		Text(G->TargetName(T), PX + 24.f * U, PY + 30.f * U, FLinearColor::White, 0.62f);
		Text(FString::Printf(TEXT("%.1f m    shot %d%%    hit %.1f m/s%s"), Dist / 100.f, FMath::RoundToInt(Frac * 100.f), Impact / 100.f,
			G->Preview.bContact ? *FString::Printf(TEXT("    cut %.0f deg"), G->Preview.CutDeg) : TEXT("")), PX + 24.f * U, PY + 72.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.4f, false, false);
	}
	// Hover tooltip.
	if (const AZoogiPlayerController* ZPC = Cast<AZoogiPlayerController>(PlayerOwner))
	{
		if (AActor* Hv = ZPC->GetHoverTarget())
		{
			if (Hv != T)
			{
				const FVector2D M = ZPC->GetCursor() - FrameOrg;
				Text(G->TargetName(Hv), M.X + 22.f * U, M.Y - 34.f * U, FLinearColor(0.9f, 0.97f, 1.f), 0.42f, false, false);
			}
		}
	}
}

// ============================================================================
// Milestone "Awaken" HUD (Canvas): Awaken cards + buttons, aim controls, shot preview, artifact panel, Resolve, anime overlay.
// ============================================================================

void AZoogiHUD::Gem(uint8 ArtifactId, float CX, float CY, float R, float Alpha)
{
	const EZoogiArtifact A = (EZoogiArtifact)ArtifactId;
	if (A == EZoogiArtifact::None) return;
	const FZoogiArtifactRow& Row = ZoogiArtifacts::Get(A);
	int32 Sides = 6;
	switch (A)
	{
	case EZoogiArtifact::Ember: Sides = 3; break;  // flame shard
	case EZoogiArtifact::Storm: Sides = 4; break;  // diamond eye
	case EZoogiArtifact::Frost: Sides = 6; break;  // hex crystal
	case EZoogiArtifact::Titan: Sides = 8; break;  // stone core
	case EZoogiArtifact::Void: Sides = 24; break;  // dark orb with a ring
	default: break;
	}
	auto NGon = [this](const FVector2D& P, float Rad, int32 N, const FLinearColor& C)
	{
		FCanvasNGonItem It(P, FVector2D(Rad, Rad), N, C);
		It.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(It);
	};
	const FVector2D P(CX, CY);
	FLinearColor Glow = Row.RimColor; Glow.A = 0.25f * Alpha;
	NGon(P, R * 1.55f, 24, Glow);
	FLinearColor Rim = Row.RimColor; Rim.A = Alpha;
	NGon(P, R * 1.08f, Sides, Rim);
	FLinearColor Core = Row.CoreColor; Core.A = Alpha;
	NGon(P, R * 0.86f, Sides, Core);
	if (A == EZoogiArtifact::Void)
	{
		for (int32 k = 0; k < 24; ++k)
		{
			const float A0 = k * UE_TWO_PI / 24.f + Time, A1 = (k + 1) * UE_TWO_PI / 24.f + Time;
			DrawLine(CX + FMath::Cos(A0) * R * 1.35f, CY + FMath::Sin(A0) * R * 0.5f, CX + FMath::Cos(A1) * R * 1.35f, CY + FMath::Sin(A1) * R * 0.5f, FLinearColor(0.75f, 0.5f, 1.f, Alpha), 2.f * U);
		}
	}
	NGon(P + FVector2D(-R * 0.28f, -R * 0.3f), R * 0.22f, 12, FLinearColor(1.f, 1.f, 1.f, 0.55f * Alpha)); // highlight
}

void AZoogiHUD::WorldCircle(const FVector& C, float Radius, const FLinearColor& Col, float Thick)
{
	FVector Prev = Project(C + FVector(Radius, 0, 0), true);
	for (int32 k = 1; k <= 28; ++k)
	{
		const float A = k * UE_TWO_PI / 28.f;
		const FVector S = Project(C + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 0), true);
		if (S.Z > 0.f && Prev.Z > 0.f) DrawLine(Prev.X, Prev.Y, S.X, S.Y, Col, Thick);
		Prev = S;
	}
}

// Trajectory palette (Evan Oct 7): the shot / target line glows in the current Awaken colour, warm gold when none.
// Fill is the solid line colour, Glow the soft halo around it.
static void AwakenLineColors(EZoogiArtifact Inf, FLinearColor& Fill, FLinearColor& Glow)
{
	switch (Inf)
	{
	case EZoogiArtifact::Ember: Fill = FLinearColor(1.f, 0.42f, 0.12f); Glow = FLinearColor(1.f, 0.18f, 0.04f); break;
	case EZoogiArtifact::Storm: Fill = FLinearColor(1.f, 0.98f, 0.35f); Glow = FLinearColor(0.15f, 0.45f, 1.f); break; // lemon core, electric-blue rim
	case EZoogiArtifact::Frost: Fill = FLinearColor(0.88f, 1.f, 1.f); Glow = FLinearColor(0.4f, 0.95f, 1.f); break;
	case EZoogiArtifact::Titan: Fill = FLinearColor(1.f, 0.6f, 0.06f); Glow = FLinearColor(0.85f, 0.38f, 0.0f); break; // amber
	case EZoogiArtifact::Void: Fill = FLinearColor(0.78f, 0.4f, 1.f); Glow = FLinearColor(0.55f, 0.12f, 1.f); break;
	default: Fill = FLinearColor(1.f, 0.9f, 0.62f); Glow = FLinearColor(1.f, 0.72f, 0.25f); break;
	}
}
// The player's own Zoogi after launch / contact: always blue.
static const FLinearColor ZoogiBlueFill(0.25f, 0.6f, 1.f), ZoogiBlueGlow(0.08f, 0.3f, 1.f);

void AZoogiHUD::DrawDragPower(AZoogiGameMode* G)
{
	const AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(PlayerOwner);
	AZoogiBall* Me = G ? G->GetActiveBall() : nullptr;
	if (!PC || !Me || !Canvas || !PC->bDragArmed || !G->IsAimStep()) return;
	float Fr = 0.f, Imp = 0.f;
	const FVector2D Dir = G->ComputeShotVelocity(Fr, Imp).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;
	const float F = PC->bDragWouldCancel ? 0.f : PC->DragFrac;
	const FLinearColor Gold(1.f, 0.855f, 0.553f, 1.f), GoldGlow(1.f, 0.95f, 0.75f, 1.f), Dim(0.6f, 0.6f, 0.65f, 0.8f);
	const FVector From = Me->BallLocation() + FVector(0, 0, 10.f);
	const FVector To = From + FVector(Dir * (ZoogiConst::ZoogiRadius * 1.3f + 160.f + 900.f * F), 0.f);
	const FVector Pts[2] = { From, To };
	TrajectoryLine(Pts, 2, PC->bDragWouldCancel ? Dim : Gold, PC->bDragWouldCancel ? Dim : GoldGlow, 0.9f + 1.1f * F, 1.f, true);
	// power ring around the Zoogi (fills clockwise with the pull) + readout
	const FVector C3 = Project(Me->BallLocation(), true);
	if (C3.Z <= 0.f) return;
	const FVector2D C(C3.X, C3.Y);
	const float R = 58.f * U;
	const int32 Seg = 48;
	for (int32 i = 0; i < Seg; ++i)
	{
		const float A0 = -UE_HALF_PI + UE_TWO_PI * i / Seg, A1 = -UE_HALF_PI + UE_TWO_PI * (i + 1) / Seg;
		const bool bOn = (float)(i + 1) / Seg <= F + 0.001f;
		Canvas->K2_DrawLine(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, (bOn ? 6.f : 2.f) * U, bOn ? Gold : FLinearColor(0.1f, 0.1f, 0.14f, 0.7f));
	}
	const FString Tier = G->bComboMode ? FString(AZoogiGameMode::ForceName(G->ComboForce)) : FString();
	const FString Label = PC->bDragWouldCancel ? FString(TEXT("RELEASE = CANCEL")) : FString::Printf(TEXT("%d%%%s%s"), FMath::RoundToInt(F * 100.f), Tier.IsEmpty() ? TEXT("") : TEXT("  "), *Tier);
	Text(Label, C.X, C.Y - R - 34.f * U, PC->bDragWouldCancel ? Dim : Gold, 0.55f, true);
}

void AZoogiHUD::WorldArrow(const FVector& From, const FVector& To, const FLinearColor& Col, float Thick)
{
	// Every aim / direction arrow is a glowing trajectory line with a static head (ui-polish).
	FLinearColor Fill, Glow;
	AwakenLineColors(EZoogiArtifact::None, Fill, Glow);
	const FVector Pts[2] = { From, To };
	TrajectoryLine(Pts, 2, Fill, Glow, FMath::Clamp(Thick / (4.f * U), 0.6f, 1.4f), Col.A, true);
}

void AZoogiHUD::TrajectoryLine(const FVector* Pts, int32 Num, const FLinearColor& Fill, const FLinearColor& Glow, float Scale, float Alpha, bool bHead, const TArray<bool>* Off)
{
	// Solid glowing trajectory line (ui-polish, Evan Oct 7): dark outline, soft static halo, solid fill, bright core,
	// optional static arrowhead at the end. Nothing moves along it; only a very gentle in-place brightness breath.
	if (!Canvas || Num < 2) return;
	TArray<FVector2D> S;
	TArray<bool> SOff;
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector V = Project(Pts[i], true);
		if (V.Z <= 0.f) continue;
		const FVector2D Q(V.X, V.Y);
		if (S.Num() && FVector2D::Distance(S.Last(), Q) < 1.5f) continue;
		S.Add(Q);
		SOff.Add(Off && Off->IsValidIndex(i - 1) && (*Off)[i - 1]);
	}
	if (S.Num() < 2) return;
	const float K = U * Scale;
	const float Breath = 0.9f + 0.1f * FMath::Sin(Time * (UE_TWO_PI / 1.6f));
	const FLinearColor Dark = FMath::Lerp(FLinearColor(0.02f, 0.03f, 0.06f, 1.f), Glow, Glow.B > Fill.B + 0.4f ? 0.55f : 0.2f), OffC(1.f, 0.22f, 0.18f, 1.f);
	FVector2D HeadDir = (S.Last() - S[S.Num() - 2]).GetSafeNormal();
	const float HeadL = 16.f * K, HeadW = 9.f * K;
	FVector2D LineEnd = S.Last();
	if (bHead && FVector2D::Distance(S.Last(), S[S.Num() - 2]) > HeadL * 1.2f) S.Last() = LineEnd - HeadDir * HeadL * 0.7f;
	else if (bHead) bHead = false;

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(512);
	auto Tri = [&Tris](const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2)
	{
		FCanvasUVTri T;
		T.V0_Pos = P0; T.V1_Pos = P1; T.V2_Pos = P2;
		T.V0_UV = T.V1_UV = T.V2_UV = FVector2D(0.5f, 0.5f);
		T.V0_Color = C0; T.V1_Color = C1; T.V2_Color = C2;
		Tris.Add(T);
	};
	auto Flush = [this, &Tris](ESimpleElementBlendMode Mode)
	{
		if (!Tris.Num()) return;
		FCanvasTriangleItem Item(Tris, Canvas->DefaultTexture ? Canvas->DefaultTexture->GetResource() : nullptr);
		Item.BlendMode = Mode;
		Canvas->DrawItem(Item);
		Tris.Reset();
	};
	// Canvas additive ignores alpha: premultiply for the additive halo.
	auto Col = [Alpha](const FLinearColor& B, float Al, bool bAdd)
	{
		const float A2 = FMath::Clamp(Al, 0.f, 1.f) * Alpha;
		return bAdd ? FLinearColor(B.R * A2, B.G * A2, B.B * A2, A2) : FLinearColor(B.R, B.G, B.B, A2);
	};
	// One pass: every segment as a quad with soft (Ce) or hard edges, round joints so corners / banks stay solid.
	auto Pass = [&](float HalfW, const FLinearColor& Cc, const FLinearColor& Ce, bool bOffAware)
	{
		for (int32 i = 0; i + 1 < S.Num(); ++i)
		{
			const FVector2D A = S[i], Bp = S[i + 1];
			const FVector2D D = (Bp - A).GetSafeNormal(), N(-D.Y, D.X);
			FLinearColor c = Cc, e = Ce;
			if (bOffAware && SOff[i + 1]) { c = FLinearColor(OffC.R, OffC.G, OffC.B, Cc.A); e = FLinearColor(OffC.R, OffC.G, OffC.B, Ce.A); }
			Tri(A, A + N * HalfW, Bp + N * HalfW, c, e, e); Tri(A, Bp + N * HalfW, Bp, c, e, c);
			Tri(A, Bp, Bp - N * HalfW, c, c, e); Tri(A, Bp - N * HalfW, A - N * HalfW, c, e, e);
		}
		for (int32 i = 0; i < S.Num(); ++i)
		{
			const int32 Seg = 14;
			for (int32 k = 0; k < Seg; ++k)
			{
				const float a0 = k * UE_TWO_PI / Seg, a1 = (k + 1) * UE_TWO_PI / Seg;
				Tri(S[i], S[i] + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * HalfW, S[i] + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * HalfW, Cc, Ce, Ce);
			}
		}
	};
	auto Head = [&](float Grow, const FLinearColor& C)
	{
		if (!bHead) return;
		const FVector2D N(-HeadDir.Y, HeadDir.X), Base = LineEnd - HeadDir * HeadL;
		Tri(LineEnd + HeadDir * Grow * 1.6f, Base + N * (HeadW + Grow) - HeadDir * Grow, Base - N * (HeadW + Grow) - HeadDir * Grow, C, C, C);
	};
	// 1) Soft halo (additive).
	Pass((7.f + 1.f * Breath) * K, Col(Glow, 0.5f * Breath, true), Col(Glow, 0.f, true), false);
	Head(3.5f * K, Col(Glow, 0.35f * Breath, true));
	Flush(SE_BLEND_Additive);
	// 2) Dark outline, so the line reads on snow and sand.
	Pass(3.6f * K, Col(Dark, 0.9f, false), Col(Dark, 0.8f, false), false);
	Head(1.6f * K, Col(Dark, 0.9f, false));
	// 3) Solid fill + bright core.
	Pass(2.3f * K, Col(Fill, 1.f, false), Col(FMath::Lerp(Fill, Glow, 0.45f), 1.f, false), true);
	Head(0.f, Col(Fill, 1.f, false));
	Pass(0.8f * K, Col(FMath::Lerp(Fill, FLinearColor::White, 0.6f), 0.95f * Breath, false), Col(Fill, 0.9f, false), true);
	Flush(SE_BLEND_Translucent);
}

void AZoogiHUD::DrawPreview(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B) return;
	const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
	const FVector Ground(0, 0, -(ZoogiConst::ZoogiRadius - 4.f));
	// Free-aim marker.
	if (G->bFreeAim && G->IsAimStep())
	{
		const float Pulse = 0.6f + 0.4f * FMath::Sin(Time * 6.f);
		WorldCircle(G->FreeAimPoint + FVector(0, 0, 3.f), 60.f, FLinearColor(PC.R, PC.G, PC.B, 0.9f * Pulse), 3.f * U);
		WorldCircle(G->FreeAimPoint + FVector(0, 0, 3.f), 18.f, FLinearColor(1, 1, 1, 0.9f), 2.f * U);
	}
	const FZoogiAimPreview& P = G->Preview;
	if (!P.bValid || !G->bAimActive) return;
	const bool bPower = G->IsPowerUpStep();
	const EZoogiArtifact Inf = B->Artifact ? B->Artifact->Infused : EZoogiArtifact::None;
	FLinearColor ShotFill, ShotGlow;
	AwakenLineColors(Inf, ShotFill, ShotGlow);
	// Predicted shot path (bank included) as the original dotted squares (Evan Oct 7: "what we initially had"), same
	// size / spacing / fade as before the UI work, tinted in the Awaken colour (gold when none). Static; off court red.
	// A slightly larger faint dark square under each dot keeps them readable on snow.
	for (int32 i = 0; i < P.Path.Num(); ++i)
	{
		const FVector S = Project(P.Path[i], true);
		if (S.Z <= 0.f) continue;
		const float T = (float)(i + 1) / P.Path.Num();
		const float Size = FMath::Lerp(12.f, 6.f, T) * U * (bPower ? 1.2f : 1.f);
		const bool bOffDot = P.PathOff.IsValidIndex(i) && P.PathOff[i];
		FLinearColor C = bOffDot ? FLinearColor(1.f, 0.25f, 0.2f, 0.9f) : FMath::Lerp(ShotFill, FLinearColor::White, T * 0.25f);
		C.A = FMath::Lerp(0.95f, 0.45f, T);
		const float E = 1.5f * U;
		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, C.A * 0.55f), S.X - Size * 0.5f - E, S.Y - Size * 0.5f - E, Size + 2.f * E, Size + 2.f * E);
		DrawRect(C, S.X - Size * 0.5f, S.Y - Size * 0.5f, Size, Size);
	}
	if (P.bBank)
	{
		const FVector S = Project(P.BankPoint + FVector(0, 0, 10.f), true);
		if (S.Z > 0.f)
		{
			const float R = 14.f * U;
			DrawLine(S.X - R, S.Y - R, S.X + R, S.Y + R, FLinearColor(1.f, 0.9f, 0.3f), 3.f * U);
			DrawLine(S.X - R, S.Y + R, S.X + R, S.Y - R, FLinearColor(1.f, 0.9f, 0.3f), 3.f * U);
			Text(TEXT("BANK"), S.X, S.Y - 46.f * U, FLinearColor(1.f, 0.9f, 0.4f), 0.4f, true);
		}
	}
	if (P.bContact)
	{
		// Ghost ball at the contact point, target departure (contact normal), shooter deflection.
		const FVector GC = P.Ghost + Ground + FVector(0, 0, 3.f);
		// Static contact ghost: dark rim + Awaken-coloured ring.
		WorldCircle(GC, ZoogiConst::ZoogiRadius, FLinearColor(0.02f, 0.03f, 0.06f, 0.8f), 4.f * U);
		WorldCircle(GC, ZoogiConst::ZoogiRadius, FLinearColor(ShotFill.R, ShotFill.G, ShotFill.B, 0.95f), 2.f * U);
		if (AActor* T = P.ContactActor.Get())
		{
			FVector TP = Cast<AZoogiOrb>(T) ? Cast<AZoogiOrb>(T)->OrbLocation() : (Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation());
			TP.Z = GC.Z;
			const float Len = FMath::Lerp(180.f, 520.f, FMath::Clamp(P.Fraction, 0.f, 1.f)) * FMath::Max(0.2f, FMath::Cos(FMath::DegreesToRadians(P.CutDeg)));
			// Target's after-contact direction: Awaken colour, slightly thinner / dimmer, static head.
			const FVector TPts[2] = { TP + P.TargetDir * (ZoogiConst::ZoogiRadius * 0.7f), TP + P.TargetDir * (Len + ZoogiConst::ZoogiRadius * 0.7f) };
			TrajectoryLine(TPts, 2, ShotFill, ShotGlow, 0.85f, 0.85f, true);
		}
		if (!P.ShooterDir.IsNearlyZero())
		{
			// The player's own Zoogi after contact: always a blue line.
			const FVector SPts[2] = { GC + P.ShooterDir * (ZoogiConst::ZoogiRadius * 0.7f), GC + P.ShooterDir * 320.f };
			TrajectoryLine(SPts, 2, ZoogiBlueFill, ZoogiBlueGlow, 0.9f, 1.f, true);
		}
		const FVector S = Project(GC, true);
		if (S.Z > 0.f) Text(FString::Printf(TEXT("cut %.0f deg"), P.CutDeg), S.X, S.Y + 26.f * U, FLinearColor(0.9f, 0.95f, 1.f), 0.38f, true, false);
	}
}

void AZoogiHUD::DrawAimControls(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 1180.f * U, PH = 118.f * U;
	const float X0 = (W - PW) * 0.5f, Y0 = H - PH - 96.f * U;
	Panel(X0, Y0, PW, PH, GPanel);
	const FLinearColor Acc(0.55f, 0.8f, 1.f);
	float Frac = 0.f, Impact = 0.f;
	if (G->HasAim()) G->ComputeShotVelocity(Frac, Impact);
	const FString Info = G->HasAim()
		? FString::Printf(TEXT("%s   cut %+.0f deg   power %d%%%s"), G->bFreeAim ? TEXT("FLOOR AIM") : *G->TargetName(G->TargetLock.Get()), G->AimCutDeg, FMath::RoundToInt(Frac * 100.f),
			G->Preview.bContact ? *FString::Printf(TEXT("   contact cut %.0f deg"), G->Preview.CutDeg) : TEXT(""))
		: FString(TEXT("Click a rival / orb to lock on, or the floor to aim at a point"));
	Text(Info, W * 0.5f, Y0 + 8.f * U, FLinearColor(0.9f, 0.95f, 1.f), 0.42f, true, false);
	float X = X0 + 16.f * U;
	const float BY = Y0 + 52.f * U, BH = 52.f * U;
	Button(TEXT("<<"), X, BY, 62.f * U, BH, "AimNudge", -5, Acc); X += 68.f * U;
	Button(TEXT("<"), X, BY, 56.f * U, BH, "AimNudge", -1, Acc); X += 62.f * U;
	Button(TEXT(">"), X, BY, 56.f * U, BH, "AimNudge", 1, Acc); X += 62.f * U;
	Button(TEXT(">>"), X, BY, 62.f * U, BH, "AimNudge", 5, Acc); X += 84.f * U;
	for (int32 i = 0; i < 4; ++i) { Button(ZoogiCards::ForceLabel(i, G->CardsOn()), X, BY, 96.f * U, BH, "AimPower", i, FLinearColor(1.f, 0.8f, 0.3f), G->AimPowerStep == i); X += 102.f * U; }
	X += 18.f * U;
	Button(TEXT("NEXT"), X, BY, 110.f * U, BH, "AimNext", 0, Acc); X += 118.f * U;
	Button(TEXT("CONFIRM"), X, BY, 170.f * U, BH, "AimConfirm", 0, FLinearColor(0.35f, 1.f, 0.45f), G->HasAim()); X += 178.f * U;
	Button(TEXT("CANCEL"), X, BY, 150.f * U, BH, "AimCancel", 0, FLinearColor(1.f, 0.4f, 0.35f));
}

void AZoogiHUD::DrawCards(AZoogiGameMode* G)
{
	UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(G);
	AZoogiBall* B = G->GetActiveBall();
	if (!Cards || !B) return;
	const FZoogiCardPlayerState* S = Cards->Rules().Player(B->PlayerIndex);
	if (!S) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float CW = 230.f * U, CH = 120.f * U, Gap = 14.f * U;
	const int32 N = S->Hand.Num();
	const float PanelW = FMath::Max(N, 1) * (CW + Gap) + 420.f * U;
	float X = (W - PanelW) * 0.5f;
	const float Y = H - CH - 120.f * U;
	Panel(X - 16.f * U, Y - 64.f * U, PanelW + 32.f * U, CH + 96.f * U, GPanel);
	int32 SelCost = 0;
	for (int32 Id : S->Selected) for (const FZoogiCardInstance& C : S->Hand) if (C.InstanceId == Id) if (const FZoogiCardDef* D = Cards->Def(C.CardId)) SelCost += D->Cost;
	Text(FString::Printf(TEXT("STANDBY  P%d %s   CARD ENERGY %d / %d   selected %d"), B->PlayerIndex + 1, *B->Def.Name, S->Energy - SelCost, S->Energy, SelCost), X, Y - 54.f * U, FLinearColor(0.75f, 0.9f, 1.f), 0.6f, false);
	const bool bHuman = !B->bCPU && !G->bAwakenDone;
	for (int32 i = 0; i < N; ++i)
	{
		const FZoogiCardDef* D = Cards->Def(S->Hand[i].CardId);
		const bool bSel = S->Selected.Contains(S->Hand[i].InstanceId);
		const FString L = D ? FString::Printf(TEXT("%d  %s  (%d)"), i + 1, *D->Name, D->Cost) : S->Hand[i].CardId.ToString();
		const FLinearColor Acc = !D ? ZoogiUIStyle::Muted : (D->Type == EZoogiCardType::Defense ? FLinearColor(0.3f, 0.9f, 1.f) : D->Type == EZoogiCardType::Impact ? FLinearColor(1.f, 0.45f, 0.3f)
			: D->Type == EZoogiCardType::Terrain ? FLinearColor(0.7f, 0.6f, 1.f) : FLinearColor(1.f, 0.85f, 0.35f));
		if (bHuman) Button(L, X, Y, CW, CH * 0.55f, "CardToggle", i, Acc, bSel);
		else { Panel(X, Y, CW, CH * 0.55f, bSel ? Acc * 0.6f : ZoogiUIStyle::ButtonNavy); Text(L, X + CW * 0.5f, Y + 18.f * U, FLinearColor::White, 0.55f, true); }
		if (D) Text(ZoogiCards::TypeName(D->Type), X + 8.f * U, Y + CH * 0.6f, FLinearColor(0.7f, 0.88f, 1.f), 0.42f, false, false);
		X += CW + Gap;
	}
	if (bHuman)
	{
		Button(TEXT("COMMIT"), X, Y, 190.f * U, CH * 0.55f, "CardCommit", 0, FLinearColor(0.35f, 1.f, 0.45f), true); X += 200.f * U;
		Button(TEXT("CLEAR"), X, Y, 150.f * U, CH * 0.55f, "CardClear", 0, FLinearColor(1.f, 0.4f, 0.35f));
	}
	else if (!G->AwakenChoiceText.IsEmpty()) Text(G->AwakenChoiceText, X, Y + 18.f * U, FLinearColor(0.6f, 1.f, 0.7f), 0.55f, false);
}

void AZoogiHUD::DrawAwaken(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !B->Artifact) return;
	UZoogiArtifactComponent* AC = B->Artifact;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
	if (G->bAwakenDone)
	{
		// Choice made: big confirmation line while the activation beat plays.
		const float A = FMath::Clamp((G->StageTime - G->AwakenDoneAt) * 6.f, 0.f, 1.f);
		const EZoogiArtifact Shown = AC->Infused != EZoogiArtifact::None ? AC->Infused : AC->Guard;
		const FLinearColor C = Shown != EZoogiArtifact::None && !G->AwakenChoiceText.StartsWith(TEXT("SAVE")) ? ZoogiArtifacts::Color(Shown) : FLinearColor(0.8f, 0.85f, 0.95f);
		Text(G->AwakenChoiceText, W * 0.5f, H * 0.70f, FLinearColor(FMath::Min(1.f, C.R + 0.3f), FMath::Min(1.f, C.G + 0.3f), FMath::Min(1.f, C.B + 0.3f), A), 0.95f, true);
		return;
	}
	// Cards: the drawn artifact (0) + saved hand slots (1..2).
	TArray<TPair<int32, EZoogiArtifact>> Cards;
	if (AC->Drawn != EZoogiArtifact::None) Cards.Add({ 0, AC->Drawn });
	for (int32 i = 0; i < AC->Hand.Num(); ++i) Cards.Add({ i + 1, AC->Hand[i] });
	const float CW = 230.f * U, CH = 250.f * U, Gap = 18.f * U;
	const float PW = FMath::Max(3, Cards.Num()) * (CW + Gap) + 40.f * U + 400.f * U;
	const float X0 = (W - PW) * 0.5f, Y0 = H - CH - 160.f * U;
	Panel(X0, Y0 - 56.f * U, PW, CH + 96.f * U, GPanel);
	Panel(X0, Y0 - 56.f * U, PW, 5.f * U, PC);
	Text(FString::Printf(TEXT("AWAKEN  -  %s%s"), *B->Def.Name.ToUpper(), B->bCPU ? TEXT("  (CPU deciding)") : TEXT("")), X0 + 24.f * U, Y0 - 44.f * U, FLinearColor(1.f, 0.95f, 0.85f), 0.6f);
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	const bool bFullHand = AC->Drawn != EZoogiArtifact::None && AC->Hand.Num() >= UZoogiArtifactComponent::HandCap;
	// Rivals with a guard up: an infused attack into them gets blocked, so it matters for the choice.
	{
		FString Guards;
		for (const AZoogiBall* O : G->Balls)
		{
			if (!O || O == B || O->bOut || !O->Artifact || O->Artifact->Guard == EZoogiArtifact::None) continue;
			Guards += FString::Printf(TEXT("%sP%d %s (%s)"), Guards.IsEmpty() ? TEXT("") : TEXT(",  "), O->PlayerIndex + 1, *O->Def.Name, *ZoogiArtifacts::Name(O->Artifact->Guard));
		}
		const FString Line = Guards.IsEmpty() ? FString(TEXT("no rival guards up")) : FString::Printf(TEXT("GUARDED: %s"), *Guards);
		Text(Line, X0 + PW - 24.f * U - TextSize(Line, 0.36f, false).X, Y0 - 40.f * U, Guards.IsEmpty() ? FLinearColor(0.55f, 0.6f, 0.7f) : FLinearColor(0.55f, 0.8f, 1.f), 0.36f, false, false);
	}
	// Footer: key hints and the hand-limit warning.
	{
		const float FY = Y0 + CH + 6.f * U;
		if (!B->bCPU) Text(TEXT("keys:  1-3 pick card    I infuse    G guard    S save"), X0 + 24.f * U, FY, FLinearColor(0.55f, 0.6f, 0.72f), 0.32f, false, false);
		if (bFullHand)
		{
			const FString Warn = FString::Printf(TEXT("HAND FULL - saving %s discards %s"), *ZoogiArtifacts::Name(AC->Drawn), *ZoogiArtifacts::Name(AC->Hand[0]));
			Text(Warn, X0 + PW - 24.f * U - TextSize(Warn, 0.34f, false).X, FY, FLinearColor(1.f, 0.6f, 0.35f), 0.34f, false, false);
		}
	}
	int32 Pick = G->AwakenPick;
	if (!Cards.ContainsByPredicate([Pick](const TPair<int32, EZoogiArtifact>& C) { return C.Key == Pick; }) && Cards.Num()) Pick = Cards[0].Key;
	float X = X0 + 20.f * U;
	for (const TPair<int32, EZoogiArtifact>& C : Cards)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(C.Value);
		const bool bSel = C.Key == Pick;
		float MX = 0.f, MY = 0.f;
		const bool bHover = PlayerOwner && PlayerOwner->GetMousePosition(MX, MY) && MX - FrameOrg.X >= X && MX - FrameOrg.X <= X + CW && MY - FrameOrg.Y >= Y0 && MY - FrameOrg.Y <= Y0 + CH;
		Panel(X, Y0, CW, CH, bSel ? FLinearColor(R.CoreColor.R * 0.3f, R.CoreColor.G * 0.3f, R.CoreColor.B * 0.3f, 0.95f) : FLinearColor(0.05f, 0.07f, 0.14f, bHover ? 0.98f : 0.9f));
		if (bSel)
		{
			Panel(X, Y0, CW, 4.f * U, R.RimColor); Panel(X, Y0 + CH - 4.f * U, CW, 4.f * U, R.RimColor);
			Panel(X, Y0, 4.f * U, CH, R.RimColor); Panel(X + CW - 4.f * U, Y0, 4.f * U, CH, R.RimColor);
		}
		const float Bob = bSel ? 4.f * U * FMath::Sin(Time * 4.f) : 0.f;
		Gem((uint8)C.Value, X + CW * 0.5f, Y0 + 82.f * U + Bob, 46.f * U);
		Text(FString::Printf(TEXT("%s%s"), B->bCPU ? TEXT("") : *FString::Printf(TEXT("[%d] "), C.Key + 1), C.Key == 0 ? TEXT("NEW") : *FString::Printf(TEXT("SAVED %d"), C.Key)), X + 12.f * U, Y0 + 8.f * U, C.Key == 0 ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.75f, 0.85f), 0.34f, false, false);
		if (bFullHand && C.Key == 1) Text(TEXT("discarded on SAVE"), X + CW * 0.5f, Y0 + CH - 28.f * U, FLinearColor(1.f, 0.55f, 0.35f), 0.3f, true, false);
		if (C.Value == Aff) Text(TEXT("AFFINITY"), X + CW - 12.f * U - TextSize(TEXT("AFFINITY"), 0.34f, false).X, Y0 + 8.f * U, FLinearColor(1.f, 0.9f, 0.4f), 0.34f, false, false);
		Text(R.DisplayName, X + CW * 0.5f, Y0 + 150.f * U, FLinearColor::White, 0.5f, true);
		Text(ZoogiArtifacts::Key(C.Value).ToUpper(), X + CW * 0.5f, Y0 + 192.f * U, R.RimColor, 0.36f, true, false);
		FZoogiButton Bt;
		Bt.Rect = FBox2D(FVector2D(X, Y0), FVector2D(X + CW, Y0 + CH));
		Bt.Action = TEXT("AwakenPick");
		Bt.Index = C.Key;
		if (!B->bCPU) Buttons.Add(Bt);
		X += CW + Gap;
	}
	// Empty hand slots (hand of 2 + the drawn card), so the hand limit reads at a glance.
	for (int32 e = Cards.Num(); e < 3; ++e)
	{
		ZoogiUIStyle::CPanel(Canvas, X, Y0, CW, CH, U, FLinearColor(0.0f, 0.24f, 0.34f, 0.6f));
		Panel(X + 14.f * U, Y0 + 12.f * U, CW - 28.f * U, 3.f * U, FLinearColor(0.3f, 0.9f, 1.f, 0.6f));
		Text(TEXT("HAND SLOT"), X + CW * 0.5f, Y0 + CH * 0.42f, FLinearColor(0.6f, 0.9f, 1.f, 0.9f), 0.38f, true, false);
		Text(TEXT("empty"), X + CW * 0.5f, Y0 + CH * 0.42f + 32.f * U, FLinearColor(0.55f, 0.8f, 1.f, 0.8f), 0.32f, true, false);
		X += CW + Gap;
	}
	// Right column: picked artifact's texts + INFUSE / GUARD / SAVE.
	const float RX = X0 + PW - 410.f * U;
	const EZoogiArtifact PA = Cards.Num() ? Cards[Cards.IndexOfByPredicate([Pick](const TPair<int32, EZoogiArtifact>& C) { return C.Key == Pick; })].Value : EZoogiArtifact::None;
	if (PA != EZoogiArtifact::None)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(PA);
		auto Wrap = [this](const FString& S, float X, float Y, float MaxW, const FLinearColor& C)
		{
			TArray<FString> Words;
			S.ParseIntoArray(Words, TEXT(" "));
			FString Line;
			for (const FString& Wd : Words)
			{
				const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
				if (TextSize(Try, 0.38f, false).X > MaxW && !Line.IsEmpty()) { Text(Line, X, Y, C, 0.38f, false, false); Y += 30.f * U; Line = Wd; }
				else Line = Try;
			}
			if (!Line.IsEmpty()) { Text(Line, X, Y, C, 0.38f, false, false); Y += 30.f * U; }
			return Y;
		};
		float TY = Y0;
		Text(TEXT("INFUSE"), RX, TY, FLinearColor(1.f, 0.7f, 0.35f), 0.38f, false, false); TY += 28.f * U;
		TY = Wrap(R.InfuseText, RX, TY, 390.f * U, FLinearColor(0.85f, 0.88f, 0.95f)) + 6.f * U;
		if (PA == Aff) { TY = Wrap(FString::Printf(TEXT("Affinity: %s fires with it"), *B->Def.Ability), RX, TY, 390.f * U, FLinearColor(1.f, 0.88f, 0.4f)) + 6.f * U; }
		Text(TEXT("GUARD"), RX, TY, FLinearColor(0.55f, 0.8f, 1.f), 0.38f, false, false); TY += 28.f * U;
		Wrap(R.GuardText, RX, TY, 390.f * U, FLinearColor(0.85f, 0.88f, 0.95f));
	}
	if (!B->bCPU)
	{
		const float BY = Y0 + CH - 56.f * U;
		const bool bHas = PA != EZoogiArtifact::None;
		Button(TEXT("INFUSE"), RX - 10.f * U, BY, 130.f * U, 56.f * U, "Infuse", Pick, FLinearColor(1.f, 0.5f, 0.15f), bHas);
		Button(TEXT("GUARD"), RX + 128.f * U, BY, 130.f * U, 56.f * U, "Guard", Pick, FLinearColor(0.35f, 0.7f, 1.f), bHas);
		Button(TEXT("SAVE"), RX + 266.f * U, BY, 124.f * U, 56.f * U, "Save", 0, FLinearColor(0.6f, 0.65f, 0.75f));
	}
}

void AZoogiHUD::DrawArtifacts(AZoogiGameMode* G)
{
	if (G && G->CardsOn() && UZoogiCardHandWidget::IsEnabled(G)) return; // UMG card hand is up: no old 'NO ARTIFACT ACTIVE' panel behind it
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !B->Artifact || G->Phase == EZoogiPhase::GameOver) return;
	if (G->Stage == EZoogiStage::Awaken && G->Phase == EZoogiPhase::Aiming && !G->bAwakenDone) return; // the Awaken panel shows it all
	if (G->Stage == EZoogiStage::Resolve || G->Stage == EZoogiStage::End) return; // infusion already spent; the Resolve panel says what happened
	UZoogiArtifactComponent* AC = B->Artifact;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 440.f * U, PH = 150.f * U;
	const float X = W - PW - 24.f * U, Y = H - PH - (G->IsHumanAiming() && G->IsAimStep() ? 230.f : 70.f) * U;
	const EZoogiArtifact Inf = AC->Infused;
	const FLinearColor Acc = Inf != EZoogiArtifact::None ? ZoogiArtifacts::Color(Inf) : (AC->Guard != EZoogiArtifact::None ? ZoogiArtifacts::Color(AC->Guard) : ZoogiUIStyle::Muted);
	Panel(X, Y, PW, PH, GPanel);
	Panel(X, Y, 8.f * U, PH, Acc);
	if (Inf != EZoogiArtifact::None)
	{
		Gem((uint8)Inf, X + 56.f * U, Y + 56.f * U, 30.f * U, 0.75f + 0.25f * FMath::Sin(Time * 6.f));
		Text(FString::Printf(TEXT("INFUSED: %s"), *ZoogiArtifacts::Name(Inf).ToUpper()), X + 104.f * U, Y + 12.f * U, Acc + FLinearColor(0.2f, 0.2f, 0.2f, 0.f), 0.48f);
		Text(ZoogiArtifacts::Affinity(B->Def.Id) == Inf ? FString::Printf(TEXT("affinity: %s rides along"), *B->Def.Ability) : FString(TEXT("powered collision on first contact")), X + 104.f * U, Y + 52.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.34f, false, false);
	}
	else if (AC->Guard != EZoogiArtifact::None)
	{
		Gem((uint8)AC->Guard, X + 56.f * U, Y + 56.f * U, 30.f * U);
		Text(FString::Printf(TEXT("GUARD: %s"), *ZoogiArtifacts::Name(AC->Guard).ToUpper()), X + 104.f * U, Y + 12.f * U, Acc + FLinearColor(0.2f, 0.2f, 0.2f, 0.f), 0.48f);
		Text(TEXT("protects you during the next opponent attack"), X + 104.f * U, Y + 52.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.34f, false, false);
	}
	else
	{
		Text(TEXT("NO ARTIFACT ACTIVE"), X + 28.f * U, Y + 12.f * U, FLinearColor(0.7f, 0.88f, 1.f), 0.48f);
		Text(TEXT("plain shot this turn"), X + 28.f * U, Y + 52.f * U, FLinearColor(0.65f, 0.85f, 1.f), 0.34f, false, false);
	}
	Text(TEXT("HAND"), X + 28.f * U, Y + PH - 44.f * U, FLinearColor(0.6f, 0.85f, 1.f), 0.34f, false, false);
	float GX = X + 110.f * U;
	for (EZoogiArtifact A : AC->Hand) { Gem((uint8)A, GX, Y + PH - 30.f * U, 14.f * U); GX += 40.f * U; }
	if (AC->Status != EZoogiStatus::None) Text(ZoogiArtifacts::StatusName(AC->Status), X + PW - 24.f * U - TextSize(ZoogiArtifacts::StatusName(AC->Status), 0.4f).X, Y + PH - 46.f * U, FLinearColor(1.f, 0.6f, 0.4f), 0.4f);
}

void AZoogiHUD::DrawResolve(AZoogiGameMode* G)
{
	if (G->Phase != EZoogiPhase::Resolving) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float A = FMath::Clamp(G->StageTime * 5.f, 0.f, 1.f);
	// Lower third, so the settled court (centre) stays visible.
	const float Y = H * 0.66f;
	const bool bNotes = !G->ResolveNotes.IsEmpty();
	const FVector2D TS = TextSize(G->ResolveSummary, 0.7f);
	const float NW = bNotes ? (float)TextSize(G->ResolveNotes, 0.42f, false).X : 0.f;
	const float PW = FMath::Max3(520.f * U, (float)TS.X + 80.f * U, NW + 80.f * U);
	const float PH = bNotes ? 150.f * U : 110.f * U;
	ZoogiUIStyle::CPanel(Canvas, (W - PW) * 0.5f, Y, PW, PH, U, FLinearColor(GPanel.R, GPanel.G, GPanel.B, GPanel.A * A), FLinearColor(1.f, 0.82f, 0.25f, A)); // uistyle: was flat near-black
	Text(TEXT("RESOLVE"), W * 0.5f, Y + 8.f * U, FLinearColor(0.7f, 0.9f, 1.f, A), 0.42f, true, false);
	Text(G->ResolveSummary, W * 0.5f, Y + 42.f * U, FLinearColor(1.f, 0.95f, 0.8f, A), 0.7f, true);
	if (bNotes) Text(G->ResolveNotes, W * 0.5f, Y + 110.f * U, G->bLastHitCrit ? FLinearColor(1.f, 0.85f, 0.4f, A) : FLinearColor(0.75f, 0.85f, 1.f, A), 0.42f, true, false);
}

void AZoogiHUD::DrawAnime(AZoogiGameMode* G)
{
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	if (!Anime || !Anime->bPlaying) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Age = Anime->Age, Len = FMath::Max(0.1f, Anime->Length);
	const float In = FMath::Clamp(Age / 0.12f, 0.f, 1.f), Out = FMath::Clamp((Len - Age) / 0.15f, 0.f, 1.f);
	const float A = In * Out;
	const FLinearColor C = Anime->Color;
	const FName Ev = Anime->Current.Event;
	const bool bImpact = Ev == TEXT("PoweredHit") || Ev == TEXT("Critical") || Ev == TEXT("GuardBlock") || Ev == TEXT("KO");
	if (Anime->HasMovie() && Anime->MovieTexture)
	{
		// Cut-in clip (Evan's anime art): a big 16:9 card with a white flash on the first frames of impact events.
		const float MW = W * 0.74f, MH = MW * 9.f / 16.f;
		if (bImpact && Age < 0.08f) Panel(0, 0, W, H, FLinearColor(1.f, 1.f, 1.f, 0.6f * (1.f - Age / 0.08f)));
		Panel((W - MW) * 0.5f - 6.f * U, (H - MH) * 0.5f - 6.f * U, MW + 12.f * U, MH + 12.f * U, FLinearColor(C.R, C.G, C.B, 0.9f * A));
		DrawTexture(Anime->MovieTexture, (W - MW) * 0.5f, (H - MH) * 0.5f, MW, MH, 0.f, 0.f, 1.f, 1.f, FLinearColor(1, 1, 1, A));
	}
	else
	{
		// Placeholder anime card: speed lines toward the focus, white impact frame, caption.
		FVector2D Focus(W * 0.5f, H * 0.5f);
		if (AActor* F = Anime->Current.Focus.Get())
		{
			const FVector S = Project(F->GetActorLocation(), true);
			if (S.Z > 0.f) Focus = FVector2D(S.X, S.Y);
		}
		if (bImpact && Age < 0.09f) Panel(0, 0, W, H, FLinearColor(1.f, 1.f, 1.f, 0.7f * (1.f - Age / 0.09f)));
		const int32 Lines = Ev == TEXT("Critical") ? 64 : 40;
		for (int32 k = 0; k < Lines; ++k)
		{
			const float Ang = k * UE_TWO_PI / Lines + FMath::Sin(k * 12.9898f) * 0.05f;
			const float Jit = 0.5f + 0.5f * FMath::Sin(Time * 40.f + k * 3.1f);
			const float R0 = FMath::Lerp(H * 0.28f, H * 0.18f, Jit), R1 = H * 1.2f;
			const FVector2D D(FMath::Cos(Ang), FMath::Sin(Ang));
			FLinearColor LC = (k % 3 == 0) ? FLinearColor(C.R, C.G, C.B, 0.55f * A) : FLinearColor(1.f, 1.f, 1.f, 0.35f * A);
			DrawLine(Focus.X + D.X * R0, Focus.Y + D.Y * R0, Focus.X + D.X * R1, Focus.Y + D.Y * R1, LC, (k % 4 == 0 ? 5.f : 2.f) * U);
		}
		if (Anime->Current.Artifact != EZoogiArtifact::None) Gem((uint8)Anime->Current.Artifact, W * 0.5f, H * 0.62f, 40.f * U * (1.f + 0.15f * FMath::Sin(Time * 10.f)), A);
	}
	const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - Age / 0.15f, 0.f, 1.f);
	const FLinearColor CC(FMath::Min(1.f, C.R + 0.35f), FMath::Min(1.f, C.G + 0.35f), FMath::Min(1.f, C.B + 0.35f), A);
	Text(Anime->Caption, W * 0.5f, H * 0.17f, CC, (Ev == TEXT("Critical") ? 2.0f : 1.4f) * Pop, true);
	// The sequence id / clip filename line is a debug aid only (Evan: no filenames on the cards); -ZoogiAnimeDebug shows it again.
	static const bool bAnimeDebugCaption = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeDebug"));
	if (bAnimeDebugCaption) Text(Anime->SubCaption, W * 0.5f, H * 0.17f + 120.f * U, FLinearColor(0.85f, 0.9f, 1.f, 0.8f * A), 0.36f, true, false);
	// Progress + skip hint.
	const float BW = 360.f * U;
	if (A > 0.05f) ZoogiUIStyle::CBar(Canvas, (W - BW) * 0.5f, H * 0.9f, BW, 8.f * U, U, FMath::Clamp(Age / Len, 0.f, 1.f), FLinearColor(C.R, C.G, C.B, A), FLinearColor(C.R * 0.6f + 0.1f, C.G * 0.6f + 0.2f, FMath::Min(1.f, C.B + 0.4f), A), nullptr, 4); // uistyle
	if (Age > 0.3f) Button(TEXT("SKIP"), W - 190.f * U, H * 0.9f - 10.f * U, 150.f * U, 52.f * U, "AnimeSkip", 0, FLinearColor(0.8f, 0.85f, 0.95f));
	if (Age > 0.3f) Text(TEXT("tap / click / SPACE to skip"), W * 0.5f, H * 0.9f + 14.f * U, FLinearColor(0.8f, 0.85f, 0.95f, 0.7f * A), 0.32f, true, false);
}

// ------------------------------------------------------------------ 2v2 Combo

void AZoogiHUD::RingMarks(int32 Marks, float X, float Y, float S, bool bKO)
{
	for (int32 i = 0; i < 2; ++i)
	{
		const float MX = X + i * (S + 8.f * U);
		const bool bOn = i < Marks && !bKO;
		const FLinearColor C = bOn ? FLinearColor(1.f, 0.85f, 0.3f) : ZoogiUIStyle::Muted;
		Panel(MX, Y, S, S, C);
		if (!bOn) Panel(MX + 3.f * U, Y + 3.f * U, S - 6.f * U, S - 6.f * U, ZoogiUIStyle::Track);
	}
}

void AZoogiHUD::DrawCombo(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const bool bCine = G->bCinematicCam && (G->Stage == EZoogiStage::Draw || G->IsPowerUpStep());
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	const bool bAnime = Anime && Anime->bPlaying;
	if (G->Phase == EZoogiPhase::GameOver) return;

	if (!bCine && !bAnime)
	{
		// Team panel (top right, under the camera / menu buttons).
		const float PW = 560.f * U, PH = 212.f * U;
		const bool bFramed = ZoogiFramed::IsActive(GetWorld()); // framed: team gauges live in the side columns, callouts stay
		const float X0 = W - PW - 24.f * U, Y0 = bFramed ? 24.f * U - PH - 12.f * U : 84.f * U;
		if (!bFramed) Panel(X0, Y0, PW, PH, GPanel);
		for (int32 t = 0; t < 2 && !bFramed; ++t)
		{
			const float RY = Y0 + 10.f * U + t * 82.f * U;
			const FLinearColor TC = AZoogiGameMode::TeamColor(t);
			const bool bTurn = G->ComboTeam == t;
			if (bTurn) Panel(X0 + 6.f * U, RY - 4.f * U, PW - 12.f * U, 78.f * U, FLinearColor(TC.R, TC.G, TC.B, 0.18f));
			Panel(X0 + 6.f * U, RY - 4.f * U, 6.f * U, 78.f * U, TC);
			FString Names;
			for (AZoogiBall* B : G->Balls) if (B && B->TeamIndex == t) Names += FString::Printf(TEXT("%s%s"), Names.IsEmpty() ? TEXT("") : TEXT(" + "), *B->Def.Name);
			Text(FString::Printf(TEXT("%s  %s"), *AZoogiGameMode::TeamName(t), bTurn ? TEXT("- TURN") : TEXT("")), X0 + 22.f * U, RY, TC + FLinearColor(0.25f, 0.25f, 0.25f, 0.f), 0.5f);
			Text(Names, X0 + 250.f * U, RY + 4.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.36f, false, false);
			// Combo Gauge with the follow-up cost tick.
			const float BX = X0 + 22.f * U, BY = RY + 36.f * U, BW = 300.f * U, BH = 16.f * U;
			const float E = G->ComboEnergy[t];
			const bool bReady = E >= ZoogiComboTune::FollowUpCost;
			Panel(BX, BY, BW, BH, FLinearColor(0.08f, 0.1f, 0.18f, 0.95f));
			const float Glow = bReady ? 0.15f * (0.5f + 0.5f * FMath::Sin(Time * 6.f)) : 0.f;
			Panel(BX, BY, BW * FMath::Clamp(E / ZoogiComboTune::GaugeMax, 0.f, 1.f), BH, FLinearColor(FMath::Min(1.f, TC.R + Glow), FMath::Min(1.f, TC.G + Glow), FMath::Min(1.f, TC.B + Glow), 1.f));
			const float TickX = BX + BW * ZoogiComboTune::FollowUpCost / ZoogiComboTune::GaugeMax;
			Panel(TickX - 1.5f * U, BY - 4.f * U, 3.f * U, BH + 8.f * U, FLinearColor(1.f, 0.9f, 0.4f, 0.95f));
			Text(FString::Printf(TEXT("COMBO %.0f"), E), BX + BW + 12.f * U, BY - 4.f * U, bReady ? FLinearColor(1.f, 0.9f, 0.4f) : FLinearColor(0.75f, 0.8f, 0.9f), 0.4f);
			Text(FString::Printf(TEXT("RESONANCE %d/3    ARENA CONTROL %.0f"), G->ComboResonance[t], G->ArenaControl[t]), BX, BY + 20.f * U, FLinearColor(0.75f, 0.65f, 1.f), 0.34f, false, false);
			if (bTurn)
			{
				const FString FU = G->bInFollowUp ? TEXT("FOLLOW-UP IN PLAY") : (G->bFollowUpUsed ? TEXT("FOLLOW-UP USED") : (bReady ? TEXT("FOLLOW-UP READY") : TEXT("")));
				if (!FU.IsEmpty()) Text(FU, X0 + PW - 20.f * U - TextSize(FU, 0.34f, false).X, BY + 20.f * U, FLinearColor(1.f, 0.85f, 0.35f), 0.34f, false, false);
			}
		}
		const FString Floor = G->ComboArena ? AZoogiComboArena::FloorStateName(G->ComboArena->GetFloorState()) : TEXT("-");
		const bool bShift = G->ComboArena && G->ComboArena->IsShifted();
		if (!bFramed) Text(FString::Printf(TEXT("FLOOR SECTION: %s      RESONANCE SHIFT: %s"), *Floor, bShift ? TEXT("ACTIVE (bumpers swell)") : *FString::Printf(TEXT("at %.0f control"), ZoogiComboTune::ShiftAtControl)),
			X0 + 22.f * U, Y0 + PH - 30.f * U, FLinearColor(1.f, 0.7f, 0.4f), 0.34f, false, false);

		// Callouts under the panel (newest on top).
		float CY = Y0 + PH + 12.f * U;
		for (const FZoogiCallout& C : G->Callouts)
		{
			const float A = FMath::Clamp((4.5f - C.Age) / 0.6f, 0.f, 1.f) * FMath::Clamp(C.Age * 8.f, 0.f, 1.f);
			if (A <= 0.f) continue;
			const float TW = TextSize(C.Text, 0.42f, true).X + 30.f * U;
			const float CX = W - 24.f * U - TW + (1.f - FMath::Clamp(C.Age * 6.f, 0.f, 1.f)) * 60.f * U;
			Panel(CX, CY, TW, 40.f * U, FLinearColor(0.02f, 0.03f, 0.08f, 0.8f * A));
			Panel(CX + TW - 6.f * U, CY, 6.f * U, 40.f * U, FLinearColor(C.Color.R, C.Color.G, C.Color.B, A));
			Text(C.Text, CX + 12.f * U, CY + 6.f * U, FLinearColor(C.Color.R * 0.35f + 0.65f, C.Color.G * 0.35f + 0.65f, C.Color.B * 0.35f + 0.65f, A), 0.42f);
			CY += 46.f * U;
		}
	}

	// Partner Follow-Up prompt.
	if (G->bFollowUpOffered && !bAnime)
	{
		AZoogiBall* P = G->PartnerOf(G->GetActiveBall());
		const float PW = 760.f * U, PH = 190.f * U;
		const float X0 = (W - PW) * 0.5f, Y0 = H * 0.56f;
		const FLinearColor TC = AZoogiGameMode::TeamColor(G->ComboTeam);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 6.f);
		Panel(X0, Y0, PW, PH, FLinearColor(0.02f, 0.03f, 0.08f, 0.92f));
		Panel(X0, Y0, PW, 6.f * U, FLinearColor::LerpUsingHSV(TC, FLinearColor(1.f, 0.85f, 0.3f), Pulse));
		Text(TEXT("PARTNER FOLLOW-UP!"), W * 0.5f, Y0 + 16.f * U, FLinearColor(1.f, 0.85f, 0.3f), 0.9f, true);
		Text(FString::Printf(TEXT("%s   ->   %s acts now      cost %.0f  (gauge %.0f)"), *AZoogiGameMode::TriggerNames(G->OfferTriggers), P ? *P->Def.Name.ToUpper() : TEXT("PARTNER"), ZoogiComboTune::FollowUpCost, G->ComboEnergy[G->ComboTeam]),
			W * 0.5f, Y0 + 72.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.42f, true, false);
		if (P && !P->bCPU)
		{
			Button(TEXT("F  ACCEPT"), W * 0.5f - 250.f * U, Y0 + 112.f * U, 240.f * U, 60.f * U, "FollowUp", 0, FLinearColor(0.35f, 1.f, 0.45f), true);
			Button(TEXT("X  SKIP"), W * 0.5f + 10.f * U, Y0 + 112.f * U, 240.f * U, 60.f * U, "FollowSkip", 0, FLinearColor(1.f, 0.45f, 0.35f));
		}
		else Text(TEXT("CPU partner takes the follow-up..."), W * 0.5f, Y0 + 124.f * U, FLinearColor(0.75f, 0.8f, 0.9f), 0.45f, true, false);
	}
}

// v3 P5: read-only episode playback panel (top centre). Reads the playback copy; SKIP only ends the presentation.
void AZoogiHUD::DrawEpisodeP5(AZoogiGameMode* G)
{
	const UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(G);
	const FZoogiEpPlayback* P = D ? D->Current() : nullptr;
	if (!P || P->Done()) return;
	const float W = Canvas->ClipX, PW = 900.f * U, X0 = (W - PW) * 0.5f, Y0 = 90.f * U;
	Panel(X0, Y0, PW, 210.f * U, GPanel); // uistyle: was a flat near-black DrawRect
	const int32 Cur = P->PerspectiveIndex();
	Text(FString::Printf(TEXT("EPISODE %d  -  %s  -  P%d    %.1f / %.0f s"), P->Ep.EpisodeId, ZoogiEpBranchName(P->Ep.Branch), P->Ep.Spender + 1, P->T, P->Ep.Duration), X0 + 20.f * U, Y0 + 12.f * U, FLinearColor(1.f, 0.85f, 0.4f), 0.5f, false, true);
	for (int32 i = 0; i < P->Ep.Perspectives.Num(); ++i)
	{
		const FZoogiEpPerspective& V = P->Ep.Perspectives[i];
		Text(FString::Printf(TEXT("%d  %s"), i + 1, *V.Line), X0 + 24.f * U, Y0 + (56.f + i * 36.f) * U, i == Cur ? FLinearColor::White : ZoogiUIStyle::MutedText, i == Cur ? 0.42f : 0.36f, false, false);
	}
	Button(TEXT("SKIP"), X0 + PW - 130.f * U, Y0 + 10.f * U, 110.f * U, 40.f * U, "EpisodeSkip", 0, FLinearColor(1.f, 0.85f, 0.4f));
}
