// b164 (Evan Nov 6 13:45 / 14:03): the ANIME DIRECTOR runtime - cues (Content/Data/Anime/AnimeCues.json), matching, the clip player and
// its full-screen layer (intro / exit templates, borders, windows). The PC editor panel is ZoogiAnimeDirector.cpp.
#include "ZoogiAnimeCues.h"
#include "ZoogiWallCine.h" // b232
#include "ZoogiAnimeCuesInt.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiStory.h"
#include "ZoogiTrickShot.h"
#include "ZoogiMusicSubsystem.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "MediaSoundComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/StrongObjectPtr.h"
#include "InputCoreTypes.h"

namespace ZoogiAnimeCues
{
	namespace Int { int32 GSel = -1; bool GEditPortrait = false; bool GDirector = false; bool GEditLayout = false; }
	class SZoogiCueLayer;
	namespace
	{
		TArray<FCue> GCues; bool GLoaded = false; double GDirtyAt = -1.0;
		TSet<FString> GFired; TMap<FString, double> GLastAt;
		struct FPlay
		{
			TStrongObjectPtr<UMediaPlayer> Player; TStrongObjectPtr<UMediaTexture> Tex; TStrongObjectPtr<UFileMediaSource> Src;
			TWeakObjectPtr<UMediaSoundComponent> Snd; TWeakObjectPtr<UWorld> World;
			FCue Cue; bool bOn = false, bStarted = false, bPreview = false, bOut = false; float T = 0.f, OutT = 0.f, Len = 0.f;
		} GP;
		int32 GPrevRound = -1; EZoogiPhase GPrevPhase = EZoogiPhase::Menu; bool GFinalFired = false; TWeakObjectPtr<UWorld> GMatchWorld;
		TSharedPtr<SZoogiCueLayer> GLayer;

		FString ArenaKey(EZoogiArena A)
		{
			switch (A)
			{
			case EZoogiArena::Meadow: return TEXT("meadow"); case EZoogiArena::Frozen: return TEXT("frozen"); case EZoogiArena::Volcanic: return TEXT("volcanic");
			case EZoogiArena::Cosmic: return TEXT("cosmic"); case EZoogiArena::Arabian: return TEXT("arabian"); case EZoogiArena::Tomb: return TEXT("tomb");
			case EZoogiArena::Neon: return TEXT("neon"); case EZoogiArena::Training: return TEXT("training"); case EZoogiArena::Teaser: return TEXT("teaser");
			case EZoogiArena::Ancient: return TEXT("ancient"); default: return TEXT("any");
			}
		}
		AZoogiGameMode* GMOf(UWorld* W) { return W ? Cast<AZoogiGameMode>(W->GetAuthGameMode()) : nullptr; }
		FString ModeOf(AZoogiGameMode* G)
		{
			if (!G) return TEXT("menu");
			if (ZoogiTrickShot::Active()) return TEXT("trickshot");
			if (ZoogiStory::InMission()) return TEXT("story");
			if (G->bRoyaleMode) return TEXT("royale");
			if (G->bSoloTraining) return TEXT("training");
			if (G->bComboMode) return TEXT("battle");
			return TEXT("classic");
		}
		bool Matches(const FCue& C, UWorld* W, const FString& Event, const FString& ZoogiId, int32 Player)
		{
			if (!C.bOn || C.Clip.IsEmpty() || !C.Trigger.Equals(Event, ESearchCase::IgnoreCase)) return false;
			AZoogiGameMode* G = GMOf(W);
			if (C.Mode != TEXT("any") && !C.Mode.Equals(ModeOf(G), ESearchCase::IgnoreCase)) return false;
			if (C.Arena != TEXT("any") && (!G || !G->Arena || !C.Arena.Equals(ArenaKey(G->Arena->GetTheme()), ESearchCase::IgnoreCase))) return false;
			if (C.Zoogi != TEXT("any") && !C.Zoogi.Equals(ZoogiId, ESearchCase::IgnoreCase)) return false;
			if (C.Who != TEXT("any"))
			{
				const AZoogiBall* B = (G && G->Balls.IsValidIndex(Player)) ? G->Balls[Player] : nullptr;
				const bool bHuman = B && !B->bCPU;
				if ((C.Who == TEXT("human")) != bHuman) return false;
			}
			const double Now = FPlatformTime::Seconds();
			if (C.Cooldown > 0.f) if (const double* L = GLastAt.Find(C.Id)) if (Now - *L < C.Cooldown) return false;
			if (C.Rule == TEXT("first") && GFired.Contains(C.Id)) return false;
			if (C.Rule == TEXT("chance") && FMath::FRand() > C.Chance) return false;
			return true;
		}
		TSharedPtr<FJsonObject> RectJ(const FRect01& R)
		{
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetNumberField(TEXT("x"), R.X); O->SetNumberField(TEXT("y"), R.Y); O->SetNumberField(TEXT("w"), R.W); O->SetNumberField(TEXT("h"), R.H); O->SetBoolField(TEXT("full"), R.bFull);
			return O;
		}
		FRect01 RectOf(const TSharedPtr<FJsonObject>& O)
		{
			FRect01 R; if (!O.IsValid()) return R;
			R.X = O->GetNumberField(TEXT("x")); R.Y = O->GetNumberField(TEXT("y")); R.W = O->GetNumberField(TEXT("w")); R.H = O->GetNumberField(TEXT("h")); O->TryGetBoolField(TEXT("full"), R.bFull);
			return R;
		}
		float Ease(float P) { P = FMath::Clamp(P, 0.f, 1.f); return 1.f - FMath::Pow(1.f - P, 3.f); }
		float EaseBack(float P) { P = FMath::Clamp(P, 0.f, 1.f); const float C1 = 1.70158f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(P - 1.f, 3.f) + C1 * FMath::Pow(P - 1.f, 2.f); }
	}

