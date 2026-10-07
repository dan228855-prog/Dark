# -*- coding: utf-8 -*-
"""
D.A.R.C. — ~1400 движковых кубов/плоскостей/цилиндров (панели отделки, плинтусы, детали) рисовались
по одному вызову на каждый — поток рендера упирался в них (~17 мс). Делаем копии BasicShapes с Nanite
в /Game/DARC/Meshes/Basic и подменяем их у непрозрачных актёров уровня. Материалы и размеры не меняются.
Вернуть: подставить /Engine/BasicShapes/<Shape> обратно (скрипт печатает, сколько заменено).
"""
import unreal

eal = unreal.EditorAssetLibrary
DEST = "/Game/DARC/Meshes/Basic"
SHAPES = ["Cube", "Plane", "Cylinder", "Sphere"]
TRANSLUCENT = (unreal.BlendMode.BLEND_TRANSLUCENT, unreal.BlendMode.BLEND_ADDITIVE, unreal.BlendMode.BLEND_MODULATE,
               unreal.BlendMode.BLEND_ALPHA_COMPOSITE, unreal.BlendMode.BLEND_ALPHA_HOLDOUT)

copies = {}
for s in SHAPES:
    src = "/Engine/BasicShapes/%s.%s" % (s, s)
    dst = "%s/SM_Nanite%s" % (DEST, s)
    if not eal.does_asset_exist(dst):
        eal.duplicate_asset(src, dst)
    m = unreal.load_asset(dst)
    ns = m.get_editor_property("nanite_settings")
    if not ns.get_editor_property("enabled"):
        ns.set_editor_property("enabled", True)
        m.set_editor_property("nanite_settings", ns)
        eal.save_loaded_asset(m)
    copies[src] = m


def opaque(comp):
    for i in range(comp.get_num_materials()):
        mat = comp.get_material(i)
        base = mat.get_base_material() if mat else None
        if base is None or base.get_editor_property("blend_mode") in TRANSLUCENT:
            return False
    return True


sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
swapped, kept = 0, 0
for a in sub.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    c = a.static_mesh_component
    sm = c.static_mesh
    if not sm or sm.get_path_name() not in copies:
        continue
    if not c.is_visible() or a.is_hidden_ed() or not opaque(c):
        kept += 1
        continue
    # Материалы, назначенные на сам меш (не переопределённые), переносим явно.
    mats = [c.get_material(i) for i in range(c.get_num_materials())]
    a.modify()
    c.set_static_mesh(copies[sm.get_path_name()])
    for i, mat in enumerate(mats):
        c.set_material(i, mat)
    swapped += 1
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("[DARC] basic shapes -> nanite: swapped %d, kept %d (translucent/hidden)" % (swapped, kept))
