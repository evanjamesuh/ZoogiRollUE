# b82b (Evan 09:38 / 09:39): yellow electricity flashes on top of the purple KriptoFX-style stems; a buzz on every tap
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('''		static const bool bOldStems = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltSparkStems"));
		if (bOldStems && GStemT >= 0.3f)''',
  '''		static const bool bNoSparks = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBoltSparks")); // b82 (Evan): the yellow electric flashes stay on top of the purple arcs
		if (!bNoSparks && GStemT >= 0.3f)'''),
])
ed(P + 'ZoogiClassicPad.cpp', [
 ('''void ZoogiClassicPad::Ensure(UWorld* World)
{''',
  '''// b82 (Evan: "add vibration on each tap"): every touch / click press anywhere (menus, buttons, the field) gives a short buzz.
// A Slate input pre-processor sees the press before any widget, so nothing can swallow it. -ZoogiNoTapBuzz = off.
class FZoogiTapBuzz : public IInputProcessor
{
public:
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override
	{
		if (E.IsTouchEvent() || E.GetEffectingButton() == EKeys::LeftMouseButton)
			if (UWorld* W = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr)
				ZoogiHaptics::Kick(0.55f, W, TEXT("tap"));
		return false; // never consumes the tap
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiTapBuzz"); }
};

void ZoogiClassicPad::Ensure(UWorld* World)
{
	{
		static TSharedPtr<FZoogiTapBuzz> GTapBuzz;
		if (!GTapBuzz.IsValid() && FSlateApplication::IsInitialized() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTapBuzz")))
		{
			GTapBuzz = MakeShared<FZoogiTapBuzz>();
			FSlateApplication::Get().RegisterInputPreProcessor(GTapBuzz);
			UE_LOG(LogZoogi, Log, TEXT("TAP BUZZ: every press vibrates"));
		}
	}'''),
], inc='#include "ZoogiHaptics.h"\n#include "Framework/Application/IInputProcessor.h"\n#include "Framework/Application/SlateApplication.h"\n#include "Engine/Engine.h"\n#include "Engine/GameViewportClient.h"', after='#include "ZoogiClassicPad.h"')
print('b82b ok')
