# Nov 11 (Claude): B251 - every build incl. B245 now closes at launch on Evan's iPhone (signing valid, 22 GB free) -> suspect saved data
# written while he played Voyager on B245: Voyager's game instance loads its SaveGames at start. At startup (before the GI's own Init):
# move every SaveGames/*.sav that is not Zoogi Roll's into SaveGames/VoyagerBackup/, and switch the Voyager GI's LoadGameOnBeginPlay off.
# Writes Saved/ZoogiBoot.txt. -ZoogiKeepVoySaves = off.
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p = S + 'Private/ZoogiVoyagerLevels.cpp'; t = io.open(p, encoding='utf-8-sig').read()
a = '''void UZoogiVoyagerLevelsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
'''
b = '''void UZoogiVoyagerLevelsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiKeepVoySaves")))
	{	// b251: launch-crash guard - Voyager's saved games are set aside at every start (Zoogi Roll's own saves stay)
		FString Report;
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
		TArray<FString> Files; IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.sav")), true, false);
		for (const FString& F : Files)
		{
			if (F.StartsWith(TEXT("Zoogi"), ESearchCase::IgnoreCase)) continue;
			const bool bOk = IFileManager::Get().Move(*(Dir / TEXT("VoyagerBackup") / F), *(Dir / F), true, true);
			Report += FString::Printf(TEXT("moved %s %s\\n"), *F, bOk ? TEXT("ok") : TEXT("FAILED"));
		}
		if (UGameInstance* GI = GetGameInstance())
			if (FBoolProperty* Bp = CastField<FBoolProperty>(GI->GetClass()->FindPropertyByName(TEXT("LoadGameOnBeginPlay"))))
			{ Bp->SetPropertyValue_InContainer(GI, false); Report += TEXT("LoadGameOnBeginPlay off\\n"); }
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER launch guard: %d save files seen\\n%s"), Files.Num(), *Report);
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s launch guard, %d saves\\n%s"), *FDateTime::Now().ToString(), Files.Num(), *Report), *(FPaths::ProjectSavedDir() / TEXT("ZoogiBoot.txt")));
	}
'''
assert t.count(a) == 1; t = t.replace(a, b)
if '#include "Misc/FileHelper.h"' not in t: t = t.replace('#include "Misc/Paths.h"\n', '#include "Misc/Paths.h"\n#include "Misc/FileHelper.h" // b251\n', 1)
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok levels')
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B250"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B251"), TEXT("LAUNCH FIX"), TEXT("Zoogi Roll opens again: old Voyager saved games that stopped the game from starting are set aside at launch.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f); print('ALL OK')
