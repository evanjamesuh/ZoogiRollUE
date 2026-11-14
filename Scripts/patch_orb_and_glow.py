# -*- coding: utf-8 -*-
from pathlib import Path
import re, sys
ROOT = Path(r"C:\dev\ZoogiRollUnreal-polish")
def read(p): return p.read_text(encoding="utf-8")
def write(p, text): p.write_text(text, encoding="utf-8", newline="")
def nl_of(text): return "\r\n" if "\r\n" in text else "\n"
def patch_orb():
    p = ROOT / r"Source\ZoogiRoll\Private\ZoogiOrb.cpp"
    text = read(p)
    nl = nl_of(text)
    if "SharedShell" in text and "MI_SharedOrb_Shell" in text:
        print("InitOrb: already patched")
    else:
        pat = re.compile(r"\t// Blue-shell look.*?\(void\)Deep;", re.S)
        new_block = nl.join([
            "\t// Shared cyan-glass look (Oct 22 polish): prefer /Game/Env/Arenas/Orbs Shared MIs (Blue look),",
            "\t// then Bible Blue shell/core, then GlowMID last (dev only — never shipping). Size/collision unchanged.",
            '\tUStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));',
            '\tUMaterialInterface* SharedShell = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/Orbs/MI_SharedOrb_Shell.MI_SharedOrb_Shell"));',
            '\tUMaterialInterface* SharedCore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/Orbs/MI_SharedOrb_Core.MI_SharedOrb_Core"));',
            '\tUMaterialInterface* BlueShell = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell.MI_Blue_Shell"));',
            '\tUMaterialInterface* BlueCore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Core.MI_Blue_Core"));',
            "\tUMaterialInterface* ShellMat = SharedShell ? SharedShell : BlueShell;",
            "\tUMaterialInterface* CoreMat = SharedCore ? SharedCore : BlueCore;",
            '\tconst FLinearColor Body = ZoogiUtil::Hex(TEXT("#87CEEB"));',
            '\tconst FLinearColor Deep = ZoogiUtil::Hex(TEXT("#0341CA"));',
            "",
            "\tVisual->SetStaticMesh(SphereMesh);",
            "\tVisual->SetRelativeScale3D(FVector(2.f * ZoogiConst::OrbRadius / 100.f));",
            "\tVisual->SetMaterial(0, ShellMat ? ShellMat : static_cast<UMaterialInterface*>(ZoogiUtil::GlowMID(this, Body, 3.f)));",
            "",
            "\tCore->SetStaticMesh(SphereMesh);",
            "\tCore->SetRelativeLocation(FVector::ZeroVector);",
            "\tCore->SetRelativeScale3D(FVector((CoreMat ? 1.25f : 1.2f) * ZoogiConst::OrbRadius / 100.f));",
            "\tCore->SetMaterial(0, CoreMat ? CoreMat : static_cast<UMaterialInterface*>(ZoogiUtil::GlowMID(this, FLinearColor(0.75f, 0.95f, 1.f), 12.f)));",
            '\tGlow->SetLightColor(ShellMat ? ZoogiUtil::Hex(TEXT("#3FD8F2")) : Body);',
            "\t(void)Deep;",
        ])
        m = pat.search(text)
        if not m:
            print("InitOrb: pattern not found", file=sys.stderr); sys.exit(1)
        text = text[:m.start()] + new_block + text[m.end():]
        print("InitOrb: patched")
    old_star = '\tif (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell.MI_Blue_Shell"))) Visual->SetMaterial(0, ZoogiUtil::GlowMID(this, Gold, 0.9f));'
    new_star = nl.join([
        '\tif (!LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/Orbs/MI_SharedOrb_Shell.MI_SharedOrb_Shell"))',
        '\t\t&& !LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell.MI_Blue_Shell")))',
        "\t\tVisual->SetMaterial(0, ZoogiUtil::GlowMID(this, Gold, 0.9f));",
    ])
    ms = text[text.find("void AZoogiOrb::MakeStar"):text.find("void AZoogiOrb::MakeStar")+800]
    if "MI_SharedOrb_Shell" in ms:
        print("MakeStar: already patched")
    elif old_star in text:
        text = text.replace(old_star, new_star, 1); print("MakeStar: patched")
    else:
        print("MakeStar: old line missing", file=sys.stderr); sys.exit(1)
    write(p, text); print("wrote", p)

