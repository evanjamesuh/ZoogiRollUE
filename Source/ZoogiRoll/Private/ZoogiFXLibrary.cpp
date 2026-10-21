#include "ZoogiFXLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraTypes.h"

#if WITH_EDITORONLY_DATA
namespace
{
	template <typename F>
	void ForEachScript(UNiagaraSystem* System, F&& Fn)
	{
		if (UNiagaraScript* S = System->GetSystemSpawnScript()) Fn(TEXT("SystemSpawn"), S);
		if (UNiagaraScript* S = System->GetSystemUpdateScript()) Fn(TEXT("SystemUpdate"), S);
		for (FNiagaraEmitterHandle& H : System->GetEmitterHandles())
		{
			FVersionedNiagaraEmitterData* D = H.GetEmitterData();
			if (!D) continue;
			TArray<UNiagaraScript*> Scripts;
			D->GetScripts(Scripts, false);
			for (UNiagaraScript* S : Scripts) if (S) Fn(H.GetName().ToString(), S);
		}
	}
}
#endif

FString UZoogiFXLibrary::DescribeNiagara(UNiagaraSystem* System)
{
	FString Out;
#if WITH_EDITORONLY_DATA
	if (!System) return TEXT("null");
	ForEachScript(System, [&](const FString& Owner, UNiagaraScript* S)
	{
		for (const FNiagaraVariableWithOffset& V : S->RapidIterationParameters.ReadParameterVariables())
		{
			const FNiagaraTypeDefinition& T = V.GetType();
			FString Val;
			if (T == FNiagaraTypeDefinition::GetColorDef())
			{
				const FLinearColor C = S->RapidIterationParameters.GetParameterValue<FLinearColor>(V);
				Val = C.ToString();
			}
			else if (T == FNiagaraTypeDefinition::GetFloatDef()) Val = FString::SanitizeFloat(S->RapidIterationParameters.GetParameterValue<float>(V));
			else if (T == FNiagaraTypeDefinition::GetIntDef()) Val = FString::FromInt(S->RapidIterationParameters.GetParameterValue<int32>(V));
			else if (T == FNiagaraTypeDefinition::GetVec3Def()) Val = S->RapidIterationParameters.GetParameterValue<FVector3f>(V).ToString();
			Out += FString::Printf(TEXT("%s | %s | %s | %s\n"), *Owner, *V.GetName().ToString(), *T.GetName(), *Val);
		}
	});
#endif
	return Out;
}

int32 UZoogiFXLibrary::TintNiagara(UNiagaraSystem* System, FLinearColor Color, float Intensity, bool bKeepBrightness)
{
	int32 Count = 0;
#if WITH_EDITORONLY_DATA
	if (!System) return 0;
	System->Modify();
	ForEachScript(System, [&](const FString& Owner, UNiagaraScript* S)
	{
		TArray<FNiagaraVariableWithOffset> Vars(S->RapidIterationParameters.ReadParameterVariables());
		bool bChanged = false;
		for (const FNiagaraVariableWithOffset& V : Vars)
		{
			if (V.GetType() != FNiagaraTypeDefinition::GetColorDef()) continue;
			const FLinearColor Old = S->RapidIterationParameters.GetParameterValue<FLinearColor>(V);
			float Scale = Intensity;
			if (bKeepBrightness) Scale *= FMath::Max3(Old.R, Old.G, Old.B);
			FLinearColor New = Color * Scale;
			New.A = Old.A;
			S->Modify();
			S->RapidIterationParameters.SetParameterValue(New, V, true);
			bChanged = true;
			++Count;
		}
		if (bChanged) S->MarkPackageDirty();
	});
	if (Count > 0)
	{
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true);
	}
#endif
	return Count;
}

