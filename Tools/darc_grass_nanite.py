# -*- coding: utf-8 -*-
"""
D.A.R.C. — трава вдоль дороги съедала ~23 мс видеокарты (замер в собранной игре: 29.7 → 6.4 мс без неё):
две модели травы — по 230–250 тыс. вершин на кочку, без LOD и без Nanite, а кочек тысячи.
Включаем Nanite (с сохранением площади — для травы и листвы), остальное делает движок.
"""
import unreal

MESHES = [
    "/Game/DARC/Imported/Meshes/grass/grass_vegitation_mix/source/SM_Grass_Vegitation_mix",
    "/Game/DARC/Imported/Meshes/grass/grass_patches_squares/source/SM_GrassPatch_Sqaure",
    "/Game/DARC/Imported/Meshes/grass/plant_shrub/source/SM_Grass",
    "/Game/DARC/Imported/Meshes/grass/ferns_lowpoly_model/source/SM_ferns_fbx",
    "/Game/DARC/Imported/Meshes/grass/flower/source/SM_lavendar",
]
for p in MESHES:
    m = unreal.load_asset(p)
    if not m:
        unreal.log_warning("[DARC] grass mesh missing: " + p)
        continue
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    try:
        ns.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.PRESERVE_AREA)
    except Exception:
        pass
    m.set_editor_property("nanite_settings", ns)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    unreal.log("[DARC] grass nanite: " + m.get_name())
