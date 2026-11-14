# b120a (Evan 17:26, inventory screenshots: "resize for portrait orientation fill screen"; 17:28: "add the weapons we created in tripo
# starting with the grenade, sword, staff, disc"):
#  - profile: EquippedWeapon ("weapon" in the save; None = gatling)
#  - inventory WEAPON tab: GATLING, PRISM GRENADE, CRYSTAL SWORD, CRYSTAL STAFF, HALO DISC - all owned, tap -> EQUIP!, tile art =
#    Content/Data/MenuArt/Weapons/w_<key>.png (Evan's Tripo renders)
#  - portrait inventory: the grid takes the full width and the full height down to the Zoogi tag (tiles up to 520), centred;
#    the skill details panel overlays the lower part of the grid
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'

p=R+F; b=open(p,'rb').read()
o=b'\t\tUTexture2D* Img = !I.Ball.IsEmpty() ? ZBallIcon(I.Ball) : !I.CardId.IsEmpty() ? ZLoadPng(TEXT("Data/CardArt/") + I.CardId) : nullptr;'
n=b'\t\tUTexture2D* Img = !I.Ball.IsEmpty() ? ZBallIcon(I.Ball) : !I.CardId.IsEmpty() ? ZLoadPng(TEXT("Data/CardArt/") + I.CardId) : !I.Icon.IsEmpty() ? ZLoadPng(I.Icon) : nullptr; // b120: weapon renders'
assert b.count(o)==2; open(p,'wb').write(b.replace(o,n))
# portrait inventory fills the screen
ed1(F, '''		const float GX = (Tab == 3 && DH > DW) ? 40.f : RobotX + 260.f; // b63: the skills list uses the full width in portrait
		float X = (Tab == 3 && DH > DW) ? RobotX + 260.f : GX;''', '''		// b120 (Evan: "resize for portrait orientation - fill screen"): every tab's grid uses the full width in portrait (b63: skills only)
		const bool bPortInv = DH > DW;
		float GX = bPortInv ? 40.f : RobotX + 260.f;
		float X = RobotX + 260.f;''')
ed1(F, '''		const float AvailW = DW - GX - 50.f, AvailH = DH - 230.f - 40.f, Gap = 22.f;
		int32 Cols = It.Num() > 8 ? 5 : 4; float W = 0.f;
		for (int32 Cn = 3; Cn <= 9; ++Cn)
		{
			const int32 Rn = FMath::Max(1, (It.Num() + Cn - 1) / Cn);
			const float Wn = FMath::Min(250.f, FMath::Min((AvailW - (Cn - 1) * Gap) / Cn, (AvailH - (Rn - 1) * Gap) / Rn));
			if (Wn >= W - 0.5f) { W = Wn; Cols = Cn; } // ties: more columns (portrait fills the width)
		}''', '''		const float AvailW = bPortInv ? DW - 80.f : DW - GX - 50.f, AvailH = bPortInv ? DH - 226.f - 175.f : DH - 230.f - 40.f, Gap = 22.f; // portrait: down to the Zoogi tag
		int32 Cols = It.Num() > 8 ? 5 : 4; float W = 0.f;
		for (int32 Cn = bPortInv ? 2 : 3; Cn <= 9; ++Cn)
		{
			const int32 Rn = FMath::Max(1, (It.Num() + Cn - 1) / Cn);
			const float Wn = FMath::Min(bPortInv ? 520.f : 250.f, FMath::Min((AvailW - (Cn - 1) * Gap) / Cn, (AvailH - (Rn - 1) * Gap) / Rn));
			if (Wn >= W - 0.5f) { W = Wn; Cols = Cn; } // ties: more columns (portrait fills the width)
		}
		if (bPortInv) GX = 40.f + FMath::Max(0.f, (AvailW - (Cols * W + (Cols - 1) * Gap)) * 0.5f); // centred''')
ed1(F, '''				const FVector2D PTL = bPortG ? FVector2D(40.f, 226.f + RowsUsed * (W + Gap) + 30.f) : FVector2D(24.f, 200.f);
				const FVector2D PSz = bPortG ? FVector2D(DW - 80.f, FMath::Min(1000.f, DH - PTL.Y - 60.f)) : FVector2D(GX - 50.f, DH - 240.f);''',
'''				// b120: the portrait grid fills the screen now - the panel overlays its lower part (under the grid when there is room)
				const float PH = FMath::Min(1000.f, FMath::Max(DH * 0.44f, DH - (226.f + RowsUsed * (W + Gap) + 30.f) - 60.f));
				const FVector2D PTL = bPortG ? FVector2D(40.f, FMath::Min(226.f + RowsUsed * (W + Gap) + 30.f, DH - 60.f - PH)) : FVector2D(24.f, 200.f);
				const FVector2D PSz = bPortG ? FVector2D(DW - 80.f, PH) : FVector2D(GX - 50.f, DH - 240.f);''')
# front test: shoot the weapon tab and equip the grenade
ed1(F, '''		case 9: Shot(TEXT("front_08_equipped")); Tab = 1; break;
		case 10: Shot(TEXT("front_09_balls")); Tab = 3;''', '''		case 9: Shot(TEXT("front_08_equipped")); Tab = 2; { TArray<FItem> I = Items(); if (I.IsValidIndex(1)) ItemAction(I[1]); } break; // b120: weapons tab, equip the grenade
		case 10: Shot(TEXT("front_09_weapons")); Tab = 3;''')
print('b120a ok')