int32 UZoogiFXLibrary::ScaleNiagaraFloats(UNiagaraSystem* System, const FString& NameContains, float Multiplier)
{
	int32 Count = 0;
#if WITH_EDITORONLY_DATA
	if (!System) return 0;
	System->Modify();
	ForEachScript(System, [&](const FString& Owner, UNiagaraScript* S)
	{
		TArray<FNiagaraVariableWithOffset> Vars(S->RapidIterationParameters.ReadParameterVariables());
		for (const FNiagaraVariableWithOffset& V : Vars)
		{
			if (V.GetType() != FNiagaraTypeDefinition::GetFloatDef() || !V.GetName().ToString().Contains(NameContains)) continue;
			const float Old = S->RapidIterationParameters.GetParameterValue<float>(V);
			S->Modify();
			S->RapidIterationParameters.SetParameterValue(Old * Multiplier, V, true);
			S->MarkPackageDirty();
			++Count;
		}
	});
#endif
	return Count;
}

void UZoogiFXLibrary::RecompileNiagara(UNiagaraSystem* System)
{
#if WITH_EDITORONLY_DATA
	if (!System) return;
	System->RequestCompile(true);
	System->WaitForCompilationComplete(true);
#endif
}

// ---------------- KriptoFX port helpers ----------------
#if WITH_EDITOR
#include "NiagaraEditorUtilities.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#endif

bool UZoogiFXLibrary::AddEmitterFromAsset(UNiagaraSystem* System, UNiagaraEmitter* Emitter, const FString& NewName)
{
#if WITH_EDITOR
	if (!System || !Emitter) return false;
	System->Modify();
	const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, Emitter->GetExposedVersion().VersionGuid, true);
	for (FNiagaraEmitterHandle& H : System->GetEmitterHandles())
	{
		if (H.GetId() == Id) { H.SetName(FName(*NewName), *System); return true; }
	}
#endif
	return false;
}

#if WITH_EDITORONLY_DATA
namespace
{
	template <typename T, typename F>
	int32 SetTyped(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, const FNiagaraTypeDefinition& Type, F&& MakeValue)
	{
		int32 Count = 0;
		if (!System) return 0;
		System->Modify();
		ForEachScript(System, [&](const FString& Owner, UNiagaraScript* S)
		{
			const FString Prefix = TEXT("Constants.") + EmitterName + TEXT(".");
			TArray<FNiagaraVariableWithOffset> Vars(S->RapidIterationParameters.ReadParameterVariables());
			for (const FNiagaraVariableWithOffset& V : Vars)
			{
				const FString VN = V.GetName().ToString();
				if (V.GetType() != Type || !VN.Contains(NameContains)) continue;
				if (!EmitterName.IsEmpty() && !VN.StartsWith(Prefix)) continue;
				S->Modify();
				T Val = MakeValue(S->RapidIterationParameters.GetParameterValue<T>(V));
				S->RapidIterationParameters.SetParameterValue(Val, V, true);
				S->MarkPackageDirty();
				++Count;
			}
		});
		return Count;
	}
}
#endif

int32 UZoogiFXLibrary::SetNiagaraFloat(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, float Value)
{
#if WITH_EDITORONLY_DATA
	return SetTyped<float>(System, EmitterName, NameContains, FNiagaraTypeDefinition::GetFloatDef(), [&](float) { return Value; });
#else
	return 0;
#endif
}
int32 UZoogiFXLibrary::SetNiagaraInt(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, int32 Value)
{
#if WITH_EDITORONLY_DATA
	return SetTyped<int32>(System, EmitterName, NameContains, FNiagaraTypeDefinition::GetIntDef(), [&](int32) { return Value; });
#else
	return 0;
#endif
}
int32 UZoogiFXLibrary::SetNiagaraColor(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, FLinearColor Value)
{
#if WITH_EDITORONLY_DATA
	return SetTyped<FLinearColor>(System, EmitterName, NameContains, FNiagaraTypeDefinition::GetColorDef(), [&](FLinearColor) { return Value; });
#else
	return 0;
#endif
}
int32 UZoogiFXLibrary::SetNiagaraVector(UNiagaraSystem* System, const FString& EmitterName, const FString& NameContains, FVector Value)
{
#if WITH_EDITORONLY_DATA
	return SetTyped<FVector3f>(System, EmitterName, NameContains, FNiagaraTypeDefinition::GetVec3Def(), [&](FVector3f) { return FVector3f(Value); });
#else
	return 0;
#endif
}

