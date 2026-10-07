# darc_fix_table_collision.py
# Стол с инструментами в техпомещении: у модели не было простой коллизии на столешнице —
# лом и батарейки проваливались на пол. Ставим коробку по габаритам модели.
import unreal

MESHES = ["/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Furniture_Table_Wood_Worn_01/SM_Ind_War_Furniture_Table_Wood_Worn_01_B"]
L = unreal.EditorStaticMeshLibrary
for path in MESHES:
    mesh = unreal.load_asset(path)
    if not mesh:
        # путь мог отличаться — ищем по имени
        name = path.split("/")[-1]
        for p in unreal.EditorAssetLibrary.list_assets("/Game/Scene_Warehouse", recursive=True):
            if p.split(".")[-1] == name or p.endswith("/" + name):
                mesh = unreal.load_asset(p)
                break
    if not mesh:
        unreal.log_warning("[DARC] table mesh not found: " + path)
        continue
    before = L.get_simple_collision_count(mesh)
    L.remove_collisions(mesh)
    L.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log("[DARC] table collision: %s %d -> %d" % (mesh.get_name(), before, L.get_simple_collision_count(mesh)))
