#pragma once
// Oct 27 (Evan's Build 9 corrections): live-tunable numbers edited from the Dev Mode HUD tab - joystick layout / feel, the DEV
// button, Frozen Ring rolling and bridge-launch values. Ordinary players get the shipped defaults below; a dev-unlocked device
// reads its overrides from the dev store (ZoogiDevConfig, scope "global", key "tune.<key>"), so they survive restarts and go out
// with EXPORT for baking.
#include "CoreMinimal.h"

namespace ZoogiTune
{
	struct FParam
	{
		const TCHAR* Key;
		const TCHAR* Group;
		const TCHAR* Label;
		float Default, Min, Max, Step;
	};
	ZOOGIROLL_API const TArray<FParam>& Params();
	/** Current value (shipped default unless the device is dev-unlocked and has an override). Unknown keys return 0. */
	ZOOGIROLL_API float Get(const TCHAR* Key);
	ZOOGIROLL_API float GetIndex(int32 Index);
	ZOOGIROLL_API void Step(int32 Index, float Dir);
	ZOOGIROLL_API void ResetGroup(const TCHAR* Group);
	ZOOGIROLL_API FText ValueText(int32 Index);
	ZOOGIROLL_API bool IsOverridden(int32 Index);
}
