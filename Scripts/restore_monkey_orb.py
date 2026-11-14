import unreal, os
EAL=unreal.EditorAssetLibrary
src="/Game/_MonkeyTmp/SM_Orb"; dst="/Game/Orbs/SM_OrbMonkey"
m=unreal.load_asset(src)
if EAL.does_asset_exist(dst): EAL.delete_asset(dst)
d=EAL.duplicate_asset(src,dst); EAL.save_asset(dst)
b=d.get_bounds()
mats=[s.get_editor_property("material_interface").get_path_name() if s.get_editor_property("material_interface") else None for s in d.get_editor_property("static_materials")]
out="C:/Users/skybi/zoogi_xfer/monkey"; os.makedirs(out,exist_ok=True)
for o,f in ((d,"orb_monkey.obj"),):
    t=unreal.AssetExportTask(); t.object=o; t.filename=out+"/"+f; t.automated=True; t.replace_identical=True; t.prompt=False
    unreal.Exporter.run_asset_export_task(t)
for i in range(3):
    tx=unreal.load_asset("/Game/Orbs/orb/Textures/orb_texture_%d"%i)
    t=unreal.AssetExportTask(); t.object=tx; t.filename=out+"/orb_texture_%d.png"%i; t.automated=True; t.replace_identical=True; t.prompt=False
    unreal.Exporter.run_asset_export_task(t)
unreal.log_warning("MONKEY_DONE r=%.2f ext=%s origin=%s tris=%d mats=%s"%(b.sphere_radius,b.box_extent,b.origin,d.get_num_triangles(0),mats))
