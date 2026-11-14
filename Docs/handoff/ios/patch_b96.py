# b96 (Evan 18:23: "build this ending and the 2 SP reserve"): duel endings = ONE full-screen clip, never two at once -
#  hit landed -> loser knocked back in its form + full-screen crash clip; DEFEND -> guard dome forms, then a full-screen guard clip;
#  counter only -> the rival's full-screen counter clip (nobody flies); KO -> purple blast + KNOCKOUT + full-screen KO clip;
#  circle missed -> nothing. Every Zoogi keeps a 2 SP duel reserve (refilled on its own turn) that cards can't spend.
#  -ZoogiDuelOldEnding / -ZoogiNoDuelReserve.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiActionFun.h', [
 ('int32 ClashWins = 0; float HP = 100.f; }; // b90: duel HP (kept for the match)',
  'int32 ClashWins = 0; float HP = 100.f; int32 DuelReserve = 2; }; // b90: duel HP (kept for the match); b96: 2 SP duel reserve'),
])
ed(P + 'ZoogiDuelV2.cpp', [
 ('''	return PS ? FMath::Max(0, PS->Energy) : 99; // cards off: SP is no limit
}''', '''	static const bool bNoReserve = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDuelReserve"));
	const int32 Rsv = (!bNoReserve && Players.IsValidIndex(P)) ? Players[P].DuelReserve : 0; // b96: the 2 SP duel reserve
	return PS ? FMath::Max(0, PS->Energy) + Rsv : 99; // cards off: SP is no limit
}'''),
 ('''	if (!CS || !G->CardsOn()) return true;
	return CS->SpendEnergy(P, N);
}''', '''	if (!CS || !G->CardsOn()) return true;
	if (DuelSp(G, P) < N) return false;
	// b96: card SP first, then the duel reserve
	const FZoogiCardPlayerState* PS = CS->Rules().Player(P);
	const int32 FromE = FMath::Min(N, PS ? FMath::Max(0, PS->Energy) : 0);
	if (FromE > 0) CS->SpendEnergy(P, FromE);
	if (N > FromE && Players.IsValidIndex(P)) Players[P].DuelReserve = FMath::Max(0, Players[P].DuelReserve - (N - FromE));
	return true;
}'''),
 ('''	if (UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>())
	{
		FZoogiCombatEvent E; E.Event = FName(TEXT("GuardBlock")); E.Player = Def->PlayerIndex; E.ZoogiName = Def->Def.Name; E.Location = Def->BallLocation(); E.Magnitude = 1.f;
		An->Emit(E);
	}''', '''	// b96: the defender's clip plays full screen once the duel scene ends (one clip at a time - DuelV2End)'''),
 ('''	if (bFought && KOs.Num() == 0 && !Clash.bVDefend && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoLoser")) && (Clash.VDmgA > 0.f || Clash.VDmgD > 0.f))''',
  '''	// b96 (Evan): one ending, at most one full-screen clip - KO > DEFENDED > hit landed (loser flies + crash clip) > counter only
	static const bool bOldEnding = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelOldEnding"));
	enum class EEnd : uint8 { None, Loser, Guard, Counter, KO };
	EEnd End = EEnd::None;
	if (bFought)
	{
		if (KOs.Num()) End = EEnd::KO;
		else if (Clash.bVDefend) End = EEnd::Guard;
		else if (Clash.VDmgD <= 0.f && Clash.VDmgA > 0.f && !bOldEnding) End = EEnd::Counter;
		else if (Clash.VDmgD > 0.f || Clash.VDmgA > 0.f) End = EEnd::Loser;
	}
	FString EndClip; FLinearColor EndTint = FLinearColor::White;
	if (!bOldEnding)
	{
		if (End == EEnd::KO) { EndClip = FString::Printf(TEXT("Anime_Any_Any_KO_%02d"), FMath::RandRange(1, 3)); EndTint = FLinearColor(0.7f, 0.35f, 1.f); }
		else if (End == EEnd::Guard) { EndClip = TEXT("Anime_Any_Any_Infuse_01"); EndTint = FLinearColor(0.45f, 0.85f, 1.f); }
		else if (End == EEnd::Counter && D) { EndClip = FString::Printf(TEXT("Anime_Any_Any_Critical_%02d"), FMath::RandRange(1, 3)); EndTint = ZoogiUtil::PlayerColor(D->PlayerIndex); PlayFighter(0, TEXT("hit_to_body_01"), 1.f, false); }
	}
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 ending %d (clip %s)"), (int32)End, EndClip.IsEmpty() ? TEXT("-") : *EndClip);
	if (End == EEnd::Loser && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoLoser")))'''),
 ('''	for (AZoogiBall* L : KOs) HpKnockout(G, L, L == A ? D : A);''',
  '''	for (AZoogiBall* L : KOs) HpKnockout(G, L, L == A ? D : A);
	if (!EndClip.IsEmpty()) if (UZoogiAnimeSubsystem* An = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) An->PlayCrash(EndClip, EndTint); // b96: full screen, refused if a clip is already up (never two)'''),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''	{	// b95c (Evan: "game stalls after the new duel"): real-time watchdogs - nothing may hold a match up for long''',
  '''	if (Players.IsValidIndex(G->ActivePlayer) && !Clash.bOn) Players[G->ActivePlayer].DuelReserve = 2; // b96: the duel reserve refills on your own turn
	{	// b95c (Evan: "game stalls after the new duel"): real-time watchdogs - nothing may hold a match up for long'''),
])
ed(P + 'ZoogiFront.cpp', [
 ('			{ TEXT("B95"), TEXT("WOLVES HOLD + METEOR CAM")',
  '			{ TEXT("B96"), TEXT("DUEL FINALES + 2 SP RESERVE"), TEXT("One full-screen clip per duel ending (crash, guard, counter or KO). Everyone keeps 2 SP to defend or counter in a duel.") },\n			{ TEXT("B95"), TEXT("WOLVES HOLD + METEOR CAM")'),
])
print('b96 ok')