	bool Enabled() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAnimeCues")); return b; }
	TArray<FCue>& Cues() { if (!GLoaded) Load(); return GCues; }
	FString FilePath() { return FPaths::ProjectContentDir() / TEXT("Data/Anime/AnimeCues.json"); }
	void Int::MarkDirty() { GDirtyAt = FPlatformTime::Seconds(); }

	bool Load()
	{
		GLoaded = true; GCues.Reset();
		FString Txt; if (!FFileHelper::LoadFileToString(Txt, *FilePath())) { UE_LOG(LogZoogi, Log, TEXT("ANIME CUES: no %s (built-in cut-ins only)"), *FilePath()); return false; }
		TSharedPtr<FJsonObject> Root; TSharedRef<TJsonReader<>> Rd = TJsonReaderFactory<>::Create(Txt);
		if (!FJsonSerializer::Deserialize(Rd, Root) || !Root.IsValid()) { UE_LOG(LogZoogi, Warning, TEXT("ANIME CUES: %s does not parse"), *FilePath()); return false; }
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root->TryGetArrayField(TEXT("cues"), Arr))
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O.IsValid()) continue;
				FCue C;
				O->TryGetStringField(TEXT("id"), C.Id); O->TryGetStringField(TEXT("clip"), C.Clip); O->TryGetBoolField(TEXT("on"), C.bOn);
				O->TryGetStringField(TEXT("trigger"), C.Trigger); O->TryGetStringField(TEXT("mode"), C.Mode); O->TryGetStringField(TEXT("arena"), C.Arena);
				O->TryGetStringField(TEXT("zoogi"), C.Zoogi); O->TryGetStringField(TEXT("who"), C.Who); O->TryGetStringField(TEXT("rule"), C.Rule);
				double D = 0.0;
				if (O->TryGetNumberField(TEXT("chance"), D)) C.Chance = D; if (O->TryGetNumberField(TEXT("cooldown"), D)) C.Cooldown = D;
				O->TryGetStringField(TEXT("scene"), C.Scene); if (O->TryGetNumberField(TEXT("line"), D)) C.Line = (int32)D;
				const TSharedPtr<FJsonObject>* RO = nullptr;
				if (O->TryGetObjectField(TEXT("land"), RO)) C.Land = RectOf(*RO); if (O->TryGetObjectField(TEXT("port"), RO)) C.Port = RectOf(*RO);
				O->TryGetStringField(TEXT("border"), C.Border); O->TryGetStringField(TEXT("intro"), C.Intro); O->TryGetStringField(TEXT("outro"), C.Outro);
				if (O->TryGetNumberField(TEXT("introSec"), D)) C.IntroSec = D; if (O->TryGetNumberField(TEXT("outroSec"), D)) C.OutroSec = D;
				O->TryGetStringField(TEXT("game"), C.Game);
				if (O->TryGetNumberField(TEXT("slow"), D)) C.Slow = D; if (O->TryGetNumberField(TEXT("duck"), D)) C.Duck = D; if (O->TryGetNumberField(TEXT("skipAfter"), D)) C.SkipAfter = D;
				if (O->TryGetNumberField(TEXT("volume"), D)) C.Volume = D; if (O->TryGetNumberField(TEXT("trimStart"), D)) C.TrimStart = D; if (O->TryGetNumberField(TEXT("trimEnd"), D)) C.TrimEnd = D;
				if (C.Id.IsEmpty()) C.Id = FString::Printf(TEXT("cue_%d"), GCues.Num() + 1);
				GCues.Add(C);
			}
		UE_LOG(LogZoogi, Log, TEXT("ANIME CUES: %d cues from %s"), GCues.Num(), *FilePath());
		return true;
	}
	bool Save()
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("version"), 1);
		Root->SetStringField(TEXT("note"), TEXT("Zoogi Roll anime cues - edited in the PC game (Dev Mode > ANIME DIRECTOR / F6). The phone build reads this file."));
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FCue& C : GCues)
		{
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), C.Id); O->SetStringField(TEXT("clip"), C.Clip); O->SetBoolField(TEXT("on"), C.bOn);
			O->SetStringField(TEXT("trigger"), C.Trigger); O->SetStringField(TEXT("mode"), C.Mode); O->SetStringField(TEXT("arena"), C.Arena);
			O->SetStringField(TEXT("zoogi"), C.Zoogi); O->SetStringField(TEXT("who"), C.Who); O->SetStringField(TEXT("rule"), C.Rule);
			O->SetNumberField(TEXT("chance"), C.Chance); O->SetNumberField(TEXT("cooldown"), C.Cooldown);
			O->SetStringField(TEXT("scene"), C.Scene); O->SetNumberField(TEXT("line"), C.Line);
			O->SetObjectField(TEXT("land"), RectJ(C.Land)); O->SetObjectField(TEXT("port"), RectJ(C.Port));
			O->SetStringField(TEXT("border"), C.Border); O->SetStringField(TEXT("intro"), C.Intro); O->SetStringField(TEXT("outro"), C.Outro);
			O->SetNumberField(TEXT("introSec"), C.IntroSec); O->SetNumberField(TEXT("outroSec"), C.OutroSec);
			O->SetStringField(TEXT("game"), C.Game); O->SetNumberField(TEXT("slow"), C.Slow); O->SetNumberField(TEXT("duck"), C.Duck);
			O->SetNumberField(TEXT("skipAfter"), C.SkipAfter); O->SetNumberField(TEXT("volume"), C.Volume);
			O->SetNumberField(TEXT("trimStart"), C.TrimStart); O->SetNumberField(TEXT("trimEnd"), C.TrimEnd);
			Arr.Add(MakeShared<FJsonValueObject>(O));
		}
		Root->SetArrayField(TEXT("cues"), Arr);
		FString Out; TSharedRef<TJsonWriter<>> Wr = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Wr);
		const bool bOk = FFileHelper::SaveStringToFile(Out, *FilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogZoogi, Log, TEXT("ANIME CUES: saved %d cues -> %s (%s)"), GCues.Num(), *FilePath(), bOk ? TEXT("ok") : TEXT("FAILED"));
		GDirtyAt = -1.0;
		return bOk;
	}
	TArray<FString> Clips()
	{
		TArray<FString> F; IFileManager::Get().FindFiles(F, *(FPaths::ProjectContentDir() / TEXT("Anime/Movies/*.mp4")), true, false);
		for (FString& S : F) S = FPaths::GetBaseFilename(S);
		F.Sort(); return F;
	}
	const TArray<FString>& Triggers()
	{
		static const TArray<FString> T = { TEXT("KO"), TEXT("Critical"), TEXT("Combo"), TEXT("PoweredHit"), TEXT("GuardBlock"), TEXT("Guard"), TEXT("Infuse"), TEXT("Miss"),
			TEXT("Ultimate"), TEXT("Showtime"), TEXT("RoundStart"), TEXT("FinalRound"), TEXT("MatchWin"), TEXT("MatchLose"), TEXT("BossAttack"), TEXT("TrickShot3Star"), TEXT("Story") };
		return T;
	}
	const TArray<FString>& Templates(bool bExit)
	{
		static const TArray<FString> In = { TEXT("none"), TEXT("fade"), TEXT("zoom"), TEXT("slide_left"), TEXT("slide_right"), TEXT("slide_up"), TEXT("slide_down"), TEXT("wipe"), TEXT("p5slash"), TEXT("comic") };
		static const TArray<FString> Out = { TEXT("none"), TEXT("fade"), TEXT("zoom"), TEXT("slide_left"), TEXT("slide_right"), TEXT("slide_up"), TEXT("slide_down"), TEXT("wipe"), TEXT("p5slash"), TEXT("comic"), TEXT("shatter") };
		return bExit ? Out : In;
	}

	// ------------------------------------------------------------------ player
	static void StopSound() { if (UMediaSoundComponent* S = GP.Snd.Get()) { S->Stop(); S->DestroyComponent(); } GP.Snd.Reset(); }
	void Stop()
	{
		if (GP.Player.IsValid()) GP.Player->Close();
		StopSound();
		if (GP.bOn) UE_LOG(LogZoogi, Log, TEXT("ANIME CUE end: %s (%s) at %.1fs"), *GP.Cue.Id, *GP.Cue.Clip, GP.T);
		GP.bOn = GP.bStarted = GP.bOut = false;
	}
	bool Play(UWorld* W, const FCue& C, bool bPreview)
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Anime/Movies") / (C.Clip + TEXT(".mp4"));
		if (!FPaths::FileExists(Path)) { UE_LOG(LogZoogi, Warning, TEXT("ANIME CUE %s: clip %s missing"), *C.Id, *C.Clip); return false; }
		Stop();
		Int::EnsureLayer();
		if (!GP.Player.IsValid())
		{
			GP.Player.Reset(NewObject<UMediaPlayer>()); GP.Player->PlayOnOpen = true; GP.Player->SetLooping(false);
			GP.Tex.Reset(NewObject<UMediaTexture>()); GP.Tex->AutoClear = true; GP.Tex->SetMediaPlayer(GP.Player.Get()); GP.Tex->UpdateResource();
			GP.Src.Reset(NewObject<UFileMediaSource>());
		}
		if (W)
		{
			AActor* Owner = W->GetAuthGameMode(); if (!Owner) Owner = W->GetFirstPlayerController();
			if (Owner)
			{
				UMediaSoundComponent* S = NewObject<UMediaSoundComponent>(Owner);
				S->bIsUISound = true; S->bAllowSpatialization = false; S->SetMediaPlayer(GP.Player.Get()); S->RegisterComponent();
				S->SetVolumeMultiplier(FMath::Clamp(C.Volume, 0.f, 2.f) * FMath::Max(0.05f, ZoogiUtil::GetSfxVolume())); S->Start();
				GP.Snd = S;
			}
		}
		GP.Src->SetFilePath(Path);
		if (!GP.Player->OpenSource(GP.Src.Get())) { Stop(); return false; }
		GP.Cue = C; GP.bOn = true; GP.bStarted = false; GP.bPreview = bPreview; GP.bOut = false; GP.T = 0.f; GP.OutT = 0.f; GP.Len = 0.f; GP.World = W;
		if (!bPreview) { GFired.Add(C.Id); GLastAt.Add(C.Id, FPlatformTime::Seconds()); }
		if (W) if (UZoogiMusicSubsystem* M = W->GetSubsystem<UZoogiMusicSubsystem>()) M->DuckFor(12.f, FMath::Clamp(C.Duck, 0.f, 1.f));
		UE_LOG(LogZoogi, Log, TEXT("ANIME CUE play: %s (%s) trigger %s, %s %s, game %s%s"), *C.Id, *C.Clip, *C.Trigger, *C.Intro, *C.Outro, *C.Game, bPreview ? TEXT(" [preview]") : TEXT(""));
		return true;
	}
	bool Playing() { return GP.bOn; }
	bool Busy() { return GP.bOn && GP.Cue.Game != TEXT("play"); }
	float WantDilation() { if (!GP.bOn || GP.bPreview) return 0.f; return GP.Cue.Game == TEXT("pause") ? 0.0001f : GP.Cue.Game == TEXT("slow") ? FMath::Clamp(GP.Cue.Slow, 0.02f, 1.f) : 0.f; }
	void Skip() { if (GP.bOn && !GP.bOut && GP.T >= GP.Cue.SkipAfter) { GP.bOut = true; GP.OutT = 0.f; } }
	static void TickPlayer(float Dt)
	{
		if (!GP.bOn) return;
		GP.T += Dt;
		UMediaPlayer* P = GP.Player.Get(); if (!P) { Stop(); return; }
		if (!GP.bStarted && P->IsPlaying())
		{
			GP.bStarted = true; GP.Len = (float)P->GetDuration().GetTotalSeconds();
			if (GP.Cue.TrimStart > 0.01f) P->Seek(FTimespan::FromSeconds(GP.Cue.TrimStart));
		}
		if (!GP.bStarted) { if (GP.T > 6.f) Stop(); return; }
		const float End = (GP.Len > 0.f ? GP.Len : 30.f) - FMath::Max(0.f, GP.Cue.TrimEnd);
		const float Now = (float)P->GetTime().GetTotalSeconds();
		if (!GP.bOut && (Now >= End - FMath::Max(0.05f, GP.Cue.OutroSec) || !P->IsReady() || GP.T > 600.f)) { GP.bOut = true; GP.OutT = 0.f; }
		if (GP.bOut) { GP.OutT += Dt; if (GP.OutT >= FMath::Max(0.01f, GP.Cue.Outro == TEXT("none") ? 0.01f : GP.Cue.OutroSec)) { P->Pause(); Stop(); if (UWorld* W = GP.World.Get()) if (UZoogiMusicSubsystem* M = W->GetSubsystem<UZoogiMusicSubsystem>()) M->DuckFor(0.1f, 1.f); } }
	}

	// ------------------------------------------------------------------ geometry
	FBox2D Int::PhoneFrame(const FVector2D& Size)
	{
		const float H = Size.Y * 0.92f, W = H * 9.f / 19.5f; const FVector2D C = Size * 0.5f - FVector2D(Int::GDirector ? Size.X * 0.15f : 0.f, 0.f);
		return FBox2D(C - FVector2D(W, H) * 0.5f, C + FVector2D(W, H) * 0.5f);
	}
	FBox2D Int::WindowRect(const FRect01& R, const FVector2D& Size, bool bPortrait)
	{
		const FBox2D Area = bPortrait && Size.X > Size.Y ? PhoneFrame(Size) : FBox2D(FVector2D::ZeroVector, Size);
		const FVector2D A = Area.GetSize();
		if (R.bFull) return Area;
		const FVector2D C = Area.Min + FVector2D(R.X * A.X, R.Y * A.Y), S(FMath::Max(0.02f, R.W) * A.X, FMath::Max(0.02f, R.H) * A.Y);
		return FBox2D(C - S * 0.5f, C + S * 0.5f);
	}

	// ------------------------------------------------------------------ the layer
	class SZoogiCueLayer : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiCueLayer) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			SetVisibility(TAttribute<EVisibility>::CreateLambda([]() { return (GP.bOn && !GP.bPreview && GP.Cue.Game != TEXT("play")) || (Int::GDirector && GLayoutDrag) ? EVisibility::Visible : (Int::GDirector && Int::GEditLayout && Int::GSel >= 0 ? EVisibility::Visible : EVisibility::HitTestInvisible); }));
		}
		static inline bool GLayoutDrag = false;
		int32 DragMode = 0; FVector2D DragFrom; FRect01 DragStart;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }
		virtual void Tick(const FGeometry& G, const double Now, const float Dt) override
		{
			TickPlayer(FMath::Min(Dt, 0.1f));
			if (GDirtyAt > 0.0 && FPlatformTime::Seconds() - GDirtyAt > 1.0) Save(); // director autosave
		}
		FRect01* SelRect() const
		{
			TArray<FCue>& C = Cues();
			if (!Int::GDirector || !C.IsValidIndex(Int::GSel)) return nullptr;
			return Int::GEditPortrait ? &C[Int::GSel].Port : &C[Int::GSel].Land;
		}
		bool InDirectorArea(const FVector2D& P, const FVector2D& Size) const { return !Int::GDirector || P.X < Size.X * 0.66f; }
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
		{
			const FVector2D Size = G.GetLocalSize(), P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
			if (FRect01* R = SelRect(); R && !R->bFull && InDirectorArea(P, Size))
			{
				const FBox2D B = Int::WindowRect(*R, Size, Int::GEditPortrait);
				const bool bCorner = FVector2D::Distance(P, B.Max) < 28.f;
				if (bCorner || B.IsInside(P)) { DragMode = bCorner ? 2 : 1; DragFrom = P; DragStart = *R; GLayoutDrag = true; return FReply::Handled().CaptureMouse(SharedThis(this)); }
			}
			if (GP.bOn && !GP.bPreview) { Skip(); return FReply::Handled(); }
			return FReply::Unhandled();
		}
		virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
		{
			if (!DragMode) return FReply::Unhandled();
			FRect01* R = SelRect(); if (!R) return FReply::Unhandled();
			const FVector2D Size = G.GetLocalSize(), P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
			const FBox2D Area = Int::GEditPortrait && Size.X > Size.Y ? Int::PhoneFrame(Size) : FBox2D(FVector2D::ZeroVector, Size);
			const FVector2D D = (P - DragFrom) / Area.GetSize();
			if (DragMode == 1) { R->X = FMath::Clamp(DragStart.X + D.X, 0.f, 1.f); R->Y = FMath::Clamp(DragStart.Y + D.Y, 0.f, 1.f); }
			else { R->W = FMath::Clamp(DragStart.W + D.X * 2.f, 0.05f, 1.2f); R->H = FMath::Clamp(DragStart.H + D.Y * 2.f, 0.05f, 1.2f); }
			Int::MarkDirty();
			return FReply::Handled();
		}
		virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override
		{
			if (!DragMode) return FReply::Unhandled();
			DragMode = 0; GLayoutDrag = false; return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { if (GP.bOn && !GP.bPreview) { Skip(); return FReply::Handled(); } return FReply::Unhandled(); }

		static void Rect(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FBox2D& B, const FLinearColor& Col, float Ang = 0.f, const FSlateBrush* Br = nullptr)
		{
			static const FSlateColorBrush White(FLinearColor::White);
			const FVector2D S = B.GetSize();
			const FGeometry C = G.MakeChild(S, FSlateLayoutTransform(B.Min), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Ang))), FVector2D(0.5f, 0.5f));
			FSlateDrawElement::MakeBox(Out, L, C.ToPaintGeometry(), Br ? Br : &White, ESlateDrawEffect::None, Col);
		}
		static void Frame(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FBox2D& B, float T, const FLinearColor& Col)
		{
			Rect(Out, G, L, FBox2D(B.Min - FVector2D(T, T), FVector2D(B.Max.X + T, B.Min.Y)), Col); Rect(Out, G, L, FBox2D(FVector2D(B.Min.X - T, B.Max.Y), B.Max + FVector2D(T, T)), Col);
			Rect(Out, G, L, FBox2D(FVector2D(B.Min.X - T, B.Min.Y), FVector2D(B.Min.X, B.Max.Y)), Col); Rect(Out, G, L, FBox2D(FVector2D(B.Max.X, B.Min.Y), FVector2D(B.Max.X + T, B.Max.Y)), Col);
		}
		void PaintClip(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
		{
			const FVector2D Size = G.GetLocalSize();
			const bool bPortrait = Size.X < Size.Y || (GP.bPreview && Int::GEditPortrait);
			const FCue& C = GP.Cue;
			FBox2D B = Int::WindowRect(bPortrait ? C.Port : C.Land, Size, bPortrait);
			UMediaTexture* Tx = GP.Tex.Get();
			// progress of the intro / exit
			const bool bOut = GP.bOut;
			const FString& Tpl = bOut ? C.Outro : C.Intro;
			const float Sec = FMath::Max(0.01f, bOut ? C.OutroSec : C.IntroSec);
			float P = bOut ? 1.f - FMath::Clamp(GP.OutT / Sec, 0.f, 1.f) : FMath::Clamp(GP.T / Sec, 0.f, 1.f); // 0 hidden .. 1 shown
			if (Tpl == TEXT("none")) P = 1.f;
			float Alpha = 1.f, Scale = 1.f, Ang = 0.f; FVector2D Off(0.f, 0.f); float Reveal = 1.f;
			const FVector2D S = B.GetSize();
			if (Tpl == TEXT("fade")) Alpha = Ease(P);
			else if (Tpl == TEXT("zoom")) { Scale = FMath::Lerp(0.55f, 1.f, EaseBack(P)); Alpha = FMath::Clamp(P * 2.f, 0.f, 1.f); }
			else if (Tpl == TEXT("slide_left")) Off.X = -(B.Max.X + 40.f) * (1.f - Ease(P));
			else if (Tpl == TEXT("slide_right")) Off.X = (Size.X - B.Min.X + 40.f) * (1.f - Ease(P));
			else if (Tpl == TEXT("slide_up")) Off.Y = -(B.Max.Y + 40.f) * (1.f - Ease(P));
			else if (Tpl == TEXT("slide_down")) Off.Y = (Size.Y - B.Min.Y + 40.f) * (1.f - Ease(P));
			else if (Tpl == TEXT("wipe") || Tpl == TEXT("p5slash")) Reveal = Ease(P);
			else if (Tpl == TEXT("comic")) { Scale = FMath::Lerp(0.3f, 1.f, EaseBack(P)); Ang = -8.f * (1.f - Ease(P)); Alpha = FMath::Clamp(P * 3.f, 0.f, 1.f); }
			const FVector2D Ctr = B.GetCenter() + Off; const FVector2D Sz = S * Scale;
			FBox2D D(Ctr - Sz * 0.5f, Ctr + Sz * 0.5f);
			const float BorderAng = C.Border == TEXT("comic") ? -2.f : 0.f;
			// border under the picture
			if (Tpl != TEXT("shatter") || !bOut)
			{
				const float A = Alpha * (Reveal > 0.01f ? 1.f : 0.f);
				if (C.Border == TEXT("p5")) { Rect(Out, G, L, FBox2D(D.Min + FVector2D(10.f, 12.f), D.Max + FVector2D(18.f, 20.f)), FLinearColor(0.9f, 0.05f, 0.2f, A), Ang + BorderAng); Rect(Out, G, L + 1, FBox2D(D.Min - FVector2D(8.f, 8.f), D.Max + FVector2D(8.f, 8.f)), FLinearColor(0.f, 0.f, 0.f, A), Ang + BorderAng); }
				else if (C.Border == TEXT("thin")) Rect(Out, G, L + 1, FBox2D(D.Min - FVector2D(3.f, 3.f), D.Max + FVector2D(3.f, 3.f)), FLinearColor(1.f, 1.f, 1.f, A), Ang);
				else if (C.Border == TEXT("comic")) { Rect(Out, G, L, FBox2D(D.Min - FVector2D(11.f, 11.f), D.Max + FVector2D(11.f, 11.f)), FLinearColor(0.f, 0.f, 0.f, A), Ang + BorderAng); Rect(Out, G, L + 1, FBox2D(D.Min - FVector2D(8.f, 8.f), D.Max + FVector2D(8.f, 8.f)), FLinearColor(1.f, 1.f, 1.f, A), Ang + BorderAng); }
				else if (C.Land.bFull || C.Port.bFull) Rect(Out, G, L + 1, D, FLinearColor(0.f, 0.f, 0.f, A), Ang);
			}
			if (!Tx || !GP.bStarted) return;
			FSlateBrush Br; Br.SetResourceObject(Tx); Br.ImageSize = Sz;
			const FLinearColor Tint(1.f, 1.f, 1.f, Alpha);
			if (bOut && Tpl == TEXT("shatter"))
			{	// the picture breaks into 6 x 4 pieces that fly out
				const float K = 1.f - P; const int32 NX = 6, NY = 4;
				for (int32 j = 0; j < NY; ++j) for (int32 i = 0; i < NX; ++i)
				{
					FSlateBrush Pc = Br; Pc.SetUVRegion(FBox2f(FVector2f((float)i / NX, (float)j / NY), FVector2f((float)(i + 1) / NX, (float)(j + 1) / NY)));
					const FVector2D TS(Sz.X / NX, Sz.Y / NY), TC = D.Min + FVector2D((i + 0.5f) * TS.X, (j + 0.5f) * TS.Y);
					const FVector2D Dir = (TC - D.GetCenter()).GetSafeNormal() + FVector2D(0.f, 0.35f);
					const uint32 Hh = (uint32)(i * 7919 + j * 104729); const float Spin = ((Hh % 200) / 100.f - 1.f) * 260.f;
					const FVector2D Pos = TC + Dir * (K * K * 900.f) + FVector2D(0.f, K * K * 600.f);
					Rect(Out, G, L + 2, FBox2D(Pos - TS * 0.5f * (1.f - 0.3f * K), Pos + TS * 0.5f * (1.f - 0.3f * K)), FLinearColor(1.f, 1.f, 1.f, 1.f - K), Spin * K, &Pc);
				}
				return;
			}
			if (Reveal < 0.999f)
			{	// wipe / P5 slash: only part of the window shows, from the left (slash: a slanted black + red bar rides the edge)
				const float Xe = FMath::Lerp(D.Min.X - (Tpl == TEXT("p5slash") ? Sz.Y * 0.35f : 0.f), D.Max.X + (Tpl == TEXT("p5slash") ? Sz.Y * 0.35f : 0.f), Reveal);
				if (Tpl == TEXT("wipe"))
				{
					const FVector2D C2 = D.GetCenter(), H = Sz * 0.5f * Reveal;
					Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2f(H * 2.f), FSlateLayoutTransform(FVector2f(C2 - H)))));
					Rect(Out, G, L + 2, D, Tint, Ang, &Br); Out.PopClip(); return;
				}
				Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2f(FMath::Max(1.f, Xe - D.Min.X), Sz.Y + 40.f), FSlateLayoutTransform(FVector2f(D.Min.X, D.Min.Y - 20.f)))));
				Rect(Out, G, L + 2, D, Tint, Ang, &Br); Out.PopClip();
				const FBox2D Bar(FVector2D(Xe - 22.f, D.Min.Y - Sz.Y * 0.2f), FVector2D(Xe + 22.f, D.Max.Y + Sz.Y * 0.2f));
				Rect(Out, G, L + 3, Bar, FLinearColor(0.f, 0.f, 0.f, 1.f), 18.f);
				Rect(Out, G, L + 4, FBox2D(Bar.Min + FVector2D(26.f, 0.f), Bar.Max + FVector2D(14.f, 0.f)), FLinearColor(0.9f, 0.05f, 0.25f, 1.f), 18.f);
				return;
			}
			Rect(Out, G, L + 2, D, Tint, Ang + BorderAng * 0.f, &Br);
			if (Tpl == TEXT("comic") && P < 0.6f) Rect(Out, G, L + 3, D, FLinearColor(1.f, 1.f, 1.f, (0.6f - P) * 1.4f), Ang);
		}
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 L, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp()) { return L; } // b232: the wall-smash cinematic plays on a clean screen
			const FVector2D Size = G.GetLocalSize();
			if (GP.bOn) PaintClip(Out, G, L);
			// director: the phone frame + the selected cue's window with its handle
			if (Int::GDirector)
			{
				if (Int::GEditPortrait && Size.X > Size.Y)
				{
					const FBox2D F = Int::PhoneFrame(Size);
					Frame(Out, G, L + 6, F, 6.f, FLinearColor(1.f, 0.85f, 0.2f, 0.9f));
				}
				TArray<FCue>& C = Cues();
				if (C.IsValidIndex(Int::GSel))
				{
					const FRect01& R = Int::GEditPortrait ? C[Int::GSel].Port : C[Int::GSel].Land;
					const FBox2D B = Int::WindowRect(R, Size, Int::GEditPortrait);
					if (!GP.bOn) Rect(Out, G, L + 5, B, FLinearColor(0.9f, 0.05f, 0.45f, 0.18f));
					Frame(Out, G, L + 7, B, 3.f, FLinearColor(0.95f, 0.1f, 0.5f, 1.f));
					if (!R.bFull) Rect(Out, G, L + 8, FBox2D(B.Max - FVector2D(14.f, 14.f), B.Max + FVector2D(14.f, 14.f)), FLinearColor(1.f, 1.f, 1.f, 1.f));
				}
			}
			return L + 9;
		}
	};
	void Int::EnsureLayer()
	{
		if (GLayer.IsValid() || !GEngine || !GEngine->GameViewport) return;
		GLayer = SNew(SZoogiCueLayer);
		GEngine->GameViewport->AddViewportWidgetContent(GLayer.ToSharedRef(), 96);
	}

	// ------------------------------------------------------------------ triggers
	bool OnEvent(UWorld* W, const FString& Event, const FString& ZoogiId, int32 Player)
	{
		if (!Enabled() || GP.bOn) return false;
		for (const FCue& C : Cues()) if (Matches(C, W, Event, ZoogiId, Player)) return Play(W, C, false);
		return false;
	}
	bool Fire(UWorld* W, const FString& Event, int32 Player) { return OnEvent(W, Event, FString(), Player); }
	bool OnStoryLine(UWorld* W, const FString& Scene, int32 Line)
	{
		if (!Enabled()) return false;
		for (const FCue& C : Cues())
			if (C.bOn && C.Trigger == TEXT("Story") && C.Scene.Equals(Scene, ESearchCase::IgnoreCase) && C.Line == Line && !C.Clip.IsEmpty())
				return Play(W, C, false);
		return false;
	}
	bool StoryOwnsFilms() { if (!Enabled()) return false; for (const FCue& C : Cues()) if (C.Trigger == TEXT("Story")) return true; return false; }
