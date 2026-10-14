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
}

void UZoogiDebugEpisodeWidget::OnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (!Canvas || !HUD || HUD->GetWorld() != GetWorld() || !GEngine) return;
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	UFont* Font = GEngine->GetSmallFont();
	const float X = 20.f;
	float Y = Canvas->ClipY * 0.55f;

	auto Box = [Canvas](float BX, float BY, float BW, float BH, const FLinearColor& Col)
	{
		FCanvasTileItem T(FVector2D(BX, BY), FVector2D(BW, BH), Col);
		T.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(T);
	};
	auto Txt = [Canvas, Font](const FString& S, float TX, float TY, const FLinearColor& Col, float Scale = 1.f)
	{
		FCanvasTextItem T(FVector2D(TX, TY), FText::FromString(S), Font, Col);
		T.Scale = FVector2D(Scale, Scale);
		T.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(T);
	};

	Box(X - 8.f, Y - 8.f, 300.f, 150.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	if (!SM) { Txt(TEXT("DEBUG UI: no battle state machine"), X, Y, FLinearColor::Yellow); return; }

	Txt(FString::Printf(TEXT("TURN STATE: %s"), ZoogiBattle::StateName(SM->GetState())), X, Y, FLinearColor::White, 1.2f);
	Y += 24.f;

	const float Cap = FMath::Max(1.f, SM->GetTuning().EnergyCap);
	const int32 N = G ? G->Balls.Num() : SM->Players.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const float E = SM->GetEnergy(i);
		const FString Name = SM->Players.IsValidIndex(i) ? SM->Players[i].Name : FString::Printf(TEXT("Zoogi %d"), i + 1);
		Txt(FString::Printf(TEXT("P%d %s  %3.0f / %.0f"), i + 1, *Name, E, Cap), X, Y, FLinearColor(0.85f, 0.95f, 1.f));
		Y += 15.f;
		const float BW = 260.f, BH = 8.f;
		Box(X, Y, BW, BH, FLinearColor(0.15f, 0.15f, 0.2f, 0.9f));
		Box(X, Y, BW * FMath::Clamp(E / Cap, 0.f, 1.f), BH, FLinearColor(0.2f, 0.75f, 1.f, 0.95f));
		Y += BH + 8.f;
	}

	if (RealClock < ShatterFlashUntil && FMath::Fmod(RealClock, 0.2) < 0.12)
	{
		const float W = Canvas->ClipX;
		Box(W * 0.5f - 160.f, Canvas->ClipY * 0.2f - 10.f, 320.f, 60.f, FLinearColor(0.6f, 0.f, 0.f, 0.6f));
		Txt(TEXT("SHATTER"), W * 0.5f - 70.f, Canvas->ClipY * 0.2f, FLinearColor(1.f, 0.2f, 0.2f), 2.5f);
	}

	if (!bLoggedFirstDraw) { bLoggedFirstDraw = true; UE_LOG(LogZoogi, Log, TEXT("ZOOGI_DEBUGUI first draw players=%d cap=%.0f"), N, Cap); }
}
