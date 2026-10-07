# -*- coding: utf-8 -*-
"""
D.A.R.C. — объёмы «помещение» (Trigger Box с тегом Indoor) по границам главного здания, пристройки и гаража.
По ним ADarcRain решает, что игрок в здании: глушит дождь и уличный фон, убирает туман. Под козырьком,
навесом и у окна — улица (раньше «крыша над головой» срабатывала и под козырьком).
Пересоздаёт объёмы в папке FacilityProps/Indoor при каждом запуске (после пересборки здания — запустить снова).
"""
import re
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
FOLDER = "FacilityProps/Indoor"
old = [a for a in sub.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)]
if old:
    sub.destroy_actors(old)

groups = {"Main": re.compile(r"^Facility/Main/F\d"), "Annex": re.compile(r"^Facility/Annex/F\d"),
          "Garage": re.compile(r"^Facility/Garage")}
boxes = {}
for a in sub.get_all_level_actors():
    f = str(a.get_folder_path())
    for name, rx in groups.items():
        if rx.match(f) and isinstance(a, unreal.StaticMeshActor) and "Roof" not in a.get_actor_label():
            o, e = a.get_actor_bounds(False)
            b = boxes.setdefault(name, [o.x - e.x, o.y - e.y, o.z - e.z, o.x + e.x, o.y + e.y, o.z + e.z])
            b[0], b[1], b[2] = min(b[0], o.x - e.x), min(b[1], o.y - e.y), min(b[2], o.z - e.z)
            b[3], b[4], b[5] = max(b[3], o.x + e.x), max(b[4], o.y + e.y), max(b[5], o.z + e.z)
log = []
for name, (x0, y0, z0, x1, y1, z1) in boxes.items():
    # Чуть внутрь по краям: в дверном проёме наружной стены — ещё улица.
    x0, y0, x1, y1 = x0 + 15, y0 + 15, x1 - 15, y1 - 15
    t = sub.spawn_actor_from_class(unreal.TriggerBox, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    # Базовая коробка Trigger Box — 80 см (габарит 40), не 64: иначе объём на 25% больше здания.
    ext = t.get_component_by_class(unreal.BoxComponent).get_unscaled_box_extent()
    t.set_actor_scale3d(unreal.Vector((x1 - x0) / (2 * ext.x), (y1 - y0) / (2 * ext.y), (z1 - z0) / (2 * ext.z)))
    t.set_folder_path(FOLDER)
    t.set_actor_label("Indoor_" + name)
    t.set_editor_property("tags", [unreal.Name("Indoor")])
    log.append("%s %.0fx%.0fx%.0f" % (name, x1 - x0, y1 - y0, z1 - z0))
unreal.log("[DARC] indoor volumes: " + " | ".join(log))