#if PLATFORM_DESKTOP
	class FZoogiF6Pre : public IInputProcessor
	{
	public:
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
		{
			if (E.GetKey() != EKeys::F6 || E.IsRepeat()) return false;
			ToggleDirector();
			UE_LOG(LogZoogi, Log, TEXT("ANIME DIRECTOR: F6 -> %s"), DirectorOpen() ? TEXT("open") : TEXT("closed"));
			return true;
		}
		virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiF6"); }
	};
#endif
	void EnsureF6Hook()
	{
#if PLATFORM_DESKTOP
		static bool bDone = false;
		if (bDone || !FSlateApplication::IsInitialized()) return;
		bDone = true;
		FSlateApplication::Get().RegisterInputPreProcessor(MakeShared<FZoogiF6Pre>());
#endif
	}
	void ResetMatch() { GFired.Reset(); GLastAt.Reset(); GPrevRound = -1; GFinalFired = false; }
	void Tick(AZoogiGameMode* G, float Dt)
	{
		EnsureF6Hook();
		if (!Enabled() || !G) return;
		UWorld* W = G->GetWorld();
		if (GMatchWorld.Get() != W) { GMatchWorld = W; ResetMatch(); GPrevPhase = G->Phase; }
		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		{
			if (GP.bOn && !GP.bPreview && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Escape))) Skip();
		}
		if (G->Phase == EZoogiPhase::Menu) { GPrevPhase = G->Phase; return; }
		if (G->RoundNumber != GPrevRound && G->Phase == EZoogiPhase::Aiming)
		{
			GPrevRound = G->RoundNumber;
			if (G->MenuRounds > 1 && G->RoundNumber >= G->MenuRounds && !GFinalFired) { GFinalFired = true; if (!Fire(W, TEXT("FinalRound"))) Fire(W, TEXT("RoundStart")); }
			else Fire(W, TEXT("RoundStart"));
		}
		if (G->Phase == EZoogiPhase::GameOver && GPrevPhase != EZoogiPhase::GameOver)
		{
			int32 Best = -1, BestS = -1; bool bHumanWin = false;
			for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) { const int32 S = G->Balls[i]->Score; if (S > BestS) { BestS = S; Best = i; } }
			bHumanWin = G->Balls.IsValidIndex(Best) && G->Balls[Best] && !G->Balls[Best]->bCPU;
			Fire(W, bHumanWin ? TEXT("MatchWin") : TEXT("MatchLose"), Best);
		}
		GPrevPhase = G->Phase;
	}
}
