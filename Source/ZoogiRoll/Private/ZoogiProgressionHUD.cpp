// Results-screen progression badge (Oct 14, branch `progression`): equipped title + equipped animated border (cosmetic,
// visible), gem balance and today's match gems earned / remaining. Canvas only; no game state touched.
#include "ZoogiHUD.h"
#include "ZoogiGameMode.h"
#include "ZoogiProgressionSubsystem.h"
#include "Engine/Canvas.h"

void AZoogiHUD::DrawProgressionBadge(AZoogiGameMode* G)
{
	UZoogiProgressionSubsystem* S = UZoogiProgressionSubsystem::Get(this);
	if (!S || !Canvas) return;
	const FZoogiProgression& P = S->Core();
	const float W = Canvas->ClipX;
	const float BW = 520.f * U, BH = 112.f * U, X = W - BW - 24.f * U, Y = 24.f * U;
	const FName Border = P.EquippedCosmetic(TEXT("animated_border"));
	FLinearColor Edge(0.45f, 0.55f, 0.7f, 0.9f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 3.f);
	if (Border == FName(TEXT("border_gold"))) Edge = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.75f, 0.2f), FLinearColor(1.f, 0.95f, 0.6f), Pulse);
	else if (Border == FName(TEXT("border_awaken"))) Edge = FLinearColor::LerpUsingHSV(FLinearColor(0.55f, 0.2f, 1.f), FLinearColor(0.2f, 0.9f, 1.f), Pulse);
	const float T = 4.f * U;
	Panel(X - T, Y - T, BW + 2 * T, BH + 2 * T, Edge);
	Panel(X, Y, BW, BH, FLinearColor(0.02f, 0.03f, 0.08f, 0.92f));
	const FString Title = P.TitleName(P.Get().EquippedTitle);
	Text(Title.IsEmpty() ? FString(TEXT("NO TITLE YET")) : Title.ToUpper(), X + 20.f * U, Y + 12.f * U, Edge, 0.6f);
	Text(FString::Printf(TEXT("GEMS %d   (earned %d / purchased %d)"), P.Get().Wallet.Total(), P.Get().Wallet.Earned(), P.Get().Wallet.Purchased()), X + 20.f * U, Y + 46.f * U, FLinearColor(0.85f, 0.9f, 1.f), 0.45f, false, false);
	const FZoogiMatchRewardResult& L = S->LastResult();
	const FString Line = L.ZeroReason.IsEmpty() ? FString::Printf(TEXT("+%d match gems   today %d / %d   remaining %d"), L.GemsPaid, P.EarnedToday(), P.Config().DailyMatchGemCap, P.RemainingToday())
		: FString::Printf(TEXT("no rewards (%s)   today %d / %d"), *L.ZeroReason, P.EarnedToday(), P.Config().DailyMatchGemCap);
	Text(Line, X + 20.f * U, Y + 76.f * U, FLinearColor(0.7f, 1.f, 0.75f), 0.45f, false, false);
}
