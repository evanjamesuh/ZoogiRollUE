#include "ZoogiHUD.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiPlayerController.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiOrb.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"

static const FLinearColor GPanel(0.02f, 0.03f, 0.08f, 0.72f);
static const FLinearColor GOrbBlue(0.53f, 0.81f, 0.92f, 1.f);
// Engine fonts are small at 1080p; scale everything up so labels read at a glance.
static float TextMul(bool bBig) { return bBig ? 1.6f : 2.1f; }

void AZoogiHUD::Panel(float X, float Y, float W, float H, const FLinearColor& C)
{
	DrawRect(C, X, Y, W, H);
}

FVector2D AZoogiHUD::TextSize(const FString& S, float Scale, bool bBig)
{
	float W = 0.f, H = 0.f;
	GetTextSize(S, W, H, bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont(), Scale * U * TextMul(bBig));
	return FVector2D(W, H);
}

void AZoogiHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter, bool bBig)
{
	UFont* F = bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	if (bCenter) X -= TextSize(S, Scale, bBig).X * 0.5f;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(S), F, C);
	Item.Scale = FVector2D(Scale * U * TextMul(bBig));
	Item.EnableShadow(FLinearColor(0, 0, 0, 0.8f), FVector2D(2.f * U, 2.f * U));
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
	Time += GetWorld()->GetDeltaSeconds();
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
	Text(TEXT("pull back - shoot - knock orbs and rivals off the edge - gold stars give power charges"), W * 0.5f, 190.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.7f, true, true);

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
	Text(TEXT("LMB: target   Tab: cycle   SPACE: fire   E/Q: Zoogi power   Esc: clear / menu"), W * 0.5f, 1000.f * U, FLinearColor(0.8f, 0.85f, 0.95f), 0.45f, true, false);
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
		Panel(X + 6.f * U, Y, 8.f * U, RowH - 6.f * U, PC);
		Text(FString::Printf(TEXT("P%d %s%s"), B->PlayerIndex + 1, *B->Def.Name, B->bCPU ? TEXT(" (CPU)") : TEXT("")), X + 26.f * U, Y + 4.f * U, bActive ? FLinearColor::White : FLinearColor(0.8f, 0.85f, 0.95f), 0.55f);
		FString Status = FString::Printf(TEXT("orbs %d  KOs %d  "), B->OrbsScored, B->KnockOuts);
		for (int32 c = 0; c < 3; ++c) Status += c < B->Charges ? TEXT("*") : TEXT("-");
		if (B->bOut) Status += TEXT("  OUT");
		if (B->StunTurns > 0) Status += TEXT("  STUNNED");
		if (B->BoundShots > 0) Status += TEXT("  BOUND");
		Text(Status, X + 26.f * U, Y + 32.f * U, (B->StunTurns > 0 || B->BoundShots > 0) ? FLinearColor(0.85f, 0.7f, 1.f) : FLinearColor(0.65f, 0.72f, 0.85f), 0.38f, false, false);
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
	const bool bBattle = G->Stage == EZoogiStage::Battle && G->Phase == EZoogiPhase::Aiming;
	const float Dt = GetWorld()->GetDeltaSeconds();
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
			else if (B->bCPU) Status = G->TargetLock.IsValid() ? FString::Printf(TEXT("CPU is targeting %s"), *G->TargetName(G->TargetLock.Get())) : TEXT("CPU is choosing a target");
			else if (G->Stage == EZoogiStage::Draw) Status = TEXT("Get ready...");
			else if (G->Stage == EZoogiStage::Battle) Status = TEXT("LOCKED - firing!");
			else if (G->TargetLock.IsValid() && !G->bSoftLock) Status = TEXT("SPACE or click the target again to fire   -   Tab: next target   -   RMB / Esc: clear");
			else Status = TEXT("Click an enemy or orb to target - Space to fire   (E / Q: power)");
			Text(Status, W * 0.5f, H - 70.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.5f, true, false);
		}
	}

	if (G->Phase == EZoogiPhase::Aiming) DrawAim(G);
	DrawPower(G);
	DrawPopups(G);

	DrawStages(G);
	if (Letterbox < 0.3f) Text(TEXT("LMB: target   Tab: cycle targets   SPACE: fire   E / Q: power   RMB / Esc: clear target (Esc again: menu)   Ctrl+R: restart"), 24.f * U, H - 36.f * U, FLinearColor(0.7f, 0.75f, 0.85f, 0.85f), 0.38f, false, false);
	Button(TEXT("MENU"), W - 150.f * U, 24.f * U, 126.f * U, 46.f * U, "Menu", 0, GOrbBlue);
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

