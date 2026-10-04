#include "ZoogiHUD.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiPlayerController.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiOrb.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
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

static const FLinearColor GPanel(0.02f, 0.03f, 0.08f, 0.72f);
static const FLinearColor GOrbBlue(0.53f, 0.81f, 0.92f, 1.f);
// Engine fonts are small at 1080p; scale everything up so labels read at a glance.
static constexpr float FlyTime = 0.65f; // orb pickup flight to the scoreboard
static float TextMul(bool bBig) { return bBig ? 1.75f : 2.3f; }

void AZoogiHUD::Panel(float X, float Y, float W, float H, const FLinearColor& C)
{
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
	Item.EnableShadow(FLinearColor(0, 0, 0, 0.85f * C.A), FVector2D(2.f * U, 2.f * U));
	Canvas->DrawItem(Item);
}

bool AZoogiHUD::Button(const FString& Label, float X, float Y, float W, float H, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected)
{
	float MX = 0.f, MY = 0.f;
	const bool bHover = PlayerOwner && PlayerOwner->GetMousePosition(MX, MY) && MX >= X && MX <= X + W && MY >= Y && MY <= Y + H;
	FLinearColor Bg = bSelected ? Accent * 0.75f : FLinearColor(0.08f, 0.1f, 0.18f, 0.9f);
	if (bHover) Bg = Bg + FLinearColor(0.12f, 0.12f, 0.12f, 0.f);
	Bg.A = 0.92f;
	Panel(X, Y, W, H, Bg);
	Panel(X, Y + H - 4.f * U, W, 4.f * U, Accent);
	const FVector2D TS = TextSize(Label, 0.55f, true);
	Text(Label, X + W * 0.5f, Y + (H - TS.Y) * 0.5f, FLinearColor::White, 0.55f, true, true);
	FZoogiButton B;
	B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	B.Action = Action;
	B.Index = Index;
	Buttons.Add(B);
	return bHover;
}

