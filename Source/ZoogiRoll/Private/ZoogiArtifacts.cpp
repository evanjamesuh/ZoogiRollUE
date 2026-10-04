#include "ZoogiArtifacts.h"
#include "ZoogiTypes.h"
#include "Engine/DataTable.h"

namespace
{
	TArray<FZoogiArtifactRow> BuildDefaults()
	{
		TArray<FZoogiArtifactRow> R;
		R.SetNum((int32)EZoogiArtifact::MAX);
		auto Make = [&R](EZoogiArtifact Id, EZoogiElement El, const TCHAR* Name, const TCHAR* Inf, const TCHAR* Grd, const TCHAR* Core, const TCHAR* Rim) -> FZoogiArtifactRow&
		{
			FZoogiArtifactRow& A = R[(int32)Id];
			A.Id = Id; A.Element = El; A.DisplayName = Name; A.InfuseText = Inf; A.GuardText = Grd;
			A.CoreColor = ZoogiUtil::Hex(Core); A.RimColor = ZoogiUtil::Hex(Rim);
			return A;
		};
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Ember, EZoogiElement::Fire, TEXT("Ember Stone"),
				TEXT("Flame burst on hit: pushes everything nearby, target Burned (-15% next shot)"),
				TEXT("Flame ring: attacker bounces back, you take 40% less knockback"), TEXT("#FF5A1F"), TEXT("#FFC14D"));
			A.ImpactRadius = 260.f; A.ImpactPush = 900.f; A.StatusTurns = 1; A.GuardAttackerMul = -0.35f;
			A.ImpactNiagara = TEXT("NS_Explosion"); A.AuraNiagara = TEXT("NS_Burst_Red"); A.AnimeKey = TEXT("Ember"); A.AffinityZoogi = TEXT("hotstreak");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Storm, EZoogiElement::Lightning, TEXT("Storm Eye"),
				TEXT("Lightning chains to 2 more targets on hit, first target Shocked (next shot max 70%)"),
				TEXT("Arc cage: attacker gets Shocked, you take 40% less knockback"), TEXT("#7FD8FF"), TEXT("#FFFFFF"));
			A.ImpactRadius = 650.f; A.ImpactPush = 600.f; A.ChainCount = 2; A.StatusTurns = 1;
			A.ImpactNiagara = TEXT("NS_Burst_White"); A.AuraNiagara = TEXT("NS_Burst_Blue"); A.AnimeKey = TEXT("Storm"); A.AffinityZoogi = TEXT("bolt");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Frost, EZoogiElement::Ice, TEXT("Frost Crystal"),
				TEXT("Target Frozen: slides on ice after the hit; ice shards push small things"),
				TEXT("Ice shell: attacker slowed 40% on contact, you take 40% less knockback"), TEXT("#9EF0FF"), TEXT("#E8FBFF"));
			A.ImpactRadius = 220.f; A.ImpactPush = 450.f; A.StatusTurns = 1; A.GuardAttackerMul = 0.6f;
			A.ImpactNiagara = TEXT("NS_Burst_Blue"); A.AuraNiagara = TEXT("NS_Burst_White"); A.AnimeKey = TEXT("Frost"); A.AffinityZoogi = TEXT("pinpoint");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Titan, EZoogiElement::Earth, TEXT("Titan Core"),
				TEXT("1.7x mass, slightly slower roll; ground shockwave on hit"),
				TEXT("Rock plating: 1.8x mass while guarded, 40% less knockback"), TEXT("#8C7A63"), TEXT("#FFB347"));
			A.MassMul = 1.7f; A.LaunchSpeedMul = 0.9f; A.ImpactRadius = 320.f; A.ImpactPush = 700.f; A.GuardMassMul = 1.8f;
			A.ImpactNiagara = TEXT("NS_Burst_Gold"); A.AuraNiagara = TEXT("NS_Burst_Gold"); A.AnimeKey = TEXT("Titan"); A.AffinityZoogi = TEXT("wraps");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Void, EZoogiElement::Void, TEXT("Void Shard"),
				TEXT("Gravity well at impact: pulls nearby orbs and rivals in for 0.6 s"),
				TEXT("Ring-warp barrier: attacker deflected 25 deg, 40% less knockback"), TEXT("#2A0F4F"), TEXT("#B06CFF"));
			A.ImpactRadius = 450.f; A.PullTime = 0.6f; A.PullAccel = 2500.f; A.GuardReflectDeg = 25.f;
			A.ImpactNiagara = TEXT("NS_Burst_Purple"); A.AuraNiagara = TEXT("NS_Burst_Purple"); A.AnimeKey = TEXT("Void"); A.AffinityZoogi = TEXT("nightshade");
		}
		// Optional DataTable override (editor-tunable).
		if (UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Artifacts.DT_Artifacts"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (DT->GetRowStruct() == FZoogiArtifactRow::StaticStruct())
			{
				for (int32 i = 1; i < (int32)EZoogiArtifact::MAX; ++i)
				{
					const FName RowName(*ZoogiArtifacts::Key((EZoogiArtifact)i));
					if (const FZoogiArtifactRow* Row = DT->FindRow<FZoogiArtifactRow>(RowName, TEXT("ZoogiArtifacts"), false))
					{
						R[i] = *Row;
						R[i].Id = (EZoogiArtifact)i;
					}
				}
			}
		}
		return R;
	}
}

