// Oct 27: live-tunable numbers (see ZoogiTune.h).
#include "ZoogiTune.h"
#include "ZoogiDevConfig.h"
#include "Dom/JsonValue.h"

namespace
{
	const FString Scope = TEXT("global");
	FString StoreKey(const TCHAR* Key) { return FString(TEXT("tune.")) + Key; }
}

const TArray<ZoogiTune::FParam>& ZoogiTune::Params()
{
	// Key, group, label, default, min, max, step. Stick / button offsets are reference pixels (1080-high screen).
	static const TArray<FParam> P = {
		{ TEXT("dev.x"),        TEXT("DEV BUTTON"),    TEXT("LEFT / RIGHT"),     0.f, -600.f,  300.f, 10.f },
		{ TEXT("dev.y"),        TEXT("DEV BUTTON"),    TEXT("UP / DOWN"),        0.f, -300.f,  600.f, 10.f },
		{ TEXT("dev.size"),     TEXT("DEV BUTTON"),    TEXT("SIZE"),             1.f,   0.6f,   1.6f, 0.05f },
		{ TEXT("dev.alpha"),    TEXT("DEV BUTTON"),    TEXT("OPACITY"),        0.9f,   0.2f,    1.f, 0.05f },
		{ TEXT("lstick.x"),     TEXT("LEFT STICK"),    TEXT("LEFT / RIGHT"),     0.f, -200.f,  600.f, 10.f },
		{ TEXT("lstick.y"),     TEXT("LEFT STICK"),    TEXT("UP / DOWN"),        0.f, -500.f,  150.f, 10.f },
		{ TEXT("lstick.size"),  TEXT("LEFT STICK"),    TEXT("SIZE"),             1.f,   0.5f,   1.6f, 0.05f },
		{ TEXT("lstick.alpha"), TEXT("LEFT STICK"),    TEXT("OPACITY"),        0.9f,   0.2f,    1.f, 0.05f },
		{ TEXT("lstick.sens"),  TEXT("LEFT STICK"),    TEXT("SENSITIVITY"),      1.f,   0.3f,    2.f, 0.05f },
		{ TEXT("lstick.dead"),  TEXT("LEFT STICK"),    TEXT("DEAD ZONE"),      0.12f,    0.f,   0.4f, 0.02f },
		{ TEXT("lstick.curve"), TEXT("LEFT STICK"),    TEXT("RESPONSE CURVE"),  1.4f,    1.f,    3.f, 0.1f },
		{ TEXT("rstick.x"),     TEXT("RIGHT STICK"),   TEXT("LEFT / RIGHT"),     0.f, -600.f,  200.f, 10.f },
		{ TEXT("rstick.y"),     TEXT("RIGHT STICK"),   TEXT("UP / DOWN"),        0.f, -500.f,  150.f, 10.f },
		{ TEXT("rstick.size"),  TEXT("RIGHT STICK"),   TEXT("SIZE"),             1.f,   0.5f,   1.6f, 0.05f },
		{ TEXT("rstick.alpha"), TEXT("RIGHT STICK"),   TEXT("OPACITY"),        0.9f,   0.2f,    1.f, 0.05f },
		{ TEXT("rstick.dead"),  TEXT("RIGHT STICK"),   TEXT("MIN PULL"),       0.12f,  0.02f,   0.5f, 0.02f },
		{ TEXT("rstick.curve"), TEXT("RIGHT STICK"),   TEXT("POWER CURVE"),      1.f,   0.5f,   2.5f, 0.1f },
		{ TEXT("rstick.max"),   TEXT("RIGHT STICK"),   TEXT("MAX POWER"),        1.f,   0.3f,    1.f, 0.05f },
		{ TEXT("roam.speed"),   TEXT("ROLLING"),       TEXT("TOP SPEED"),      900.f,  300.f, 2200.f, 50.f },
		{ TEXT("roam.accel"),   TEXT("ROLLING"),       TEXT("ACCELERATION"),  1700.f,  400.f, 4000.f, 100.f },
		{ TEXT("roam.camturn"), TEXT("ROLLING"),       TEXT("CAMERA TURN"),    150.f,   40.f,  480.f, 10.f },
		{ TEXT("bridge.catch"), TEXT("BRIDGE LAUNCH"), TEXT("CATCH RADIUS"),    60.f,   20.f,  160.f, 5.f },
		{ TEXT("bridge.settle"),TEXT("BRIDGE LAUNCH"), TEXT("SETTLE TIME"),    0.35f,    0.f,   1.5f, 0.05f },
		{ TEXT("bridge.cone"),  TEXT("BRIDGE LAUNCH"), TEXT("AIM CONE (DEG)"),  30.f,    0.f,   60.f, 5.f },
		{ TEXT("bridge.width"), TEXT("BRIDGE LAUNCH"), TEXT("GATE WIDTH"),     105.f,   50.f,  200.f, 5.f },
	};
	return P;
}