bool AZoogiHUD::HitTest(const FVector2D& Mouse, FName& OutAction, int32& OutIndex) const
{
	for (const FZoogiButton& B : Buttons)
	{
		if (B.Rect.IsInside(Mouse))
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
	Time += FApp::GetDeltaTime(); // real time: keeps animating while an anime freeze pauses the world
	static const bool bNoHUD = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHUD")); // clean captures (portraits)
	if (bNoHUD && G->Phase != EZoogiPhase::Menu) return;

	if (CanvasMenuMode < 0) CanvasMenuMode = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCanvasMenu")) ? 1 : 0;
	if (G->Phase == EZoogiPhase::Menu)
	{
		if (CanvasMenuMode == 0 && PlayerOwner)
		{
			if (!MenuWidget)
			{
				MenuWidget = CreateWidget<UZoogiMenuWidget>(PlayerOwner, UZoogiMenuWidget::StaticClass());
				if (MenuWidget) MenuWidget->AddToViewport(10);
			}
			if (MenuWidget)
			{
				if (MenuWidget->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
				{
					MenuWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					MenuWidget->MarkDirty();
				}
				MenuWidget->Refresh(GetWorld()->GetDeltaSeconds());
				return;
			}
		}
		DrawMenu(G);
		return;
	}
	if (MenuWidget && MenuWidget->GetVisibility() != ESlateVisibility::Collapsed) MenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	DrawMatch(G);
	if (G->Phase == EZoogiPhase::GameOver) DrawResults(G);
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
	Panel(X, Y, 400.f * U, RowH * G->Balls.Num() + 16.f * U, GPanel);
	Y += 8.f * U;
	for (AZoogiBall* B : G->Balls)
	{
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
		FString Status = FString::Printf(TEXT("orbs %d  KOs %d  "), B->OrbsScored, B->KnockOuts);
		if (G->bArtifactMode && B->Artifact)
		{
			Status += FString::Printf(TEXT("hand %d"), B->Artifact->Hand.Num());
			if (B->Artifact->Guard != EZoogiArtifact::None) Status += TEXT("  GUARD");
			if (B->Artifact->Status != EZoogiStatus::None) Status += FString::Printf(TEXT("  %s"), ZoogiArtifacts::StatusName(B->Artifact->Status));
		}
		else for (int32 c = 0; c < 3; ++c) Status += c < B->Charges ? TEXT("*") : TEXT("-");
		if (B->bOut) Status += TEXT("  OUT");
		if (B->StunTurns > 0) Status += TEXT("  STUNNED");
		if (B->BoundShots > 0) Status += TEXT("  BOUND");
		Text(Status, X + 26.f * U, Y + 32.f * U, (B->StunTurns > 0 || B->BoundShots > 0) ? FLinearColor(0.85f, 0.7f, 1.f) : FLinearColor(0.65f, 0.72f, 0.85f), 0.38f, false, false);
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
	// Event feed under the scoreboard: newest on top, fades out after a few seconds.
	Y += 22.f * U;
	for (const FZoogiFeedItem& F : G->Feed)
	{
		const float A = FMath::Clamp(1.f - (F.Age - 4.5f) / 1.5f, 0.f, 1.f) * FMath::Clamp(F.Age * 6.f, 0.f, 1.f);
		if (A <= 0.f) continue;
		const float Slide = (1.f - FMath::Clamp(F.Age * 5.f, 0.f, 1.f)) * -40.f * U;
		const float W = TextSize(F.Text, 0.42f, false).X + 30.f * U;
		Panel(X + Slide, Y, W, 34.f * U, FLinearColor(0.02f, 0.03f, 0.08f, 0.72f * A));
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

	// Top centre: round, orbs left, turn banner.
	const FString Info = FString::Printf(TEXT("ROUND %d / %d     ORBS LEFT %d"), FMath::Min(G->RoundNumber, G->MenuRounds), G->MenuRounds, G->OrbsLeft());
	Panel(W * 0.5f - 260.f * U, 20.f * U, 520.f * U, 46.f * U, GPanel);
	Text(Info, W * 0.5f, 28.f * U, GOrbBlue, 0.55f, true);
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
			if (!Status.IsEmpty()) Text(Status, W * 0.5f, H - 70.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.5f, true, false);
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
	if (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken) DrawAwaken(G);
	if (G->IsHumanAiming() && G->IsAimStep()) DrawAimControls(G);
	if (Letterbox < 0.3f) Text(TEXT("Awaken: click a card + INFUSE / GUARD / SAVE (I/G/S)   Aim: click target or floor, A/D cut, Up/Down power, SPACE confirm   RMB/Esc cancel   C camera   Ctrl+R restart"), 24.f * U, H - 36.f * U, FLinearColor(0.7f, 0.75f, 0.85f, 0.85f), 0.36f, false, false);
	Button(TEXT("MENU"), W - 150.f * U, 24.f * U, 126.f * U, 46.f * U, "Menu", 0, GOrbBlue);
	Button(G->bCinematicCam ? TEXT("CAM: CINE") : TEXT("CAM: TACT"), W - 340.f * U, 24.f * U, 176.f * U, 46.f * U, "CamStyle", 0, FLinearColor(0.6f, 0.9f, 0.6f), G->bCinematicCam);
	DrawAnime(G);
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
		if (S0.Z > 0.f && S1.Z > 0.f)
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
	const FLinearColor Accent = bArmed ? FLinearColor(1.f, 0.8f, 0.2f) : (bCan ? GOrbBlue : FLinearColor(0.35f, 0.38f, 0.45f));
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
	Text(B->Def.AbilityDesc, X + 200.f * U, Y + PH - 40.f * U, FLinearColor(0.6f, 0.65f, 0.75f), 0.3f, false, false);
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
	Sorted.Sort([](const AZoogiBall& A, const AZoogiBall& B) { return A.Score > B.Score; });
	const int32 Top = Sorted.Num() ? Sorted[0]->Score : 0;
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		AZoogiBall* B = Sorted[i];
		const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
		const bool bWin = B->Score == Top && G->Balls.IsValidIndex(G->WinnerIndex) && B->PlayerIndex == G->WinnerIndex;
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
		Text(FString::Printf(TEXT("%d orbs   %d KOs%s"), B->OrbsScored, B->KnockOuts, B->bCPU ? TEXT("   CPU") : TEXT("")), TX, Y + 50.f * U, FLinearColor(0.72f, 0.78f, 0.9f), 0.42f, false, false);
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
	for (int32 i = 0; i < N; ++i)
	{
		const float X = X0 + i * (BW + Gap);
		const bool bNow = i == Cur, bDone = i < Cur;
		const FLinearColor Bg = bNow ? FLinearColor(PC.R * 0.75f, PC.G * 0.75f, PC.B * 0.75f, 0.92f) : FLinearColor(0.03f, 0.04f, 0.1f, bDone ? 0.75f : 0.55f);
		Panel(X, Y, BW, BH, Bg);
		if (bNow) Panel(X, Y + BH - 3.f * U, BW, 3.f * U, FLinearColor::White);
		FString L = AZoogiGameMode::StageName((EZoogiStage)i);
		if (bNow && G->Stage == EZoogiStage::Battle) L = FString::Printf(TEXT("%s: %s"), *L, AZoogiGameMode::StepName(G->BattleStep));
		Text(L, X + BW * 0.5f, Y + 4.f * U, bNow ? FLinearColor::White : FLinearColor(0.6f, 0.66f, 0.8f, bDone ? 0.9f : 0.6f), 0.38f, true, false);
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
	if (T && B && G->Phase == EZoogiPhase::Aiming)
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
				const FVector2D M = ZPC->GetCursor();
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

void AZoogiHUD::WorldArrow(const FVector& From, const FVector& To, const FLinearColor& Col, float Thick)
{
	const FVector S0 = Project(From, true), S1 = Project(To, true);
	if (S0.Z <= 0.f || S1.Z <= 0.f) return;
	DrawLine(S0.X, S0.Y, S1.X, S1.Y, FLinearColor(0, 0, 0, 0.4f * Col.A), Thick + 3.f * U);
	DrawLine(S0.X, S0.Y, S1.X, S1.Y, Col, Thick);
	const FVector2D D = FVector2D(S1.X - S0.X, S1.Y - S0.Y).GetSafeNormal();
	const FVector2D N(-D.Y, D.X);
	const float L = 18.f * U;
	DrawLine(S1.X, S1.Y, S1.X - D.X * L + N.X * L * 0.6f, S1.Y - D.Y * L + N.Y * L * 0.6f, Col, Thick);
	DrawLine(S1.X, S1.Y, S1.X - D.X * L - N.X * L * 0.6f, S1.Y - D.Y * L - N.Y * L * 0.6f, Col, Thick);
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
	const FLinearColor PathC = Inf != EZoogiArtifact::None ? ZoogiArtifacts::Color(Inf) : PC;
	for (int32 i = 0; i < P.Path.Num(); ++i)
	{
		const FVector S = Project(P.Path[i], true);
		if (S.Z <= 0.f) continue;
		const float T = (float)(i + 1) / P.Path.Num();
		const float Size = FMath::Lerp(12.f, 6.f, T) * U * (bPower ? 1.2f : 1.f);
		FLinearColor C = P.PathOff.IsValidIndex(i) && P.PathOff[i] ? FLinearColor(1.f, 0.25f, 0.2f, 0.9f) : FMath::Lerp(PathC, FLinearColor::White, T * 0.5f);
		C.A = FMath::Lerp(0.95f, 0.45f, T);
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
		WorldCircle(GC, ZoogiConst::ZoogiRadius, FLinearColor(1.f, 1.f, 1.f, 0.85f), 2.5f * U);
		if (AActor* T = P.ContactActor.Get())
		{
			FVector TP = Cast<AZoogiOrb>(T) ? Cast<AZoogiOrb>(T)->OrbLocation() : (Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation());
			TP.Z = GC.Z;
			const float Len = FMath::Lerp(180.f, 520.f, FMath::Clamp(P.Fraction, 0.f, 1.f)) * FMath::Max(0.2f, FMath::Cos(FMath::DegreesToRadians(P.CutDeg)));
			WorldArrow(TP, TP + P.TargetDir * Len, FLinearColor(1.f, 0.8f, 0.25f, 0.95f), 4.f * U);
		}
		if (!P.ShooterDir.IsNearlyZero())
		{
			WorldArrow(GC, GC + P.ShooterDir * 300.f, FLinearColor(PC.R, PC.G, PC.B, 0.7f), 3.f * U);
		}
		const FVector S = Project(GC, true);
		if (S.Z > 0.f) Text(FString::Printf(TEXT("cut %.0f deg"), P.CutDeg), S.X, S.Y + 26.f * U, FLinearColor(0.9f, 0.95f, 1.f), 0.38f, true, false);
	}
}

void AZoogiHUD::DrawAimControls(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 1060.f * U, PH = 118.f * U;
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
	static const TCHAR* PowerNames[4] = { TEXT("AUTO"), TEXT("SOFT"), TEXT("MED"), TEXT("FULL") };
	for (int32 i = 0; i < 4; ++i) { Button(PowerNames[i], X, BY, 96.f * U, BH, "AimPower", i, FLinearColor(1.f, 0.8f, 0.3f), G->AimPowerStep == i); X += 102.f * U; }
	X += 18.f * U;
	Button(TEXT("CONFIRM"), X, BY, 170.f * U, BH, "AimConfirm", 0, FLinearColor(0.35f, 1.f, 0.45f), G->HasAim()); X += 178.f * U;
	Button(TEXT("CANCEL"), X, BY, 150.f * U, BH, "AimCancel", 0, FLinearColor(1.f, 0.4f, 0.35f));
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
	const float PW = FMath::Max(3, Cards.Num()) * (CW + Gap) + 40.f * U + 300.f * U;
	const float X0 = (W - PW) * 0.5f, Y0 = H - CH - 160.f * U;
	Panel(X0, Y0 - 56.f * U, PW, CH + 96.f * U, GPanel);
	Panel(X0, Y0 - 56.f * U, PW, 5.f * U, PC);
	Text(FString::Printf(TEXT("AWAKEN  -  %s%s"), *B->Def.Name.ToUpper(), B->bCPU ? TEXT("  (CPU deciding)") : TEXT("")), X0 + 24.f * U, Y0 - 44.f * U, FLinearColor(1.f, 0.95f, 0.85f), 0.6f);
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	int32 Pick = G->AwakenPick;
	if (!Cards.ContainsByPredicate([Pick](const TPair<int32, EZoogiArtifact>& C) { return C.Key == Pick; }) && Cards.Num()) Pick = Cards[0].Key;
	float X = X0 + 20.f * U;
	for (const TPair<int32, EZoogiArtifact>& C : Cards)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(C.Value);
		const bool bSel = C.Key == Pick;
		float MX = 0.f, MY = 0.f;
		const bool bHover = PlayerOwner && PlayerOwner->GetMousePosition(MX, MY) && MX >= X && MX <= X + CW && MY >= Y0 && MY <= Y0 + CH;
		Panel(X, Y0, CW, CH, bSel ? FLinearColor(R.CoreColor.R * 0.3f, R.CoreColor.G * 0.3f, R.CoreColor.B * 0.3f, 0.95f) : FLinearColor(0.05f, 0.07f, 0.14f, bHover ? 0.98f : 0.9f));
		if (bSel)
		{
			Panel(X, Y0, CW, 4.f * U, R.RimColor); Panel(X, Y0 + CH - 4.f * U, CW, 4.f * U, R.RimColor);
			Panel(X, Y0, 4.f * U, CH, R.RimColor); Panel(X + CW - 4.f * U, Y0, 4.f * U, CH, R.RimColor);
		}
		const float Bob = bSel ? 4.f * U * FMath::Sin(Time * 4.f) : 0.f;
		Gem((uint8)C.Value, X + CW * 0.5f, Y0 + 82.f * U + Bob, 46.f * U);
		Text(C.Key == 0 ? TEXT("NEW") : FString::Printf(TEXT("SAVED %d"), C.Key), X + 12.f * U, Y0 + 8.f * U, C.Key == 0 ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.75f, 0.85f), 0.34f, false, false);
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
	// Right column: picked artifact's texts + INFUSE / GUARD / SAVE.
	const float RX = X0 + PW - 320.f * U;
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
				if (TextSize(Try, 0.32f, false).X > MaxW && !Line.IsEmpty()) { Text(Line, X, Y, C, 0.32f, false, false); Y += 26.f * U; Line = Wd; }
				else Line = Try;
			}
			if (!Line.IsEmpty()) { Text(Line, X, Y, C, 0.32f, false, false); Y += 26.f * U; }
			return Y;
		};
		float TY = Y0;
		Text(TEXT("INFUSE"), RX, TY, FLinearColor(1.f, 0.7f, 0.35f), 0.38f, false, false); TY += 28.f * U;
		TY = Wrap(R.InfuseText, RX, TY, 300.f * U, FLinearColor(0.85f, 0.88f, 0.95f)) + 6.f * U;
		Text(TEXT("GUARD"), RX, TY, FLinearColor(0.55f, 0.8f, 1.f), 0.38f, false, false); TY += 28.f * U;
		Wrap(R.GuardText, RX, TY, 300.f * U, FLinearColor(0.85f, 0.88f, 0.95f));
	}
	if (!B->bCPU)
	{
		const float BY = Y0 + CH - 56.f * U;
		const bool bHas = PA != EZoogiArtifact::None;
		Button(TEXT("INFUSE"), RX - 10.f * U, BY, 104.f * U, 56.f * U, "Infuse", Pick, FLinearColor(1.f, 0.5f, 0.15f), bHas);
		Button(TEXT("GUARD"), RX + 102.f * U, BY, 104.f * U, 56.f * U, "Guard", Pick, FLinearColor(0.35f, 0.7f, 1.f), bHas);
		Button(TEXT("SAVE"), RX + 214.f * U, BY, 96.f * U, 56.f * U, "Save", 0, FLinearColor(0.6f, 0.65f, 0.75f));
	}
}

void AZoogiHUD::DrawArtifacts(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !B->Artifact || G->Phase == EZoogiPhase::GameOver) return;
	if (G->Stage == EZoogiStage::Awaken && G->Phase == EZoogiPhase::Aiming && !G->bAwakenDone) return; // the Awaken panel shows it all
	UZoogiArtifactComponent* AC = B->Artifact;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 440.f * U, PH = 150.f * U;
	const float X = W - PW - 24.f * U, Y = H - PH - (G->IsHumanAiming() && G->IsAimStep() ? 230.f : 70.f) * U;
	const EZoogiArtifact Inf = AC->Infused;
	const FLinearColor Acc = Inf != EZoogiArtifact::None ? ZoogiArtifacts::Color(Inf) : (AC->Guard != EZoogiArtifact::None ? ZoogiArtifacts::Color(AC->Guard) : FLinearColor(0.35f, 0.38f, 0.45f));
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
		Text(TEXT("NO ARTIFACT ACTIVE"), X + 28.f * U, Y + 12.f * U, FLinearColor(0.7f, 0.74f, 0.85f), 0.48f);
		Text(TEXT("plain shot this turn"), X + 28.f * U, Y + 52.f * U, FLinearColor(0.65f, 0.7f, 0.8f), 0.34f, false, false);
	}
	Text(TEXT("HAND"), X + 28.f * U, Y + PH - 44.f * U, FLinearColor(0.6f, 0.66f, 0.8f), 0.34f, false, false);
	float GX = X + 110.f * U;
	for (EZoogiArtifact A : AC->Hand) { Gem((uint8)A, GX, Y + PH - 30.f * U, 14.f * U); GX += 40.f * U; }
	if (AC->Status != EZoogiStatus::None) Text(ZoogiArtifacts::StatusName(AC->Status), X + PW - 24.f * U - TextSize(ZoogiArtifacts::StatusName(AC->Status), 0.4f).X, Y + PH - 46.f * U, FLinearColor(1.f, 0.6f, 0.4f), 0.4f);
}

