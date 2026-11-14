# b107b: the defend window gets the timing ring, 0.7 s black and white, DIRECT / AROUND, GUARD (holo dome), the loser's fly-back
# b107c: every CPU gets its own random skill hand each match
import os, shutil
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
shutil.copy(os.path.expanduser('~/mnt/dev/ios/lab/ZoogiDefendWin.cpp'), R + 'Private/ZoogiDefendWin.cpp')
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiActionFun.h', [
 ('''	struct FDefWin { bool bOn = false; TWeakObjectPtr<AZoogiBall> A, R; double T0 = 0.0; bool bDecided = false, bDefend = false, bContact = false, bCamPlaced = false; double ContactAt = 0.0, GameT0 = 0.0; float SavedMass = 0.f, CpuAt = -1.f, GameLimit = 0.6f; FVector CamSide = FVector::RightVector; TWeakObjectPtr<AActor> Cam; };''',
  '''	struct FDefWin { bool bOn = false; TWeakObjectPtr<AZoogiBall> A, R; double T0 = 0.0; bool bDecided = false, bDefend = false, bContact = false, bCamPlaced = false; double ContactAt = 0.0, GameT0 = 0.0; float SavedMass = 0.f, CpuAt = -1.f, GameLimit = 0.6f; FVector CamSide = FVector::RightVector; TWeakObjectPtr<AActor> Cam;
		// b107b: the timing ring, black and white, DIRECT / AROUND, the loser's fly-back
		int32 Stage = 0, Grade = -1, Choice = -1; float CpuTapAt = -1.f, CpuChoiceAt = -1.f; double GradeAt = 0.0, BwAt = 0.0; bool bBw = false, bAroundOk = false, bLoserDone = false;
		FBox2D Btn[2] = { FBox2D(ForceInit), FBox2D(ForceInit) }; FBox2D GuardBtn = FBox2D(ForceInit); };'''),
 ('''	void EndDefendWindow(AZoogiGameMode* G, const TCHAR* Why);''', '''	void EndDefendWindow(AZoogiGameMode* G, const TCHAR* Why);
	float DefWinDilation() const;
	void DefWinBW(bool bOn);
	AActor* DefWinAroundTarget(AZoogiGameMode* G) const;
	void DefWinRingTap(AZoogiGameMode* G);
	void DefWinChoose(AZoogiGameMode* G, int32 Choice);
	void DefWinFlyLoser(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);
	void DefWinTap(AZoogiGameMode* G, const FVector2D& Screen);
	void DrawDefendWindow(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);'''),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''	if (DefWin.bOn && !DefWin.bDecided) { if (AZoogiBall* Rd = DefWin.R.Get()) if (!Rd->bCPU) DefendNow(G); return true; } // b106: only the rival can act''',
  '''	if (DefWin.bOn) { DefWinTap(G, Screen); return true; } // b106 / b107b: the ring (turn player), DIRECT / AROUND, GUARD (rival)'''),
 ('''			else if (DefWin.bOn && !DefWin.bContact) Want = 0.12f; // b106: the defend window''',
  '''			else if (DefWin.bOn && !DefWin.bContact) Want = DefWinDilation(); // b106: the defend window (b107b: by stage)'''),
 ('''	if (Clash.bOn) { if (Clash.bDuel) DrawDuel(C, G, Wd, Ht, U); else DrawClash(C, G, Wd, Ht, U); } // b66: duel cutscene UI''',
  '''	if (Clash.bOn) { if (Clash.bDuel) DrawDuel(C, G, Wd, Ht, U); else DrawClash(C, G, Wd, Ht, U); } // b66: duel cutscene UI
	if (DefWin.bOn) DrawDefendWindow(C, G, Wd, Ht, U); // b107b'''),
])
# ---- b107c: random CPU skills
ed('Source/ZoogiRoll/Public/ZoogiSkills.h', [('	ZOOGIROLL_API bool IsSkill(FName Id);',
 '	ZOOGIROLL_API bool IsSkill(FName Id);\n	/** b107c: every skill a match hand can hold (not the SKILL LAB) */\n	ZOOGIROLL_API TArray<FName> AllSkillIds();')])
ed(P + 'ZoogiSkills.cpp', [('	bool IsSkill(FName Id) { return Find(Id) != nullptr; }',
 '	bool IsSkill(FName Id) { return Find(Id) != nullptr; }\n	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0) Out.Add(FName(D.Id)); return Out; }')])
ed(P + 'ZoogiCardSubsystem.cpp', [
 ('#include "ZoogiCardSubsystem.h"\n#include "ZoogiCrackFX.h"\n', '#include "ZoogiCardSubsystem.h"\n#include "ZoogiSkills.h"\n#include "ZoogiCrackFX.h"\n'),
 ('''	for (const AZoogiBall* B : G->Balls) Decks.Add(B && !B->bCPU ? HumanDeck : Deck);''',
  '''	for (const AZoogiBall* B : G->Balls) Decks.Add(B && !B->bCPU ? HumanDeck : Deck);
	// b107c (Evan: "make AI use random skills every match, each AI different random skills"): each CPU gets its own hand of DeckSize
	// distinct skills, drawn fresh every match (the match seed is new each match). Card tests / forced cards keep the starter deck.
	// -ZoogiCpuStarterDeck = the old CPU deck.
	{
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuStarterDeck"));
		TArray<FName> Pool;
		for (const FName& Id : ZoogiSkills::AllSkillIds()) if (Backend->FindDef(Id)) Pool.Add(Id);
		FString ForcedC; FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceCard="), ForcedC);
		if (!bOld && TestCase.IsEmpty() && ForcedC.IsEmpty() && Pool.Num() >= 2)
		{
			const int32 N = FMath::Clamp(Tune.DeckSize, 1, Pool.Num());
			TArray<TArray<FName>> Given;
			for (int32 i = 0; i < G->Balls.Num(); ++i)
			{
				const AZoogiBall* B = G->Balls[i];
				if (!B || !B->bCPU || !Decks.IsValidIndex(i)) continue;
				FRandomStream Rs(HashCombine(GetTypeHash(G->GameSeed), GetTypeHash(0x51C0 + i * 7919)));
				TArray<FName> Hand;
				for (int32 Try = 0; Try < 8; ++Try)
				{
					TArray<FName> Bag = Pool;
					for (int32 k = Bag.Num() - 1; k > 0; --k) Bag.Swap(k, Rs.RandRange(0, k));
					Hand = TArray<FName>(Bag.GetData(), N);
					TArray<FName> Sorted = Hand; Sorted.Sort([](const FName& X, const FName& Y) { return X.LexicalLess(Y); });
					bool bSame = false;
					for (const TArray<FName>& O : Given) if (O == Sorted) { bSame = true; break; }
					if (!bSame) { Given.Add(Sorted); break; }
				}
				Decks[i] = Hand;
				TArray<FString> Nm; for (const FName& Id : Hand) Nm.Add(Id.ToString());
				LogS += FString::Printf(TEXT(" cpu P%d random skills [%s];"), i + 1, *FString::Join(Nm, TEXT(",")));
			}
		}
	}'''),
])
print('b107b ok')
