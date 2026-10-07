# -*- coding: utf-8 -*-
"""
D.A.R.C. — решения автора 07.10.2026 по уровню 1:
  - лом не находился (лежал на полу кладовой) → на стол с инструментами в техпомещении (к ключу и тестеру);
  - вентиляция в серверную убрана из выезда 1 (переносится на уровень, где без неё никак): решётка, короб и
    проём — стена снова целая;
  - рации убраны из выезда 1 (появятся позже, где группе есть смысл разделяться);
  - длинные полосы-декали на полу (похожи на след шины) убраны.
"""
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
log = []
actors = sub.get_all_level_actors()
by = {a.get_actor_label(): a for a in actors}

# --- Лом на стол с инструментами ---
crow, table, wrench = by.get("Crowbar"), by.get("ModuleTable"), by.get("Wrench")
if crow and table:
    o, e = table.get_actor_bounds(False)
    top = o.z + e.z
    crow.modify()
    crow.set_actor_rotation(unreal.Rotator(0, 90.0, 10.0), False)          # лёжа вдоль стола
    co, ce = crow.get_actor_bounds(False)
    crow.set_actor_location(crow.get_actor_location() + unreal.Vector(o.x - co.x, (o.y - e.y * 0.55) - co.y, top + 0.5 - (co.z - ce.z)), False, False)
    log.append("crowbar on tool table")

# --- Вентиляция: решётка, короб, проём ---
doomed = [a for a in actors if a.get_actor_label() in ("VentGrille", "VentDuct_S", "VentDuct_N", "VentDuct_Top")]
pieces = [a for a in actors if a.get_actor_label().startswith("W_Cor_ServerSect_020_00_")]
if pieces:
    ref = next((p for p in pieces if p.get_actor_label().endswith("_a")), pieces[0])
    loc, rot = ref.get_actor_location(), ref.get_actor_rotation()
    folder, label = ref.get_folder_path(), ref.get_actor_label().rsplit("_", 1)[0]
    w = sub.spawn_actor_from_object(eal.load_asset("/Game/AbandonedPowerPlant/Assets/Walls/SM_Wall_Plain_300"), loc, rot)
    w.set_folder_path(folder)
    w.set_actor_label(label)
    doomed += pieces
    log.append("wall restored")
# --- Рации ---
doomed += [a for a in actors if a.get_actor_label() in ("Radio_1", "Radio_2")]
# --- Длинные полосы на полу ---
lines = [a for a in actors if isinstance(a, unreal.DecalActor) and a.get_actor_label().startswith("FloorLine_")]
doomed += lines
log.append("removed: vent %d, radios %d, floor lines %d" % (
    len([a for a in doomed if "Vent" in a.get_actor_label() or a in pieces]),
    len([a for a in doomed if a.get_actor_label().startswith("Radio_")]), len(lines)))
if doomed:
    sub.destroy_actors(doomed)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] cleanup: " + " | ".join(log))
