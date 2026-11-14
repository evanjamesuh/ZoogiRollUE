# b143d (Evan 12:37 "Better touch radius for arrow and the ability to simply drag back without needing to touch the arrow first";
# 12:38 "Start me with max items I can carry of each which is 5 with the exception of the sword and staff"):
#  - arrow grab area: shaft margin 40 -> 95 px, head 70 -> 150 px, and a press on your own Zoogi grabs it too (1.6x its disc)
#  - a press over an orb / rival no longer just re-aims: it arms the slingshot along the arrow (drag back = launch), released without
#    moving it still aims at what you touched
#  - items: you carry at most 5 of each; start (and a one-time top-up for existing profiles) = 5 Gatling, 5 Prism Grenade, 5 Halo Disc;
#    Crystal Sword and Crystal Staff stay at 3
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
A = 'Private/ZoogiAimArrow.cpp'
ed1(A, '''			GConfig->GetFloat(S, TEXT("GrabShaftPx"), GTune.GrabShaftPx, GGameIni);''', '''			GTune.GrabShaftPx = 95.f; GTune.GrabHeadPx = 150.f; GTune.bGrabOnZoogi = true; // b143d (Evan): a bigger touch area
			GConfig->GetFloat(S, TEXT("GrabShaftPx"), GTune.GrabShaftPx, GGameIni);''')
ed1(A, '''		return T.bGrabOnZoogi && FVector2D::Distance(P, BallS) < BallRpx * 1.15f;''', '''		return T.bGrabOnZoogi && FVector2D::Distance(P, BallS) < FMath::Max(BallRpx * 1.6f, 60.f * Px); // b143d''')
C = 'Private/ZoogiPlayerController.cpp'
ed1(C, '''		else if (!HoverTarget.IsValid() && bPtrDown && !bSimClick && G->GetActiveBall() && !G->IsOnLaunchPad(G->GetActiveBall()) && G->EnsureArrowAim(TEXT("press")))
		{
			// build 28 (Evan): pull back right away, no aim needed - a press on open floor arms the slingshot along the arrow;
			// released without moving it is still the old tap-to-aim at the press point
			bDragArmed = true; bPendingTap = true; TapPos = Ptr; TapTarget = nullptr;''', '''		else if (bPtrDown && !bSimClick && G->GetActiveBall() && !G->IsOnLaunchPad(G->GetActiveBall()) && G->EnsureArrowAim(TEXT("press")))
		{
			// build 28 (Evan): pull back right away, no aim needed - a press on open floor arms the slingshot along the arrow;
			// released without moving it is still the old tap-to-aim at the press point
			// b143d (Evan: "drag back without needing to touch the arrow first"): a press over an orb / rival arms it too (tap = aim at it)
			bDragArmed = true; bPendingTap = true; TapPos = Ptr; TapTarget = HoverTarget;''')
I = 'Private/ZoogiItems.cpp'
ed1(I, '''				if (!D.bItemsSeeded) { for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = 3; D.bItemsSeeded = true; P->Save(); }''', '''				// b143d (Evan): carry 5 of each at most; start with 5 (Crystal Sword / Staff: 3); old profiles topped up once
				auto Start = [](const FName& W) { return (W == FName(TEXT("sword")) || W == FName(TEXT("staff"))) ? 3 : MaxCarry; };
				if (!D.bItemsSeeded) { for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = Start(W); D.bItemsSeeded = true; D.ItemCounts.FindOrAdd(FName(TEXT("seed_v2"))) = 1; P->Save(); }
				if (!D.ItemCounts.Contains(FName(TEXT("seed_v2")))) { for (const FName& W : WeaponIds()) { int32& Cn = D.ItemCounts.FindOrAdd(W); Cn = FMath::Max(Cn, Start(W)); } D.ItemCounts.Add(FName(TEXT("seed_v2")), 1); P->Save(); UE_LOG(LogZoogi, Log, TEXT("ITEMS topped up to the b143 start (5 / sword + staff 3)")); }''')
ed1(I, '''			for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = Have.FindRef(W) + (W == FName(TEXT("grenade")) ? ZoogiGrenade::Count(G, Human) : 0); // pickups are kept''', '''			for (const FName& W : WeaponIds()) D.ItemCounts.FindOrAdd(W) = FMath::Min(MaxCarry, Have.FindRef(W) + (W == FName(TEXT("grenade")) ? ZoogiGrenade::Count(G, Human) : 0)); // pickups are kept (b143d: at most 5)''')
ed1(I, '''namespace ZoogiItems
{''', '''namespace ZoogiItems
{
	constexpr int32 MaxCarry = 5; // b143d (Evan): the most of one item you can carry''')
F = 'Private/ZoogiFront.cpp'
ed1(F, '''P && P->Data().bItemsSeeded ? Have : 3); // b137: the collection count''', '''P && P->Data().bItemsSeeded ? Have : ((FCString::Strcmp(W[i].K, TEXT("sword")) == 0 || FCString::Strcmp(W[i].K, TEXT("staff")) == 0) ? 3 : 5)); // b137: the collection count (b143d: start 5)''')
print('b143d ok')
