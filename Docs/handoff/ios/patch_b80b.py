# b80b (Evan 01:43: "Awaken power should last a lot longer"): an Awakening carries on through that player's next 3 turns (-ZoogiAwakenTurns=N, 0 = old one shot)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiCardGlue.cpp', [
 ('''void AZoogiGameMode::CardsBeginStandby()
{''',
  '''// b80 (Evan: "Awaken power should last a lot longer"): turns of Awakening still to come per player (re-infused at each Standby)
namespace { TWeakObjectPtr<AZoogiGameMode> GAwakeG; TMap<int32, int32> GAwakeLeft; }
static int32 ZoogiAwakenExtraTurns() { static int32 N = -1; if (N < 0) { N = 3; FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenTurns="), N); N = FMath::Clamp(N, 0, 20); } return N; }

void AZoogiGameMode::CardsBeginStandby()
{'''),
 ('''	AwakenChoiceText.Reset();
	bStandbyAwaken = false;''',
  '''	AwakenChoiceText.Reset();
	bStandbyAwaken = false;
	if (GAwakeG.Get() != this) { GAwakeLeft.Reset(); GAwakeG = this; }
	if (AZoogiBall* Bw = GetActiveBall())
		if (int32* Left = GAwakeLeft.Find(Bw->PlayerIndex))
			if (*Left > 0 && Bw->Artifact)
			{	// b80: still awakened - the affinity stays infused for this turn's shot too
				--(*Left);
				bStandbyAwaken = true;
				const EZoogiArtifact Aw = ZoogiArtifacts::Affinity(Bw->Def.Id);
				Bw->Artifact->SetInfused(Aw);
				AddPopup(*Left > 0 ? FString::Printf(TEXT("AWAKENED  -  %d MORE TURNS"), *Left) : FString(TEXT("AWAKENED  -  LAST TURN")), Bw->BallLocation() + FVector(0, 0, 190.f), ZoogiArtifacts::Color(Aw));
				UE_LOG(LogZoogi, Log, TEXT("AWAKEN carries on for P%d (%d turns after this)"), Bw->PlayerIndex + 1, *Left);
				if (*Left <= 0) GAwakeLeft.Remove(Bw->PlayerIndex);
			}'''),
 ('''	AwakenCharge[B->PlayerIndex] = 0;
	bStandbyAwaken = true;''',
  '''	AwakenCharge[B->PlayerIndex] = 0;
	bStandbyAwaken = true;
	if (GAwakeG.Get() != this) { GAwakeLeft.Reset(); GAwakeG = this; }
	if (ZoogiAwakenExtraTurns() > 0) GAwakeLeft.Add(B->PlayerIndex, ZoogiAwakenExtraTurns()); // b80: lasts this turn + the next N of this player's'''),
])
print('b80b ok')
