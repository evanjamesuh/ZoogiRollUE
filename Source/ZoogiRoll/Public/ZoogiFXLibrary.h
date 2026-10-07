#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZoogiFXLibrary.generated.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UMaterialInterface;
class UStaticMesh;

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

	/** --- KriptoFX port helpers (editor only) --- */
	/** Adds a copy of an emitter asset (e.g. a Niagara template emitter) to System under NewName. Returns false on failure. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static bool AddEmitterFromAsset(UNiagaraSystem* System, UNiagaraEmitter* Emitter, const FString& NewName);

	/** Sets (absolute) every float/int/bool/color/vec3 module input in emitter EmitterName (empty = all) whose name contains NameContains. Returns count. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 SetNiagaraFloat(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, float Value);
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 SetNiagaraInt(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, int32 Value);
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 SetNiagaraColor(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, FLinearColor Value);
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 SetNiagaraVector(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, FVector Value);

	/** Sets the material of every sprite/ribbon renderer in the emitter; for mesh renderers sets mesh (if given) and override material. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static int32 SetEmitterRender(UNiagaraSystem* System, const FString& EmitterName, UMaterialInterface* Material, UStaticMesh* Mesh);

	/** Sets emitter properties: local space, deterministic etc. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static bool SetEmitterLocalSpace(UNiagaraSystem* System, const FString& EmitterName, bool bLocalSpace);

	/** Recompiles and returns a report: "OK" or the compile errors/warnings of every script. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|FX")
	static FString CompileReport(UNiagaraSystem* System);
};
