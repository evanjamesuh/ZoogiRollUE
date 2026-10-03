#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoogiFXLibrary.generated.h"

class UNiagaraSystem;

/** Editor helpers (called from Scripts/make_niagara.py) to recolour duplicated Niagara template systems. */
UCLASS()
class ZOOGIROLL_API UZoogiFXLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Lists emitters and their colour-like rapid-iteration (module input) parameters. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static FString DescribeNiagara(UNiagaraSystem* System);

	/** Replaces every colour module input (Initialize Particle colour etc.) with Color * Intensity and recompiles. Returns the count changed. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 TintNiagara(UNiagaraSystem* System, FLinearColor Color, float Intensity, bool bKeepBrightness);

	/** Multiplies every float module input whose name contains NameContains (e.g. "Sprite Size") by Multiplier. No recompile (call TintNiagara or Recompile after). */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 ScaleNiagaraFloats(UNiagaraSystem* System, const FString& NameContains, float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static void RecompileNiagara(UNiagaraSystem* System);
};
