# -*- coding: utf-8 -*-
"""
D.A.R.C. — свет в здании в 4 раза тусклее + дальние лампы не рисуются (FPS). Для уже построенного уровня;
новые сборки darc_build_facility.py делают то же сами (INTERIOR_LIGHT_SCALE). Повторный запуск ничего не меняет:
обработанные лампы помечаются тегом DarcDimmed.
"""
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SCALE = 0.25
MARK = "DarcDimmed"
n_light = n_tag = 0
for a in sub.get_all_level_actors():
    folder = str(a.get_folder_path())
    if not (folder.startswith("Facility/Main") or folder.startswith("Facility/Annex")):
        continue
    tags = [str(t) for t in a.tags]
    if MARK in tags:
        continue
    if isinstance(a, unreal.PointLight):
        c = a.light_component
        c.set_intensity(c.intensity * SCALE)
        c.set_editor_property("max_draw_distance", 3000.0)
        c.set_editor_property("max_distance_fade_range", 600.0)
        a.tags = [unreal.Name(t) for t in tags + [MARK]]
        n_light += 1
    elif any(t.startswith("DARC.Lamp") for t in tags):
        new = []
        for t in tags:
            if t.startswith("Lumens="):
                t = "Lumens=%d" % round(float(t[7:]) * SCALE)
            new.append(t)
        a.tags = [unreal.Name(t) for t in new + [MARK]]
        n_tag += 1
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] dim: point lights %d, circuit lamps %d" % (n_light, n_tag))