def patch_frozen():
    p = ROOT / r"Source\ZoogiRoll\Private\ZoogiArenaFrozen.cpp"
    text = read(p)
    reps = [
        ('ZoogiUtil::GlowMID(this, Cyan, 3.f)', 'ZoogiUtil::GlowMID(this, Cyan, 1.0f) /* Oct22 glow cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.6f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.0f) /* Oct22 glow cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 3.4f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 1.0f) /* Oct22 glow cap boundary */'),
        ('ZoogiUtil::GlowMID(this, Cyan, 2.4f)', 'ZoogiUtil::GlowMID(this, Cyan, 1.2f) /* Oct22 hub accent cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 3.0f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 1.0f) /* Oct22 glow cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.8f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.0f) /* Oct22 glow cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#8FEFFF")), 1.3f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#8FEFFF")), 1.0f) /* Oct22 glow cap */'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 3.0f)', 'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.0f) /* Oct22 glow cap hub crystal */'),
        (', 1.0f, 2.6f, 1.0f, FLinearColor(0.75f, 0.95f, 1.f), &SealMID); // r6: emissive 2.0 -> 2.6',
         ', 1.0f, 1.0f, 1.0f, FLinearColor(0.75f, 0.95f, 1.f), &SealMID); // Oct22 glow cap: 2.6 -> 1.0'),
        ('ZoogiUtil::GlowMID(this, OrbCol[k], k == 2 ? 2.2f : 3.2f)',
         'ZoogiUtil::GlowMID(this, OrbCol[k], 1.0f) /* Oct22 awaken-sphere cap (not score orbs) */'),
        (', 0.55f, true, 0.55f, Cyan, 2.2f);',
         ', 0.55f, true, 0.55f, Cyan, 1.0f); /* Oct22 rim glow cap */'),
    ]
    n=0
    for a,b in reps:
        c=text.count(a)
        if c: text=text.replace(a,b); print(f"Frozen: {c}x {a[:55]}"); n+=c
        else: print(f"Frozen: skip {a[:55]}")
    write(p,text); print(f"Frozen total {n}")

def patch_neon():
    p = ROOT / r"Source\ZoogiRoll\Private\ZoogiArena.cpp"
    text = read(p)
    reps = [
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#22E7FF")), 2.f), false)->SetCastShadow(false);\r\n\t\tAddCylinder(FVector2D::ZeroVector, 250.f',
         'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#22E7FF")), 1.0f), false)->SetCastShadow(false); // Oct22 NC glow cap\r\n\t\tAddCylinder(FVector2D::ZeroVector, 250.f'),
        ('UMaterialInterface* Glow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(PilCols[k]), 7.f);',
         'UMaterialInterface* Glow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(PilCols[k]), 1.0f); // Oct22 NC code-pillar glow cap'),
        ('ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 4.f), false);',
         'ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 1.0f), false); // Oct22 NC code-pillar cap'),
        ('UMaterialInterface* Win = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(WinCols[i % 4]), Rng.FRandRange(2.f, 6.f));',
         'UMaterialInterface* Win = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(WinCols[i % 4]), Rng.FRandRange(0.6f, 1.0f)); // Oct22 NC skyline glow cap'),
    ]
    n=0
    for a,b in reps:
        c=text.count(a)
        if c: text=text.replace(a,b); print(f"Neon: {c}x"); n+=c
        else: print(f"Neon: skip {a[:70]}")
    # Volcanic orb floor glow scale
    needle='case EZoogiArena::Volcanic: Set(TEXT("#FF7A2E"), 0.9f); break;               // lava orange (exposure +0.2)'
    repl='case EZoogiArena::Volcanic: Set(TEXT("#FF7A2E"), 1.0f); break;               // Oct22 volcanic realism: orb floor glow full on contact'
    if needle in text:
        text=text.replace(needle,repl,1); print("Volcanic: OrbFloorGlow 0.9->1.0")
    else:
        print("Volcanic: OrbFloorGlow line skip")
    write(p,text); print(f"Neon/Arena total {n}")

patch_orb(); patch_frozen(); patch_neon(); print("DONE")
