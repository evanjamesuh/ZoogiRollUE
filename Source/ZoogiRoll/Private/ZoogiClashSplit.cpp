// Nov 2 b68 - see ZoogiClashSplit.h.
#include "ZoogiClashSplit.h"
#include "ZoogiActionFun.h"
#include "Engine/Texture2D.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiArtifacts.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
	struct FSide
	{
		TStrongObjectPtr<UMediaPlayer> Player;
		TStrongObjectPtr<UMediaTexture> Tex;
		TStrongObjectPtr<UFileMediaSource> Src;
		FString Clip;
		TWeakObjectPtr<UTexture2D> Art; // b81: the Zoogi's own ultimate art over its half
	};
	FSide GSplitSides[2];
	bool bSplitOn = false;

	FString SplitClipPath(const FString& Id) { return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Id + TEXT(".mp4")); }

	/** The awakening clip for a Zoogi: its element's own Infuse clip first, then the shared awaken / infuse clips (never the other side's). */
	FString SplitClipFor(const AZoogiBall* B, const FString& Avoid)
	{
		TArray<FString> Pool;
		if (B) Pool.Add(FString::Printf(TEXT("Anime_Zoogi_%s_Clash_01"), *B->Def.Name.Replace(TEXT(" "), TEXT("")))); // b81: a clip made for this Zoogi wins when it exists
		if (B)
		{
			const EZoogiArtifact Af = ZoogiArtifacts::Affinity(B->Def.Id);
			const FString El = ZoogiArtifacts::Name(Af).Replace(TEXT(" "), TEXT(""));
			if (!El.IsEmpty()) Pool.Add(FString::Printf(TEXT("Anime_Any_%s_Infuse_01"), *El));
		}
		Pool.Append({ TEXT("Anime_Any_Any_Infuse_01"), TEXT("Anime_Awaken_WolfPack_01"), TEXT("Anime_Awaken_BoltBalls_01"), TEXT("Anime_Awaken_MeteorStrike_01"),
			TEXT("Anime_Any_Ember_Infuse_01"), TEXT("Anime_Any_Titan_Infuse_01") });
		for (const FString& Id : Pool) if (Id != Avoid && FPaths::FileExists(SplitClipPath(Id))) return Id;
		return FString();
	}

	void SplitOpen(FSide& S, const FString& Id)
	{
		S.Clip = Id;
		if (Id.IsEmpty()) return;
		if (!S.Player.IsValid())
		{
			S.Player.Reset(NewObject<UMediaPlayer>(GetTransientPackage()));
			S.Player->PlayOnOpen = true;
			S.Tex.Reset(NewObject<UMediaTexture>(GetTransientPackage()));
			S.Tex->AutoClear = true;
			S.Tex->SetMediaPlayer(S.Player.Get());
			S.Tex->UpdateResource();
			S.Src.Reset(NewObject<UFileMediaSource>(GetTransientPackage()));
		}
		S.Player->SetLooping(true);
		S.Src->SetFilePath(SplitClipPath(Id));
		S.Player->OpenSource(S.Src.Get());
	}
}

