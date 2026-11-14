// b137: the item collection in classic matches (see ZoogiItems.h).
#include "ZoogiItems.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGrenadeItem.h"
#include "ZoogiMatchEvents.h"

namespace ZoogiItems
{
	constexpr int32 MaxCarry = 5; // b143d (Evan): the most of one item you can carry
	namespace
	{
		TWeakObjectPtr<AZoogiGameMode> ForG; int32 ForSeed = 0; bool bSaved = false;
		int32 Human = -1;
		TMap<FName, int32> Have;            // the human's collection for this match (grenade: the saved ones; pickups live in ZoogiGrenade)
		TMap<int32, int32> UsedTurn;        // player -> turn an item was used
		FDelegateHandle EvH;

		void Ensure(AZoogiGameMode* G)
		{
			if (!G || (ForG.Get() == G && ForSeed == G->GameSeed)) return;
			if (AZoogiGameMode* Old = ForG.Get()) if (EvH.IsValid()) Old->OnMatchEvent.Remove(EvH);
			ForG = G; ForSeed = G->GameSeed; bSaved = false; Have.Reset(); UsedTurn.Reset(); Human = -1;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) { Human = B->PlayerIndex; break; }
			if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(G))
			{
				FZoogiFrontProfileData& D = P->Mutable();
				// b143d (Evan): carry 5 of each at most; start with 5 (Crystal Sword / Staff: 3); old profiles topped up once
				auto Start = [](const FName& W) { return (W == FName(TEXT("sword")) || W == FName(TEXT("staff")) || W == FName(TEXT("apple"))) ? 3 : MaxCarry; };
				// b188: the Golden Apple joins the items - every profile gets 3 once
				if (!D.ItemCounts.Contains(FName(TEXT("seed_v3")))) { int32& Ap = D.ItemCounts.FindOrAdd(FName(TEXT("apple"))); Ap = FMath::Max(Ap, 3); D.ItemCounts.Add(FName(TEXT("seed_v3")), 1); P->Save(); }
				if (!D.bItemsSeeded) { for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = Start(W); D.bItemsSeeded = true; D.ItemCounts.FindOrAdd(FName(TEXT("seed_v2"))) = 1; P->Save(); }
				if (!D.ItemCounts.Contains(FName(TEXT("seed_v2")))) { for (const FName& W : WeaponIds()) { int32& Cn = D.ItemCounts.FindOrAdd(W); Cn = FMath::Max(Cn, Start(W)); } D.ItemCounts.Add(FName(TEXT("seed_v2")), 1); P->Save(); UE_LOG(LogZoogi, Log, TEXT("ITEMS topped up to the b143 start (5 / sword + staff 3)")); }
				for (const FName& W : WeaponIds()) Have.Add(W, D.ItemCounts.FindRef(W));
			}
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			EvH = G->OnMatchEvent.AddLambda([WG](EZoogiMatchEvent E, int32, float) { if ((E == EZoogiMatchEvent::MatchOver || E == EZoogiMatchEvent::ReturnToMenu) && WG.IsValid()) SaveBack(WG.Get()); });
			UE_LOG(LogZoogi, Log, TEXT("ITEMS match: human P%d - gatling %d grenade %d sword %d staff %d disc %d"), Human + 1, Have.FindRef(TEXT("gatling")), Have.FindRef(TEXT("grenade")), Have.FindRef(TEXT("sword")), Have.FindRef(TEXT("staff")), Have.FindRef(TEXT("disc")));
		}
	}

	const TArray<FName>& WeaponIds()
	{
		static const TArray<FName> Ids = { FName(TEXT("gatling")), FName(TEXT("grenade")), FName(TEXT("sword")), FName(TEXT("staff")), FName(TEXT("disc")), FName(TEXT("apple")) }; // b188: + the Golden Apple
		return Ids;
	}
	FString WeaponName(FName Id)
	{
		if (Id == FName(TEXT("gatling"))) return TEXT("GATLING TURRET"); // b203 (Evan): the turret is this item now
		if (Id == FName(TEXT("grenade"))) return TEXT("PRISM GRENADE");
		if (Id == FName(TEXT("sword"))) return TEXT("CRYSTAL SWORD");
		if (Id == FName(TEXT("staff"))) return TEXT("CRYSTAL STAFF");
		if (Id == FName(TEXT("disc"))) return TEXT("HALO DISC");
		if (Id == FName(TEXT("apple"))) return TEXT("GOLDEN APPLE");
		return Id.ToString().ToUpper();
	}
	FString WeaponRule(FName Id)
	{
		if (Id == FName(TEXT("gatling"))) return TEXT("Tap where the turret goes, then FIRE: it turns to the nearest rival or orb and slashes it, one shot at a time, for 2 of your turns.");
		if (Id == FName(TEXT("grenade"))) return TEXT("Drag where it lands (2x the arc range): a shockwave pulls everything in, then blasts it away.");
		if (Id == FName(TEXT("sword"))) return TEXT("Instant: a crystal slash ring round your Zoogi throws everything close outward.");
		if (Id == FName(TEXT("staff"))) return TEXT("Tap a rival or an orb in reach, then FIRE: a heavy magic bolt blasts it straight away from you.");
		if (Id == FName(TEXT("apple"))) return TEXT("Tap an orb or a rival's WOOD SHIELD in reach, then FIRE: the apple is lobbed there and blasts - it breaks any wood shield it hits.");
		if (Id == FName(TEXT("disc"))) return TEXT("Swipe the disc's path, then FIRE: the halo cuts along it and back, knocking everything aside.");
		return FString();
	}
	TArray<FName> Selected(AZoogiGameMode* G)
	{
		TArray<FName> Out;
		if (const UZoogiFrontProfileSubsystem* P = G ? UZoogiFrontProfileSubsystem::Get(G) : nullptr) for (const FName& W : P->Data().SelWeapons) if (WeaponIds().Contains(W)) Out.AddUnique(W);
		if (Out.Num() == 0) Out.Add(FName(TEXT("grenade")));
		return Out;
	}
	int32 Count(AZoogiGameMode* G, int32 Player, FName Id)
	{
		Ensure(G);
		const int32 Picked = Id == FName(TEXT("grenade")) ? ZoogiGrenade::Count(G, Player) : 0;
		return Picked + (Player == Human ? Have.FindRef(Id) : 0);
	}
	bool UsedThisTurn(AZoogiGameMode* G, int32 Player) { Ensure(G); const int32* T = UsedTurn.Find(Player); return G && T && *T == G->TurnCounter; }
	void MarkUsed(AZoogiGameMode* G, int32 Player) { Ensure(G); if (G) UsedTurn.Add(Player, G->TurnCounter); }
	bool Take(AZoogiGameMode* G, int32 Player, FName Id)
	{
		Ensure(G);
		if (!G || UsedThisTurn(G, Player)) return false;
		if (Id == FName(TEXT("grenade")) && ZoogiGrenade::Take(G, Player)) { MarkUsed(G, Player); return true; } // a picked-up one first
		if (Player != Human) return false;
		int32& N = Have.FindOrAdd(Id);
		if (N <= 0) return false;
		--N; MarkUsed(G, Player);
		UE_LOG(LogZoogi, Log, TEXT("ITEMS P%d used %s (%d left)"), Player + 1, *Id.ToString(), N);
		return true;
	}
	void SaveBack(AZoogiGameMode* G)
	{
		if (!G || ForG.Get() != G || bSaved || Human < 0) return;
		bSaved = true;
		if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(G))
		{
			FZoogiFrontProfileData& D = P->Mutable();
			for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = FMath::Min(MaxCarry, Have.FindRef(W) + (W == FName(TEXT("grenade")) ? ZoogiGrenade::Count(G, Human) : 0)); // pickups are kept (b143d: at most 5)
			P->Save();
			UE_LOG(LogZoogi, Log, TEXT("ITEMS saved: gatling %d grenade %d sword %d staff %d disc %d"), D.ItemCounts.FindRef(TEXT("gatling")), D.ItemCounts.FindRef(TEXT("grenade")), D.ItemCounts.FindRef(TEXT("sword")), D.ItemCounts.FindRef(TEXT("staff")), D.ItemCounts.FindRef(TEXT("disc")));
		}
	}
}
