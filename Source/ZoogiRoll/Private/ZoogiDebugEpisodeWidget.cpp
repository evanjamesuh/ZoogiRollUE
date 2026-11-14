#include "ZoogiDebugEpisodeWidget.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "CanvasItem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiUIStyle.h"

static constexpr double ZoogiDebugShatterFlashSeconds = 1.5;

bool UZoogiDebugEpisodeWidget::IsEnabledByCommandLine()
{
	return FParse::Param(FCommandLine::Get(), TEXT("ZoogiDebugUI"));
}

bool UZoogiDebugEpisodeWidget::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsEnabledByCommandLine() && Super::ShouldCreateSubsystem(Outer);
}

bool UZoogiDebugEpisodeWidget::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UZoogiDebugEpisodeWidget::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	hHUD = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiDebugEpisodeWidget::OnHUDPostRender);
	UE_LOG(LogZoogi, Log, TEXT("ZOOGI_DEBUGUI spawned world=%s"), GetWorld() ? *GetWorld()->GetName() : TEXT("-"));
}

void UZoogiDebugEpisodeWidget::Deinitialize()
{
	if (hHUD.IsValid()) AHUD::OnHUDPostRender.Remove(hHUD);
	Super::Deinitialize();
}

TStatId UZoogiDebugEpisodeWidget::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiDebugEpisodeWidget, STATGROUP_Tickables);
}

void UZoogiDebugEpisodeWidget::Tick(float DeltaTime)
{
	RealClock += FApp::GetDeltaTime();
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	if (!SM) return;
	const EZoogiBattleState S = SM->GetState();
	// Entering SHATTER_FAILURE can pass through to NEXT_TURN within one frame, so also watch the shatter counter.
	const bool bEntered = (S == EZoogiBattleState::ShatterFailure && LastState != EZoogiBattleState::ShatterFailure);
	if (bEntered || SM->ShatterCount != LastShatterCount)
	{
		ShatterFlashUntil = RealClock + ZoogiDebugShatterFlashSeconds;
		UE_LOG(LogZoogi, Log, TEXT("ZOOGI_DEBUGUI shatter flash reason=\"%s\""), *SM->LastShatterReason);
	}
	if (S != LastState)
		UE_LOG(LogZoogi, Verbose, TEXT("ZOOGI_DEBUGUI state %s"), ZoogiBattle::StateName(S));
	LastState = S;
	LastShatterCount = SM->ShatterCount;
	// uistyle: bar juice follows each Zoogi's energy (display only)
	const float Cap = FMath::Max(1.f, SM->GetTuning().EnergyCap);
	const int32 N = SM->Players.Num();
	if (EnergyJuice.Num() < N) EnergyJuice.SetNum(N);
	for (int32 i = 0; i < N; ++i) EnergyJuice[i].Update(FMath::Clamp(SM->GetEnergy(i) / Cap, 0.f, 1.f), FApp::GetDeltaTime());
}

void UZoogiDebugEpisodeWidget::OnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (!Canvas || !HUD || HUD->GetWorld() != GetWorld() || !GEngine) return;
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	// uistyle: navy outlined panel, chunky cyan -> electric blue Episode Energy bars with juice, outlined bold text
	using namespace ZoogiUIStyle;
	const float X = 20.f;
	float Y = Canvas->ClipY * 0.55f;
	const FSlateFontInfo FTitle = CanvasFont(10), FRow = CanvasFont(8, 1.5f), FBig = CanvasFont(24, 3.f);
	auto Txt = [Canvas](const FString& S, float TX, float TY, const FSlateFontInfo& F, const FLinearColor& Col) { CText(Canvas, S, TX, TY, F, Col); };

	const int32 N = G ? G->Balls.Num() : (SM ? SM->Players.Num() : 0);
	CPanel(Canvas, X - 12.f, Y - 12.f, 316.f, 50.f + N * 40.f, 1.f, PanelNavy, EnergyA);
	if (!SM) { Txt(TEXT("DEBUG UI: no battle state machine"), X, Y, FTitle, Gold); return; }

	Txt(FString::Printf(TEXT("TURN STATE: %s"), ZoogiBattle::StateName(SM->GetState())), X, Y, FTitle, FLinearColor::White);
	Y += 28.f;

	const float Cap = FMath::Max(1.f, SM->GetTuning().EnergyCap);
	for (int32 i = 0; i < N; ++i)
	{
		const float E = SM->GetEnergy(i);
		const FString Name = SM->Players.IsValidIndex(i) ? SM->Players[i].Name : FString::Printf(TEXT("Zoogi %d"), i + 1);
		Txt(FString::Printf(TEXT("P%d %s  %3.0f / %.0f"), i + 1, *Name, E, Cap), X, Y, FRow, FLinearColor(0.85f, 0.97f, 1.f));
		Y += 19.f;
		const float BW = 250.f, BH = 12.f;
		CBar(Canvas, X + 2.f, Y, BW, BH, 1.f, FMath::Clamp(E / Cap, 0.f, 1.f), EnergyA, EnergyB, EnergyJuice.IsValidIndex(i) ? &EnergyJuice[i] : nullptr, 4, (float)RealClock);
		Y += BH + 9.f;
	}

	if (RealClock < ShatterFlashUntil && FMath::Fmod(RealClock, 0.2) < 0.12)
	{
		const float W = Canvas->ClipX;
		CPanel(Canvas, W * 0.5f - 160.f, Canvas->ClipY * 0.2f - 12.f, 320.f, 64.f, 1.f, FLinearColor(0.55f, 0.02f, 0.12f, 0.85f), Gold);
		Txt(TEXT("SHATTER"), W * 0.5f - 80.f, Canvas->ClipY * 0.2f - 4.f, FBig, FLinearColor(1.f, 0.92f, 0.3f));
	}

	if (!bLoggedFirstDraw) { bLoggedFirstDraw = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI_DEBUGUI first draw players=%d cap=%.0f"), N, Cap); }
}
