// Oct 27: Dev Mode configuration store + PIN gate (see ZoogiDevConfig.h).
#include "ZoogiDevConfig.h"
#include "ZoogiRoll.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"

namespace
{
	struct FDevChange
	{
		FString Scope, Key, Why, Time;
		TSharedPtr<FJsonValue> Before, After; // null = not set
	};

	struct FDevStore
	{
		bool bLoaded = false;
		bool bDirty = false;
		TMap<FString, TMap<FString, TSharedPtr<FJsonValue>>> Scopes;
		TArray<FDevChange> Stack;    // this session's edits (undo / redo)
		int32 UndoPos = 0;           // Stack[0 .. UndoPos) are applied
		TArray<TSharedPtr<FJsonValue>> PastHistory; // changes saved by earlier sessions (export)
	};
	FDevStore& DevStoreS() { static FDevStore St; return St; }
	uint32 GGeneration = 1;

	FString NowText() { return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")); }

	void RawSet(const FString& Scope, const FString& Key, const TSharedPtr<FJsonValue>& V)
	{
		FDevStore& St = DevStoreS();
		if (!V.IsValid() || V->Type == EJson::Null)
		{
			if (TMap<FString, TSharedPtr<FJsonValue>>* M = St.Scopes.Find(Scope)) { M->Remove(Key); if (M->Num() == 0) St.Scopes.Remove(Scope); }
		}
		else St.Scopes.FindOrAdd(Scope).Add(Key, V);
		St.bDirty = true;
		++GGeneration;
	}

	TSharedPtr<FJsonObject> ChangeToJson(const FDevChange& C)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("time"), C.Time);
		O->SetStringField(TEXT("scope"), C.Scope);
		O->SetStringField(TEXT("key"), C.Key);
		O->SetStringField(TEXT("why"), C.Why);
		O->SetField(TEXT("before"), C.Before.IsValid() ? C.Before : MakeShared<FJsonValueNull>());
		O->SetField(TEXT("after"), C.After.IsValid() ? C.After : MakeShared<FJsonValueNull>());
		return O;
	}

	FString PlatformName() { return FString(FPlatformProperties::IniPlatformName()); }
}

FString ZoogiDevConfig::StorePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ZoogiDev"), TEXT("devconfig.json"));
}

void ZoogiDevConfig::EnsureLoaded()
{
	if (!DevStoreS().bLoaded) Reload();
}

