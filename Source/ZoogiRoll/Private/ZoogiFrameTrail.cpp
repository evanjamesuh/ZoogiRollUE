// See ZoogiFrameTrail.h.
#include "ZoogiFrameTrail.h"
#include "ZoogiRoll.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiGameMode.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	class SZoogiFrameTrail : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiFrameTrail) {}
		SLATE_END_ARGS()

		TWeakObjectPtr<UWorld> World;
		FZoogiFrameTrailTune T;
		double Start = 0.0;

		void Construct(const FArguments&, UWorld* InWorld)
		{
			World = InWorld;
			T = UZoogiFrameTrail::GetTune();
			Start = FPlatformTime::Seconds();
			SetVisibility(EVisibility::HitTestInvisible);
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([](double, float) { return EActiveTimerReturnType::Continue; }));
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
			if (!L.bValid || !World.IsValid() || !ZoogiFramed::IsActive(World.Get()) || L.Viewport.X < 1.f) return Layer;
			// viewport pixels -> this widget's local (DPI-scaled) space
			const FVector2D K = G.GetLocalSize() / L.Viewport;
			const float In = T.Inset * L.U;
			const FVector2D P0 = (L.Arena.Min + FVector2D(In)) * K, P1 = (L.Arena.Max - FVector2D(In)) * K;
			const FVector2D Sz = P1 - P0;
			if (Sz.X < 4.f || Sz.Y < 4.f) return Layer;
			const float Per = 2.f * (Sz.X + Sz.Y);
			auto At = [&](float D) -> FVector2D // clockwise from the top-left corner
			{
				D = FMath::Fmod(D, Per); if (D < 0.f) D += Per;
				if (D < Sz.X) return P0 + FVector2D(D, 0.f); D -= Sz.X;
				if (D < Sz.Y) return FVector2D(P1.X, P0.Y + D); D -= Sz.Y;
				if (D < Sz.X) return FVector2D(P1.X - D, P1.Y); D -= Sz.X;
				return FVector2D(P0.X, P1.Y - D);
			};
			const float Corners[4] = { 0.f, Sz.X, Sz.X + Sz.Y, 2.f * Sz.X + Sz.Y };
			const double Age = FPlatformTime::Seconds() - Start;
			const float Head = (float)FMath::Fmod(Age / FMath::Max(0.2f, T.LapSeconds), 1.0) * Per;
			const float Tail = FMath::Clamp(T.TailFrac, 0.01f, 0.9f) * Per;

			// tail polyline from the tail end to the head, with the frame corners inserted so the trail bends crisply
			TArray<float> Ds;
			const int32 N = 24;
			for (int32 i = 0; i <= N; ++i) Ds.Add(Head - Tail + Tail * i / N);
			for (float C : Corners) for (int32 Lap = -1; Lap <= 1; ++Lap) { const float Dc = C + Lap * Per; if (Dc > Head - Tail && Dc < Head) Ds.Add(Dc); }
			Ds.Sort();
			TArray<FVector2f> Pts; Pts.Reserve(Ds.Num());
			for (float D : Ds) Pts.Add(FVector2f(At(D)));

			auto Pass = [&](float Width, float Alpha, float Pow)
			{
				TArray<FLinearColor> Cols; Cols.Reserve(Ds.Num());
				for (float D : Ds)
				{
					const float F = FMath::Clamp((D - (Head - Tail)) / Tail, 0.f, 1.f); // 0 tail end .. 1 head
					FLinearColor C = T.Color; C.A = Alpha * T.Brightness * FMath::Pow(F, Pow);
					Cols.Add(C);
				}
				FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Pts, Cols, ESlateDrawEffect::None, FLinearColor::White, true, Width * L.U * K.X);
				++Layer;
			};
			Pass(T.GlowWidth, T.GlowAlpha * 0.5f, 2.5f);   // wide soft halo (fake bloom)
			Pass(T.GlowWidth * 0.45f, T.GlowAlpha, 2.0f);
			Pass(T.CoreWidth, 1.f, 1.4f);                  // bright core

			// hot head: short bright stub with a small flare
			const FVector2f H(At(Head)), Hb(At(Head - FMath::Min(Tail * 0.06f, 14.f * L.U * K.X)));
			TArray<FVector2f> Stub = { Hb, H };
			FLinearColor W = T.Color; W.A = T.Brightness;
			FSlateDrawElement::MakeLines(Out, Layer++, G.ToPaintGeometry(), Stub, ESlateDrawEffect::None, W, true, T.CoreWidth * 2.2f * L.U * K.X);
			FLinearColor Fl = T.Color; Fl.A = 0.25f * T.Brightness;
			FSlateDrawElement::MakeLines(Out, Layer++, G.ToPaintGeometry(), Stub, ESlateDrawEffect::None, Fl, true, T.GlowWidth * 1.3f * L.U * K.X);
			return Layer;
		}
	};
}

