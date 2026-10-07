#include "ZoogiArtifacts.h"
#include "ZoogiTypes.h"
#include "Engine/DataTable.h"
#include "ZoogiRoll.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	TArray<FZoogiArtifactRow> BuildArtifactDefaults()
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
			A.ImpactNiagara = TEXT("NS_Explosion"); A.AuraNiagara = TEXT("NS_Burst_Red"); A.AnimeKey = TEXT("Ember"); A.AffinityZoogi = TEXT("vex");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Storm, EZoogiElement::Lightning, TEXT("Storm Eye"),
				TEXT("Lightning chains to 2 more targets on hit, first target Shocked (next shot max 70%)"),
				TEXT("Arc cage: attacker gets Shocked, you take 40% less knockback"), TEXT("#7FD8FF"), TEXT("#FFFFFF"));
			A.ImpactRadius = 650.f; A.ImpactPush = 600.f; A.ChainCount = 2; A.StatusTurns = 1;
			A.ImpactNiagara = TEXT("NS_Burst_White"); A.AuraNiagara = TEXT("NS_Burst_Blue"); A.AnimeKey = TEXT("Storm"); A.AffinityZoogi = TEXT("luma");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Frost, EZoogiElement::Ice, TEXT("Frost Crystal"),
				TEXT("Target Frozen: slides on ice after the hit; ice shards push small things"),
				TEXT("Ice shell: attacker slowed 40% on contact, you take 40% less knockback"), TEXT("#9EF0FF"), TEXT("#E8FBFF"));
			A.ImpactRadius = 220.f; A.ImpactPush = 450.f; A.StatusTurns = 1; A.GuardAttackerMul = 0.6f;
			A.ImpactNiagara = TEXT("NS_Burst_Blue"); A.AuraNiagara = TEXT("NS_Burst_White"); A.AnimeKey = TEXT("Frost"); A.AffinityZoogi = TEXT("moss");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Titan, EZoogiElement::Earth, TEXT("Titan Core"),
				TEXT("1.7x mass, slightly slower roll; ground shockwave on hit"),
				TEXT("Rock plating: 1.8x mass while guarded, 40% less knockback"), TEXT("#8C7A63"), TEXT("#FFB347"));
			A.MassMul = 1.7f; A.LaunchSpeedMul = 0.9f; A.ImpactRadius = 320.f; A.ImpactPush = 700.f; A.GuardMassMul = 1.8f;
			A.ImpactNiagara = TEXT("NS_Burst_Gold"); A.AuraNiagara = TEXT("NS_Burst_Gold"); A.AnimeKey = TEXT("Titan"); A.AffinityZoogi = TEXT("orin");
		}
		{
			FZoogiArtifactRow& A = Make(EZoogiArtifact::Void, EZoogiElement::Void, TEXT("Void Shard"),
				TEXT("Gravity well at impact: pulls nearby orbs and rivals in for 0.6 s"),
				TEXT("Ring-warp barrier: attacker deflected 25 deg, 40% less knockback"), TEXT("#2A0F4F"), TEXT("#B06CFF"));
			A.ImpactRadius = 450.f; A.PullTime = 0.6f; A.PullAccel = 2500.f; A.GuardReflectDeg = 25.f;
			A.ImpactNiagara = TEXT("NS_Burst_Purple"); A.AuraNiagara = TEXT("NS_Burst_Purple"); A.AnimeKey = TEXT("Void"); A.AffinityZoogi = TEXT("darkzoogy");
		}
		return R;
	}

	struct FZoogiArtifactTableData
	{
		TArray<FZoogiArtifactRow> Rows;
		TArray<FString> Sources;
	};

	const TArray<FZoogiArtifactRow>& BuiltInArtifactRows()
	{
		static TArray<FZoogiArtifactRow> Rows = BuildArtifactDefaults();
		return Rows;
	}

	/** Built-ins overridden per artifact by DA_Artifact_<Key>, then DT_Artifacts. Logged once. */
	FZoogiArtifactTableData BuildArtifactTable()
	{
		FZoogiArtifactTableData T;
		T.Rows = BuiltInArtifactRows();
		T.Sources.Init(TEXT("BuiltIn"), T.Rows.Num());
		const bool bForceBuiltIn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBuiltInArtifacts"));
		if (bForceBuiltIn) UE_LOG(LogZoogi, Log, TEXT("Artifacts: -ZoogiBuiltInArtifacts, using the C++ table"));
		UDataTable* DT = bForceBuiltIn ? nullptr : LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Artifacts.DT_Artifacts"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (DT && DT->GetRowStruct() != FZoogiArtifactRow::StaticStruct()) DT = nullptr;
		FString Summary;
		int32 NData = 0, NSame = 0;
		for (int32 i = 1; i < (int32)EZoogiArtifact::MAX; ++i)
		{
			const FString Key = ZoogiArtifacts::Key((EZoogiArtifact)i);
			const FString Path = FString::Printf(TEXT("/Game/Data/Artifacts/DA_Artifact_%s.DA_Artifact_%s"), *Key, *Key);
			if (const UZoogiArtifactData* DA = bForceBuiltIn ? nullptr : LoadObject<UZoogiArtifactData>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				T.Rows[i] = DA->Row;
				T.Sources[i] = TEXT("DataAsset");
			}
			else if (const FZoogiArtifactRow* Row = DT ? DT->FindRow<FZoogiArtifactRow>(FName(*Key), TEXT("ZoogiArtifacts"), false) : nullptr)
			{
				T.Rows[i] = *Row;
				T.Sources[i] = TEXT("DataTable");
			}
			T.Rows[i].Id = (EZoogiArtifact)i;
			const bool bSame = FZoogiArtifactRow::StaticStruct()->CompareScriptStruct(&T.Rows[i], &BuiltInArtifactRows()[i], PPF_None);
			if (T.Sources[i] != TEXT("BuiltIn")) { ++NData; if (bSame) ++NSame; }
			Summary += FString::Printf(TEXT(" %s=%s%s"), *Key, *T.Sources[i], T.Sources[i] == TEXT("BuiltIn") ? TEXT("") : (bSame ? TEXT("(identical)") : TEXT("(TUNED)")));
			const FZoogiArtifactRow& X = T.Rows[i];
			UE_LOG(LogZoogi, Log, TEXT("ArtifactRow %s [%s]: launch %.3f mass %.3f radius %.1f push %.1f chain %d pull %.2f s / %.0f status %d | guard kb %.3f mass %.3f reflect %.1f attacker %.3f | fx %s / %s"),
				*Key, *T.Sources[i], X.LaunchSpeedMul, X.MassMul, X.ImpactRadius, X.ImpactPush, X.ChainCount, X.PullTime, X.PullAccel, X.StatusTurns,
				X.GuardKnockbackMul, X.GuardMassMul, X.GuardReflectDeg, X.GuardAttackerMul, *X.ImpactNiagara.ToString(), *X.AuraNiagara.ToString());
		}
		UE_LOG(LogZoogi, Log, TEXT("Artifacts:%s  [%d/%d from data, %d identical to the C++ table]"), *Summary, NData, (int32)EZoogiArtifact::MAX - 1, NSame);
		return T;
	}

	const FZoogiArtifactTableData& ArtifactTable()
	{
		static FZoogiArtifactTableData T = BuildArtifactTable();
		return T;
	}
}

