import unreal
s=unreal.load_asset("/Game/VFX/KriptoFX/Effect4/NS_KFX_Effect4")
open(r"C:\Users\skybi\kfx_desc.txt","w").write(unreal.ZoogiFXLibrary.describe_niagara(s))

