# b120g (Evan 19:19: "I like build 120 combo system but to make it better don't start the skills or their effects until after launch and
# SUPERPOWER is tapped - first tap activates cast 1 relative to the player's position, tap 2 activates cast 2 and so on"):
#  - CAST n! now ARMS the ticked skills: the cards are committed (SP spent, popup "COMBO ARMED"), no effect yet
#    (classic launch cards like Heavy Roll still apply at once - they shape the launch itself)
#  - after the launch each SUPERPOWER tap casts the next armed skill from where the Zoogi is now (ZoogiSkills::AutoCast: nearest
#    targets / spots round the ball); PRECISION starts its volley on its tap. SUPERPOWER reads "CAST 1/3!" etc.
#  - armed skills stay until used (like banked wolves); a new match clears them
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
# card subsystem: no PRECISION volley while arming a combo
CS = 'Private/ZoogiCardSubsystem.cpp'
ed1(CS, '''EZoogiCardResult UZoogiCardSubsystem::Commit(AZoogiGameMode* G, FString& OutChoice)''', '''bool GZoogiComboArming = false; // b120g: a COMBO arm commits the card, its effect waits for a SUPERPOWER tap after the launch
EZoogiCardResult UZoogiCardSubsystem::Commit(AZoogiGameMode* G, FString& OutChoice)''')
ed1(CS, '''		if (K.Card.CardId == FName(TEXT("precision")) && G->Balls.IsValidIndex(P) && G->Balls[P]) // b119''', '''		if (K.Card.CardId == FName(TEXT("precision")) && !GZoogiComboArming && G->Balls.IsValidIndex(P) && G->Balls[P]) // b119''')
# game mode: arm a card for the combo
GH = 'Public/ZoogiGameMode.h'
ed1(GH, '''	bool CardsTapPlay(int32 HandSlot);''', '''	bool CardsTapPlay(int32 HandSlot);
	/** b120g COMBO: commit the card in this slot (SP spent) without casting it; true + its id when armed */
	bool CardsArmCombo(int32 HandSlot, FName& OutId);''')
