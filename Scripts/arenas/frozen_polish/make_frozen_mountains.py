# Frozen polish r4: backdrop mountains from the free "Iceland_Environment" mountain pack (Fab/Marketplace, saved with UE 5.3;
# copied verbatim from the Epic launcher vault cache into Content/Iceland_Environment, gitignored, never cooked).
# r5: the material is rebuilt in place and existing derived meshes are kept; adds a camera-distance sky haze.
# Headless: fzjob.ps1 -Job import -Script Scripts\arenas\frozen_polish\make_frozen_mountains.py
#  * builds M_FrozenMountain: lit opaque (no toon), Nanite-ready. Icy blue cliff colour x triplanar painterly vertical
#    striations (T_FrozenStreakTile), deep-blue toward the base, white snow on up-facing slopes above SnowZ, soft emissive fill.
#  * duplicates the chosen pack meshes into /Game/Env/Arenas/FrozenMountains (always cooked) with every material slot on
#    M_FrozenMountain and collision stripped, so shipped meshes do not reference the pack's ~1 GB of 4K textures.
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
SML = unreal.EditorStaticMeshLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
SRC = "/Game/Iceland_Environment/Static_Meshes/"
DST = DIR + "/FrozenMountains"
NAMES = ["SM_Mountain_02", "SM_Mountain_03", "SM_Mountain_04", "SM_Mountain_05",
         "SM_Mountain_06", "SM_Mountain_07", "SM_Mountain_08", "SM_Mountain_09"]
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def log(m):
    unreal.log("FZM " + m)


class G:
    def __init__(self, m): self.m = m
    def e(self, cls, x, y): return MEL.create_material_expression(self.m, cls, x, y)
    def scalar(self, name, v, x, y):
        p = self.e(unreal.MaterialExpressionScalarParameter, x, y)
        p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", v); return p
    def vec(self, name, c, x, y):
        p = self.e(unreal.MaterialExpressionVectorParameter, x, y)
        p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*c)); return p
    def texobj(self, name, t, x, y):
        p = self.e(unreal.MaterialExpressionTextureObjectParameter, x, y)
        p.set_editor_property("parameter_name", name)
        if t: p.set_editor_property("texture", t)
        return p
    def custom(self, code, inputs, out_type, x, y, desc):
        c = self.e(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code); c.set_editor_property("output_type", out_type); c.set_editor_property("description", desc)
        ins = []
        for n, _ in inputs:
            ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
        c.set_editor_property("inputs", ins)
        for n, src in inputs:
            if not MEL.connect_material_expressions(src, "", c, n):
                unreal.log_error("FZM connect failed " + desc + "." + n)
        return c


# ---------------------------------------------------------------- M_FrozenMountain
# r5: rebuilt in place (graph cleared, same asset) so the derived meshes keep their slot references untouched.
path = DIR + "/M_FrozenMountain"
if EAL.does_asset_exist(path):
    m = unreal.load_asset(path)
    MEL.delete_all_material_expressions(m)
else:
    m = AT.create_asset("M_FrozenMountain", DIR, unreal.Material, unreal.MaterialFactoryNew())
try:
    m.set_editor_property("used_with_nanite", True)
except Exception as ex:
    unreal.log_warning("FZM used_with_nanite: %s" % ex)