const FZoogiArtifactRow& ZoogiArtifacts::Get(EZoogiArtifact A)
{
	static TArray<FZoogiArtifactRow> Rows = BuildDefaults();
	const int32 I = FMath::Clamp((int32)A, 0, Rows.Num() - 1);
	return Rows[I];
}

FString ZoogiArtifacts::Key(EZoogiArtifact A)
{
	switch (A)
	{
	case EZoogiArtifact::Ember: return TEXT("Ember");
	case EZoogiArtifact::Storm: return TEXT("Storm");
	case EZoogiArtifact::Frost: return TEXT("Frost");
	case EZoogiArtifact::Titan: return TEXT("Titan");
	case EZoogiArtifact::Void: return TEXT("Void");
	default: return TEXT("Any");
	}
}

FString ZoogiArtifacts::Name(EZoogiArtifact A)
{
	if (A == EZoogiArtifact::None) return TEXT("-");
	const FString& N = Get(A).DisplayName;
	return N.IsEmpty() ? Key(A) : N;
}

FLinearColor ZoogiArtifacts::Color(EZoogiArtifact A)
{
	if (A == EZoogiArtifact::None) return FLinearColor(0.6f, 0.65f, 0.75f);
	// Void's core is near-black: use its rim for UI and light.
	return A == EZoogiArtifact::Void ? Get(A).RimColor : Get(A).CoreColor;
}

EZoogiArtifact ZoogiArtifacts::Affinity(FName Id)
{
	// FName compares case-insensitively.
	if (Id == FName(TEXT("hotstreak"))) return EZoogiArtifact::Ember;
	if (Id == FName(TEXT("bolt")) || Id == FName(TEXT("lars"))) return EZoogiArtifact::Storm;
	if (Id == FName(TEXT("pinpoint")) || Id == FName(TEXT("wolfgang"))) return EZoogiArtifact::Frost;
	if (Id == FName(TEXT("nightshade"))) return EZoogiArtifact::Void;
	if (Id == FName(TEXT("wraps"))) return EZoogiArtifact::Titan;
	return EZoogiArtifact::None;
}

EZoogiArtifact ZoogiArtifacts::Random(FRandomStream* Stream)
{
	const int32 N = (int32)EZoogiArtifact::MAX - 1;
	return (EZoogiArtifact)(1 + (Stream ? Stream->RandRange(0, N - 1) : FMath::RandRange(0, N - 1)));
}

const TCHAR* ZoogiArtifacts::StatusName(EZoogiStatus S)
{
	switch (S)
	{
	case EZoogiStatus::Burned: return TEXT("BURNED");
	case EZoogiStatus::Shocked: return TEXT("SHOCKED");
	case EZoogiStatus::Frozen: return TEXT("FROZEN");
	default: return TEXT("");
	}
}
