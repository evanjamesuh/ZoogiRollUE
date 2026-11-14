#pragma once
// Oct 27 (Evan, Dev Mode spec Part 10): the Configuration Persistence Manager. ONE versioned JSON store for everything the
// in-game Dev Mode edits (object layouts, later cameras / spawns / HUD / physics), saved in the app's own storage so it
// survives restarts on the iPhone. Values live in scopes and resolve with a fixed precedence:
//   global  ->  mode:<royale|classic|combo|training>  ->  level:<arena slug>  ->  screen:<name>  ->  dev   (later wins)
// Every Set records a change (before / after) for UNDO / REDO and for the EXPORT package Claude bakes into the build.
// Nothing here is read unless the device is dev-unlocked (ZoogiDevGate), so ordinary players always run the shipped values.
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace ZoogiDevConfig
{
	constexpr int32 SchemaVersion = 1;

	struct FContext
	{
		FString Mode, Level, Screen;
	};

	/** <Saved>/ZoogiDev/devconfig.json */
	ZOOGIROLL_API FString StorePath();
	/** Loads the store from disk (once; Reload forces). */
	ZOOGIROLL_API void EnsureLoaded();
	ZOOGIROLL_API bool Reload();
	/** Writes the store (atomic: tmp file + move). */
	ZOOGIROLL_API bool Save();
	ZOOGIROLL_API bool IsDirty();

	ZOOGIROLL_API TSharedPtr<FJsonValue> Get(const FString& Scope, const FString& Key);
	/** Highest-precedence value for Key in this context (null when nothing overrides it). */
	ZOOGIROLL_API TSharedPtr<FJsonValue> Resolve(const FString& Key, const FContext& Ctx);
	ZOOGIROLL_API double ResolveNumber(const FString& Key, const FContext& Ctx, double Default);

	/** Sets (or with a null Value removes) a value; recorded for undo / redo and export. Why = a short human label. */
	ZOOGIROLL_API void Set(const FString& Scope, const FString& Key, const TSharedPtr<FJsonValue>& Value, const FString& Why);
	ZOOGIROLL_API void Remove(const FString& Scope, const FString& Key, const FString& Why);

	ZOOGIROLL_API bool CanUndo();
	ZOOGIROLL_API bool CanRedo();
	/** Returns the Scope/Key that changed (empty when nothing to undo / redo). */
	ZOOGIROLL_API bool Undo(FString& OutScope, FString& OutKey);
	ZOOGIROLL_API bool Redo(FString& OutScope, FString& OutKey);

	/** Builds the export package (JSON) + a human-readable summary, writes both under <Saved>/ZoogiDev/exports and puts the
	 *  JSON on the clipboard. Returns the JSON text. */
	ZOOGIROLL_API FString ExportPackage(const FContext& Ctx, FString& OutSummary, FString& OutPath);

	ZOOGIROLL_API FString ValueToString(const TSharedPtr<FJsonValue>& V);
	ZOOGIROLL_API int32 NumEntries();
	ZOOGIROLL_API int32 NumChanges();
	/** Bumps on every change / undo / redo / load: readers cache resolved values against it. */
	ZOOGIROLL_API uint32 Generation();
}

/** Dev access (spec 1.1): a DEV button ([ZoogiRoll.Dev] bDevButton, off for App Store builds) + a 4-digit PIN. The unlock is
 *  remembered on the device (GameUserSettings [ZoogiRoll.Dev] bUnlocked) until LOCK. */
namespace ZoogiDevGate
{
	ZOOGIROLL_API bool ButtonEnabled();
	ZOOGIROLL_API bool IsUnlocked();
	ZOOGIROLL_API bool TryUnlock(const FString& Pin);
	ZOOGIROLL_API void Lock();
}