void AZoogiHUD::DrawResolve(AZoogiGameMode* G)
{
	if (G->Phase != EZoogiPhase::Resolving) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float A = FMath::Clamp(G->StageTime * 5.f, 0.f, 1.f);
	const float Y = H * 0.36f;
	const FVector2D TS = TextSize(G->ResolveSummary, 0.7f);
	const float PW = FMath::Max(520.f * U, TS.X + 80.f * U);
	Panel((W - PW) * 0.5f, Y, PW, 110.f * U, FLinearColor(0.02f, 0.03f, 0.08f, 0.8f * A));
	Text(TEXT("RESOLVE"), W * 0.5f, Y + 8.f * U, FLinearColor(0.7f, 0.78f, 0.95f, A), 0.42f, true, false);
	Text(G->ResolveSummary, W * 0.5f, Y + 42.f * U, FLinearColor(1.f, 0.95f, 0.8f, A), 0.7f, true);
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
		const float MW = W * 0.62f, MH = MW * 9.f / 16.f;
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
	Text(Anime->SubCaption, W * 0.5f, H * 0.17f + 120.f * U, FLinearColor(0.85f, 0.9f, 1.f, 0.8f * A), 0.36f, true, false);
	// Progress + skip hint.
	const float BW = 360.f * U;
	Panel((W - BW) * 0.5f, H * 0.9f, BW, 6.f * U, FLinearColor(0.1f, 0.1f, 0.15f, 0.8f * A));
	Panel((W - BW) * 0.5f, H * 0.9f, BW * FMath::Clamp(Age / Len, 0.f, 1.f), 6.f * U, FLinearColor(C.R, C.G, C.B, A));
	if (Age > 0.3f) Text(TEXT("click / SPACE to skip"), W * 0.5f, H * 0.9f + 14.f * U, FLinearColor(0.8f, 0.85f, 0.95f, 0.7f * A), 0.32f, true, false);
}
