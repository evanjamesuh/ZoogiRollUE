# b89 (Evan 14:51-14:53): CPUs no longer fill all 4 crystals on their first turn (1 crystal per player per turn at most);
# the SUPER meter fills at half speed; Cosmic has no launch ship - everyone starts on the field.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiCardGlue.cpp', [
 ('''	if (!AwakenChargeOn() || Player < 0 || Player > 3 || Pips <= 0) return;
	const int32 Was = AwakenCharge[Player];''',
  '''	if (!AwakenChargeOn() || Player < 0 || Player > 3 || Pips <= 0) return;
	// b89 (Evan: "fix CPU instantly getting full crystals"): a CPU committing 4 cards at once filled all 4; now one crystal per
	// player per turn at most (-ZoogiPipPerCard = one per card again)
	static const bool bPerCard = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPipPerCard"));
	static int32 PipTurn[4] = { -1, -1, -1, -1 };
	if (!bPerCard)
	{
		if (PipTurn[Player] == TurnCounter && AwakenCharge[Player] > 0) { UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d: already +1 this turn (%s ignored)"), Player + 1, Why); return; }
		PipTurn[Player] = TurnCounter;
		Pips = 1;
	}
	const int32 Was = AwakenCharge[Player];'''),
])
ed(P + 'ZoogiSuperFun.cpp', [
 ('''	if (!Players.IsValidIndex(Player) || Amount == 0.f) return;
	FPlayerFun& P = Players[Player];''',
  '''	if (!Players.IsValidIndex(Player) || Amount == 0.f) return;
	static const bool bEasy = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEasyMeter"));
	if (Amount > 0.f && !bEasy) Amount *= 0.5f; // b89 (Evan: "make filling the super meter harder"): half gain; -ZoogiEasyMeter = full
	FPlayerFun& P = Players[Player];'''),
])
ed(P + 'ZoogiArenaLaunchPad.cpp', [
 ('''	if (Theme != EZoogiArena::Cosmic && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiShipAllMaps")))
	{
		UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: none on this map (ship only on Cosmic) - players start on the field"));''',
  '''	// b89 (Evan: "delete launch ship from cosmos, make players start on field"): no ship anywhere (-ZoogiCosmicShip = Cosmic keeps it)
	if ((Theme != EZoogiArena::Cosmic || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmicShip"))) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiShipAllMaps")))
	{
		UE_LOG(LogZoogi, Log, TEXT("LAUNCH PAD: none on this map - players start on the field"));'''),
])
print('b89 ok')
