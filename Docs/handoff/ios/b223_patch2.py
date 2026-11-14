import os, sys
SRC = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:100]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)
patch('Private/ZoogiSkills.cpp', [
("\t\t\t\t// the human: remember the last tap on the court (not the pad buttons)\n",
 "\t\t\t\tstatic const bool bDashTest = FParse::Param(FCommandLine::Get(), TEXT(\"ZoogiRapidDashTest\")); // preview: a dash every 0.7 s toward a random spot\n"
 "\t\t\t\tif (bDashTest && Tn >= R.NextCpu) { R.NextCpu = Tn + 0.7; R.Tap = Db->BallLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 500.f; R.bHasTap = true; AZoogiFX::Spawn(W, R.Tap, GatGold, 90.f, 0.35f, true, 8.f); DashOnce(G, R, R.Tap); continue; }\n"
 "\t\t\t\t// the human: remember the last tap on the court (not the pad buttons)\n"),
])
patch('Private/ZoogiFront.cpp', [
("\t\tcase 7:\n\t\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"ZoogiBallerTour\")))",
 "\t\tcase 7:\n"
 "\t\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"ZoogiAmmoTour\")))\n"
 "\t\t\t{\t// b223 preview: the Gatling Turret card with its AMMO row (rocket, then laser), then back to rocket\n"
 "\t\t\t\tstatic int32 Ai = 0;\n"
 "\t\t\t\tif (Ai < 5)\n"
 "\t\t\t\t{\n"
 "\t\t\t\t\tif (Ai == 0) { Tab = 2; b3D = false; ItemSel = 0; Pop = EPop::Item; PopT = T; if (P) P->Mutable().ItemCounts.FindOrAdd(FName(TEXT(\"turret_ammo\"))) = 5; }\n"
 "\t\t\t\t\telse if (Ai == 1) Shot(TEXT(\"ammo_1_rocket\"));\n"
 "\t\t\t\t\telse if (Ai == 2) { if (P) P->Mutable().ItemCounts.FindOrAdd(FName(TEXT(\"turret_ammo\"))) = 4; }\n"
 "\t\t\t\t\telse if (Ai == 3) Shot(TEXT(\"ammo_2_laser\"));\n"
 "\t\t\t\t\telse if (P) { P->Mutable().ItemCounts.FindOrAdd(FName(TEXT(\"turret_ammo\"))) = 5; P->Save(); }\n"
 "\t\t\t\t\t++Ai; --TestStep; break;\n"
 "\t\t\t\t}\n"
 "\t\t\t}\n"
 "\t\t\tif (FParse::Param(FCommandLine::Get(), TEXT(\"ZoogiBallerTour\")))"),
])
print('patched b223 part 2')
