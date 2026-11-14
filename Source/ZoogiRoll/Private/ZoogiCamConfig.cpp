// Oct 27: Camera Configuration Manager (see ZoogiCamConfig.h).
#include "ZoogiCamConfig.h"
#include "ZoogiDevConfig.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoyale.h"
#include "ZoogiRoll.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"

namespace
{
	FString CamScope(const FString& Mode) { return TEXT("mode:") + Mode; }
	FString ProfileKey(TCHAR P) { return FString::Printf(TEXT("camera.profile.%c"), P); }
	TCHAR CleanProfile(TCHAR P) { return (P == TEXT('B') || P == TEXT('C')) ? P : TEXT('A'); }
}

const TArray<ZoogiCamConfig::FParam>& ZoogiCamConfig::Params()
{
	static const TArray<FParam> P = {
		{ TEXT("back"),      TEXT("DISTANCE"),        100.f, 1500.f, 10.f,  true,  &FZoogiCamSettings::Back },
		{ TEXT("up"),        TEXT("HEIGHT"),           30.f, 1200.f, 10.f,  true,  &FZoogiCamSettings::Up },
		{ TEXT("pitch"),     TEXT("TILT DOWN"),       -10.f,   60.f,  1.f,  true,  &FZoogiCamSettings::Pitch },
		{ TEXT("fov"),       TEXT("FIELD OF VIEW"),    60.f,  120.f,  1.f,  true,  &FZoogiCamSettings::Fov },
		{ TEXT("robotBack"), TEXT("ROBOT DISTANCE"),  100.f, 1500.f, 10.f,  true,  &FZoogiCamSettings::RobotBack },
		{ TEXT("robotUp"),   TEXT("ROBOT HEIGHT"),     30.f, 1200.f, 10.f,  true,  &FZoogiCamSettings::RobotUp },
		{ TEXT("follow"),    TEXT("FOLLOW TIGHTNESS"),  4.f,   40.f,  1.f,  true,  &FZoogiCamSettings::Follow },
		{ TEXT("turn"),      TEXT("TURN SPEED"),        4.f,   30.f,  1.f,  true,  &FZoogiCamSettings::Turn },
		{ TEXT("shake"),     TEXT("SHAKE"),             0.f,    2.f, 0.1f,  true,  &FZoogiCamSettings::Shake },
		{ TEXT("tacPitch"),  TEXT("ANGLE"),            10.f,   85.f,  1.f, false,  &FZoogiCamSettings::TacPitch },
		{ TEXT("tacDist"),   TEXT("DISTANCE"),        600.f, 5000.f, 50.f, false,  &FZoogiCamSettings::TacDist },
		{ TEXT("tacFov"),    TEXT("FIELD OF VIEW"),    50.f,  110.f,  1.f, false,  &FZoogiCamSettings::TacFov },
		{ TEXT("tacLag"),    TEXT("SMOOTHING"),       0.5f,   10.f, 0.25f, false, &FZoogiCamSettings::TacLag },
	};
	return P;
}

FString ZoogiCamConfig::ModeName(const UObject* WorldContext)
{
	const AZoogiGameMode* G = WorldContext ? Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(WorldContext)) : nullptr;
	if (!G) return TEXT("classic");
	return G->bRoyaleTraining ? TEXT("training") : (G->bRoyaleMode ? TEXT("royale") : (G->bComboMode ? TEXT("combo") : TEXT("classic")));
}

bool ZoogiCamConfig::IsChaseMode(const FString& Mode) { return Mode == TEXT("royale") || Mode == TEXT("training"); }