void AZoogiHUD::DrawResults(AZoogiGameMode* G)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float PW = 720.f * U;
	const float X0 = (W - PW) * 0.5f;
	float Y = 200.f * U;
	Panel(X0, Y, PW, 140.f * U + 70.f * U * G->Balls.Num() + 120.f * U, GPanel);
	const FLinearColor WC = G->Balls.IsValidIndex(G->WinnerIndex) ? ZoogiUtil::PlayerColor(G->WinnerIndex) : FLinearColor::White;
	Text(G->Banner, W * 0.5f, Y + 30.f * U, WC + FLinearColor(0.2f, 0.2f, 0.2f, 0.f), 1.3f, true);
	Y += 140.f * U;
	TArray<AZoogiBall*> Sorted;
	for (AZoogiBall* B : G->Balls) Sorted.Add(B);
	Sorted.Sort([](const AZoogiBall& A, const AZoogiBall& B) { return A.Score > B.Score; });
	for (int32 i = 0; i < Sorted.Num(); ++i)
	{
		AZoogiBall* B = Sorted[i];
		const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
		Panel(X0 + 30.f * U, Y, PW - 60.f * U, 60.f * U, FLinearColor(0.05f, 0.07f, 0.14f, 0.9f));
		Panel(X0 + 30.f * U, Y, 10.f * U, 60.f * U, PC);
		Text(FString::Printf(TEXT("#%d  P%d %s"), i + 1, B->PlayerIndex + 1, *B->Def.Name), X0 + 60.f * U, Y + 10.f * U, FLinearColor::White, 0.65f);
		Text(FString::Printf(TEXT("%d orbs  %d KOs"), B->OrbsScored, B->KnockOuts), X0 + 400.f * U, Y + 20.f * U, FLinearColor(0.75f, 0.8f, 0.9f), 0.45f, false, false);
		Text(FString::FromInt(B->Score), X0 + PW - 50.f * U - TextSize(FString::FromInt(B->Score), 0.75f).X, Y + 8.f * U, PC, 0.75f);
		Y += 70.f * U;
	}
	Y += 20.f * U;
	Button(TEXT("REMATCH"), W * 0.5f - 300.f * U, Y, 280.f * U, 70.f * U, "Rematch", 0, FLinearColor(1.f, 0.75f, 0.2f), true);
	Button(TEXT("MENU"), W * 0.5f + 20.f * U, Y, 280.f * U, 70.f * U, "Menu", 0, GOrbBlue);
}

void AZoogiHUD::DrawStages(AZoogiGameMode* G)
{
	if (G->Phase != EZoogiPhase::Aiming && G->Phase != EZoogiPhase::Rolling) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	AZoogiBall* B = G->GetActiveBall();
	const FLinearColor PC = B ? ZoogiUtil::PlayerColor(B->PlayerIndex) : GOrbBlue;

	// Phase tracker: DRAW > STANDBY > MAIN > BATTLE > END.
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
		const FString L = AZoogiGameMode::StageName((EZoogiStage)i);
		Text(L, X + BW * 0.5f, Y + 4.f * U, bNow ? FLinearColor::White : FLinearColor(0.6f, 0.66f, 0.8f, bDone ? 0.9f : 0.6f), 0.34f, true, false);
	}

	// Big phase banner on every stage change.
	const float A = FMath::Clamp(1.f - (G->StageTime - 0.55f) / 0.45f, 0.f, 1.f) * FMath::Clamp(G->StageTime / 0.12f, 0.f, 1.f);
	if (A > 0.f && G->Phase != EZoogiPhase::GameOver)
	{
		static const TCHAR* Subs[] = { TEXT("hero introduction"), TEXT("pick a target"), TEXT("lock on and plan"), TEXT("lock-on - fire!"), TEXT("resolution") };
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
		G->LockShotVelocity(Frac, Impact);
		const float PW = 470.f * U, PH = 104.f * U;
		const float PX = W - PW - 24.f * U, PY = 90.f * U;
		const bool bBattle = G->Stage == EZoogiStage::Battle;
		const FLinearColor Acc = bBattle ? FLinearColor(1.f, 0.35f, 0.2f) : (G->bSoftLock ? FLinearColor(0.7f, 0.8f, 0.95f) : PC);
		Panel(PX, PY, PW, PH, GPanel);
		Panel(PX, PY, 8.f * U, PH, Acc);
		Text(G->bSoftLock ? TEXT("TARGET (soft)") : (bBattle ? TEXT("LOCKED - FIRING") : TEXT("TARGET LOCKED")), PX + 24.f * U, PY + 8.f * U, Acc, 0.36f, false, false);
		Text(G->TargetName(T), PX + 24.f * U, PY + 30.f * U, FLinearColor::White, 0.62f);
		Text(FString::Printf(TEXT("%.1f m    shot %d%%    hit %.1f m/s"), Dist / 100.f, FMath::RoundToInt(Frac * 100.f), Impact / 100.f), PX + 24.f * U, PY + 72.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.4f, false, false);
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