g = G(m)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1500, -400)
nrm = g.e(unreal.MaterialExpressionVertexNormalWS, -1500, -320)
cam = g.e(unreal.MaterialExpressionCameraPositionWS, -1500, -480)
det = g.texobj("Detail", unreal.load_asset(DIR + "/T_FrozenStreakTile") or unreal.load_asset(DIR + "/T_FrozenIceTile"), -1500, -200)
ice = g.vec("Color", (0.62, 0.84, 0.98, 1), -1500, -40)
deep = g.vec("DeepColor", (0.30, 0.58, 0.88, 1), -1500, 120)
snow = g.vec("SnowColor", (0.92, 0.97, 1.0, 1), -1500, 280)
dsc = g.scalar("DetailScale", 2600.0, -1500, 440)
dst = g.scalar("DetailStrength", 0.5, -1500, 520)
s0 = g.scalar("SnowSlope", 0.62, -1500, 600)
sz = g.scalar("SnowZ", 0.0, -1500, 680)
szf = g.scalar("SnowZFade", 3000.0, -1500, 760)
dz = g.scalar("DeepZ", -4000.0, -1500, 840)
dzf = g.scalar("DeepZFade", 5000.0, -1500, 920)
ecol = g.vec("EmissiveColor", (0.45, 0.78, 1.0, 1), -1500, 1000)
estr = g.scalar("EmissiveStrength", 0.15, -1500, 1160)
# r5: painterly aerial perspective - blend toward a bright sky-haze colour with camera distance (base colour + glow).
hcol = g.vec("HazeColor", (0.80, 0.90, 1.0, 1), -1500, 1240)
hs = g.scalar("HazeStart", 6000.0, -1500, 1400)
he = g.scalar("HazeEnd", 30000.0, -1500, 1480)
hm = g.scalar("HazeMax", 0.6, -1500, 1560)
hg = g.scalar("HazeGlow", 0.6, -1500, 1640)
snowa = g.custom("""
float slope = saturate((normalize(N).z - SnowSlope) * 5.0);
float h = saturate((WP.z - SnowZ) / max(SnowZFade, 1.0));
return slope * h;
""", [("WP", wp), ("N", nrm), ("SnowSlope", s0), ("SnowZ", sz), ("SnowZFade", szf)], F1, -900, 500, "SnowMask")
haze = g.custom("""
float d = length(WP - Cam);
return saturate((d - HazeStart) / max(HazeEnd - HazeStart, 1.0)) * HazeMax;
""", [("WP", wp), ("Cam", cam), ("HazeStart", hs), ("HazeEnd", he), ("HazeMax", hm)], F1, -900, 900, "HazeMask")
bc = g.custom("""
float3 nw = abs(normalize(N)); nw = pow(nw, 3.0); nw /= (nw.x + nw.y + nw.z + 1e-4);
float3 p = WP / DetailScale;
float d = Texture2DSample(Det, DetSampler, p.yz).r * nw.x + Texture2DSample(Det, DetSampler, p.xz).r * nw.y + Texture2DSample(Det, DetSampler, p.xy).r * nw.z;
float lo = saturate((WP.z - DeepZ) / max(DeepZFade, 1.0));
float3 c = lerp(DeepColor.rgb, Color.rgb, lo);
c *= lerp(1.0, d * 2.0, DetailStrength);
c = lerp(c, SnowColor.rgb * lerp(0.92, 1.04, d), S);
return lerp(c, HazeColor.rgb, H);
""", [("WP", wp), ("N", nrm), ("Det", det), ("Color", ice), ("DeepColor", deep), ("SnowColor", snow), ("DetailScale", dsc),
      ("DetailStrength", dst), ("DeepZ", dz), ("DeepZFade", dzf), ("S", snowa), ("HazeColor", hcol), ("H", haze)], F3, -600, -100, "MountainBase")
MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
em = g.custom("return EmissiveColor.rgb * EmissiveStrength * (1.0 - 0.5 * S) + HazeColor.rgb * H * HazeGlow;",
              [("EmissiveColor", ecol), ("EmissiveStrength", estr), ("S", snowa), ("HazeColor", hcol), ("H", haze), ("HazeGlow", hg)], F3, -600, 400, "MountainEmissive")
MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(g.scalar("Roughness", 0.55, -600, 800), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(g.scalar("Specular", 0.4, -600, 900), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m)
EAL.save_loaded_asset(m, False)
log("built M_FrozenMountain nanite=%s" % m.get_editor_property("used_with_nanite"))

# ---------------------------------------------------------------- derived meshes
for n in NAMES:
    if not EAL.does_asset_exist(SRC + n):
        unreal.log_warning("FZM missing %s" % n); continue
    dn = n.replace("SM_Mountain_", "SM_FrozenMtn_")
    p = DST + "/" + dn
    if EAL.does_asset_exist(p):
        log("%s exists, kept" % dn)  # r5: never re-duplicate (keeps the committed meshes byte-stable)
        continue
    mesh = EAL.duplicate_asset(SRC + n, p)
    if not mesh:
        unreal.log_error("FZM duplicate failed %s" % n); continue
    mats = mesh.get_editor_property("static_materials")
    for s in mats:
        s.set_editor_property("material_interface", m)
    mesh.set_editor_property("static_materials", mats)
    try:
        SML.remove_collisions(mesh)
    except Exception as ex:
        unreal.log_warning("FZM remove_collisions %s: %s" % (n, ex))
    ns = mesh.get_editor_property("nanite_settings")
    b = mesh.get_bounding_box()
    ext = b.max - b.min
    log("%s nanite=%s lods=%d verts0=%d ext=(%.0f,%.0f,%.0f) min=(%.0f,%.0f,%.0f) slots=%d" % (
        dn, ns.enabled, mesh.get_num_lods(), SML.get_number_verts(mesh, 0), ext.x, ext.y, ext.z, b.min.x, b.min.y, b.min.z, len(mats)))
    EAL.save_loaded_asset(mesh, False)
    deps = unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies(p.rsplit("/", 1)[0] + "/" + dn, unreal.AssetRegistryDependencyOptions())
    log("%s deps=%s" % (dn, [str(d) for d in (deps or [])]))
log("DONE")