bool ZoogiDevConfig::Reload()
{
	FDevStore& St = DevStoreS();
	St.bLoaded = true;
	St.Scopes.Reset();
	St.PastHistory.Reset();
	St.Stack.Reset();
	St.UndoPos = 0;
	St.bDirty = false;
	++GGeneration;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *StorePath())) { UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: no store yet (%s)"), *StorePath()); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		// keep the broken file for inspection, start clean
		IFileManager::Get().Move(*(StorePath() + TEXT(".bad")), *StorePath(), true);
		UE_LOG(LogZoogi, Warning, TEXT("DEVCONFIG: store unreadable, moved aside to .bad"));
		return false;
	}
	int32 Schema = 0;
	Root->TryGetNumberField(TEXT("schema"), Schema);
	// migration: schema 0/1 share the same shape; later versions convert here before reading
	const TSharedPtr<FJsonObject>* ScopesObj = nullptr;
	if (Root->TryGetObjectField(TEXT("scopes"), ScopesObj) && ScopesObj && ScopesObj->IsValid())
	{
		for (const auto& Sc : (*ScopesObj)->Values)
		{
			const TSharedPtr<FJsonObject> M = Sc.Value.IsValid() ? Sc.Value->AsObject() : nullptr;
			if (!M) continue;
			for (const auto& Kv : M->Values) St.Scopes.FindOrAdd(Sc.Key).Add(Kv.Key, Kv.Value);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Hist = nullptr;
	if (Root->TryGetArrayField(TEXT("history"), Hist) && Hist) St.PastHistory = *Hist;
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: loaded %s (schema %d, %d scopes, %d past changes)"), *StorePath(), Schema, St.Scopes.Num(), St.PastHistory.Num());
	return true;
}

bool ZoogiDevConfig::Save()
{
	EnsureLoaded();
	FDevStore& St = DevStoreS();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema"), SchemaVersion);
	Root->SetStringField(TEXT("game"), TEXT("ZoogiRoll"));
	Root->SetStringField(TEXT("savedAt"), NowText());
	Root->SetStringField(TEXT("platform"), PlatformName());
	TSharedRef<FJsonObject> Sc = MakeShared<FJsonObject>();
	for (const auto& Scope : St.Scopes)
	{
		TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		for (const auto& Kv : Scope.Value) M->SetField(Kv.Key, Kv.Value);
		Sc->SetObjectField(Scope.Key, M);
	}
	Root->SetObjectField(TEXT("scopes"), Sc);
	// history = earlier sessions + this session's applied edits (last 400 kept)
	TArray<TSharedPtr<FJsonValue>> Hist = St.PastHistory;
	for (int32 i = 0; i < St.UndoPos; ++i) Hist.Add(MakeShared<FJsonValueObject>(ChangeToJson(St.Stack[i])));
	if (Hist.Num() > 400) Hist.RemoveAt(0, Hist.Num() - 400);
	Root->SetArrayField(TEXT("history"), Hist);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	const FString Path = StorePath(), Tmp = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const bool bOk = FFileHelper::SaveStringToFile(Text, *Tmp) && IFileManager::Get().Move(*Path, *Tmp, true);
	if (bOk)
	{
		// the saved edits become history; undo keeps working on what is still in the session stack
		St.PastHistory = Hist;
		St.Stack.RemoveAt(0, St.UndoPos);
		St.UndoPos = 0;
		St.bDirty = false;
	}
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: SAVE %s %s (%d scopes)"), *Path, bOk ? TEXT("OK") : TEXT("FAILED"), St.Scopes.Num());
	return bOk;
}

bool ZoogiDevConfig::IsDirty() { return DevStoreS().bDirty; }

TSharedPtr<FJsonValue> ZoogiDevConfig::Get(const FString& Scope, const FString& Key)
{
	EnsureLoaded();
	if (const TMap<FString, TSharedPtr<FJsonValue>>* M = DevStoreS().Scopes.Find(Scope))
		if (const TSharedPtr<FJsonValue>* V = M->Find(Key)) return *V;
	return nullptr;
}

TSharedPtr<FJsonValue> ZoogiDevConfig::Resolve(const FString& Key, const FContext& Ctx)
{
	TSharedPtr<FJsonValue> Out = Get(TEXT("global"), Key);
	auto Over = [&Out, &Key](const FString& Scope) { if (TSharedPtr<FJsonValue> V = Get(Scope, Key)) Out = V; };
	if (!Ctx.Mode.IsEmpty()) Over(TEXT("mode:") + Ctx.Mode);
	if (!Ctx.Level.IsEmpty()) Over(TEXT("level:") + Ctx.Level);
	if (!Ctx.Screen.IsEmpty()) Over(TEXT("screen:") + Ctx.Screen);
	Over(TEXT("dev"));
	return Out;
}

double ZoogiDevConfig::ResolveNumber(const FString& Key, const FContext& Ctx, double Default)
{
	const TSharedPtr<FJsonValue> V = Resolve(Key, Ctx);
	double D = Default;
	return (V.IsValid() && V->TryGetNumber(D)) ? D : Default;
}

void ZoogiDevConfig::Set(const FString& Scope, const FString& Key, const TSharedPtr<FJsonValue>& Value, const FString& Why)
{
	EnsureLoaded();
	FDevStore& St = DevStoreS();
	FDevChange C;
	C.Scope = Scope; C.Key = Key; C.Why = Why; C.Time = NowText();
	C.Before = Get(Scope, Key);
	C.After = (Value.IsValid() && Value->Type != EJson::Null) ? Value : nullptr;
	if (St.UndoPos < St.Stack.Num()) St.Stack.SetNum(St.UndoPos); // a new edit drops the redo branch
	St.Stack.Add(C);
	if (St.Stack.Num() > 200) St.Stack.RemoveAt(0); // undo depth
	St.UndoPos = St.Stack.Num();
	RawSet(Scope, Key, C.After);
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: set %s / %s (%s)"), *Scope, *Key, *Why);
}

void ZoogiDevConfig::Remove(const FString& Scope, const FString& Key, const FString& Why)
{
	Set(Scope, Key, nullptr, Why);
}

bool ZoogiDevConfig::CanUndo() { return DevStoreS().UndoPos > 0; }
bool ZoogiDevConfig::CanRedo() { return DevStoreS().UndoPos < DevStoreS().Stack.Num(); }

bool ZoogiDevConfig::Undo(FString& OutScope, FString& OutKey)
{
	FDevStore& St = DevStoreS();
	if (St.UndoPos <= 0) return false;
	const FDevChange& C = St.Stack[--St.UndoPos];
	RawSet(C.Scope, C.Key, C.Before);
	OutScope = C.Scope; OutKey = C.Key;
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: undo %s"), *C.Why);
	return true;
}

bool ZoogiDevConfig::Redo(FString& OutScope, FString& OutKey)
{
	FDevStore& St = DevStoreS();
	if (St.UndoPos >= St.Stack.Num()) return false;
	const FDevChange& C = St.Stack[St.UndoPos++];
	RawSet(C.Scope, C.Key, C.After);
	OutScope = C.Scope; OutKey = C.Key;
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: redo %s"), *C.Why);
	return true;
}

FString ZoogiDevConfig::ValueToString(const TSharedPtr<FJsonValue>& V)
{
	if (!V.IsValid() || V->Type == EJson::Null) return TEXT("(default)");
	FString Out;
	if (V->Type == EJson::Number) return FString::SanitizeFloat(V->AsNumber());
	if (V->Type == EJson::String) return V->AsString();
	if (V->Type == EJson::Boolean) return V->AsBool() ? TEXT("true") : TEXT("false");
	if (V->Type == EJson::Array) return FString::Printf(TEXT("[%d items]"), V->AsArray().Num());
	return TEXT("{...}");
}

int32 ZoogiDevConfig::NumEntries()
{
	EnsureLoaded();
	int32 N = 0;
	for (const auto& Sc : DevStoreS().Scopes) N += Sc.Value.Num();
	return N;
}

int32 ZoogiDevConfig::NumChanges() { return DevStoreS().PastHistory.Num() + DevStoreS().UndoPos; }

uint32 ZoogiDevConfig::Generation() { EnsureLoaded(); return GGeneration; }

FString ZoogiDevConfig::ExportPackage(const FContext& Ctx, FString& OutSummary, FString& OutPath)
{
	EnsureLoaded();
	FDevStore& St = DevStoreS();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("zoogiDevExport"), 1);
	Root->SetNumberField(TEXT("schema"), SchemaVersion);
	Root->SetStringField(TEXT("exportedAt"), NowText());
	Root->SetStringField(TEXT("platform"), PlatformName());
	Root->SetStringField(TEXT("build"), FApp::GetBuildVersion());
	TSharedRef<FJsonObject> CtxO = MakeShared<FJsonObject>();
	CtxO->SetStringField(TEXT("mode"), Ctx.Mode);
	CtxO->SetStringField(TEXT("level"), Ctx.Level);
	CtxO->SetStringField(TEXT("screen"), Ctx.Screen);
	Root->SetObjectField(TEXT("context"), CtxO);
	// the full current overrides (what the build should end up with) ...
	TSharedRef<FJsonObject> Sc = MakeShared<FJsonObject>();
	int32 Entries = 0;
	for (const auto& Scope : St.Scopes)
	{
		TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		for (const auto& Kv : Scope.Value) { M->SetField(Kv.Key, Kv.Value); ++Entries; }
		Sc->SetObjectField(Scope.Key, M);
	}
	Root->SetObjectField(TEXT("overrides"), Sc);
	// ... and every change with its before / after values
	TArray<TSharedPtr<FJsonValue>> Changes = St.PastHistory;
	for (int32 i = 0; i < St.UndoPos; ++i) Changes.Add(MakeShared<FJsonValueObject>(ChangeToJson(St.Stack[i])));
	Root->SetArrayField(TEXT("changes"), Changes);
	// human-readable summary
	OutSummary = FString::Printf(TEXT("Zoogi Roll dev export %s (%s, build %s)\nContext: mode %s, level %s\n%d override(s) in %d scope(s), %d change(s):\n"),
		*NowText(), *PlatformName(), FApp::GetBuildVersion(), *Ctx.Mode, *Ctx.Level, Entries, St.Scopes.Num(), Changes.Num());
	TArray<TSharedPtr<FJsonValue>> Lines;
	for (const TSharedPtr<FJsonValue>& V : Changes)
	{
		const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
		if (!O) continue;
		const FString L = FString::Printf(TEXT("- %s  %s  [%s / %s]"), *O->GetStringField(TEXT("time")), *O->GetStringField(TEXT("why")), *O->GetStringField(TEXT("scope")), *O->GetStringField(TEXT("key")));
		OutSummary += L + TEXT("\n");
		Lines.Add(MakeShared<FJsonValueString>(L));
	}
	Root->SetArrayField(TEXT("summary"), Lines);
	TSharedRef<FJsonObject> Val = MakeShared<FJsonObject>();
	Val->SetNumberField(TEXT("entries"), Entries);
	Val->SetNumberField(TEXT("scopes"), St.Scopes.Num());
	Val->SetNumberField(TEXT("changes"), Changes.Num());
	Val->SetBoolField(TEXT("unsavedEdits"), St.bDirty);
	Root->SetObjectField(TEXT("validation"), Val);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ZoogiDev"), TEXT("exports"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	OutPath = FPaths::Combine(Dir, FString::Printf(TEXT("zoogi-dev-export-%s.json"), *Stamp));
	FFileHelper::SaveStringToFile(Text, *OutPath);
	FFileHelper::SaveStringToFile(OutSummary, *FPaths::Combine(Dir, FString::Printf(TEXT("zoogi-dev-export-%s.txt"), *Stamp)));
	FPlatformApplicationMisc::ClipboardCopy(*Text); // paste it straight into a chat with Claude
	UE_LOG(LogZoogi, Log, TEXT("DEVCONFIG: EXPORT %s (%d overrides, %d changes, copied to clipboard)"), *OutPath, Entries, Changes.Num());
	return Text;
}