GL = 'Private/ZoogiCardGlue.cpp'
ed1(GL, '''// ---- Awaken charge (Evan, Oct 20)''', '''extern bool GZoogiComboArming;
bool AZoogiGameMode::CardsArmCombo(int32 Slot, FName& OutId)
{	// b120g: like CardsTapPlay, but no target pick and no cast - the skill waits for a SUPERPOWER tap after the launch
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	AZoogiBall* B = GetActiveBall();
	if (!C || !B || !CardsPlayWindowOpen() || !CardsSlotPlayableNow(Slot)) return false;
	const FZoogiCardPlayerState* Ps = C->Rules().Player(B->PlayerIndex);
	if (!Ps || !Ps->Hand.IsValidIndex(Slot)) return false;
	OutId = Ps->Hand[Slot].CardId;
	const FZoogiCardDef* Sd = C->Def(OutId);
	if (Sd && Sd->Cost > Ps->Energy) return false;
	C->AllowAnotherCommit(this);
	C->ClearSelection(this);
	if (C->ToggleSlot(this, Slot) != EZoogiCardResult::Ok) return false;
	FString Choice;
	GZoogiComboArming = true;
	const EZoogiCardResult R = C->Commit(this, Choice);
	GZoogiComboArming = false;
	if (R != EZoogiCardResult::Ok) { C->ClearSelection(this); return false; }
	C->AllowAnotherCommit(this);
	CardsAddAwakenCharge(B->PlayerIndex, 1, TEXT("combo arm"));
	AddFeed(FString::Printf(TEXT("P%d %s  COMBO ARMED %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice.Replace(TEXT("CARDS "), TEXT("")).TrimStart()), FLinearColor(1.f, 0.82f, 0.15f));
	UE_LOG(LogZoogi, Log, TEXT("CARDS combo arm P%d slot %d: %s"), B->PlayerIndex + 1, Slot, *Choice);
	return true;
}

// ---- Awaken charge (Evan, Oct 20)''')
# skills: the armed combo queue
SH = 'Public/ZoogiSkills.h'
ed1(SH, '''	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);''', '''	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);
	/** b120g COMBO: skills armed before the launch; each SUPERPOWER tap after the launch casts the next one from the Zoogi */
	ZOOGIROLL_API void ComboArm(AZoogiGameMode* G, int32 Player, FName Id);
	ZOOGIROLL_API int32 ComboCount(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API int32 ComboTotal(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool ComboReady(const AZoogiGameMode* G);
	ZOOGIROLL_API bool ComboGo(AZoogiGameMode* G);''')
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''	void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N)''', '''	// ---- b120g COMBO: armed skills, cast one per SUPERPOWER tap after the launch
	static TWeakObjectPtr<const AZoogiGameMode> GComboG;
	static TMap<int32, TArray<FName>> GCombo;
	static TMap<int32, int32> GComboTotal, GComboDone;
	static void ComboResetFor(const AZoogiGameMode* G) { if (GComboG.Get() != G) { GCombo.Reset(); GComboTotal.Reset(); GComboDone.Reset(); GComboG = G; } }
	void ComboArm(AZoogiGameMode* G, int32 Player, FName Id)
	{
		if (!G) return;
		ComboResetFor(G);
		TArray<FName>& Q = GCombo.FindOrAdd(Player);
		if (Q.Num() == 0) { GComboTotal.Add(Player, 0); GComboDone.Add(Player, 0); }
		Q.Add(Id); GComboTotal.FindOrAdd(Player) += 1;
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d armed %s (%d waiting)"), Player + 1, *Id.ToString(), Q.Num());
	}
	int32 ComboCount(const AZoogiGameMode* G, int32 Player) { const TArray<FName>* Q = (G && GComboG.Get() == G) ? GCombo.Find(Player) : nullptr; return Q ? Q->Num() : 0; }
	int32 ComboTotal(const AZoogiGameMode* G, int32 Player) { const int32* T = (G && GComboG.Get() == G) ? GComboTotal.Find(Player) : nullptr; return T ? *T : 0; }
	bool ComboReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && !Picking() && ComboCount(G, B->PlayerIndex) > 0;
	}
	bool ComboGo(AZoogiGameMode* G)
	{
		if (!ComboReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		TArray<FName>& Q = GCombo.FindOrAdd(B->PlayerIndex);
		const FName Id = Q[0]; Q.RemoveAt(0);
		const int32 Nth = ++GComboDone.FindOrAdd(B->PlayerIndex);
		bool bOk = false;
		if (Id == FName(TEXT("precision"))) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }
		else bOk = AutoCast(G, Id); // relative to where the Zoogi is now: nearest targets / spots round the ball
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("CAST %d!"), Nth), Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(), FLinearColor(1.f, 0.82f, 0.15f), 1.2f, 1.f);
		ZoogiHaptics::Kick(0.6f, G->GetWorld(), TEXT("combo cast"));
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d cast %d: %s (%s, %d left)"), B->PlayerIndex + 1, Nth, *Id.ToString(), bOk ? TEXT("ok") : TEXT("failed"), Q.Num());
		if (Q.Num() == 0) { GComboTotal.Remove(B->PlayerIndex); GComboDone.Remove(B->PlayerIndex); }
		return true;
	}

	void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N)''')
ed1(S, '''				if (bNewMatch || (G && G->Phase == EZoogiPhase::Menu)) GWolfArmed.Reset();''', '''				if (bNewMatch || (G && G->Phase == EZoogiPhase::Menu)) { GWolfArmed.Reset(); GCombo.Reset(); GComboTotal.Reset(); GComboDone.Reset(); } // b120g: the combo goes with it''')
# P5 menu: CAST arms instead of playing
C = 'Private/ZoogiP5Command.cpp'
ed1(C, '''			GS.ComboQueue.Reset();
			if (PSq) for (int32 Ri : GS.ComboSel) if (W.Skills.IsValidIndex(Ri) && PSq->Hand.IsValidIndex(W.Skills[Ri].Slot)) GS.ComboQueue.Add(PSq->Hand[W.Skills[Ri].Slot].CardId);
			GS.ComboTurn = G->TurnCounter; GS.ComboNextAt = 0.0;
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO cast %d: %s"), GS.ComboQueue.Num(), *FString::JoinBy(GS.ComboQueue, TEXT(", "), [](const FName& N) { return N.ToString(); }));
			GS.bCombo = false; GS.ComboSel.Reset();
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.35f);
			if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(TEXT("COMBO!"), FString::Printf(TEXT("%d SKILLS AT ONCE"), GS.ComboQueue.Num()), FLinearColor(1.f, 0.82f, 0.15f), 1.6f, 1.f);
			Dismiss(G);
			return;''', '''			// b120g (Evan: "don't start the skills or their effects until after launch and SUPERPOWER is tapped"): CAST arms them -
			// the cards are committed now, each SUPERPOWER tap after the launch casts the next one from the Zoogi
			TArray<FName> Ids;
			if (PSq) for (int32 Ri : GS.ComboSel) if (W.Skills.IsValidIndex(Ri) && PSq->Hand.IsValidIndex(W.Skills[Ri].Slot)) Ids.Add(PSq->Hand[W.Skills[Ri].Slot].CardId);
			GS.ComboQueue.Reset(); GS.bCombo = false; GS.ComboSel.Reset();
			int32 Armed = 0;
			for (const FName& Id : Ids)
			{
				const FZoogiCardPlayerState* PSn = (CSq && Hq) ? CSq->Rules().Player(Hq->PlayerIndex) : nullptr;
				int32 Slot = INDEX_NONE;
				if (PSn) for (int32 k = 0; k < PSn->Hand.Num(); ++k) if (PSn->Hand[k].CardId == Id) { Slot = k; break; }
				FName Got;
				if (Slot == INDEX_NONE || !G->CardsArmCombo(Slot, Got)) { UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO could not arm %s"), *Id.ToString()); continue; }
				if (ZoogiSkills::IsSkill(Got) || Got == FName(TEXT("precision"))) { ZoogiSkills::ComboArm(G, Hq->PlayerIndex, Got); ++Armed; } // classic launch cards apply at once
			}
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO armed %d of %d"), Armed, Ids.Num());
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.35f);
			if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(FString::Printf(TEXT("COMBO x%d ARMED!"), Armed), TEXT("LAUNCH, THEN TAP SUPERPOWER - ONE CAST PER TAP"), FLinearColor(1.f, 0.82f, 0.15f), 2.f, 1.f);
			Dismiss(G);
			return;''')
