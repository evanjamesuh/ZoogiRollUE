// b238: the round award banner (option 1, hanging stadium banner) - see ZoogiAwardBanner.h
#include "ZoogiAwardBanner.h"
#include "ZoogiWallCine.h" // b239
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiFilm.h"
#include "ZoogiP5Style.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiAwardBanner
{
	namespace
	{
		struct FState
		{
			TWeakObjectPtr<AZoogiGameMode> G; int32 LastRound = 0;
			int32 KO0[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, Sc0[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			double ShowAt = -100.0; FString Title, Name, Stat; FLinearColor Col = FLinearColor::White;
		};
		FState S;
		const float Dur = 3.8f;
		void Sync(AZoogiGameMode* G)
		{
			if (S.G.Get() != G || G->RoundNumber < S.LastRound) { const FState Fresh; S = Fresh; S.G = G; }
			S.LastRound = G->RoundNumber;
		}
		void Quad(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& Cc, const FVector2D& D, const FLinearColor& Col)
		{
			const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
			TArray<FCanvasUVTri> Tris; FCanvasUVTri T1, T2;
			T1.V0_Pos = A; T1.V1_Pos = B; T1.V2_Pos = Cc; T2.V0_Pos = A; T2.V1_Pos = Cc; T2.V2_Pos = D;
			T1.V0_Color = T1.V1_Color = T1.V2_Color = Col; T2.V0_Color = T2.V1_Color = T2.V2_Color = Col;
			Tris.Add(T1); Tris.Add(T2);
			FCanvasTriangleItem TI(Tris, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
		}
		void Disc(UCanvas* C, const FVector2D& Ctr, float R, const FLinearColor& Col)
		{
			const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
			const int32 N = 40; TArray<FCanvasUVTri> Tris; Tris.Reserve(N);
			for (int32 i = 0; i < N; ++i)
			{
				const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
				FCanvasUVTri T; T.V0_Pos = Ctr; T.V1_Pos = Ctr + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R; T.V2_Pos = Ctr + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R;
				T.V0_Color = T.V1_Color = T.V2_Color = Col; Tris.Add(T);
			}
			FCanvasTriangleItem TI(Tris, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
		}
	}

	void OnRoundEnd(AZoogiGameMode* G)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAwardBanner"));
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAwardBannerTest"));
		if (bOff || !G) return;
		Sync(G);
		int32 BestKO = -1, BestSc = -1;
		for (int32 i = 0; i < G->Balls.Num() && i < 8; ++i)
		{
			const AZoogiBall* B = G->Balls[i]; if (!B) continue;
			const int32 dK = B->KnockOuts - S.KO0[i], dS = B->Score - S.Sc0[i];
			if (dK > 0 && (BestKO < 0 || dK > G->Balls[BestKO]->KnockOuts - S.KO0[BestKO] || (dK == G->Balls[BestKO]->KnockOuts - S.KO0[BestKO] && dS > G->Balls[BestKO]->Score - S.Sc0[BestKO]))) BestKO = i;
			if (dS > 0 && (BestSc < 0 || dS > G->Balls[BestSc]->Score - S.Sc0[BestSc])) BestSc = i;
		}
		int32 Who = BestKO >= 0 ? BestKO : BestSc;
		if (Who < 0 && bTest) Who = 0;
		if (Who >= 0)
		{
			const AZoogiBall* B = G->Balls[Who];
			const int32 dK = B->KnockOuts - S.KO0[Who], dS = B->Score - S.Sc0[Who];
			if (BestKO >= 0) { S.Title = TEXT("KNOCKOUT KING"); S.Stat = FString::Printf(TEXT("%d KNOCKOUT%s"), dK, dK == 1 ? TEXT("") : TEXT("S")); }
			else { S.Title = TEXT("ORB HUNTER"); S.Stat = FString::Printf(TEXT("+%d POINTS"), FMath::Max(0, dS)); }
			S.Name = B->Def.Name.ToUpper(); S.Col = ZoogiUtil::PlayerColor(B->PlayerIndex);
			S.ShowAt = FPlatformTime::Seconds() + 0.7; // b239: after the turn change (a queued wall-smash cinematic goes first)
			ZoogiFilm::Begin(TEXT("award_banner"), Dur + 0.6f);
			UE_LOG(LogZoogi, Log, TEXT("AWARD BANNER round %d: %s - %s (P%d) %s"), G->RoundNumber - 1, *S.Title, *S.Name, B->PlayerIndex + 1, *S.Stat);
		}
		for (int32 i = 0; i < G->Balls.Num() && i < 8; ++i) if (const AZoogiBall* B = G->Balls[i]) { S.KO0[i] = B->KnockOuts; S.Sc0[i] = B->Score; }
	}

	bool Showing() { const double t = FPlatformTime::Seconds() - S.ShowAt; return S.G.IsValid() && t >= 0.0 && t <= Dur; }

	void Draw(AZoogiGameMode* G, UCanvas* C, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text)
	{
		if (!G || !C || S.G.Get() != G) return;
		if (ZoogiWallCine::Active()) { if (FPlatformTime::Seconds() - S.ShowAt < Dur) S.ShowAt = FPlatformTime::Seconds() + 0.3; return; } // b239: the banner waits for the cinematic
		const float t = (float)(FPlatformTime::Seconds() - S.ShowAt);
		if (t < 0.f || t > Dur) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float BW = FMath::Min(430.f * U, Wd * 0.62f), K = BW / 430.f, BH = 440.f * K, Tail = 64.f * K;
		// drop in (overshoot), hang, lift out
		auto Back = [](float x) { x = FMath::Clamp(x, 0.f, 1.f); const float c1 = 1.70158f, c3 = c1 + 1.f; return 1.f + c3 * FMath::Pow(x - 1.f, 3.f) + c1 * FMath::Pow(x - 1.f, 2.f); };
		const float In = Back(t / 0.55f), Out = FMath::Clamp((t - (Dur - 0.4f)) / 0.4f, 0.f, 1.f);
		const float Y0 = -(BH + Tail + 40.f * K) * (1.f - In) - (BH + Tail + 40.f * K) * Out * Out;
		const float Ang = FMath::DegreesToRadians(2.6f) * FMath::Sin(t * 2.3f) * FMath::Clamp(1.2f - t * 0.18f, 0.35f, 1.f);
		const FVector2D Piv(Wd * 0.5f, Y0);
		auto P = [&](float x, float y) { const float cs = FMath::Cos(Ang), sn = FMath::Sin(Ang); return Piv + FVector2D(x * cs - y * sn, x * sn + y * cs); }; // banner space: x from the centre, y down from the rod
		const FLinearColor Purple(0.33f, 0.10f, 0.62f, 1.f), Dark(0.20f, 0.05f, 0.42f, 1.f), Gold(1.f, 0.78f, 0.2f, 1.f), Shade(0.f, 0.f, 0.05f, 0.35f);
		const float hw = BW * 0.5f;
		// soft shadow, body, swallowtail
		Quad(C, P(-hw + 10.f * K, 10.f * K), P(hw + 10.f * K, 10.f * K), P(hw + 10.f * K, BH + 10.f * K), P(-hw + 10.f * K, BH + 10.f * K), Shade);
		Quad(C, P(-hw, 0.f), P(hw, 0.f), P(hw, BH), P(-hw, BH), Purple);
		Quad(C, P(-hw, BH), P(0.f, BH), P(0.f, BH), P(-hw, BH + Tail), Purple);
		Quad(C, P(0.f, BH), P(hw, BH), P(hw, BH + Tail), P(0.f, BH), Purple);
		Quad(C, P(-hw, 0.f), P(hw, 0.f), P(hw, 96.f * K), P(-hw, 96.f * K), Dark); // title band
		// gold trim down both sides + the rod it hangs from
		for (int32 s = -1; s <= 1; s += 2)
		{
			const float x0 = s * (hw - 20.f * K), x1 = s * (hw - 10.f * K), yb0 = BH + Tail * (1.f - 20.f / 215.f), yb1 = BH + Tail * (1.f - 10.f / 215.f);
			Quad(C, P(x0, 0.f), P(x1, 0.f), P(x1, yb1), P(x0, yb0), Gold);
		}
		Quad(C, P(-hw - 16.f * K, -8.f * K), P(hw + 16.f * K, -8.f * K), P(hw + 16.f * K, 8.f * K), P(-hw - 16.f * K, 8.f * K), Gold);
		// portrait medallion
		const FVector2D Mc = P(0.f, 200.f * K); const float R = 84.f * K;
		Disc(C, Mc, R + 8.f * K, Gold); Disc(C, Mc, R, FLinearColor(0.96f, 0.96f, 1.f, 1.f)); Disc(C, Mc, R - 5.f * K, FLinearColor(S.Col.R * 0.55f, S.Col.G * 0.55f, S.Col.B * 0.55f, 1.f));
		if (UTexture2D* Tx = ZoogiP5::Portrait(S.Name))
		{
			const float Sd = (R - 5.f * K) * 1.36f;
			FCanvasTileItem Ic(Mc - FVector2D(Sd, Sd) * 0.5f, Tx->GetResource(), FVector2D(Sd, Sd), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic);
		}
		const FVector2D Tt = P(0.f, 50.f * K), Tn = P(0.f, 330.f * K), Ts = P(0.f, 392.f * K);
		Text(S.Title, Tt.X, Tt.Y, 50.f * K, Gold);
		Text(S.Name, Tn.X, Tn.Y, 62.f * K, FLinearColor::White);
		Text(S.Stat, Ts.X, Ts.Y, 32.f * K, Gold);
	}
}