// ------------------------------------------------------------------ gate

namespace
{
	int32 GUnlocked = -1; // -1 = read settings
	uint32 Fnv(const FString& Str)
	{
		uint32 H = 0x811c9dc5u;
		const FTCHARToUTF8 U(*Str);
		for (int32 i = 0; i < U.Length(); ++i) { H ^= (uint8)U.Get()[i]; H *= 0x01000193u; }
		return H;
	}
	constexpr uint32 GPinHash = 0x4e4a80adu; // FNV-1a of "zoogi-dev:" + the PIN Evan chose (the PIN itself is not stored)
}

bool ZoogiDevGate::ButtonEnabled()
{
	static const bool bOn = []
	{
		bool b = true; // TestFlight / dev builds: on. App Store release: [ZoogiRoll.Dev] bDevButton=False
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Dev"), TEXT("bDevButton"), b, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDevButton"))) b = false;
		return b;
	}();
	return bOn;
}

bool ZoogiDevGate::IsUnlocked()
{
	if (GUnlocked < 0)
	{
		bool b = false;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Dev"), TEXT("bUnlocked"), b, GGameUserSettingsIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiDev"))) b = true; // PC capture runs
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevPin"))) b = true; // b168 (Evan: "no pin"): Dev Mode opens straight away on any build with the DEV button (-ZoogiDevPin = the old PIN pad)
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevLockedTest"))) b = false; // b167: test a locked device (PIN pad)
		GUnlocked = b ? 1 : 0;
	}
	return GUnlocked == 1 && ButtonEnabled();
}

bool ZoogiDevGate::TryUnlock(const FString& Pin)
{
	const bool bOk = Fnv(TEXT("zoogi-dev:") + Pin) == GPinHash;
	if (bOk)
	{
		GUnlocked = 1;
		if (GConfig) { GConfig->SetBool(TEXT("ZoogiRoll.Dev"), TEXT("bUnlocked"), true, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	}
	UE_LOG(LogZoogi, Log, TEXT("DEVGATE: PIN %s"), bOk ? TEXT("accepted") : TEXT("rejected"));
	return bOk;
}

void ZoogiDevGate::Lock()
{
	GUnlocked = 0;
	if (GConfig) { GConfig->SetBool(TEXT("ZoogiRoll.Dev"), TEXT("bUnlocked"), false, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	UE_LOG(LogZoogi, Log, TEXT("DEVGATE: locked"));
}
