# b71 (Claude): FxTune.csv - per-effect Size / Height / Duration / Tint multipliers for every Fab Niagara effect (live reload in the editor)
import os
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style')
def ed(rel, pairs, inc=None, after=None):
    p = os.path.join(ROOT, rel)
    raw = open(p, 'rb').read(); bom = raw[:3] == b'\xef\xbb\xbf'; s = raw.decode('utf-8-sig'); nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in pairs:
        a = a.replace('\n', nl); b = b.replace('\n', nl)
        assert s.count(a) == 1, (rel, a[:80], s.count(a)); s = s.replace(a, b)
    if inc and inc not in s:
        i = s.index(after); j = s.index(nl, i) + len(nl); s = s[:j] + inc + nl + s[j:]
    open(p, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='').write(s)

ed('Source/ZoogiRoll/Private/ZoogiFabFX.cpp', [
 ('AZoogiFabFX* AZoogiFabFX::Play(UWorld* W, const TCHAR* Path, const FVector& Loc, float Scale, float RealLife, AActor* InFollow, const FVector& InOffset, const FRotator& Rot)\n{\n	if (!W) return nullptr;\n',
  '''// b71 (Evan: "how can I control how this effect looks?"): Content/Data/FxTune.csv - one row per Niagara system (file name, e.g.
// NS_Water_hit): Size, Height (extra vertical stretch), Duration multipliers and an optional tint (R,G,B; 0 0 0 = none). The file is
// re-read when it changes (editor / desktop), so a tweak shows on the next cast. -ZoogiNoFxTune ignores it.
namespace
{
	struct FFxTuneRow { float Size = 1.f, Height = 1.f, Life = 1.f; FLinearColor Tint = FLinearColor::Black; };
	const FFxTuneRow* FxTuneFor(const FString& System)
	{
		static TMap<FString, FFxTuneRow> Rows; static FDateTime Stamp; static double NextCheck = 0.0;
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFxTune"));
		if (bOff) return nullptr;
		const double Now = FPlatformTime::Seconds();
		if (Now >= NextCheck)
		{
			NextCheck = Now + 1.0;
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/FxTune.csv"));
			const FDateTime T = IFileManager::Get().GetTimeStamp(*Path);
			if (T != Stamp)
			{
				Stamp = T; Rows.Reset();
				TArray<FString> Lines;
				if (FFileHelper::LoadFileToStringArray(Lines, *Path))
					for (const FString& L0 : Lines)
					{
						const FString L = L0.TrimStartAndEnd();
						if (L.IsEmpty() || L.StartsWith(TEXT("#")) || L.StartsWith(TEXT("System"))) continue;
						TArray<FString> C; L.ParseIntoArray(C, TEXT(","), false);
						if (C.Num() < 2) continue;
						FFxTuneRow R;
						auto F = [&](int32 i, float D) { return C.IsValidIndex(i) && !C[i].TrimStartAndEnd().IsEmpty() ? FCString::Atof(*C[i]) : D; };
						R.Size = FMath::Clamp(F(1, 1.f), 0.05f, 10.f); R.Height = FMath::Clamp(F(2, 1.f), 0.05f, 10.f); R.Life = FMath::Clamp(F(3, 1.f), 0.1f, 10.f);
						R.Tint = FLinearColor(F(4, 0.f), F(5, 0.f), F(6, 0.f), 1.f);
						Rows.Add(C[0].TrimStartAndEnd(), R);
					}
				UE_LOG(LogZoogi, Log, TEXT("FABFX tune: %d rows from Data/FxTune.csv"), Rows.Num());
			}
		}
		return Rows.Find(System);
	}
}

AZoogiFabFX* AZoogiFabFX::Play(UWorld* W, const TCHAR* Path, const FVector& Loc, float Scale, float RealLife, AActor* InFollow, const FVector& InOffset, const FRotator& Rot)
{
	if (!W) return nullptr;
	const FFxTuneRow* Tune = FxTuneFor(FPaths::GetBaseFilename(FString(Path)));
	float HeightMul = 1.f;
	if (Tune) { Scale *= Tune->Size; HeightMul = Tune->Height; RealLife *= Tune->Life; }
'''),
 ('	AZoogiFabFX* A = W->SpawnActor<AZoogiFabFX>(AZoogiFabFX::StaticClass(), FTransform(Rot, Loc, FVector(FMath::Max(0.01f, Scale))), P);',
  '	AZoogiFabFX* A = W->SpawnActor<AZoogiFabFX>(AZoogiFabFX::StaticClass(), FTransform(Rot, Loc, FVector(FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale * HeightMul))), P);'),
 ('	A->NC->Activate(true);\n', '	A->NC->Activate(true);\n	if (Tune && (Tune->Tint.R + Tune->Tint.G + Tune->Tint.B) > 0.f) { A->NC->SetVariableLinearColor(FName(TEXT("User.Color")), Tune->Tint); A->NC->SetVariableLinearColor(FName(TEXT("Color")), Tune->Tint); A->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), Tune->Tint); }\n'),
], '#include "HAL/FileManager.h"\n#include "Misc/FileHelper.h"\n#include "Misc/CommandLine.h"\n#include "Misc/Parse.h"\n#include "HAL/PlatformTime.h"', '#include "Misc/Paths.h"')
names = 'NS_Apple NS_Apple_bombs NS_Apple_of_Eden NS_Energy_buff NS_Energy_shockwave NS_Fire_meteors NS_Magic_ray NS_Magic_rays_attack NS_Meteor_hit NS_Meteor_hit_2 NS_Sword_AoE NS_Sword_crater NS_Water_cannon NS_Water_hit NS_Wood_shield'.split()
rows = ['# Zoogi Roll effect tuning (b71). One row per Niagara effect (the file name without the folder). Multipliers: 1 = as authored.',
        '# Size = overall scale, Height = extra vertical stretch, Duration = how long it stays, TintR/G/B = colour (0,0,0 = leave as is).',
        '# Edit and save - the game re-reads this file within a second (editor); phone builds pick it up on the next TestFlight build.',
        'System,Size,Height,Duration,TintR,TintG,TintB']
for n in names:
    rows.append('%s,%s,%s,1,0,0,0' % (n, '1.8' if n == 'NS_Water_hit' else '1', '1.6' if n == 'NS_Water_hit' else '1'))
for v in range(1, 13): rows.append('NS_MeteorRainV%02d,1,1,1,0,0,0' % v)
open(os.path.join(ROOT, 'Content/Data/FxTune.csv'), 'w', newline='\r\n').write('\n'.join(rows) + '\n')
print('patched b71 FxTune')