int32 UZoogiFXLibrary::SetEmitterRender(UNiagaraSystem* System, const FString& EmitterName, UMaterialInterface* Material, UStaticMesh* Mesh)
{
	int32 Count = 0;
#if WITH_EDITOR
	if (!System) return 0;
	for (FNiagaraEmitterHandle& H : System->GetEmitterHandles())
	{
		if (H.GetName().ToString() != EmitterName) continue;
		FVersionedNiagaraEmitterData* D = H.GetEmitterData();
		if (!D) continue;
		for (UNiagaraRendererProperties* R : D->GetRenderers())
		{
			if (!R) continue;
			R->Modify();
			if (UNiagaraSpriteRendererProperties* SR = Cast<UNiagaraSpriteRendererProperties>(R))
			{
				SR->Material = Material; SR->MICMaterial = nullptr; ++Count;
			}
			else if (UNiagaraRibbonRendererProperties* RR = Cast<UNiagaraRibbonRendererProperties>(R))
			{
				RR->Material = Material; ++Count;
			}
			else if (UNiagaraMeshRendererProperties* MR = Cast<UNiagaraMeshRendererProperties>(R))
			{
				if (Mesh)
				{
					if (MR->Meshes.Num() == 0) MR->Meshes.AddDefaulted();
					MR->Meshes.SetNum(1);
					MR->Meshes[0].Mesh = Mesh;
				}
				if (Material)
				{
					MR->bOverrideMaterials = true;
					MR->OverrideMaterials.SetNum(1);
					MR->OverrideMaterials[0].ExplicitMat = Material;
				}
				MR->FacingMode = ENiagaraMeshFacingMode::Default; // template faces velocity: zero-velocity decals/portals would stand on edge
				++Count;
			}
			FPropertyChangedEvent Ev(nullptr);
			R->PostEditChangeProperty(Ev);
		}
	}
	System->MarkPackageDirty();
#endif
	return Count;
}

bool UZoogiFXLibrary::SetEmitterLocalSpace(UNiagaraSystem* System, const FString& EmitterName, bool bLocalSpace)
{
#if WITH_EDITOR
	if (!System) return false;
	for (FNiagaraEmitterHandle& H : System->GetEmitterHandles())
	{
		if (H.GetName().ToString() != EmitterName) continue;
		if (FVersionedNiagaraEmitterData* D = H.GetEmitterData()) { D->bLocalSpace = bLocalSpace; return true; }
	}
#endif
	return false;
}

FString UZoogiFXLibrary::CompileReport(UNiagaraSystem* System)
{
	FString Out;
#if WITH_EDITORONLY_DATA
	if (!System) return TEXT("null");
	System->RequestCompile(true);
	System->WaitForCompilationComplete(true);
	int32 Errors = 0;
	ForEachScript(System, [&](const FString& Owner, UNiagaraScript* S)
	{
		if (S->IsEmitterSpawnScript() || S->IsEmitterUpdateScript()) return;
		const ENiagaraScriptCompileStatus St = S->GetLastCompileStatus();
		if (St == ENiagaraScriptCompileStatus::NCS_Error || St == ENiagaraScriptCompileStatus::NCS_Unknown || St == ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			++Errors;
			Out += FString::Printf(TEXT("%s:%s status=%d; "), *Owner, *S->GetName(), (int32)St);
			for (const FNiagaraCompileEvent& E : S->GetVMExecutableData().LastCompileEvents)
				if (E.Severity == FNiagaraCompileEventSeverity::Error) Out += E.Message + TEXT(" | ");
		}
	});
	if (!System->IsValid()) Out += TEXT("System invalid; ");
	if (Errors == 0 && Out.IsEmpty()) Out = TEXT("OK");
#endif
	return Out;
}

