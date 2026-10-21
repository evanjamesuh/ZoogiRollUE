# v3 cards: builds /Game/Cards/DT_ZoogiCards, a DataTable on the Roguelike Deckbuilder Toolkit row struct
# /Game/RLDB/Blueprints/Structs/FCard, from Content/Data/Cards.csv (the 15 universal cards).
# Card Energy = the toolkit's Mana stat: UseRules = BP_UseRule_StatCost(Status=BP_Status_Mana, Cost=<cost>).
# PostUseEvent / EndTurnEvent = Event.Card.Discard (matches our turn-end discard). CardEffects stay EMPTY on purpose:
# toolkit effects (Attack, AddStatus...) would fabricate damage; Zoogy card effects run through real physics (Phase 3/4).
import unreal, json, os, csv
proj = unreal.Paths.project_dir()
rows = []
with open(os.path.join(proj, 'Content', 'Data', 'Cards.csv'), newline='') as f:
    lines = [l for l in f if not l.lstrip().startswith('#')]
rc = 11 if [h.strip() for h in next(csv.reader(lines[:1]))][10:11] == ['element'] else 10  # Oct 14: optional element column
for r in csv.reader(lines[1:]):
    if len(r) < 11: continue
    cid, name, cost, ctype = r[0].strip(), r[1].strip(), int(r[2]), r[3].strip()
    rule = ','.join(r[rc:]).strip()
    rows.append({
        'Name': cid,
        'CardName': 'NSLOCTEXT("ZoogiCards", "%s_Name", "%s")' % (cid, name.title()),
        'Description': 'NSLOCTEXT("ZoogiCards", "%s_Desc", "%s")' % (cid, rule.replace('"', "'")),
        'Portrait': "/Script/Engine.Texture2D'/Game/RLDB/Demo/Textures/Portraits/T_Portrait_Block.T_Portrait_Block'",
        'Tooltips': [],
        'CardTags': '(GameplayTags=())',
        'CardEffects': [],
        'bTargeted': False,
        'UseRules': ["(Rule=\"/Script/Engine.BlueprintGeneratedClass'/Game/RLDB/Blueprints/CardUseRules/BP_UseRule_StatCost.BP_UseRule_StatCost_C'\",Status=\"/Script/Engine.BlueprintGeneratedClass'/Game/RLDB/Blueprints/Statuses/BP_Status_Mana.BP_Status_Mana_C'\",Cost=%d)" % cost],
        'PostUseEvent': '(TagName="Event.Card.Discard")',
        'EndTurnEvent': '(TagName="Event.Card.Discard")',
        'StartingStatuses': [],
        'Repetitions': 0,
        'Frame': "/Script/Engine.Texture2D'/Game/RLDB/Demo/Textures/Card/T_CardFrame.T_CardFrame'",
        'FrameTint': '(SpecifiedColor=(R=1.000000,G=1.000000,B=1.000000,A=1.000000),ColorUseRule=UseColor_Specified)',
        'CardVisualWidget': "/Script/UMG.WidgetBlueprintGeneratedClass'/Game/RLDB/Widgets/WBP_CardVisual.WBP_CardVisual_C'",
        'DataRow': '(DataTable=None,RowName="")',
        'OwnerID': ''})
path = '/Game/Cards/DT_ZoogiCards'
st = unreal.EditorAssetLibrary.load_asset('/Game/RLDB/Blueprints/Structs/FCard')
if unreal.EditorAssetLibrary.does_asset_exist(path):
    dt = unreal.EditorAssetLibrary.load_asset(path)
else:
    fac = unreal.DataTableFactory(); fac.set_editor_property('struct', st)
    dt = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_ZoogiCards', '/Game/Cards', unreal.DataTable, fac)
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows))
unreal.EditorAssetLibrary.save_loaded_asset(dt)
names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)]
back = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
open(os.path.join(unreal.Paths.project_saved_dir(), 'DT_ZoogiCards.json'), 'w').write(back)
msg = 'MAKE_TOOLKIT_CARDS ok=%s rows=%d names=%s' % (ok, len(names), ','.join(names))
unreal.log(msg)
open(os.path.join(unreal.Paths.project_saved_dir(), 'make_toolkit_cards.txt'), 'w').write(msg)
