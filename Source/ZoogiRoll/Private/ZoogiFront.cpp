// Nov 1 b52: the new front end (see ZoogiFront.h). Straight P5 look over the live arena, Marble Clash layout.
#include "ZoogiFront.h"
#include "ZoogiPlayerController.h" // b187
#include "ZoogiSkills.h"
#include "ZoogiBall.h"
#include "ZoogiWolfPack.h"
#include "ZoogiMenuRoot.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiMorph.h"
#include "ZoogiP5Style.h"
#include "ZoogiUIFx.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "ZoogiApplePad.h"
#include "ZoogiScoringUI.h"
#include "Misc/ConfigCacheIni.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ZoogiHaptics.h"
#include "ZoogiTrickShot.h"
#include "ZoogiTutorial.h" // b211
#include "Engine/StaticMesh.h"

// ================================================================================================ data
namespace ZoogiFront
{
	bool On()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFront")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMenu"))
			|| FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicMenu"));
		return !bOff;
	}
	const TArray<FHero>& Heroes()
	{
		// rarity 0 common (green) .. 3 legendary (gold), Marble Clash colours
		static const TArray<FHero> H = {
			{ TEXT("Blue"),      TEXT("blue"),      TEXT("Blue"),      TEXT("WATER"), 1, TEXT("Rolls as the deep-sea wave ball and stands up as a navy Aurex. Steady, strong, never panics.") },
			{ TEXT("Frosti"),    TEXT("pinpoint"),  TEXT("Frosti"),    TEXT("ICE"),   2, TEXT("The snowflake ball. Ice armour and freezing shots: defense and control.") },
			{ TEXT("Lumi"),      TEXT("luma"),      TEXT("Lumi"),      TEXT("LIGHT"), 2, TEXT("The sun ball. A gold and white Aurex that heals and shields the team.") },
			{ TEXT("Vex"),       TEXT("vex"),       TEXT("Vex"),       TEXT("VOID"),  3, TEXT("The violet storm ball. A void-purple Aurex built for speed and offense.") },
			{ TEXT("Hotstreak"), TEXT("hotstreak"), TEXT("Hotstreak"), TEXT("FIRE"),  3, TEXT("The lava ball. A black and orange Aurex: burst damage, all gas.") },
			{ TEXT("Terra"),     TEXT("moss"),      TEXT("Terra"),     TEXT("EARTH"), 0, TEXT("The mountain ball. A moss-green Aurex that holds the middle and will not move.") },
			{ TEXT("Zephyr"),    TEXT("wraps"),     TEXT("Zephyr"),    TEXT("WIND"),  1, TEXT("The wind ball. A teal Aurex that dashes, dodges and slips through gaps.") },
			{ TEXT("Ember"),     TEXT("sera"),      TEXT("Ember"),     TEXT("HEART"), 1, TEXT("The pink heart ball. A rose Aurex with a big heart and a bigger punch.") },
			// b202 (Evan: "the new dark zoogi - put it in the game, in the inventory"): his Tripo dark warrior
			{ TEXT("Dark"),      TEXT("darkzoogy"), TEXT("Dark"),      TEXT("SHADOW"), 3, TEXT("The shadow-crystal ball. A navy Aurex in black armour with glowing orange crystal spikes: heavy hits and crystal bursts.") },
		};
		return H;
	}
	const FAwaken* AwakenForBall(const FString& Ball)
	{
		static const FAwaken AwTable[] = {
			{ TEXT("Hotstreak"), TEXT("METEOR STRIKE"), TEXT(""), TEXT("A huge fire blast where you roll, then a meteor shower rains across the whole arena.") },
			{ TEXT("Vex"), TEXT("VOID PACK"), TEXT("wolf_pack"), TEXT("Three hologram wolves at full power: time slows, pick 3 targets and they hunt them down.") },
			{ TEXT("Blue"), TEXT("TIDAL CANNON"), TEXT("water_cannon"), TEXT("A full-power water jet blasts the nearest rival or orb straight away from you.") },
			{ TEXT("Frosti"), TEXT("GLACIER WALL"), TEXT("ice_wall"), TEXT("A solid wall of ice rises across the arena - it cracks and bursts into ice blocks.") },
			{ TEXT("Lumi"), TEXT("SOLAR BOLTS"), TEXT("bolt_balls"), TEXT("Your ball crackles with sun lightning - every redirect shocks the 3 nearest away.") },
			{ TEXT("Terra"), TEXT("IRONWOOD DOME"), TEXT("wood_shield"), TEXT("An unbreakable wooden dome: you will not move and everything bounces off you.") },
			{ TEXT("Zephyr"), TEXT("CYCLONE PULSE"), TEXT("energy_shockwave"), TEXT("A cyclone sucks the orbs together, then blows them out at double wolf force.") },
			{ TEXT("Ember"), TEXT("HEARTBREAKER"), TEXT("meteor_rain"), TEXT("A void portal tears open and drops a boulder of purple lightning on your target.") },
			{ TEXT("Dark"), TEXT("CRYSTAL BARRAGE"), TEXT("gattling_burst"), TEXT("Six shadow-crystal bolts burst out of you at everything in reach - in a boss round every other one locks onto the boss.") }, // b202
		};
		for (const FAwaken& Ent : AwTable) if (Ball.Equals(Ent.Ball, ESearchCase::IgnoreCase)) return &Ent;
		return nullptr;
	}
	const FAwaken* AwakenFor(const AZoogiBall* B, const UObject* Ctx)
	{
		if (!B) return nullptr;
		if (!B->bCPU)
		{
			FString FilmBall; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenBall="), FilmBall)) if (const FAwaken* Fb = AwakenForBall(FilmBall)) return Fb; // b101 film
			const UZoogiFrontProfileSubsystem* Fp = UZoogiFrontProfileSubsystem::Get(Ctx);
			if (Fp && !Fp->Data().EquippedBall.IsNone()) if (const FAwaken* Eq = AwakenForBall(Fp->Data().EquippedBall.ToString())) return Eq;
		}
		for (const FHero& H : Heroes()) if (B->Def.Id == FName(H.RosterId)) return AwakenForBall(H.Ball);
		return nullptr;
	}
	const FHero* HeroByName(const FString& Name)
	{
		for (const FHero& H : Heroes()) if (Name.Equals(H.Name, ESearchCase::IgnoreCase)) return &H;
		return nullptr;
	}
	const FHero& Equipped(const UObject* Ctx)
	{
		const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(Ctx);
		const FHero* H = FP ? HeroByName(FP->Data().EquippedZoogi.ToString()) : nullptr;
		return H ? *H : Heroes()[0];
	}
	void ApplyEquipped(UObject* Ctx)
	{
		AZoogiGameMode* G = Ctx ? Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(Ctx)) : nullptr;
		if (!G || !G->MenuPlayers.Num()) return;
		const FHero& H = Equipped(Ctx);
		const TArray<FZoogiDef>& R = ZoogiUtil::Roster();
		for (int32 i = 0; i < R.Num(); ++i)
			if (R[i].Id == FName(H.RosterId)) { G->MenuPlayers[0].ZoogiIndex = i; break; }
	}
	FView& View() { static FView V; return V; }
}

// ================================================================================================ drawing kit
namespace ZoogiFrontDraw
{
	using ZoogiP5::Font;
	static const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(8, 6, 10));
	static const FLinearColor Paper = FLinearColor::White;
	static FLinearColor Mag() { return ZoogiP5::Accent(); }
	static bool ZInvRobot() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiInvRobot")); return b; } // b179 (Evan: "remove model from inventory"): -ZoogiInvRobot = the robot back
	static bool ZHomeB() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldHome")); return !bOld; } // b209 (Evan: "Home B"): hero + LOADOUT card on the home screen
	static bool ZNewSkillsUi() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkillGrid")); return !bOld; } // b179: -ZoogiOldSkillGrid = the tile grid
	static const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(232, 32, 42));
	static const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(255, 198, 26));
	static const FLinearColor Green = FLinearColor::FromSRGBColor(FColor(60, 207, 90));
	static FLinearColor ZRarityCol(int32 R)
	{
		switch (R) { case 0: return FLinearColor::FromSRGBColor(FColor(92, 207, 74)); case 1: return FLinearColor::FromSRGBColor(FColor(58, 140, 240));
		case 2: return FLinearColor::FromSRGBColor(FColor(154, 77, 240)); default: return FLinearColor::FromSRGBColor(FColor(255, 178, 26)); }
	}
	static const TCHAR* ZRarityName(int32 R) { return R == 0 ? TEXT("COMMON") : R == 1 ? TEXT("RARE") : R == 2 ? TEXT("EPIC") : TEXT("LEGENDARY"); }

	static UTexture2D* ZMakeTex(int32 Size, TFunctionRef<float(float, float)> Coverage)
	{
		ZoogiUIFx::FPixels P; P.W = P.H = Size; P.Px.SetNumZeroed(Size * Size);
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				float A = 0.f;
				for (int32 s = 0; s < 4; ++s) A += Coverage(x + ((s % 2) + 0.5f) / 2.f, y + ((s / 2) + 0.5f) / 2.f);
				A *= 0.25f;
				if (A > 0.f) P.Px[y * Size + x] = FColor(255, 255, 255, (uint8)FMath::Clamp(A * 255.f, 0.f, 255.f));
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		if (T) T->AddToRoot();
		return T;
	}
	static bool ZInPoly(const TArray<FVector2D>& Poly, float X, float Y)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
			if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
		return bIn;
	}
	/** regular 12-point starburst (symmetric, level - Evan: no crookedness) */
	static UTexture2D* ZBurstTex()
	{
		static UTexture2D* T = nullptr;
		if (T) return T;
		TArray<FVector2D> Poly;
		for (int32 k = 0; k < 24; ++k)
		{
			const float A = UE_TWO_PI * k / 24.f - UE_HALF_PI;
			const float R = (k % 2 == 0) ? 126.f : 76.f;
			Poly.Add(FVector2D(128.f + FMath::Cos(A) * R, 128.f + FMath::Sin(A) * R));
		}
		T = ZMakeTex(256, [&Poly](float X, float Y) { return ZInPoly(Poly, X, Y) ? 1.f : 0.f; });
		return T;
	}
	static UTexture2D* ZDiscTex()
	{
		static UTexture2D* T = nullptr;
		if (!T) T = ZMakeTex(128, [](float X, float Y) { return FMath::Square(X - 64.f) + FMath::Square(Y - 64.f) < 62.f * 62.f ? 1.f : 0.f; });
		return T;
	}
	static UTexture2D* ZDotsTex()
	{
		static UTexture2D* T = nullptr;
		if (!T) T = ZMakeTex(128, [](float X, float Y) { const float Cx = FMath::Fmod(X, 16.f) - 8.f, Cy = FMath::Fmod(Y, 16.f) - 8.f; return FMath::Sqrt(Cx * Cx + Cy * Cy) < 3.6f ? 1.f : 0.f; });
		return T;
	}
	static UTexture2D* ZLoadPng(const FString& Rel)
	{
		static TMap<FString, UTexture2D*> Cache;
		if (UTexture2D** Hit = Cache.Find(Rel)) return *Hit;
		UTexture2D* T = nullptr;
		for (const TCHAR* E : { TEXT(".png"), TEXT(".jpg") })
		{
			const FString P = FPaths::ProjectContentDir() / (Rel + E);
			if (!T && FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
		}
		if (T) T->AddToRoot();
		else UE_LOG(LogZoogi, Warning, TEXT("FRONT art missing: %s"), *Rel);
		Cache.Add(Rel, T);
		return T;
	}
	/** b136 (Evan): old cards that have no way to be played in classic yet */
	static bool ZoogiCardLockedInClassic(FName Id) { return Id == FName(TEXT("bank_strike")) || Id == FName(TEXT("orb_relay")) || Id == FName(TEXT("recovery_roll")) || Id == FName(TEXT("fire_meteors")); } // b139a: fire meteors = Volcano event
	static UTexture2D* ZBallIcon(const FString& Ball) { return ZLoadPng(TEXT("Data/MenuArt/Front/ball_") + Ball.ToLower()); }
	/** b138 (Evan): the BALLERS tiles show the Zoogi itself (front view) when its art is in, else the ball */
	static UTexture2D* ZBallerArt(const FString& Ball)
	{
		static const bool bBalls = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBallerBalls"));
		if (!bBalls) if (UTexture2D* Z = ZLoadPng(TEXT("Data/MenuArt/Front/zoogi_") + Ball.ToLower())) return Z;
		return ZBallIcon(Ball);
	}

	static const FSlateBrush* ZWhite() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	// b136 (Evan: "shrink buttons when they get pressed"): the pressed button's box; everything drawn inside it squashes about its centre
	static FBox2D GPressBox(ForceInit); static double GPressAt = -10.0;
	static float ZPressK()
	{
		const float Tp = (float)(FPlatformTime::Seconds() - GPressAt);
		return (Tp < 0.f || Tp > 0.16f || !GPressBox.bIsValid) ? 1.f : 1.f - 0.1f * FMath::Sin(PI * Tp / 0.16f);
	}
	static bool ZPress(FVector2D& TL, FVector2D& Sz, float* OutK = nullptr)
	{
		const float K = ZPressK();
		if (K >= 0.999f) return false;
		const FBox2D Bx = GPressBox.ExpandBy(14.f);
		if (TL.X < Bx.Min.X || TL.Y < Bx.Min.Y || TL.X + Sz.X > Bx.Max.X || TL.Y + Sz.Y > Bx.Max.Y) return false; // only what sits inside the button
		const FVector2D C = GPressBox.GetCenter();
		TL = C + (TL - C) * K; Sz *= K;
		if (OutK) *OutK = K;
		return true;
	}
	static void ZRect(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& TL0, const FVector2D& Sz0, const FLinearColor& C, const FSlateBrush* B = nullptr)
	{
		FVector2D TL = TL0, Sz = Sz0; ZPress(TL, Sz);
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL)), B ? B : ZWhite(), ESlateDrawEffect::None, C);
	}
	static void ZTex(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Sz, const FLinearColor& C = FLinearColor::White)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		ZRect(Out, G, L, TL, Sz, C, &B);
	}
	/** fit a texture inside a box, keeping its aspect, centred */
	static void ZTexFit(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Box, const FLinearColor& C = FLinearColor::White)
	{
		if (!T) return;
		const float A = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY());
		FVector2D Sz = Box;
		if (Box.X / FMath::Max(1.f, Box.Y) > A) Sz.X = Box.Y * A; else Sz.Y = Box.X / A;
		ZTex(Out, G, L, T, TL + (Box - Sz) * 0.5f, Sz, C);
	}
	static void ZTiled(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Size, float Tile, const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(Tile, Tile); B.Tiling = ESlateBrushTileType::Both;
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Size, FSlateLayoutTransform(TL)), &B, ESlateDrawEffect::None, Col);
	}
	static FVector2D ZMeasure(const FString& S, const FSlateFontInfo& F) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F); }
	static void ZTextAt(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& TL0, const FLinearColor& C)
	{
		FVector2D TL = TL0, Sz = ZMeasure(S, F); float K = 1.f;
		if (ZPress(TL, Sz, &K)) { FSlateFontInfo Fk = F; Fk.Size = FMath::Max(1.f, F.Size * K); Fk.OutlineSettings.OutlineSize = FMath::RoundToInt(F.OutlineSettings.OutlineSize * K); FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(ZMeasure(S, Fk), FSlateLayoutTransform(TL)), S, Fk, ESlateDrawEffect::None, C); return; } // b136
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(ZMeasure(S, F), FSlateLayoutTransform(TL)), S, F, ESlateDrawEffect::None, C);
	}
	static void ZTextC(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& Ctr, const FLinearColor& C)
	{
		ZTextAt(Out, G, L, S, F, Ctr - ZMeasure(S, F) * 0.5f, C);
	}
	/** text shrunk to fit a width */
	static FSlateFontInfo ZFitFont(const FString& S, int32 Which, float Size, float MaxW, float Outline = 0.f)
	{
		FSlateFontInfo F = Font(Which, Size, Outline);
		const float W = ZMeasure(S, F).X;
		if (W > MaxW && W > 1.f) F = Font(Which, FMath::Max(10.f, Size * MaxW / W), Outline);
		return F;
	}
	/** P5 panel: hard offset shadow + white rim + body (all straight) */
	static void ZPanel(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& TL, const FVector2D& Sz, const FLinearColor& Body, const FLinearColor& Shadow, float Rim = 5.f, float Off = 10.f)
	{
		ZRect(Out, G, L, TL + FVector2D(Off, Off), Sz, Shadow);
		ZRect(Out, G, L + 1, TL - FVector2D(Rim, Rim), Sz + FVector2D(Rim * 2.f, Rim * 2.f), Body == Paper ? Ink : Paper);
		ZRect(Out, G, L + 2, TL, Sz, Body);
	}
	// ---- b238 (Evan Nov 11: "the new inventory and card style, why aren't those updated?"): the look of his Grok inventory
	// mockups (Nov 10) - gold chamfered card frames, navy name plates, magenta LV tags, slanted gold / magenta chips, neon panels.
	// Art: Content/Data/MenuArt/Inv (frames) + Data/CardArt/IconsNew (the mockup's skill paintings). -ZoogiOldInv = the P5 tiles.
	static bool ZInvNew() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldInv")); return !bOld; }
	static UTexture2D* ZInv(const TCHAR* N) { return ZLoadPng(FString(TEXT("Data/MenuArt/Inv/")) + N); }
	/** 9-slice box: M = the corner's fraction of the texture (the corners keep the texture's pixel size); MV = vertical fraction (0 = stretch) */
	static void ZBox9(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL0, const FVector2D& Sz0, float M, float MV = -1.f, const FLinearColor& C = FLinearColor::White)
	{
		if (!T) return;
		FVector2D TL = TL0, Sz = Sz0; ZPress(TL, Sz);
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		B.DrawAs = ESlateBrushDrawType::Box; B.Margin = FMargin(M, MV < 0.f ? M : MV, M, MV < 0.f ? M : MV);
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL)), &B, ESlateDrawEffect::None, C);
	}
	/** a label tag: ink or paper slab with a coloured shadow, text centred; returns its width */
	static float ZTag(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FVector2D& TL, float H, bool bPaper, const FLinearColor& Shadow, float Size = 34.f)
	{
		const FSlateFontInfo F = Font(0, Size);
		const float W = ZMeasure(S, F).X + 40.f;
		ZPanel(Out, G, L, TL, FVector2D(W, H), bPaper ? Paper : Ink, Shadow, 4.f, 7.f);
		ZTextC(Out, G, L + 3, S, F, TL + FVector2D(W, H) * 0.5f, bPaper ? Ink : Paper);
		return W;
	}
	/** ransom-note title, straight blocks: alternates paper / magenta / ink */
	static float ZRansom(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const TArray<FString>& Parts, const FVector2D& TL, float Size)
	{
		float X = TL.X;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const FSlateFontInfo F = Font(0, Size);
			const FVector2D M = ZMeasure(Parts[i], F);
			const FVector2D Sz(M.X + 22.f, Size * 1.32f);
			const int32 K = i % 3;
			const FLinearColor Bg = K == 0 ? Paper : K == 1 ? Mag() : Ink;
			if (K == 2) ZRect(Out, G, L, FVector2D(X - 4.f, TL.Y - 4.f), Sz + FVector2D(8.f, 8.f), Paper);
			ZRect(Out, G, L + 1, FVector2D(X, TL.Y), Sz, Bg);
			ZTextC(Out, G, L + 2, Parts[i], F, FVector2D(X, TL.Y) + Sz * 0.5f, K == 0 ? Ink : Paper);
			X += Sz.X + (K == 2 ? 8.f : 0.f);
		}
		return X - TL.X;
	}
	/** word-wrapped text block */
	static void ZWrap(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& TL, float MaxW, float LineH, const FLinearColor& C)
	{
		TArray<FString> Words; S.ParseIntoArray(Words, TEXT(" "));
		FString Line; float Y = TL.Y;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (ZMeasure(Try, F).X > MaxW && !Line.IsEmpty()) { ZTextAt(Out, G, L, Line, F, FVector2D(TL.X, Y), C); Y += LineH; Line = W; }
			else Line = Try;
		}
		if (!Line.IsEmpty()) ZTextAt(Out, G, L, Line, F, FVector2D(TL.X, Y), C);
	}
	static int32 ZWrapLines(const FString& S, const FSlateFontInfo& F, float MaxW) // b127: how many lines ZWrap draws
	{
		TArray<FString> Words; S.ParseIntoArray(Words, TEXT(" "));
		FString Line; int32 N = 0;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (ZMeasure(Try, F).X > MaxW && !Line.IsEmpty()) { ++N; Line = W; }
			else Line = Try;
		}
		return N + (Line.IsEmpty() ? 0 : 1);
	}
	static FString ZNumS(int32 N) { return FText::AsNumber(N).ToString(); }
	static FString ZClock(int64 S)
	{
		S = FMath::Max<int64>(0, S);
		return S >= 3600 ? FString::Printf(TEXT("%lldh %02lldm"), S / 3600, (S / 60) % 60) : FString::Printf(TEXT("%lld:%02lld"), S / 60, S % 60);
	}
	static FLinearColor ZChestCol(FName Type)
	{
		if (Type == FName(TEXT("epic"))) return Ink;
		if (Type == FName(TEXT("royal"))) return Red;
		return FLinearColor::FromSRGBColor(FColor(176, 106, 28));
	}
	/** a chest icon: lid + body, coloured by type */
	static void ZChest(FSlateWindowElementList& Out, const FGeometry& G, int32 L, FName Type, const FVector2D& Ctr, float W)
	{
		static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"));
		if (!bFlat)
		{	// b137 (Evan): the glossy 3D Zoogi chests
			const TCHAR* Key = Type == FName(TEXT("epic")) ? TEXT("epic") : Type == FName(TEXT("royal")) ? TEXT("rare") : TEXT("common");
			if (UTexture2D* Tx = ZLoadPng(FString(TEXT("Data/MenuArt/Front/icon_chest_")) + Key)) { const float S2 = W * 1.25f; ZTex(Out, G, L + 1, Tx, Ctr - FVector2D(S2, S2) * 0.5f, FVector2D(S2, S2)); return; }
		}
		const float H = W * 0.72f;
		const FVector2D TL = Ctr - FVector2D(W, H) * 0.5f;
		const FLinearColor C = ZChestCol(Type);
		const FLinearColor Rim = Type == FName(TEXT("epic")) ? Mag() : Ink;
		ZRect(Out, G, L, TL - FVector2D(5.f, 5.f), FVector2D(W, H) + FVector2D(10.f, 10.f), Rim);
		ZRect(Out, G, L + 1, TL, FVector2D(W, H), C);
		ZRect(Out, G, L + 2, TL + FVector2D(0.f, H * 0.36f), FVector2D(W, 6.f), Rim);
		ZRect(Out, G, L + 2, TL + FVector2D(W * 0.42f, H * 0.28f), FVector2D(W * 0.16f, H * 0.26f), Type == FName(TEXT("epic")) ? Mag() : Gold);
	}
}

namespace ZoogiFrontDraw
{
	// b156 (Evan: "add color to white backgrounds", the shop like his reference): tile colours
	static FLinearColor ZTile(int32 K)
	{
		switch (((K % 6) + 6) % 6)
		{
		case 0: return FLinearColor::FromSRGBColor(FColor(46, 184, 60));
		case 1: return FLinearColor::FromSRGBColor(FColor(28, 122, 240));
		case 2: return FLinearColor::FromSRGBColor(FColor(150, 48, 226));
		case 3: return FLinearColor::FromSRGBColor(FColor(246, 138, 12));
		case 4: return FLinearColor::FromSRGBColor(FColor(84, 40, 214));
		default: return FLinearColor::FromSRGBColor(FColor(0, 168, 186));
		}
	}
	// b156: the player level that opens a shop item (owned items are never locked)
	static int32 ZSkinLockLv(int32 N) { return 1 + N; }
	static int32 ZPackLockLv(int32 N, int32 Price) { return Price == 0 ? 1 : 2 + N; }
	static int32 ZChestLockLv(FName Id) { return Id == FName(TEXT("epic")) ? 5 : Id == FName(TEXT("royal")) ? 3 : 1; }
	static constexpr int32 HotstreakLockLv = 4;
	static UTexture2D* ZCoinIcon() { return ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin3d")); }
	static UTexture2D* ZGemIcon() { return ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem3d")); }
}

// ================================================================================================ shared settings rows
// Nov 1 b53: one settings list for the Royale pause panel and the front-end SET popup
namespace ZoogiFrontDraw
{
	static float SettingsRows(FSlateWindowElementList& Out, const FGeometry& G, int32 L, float RX, float Y, float RW,
		TFunctionRef<void(const FVector2D&, const FVector2D&, TFunction<void()>)> Hit, TFunction<void(float)> Click)
	{
		const float RowH = 62.f; // b93: 11 rows fit the pause panel
		auto Row = [&](const TCHAR* Label, const FString& Val, float Fill, TFunction<void()> Minus, TFunction<void()> Plus)
		{
			ZTextAt(Out, G, L + 7, Label, Font(0, 30.f), FVector2D(RX + 30.f, Y + 8.f), Ink);
			const float BX = RX + 360.f, BW = 230.f;
			ZRect(Out, G, L + 7, FVector2D(BX, Y + 16.f), FVector2D(BW, 24.f), Ink);
			ZRect(Out, G, L + 8, FVector2D(BX, Y + 16.f), FVector2D(BW * FMath::Clamp(Fill, 0.03f, 1.f), 24.f), Mag());
			ZTextC(Out, G, L + 9, Val, Font(0, 22.f), FVector2D(BX + BW * 0.5f, Y + 28.f), Paper);
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector2D TL(RX + RW - 170.f + k * 80.f, Y), Sz(64.f, 54.f);
				ZRect(Out, G, L + 7, TL + FVector2D(5.f, 5.f), Sz, Mag());
				ZRect(Out, G, L + 8, TL, Sz, Ink);
				ZTextC(Out, G, L + 9, k ? TEXT("+") : TEXT("-"), Font(0, 40.f), TL + Sz * 0.5f, Paper);
				Hit(TL - FVector2D(8.f, 8.f), Sz + FVector2D(16.f, 16.f), k ? Plus : Minus);
			}
			Y += RowH;
		};
		auto Toggle = [&](const TCHAR* Label, const FString& Val, bool bOn, TFunction<void()> Fn)
		{
			ZTextAt(Out, G, L + 7, Label, Font(0, 30.f), FVector2D(RX + 30.f, Y + 8.f), Ink);
			const FVector2D TL(RX + RW - 250.f, Y), Sz(220.f, 56.f);
			ZRect(Out, G, L + 7, TL + FVector2D(6.f, 6.f), Sz, bOn ? Ink : Mag());
			ZRect(Out, G, L + 8, TL, Sz, bOn ? Mag() : Ink);
			ZTextC(Out, G, L + 9, Val, ZFitFont(Val, 0, 32.f, Sz.X - 16.f), TL + Sz * 0.5f, Paper);
			Hit(TL - FVector2D(8.f, 8.f), Sz + FVector2D(16.f, 16.f), Fn);
			Y += RowH + 2.f;
		};
		auto Step = [Click](const TCHAR* Key, float Cur, float D, float Lo, float Hi) { ZoogiControls::Set(Key, FMath::Clamp(FMath::RoundToFloat((Cur + D) * 10.f) / 10.f, Lo, Hi)); if (Click) Click(1.f + D); };
		const float Ls = ZoogiControls::LookSens(), Ss = ZoogiControls::StickSens(), Ms = ZoogiControls::MoveSens();
		Row(TEXT("SWIPE LOOK"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Ls * 100.f)), (Ls - 0.3f) / 2.7f,
			[Step, Ls]() { Step(TEXT("LookSens"), Ls, -0.1f, 0.3f, 3.f); }, [Step, Ls]() { Step(TEXT("LookSens"), Ls, 0.1f, 0.3f, 3.f); });
		Row(TEXT("RIGHT STICK LOOK"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Ss * 100.f)), (Ss - 0.3f) / 2.7f,
			[Step, Ss]() { Step(TEXT("StickSens"), Ss, -0.1f, 0.3f, 3.f); }, [Step, Ss]() { Step(TEXT("StickSens"), Ss, 0.1f, 0.3f, 3.f); });
		Row(TEXT("MOVE STICK"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Ms * 100.f)), (Ms - 0.5f) / 1.5f,
			[Step, Ms]() { Step(TEXT("MoveSens"), Ms, -0.1f, 0.5f, 2.f); }, [Step, Ms]() { Step(TEXT("MoveSens"), Ms, 0.1f, 0.5f, 2.f); });
		const bool bInv = ZoogiControls::InvertY();
		Toggle(TEXT("INVERT LOOK UP / DOWN"), bInv ? TEXT("ON") : TEXT("OFF"), bInv, [Click, bInv]() { ZoogiControls::Set(TEXT("InvertY"), bInv ? 0.f : 1.f); if (Click) Click(1.f); });
		const int32 Hide = ZoogiControls::StickHide();
		static const TCHAR* HideNames[] = { TEXT("SHOW BOTH"), TEXT("HIDE LEFT"), TEXT("HIDE RIGHT"), TEXT("HIDE BOTH") };
		Toggle(TEXT("ROYALE STICKS"), HideNames[Hide & 3], Hide != 0, [Click, Hide]() { ZoogiControls::Set(TEXT("StickHide"), (float)((Hide + 1) % 4)); if (Click) Click(1.f); });
		const bool bClean = ZoogiControls::CleanAim();
		Toggle(TEXT("CLASSIC AIM GUIDES"), bClean ? TEXT("CLEAN") : TEXT("FULL"), bClean, [Click, bClean]() { ZoogiControls::Set(TEXT("CleanAim"), bClean ? 0.f : 1.f); if (Click) Click(1.f); });
		const bool bNoFlash = ZoogiControls::FlashMul() <= 0.f; // b66 (playtest): photosensitivity / white-out frames
		Toggle(TEXT("REDUCE FLASHES"), bNoFlash ? TEXT("ON") : TEXT("OFF"), bNoFlash, [Click, bNoFlash]() { ZoogiControls::Set(TEXT("ReduceFlash"), bNoFlash ? 0.f : 1.f); if (Click) Click(1.f); });
		const bool bRep = ZoogiControls::ReplayCam(); // b70 (Evan): replay cam optional
		Toggle(TEXT("REPLAY CAM"), bRep ? TEXT("ON") : TEXT("OFF"), bRep, [Click, bRep]() { ZoogiControls::Set(TEXT("ReplayCam"), bRep ? 0.f : 1.f); if (Click) Click(1.f); });
		const bool bDark = ZoogiControls::DarkArena(); // b243 (Evan: "make the effect where the lights get dark a toggle in settings to make permanent")
		Toggle(TEXT("DARK ARENA"), bDark ? TEXT("ON") : TEXT("OFF"), bDark, [Click, bDark]() { ZoogiControls::Set(TEXT("DarkArena"), bDark ? 0.f : 1.f); if (Click) Click(1.f); });
		const bool bWatch = ZoogiControls::WatchCPU(); // b93 (Evan): sit back and watch the CPUs play
		Toggle(TEXT("WATCH CPU PLAY"), bWatch ? TEXT("ON - NEXT MATCH") : TEXT("OFF"), bWatch, [Click, bWatch]() { ZoogiControls::Set(TEXT("WatchCPU"), bWatch ? 0.f : 1.f); if (Click) Click(1.f); });
		const int32 TBots = ZoogiControls::TrainBots(); // b96c (Evan): bots to practice duels and everything in TRAINING
		static const TCHAR* BotNames[] = { TEXT("NONE - SOLO"), TEXT("1 BOT"), TEXT("2 BOTS"), TEXT("3 BOTS") };
		Toggle(TEXT("TRAINING BOTS"), BotNames[TBots & 3], TBots != 0, [Click, TBots]() { ZoogiControls::Set(TEXT("TrainBots"), (float)((TBots + 1) % 4)); if (Click) Click(1.f); });
		const bool bAllSk = ZoogiDevSkills::AllOpen(); // b177r (Evan: "where is the dev button, add it here so I can use the skills")
		Toggle(TEXT("ALL SKILLS (DEV)"), bAllSk ? TEXT("ON - NEXT MATCH") : TEXT("OFF"), bAllSk, [Click, bAllSk]() { ZoogiDevSkills::SetAllOpen(!bAllSk); if (Click) Click(1.f); });
		auto Audio = [&](const TCHAR* Label, const TCHAR* Key)
		{
			const int32 Pct = ZoogiScoringUI::GetAudioPct(Key, 50);
			const FString K = Key;
			Row(Label, FString::Printf(TEXT("%d%%"), Pct), Pct / 100.f,
				[K, Pct]() { ZoogiScoringUI::SetAudioPct(*K, FMath::Max(0, Pct - 10)); ZoogiScoringUI::ApplyAudio(); },
				[K, Pct]() { ZoogiScoringUI::SetAudioPct(*K, FMath::Min(100, Pct + 10)); ZoogiScoringUI::ApplyAudio(); });
		};
		Audio(TEXT("SOUND FX"), TEXT("ClashSfx"));
		Audio(TEXT("MUSIC"), TEXT("ClashMusic"));
		return Y;
	}
}


// ================================================================================================ the widget
using namespace ZoogiFrontDraw;

// b211 (Evan's splash): the opening splash, once per app launch (-ZoogiNoSplash off, -ZoogiSplashTest also in front tests)
static double GZSplashT0 = -1.0; static bool GZSplashDone = false; static bool GZSplashTapped = false;
class SZoogiFront : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiFront) {}
	SLATE_END_ARGS()

	enum class EPop : uint8 { None, Item, Login, Chest, Tasks, News, Settings, Confirm, LevelUp, Trick };
	struct FHit { FBox2D R; TFunction<void()> Fn; };
	struct FItem { FString Name, Sub, Desc, Ball, CardId, WeaponKey, Icon; int32 Rarity = 0, Level = 1, Price = 0, PriceKind = 0; bool bOwned = true, bEquipped = false; int32 Kind = 0; int32 HandSlot = -1; int32 Count = -1; /* b227: items - how many you carry */ int32 NewN = 0; /* b228: +N NEW from chests */ };

	TWeakObjectPtr<UZoogiMenuRoot> Menu;
	TWeakObjectPtr<UObject> Ctx;
	int32 Screen = 0, Tab = 0, ItemSel = -1;
	bool b3D = false; // b203 (Evan: "3D model preview option for the ballers screen")
	bool bTutChecked = false, bSplashShot = false; int32 BuddyTip = 0; float BuddyAt = 0.f; // b211
	FString SkillDetail; // b65: SKILLS tab details panel (where the robot stands); empty = closed
	FString SkillPick; int32 SkillScroll = 0, SkillFilter = 0; // b179 (Evan picked C + B arrows): big detail card + scrolling list
	EPop Pop = EPop::None;
	float T = 0.f, PopT = 0.f, ToastT = -10.f;
	FString Toast;
	TArray<FZoogiFrontReward> Rewards;
	FString RewardTitle;
	FName RewardChest;
	float ChestSkipAt = 0.6f; int32 ChestSfxStep = 0; // b166: the chest show length (a tap before it skips to the end) + its next sound
	int32 ChestTestC = 0, ChestTestShot = -1; bool bChestTest = false; // b166: -ZoogiChestTest
	float ShopX = 0.f, ShopXVel = 0.f, ShopMax = 0.f;
	int32 ShopCat = -1; // b172g: phone shop - -1 = the zig-zag category menu, else the open section id
	TMap<int32, float> SectionX;
	bool bDown = false, bDrag = false;
	FVector2D DownAt = FVector2D::ZeroVector, LastAt = FVector2D::ZeroVector;
	int32 DragKind = 0; // 1 robot spin, 2 shop scroll, 3 news scroll
	float NewsY = 0.f, NewsMax = 0.f, NewsVel = 0.f; FBox2D NewsBox = FBox2D(ForceInit); // b127: the NEWS list scrolls
	// b156 (Evan: "scrolling ... needs to be smooth no exceptions"): the content follows the finger from an anchor; the release
	// velocity comes from the last 100 ms of samples (not one frame); TOP eases up
	float DragAnchorV = 0.f; FVector2D DragAnchorP = FVector2D::ZeroVector; bool bAnchored = false, bNewsToTop = false;
	float NewsGoal = -1.f; // b188 (Evan: "add up and down arrows to news"): an arrow tap eases the list a page
	TArray<TPair<double, float>> VelS;
	float NewsTgt = 0.f, ShopTgt = 0.f; // b159: the finger sets the target, Tick glides the list to it
	void AddVel(float V) { VelS.Add(TPair<double, float>(FPlatformTime::Seconds(), V)); if (VelS.Num() > 16) VelS.RemoveAt(0); }
	float RelVel() const
	{
		if (VelS.Num() < 2) return 0.f;
		const double Now = FPlatformTime::Seconds();
		const TPair<double, float>& Last = VelS.Last();
		if (Now - Last.Key > 0.08) return 0.f; // the finger stopped before letting go
		int32 i0 = VelS.Num() - 1; while (i0 > 0 && Last.Key - VelS[i0 - 1].Key <= 0.1) --i0;
		const double Dt = Last.Key - VelS[i0].Key;
		if (Dt < 0.012) return 0.f;
		return FMath::Clamp((float)((Last.Value - VelS[i0].Value) / Dt), -7000.f, 7000.f);
	}
	// b156: the BUY pop-up + LEVEL UP pop-up
	FString CfmTitle, CfmName; TWeakObjectPtr<UTexture2D> CfmIcon; FName CfmChest; FLinearColor CfmCol = FLinearColor::White; int32 CfmPrice = 0, CfmKind = 0; TFunction<void()> CfmFn;
	int32 LvShown = 0;
	void AskBuy(const FString& Title, const FString& Name, UTexture2D* Icon, const FLinearColor& Col, int32 Price, int32 Kind, TFunction<void()> Fn, FName Chest = NAME_None)
	{
		CfmTitle = Title; CfmName = Name; CfmIcon = Icon; CfmCol = Col; CfmPrice = Price; CfmKind = Kind; CfmFn = MoveTemp(Fn); CfmChest = Chest;
		Pop = EPop::Confirm; PopT = T;
		UE_LOG(LogZoogi, Log, TEXT("FRONT buy pop-up: %s - %d %s"), *Name, Price, Kind == 1 ? TEXT("gems") : TEXT("coins"));
	}
	bool bTest = false;
	int32 TestStep = 0;
	float TestNext = 1.5f;
	bool bLoginChecked = false;
	bool bTrickChecked = false; // b157: the DAILY TRICK SHOT pop-up (once a day)

	mutable TArray<FHit> Hits;
	mutable float S = 1.f, DW = 1920.f, DH = 1080.f;
	mutable FVector2D OffPx = FVector2D::ZeroVector, ScrPx = FVector2D(1920.f, 1080.f); // b56: safe inset (local px) + full size
	/** design point -> 0..1 of the whole screen (the 3D stage places the robot in screen space) */
	FVector2D ScrN(const FVector2D& D) const { return (OffPx + D * S) / FVector2D(FMath::Max(1.f, ScrPx.X), FMath::Max(1.f, ScrPx.Y)); }
	mutable FBox2D RobotBox = FBox2D(ForceInit);

	// b67 (Evan: "fix skills inventory to not exit when I tap a skill"): a SKILLS tap saves the hand -> profile save -> the menu root
	// rebuilds and this widget is created again (it came back on HOME). A rebuild within 2 s restores the screen / tab / details.
	struct FKeep { double At = -100.0; int32 Screen = 0, Tab = 0; FString SkillDetail; float ShopX = 0.f; };
	static FKeep& Keep() { static FKeep K; return K; }
	void Construct(const FArguments&)
	{
		SetCanTick(true); bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrontTest")); bChestTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiChestTest")); if (bChestTest) bTest = true;
		const FKeep& K = Keep();
		if (FPlatformTime::Seconds() - K.At < 2.0 && K.Screen != 0)
		{
			Screen = K.Screen; Tab = K.Tab; SkillDetail = K.SkillDetail; ShopX = K.ShopX; bLoginChecked = true; bTrickChecked = true;
			UE_LOG(LogZoogi, Log, TEXT("FRONT rebuilt - kept screen %d tab %d"), Screen, Tab);
		}
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool ComputeVolatility() const override { return true; }

	UZoogiFrontProfileSubsystem* FP() const { return UZoogiFrontProfileSubsystem::Get(Ctx.Get()); }
	AZoogiGameMode* GM() const { return Ctx.IsValid() ? Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(Ctx.Get())) : nullptr; }
	static bool SplashWanted() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSplash")) && (FParse::Param(FCommandLine::Get(), TEXT("ZoogiSplashTest")) || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrontTest"))) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto")); return b; }
	static float SplashLen() { return 3.4f; }
	void Sfx(const TCHAR* N, float V = 0.8f, float P = 1.f) const { if (UObject* C = Ctx.Get()) ZoogiUtil::PlaySfx(C, N, V, P); }
	void Say(const FString& M) { Toast = M; ToastT = T; UE_LOG(LogZoogi, Log, TEXT("FRONT toast: %s"), *M); }
	void Shot(const FString& N) const { if (AZoogiGameMode* G = GM()) G->TakeShot(N); }

	// ---------------------------------------------------------------- actions
	void Go(int32 NewScreen)
	{
		if (NewScreen == Screen) return;
		Screen = NewScreen; Pop = EPop::None; ItemSel = -1;
		if (NewScreen == 2) ShopCat = -1; // b172g: the phone shop opens on its category menu (entry points pick a category after this)
		Sfx(TEXT("UiClick"), 0.7f, 1.05f);
		UE_LOG(LogZoogi, Log, TEXT("FRONT screen %d"), Screen);
	}
	void OpenPop(EPop P)
	{
		Pop = P; PopT = T;
		if (P == EPop::News) { NewsY = 0.f; NewsVel = 0.f; } // b127: newest first
		Sfx(TEXT("UiClick"), 0.8f, 1.1f);
		if (P == EPop::Tasks) if (UZoogiFrontProfileSubsystem* Pr = FP()) Pr->EnsureTasks();
		UE_LOG(LogZoogi, Log, TEXT("FRONT popup %d"), (int32)P);
	}
	void Battle()
	{
		Sfx(TEXT("UiConfirm"), 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("FRONT BATTLE -> mode select"));
		TWeakObjectPtr<UZoogiMenuRoot> M = Menu;
		UWorld* W = Ctx.IsValid() ? Ctx->GetWorld() : nullptr;
		ZoogiP5::PlayWipe(W, [M]() { if (M.IsValid()) M->SetFrontBattle(true); }, Mag(), TEXT("LET'S ROLL!"));
	}
	void SkillLab()
	{	// b108 (Evan): the black P5 SKILL LAB button
		Sfx(TEXT("UiConfirm"), 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("FRONT SKILL LAB"));
		TWeakObjectPtr<UZoogiMenuRoot> M = Menu;
		UWorld* W = Ctx.IsValid() ? Ctx->GetWorld() : nullptr;
		ZoogiP5::PlayWipe(W, [M]() { if (M.IsValid()) M->StartSkillLab(); }, Ink, TEXT("SKILL LAB"));
	}
	void ShowRewards(const FString& Title, FName Chest, const TArray<FZoogiFrontReward>& R)
	{
		Rewards = R; RewardTitle = Title; RewardChest = Chest; Pop = EPop::Chest; PopT = T;
		ChestSfxStep = 0; ChestSkipAt = 2.f;
		Sfx(TEXT("UiConfirm"), 1.f, 0.9f);
	}
	void TapChestSlot(int32 i)
	{
		UZoogiFrontProfileSubsystem* P = FP();
		if (!P || !P->Data().Chests.IsValidIndex(i)) return;
		const FZoogiFrontChestSlot& C = P->Data().Chests[i];
		FString Why; TArray<FZoogiFrontReward> Out;
		if (C.Type.IsNone()) { Say(TEXT("WIN A BATTLE TO EARN A CHEST!")); return; }
		const FZoogiFrontChestDef* Def = UZoogiFrontProfileSubsystem::FindChest(C.Type);
		const FString Name = Def ? FString(Def->Name).ToUpper() : TEXT("CHEST");
		const FName Type0 = C.Type; // OpenChest clears the slot (C is a reference into it)
		if (P->IsReady(i)) { if (P->OpenChest(i, false, Out, Why)) ShowRewards(Name, Type0, Out); else Say(Why.ToUpper()); return; }
		if (P->SecondsLeft(i) < 0)
		{
			if (P->StartUnlock(i, Why)) { Sfx(TEXT("UiClick")); Say(FString::Printf(TEXT("%s UNLOCKING: %s"), *Name, *ZClock(P->SecondsLeft(i)))); }
			else Say(Why.ToUpper());
			return;
		}
		// unlocking: open now for gems
		const int32 Cost = P->OpenNowCost(i);
		if (P->OpenChest(i, true, Out, Why)) ShowRewards(Name, Type0, Out);
		else Say(FString::Printf(TEXT("OPEN NOW: %d GEMS  -  %s"), Cost, *Why.ToUpper()));
	}
	void ClaimLogin()
	{
		UZoogiFrontProfileSubsystem* P = FP();
		if (!P) return;
		TArray<FZoogiFrontReward> Out; FName Chest;
		if (!P->ClaimLogin(Out, Chest)) { Pop = EPop::None; return; }
		const int32 Day = P->Data().LoginStreak; // already advanced
		if (!Chest.IsNone()) { const FZoogiFrontChestDef* D = UZoogiFrontProfileSubsystem::FindChest(Chest); ShowRewards(D ? FString(D->Name).ToUpper() : TEXT("CHEST"), Chest, Out); }
		else { Pop = EPop::None; int32 G = 0; for (const FZoogiFrontReward& R : Out) if (R.Kind == FName(TEXT("gold"))) G += R.Amount; Say(FString::Printf(TEXT("DAY %d CLAIMED: +%s GOLD"), Day, *ZNumS(G))); Sfx(TEXT("UiConfirm")); }
	}

	// inventory items for the current tab
	TArray<FItem> Items() const
	{
		TArray<FItem> Out;
		const UZoogiFrontProfileSubsystem* P = FP();
		const FString Eq = ZoogiFront::Equipped(Ctx.Get()).Name;
		const FString EqBall = P && !P->Data().EquippedBall.IsNone() ? P->Data().EquippedBall.ToString() : Eq;
		if (Tab == 0)
			for (const ZoogiFront::FHero& H : ZoogiFront::Heroes())
			{
				FItem I; I.Kind = 0; I.Name = H.Name; I.Sub = FString::Printf(TEXT("%s - %s"), ZRarityName(H.Rarity), H.Element); I.Desc = H.Blurb; I.Ball = H.Ball; I.Rarity = H.Rarity;
				I.bEquipped = Eq == H.Name;
				Out.Add(I);
			}
		else if (Tab == 1)
			for (const ZoogiFront::FHero& H : ZoogiFront::Heroes())
			{
				FItem I; I.Kind = 1; I.Name = FString(H.Ball).ToUpper(); I.Ball = H.Ball; I.Rarity = H.Rarity;
				// b100 (Evan): the balls are the ZOOGIS - your Awaken collection; each carries its own Awaken power
				const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenForBall(H.Ball);
				I.Sub = Aw ? FString::Printf(TEXT("AWAKEN: %s"), Aw->Name) : FString::Printf(TEXT("%s ZOOGI"), ZRarityName(H.Rarity));
				I.Desc = Aw ? FString::Printf(TEXT("%s ZOOGI. %s"), ZRarityName(H.Rarity), Aw->Desc) : FString(TEXT("Roll in this Zoogi in every mode."));
				I.bOwned = P ? P->OwnsSkin(FName(H.Ball)) : false;
				I.bEquipped = EqBall == H.Ball;
				I.Price = UZoogiFrontProfileSubsystem::SkinPrice(FName(H.Ball), I.PriceKind);
				Out.Add(I);
			}
		else if (Tab == 2)
		{
			// b120 (Evan: "add the weapons we created in tripo starting with the grenade, sword, staff, disc"): the robot form's weapon,
			// equipped here (FZoogiFrontProfile::EquippedWeapon) and carried into Battle Royale (ZoogiRoyaleHero.cpp)
			struct FW { const TCHAR* K; const TCHAR* N; const TCHAR* D; int32 R; };
			static const FW W[] = {
				{ TEXT("gatling"), TEXT("GATLING TURRET"), TEXT("Turns to the nearest orb or rival (the boss in a RAID) and fires 10 shots from its barrel. Pick its AMMO below."), 3 }, // b224: shorter - it ran into the bars // b203 (Evan): the turret is an item
				{ TEXT("grenade"), TEXT("PRISM GRENADE"), TEXT("Lob a crystal grenade along your aim - it bursts on landing and blasts everyone near it away."), 3 },
				{ TEXT("sword"), TEXT("CRYSTAL SWORD"), TEXT("Dash in and cut a wide arc in front of you - big knockback up close."), 3 },
				{ TEXT("staff"), TEXT("CRYSTAL STAFF"), TEXT("Fires heavy magic bolts from the orb on top - slower than the Gatling, hits much harder."), 2 },
				{ TEXT("disc"), TEXT("HALO DISC"), TEXT("Throw the halo like a boomerang - it cuts through everyone on the way out and on the way back."), 2 },
				{ TEXT("apple"), TEXT("GOLDEN APPLE"), TEXT("Tap an orb in reach: a golden apple is lobbed there and its big blast throws everything away."), 1 } }; // b227: the Golden Apple is an item too
			const FName EqW = P && !P->Data().EquippedWeapon.IsNone() ? P->Data().EquippedWeapon : FName(TEXT("gatling"));
			for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(W); ++i)
			{
				FItem I; I.Kind = 2; I.WeaponKey = W[i].K; I.Name = W[i].N; I.Desc = W[i].D; I.Rarity = W[i].R;
				const int32 Have = P ? P->Data().ItemCounts.FindRef(FName(W[i].K)) : 0;
				I.Count = P && P->Data().bItemsSeeded ? Have : ((FCString::Strcmp(W[i].K, TEXT("sword")) == 0 || FCString::Strcmp(W[i].K, TEXT("staff")) == 0 || FCString::Strcmp(W[i].K, TEXT("apple")) == 0) ? 3 : 5); // b227
				I.NewN = P ? P->Data().ItemCounts.FindRef(FName(*(FString(TEXT("new_")) + W[i].K))) : 0; // b228
				I.Sub = FString::Printf(TEXT("%s ITEM  x%d"), ZRarityName(W[i].R), I.Count); // b137: the collection count (b143d: start 5)
				I.Icon = FCString::Strcmp(W[i].K, TEXT("apple")) == 0 ? FString(TEXT("Data/CardArt/golden_apple")) : FString(TEXT("Data/MenuArt/Weapons/w_")) + W[i].K; I.bOwned = true;
				I.bEquipped = P && P->Data().SelWeapons.Num() ? P->Data().SelWeapons.Contains(FName(W[i].K)) : EqW == FName(W[i].K); // b137: selected for the ITEM list
				Out.Add(I);
			}
		}
		else if (P)
			for (const FZoogiCardDef& C : UZoogiFrontProfileSubsystem::CardDefs())
			{
				FItem I; I.Kind = 3; I.Name = C.Name.ToUpper(); I.CardId = C.Id.ToString(); I.Desc = C.Rule; I.bOwned = P->IsCardOwned(C.Id); I.Level = P->CardLevelOf(C.Id);
				if (ZoogiCardLockedInClassic(C.Id)) { I.bOwned = false; I.Price = 0; } // b136 (Evan): no way to play it yet - grey + LOCKED
				I.Rarity = FMath::Clamp(C.Cost - 1, 0, 3); I.Sub = FString::Printf(TEXT("SKILL - %d SP"), C.Cost);
				// b63 (Evan): the SKILLS tab is the hand loadout - selected = in the hand (slot number), tap again frees the slot
				I.HandSlot = I.bOwned ? P->Deck(P->Data().ActiveDeck).IndexOfByKey(C.Id) : INDEX_NONE; I.bEquipped = I.HandSlot != INDEX_NONE; // b137d: a locked skill never shows ON
				Out.Add(I);
			}
		return Out;
	}
	// b63 (Evan: "only the previous limit of cards in my Hand and those must be selected from the inventory and selected tapping
	// again unselects and frees a slot for another skill"): tap a skill = into the hand (max DeckSize), tap again = out
	void ToggleSkill(const FItem& I)
	{
		UZoogiFrontProfileSubsystem* P = FP();
		if (!P) return;
		if (!I.bOwned) { Say(TEXT("LOCKED - UNLOCK IT FIRST")); Sfx(TEXT("UiCancel"), 0.6f); return; }
		const int32 Dk = P->Data().ActiveDeck;
		TArray<FName> Hand = P->Deck(Dk);
		const FName Id(*I.CardId);
		if (ZNewSkillsUi()) SkillPick = I.CardId; else SkillDetail = I.CardId; // b65 (Evan): the tap also shows the skill's details where the robot stands; tap the panel to close
		if (Hand.Contains(Id)) { Hand.Remove(Id); P->SetDeck(Dk, Hand); Sfx(TEXT("UiCancel"), 0.7f, 1.1f); Say(FString::Printf(TEXT("%s REMOVED  -  %d / %d"), *I.Name, Hand.Num(), UZoogiFrontProfileSubsystem::DeckSize)); }
		else if (Hand.Num() >= UZoogiFrontProfileSubsystem::DeckSize) { Sfx(TEXT("UiCancel"), 0.6f); Say(TEXT("HAND FULL - TAP A SELECTED SKILL TO FREE A SLOT")); }
		else { Hand.Add(Id); P->SetDeck(Dk, Hand); Sfx(TEXT("UiConfirm"), 0.8f, 1.15f); Say(FString::Printf(TEXT("%s ADDED  -  %d / %d"), *I.Name, Hand.Num(), UZoogiFrontProfileSubsystem::DeckSize)); }
		UE_LOG(LogZoogi, Log, TEXT("FRONT skills hand: %s"), *FString::JoinBy(P->Deck(Dk), TEXT(","), [](const FName& N) { return N.ToString(); }));
	}
	void ItemAction(const FItem& I)
	{
		UZoogiFrontProfileSubsystem* P = FP();
		if (!P) return;
		FString Why;
		if (I.Kind == 0)
		{
			if (I.bEquipped) { Say(TEXT("ALREADY EQUIPPED")); return; }
			P->SetEquipped(FName(*I.Name), NAME_None);
			ZoogiFront::ApplyEquipped(Ctx.Get());
			ZoogiFront::View().CheerSerial++;
			Sfx(TEXT("UiConfirm"));
			Say(FString::Printf(TEXT("%s EQUIPPED!"), *I.Name.ToUpper()));
			Pop = EPop::None;
		}
		else if (I.Kind == 1)
		{
			if (!I.bOwned)
			{
				if (P->BuySkin(FName(*I.Ball), Why)) { Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s UNLOCKED!"), *I.Ball.ToUpper())); }
				else Say(Why.ToUpper());
				return;
			}
			P->SetEquipped(NAME_None, FName(*I.Ball));
			const ZoogiFront::FAwaken* AwE = ZoogiFront::AwakenForBall(I.Ball);
			Sfx(TEXT("UiConfirm")); Say(AwE ? FString::Printf(TEXT("%s EQUIPPED - AWAKEN: %s"), *I.Ball.ToUpper(), AwE->Name) : FString::Printf(TEXT("%s EQUIPPED!"), *I.Ball.ToUpper())); Pop = EPop::None;
		}
		else if (I.Kind == 2)
		{	// b120: equip the weapon (the robot form uses it in Battle Royale)
			// b137 (Evan: "selected weapons appear in ITEM as a list"): tap selects / unselects, up to 3; the last one picked is the robot's weapon
			TArray<FName>& Sel = P->Mutable().SelWeapons;
			if (Sel.Num() == 0 && !P->Data().EquippedWeapon.IsNone()) Sel.Add(P->Data().EquippedWeapon);
			const FName Wk(*I.WeaponKey);
			if (Sel.Contains(Wk)) { Sel.Remove(Wk); P->Save(); Sfx(TEXT("UiCancel")); Say(FString::Printf(TEXT("%s REMOVED FROM ITEMS"), *I.Name)); Pop = EPop::None; return; }
			if (Sel.Num() >= 3) { Say(TEXT("3 ITEMS MAX - REMOVE ONE FIRST")); return; }
			Sel.Add(Wk);
			P->Mutable().EquippedWeapon = Wk;
			P->Save();
			Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s ADDED TO ITEMS (%d/3)"), *I.Name, Sel.Num())); Pop = EPop::None;
			UE_LOG(LogZoogi, Log, TEXT("FRONT weapon equipped: %s"), *I.WeaponKey);
		}
		else
		{
			int32 XP = 0; bool bLv = false;
			if (P->UpgradeCard(FName(*I.CardId), Why, XP, bLv)) { Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s UPGRADED! +%d XP"), *I.Name, XP)); }
			else Say(Why.ToUpper());
		}
	}

	// ---------------------------------------------------------------- input
	FVector2D ToDesign(const FGeometry& G, const FVector2D& Screen01) const { return (G.AbsoluteToLocal(Screen01) - OffPx) / FMath::Max(S, 0.01f); }
	// Nov 1 b53 (Evan: "the first menu buttons don't work"): a press on a button fires at once (the P5 menu way, proven on the
	// phone); only the robot (spin) and the shop strip (scroll) wait for the release to tell a tap from a drag
	FReply Down(const FGeometry& G, const FPointerEvent& E)
	{
		bDrag = false;
		DownAt = LastAt = ToDesign(G, E.GetScreenSpacePosition());
		DragKind = (Pop == EPop::News && NewsBox.bIsValid && NewsBox.IsInside(DownAt)) ? 3 : Pop != EPop::None ? 0 : (Screen == 2 && DownAt.Y > 120.f && DownAt.Y < DH - 80.f) ? 2 : (RobotBox.bIsValid && RobotBox.IsInside(DownAt) && !HitAt(DownAt)) ? 1 : 0;
		if (DragKind == 0) { bDown = false; Tap(DownAt); return FReply::Handled(); }
		VelS.Reset(); bAnchored = false; bNewsToTop = false; NewsGoal = -1.f;
		if (DragKind == 3) { bDrag = true; bAnchored = true; DragAnchorP = DownAt; DragAnchorV = NewsY; NewsVel = 0.f; NewsTgt = NewsY; AddVel(NewsY); } // b156: the list follows from the first pixel
		if (DragKind == 2) { ShopXVel = 0.f; ShopTgt = ShopX; }
		bDown = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	bool HitAt(const FVector2D& P) const { for (int32 i = Hits.Num() - 1; i >= 0; --i) if (Hits[i].R.IsInside(P)) return Hits[i].Fn != nullptr; return false; }
	FReply Move(const FGeometry& G, const FPointerEvent& E)
	{
		if (!bDown) return FReply::Unhandled();
		const FVector2D P = ToDesign(G, E.GetScreenSpacePosition());
		if (!bDrag && FVector2D::Distance(P, DownAt) > 14.f) bDrag = true;
		if (bDrag)
		{
			const float Dx = P.X - LastAt.X;
			if (DragKind == 1) ZoogiFront::View().Spin += Dx * 0.45f;
			if (DragKind == 2) { if (!bAnchored) { bAnchored = true; DragAnchorP = P; DragAnchorV = ShopX; } ShopTgt = FMath::Clamp(DragAnchorV - (P.X - DragAnchorP.X), 0.f, ShopMax); AddVel(ShopTgt); } // b156: no jump when the drag starts (b159: target)
			if (DragKind == 3) { NewsTgt = FMath::Clamp(DragAnchorV - (P.Y - DragAnchorP.Y), 0.f, NewsMax); AddVel(NewsTgt); }
		}
		LastAt = P;
		return FReply::Handled();
	}
	FReply Up(const FGeometry& G, const FPointerEvent& E)
	{
		if (!bDown) return FReply::Handled().ReleaseMouseCapture();
		bDown = false;
		const FVector2D P = ToDesign(G, E.GetScreenSpacePosition());
		if (!bDrag) Tap(P);
		ShopXVel = (bDrag && DragKind == 2) ? RelVel() : 0.f; // b156: release velocity from the last 100 ms
		NewsVel = (bDrag && DragKind == 3) ? RelVel() : 0.f;
		return FReply::Handled().ReleaseMouseCapture();
	}
	TFunction<void()> PendingTap; float PendingTapAt = -1.f; // b136: the button squashes first, its action follows
	void Tap(const FVector2D& P)
	{
		static const bool bNoPressFx = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPressFx"));
		for (int32 i = Hits.Num() - 1; i >= 0; --i)
			if (Hits[i].R.IsInside(P))
			{
				TFunction<void()> Fn = Hits[i].Fn;
				if (!Fn) return;
				const FVector2D Bs = Hits[i].R.GetSize();
				if (bNoPressFx || bTest || Bs.X * Bs.Y > DW * DH * 0.5f) { Fn(); return; } // a full-screen dismiss area just runs
				if (PendingTap) { TFunction<void()> Prev = MoveTemp(PendingTap); PendingTap = nullptr; Prev(); } // a quick double tap: the first one runs now
				GPressBox = Hits[i].R; GPressAt = FPlatformTime::Seconds();
				PendingTap = MoveTemp(Fn); PendingTapAt = T + 0.1f;
				if (UObject* C = Ctx.Get()) ZoogiHaptics::Kick(0.25f, C->GetWorld(), TEXT("ui_tap"));
				return;
			}
		if (RobotBox.bIsValid && RobotBox.IsInside(P) && Pop == EPop::None) { ZoogiFront::View().CheerSerial++; Sfx(TEXT("UiClick"), 0.6f, 1.3f); }
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Down(G, E); }
	virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Up(G, E); }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Unhandled() : Move(G, E); }
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Down(G, E); }
	virtual FReply OnTouchMoved(const FGeometry& G, const FPointerEvent& E) override { return Move(G, E); }
	virtual FReply OnTouchEnded(const FGeometry& G, const FPointerEvent& E) override { return Up(G, E); }
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent& E) override // b127: the mouse wheel scrolls NEWS
	{
		if (Pop != EPop::News) return FReply::Unhandled();
		NewsY = FMath::Clamp(NewsY - E.GetWheelDelta() * 110.f, 0.f, NewsMax); NewsVel = 0.f;
		return FReply::Handled();
	}

	// ---------------------------------------------------------------- tick (+ scripted capture)
	virtual void Tick(const FGeometry& G, const double Now, const float Dt) override
	{
		const float Step = bTest ? 1.f / 30.f : FMath::Min(Dt, 0.05f);
		T += Step;
		if (PendingTap && T >= PendingTapAt) { TFunction<void()> Fn = MoveTemp(PendingTap); PendingTap = nullptr; Fn(); } // b136: after the squash
		if (bDown && bAnchored && (DragKind == 2 || DragKind == 3))
		{	// b159: glide to the finger (frame-rate independent); snaps when within half a pixel
			static const bool bRaw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRawScroll"));
			const float A = bRaw ? 1.f : 1.f - FMath::Exp(-FMath::Min(Dt, 0.1f) / 0.028f);
			float& Cur = DragKind == 3 ? NewsY : ShopX; const float Tg = DragKind == 3 ? NewsTgt : ShopTgt;
			Cur = FMath::Abs(Tg - Cur) < 0.5f ? Tg : Cur + (Tg - Cur) * A;
		}
		if (!bDown && FMath::Abs(NewsVel) > 4.f) { const float Ny = NewsY + NewsVel * Step; NewsY = FMath::Clamp(Ny, 0.f, NewsMax); NewsVel = Ny != NewsY ? 0.f : NewsVel * FMath::Exp(-3.2f * Step); } else if (!bDown) NewsVel = 0.f; // b127 / b156: a flick glides
		if (NewsGoal >= 0.f && !bDown) { NewsGoal = FMath::Clamp(NewsGoal, 0.f, NewsMax); NewsY = FMath::FInterpTo(NewsY, NewsGoal, Step, 10.f); if (FMath::Abs(NewsY - NewsGoal) < 0.5f) { NewsY = NewsGoal; NewsGoal = -1.f; } } // b188: UP / DOWN arrows
		if (bNewsToTop && !bDown) { NewsY = FMath::FInterpTo(NewsY, 0.f, Step, 9.f); if (NewsY < 0.5f) { NewsY = 0.f; bNewsToTop = false; } } // b156: TOP eases up
		if (!bDown && FMath::Abs(ShopXVel) > 4.f) { const float Nx = ShopX + ShopXVel * Step; ShopX = FMath::Clamp(Nx, 0.f, ShopMax); ShopXVel = Nx != ShopX ? 0.f : ShopXVel * FMath::Exp(-3.2f * Step); } else if (!bDown) ShopXVel = 0.f;
		if (!GZSplashDone)
		{	// b211: the splash ends by itself or with a tap (after 0.5 s)
			if (!SplashWanted()) GZSplashDone = true;
			else if (GZSplashT0 > 0.0)
			{
				const float Tt = (float)(FPlatformTime::Seconds() - GZSplashT0);
				if (bDown && Tt > 0.5f) GZSplashTapped = true;
				if (!bSplashShot && Tt > 1.4f) { bSplashShot = true; Shot(TEXT("splash")); }
				if (Tt >= SplashLen() || (GZSplashTapped && !bDown)) { GZSplashDone = true; UE_LOG(LogZoogi, Log, TEXT("SPLASH done after %.1f s%s"), Tt, GZSplashTapped ? TEXT(" (tap)") : TEXT("")); }
			}
			return;
		}
		if (!bTutChecked)
		{	// b211: the first launch plays the guided first match (WALK 3)
			bTutChecked = true;
			if (Screen == 0 && ZoogiTutorial::ShouldAutoRun()) if (AZoogiGameMode* Gm = GM()) { Pop = EPop::None; ZoogiTutorial::Begin(Gm); return; }
		}
		if (!bLoginChecked && T > 0.6f)
		{
			bLoginChecked = true;
			if (UZoogiFrontProfileSubsystem* P = FP()) if (P->LoginClaimable() && Pop == EPop::None) { Pop = EPop::Login; PopT = T; UE_LOG(LogZoogi, Log, TEXT("FRONT daily login: day %d"), P->LoginDayIndex() + 1); }
		}
		{	// b171: -ZoogiTrickTest - opens the daily trick shot pop-up and shoots it (Evan: "fill the white with the image")
			static const bool bTrickTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTrickTest"));
			static int32 TrickTestStep = 0;
			if (bTrickTest && TrickTestStep == 0 && T > 2.5f) { Pop = EPop::Trick; PopT = T; TrickTestStep = 1; }
			else if (bTrickTest && TrickTestStep == 1 && T - PopT > 1.0f) { Shot(TEXT("trick_pop")); TrickTestStep = 2; UE_LOG(LogZoogi, Log, TEXT("FRONT trick test shot")); }
		}
		if (!bTrickChecked && T > 1.4f && Pop == EPop::None && Screen == 0)
		{	// b157 (Evan: "daily trick shot ... is a pop up that happens at start up main menu")
			bTrickChecked = true;
			if (ZoogiTrickShot::PopupDue()) { ZoogiTrickShot::MarkPopupShown(); Pop = EPop::Trick; PopT = T; Sfx(TEXT("UiConfirm"), 0.9f, 1.1f); UE_LOG(LogZoogi, Log, TEXT("FRONT daily trick shot pop-up #%d"), ZoogiTrickShot::TodayNumber()); }
		}
		{	// b156: LEVEL UP pop-up, once per new level (after a match, a trick shot, a card upgrade)
			static int32 GSeenLv = -1;
			if (UZoogiFrontProfileSubsystem* Pl = FP())
			{
				const int32 Lv = Pl->Data().Level;
				if (GSeenLv < 0 || Lv < GSeenLv) GSeenLv = Lv;
				else if (Lv > GSeenLv && Pop == EPop::None && T > 1.f)
				{
					GSeenLv = Lv; LvShown = Lv; Pop = EPop::LevelUp; PopT = T;
					Sfx(TEXT("UiConfirm"), 1.f, 1.2f);
					if (UObject* C = Ctx.Get()) ZoogiHaptics::Kick(0.6f, C->GetWorld(), TEXT("level_up"));
					UE_LOG(LogZoogi, Log, TEXT("FRONT LEVEL UP -> %d"), Lv);
				}
			}
		}
		ZoogiFront::FView& V = ZoogiFront::View();
		V.AliveT = FPlatformTime::Seconds();
		{ FKeep& K = Keep(); K.At = V.AliveT; K.Screen = Screen; K.Tab = Tab; K.SkillDetail = SkillDetail; K.ShopX = ShopX; } // b67
		V.Screen = Screen;
		V.Hero = ZoogiFront::Equipped(Ctx.Get()).Name;
		if (Screen == 1 && Tab == 0 && Pop == EPop::Item) { const TArray<FItem> Iv = Items(); if (Iv.IsValidIndex(ItemSel)) V.Hero = Iv[ItemSel].Name; } // b203: the 3D view shows the Zoogi you opened
		if (Tab != 3 || Screen != 1) SkillDetail.Reset();
		V.bRobot = Screen == 1 && (ZInvRobot() || (b3D && Tab == 0)) && !(Pop == EPop::Chest || Pop == EPop::Settings || Pop == EPop::News || Pop == EPop::Tasks) && SkillDetail.IsEmpty(); // b65: the details panel takes the robot's spot
		if (Screen == 0 && ZHomeB() && Pop == EPop::None) V.bRobot = true; // b209 Home B: the equipped Baller stands on the home screen
		if (bChestTest) TickChestTest(); else if (bTest) TickTest();
	}
	void TickChestTest()
	{	// b166: -ZoogiChestTest - opens a common, a rare and an epic chest and shoots each show at fixed moments
		if (T < 2.f || ChestTestC >= 3) return;
		static const float At[6] = { 0.5f, 0.85f, 1.05f, 1.4f, 1.9f, 3.0f };
		static const TCHAR* Ty[3] = { TEXT("wooden"), TEXT("royal"), TEXT("epic") };
		static const TCHAR* Nm[3] = { TEXT("COMMON CHEST"), TEXT("RARE CHEST"), TEXT("EPIC CHEST") };
		if (ChestTestShot < 0)
		{
			TArray<FZoogiFrontReward> R;
			R.Add(FZoogiFrontReward{ FName(TEXT("gold")), NAME_None, 250 + 250 * ChestTestC });
			R.Add(FZoogiFrontReward{ FName(TEXT("item")), FName(TEXT("apple")), 1 }); // b265: Evan's wooden chest (gold, apple, wood shield, Gattling Burst)
			R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("wood_shield")), 4 });
			R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("gattling_burst")), 2 });
			if (ChestTestC >= 1) R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("ice_wall")), 1 });
			if (ChestTestC >= 2) R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("meteor_rain")), 1 });
			ShowRewards(Nm[ChestTestC], FName(Ty[ChestTestC]), R);
			ChestTestShot = 0; return;
		}
		if (ChestTestShot < 6 && T - PopT >= At[ChestTestShot]) { Shot(FString::Printf(TEXT("chest_%d_%s_%03d"), ChestTestC, Ty[ChestTestC], FMath::RoundToInt(At[ChestTestShot] * 100.f))); ++ChestTestShot; return; }
		if (ChestTestShot >= 6 && T - PopT > 3.3f)
		{
			UE_LOG(LogZoogi, Log, TEXT("FRONT chest test %s style %d end %.2f"), Ty[ChestTestC], ChestStyle(), ChestSkipAt);
			++ChestTestC; ChestTestShot = -1;
			if (ChestTestC >= 3) { Pop = EPop::None; UE_LOG(LogZoogi, Log, TEXT("FRONT chest test done")); }
		}
	}
	void TickTest()
	{
		if (T < TestNext) return;
		TestNext = T + 1.2f;
		UZoogiFrontProfileSubsystem* P = FP();
		switch (TestStep++)
		{
		case 0: Shot(TEXT("front_00_open")); break;
		case 1: if (Pop == EPop::Login) { Shot(TEXT("front_01_login")); ClaimLogin(); } break;
		case 2: Shot(TEXT("front_02_after_login")); Pop = EPop::None; break;
		case 3: Shot(TEXT("front_03_home")); if (P) { P->Mutable().Chests[0].Type = FName(TEXT("royal")); P->Mutable().Chests[0].UnlockAtUtc = 1; P->Mutable().Chests[1].Type = FName(TEXT("wooden")); P->Mutable().Chests[1].UnlockAtUtc = 0; } break;
		case 4: TapChestSlot(0); break;
		case 5: Shot(TEXT("front_05_chest_rewards")); break;
		case 6: Pop = EPop::None; Go(1); break;
		case 7:
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAmmoTour")))
			{	// b223 preview: the Gatling Turret card with its AMMO row (rocket, then laser), then back to rocket
				static int32 Ai = 0;
				if (Ai < 5)
				{
					if (Ai == 0) { Tab = 2; b3D = false; ItemSel = 0; Pop = EPop::Item; PopT = T; if (P) P->Mutable().ItemCounts.FindOrAdd(FName(TEXT("turret_ammo"))) = 5; }
					else if (Ai == 1) Shot(TEXT("ammo_1_rocket"));
					else if (Ai == 2) { if (P) P->Mutable().ItemCounts.FindOrAdd(FName(TEXT("turret_ammo"))) = 4; }
					else if (Ai == 3) Shot(TEXT("ammo_2_laser"));
					else if (P) { P->Mutable().ItemCounts.FindOrAdd(FName(TEXT("turret_ammo"))) = 5; P->Save(); }
					++Ai; --TestStep; break;
				}
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBallerTour")))
			{	// b208: each Tripo Baller in the 3D view, one frame each
				static const TCHAR* Tn[] = { TEXT("Blue"), TEXT("Frosti"), TEXT("Lumi"), TEXT("Vex"), TEXT("Hotstreak"), TEXT("Dark") };
				static int32 Ti = 0;
				const TArray<FItem> Iv = Items();
				if (Ti < 12)
				{
					const int32 K = Ti / 2;
					if (Ti % 2 == 0) { Tab = 0; b3D = true; Pop = EPop::Item; PopT = T; for (int32 i = 0; i < Iv.Num(); ++i) if (Iv[i].Name.Equals(Tn[K], ESearchCase::IgnoreCase)) { ItemSel = i; break; } }
					else Shot(FString::Printf(TEXT("baller3d_%d_%s"), K, Tn[K]));
					++Ti; --TestStep; break;
				}
			}
			Shot(TEXT("front_06_inventory")); b3D = true; ItemSel = FMath::Min(8, Items().Num() - 1); Pop = EPop::Item; PopT = T; break; // b202: the new Dark Zoogi (was Vex, 3)
		case 8: Shot(TEXT("front_07_item")); { TArray<FItem> I = Items(); const int32 Ix = FMath::Min(8, I.Num() - 1); if (I.IsValidIndex(Ix)) ItemAction(I[Ix]); } break;
		case 9: Shot(TEXT("front_08_equipped")); Tab = 2; { TArray<FItem> I = Items(); if (I.IsValidIndex(1)) ItemAction(I[1]); } break; // b120: weapons tab, equip the grenade
		case 10: Shot(TEXT("front_09_weapons")); break; // b120: the shot shows this step's state - nothing else changes here
		case 11: Tab = 3; Pop = EPop::None; { TArray<FItem> I = Items(); if (I.IsValidIndex(16)) ToggleSkill(I[16]); } Shot(TEXT("front_10_cards")); break; // b65: the skill details panel (shots render after this frame's changes)
		// b66: a shot renders after this frame's changes, so every change gets its own step before the shot (Tasks/News
		// looked mis-wired in the b64 captures only because the shot and the next popup shared a step).
		case 12: Go(2); ShopX = 0.f; break;
		case 13: Shot(TEXT("front_11_shop")); break;
		case 14: ShopX = FMath::Min(ShopMax, 900.f); ShopCat = 1; break; // b172g phone: skins
		case 15: Shot(TEXT("front_12_shop2")); break;
		case 16: ShopX = ShopMax; ShopCat = 2; break; // b172g phone: chests
		case 17: Shot(TEXT("front_13_shop3")); break;
		case 18: Go(0); OpenPop(EPop::Tasks); break;
		case 19: Shot(TEXT("front_14_tasks")); break;
		case 20: OpenPop(EPop::News); break;
		case 21: Shot(TEXT("front_15_news")); break;
		case 22: OpenPop(EPop::Settings); break;
		case 23: Shot(TEXT("front_16_settings")); break;
		case 24: Pop = EPop::None; break;
		case 25: Shot(TEXT("front_17_home")); Battle(); break;
		case 27: Shot(TEXT("front_18_modeselect")); break;
		default: break;
		}
	}

	// ---------------------------------------------------------------- paint
	void AddHit(const FVector2D& TL, const FVector2D& Sz, TFunction<void()> Fn) const { Hits.Add({ FBox2D(TL, TL + Sz), MoveTemp(Fn) }); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 L0, const FWidgetStyle&, bool) const override
	{
		static const bool bCleanShot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrontClean")); // b209: the home arena with no menu on top (mockup backgrounds)
		if (bCleanShot) return L0;
		if (!GZSplashDone && SplashWanted())
		{	// b211: Evan's splash - the logo + PixelIntel / EvanJames / Unreal Engine, black around it, fading in and out
			const FVector2D Px0 = Geo.GetLocalSize();
			const double Now = FPlatformTime::Seconds(); if (GZSplashT0 < 0.0) GZSplashT0 = Now;
			const float Tt = (float)(Now - GZSplashT0);
			const float A = FMath::Clamp(FMath::Min(Tt / 0.35f, (SplashLen() - Tt) / 0.4f), 0.f, 1.f);
			ZRect(Out, Geo, L0, FVector2D::ZeroVector, Px0, FLinearColor::Black);
			if (UTexture2D* Sp = ZLoadPng(TEXT("Data/MenuArt/splash_zoogi")))
			{
				const float Ia = (float)Sp->GetSizeX() / FMath::Max(1, Sp->GetSizeY());
				FVector2D Sz(Px0.X, Px0.X / Ia); if (Sz.Y > Px0.Y) Sz = FVector2D(Px0.Y * Ia, Px0.Y); // contain: landscape fills, portrait letterboxes
				ZTex(Out, Geo, L0 + 1, Sp, (Px0 - Sz) * 0.5f, Sz, FLinearColor(1.f, 1.f, 1.f, A));
			}
			return L0 + 2;
		}
		const FVector2D Px = Geo.GetLocalSize();
		const bool bPortrait = Px.Y > Px.X;
		// b56 (Evan: "adjust menu to fit more inward"): everything sits inside a safe inset (rounded corners, notch, home bar)
		// b62: tall phones (iPhone ~19.5:9) get a deeper top / bottom inset for the Dynamic Island + home bar
		const bool bTall = bPortrait && Px.Y > 1.9f * Px.X;
		OffPx = FVector2D(Px.X * (bPortrait ? 0.05f : 0.04f), Px.Y * (bTall ? 0.06f : bPortrait ? 0.04f : 0.035f));
		ScrPx = Px;
		const FVector2D In(Px.X - 2.f * OffPx.X, Px.Y - OffPx.Y - Px.Y * (bTall ? 0.045f : bPortrait ? 0.035f : 0.03f));
		S = bPortrait ? In.X / 1700.f : In.Y / 1080.f;
		DW = In.X / S; DH = In.Y / S;
		const FGeometry G = Geo.MakeChild(FVector2D(DW, DH), FSlateLayoutTransform(S, OffPx));
		Hits.Reset();
		RobotBox = FBox2D(ForceInit);
		SZoogiFront* Self = const_cast<SZoogiFront*>(this);
		int32 L = L0;

		// halftone dots at the screen edges only (the arena shows in the middle)
		ZTiled(Out, G, L, ZDotsTex(), FVector2D(0.f, 0.f), FVector2D(DW * 0.16f, DH), 32.f, Mag().CopyWithNewOpacity(0.35f));
		ZTiled(Out, G, L, ZDotsTex(), FVector2D(DW * 0.84f, 0.f), FVector2D(DW * 0.16f, DH), 32.f, Mag().CopyWithNewOpacity(0.35f));
		L += 2;

		if (Screen == 0) PaintHome(Out, G, L, Self);
		else if (Screen == 1) PaintInventory(Out, G, L, Self);
		else PaintShop(Out, G, L, Self);
		L += 40;
		PaintTopBar(Out, G, L, Self);
		L += 10;
		if (Pop != EPop::None)
		{
			// a full-screen blocker first (taps outside a card close it), then the popup's own buttons on top
			ZRect(Out, G, L, FVector2D::ZeroVector, FVector2D(DW, DH), FLinearColor(0.01f, 0.01f, 0.05f, Pop == EPop::Chest ? 0.55f : 0.45f));
			AddHit(FVector2D::ZeroVector, FVector2D(DW, DH), [Self]() { if (Self->Pop == SZoogiFront::EPop::Item || Self->Pop == SZoogiFront::EPop::Trick || Self->Pop == SZoogiFront::EPop::Confirm || Self->Pop == SZoogiFront::EPop::LevelUp || Self->Pop == SZoogiFront::EPop::Tasks || Self->Pop == SZoogiFront::EPop::News || Self->Pop == SZoogiFront::EPop::Settings) Self->Pop = SZoogiFront::EPop::None; else if (Self->Pop == SZoogiFront::EPop::Chest) { if (Self->T - Self->PopT < Self->ChestSkipAt) { Self->PopT = Self->T - Self->ChestSkipAt; Self->ChestSfxStep = 999; } else Self->Pop = SZoogiFront::EPop::None; } });
			L += 2;
			if (Pop == EPop::Item) PaintItem(Out, G, L, Self);
			else if (Pop == EPop::Login) PaintLogin(Out, G, L, Self);
			else if (Pop == EPop::Tasks) PaintTasks(Out, G, L, Self);
			else if (Pop == EPop::News) PaintNews(Out, G, L, Self);
			else if (Pop == EPop::Settings) PaintSettings(Out, G, L, Self);
			else if (Pop == EPop::Confirm) PaintConfirm(Out, G, L, Self);
			else if (Pop == EPop::LevelUp) PaintLevelUp(Out, G, L, Self);
			else if (Pop == EPop::Trick) PaintTrick(Out, G, L, Self);
			else PaintChest(Out, G, L, Self);
			L += 90; // b166: the card fan stacks up to ~60 layers
		}
		if (T - ToastT < 2.4f && !Toast.IsEmpty())
		{
			const FSlateFontInfo F = Font(0, 38.f);
			const float W = ZMeasure(Toast, F).X + 70.f;
			const float A = FMath::Clamp((2.4f - (T - ToastT)) / 0.3f, 0.f, 1.f);
			const FVector2D TL(DW * 0.5f - W * 0.5f, 14.f); // b266 (Evan: nothing covered): toasts sit at the top edge, not over the middle of the screen
			ZRect(Out, G, L, TL + FVector2D(9.f, 9.f), FVector2D(W, 80.f), Mag().CopyWithNewOpacity(A));
			ZRect(Out, G, L + 1, TL - FVector2D(4.f, 4.f), FVector2D(W + 8.f, 88.f), Paper.CopyWithNewOpacity(A));
			ZRect(Out, G, L + 2, TL, FVector2D(W, 80.f), Ink.CopyWithNewOpacity(A));
			ZTextC(Out, G, L + 3, Toast, F, TL + FVector2D(W, 80.f) * 0.5f, Paper.CopyWithNewOpacity(A));
			L += 4;
		}
		return L;
	}

	void PaintTopBar(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const UZoogiFrontProfileSubsystem* P = FP();
		if (Screen == 0 && DH > DW) { PaintTopBarPortrait(Out, G, L, Self); return; }
		const int32 Lv = P ? P->Data().Level : 1, Xp = P ? P->Data().XP : 0, Need = P ? FMath::Max(1, UZoogiFrontProfileSubsystem::XPForLevel(Lv)) : 1;
		float X = 28.f;
		if (Screen != 0)
		{
			const bool bBigBack = DH > DW; // b172g (Evan: "Larger back button")
			X += ZTag(Out, G, L, TEXT("BACK"), FVector2D(X, bBigBack ? 10.f : 22.f), bBigBack ? 170.f : 64.f, true, Mag(), bBigBack ? 110.f : 36.f) + 24.f;
			AddHit(FVector2D(0.f, 0.f), FVector2D(X + 10.f, bBigBack ? 200.f : 90.f), [Self, bBigBack]() { Self->Sfx(TEXT("UiCancel")); if (bBigBack && Self->Screen == 2 && Self->ShopCat >= 0) { Self->ShopCat = -1; return; } Self->Go(0); });
			TArray<FString> Title;
			if (Screen == 1) Title = { TEXT("IN"), TEXT("VEN"), TEXT("TO"), TEXT("RY") }; else Title = { TEXT("S"), TEXT("H"), TEXT("O"), TEXT("P"), TEXT("!") };
			ZRansom(Out, G, L, Title, FVector2D(X + 10.f, 14.f), 58.f);
		}
		else
		{
			const FString Name = P ? P->Data().GuestName.ToUpper() : TEXT("PLAYER");
			const float W = ZTag(Out, G, L, FString::Printf(TEXT("LV %d   %s"), Lv, *Name), FVector2D(X, 22.f), 60.f, true, Mag(), 32.f);
			ZRect(Out, G, L, FVector2D(X, 92.f), FVector2D(W, 14.f), Ink);
			ZRect(Out, G, L + 1, FVector2D(X + 2.f, 94.f), FVector2D((W - 4.f) * FMath::Clamp((float)Xp / Need, 0.f, 1.f), 10.f), Mag());
			X += W + 30.f;
			ZTag(Out, G, L, FString::Printf(TEXT("TROPHIES %s"), *ZNumS(P ? P->Data().Trophies : 0)), FVector2D(X, 22.f), 60.f, false, Gold, 30.f);
		}
		// currencies + gear, right aligned
		const FString Coins = FString::Printf(TEXT("COINS  %s  +"), *ZNumS(P ? P->Data().Gold : 0));
		const FString Gems = FString::Printf(TEXT("GEMS  %s  +"), *ZNumS(P ? P->Gems() : 0));
		const float Wc = ZMeasure(Coins, Font(0, 30.f)).X + 40.f, Wg = ZMeasure(Gems, Font(0, 30.f)).X + 40.f;
		float R = DW - 30.f - (Screen == 0 ? 80.f : 0.f);
		ZTag(Out, G, L, Coins, FVector2D(R - Wc, 22.f), 60.f, true, Gold, 30.f);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"))) if (UTexture2D* Ic = ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin3d"))) ZTex(Out, G, L + 3, Ic, FVector2D(R - Wc - 46.f, 12.f), FVector2D(80.f, 80.f)); // b137
		AddHit(FVector2D(R - Wc, 16.f), FVector2D(Wc, 70.f), [Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(5); Self->ShopCat = 5; });
		R -= Wc + (FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons")) ? 26.f : 72.f); // b137: room for the coin icon
		ZTag(Out, G, L, Gems, FVector2D(R - Wg, 22.f), 60.f, true, Mag(), 30.f);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"))) if (UTexture2D* Ic = ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem3d"))) ZTex(Out, G, L + 3, Ic, FVector2D(R - Wg - 46.f, 12.f), FVector2D(80.f, 80.f)); // b137
		AddHit(FVector2D(R - Wg, 16.f), FVector2D(Wg, 70.f), [Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(4); Self->ShopCat = 4; });
		if (Screen == 0)
		{
			ZTag(Out, G, L, TEXT("SET"), FVector2D(DW - 100.f, 22.f), 60.f, false, Mag(), 28.f);
			AddHit(FVector2D(DW - 104.f, 16.f), FVector2D(100.f, 70.f), [Self]() { Self->OpenPop(SZoogiFront::EPop::Settings); });
		}
	}

	// b57 (Evan's portrait mock, 5e97e948-image.png): centred LV + XP bar box, then TROPHIES / GEMS / GOLD / SET boxes across
	void PaintTopBarPortrait(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const UZoogiFrontProfileSubsystem* P = FP();
		const int32 Lv = P ? P->Data().Level : 1, Xp = P ? P->Data().XP : 0, Need = P ? FMath::Max(1, UZoogiFrontProfileSubsystem::XPForLevel(Lv)) : 1;
		const FString Name = P ? P->Data().GuestName.ToUpper() : TEXT("PLAYER");
		{
			const FVector2D Sz(1000.f, 170.f), TL(DW * 0.5f - Sz.X * 0.5f, 10.f);
			ZPanel(Out, G, L, TL, Sz, Paper, Mag(), 6.f, 12.f);
			const FString LvTxt = FString::Printf(TEXT("LV %d   %s"), Lv, *Name);
			ZTextC(Out, G, L + 3, LvTxt, ZFitFont(LvTxt, 0, 76.f, Sz.X - 80.f), TL + FVector2D(Sz.X * 0.5f, 62.f), Ink);
			ZRect(Out, G, L + 3, TL + FVector2D(40.f, 118.f), FVector2D(Sz.X - 80.f, 26.f), Ink);
			ZRect(Out, G, L + 4, TL + FVector2D(44.f, 122.f), FVector2D((Sz.X - 88.f) * FMath::Clamp((float)Xp / Need, 0.f, 1.f), 18.f), Mag());
		}
		struct FBox { FString Top, Val; float W; FLinearColor Edge; int32 Act; };
		const FBox Boxes[4] = {
			{ TEXT("TROPHIES"), ZNumS(P ? P->Data().Trophies : 0), 400.f, Gold, -1 },
			{ TEXT("GEMS"), ZNumS(P ? P->Gems() : 0) + TEXT(" +"), 400.f, Mag(), 4 },
			{ TEXT("GOLD"), ZNumS(P ? P->Data().Gold : 0) + TEXT(" +"), 470.f, Gold, 5 },
			{ TEXT("SET"), TEXT(""), 190.f, Mag(), 99 } };
		const float Gap = 34.f, Y = 230.f, H = 170.f;
		float X = DW * 0.5f - (400.f + 400.f + 470.f + 190.f + 3.f * Gap) * 0.5f;
		for (const FBox& B : Boxes)
		{
			ZPanel(Out, G, L, FVector2D(X, Y), FVector2D(B.W, H), Ink, B.Edge, 6.f, 10.f);
			if (B.Val.IsEmpty()) ZTextC(Out, G, L + 3, B.Top, ZFitFont(B.Top, 0, 58.f, B.W - 30.f), FVector2D(X + B.W * 0.5f, Y + H * 0.5f), Paper);
			else
			{
				ZTextC(Out, G, L + 3, B.Top, ZFitFont(B.Top, 0, 44.f, B.W - 40.f), FVector2D(X + B.W * 0.5f, Y + 50.f), B.Edge == Gold ? Gold : Paper);
				ZTextC(Out, G, L + 3, B.Val, ZFitFont(B.Val, 0, 60.f, B.W - 40.f), FVector2D(X + B.W * 0.5f, Y + 116.f), Paper);
			}
			const int32 Act = B.Act;
			if (Act == 99) AddHit(FVector2D(X, Y), FVector2D(B.W, H), [Self]() { Self->OpenPop(SZoogiFront::EPop::Settings); });
			else if (Act >= 0) AddHit(FVector2D(X, Y), FVector2D(B.W, H), [Self, Act]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(Act); });
			X += B.W + Gap;
		}
	}

	void Slab(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& Label, const FVector2D& TL, const FVector2D& Sz, const FLinearColor& Body, const FLinearColor& Shadow, float Size, TFunction<void()> Fn) const
	{
		ZPanel(Out, G, L, TL, Sz, Body, Shadow, 5.f, 11.f);
		const FLinearColor TC = Body == Paper ? Ink : Paper;
		const FSlateFontInfo F = ZFitFont(Label, 0, Size, Sz.X - 30.f, Body == Paper ? 0.f : 3.f);
		ZTextC(Out, G, L + 3, Label, F, TL + Sz * 0.5f, TC);
		if (Fn) AddHit(TL - FVector2D(8.f, 8.f), Sz + FVector2D(16.f, 16.f), MoveTemp(Fn));
	}
	void Burst(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& Ctr, float D, const FString& A, const FString& B) const
	{
		ZTex(Out, G, L, ZBurstTex(), Ctr - FVector2D(D, D) * 0.5f - FVector2D(6.f, 6.f), FVector2D(D + 12.f, D + 12.f), Paper);
		ZTex(Out, G, L + 1, ZBurstTex(), Ctr - FVector2D(D, D) * 0.5f, FVector2D(D, D), Red);
		const FSlateFontInfo F = Font(3, D * 0.17f, 2.f);
		ZTextC(Out, G, L + 2, A, F, Ctr - FVector2D(0.f, D * 0.085f), Paper);
		ZTextC(Out, G, L + 2, B, F, Ctr + FVector2D(0.f, D * 0.095f), Paper);
	}

	// b57 (Evan's portrait mock): big TASKS / NEWS left, logo + hero name centre, NO ADS + SHOP right, the arena in the middle,
	// INVENTORY + the 4 chest slots in one row, a full-width BATTLE! at the bottom (no stripe band)
	void PaintHomePortrait(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const UZoogiFrontProfileSubsystem* P = FP();
		// b62 (Evan: keep the sizes, arrange better for iPhones): the extra height of a tall phone spreads the groups out
		const float Extra = FMath::Clamp(DH - 3100.f, 0.f, 900.f);
		const float Y0 = 450.f + Extra * 0.12f, Sp = Extra * 0.08f;
		Slab(Out, G, L, TEXT("TASKS"), FVector2D(10.f, Y0), FVector2D(470.f, 170.f), Ink, Mag(), 72.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::Tasks); });
		Slab(Out, G, L, TEXT("NEWS"), FVector2D(10.f, Y0 + 220.f + Sp), FVector2D(470.f, 170.f), Ink, Mag(), 72.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::News); });
		if (UTexture2D* Lg = ZLoadPng(TEXT("Data/MenuArt/") + ZoogiUtil::SeasonLogoKey())) ZTexFit(Out, G, L, Lg, FVector2D(DW * 0.5f - 300.f, Y0 - 20.f), FVector2D(600.f, 400.f));
		{
			const FString Hero = ZoogiFront::Equipped(Ctx.Get()).Name;
			TArray<FString> Letters; for (TCHAR C : Hero.ToUpper()) Letters.Add(FString(1, &C));
			const float W = Letters.Num() * 82.f;
			ZRansom(Out, G, L, Letters, FVector2D(DW * 0.5f - W * 0.5f, Y0 + 400.f + Sp * 0.6f), 70.f);
		}
		Burst(Out, G, L, FVector2D(DW - 250.f, Y0 + 120.f), 340.f, TEXT("NO ADS"), TEXT("-80%!"));
		AddHit(FVector2D(DW - 420.f, Y0 - 50.f), FVector2D(340.f, 340.f), [Self]() { Self->Go(2); Self->ShopX = 0.f; Self->ShopCat = -1; });
		Slab(Out, G, L, TEXT("SHOP"), FVector2D(DW - 500.f, Y0 + 330.f + Sp), FVector2D(490.f, 170.f), Paper, Mag(), 80.f, [Self]() { Self->Go(2); });
		// bottom: full-width BATTLE!, INVENTORY + chests above it
		const FVector2D BTL(40.f, DH - 280.f - Extra * 0.1f), BSz(DW - 80.f, 250.f);
		Slab(Out, G, L + 3, TEXT("BATTLE!"), BTL, BSz, Mag(), Paper, 140.f, [Self]() { Self->Battle(); });
		const float RY = BTL.Y - 290.f - Extra * 0.05f, RH = 250.f;
		Slab(Out, G, L + 3, TEXT("INVENTORY"), FVector2D(10.f, RY + 20.f), FVector2D(470.f, 210.f), Paper, Mag(), 72.f, [Self]() { Self->Go(1); });
		Slab(Out, G, L + 3, TEXT("SKILL LAB"), FVector2D(10.f, RY - 190.f), FVector2D(470.f, 150.f), Ink, Mag(), 64.f, [Self]() { Self->SkillLab(); }); // b108
		const int32 NS = P ? P->Data().Chests.Num() : 4;
		const float X0 = 520.f, CG = 24.f, CW = FMath::Min(280.f, (DW - 10.f - X0 - (NS - 1) * CG) / FMath::Max(1, NS));
		for (int32 i = 0; i < NS; ++i)
		{
			const FVector2D TL(X0 + i * (CW + CG), RY);
			const FZoogiFrontChestSlot C = P ? P->Data().Chests[i] : FZoogiFrontChestSlot();
			const bool bEmpty = C.Type.IsNone();
			const bool bReady = P && !bEmpty && P->IsReady(i);
			const int64 Left = P && !bEmpty ? P->SecondsLeft(i) : -1;
			const FLinearColor SlotC = bReady ? Mag() : Left < 0 ? FLinearColor::FromSRGBColor(FColor(30, 170, 200)) : FLinearColor::FromSRGBColor(FColor(16, 104, 128));
			auto Dash = [&](const FVector2D& P0, const FVector2D& Sz, float Th)
			{	// b172a: an empty slot is just a dashed outline (the room shows through)
				const FLinearColor Dc = FLinearColor::FromSRGBColor(FColor(200, 190, 230)); const float Dl = 14.f, Gp = 9.f;
				for (float X = 0.f; X < Sz.X; X += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.X - X); ZRect(Out, G, L + 3, P0 + FVector2D(X, 0.f), FVector2D(Ln, Th), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(X, Sz.Y - Th), FVector2D(Ln, Th), Dc); }
				for (float Y = 0.f; Y < Sz.Y; Y += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.Y - Y); ZRect(Out, G, L + 3, P0 + FVector2D(0.f, Y), FVector2D(Th, Ln), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(Sz.X - Th, Y), FVector2D(Th, Ln), Dc); }
			};
			if (bEmpty) { ZRect(Out, G, L + 2, TL, FVector2D(CW, RH), FLinearColor(0.02f, 0.015f, 0.05f, 0.25f)); Dash(TL, FVector2D(CW, RH), 4.f); }
			else ZPanel(Out, G, L + 3, TL, FVector2D(CW, RH), SlotC, Ink, 5.f, 9.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 64.f), FLinearColor(0.012f, 0.008f, 0.03f, 0.92f));
			ZTextC(Out, G, L + 7, Head, ZFitFont(Head, 0, 44.f, CW - 16.f), TL + FVector2D(CW * 0.5f, 32.f), Paper);
			if (!bEmpty) ZChest(Out, G, L + 7, C.Type, TL + FVector2D(CW * 0.5f, 160.f), 130.f * (bReady ? 1.f + 0.06f * FMath::Sin(T * 8.f) : 1.f));
			AddHit(TL - FVector2D(6.f, 6.f), FVector2D(CW + 12.f, RH + 12.f), [Self, i]() { Self->TapChestSlot(i); });
		}
		// the robot is hidden on home (b56); keep the stage numbers sane anyway
		ZoogiFront::FView& V = ZoogiFront::View();
		V.Feet01 = ScrN(FVector2D(DW * 0.5f, RY - 40.f));
		V.Height01 = 0.3f;
		RobotBox = FBox2D(ForceInit);
	}

	TArray<FItem> ItemsOfTab(int32 InTab) const
	{	// b209: the inventory list of another tab (Items() reads Tab)
		int32& Tb = const_cast<int32&>(Tab); const int32 Was = Tb; Tb = InTab;
		TArray<FItem> R = Items(); Tb = Was; return R;
	}
	static UTexture2D* ItemArt(const FItem& I)
	{
		if (I.Kind == 0) return ZBallerArt(I.Ball);
		if (I.Kind == 1) return ZBallIcon(I.Ball);
		if (I.Kind == 3) return ZLoadPng(TEXT("Data/CardArt/Icons/") + I.CardId);
		return I.Icon.IsEmpty() ? nullptr : ZLoadPng(I.Icon);
	}
	// b209 (Evan: "Home B"): the LOADOUT card - Baller, Zoogi, Item, Skill 1, Skill 2 with the set's POWER and a CHANGE on every row
	void PaintLoadout(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		struct FRow { FItem I; int32 Tab = 0; const TCHAR* Tag = TEXT(""); FLinearColor Col; bool bHave = false; };
		TArray<FRow> Rows;
		auto Pick = [&](int32 InTab, const TCHAR* InTag, const FLinearColor& InCol, int32 Nth)
		{
			FRow R; R.Tab = InTab; R.Tag = InTag; R.Col = InCol;
			TArray<FItem> Iv = ItemsOfTab(InTab);
			if (InTab == 3) { Iv = Iv.FilterByPredicate([](const FItem& X) { return X.bEquipped; }); Iv.Sort([](const FItem& A, const FItem& B) { return A.HandSlot < B.HandSlot; }); if (Iv.IsValidIndex(Nth)) { R.I = Iv[Nth]; R.bHave = true; } }
			else for (const FItem& X : Iv) if (X.bEquipped) { R.I = X; R.bHave = true; break; }
			Rows.Add(R);
		};
		const FLinearColor Blue = FLinearColor::FromSRGBColor(FColor(60, 130, 255)), Purp = FLinearColor::FromSRGBColor(FColor(150, 80, 255)), Orng = FLinearColor::FromSRGBColor(FColor(255, 140, 30)), Cyan = FLinearColor::FromSRGBColor(FColor(70, 210, 255));
		Pick(0, TEXT("BALLER"), Blue, 0); Pick(1, TEXT("ZOOGI"), Purp, 0); Pick(2, TEXT("ITEM"), Orng, 0); Pick(3, TEXT("SKILL 1"), Cyan, 0); Pick(3, TEXT("SKILL 2"), Cyan, 1);
		int32 Power = 0; for (const FRow& R : Rows) if (R.bHave) Power += (R.I.Rarity + 1) * 150 + R.I.Level * 40;
		const float CW = 660.f, X0 = DW - CW - 40.f, Y0 = 118.f, RH = 104.f, HH = 74.f;
		const float CH = HH + Rows.Num() * RH + 20.f;
		ZPanel(Out, G, L, FVector2D(X0, Y0), FVector2D(CW, CH), Ink, Mag(), 5.f, 11.f);
		ZRect(Out, G, L + 3, FVector2D(X0, Y0), FVector2D(CW, HH), FLinearColor::FromSRGBColor(FColor(40, 24, 90)));
		ZTextAt(Out, G, L + 4, TEXT("LOADOUT"), Font(0, 46.f), FVector2D(X0 + 24.f, Y0 + 8.f), Paper);
		{ const FString Pw = FString::Printf(TEXT("POWER %s"), *ZNumS(Power)); const FSlateFontInfo F = Font(0, 34.f); ZTextAt(Out, G, L + 4, Pw, F, FVector2D(X0 + CW - 24.f - ZMeasure(Pw, F).X, Y0 + 16.f), Gold); }
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const FRow& R = Rows[r];
			const FVector2D TL(X0 + 14.f, Y0 + HH + 10.f + r * RH);
			const FVector2D Sz(CW - 28.f, RH - 12.f);
			ZRect(Out, G, L + 3, TL - FVector2D(3.f, 3.f), Sz + FVector2D(6.f, 6.f), R.Col);
			ZRect(Out, G, L + 4, TL, Sz, FLinearColor::FromSRGBColor(FColor(26, 30, 70)));
			ZRect(Out, G, L + 5, TL + FVector2D(8.f, 8.f), FVector2D(Sz.Y - 16.f, Sz.Y - 16.f), Paper);
			if (R.bHave) if (UTexture2D* Tx = ItemArt(R.I)) ZTex(Out, G, L + 6, Tx, TL + FVector2D(11.f, 11.f), FVector2D(Sz.Y - 22.f, Sz.Y - 22.f));
			const float Tx0 = TL.X + Sz.Y + 10.f, TxW = Sz.X - Sz.Y - 190.f;
			ZTextAt(Out, G, L + 6, R.Tag, Font(0, 17.f), FVector2D(Tx0, TL.Y + 2.f), R.Col);
			const FString Nm = R.bHave ? R.I.Name.ToUpper() : FString(TEXT("EMPTY - TAP CHANGE"));
			ZTextAt(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 29.f, TxW), FVector2D(Tx0, TL.Y + 22.f), R.bHave ? Paper : FLinearColor(0.6f, 0.62f, 0.75f, 1.f));
			if (R.bHave && !R.I.Sub.IsEmpty()) ZTextAt(Out, G, L + 6, R.I.Sub.ToUpper(), ZFitFont(R.I.Sub.ToUpper(), 0, 16.f, TxW), FVector2D(Tx0, TL.Y + 64.f), FLinearColor(0.75f, 0.78f, 0.9f, 1.f));
			const int32 T2 = R.Tab;
			Slab(Out, G, L + 6, TEXT("CHANGE"), FVector2D(TL.X + Sz.X - 160.f, TL.Y + 18.f), FVector2D(146.f, 56.f), Green, Ink, 28.f, [Self, T2]() { Self->Go(1); Self->Tab = T2; Self->Sfx(TEXT("UiClick"), 0.8f, 1.1f); });
		}
		// SHOP sits under the card now (the NO ADS burst lives in the shop)
		Slab(Out, G, L, TEXT("SHOP"), FVector2D(X0, Y0 + CH + 30.f), FVector2D(230.f, 90.f), Paper, Mag(), 48.f, [Self]() { Self->Go(2); });
	}
	// b211 (Evan: "WALK 6 - BLUE BUDDY"): Blue in the corner of the home screen with a tip; tap him for the next tip,
	// the last tip replays the guided first match
	void PaintBuddy(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self, float BY) const
	{
		static const TCHAR* Tips[] = {
			TEXT("Tap CHANGE on your LOADOUT to swap your Baller, Zoogi, Item or Skills."),
			TEXT("New: the DARK ZOOGI - find it in the SHOP!"),
			TEXT("Chests unlock while you play - tap a slot to start one."),
			TEXT("RAID: team up vs a giant boss. Hit its glowing weak point!"),
			TEXT("Aim at a wall - the BANK! line shows where you go after the bounce."),
			TEXT("Fill AWAKEN to unleash your Zoogi's ultimate power."),
			TEXT("Tap me again to replay the tutorial match."),
		};
		const int32 N = UE_ARRAY_COUNT(Tips);
		const int32 Ti = ((BuddyTip % N) + N) % N;
		const float HS = 132.f, Bob = 5.f * FMath::Sin(T * 2.4f);
		const FVector2D HTL(40.f, BY - 200.f + Bob);
		if (UTexture2D* Hd = ZLoadPng(TEXT("Data/MenuArt/Front/zoogi_blue"))) ZTex(Out, G, L + 4, Hd, HTL, FVector2D(HS, HS));
		// the bubble
		const FVector2D BTL(HTL.X + HS - 6.f, BY - 206.f), BSz(470.f, 142.f); // b266: room for the caption under the 3 lines
		ZPanel(Out, G, L + 4, BTL, BSz, Paper, Mag(), 4.f, 8.f);
		const FString Tip = Tips[Ti];
		TArray<FString> Words; Tip.ParseIntoArray(Words, TEXT(" "));
		TArray<FString> Lines; FString Cur; const FSlateFontInfo F = Font(1, 20.f);
		for (const FString& Wd : Words) { const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd; if (!Cur.IsEmpty() && ZMeasure(Try, F).X > BSz.X - 30.f) { Lines.Add(Cur); Cur = Wd; } else Cur = Try; }
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		for (int32 k = 0; k < FMath::Min(3, Lines.Num()); ++k) ZTextAt(Out, G, L + 7, Lines[k], F, BTL + FVector2D(14.f, 10.f + k * 31.f), Ink);
		{ const FString Cap = FString::Printf(TEXT("BLUE  %d/%d  - TAP ME"), Ti + 1, N); const FVector2D Cs = ZMeasure(Cap, Font(0, 15.f)); ZTextAt(Out, G, L + 7, Cap, Font(0, 15.f), BTL + FVector2D(BSz.X - Cs.X - 14.f, BSz.Y - Cs.Y - 6.f), FLinearColor(0.55f, 0.5f, 0.6f, 1.f)); } // b266: inside the bubble
		AddHit(HTL - FVector2D(10.f, 10.f), FVector2D(HS + BSz.X + 20.f, HS + 20.f), [Self, Ti, N]()
		{
			if (Ti == N - 1) { if (AZoogiGameMode* Gm = Self->GM()) { UE_LOG(LogZoogi, Log, TEXT("BUDDY: replay the tutorial")); ZoogiTutorial::Begin(Gm); } Self->BuddyTip = 0; return; }
			Self->BuddyTip = Ti + 1; Self->Sfx(TEXT("UiClick"), 0.8f, 1.2f);
		});
	}
	void PaintHome(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		if (DH > DW) { PaintHomePortrait(Out, G, L, Self); return; }
		const UZoogiFrontProfileSubsystem* P = FP();
		// season logo under the level tag
		if (UTexture2D* Lg = ZLoadPng(TEXT("Data/MenuArt/") + ZoogiUtil::SeasonLogoKey())) ZTexFit(Out, G, L, Lg, FVector2D(250.f, 120.f), FVector2D(250.f, 230.f));
		// left column
		Slab(Out, G, L, TEXT("TASKS"), FVector2D(40.f, 140.f), FVector2D(180.f, 86.f), Ink, Mag(), 40.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::Tasks); });
		Slab(Out, G, L, TEXT("NEWS"), FVector2D(40.f, 262.f), FVector2D(180.f, 86.f), Ink, Mag(), 40.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::News); });
		Slab(Out, G, L, TEXT("SKILL LAB"), FVector2D(40.f, 384.f), FVector2D(180.f, 86.f), Ink, Mag(), 30.f, [Self]() { Self->SkillLab(); }); // b108
		// the hero's name over the robot (ransom, letter blocks)
		const FString Hero = ZoogiFront::Equipped(Ctx.Get()).Name;
		{
			TArray<FString> Letters; for (TCHAR C : Hero.ToUpper()) Letters.Add(FString(1, &C));
			const float W = Letters.Num() * 66.f;
			ZRansom(Out, G, L, Letters, FVector2D((ZHomeB() ? DW * 0.40f : DW * 0.5f) - W * 0.5f, 120.f), 56.f);
		}
		if (ZHomeB()) PaintLoadout(Out, G, L, Self);
		else
		{	// right column: NO ADS burst + SHOP
			Burst(Out, G, L, FVector2D(DW - 175.f, 245.f), 230.f, TEXT("NO ADS"), TEXT("-80%!"));
			AddHit(FVector2D(DW - 290.f, 130.f), FVector2D(230.f, 230.f), [Self]() { Self->Go(2); Self->ShopX = 0.f; Self->ShopCat = -1; });
			Slab(Out, G, L, TEXT("SHOP"), FVector2D(DW - 270.f, 400.f), FVector2D(210.f, 96.f), Paper, Mag(), 50.f, [Self]() { Self->Go(2); });
		}
		// bottom: INVENTORY / 4 chest slots / BATTLE, stripe band
		const float BY = DH - 190.f;
		{
			const float Sl = FMath::Fmod(T * 90.f, 120.f);
			ZRect(Out, G, L, FVector2D(0.f, DH - 54.f), FVector2D(DW, 54.f), Ink);
			for (float X = -120.f + Sl; X < DW; X += 120.f) ZRect(Out, G, L + 1, FVector2D(X, DH - 54.f), FVector2D(52.f, 54.f), Mag());
		}
		Slab(Out, G, L + 3, TEXT("INVENTORY"), FVector2D(40.f, BY - 10.f), FVector2D(330.f, 110.f), Paper, Mag(), 54.f, [Self]() { Self->Go(1); });
		const int32 NS = P ? P->Data().Chests.Num() : 4;
		const float CW = 150.f, CG = 22.f, CX0 = DW * 0.5f - (NS * CW + (NS - 1) * CG) * 0.5f;
		for (int32 i = 0; i < NS; ++i)
		{
			const FVector2D TL(CX0 + i * (CW + CG), BY - 20.f);
			const FZoogiFrontChestSlot C = P ? P->Data().Chests[i] : FZoogiFrontChestSlot();
			const bool bEmpty = C.Type.IsNone();
			const bool bReady = P && !bEmpty && P->IsReady(i);
			const int64 Left = P && !bEmpty ? P->SecondsLeft(i) : -1;
			const FLinearColor SlotC = bReady ? Mag() : Left < 0 ? FLinearColor::FromSRGBColor(FColor(30, 170, 200)) : FLinearColor::FromSRGBColor(FColor(16, 104, 128));
			auto Dash = [&](const FVector2D& P0, const FVector2D& Sz, float Th)
			{	// b172a: an empty slot is just a dashed outline (the room shows through)
				const FLinearColor Dc = FLinearColor::FromSRGBColor(FColor(200, 190, 230)); const float Dl = 14.f, Gp = 9.f;
				for (float X = 0.f; X < Sz.X; X += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.X - X); ZRect(Out, G, L + 3, P0 + FVector2D(X, 0.f), FVector2D(Ln, Th), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(X, Sz.Y - Th), FVector2D(Ln, Th), Dc); }
				for (float Y = 0.f; Y < Sz.Y; Y += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.Y - Y); ZRect(Out, G, L + 3, P0 + FVector2D(0.f, Y), FVector2D(Th, Ln), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(Sz.X - Th, Y), FVector2D(Th, Ln), Dc); }
			};
			if (bEmpty) { ZRect(Out, G, L + 2, TL, FVector2D(CW, 150.f), FLinearColor(0.02f, 0.015f, 0.05f, 0.25f)); Dash(TL, FVector2D(CW, 150.f), 3.f); }
			else ZPanel(Out, G, L + 3, TL, FVector2D(CW, 150.f), SlotC, Ink, 4.f, 8.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 38.f), FLinearColor(0.012f, 0.008f, 0.03f, 0.92f));
			ZTextC(Out, G, L + 7, Head, ZFitFont(Head, 0, 26.f, CW - 10.f), TL + FVector2D(CW * 0.5f, 19.f), Paper);
			if (!bEmpty) ZChest(Out, G, L + 7, C.Type, TL + FVector2D(CW * 0.5f, 98.f), 84.f * (bReady ? 1.f + 0.06f * FMath::Sin(T * 8.f) : 1.f));
			AddHit(TL - FVector2D(6.f, 6.f), FVector2D(CW + 12.f, 162.f), [Self, i]() { Self->TapChestSlot(i); });
		}
		Slab(Out, G, L + 3, TEXT("BATTLE!"), FVector2D(DW - 470.f, BY - 30.f), FVector2D(420.f, 136.f), Mag(), Paper, 78.f, [Self]() { Self->Battle(); });
		if (ZoogiTutorial::Done() || FParse::Param(FCommandLine::Get(), TEXT("ZoogiBuddyTest"))) PaintBuddy(Out, G, L, Self, BY);
		// robot area (tap = cheer, drag = spin) + where the stage should stand it
		const float FeetY = ZHomeB() ? BY - 80.f : BY - 44.f, TopY = ZHomeB() ? 290.f : 205.f; // b209 Home B: a smaller hero clear of the chests
		const float HeroX = ZHomeB() ? DW * 0.40f : DW * 0.5f; // b209 Home B: the hero stands left of the LOADOUT card
		RobotBox = FBox2D(FVector2D(HeroX - 230.f, TopY), FVector2D(HeroX + 230.f, FeetY));
		ZoogiFront::FView& V = ZoogiFront::View();
		V.Feet01 = ScrN(FVector2D(HeroX, FeetY));
		V.Height01 = (FeetY - TopY) * S / FMath::Max(1.f, ScrPx.Y);
		if (DH > DW) V.Height01 = FMath::Min(V.Height01, 0.36f * DW / FMath::Max(1.f, DH) * 1.9f); // portrait: the robot stays a sensible size
	}

	void Tile(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FItem& I, const FVector2D& TL, float W) const
	{
		ZPanel(Out, G, L, TL, FVector2D(W, W), Paper, I.bEquipped ? Mag() : Ink, 4.f, 8.f);
		ZRect(Out, G, L + 3, TL, FVector2D(W, 8.f), ZRarityCol(I.Rarity));
		const float NB = FMath::Max(34.f, W * 0.17f);
		UTexture2D* Img = !I.Ball.IsEmpty() ? (I.Kind == 0 ? ZBallerArt(I.Ball) : ZBallIcon(I.Ball)) : !I.CardId.IsEmpty() ? ZLoadPng(TEXT("Data/CardArt/") + I.CardId) : !I.Icon.IsEmpty() ? ZLoadPng(I.Icon) : nullptr; // b120: weapon renders
		const FLinearColor Dim = (I.bOwned && !(I.Kind == 2 && I.Count == 0)) ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f); // b228: an empty item is grey
		// b65 (Evan: "Fill the white with a more visible image"): skills fill the whole tile with their art (Data/CardArt/Icons)
		UTexture2D* SkIcon = I.Kind == 3 ? ZLoadPng(TEXT("Data/CardArt/Icons/") + I.CardId) : nullptr;
		if (SkIcon) { ZRect(Out, G, L + 2, TL + FVector2D(0.f, 8.f), FVector2D(W, W - NB - 8.f), Ink); ZTex(Out, G, L + 3, SkIcon, TL + FVector2D(3.f, 8.f), FVector2D(W - 6.f, W - NB - 8.f), Dim); }
		else if (Img)
		{
			static const bool bPlain = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"));
			if ((!I.Ball.IsEmpty() || I.Kind == 2) && !bPlain) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers"))) ZTex(Out, G, L + 2, Bg, TL + FVector2D(0.f, 8.f), FVector2D(W, W - NB - 8.f), Dim); // b136 (Evan): ZOOGI ROLL ARENA burst
			ZTexFit(Out, G, L + 3, Img, TL + FVector2D(W * 0.12f, 14.f), FVector2D(W * 0.76f, W - NB - 22.f), Dim);
		}
		else ZTextC(Out, G, L + 3, I.Name.Left(1), Font(0, W * 0.38f), TL + FVector2D(W * 0.5f, (W - NB) * 0.52f), I.bOwned ? Ink : FLinearColor(0.5f, 0.5f, 0.5f));
		const FString NameLine = I.bEquipped ? FString::Printf(TEXT("%s - ON"), *I.Name) : !I.bOwned && I.Price > 0 ? FString::Printf(TEXT("%s %s"), I.PriceKind == 1 ? TEXT("GEMS") : TEXT("GOLD"), *ZNumS(I.Price)) : !I.bOwned ? FString(TEXT("LOCKED")) : I.Name;
		ZRect(Out, G, L + 4, TL + FVector2D(0.f, W - NB), FVector2D(W, NB), I.bEquipped ? Mag() : Ink);
		ZTextC(Out, G, L + 5, NameLine, ZFitFont(NameLine, 0, NB * 0.62f, W - 12.f), TL + FVector2D(W * 0.5f, W - NB * 0.5f), Paper);
		if (I.Kind == 2 && I.Count >= 0)
		{	// b227 (Evan picked ITEMS option A): how many you carry - a gold-rimmed pill, top-right
			const FString Cn = FString::Printf(TEXT("x%d"), I.Count);
			const float Fs = FMath::Max(28.f, W * 0.2f), Pw = ZMeasure(Cn, Font(0, Fs)).X + Fs * 0.7f, Ph = Fs * 1.3f; // b228: as big as the mockup
			const FVector2D Pt = TL + FVector2D(W - Pw - 8.f, 14.f);
			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), FLinearColor(0.06f, 0.05f, 0.16f, 1.f)); // b229: dark rim
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f)); // b229: solid gold pill (option A)
			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? FLinearColor(0.06f, 0.05f, 0.16f, 1.f) : FLinearColor(0.8f, 0.8f, 0.85f));
			if (I.Count == 0)
			{	// b228: none left - where to get more
				const FString Gt = TEXT("GET FROM CHESTS"); const float Gf = FMath::Max(16.f, W * 0.085f), Gw = FMath::Min(W - 16.f, ZMeasure(Gt, Font(0, Gf)).X + 20.f);
				ZRect(Out, G, L + 6, TL + FVector2D(W * 0.5f - Gw * 0.5f, W * 0.36f), FVector2D(Gw, Gf * 1.6f), FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));
				ZTextC(Out, G, L + 7, Gt, ZFitFont(Gt, 0, Gf, Gw - 8.f), TL + FVector2D(W * 0.5f, W * 0.36f + Gf * 0.8f), FLinearColor(0.85f, 0.85f, 0.9f));
			}
			if (I.NewN > 0)
			{	// b228: +N NEW (from a chest), top-left, until you open the item
				const FString Nw = FString::Printf(TEXT("+%d NEW"), I.NewN); const float Nf = FMath::Max(20.f, W * 0.12f), Nwid = ZMeasure(Nw, Font(0, Nf)).X + 18.f;
				const float Ny = W - NB - Nf * 1.35f - 6.f; // b266: just above the name band, clear of the count pill
				ZRect(Out, G, L + 6, TL + FVector2D(6.f, Ny), FVector2D(Nwid, Nf * 1.35f), FLinearColor(0.27f, 0.88f, 0.43f, 1.f));
				ZTextC(Out, G, L + 7, Nw, Font(0, Nf), TL + FVector2D(6.f + Nwid * 0.5f, Ny + Nf * 0.68f), Paper);
			}
		}
		if (I.Kind == 3 && I.HandSlot >= 0)
		{
			// b63: hand slot badge (top right)
			const float Bd = FMath::Max(40.f, W * 0.22f);
			ZPanel(Out, G, L + 6, TL + FVector2D(W - Bd - 4.f, 12.f), FVector2D(Bd, Bd), Mag(), Paper, 3.f, 4.f);
			ZTextC(Out, G, L + 7, FString::FromInt(I.HandSlot + 1), Font(0, Bd * 0.62f), TL + FVector2D(W - Bd * 0.5f - 4.f, 12.f + Bd * 0.5f), Paper);
		}
		if (I.Kind == 3 && I.bOwned)
		{
			const FString Lv = FString::Printf(TEXT("LV %d"), I.Level);
			ZRect(Out, G, L + 5, TL + FVector2D(0.f, 8.f), FVector2D(ZMeasure(Lv, Font(0, 22.f)).X + 16.f, 30.f), Mag());
			ZTextAt(Out, G, L + 6, Lv, Font(0, 22.f), TL + FVector2D(8.f, 9.f), Paper);
		}
	}

	// ---- b238: the Grok-mockup cards (see ZInvNew)
	UTexture2D* InvFrame(const TCHAR* N) const { if (S > 1.45f) if (UTexture2D* T2 = ZInv(*(FString(N) + TEXT("_2x")))) return T2; return ZInv(N); }
	float InvIn() const { return (S > 1.45f ? 18.f : 9.f) / FMath::Max(0.2f, S); } // the frame's band, in design units
	void TileN(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FItem& I, const FVector2D& TL, float W) const
	{
		const float In = InvIn(), NB = FMath::Max(34.f, W * 0.19f);
		const bool bNone = I.Kind == 2 && I.Count == 0;
		const FLinearColor Dim = (I.bOwned && !bNone) ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
		const FVector2D ITL = TL + FVector2D(In, In), ISz(W - 2.f * In, W - 2.f * In);
		ZTex(Out, G, L, ZInv(I.Kind == 1 ? TEXT("item_bg_epic") : TEXT("item_bg")), ITL, ISz, Dim);
		UTexture2D* Img = !I.Ball.IsEmpty() ? (I.Kind == 0 ? ZBallerArt(I.Ball) : ZBallIcon(I.Ball)) : !I.CardId.IsEmpty() ? ZLoadPng(TEXT("Data/CardArt/") + I.CardId) : !I.Icon.IsEmpty() ? ZLoadPng(I.Icon) : nullptr;
		UTexture2D* SkIcon = I.Kind == 3 ? ZLoadPng(TEXT("Data/CardArt/Icons/") + I.CardId) : nullptr;
		if (SkIcon) ZTex(Out, G, L + 1, SkIcon, ITL, FVector2D(ISz.X, ISz.Y - NB * 0.6f), Dim);
		else if (Img) ZTexFit(Out, G, L + 1, Img, ITL + FVector2D(ISz.X * 0.07f, ISz.Y * 0.03f), FVector2D(ISz.X * 0.86f, ISz.Y - NB - ISz.Y * 0.04f), Dim);
		else ZTextC(Out, G, L + 1, I.Name.Left(1), Font(0, W * 0.38f, 3.f), TL + FVector2D(W * 0.5f, (W - NB) * 0.52f), Paper);
		const FString NameLine = I.bEquipped ? FString::Printf(TEXT("%s - ON"), *I.Name) : !I.bOwned && I.Price > 0 ? FString::Printf(TEXT("%s %s"), I.PriceKind == 1 ? TEXT("GEMS") : TEXT("GOLD"), *ZNumS(I.Price)) : !I.bOwned ? FString(TEXT("LOCKED")) : I.Name;
		ZTex(Out, G, L + 2, ZInv(I.bEquipped ? TEXT("chip_on") : TEXT("plate")), FVector2D(TL.X + In * 0.4f, TL.Y + W - In - NB), FVector2D(W - In * 0.8f, NB));
		ZTextC(Out, G, L + 3, NameLine.ToUpper(), ZFitFont(NameLine.ToUpper(), 0, NB * 0.6f, W - 2.f * In - 34.f, 2.f), FVector2D(TL.X + W * 0.5f, TL.Y + W - In - NB * 0.5f), Paper);
		ZBox9(Out, G, L + 4, InvFrame(I.bEquipped ? TEXT("card_frame_mag") : TEXT("card_frame")), TL, FVector2D(W, W), 0.1875f);
		if (I.Kind == 2 && I.Count >= 0)
		{	// ITEMS option A: the carried count, top-right
			const FString Cn = FString::Printf(TEXT("x%d"), I.Count);
			const float Fs = FMath::Max(28.f, W * 0.2f), Pw = ZMeasure(Cn, Font(0, Fs)).X + Fs * 1.0f, Ph = Fs * 1.3f;
			const FVector2D Pt = TL + FVector2D(W - Pw - In - 2.f, In + 4.f);
			ZTex(Out, G, L + 6, ZInv(TEXT("pill")), Pt, FVector2D(Pw, Ph), I.Count > 0 ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.6f, 1.f));
			ZTextC(Out, G, L + 7, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
			if (I.Count == 0)
			{
				const FString Gt = TEXT("GET FROM CHESTS"); const float Gf = FMath::Max(16.f, W * 0.085f), Gw = FMath::Min(W - 16.f, ZMeasure(Gt, Font(0, Gf)).X + 20.f);
				ZRect(Out, G, L + 6, TL + FVector2D(W * 0.5f - Gw * 0.5f, W * 0.36f), FVector2D(Gw, Gf * 1.6f), FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));
				ZTextC(Out, G, L + 7, Gt, ZFitFont(Gt, 0, Gf, Gw - 8.f), TL + FVector2D(W * 0.5f, W * 0.36f + Gf * 0.8f), FLinearColor(0.85f, 0.85f, 0.9f));
			}
			if (I.NewN > 0)
			{
				const FString Nw = FString::Printf(TEXT("+%d NEW"), I.NewN); const float Nf = FMath::Max(20.f, W * 0.12f), Nwid = ZMeasure(Nw, Font(0, Nf)).X + 18.f;
				const float Ny = W - In - NB - Nf * 1.35f - 6.f; // b266: just above the name band, clear of the count pill
				ZRect(Out, G, L + 6, TL + FVector2D(In + 2.f, Ny), FVector2D(Nwid, Nf * 1.35f), FLinearColor(0.27f, 0.88f, 0.43f, 1.f));
				ZTextC(Out, G, L + 7, Nw, Font(0, Nf), TL + FVector2D(In + 2.f + Nwid * 0.5f, Ny + Nf * 0.68f), Paper);
			}
		}
	}
	static float SkillCardNH(float W) { return W * 0.92f; }
	void SkillCardN(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FItem& I, const FVector2D& TL, float W, bool bSel) const
	{
		const float In = InvIn(), H = SkillCardNH(W), NB = FMath::Max(28.f, W * 0.2f);
		if (bSel) { const float Gp = 16.f / FMath::Max(0.2f, S); ZBox9(Out, G, L, InvFrame(TEXT("card_sel")), TL - FVector2D(Gp, Gp), FVector2D(W + 2.f * Gp, H + 2.f * Gp), 0.25f); }
		const FLinearColor Dim = I.bOwned ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
		const FVector2D ITL = TL + FVector2D(In, In), ISz(W - 2.f * In, H - 2.f * In);
		ZRect(Out, G, L + 1, ITL, ISz, FLinearColor(0.03f, 0.035f, 0.11f, 1.f));
		if (UTexture2D* Nw = ZLoadPng(TEXT("Data/CardArt/IconsNew/") + I.CardId)) ZTex(Out, G, L + 2, Nw, ITL, FVector2D(ISz.X, ISz.Y - NB * 0.55f), Dim); // the mockup's painting (480 x 336)
		else if (UTexture2D* Ic = ZLoadPng(TEXT("Data/CardArt/Icons/") + I.CardId)) ZTexFit(Out, G, L + 2, Ic, ITL, FVector2D(ISz.X, ISz.Y - NB * 0.55f), Dim);
		else if (UTexture2D* Ca = ZLoadPng(TEXT("Data/CardArt/") + I.CardId)) ZTexFit(Out, G, L + 2, Ca, ITL, FVector2D(ISz.X, ISz.Y - NB * 0.55f), Dim);
		ZTex(Out, G, L + 3, ZInv(I.bEquipped ? TEXT("chip_on") : TEXT("plate")), FVector2D(TL.X + In * 0.4f, TL.Y + H - In - NB), FVector2D(W - In * 0.8f, NB));
		const FString Nm = !I.bOwned ? FString(TEXT("LOCKED")) : I.Name.ToUpper();
		ZTextC(Out, G, L + 4, Nm, ZFitFont(Nm, 0, NB * 0.6f, W - 2.f * In - 36.f, 2.f), FVector2D(TL.X + W * 0.5f, TL.Y + H - In - NB * 0.5f), Paper);
		{	// LV tag, top-left (covers the mockup's own LV 1)
			const FString Lv = FString::Printf(TEXT("LV %d"), I.Level);
			const FVector2D LSz(W * 0.40f, W * 0.185f);
			ZTex(Out, G, L + 4, ZInv(TEXT("lv")), ITL - FVector2D(1.f, 1.f), LSz);
			ZTextC(Out, G, L + 5, Lv, ZFitFont(Lv, 0, LSz.Y * 0.5f, LSz.X - 30.f, 2.f), ITL + FVector2D(LSz.X * 0.46f, LSz.Y * 0.48f), Paper);
		}
		ZBox9(Out, G, L + 6, InvFrame(I.bEquipped ? TEXT("card_frame_mag") : TEXT("card_frame")), TL, FVector2D(W, H), 0.1875f);
		if (I.bEquipped && I.HandSlot >= 0)
		{	// the hand slot, top right
			const float Bd = FMath::Max(32.f, W * 0.17f);
			const FVector2D Bt = TL + FVector2D(W - Bd - In - 2.f, In + 2.f);
			ZTex(Out, G, L + 7, ZInv(TEXT("pill")), Bt, FVector2D(Bd, Bd));
			ZTextC(Out, G, L + 8, FString::FromInt(I.HandSlot + 1), Font(0, Bd * 0.62f), Bt + FVector2D(Bd * 0.5f, Bd * 0.5f), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
		}
	}
	void InvChip(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& Tx, const FVector2D& TL, const FVector2D& Sz, bool bOn, float Px) const
	{
		ZBox9(Out, G, L, ZInv(bOn ? TEXT("chip_on") : TEXT("chip")), TL, Sz, 0.2f, 0.f);
		ZTextC(Out, G, L + 2, Tx, ZFitFont(Tx, 0, Px, Sz.X - 40.f, 2.f), TL + Sz * 0.5f, Paper);
	}
	void PaintSkillsN(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self, const TArray<FItem>& All, float GX) const
	{
		const bool bPort = DH > DW;
		static const TCHAR* FN[] = { TEXT("ALL"), TEXT("HAND"), TEXT("CLASSIC"), TEXT("ICE"), TEXT("EARTH"), TEXT("SLASH") };
		TArray<FItem> It;
		for (const FItem& I : All) { const int32 F = ZSkillFam(I.CardId); if (SkillFilter == 0 || (SkillFilter == 1 && I.bEquipped) || (SkillFilter >= 2 && F == SkillFilter - 1)) It.Add(I); }
		const FItem* Sel = All.FindByPredicate([this](const FItem& X) { return X.CardId == SkillPick; });
		if (!Sel) Sel = It.Num() ? &It[0] : (All.Num() ? &All[0] : nullptr);
		const float Pk = bPort ? 1.7f : 1.f;
		const float Top = 222.f, Bot = DH - 40.f, BtnW = bPort ? 150.f : 112.f, Gap = 20.f;
		const float X0 = bPort ? 46.f : GX + 6.f, X1 = DW - 46.f;
		float LX0, LY0, LY1;
		if (Sel)
		{
			const FItem& D = *Sel;
			const float DetW = bPort ? (X1 - X0) * 0.42f : FMath::Clamp((X1 - X0) * 0.27f, 300.f, 440.f);
			const float CardH = SkillCardNH(DetW);
			const FVector2D ITL = bPort ? FVector2D(X0 + DetW + 30.f, Top) : FVector2D(X0, Top + CardH + 18.f);
			const FVector2D ISz = bPort ? FVector2D(X1 - ITL.X, CardH) : FVector2D(DetW, Bot - ITL.Y);
			if (!bPort) ZBox9(Out, G, L + 1, InvFrame(TEXT("panel")), FVector2D(X0 - 22.f, Top - 22.f), FVector2D(DetW + 44.f, Bot - Top + 44.f), 0.25f);
			SkillCardN(Out, G, L + 3, D, FVector2D(X0, Top), DetW, false);
			const float Fk = bPort ? FMath::Clamp(DetW / 380.f, 1.f, 2.6f) : 1.f;
			ZBox9(Out, G, L + 3, InvFrame(TEXT("panel_gold")), ITL, ISz, 0.1875f);
			ZTextAt(Out, G, L + 6, D.Name.ToUpper(), ZFitFont(D.Name.ToUpper(), 0, 40.f * Fk, ISz.X - 36.f, 2.f), ITL + FVector2D(18.f, 10.f) * Fk, Paper);
			const FName Sid(*D.CardId);
			const TCHAR* AimTxt = !ZoogiSkills::IsSkill(Sid) ? TEXT("INSTANT - ON YOUR ZOOGI")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Area ? TEXT("AREA - DRAG A CIRCLE")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Line ? TEXT("LINE - SWIPE IT")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Tap ? TEXT("TARGET - TAP ONE")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::MultiTap ? TEXT("TARGETS - TAP UP TO 3")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Spots ? TEXT("SPOTS - TAP 3") : TEXT("SELF - ON YOUR ZOOGI");
			const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(Sid);
			const FString Cost = FString::Printf(TEXT("%d SP"), Cd ? Cd->Cost : 1);
			ZTex(Out, G, L + 6, ZInv(TEXT("lv")), ITL + FVector2D(16.f, 72.f) * Fk, FVector2D(100.f, 38.f) * Fk);
			ZTextC(Out, G, L + 7, Cost, Font(0, 23.f * Fk, 2.f), ITL + FVector2D(63.f, 90.f) * Fk, Paper);
			ZTextAt(Out, G, L + 6, AimTxt, ZFitFont(AimTxt, 0, 22.f * Fk, ISz.X - 150.f * Fk), ITL + FVector2D(126.f, 77.f) * Fk, FLinearColor(1.f, 0.9f, 0.25f, 1.f));
			const float BtnH = 66.f * Fk;
			ZWrap(Out, G, L + 6, D.Desc, Font(1, 21.f * Fk), ITL + FVector2D(18.f, 124.f) * Fk, ISz.X - 36.f * Fk, 27.f * Fk, Paper);
			const FString Bt = !D.bOwned ? FString(TEXT("LOCKED")) : D.bEquipped ? FString(TEXT("-  REMOVE FROM HAND")) : FString(TEXT("+  ADD TO HAND"));
			const FVector2D BTL = ITL + FVector2D(12.f, ISz.Y - BtnH - 12.f), BSz(ISz.X - 24.f, BtnH);
			ZBox9(Out, G, L + 6, ZInv(!D.bOwned ? TEXT("btn_grey") : D.bEquipped ? TEXT("btn") : TEXT("btn_green")), BTL, BSz, 0.2f, 0.f);
			ZTextC(Out, G, L + 10, Bt, ZFitFont(Bt, 0, 30.f * Fk, BSz.X - 40.f, 2.f), BTL + BSz * 0.5f, Paper);
			const FItem Dc = D;
			AddHit(BTL, BSz, [Self, Dc]() { Self->ToggleSkill(Dc); });
			LX0 = bPort ? X0 : X0 + DetW + 52.f; LY0 = bPort ? Top + CardH + 40.f : Top; LY1 = Bot;
		}
		else { LX0 = X0; LY0 = Top; LY1 = Bot; }
		// the list: 4 x 2 (portrait 2 x 3) cards on a neon panel, chips under it, UP / DOWN + scroll bar on the right
		ZBox9(Out, G, L + 1, InvFrame(TEXT("panel")), FVector2D(LX0 - 22.f, LY0 - 22.f), FVector2D(X1 - LX0 + 44.f, LY1 - LY0 + 44.f), 0.25f);
		const float ChipH = 58.f * Pk, LX1 = X1 - BtnW - 16.f;
		const int32 Rows = bPort ? 3 : 2;
		const float RowGap = Gap + 6.f;
		const float ByH = ((LY1 - LY0 - ChipH - 26.f) - (Rows - 1) * RowGap) / Rows / 0.92f;
		auto ColW = [&](int32 Cn) { return FMath::Min((LX1 - LX0 - (Cn - 1) * Gap) / Cn, ByH); };
		const int32 Cols = bPort ? 2 : (ColW(4) >= ColW(3) - 1.f ? 4 : 3); // b239: 4 across when the screen is wide enough (phones), else 3 bigger cards
		const float W = FMath::Max(80.f, ColW(Cols));
		const float CH = SkillCardNH(W);
		const int32 TotalRows = FMath::Max(1, (It.Num() + Cols - 1) / Cols), MaxScroll = FMath::Max(0, TotalRows - Rows);
		const int32 Sc = FMath::Clamp(SkillScroll, 0, MaxScroll);
		const float GridW = Cols * W + (Cols - 1) * Gap, GX0 = LX0 + FMath::Max(0.f, (LX1 - LX0 - GridW) * 0.5f);
		const float GridH = Rows * CH + (Rows - 1) * RowGap, GY0 = LY0 + FMath::Max(0.f, (LY1 - LY0 - ChipH - 26.f - GridH) * 0.5f);
		for (int32 r = 0; r < Rows; ++r) for (int32 c = 0; c < Cols; ++c)
		{
			const int32 i = (Sc + r) * Cols + c; if (!It.IsValidIndex(i)) continue;
			const FVector2D TL(GX0 + c * (W + Gap), GY0 + r * (CH + RowGap));
			SkillCardN(Out, G, L + 12, It[i], TL, W, Sel && It[i].CardId == Sel->CardId);
			const FString Id = It[i].CardId;
			AddHit(TL, FVector2D(W, CH), [Self, Id]() { Self->SkillPick = Id; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });
		}
		if (It.Num() == 0) ZTextC(Out, G, L + 12, TEXT("NO SKILLS HERE"), Font(0, 34.f, 2.f), FVector2D((LX0 + LX1) * 0.5f, LY0 + 120.f), Paper);
		{	// filter chips
			float Cx = GX0; const float Cy = LY1 - ChipH + 2.f;
			for (int32 f = 0; f < 6; ++f)
			{
				const FSlateFontInfo Ff = Font(0, 26.f * Pk, 2.f); const float Wc = ZMeasure(FN[f], Ff).X + 58.f * Pk;
				if (Cx + Wc > X1) break;
				InvChip(Out, G, L + 12, FN[f], FVector2D(Cx, Cy), FVector2D(Wc, ChipH - 6.f), f == SkillFilter, 26.f * Pk);
				AddHit(FVector2D(Cx, Cy - 6.f), FVector2D(Wc, ChipH + 4.f), [Self, f]() { Self->SkillFilter = f; Self->SkillScroll = 0; Self->Sfx(TEXT("UiClick"), 0.7f, 1.1f); });
				Cx += Wc + 4.f;
			}
		}
		const float BS = BtnW, BX = X1 - BtnW, UY = GY0, DY = GY0 + GridH - BS;
		for (int32 k = 0; k < 2; ++k)
		{
			const bool bUp = k == 0; const float Y = bUp ? UY : DY;
			const bool bCan = bUp ? Sc > 0 : Sc < MaxScroll;
			ZTex(Out, G, L + 12, ZInv(bCan ? (bUp ? TEXT("arrow_up") : TEXT("arrow_dn")) : (bUp ? TEXT("arrow_off_up") : TEXT("arrow_off_dn"))), FVector2D(BX, Y), FVector2D(BS, BS));
			AddHit(FVector2D(BX - 6.f, Y - 6.f), FVector2D(BS + 12.f, BS + 12.f), [Self, bUp, MaxScroll]() { const int32 Nw = FMath::Clamp(Self->SkillScroll + (bUp ? -1 : 1), 0, MaxScroll); if (Nw != Self->SkillScroll) { Self->SkillScroll = Nw; Self->Sfx(TEXT("UiClick"), 0.7f, bUp ? 1.25f : 1.05f); } else Self->Sfx(TEXT("UiCancel"), 0.4f, 1.3f); });
		}
		{
			const float TY0 = UY + BS + 10.f, TY1 = DY - 10.f, TW = 26.f, TX = BX + (BS - TW) * 0.5f;
			if (TY1 > TY0 + 20.f)
			{
				ZRect(Out, G, L + 12, FVector2D(TX - 3.f, TY0 - 3.f), FVector2D(TW + 6.f, TY1 - TY0 + 6.f), FLinearColor(0.25f, 0.75f, 1.f, 0.9f));
				ZRect(Out, G, L + 13, FVector2D(TX, TY0), FVector2D(TW, TY1 - TY0), FLinearColor(0.03f, 0.035f, 0.11f, 1.f));
				const float Frac = FMath::Clamp((float)Rows / TotalRows, 0.12f, 1.f), Th = (TY1 - TY0) * Frac;
				const float Ty = TY0 + (MaxScroll > 0 ? (TY1 - TY0 - Th) * Sc / MaxScroll : 0.f);
				ZRect(Out, G, L + 14, FVector2D(TX + 4.f, Ty + 4.f), FVector2D(TW - 8.f, Th - 8.f), FLinearColor(1.f, 0.84f, 0.16f, 1.f));
			}
		}
	}

	void PaintInventory(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		// the robot stands on the left (3D), stats tag under it
		const float RobotX = FMath::Min(330.f, DW * 0.19f), FeetY = DH - 170.f, TopY = 175.f;
		RobotBox = FBox2D(FVector2D(RobotX - 210.f, TopY), FVector2D(RobotX + 210.f, FeetY));
		ZoogiFront::FView& V = ZoogiFront::View();
		V.Feet01 = ScrN(FVector2D(RobotX, FeetY));
		V.Height01 = (FeetY - TopY) * S / FMath::Max(1.f, ScrPx.Y);
		if (DH > DW) V.Height01 = FMath::Min(V.Height01, 0.36f * DW / FMath::Max(1.f, DH) * 1.9f); // portrait: the robot stays a sensible size
		{
			const ZoogiFront::FHero& H = ZoogiFront::Equipped(Ctx.Get());
			const FString L1 = FString::Printf(TEXT("%s - %s"), H.Name, H.Element);
			const float W = ZMeasure(L1.ToUpper(), Font(0, 30.f)).X + 40.f;
			if (ZInvRobot() || (Self->b3D && Tab == 0)) ZTag(Out, G, L, L1.ToUpper(), FVector2D(RobotX - W * 0.5f, DH - 130.f), 56.f, false, Mag(), 30.f);
		}
		// tabs
		static const TCHAR* Tabs[] = { TEXT("BALLERS"), TEXT("ZOOGIS"), TEXT("ITEM"), TEXT("SKILLS") }; // b143a (Evan): WEAPON -> ITEM // b63: CARDS -> SKILLS (the hand loadout); b100 (Evan): ZOOGIS -> BALLERS, BALLS -> ZOOGIS
		// b120 (Evan: "resize for portrait orientation - fill screen"): every tab's grid uses the full width in portrait (b63: skills only)
		const bool bPortInv = DH > DW;
		const bool bRobotCol = ZInvRobot() || (Self->b3D && Tab == 0); // b203: the 3D view takes the left column
		float GX = bPortInv || !bRobotCol ? 40.f : RobotX + 260.f; // b179: no robot - the grid takes the whole width
		float X = bRobotCol && !bPortInv ? RobotX + 260.f : 40.f;
		for (int32 i = 0; i < 4; ++i)
		{
			const bool bOn = i == Tab;
			const FSlateFontInfo F = Font(0, 32.f);
			const float W = ZMeasure(Tabs[i], F).X + 44.f;
			if (ZInvNew()) ZBox9(Out, G, L, ZInv(bOn ? TEXT("chip_on") : TEXT("chip")), FVector2D(X - 8.f, 124.f), FVector2D(W + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X, 130.f), FVector2D(W, 58.f), bOn ? Mag() : Ink, bOn ? Paper : Ink.CopyWithNewOpacity(0.6f), 4.f, 6.f);
			ZTextC(Out, G, L + 3, Tabs[i], F, FVector2D(X + W * 0.5f, 159.f), Paper);
			AddHit(FVector2D(X - 4.f, 120.f), FVector2D(W + 8.f, 78.f), [Self, i]() { if (Self->Tab != i) { Self->Tab = i; Self->Pop = SZoogiFront::EPop::None; Self->SkillDetail.Reset(); Self->Sfx(TEXT("UiClick"), 0.7f, 1.1f); } });
			X += W + 14.f;
		}
		if (Tab == 0)
		{	// b203 (Evan): 3D VIEW - the Zoogi's 3D model on the left, drag it to spin
			const FString Lb = Self->b3D ? TEXT("3D VIEW: ON") : TEXT("3D VIEW");
			const FSlateFontInfo F = Font(0, 30.f);
			const float W3 = ZMeasure(Lb, F).X + 44.f;
			const float X3 = DH > DW ? 40.f : X + 16.f, Y3 = DH > DW ? 200.f : 130.f;
			if (ZInvNew()) ZBox9(Out, G, L, ZInv(Self->b3D ? TEXT("chip_on") : TEXT("chip")), FVector2D(X3 - 8.f, Y3 - 6.f), FVector2D(W3 + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X3, Y3), FVector2D(W3, 58.f), Self->b3D ? Gold : Paper, Ink, 4.f, 6.f);
			ZTextC(Out, G, L + 3, Lb, F, FVector2D(X3 + W3 * 0.5f, Y3 + 29.f), ZInvNew() ? Paper : Ink);
			AddHit(FVector2D(X3 - 4.f, Y3 - 10.f), FVector2D(W3 + 8.f, 78.f), [Self]() { Self->b3D = !Self->b3D; Self->Sfx(TEXT("UiClick"), 0.8f, Self->b3D ? 1.3f : 0.9f); });
		}
		// grid, sized to fit (b63: the column count that gives the biggest tiles - the skills list is long now)
		const TArray<FItem> It = Items();
		const float AvailW = bPortInv ? DW - 80.f : DW - GX - 50.f, AvailH = bPortInv ? DH - 226.f - 175.f : DH - 230.f - 40.f, Gap = 22.f; // portrait: down to the Zoogi tag
		int32 Cols = It.Num() > 8 ? 5 : 4; float W = 0.f;
		for (int32 Cn = bPortInv ? 2 : 3; Cn <= 9; ++Cn)
		{
			const int32 Rn = FMath::Max(1, (It.Num() + Cn - 1) / Cn);
			const float Wn = FMath::Min(bPortInv ? 1000.f : 250.f, FMath::Min((AvailW - (Cn - 1) * Gap) / Cn, (AvailH - (Rn - 1) * Gap) / Rn));
			if (Wn >= W - 0.5f) { W = Wn; Cols = Cn; } // ties: more columns (portrait fills the width)
		}
		if (bPortInv) GX = 40.f + FMath::Max(0.f, (AvailW - (Cols * W + (Cols - 1) * Gap)) * 0.5f); // centred
		if (Tab == 3)
		{
			// b63: hand counter next to the tabs
			int32 NIn = 0; for (const FItem& I : It) NIn += I.bEquipped ? 1 : 0;
			const FString Hc = DH > DW ? FString::Printf(TEXT("HAND %d/%d"), NIn, UZoogiFrontProfileSubsystem::DeckSize) : FString::Printf(TEXT("HAND  %d / %d   -   TAP TO ADD / REMOVE"), NIn, UZoogiFrontProfileSubsystem::DeckSize);
			const FSlateFontInfo Hf = Font(0, 30.f);
			const float Hw = ZMeasure(Hc, Hf).X + 40.f;
			if (ZInvNew()) ZBox9(Out, G, L, ZInv(NIn >= UZoogiFrontProfileSubsystem::DeckSize ? TEXT("chip_on") : TEXT("chip")), FVector2D(X + 2.f, 124.f), FVector2D(Hw + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X + 10.f, 134.f), FVector2D(Hw, 50.f), NIn >= UZoogiFrontProfileSubsystem::DeckSize ? Mag() : Ink, Paper, 4.f, 5.f);
			ZTextC(Out, G, L + 3, Hc, Hf, FVector2D(X + 10.f + Hw * 0.5f, 159.f), Paper);
			if (DH <= DW)
			{	// b273 (skills review "Play styles"): one tap sets a suggested hand
				struct FStyle { const TCHAR* Name; const TCHAR* Ids[5]; };
				static const FStyle Styles[] = {
					{ TEXT("BRUISER"), { TEXT("crescent_slash"), TEXT("glacier_lance"), TEXT("water_cannon"), TEXT("cross_slash"), TEXT("grapple_hook") } },
					{ TEXT("SWEEPER"), { TEXT("energy_shockwave"), TEXT("bolt_balls"), TEXT("gattling_burst"), TEXT("ice_shards"), TEXT("star_magnet") } },
					{ TEXT("FORTRESS"), { TEXT("ice_wall"), TEXT("wood_shield"), TEXT("decoy"), TEXT("edge_save"), TEXT("sapphire_wall") } },
					{ TEXT("TRICKSTER"), { TEXT("apple_bombs"), TEXT("frost_spikes"), TEXT("hex"), TEXT("tether"), TEXT("tide_shift") } },
					{ TEXT("SPEEDSTER"), { TEXT("gattling_beat"), TEXT("grapple_hook"), TEXT("ricochet"), TEXT("bounce_pad"), TEXT("sword_slash") } } };
				const FSlateFontInfo Sf = Font(0, 22.f);
				float Sx = X + 10.f + Hw + 30.f;
				for (const FStyle& St : Styles)
				{
					const float Sw = ZMeasure(St.Name, Sf).X + 26.f;
					ZPanel(Out, G, L, FVector2D(Sx, 139.f), FVector2D(Sw, 40.f), Ink, Mag(), 3.f, 4.f);
					ZTextC(Out, G, L + 3, St.Name, Sf, FVector2D(Sx + Sw * 0.5f, 159.f), Paper);
					const FStyle* Sp = &St;
					AddHit(FVector2D(Sx, 139.f), FVector2D(Sw, 40.f), [Self, Sp]()
					{
						UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (!Pp) return;
						TArray<FName> Hand; for (const TCHAR* Id : Sp->Ids) { const FName N(Id); if (Pp->IsCardOwned(N) && Hand.Num() < UZoogiFrontProfileSubsystem::DeckSize) Hand.AddUnique(N); }
						Pp->SetDeck(Pp->Data().ActiveDeck, Hand);
						Self->Say(FString::Printf(TEXT("%s HAND SET - %d SKILLS"), Sp->Name, Hand.Num()));
						UE_LOG(LogZoogi, Log, TEXT("FRONT play style %s: %d skills"), Sp->Name, Hand.Num());
					});
					Sx += Sw + 10.f;
				}
			}
		}
		if (Tab == 3 && ZNewSkillsUi()) { if (ZInvNew()) PaintSkillsN(Out, G, L, Self, It, GX); else PaintSkillsC(Out, G, L, Self, It, GX); return; } // b179, b238
		if (ZInvNew() && It.Num() > 0)
		{	// b238: the grid sits on a neon panel
			const int32 Rn = (It.Num() + Cols - 1) / Cols;
			ZBox9(Out, G, L + 1, InvFrame(TEXT("panel")), FVector2D(GX - 26.f, 226.f - 26.f), FVector2D(Cols * W + (Cols - 1) * Gap + 52.f, Rn * W + (Rn - 1) * Gap + 52.f), 0.25f);
		}
		for (int32 i = 0; i < It.Num(); ++i)
		{
			const FVector2D TL(GX + (i % Cols) * (W + Gap), 226.f + (i / Cols) * (W + Gap));
			if (ZInvNew()) TileN(Out, G, L + 4, It[i], TL, W); else Tile(Out, G, L + 4, It[i], TL, W);
			if (It[i].Kind == 3) { const FItem Ci = It[i]; AddHit(TL, FVector2D(W, W), [Self, Ci]() { Self->ToggleSkill(Ci); }); }
			else { const FString Wk = It[i].WeaponKey; AddHit(TL, FVector2D(W, W), [Self, i, Wk]() { Self->ItemSel = i; Self->Pop = SZoogiFront::EPop::Item; Self->PopT = Self->T; Self->Sfx(TEXT("UiClick"), 0.8f, 1.15f);
				if (!Wk.IsEmpty()) if (UZoogiFrontProfileSubsystem* Pn = Self->FP()) if (Pn->Mutable().ItemCounts.Remove(FName(*(FString(TEXT("new_")) + Wk))) > 0) Pn->Save(); }); } // b228: opening it clears +N NEW
		}
		if (Tab == 3 && !SkillDetail.IsEmpty())
		{
			// b65 (Evan: "Details appear where the current 3d model we bought stands ... tap for details, tap to exit")
			const FItem* Di = It.FindByPredicate([this](const FItem& X) { return X.CardId == SkillDetail; });
			if (Di)
			{
				const int32 RowsUsed = FMath::Max(1, (It.Num() + Cols - 1) / Cols);
				const bool bPortG = DH > DW;
				// b120: the portrait grid fills the screen now - the panel overlays its lower part (under the grid when there is room)
				const float PH = FMath::Min(1000.f, FMath::Max(DH * 0.44f, DH - (226.f + RowsUsed * (W + Gap) + 30.f) - 60.f));
				const FVector2D PTL = bPortG ? FVector2D(40.f, FMath::Min(226.f + RowsUsed * (W + Gap) + 30.f, DH - 60.f - PH)) : FVector2D(24.f, 200.f);
				const FVector2D PSz = bPortG ? FVector2D(DW - 80.f, PH) : FVector2D(GX - 50.f, DH - 240.f);
				ZPanel(Out, G, L + 20, PTL, PSz, Ink, Mag(), 6.f, 12.f);
				// big card art
				const float ArtH = bPortG ? FMath::Min(PSz.Y * 0.45f, 420.f) : FMath::Min(PSz.Y * 0.36f, 400.f); // b65b: smaller art in portrait so the text fits
				const float ArtW = ArtH * 245.f / 386.f;
				const FVector2D ATL = PTL + FVector2D(bPortG ? 30.f : (PSz.X - ArtW) * 0.5f, 30.f);
				if (UTexture2D* Ca = ZLoadPng(TEXT("Data/CardArt/") + Di->CardId)) ZTexFit(Out, G, L + 22, Ca, ATL, FVector2D(ArtW, ArtH));
				// text column (right of the art in portrait, under it in landscape)
				const FVector2D TX = bPortG ? PTL + FVector2D(60.f + ArtW, 40.f) : PTL + FVector2D(30.f, 60.f + ArtH);
				const float TW = bPortG ? PSz.X - ArtW - 90.f : PSz.X - 60.f;
				ZTextAt(Out, G, L + 22, Di->Name, ZFitFont(Di->Name, 0, 50.f, TW - 10.f), TX, Paper);
				const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(FName(*Di->CardId));
				const FName Sid(*Di->CardId);
				const TCHAR* AimTxt = !ZoogiSkills::IsSkill(Sid) ? TEXT("INSTANT - ON YOUR ZOOGI")
					: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Area ? TEXT("AREA - DRAG A CIRCLE IN YOUR ARC RANGE")
					: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Line ? TEXT("LINE - SWIPE IN YOUR ARC RANGE")
					: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Tap ? TEXT("TARGET - TAP ONE IN YOUR ARC RANGE")
					: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::MultiTap ? TEXT("TARGETS - TAP UP TO 3")
					: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Spots ? TEXT("SPOTS - TAP 3 IN YOUR ARC RANGE") : TEXT("SELF - CAST ON YOUR ZOOGI");
				const FString Cost = FString::Printf(TEXT("%d SP"), Cd ? Cd->Cost : 1);
				ZRect(Out, G, L + 22, TX + FVector2D(0.f, 74.f), FVector2D(110.f, 40.f), Mag());
				ZTextC(Out, G, L + 23, Cost, Font(0, 26.f), TX + FVector2D(55.f, 94.f), Paper);
				ZWrap(Out, G, L + 22, FString(AimTxt), Font(0, 22.f), TX + FVector2D(0.f, 126.f), TW, 26.f, FLinearColor(1.f, 0.85f, 0.35f, 1.f));
				// b65b: aim on its own line under the SP plate; the description goes full width under the art in portrait
				if (bPortG) ZWrap(Out, G, L + 22, Di->Desc, Font(1, 26.f), FVector2D(PTL.X + 30.f, ATL.Y + ArtH + 30.f), PSz.X - 60.f, 34.f, Paper);
				else ZWrap(Out, G, L + 22, Di->Desc, Font(1, 24.f), TX + FVector2D(0.f, 190.f), TW, 32.f, Paper);
				const FString St = Di->bEquipped ? FString::Printf(TEXT("IN YOUR HAND - SLOT %d"), Di->HandSlot + 1) : FString(TEXT("NOT IN YOUR HAND"));
				const float SY = PTL.Y + PSz.Y - 116.f;
				ZRect(Out, G, L + 22, FVector2D(bPortG ? PTL.X + 30.f : TX.X, SY), FVector2D(bPortG ? PSz.X - 60.f : TW, 50.f), Di->bEquipped ? Mag() : FLinearColor(0.15f, 0.15f, 0.2f, 1.f));
				ZTextAt(Out, G, L + 23, St, ZFitFont(St, 0, 28.f, (bPortG ? PSz.X - 60.f : TW) - 20.f), FVector2D((bPortG ? PTL.X + 30.f : TX.X) + 10.f, SY + 8.f), Paper);
				ZTextC(Out, G, L + 23, TEXT("TAP HERE TO CLOSE"), Font(0, 26.f), FVector2D(PTL.X + PSz.X * 0.5f, PTL.Y + PSz.Y - 34.f), FLinearColor(1.f, 1.f, 1.f, 0.7f));
				AddHit(PTL, PSz, [Self]() { Self->SkillDetail.Reset(); Self->Sfx(TEXT("UiCancel"), 0.6f, 1.2f); });
			}
		}
	}

	// b179 (Evan Nov 7: "make them big cards" - picked layout C with the B arrows): the SKILLS tab is a big detail card (art, how to
	// use it, ADD / REMOVE) beside a 3 x 2 list of big cards; UP / DOWN (with a scroll bar between them) scroll the list a row at a
	// time, filter chips under it (ALL / HAND / CLASSIC / ICE / EARTH / SLASH). Tap a card = show it; the big button adds / removes it.
	static void ZArrow(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& C, float Sz, bool bUp, const FLinearColor& Col)
	{
		const int32 N = 14;
		for (int32 k = 0; k < N; ++k)
		{
			const float F = (k + 1.f) / N, Y = bUp ? C.Y - Sz * 0.5f + Sz * k / N : C.Y + Sz * 0.5f - Sz * (k + 1.f) / N;
			ZRect(Out, G, L, FVector2D(C.X - Sz * 0.6f * F, Y), FVector2D(Sz * 1.2f * F, Sz / N + 1.f), Col);
		}
	}
	static int32 ZSkillFam(const FString& Id)
	{	// 1 CLASSIC, 2 ICE, 3 EARTH, 4 SLASH
		static const TSet<FString> Ice = { TEXT("frost_spikes"), TEXT("glacier_lance"), TEXT("ice_shards"), TEXT("ice_wall") };
		static const TSet<FString> Slash = { TEXT("crescent_slash"), TEXT("cross_slash"), TEXT("ground_slash"), TEXT("sword_slash") };
		static const TSet<FString> Earth = { TEXT("stone_pillar"), TEXT("earthquake"), TEXT("rock_rampart"), TEXT("sapphire_wall"), TEXT("pillar_field"), TEXT("long_rampart"), TEXT("fault_line") };
		return Ice.Contains(Id) ? 2 : Slash.Contains(Id) ? 4 : Earth.Contains(Id) ? 3 : 1;
	}
	void SkillCard(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FItem& I, const FVector2D& TL, float W, bool bSel) const
	{
		const float ArtH = W * 0.83f, NB = FMath::Max(30.f, W * 0.2f), H = ArtH + NB + 8.f;
		if (bSel) ZRect(Out, G, L, TL - FVector2D(9.f, 9.f), FVector2D(W + 18.f, H + 18.f), FLinearColor(1.f, 0.84f, 0.16f, 1.f));
		ZPanel(Out, G, L + 1, TL, FVector2D(W, H), Ink, I.bEquipped ? Mag() : FLinearColor(0.f, 0.f, 0.f, 0.6f), I.bEquipped ? 5.f : 3.f, 8.f);
		const FLinearColor Dim = I.bOwned ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
		if (UTexture2D* Ic = ZLoadPng(TEXT("Data/CardArt/Icons/") + I.CardId)) ZTex(Out, G, L + 4, Ic, TL + FVector2D(4.f, 4.f), FVector2D(W - 8.f, ArtH), Dim);
		else if (UTexture2D* Ca = ZLoadPng(TEXT("Data/CardArt/") + I.CardId)) ZTexFit(Out, G, L + 4, Ca, TL + FVector2D(4.f, 4.f), FVector2D(W - 8.f, ArtH), Dim);
		ZRect(Out, G, L + 5, TL + FVector2D(0.f, ArtH + 8.f), FVector2D(W, NB), I.bEquipped ? Mag() : Ink);
		const FString Nm = !I.bOwned ? FString(TEXT("LOCKED")) : I.Name;
		ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, NB * 0.6f, W - 14.f), TL + FVector2D(W * 0.5f, ArtH + 8.f + NB * 0.5f), Paper);
		const FString Lv = FString::Printf(TEXT("LV %d"), I.Level);
		const float Lp = FMath::Max(18.f, W * 0.1f);
		ZRect(Out, G, L + 6, TL + FVector2D(0.f, 4.f), FVector2D(ZMeasure(Lv, Font(0, Lp)).X + 18.f, Lp * 1.35f), Mag());
		ZTextAt(Out, G, L + 7, Lv, Font(0, Lp), TL + FVector2D(9.f, 4.f + Lp * 0.12f), Paper);
		if (I.bEquipped && I.HandSlot >= 0)
		{	// the hand slot, top right
			const float Bd = FMath::Max(32.f, W * 0.16f);
			ZRect(Out, G, L + 6, TL + FVector2D(W - Bd - 6.f, 6.f), FVector2D(Bd, Bd), Paper);
			ZRect(Out, G, L + 7, TL + FVector2D(W - Bd - 3.f, 9.f), FVector2D(Bd - 6.f, Bd - 6.f), Mag());
			ZTextC(Out, G, L + 8, FString::FromInt(I.HandSlot + 1), Font(0, Bd * 0.62f), TL + FVector2D(W - Bd * 0.5f - 6.f, 6.f + Bd * 0.5f), Paper);
		}
	}
	void PaintSkillsC(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self, const TArray<FItem>& All, float GX) const
	{
		const bool bPort = DH > DW;
		static const TCHAR* FN[] = { TEXT("ALL"), TEXT("HAND"), TEXT("CLASSIC"), TEXT("ICE"), TEXT("EARTH"), TEXT("SLASH") };
		TArray<FItem> It;
		for (const FItem& I : All) { const int32 F = ZSkillFam(I.CardId); if (SkillFilter == 0 || (SkillFilter == 1 && I.bEquipped) || (SkillFilter >= 2 && F == SkillFilter - 1)) It.Add(I); }
		const FItem* Sel = All.FindByPredicate([this](const FItem& X) { return X.CardId == SkillPick; });
		if (!Sel) Sel = It.Num() ? &It[0] : (All.Num() ? &All[0] : nullptr);
		const float Pk = bPort ? 1.7f : 1.f; // b179b: portrait chips / arrows bigger
		const float Top = 215.f, Bot = DH - 34.f, BtnW = bPort ? 170.f : 130.f, Gap = 22.f;
		const float X0 = bPort ? 40.f : GX, X1 = DW - 40.f;
		// --- the big detail card + its info
		float LX0, LY0, LY1;
		if (Sel)
		{
			const FItem& D = *Sel;
			const float DetW = bPort ? (X1 - X0) * 0.42f : FMath::Clamp((X1 - X0) * 0.3f, 300.f, 460.f); // b179b: portrait - a big card
			const FVector2D CTL(X0, Top);
			SkillCard(Out, G, L + 2, D, CTL, DetW, false);
			const float CardH = DetW * 0.83f + FMath::Max(30.f, DetW * 0.2f) + 8.f;
			const float Fk = bPort ? FMath::Clamp(DetW / 380.f, 1.f, 2.6f) : 1.f; // b179b: portrait text scales with the big card
			const FVector2D ITL = bPort ? FVector2D(X0 + DetW + 30.f, Top) : FVector2D(X0, Top + CardH + 20.f);
			const FVector2D ISz = bPort ? FVector2D(X1 - ITL.X, CardH) : FVector2D(DetW, Bot - ITL.Y);
			ZPanel(Out, G, L + 2, ITL, ISz, Ink, FLinearColor(0.f, 0.f, 0.f, 0.55f), 3.f, 8.f);
			ZTextAt(Out, G, L + 6, D.Name, ZFitFont(D.Name, 0, 40.f * Fk, ISz.X - 28.f), ITL + FVector2D(14.f, 8.f) * Fk, Paper);
			const FName Sid(*D.CardId);
			const TCHAR* AimTxt = !ZoogiSkills::IsSkill(Sid) ? TEXT("INSTANT - ON YOUR ZOOGI")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Area ? TEXT("AREA - DRAG A CIRCLE")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Line ? TEXT("LINE - SWIPE IT")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Tap ? TEXT("TARGET - TAP ONE")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::MultiTap ? TEXT("TARGETS - TAP UP TO 3")
				: ZoogiSkills::AimOf(Sid) == ZoogiSkills::EAim::Spots ? TEXT("SPOTS - TAP 3") : TEXT("SELF - ON YOUR ZOOGI");
			const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(Sid);
			const FString Cost = FString::Printf(TEXT("%d SP"), Cd ? Cd->Cost : 1);
			ZRect(Out, G, L + 6, ITL + FVector2D(14.f, 60.f) * Fk, FVector2D(90.f, 34.f) * Fk, Mag());
			ZTextC(Out, G, L + 7, Cost, Font(0, 22.f * Fk), ITL + FVector2D(59.f, 77.f) * Fk, Paper);
			ZTextAt(Out, G, L + 6, AimTxt, ZFitFont(AimTxt, 0, 22.f * Fk, ISz.X - 140.f * Fk), ITL + FVector2D(116.f, 64.f) * Fk, FLinearColor(1.f, 0.85f, 0.35f, 1.f));
			const float BtnH = 58.f * Fk;
			ZWrap(Out, G, L + 6, D.Desc, Font(1, 21.f * Fk), ITL + FVector2D(14.f, 106.f) * Fk, ISz.X - 28.f * Fk, 27.f * Fk, Paper);
			const FString Bt = !D.bOwned ? FString(TEXT("LOCKED")) : D.bEquipped ? FString(TEXT("REMOVE FROM HAND")) : FString(TEXT("ADD TO HAND"));
			const FVector2D BTL = ITL + FVector2D(14.f, ISz.Y - BtnH - 14.f), BSz(ISz.X - 28.f, BtnH);
			ZPanel(Out, G, L + 6, BTL, BSz, D.bOwned ? Mag() : FLinearColor(0.3f, 0.3f, 0.35f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.7f), 3.f, 6.f);
			ZTextC(Out, G, L + 10, Bt, ZFitFont(Bt, 0, 30.f * Fk, BSz.X - 20.f), BTL + BSz * 0.5f, Paper);
			const FItem Dc = D;
			AddHit(BTL, BSz, [Self, Dc]() { Self->ToggleSkill(Dc); });
			LX0 = bPort ? X0 : X0 + DetW + 44.f; LY0 = bPort ? Top + CardH + 34.f : Top; LY1 = Bot;
		}
		else { LX0 = X0; LY0 = Top; LY1 = Bot; }
		// --- the list: 3 columns x 2 rows of big cards
		const float ChipH = 52.f * Pk, LX1 = X1 - BtnW - 30.f;
		const int32 Cols = bPort ? 2 : 3, Rows = bPort ? 3 : 2; // b179b: portrait - 2 x 3 bigger cards
		const float ByW = (LX1 - LX0 - (Cols - 1) * Gap) / Cols;
		const float ByH = ((LY1 - LY0 - ChipH - 24.f) - (Rows - 1) * (Gap + 10.f)) / Rows / 1.06f;
		const float W = FMath::Max(80.f, FMath::Min(ByW, ByH));
		const float CH = W * 0.83f + FMath::Max(30.f, W * 0.2f) + 8.f;
		const int32 TotalRows = FMath::Max(1, (It.Num() + Cols - 1) / Cols), MaxScroll = FMath::Max(0, TotalRows - Rows);
		const int32 Sc = FMath::Clamp(SkillScroll, 0, MaxScroll);
		const float GridW = Cols * W + (Cols - 1) * Gap, GX0 = LX0 + FMath::Max(0.f, (LX1 - LX0 - GridW) * 0.5f);
		for (int32 r = 0; r < Rows; ++r) for (int32 c = 0; c < Cols; ++c)
		{
			const int32 i = (Sc + r) * Cols + c; if (!It.IsValidIndex(i)) continue;
			const FVector2D TL(GX0 + c * (W + Gap), LY0 + r * (CH + Gap + 10.f));
			SkillCard(Out, G, L + 12, It[i], TL, W, Sel && It[i].CardId == Sel->CardId);
			const FString Id = It[i].CardId;
			AddHit(TL, FVector2D(W, CH), [Self, Id]() { Self->SkillPick = Id; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });
		}
		if (It.Num() == 0) ZTextC(Out, G, L + 12, TEXT("NO SKILLS HERE"), Font(0, 34.f), FVector2D((LX0 + LX1) * 0.5f, LY0 + 120.f), Paper);
		// filter chips under the list
		{
			float Cx = GX0; const float Cy = LY0 + Rows * (CH + Gap + 10.f) + 4.f;
			for (int32 f = 0; f < 6; ++f)
			{
				const FSlateFontInfo Ff = Font(0, 26.f * Pk); const float Wc = ZMeasure(FN[f], Ff).X + 34.f * Pk;
				if (Cx + Wc > LX1 + BtnW) break;
				ZPanel(Out, G, L + 12, FVector2D(Cx, Cy), FVector2D(Wc, ChipH - 8.f), f == SkillFilter ? Mag() : Ink, FLinearColor(0.f, 0.f, 0.f, 0.6f), 3.f, 5.f);
				ZTextC(Out, G, L + 16, FN[f], Ff, FVector2D(Cx + Wc * 0.5f, Cy + (ChipH - 8.f) * 0.5f), Paper);
				AddHit(FVector2D(Cx, Cy - 6.f), FVector2D(Wc, ChipH + 4.f), [Self, f]() { Self->SkillFilter = f; Self->SkillScroll = 0; Self->Sfx(TEXT("UiClick"), 0.7f, 1.1f); });
				Cx += Wc + 12.f;
			}
		}
		// UP / DOWN (B style) with the scroll bar between them
		const float BX = X1 - BtnW, BH = 104.f * Pk, UY = LY0, DY = LY0 + Rows * (CH + Gap + 10.f) - Gap - 10.f - BH;
		for (int32 k = 0; k < 2; ++k)
		{
			const bool bUp = k == 0; const float Y = bUp ? UY : DY;
			const bool bCan = bUp ? Sc > 0 : Sc < MaxScroll;
			ZPanel(Out, G, L + 12, FVector2D(BX, Y), FVector2D(BtnW, BH), bCan ? Mag() : FLinearColor(0.3f, 0.3f, 0.35f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.7f), 4.f, 8.f);
			ZArrow(Out, G, L + 16, FVector2D(BX + BtnW * 0.5f, Y + BH * 0.5f), 46.f * Pk, bUp, Paper);
			AddHit(FVector2D(BX - 6.f, Y - 6.f), FVector2D(BtnW + 12.f, BH + 12.f), [Self, bUp, MaxScroll]() { const int32 Nw = FMath::Clamp(Self->SkillScroll + (bUp ? -1 : 1), 0, MaxScroll); if (Nw != Self->SkillScroll) { Self->SkillScroll = Nw; Self->Sfx(TEXT("UiClick"), 0.7f, bUp ? 1.25f : 1.05f); } else Self->Sfx(TEXT("UiCancel"), 0.4f, 1.3f); });
		}
		{
			const float TY0 = UY + BH + 18.f, TY1 = DY - 18.f, TW = 34.f, TX = BX + (BtnW - TW) * 0.5f;
			if (TY1 > TY0 + 20.f)
			{
				ZRect(Out, G, L + 12, FVector2D(TX, TY0), FVector2D(TW, TY1 - TY0), Ink);
				const float Frac = FMath::Clamp((float)Rows / TotalRows, 0.12f, 1.f), Th = (TY1 - TY0) * Frac;
				const float Ty = TY0 + (MaxScroll > 0 ? (TY1 - TY0 - Th) * Sc / MaxScroll : 0.f);
				ZRect(Out, G, L + 13, FVector2D(TX + 4.f, Ty + 4.f), FVector2D(TW - 8.f, Th - 8.f), FLinearColor(1.f, 0.84f, 0.16f, 1.f));
			}
		}
	}

	void PaintItem(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const TArray<FItem> It = Items();
		if (!It.IsValidIndex(ItemSel)) return;
		const FItem& I = It[ItemSel];
		const FVector2D Sz(1060.f, 620.f), TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f + 20.f);
		ZRect(Out, G, L, TL + FVector2D(22.f, 22.f), Sz + FVector2D(8.f, 8.f), Ink);
		ZRect(Out, G, L + 1, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 2, TL, Sz, Paper);
		AddHit(TL, Sz, nullptr); // taps on the card itself do nothing
		// art box
		ZRect(Out, G, L + 3, TL + FVector2D(30.f, 30.f), FVector2D(250.f, 250.f), Ink);
		ZRect(Out, G, L + 4, TL + FVector2D(30.f, 30.f), FVector2D(250.f, 10.f), ZRarityCol(I.Rarity));
		UTexture2D* Img = !I.Ball.IsEmpty() ? (I.Kind == 0 ? ZBallerArt(I.Ball) : ZBallIcon(I.Ball)) : !I.CardId.IsEmpty() ? ZLoadPng(TEXT("Data/CardArt/") + I.CardId) : !I.Icon.IsEmpty() ? ZLoadPng(I.Icon) : nullptr; // b120: weapon renders
		if ((!I.Ball.IsEmpty() || I.Kind == 2) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"))) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers_sq"))) ZTex(Out, G, L + 4, Bg, TL + FVector2D(30.f, 40.f), FVector2D(250.f, 240.f)); // b136
		if (Img) ZTexFit(Out, G, L + 5, Img, TL + FVector2D(40.f, 46.f), FVector2D(230.f, 226.f));
		if (I.Kind == 2 && I.Count >= 0)
		{	// b227: the carried count on the item card too (option A)
			const FString Cn = FString::Printf(TEXT("x%d"), I.Count);
			const float Pw = ZMeasure(Cn, Font(0, 40.f)).X + 28.f, Ph = 54.f; const FVector2D Pt = TL + FVector2D(30.f + 250.f - Pw - 10.f, 46.f);
			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f));
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
			ZTextC(Out, G, L + 8, Cn, Font(0, 40.f), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), Paper);
		}
		else ZTextC(Out, G, L + 5, I.Name.Left(1), Font(0, 140.f), TL + FVector2D(155.f, 160.f), Paper);
		// name (ransom) + rarity chip + text
		{
			TArray<FString> Parts;
			const FString N = I.Name.ToUpper();
			const int32 Cut = FMath::Max(1, N.Len() / 2);
			Parts.Add(N.Left(Cut)); if (N.Len() > Cut) Parts.Add(N.Mid(Cut));
			ZRansom(Out, G, L + 3, Parts, TL + FVector2D(310.f, 28.f), 62.f);
		}
		const float RW = ZMeasure(I.Sub, Font(0, 26.f)).X + 30.f;
		ZRect(Out, G, L + 3, TL + FVector2D(310.f, 128.f), FVector2D(RW, 40.f), Ink);
		ZTextC(Out, G, L + 4, I.Sub, Font(0, 26.f), TL + FVector2D(310.f + RW * 0.5f, 148.f), Paper);
		{	// b266 (Evan: nothing covered): long descriptions shrink so they never run into the stat bars below
			const float Wt = ZMeasure(I.Desc, Font(1, 24.f)).X;
			const float Fs = Wt > 700.f * 3.4f ? 17.f : Wt > 700.f * 2.6f ? 20.f : 24.f, Lh = Fs * 1.38f;
			ZWrap(Out, G, L + 3, I.Desc, Font(1, Fs), TL + FVector2D(312.f, 186.f), 700.f, Lh, Ink);
		}
		// stats (Zoogis: roster stats; others: level / rarity bars)
		const FZoogiDef* D = nullptr;
		if (I.Kind == 0) if (const ZoogiFront::FHero* H = ZoogiFront::HeroByName(I.Name)) D = ZoogiUtil::FindZoogi(FName(H->RosterId));
		struct FS { const TCHAR* N; float V; };
		TArray<FS> St;
		if (D) St = { { TEXT("SPEED"), D->Speed / 100.f }, { TEXT("POWER"), D->Power / 100.f }, { TEXT("DEFENSE"), D->Defense / 100.f }, { TEXT("CONTROL"), D->Control / 100.f } };
		else St = { { TEXT("RARITY"), (I.Rarity + 1) / 4.f }, { TEXT("LEVEL"), FMath::Clamp(I.Level / 8.f, 0.f, 1.f) } };
		for (int32 k = 0; k < St.Num(); ++k)
		{
			const FVector2D R0 = TL + FVector2D(30.f, 310.f + k * 50.f);
			ZTextAt(Out, G, L + 3, St[k].N, Font(0, 30.f), R0, Ink);
			ZRect(Out, G, L + 3, R0 + FVector2D(200.f, 8.f), FVector2D(780.f, 26.f), Ink);
			ZRect(Out, G, L + 4, R0 + FVector2D(200.f, 8.f), FVector2D(780.f * FMath::Clamp(St[k].V, 0.05f, 1.f), 26.f), Mag());
		}
		if (I.Kind == 2 && I.WeaponKey == TEXT("gatling"))
		{	// b223 (Evan: "in inventory for turret only create a separate edit for selecting ammo type with the option to choose from all
			// options above except slash"): the turret's AMMO - saved in the profile (turret_ammo), used in every match
			struct FAm { int32 S; const TCHAR* N; };
			static const FAm Am[] = { { 5, TEXT("ROCKET") }, { 2, TEXT("TRACERS") }, { 3, TEXT("PLASMA") }, { 4, TEXT("LASER") }, { 6, TEXT("RING") } };
			const UZoogiFrontProfileSubsystem* Pa = FP();
			int32 CurAm = Pa ? Pa->Data().ItemCounts.FindRef(FName(TEXT("turret_ammo"))) : 0; if (CurAm < 2 || CurAm > 6) CurAm = 5;
			const FVector2D A0 = TL + FVector2D(30.f, 420.f);
			ZTextAt(Out, G, L + 3, TEXT("AMMO"), Font(0, 30.f), A0, Ink);
			for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(Am); ++k)
			{
				const bool bOn = Am[k].S == CurAm; const int32 Sv = Am[k].S; const FString Nm = Am[k].N;
				Slab(Out, G, L + 5, Nm, FVector2D(TL.X + 200.f + k * 160.f, A0.Y - 6.f), FVector2D(148.f, 60.f), bOn ? Mag() : Ink, bOn ? Ink : Ink.CopyWithNewOpacity(0.4f), 26.f,
					[Self, Sv, Nm]() { if (UZoogiFrontProfileSubsystem* Pw = Self->FP()) { Pw->Mutable().ItemCounts.FindOrAdd(FName(TEXT("turret_ammo"))) = Sv; Pw->Save(); } Self->Sfx(TEXT("UiConfirm"), 0.8f, 1.2f); Self->Say(FString::Printf(TEXT("TURRET AMMO: %s"), *Nm)); });
			}
		}
		// buttons
		const float BY = TL.Y + Sz.Y - 100.f;
		const FString Main = I.Kind == 0 ? (I.bEquipped ? TEXT("EQUIPPED") : TEXT("EQUIP!"))
			: I.Kind == 1 ? (!I.bOwned ? FString::Printf(TEXT("BUY  %s %s"), I.PriceKind == 1 ? TEXT("GEMS") : TEXT("GOLD"), *ZNumS(I.Price)) : I.bEquipped ? FString(TEXT("EQUIPPED")) : FString(TEXT("EQUIP!")))
			: I.Kind == 2 ? (I.bEquipped ? TEXT("EQUIPPED") : TEXT("EQUIP!")) : (I.bOwned ? TEXT("UPGRADE") : TEXT("LOCKED"));
		const FItem Copy = I;
		Slab(Out, G, L + 5, Main, FVector2D(TL.X + Sz.X - 380.f, BY), FVector2D(340.f, 76.f), Mag(), Ink, 40.f, [Self, Copy]() { Self->ItemAction(Copy); });
		Slab(Out, G, L + 5, TEXT("CLOSE"), FVector2D(TL.X + 30.f, BY), FVector2D(200.f, 76.f), Ink, Ink.CopyWithNewOpacity(0.4f), 34.f, [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
	}

	/** a centred P5 card with a ransom title and a CLOSE tab; returns its top-left */
	FVector2D PopCard(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self, const FVector2D& Sz, const TArray<FString>& Title) const
	{
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, FMath::Max(130.f, DH * 0.5f - Sz.Y * 0.5f + 30.f));
		ZRect(Out, G, L, TL + FVector2D(22.f, 22.f), Sz + FVector2D(8.f, 8.f), Ink);
		ZRect(Out, G, L + 1, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 2, TL, Sz, Paper);
		AddHit(TL, Sz, nullptr);
		ZRansom(Out, G, L + 3, Title, TL + FVector2D(30.f, -44.f), 58.f);
		const FVector2D CT(TL.X + Sz.X - 150.f, TL.Y - 86.f), CS(180.f, 150.f); // b142m / b156 (Evan): a bigger X
		ZRect(Out, G, L + 3, CT + FVector2D(8.f, 8.f), CS, Mag());
		ZRect(Out, G, L + 4, CT, CS, Ink);
		ZTextC(Out, G, L + 5, TEXT("X"), Font(0, 104.f), CT + CS * 0.5f, Paper);
		AddHit(CT - FVector2D(26.f, 26.f), CS + FVector2D(52.f, 52.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
		return TL;
	}

	void PaintTasks(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		UZoogiFrontProfileSubsystem* P = FP();
		const TArray<UZoogiFrontProfileSubsystem::FTaskDef>& Defs = UZoogiFrontProfileSubsystem::TaskDefs();
		const FVector2D Sz(FMath::Min(1300.f, DW - 120.f), 120.f + Defs.Num() * 112.f);
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("TA"), TEXT("SKS") });
		const FString Reset = P ? FString::Printf(TEXT("DAILY TASKS RESET IN %s"), *ZClock((int64)P->TimeToDealsReset().GetTotalSeconds())) : FString();
		ZTextAt(Out, G, L + 3, Reset, Font(1, 22.f), TL + FVector2D(36.f, 40.f), Mag());
		for (int32 i = 0; i < Defs.Num(); ++i)
		{
			const FVector2D R0 = TL + FVector2D(30.f, 90.f + i * 112.f), RS(Sz.X - 60.f, 92.f);
			const int32 Prog = P ? P->TaskProgress(i) : 0, Goal = Defs[i].Goal;
			const bool bDone = Prog >= Goal, bClaimed = P && P->TaskClaimed(i);
			ZRect(Out, G, L + 3, R0 + FVector2D(7.f, 7.f), RS, bDone && !bClaimed ? Mag() : Ink.CopyWithNewOpacity(0.25f));
			ZRect(Out, G, L + 4, R0, RS, bClaimed ? FLinearColor(0.85f, 0.85f, 0.85f) : Paper);
			ZRect(Out, G, L + 5, R0, FVector2D(RS.X, 4.f), Ink);
			const FString Nm = Defs[i].bWeekly ? FString::Printf(TEXT("WEEKLY: %s"), Defs[i].Name) : FString(Defs[i].Name);
			ZTextAt(Out, G, L + 5, Nm, ZFitFont(Nm, 0, 32.f, RS.X * 0.42f), R0 + FVector2D(20.f, 26.f), Ink);
			const float BX = R0.X + RS.X * 0.45f, BW = RS.X * 0.22f;
			ZRect(Out, G, L + 5, FVector2D(BX, R0.Y + 34.f), FVector2D(BW, 26.f), Ink);
			ZRect(Out, G, L + 6, FVector2D(BX, R0.Y + 34.f), FVector2D(BW * FMath::Clamp((float)Prog / FMath::Max(1, Goal), 0.f, 1.f), 26.f), bDone ? Green : Mag());
			ZTextC(Out, G, L + 7, FString::Printf(TEXT("%d / %d"), FMath::Min(Prog, Goal), Goal), Font(0, 22.f), FVector2D(BX + BW * 0.5f, R0.Y + 47.f), Paper);
			const FString Rw = Defs[i].RewardText();
			const FVector2D BT(R0.X + RS.X - 270.f, R0.Y + 14.f), BS(250.f, 64.f);
			const FLinearColor Bb = bClaimed ? FLinearColor(0.6f, 0.6f, 0.6f) : bDone ? Mag() : Ink;
			ZRect(Out, G, L + 5, BT + FVector2D(5.f, 5.f), BS, bDone && !bClaimed ? Ink : Mag().CopyWithNewOpacity(0.5f));
			ZRect(Out, G, L + 6, BT, BS, Bb);
			const FString Btn = bClaimed ? FString(TEXT("CLAIMED")) : bDone ? FString::Printf(TEXT("CLAIM %s"), *Rw) : Rw;
			ZTextC(Out, G, L + 7, Btn, ZFitFont(Btn, 0, 28.f, BS.X - 16.f), BT + BS * 0.5f, Paper);
			if (bDone && !bClaimed)
				AddHit(BT - FVector2D(8.f, 8.f), BS + FVector2D(16.f, 16.f), [Self, i]()
				{
					UZoogiFrontProfileSubsystem* Pp = Self->FP();
					TArray<FZoogiFrontReward> Out2; FName Chest;
					if (!Pp || !Pp->ClaimTask(i, Out2, Chest)) return;
					if (!Chest.IsNone()) { const FZoogiFrontChestDef* D = UZoogiFrontProfileSubsystem::FindChest(Chest); Self->ShowRewards(D ? FString(D->Name).ToUpper() : TEXT("CHEST"), Chest, Out2); }
					else { int32 Gd = 0; for (const FZoogiFrontReward& R : Out2) if (R.Kind == FName(TEXT("gold"))) Gd += R.Amount; Self->Say(FString::Printf(TEXT("TASK DONE! +%s GOLD"), *ZNumS(Gd))); Self->Sfx(TEXT("UiConfirm")); }
				});
		}
	}

	void PaintNews(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		struct FN { const TCHAR* Tag; const TCHAR* Head; const TCHAR* Body; };
		static const FN News[] = {
			// b90 (Evan: "news update with what we actually changed so I can use it as a checklist - tell folks to try it out and give feedback");
			// b127 (Evan: "why isn't news updating - make more and scroll on page"): every build from b108, newest first, scrolling list
			{ TEXT("TEST"), TEXT("TRY IT - SEND FEEDBACK"), TEXT("Play a few matches and check off each item below, newest build first. Tell us what feels great and what is broken. Swipe (or use the mouse wheel) to scroll.") },
{ TEXT("B286"), TEXT("KING OF THE HILL: BUILD AND BREAK"), TEXT("King of the Hill is now a siege: the centre ring starts inside a fort of ice walls (3 knocks shatter one, and they rise again every 3 rounds). End your turn in or near the ring and you build your own wall toward the nearest rival. Smash a wall you didn't build for +5, and every Zoogi in the ring at the end of a round still scores 10.") },
{ TEXT("B285"), TEXT("NO MORE ENDLESS FALLS + RAID TIPS"), TEXT("A Zoogi that drops off the arena can no longer fall forever: while rolling it counts as a ring-out, and at any other time it is rescued back onto safe ground. The menu waits until your Zoogi has landed. The RAID loading screen now leads with KNOCK ORBS OFF = DAMAGE.") },
{ TEXT("B284"), TEXT("SWIPE TO LOOK AROUND"), TEXT("During a REDIRECT pause, swipe to turn the camera round your Zoogi and find targets that were off screen - a swipe never picks. A tap still picks the glowing target under your finger.") },
{ TEXT("B283"), TEXT("LOADING SCREENS FOR EVERY MODE"), TEXT("Every mode now opens on its own loading screen with fresh how-to cards: Classic, Raid, Battle Royale, Multiplayer, Training, King of the Hill, Hot Orb, Orb Storm, Boss Rush, Trick Course and the Daily Trick Shot. Tap to start when it is ready.") },
{ TEXT("B282"), TEXT("CAMERA BACK FOR REACTION"), TEXT("After a strong launch the camera rides behind your Zoogi - now, the moment the game pauses for a REACTION, it glides straight back to the normal match view so you can see your targets.") },
{ TEXT("B281"), TEXT("PORTRAIT MENU BACK AT YOUR ZOOGI"), TEXT("Holding the phone upright, the command menu (SKILL, AWAKEN, SHOWTIME and the rest) opens beside your Zoogi again, like before. Landscape keeps it at the SUPERPOWER button.") },
{ TEXT("B280"), TEXT("SKILL HOW-TO AT THE BOTTOM"), TEXT("In landscape, tapping a skill (in the skill list, an item, or a COMBO plate in a REACTION pause) shows what it does and how to use it in a band along the bottom of the screen. It stays while you aim the skill.") },
{ TEXT("B279"), TEXT("SKILL + REACTION MID-ROLL"), TEXT("Casting a combo skill during a REACTION pause no longer kills your roll: once the skill is done your Zoogi keeps its speed and the REACTION pause opens again, so you can mix skills and REACTION redirects in one roll.") },
{ TEXT("B278"), TEXT("COMBO PLATES NEXT TO REACTION"), TEXT("Your COMBO skill plates now stack up just right of SUPERPOWER and stop at the edge of the REACTION button, so REACTION is never covered during a pause.") },
{ TEXT("B277"), TEXT("COMBO PLATES RIGHT OF SUPERPOWER"), TEXT("In a REACTION pause, your COMBO skill plates now stack up on the right side of the SUPERPOWER button, so your thumb lands on them quickly.") },
{ TEXT("B276"), TEXT("MENU AT SUPERPOWER + COMBO IN REACTION PAUSES"), TEXT("The command menu (ATTACK, SKILL, GUARD, ITEM, AWAKEN, SHOWTIME) now opens around the SUPERPOWER button instead of over your Zoogi. During a REACTION pause, your queued COMBO skills appear there as P5 plates with their names - tap the one you want to cast it right then. When the pause ends they hide again and only the REACTION button shows.") },
{ TEXT("B275"), TEXT("CPU SPEECH BUBBLE MOVED"), TEXT("The CPU's speech bubble (\"Let's see... there!\") no longer covers the MENU button and the timer - it sits just to their left.") },
{ TEXT("B274"), TEXT("HOT ORB FIX"), TEXT("HOT ORB now knows who touched the bomb last (it blamed nobody before), so the blast sends the right Zoogi to the edge and clearing the bomb pays +20 to the right player.") },
{ TEXT("B273"), TEXT("PARTY MODES + PLAY STYLES"), TEXT("Mode Select has a PARTY MODES card with 5 new modes: KING OF THE HILL (end each round in the gold centre ring for +10), HOT ORB (the red bomb orb blows up after 4 turns - whoever touched it last is blasted to the edge), ORB STORM (90 seconds, orbs keep raining in), BOSS RUSH (three titans in a row on one pool of team lives) and TRICK COURSE (3 holes - knock the glowing orb into the gold hole in as few shots as you can). INVENTORY > SKILLS has 5 PLAY STYLE buttons - BRUISER, SWEEPER, FORTRESS, TRICKSTER, SPEEDSTER - that set a matching hand in one tap. PILLAR FIELD pillars are pinball bumpers now, and FAULT LINE tips everything on its outer side toward the edge.") },
{ TEXT("B272"), TEXT("9 NEW SKILLS"), TEXT("GRAPPLE HOOK (yank yourself to a rival or orb), EDGE SAVE (a vine pulls you back from the edge), BOUNCE PAD (rebounds anything 40% faster), RICOCHET (your next wall hit doubles your speed), DECOY (a ghost that stops the next rival dead), TETHER (chain two rivals - hit one, drag the other), TIDE SHIFT (the arena tilts for a round), HEX (a rival loses its aim line and is capped at 70% power) and STAR MAGNET (star orbs drift to you). All nine are unlocked for everyone - add them to your hand in INVENTORY > SKILLS.") },
{ TEXT("B271"), TEXT("ELEMENT BONUS + RAID RESCUE"), TEXT("ELEMENT BONUS: two skills of the same element in your deck give a bonus - FIRE +10% skill knockback, FROST orbs you hit slide further, SHOCK +1 redirect crystal every turn, VOID +1 crystal per knockout. RAID: every Zoogi gets one free rescue (its first fall costs no team life), a weak-point hit in the same shot as an orb volley does double damage (COMBO x2), and an MVP card shows the top damage dealer at the end.") },
{ TEXT("B270"), TEXT("SKILL REVIEW: FIXES"), TEXT("SWORD SLASH is a 1.5 s spin dash that slashes everything along your path. CROSS SLASH is a counter: the next rival who rams you is slashed away. FROST SPIKES is a hidden trap for 2 rounds. ICE SHARDS leave slippery ice patches. GLACIER LANCE pierces on behind its target. ICE WALL and SAPPHIRE WALL: your ball bounces off them 25% faster and a rival that hits them loses 1 SP. IMPULSE is bigger the faster you roll (up to 2x). ENERGY BUFF costs 2 SP. ENERGY SHOCKWAVE pulls from a smaller area. CRESCENT SLASH shoves rivals harder. CPUs now use Apple Bombs, Frost Spikes, Ice Shards, Pillar Field and Fault Line.") },
{ TEXT("B270"), TEXT("RAID BALANCE + KO CAMERA"), TEXT("RAID: an orb volley now does at most 15 damage per shot, so a titan lasts about 6 to 8 rounds instead of falling in 4. The knockout camera sits further back and higher, so the ring-out burst no longer fills the screen.") },
{ TEXT("B269"), TEXT("FULL CRYSTALS + RAID ORB VOLLEY"), TEXT("You start every match with full crystals. RAID: every orb you knock out of the arena now flies into the titan for 3 damage (a big shot that clears 6 orbs = 18), and CPU teammates only go for the weak point when they are close enough to reach it - otherwise they clear orbs. The knockout camera now looks out over the rim at the stands. The COMBO badge sits below the AWAKEN bar, and the TURN CHALLENGE waits until the round award card is gone.") },
{ TEXT("B268"), TEXT("MORE SPECTACLE"), TEXT("Knockouts get a moment: a short slow-motion, a low camera on the court watches the Zoogi fly off, the crowd jumps up and confetti blasts out of the stands. After a strong shot (70% power or more) the camera rides behind your ball for a moment. Every extra orb you knock out in one shot lights the next ring outward in gold with a rising note. In the final round the lights dim and the arena pulses gold on the beat of the music.") },
{ TEXT("B267"), TEXT("AWAKEN CAMERA"), TEXT("When an Awaken fires, the camera cuts low behind your Zoogi, looks up at the vortex opening in the sky, follows the meteor down onto your Zoogi through the explosions, then goes back to the match. Meadow fans sit only on the wooden bleachers (none on the grassy slope). The skill card and the COMBO badge no longer cover the MENU button, timer or AWAKEN bar.") },
{ TEXT("B266"), TEXT("AWAKEN SHOW + CLEAN SCREENS"), TEXT("SHOWTIME Awaken now plays the full show: a vortex opens in the sky, a meteor lands on your Zoogi, then its Awaken fires and explosions go off. Chest rewards land in one straight row (no tilted cards) over a dark backdrop. Meadow fans sit on the bleachers instead of floating in front of them. LONG RAMPART and MAGIC RAYS are no longer skills. Nothing covers the screen any more: messages sit at the top edge, comments stay below the MENU button and timer, the BLOCK burst is smaller, NEW badges no longer sit on the item counts, and long descriptions shrink to fit.") },
{ TEXT("B265"), TEXT("AWAKEN CINEMATIC + NEW CHEST CARDS"), TEXT("AWAKEN is a show now: a vortex opens in the sky, a meteor drops onto your Zoogi, then your Zoogi's own Awaken fires and explosions go off around it. Chest rewards all use one card style: full name in a band at the bottom, the count in the top corner (it no longer covers the name), and skills show their art. The VOYAGER card is removed from Mode Select, and Battle Royale no longer starts on Ancient Collision Meadows.") },
			{ TEXT("B264"), TEXT("SHOOTER CAMERA"), TEXT("SHOOTER modes: when you lock on and shoot, the camera zooms in over your shoulder, and every shot kicks the view up a little (recoil) before it settles. Builds 254-263 tested the Voyager add-ons - none opened on iPhone, so the Voyager feel is being rebuilt inside Zoogi Roll one piece at a time. Enemy shots still never shove you.") },
			{ TEXT("B253"), TEXT("OPENS AGAIN"), TEXT("Builds 241-252 would not open on iPhone: the full Voyager import made the game run out of start-up room. Voyager is switched off for now (the SHOOTER modes use the earlier shooter) while it is rebuilt to load only when you pick it.") },
			{ TEXT("B252"), TEXT("LAUNCH CHECK"), TEXT("Writes ZoogiLaunch.txt (Files app > On My iPhone > Zoogi Roll) so the launch problem can be found.") },
			{ TEXT("B251"), TEXT("LAUNCH FIX"), TEXT("Zoogi Roll opens on iPhone again: the engine ran out of object slots while starting up after the Voyager content was added - the limit is raised. Old Voyager saved games are also set aside at launch.") },
			{ TEXT("B250"), TEXT("BACK TO BUILD 245"), TEXT("Builds 246-249 would not open on iPhone, so this build is the build 245 game again. Voyager touch controls, auto cover and the new Voyager card art return once the cause is fixed.") },
			{ TEXT("B245"), TEXT("WALL SMASH POP-OUT"), TEXT("The wall-smash moments (FIRST KNOCKOUT!, MEGA HIT! ...) no longer take over the screen: the flying-ball knockout anime pops out in the side window like the other celebrations, with the title on top, and the game keeps going.") },
			{ TEXT("B244"), TEXT("SMARTER CPU: KNOCK + BLOCK"), TEXT("CPU Zoogis now carry an earth wall and use both powers in one roll: they knock orbs out and, while still rolling fast, raise the wall across the nearest rival's way to the orbs (EARTH WALL - BLOCKED!). Watch them - then do it yourself: launch, then cast while you still have momentum. FIX: earth-bending walls and rocks are solid now - Zoogis and orbs bounce off them instead of passing through (they used to switch off on their first frame).") },
			{ TEXT("B243"), TEXT("NO SOLDIERS, NO ARCHES, DARK ARENA"), TEXT("Removed the two-legged soldier / robot from Classic, Raid and Battle Royale: in Battle Royale everyone stays a ball, and the Voyager soldier models only appear in the VOYAGER modes. Every arch and gate obstacle is gone from all arenas. NEW: Settings > DARK ARENA keeps the SHOWTIME dimmed lights on for the whole match.") },
			{ TEXT("B242"), TEXT("VOYAGER CARDS"), TEXT("The VOYAGER card and its 3 modes have their own card art now (they showed the BATTLE ROYALE picture).") },
			{ TEXT("B241"), TEXT("VOYAGER MODE"), TEXT("Mode Select has one VOYAGER card now: tap it to pick HOLD THE LINE, SHOOTER ROYALE or TEAM DEATHMATCH (the real Voyager game). BACK returns to the modes.") },
			{ TEXT("B240"), TEXT("REAL VOYAGER SHOOTER"), TEXT("The SHOOTER cards now open the real Voyager game: Voyager's hero, movement, cover, weapons and enemies. HOLD THE LINE = the wave assault, SHOOTER ROYALE = the advanced AI arena, TEAM DEATHMATCH = the cover course for now. On iPhone Apple's on-screen controller appears (left stick move, right stick look, triggers aim / fire). EXIT top-left goes back to Zoogi Roll.") },
			{ TEXT("B239"), TEXT("BANNER + INVENTORY POLISH"), TEXT("The round award banner now drops on a clean screen (the turn cards step aside) and waits for a wall-smash cinematic to finish. INVENTORY skills: bigger cards - 4 across on phones, 3 on narrower screens.") },
			{ TEXT("B238"), TEXT("WALL SMASH + AWARD BANNER + NEW INVENTORY"), TEXT("The wall-smash cinematic now plays in real matches: MATCH POINT KO (a knockout in the last round), AWAKEN KNOCKOUT, WALL SLAM, FIRST KNOCKOUT and MEGA HIT (a Perfect Hit on a rival) - at the start of the next turn, a few times a match at most. A hanging stadium banner drops at the end of each round for the round's star (KNOCKOUT KING / ORB HUNTER). INVENTORY has the new look: gold-framed cards, new skill art, neon panels. SHOOTER modes: enemy shots no longer shove or stun you.") },
			{ TEXT("B237"), TEXT("MEADOW VILLAGES"), TEXT("Six hill villages now ring the Meadow arena behind the stands - houses with red, blue, green and purple roofs, trees, halls, a shrine and a pagoda - with mountains beyond. BATTLE ROYALE and the shooter modes now play on Meadow (Ancient Collision Meadows is out of Battle Royale).") },
			{ TEXT("B236"), TEXT("SHOOTER: CRYSTALS + BALL DASH"), TEXT("In the shooter modes every orb you pick up is a crystal. With 3 crystals, TRANSFORM rolls you up into your ball for a fast dash (2 seconds per crystal), then you are back on foot. Your soldier now wears your Baller's own 3D head. Removed: the spider boss (the Tomb titan is back), the soldier and turret on the arena floor, and the soldiers in the wall-smash cinematic - two-legged soldiers only appear in the three shooter modes. 5 lives in HOLD THE LINE.") },
			{ TEXT("B235"), TEXT("EPISODE 5: RING GUARD"), TEXT("STORY has a new episode. The cores are free but the Rings have gone dark, so nobody can roll - Master Orin opens the Ring Guard armoury and the Zoogis fight on foot. Three missions on the shooter modes: the Ring Guard Trials, Squad vs Squad against Vex, and Hold the Line against the Void Hollows. The soldiers now wear their Zoogi's colours with the Zoogi's own face as the head; the Hollows are black and purple.") },
			{ TEXT("B234"), TEXT("SHOOTER MODES"), TEXT("Three new 3rd-person shooter modes in Mode Select - everyone on foot as a Voyager soldier with Voyager guns. HOLD THE LINE: you, a Voyager trooper and a minigun turret against 5 waves (wave 5 is the boss wave). SHOOTER ROYALE: last one standing. TEAM DEATHMATCH: 2v2, no friendly fire, first team to 20 knockouts.") },
			{ TEXT("B233"), TEXT("VOYAGER ARRIVES"), TEXT("The Voyager shooter pack is in. Battle Royale: your hero is a Voyager soldier with real rifle running, jumping and a Heavy Machine Gun. The wall-smash cinematic uses the Voyager fighters (a real punch, then a ragdoll off the rock). Turret ammo has Voyager rocket smoke, tracer and plasma trails and explosions. RAID: a Voyager trooper and a minigun turret fight beside you, and the TOMB titan is now a giant spider.") },
			{ TEXT("B232"), TEXT("CLEAN CINEMATIC"), TEXT("Behind the scenes: the action menu, pad buttons and speech bubbles stay off the screen while the wall-smash cinematic plays.") },
			{ TEXT("B231"), TEXT("CINEMATIC POLISH"), TEXT("Behind the scenes: the wall-smash cinematic now plays on a clean screen (no menus or orbs in the way) with a clear view of the rock.") },
			{ TEXT("B230"), TEXT("SIGNS THE RIGHT WAY UP"), TEXT("The fans' signs now read the right way up. Behind the scenes: the wall-smash cinematic (a Baller punches a rival back into a rock and the FIRE BLAST bursts out of it) is built - pick when it plays and it goes live.") },
			{ TEXT("B229"), TEXT("TIRE SMOKE + FIRE BLAST"), TEXT("Your Zoogi spins up and pours tire smoke while you pull back to aim, and the launch kicks up a smoke burst. A ball that slams into a rock now bursts a FIRE BLAST out of the rock face. The fans' signs now show their words (they were black boards). The item count is a solid gold pill. Chest items now add to your starting items instead of being lost. ORB RELAY refunds are capped at your real dash count.") },
			{ TEXT("B228"), TEXT("ORB RELAY 2 + SIGN-WAVING CROWD"), TEXT("ORB RELAY: tap the court and you dash there at once (no SUPERPOWER), a short slow-mo follows each dash, and a dash that hits an orb or a rival gives the dash back and builds a RELAY combo. In a RAID, dashing into the weak point hurts the boss. The crowd is now round Zoogi fans in team colours, and some hold up signs (GO BLUE!, KO!, BOOM!...) that pop up on big moments. Chests now drop items too: the ITEM tab shows +N NEW, the count in the corner, and GET FROM CHESTS when you run out.") },
			{ TEXT("B227"), TEXT("ITEM COUNTS"), TEXT("Every item card in the inventory shows how many you carry in a gold pill (top-right), and the Golden Apple has its own card. Skills have no counters.") },
			{ TEXT("B226"), TEXT("ORB RELAY"), TEXT("GATTLING RAPID is now called ORB RELAY: tap where to go, then press SUPERPOWER to dash there, 10 times.") },
			{ TEXT("B225"), TEXT("TURRET IN THE SKILL LAB"), TEXT("The Gatling Turret is in the Skill Lab: tune its STRENGTH, RADIUS, HEIGHT, FIRE RATE, SHOTS, AMMO, FX SIZE, COLOUR, BLAST and BLAST SIZE, then PLAY to watch it fire at the orb rack.") },
			{ TEXT("B224"), TEXT("RAPID DASH FIX"), TEXT("Casting GATTLING RAPID before your launch no longer blocks aiming. The Gatling Turret card text is shorter so it fits.") },
			{ TEXT("B223"), TEXT("TURRET AMMO + RAPID DASH"), TEXT("The Gatling Turret gets 10 shots and goes after the nearest orb or rival, and the boss in a RAID. Pick its AMMO in the inventory (tap the Gatling Turret): ROCKET, TRACERS, PLASMA, LASER or RING. GATTLING RAPID no longer pauses the game: tap where you want to go, then press SUPERPOWER to dash there, 10 times.") },
			{ TEXT("B222"), TEXT("ROCKET TURRET"), TEXT("The Gatling Turret now fires rockets: a nose cone with a smoke trail that flies straight out of the barrel and explodes on impact.") },
			{ TEXT("B221"), TEXT("TURRET FIRES FROM ITS BARREL"), TEXT("The Gatling Turret now shoots out of the tip of its barrel, straight ahead along the barrel, with no more sideways or curving shots. Five new shot styles are being tested.") },
			{ TEXT("B220"), TEXT("ITEM KNOCKOUTS PAY CRYSTALS"), TEXT("Knock a rival off on a turn you used an item - the Crystal Staff, Sword, Gatling, Grenade, Halo Disc or any other - and you earn +2 crystals.") },
			{ TEXT("B219"), TEXT("THE ROBOT IS BACK"), TEXT("The Meadow titan is the mecha robot again. The centre pillar is gone from the Meadow court and the big Z sits in the middle of the floor instead. RAID no longer offers the Meadow Arena.") },
			{ TEXT("B218"), TEXT("STONE GOLEM COLOURS"), TEXT("The animated Meadow golem gets its real stone colours back (B217 showed it nearly black).") },
			{ TEXT("B217"), TEXT("GOLEM COMES ALIVE"), TEXT("The Meadow golem has a full skeleton now - it breathes, looks around, chops and slashes on its attacks, flinches when hit, roars and falls. Banners clear away whenever the camera shows it.") },
			{ TEXT("B216"), TEXT("MEADOW GOLEM, WHOLE"), TEXT("The Meadow golem is back to its stone colour and stands whole on the rim - the boss camera and the boss window show it head to toe.") },
			{ TEXT("B215"), TEXT("TITANS FACE THE COURT"), TEXT("The Meadow golem and the Dark Realm crystal brute now face the centre of the court instead of looking sideways.") },
			{ TEXT("B214"), TEXT("NEW TITANS"), TEXT("Meadow's titan is now a purple stone golem and the Dark Realm has a crystal brute titan. Play a match or raid on each map to see them.") },
			{ TEXT("B213"), TEXT("SOLID ROCK BENDS"), TEXT("Earth-bend walls and pillars from skills are solid while they stand: bank off them or smash a rival into them. A hit blasts out of the rock's side.") },
			{ TEXT("B212"), TEXT("BLUE'S TIPS FIT"), TEXT("Blue's tip bubble on the home screen is bigger and wraps over up to 3 lines, so every tip fits.") },
			{ TEXT("B211"), TEXT("SPLASH + FIRST MATCH + BLUE"), TEXT("The game opens on the new ZOOGI ROLL ARENA splash. Your first launch plays a short guided match (SKIP any time), then Blue waits on the home screen with tips - tap him for the next one.") },
			{ TEXT("B210"), TEXT("RAID EDGE ARROW"), TEXT("In a raid, when the boss is off-screen a red arrow on the screen edge points at it with its face, distance and health. When it is on-screen, red brackets lock on to its weak point.") },
			{ TEXT("B209"), TEXT("NEW HOME + LOADOUT"), TEXT("The home screen shows your equipped Baller with a LOADOUT card beside it: Baller, Zoogi, Item and 2 Skills with your set POWER. Tap CHANGE on any row to swap it.") },
			{ TEXT("B208"), TEXT("NEW 3D BALLERS"), TEXT("Blue, Frosti, Lumi, Vex and Hotstreak now use their brand-new crystal armour models - open a Baller in INVENTORY and tap 3D VIEW to spin them.") },
			{ TEXT("B207"), TEXT("RAID HUD + LOADING SCREENS"), TEXT("Raids have ONE boss plate up top: the boss face, health with phase ticks, SHOWTIME + ANGER meters, team-life hearts and the round. Warnings share one slot under it, and a HOW TO HURT IT card shows what to do for each boss attack. In a raid, SHOWTIME is the BOSS ATTACK - your grade is the damage. Every raid and classic match now opens on a loading screen with 3 tips - TAP TO START.") },
			{ TEXT("B206"), TEXT("NEW MEADOW TITAN"), TEXT("The Meadow Arena boss is your Tally Golem now - the stone golem with the glowing lantern core, about 11 m tall, peeking over the rim (classic boss rounds and RAID on Meadow).") },
			{ TEXT("B205"), TEXT("SKILL ACTIVATED BANNER"), TEXT("Every time a skill goes off, the screen edges flash in the caster's colour and a skill card slides in from the right: the skill's picture, its name, who cast it and ACTIVATED! - so everyone knows it started.") },
			{ TEXT("B205"), TEXT("BIGGER BANK TRAIL"), TEXT("After a BANK, the trail on the court that shows where your Zoogi goes after it bounces off the wall is twice as big - and it always shows, even with the clean aim view on. The BANK! card is bigger too.") },
			{ TEXT("B204"), TEXT("BIGGER BANK PREVIEW"), TEXT("The BANK! preview card that shows where your shot hits the wall is 70% bigger, with thicker in / out arrows.") },
			{ TEXT("B203"), TEXT("3D VIEW, TURRET ITEM, DARK REALM"), TEXT("INVENTORY - BALLERS: tap 3D VIEW to see the Zoogi as a 3D model on the left (drag to spin) - the Dark Zoogi shows your own Tripo model. The GATLING item is now the GATLING TURRET: drop it from ITEM, it is no longer a skill. Stone Pillar, Earthquake and Rock Rampart are gone from SKILLS. New map: DARK REALM - the shadow underworld court.") },
			{ TEXT("B202"), TEXT("NEW ZOOGI: DARK"), TEXT("Your new Dark Zoogi is in the INVENTORY (ZOOGIS and BALLS tabs) and already yours: a navy shadow-crystal Zoogi with orange crystal spikes. Its Awaken is CRYSTAL BARRAGE - six crystal bolts at everything in reach. The TEASER ARENA is gone from the map list.") },
			{ TEXT("B202"), TEXT("YOUR TURRET"), TEXT("GATTLING TURRET now drops your own Tripo turret - the purple and gold cat statue with the Z emblem, about 2.4 m tall. It keeps turning to face the nearest rival or orb and, while things roll, fires a slash from the end of its barrel - one at a time, each shot kicks the turret back with a jolt and a rattle (and a buzz on your phone). Up to 8 shots a turn, for 2 of your turns.") },
			{ TEXT("B201"), TEXT("RAID CHALLENGES"), TEXT("Raid turn challenges now teach the tricks to beating the boss: DOUBLE TAP (hit the weak point twice in one roll), STAR STRIKE (a gold star orb into it), BANK SHOT (bounce first), INTERRUPT (hit it before its attack lands - the attack is cancelled), ARMOR BREAK (smash 2 ice shards - the team hits x2 next turn) and SAFE ZONE (hit it and stop outside the red zone). Each pays off in the raid: bonus damage, SP or crystals.") },
			{ TEXT("B200"), TEXT("RAPID = 10 REACTIONS + RAID BOSS FINDER"), TEXT("GATTLING RAPID now works exactly like REACTION: it gives you 10 crystals, so you freeze time and redirect your own Zoogi up to 10 times (it opens right away if you are rolling). RAID: a radar, a live boss window and a red screen-edge glow + aim line show where the boss is. Frozen: the tall cables in the sky are gone.") },
			{ TEXT("B199"), TEXT("RAPID COUNTER ON SUPERPOWER"), TEXT("During GATTLING RAPID the SUPERPOWER button shows the count, like Bolt Balls: a badge with the rapid art and xN shots on top, RAPID xN! under it, and a gold ring of dots that runs down with the 6 s clock. Tapping SUPERPOWER fires bolts too. GATTLING BURST and GATTLING TURRET have skill card pictures now (the turret is your new turret art).") },
			{ TEXT("B198"), TEXT("RAPID FIRE ICON"), TEXT("GATTLING RAPID has its own icon, like Bolt Balls: the bolt Zoogi firing a fan of purple and gold bolts inside a stopped clock. It shows on the skill card and over the RAPID FIRE counter.") },
			{ TEXT("B197"), TEXT("RAPID FIRE STOPS TIME"), TEXT("GATTLING RAPID now stops time like a REACTION: for 6 seconds every tap fires a Bolt Balls bolt (a counter and a clock show), the bolts hang in the air, then fly when time starts again.") },
			{ TEXT("B196"), TEXT("GATTLING REWORK + SOLID EARTH"), TEXT("GATTLING BURST and RAPID now fire real Bolt Balls bolts (the purple bolt that flies and blasts where it hits). The TURRET is a big gold tower with a glowing bolt orb on top that fires bolts at rivals rolling near it. SHOWTIME in portrait looks straight down on the court. Earthbender formations are solid all the way out to the effect's edge.") },
			{ TEXT("B195"), TEXT("COMBO ORDER"), TEXT("In a COMBO, AWAKEN and SHOWTIME now go off in the order you ticked them, mixed with your skills: launch, then each SUPERPOWER tap casts the next one - a skill, your Awaken or your SHOWTIME (it can start mid-roll; the roll waits for the show). Awaken's target is still picked before the launch.") },
			{ TEXT("B194"), TEXT("SHOWTIME BUTTON"), TEXT("SHOWTIME no longer starts by itself: when your meter is full a gold SHOWTIME plate (with your name) appears in your command menu, under AWAKEN when both are ready - tap it to start your show. With COMBO on, AWAKEN and SHOWTIME are in the list too and play in the order you tick them.") },
			{ TEXT("B193"), TEXT("ATTACK JOLT + DRACORIA"), TEXT("ATTACK now launches you at once along your aim with a tunnel-vision rush - then steer with REACTION. Boss cameras never cut away during a roll, and Dracoria shows up a little less. At round 2 Dracoria blasts the snowman and its platform (it hates snowmen), and it shatters any Earthbender rock that has stood for 2 rounds. The orb generator now pulses and sends up to 2 fresh orbs at every turn when orbs are missing, plus the full refill each round. GATTLING BEAT is now GATTLING RAPID: for 3 seconds every tap fires a shot, as fast as you can tap, while the orb generator keeps refilling. Volcano Pit's Stone Golem is about 45% bigger.") },
			{ TEXT("B192"), TEXT("3X FAST-FORWARD + EARTH EVENTS"), TEXT("On a CPU turn tap once for 2x, tap again for 3x. The Earthbender events start after round one and are gone from Cosmos. When a Zoogi or an orb hits an Earthbender rock, the ORB KNOCKOUT blast bursts on the rock where it hit.") },
			{ TEXT("B191"), TEXT("CLEAN BOSS DUELS"), TEXT("During a boss grab or a boss duel the command menu and the turn challenge step aside, so only the TAP prompt is on screen. The boss rounds, grabs, invasions, anger duels and Gattling lock-on run on every boss map - Cosmos, Frozen (Dracoria) and Volcano Pit (Stone Golem) - and lock-on shots aim at Dracoria and the Golem.") },
			{ TEXT("B190"), TEXT("FAST FX + GATTLING + BOSS"), TEXT("Skills land faster and the menu snaps open and shut. Three new GATTLING skills: BURST (6 rapid shots), BEAT (tap on the music beat for PERFECT shots) and TURRET (fires at rivals rolling near it for 2 turns). On boss maps the boss attacks every 3rd round, grabs the leader (tap fast to break free), invades with small slams (dodge = +1 crystal) and gets angry at big scores (win the reaction duel or it steals 50). Gattling shots lock on to the boss and weaken its attacks.") },
			{ TEXT("B189"), TEXT("WOOD SHIELD, GOLDEN APPLE, ENERGY BUFF"), TEXT("Wood Shield is cast on its own before the launch - no combos and never in the air. The Golden Apple is an ITEM now (you start with 3): throw it at an orb or at a rival's Wood Shield - its blast breaks the shield. Energy Buff costs 3 SP and gives 3 crystals right away.") },
			{ TEXT("B188"), TEXT("NEWS ARROWS"), TEXT("NEWS has UP and DOWN arrow buttons at the bottom: tap one to glide the list a page at a time (swiping still works).") },
			{ TEXT("B187"), TEXT("CAMERA FIX + LOUDER MUSIC + AUTO SHOWTIME"), TEXT("The camera can no longer get stuck after a shot, a replay or a ring out, and a new match always starts on the normal camera. The theme music is about 5x louder. SHOWTIME now starts by itself on your next aim once the meter is full, and the SHOWTIME bar stays on screen all match (grey when you need to take the lead). No more wait before you can aim after picking a skill.") },
			{ TEXT("B186"), TEXT("SHOWTIME FIX + AWAKEN NAME"), TEXT("Fixed SHOWTIME freezing with no targets late in a match (when the music had looped). The Awaken command is called AWAKEN again, so it can't be mixed up with the gold SHOWTIME button.") },
			{ TEXT("B185"), TEXT("THEME MUSIC FROM THE START"), TEXT("A theme now plays as soon as the game opens, in the menus too, and each match opens with its own random pick. The themes now load whole so they play on the phone. The old male announcer is muted on every map.") },
			{ TEXT("B184"), TEXT("MUSIC IS ON"), TEXT("The music switch is on: every match opens with one of the two new themes, picked at random, at your music volume. The old menu and arena music stay off, so nothing plays over the themes.") },
			{ TEXT("B183"), TEXT("SHOWTIME FIXES"), TEXT("The SHOWTIME camera now stays high over the dance floor for the whole show, so you can see every tile. At the SHOWDOWN the camera drops in and tilts up to watch the explosion climb over the court before normal play resumes. Points are rebalanced: a great SHOWTIME is worth around 250 points, a big reward that no longer decides the match on its own. Also fixed a crash that could happen after a few matches (the Meadow birdsong).") },
			{ TEXT("B182"), TEXT("SHOWTIME!"), TEXT("A new reward for the leader. While you're in the lead, every orb and rival you hit in a row fills the gold SHOWTIME bar at the top. When it's full, tap the gold SHOWTIME button: the match pauses, the court turns into a dance floor flashing to the music, and orbs gush from the generator onto the tiles. Tap anywhere on the beat and your Zoogi dash-hops through the next target - chain them all, rivals included. Hold your finger down to auto-fire on the beat, and your first two misses are free. It ends in a SHOWDOWN blast that throws the rivals outward - the bigger your combo, the harder it hits.") },
			{ TEXT("B181"), TEXT("ACTION CLOSE-UPS"), TEXT("When a rival's shot is about to hit you, the slow-motion moment opens with two quick action close-ups of the attacker - head-on, then low from the side - before the side view where you tap REACTION to block. You still get the full time to react. SP is now 10 every turn (was 5) - room for two skills, or a skill plus a combo.") },
			{ TEXT("B180"), TEXT("THE MEADOW RETURNS + SMASHABLE ROCKS"), TEXT("The Meadow is back to its old look - the island, the wooden bleachers, the village and the red shrines - around the new court ring, and the bleachers are packed with Zoogi fans who cheer and gasp. Fly into the bleachers and the crowd throws you back. Smash into a Meadow building for a BUILDING BLAST: it fires from the wall toward the middle and throws everything in its path. Each Earthbender event formation now stays on the court until it is smashed (at most 5 at a time - the oldest crumbles when a 6th rises). They land spread around the court (no more floating rock in the middle), make great bank shots, and break: crack one with a hit, smash it with a hard hit or a skill blast and it bursts into tumbling chunks. A REACTION while you are in the air now dives down at your target. The aim preview lines are a little thicker, the roll camera follows your Zoogi more closely (especially near the edge), and the TURN CHALLENGE card hides as soon as you start aiming. Two new match themes: every match opens with one of them, picked at random.") },
			{ TEXT("B179"), TEXT("EARTHBENDER EVENTS + BOUNTY + TURN CHALLENGES"), TEXT("STRIKE combos are gone. Every turn a gold crown marks the BOUNTY (the leader) - make them your first hit for +50. A TURN CHALLENGE gives you a small optional goal (2 orbs in one roll, hit 2 rivals, knock a rival off...) for points and AWAKEN meter. Missing costs nothing. Songbirds live on the Meadow court and fly off when you roll near. The skills list keeps 4 Earthbender skills (Sapphire Wall, Pillar Field, Long Rampart, Fault Line); the rest move to map events. Ground Slash now belongs to the Volcano Golem: its hammer sends the wave across the court. Dracoria roars for real, and your phone rumbles through her ICE BREATH. Frozen RAID: hit her HEART CRYSTAL. New Skills screen with big cards. On the Meadow, knocked-out Zoogis and orbs drop through the ground, and the centre Z is gone. Knockouts from skills and items now count. The AWAKEN bar moved right, away from the phone notch. Crescent Slash is a target skill now: tap up to 3 rivals or orbs and a slash flies at each. EARTHBENDER EVENTS: every map reshapes itself - a COURT SHIFT at the start of each round leaves solid rock for the rest of the match, TREMORS (watch for the warning circle) burst under the court mid-roll, and the final round opens with a CATACLYSM. Each map has its own stone.") },
			{ TEXT("B178"), TEXT("47 EARTHBENDER SKILLS + NEW MEADOWS"), TEXT("Every Earthbender effect is a skill now - pillars, walls, fault lines, bursts in slate, sandstone, jade, obsidian, sapphire, gold and rune stone. Meadow Hollow gets its new look, and Episode 4 opens in THE UNDERREALM. DRACORIA now dives out of the clouds, lands and roars to open every Frozen Ring match (tap to skip), and meets Frosti in Episode 1. Earth skills rumble, crack and smash. Skills show a big HOW TO USE card. Stone Pillar reaches twice as far and hits up to 3 targets, one after another. Rock Rampart runs along your swipe, with an arrow on the line. Volcano meteors wait while you pick a REACTION or a skill.") },
			{ TEXT("B177"), TEXT("DRACORIA - FROZEN BOSS"), TEXT("An ice dragon now rules the Frozen map. It lands on the rim, roars at knockouts, and its ICE BREATH dives in and sweeps a frost beam across the court - roll out of the marked slice or hide behind cover.") },
			{ TEXT("B176"), TEXT("9 NEW SKILLS"), TEXT("Frost Spikes, Glacier Lance, Ice Shards, Stone Pillar, Earthquake, Rock Rampart, Crescent Slash, Cross Slash and Ground Slash. They replace the old card-book skills - win them from chests or buy them in the shop. Trainer Frosti now waits for your tap between steps. The finder trail and floor ring are gone - just the YOU pin now. The Magma Titan golem now ships to phones.") },
			{ TEXT("B175"), TEXT("WATCH THE GRAB"), TEXT("When a Zoogi grabs the edge and loses its grip, the camera drifts over so you can watch it. The YOU pin sits above your marble.") },
			{ TEXT("B174"), TEXT("FINDER FIX"), TEXT("Your ring, YOU pin and trail now sit right on your marble on iPhone, and stay up through the launch.") },
			{ TEXT("B173"), TEXT("CLEANER TOP"), TEXT("The yellow ROUND and YOUR TURN lines are gone - the player badges and the marble finder show whose turn it is.") },
			{ TEXT("B172"), TEXT("TRAINER FROSTI + WEATHER"), TEXT("The play guide is a hands-free trainer: Frosti plays the turn and explains Bolt Balls and Ice Wall. Matches can roll arena weather. Find your marble with the ring, face pin and trail. New shop menu.") },
			{ TEXT("B171"), TEXT("BIG PLAYER BADGES"), TEXT("Bigger player badges show the face, name, score, SP and crystals. Knockout blasts are back on the court edge and big blasts shake the screen. Meadow Hollow has a new rim and purple pavilion stands.") },
			{ TEXT("B170"), TEXT("NEW MODE CARDS"), TEXT("All six cards in mode select have new art: Story, Battle Royale, Multiplayer, Classic, Raid and Training.") },
			{ TEXT("B169"), TEXT("NEW STORY CARD"), TEXT("The STORY card in mode select has new art: Frosti vs Vex in the Zoogi Roll Arena.") },
			{ TEXT("B168"), TEXT("DEV MODE: NO PIN"), TEXT("DEV MODE opens straight away - no PIN to type.") },
			{ TEXT("B167"), TEXT("DEV MODE OPENS AGAIN"), TEXT("DEV MODE in the pause menu opens the dev panel again (PIN first on a new device); tap it again to close.") },
			{ TEXT("B166"), TEXT("VOID CALLING: NEW LOOK"), TEXT("Episode 4 now shows Frosti's void-crystal form on its cover and on the story map.") },
			{ TEXT("B166"), TEXT("TRAINING: LEAGUE COURT"), TEXT("The Zoogi League Court from Episode 1 is now a Training level - pick it first in TRAINING's map list.") },
			{ TEXT("B166"), TEXT("HARDER HITS"), TEXT("Zoogi-on-Zoogi hits now crunch: a heavy body thump and a rock-smash crack that grow with the hit, and a deep boom on the biggest slams.") },
			{ TEXT("B166"), TEXT("STORY 2V2 FIGHTS GET THE NEW CONTROLS"), TEXT("The six team battles in Story Mode (the Frozen Final, the Collapse, the Dunes, the Caldera Heart, the Sky Gate and Void Calling) now use today's arenas and the ITEM / SKILL / GUARD / ATTACK menu with drag-back launch. Your partner is still on your side.") },
			{ TEXT("B166"), TEXT("CHESTS NOW OPEN"), TEXT("Every chest opens with its own show: Common chests shake and burst open, Rare chests fan their cards out like a hand, Epic chests get a red slash reveal. Tap to skip to your rewards.") },
			{ TEXT("B165"), TEXT("STORY: EPISODE 1 STARTS AT THE LEAGUE COURT"), TEXT("The first story stop is now the Zoogi League Court, with a new map picture and title on the story map.") },
			{ TEXT("B164"), TEXT("ANCIENT MEADOWS: BACK TO NORMAL SIZE"), TEXT("Ancient Collision Meadows is back to its original size. The red-and-yellow centre drums stay gone, and the Z flags and see-through props stay.") },
			{ TEXT("B163"), TEXT("ANIME DIRECTOR (PC)"), TEXT("Evan can now place every anime clip himself on the PC: when it plays, on which modes and maps, full screen or in a window (landscape and portrait), intro and exit animations, pause or keep playing. Story scenes use the same tool.") },
			{ TEXT("B163"), TEXT("LEAGUE TRAINING COURT"), TEXT("The practice field is now the Zoogi League Training Court: the Ancient Meadows ring at dawn with bleachers, a PRACTICE scoreboard, light towers, banners and pink target pads. Episode 1's practice spar has no titan any more.") },
			{ TEXT("B163"), TEXT("ARENA SOUNDSCAPES"), TEXT("Every arena now has its own sound: meadow wind and birds, creaking ice, bubbling lava, the hum of space, a desert market, a chanting tomb, city traffic. The crowd builds up as the match goes on, a drumroll plays in the final moments, a missed shot gets an \"awww\" and a Zoogi falling off whooshes down and booms far below.") },
			{ TEXT("B162"), TEXT("ANCIENT MEADOWS: TWICE THE RING"), TEXT("The battle ring on Ancient Collision Meadows is now twice as wide. The islands, volcano and bridges moved out with it, so the whole map is bigger.") },
			{ TEXT("B161"), TEXT("ANCIENT MEADOWS: CLEAR VIEW"), TEXT("The red-and-yellow drums in the middle of Ancient Collision Meadows are gone, and any prop between the camera and your Zoogi turns see-through right where it blocks you. New Ancient map picture.") },
			{ TEXT("B160"), TEXT("ZOOGI FLAGS EVERYWHERE"), TEXT("Four purple-and-gold Z flags now wave round every arena, lower and bigger so you always see one.") },
			{ TEXT("B159"), TEXT("SMOOTH SCROLL + TRICK SHOT POP-UP"), TEXT("Press-and-hold scrolling in NEWS and the SHOP glides with your finger, no stutter. For now the DAILY TRICK SHOT pop-up opens every time you start the app.") },
			{ TEXT("B158"), TEXT("BANK OFF THE TITAN + TRICK SHOT PLAZA"), TEXT("The map titan is solid now: its hands, arms and head stop Zoogis and orbs - bank shots off it (it never lets anything pass through). Daily Trick Shot on Ancient Meadows: the ring of drums around the centre is gone so shots run clean across the plaza; the curb ring, hazard drums and lantern posts stay for bank shots. Every arena now flies two purple-and-gold Z flags waving in the wind. The ON FIRE circles are thinner. DAILY TRICK SHOT is no longer in the mode list: it pops up on the main menu once a day at start-up (PLAY! or LATER).") },
			{ TEXT("B156"), TEXT("DAILY TRICK SHOT + SHOP & LEVELS"), TEXT("BATTLE -> DAILY TRICK SHOT: one set-up shot a day, the same for everyone. 3 tries, your best counts: 1 star per rival you knock off in ONE shot (all 3 = 3 stars). New stars pay coins, gems and XP; play daily for a streak (every 7th day = +20 gems). STORY is now first in the mode list. SHOP: coloured tiles, real gem / coin art, a BUY pop-up on every purchase, and items that unlock at a player level (LV badge). A LEVEL UP pop-up shows what you just unlocked. Every button vibrates and squashes when you press it. NEWS: TOP works, smooth scrolling, a bigger X. The Daily Trick Shot rotates Ancient Meadows, Volcano, Arabian Nights, Tomb and Neon. BATTLE ROYALE now opens on ANCIENT MEADOWS, with the ZOOGI KING's throne on the Sky Citadel.") },
			{ TEXT("B155"), TEXT("THE TITAN PICKS ITS MOMENTS"), TEXT("Classic opens on the arena forming - no titan. It rises over the rim for its attack rounds and to watch a knockout, then sinks back out of sight. In RAID it rises and roars right after the arena forms and stays for the fight.") },
			{ TEXT("B154"), TEXT("MULTIPLAYER + NEW MODE LIST"), TEXT("BATTLE -> MULTIPLAYER replaces 2V2: pick 2, 3 or 4 players, pick the map, and pass the phone - everyone plays classic on one device. RAID now sits right under CLASSIC (with its own card art). Raid teammates aim only at the titan - no more friendly knock-offs. Episode 3 films have sound. The air-flight zoom cam is gone.") },
			{ TEXT("B153"), TEXT("BATTLE ROYALE IS ALL CLASSIC"), TEXT("No more transform brawl: every Battle Royale round is a classic round - take turns, one shot each - and every round moves to the next arena with its titan (Meadow, Frozen, Volcano, Cosmos, Arabian Nights, Tomb, Neon, Star). Points carry over; the highest total wins.") },
			{ TEXT("B152"), TEXT("BOLT BALLS FROM THE AIR"), TEXT("Fire Bolt Balls while you are flying and each bolt shoots DOWN at its target on the ground (or the floor ahead) and blasts where it lands - you keep flying.") },
			{ TEXT("B151"), TEXT("RAID MODE"), TEXT("BATTLE -> RAID -> pick a map: you and 2 CPU allies against that map's titan. Hit its glowing hand on the rim - roll into it or knock orbs into it (faster = more damage, gold stars hit hardest); SHOWTIME hits it for 20%. After every round it attacks (dodge the red zone) and moves; at 66% it moves after every turn, at 33% it is ENRAGED. Falls cost team lives (5). Beat it within 10 rounds - the results show the MVP. STORY: Episode 3 now plays an anime clip before every level and at the end, and BLACKOUT BRAWL is a classic match (no more transform brawl).") },
			{ TEXT("B150"), TEXT("OLD ARMS REMOVED"), TEXT("The little leftover arms from the old character models no longer show up around any Zoogi (they used to reappear after launch).") },
			{ TEXT("B149"), TEXT("BATTLE ROYALE: CLASSIC + ACTION ROUNDS"), TEXT("Battle Royale now alternates CLASSIC rounds (take turns, one shot each - the map boss attacks after the last shot) and ACTION rounds (drive, jump, transform and shoot for 45 seconds), and every round moves to the next arena. Points per round: classic rounds score like Classic, action rounds give + points per ring-out / destroy and - points per fall. The totals carry over and the highest total after the last round (always a classic round) wins.") },
			{ TEXT("B148"), TEXT("A BOSS FOR EVERY MAP"), TEXT("Every arena now has its own titan on the rim, watching, reacting and attacking in the closing rounds: GROVE TITAN (Meadow), FROST TITAN (Frozen), MAGMA TITAN (Volcano), VOID TITAN (Cosmos), DJINN TITAN (Arabian Nights), PHARAOH TITAN (Tomb), NEON TITAN (Neon) and STAR TITAN. Placeholder looks for now - each is the mech in that map's colours.") },
			{ TEXT("B147"), TEXT("BOSS ATTACKS: BRACE, HANG ON, SCORE"), TEXT("TAP TO BRACE when the boss hand is on you: you get knocked just over the rim and hang on - tap to pull yourself back up (free). Knock a rival or orb into the red zone and the boss ring-out scores for YOU. CPU Zoogis now dodge the zones too (or GUARD against the slam). A boss camera shows each attack land, and the zones glow brighter.") },
			{ TEXT("B146"), TEXT("THE COSMOS BOSS ATTACKS"), TEXT("In the closing rounds on Cosmos the boss wakes up. Each round it marks a red zone on the floor with a countdown - HAND SWEEP (a slice of the outer court) or GROUND SLAM (rings around a spot) - and it lands after everyone has had a turn. Roll out of the zone, hide behind a pillar or wall (cover stops the hand), GUARD to anchor against the slam, and when the hand is on you TAP to brace. It usually goes after the leader. Boss ring-outs score for nobody.") },
			{ TEXT("B145"), TEXT("AWAKEN ANIME IN THE TOP CARD"), TEXT("Hotstreak's METEOR STRIKE Awaken now plays one anime clip (the second one is gone), and it plays in the top anime card instead of full screen - the meteor rain keeps going underneath so you can watch it land.") },
			{ TEXT("B144"), TEXT("SKY DIVE CAM + GRENADE FIX"), TEXT("Thrown high into the air (a banked Bolt Balls, a bounce off the stands)? The camera now swoops up above your Zoogi with a burst of slow-mo and a SKY HIGH! callout, then rides it back down to the landing with a thump - and it never gets stuck up high afterwards. A saved PRISM GRENADE thrown early in a match now always flies and explodes.") },
			{ TEXT("B143"), TEXT("SUPERPOWER FIRES YOUR AWAKEN, ITEMS OPEN"), TEXT("The POWER entry is gone from the command fan - after SHOWTIME, press the star SUPERPOWER button to unleash your Awaken (like a banked cast). ITEM is no longer locked when you carry an item with uses left. Inventory: the WEAPON tab is now ITEM, with the ZOOGI ROLL ARENA burst behind each item. BALLERS show their crystal heads (Frosti, Hotstreak, Vex, Lumi). AUTO and ARC are gone - just drag back anywhere to launch (touching an orb or rival works too), and the arrow is much easier to grab. You carry up to 5 of each item and start with 5 (Crystal Sword and Staff: 3).") },
			{ TEXT("B142"), TEXT("COSMOS BOSS PEEKS, STANDS SAVE YOU"), TEXT("Cosmos: the boss now hangs on the rim right in front of you every turn - hands on the edge, looking at you - and slams it on knockouts. No gravity on Cosmos: a knocked-out Zoogi floats away, then explodes. The rim no longer bounces orbs back in. Stands (Frozen and the arenas with grandstands): fly into them and you are NOT out - the crowd throws you back with a CROWD TOKEN (+1 crystal). Skill Lab: ORB KNOCKOUT and PLAYER KNOCKOUT entries to swap the knockout explosions. SHOWTIME now arms your Awaken: press SUPERPOWER to unleash it (Hotstreak: a meteor rain), then the full screen anime plays. The golden apple is solid gold with an icon above it. The centre scoreboard gets out of the way when you are under it. Energy Buff now costs 4 SP. Water Cannon: its water ball flies at the target and hits when it arrives. Training: CPU OPPONENTS works again. NEWS has TOP and CLOSE buttons. Volcano meteors only hit Zoogis and count for nobody.") },
			{ TEXT("B141"), TEXT("ENERGY BUFF: NO CRYSTAL CAP"), TEXT("Energy Buff always adds a crystal now - it can stack past 4 and works even if you already got a crystal this turn. Extra crystals show as +N next to the four circles.") },
			{ TEXT("B140"), TEXT("KO BUZZ, KO BLAST ON THE SPOT"), TEXT("Every knock-out now buzzes the phone for 2 seconds. The knock-out blast goes off right where the Zoogi was knocked out (it used to pop on the court edge). Frozen: the unfinished curved sky ramps and their floating poles are gone.") },
			{ TEXT("B139"), TEXT("THE VOLCANO ERUPTS"), TEXT("Volcanic Pit: the volcano erupts at random during rolls - red METEOR! circles flash on the floor, then 5 meteor strikes slam those spots. It happens more often every two rounds. Fire Meteors is no longer a skill card - it belongs to the volcano now. CPUs only use skills you have unlocked. SKILL LAB: BLAST cycles for every skill (skills without a blast get one where they hit) and a new BLAST SIZE row.") },
			{ TEXT("B138"), TEXT("GOLDEN APPLE STAYS, ZOOGIS ON BALLERS"), TEXT("Golden Apple: after its blast the golden apple stays on the court - the first Zoogi to touch it sets off a medium blast. BALLERS cards show the Zoogi itself instead of the ball (as the art arrives).") },
			{ TEXT("B137"), TEXT("WEAPONS IN ITEM, PRECISION, NEW ICONS"), TEXT("INVENTORY - WEAPON: tap up to 3 weapons to carry - they show in ITEM during a match with how many you own (you start with 3 of each). One item a turn: Gatling (3 quick shots), Prism Grenade, Crystal Sword (slash ring), Crystal Staff (heavy bolt), Halo Disc (cuts along a swipe). Grenades you pick up and don't throw are saved. PRECISION: tap up to 3 targets, each gets a water-cannon blast - no slow motion. New 3D chest, coin and gem icons. SKILLS ARE EARNED NOW: everyone starts with Bolt Balls, Ice Wall and Energy Buff - every chest can unlock a new skill (Epic chests always do). Energy Buff and Wood Shield are cast on their own, not in a COMBO.") },
			{ TEXT("B136"), TEXT("THE COSMOS IS WATCHING"), TEXT("On the Cosmic arena a colossal dark titan rises out of the void and watches the match: it follows the leader, its red eyes blaze on knockouts and criticals, it leans in on big REDIRECT combos, scoffs at misses and roars at the final round. The Skill Lab's strength and radius now work on every skill - Golden Apple, Apple Bombs, Wolf Pack, Bolt Balls, Ice Wall height and every delayed blast. METEOR RAIN is now IMPULSE: no aiming, a wolf-pack blast bursts out from where your Zoogi is (also mid-roll) and its anime plays every time. Menu buttons squash and buzz when you tap them. Ball cards show the ZOOGI ROLL ARENA burst.") },
			{ TEXT("B135"), TEXT("ONE AWAKEN METER"), TEXT("The SUPER bar is now your AWAKEN meter. It fills from perfect taps, blocks, scoring, knockouts and every card you cast. Full = SHOWTIME lights up before your shot (anime cutscene + your Awaken power). Getting knocked off empties it. Crystals are REDIRECT fuel only.") },
			{ TEXT("B134"), TEXT("GOLDEN APPLE, METEOR FINALE"), TEXT("Golden Apple: the Apple of Eden forms on you, a golden apple is lobbed to the orb you tapped and a bigger blast goes off where it lands. SHOWTIME Meteor Strike: a second clip of one giant meteor, then the camera looks up to watch it hit the arena.") },
			{ TEXT("B133"), TEXT("TRAINING BOTS, SHOWTIME ANIME"), TEXT("Training pause menu: CPU OPPONENTS - tap to add up to 4 practice bots. SHOWTIME with a Meteor Strike Zoogi plays a 10 second full-screen anime first.") },
			{ TEXT("B132"), TEXT("STORY EP3 + TEMPEST"), TEXT("Story mode episode 3 'Storm Season' with its films and the new rival TEMPEST. Top banners stay clear of the round header.") },
			{ TEXT("B131"), TEXT("CAMERA + CLEAN TEXT"), TEXT("The roll camera no longer flies into the sky following flung orbs. Skill plates, meter labels, combo counter, hints and banners no longer overlap.") },
			{ TEXT("B130"), TEXT("PRISM GRENADE PICKUP"), TEXT("From round 2 the orb generator drops a PRISM GRENADE - the first Zoogi to touch it takes it. ITEM - throw it twice the arc range for a shockwave where it lands. New grenade art. Cosmic markers are a deeper purple.") },
			{ TEXT("B129"), TEXT("WOLF TARGETS, COSMIC MARKERS"), TEXT("Pick a different target for each wolf before they run. Recalled wolves no longer push you off. Cosmic markers rise purple without smoke from round 2 and grow every round.") },
			{ TEXT("B128"), TEXT("BOLTS FIRST, NO SHOWTIME GLOW"), TEXT("Use up all your bolts or wolves before the next COMBO cast (CPUs too). Bolt Balls hit orbs right next to you. No pulsing glow on the orbs during SHOWTIME.") },
			{ TEXT("B127"), TEXT("NEWS SCROLLS"), TEXT("This news page scrolls and lists every build.") },
			{ TEXT("B126"), TEXT("REDIRECT HITS HARD"), TEXT("REDIRECT keeps its full speed all the way to the target you tapped - your Zoogi really travels there and smashes in.") },
			{ TEXT("B125"), TEXT("TAPS FIRE ON TOUCH, FAIR CPUS"), TEXT("REDIRECT and superpower targets fire the moment your finger touches them. A button press can never leave taps stuck. REDIRECT launches harder. CPUs play by your rules: every skill is cast after launch, one at a time, and Wooden Shield, Bolt Balls and Energy Buff never stack.") },
			{ TEXT("B124"), TEXT("SMARTER CPUS, ICE WALLS"), TEXT("CPUs pick skills that fit the moment (no cannon with nothing in reach, a shield when they're near the edge, meteors on a crowd), cast aiming skills mid-roll, go after the leader and rivals near the edge, look one move ahead, and spend a crystal to save themselves from rolling off. Ice walls you build stay the whole match until 3 hits break them.") },
			{ TEXT("B123"), TEXT("COMBO PICKS MID-ROLL"), TEXT("A COMBO cast that needs a target, a line or an area opens the normal pick from where your Zoogi is mid-roll: FIRE casts it, CANCEL puts it back in the queue. Mid-roll picks keep the roll camera angle.") },
			{ TEXT("B122"), TEXT("REACTION VS SUPERPOWER"), TEXT("REDIRECT circles can be tapped on phones again. REACTION and SUPERPOWER each do their own job at any time of the roll - REACTION closes an open REDIRECT pick, SUPERPOWER closes it and does its own job (next COMBO cast, wolves, bolts, ultimate).") },
			{ TEXT("B121"), TEXT("SHOWTIME, GUARD, COMBO CASTS"), TEXT("SHOWTIME: the phone rumbles 2 s, the ancient Frozen symbols glow, the lights dim, an energy buff flares - then tap your target. GUARD raises a 10 s force field round your Zoogi. COMBO: tick skills, CAST - launch, then each SUPERPOWER tap fires the next cast from where your Zoogi is. No target in range = no REDIRECT slow-down.") },
			{ TEXT("B120"), TEXT("WEAPONS, MAPS, BUTTON TAPS"), TEXT("WEAPON tab: Prism Grenade, Crystal Sword, Crystal Staff and Halo Disc - equip one for Battle Royale. New map pictures (portrait fills the screen), portrait inventory fills the screen. The black-and-white impact flash lasts 0.2 s. Button taps never count as a target tap on the field.") },
			{ TEXT("B119"), TEXT("WOLVES, PRECISION, FROZEN"), TEXT("Wolf Pack orbs wait on the field with you - tap WOLF while aiming or rolling, the closest wolf strikes. PRECISION fires 3 missile orbs. Frozen Ring: gap filled, no bridges or walls. No slow-mo duel ring; knockouts play anime clips every other turn. SKILL opens the command menu; wider aim arrow; landscape aim camera behind your Zoogi.") },
			{ TEXT("B115"), TEXT("ENERGY BUFF = +1 CRYSTAL"), TEXT("Energy Buff gives you +1 crystal (the circles under SP) on top of the heavier, faster roll. In the Skill Lab the Wolf Pack picks its own targets so you see the whole run.") },
			{ TEXT("B114"), TEXT("ENERGIZED WOLF PACK"), TEXT("Wolf Pack now carries the Energy Buff glow: it flares under your Zoogi as the pack is called and under every wolf as it runs. Skill Lab shows the reach ring and height arc of every PLAY.") },
			{ TEXT("B108"), TEXT("SKILL LAB BUTTON"), TEXT("SKILL LAB is a black button on the home screen now (not a skill). Just the effect and the orbs: pick with the arrows, PLAY, RESET cleans the field, swipe to turn the camera.") },
		};
		const int32 N = UE_ARRAY_COUNT(News);
		const bool bPortN = DH > DW;
		const float Ks = bPortN ? 1.3f : 1.f;
		const FSlateFontInfo BodyF = Font(1, 22.f * Ks);
		const float LineH = 30.f * Ks;
		// the card fills the screen; rows are as tall as their text, the list scrolls inside the card
		const FVector2D Sz(bPortN ? DW - 50.f : FMath::Min(1300.f, DW - 120.f), FMath::Max(420.f, DH - (bPortN ? 230.f : 200.f)));
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("NE"), TEXT("WS") });
		const float FootH = 110.f * Ks; // b142m: a footer the list never runs under
		const float Top = 64.f, Bot = 24.f + FootH, ViewH = Sz.Y - Top - Bot, TextW = (Sz.X - 100.f) * (bPortN ? 0.9f : 1.f); // b129: portrait lines ran past the card
		TArray<float> Hs; float Total = 10.f;
		for (int32 i = 0; i < N; ++i) { const float H = 56.f * Ks + ZWrapLines(News[i].Body, BodyF, TextW) * LineH + 30.f; Hs.Add(H); Total += H; }
		Self->NewsMax = FMath::Max(0.f, Total - ViewH);
		Self->NewsY = FMath::Clamp(Self->NewsY, 0.f, Self->NewsMax);
		Self->NewsBox = FBox2D(TL + FVector2D(0.f, Top), TL + FVector2D(Sz.X, Top + ViewH)); // b156: only the list scrolls (TOP / CLOSE / X are buttons)
		Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2D(Sz.X, ViewH), FSlateLayoutTransform(TL + FVector2D(0.f, Top)))));
		float Y = TL.Y + Top + 10.f - Self->NewsY;
		for (int32 i = 0; i < N; ++i)
		{
			const float H = Hs[i];
			if (Y + H > TL.Y + Top - 10.f && Y < TL.Y + Top + ViewH + 10.f)
			{
				const FVector2D R0(TL.X + 30.f, Y);
				const float TW = ZMeasure(News[i].Tag, Font(0, 26.f * Ks)).X + 30.f;
				ZRect(Out, G, L + 4, R0, FVector2D(TW, 40.f * Ks), i <= 1 ? Mag() : Ink);
				if (i == 1) { const FVector2D Nb(TL.X + Sz.X - 150.f * Ks, Y + 4.f); ZRect(Out, G, L + 4, Nb, FVector2D(84.f * Ks, 34.f * Ks), Mag()); ZTextC(Out, G, L + 5, TEXT("NEW"), Font(0, 24.f * Ks), Nb + FVector2D(42.f, 17.f) * Ks, Paper); } // b142m
				ZTextC(Out, G, L + 5, News[i].Tag, Font(0, 26.f * Ks), R0 + FVector2D(TW * 0.5f, 20.f * Ks), Paper);
				ZTextAt(Out, G, L + 5, News[i].Head, ZFitFont(News[i].Head, 0, 36.f * Ks, Sz.X - TW - 110.f), R0 + FVector2D(TW + 20.f, -2.f), Ink);
				ZWrap(Out, G, L + 5, News[i].Body, BodyF, R0 + FVector2D(0.f, 52.f * Ks), TextW, LineH, Ink);
				if (i < N - 1) ZRect(Out, G, L + 4, R0 + FVector2D(0.f, H - 16.f), FVector2D(Sz.X - 80.f, 3.f), Ink.CopyWithNewOpacity(0.2f));
			}
			Y += H;
		}
		Out.PopClip();
		if (Self->NewsMax > 0.f)
		{	// scrollbar (b142m: thicker)
			const float BarX = TL.X + Sz.X - 30.f, BarY = TL.Y + Top;
			ZRect(Out, G, L + 4, FVector2D(BarX, BarY), FVector2D(16.f, ViewH), Ink.CopyWithNewOpacity(0.12f));
			const float Th = FMath::Max(80.f, ViewH * ViewH / Total);
			ZRect(Out, G, L + 5, FVector2D(BarX, BarY + (ViewH - Th) * Self->NewsY / Self->NewsMax), FVector2D(16.f, Th), Mag());
		}
		{	// b142m footer: [TOP]  hint  [CLOSE]
			const float Fy = TL.Y + Sz.Y - FootH - 12.f;
			ZRect(Out, G, L + 4, FVector2D(TL.X + 20.f, Fy - 8.f), FVector2D(Sz.X - 40.f, 4.f), Ink);
			const FVector2D BS(FMath::Min(260.f * Ks, Sz.X * 0.3f), FootH - 18.f);
			const FVector2D TopP(TL.X + 30.f, Fy + 6.f), CloseP(TL.X + Sz.X - 30.f - BS.X, Fy + 6.f);
			const bool bAtTop = Self->NewsY < 4.f;
			ZRect(Out, G, L + 5, TopP + FVector2D(6.f, 6.f), BS, Mag().CopyWithNewOpacity(0.5f));
			ZRect(Out, G, L + 6, TopP, BS, bAtTop ? FLinearColor(0.55f, 0.55f, 0.58f) : Ink);
			ZTextC(Out, G, L + 7, TEXT("TOP"), Font(0, 40.f * Ks), TopP + BS * 0.5f, Paper);
			AddHit(TopP - FVector2D(10.f, 10.f), BS + FVector2D(20.f, 20.f), [Self]() { Self->bNewsToTop = true; Self->NewsVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });
			ZRect(Out, G, L + 5, CloseP + FVector2D(6.f, 6.f), BS, Ink);
			ZRect(Out, G, L + 6, CloseP, BS, Mag());
			ZTextC(Out, G, L + 7, TEXT("CLOSE"), Font(0, 40.f * Ks), CloseP + BS * 0.5f, Paper);
			AddHit(CloseP - FVector2D(10.f, 10.f), BS + FVector2D(20.f, 20.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
			// b188 (Evan: "add up and down arrows to news"): UP / DOWN arrow buttons between TOP and CLOSE - a tap glides the list ~a page
			const float HwMax = CloseP.X - (TopP.X + BS.X) - 30.f;
			const float Ab = FMath::Min(BS.Y, (HwMax - 30.f) * 0.5f), Mx = (TopP.X + BS.X + CloseP.X) * 0.5f;
			const float PageStep = FMath::Max(120.f, ViewH * 0.75f);
			for (int32 k = 0; k < 2; ++k)
			{
				const bool bUp = k == 0;
				const float Cur = Self->NewsGoal >= 0.f ? Self->NewsGoal : Self->NewsY;
				const bool bCan = Self->NewsMax > 0.f && (bUp ? Cur > 4.f : Cur < Self->NewsMax - 4.f);
				const FVector2D P0(bUp ? Mx - 15.f - Ab : Mx + 15.f, Fy + 6.f + (BS.Y - Ab) * 0.5f);
				ZRect(Out, G, L + 5, P0 + FVector2D(6.f, 6.f), FVector2D(Ab, Ab), Mag().CopyWithNewOpacity(0.5f));
				ZRect(Out, G, L + 6, P0, FVector2D(Ab, Ab), bCan ? Ink : FLinearColor(0.55f, 0.55f, 0.58f));
				ZArrow(Out, G, L + 7, P0 + FVector2D(Ab, Ab) * 0.5f, Ab * 0.5f, bUp, Paper);
				AddHit(P0 - FVector2D(10.f, 10.f), FVector2D(Ab, Ab) + FVector2D(20.f, 20.f), [Self, bUp, PageStep]()
				{
					const float Base = Self->NewsGoal >= 0.f ? Self->NewsGoal : Self->NewsY;
					const float Nw = FMath::Clamp(Base + (bUp ? -PageStep : PageStep), 0.f, Self->NewsMax);
					if (FMath::Abs(Nw - Base) < 1.f) { Self->Sfx(TEXT("UiCancel"), 0.5f); return; }
					Self->NewsGoal = Nw; Self->NewsVel = 0.f; Self->bNewsToTop = false; Self->Sfx(TEXT("UiClick"), 0.7f, bUp ? 1.25f : 1.05f);
				});
			}
		}
	}

	void PaintConfirm(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b156 (Evan's reference): the BUY pop-up - item tile, price, CANCEL / BUY (short of it -> GET GEMS / GET COINS)
		UZoogiFrontProfileSubsystem* P = FP();
		const int32 Have = P ? (CfmKind == 1 ? P->Gems() : P->Data().Gold) : 0;
		const bool bShort = Have < CfmPrice;
		const float K = FMath::Clamp((T - PopT) / 0.16f, 0.f, 1.f), Pk = 0.86f + 0.14f * K;
		const FVector2D Sz = FVector2D(840.f, 480.f) * Pk;
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f);
		ZRect(Out, G, L, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 1, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 2, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRect(Out, G, L + 3, TL, FVector2D(Sz.X, 94.f * Pk), Mag());
		ZTextC(Out, G, L + 4, CfmTitle, Font(0, 56.f * Pk), TL + FVector2D(Sz.X * 0.5f, 47.f * Pk), Paper);
		const FVector2D It = TL + FVector2D(40.f, 126.f) * Pk, Is = FVector2D(220.f, 220.f) * Pk;
		ZPanel(Out, G, L + 3, It, Is, CfmCol, Mag(), 4.f, 8.f);
		if (!CfmChest.IsNone()) ZChest(Out, G, L + 6, CfmChest, It + Is * 0.5f, Is.X * 0.6f);
		else if (UTexture2D* Ic = CfmIcon.Get()) ZTexFit(Out, G, L + 6, Ic, It + FVector2D(14.f, 14.f), Is - FVector2D(28.f, 28.f));
		const float Tx = It.X + Is.X + 40.f * Pk, Tw = TL.X + Sz.X - 30.f - Tx;
		ZTextAt(Out, G, L + 4, CfmName, ZFitFont(CfmName, 0, 50.f * Pk, Tw), FVector2D(Tx, It.Y + 4.f), Paper);
		if (UTexture2D* Cur = CfmKind == 1 ? ZGemIcon() : ZCoinIcon()) ZTex(Out, G, L + 4, Cur, FVector2D(Tx, It.Y + 80.f * Pk), FVector2D(66.f, 66.f) * Pk);
		ZTextAt(Out, G, L + 4, FString::Printf(TEXT("%s %s"), *ZNumS(CfmPrice), CfmKind == 1 ? TEXT("GEMS") : TEXT("COINS")), Font(0, 46.f * Pk), FVector2D(Tx + 80.f * Pk, It.Y + 86.f * Pk), Gold);
		const FString Hv = bShort ? FString::Printf(TEXT("YOU HAVE %s - NEED %s MORE"), *ZNumS(Have), *ZNumS(CfmPrice - Have)) : FString::Printf(TEXT("YOU HAVE %s"), *ZNumS(Have));
		ZTextAt(Out, G, L + 4, Hv, ZFitFont(Hv, 1, 26.f * Pk, Tw), FVector2D(Tx, It.Y + 170.f * Pk), bShort ? Red : FLinearColor(0.82f, 0.82f, 0.86f));
		const FVector2D Bs = FVector2D(300.f, 96.f) * Pk;
		const FVector2D Cp(TL.X + Sz.X * 0.5f - Bs.X - 20.f, TL.Y + Sz.Y - Bs.Y - 30.f * Pk), Bp(TL.X + Sz.X * 0.5f + 20.f, Cp.Y);
		ZPanel(Out, G, L + 3, Cp, Bs, Paper, Ink, 4.f, 8.f);
		ZTextC(Out, G, L + 6, TEXT("CANCEL"), Font(0, 44.f * Pk), Cp + Bs * 0.5f, Ink);
		AddHit(Cp, Bs, [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; Self->CfmFn = nullptr; });
		ZPanel(Out, G, L + 3, Bp, Bs, bShort ? Mag() : Green, Ink, 4.f, 8.f);
		const FString BuyTxt = bShort ? FString(CfmKind == 1 ? TEXT("GET GEMS") : TEXT("GET COINS")) : FString(TEXT("BUY"));
		ZTextC(Out, G, L + 6, BuyTxt, Font(0, 46.f * Pk), Bp + Bs * 0.5f, Paper);
		const bool bGems = CfmKind == 1;
		AddHit(Bp, Bs, [Self, bShort, bGems]()
		{
			Self->Pop = SZoogiFront::EPop::None;
			if (bShort) { Self->CfmFn = nullptr; Self->Go(2); Self->ShopX = FMath::Min(Self->ShopMax, Self->SectionX.FindRef(bGems ? 4 : 5)); Self->ShopXVel = 0.f; Self->ShopCat = bGems ? 4 : 5; return; }
			TFunction<void()> Fn = MoveTemp(Self->CfmFn); Self->CfmFn = nullptr;
			if (Fn) Fn();
			Self->Sfx(TEXT("UiConfirm"), 1.f, 1.15f);
			if (UObject* C = Self->Ctx.Get()) ZoogiHaptics::Kick(0.55f, C->GetWorld(), TEXT("buy"));
		});
	}

	void PaintTrick(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b157: the DAILY TRICK SHOT pop-up (start-up, once a day)
		const float K = FMath::Clamp((T - PopT) / 0.22f, 0.f, 1.f), Pk = 0.88f + 0.12f * K;
		const FVector2D Sz = FVector2D(FMath::Min(1180.f, DW - 80.f), 640.f) * Pk;
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f + 30.f);
		ZRect(Out, G, L, TL + FVector2D(20.f, 20.f), Sz, Mag());
		ZRect(Out, G, L + 1, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 2, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRansom(Out, G, L + 3, { TEXT("DAILY"), TEXT("TRICK"), TEXT("SHOT!") }, TL + FVector2D(30.f, -58.f), 70.f * Pk);
		const EZoogiArena Ar = ZoogiTrickShot::TodayArena();
		const FString ArName = ZoogiUtil::ArenaName(Ar).ToUpper();
		const TCHAR* Key = Ar == EZoogiArena::Ancient ? TEXT("ancient") : Ar == EZoogiArena::Volcanic ? TEXT("volcanic") : Ar == EZoogiArena::Arabian ? TEXT("arabian") : Ar == EZoogiArena::Tomb ? TEXT("tomb") : Ar == EZoogiArena::Neon ? TEXT("neon") : TEXT("meadow");
		const FVector2D At = TL + FVector2D(36.f, 70.f) * Pk, As = FVector2D(Sz.X * 0.46f, Sz.Y - 210.f * Pk);
		ZRect(Out, G, L + 3, At - FVector2D(6.f, 6.f), As + FVector2D(12.f, 12.f), Paper);
		if (UTexture2D* Full = ZLoadPng(FString(TEXT("Data/MenuArt/ModeSelect/ms_map_")) + Key + TEXT("_p")))
		{	// b171 (Evan: "fill the white with the image"): the arena shot fills the whole box (cover crop), no white around a framed card
			FSlateBrush B; B.SetResourceObject(Full); B.ImageSize = As;
			const float Ta = (float)Full->GetSizeX() / FMath::Max(1, Full->GetSizeY()), Ba = As.X / FMath::Max(1.f, As.Y);
			if (Ta > Ba) { const float Wu = Ba / Ta; B.SetUVRegion(FBox2f(FVector2f(0.5f - Wu * 0.5f, 0.f), FVector2f(0.5f + Wu * 0.5f, 1.f))); }
			else { const float Hu = Ta / Ba; B.SetUVRegion(FBox2f(FVector2f(0.f, 0.5f - Hu * 0.5f), FVector2f(1.f, 0.5f + Hu * 0.5f))); }
			ZRect(Out, G, L + 4, At, As, FLinearColor::White, &B);
		}
		else if (UTexture2D* Img = ZLoadPng(FString(TEXT("Data/MenuArt/ModeSelect/ms_map_")) + Key)) ZTexFit(Out, G, L + 4, Img, At, As);
		ZRect(Out, G, L + 5, At + FVector2D(0.f, As.Y - 50.f * Pk), FVector2D(As.X, 50.f * Pk), Ink.CopyWithNewOpacity(0.85f));
		ZTextC(Out, G, L + 6, FString::Printf(TEXT("#%d  -  %s"), ZoogiTrickShot::TodayNumber(), *ArName), ZFitFont(ArName + TEXT("  #00"), 0, 30.f * Pk, As.X - 20.f), At + FVector2D(As.X * 0.5f, As.Y - 25.f * Pk), Paper);
		const float Rx = At.X + As.X + 40.f * Pk, Rw = TL.X + Sz.X - 30.f - Rx;
		float Y = At.Y;
		ZTextAt(Out, G, L + 4, TEXT("ONE SHOT  -  3 TRIES  -  YOUR BEST COUNTS"), ZFitFont(TEXT("ONE SHOT  -  3 TRIES  -  YOUR BEST COUNTS"), 0, 30.f * Pk, Rw), FVector2D(Rx, Y), Gold); Y += 58.f * Pk;
		const TCHAR* Goals[] = { TEXT("1 STAR    KNOCK 1 RIVAL OFF"), TEXT("2 STARS   2 RIVALS IN ONE SHOT"), TEXT("3 STARS   ALL 3 IN ONE SHOT") };
		for (int32 g = 0; g < 3; ++g)
		{
			ZRect(Out, G, L + 3, FVector2D(Rx, Y), FVector2D(Rw, 54.f * Pk), FLinearColor(0.12f, 0.1f, 0.14f));
			ZRect(Out, G, L + 4, FVector2D(Rx, Y), FVector2D(8.f, 54.f * Pk), Mag());
			ZTextAt(Out, G, L + 5, Goals[g], ZFitFont(Goals[g], 0, 28.f * Pk, Rw - 30.f), FVector2D(Rx + 22.f, Y + 10.f * Pk), Paper);
			Y += 64.f * Pk;
		}
		Y += 6.f * Pk;
		if (UTexture2D* Ci = ZCoinIcon()) ZTex(Out, G, L + 4, Ci, FVector2D(Rx, Y), FVector2D(46.f, 46.f) * Pk);
		ZTextAt(Out, G, L + 4, TEXT("+20 PER NEW STAR"), ZFitFont(TEXT("+20 PER NEW STAR"), 0, 28.f * Pk, Rw * 0.55f - 64.f * Pk), FVector2D(Rx + 54.f * Pk, Y + 6.f * Pk), Paper); // b266: fits its half
		if (UTexture2D* Gi = ZGemIcon()) ZTex(Out, G, L + 4, Gi, FVector2D(Rx + Rw * 0.55f, Y), FVector2D(46.f, 46.f) * Pk);
		ZTextAt(Out, G, L + 4, TEXT("+5 FOR 3 STARS"), ZFitFont(TEXT("+5 FOR 3 STARS"), 0, 28.f * Pk, Rw * 0.45f - 58.f * Pk), FVector2D(Rx + Rw * 0.55f + 54.f * Pk, Y + 6.f * Pk), Paper);
		Y += 62.f * Pk;
		const int32 Best = ZoogiTrickShot::BestToday(), Streak = ZoogiTrickShot::Streak();
		const FString Info = FString::Printf(TEXT("STREAK %d DAY%s   -   TODAY'S BEST %d STAR%s"), Streak, Streak == 1 ? TEXT("") : TEXT("S"), Best, Best == 1 ? TEXT("") : TEXT("S"));
		ZTextAt(Out, G, L + 4, Info, ZFitFont(Info, 0, 28.f * Pk, Rw), FVector2D(Rx, Y), Mag());
		const FVector2D Bs = FVector2D(320.f, 100.f) * Pk;
		const FVector2D Lp(TL.X + Sz.X - 2.f * Bs.X - 70.f, TL.Y + Sz.Y - Bs.Y - 30.f * Pk), Pp(TL.X + Sz.X - Bs.X - 36.f, Lp.Y);
		ZPanel(Out, G, L + 3, Lp, Bs, Paper, Ink, 4.f, 8.f);
		ZTextC(Out, G, L + 6, TEXT("LATER"), Font(0, 46.f * Pk), Lp + Bs * 0.5f, Ink);
		AddHit(Lp, Bs, [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
		ZPanel(Out, G, L + 3, Pp, Bs, Mag(), Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 6, Best >= 3 ? TEXT("AGAIN!") : TEXT("PLAY!"), Font(0, 52.f * Pk), Pp + Bs * 0.5f, Paper);
		AddHit(Pp, Bs, [Self]()
		{
			Self->Pop = SZoogiFront::EPop::None;
			Self->Sfx(TEXT("UiConfirm"), 0.9f);
			TWeakObjectPtr<UZoogiMenuRoot> M = Self->Menu;
			UWorld* W = Self->Ctx.IsValid() ? Self->Ctx->GetWorld() : nullptr;
			ZoogiP5::PlayWipe(W, [M]() { if (M.IsValid()) M->StartTrickShot(); }, Mag(), TEXT("TRICK SHOT!"));
		});
	}

	void PaintLevelUp(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b156: LEVEL UP! - the XP bar and what this level opened in the shop
		const UZoogiFrontProfileSubsystem* P = FP();
		const float K = FMath::Clamp((T - PopT) / 0.25f, 0.f, 1.f);
		const FVector2D Sz(900.f, 580.f);
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f + 30.f);
		ZTex(Out, G, L, ZBurstTex(), FVector2D(DW * 0.5f - 560.f, DH * 0.5f - 530.f), FVector2D(1120.f, 1120.f), Mag().CopyWithNewOpacity(0.35f * K));
		ZRect(Out, G, L + 1, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 2, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 3, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRansom(Out, G, L + 4, { TEXT("LEVEL"), FString::FromInt(LvShown), TEXT("!") }, TL + FVector2D(Sz.X * 0.5f - 240.f, -64.f), 76.f * (0.75f + 0.25f * K));
		const int32 Xp = P ? P->Data().XP : 0, Need = FMath::Max(1, UZoogiFrontProfileSubsystem::XPForLevel(LvShown));
		const FVector2D Bt = TL + FVector2D(60.f, 74.f), Bz(Sz.X - 120.f, 36.f);
		ZRect(Out, G, L + 4, Bt - FVector2D(4.f, 4.f), Bz + FVector2D(8.f, 8.f), Paper);
		ZRect(Out, G, L + 5, Bt, Bz, Ink);
		ZRect(Out, G, L + 6, Bt, FVector2D(Bz.X * FMath::Clamp((float)Xp / Need, 0.f, 1.f), Bz.Y), Mag());
		ZTextC(Out, G, L + 7, FString::Printf(TEXT("%s / %s XP TO LEVEL %d"), *ZNumS(Xp), *ZNumS(Need), LvShown + 1), Font(0, 24.f), Bt + Bz * 0.5f, Paper);
		struct FUn { UTexture2D* Ic; FName Chest; FString Name; };
		TArray<FUn> Un;
		int32 n = 0;
		for (const ZoogiFront::FHero& Hh : ZoogiFront::Heroes()) { if (FString(Hh.Ball) == TEXT("Hotstreak")) continue; if (ZSkinLockLv(n) == LvShown) Un.Add({ ZBallIcon(Hh.Ball), NAME_None, FString(Hh.Ball).ToUpper() + TEXT(" BALL") }); ++n; }
		if (HotstreakLockLv == LvShown) Un.Add({ ZBallIcon(TEXT("Hotstreak")), NAME_None, TEXT("HOTSTREAK") });
		n = 0; for (const ZoogiWolf::FSkin& Sk : ZoogiWolf::Skins()) { if (Sk.Price > 0 && ZPackLockLv(n, Sk.Price) == LvShown) Un.Add({ nullptr, NAME_None, FString(Sk.Name).ToUpper() }); ++n; }
		if (ZChestLockLv(FName(TEXT("royal"))) == LvShown) Un.Add({ nullptr, FName(TEXT("royal")), TEXT("RARE CHEST") });
		if (ZChestLockLv(FName(TEXT("epic"))) == LvShown) Un.Add({ nullptr, FName(TEXT("epic")), TEXT("EPIC CHEST") });
		ZTextC(Out, G, L + 4, Un.Num() ? FString(TEXT("NEW IN THE SHOP")) : FString(TEXT("KEEP ROLLING - MORE UNLOCKS NEXT LEVEL")), Font(0, 34.f), TL + FVector2D(Sz.X * 0.5f, 152.f), Gold);
		const int32 Nu = FMath::Min(Un.Num(), 3);
		const float Cw = 200.f; float X = TL.X + Sz.X * 0.5f - (Nu * (Cw + 24.f) - 24.f) * 0.5f;
		for (int32 i = 0; i < Nu; ++i)
		{
			const FVector2D Ct(X, TL.Y + 196.f);
			ZPanel(Out, G, L + 4, Ct, FVector2D(Cw, Cw), ZTile(i + 2), Mag(), 4.f, 8.f);
			if (!Un[i].Chest.IsNone()) ZChest(Out, G, L + 8, Un[i].Chest, Ct + FVector2D(Cw, Cw - 30.f) * 0.5f, Cw * 0.55f);
			else if (Un[i].Ic) ZTexFit(Out, G, L + 8, Un[i].Ic, Ct + FVector2D(16.f, 10.f), FVector2D(Cw - 32.f, Cw - 54.f));
			ZTextC(Out, G, L + 9, Un[i].Name, ZFitFont(Un[i].Name, 0, 24.f, Cw - 12.f), Ct + FVector2D(Cw * 0.5f, Cw - 22.f), Paper);
			X += Cw + 24.f;
		}
		const FVector2D Bs(300.f, 92.f);
		const FVector2D Sp(TL.X + Sz.X * 0.5f - Bs.X - 20.f, TL.Y + Sz.Y - Bs.Y - 26.f), Op(TL.X + Sz.X * 0.5f + 20.f, Sp.Y);
		ZPanel(Out, G, L + 4, Sp, Bs, Mag(), Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 8, TEXT("SHOP"), Font(0, 46.f), Sp + Bs * 0.5f, Paper);
		AddHit(Sp, Bs, [Self]() { Self->Pop = SZoogiFront::EPop::None; Self->Go(2); Self->ShopX = 0.f; Self->ShopCat = -1; });
		ZPanel(Out, G, L + 4, Op, Bs, Green, Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 8, TEXT("NICE!"), Font(0, 46.f), Op + Bs * 0.5f, Paper);
		AddHit(Op, Bs, [Self]() { Self->Sfx(TEXT("UiConfirm")); Self->Pop = SZoogiFront::EPop::None; });
	}

	void PaintSettings(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const FVector2D Sz(860.f, 914.f); // b93: + WATCH CPU PLAY row; b96c: + TRAINING BOTS row; b177r: + ALL SKILLS (DEV)
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("SET"), TEXT("TINGS") });
		SettingsRows(Out, G, L + 2, TL.X, TL.Y + 40.f, Sz.X, [this](const FVector2D& A, const FVector2D& B, TFunction<void()> Fn) { AddHit(A, B, Fn); }, [Self](float Pp) { Self->Sfx(TEXT("UiClick"), 0.7f, Pp); });
	}

	void PaintLogin(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const UZoogiFrontProfileSubsystem* P = FP();
		const int32 Today = P ? P->LoginDayIndex() : 0;
		const int32 Streak = P ? P->Data().LoginStreak : 0;
		const float TW = ZRansom(Out, G, L, { TEXT("DAILY"), TEXT("LOGIN"), TEXT("!") }, FVector2D(DW * 0.5f - 230.f, 70.f), 64.f);
		(void)TW;
		ZTag(Out, G, L, FString::Printf(TEXT("DAY %d OF 7  -  STREAK %d"), Today + 1, Streak), FVector2D(DW * 0.5f - 210.f, 180.f), 54.f, false, Mag(), 30.f);
		const TArray<FZoogiFrontReward>& Tbl = UZoogiFrontProfileSubsystem::LoginTable();
		const float W = FMath::Min(200.f, (DW - 160.f - 6 * 20.f) / 7.f), X0 = DW * 0.5f - (7 * W + 6 * 20.f) * 0.5f, Y0 = 280.f, H = W * 1.35f;
		for (int32 d = 0; d < 7; ++d)
		{
			const FVector2D TL(X0 + d * (W + 20.f), Y0);
			const bool bDone = d < Today, bNow = d == Today;
			const FLinearColor Body = d == 6 ? Gold : Paper;
			ZPanel(Out, G, L + 2, TL, FVector2D(W, H), bDone ? FLinearColor(0.75f, 0.75f, 0.75f) : Body, bNow ? Mag() : Ink, bNow ? 7.f : 4.f, bNow ? 12.f : 7.f);
			ZRect(Out, G, L + 5, TL, FVector2D(84.f, 34.f), Mag());
			ZTextC(Out, G, L + 6, FString::Printf(TEXT("DAY %d"), d + 1), Font(0, 22.f), TL + FVector2D(42.f, 17.f), Paper);
			const FZoogiFrontReward& R = Tbl[d];
			if (R.Kind == FName(TEXT("chest"))) ZChest(Out, G, L + 6, R.Id, TL + FVector2D(W * 0.5f, H * 0.46f), W * 0.5f);
			else ZTextC(Out, G, L + 6, FString::Printf(TEXT("%s"), *ZNumS(R.Amount)), ZFitFont(ZNumS(R.Amount), 0, W * 0.32f, W - 20.f), TL + FVector2D(W * 0.5f, H * 0.44f), Ink);
			const FString Sub = bDone ? TEXT("CLAIMED") : bNow ? TEXT("TODAY!") : R.Kind == FName(TEXT("chest")) ? (R.Id == FName(TEXT("epic")) ? TEXT("EPIC CHEST") : R.Id == FName(TEXT("royal")) ? TEXT("ROYAL CHEST") : TEXT("CHEST")) : TEXT("GOLD");
			ZRect(Out, G, L + 6, TL + FVector2D(0.f, H - 46.f), FVector2D(W, 46.f), bDone ? Green : bNow ? Mag() : Ink);
			ZTextC(Out, G, L + 7, Sub, ZFitFont(Sub, 0, 26.f, W - 10.f), TL + FVector2D(W * 0.5f, H - 23.f), Paper);
		}
		const float Pulse = 1.f + 0.03f * FMath::Sin(T * 6.f);
		const FZoogiFrontReward& R = Tbl[FMath::Clamp(Today, 0, 6)];
		const FString Claim = R.Kind == FName(TEXT("chest")) ? FString(TEXT("CLAIM CHEST!")) : FString::Printf(TEXT("CLAIM  %s GOLD"), *ZNumS(R.Amount));
		const FVector2D BS(460.f * Pulse, 100.f * Pulse);
		Slab(Out, G, L + 9, Claim, FVector2D(DW * 0.5f - BS.X * 0.5f, Y0 + H + 60.f), BS, Mag(), Paper, 50.f, [Self]() { Self->ClaimLogin(); });
		ZTextC(Out, G, L + 9, TEXT("COME BACK EVERY DAY - MISS A DAY AND THE STREAK STARTS OVER"), Font(1, 22.f), FVector2D(DW * 0.5f, Y0 + H + 200.f), Paper);
	}

	// b166 (Evan Nov 6: "I want the chest to open" + "All chest pop up types"): every chest pop-up opens with its own show -
	//  COMMON = CLASSIC BURST (shake, lid pops, gold beam, rewards fly into full-colour tiles), RARE = CARD FAN (hops, lid lifts,
	//  purple glow, cards rise and fan like a hand), EPIC = P5 SLASH REVEAL (red slash, lid knocked off, EPIC CHEST! slam, torn
	//  white panels smash in). A tap during the show skips to the end. -ZoogiChestStyle=0/1/2 forces one, -ZoogiOldChest = b165.
	int32 ChestStyle() const
	{
		static int32 Force = -2;
		if (Force == -2) { Force = -1; FParse::Value(FCommandLine::Get(), TEXT("ZoogiChestStyle="), Force); }
		if (Force >= 0 && Force <= 2) return Force;
		if (RewardChest == FName(TEXT("epic"))) return 2;
		if (RewardChest == FName(TEXT("royal"))) return 1;
		return 0;
	}

	void PaintChest(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChest"));
		if (bOld) { Self->ChestSkipAt = 0.6f; PaintChestOld(Out, G, L, Self); return; }
		const float K = T - PopT;
		const int32 St = ChestStyle();
		const FName Ty = RewardChest.IsNone() ? FName(TEXT("wooden")) : RewardChest;
		const TCHAR* Key = Ty == FName(TEXT("epic")) ? TEXT("epic") : Ty == FName(TEXT("royal")) ? TEXT("rare") : TEXT("common");
		const float Seam = Ty == FName(TEXT("epic")) ? 122.f / 256.f : Ty == FName(TEXT("royal")) ? 83.f / 256.f : 76.f / 256.f;
		UTexture2D* WholeTx = ZLoadPng(FString(TEXT("Data/MenuArt/Front/icon_chest_")) + Key);
		UTexture2D* LidTx = ZLoadPng(FString(TEXT("Data/MenuArt/Front/icon_chest_")) + Key + TEXT("_lid"));
		UTexture2D* BaseTx = ZLoadPng(FString(TEXT("Data/MenuArt/Front/icon_chest_")) + Key + TEXT("_base"));
		const int32 N = Rewards.Num();
		const float U = DH > DW ? 1.6f : 1.f; // b166: portrait (DH ~2900) draws the show bigger
		auto Cl = [](float V) { return FMath::Clamp(V, 0.f, 1.f); };
		auto Ease = [](float X) { return 1.f - FMath::Pow(1.f - X, 3.f); };
		auto Back = [](float X) { const float C1 = 1.70158f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f); };
		auto Poly = [&](int32 Lv, const TArray<FVector2D>& P, const FLinearColor& Col)
		{	// a filled convex polygon
			const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*ZWhite());
			const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
			const FColor Cc = Col.ToFColor(true);
			TArray<FSlateVertex> V; TArray<SlateIndex> I;
			for (const FVector2D& Pp : P) V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(Pp), FVector2f(0.5f, 0.5f), Cc));
			for (int32 i = 1; i + 1 < P.Num(); ++i) { I.Add(0); I.Add(i); I.Add(i + 1); }
			FSlateDrawElement::MakeCustomVerts(Out, Lv, RH, V, I, nullptr, 0, 0);
		};
		auto Rot = [&](const FVector2D& TL, const FVector2D& Sz, float Deg) { return G.MakeChild(Sz, FSlateLayoutTransform(TL), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Deg))), FVector2D(0.5f, 0.5f)); };
		auto RTex = [&](int32 Lv, UTexture2D* Tx, const FVector2D& Ctr, const FVector2D& Sz, float Deg, const FLinearColor& Col)
		{
			if (!Tx || Sz.X < 1.f || Sz.Y < 1.f) return;
			ZTex(Out, Rot(Ctr - Sz * 0.5f, Sz, Deg), Lv, Tx, FVector2D::ZeroVector, Sz, Col);
		};
		auto RText = [&](int32 Lv, const FString& CS, const FSlateFontInfo& F, const FVector2D& Ctr, float Deg, const FLinearColor& Col)
		{
			const FVector2D Sz = ZMeasure(CS, F);
			FSlateDrawElement::MakeText(Out, Lv, Rot(Ctr - Sz * 0.5f, Sz, Deg).ToPaintGeometry(), CS, F, ESlateDrawEffect::None, Col);
		};
		// the chest: closed = the whole icon; open = base + the lid (squashed toward the seam, lifted, shifted, turned)
		auto DrawChest = [&](int32 Lv, const FVector2D& Ctr, float CS, float Deg, float Open, float LidUp, float LidDx, float LidDeg, float Squash, float A, float LidA)
		{
			if (CS < 2.f) return;
			if (!LidTx || !BaseTx || Open <= 0.f)
			{
				if (WholeTx) RTex(Lv, WholeTx, Ctr, FVector2D(CS, CS), Deg, FLinearColor(1.f, 1.f, 1.f, A)); else ZChest(Out, G, Lv, Ty, Ctr, CS / 1.25f);
				return;
			}
			RTex(Lv, BaseTx, Ctr, FVector2D(CS, CS), Deg, FLinearColor(1.f, 1.f, 1.f, A));
			const float H = CS * (1.f - Squash), SeamY = Ctr.Y - CS * 0.5f + CS * Seam;
			if (LidA > 0.f) RTex(Lv + 1, LidTx, FVector2D(Ctr.X + LidDx, SeamY - Seam * H + H * 0.5f - LidUp), FVector2D(CS, H), Deg + LidDeg, FLinearColor(1.f, 1.f, 1.f, LidA));
		};
		// one reward tile drawn at (0,0) of TG (so it can be turned / scaled)
		auto DrawTile = [&](const FGeometry& TG, int32 Lv, const FVector2D& Sz, const FZoogiFrontReward& R, bool bP5)
		{	// b265 (Evan: "restyle Chest cards"): ONE card style for every reward - white rim, ink body, the art on top, the full name
			// in a coloured band at the bottom (shrinks to fit, never cut off), the count in a badge at the top-right corner (never over the name)
			const FVector2D O = FVector2D::ZeroVector;
			const float Rim = FMath::Clamp(Sz.X * 0.035f, 4.f, 10.f);
			ZRect(Out, TG, Lv, O - FVector2D(Rim, Rim), Sz + FVector2D(Rim, Rim) * 2.f, Paper);
			ZRect(Out, TG, Lv + 1, O, Sz, Ink);
			const bool bGold = R.Kind == FName(TEXT("gold")), bChestR = R.Kind == FName(TEXT("chest"));
			const FString Ids = R.Id.ToString();
			FString Name;
			if (bGold) Name = FString::Printf(TEXT("%s GOLD"), *ZNumS(R.Amount));
			else if (bChestR) Name = (Ids.IsEmpty() ? FString(TEXT("WOODEN")) : Ids.ToUpper()) + TEXT(" CHEST");
			else if (const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(R.Id)) Name = Cd->Name.ToUpper();
			else if (R.Id == FName(TEXT("apple"))) Name = TEXT("GOLDEN APPLE");
			else Name = Ids.Replace(TEXT("_"), TEXT(" ")).ToUpper();
			if (Name.IsEmpty()) Name = R.Kind.ToString().ToUpper();
			const float Band = FMath::Max(30.f, Sz.Y * 0.2f), Pad = FMath::Max(6.f, Sz.X * 0.04f);
			const FVector2D ArtTL(Pad, Pad), ArtSz(Sz.X - Pad * 2.f, Sz.Y - Band - Pad * 1.5f);
			ZRect(Out, TG, Lv + 2, ArtTL, ArtSz, FLinearColor(0.01f, 0.01f, 0.02f, 1.f)); // art well
			if (bGold)
			{
				if (UTexture2D* Ic = ZCoinIcon()) { const float IcS = FMath::Min(ArtSz.X, ArtSz.Y) * 0.72f; ZTexFit(Out, TG, Lv + 3, Ic, ArtTL + (ArtSz - FVector2D(IcS, IcS)) * 0.5f, FVector2D(IcS, IcS)); }
			}
			else if (bChestR) ZChest(Out, TG, Lv + 3, R.Id, ArtTL + ArtSz * 0.5f, FMath::Min(ArtSz.X, ArtSz.Y) * 0.6f);
			else
			{	// art only (the Icons set has no printed name), else the full card art, else the weapon picture
				const FString Key = R.Id == FName(TEXT("apple")) ? FString(TEXT("golden_apple")) : Ids;
				UTexture2D* A = ZLoadPng(TEXT("Data/CardArt/Icons/") + Key);
				if (!A) A = ZLoadPng(TEXT("Data/CardArt/IconsNew/") + Key);
				if (!A) A = ZLoadPng(TEXT("Data/CardArt/") + Key);
				if (!A) A = ZLoadPng(TEXT("Data/MenuArt/Weapons/w_") + Key);
				if (A) ZTexFit(Out, TG, Lv + 3, A, ArtTL + FVector2D(2.f, 2.f), ArtSz - FVector2D(4.f, 4.f));
				else ZTextC(Out, TG, Lv + 3, TEXT("?"), Font(0, ArtSz.Y * 0.4f), ArtTL + ArtSz * 0.5f, Paper);
			}
			// name band
			const FVector2D BandTL(0.f, Sz.Y - Band);
			ZRect(Out, TG, Lv + 4, BandTL, FVector2D(Sz.X, Band), bP5 ? Red : Mag());
			ZTextC(Out, TG, Lv + 5, Name, ZFitFont(Name, 0, Band * 0.52f, Sz.X - 12.f), BandTL + FVector2D(Sz.X * 0.5f, Band * 0.5f), Paper);
			// count badge, top-right
			if (!bGold && !bChestR)
			{
				const FString Badge = FString::Printf(TEXT("x%d"), FMath::Max(1, R.Amount));
				const FSlateFontInfo F = Font(0, FMath::Max(16.f, Sz.Y * 0.09f));
				const FVector2D Bs = ZMeasure(Badge, F) + FVector2D(14.f, 4.f);
				const FVector2D BTL(Sz.X - Bs.X + Rim * 0.5f, -Rim * 0.5f);
				ZRect(Out, TG, Lv + 5, BTL - FVector2D(3.f, 3.f), Bs + FVector2D(6.f, 6.f), Paper);
				ZRect(Out, TG, Lv + 5, BTL, Bs, Ink);
				ZTextC(Out, TG, Lv + 6, Badge, F, BTL + Bs * 0.5f, Paper);
			}
		};
		struct FEv { float At; const TCHAR* Name; float Vol, Pitch; };
		TArray<FEv> Ev;
		float End = 1.f;

		// rotating rays behind everything (gold/magenta, purple for rare, red/black for epic)
		const FVector2D C(DW * 0.5f, DH * 0.40f); // b266: every chest sits in the same spot (the rare one sat on its cards)
		ZRect(Out, G, L, FVector2D::ZeroVector, FVector2D(DW, DH), FLinearColor(0.02f, 0.02f, 0.05f, 0.9f * Cl(K / 0.15f))); // b266: the home screen no longer shows through the chest show
		{
			const FLinearColor RA = St == 2 ? Red : Gold, RB = St == 2 ? Ink : St == 1 ? FLinearColor::FromSRGBColor(FColor(124, 58, 210)) : Mag();
			for (int32 k = 0; k < 16; k += 2)
			{
				const float A0 = FMath::DegreesToRadians(K * 20.f + 360.f * k / 16.f), A1 = FMath::DegreesToRadians(K * 20.f + 360.f * (k + 1) / 16.f);
				Poly(L, { C, C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * DW, C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * DW }, ((k / 2) % 2 ? RA : RB).CopyWithNewOpacity(0.85f));
			}
		}
		if (St != 2)
		{
			TArray<FString> Parts; RewardTitle.ParseIntoArray(Parts, TEXT(" "));
			if (!Parts.Num()) Parts.Add(TEXT("CHEST"));
			float Tw = 0.f; for (const FString& Pp : Parts) Tw += ZMeasure(Pp, Font(0, 62.f)).X + 30.f;
			ZRansom(Out, G, L + 1, Parts, FVector2D(DW * 0.5f - Tw * 0.5f, 40.f), 62.f);
		}

		if (St == 0)
		{	// ---------------- CLASSIC BURST
			const float Pop01 = Cl(K / 0.25f), OpenT = 0.95f, o = Cl((K - OpenT) / 0.16f);
			ZTex(Out, G, L + 2, ZBurstTex(), C - FVector2D(190.f, 190.f) * U * Pop01, FVector2D(380.f, 380.f) * U * Pop01, Paper);
			if (K > OpenT)
			{
				const float a = Cl((K - OpenT) / 0.2f) * (0.45f + 0.1f * FMath::Sin(K * 6.f));
				Poly(L + 2, { C + FVector2D(-47.f, -13.f) * U, C + FVector2D(47.f, -13.f) * U, FVector2D(C.X + 160.f * U, 0.f), FVector2D(C.X - 160.f * U, 0.f) }, FLinearColor(1.f, 0.93f, 0.6f, a));
			}
			const float Sh = (K > 0.3f && K < OpenT) ? (K - 0.3f) / (OpenT - 0.3f) : 0.f;
			const float Deg = Sh > 0.f ? FMath::Sin(K * 40.f) * (3.f + 6.f * Sh) : 0.f;
			const float CS = (290.f * U) * Pop01 * (1.f + 0.07f * Sh);
			if (Pop01 > 0.3f) DrawChest(L + 3, C, CS, Deg, o, CS * 0.26f * Back(o) + (o >= 1.f ? 4.f * FMath::Sin(K * 3.f) : 0.f), 0.f, -8.f * o, 0.3f * o, 1.f, 1.f);
			if (K > OpenT && K < OpenT + 0.35f) ZRect(Out, G, L + 6, FVector2D::ZeroVector, FVector2D(DW, DH), FLinearColor(1.f, 0.97f, 0.8f, 0.8f * (1.f - (K - OpenT) / 0.35f)));
			const float W = FMath::Min(220.f * U, (DW - 160.f - (N - 1) * 22.f) / FMath::Max(1, N)), H = W * 1.42f;
			const float X0 = DW * 0.5f - (N * W + (N - 1) * 22.f) * 0.5f, Y0 = DH * 0.56f;
			for (int32 i = 0; i < N; ++i)
			{
				const float Ti = 1.1f + 0.2f * i, f = Cl((K - Ti) / 0.24f);
				Ev.Add(FEv{ Ti + 0.2f, TEXT("Score"), 0.7f, 1.f + 0.06f * i });
				if (f <= 0.f) continue;
				const float e = Ease(f);
				const FVector2D Dst(X0 + i * (W + 22.f) + W * 0.5f, Y0 + H * 0.5f);
				const FVector2D Ctr = FMath::Lerp(C, Dst, e) - FVector2D(0.f, 120.f * FMath::Sin(UE_PI * f));
				const float Sc = FMath::Lerp(0.2f, 1.f, e) * (f < 1.f ? 1.f + 0.12f * FMath::Sin(UE_PI * f) : 1.f);
				const FVector2D Sz(W * Sc, H * Sc);
				DrawTile(Rot(Ctr - Sz * 0.5f, Sz, -25.f * (1.f - e)), L + 8, Sz, Rewards[i], false);
			}
			Ev.Insert(FEv{ 0.3f, TEXT("Clack"), 0.5f, 0.8f }, 0); Ev.Insert(FEv{ 0.55f, TEXT("Clack"), 0.6f, 0.9f }, 1); Ev.Insert(FEv{ 0.8f, TEXT("Clack"), 0.7f, 1.f }, 2); Ev.Insert(FEv{ OpenT, TEXT("Launch"), 0.8f, 1.1f }, 3);
			End = 1.1f + 0.2f * FMath::Max(0, N - 1) + 0.3f;
		}
		else if (St == 1)
		{	// ---------------- CARD FAN
			const float Pop01 = Cl(K / 0.25f), OpenT = 0.85f, o = Cl((K - OpenT) / 0.18f);
			const float Hop = (K > 0.3f && K < 0.5f) ? FMath::Sin((K - 0.3f) / 0.2f * UE_PI) * 26.f * U : (K > 0.55f && K < 0.78f) ? FMath::Sin((K - 0.55f) / 0.23f * UE_PI) * 40.f * U : 0.f;
			const float g = Cl((K - OpenT) / 0.3f);
			if (g > 0.f) for (int32 d = 0; d < 3; ++d)
			{
				const float R = (290.f * U) * (0.5f + 0.9f * g) * (1.f - 0.25f * d);
				ZTex(Out, G, L + 2, ZDiscTex(), C - FVector2D(R, R), FVector2D(R, R) * 2.f, FLinearColor(0.63f, 0.43f, 1.f, 0.22f * g));
			}
			const float CS = (290.f * U) * Pop01;
			if (Pop01 > 0.3f) DrawChest(L + 3, C - FVector2D(0.f, Hop), CS, 0.f, o, CS * 0.17f * Back(o), 0.f, 5.f * o, 0.45f * o, 1.f, 1.f);
			TArray<int32> Ci, Oi;
			for (int32 i = 0; i < N; ++i) (Rewards[i].Kind == FName(TEXT("card")) ? Ci : Oi).Add(i);
			const int32 Nc = Ci.Num(), Mo = Oi.Num();
			const float RowW = FMath::Min(220.f * U, (DW - 160.f - (N - 1) * 22.f) / FMath::Max(1, N)), RowH = RowW * 1.42f; // b266: one straight row
			const float RowX0 = DW * 0.5f - (N * RowW + (N - 1) * 22.f) * 0.5f, RowY = DH * 0.56f;
			auto RowCtr = [&](int32 Slot) { return FVector2D(RowX0 + Slot * (RowW + 22.f) + RowW * 0.5f, RowY + RowH * 0.5f); };
			const float Ch = RowH, Cw = RowW;
			const float Stp = Nc > 1 ? FMath::Min(14.f, 56.f / (Nc - 1)) : 0.f;
			const FVector2D FanC(DW * 0.5f, DH * 0.30f), Pivot = FanC + FVector2D(0.f, Ch * 1.6f), Mouth = C - FVector2D(0.f, 60.f * U);
			for (int32 j = 0; j < Nc; ++j)
			{
				const float Tj = OpenT + 0.1f + 0.13f * j, r = Cl((K - Tj) / 0.4f);
				Ev.Add(FEv{ Tj, TEXT("Whoosh"), 0.6f, 1.f + 0.08f * j });
				if (r <= 0.f) continue;
				const float Ang = (j - (Nc - 1) * 0.5f) * Stp, a = Ease(Cl(r / 0.55f)), b = Ease(Cl((r - 0.55f) / 0.45f));
				const FVector2D Fan = RowCtr(Mo + j); (void)Ang; // b266 (Evan: no tilted cards): the cards land straight in the row
				const FVector2D P = FMath::Lerp(FMath::Lerp(Mouth, FanC, a), Fan, b);
				const float Sc = FMath::Lerp(0.35f, 1.f, a);
				const FVector2D Sz(Cw * Sc, Ch * Sc);
				if (a > 0.6f) ZTex(Out, Rot(P - Sz * 0.6f, Sz * 1.2f, 0.f), L + 7 + j * 7, ZDiscTex(), FVector2D::ZeroVector, Sz * 1.2f, FLinearColor(1.f, 0.85f, 0.4f, 0.25f * b));
				DrawTile(Rot(P - Sz * 0.5f, Sz, 0.f), L + 8 + j * 7, Sz, Rewards[Ci[j]], false);
			}
			const float T2 = OpenT + 0.1f + 0.13f * FMath::Max(0, Nc - 1) + (Nc ? 0.4f : 0.f);
			const float Ow = 150.f * U, Oh = 150.f * U, Ox = DW * 0.5f - (Mo * Ow + (Mo - 1) * 20.f) * 0.5f, Oy = DH * 0.76f;
			for (int32 m = 0; m < Mo; ++m)
			{
				const float Tm = T2 + 0.12f * m, f = Cl((K - Tm) / 0.18f);
				Ev.Add(FEv{ Tm, TEXT("Score"), 0.7f, 1.1f });
				if (f <= 0.f) continue;
				const float Sc = f < 1.f ? Back(f) : 1.f;
				const FVector2D Sz(RowW * Sc, RowH * Sc), Ctr = RowCtr(m); (void)Ox; (void)Oy; (void)Oh; // b266: same size, same row
				DrawTile(Rot(Ctr - Sz * 0.5f, Sz, 0.f), L + 8, Sz, Rewards[Oi[m]], false);
			}
			Ev.Sort([](const FEv& A, const FEv& B) { return A.At < B.At; });
			Ev.Insert(FEv{ 0.3f, TEXT("Clack"), 0.6f, 0.9f }, 0); Ev.Insert(FEv{ 0.55f, TEXT("Clack"), 0.7f, 1.f }, 1); Ev.Insert(FEv{ OpenT, TEXT("Launch"), 0.8f, 1.f }, 2);
			End = T2 + 0.12f * Mo + 0.15f;
		}
		else
		{	// ---------------- P5 SLASH REVEAL
			const float Pop01 = Cl(K / 0.2f), SlT = 0.62f, s = Cl((K - SlT) / 0.1f), Hit = 0.7f, o = Cl((K - Hit) / 0.2f);
			ZRect(Out, G, L + 1, FVector2D::ZeroVector, FVector2D(DW, DH), FLinearColor(Ink.R, Ink.G, Ink.B, 0.6f * Cl((K - 0.5f) / 0.2f)));
			const FVector2D J = (K > 0.4f && K < SlT) ? FVector2D(FMath::Sin(K * 90.f), FMath::Cos(K * 70.f)) * 8.f : FVector2D::ZeroVector;
			const float Dim = Cl((K - 0.85f) / 0.3f), CS = (290.f * U) * Pop01 * (1.f - 0.2f * Dim);
			if (Pop01 > 0.3f) DrawChest(L + 3, C + J + FVector2D(0.f, 120.f * U * Dim), CS, 0.f, o, CS * 0.9f * Ease(o), CS * 0.6f * Ease(o), 34.f * o, 0.f, 1.f - 0.7f * Dim, 1.f - Cl((K - 1.15f) / 0.3f));
			if (K > Hit && K < Hit + 0.25f) Poly(L + 5, { FVector2D(0.f, 0.f), FVector2D(DW, 0.f), FVector2D(DW, DH * 0.3f), FVector2D(0.f, DH * 0.7f) }, Red.CopyWithNewOpacity(K < Hit + 0.08f ? 1.f : 1.f - (K - Hit - 0.08f) / 0.17f));
			if (s > 0.f && K < 1.05f)
			{
				const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(-28.f)), FMath::Sin(FMath::DegreesToRadians(-28.f))), Nn(-Dir.Y, Dir.X);
				const FVector2D A0 = C - Dir * DW, P1 = A0 + Dir * (2.f * DW * s);
				const float a = 1.f - Cl((K - 0.85f) / 0.2f);
				Poly(L + 6, { A0 + Nn * 34.f, P1 + Nn * 34.f, P1 - Nn * 34.f, A0 - Nn * 34.f }, Red.CopyWithNewOpacity(a));
				Poly(L + 7, { A0 + Nn * 6.f, P1 + Nn * 6.f, P1 - Nn * 6.f, A0 - Nn * 6.f }, Paper.CopyWithNewOpacity(a));
			}
			const float t = Cl((K - 0.75f) / 0.13f);
			if (t > 0.f)
			{
				const FString Ttl = (RewardTitle.IsEmpty() ? FString(TEXT("CHEST")) : RewardTitle) + TEXT("!");
				FSlateFontInfo F = ZFitFont(Ttl, 0, 110.f * U, DW * 0.86f);
				F.Size *= FMath::Lerp(2.2f, 1.f, Ease(t));
				const FVector2D Tc(DW * 0.5f, DH * 0.17f);
				RText(L + 8, Ttl, F, Tc + FVector2D(9.f, 9.f), 0.f, Red); // b266: straight
				RText(L + 9, Ttl, F, Tc - FVector2D(4.f, 4.f), 0.f, Ink);
				RText(L + 10, Ttl, F, Tc, 0.f, Paper);
			}
			static const float Tilt[4] = { -7.f, 6.f, 5.f, -6.f };
			const int32 Cols = (U > 1.f && N > 3) ? (N + 1) / 2 : N; // b166: portrait = two rows
			const float W = FMath::Min(240.f * U, (DW - 160.f - (Cols - 1) * 30.f) / FMath::Max(1, Cols)), H = W * 1.42f;
			const float Y0 = DH * 0.36f;
			for (int32 i = 0; i < N; ++i)
			{
				const float Ti = 0.95f + 0.14f * i, f = Cl((K - Ti) / 0.12f), Side = (i % 2) ? 1.f : -1.f;
				Ev.Add(FEv{ Ti + 0.1f, TEXT("Clack"), 0.8f, 1.1f + 0.05f * i });
				if (f <= 0.f) continue;
				const float e = Ease(f), Punch = f >= 1.f ? 1.f + 0.1f * (1.f - Cl((K - Ti - 0.12f) / 0.1f)) : 1.f;
				const int32 Row = i / FMath::Max(1, Cols), Col = i % FMath::Max(1, Cols), RowN = Row == 0 ? FMath::Min(N, Cols) : N - Cols;
				const float X0 = DW * 0.5f - (RowN * W + (RowN - 1) * 30.f) * 0.5f;
				const FVector2D Ctr(X0 + Col * (W + 30.f) + W * 0.5f + Side * DW * 0.6f * (1.f - e), Y0 + Row * (H + 44.f) + H * 0.5f);
				const FVector2D Sz(W * Punch, H * Punch);
				DrawTile(Rot(Ctr - Sz * 0.5f, Sz, Side * 30.f * (1.f - e)), L + 12, Sz, Rewards[i], true); (void)Tilt; // b266 (Evan: no tilted cards): they settle straight
			}
			Ev.Insert(FEv{ 0.4f, TEXT("Clack"), 0.6f, 0.8f }, 0); Ev.Insert(FEv{ SlT, TEXT("Whoosh"), 0.9f, 0.8f }, 1); Ev.Insert(FEv{ Hit, TEXT("EpicImpact"), 0.8f, 1.f }, 2);
			End = 0.95f + 0.14f * FMath::Max(0, N - 1) + 0.3f;
			Ev.Add(FEv{ End - 0.05f, TEXT("Score"), 0.7f, 1.2f });
		}
		while (Self->ChestSfxStep < Ev.Num() && K >= Ev[Self->ChestSfxStep].At) { const FEv& E = Ev[Self->ChestSfxStep++]; Sfx(E.Name, E.Vol, E.Pitch); }
		Self->ChestSkipAt = End;
		if (K > End) ZTextC(Out, G, L + 30, TEXT("TAP TO CONTINUE"), Font(0, 32.f, 2.f), FVector2D(DW * 0.5f, DH - 60.f), Paper.CopyWithNewOpacity(0.6f + 0.4f * FMath::Sin(T * 5.f)));
	}

	void PaintChestOld(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		const float K = T - PopT;
		// rotating rays behind the chest (straight-edged wedges)
		const FVector2D C(DW * 0.5f, DH * 0.40f);
		for (int32 k = 0; k < 16; k += 2)
		{
			const float A0 = FMath::DegreesToRadians(K * 20.f + 360.f * k / 16.f), A1 = FMath::DegreesToRadians(K * 20.f + 360.f * (k + 1) / 16.f);
			const float R = DW;
			const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*ZWhite());
			const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
			const FColor Cc = ((k / 2) % 2 ? Gold : Mag()).CopyWithNewOpacity(0.85f).ToFColor(true);
			auto Vx = [&RT, &Cc](const FVector2D& P) { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.5f, 0.5f), Cc); };
			TArray<FSlateVertex> V = { Vx(C), Vx(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R), Vx(C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R) };
			TArray<SlateIndex> I = { 0, 1, 2 };
			FSlateDrawElement::MakeCustomVerts(Out, L, RH, V, I, nullptr, 0, 0);
		}
		{
			TArray<FString> Parts; RewardTitle.ParseIntoArray(Parts, TEXT(" "));
			if (!Parts.Num()) Parts.Add(TEXT("CHEST"));
			float Tw = 0.f; for (const FString& Pp : Parts) Tw += ZMeasure(Pp, Font(0, 62.f)).X + 30.f;
			ZRansom(Out, G, L + 1, Parts, FVector2D(DW * 0.5f - Tw * 0.5f, 40.f), 62.f);
		}
		const float Pop01 = FMath::Clamp(K / 0.25f, 0.f, 1.f);
		ZTex(Out, G, L + 2, ZBurstTex(), C - FVector2D(190.f, 190.f) * Pop01, FVector2D(380.f, 380.f) * Pop01, Paper);
		if (Pop01 > 0.3f) ZChest(Out, G, L + 3, RewardChest.IsNone() ? FName(TEXT("wooden")) : RewardChest, C, 210.f * Pop01);
		const int32 N = Rewards.Num();
		const float W = FMath::Min(220.f, (DW - 200.f - (N - 1) * 20.f) / FMath::Max(1, N));
		const float X0 = DW * 0.5f - (N * W + (N - 1) * 20.f) * 0.5f, Y0 = DH * 0.66f;
		for (int32 i = 0; i < N; ++i)
		{
			const float In = FMath::Clamp((K - 0.35f - 0.22f * i) / 0.18f, 0.f, 1.f);
			if (In <= 0.f) continue;
			const FZoogiFrontReward& R = Rewards[i];
			const float Sc = 1.f + 0.4f * (1.f - In);
			const FVector2D Sz(W * Sc, W * 0.8f * Sc), TL(X0 + i * (W + 20.f) + (W - Sz.X) * 0.5f, Y0 + (W * 0.8f - Sz.Y) * 0.5f);
			ZPanel(Out, G, L + 4, TL, Sz, Paper, Mag(), 4.f, 8.f);
			FString Big, Small;
			if (R.Kind == FName(TEXT("gold"))) { Big = ZNumS(R.Amount); Small = TEXT("GOLD"); }
			else if (R.Kind == FName(TEXT("card"))) { const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(R.Id); Big = FString::Printf(TEXT("x%d"), R.Amount); Small = Cd ? Cd->Name.ToUpper() : R.Id.ToString().ToUpper(); }
			else { Big = ZNumS(R.Amount); Small = R.Kind.ToString().ToUpper(); }
			if (R.Kind == FName(TEXT("card"))) if (UTexture2D* A = ZLoadPng(TEXT("Data/CardArt/") + R.Id.ToString())) ZTexFit(Out, G, L + 7, A, TL + FVector2D(10.f, 8.f), FVector2D(Sz.X - 20.f, Sz.Y - 60.f), FLinearColor(1.f, 1.f, 1.f, 0.35f));
			ZTextC(Out, G, L + 8, Big, ZFitFont(Big, 0, Sz.Y * 0.36f, Sz.X - 20.f), TL + FVector2D(Sz.X * 0.5f, Sz.Y * 0.4f), Ink);
			ZRect(Out, G, L + 8, TL + FVector2D(0.f, Sz.Y - 44.f), FVector2D(Sz.X, 44.f), Ink);
			ZTextC(Out, G, L + 9, Small, ZFitFont(Small, 0, 24.f, Sz.X - 10.f), TL + FVector2D(Sz.X * 0.5f, Sz.Y - 22.f), Paper);
		}
		if (K > 0.6f) ZTextC(Out, G, L + 10, TEXT("TAP TO CONTINUE"), Font(0, 32.f, 2.f), FVector2D(DW * 0.5f, DH - 60.f), Paper.CopyWithNewOpacity(0.6f + 0.4f * FMath::Sin(T * 5.f)));
	}

	// b172g (Evan picked "C zig-zag"): the phone shop opens on one P5 row per category - alternating black / white slabs with a red
	// shadow, sheared, the picture on the opposite side; a tap opens that category
	void PaintShopMenu(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		struct FRow { int32 Id; const TCHAR* Title; const TCHAR* Sub; };
		static const FRow Rows[] = { { 0, TEXT("SPECIAL OFFERS"), TEXT("LIMITED DEALS") }, { 1, TEXT("SKINS"), TEXT("NEW LOOKS FOR YOUR BALL") }, { 7, TEXT("WOLF PACKS"), TEXT("PACK SKINS + PACK LEVEL") },
			{ 2, TEXT("CHESTS"), TEXT("COMMON - RARE - EPIC") }, { 3, TEXT("EQUIPMENT"), TEXT("TODAY'S CARD DEALS") }, { 4, TEXT("GEMS"), TEXT("GEM PACKS") }, { 5, TEXT("COINS"), TEXT("COINS FOR GEMS") } };
		const int32 N = UE_ARRAY_COUNT(Rows);
		const float Y0 = 240.f, Gap = 30.f, RH = (DH - Y0 - 40.f - Gap * (N - 1)) / N, RW = DW * 0.9f;
		const FLinearColor ShopRed = FLinearColor::FromSRGBColor(FColor(230, 20, 50));
		auto Slab = [&](int32 Lay, const FVector2D& TL, const FVector2D& Sz, float Shear, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(Out, Lay, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL), FSlateRenderTransform(FShear2D(Shear, 0.f)), FVector2D(0.5f, 0.5f)), ZWhite(), ESlateDrawEffect::None, C);
		};
		for (int32 i = 0; i < N; ++i)
		{
			const FRow& R = Rows[i];
			const bool bLeft = i % 2 == 0, bDark = i % 2 == 0;
			const float Y = Y0 + i * (RH + Gap), X0 = bLeft ? 24.f : DW - 24.f - RW, Sh = bLeft ? -0.18f : 0.18f;
			Slab(L + 2, FVector2D(X0 + 14.f, Y + 12.f), FVector2D(RW, RH), Sh, ShopRed);
			Slab(L + 3, FVector2D(X0, Y), FVector2D(RW, RH), Sh, bDark ? Ink : Paper);
			// the picture, on the far side
			const float Pic = RH * 1.02f, PX = bLeft ? X0 + RW - Pic * 0.92f : X0 - Pic * 0.08f;
			const FVector2D PTL(PX, Y + (RH - Pic) * 0.5f);
			if (R.Id == 0) ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Hotstreak")), PTL, FVector2D(Pic, Pic));
			else if (R.Id == 1) ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Blue")), PTL, FVector2D(Pic, Pic));
			else if (R.Id == 7) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/CardArt/wolf_pack")), PTL + FVector2D(Pic * 0.1f, Pic * 0.1f), FVector2D(Pic * 0.8f, Pic * 0.8f));
			else if (R.Id == 2) ZChest(Out, G, L + 5, FName(TEXT("epic")), PTL + FVector2D(Pic * 0.5f, Pic * 0.55f), Pic * 0.8f);
			else if (R.Id == 3) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/CardArt/ice_wall")), PTL + FVector2D(Pic * 0.1f, Pic * 0.1f), FVector2D(Pic * 0.8f, Pic * 0.8f));
			else if (R.Id == 4) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem_pile")), PTL, FVector2D(Pic, Pic));
			else ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), PTL, FVector2D(Pic, Pic));
			const float TX = bLeft ? X0 + 60.f : X0 + Pic + 10.f, TW = RW - Pic - 70.f;
			ZTextAt(Out, G, L + 6, R.Title, ZFitFont(R.Title, 0, RH * 0.36f, TW), FVector2D(TX, Y + RH * 0.08f), bDark ? Paper : Ink);
			ZTextAt(Out, G, L + 6, R.Sub, Font(1, RH * 0.11f), FVector2D(TX + 4.f, Y + RH * 0.72f), ShopRed);
			const int32 Id = R.Id;
			AddHit(FVector2D(X0, Y), FVector2D(RW, RH), [Self, Id]() { Self->ShopCat = Id; Self->ShopX = 0.f; Self->ShopXVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.1f); });
		}
		Self->ShopMax = 0.f;
	}

	void PaintShop(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		UZoogiFrontProfileSubsystem* P = FP();
		if (P) P->EnsureDeals();
		const bool bPhone = DH > DW;
		if (bPhone && ShopCat < 0) { PaintShopMenu(Out, G, L, Self); return; }
		// b66 (playtest): portrait used the whole screen height -> giant, mostly blank cards. Cap the card height to the
		// landscape size and centre the strip vertically.
		const float HRaw = DH - 110.f - 150.f - 56.f, H = bPhone ? FMath::Max(420.f, (DH - 480.f) / 3.f - 40.f) : FMath::Min(HRaw, 780.f);
		const float Top = bPhone ? 230.f : (DH > DW ? 150.f + (HRaw - H) * 0.5f : 150.f), Bot = Top + H + 56.f;
		float X = bPhone ? 50.f : 50.f - ShopX;
		// b172g phone: only the open section is drawn; its cards wrap into rows (each original column keeps its slot)
		int32 CurSec = -1; float SecX = 0.f, WrapX = 0.f, WrapW = 0.f; int32 WrapRow = 0; TMap<int32, FVector2D> WrapAt;
		auto Section = [&](int32 Id, const TCHAR* Title)
		{
			Self->SectionX.Add(Id, FMath::Max(0.f, X + ShopX - 50.f));
			CurSec = Id; SecX = X; WrapX = 0.f; WrapW = 0.f; WrapRow = 0; WrapAt.Reset();
			if (!bPhone) ZTag(Out, G, L, Title, FVector2D(X, Top), 56.f, false, Mag(), 32.f);
			else if (Id == ShopCat) ZTag(Out, G, L, Title, FVector2D(50.f, Top - 10.f), 90.f, false, Mag(), 60.f);
		};
		auto Place = [&](const FVector2D& Raw, float W) -> FVector2D
		{
			if (!bPhone) return Raw;
			if (CurSec != ShopCat) return FVector2D(Raw.X, Raw.Y + 100000.f); // not this category: off screen
			const int32 Key = FMath::RoundToInt(Raw.X - SecX);
			if (!WrapAt.Contains(Key))
			{
				if (WrapX > 0.f && 50.f + WrapX + W > DW - 30.f) { WrapX = 0.f; ++WrapRow; }
				WrapAt.Add(Key, FVector2D(50.f + WrapX, (float)WrapRow));
				WrapX += W + 22.f;
			}
			const FVector2D Slot = WrapAt[Key];
			return FVector2D(Slot.X, Raw.Y + Slot.Y * (H + 40.f) + 20.f);
		};
		auto Card = [&](float W, float Hh, float Y, const FLinearColor& Body, const FString& Price, TFunction<void()> Fn) -> FVector2D
		{
			const FVector2D TL = Place(FVector2D(X, Y), W);
			if (Body.A > 0.f) ZPanel(Out, G, L + 2, TL, FVector2D(W, Hh), Body, Mag(), 4.f, 8.f);
			ZRect(Out, G, L + 5, TL + FVector2D(0.f, Hh - 46.f), FVector2D(W, 46.f), Ink);
			ZTextC(Out, G, L + 6, Price, ZFitFont(Price, 0, 28.f, W - 10.f), TL + FVector2D(W * 0.5f, Hh - 23.f), Paper);
			if (Fn) AddHit(TL, FVector2D(W, Hh), MoveTemp(Fn));
			return TL;
		};
		const float Y0 = Top + 80.f, HalfH = (H - 20.f) * 0.5f;
		const int32 PLv = P ? P->Data().Level : 1;
		auto Lock = [&](const FVector2D& TL, float W, float Hh, int32 Lv) -> bool
		{	// b156: dimmed + a round UNLOCK / LV badge; a tap says when it opens
			if (PLv >= Lv) return false;
			ZRect(Out, G, L + 7, TL, FVector2D(W, Hh), FLinearColor(0.02f, 0.01f, 0.05f, 0.55f));
			const float R = FMath::Min(W, Hh) * 0.30f;
			const FVector2D C(TL.X + W - R * 0.85f, TL.Y + Hh - 46.f - R * 0.6f);
			ZTex(Out, G, L + 8, ZDiscTex(), C - FVector2D(R + 6.f, R + 6.f), FVector2D(R + 6.f, R + 6.f) * 2.f, Paper);
			ZTex(Out, G, L + 9, ZDiscTex(), C - FVector2D(R, R), FVector2D(R, R) * 2.f, FLinearColor::FromSRGBColor(FColor(42, 36, 112)));
			if (UTexture2D* Lk = ZLoadPng(TEXT("Data/MenuArt/icon_lock"))) ZTex(Out, G, L + 10, Lk, C + FVector2D(-R * 0.3f, -R * 0.82f), FVector2D(R * 0.6f, R * 0.6f));
			ZTextC(Out, G, L + 10, TEXT("UNLOCK"), Font(0, FMath::Max(16.f, R * 0.3f)), C + FVector2D(0.f, R * 0.12f), Paper);
			ZTextC(Out, G, L + 10, FString::Printf(TEXT("LV %d"), Lv), Font(0, FMath::Max(20.f, R * 0.42f)), C + FVector2D(0.f, R * 0.52f), Paper);
			AddHit(TL, FVector2D(W, Hh), [Self, Lv]() { Self->Say(FString::Printf(TEXT("REACH LEVEL %d TO UNLOCK"), Lv)); Self->Sfx(TEXT("UiCancel"), 0.7f); });
			return true;
		};
		// SPECIAL OFFERS
		Section(0, TEXT("SPECIAL OFFERS"));
		{
			const FVector2D TL = Card(240.f, H, Y0, FLinearColor(0.f, 0.f, 0.f, 0.f), TEXT("$1.99"), [Self]() { Self->Say(TEXT("THE STORE OPENS IN A LATER BUILD")); });
			Burst(Out, G, L + 4, TL + FVector2D(120.f, (H - 46.f) * 0.5f), 230.f, TEXT("NO"), TEXT("ADS!"));
			X += 270.f;
			const bool bHot = P && P->OwnsSkin(FName(TEXT("Hotstreak")));
			const FVector2D T2 = Card(340.f, H, Y0, ZTile(3), bHot ? FString(TEXT("OWNED")) : FString(TEXT("GEMS 120")), [Self, bHot]() { if (bHot) { Self->Say(TEXT("OWNED - EQUIP IT IN INVENTORY")); return; } Self->AskBuy(TEXT("UNLOCK ZOOGI?"), TEXT("HOTSTREAK"), ZBallIcon(TEXT("Hotstreak")), ZTile(3), 120, 1, [Self]() { FString W; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(TEXT("Hotstreak")), W)) Self->Say(TEXT("HOTSTREAK ZOOGI UNLOCKED!")); else Self->Say(W.ToUpper()); }); });
			ZRect(Out, G, L + 5, T2, FVector2D(340.f, 56.f), Mag());
			ZTextC(Out, G, L + 6, TEXT("LIMITED!"), Font(0, 40.f), T2 + FVector2D(170.f, 28.f), Paper);
			ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Hotstreak")), T2 + FVector2D(40.f, 80.f), FVector2D(260.f, H - 190.f));
			if (!bHot) Lock(T2, 340.f, H, HotstreakLockLv);
			X += 380.f;
		}
		// SKINS (2 rows)
		Section(1, TEXT("SKINS"));
		{
			int32 n = 0;
			const float W = HalfH;
			for (const ZoogiFront::FHero& Hh : ZoogiFront::Heroes())
			{
				if (FString(Hh.Ball) == TEXT("Hotstreak")) continue;
				int32 Kind = 0; const int32 Price = UZoogiFrontProfileSubsystem::SkinPrice(FName(Hh.Ball), Kind);
				const bool bOwn = P && P->OwnsSkin(FName(Hh.Ball));
				const FString Ball = Hh.Ball;
				const float Sx = X + (n / 2) * (W + 22.f);
				const float Sy = Y0 + (n % 2) * (HalfH + 20.f);
				const float SaveX = X; X = Sx;
				const FVector2D TL = Card(W, HalfH, Sy, ZTile(Kind == 1 ? 2 : (n % 2 ? 1 : 0)), bOwn ? FString(TEXT("OWNED")) : FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("COINS"), *ZNumS(Price)),
					[Self, Ball, bOwn, Price, Kind]() { if (bOwn) { Self->Say(TEXT("OWNED - EQUIP IT IN INVENTORY")); return; } Self->AskBuy(TEXT("UNLOCK SKIN?"), Ball.ToUpper() + TEXT(" BALL"), ZBallIcon(Ball), ZTile(Kind == 1 ? 2 : 3), Price, Kind, [Self, Ball]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(*Ball), Wy)) Self->Say(FString::Printf(TEXT("%s BALL UNLOCKED!"), *Ball.ToUpper())); else Self->Say(Wy.ToUpper()); }); });
				ZTexFit(Out, G, L + 5, ZBallIcon(Ball), TL + FVector2D(16.f, 12.f), FVector2D(W - 32.f, HalfH - 70.f), bOwn ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor::White);
				if (!bOwn) Lock(TL, W, HalfH, ZSkinLockLv(n));
				X = SaveX;
				++n;
			}
			X += ((n + 1) / 2) * (W + 22.f) + 30.f;
		}
		// WOLF PACKS (Nov 1 Wolf Pack 2.0, ZoogiWolfPack.h): pack skins - buy, then tap to equip; pack level + badges card
		Section(7, TEXT("WOLF PACKS"));
		{
			int32 n = 0;
			const float W = HalfH;
			auto WolfHead = [&](const FVector2D& Cc, float R, const FLinearColor& K)
			{
				auto D = [&](const FVector2D& C0, float Rr, const FLinearColor& Kk) { ZTex(Out, G, L + 6, ZDiscTex(), C0 - FVector2D(Rr, Rr), FVector2D(Rr * 2.f, Rr * 2.f), Kk); };
				D(Cc + FVector2D(-R * 0.72f, -R * 0.78f), R * 0.42f, Ink); D(Cc + FVector2D(R * 0.72f, -R * 0.78f), R * 0.42f, Ink);
				D(Cc + FVector2D(-R * 0.72f, -R * 0.78f), R * 0.33f, K); D(Cc + FVector2D(R * 0.72f, -R * 0.78f), R * 0.33f, K);
				D(Cc, R * 1.08f, Ink); D(Cc, R, K);
				D(Cc + FVector2D(0.f, R * 0.38f), R * 0.46f, FLinearColor::LerpUsingHSV(K, FLinearColor::White, 0.6f));
				D(Cc + FVector2D(0.f, R * 0.22f), R * 0.14f, Ink);
				D(Cc + FVector2D(-R * 0.4f, -R * 0.15f), R * 0.12f, Ink); D(Cc + FVector2D(R * 0.4f, -R * 0.15f), R * 0.12f, Ink);
			};
			for (const ZoogiWolf::FSkin& Sk : ZoogiWolf::Skins())
			{
				const bool bOwn = Sk.Price == 0 || (P && P->Data().OwnedPackSkins.Contains(Sk.Id));
				const bool bEq = P && ((P->Data().EquippedPackSkin.IsNone() && Sk.Id == FName(TEXT("frost"))) || P->Data().EquippedPackSkin == Sk.Id);
				const FName Id = Sk.Id; const int32 Price = Sk.Price, Kind = Sk.PriceKind; const FString Nm = Sk.Name;
				const float Sx = X + (n / 2) * (W + 22.f), Sy = Y0 + (n % 2) * (HalfH + 20.f);
				const float SaveX = X; X = Sx;
				const FString Lbl = bEq ? FString(TEXT("EQUIPPED")) : (bOwn ? FString(TEXT("EQUIP")) : FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("COINS"), *ZNumS(Price)));
				const FVector2D TL = Card(W, HalfH, Sy, ZTile(1), Lbl, [Self, Id, Price, Kind, Nm, bOwn]()
				{
					UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (!Pp) return;
					if (!bOwn)
					{
						if (Kind == 1) { if (!Pp->SpendGems(Price, FName(TEXT("pack_skin")))) { Self->Say(TEXT("NOT ENOUGH GEMS")); return; } }
						else { if (Pp->Data().Gold < Price) { Self->Say(TEXT("NOT ENOUGH GOLD")); return; } Pp->Mutable().Gold -= Price; }
						Pp->Mutable().OwnedPackSkins.Add(Id);
						Self->Say(FString::Printf(TEXT("%s UNLOCKED!"), *Nm));
					}
					else Self->Say(FString::Printf(TEXT("%s EQUIPPED"), *Nm));
					Pp->Mutable().EquippedPackSkin = Id;
					Pp->Save();
				});
				WolfHead(TL + FVector2D(W * 0.5f, (HalfH - 46.f) * 0.5f + 14.f), FMath::Min(W, HalfH - 60.f) * 0.30f, bOwn ? Sk.Tint : FLinearColor(Sk.Tint.R * 0.6f, Sk.Tint.G * 0.6f, Sk.Tint.B * 0.6f, 1.f));
				ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 16.f), TL + FVector2D(W * 0.5f, 20.f), Paper);
				if (!bOwn && !Lock(TL, W, HalfH, ZPackLockLv(n, Price))) // b156: buying goes through the BUY pop-up
					AddHit(TL, FVector2D(W, HalfH), [Self, Id, Price, Kind, Nm]() { Self->AskBuy(TEXT("UNLOCK PACK?"), Nm, nullptr, ZTile(1), Price, Kind, [Self, Id, Price, Kind, Nm]()
					{
						UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (!Pp) return;
						if (Kind == 1) { if (!Pp->SpendGems(Price, FName(TEXT("pack_skin")))) { Self->Say(TEXT("NOT ENOUGH GEMS")); return; } }
						else { if (Pp->Data().Gold < Price) { Self->Say(TEXT("NOT ENOUGH COINS")); return; } Pp->Mutable().Gold -= Price; }
						Pp->Mutable().OwnedPackSkins.Add(Id); Pp->Mutable().EquippedPackSkin = Id; Pp->Save();
						Self->Say(FString::Printf(TEXT("%s UNLOCKED!"), *Nm));
					}); });
				X = SaveX; ++n;
			}
			X += ((n + 1) / 2) * (W + 22.f);
			// pack level + badges
			{
				const int32 Xp = P ? P->Data().PackXP : 0; int32 Base = 0, Next = 0;
				const int32 Lv = ZoogiWolf::LevelForXP(Xp, &Base, &Next);
				const FVector2D TL = Card(300.f, H, Y0, Paper, FString::Printf(TEXT("PACK LV %d"), Lv), nullptr);
				WolfHead(TL + FVector2D(150.f, 110.f), 60.f, P ? ZoogiWolf::SkinById(P->Data().EquippedPackSkin).Tint : ZoogiWolf::Skins()[0].Tint);
				const FString Prog = Lv >= 5 ? FString(TEXT("MAX LEVEL")) : FString::Printf(TEXT("%d / %d XP"), Xp, Next);
				ZTextC(Out, G, L + 6, Prog, Font(0, 24.f), TL + FVector2D(150.f, 200.f), Mag());
				static const TCHAR* Perks[] = { TEXT("LV2 LONGER HUNT"), TEXT("LV3 BIGGER BLAST"), TEXT("LV4 WOLVES REBOUND"), TEXT("LV5 A 4TH WOLF") };
				for (int32 k = 0; k < 4; ++k) ZTextC(Out, G, L + 6, Perks[k], Font(0, 19.f), TL + FVector2D(150.f, 236.f + 26.f * k), Lv >= k + 2 ? Ink : FLinearColor(0.f, 0.f, 0.f, 0.35f));
				static const TCHAR* Bd[] = { TEXT("triple_hunt"), TEXT("blood_moon"), TEXT("lone_wolf"), TEXT("alpha_strike") };
				float By = 352.f;
				for (int32 k = 0; k < 4; ++k)
				{
					const bool bGot = P && P->Data().PackBadges.Contains(FName(Bd[k]));
					const FString Bt = (bGot ? FString(TEXT("* ")) : FString(TEXT("- "))) + ZoogiWolf::BadgeTitle(FName(Bd[k]));
					if (By < H - 60.f) ZTextC(Out, G, L + 6, Bt, Font(0, 19.f), TL + FVector2D(150.f, By), bGot ? Mag() : FLinearColor(0.f, 0.f, 0.f, 0.35f));
					By += 24.f;
				}
				X += 330.f;
			}
		}
		// CHESTS
		Section(2, TEXT("CHESTS"));
		for (const FZoogiFrontChestDef& Cd : UZoogiFrontProfileSubsystem::ChestDefs())
		{
			const int32 Left = P ? P->ChestStockLeft(Cd.Id) : -1;
			const FName Id = Cd.Id;
			const FString Name = FString(Cd.Name).ToUpper();
			const FVector2D TL = Card(250.f, H, Y0, ZTile(Id == FName(TEXT("epic")) ? 3 : Id == FName(TEXT("royal")) ? 2 : 0), Left == 0 ? FString(TEXT("SOLD OUT")) : FString::Printf(TEXT("GEMS %s"), *ZNumS(Cd.GemCost)),
				[Self, Id, Name, Cost = Cd.GemCost, Left]() { if (Left == 0) { Self->Say(TEXT("SOLD OUT - BACK TOMORROW")); return; } Self->AskBuy(TEXT("OPEN CHEST?"), Name, nullptr, ZTile(Id == FName(TEXT("epic")) ? 3 : Id == FName(TEXT("royal")) ? 2 : 0), Cost, 1, [Self, Id, Name]() { FString Wy; TArray<FZoogiFrontReward> R; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyChest(Id, R, Wy)) Self->ShowRewards(Name, Id, R); else Self->Say(Wy.ToUpper()); }, Id); });
			ZTextC(Out, G, L + 5, Name, ZFitFont(Name, 0, 30.f, 230.f), TL + FVector2D(125.f, 40.f), Paper);
			ZChest(Out, G, L + 5, Id, TL + FVector2D(125.f, H * 0.45f), 140.f);
			Lock(TL, 250.f, H, ZChestLockLv(Id));
			if (Left > 0) ZTextC(Out, G, L + 5, FString::Printf(TEXT("%d LEFT TODAY"), Left), Font(0, 22.f), TL + FVector2D(125.f, H - 80.f), Gold);
			X += 280.f;
		}
		X += 20.f;
		// EQUIPMENT: today's card deals (gold)
		Section(3, TEXT("EQUIPMENT"));
		if (P)
		{
			const TArray<FZoogiFrontDeal>& Deals = P->Data().Deals;
			const float W = HalfH;
			for (int32 i = 0; i < Deals.Num(); ++i)
			{
				const FZoogiFrontDeal& Dl = Deals[i];
				const FZoogiCardDef* Cd = UZoogiFrontProfileSubsystem::FindCard(Dl.CardId);
				const float SaveX = X; X = X + (i / 2) * (W + 22.f);
				const FVector2D TL = Card(W, HalfH, Y0 + (i % 2) * (HalfH + 20.f), ZTile(Dl.bFree ? 0 : 5), Dl.bBought ? FString(TEXT("BOUGHT")) : Dl.bFree ? FString(TEXT("FREE!")) : FString::Printf(TEXT("COINS %s"), *ZNumS(Dl.Price)),
					[Self, i, Free = Dl.bFree, Bought = Dl.bBought, Price = Dl.Price, Nm2 = (Cd ? Cd->Name.ToUpper() : Dl.CardId.ToString()), Art = Dl.CardId]()
					{
						auto Do = [Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyDeal(i, Wy)) Self->Say(TEXT("ADDED TO YOUR CARDS!")); else Self->Say(Wy.ToUpper()); };
						if (Free || Bought) { Do(); return; }
						Self->AskBuy(TEXT("BUY CARD?"), Nm2, ZLoadPng(TEXT("Data/CardArt/") + Art.ToString()), ZTile(5), Price, 0, Do);
					});
				if (UTexture2D* A = ZLoadPng(TEXT("Data/CardArt/") + Dl.CardId.ToString())) ZTexFit(Out, G, L + 5, A, TL + FVector2D(12.f, 10.f), FVector2D(W - 24.f, HalfH - 64.f), Dl.bBought ? FLinearColor(1.f, 1.f, 1.f, 0.4f) : FLinearColor::White);
				const FString Nm = FString::Printf(TEXT("%s x%d"), Cd ? *Cd->Name.ToUpper() : *Dl.CardId.ToString(), Dl.Count);
				ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 12.f), TL + FVector2D(W * 0.5f, HalfH - 64.f), Paper);
				X = SaveX;
			}
			X += ((Deals.Num() + 1) / 2) * (W + 22.f) + 30.f;
		}
		// CRYSTALS (gem packs: store later)
		Section(4, TEXT("GEMS"));
		if (P)
		{
			const TArray<UZoogiFrontProfileSubsystem::FGemPack> Gp = P->GemPacks();
			const float W = HalfH;
			for (int32 i = 0; i < Gp.Num(); ++i)
			{
				const float SaveX = X; X = X + (i / 2) * (W + 22.f);
				const FVector2D TL = Card(W, HalfH, Y0 + (i % 2) * (HalfH + 20.f), ZTile(2), FString::Printf(TEXT("$%.2f"), Gp[i].USD), [Self]() { Self->Say(TEXT("GEM PACKS OPEN ONCE THE APP STORE IS SET UP")); });
				ZTexFit(Out, G, L + 5, ZLoadPng(i < 2 ? TEXT("Data/MenuArt/Front/icon_gem3d") : TEXT("Data/MenuArt/Front/icon_gem_pile")), TL + FVector2D(14.f, 8.f), FVector2D(W - 28.f, HalfH - 128.f)); // b156: the real gem art
				ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gems), ZFitFont(ZNumS(Gp[i].Gems), 0, 40.f, W - 20.f), TL + FVector2D(W * 0.5f, HalfH - 74.f), Paper);
				X = SaveX;
			}
			X += ((Gp.Num() + 1) / 2) * (W + 22.f) + 30.f;
		}
		// COINS (gold for gems)
		Section(5, TEXT("COINS"));
		{
			const TArray<UZoogiFrontProfileSubsystem::FGoldPack>& Gp = UZoogiFrontProfileSubsystem::GoldPacks();
			for (int32 i = 0; i < Gp.Num(); ++i)
			{
				const FVector2D TL = Card(250.f, H, Y0, ZTile(3), FString::Printf(TEXT("GEMS %s"), *ZNumS(Gp[i].Gems)), [Self, i, Gm = Gp[i].Gems, Gd = Gp[i].Gold]() { Self->AskBuy(TEXT("BUY COINS?"), ZNumS(Gd) + TEXT(" COINS"), ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), ZTile(3), Gm, 1, [Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyGold(i, Wy)) Self->Say(TEXT("COINS ADDED!")); else Self->Say(Wy.ToUpper()); }); });
				ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), TL + FVector2D(25.f, 30.f), FVector2D(200.f, 200.f)); // b156: the real coin art
				ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gold), Font(0, 44.f), TL + FVector2D(125.f, H * 0.62f), Paper);
				ZTextC(Out, G, L + 6, FString(Gp[i].Name).ToUpper().Replace(TEXT("GOLD"), TEXT("COINS")), ZFitFont(FString(Gp[i].Name).ToUpper().Replace(TEXT("GOLD"), TEXT("COINS")), 0, 26.f, 230.f), TL + FVector2D(125.f, H * 0.62f + 50.f), Ink);
				X += 280.f;
			}
		}
		Self->ShopMax = bPhone ? 0.f : FMath::Max(0.f, X + ShopX - DW + 50.f);
		if (bPhone) return; // b172g: no side-scrolling tab bar on the phone - BACK returns to the category menu
		// bottom tab bar
		ZRect(Out, G, L + 20, FVector2D(0.f, DH - 76.f), FVector2D(DW, 76.f), Paper);
		static const TCHAR* Names[] = { TEXT("SPECIAL OFFERS"), TEXT("SKINS"), TEXT("CHESTS"), TEXT("EQUIPMENT"), TEXT("GEMS"), TEXT("COINS") };
		int32 Cur = 0;
		for (int32 k = 0; k < 6; ++k) if (ShopX + 60.f >= SectionX.FindRef(k)) Cur = k;
		float Tw = 0.f; for (const TCHAR* N : Names) Tw += ZMeasure(N, Font(0, 30.f)).X + 60.f;
		float Tx = DW * 0.5f - Tw * 0.5f;
		for (int32 k = 0; k < 6; ++k)
		{
			const float W = ZMeasure(Names[k], Font(0, 30.f)).X + 60.f;
			if (k == Cur) ZRect(Out, G, L + 21, FVector2D(Tx + 8.f, DH - 66.f), FVector2D(W - 16.f, 56.f), Ink);
			ZTextC(Out, G, L + 22, Names[k], Font(0, 30.f), FVector2D(Tx + W * 0.5f, DH - 38.f), k == Cur ? Paper : Ink);
			AddHit(FVector2D(Tx, DH - 76.f), FVector2D(W, 76.f), [Self, k]() { Self->ShopX = FMath::Min(Self->ShopMax, Self->SectionX.FindRef(k)); Self->ShopXVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.6f, 1.1f); });
			Tx += W;
		}
	}
};

// ================================================================================================ UMG wrapper
TSharedRef<SWidget> UZoogiFrontWidget::RebuildWidget()
{
	TSharedRef<SZoogiFront> W = SNew(SZoogiFront);
	W->Menu = Menu;
	W->Ctx = Menu.IsValid() ? (UObject*)Menu.Get() : (UObject*)this;
	MySlate = W;
	if (UWorld* World = GetWorld()) AZoogiFrontStage::Ensure(World);
	ZoogiFront::ApplyEquipped(W->Ctx.Get());
	return W;
}

void UZoogiFrontWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MySlate.Reset();
}

// ================================================================================================ the 3D stage
AZoogiFrontStage::AZoogiFrontStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Aurex"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetHiddenInGame(true);
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	DiscRim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscRim"));
	DiscRim2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscRim2"));
	for (UStaticMeshComponent* C : { Disc.Get(), DiscRim.Get(), DiscRim2.Get() })
	{
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetHiddenInGame(true);
	}
	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetupAttachment(RootComponent); Model->SetCollisionEnabled(ECollisionEnabled::NoCollision); Model->SetCastShadow(false); Model->SetHiddenInGame(true);
	Key = CreateDefaultSubobject<UPointLightComponent>(TEXT("Key"));
	Key->SetupAttachment(RootComponent);
	Key->SetCastShadows(false);
	Key->SetIntensity(9000.f);
	Key->SetAttenuationRadius(900.f);
	Key->SetVisibility(false);
}

AZoogiFrontStage* AZoogiFrontStage::Ensure(UWorld* World)
{
	if (!World || !ZoogiFront::On()) return nullptr;
	for (TActorIterator<AZoogiFrontStage> It(World); It; ++It) return *It;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiFrontStage* S = World->SpawnActor<AZoogiFrontStage>(AZoogiFrontStage::StaticClass(), FTransform::Identity, SP);
	if (!S) return nullptr;
	if (USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet)) S->Mesh->SetSkeletalMeshAsset(SM);
	S->Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Mecha_Aurex/Anims/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (const TCHAR* C : { TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cheer.GEN_cheer"), TEXT("/Game/Mecha_Aurex/Anims/Rifle/R_MF_Emote_FingerGuns.R_MF_Emote_FingerGuns"), TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell") })
		if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, C, nullptr, LOAD_NoWarn | LOAD_Quiet)) S->Cheers.Add(A);
	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	S->Disc->SetStaticMesh(Cyl); S->DiscRim->SetStaticMesh(Cyl); S->DiscRim2->SetStaticMesh(Cyl);
	S->Disc->SetMaterial(0, ZoogiUtil::SurfaceMID(S, FLinearColor(0.01f, 0.01f, 0.012f), 0.4f));
	S->DiscRim->SetMaterial(0, ZoogiUtil::GlowMID(S, FLinearColor(1.f, 1.f, 1.f), 2.f));
	S->DiscRim2->SetMaterial(0, ZoogiUtil::GlowMID(S, ZoogiP5::Accent(), 3.f));
	FActorSpawnParameters CP; CP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	S->Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, CP);
	if (S->Cam) { S->Cam->GetCameraComponent()->bConstrainAspectRatio = false; S->Cam->GetCameraComponent()->SetFieldOfView(80.f); }
	UE_LOG(LogZoogi, Log, TEXT("FRONT stage spawned (Aurex %s, %d cheer clips)"), S->Mesh->GetSkeletalMeshAsset() ? TEXT("ok") : TEXT("MISSING"), S->Cheers.Num());
	return S;
}

namespace
{
	// scenic arena views per screen (Ancient Collision Meadows poses, ZoogiArenaShowcase): look point, yaw, pitch, distance, fov
	struct FPose { FVector Look; float Yaw, Pitch, Dist, Fov; };
	const FPose GPoses[3] = {
		{ FVector(0, -2200, 700), 272.f, 14.f, 8200.f, 72.f },   // HOME: v14 front - plaza, volcano, the four islands (b56)
		{ FVector(2600, -900, 200), 255.f, 8.f, 2600.f, 78.f },   // INVENTORY: low on the Sunstone side, up to the Citadel
		{ FVector(0, -1600, -100), 270.f, 48.f, 14500.f, 55.f },  // SHOP: the whole diorama from the south
	};
	FTransform PoseXf(const FPose& P, float Drift)
	{
		const FRotator H(0.f, P.Yaw + Drift, 0.f);
		const float Pr = FMath::DegreesToRadians(P.Pitch);
		const FVector Loc = P.Look - H.Vector() * P.Dist * FMath::Cos(Pr) + FVector(0, 0, P.Dist * FMath::Sin(Pr));
		return FTransform((P.Look - Loc).Rotation(), Loc);
	}
}

void AZoogiFrontStage::Activate(bool bOn)
{
	if (bOn == bActive) return;
	bActive = bOn;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (bOn)
	{
		// the menu shows the arena the Royale is played on
		if (AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(this)))
			if (G->Arena && G->Arena->GetTheme() != EZoogiArena::Ancient) { G->Arena->Build(EZoogiArena::Ancient); UE_LOG(LogZoogi, Log, TEXT("FRONT built Ancient Collision Meadows for the menu")); }
		if (PC && Cam) { if (PC->GetViewTarget() != Cam) PrevView = PC->GetViewTarget(); PC->SetViewTarget(Cam); }
		ScreenShown = -1;
	}
	else if (PC && Cam && PC->GetViewTarget() == Cam)
	{	// b187: leaving the menu goes to the match camera, never back to whatever special cam was live when the menu opened (stuck after exit)
		AZoogiPlayerController* Zpc = Cast<AZoogiPlayerController>(PC);
		AActor* To = (Zpc && Zpc->MainCam()) ? static_cast<AActor*>(Zpc->MainCam()) : PrevView.Get();
		if (To) PC->SetViewTarget(To);
		PrevView.Reset();
	}
	Mesh->SetHiddenInGame(!bOn);
	for (UStaticMeshComponent* C : { Disc.Get(), DiscRim.Get(), DiscRim2.Get() }) C->SetHiddenInGame(!bOn);
	Key->SetVisibility(bOn);
	UE_LOG(LogZoogi, Log, TEXT("FRONT stage %s"), bOn ? TEXT("on") : TEXT("off"));
}

void AZoogiFrontStage::ApplyHero(const FString& Hero)
{
	HeroShown = Hero;
	{	// b203: Zoogis with their own Tripo model show it in the 3D view
		// b208 (Evan: "add model to game in ballers 3d view"): his Tripo Ballers - Blue, Frosti, Lumi, Vex, Hotstreak (-ZoogiNoTripoBallers = the Aurex robot)
		static const TCHAR* TripoBallers[][2] = {
			{ TEXT("Dark"), TEXT("/Game/Zoogis/Dark/SM_DarkZoogi.SM_DarkZoogi") },
			{ TEXT("Blue"), TEXT("/Game/Zoogis/Ballers/Blue/SM_Blue.SM_Blue") },
			{ TEXT("Frosti"), TEXT("/Game/Zoogis/Ballers/Frosti/SM_Frosti.SM_Frosti") },
			{ TEXT("Lumi"), TEXT("/Game/Zoogis/Ballers/Lumi/SM_Lumi.SM_Lumi") },
			{ TEXT("Vex"), TEXT("/Game/Zoogis/Ballers/Vex/SM_Vex.SM_Vex") },
			{ TEXT("Hotstreak"), TEXT("/Game/Zoogis/Ballers/Hotstreak/SM_Hotstreak.SM_Hotstreak") },
		};
		static const bool bNoTripo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTripoBallers"));
		UStaticMesh* Sm = nullptr;
		for (const auto& Tb : TripoBallers)
			if (Hero.Equals(Tb[0], ESearchCase::IgnoreCase) && (!bNoTripo || Hero.Equals(TEXT("Dark"), ESearchCase::IgnoreCase))) { Sm = LoadObject<UStaticMesh>(nullptr, Tb[1], nullptr, LOAD_NoWarn | LOAD_Quiet); break; }
		bStaticModel = Sm != nullptr;
		if (Model) Model->SetStaticMesh(Sm);
	}
	UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!AB || !Mesh->GetSkeletalMeshAsset()) return;
	const ZoogiFront::FHero* H = ZoogiFront::HeroByName(Hero);
	FLinearColor Z[4];
	ZoogiAurexPalette(H ? FString(H->RosterId) : Hero, 0, Z);
	if (!MID) MID = UMaterialInstanceDynamic::Create(AB, this);
	MID->SetVectorParameterValue(TEXT("Color_Zone_R"), Z[0]); MID->SetVectorParameterValue(TEXT("Color_Zone_G"), Z[1]);
	MID->SetVectorParameterValue(TEXT("Color_Zone_B"), Z[2]); MID->SetVectorParameterValue(TEXT("Emissive_Color"), Z[3]);
	for (int32 m = 0; m < Mesh->GetNumMaterials(); ++m) Mesh->SetMaterial(m, MID);
	if (DiscRim2) DiscRim2->SetMaterial(0, ZoogiUtil::GlowMID(this, Z[3], 3.f));
}

void AZoogiFrontStage::PlaceRobot(float Dt)
{
	const ZoogiFront::FView& V = ZoogiFront::View();
	if (!Cam) return;
	const FTransform CT = Cam->GetActorTransform();
	const float HFov = Cam->GetCameraComponent()->FieldOfView;
	FVector2D VP(16.f, 9.f);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(VP);
	const float TanH = FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f));
	const float TanV = TanH * VP.Y / FMath::Max(1.f, VP.X);
	const float RobotH = 178.f;
	const float D = RobotH / FMath::Max(0.05f, V.Height01 * 2.f * TanV);
	const FVector Fwd = CT.GetRotation().GetForwardVector(), Right = CT.GetRotation().GetRightVector(), Up = CT.GetRotation().GetUpVector();
	const FVector Feet = CT.GetLocation() + Fwd * D + Right * ((2.f * V.Feet01.X - 1.f) * D * TanH) + Up * ((1.f - 2.f * V.Feet01.Y) * D * TanV);
	const float MeshH = Mesh->GetSkeletalMeshAsset() ? FMath::Max(10.f, (float)Mesh->GetSkeletalMeshAsset()->GetBounds().BoxExtent.Z * 2.f) : RobotH;
	const float FaceYaw = CT.Rotator().Yaw + 180.f + V.Spin + 8.f * FMath::Sin(T * 0.6f);
	Mesh->SetWorldScale3D(FVector(RobotH / MeshH));
	Mesh->SetWorldLocationAndRotation(Feet, FRotator(0.f, FaceYaw - 90.f, 0.f)); // UE4-mannequin rig faces +Y
	if (bStaticModel && Model && Model->GetStaticMesh())
	{	// b203: the Tripo model, the same height, feet on the disc
		const FBox Bb = Model->GetStaticMesh()->GetBoundingBox(); const float Hm = FMath::Max(1.f, (float)Bb.GetSize().Z), Sm = RobotH / Hm;
		static float ModelYaw = -1000.f; if (ModelYaw < -999.f) { ModelYaw = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiModelYaw="), ModelYaw); }
		Model->SetWorldScale3D(FVector(Sm));
		Model->SetWorldLocationAndRotation(Feet - FVector(0, 0, Bb.Min.Z * Sm), FRotator(0.f, FaceYaw + ModelYaw, 0.f));
	}
	const float R = 85.f;
	Disc->SetWorldLocation(Feet - FVector(0, 0, 3.f)); Disc->SetWorldScale3D(FVector(R / 50.f, R / 50.f, 0.06f));
	DiscRim->SetWorldLocation(Feet - FVector(0, 0, 4.f)); DiscRim->SetWorldScale3D(FVector((R + 7.f) / 50.f, (R + 7.f) / 50.f, 0.05f));
	DiscRim2->SetWorldLocation(Feet - FVector(0, 0, 5.f)); DiscRim2->SetWorldScale3D(FVector((R + 15.f) / 50.f, (R + 15.f) / 50.f, 0.04f));
	Key->SetWorldLocation(Feet + FVector(0, 0, 130.f) - Fwd * 160.f + Right * 60.f);
}

void AZoogiFrontStage::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	const ZoogiFront::FView& V = ZoogiFront::View();
	AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bWant = G && G->Phase == EZoogiPhase::Menu && FPlatformTime::Seconds() - V.AliveT < 0.4;
	Activate(bWant);
	if (!bActive) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && Cam && PC->GetViewTarget() != Cam) PC->SetViewTarget(Cam); // the menu orbit camera keeps reclaiming
	// camera: glide to the screen's pose, slow drift
	const int32 Sc = FMath::Clamp(V.Screen, 0, 2);
	if (Sc != ScreenShown)
	{
		CamFrom = ScreenShown < 0 ? PoseXf(GPoses[Sc], 0.f) : Cam->GetActorTransform();
		FovFrom = ScreenShown < 0 ? GPoses[Sc].Fov : Cam->GetCameraComponent()->FieldOfView;
		ScreenShown = Sc; ScreenBlend = 0.f;
	}
	ScreenBlend = FMath::Min(1.f, ScreenBlend + Dt / 0.7f);
	const float E = 1.f - FMath::Pow(1.f - ScreenBlend, 3.f);
	const FTransform To = PoseXf(GPoses[Sc], 3.f * FMath::Sin(T * 0.12f));
	FTransform Now; Now.Blend(CamFrom, To, E);
	Cam->SetActorTransform(Now);
	Cam->GetCameraComponent()->SetFieldOfView(FMath::Lerp(FovFrom, GPoses[Sc].Fov, E));
	// robot
	if (V.Hero != HeroShown) ApplyHero(V.Hero);
	const bool bShow = V.bRobot && Mesh->GetSkeletalMeshAsset();
	Mesh->SetHiddenInGame(!bShow || bStaticModel);
	if (Model) Model->SetHiddenInGame(!bShow || !bStaticModel);
	for (UStaticMeshComponent* C : { Disc.Get(), DiscRim.Get(), DiscRim2.Get() }) C->SetHiddenInGame(!bShow);
	if (!bShow) return;
	if (V.CheerSerial != CheerSeen && Cheers.Num())
	{
		CheerSeen = V.CheerSerial;
		UAnimSequence* A = Cheers[CheerSeen % Cheers.Num()];
		Mesh->PlayAnimation(A, false);
		CheerLeft = A->GetPlayLength();
	}
	else if (CheerLeft > 0.f) { CheerLeft -= Dt; if (CheerLeft <= 0.f && Idle) Mesh->PlayAnimation(Idle, true); }
	else if (Idle && Mesh->GetSingleNodeInstance() == nullptr) Mesh->PlayAnimation(Idle, true);
	PlaceRobot(Dt);
}

// ================================================================================================ control settings
namespace ZoogiControls
{
	static const TCHAR* Sec = TEXT("ZoogiRoll.Controls");
	static float Get(const TCHAR* Key, float Def, float Lo, float Hi)
	{
		float V = Def;
		if (GConfig) GConfig->GetFloat(Sec, Key, V, GGameUserSettingsIni);
		return FMath::Clamp(V, Lo, Hi);
	}
	float LookSens() { return Get(TEXT("LookSens"), 1.f, 0.3f, 3.f); }
	float StickSens() { return Get(TEXT("StickSens"), 1.f, 0.3f, 3.f); }
	float MoveSens() { return Get(TEXT("MoveSens"), 1.f, 0.5f, 2.f); }
	bool InvertY() { return Get(TEXT("InvertY"), 0.f, 0.f, 1.f) > 0.5f; }
	int32 StickHide() { return FMath::RoundToInt(Get(TEXT("StickHide"), 0.f, 0.f, 3.f)); }
	bool CleanAim() { return Get(TEXT("CleanAim"), 1.f, 0.f, 1.f) > 0.5f; }
	float FlashMul() { return Get(TEXT("ReduceFlash"), 0.f, 0.f, 1.f) > 0.5f ? 0.f : 0.45f; }
	bool WatchCPU() { return Get(TEXT("WatchCPU"), 0.f, 0.f, 1.f) > 0.5f; }
	int32 TrainBots() { return FMath::RoundToInt(Get(TEXT("TrainBots"), 0.f, 0.f, 3.f)); }
	bool DarkArena() { return FParse::Param(FCommandLine::Get(), TEXT("ZoogiDarkArena")) || Get(TEXT("DarkArena"), 0.f, 0.f, 1.f) > 0.5f; } // b243
	bool ReplayCam() { return FParse::Param(FCommandLine::Get(), TEXT("ZoogiReplayCam")) || Get(TEXT("ReplayCam"), 0.f, 0.f, 1.f) > 0.5f; }
	static bool GAimHintDone = false;
	bool AimHintDone() { return GAimHintDone; }
	void SetAimHintDone(bool b) { GAimHintDone = b; }
	void Set(const TCHAR* Key, float V)
	{
		if (!GConfig) return;
		GConfig->SetFloat(Sec, Key, V, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		UE_LOG(LogZoogi, Log, TEXT("CONTROLS %s = %.2f"), Key, V);
	}
}


// ================================================================================================ Royale pause / settings panel
class SZoogiRoyalePause : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiRoyalePause) {}
	SLATE_END_ARGS()
	TWeakObjectPtr<UWorld> World;
	float T = 0.f;
	struct FHit { FBox2D R; TFunction<void()> Fn; };
	mutable TArray<FHit> Hits;
	mutable float S = 1.f, DW = 1920.f, DH = 1080.f;

	void Construct(const FArguments&) { SetCanTick(true); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool ComputeVolatility() const override { return true; }
	bool bShotDone = false;
	virtual void Tick(const FGeometry&, const double, const float Dt) override
	{
		const float T0 = T;
		T += FMath::Min(Dt, 0.05f);
		if (FMath::FloorToInt(T0) != FMath::FloorToInt(T)) UE_LOG(LogZoogi, Log, TEXT("ROYALE pause panel up %.0f s"), T);
		if (!bShotDone && T > 1.f && World.IsValid() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyalePauseTest")))
		{
			bShotDone = true;
			if (AZoogiGameMode* Gm = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(World.Get()))) Gm->TakeShot(TEXT("royale_pause"));
		}
	}
	void Sfx(const TCHAR* N, float P = 1.f) const { if (World.IsValid()) ZoogiUtil::PlaySfx(World.Get(), N, 0.8f, P); }
	FReply Press(const FGeometry& G, const FVector2D& Scr)
	{
		const FVector2D P = G.AbsoluteToLocal(Scr) / FMath::Max(S, 0.01f);
		for (int32 i = Hits.Num() - 1; i >= 0; --i) if (Hits[i].R.IsInside(P)) { TFunction<void()> Fn = Hits[i].Fn; if (Fn) Fn(); break; }
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 L0, const FWidgetStyle&, bool) const override
	{
		const FVector2D Px = Geo.GetLocalSize();
		S = Px.Y > Px.X ? Px.X / 1300.f : Px.Y / 1080.f;
		DW = Px.X / S; DH = Px.Y / S;
		const FGeometry G = Geo.MakeChild(FVector2D(DW, DH), FSlateLayoutTransform(S, FVector2D::ZeroVector));
		Hits.Reset();
		SZoogiRoyalePause* Self = const_cast<SZoogiRoyalePause*>(this);
		int32 L = L0;
		ZRect(Out, G, L, FVector2D::ZeroVector, FVector2D(DW, DH), FLinearColor(0.01f, 0.01f, 0.04f, 0.72f));
		Hits.Add({ FBox2D(FVector2D::ZeroVector, FVector2D(DW, DH)), nullptr });
		ZTiled(Out, G, L + 1, ZDotsTex(), FVector2D::ZeroVector, FVector2D(DW * 0.14f, DH), 32.f, Mag().CopyWithNewOpacity(0.4f));
		ZTiled(Out, G, L + 1, ZDotsTex(), FVector2D(DW * 0.86f, 0.f), FVector2D(DW * 0.14f, DH), 32.f, Mag().CopyWithNewOpacity(0.4f));
		ZRansom(Out, G, L + 2, { TEXT("PAU"), TEXT("SED"), TEXT("!") }, FVector2D(DW * 0.5f - 160.f, 40.f), 66.f);
		// left: actions
		const float LX = DW * 0.5f - 640.f, TopY = 190.f;
		struct FA { const TCHAR* N; bool bMag; TFunction<void()> Fn; };
		TArray<FA> Acts = {
			{ TEXT("RESUME"), true, [Self]() { Self->Sfx(TEXT("UiConfirm")); ZoogiFront::CloseRoyalePause(true); } },
			{ TEXT("RESTART MATCH"), false, [Self]() { TWeakObjectPtr<UWorld> W = Self->World; ZoogiFront::CloseRoyalePause(true); if (W.IsValid()) if (AZoogiGameMode* Gm = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(W.Get()))) Gm->MenuAction(TEXT("Rematch"), 0); } },
			{ TEXT("MAIN MENU"), false, [Self]() { TWeakObjectPtr<UWorld> W = Self->World; ZoogiFront::CloseRoyalePause(true); if (W.IsValid()) if (AZoogiGameMode* Gm = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(W.Get()))) Gm->MenuAction(TEXT("Menu"), 0); } },
		};
		for (int32 i = 0; i < Acts.Num(); ++i)
		{
			const FVector2D TL(LX, TopY + i * 130.f), Sz(420.f, 100.f);
			ZPanel(Out, G, L + 3, TL, Sz, Acts[i].bMag ? Mag() : Paper, Acts[i].bMag ? Paper : Mag(), 5.f, 10.f);
			ZTextC(Out, G, L + 6, Acts[i].N, Font(0, 46.f, Acts[i].bMag ? 3.f : 0.f), TL + Sz * 0.5f, Acts[i].bMag ? Paper : Ink);
			Hits.Add({ FBox2D(TL - FVector2D(6.f, 6.f), TL + Sz + FVector2D(6.f, 6.f)), Acts[i].Fn });
		}
		// right: settings
		const float RX = DW * 0.5f - 160.f, RW = 800.f;
		ZPanel(Out, G, L + 3, FVector2D(RX, TopY - 10.f), FVector2D(RW, 780.f), Paper, Mag(), 5.f, 12.f);
		ZRect(Out, G, L + 6, FVector2D(RX, TopY - 10.f), FVector2D(RW, 64.f), Ink);
		ZTextC(Out, G, L + 7, TEXT("SETTINGS"), Font(0, 40.f), FVector2D(RX + RW * 0.5f, TopY + 22.f), Paper);
		SettingsRows(Out, G, L, RX, TopY + 72.f, RW, [this](const FVector2D& TL, const FVector2D& Sz, TFunction<void()> Fn) { Hits.Add({ FBox2D(TL, TL + Sz), Fn }); }, [Self](float P) { Self->Sfx(TEXT("UiClick"), P); });
		ZTextC(Out, G, L + 7, TEXT("CHANGES APPLY RIGHT AWAY"), Font(1, 20.f), FVector2D(RX + RW * 0.5f, TopY + 748.f), Mag());
		return L + 12;
	}
};

namespace ZoogiFront
{
	static TSharedPtr<SZoogiRoyalePause> GPause;
	static TWeakObjectPtr<UGameViewportClient> GPauseVC;
	void OpenRoyalePause(UWorld* World)
	{
		if (GPause.IsValid() || !World) return;
		UGameViewportClient* VC = World->GetGameViewport();
		if (!VC) return;
		GPause = SNew(SZoogiRoyalePause);
		GPause->World = World;
		GPauseVC = VC;
		VC->AddViewportWidgetContent(GPause.ToSharedRef(), 200);
		ZoogiApplePad::Want(false);
		UGameplayStatics::SetGamePaused(World, true);
		ZoogiUtil::PlaySfx(World, TEXT("UiClick"), 0.8f, 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("ROYALE pause panel open"));
	}
	void CloseRoyalePause(bool bResume)
	{
		if (!GPause.IsValid()) return;
		UWorld* W = GPause->World.Get();
		if (UGameViewportClient* VC = GPauseVC.Get()) VC->RemoveViewportWidgetContent(GPause.ToSharedRef());
		GPause.Reset();
		if (W && bResume) UGameplayStatics::SetGamePaused(W, false);
		UE_LOG(LogZoogi, Log, TEXT("ROYALE pause panel closed"));
	}
	bool RoyalePauseOpen() { return GPause.IsValid(); }
}