const FZoogiArtifactRow& ZoogiArtifacts::Get(EZoogiArtifact A)
{
	const TArray<FZoogiArtifactRow>& Rows = ArtifactTable().Rows;
	return Rows[FMath::Clamp((int32)A, 0, Rows.Num() - 1)];
}

const FZoogiArtifactRow& ZoogiArtifacts::BuiltIn(EZoogiArtifact A)
{
	const TArray<FZoogiArtifactRow>& Rows = BuiltInArtifactRows();
	return Rows[FMath::Clamp((int32)A, 0, Rows.Num() - 1)];
}

FString ZoogiArtifacts::Source(EZoogiArtifact A)
{
	const TArray<FString>& S = ArtifactTable().Sources;
	return S[FMath::Clamp((int32)A, 0, S.Num() - 1)];
}

FZoogiArtifactRow UZoogiArtifactLibrary::GetBuiltInArtifact(EZoogiArtifact Artifact) { return ZoogiArtifacts::BuiltIn(Artifact); }
FZoogiArtifactRow UZoogiArtifactLibrary::GetArtifact(EZoogiArtifact Artifact) { return ZoogiArtifacts::Get(Artifact); }
FLinearColor UZoogiArtifactLibrary::GetArtifactColor(EZoogiArtifact Artifact) { return ZoogiArtifacts::Color(Artifact); }
FString UZoogiArtifactLibrary::GetArtifactKey(EZoogiArtifact Artifact) { return ZoogiArtifacts::Key(Artifact); }
FString UZoogiArtifactLibrary::GetArtifactSource(EZoogiArtifact Artifact) { return ZoogiArtifacts::Source(Artifact); }

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
	// Oct 6 (bible roster): affinity follows the power each character inherited, so the balance tuned per power
	// (Ember x0.3 burst, Storm stun-on-impact, Void stun without freeze, ...) carries over unchanged.
	// Old ids are kept for anything still referring to them. FName compares case-insensitively.
	if (const FZoogiDef* D = ZoogiUtil::FindZoogi(Id))
	{
		switch (D->PowerKind)
		{
		case EZoogiPower::Explosion: return EZoogiArtifact::Ember;                                  // Vex
		case EZoogiPower::StaticShock: case EZoogiPower::Ricochet: return EZoogiArtifact::Storm;   // Luma, Blue
		case EZoogiPower::LockOn: case EZoogiPower::WolfPack: return EZoogiArtifact::Frost;        // Moss, Tavi
		case EZoogiPower::ShadowStun: case EZoogiPower::RuneSeal: return EZoogiArtifact::Void;     // Dark Zoogy, Sera
		case EZoogiPower::BandageBind: return EZoogiArtifact::Titan;                               // Master Orin
		default: break;
		}
	}
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