namespace ZoogiClashSplit
{
	bool Enabled() { static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoClashSplit")); return !bOff; }
	bool On() { return bSplitOn; }

	void Begin(UWorld* W, const AZoogiBall* Left, const AZoogiBall* Right)
	{
		if (!Enabled()) return;
		const FString L = SplitClipFor(Left, FString());
		const FString R = SplitClipFor(Right, L);
		SplitOpen(GSplitSides[0], L);
		SplitOpen(GSplitSides[1], R);
		// b81 (Evan: "for the clash the opposing player needs to be generated, not the ball-class anime clip"): each half shows that
		// Zoogi's own ultimate art, big, over its (dimmed) clip. -ZoogiClashClipsOnly = clips alone
		static const bool bClipsOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashClipsOnly"));
		UZoogiFunSys* Fs = UZoogiFunSys::Get(W);
		GSplitSides[0].Art = (!bClipsOnly && Fs && Left) ? Fs->UltAvatar(Left) : nullptr;
		GSplitSides[1].Art = (!bClipsOnly && Fs && Right) ? Fs->UltAvatar(Right) : nullptr;
		bSplitOn = !L.IsEmpty() || !R.IsEmpty() || GSplitSides[0].Art.IsValid() || GSplitSides[1].Art.IsValid();
		UE_LOG(LogZoogi, Log, TEXT("CLASH SPLIT on: left %s (%s) | right %s (%s)"), Left ? *Left->Def.Name : TEXT("?"), *L, Right ? *Right->Def.Name : TEXT("?"), *R);
	}

	void End()
	{
		if (!bSplitOn) return;
		bSplitOn = false;
		for (FSide& S : GSplitSides) if (S.Player.IsValid()) S.Player->Close();
		UE_LOG(LogZoogi, Log, TEXT("CLASH SPLIT off"));
	}

	void Draw(UCanvas* C, float Split, float Alpha, float T, const FLinearColor& LCol, const FLinearColor& RCol)
	{
		if (!bSplitOn || !C || Alpha <= 0.f) return;
		const float W = C->ClipX, H = C->ClipY, U = FMath::Min(W, H) / 1080.f;
		const float VidA = 16.f / 9.f;
		const float VW = FMath::Max(W, H * VidA), VH = VW / VidA;   // each clip fills the screen (aspect kept), framed on its own side
		const float Xs = FMath::Clamp(Split, 0.f, 1.f) * W;
		const float Cx[2] = { Xs * 0.5f, (Xs + W) * 0.5f };
		// the tear: a jagged line that crackles (two travelling waves + a per-row flicker), wider while the tug is close
		const int32 N = 36;
		const float Close = 1.f - FMath::Abs(Split - 0.5f) * 2.f;
		auto SeamX = [&](float Y)
		{
			const float Amp = (14.f + 22.f * Close) * U;
			const float Fl = FMath::Frac(FMath::Sin(FMath::FloorToFloat(Y / (H / N)) * 12.9898f + FMath::FloorToFloat(T * 24.f) * 78.233f) * 43758.547f) - 0.5f;
			return Xs + Amp * (FMath::Sin(Y * 0.021f + T * 9.f) + 0.6f * FMath::Sin(Y * 0.057f - T * 17.f)) + Fl * Amp * 0.9f;
		};
		TArray<FVector2D> Seam; Seam.Reserve(N + 1);
		for (int32 i = 0; i <= N; ++i) { const float Y = H * i / N; Seam.Add(FVector2D(FMath::Clamp(SeamX(Y), -40.f * U, W + 40.f * U), Y)); }
		const FLinearColor Wt(1.f, 1.f, 1.f, Alpha);
		for (int32 s = 0; s < 2; ++s)
		{
			const FSide& Sd = GSplitSides[s];
			const FTexture* Res = (Sd.Tex.IsValid() && Sd.Tex->GetResource() && !Sd.Clip.IsEmpty()) ? Sd.Tex->GetResource() : nullptr;
			UTexture2D* Art = Sd.Art.Get();
			const FTexture* ARes = Art ? Art->GetResource() : nullptr;
			if (!Res && !ARes) continue;
			const FLinearColor Ec = s == 0 ? LCol : RCol;
			const float Zm = 1.f + 0.05f * FMath::Sin(T * 1.7f + s * 1.3f);
			const float As = FMath::Min(H * 0.8f, (s == 0 ? FMath::Max(Xs, W * 0.3f) : FMath::Max(W - Xs, W * 0.3f)) * 0.95f) * Zm;
			const FVector2D AC(Cx[s] + 6.f * U * FMath::Sin(T * 23.f + s * 2.f), H * 0.54f + 5.f * U * FMath::Sin(T * 19.f + s));
			const float AX0 = AC.X - As * 0.5f, AY0 = AC.Y - As * 0.5f;
			for (int32 i = 0; i < N; ++i)
			{
				const float Y0 = H * i / N, Y1 = H * (i + 1) / N;
				const float Xe = (Seam[i].X + Seam[i + 1].X) * 0.5f;      // this row's edge
				const float X0 = s == 0 ? 0.f : FMath::Max(Xe, 0.f), X1 = s == 0 ? FMath::Min(Xe, W) : W;
				if (X1 - X0 < 1.f) continue;
				if (Res)
				{
					const FVector2D UV0(0.5f + (X0 - Cx[s]) / VW, 0.5f + (Y0 - H * 0.5f) / VH), UV1(0.5f + (X1 - Cx[s]) / VW, 0.5f + (Y1 - H * 0.5f) / VH);
					FCanvasTileItem Tile(FVector2D(X0, Y0), Res, FVector2D(X1 - X0, Y1 - Y0 + 0.5f), UV0, UV1, Wt);
					Tile.BlendMode = SE_BLEND_Translucent;
					C->DrawItem(Tile);
				}
				if (!ARes) continue;
				// the clip goes dim and takes the Zoogi's colour; the Zoogi's own art fills its half
				FCanvasTileItem Dim(FVector2D(X0, Y0), FVector2D(X1 - X0, Y1 - Y0 + 0.5f), FLinearColor(Ec.R * 0.22f, Ec.G * 0.22f, Ec.B * 0.22f, (Res ? 0.55f : 0.95f) * Alpha));
				Dim.BlendMode = SE_BLEND_Translucent; C->DrawItem(Dim);
				const float Ix0 = FMath::Max(X0, AX0), Ix1 = FMath::Min(X1, AX0 + As), Iy0 = FMath::Max(Y0, AY0), Iy1 = FMath::Min(Y1 + 0.5f, AY0 + As);
				if (Ix1 - Ix0 < 1.f || Iy1 - Iy0 < 0.5f) continue;
				FCanvasTileItem ArtT(FVector2D(Ix0, Iy0), ARes, FVector2D(Ix1 - Ix0, Iy1 - Iy0), FVector2D((Ix0 - AX0) / As, (Iy0 - AY0) / As), FVector2D((Ix1 - AX0) / As, (Iy1 - AY0) / As), Wt);
				ArtT.BlendMode = SE_BLEND_Translucent;
				C->DrawItem(ArtT);
			}
		}
		// the tear's glow: wide colour halos from both sides, a hot white core, sparks flying off it
		auto Line = [&](float Th, const FLinearColor& Col, float Dx)
		{
			for (int32 i = 0; i < N; ++i)
			{
				FCanvasLineItem L(Seam[i] + FVector2D(Dx, 0.f), Seam[i + 1] + FVector2D(Dx, 0.f));
				L.LineThickness = Th; L.SetColor(Col); L.BlendMode = SE_BLEND_Additive; C->DrawItem(L);
			}
		};
		Line(26.f * U, FLinearColor(LCol.R, LCol.G, LCol.B, 0.35f * Alpha), -8.f * U);
		Line(26.f * U, FLinearColor(RCol.R, RCol.G, RCol.B, 0.35f * Alpha), 8.f * U);
		Line(10.f * U, FLinearColor(1.f, 0.95f, 0.85f, 0.85f * Alpha), 0.f);
		Line(3.f * U, FLinearColor(1.f, 1.f, 1.f, Alpha), 0.f);
		for (int32 k = 0; k < 14; ++k)
		{
			const float Sd = FMath::Frac(FMath::Sin((k + 1) * 91.17f + FMath::FloorToFloat(T * 30.f) * 3.3f) * 43758.547f);
			const int32 Row = FMath::Clamp((int32)(Sd * N), 0, N);
			const FVector2D P = Seam[Row] + FVector2D((Sd - 0.5f) * 120.f * U, (FMath::Frac(Sd * 7.31f) - 0.5f) * 40.f * U);
			const float Sz = (4.f + 10.f * FMath::Frac(Sd * 3.7f)) * U;
			FCanvasTileItem Sp(P - FVector2D(Sz, Sz) * 0.5f, FVector2D(Sz, Sz), (k & 1) ? FLinearColor(LCol.R, LCol.G, LCol.B, Alpha) : FLinearColor(RCol.R, RCol.G, RCol.B, Alpha));
			Sp.BlendMode = SE_BLEND_Additive; C->DrawItem(Sp);
		}
	}
}
