# -*- coding: utf-8 -*-
"""
D.A.R.C. — деревья (230 скелетных Nanite-деревьев с ветром) съедали ~4.8 мс кадра на игровом потоке:
анимация ветра считалась каждый кадр. Теперь — 20 раз в секунду (медленное ночное качание выглядит так же).
Повторный запуск безопасен. Вернуть: TICK = 0.0.
"""
import unreal

TICK = 0.05
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = 0
for a in sub.get_all_level_actors():
    if not isinstance(a, unreal.SkeletalMeshActor):
        continue
    c = a.skeletal_mesh_component
    m = c.get_skeletal_mesh_asset()
    if not m or "/Megaplant_Library/" not in m.get_path_name():
        continue
    a.modify()
    t = c.get_editor_property("primary_component_tick")
    t.set_editor_property("tick_interval", TICK)
    c.set_editor_property("primary_component_tick", t)
    n += 1
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("[DARC] tree tick interval %.2f on %d trees" % (TICK, n))