FZoogiFrameTrailTune UZoogiFrameTrail::GetTune()
{
	FZoogiFrameTrailTune T;
	if (GConfig)
	{
		const TCHAR* S = TEXT("ZoogiRoll.FrameTrail");
		GConfig->GetFloat(S, TEXT("LapSeconds"), T.LapSeconds, GGameIni);
		GConfig->GetFloat(S, TEXT("TailFrac"), T.TailFrac, GGameIni);
		GConfig->GetFloat(S, TEXT("CoreWidth"), T.CoreWidth, GGameIni);
		GConfig->GetFloat(S, TEXT("GlowWidth"), T.GlowWidth, GGameIni);
		GConfig->GetFloat(S, TEXT("GlowAlpha"), T.GlowAlpha, GGameIni);
		GConfig->GetFloat(S, TEXT("Brightness"), T.Brightness, GGameIni);
		GConfig->GetFloat(S, TEXT("Inset"), T.Inset, GGameIni);
		FString Col; if (GConfig->GetString(S, TEXT("Color"), Col, GGameIni)) T.Color.InitFromString(Col);
	}
	return T;
}

void UZoogiFrameTrail::Show(UWorld* World) { if (UZoogiFrameTrail* S = World ? World->GetSubsystem<UZoogiFrameTrail>() : nullptr) S->DoShow(); }
void UZoogiFrameTrail::Hide(UWorld* World) { if (UZoogiFrameTrail* S = World ? World->GetSubsystem<UZoogiFrameTrail>() : nullptr) S->DoHide(); }

void UZoogiFrameTrail::DoShow()
{
	if (Widget.IsValid() || !GEngine || !GEngine->GameViewport || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	Widget = SNew(SZoogiFrameTrail, GetWorld());
	GEngine->GameViewport->AddViewportWidgetContent(Widget.ToSharedRef(), 50); // above the framed HUD panels
	UE_LOG(LogZoogi, Log, TEXT("FrameTrail: shown (lap %.2f s, tail %.0f%%)"), GetTune().LapSeconds, GetTune().TailFrac * 100.f);
}

void UZoogiFrameTrail::DoHide()
{
	if (Widget.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	Widget.Reset();
}

void UZoogiFrameTrail::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrameTrail"))) return;
	DoShow();
	FString Dir;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), Dir)) InWorld.GetTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateUObject(this, &UZoogiFrameTrail::ShotTick), 0.25f, true, 12.f);
}

void UZoogiFrameTrail::ShotTick()
{
	// 13 captures 0.25 s apart (one full lap at 3 s) once the framed layout is up
	if (!ZoogiFramed::IsActive(GetWorld()) || !ZoogiFramed::Current().bValid) return;
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) G->TakeShot(FString::Printf(TEXT("frametrail_%02d"), Shots));
	if (++Shots >= 13) GetWorld()->GetTimerManager().ClearTimer(ShotTimer);
}

void UZoogiFrameTrail::Deinitialize()
{
	DoHide();
	Super::Deinitialize();
}