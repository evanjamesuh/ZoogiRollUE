// Milestone "Awaken": artifacts (Ember Stone, Storm Eye, Frost Crystal, Titan Core, Void Shard).
// Data lives in FZoogiArtifactRow. Lookup order per artifact (checklist item 7), resolved once on first use:
//   1. DataAsset /Game/Data/Artifacts/DA_Artifact_<Key> (UZoogiArtifactData; made by Scripts/make_artifact_data.py from the built-ins)
//   2. DataTable /Game/Data/DT_Artifacts row <Key> (optional, row struct ZoogiArtifactRow)
//   3. the built-in C++ table in ZoogiArtifacts.cpp (fallback; -ZoogiBuiltInArtifacts forces it)
// The log says which source each artifact used and whether it is identical to the built-in row.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/RandomStream.h"
#include "ZoogiArtifacts.generated.h"

UENUM(BlueprintType)
enum class EZoogiArtifact : uint8
{
	None,
	Ember,
	Storm,
	Frost,
	Titan,
	Void,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EZoogiElement : uint8
{
	Neutral,
	Fire,
	Lightning,
	Ice,
	Earth,
	Void
};

/** One artifact's tuning, visuals and anime keys (a DataTable row). */
USTRUCT(BlueprintType)
struct FZoogiArtifactRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) EZoogiArtifact Id = EZoogiArtifact::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EZoogiElement Element = EZoogiElement::Neutral;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString InfuseText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString GuardText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor CoreColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor RimColor = FLinearColor::White;

	// Infuse (powered collision)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float LaunchSpeedMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MassMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ImpactPush = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ChainCount = 0;      // Storm
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PullTime = 0.f;      // Void
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PullAccel = 0.f;     // Void
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 StatusTurns = 0;     // Burned / Shocked / Frozen

	// Guard (next opponent attack only)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuardKnockbackMul = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuardMassMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuardReflectDeg = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float GuardAttackerMul = 1.f;

	// Visuals (procedural placeholders until Aura's art lands)
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ImpactNiagara;   // /Game/FX/<name>
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AuraNiagara;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AnimeKey;        // "Ember" etc. in sequence ids
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AffinityZoogi;   // primary affinity (see ZoogiArtifacts::Affinity for the full map)
};

/** One artifact as a DataAsset (DA_Artifact_<Key> in /Game/Data/Artifacts). Edit Row in the editor to retune Infuse / Guard. */
UCLASS(BlueprintType)
class ZOOGIROLL_API UZoogiArtifactData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Artifact") FZoogiArtifactRow Row;
};

/** Script / Blueprint access to the artifact table (used by Scripts/make_artifact_data.py and make_artifact_instances.py). */
UCLASS()
class ZOOGIROLL_API UZoogiArtifactLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** The built-in C++ row (ignores DataAssets / DataTable). */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Artifacts") static FZoogiArtifactRow GetBuiltInArtifact(EZoogiArtifact Artifact);
	/** The row the game uses (DataAsset > DataTable > built-in). */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Artifacts") static FZoogiArtifactRow GetArtifact(EZoogiArtifact Artifact);
	/** UI / light colour (rim colour for Void), same as ZoogiArtifacts::Color. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Artifacts") static FLinearColor GetArtifactColor(EZoogiArtifact Artifact);
	/** "Ember", "Storm", ... */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Artifacts") static FString GetArtifactKey(EZoogiArtifact Artifact);
	/** "DataAsset", "DataTable" or "BuiltIn". */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Artifacts") static FString GetArtifactSource(EZoogiArtifact Artifact);
};

/** Status effects applied by artifacts (in addition to the existing StunTurns / BoundShots). */
UENUM(BlueprintType)
enum class EZoogiStatus : uint8
{
	None,
	Burned,   // next shot -15%
	Shocked,  // next shot capped at 70% power
	Frozen    // ice-slick: low rolling deceleration for the next turn
};

namespace ZoogiArtifacts
{
	ZOOGIROLL_API const FZoogiArtifactRow& Get(EZoogiArtifact A);
	/** Built-in C++ row only (the fallback table). */
	ZOOGIROLL_API const FZoogiArtifactRow& BuiltIn(EZoogiArtifact A);
	/** Where Get(A) came from: "DataAsset", "DataTable" or "BuiltIn". */
	ZOOGIROLL_API FString Source(EZoogiArtifact A);
	ZOOGIROLL_API FString Name(EZoogiArtifact A);
	ZOOGIROLL_API FString Key(EZoogiArtifact A);        // "Ember", "Storm", ...
	ZOOGIROLL_API FLinearColor Color(EZoogiArtifact A);
	/** Affinity artifact for a Zoogi id (approved by Evan Oct 4): Hotstreak Ember, Bolt Storm, Lars Storm, Pinpoint Frost,
	 *  Nightshade Void, Wraps Titan, Wolfgang Frost. */
	ZOOGIROLL_API EZoogiArtifact Affinity(FName ZoogiId);
	ZOOGIROLL_API EZoogiArtifact Random(FRandomStream* Stream = nullptr);
	ZOOGIROLL_API const TCHAR* StatusName(EZoogiStatus S);
}