float ZoogiTune::GetIndex(int32 Index)
{
	const TArray<FParam>& P = Params();
	if (!P.IsValidIndex(Index)) return 0.f;
	if (!ZoogiDevGate::IsUnlocked()) return P[Index].Default;
	struct FCache { uint32 Gen = MAX_uint32; TArray<float> V; };
	static FCache C;
	const uint32 Gen = ZoogiDevConfig::Generation();
	if (C.Gen != Gen || C.V.Num() != P.Num())
	{
		C.V.SetNum(P.Num());
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const TSharedPtr<FJsonValue> V = ZoogiDevConfig::Get(Scope, StoreKey(P[i].Key));
			double D = P[i].Default;
			if (V.IsValid()) V->TryGetNumber(D);
			C.V[i] = FMath::Clamp((float)D, P[i].Min, P[i].Max);
		}
		C.Gen = Gen;
	}
	return C.V[Index];
}

float ZoogiTune::Get(const TCHAR* Key)
{
	const TArray<FParam>& P = Params();
	for (int32 i = 0; i < P.Num(); ++i) if (FCString::Strcmp(P[i].Key, Key) == 0) return GetIndex(i);
	return 0.f;
}

void ZoogiTune::Step(int32 Index, float Dir)
{
	const TArray<FParam>& P = Params();
	if (!P.IsValidIndex(Index)) return;
	const float V = FMath::Clamp(FMath::GridSnap(GetIndex(Index) + Dir * P[Index].Step, P[Index].Step), P[Index].Min, P[Index].Max);
	ZoogiDevConfig::Set(Scope, StoreKey(P[Index].Key), MakeShared<FJsonValueNumber>(V), FString::Printf(TEXT("%s %s = %s"), P[Index].Group, P[Index].Label, *FString::SanitizeFloat(V)));
}

void ZoogiTune::ResetGroup(const TCHAR* Group)
{
	for (const FParam& Prm : Params())
		if (FCString::Strcmp(Prm.Group, Group) == 0 && ZoogiDevConfig::Get(Scope, StoreKey(Prm.Key)).IsValid())
			ZoogiDevConfig::Remove(Scope, StoreKey(Prm.Key), FString::Printf(TEXT("%s %s reset"), Prm.Group, Prm.Label));
}

bool ZoogiTune::IsOverridden(int32 Index)
{
	const TArray<FParam>& P = Params();
	return P.IsValidIndex(Index) && ZoogiDevGate::IsUnlocked() && ZoogiDevConfig::Get(Scope, StoreKey(P[Index].Key)).IsValid();
}

FText ZoogiTune::ValueText(int32 Index)
{
	const TArray<FParam>& P = Params();
	if (!P.IsValidIndex(Index)) return FText::GetEmpty();
	const float V = GetIndex(Index);
	const FString Num = P[Index].Step < 1.f ? FString::Printf(TEXT("%.2f"), V) : FString::Printf(TEXT("%.0f"), V);
	return FText::FromString(FString::Printf(TEXT("%s  %s%s"), P[Index].Label, *Num, IsOverridden(Index) ? TEXT(" *") : TEXT("")));
}