FZoogiCamSettings ZoogiCamConfig::ProfileDefaults(TCHAR Profile)
{
	FZoogiCamSettings S; // A: what the game ships with (chase numbers from [ZoogiRoll.Royale], so ini / command-line tuning still works)
	{
		const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
		S.Back = T.ChaseBack; S.Up = T.ChaseUp; S.Pitch = T.ChasePitch; S.Fov = T.ChaseFov; S.RobotBack = T.RobotChaseBack; S.RobotUp = T.RobotChaseUp;
	}
	switch (CleanProfile(Profile))
	{
	case TEXT('B'): // close cinematic
		S.Back = 220.f; S.Up = 95.f; S.Pitch = 0.f; S.Fov = 90.f; S.RobotBack = 380.f; S.RobotUp = 150.f; S.Follow = 32.f; S.Turn = 16.f;
		S.TacPitch = 30.f; S.TacDist = 1400.f; S.TacFov = 70.f; S.TacLag = 3.f;
		break;
	case TEXT('C'): // overhead tactical
		S.Back = 650.f; S.Up = 650.f; S.Pitch = 32.f; S.Fov = 85.f; S.RobotBack = 750.f; S.RobotUp = 700.f; S.Follow = 18.f; S.Turn = 10.f; S.Shake = 0.5f;
		S.TacPitch = 65.f; S.TacDist = 3000.f; S.TacFov = 80.f; S.TacLag = 1.5f;
		break;
	default: break;
	}
	return S;
}

TCHAR ZoogiCamConfig::ActiveProfile(const FString& Mode)
{
	if (!ZoogiDevGate::IsUnlocked()) return TEXT('A');
	const TSharedPtr<FJsonValue> V = ZoogiDevConfig::Get(CamScope(Mode), TEXT("camera.active"));
	FString S;
	return (V.IsValid() && V->TryGetString(S) && S.Len() == 1) ? CleanProfile(S[0]) : TEXT('A');
}

void ZoogiCamConfig::SetActiveProfile(const FString& Mode, TCHAR Profile)
{
	Profile = CleanProfile(Profile);
	ZoogiDevConfig::Set(CamScope(Mode), TEXT("camera.active"), MakeShared<FJsonValueString>(FString::Chr(Profile)), FString::Printf(TEXT("%s camera: profile %c is the default"), *Mode, Profile));
}

float ZoogiCamConfig::GetValue(const FString& Mode, TCHAR Profile, const FParam& P)
{
	FZoogiCamSettings S = ProfileDefaults(Profile);
	float Out = S.*(P.Field);
	const TSharedPtr<FJsonValue> V = ZoogiDevConfig::Get(CamScope(Mode), ProfileKey(CleanProfile(Profile)));
	const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
	double D = 0.0;
	if (O && O->TryGetNumberField(P.Key, D)) Out = (float)D;
	return FMath::Clamp(Out, P.Min, P.Max);
}

void ZoogiCamConfig::SetValue(const FString& Mode, TCHAR Profile, const FParam& P, float Value)
{
	Profile = CleanProfile(Profile);
	Value = FMath::Clamp(FMath::GridSnap(Value, P.Step), P.Min, P.Max);
	const TSharedPtr<FJsonValue> V = ZoogiDevConfig::Get(CamScope(Mode), ProfileKey(Profile));
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	if (V.IsValid() && V->AsObject().IsValid()) O->Values = V->AsObject()->Values; // copy: the old value stays intact for undo
	O->SetNumberField(P.Key, Value);
	ZoogiDevConfig::Set(CamScope(Mode), ProfileKey(Profile), MakeShared<FJsonValueObject>(O),
		FString::Printf(TEXT("%s camera %c: %s = %s"), *Mode, Profile, P.Label, *FString::SanitizeFloat(Value)));
}

void ZoogiCamConfig::ResetProfile(const FString& Mode, TCHAR Profile)
{
	Profile = CleanProfile(Profile);
	ZoogiDevConfig::Remove(CamScope(Mode), ProfileKey(Profile), FString::Printf(TEXT("%s camera %c reset to shipped values"), *Mode, Profile));
}

const FZoogiCamSettings& ZoogiCamConfig::Get(const FString& Mode)
{
	static const FZoogiCamSettings Shipped = ProfileDefaults(TEXT('A'));
	if (!ZoogiDevGate::IsUnlocked()) return Shipped;
	struct FCache { uint32 Gen = MAX_uint32; FZoogiCamSettings S; };
	static TMap<FString, FCache> Cache;
	FCache& C = Cache.FindOrAdd(Mode);
	const uint32 Gen = ZoogiDevConfig::Generation();
	if (C.Gen != Gen)
	{
		const TCHAR P = ActiveProfile(Mode);
		C.S = ProfileDefaults(P);
		for (const FParam& Prm : Params()) C.S.*(Prm.Field) = GetValue(Mode, P, Prm);
		C.Gen = Gen;
	}
	return C.S;
}