# pad: SUPERPOWER fires the next armed cast while rolling
CP = 'Private/ZoogiClassicPad.cpp'
ed1(CP, '''			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::WolfReady(G) || ZoogiSkills::BoltReady(G)) : ReactionNeeded());''',
'''			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::ComboReady(G) || ZoogiSkills::WolfReady(G) || ZoogiSkills::BoltReady(G)) : ReactionNeeded());''')
ed1(CP, '''				else if (bPower && ZoogiSkills::WolfReady(G)) ZoogiSkills::WolfGo(G);''', '''				else if (bPower && ZoogiSkills::ComboReady(G)) ZoogiSkills::ComboGo(G); // b120g: the next armed COMBO cast
				else if (bPower && G->Phase == EZoogiPhase::Aiming && G->GetActiveBall() && !G->GetActiveBall()->bCPU && ZoogiSkills::ComboCount(G, G->GetActiveBall()->PlayerIndex) > 0 && !ZoogiSkills::WolfReady(G) && !ZoogiSkills::BoltReady(G))
				{ if (const AZoogiBall* Bc = G->GetActiveBall()) G->PublicPopup(TEXT("LAUNCH, THEN TAP SUPERPOWER TO CAST"), Bc->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.82f, 0.15f)); }
				else if (bPower && ZoogiSkills::WolfReady(G)) ZoogiSkills::WolfGo(G);''')
ed1(CP, '''			Label(bPower ? ((PadGM() && ZoogiSkills::WolfReady(PadGM())) ?''', '''			Label(bPower ? ((PadGM() && ZoogiSkills::ComboReady(PadGM()) && PadGM()->GetActiveBall()) ? FString::Printf(TEXT("CAST %d/%d!"), ZoogiSkills::ComboTotal(PadGM(), PadGM()->GetActiveBall()->PlayerIndex) - ZoogiSkills::ComboCount(PadGM(), PadGM()->GetActiveBall()->PlayerIndex) + 1, ZoogiSkills::ComboTotal(PadGM(), PadGM()->GetActiveBall()->PlayerIndex)) : (PadGM() && ZoogiSkills::WolfReady(PadGM())) ?''')
print('b120g ok')
