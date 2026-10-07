# -*- coding: utf-8 -*-
"""
D.A.R.C. — оптимизация выезда 1 (07.10.2026). Обратимо:
  1. Nanite на непрозрачных моделях уровня (кроме движковых BasicShapes и моделей со стеклом/прозрачностью).
  2. Текстуры, которые реально используются на уровне, — не больше 2048 (настройка «макс. размер», исходник цел).
  3. Свет без дальности отрисовки (у дверей, гаража, КПП) — гаснет дальше 40 м плавно.
Деревья (Nanite-листва со скелетом) и наземный туман не трогаем: они уже на быстром пути движка.
"""
import unreal

eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()

# --- 1. Nanite ---
TRANSLUCENT = (unreal.BlendMode.BLEND_TRANSLUCENT, unreal.BlendMode.BLEND_ADDITIVE, unreal.BlendMode.BLEND_MODULATE,
               unreal.BlendMode.BLEND_ALPHA_COMPOSITE, unreal.BlendMode.BLEND_ALPHA_HOLDOUT)


def is_translucent(mat):
    base = mat.get_base_material() if mat else None
    try:
        return base is not None and base.get_editor_property("blend_mode") in TRANSLUCENT
    except Exception:
        return True  # не поняли — не трогаем


meshes = {}
for a in actors:
    if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
        m = a.static_mesh_component.static_mesh
        meshes[m.get_path_name()] = m
nanite_on, skipped = [], []
for p, m in sorted(meshes.items()):
    if p.startswith("/Engine/"):
        continue
    ns = m.get_editor_property("nanite_settings")
    if ns.get_editor_property("enabled"):
        continue
    mats = [sm.get_editor_property("material_interface") for sm in m.get_editor_property("static_materials")]
    if any(is_translucent(x) for x in mats):
        skipped.append(m.get_name())
        continue
    ns.set_editor_property("enabled", True)
    m.set_editor_property("nanite_settings", ns)
    eal.save_loaded_asset(m)
    nanite_on.append(m.get_name())
unreal.log("[DARC] nanite on %d meshes; skipped (glass/translucent) %d: %s" % (len(nanite_on), len(skipped), ", ".join(skipped)))

# --- 2. Текстуры уровня ≤ 2048 ---
LIMIT = 2048
mats = set()
for a in actors:
    for c in a.get_components_by_class(unreal.MeshComponent):
        for mat in c.get_materials():
            if mat:
                mats.add(mat)
textures = {}
for mat in mats:
    try:
        for t in unreal.MaterialEditingLibrary.get_used_textures(mat):
            if isinstance(t, unreal.Texture2D):
                textures[t.get_path_name()] = t
    except Exception:
        pass
capped = 0
for p, t in textures.items():
    if p.startswith("/Game/DARC/Imported/Textures/UI"):
        continue
    w, h = t.blueprint_get_size_x(), t.blueprint_get_size_y()
    cur = t.get_editor_property("max_texture_size") or 0
    if max(w, h) > LIMIT and (cur == 0 or cur > LIMIT):
        t.set_editor_property("max_texture_size", LIMIT)
        eal.save_loaded_asset(t)
        capped += 1
unreal.log("[DARC] textures capped to %d: %d" % (LIMIT, capped))

# --- 3. Дальность отрисовки света ---
lit = 0
for a in actors:
    for lc in a.get_components_by_class(unreal.LocalLightComponent):
        if lc.get_editor_property("max_draw_distance") <= 0.0:
            a.modify()
            lc.set_editor_property("max_draw_distance", 4000.0)
            lc.set_editor_property("max_distance_fade_range", 1000.0)
            lit += 1
unreal.log("[DARC] lights given draw distance: %d" % lit)
unreal.EditorLoadingAndSavingUtils.save_current_level()
