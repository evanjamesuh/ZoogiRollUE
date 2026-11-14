# b90 (Evan 15:18): duel v2 replaces the clash button-mash (ZoogiDuelV2.cpp); unspent SP stays until your next turn so a rival can
# spend it in a duel; NEWS popup = the tester checklist (Evan 15:32).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiActionFun.h', [
 ('		int32 DHover = -1, DCpuWant = 0, DAttG = -1, DDefG = -1; float DCpuPickAt = 2.f, DCpuTapAt = -1.f, DDefTapAt = -1.f; // b67: the CPU goes through the duel menu + timing taps\n',
  '		int32 DHover = -1, DCpuWant = 0, DAttG = -1, DDefG = -1; float DCpuPickAt = 2.f, DCpuTapAt = -1.f, DDefTapAt = -1.f; // b67: the CPU goes through the duel menu + timing taps\n		bool bV2 = false, bVGraded = false, bVDefend = false, bVCounter = false, bVLanded = false, bVCountered = false; int32 VPick = -1, VRivalWant = 3; float VGradeAt = 0.f; // b90 duel v2\n'),
 ('int32 ClashWins = 0; };', 'int32 ClashWins = 0; float HP = 100.f; }; // b90: duel HP (kept for the match)'),
 ('	void DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);\n	FBox2D DuelBtn[4];',
  '''	void DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	// b90 duel v2 (ZoogiDuelV2.cpp): circle match -> SP -> move -> the rival's SP answer -> HP / knockout
	void TickDuelV2(AZoogiGameMode* G, float RealDt);
	void DrawDuelV2(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	void DuelV2Tap(AZoogiGameMode* G);
	void DuelV2Pick(AZoogiGameMode* G, int32 i);
	void DuelV2Damage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* What);
	void DuelV2End(AZoogiGameMode* G, bool bFought);
	int32 DuelSp(AZoogiGameMode* G, int32 P) const;
	bool DuelSpend(AZoogiGameMode* G, int32 P, int32 N);
	float DuelHP(int32 P) const;
	FBox2D DuelBtn[4];'''),
])
ed(H + 'ZoogiCards.h', [
 ('	void AddEnergyBonus(int32 P, int32 Amount) {', '	/** b90: duel SP (circle match) - straight into P\'s Card Energy, usable right now */\n	void AddEnergyNow(int32 P, int32 Amount) { if (Players.IsValidIndex(P) && Amount > 0) Players[P].Energy += Amount; }\n	void AddEnergyBonus(int32 P, int32 Amount) {'),
])
ed(H + 'ZoogiCardSubsystem.h', [
 ('	void AddEnergyBonus(int32 P, int32 Amount) { RulesV.AddEnergyBonus(P, Amount); } // b87',
  '	void AddEnergyBonus(int32 P, int32 Amount) { RulesV.AddEnergyBonus(P, Amount); } // b87\n	void AddEnergyNow(int32 P, int32 Amount) { RulesV.AddEnergyNow(P, Amount); } // b90'),
])
ed(P + 'ZoogiCards.cpp', [
 ('	S.Energy = 0; // unspent Card Energy does not carry over (refresh, not bank)',
  '	static const bool bZeroOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSpZeroOffTurn"));\n	if (bZeroOff) S.Energy = 0; // unspent Card Energy does not carry over (refresh, not bank); b90 (Evan): it stays until your next Standby refresh so a rival can spend it in a duel'),
])
ed(P + 'ZoogiDuel.cpp', [
 ('void UZoogiFunSys::StartDuel(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def)\n{',
  'namespace ZoogiDuelV2 { bool On(); }\n\nvoid UZoogiFunSys::StartDuel(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def)\n{'),
 ('	Clash.bDuel = true; Clash.DStage = 0; Clash.DAge = 0.f; Clash.bGo = false;',
  '	Clash.bDuel = true; Clash.DStage = 0; Clash.DAge = 0.f; Clash.bGo = false;\n	Clash.bV2 = ZoogiDuelV2::On(); // b90: circle match -> SP -> move -> rival SP answer -> HP'),
 ('''void UZoogiFunSys::DuelTap(AZoogiGameMode* G)
{
	if (!Clash.bDuel) return;''', '''void UZoogiFunSys::DuelTap(AZoogiGameMode* G)
{
	if (!Clash.bDuel) return;
	if (Clash.bV2) { DuelV2Tap(G); return; } // b90'''),
 ('''void UZoogiFunSys::TickDuel(AZoogiGameMode* G, float RealDt)
{''', '''void UZoogiFunSys::TickDuel(AZoogiGameMode* G, float RealDt)
{
	if (Clash.bV2) { TickDuelV2(G, RealDt); return; } // b90'''),
 ('''void UZoogiFunSys::DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{''', '''void UZoogiFunSys::DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{
	if (Clash.bV2) { DrawDuelV2(C, G, Wd, Ht, U); return; } // b90'''),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''		if (Clash.bDuel) // b66: duel - menu buttons by touch, REACTION = skill / timing tap
		{''', '''		if (Clash.bDuel) // b66: duel - menu buttons by touch, REACTION = skill / timing tap
		{
			if (Clash.bV2 && ((Clash.DStage == 11 && Clash.bHumanAtt) || (Clash.DStage == 12 && Clash.bHumanDef))) // b90: the move / answer menus
			{
				if (Screen.X >= 0.f) for (int32 i = 0; i < 4; ++i) if (DuelBtn[i].bIsValid && DuelBtn[i].IsInside(Screen)) { DuelV2Pick(G, i); return true; }
				return true;
			}'''),
 ('	if (Clash.bOn) return (Clash.bHumanAtt || Clash.bHumanDef) && !Clash.bFinish && (!Clash.bDuel || Clash.DStage <= 2) ? 1 : 0;',
  '	if (Clash.bOn) return (Clash.bHumanAtt || Clash.bHumanDef) && !Clash.bFinish && (!Clash.bDuel || (Clash.bV2 ? (Clash.DStage <= 10 && Clash.bHumanAtt) : Clash.DStage <= 2)) ? 1 : 0; // b90: REACTION = the circle match tap'),
])
ed(P + 'ZoogiSuperFun.cpp', [
 ('void UZoogiFunSys::SuperContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit, bool bFirst)\n{',
  'namespace ZoogiDuelV2 { bool ReplacesClash(); }\n\nvoid UZoogiFunSys::SuperContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit, bool bFirst)\n{'),
 ('	if (bClashOn && !bClashThisShot && ((bPowered && bCanAnswer) || bClashTest)) { StartClash(G, Sh, R); return; }',
  '	if (bClashOn && !bClashThisShot && ((bPowered && bCanAnswer) || bClashTest)) { if (ZoogiDuelV2::ReplacesClash()) StartDuel(G, Sh, R); else StartClash(G, Sh, R); return; } // b90 (Evan): no more button-mash - the new duel'),
])
# NEWS = tester checklist
ed(P + 'ZoogiFront.cpp', [
 ('''			{ TEXT("NEW"), TEXT("THE NEW HOME IS HERE"), TEXT("Your Zoogi stands in the arena as an Aurex. Spin it, swap it in INVENTORY, open chests and grab your DAILY LOGIN reward.") },
			{ TEXT("SEASON"), TEXT("FALL SEASON - COMIC LOGO"), TEXT("The ZOOGI ROLL ARENA logo changes every season: classic in spring, fire in summer, comic in fall, ice in winter.") },
			{ TEXT("ROYALE"), TEXT("TUNE YOUR STICKS"), TEXT("Tap the cog in Battle Royale: swipe / stick / move sensitivity, invert look, hide either stick.") },
			{ TEXT("CLASSIC"), TEXT("CLEAN AIM VIEW"), TEXT("Touch anywhere and drag back to launch. Bank shots show a small preview where the ball hits the wall.") },''',
  '''			// b90 (Evan: "news update with what we actually changed so I can use it as a checklist - tell folks to try it out and give feedback")
			{ TEXT("TEST"), TEXT("TRY IT - SEND FEEDBACK"), TEXT("Play a few matches and check off each item below. Tell us what feels great and what is broken.") },
			{ TEXT("DUEL"), TEXT("NEW ZOOGI vs ZOOGI DUEL"), TEXT("Circle match: GOOD +3 SP, PERFECT +5 SP. Pick ATTACK or SKILL; the rival spends SP to DEFEND / COUNTER. 0 HP = KNOCKOUT.") },
			{ TEXT("WOLF"), TEXT("HOLOGRAM WOLF PACK"), TEXT("3 SP = 3 purple wolves. Launch, tap SUPERPOWER, pick up to 3 targets - each wolf explodes on its target.") },
			{ TEXT("ANIME"), TEXT("CLIPS IN YOUR AVATAR"), TEXT("Skill summons, combo clips and comic panels play inside that player's avatar. Criticals play on the spire scoreboard.") },
			{ TEXT("SKILLS"), TEXT("SKILL CHANGES"), TEXT("Energy Shockwave pulls in then blasts. Water Cannon = DRIVE force. GUARD = unbreakable bumper. All skills unlocked.") },
			{ TEXT("ARENA"), TEXT("ORB GENERATORS + COSMIC"), TEXT("The spire throws new orbs every round, plus purple orbs (+2 SP). Cosmic: no ship, rune markers rise from the floor.") },
			{ TEXT("BALANCE"), TEXT("FAIRER METERS"), TEXT("One crystal per turn (CPUs too), the SUPER meter fills slower, PRECISION no longer gets stuck.") },'''),
 ('''		const FVector2D Sz(FMath::Min(1300.f, DW - 120.f), 110.f + N * 150.f);
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("NE"), TEXT("WS") });
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D R0 = TL + FVector2D(30.f, 70.f + i * 150.f);''',
  '''		const float RowH = FMath::Clamp((DH - 260.f) / N, 112.f, 150.f); // b90: the checklist fits the screen
		const FVector2D Sz(FMath::Min(1300.f, DW - 120.f), 110.f + N * RowH);
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("NE"), TEXT("WS") });
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D R0 = TL + FVector2D(30.f, 70.f + i * RowH);'''),
 ('			if (i < N - 1) ZRect(Out, G, L + 4, R0 + FVector2D(0.f, 136.f), FVector2D(Sz.X - 60.f, 3.f), Ink.CopyWithNewOpacity(0.2f));',
  '			if (i < N - 1) ZRect(Out, G, L + 4, R0 + FVector2D(0.f, RowH - 14.f), FVector2D(Sz.X - 60.f, 3.f), Ink.CopyWithNewOpacity(0.2f));'),
])
print('b90 ok')
