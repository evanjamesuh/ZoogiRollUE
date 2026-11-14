import unreal
EAL = unreal.EditorAssetLibrary
LOG = unreal.log
SRC_SHELL = "/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell"
SRC_CORE = "/Game/Characters/Bible/Blue/Materials/MI_Blue_Core"
DST_DIR = "/Game/Env/Arenas/Orbs"
DST_SHELL = DST_DIR + "/MI_SharedOrb_Shell"
DST_CORE = DST_DIR + "/MI_SharedOrb_Core"

def ensure_dir(path):
    if not EAL.does_directory_exist(path):
        EAL.make_directory(path)
        LOG("[SharedOrb] mkdir " + path)

def dup(src, dst):
    if EAL.does_asset_exist(dst):
        LOG("[SharedOrb] exists " + dst)
        return EAL.load_asset(dst)
    if not EAL.does_asset_exist(src):
        raise RuntimeError("missing source " + src)
    out = EAL.duplicate_asset(src, dst)
    if not out:
        raise RuntimeError("duplicate failed " + src + " -> " + dst)
    EAL.save_asset(dst)
    LOG("[SharedOrb] duplicated " + src + " -> " + dst)
    return out

ensure_dir(DST_DIR)
dup(SRC_SHELL, DST_SHELL)
dup(SRC_CORE, DST_CORE)
LOG("[SharedOrb] MI_SharedOrb_Shell/Core ready (Blue look duplicates)")
