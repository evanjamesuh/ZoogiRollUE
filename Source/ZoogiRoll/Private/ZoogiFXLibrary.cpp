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
